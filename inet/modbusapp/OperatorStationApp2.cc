/*
 * OperatorStationApp2.cc
 *
 *  Created on: Oct 18, 2025
 *      Author: llw
 */


#include "OperatorStationApp2.h"

#include "ListMsg_m.h"
#include "inet/common/ModuleAccess.h"
#include "inet/common/TimeTag_m.h"
#include "inet/common/lifecycle/ModuleOperations.h"
#include "inet/common/packet/Packet.h"
#include <cstdint>
#include <stdexcept>
#include <algorithm>
#include <sstream>
#include <limits>
#include "inet/networklayer/common/L3AddressResolver.h"
#include <cmath>

namespace inet
{
    // 定时器触发类型不再使用kind，改为通过消息指针区分

    Define_Module(OperatorStationApp2);

    OperatorStationApp2::~OperatorStationApp2()
    {
        // only cancel/delete self messages here; do NOT call socket.destroy() in the destructor
        if (connectMsg) { cancelAndDelete(connectMsg); connectMsg = nullptr; }
        if (sendMsg) { cancelAndDelete(sendMsg); sendMsg = nullptr; }
    }

    static std::vector<std::string> splitBySemicolon(const std::string& s) {
        std::vector<std::string> res;
        std::string cur;
        for (char c : s) {
            if (c == ';') {
                if (!cur.empty()) { res.push_back(cur); cur.clear(); }
            }
            else {
                cur += c;
            }
        }
        if (!cur.empty()) res.push_back(cur);
        return res;
    }

    void OperatorStationApp2::initialize(int stage)
    {
        TcpAppBase::initialize(stage);
        if (stage == INITSTAGE_LOCAL) {
            earlySend = false;

            // 读取并解析modbusRequest（多条命令，;分隔）
            modbusRequest = par("modbusRequest").stdstringValue();
            auto reqItems = splitBySemicolon(modbusRequest);
            if (reqItems.empty())
                throw cRuntimeError("modbusRequest为空，至少需要一条命令");

            commands.clear();
            for (auto &item : reqItems) {
                auto parts = splitBySpace(item);
                if (parts.size() < 5) {
                    throw cRuntimeError("modbusRequest格式错误，应为：targetHostName slaveId functionCode startAddress number [data...];... (当前项: %s)", item.c_str());
                }
                ModbusCommand cmd;
                cmd.targetHostName = parts[0];
                cmd.slaveId = hexStringToUint8_t(parts[1]);
                cmd.functionCode = hexStringToUint8_t(parts[2]);
                cmd.startAddress = hexStringToUint16_t(parts[3]);
                cmd.quantity = hexStringToUint16_t(parts[4]);
                for (size_t i = 5; i < parts.size(); ++i)
                    cmd.data.push_back(hexStringToUint8_t(parts[i]));
                commands.push_back(std::move(cmd));
            }

            // 读取并解析sendTime（空格分隔，需与命令数量一致且单调递增）
            sendTimeStr = par("sendTime").stdstringValue();
            auto timeTokens = splitBySpace(sendTimeStr);
            if (timeTokens.size() != commands.size()) {
                throw cRuntimeError("sendTime数量(%zu)与modbusRequest命令数(%zu)不一致", timeTokens.size(), commands.size());
            }
            sendTimes.resize(timeTokens.size());
            for (size_t i = 0; i < timeTokens.size(); ++i) {
                // 将时间字符串按秒解析
                double sec = std::stod(timeTokens[i]);
                if (sec < 0.0)
                    throw cRuntimeError("sendTime不能为负数 (index=%zu, value=%s)", i, timeTokens[i].c_str());
                sendTimes[i] = SimTime(sec, SIMTIME_S);
                if (i > 0 && !(sendTimes[i] > sendTimes[i-1])) {
                    throw cRuntimeError("sendTime必须严格递增 (index=%zu, value=%s)", i, timeTokens[i].c_str());
                }
            }

            seed = par("seed").intValue();

            // 读取seed并初始化可重复性随机数引擎
            if (seed != 0) {
                rng.seed(seed);

                // 计算每条命令的随机扰动与实际发送时间
                actualTimes.resize(sendTimes.size());
                for (size_t i = 0; i < sendTimes.size(); ++i) {
                    simtime_t lower(0), upper(0);
                    if (sendTimes.size() == 1) {
                        // 仅一条命令：不添加扰动
                        lower = SIMTIME_ZERO;
                        upper = SIMTIME_ZERO;
                    }
                    else if (i == 0) {
                        simtime_t dNext = sendTimes[1] - sendTimes[0];
                        lower = SIMTIME_ZERO;
                        upper = dNext / 10;
                    }
                    else if (i == sendTimes.size() - 1) {
                        simtime_t dPrev = sendTimes[i-1] - sendTimes[i]; // 负数
                        lower = dPrev / 10; // 负向扰动
                        upper = SIMTIME_ZERO;
                    }
                    else {
                        simtime_t dPrev = sendTimes[i-1] - sendTimes[i]; // 负数
                        simtime_t dNext = sendTimes[i+1] - sendTimes[i]; // 正数
                        lower = dPrev / 10;
                        upper = dNext / 10;
                    }

                    // 将上下界转换为皮秒刻度的整数，确保后续产生的抖动严格落在 (lower, upper) 内
                    long long lowerPs = (long long) llround(lower.inUnit(SIMTIME_PS));
                    long long upperPs = (long long) llround(upper.inUnit(SIMTIME_PS));

                    simtime_t jitter = SIMTIME_ZERO;
                    if (upperPs - lowerPs > 1) {
                        // 在 (lowerPs, upperPs) 之间选择一个整数皮秒（严格不含端点）
                        std::uniform_int_distribution<long long> dist(0, (upperPs - lowerPs - 2));
                        long long k = dist(rng);
                        long long jPs = lowerPs + 1 + k;
                        jitter = SimTime((double)jPs, SIMTIME_PS);
                    }
                    else {
                        // 无法满足严格不含端点的范围，退化：不加扰动，同时提示
                        EV_WARN << "扰动范围过小，无法满足严格不含端点 (i=" << i << ")，将不添加扰动" << endl;
                        jitter = SIMTIME_ZERO;
                    }

                    actualTimes[i] = sendTimes[i] + jitter;
                }

            }
            else {
                // 若未提供seed，则不偏移
                actualTimes.resize(sendTimes.size());
                for (size_t i = 0; i < sendTimes.size(); ++i){
                    actualTimes[i] = sendTimes[i];
                }
                EV_WARN << "未提供seed参数，发送时间将不添加随机扰动";
            }


            // 生成发送顺序（按actualTimes排序）
            scheduleOrder.resize(actualTimes.size());
            for (size_t i = 0; i < scheduleOrder.size(); ++i) scheduleOrder[i] = i;
            std::sort(scheduleOrder.begin(), scheduleOrder.end(), [&](size_t a, size_t b){
                if (actualTimes[a] == actualTimes[b]) return a < b;
                return actualTimes[a] < actualTimes[b];
            });
            nextSendIdx = 0;

            // 创建定时器
            connectMsg = new cMessage("connectTimer");
            sendMsg = new cMessage("sendTimer");
        }
    }

    void OperatorStationApp2::handleStartOperation(LifecycleOperation *operation)
    {
        simtime_t now = simTime();
        if (connectMsg && !connectMsg->isScheduled())
            scheduleAt(now, connectMsg);
    }

    void OperatorStationApp2::handleStopOperation(LifecycleOperation *operation)
    {
        if (connectMsg) cancelEvent(connectMsg);
        if (sendMsg) cancelEvent(sendMsg);
        if (socket.getState() == TcpSocket::CONNECTED || socket.getState() == TcpSocket::CONNECTING || socket.getState() == TcpSocket::PEER_CLOSED)
            close();
    }

    void OperatorStationApp2::handleCrashOperation(LifecycleOperation *operation)
    {
        if (connectMsg) cancelEvent(connectMsg);
        if (sendMsg) cancelEvent(sendMsg);
        if (operation->getRootModule() != getContainingNode(this))
            socket.destroy();
    }


    // 发送OperatorRequest报文（按索引）
    void OperatorStationApp2::sendModbusRequest(size_t index)
    {
        if (index >= commands.size()) return;
        const auto &cmd = commands[index];

        // 创建Modbus请求报文
        const auto& request = makeShared<OperatorRequest>();
        EV_INFO << "targetHostName length: " << cmd.targetHostName.length() << endl;
        request->setChunkLength(B(2 + cmd.targetHostName.length() + 12));
        EV_INFO << "request length: " << request->getChunkLength() << endl;
        request->setTargetHostName(cmd.targetHostName.c_str());
        request->setTransactionId(++transactionId);  // 事务ID自增
        request->setProtocolId(0);  // Modbus TCP协议标识固定为0
        request->setSlaveId(cmd.slaveId);
        request->setFunctionCode(cmd.functionCode);
        request->setStartAddress(cmd.startAddress);
        request->setQuantity(cmd.quantity);

        // 根据功能码构造有效载荷与长度
        std::vector<uint8_t> payloadBytes;
        if (cmd.functionCode == 0x17) {
            // 约定：cmd.startAddress/quantity 为读起始与读数量；
            // cmd.data: [writeStartHigh, writeStartLow, writeQtyHigh, writeQtyLow, writeData...]
            if (cmd.data.size() < 4) {
                throw cRuntimeError("功能码0x17数据至少需要4字节（写起始、写数量）");
            }
            uint16_t writeStart = (uint16_t(cmd.data[0]) << 8) | uint16_t(cmd.data[1]);
            uint16_t writeQty   = (uint16_t(cmd.data[2]) << 8) | uint16_t(cmd.data[3]);
            size_t expectedBytes = size_t(writeQty) * 2;
            if (cmd.data.size() < 4 + expectedBytes) {
                throw cRuntimeError("功能码0x17写入数据长度不足：期望%zu字节，实际%zu字节", expectedBytes, cmd.data.size() - 4);
            }
            // 构造PDU：func(1)+readStart(2)+readQty(2)+writeStart(2)+writeQty(2)+byteCount(1)+writeData
            payloadBytes.reserve(1 + 2 + 2 + 2 + 2 + 1 + expectedBytes);
            payloadBytes.push_back(0x17);
            payloadBytes.push_back((cmd.startAddress >> 8) & 0xFF);
            payloadBytes.push_back(cmd.startAddress & 0xFF);
            payloadBytes.push_back((cmd.quantity >> 8) & 0xFF);
            payloadBytes.push_back(cmd.quantity & 0xFF);
            payloadBytes.push_back((writeStart >> 8) & 0xFF);
            payloadBytes.push_back(writeStart & 0xFF);
            payloadBytes.push_back((writeQty >> 8) & 0xFF);
            payloadBytes.push_back(writeQty & 0xFF);
            payloadBytes.push_back(uint8_t(writeQty * 2));
            for (size_t i = 0; i < expectedBytes; ++i) {
                payloadBytes.push_back(cmd.data[4 + i]);
            }
            // 修正length字段：从slaveId到PDU的总长度（slaveId(1)+PDU长度）
            request->setLength(1 + payloadBytes.size());
        }
        else {
            // 非0x17：沿用原有逻辑，长度= slaveId(1)+func(1)+startAddr(2)+quantity(2)+data.size()
            request->setLength(1 + 1 + 2 + 2 + cmd.data.size());
            // 如果存在data，直接按原设计附加
            if (!cmd.data.empty()) {
                payloadBytes.reserve(1 + 2 + 2 + cmd.data.size());
                payloadBytes.push_back(cmd.functionCode);
                payloadBytes.push_back((cmd.startAddress >> 8) & 0xFF);
                payloadBytes.push_back(cmd.startAddress & 0xFF);
                payloadBytes.push_back((cmd.quantity >> 8) & 0xFF);
                payloadBytes.push_back(cmd.quantity & 0xFF);
                for (auto b : cmd.data) payloadBytes.push_back(b);
            }
            else {
                // 无data时，仅功能码+地址+数量由上面的字段体现，Payload可为空，由服务端按头部解析
            }
        }

        // 封装为Packet并发送
        Packet *packet = new Packet("modbusRequest");
        request->addTag<CreationTimeTag>()->setCreationTime(simTime());
        packet->insertAtFront(request);

        if (!payloadBytes.empty()) {
            auto payload = makeShared<BytesChunk>();
            payload->setBytes(payloadBytes);
            packet->insertAtBack(payload);
        }

        packet->addTag<CreationTimeTag>()->setCreationTime(simTime());
        if (socket.getState() == TcpSocket::CONNECTED) {
            EV_INFO << "Sending modbusRequest packet, id=" << packet->getId() << ", socket state=" << socket.getState() << "; idx=" << index << "\n";
            sendPacket(packet);
        }
        else {
            EV_WARN << "Socket not CONNECTED for modbusRequest (state=" << socket.getState() << "); dropping packet to avoid leak" << endl;
            delete packet;
        }

        EV_INFO << "请求报文长度： " << packet->getByteLength() << " 字节" << endl;

        EV_INFO << "发送Modbus请求报文，事务ID：" << transactionId
                 << "，目标从站：" << (int)cmd.slaveId << ", 计划时间=" << sendTimes[index] << ", 当前时间=" << actualTimes[index] << "偏移" << sendTimes[index]-actualTimes[index] << endl;
    }

    void OperatorStationApp2::handleTimer(cMessage *msg)
    {
        if (msg == connectMsg) {
            connect();
        }
        else if (msg == sendMsg) {
            // 发送下一条
            if (nextSendIdx < scheduleOrder.size()) {
                size_t idx = scheduleOrder[nextSendIdx];
                sendModbusRequest(idx);
                nextSendIdx++;
                // 安排下一条
                if (nextSendIdx < scheduleOrder.size()) {
                    size_t nextIdx = scheduleOrder[nextSendIdx];
                    simtime_t t = actualTimes[nextIdx];
                    simtime_t now = simTime();
                    scheduleAt(t > now ? t : now, sendMsg);
                }
            }
        }
        else {
            throw cRuntimeError("未知定时器消息");
        }
    }

    void OperatorStationApp2::socketEstablished(TcpSocket *socket)
    {
        TcpAppBase::socketEstablished(socket);

        if (!earlySend) {
            // 连接建立后调度第一次Modbus请求发送（确保在连接建立后发送）
            if (sendMsg) {
                if (sendMsg->isScheduled()) cancelEvent(sendMsg);
                if (nextSendIdx < scheduleOrder.size()) {
                    size_t idx = scheduleOrder[nextSendIdx];
                    simtime_t t = actualTimes[idx];
                    simtime_t when = t > simTime() ? t : simTime();
                    scheduleAt(when, sendMsg);
                    EV_INFO << "已调度Modbus请求在 " << when << " 发送 (index=" << idx << ")" << endl;
                }
            }
        }
    }


    void OperatorStationApp2::socketDataArrived(TcpSocket *socket, Packet *msg, bool urgent)
    {
        EV_INFO << "收到服务器回复报文" << endl;
        // Forward to base to update packetsRcvd/bytesRcvd, emit signal, and delete the packet
        TcpAppBase::socketDataArrived(socket, msg, urgent);
    }

    void OperatorStationApp2::close()
    {
        TcpAppBase::close();
        if (sendMsg) cancelEvent(sendMsg);
    }

    void OperatorStationApp2::socketClosed(TcpSocket *socket)
    {
        TcpAppBase::socketClosed(socket);
    }

    void OperatorStationApp2::socketFailure(TcpSocket *socket, int code)
    {
        TcpAppBase::socketFailure(socket, code);
        if (connectMsg) {
            simtime_t d = par("reconnectInterval");
            scheduleAt(simTime() + d, connectMsg);
        }
    }

    void OperatorStationApp2::finish()
    {
        // try to destroy the socket while module and gates are still intact
        try {
            if (socket.getState() != TcpSocket::CLOSED && socket.getState() != TcpSocket::NOT_BOUND)
                socket.destroy();
        }
        catch (const std::exception &e) {
            EV_WARN << "socket.destroy() threw exception in finish(): " << e.what() << endl;
        }
        catch (...) {
            EV_WARN << "socket.destroy() threw unknown exception in finish()" << endl;
        }

        // call base finish (collect stats etc.)
        TcpAppBase::finish();
    }

    std::vector<std::string> OperatorStationApp2::splitBySpace(const std::string& str)
    {
        std::vector<std::string> result;
        std::string current;
        for (char c : str) {
            if (isspace(c)) {
                if (!current.empty()) {
                    result.push_back(current);
                    current.clear();
                }
            }
            else {
                current += c;
            }
        }
        if (!current.empty()) {
            result.push_back(current);
        }
        return result;
    }

    bool OperatorStationApp2::isHexChar(char c) {
        return (c >= '0' && c <= '9') ||
               (c >= 'a' && c <= 'f') ||
               (c >= 'A' && c <= 'F');
    }

    // 实现16进制字符串转uint8_t
    uint8_t OperatorStationApp2::hexStringToUint8_t(std::string hexString) {
        if (hexString.size() != 2) {
            throw cRuntimeError("hexStringToUint8_t: 输入必须为2个十六进制字符");
        }
        for (char c : hexString) {
            if (!isHexChar(c)) {
                throw cRuntimeError("hexStringToUint8_t: 包含非十六进制字符: %c", c);
            }
        }
        unsigned long val = std::stoul(hexString, nullptr, 16);
        if (val > 0xFF) {
            throw cRuntimeError("hexStringToUint8_t: 值超过uint8_t范围");
        }
        return static_cast<uint8_t>(val);
    }

    // 新增：16进制字符串转uint16_t
    uint16_t OperatorStationApp2::hexStringToUint16_t(std::string hexString) {
        if (hexString.size() != 4) {
            throw cRuntimeError("hexStringToUint16_t: 输入必须为4个十六进制字符");
        }
        for (char c : hexString) {
            if (!isHexChar(c)) {
                throw cRuntimeError("hexStringToUint16_t: 包含非十六进制字符: %c", c);
            }
        }
        unsigned long val = std::stoul(hexString, nullptr, 16);
        if (val > 0xFFFF) {
            throw cRuntimeError("hexStringToUint16_t: 值超过uint16_t范围");
        }
        return static_cast<uint16_t>(val);
    }


}