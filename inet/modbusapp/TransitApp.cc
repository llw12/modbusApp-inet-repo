//
// Copyright (C) 2025 llw
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "TransitApp.h"
#include "ModbusMasterApp.h"

#include "ListMsg_m.h"
#include "ModbusStorage.h"
#include "inet/common/ModuleAccess.h"
#include "inet/common/ProtocolTag_m.h"
#include "inet/common/TimeTag_m.h"
#include "inet/common/lifecycle/NodeStatus.h"
#include "inet/common/packet/Message.h"
#include "inet/common/packet/chunk/ByteCountChunk.h"
#include "inet/common/socket/SocketTag_m.h"
#include "inet/networklayer/common/L3AddressResolver.h"
#include "inet/transportlayer/contract/tcp/TcpCommand_m.h"
#include <arpa/inet.h> // 用于htonl、htons等字节序转换函数
#include "OperatorRequest_m.h"

namespace inet {

Define_Module(TransitApp);

void TransitApp::initialize(int stage)
{
    cSimpleModule::initialize(stage);

    if (stage == INITSTAGE_LOCAL) {
        delay = par("replyDelay");
        maxMsgDelay = 0;

        // statistics
        msgsRcvd = msgsSent = bytesRcvd = bytesSent = 0;

        WATCH(msgsRcvd);
        WATCH(msgsSent);
        WATCH(bytesRcvd);
        WATCH(bytesSent);
    }
    else if (stage == INITSTAGE_APPLICATION_LAYER) {
        const char *localAddress = par("localAddress");
        int localPort = par("localPort");
        socket.setOutputGate(gate("socketOut"));
        socket.bind(localAddress[0] ? L3AddressResolver().resolve(localAddress) : L3Address(), localPort);
        socket.listen();

        cModule *node = findContainingNode(this);
        NodeStatus *nodeStatus = node ? check_and_cast_nullable<NodeStatus *>(node->getSubmodule("status")) : nullptr;
        bool isOperational = (!nodeStatus) || nodeStatus->getState() == NodeStatus::UP;
        if (!isOperational)
            throw cRuntimeError("This module doesn't support starting in node DOWN state");
    }
}

void TransitApp::sendOrSchedule(cMessage *msg, simtime_t delay)
{
    if (delay == 0)
        sendBack(msg);
    else
        scheduleAfter(delay, msg);
}

void TransitApp::sendBack(cMessage* msg)
{
    Enter_Method("sendBack");

    take(msg);
    Packet *packet = dynamic_cast<Packet *>(msg);
    packet->addTag<SocketReq>()->setSocketId(remoteSocketId);

    if (packet) {


        msgsSent++;
        bytesSent += packet->getByteLength();
        emit(packetSentSignal, packet);

        EV_INFO << "sending \"" << packet->getName() << "\" to TCP, " << packet->getByteLength() << " bytes\n";
    }
    else {
        EV_INFO << "sending \"" << msg->getName() << "\" to TCP\n";
    }

    auto& tags = check_and_cast<ITaggedObject *>(msg)->getTags();
    tags.addTagIfAbsent<DispatchProtocolReq>()->setProtocol(&Protocol::tcp);
    send(msg, "socketOut");
}

void TransitApp::handleMessage(cMessage *msg)
{
    EV_INFO << "开始处理消息，消息类型: " << (msg->isSelfMessage() ? "自消息" : "外部消息")
            << ", 消息ID: " << msg->getId() << ", 消息名称: " << msg->getName() << endl;

    if (msg->isSelfMessage()) {
        EV_INFO << "处理自消息，即将返回消息: " << msg->getName() << " (ID: " << msg->getId() << ")" << endl;
        sendBack(msg);
    }
    else if (msg->getKind() == TCP_I_PEER_CLOSED) {
        EV_INFO << "收到连接关闭指示消息(TCP_I_PEER_CLOSED)，开始处理关闭逻辑" << endl;

        // 处理连接关闭
        int connId = check_and_cast<Indication *>(msg)->getTag<SocketInd>()->getSocketId();
        EV_INFO << "获取到关闭连接的ID: " << connId << endl;

        delete msg;
        EV_INFO << "已释放关闭指示消息内存" << endl;

        auto request = new Request("close", TCP_C_CLOSE);
        request->addTag<SocketReq>()->setSocketId(connId);
        EV_INFO << "创建关闭连接请求，目标连接ID: " << connId << ", 请求ID: " << request->getId() << endl;

        sendOrSchedule(request, delay + maxMsgDelay);
        EV_INFO << "已发送/调度关闭连接请求，延迟时间: " << (delay + maxMsgDelay) << endl;
    }
    else if (msg->getKind() == TCP_I_DATA || msg->getKind() == TCP_I_URGENT_DATA) {
        EV_INFO << "收到数据消息，消息类型: " << (msg->getKind() == TCP_I_DATA ? "普通数据(TCP_I_DATA)" : "紧急数据(TCP_I_URGENT_DATA)")
                << ", 消息ID: " << msg->getId() << endl;

        Packet *packet = check_and_cast<Packet *>(msg);
        int connId = packet->getTag<SocketInd>()->getSocketId();
        remoteSocketId = connId;
        EV_INFO << "解析数据包成功，来源连接ID: " << connId << ", 数据包总长度: " << packet->getTotalLength() << endl;

        ChunkQueue& queue = socketQueue[connId];
        auto chunk = packet->peekDataAt(B(0), packet->getTotalLength());
        queue.push(chunk);
        EV_INFO << "已将数据块加入队列，队列当前长度: " << queue.getLength() << ", 连接ID: " << connId << endl;

        emit(packetReceivedSignal, packet);
        EV_INFO << "已触发数据包接收信号" << endl;

        EV_INFO << "开始处理消息队列，当前队列长度: " << queue.getLength() << endl;
        while (queue.getLength() != b(0)) {
            if (queue.has<OperatorRequest>(b(-1))) {
                EV_INFO << "队列中存在OperatorRequest，开始提取处理" << endl;
                const auto& opReq = queue.pop<OperatorRequest>(b(-1));
                msgsRcvd++;
                bytesRcvd += B(opReq->getChunkLength()).get();
                EV_INFO << "成功提取OperatorRequest，事务ID: " << opReq->getTransactionId()
                        << ", 累计接收消息数: " << msgsRcvd
                        << ", 累计接收字节数: " << bytesRcvd << endl;

                // 2. 获取ModbusMasterApp实例
                EV_INFO << "开始查找ModbusMasterApp实例" << endl;
                ModbusMasterApp *modbusMasterApp = check_and_cast<ModbusMasterApp*>(findModuleByPath("^.app[0]"));
                if (!modbusMasterApp) {
                    EV_ERROR << "未找到ModbusMasterApp模块!" << endl;
                    continue;
                }
                EV_INFO << "成功获取ModbusMasterApp实例" << endl;

                // 3. 获取ModbusStorage实例
                EV_INFO << "开始获取ModbusStorage实例" << endl;
                ModbusStorage *modbusStorage = &modbusMasterApp->getModbusStorage();
                if (!modbusStorage) {
                    EV_ERROR << "在ModbusMasterApp中未找到ModbusStorage!" << endl;
                    continue;
                }
                EV_INFO << "成功获取ModbusStorage实例" << endl;

                // 1. 解析目标主机名并获取IP地址
                const char* targetHostName = opReq->getTargetHostName();
                EV_INFO << "开始解析目标主机名: " << targetHostName << endl;
                std::string targetHostPath = std::string("^.^.") + targetHostName;
                auto targetHost = findModuleByPath(targetHostPath.c_str());

                L3Address targetIp;
                bool flag = false;
                IInterfaceTable *ift = check_and_cast<IInterfaceTable *>(targetHost->getSubmodule("interfaceTable"));
                for (int i = 0; i < ift->getNumInterfaces(); i++) {
                    NetworkInterface *ie = ift->getInterface(i);
                    if (ie) {
                        L3Address tempIp = ie->getNetworkAddress();
                        if(modbusStorage->findConnectIndexByIpAddress(tempIp) != -1){
                            targetIp = tempIp;
                            flag = true;
                            break;
                        }
                    }
                }

                if(!flag){
                    EV_ERROR << "解析目标主机名失败: " << targetHostName << endl;
                    continue;
                }
                EV_INFO << "成功解析目标IP地址: " << targetIp << endl;



                // 4. 根据IP地址查找连接索引
                EV_INFO << "开始根据IP地址查找连接索引: " << targetIp << endl;
                int connectIndex = modbusStorage->findConnectIndexByIpAddress(targetIp);
                if (connectIndex == -1) {
                    EV_ERROR << "未找到目标IP对应的连接: " << targetIp << endl;
                    continue;
                }
                EV_INFO << "成功找到连接索引: " << connectIndex << " (对应IP: " << targetIp << ")" << endl;

                // 5. 获取对应的socketId
                int targetSocketId = modbusStorage->getConnect(connectIndex).socketId;
                EV_INFO << "获取到目标socketId: " << targetSocketId << " (对应IP: " << targetIp << ")" << endl;

                // 6. 提取OperatorRequest中的关键参数
                uint8_t slaveId = opReq->getSlaveId();
                uint8_t functionCode = opReq->getFunctionCode();
                uint16_t startAddress = opReq->getStartAddress();
                uint16_t quantity = opReq->getQuantity();
                EV_INFO << "提取到Modbus请求参数 - 从站ID: " << (int)slaveId
                        << ", 功能码: 0x" << std::hex << (int)functionCode << std::dec
                        << ", 起始地址: " << startAddress
                        << ", 数量: " << quantity << endl;

                // 7. 提取data字段（核心逻辑）
                std::vector<uint8_t> data;
                B dataLength = B(0); // 数据总长度（字节）

                // 根据functionCode判断数据类型（参考Modbus协议功能码）
                EV_INFO << "开始根据功能码判断数据长度，功能码: 0x" << std::hex << (int)functionCode << std::dec << endl;
                switch (functionCode) {
                    case 0x05:  // 写单个线圈（发送uint8_t）
                    case 0x0F: // 写多个线圈（发送uint8_t）
                        dataLength = B(quantity * 1); // 每个元素1字节
                        EV_INFO << "功能码对应数据长度: " << dataLength << " (每个元素1字节)" << endl;
                        break;
                    case 0x06:  // 写单个寄存器（发送uint16_t）
                    case 0x10: // 写多个寄存器（发送uint16_t）
                        dataLength = B(quantity * 2); // 每个元素2字节
                        EV_INFO << "功能码对应数据长度: " << dataLength << " (每个元素2字节)" << endl;
                        break;
                    case 0x17: {
                        // 读取完整的0x17 PDU长度：func(1)+readStart(2)+readQty(2)+writeStart(2)+writeQty(2)+byteCount(1)+writeData
                        if (queue.getLength() < B(10)) {
                            EV_ERROR << "队列数据不足以解析0x17头部（至少10字节）" << endl;
                            std::vector<uint8_t> exceptionPdu{ uint8_t(0x17 | 0x80), uint8_t(0x03) };
                            auto exceptionPduChunk = makeShared<BytesChunk>();
                            exceptionPduChunk->setBytes(exceptionPdu);
                            auto exceptionPkt = new Packet("exceptionPkt");
                            exceptionPkt->insertAtBack(exceptionPduChunk);
                            exceptionPkt->addTag<CreationTimeTag>()->setCreationTime(simTime());
                            sendBack(exceptionPkt);
                            continue;
                        }
                        // peek前10字节以获取byteCount
                        const auto& hdr10 = queue.peek<BytesChunk>(B(10));
                        auto h = hdr10->getBytes();
                        // 校验功能码与读参数（可选，不做严格核对）；关键是获取byteCount
                        uint8_t byteCount = h[9];
                        B totalLen = B(10 + byteCount);
                        dataLength = totalLen;
                        EV_INFO << "功能码0x17完整PDU长度: " << dataLength << " (10字节头 + " << (int)byteCount << "字节写数据)" << endl;
                        break;
                    }
                    case 0x01:
                    case 0x02:
                    case 0x03:
                    case 0x04:
                        EV_INFO << "功能码无需数据字段，跳过数据提取" << endl;
                        break;
                    default:
                        std::vector<uint8_t> exceptionPdu;
                        exceptionPdu.push_back(functionCode | 0x80); // 异常功能码（最高位置1）
                        exceptionPdu.push_back(0x01);       // 异常代码
                        auto exceptionPduChunk = makeShared<BytesChunk>();
                        exceptionPduChunk->setBytes(exceptionPdu);
                        auto exceptionPkt = new Packet("exceptionPkt");
                        exceptionPkt->insertAtBack(exceptionPduChunk);
                        // Tag the packet's creation time so receivers can compute dataAge
                        exceptionPkt->addTag<CreationTimeTag>()->setCreationTime(simTime());
                        sendBack(exceptionPkt);
                        EV_ERROR << "不支持的功能码: 0x" << std::hex << (int)functionCode << std::dec << endl;
                        continue; // 不支持的功能码，跳过处理
                }

                Packet *requestPacket = nullptr;
                EV_INFO << "准备生成请求数据包" << endl;

                if(dataLength != B(0)){
                    // 从队列中提取data（头部之后的二进制数据）
                    EV_INFO << "开始提取数据，需要长度: " << dataLength << ", 队列可用长度: " << queue.getLength() << endl;
                    if (queue.getLength() >= dataLength) {
                        // 提取指定长度的字节数据
                        const auto& bytesChunk = queue.pop<BytesChunk>(dataLength);
                        auto raw = bytesChunk->getBytes();
                        // 针对0x17：raw是完整PDU，需要转换为ModbusMasterApp::createRequest所需的data格式（仅写起始/写数量/写数据）
                        if (functionCode == 0x17) {
                            if (raw.size() < 10) {
                                EV_ERROR << "0x17 PDU长度不足，无法解析" << endl;
                                continue;
                            }
                            // 提取写起始和写数量
                            uint16_t writeStart = (uint16_t(raw[5]) << 8) | uint16_t(raw[6]);
                            uint16_t writeQty   = (uint16_t(raw[7]) << 8) | uint16_t(raw[8]);
                            uint8_t  byteCount  = raw[9];
                            if (byteCount != writeQty * 2 || raw.size() != size_t(10 + byteCount)) {
                                EV_ERROR << "0x17 PDU字节数与写数量不匹配，或总长度不匹配" << endl;
                                continue;
                            }
                            // 构造data：[writeStart(2), writeQty(2), writeData]
                            data.clear();
                            data.push_back((writeStart >> 8) & 0xFF);
                            data.push_back(writeStart & 0xFF);
                            data.push_back((writeQty >> 8) & 0xFF);
                            data.push_back(writeQty & 0xFF);
                            data.insert(data.end(), raw.begin() + 10, raw.end());
                        }
                        else {
                            data = raw; // 其他功能码保持原样
                        }
                        bytesRcvd += data.size(); // 更新接收字节数统计
                        EV_INFO << "成功提取数据，长度: " << data.size() << "字节, 累计接收字节数: " << bytesRcvd << endl;
                    }
                    else {
                        EV_ERROR << "队列数据不足（需要: " << dataLength << ", 可用: " << queue.getLength() << "）" << endl;
                        delete requestPacket; // 释放已创建的包
                        continue;
                    }
                    // 7. 调用ModbusMasterApp生成请求报文
                    EV_INFO << "调用ModbusMasterApp生成带数据的请求报文" << endl;
                    requestPacket = modbusMasterApp->createRequest(
                       slaveId, functionCode, startAddress, quantity, data
                   );
                   if (!requestPacket) {
                       EV_ERROR << "生成请求报文失败!" << endl;
                       continue;
                   }
                   EV_INFO << "成功生成带数据的请求报文，报文ID: " << requestPacket->getId()
                           << ", 总长度: " << requestPacket->getDataLength() << endl;
                }
                else{
                    // 7. 调用ModbusMasterApp生成请求报文
                    EV_INFO << "调用ModbusMasterApp生成无数据的请求报文" << endl;
                    requestPacket = modbusMasterApp->createRequest(
                    slaveId, functionCode, startAddress, quantity
                    );
                    if (!requestPacket) {
                        EV_ERROR << "生成请求报文失败!" << endl;
                        continue;
                    }
                    EV_INFO << "成功生成无数据的请求报文，报文ID: " << requestPacket->getId()
                            << ", 总长度: " << requestPacket->getDataLength() << endl;
                }


                // 8. 获取目标socket并发送报文
                if (requestPacket) {
                    EV_INFO << "将请求报文加入发送队列，目标socketId: " << targetSocketId << ", 报文ID: " << requestPacket->getId() << endl;
                    // peek chunk BEFORE calling addPacketToQueue because addPacketToQueue deletes the Packet
                    auto transitChunk = requestPacket->peekDataAt(B(0), requestPacket->getTotalLength());
                    transitQueue->push(transitChunk);
                    EV_INFO << "中转消息已成功加入发送队列" << endl;

                    // addPacketToQueue will delete the Packet, so call it after we've extracted the chunk
                    modbusMasterApp->addPacketToQueue(requestPacket, targetSocketId);
                    requestPacket = nullptr; // ownership moved/deleted
                }
            }
            // 处理其他类型消息
            else {
                EV_WARN << "队列中存在不支持的消息类型，已跳过处理，当前队列剩余长度: " << queue.getLength() << endl;
                break; // 避免无限循环，遇到不支持的类型退出循环
            }
        }
        delete packet;
        EV_INFO << "已释放数据包包内存，当前处理周期结束" << endl;
    }
    else if (msg->getKind() == TCP_I_AVAILABLE) {
        EV_INFO << "收到TCP可用指示消息(TCP_I_AVAILABLE)，交由socket处理" << endl;
        socket.processMessage(msg);
    }
    else {
        // 忽略其他指示消息
        EV_WARN << "丢弃未知类型消息: " << msg->getName()
                << ", 类型标识: " << msg->getKind()
                << "(" << cEnum::get("inet::TcpStatusInd")->getStringFor(msg->getKind()) << ")" << endl;
        delete msg;
        EV_INFO << "已释放未知类型消息内存" << endl;
    }
    EV_INFO << "消息处理完成，消息ID: " << msg->getId() << endl;
}

void TransitApp::refreshDisplay() const
{
    char buf[64];
    sprintf(buf, "rcvd: %ld pks %ld bytes\nsent: %ld pks %ld bytes", msgsRcvd, bytesRcvd, msgsSent, bytesSent);
    getDisplayString().setTagArg("t", 0, buf);
}

void TransitApp::finish()
{
    EV_INFO << getFullPath() << ": sent " << bytesSent << " bytes in " << msgsSent << " packets\n";
    EV_INFO << getFullPath() << ": received " << bytesRcvd << " bytes in " << msgsRcvd << " packets\n";
}

} // namespace inet