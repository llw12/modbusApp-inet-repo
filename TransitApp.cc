//
// Copyright (C) 2025 llw
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "inet/applications/tcpapp/TransitApp.h"
#include "ModbusMasterApp.h"

#include "inet/applications/tcpapp/ListMsg_m.h"
#include "inet/applications/tcpapp/ModbusStorage.h"
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
#include "inet/applications/tcpapp/OperatorRequest_m.h"

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

void TransitApp::sendBack(cMessage *msg)
{
    Packet *packet = dynamic_cast<Packet *>(msg);

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
    if (msg->isSelfMessage()) {
        sendBack(msg);
    }
    else if (msg->getKind() == TCP_I_PEER_CLOSED) {
        // 处理连接关闭
        int connId = check_and_cast<Indication *>(msg)->getTag<SocketInd>()->getSocketId();
        delete msg;
        auto request = new Request("close", TCP_C_CLOSE);
        request->addTag<SocketReq>()->setSocketId(connId);
        sendOrSchedule(request, delay + maxMsgDelay);
    }
    else if (msg->getKind() == TCP_I_DATA || msg->getKind() == TCP_I_URGENT_DATA) {
        Packet *packet = check_and_cast<Packet *>(msg);
        int connId = packet->getTag<SocketInd>()->getSocketId();
        ChunkQueue& queue = socketQueue[connId];
        auto chunk = packet->peekDataAt(B(0), packet->getTotalLength());
        queue.push(chunk);
        emit(packetReceivedSignal, packet);

        while (queue.getLength() != b(0)) {
            if (queue.has<OperatorRequest>(b(-1))) {
                const auto& opReq = queue.pop<OperatorRequest>(b(-1));
                msgsRcvd++;
                bytesRcvd += B(opReq->getChunkLength()).get();

                EV_INFO << "Received OperatorRequest with transactionId: " << opReq->getTransactionId() << endl;

                // 1. 解析目标主机名并获取IP地址
                const char* targetHostName = opReq->getTargetHostName();
                L3Address targetIp = L3AddressResolver().resolve(targetHostName);
                if (targetIp.isUnspecified()) {
                    EV_ERROR << "Failed to resolve target host name: " << targetHostName << endl;
                    continue;
                }
                EV_INFO << "Resolved target IP: " << targetIp << endl;

                // 2. 获取ModbusMasterApp实例
                ModbusMasterApp *modbusMasterApp = check_and_cast<ModbusMasterApp*>(findModuleByPath("^.app[0]"));
                if (!modbusMasterApp) {
                    EV_ERROR << "ModbusMasterApp module not found!" << endl;
                    continue;
                }

                // 3. 获取ModbusStorage实例
                ModbusStorage *modbusStorage = &modbusMasterApp->getModbusStorage();
                if (!modbusStorage) {
                    EV_ERROR << "ModbusStorage not found in ModbusMasterApp!" << endl;
                    continue;
                }

                // 4. 根据IP地址查找连接索引
                int connectIndex = modbusStorage->findConnectIndexByIpAddress(targetIp);
                if (connectIndex == -1) {
                    EV_ERROR << "No connection found for target IP: " << targetIp << endl;
                    continue;
                }

                // 5. 获取对应的socketId
                int targetSocketId = modbusStorage->getConnect(connectIndex).socketId;
                EV_INFO << "Found target socketId: " << targetSocketId << " for IP: " << targetIp << endl;

                // 6. 提取OperatorRequest中的关键参数
                uint8_t slaveId = opReq->getSlaveId();
                uint8_t functionCode = opReq->getFunctionCode();
                uint16_t startAddress = opReq->getStartAddress();
                uint16_t quantity = opReq->getQuantity();

                // 7. 提取data字段（核心逻辑）
                   std::vector<uint8_t> data;
                   B dataLength = B(0); // 数据总长度（字节）

                   // 根据functionCode判断数据类型（参考Modbus协议功能码）
                   switch (functionCode) {
                       case 0x05:  // 写单个线圈（发送uint8_t）
                       case 0x0F: // 写多个线圈（发送uint8_t）
                           dataLength = B(quantity * 1); // 每个元素1字节
                           break;
                       case 0x06:  // 写单个寄存器（发送uint16_t）
                       case 0x10: // 写多个寄存器（发送uint16_t）
                           dataLength = B(quantity * 2); // 每个元素2字节
                           break;
                       case 0x01:
                       case 0x02:
                       case 0x03:
                       case 0x04:
                           continue;
                       default:
                           EV_ERROR << "Unsupported function code: " << (int)functionCode << endl;
                           continue; // 不支持的功能码，跳过处理
                   }

                   auto requestPacket = new Packet("requestPacket");
                   EV_INFO << "发送requestPacket，ID=" << requestPacket->getId() << endl;

                   if(dataLength != B(0)){
                       // 从队列中提取data（头部之后的二进制数据）
                       if (queue.getLength() >= dataLength) {
                           // 提取指定长度的字节数据
                           const auto& bytesChunk = queue.pop<BytesChunk>(dataLength);
                           data = bytesChunk->getBytes(); // 转换为vector<uint8_t>
                           bytesRcvd += data.size(); // 更新接收字节数统计
                           EV_INFO << "Extracted " << data.size() << " bytes of data for function code " << (int)functionCode << endl;
                       }
                       else {
                           EV_ERROR << "Insufficient data in queue (need " << dataLength << ", available " << queue.getLength() << ")" << endl;
                           continue;
                       }
                       // 7. 调用ModbusMasterApp生成请求报文
                       requestPacket = modbusMasterApp->createRequest(
                          slaveId, functionCode, startAddress, quantity, data
                      );
                      EV_INFO << "requestPacket总长度：" << requestPacket->getDataLength()  << endl;
                      EV_INFO << "发送requestPacket，ID=" << requestPacket->getId() << endl;
                      if (!requestPacket) {
                          EV_ERROR << "Failed to create request packet!" << endl;
                          continue;
                      }
                   }
                   else{
                       // 7. 调用ModbusMasterApp生成请求报文
                             requestPacket = modbusMasterApp->createRequest(
                             slaveId, functionCode, startAddress, quantity
                         );
                         if (!requestPacket) {
                             EV_ERROR << "Failed to create request packet!" << endl;
                             continue;
                         }

                   }


                // 8. 获取目标socket并发送报文
                // 假设ModbusMasterApp有获取socket的方法，若没有需在ModbusMasterApp中添加
                auto targetSocket = modbusMasterApp->getSocketMap().getSocketById(targetSocketId);
                if (!targetSocket) {
                    EV_ERROR << "Target socket with id " << targetSocketId << " not found!" << endl;
                    delete requestPacket;
                    continue;
                }


                modbusMasterApp->sendPacket(requestPacket, check_and_cast<TcpSocket*>(targetSocket));
//                drop(requestPacket);
                EV_INFO << "Sent request packet via socket " << targetSocketId << endl;
            }
            // 处理其他类型消息
            else {
                EV_WARN << "Unsupported message type in queue, skipping" << endl;
            }
        }
        delete packet;
    }
    else if (msg->getKind() == TCP_I_AVAILABLE)
        socket.processMessage(msg);
    else {
        // 忽略其他指示消息
        EV_WARN << "drop msg: " << msg->getName() << ", kind:" << msg->getKind() << "(" << cEnum::get("inet::TcpStatusInd")->getStringFor(msg->getKind()) << ")\n";
        delete msg;
    }
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
