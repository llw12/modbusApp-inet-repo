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
#include "inet/networklayer/common/L3AddressResolver.h"

namespace inet
{
    // 定时器触发类型定义
    #define MSGKIND_CONNECT    0  // 定时器触发：发起TCP连接
    #define MSGKIND_MODBUS_SEND 1 // 新增：发送Modbus请求报文

    Define_Module(OperatorStationApp2);

    OperatorStationApp2::~OperatorStationApp2()
    {
        // only cancel/delete self messages here; do NOT call socket.destroy() in the destructor
        cancelAndDelete(timeoutMsg);
        timeoutMsg = nullptr;
    }

    void OperatorStationApp2::initialize(int stage)
    {
        TcpAppBase::initialize(stage);
        if (stage == INITSTAGE_LOCAL) {
            earlySend = false;

            //读取modbusRequest参数并解析
            modbusRequest = par("modbusRequest").stdstringValue();
            std::vector<std::string> parts = splitBySpace(modbusRequest);
            if (parts.size() < 5) {  // 至少需要5个基础字段
                throw cRuntimeError("modbusRequest格式错误，应为：targetHostName slaveId functionCode startAddress number [data...]");
            }
            targetHostName = parts[0];
            targetSlaveId = hexStringToUint8_t(parts[1]);
            functionCode = hexStringToUint8_t(parts[2]);
            targetStartAddress = hexStringToUint16_t(parts[3]);
            number = hexStringToUint16_t(parts[4]);

            // 解析可选的data字段
            for (size_t i = 5; i < parts.size(); ++i) {
                data.push_back(hexStringToUint8_t(parts[i]));
            }

            // 新增：读取sendTime参数
            sendTime = par("sendTime");
            if (sendTime < SIMTIME_ZERO) {
                throw cRuntimeError("sendTime不能为负数");
            }


            timeoutMsg = new cMessage("timer");
        }
    }

    void OperatorStationApp2::handleStartOperation(LifecycleOperation *operation)
    {
        simtime_t now = simTime();

        // 调度首次连接事件
        if (timeoutMsg) {
            timeoutMsg->setKind(MSGKIND_CONNECT);
            scheduleAt(now, timeoutMsg);
        }
    }

    void OperatorStationApp2::handleStopOperation(LifecycleOperation *operation)
    {
        cancelEvent(timeoutMsg);
        if (socket.getState() == TcpSocket::CONNECTED || socket.getState() == TcpSocket::CONNECTING || socket.getState() == TcpSocket::PEER_CLOSED)
            close();
    }

    void OperatorStationApp2::handleCrashOperation(LifecycleOperation *operation)
    {
        cancelEvent(timeoutMsg);
        if (operation->getRootModule() != getContainingNode(this))
            socket.destroy();
    }


    // 发送OperatorRequest报文
    void OperatorStationApp2::sendModbusRequest()
    {
        // 创建Modbus请求报文
        const auto& request = makeShared<OperatorRequest>();
        EV_INFO << "targetHostName length: " << targetHostName.length() << endl;
        request->setChunkLength(B(2 + targetHostName.length() + 12));
        EV_INFO << "request length: " << request->getChunkLength() << endl;
        request->setTargetHostName(targetHostName.c_str());
        request->setTransactionId(++transactionId);  // 事务ID自增
        request->setProtocolId(0);  // Modbus TCP协议标识固定为0
        // 计算长度：从slaveId到数据的总字节数
        request->setLength(1 + 1 + 2 + 2 + data.size());  // slaveId(1) + funcCode(1) + startAddr(2) + quantity(2) + data
        request->setSlaveId(targetSlaveId);
        request->setFunctionCode(functionCode);
        request->setStartAddress(targetStartAddress);
        request->setQuantity(number);


        // 封装为Packet并发送
        Packet *packet = new Packet("modbusRequest");
        packet->insertAtFront(request);

        if(data.size() != 0){
            auto payload = makeShared<BytesChunk>();
            payload->setBytes(data);
            packet->insertAtBack(payload);
        }

//        //测试是否由于TCP segement长度为奇数导致CRC校验失败
//        auto testPayload = makeShared<ByteCountChunk>(B(1), '?');
//        packet->insertAtBack(testPayload);

        packet->addTag<CreationTimeTag>()->setCreationTime(simTime());
        // Only send if socket is fully connected. Otherwise delete packet to avoid undisposed objects.
        if (socket.getState() == TcpSocket::CONNECTED) {
            EV_INFO << "Sending modbusRequest packet, id=" << packet->getId() << ", socket state=" << socket.getState() << "\n";
            sendPacket(packet);
        }
        else {
            EV_WARN << "Socket not CONNECTED for modbusRequest (state=" << socket.getState() << "); dropping packet to avoid leak" << endl;
            delete packet;
        }

        EV_INFO << "请求报文长度： " << packet->getByteLength() << " 字节" << endl;

        EV_INFO << "发送Modbus请求报文，事务ID：" << transactionId
                 << "，目标从站：" << (int)targetSlaveId << endl;
    }

    void OperatorStationApp2::handleTimer(cMessage *msg)
    {
        switch (msg->getKind()) {
            case MSGKIND_CONNECT:
                connect();
                break;
            case MSGKIND_MODBUS_SEND:  // 处理Modbus请求发送
                sendModbusRequest();
                break;
            default:
                throw cRuntimeError("未知定时器类型: kind=%d", msg->getKind());
        }
    }

    void OperatorStationApp2::socketEstablished(TcpSocket *socket)
    {
        TcpAppBase::socketEstablished(socket);

        if (!earlySend) {
            // 连接建立后调度Modbus请求发送（确保在连接建立后发送）
            simtime_t now = simTime();
            simtime_t actualSendTime = std::max(sendTime, now);  // 确保不早于当前时间
            if (timeoutMsg) {
                if (timeoutMsg->isScheduled()) {
                    cancelEvent(timeoutMsg);
                }
                timeoutMsg->setKind(MSGKIND_MODBUS_SEND);
                scheduleAt(actualSendTime, timeoutMsg);
                EV_INFO << "已调度Modbus请求在 " << actualSendTime << " 发送" << endl;
             }
        }
    }


    void OperatorStationApp2::socketDataArrived(TcpSocket *socket, Packet *msg, bool urgent)
    {
        EV_INFO << "收到服务器回复报文" << endl;
    }

    void OperatorStationApp2::close()
    {
        TcpAppBase::close();
        cancelEvent(timeoutMsg);
    }

    void OperatorStationApp2::socketClosed(TcpSocket *socket)
    {
        TcpAppBase::socketClosed(socket);
    }

    void OperatorStationApp2::socketFailure(TcpSocket *socket, int code)
    {
        TcpAppBase::socketFailure(socket, code);
        if (timeoutMsg) {
            simtime_t d = par("reconnectInterval");
            scheduleAfter(d, MSGKIND_CONNECT);
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
