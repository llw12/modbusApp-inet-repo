//
// Copyright (C) 2025 llw
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "ModbusTcpServerApp.h"
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

Define_Module(ModbusTcpServerApp);

void ModbusTcpServerApp::initialize(int stage)
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

void ModbusTcpServerApp::sendOrSchedule(cMessage *msg, simtime_t delay)
{
    if (delay == 0)
        sendBack(msg);
    else
        scheduleAfter(delay, msg);
}

void ModbusTcpServerApp::sendBack(cMessage *msg)
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

void ModbusTcpServerApp::handleMessage(cMessage *msg)
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
            // 检查是否为ListMsg
            if (queue.has<ListMsg>(b(-1))) {
                const auto& listMsg = queue.pop<ListMsg>(b(-1));
                msgsRcvd++;
                bytesRcvd += B(listMsg->getChunkLength()).get();

                EV_INFO << "Received ListMsg with sequence number: " << listMsg->getSequenceNumber() << endl;

                // 获取兄弟模块ModbusMasterApp
                ModbusMasterApp *modbusMasterApp = check_and_cast<ModbusMasterApp*>(findModuleByPath("^.app[0]"));
                if (!modbusMasterApp) {
                    EV_ERROR << "ModbusMasterApp module not found!" << endl;
                    delete packet;
                    continue;
                }

                // 获取modbusStorage
                ModbusStorage *modbusStorage =&modbusMasterApp->getModbusStorage();
                if (!modbusStorage) {
                    EV_ERROR << "ModbusStorage not found in ModbusMasterApp!" << endl;
                    delete packet;
                    continue;
                }

                // 序列化modbusStorage到BytesChunk
                auto payload = makeShared<BytesChunk>();
                payload->setBytes(modbusStorage->serializeModbusStorage(modbusStorage));
                if (!payload) {
                    EV_ERROR << "Failed to serialize ModbusStorage" << endl;
                    delete packet;
                    continue;
                }

                // 创建回复包
                Packet *outPacket = new Packet("ModbusStorageReply", TCP_C_SEND);
                outPacket->addTag<SocketReq>()->setSocketId(connId);
                outPacket->insertAtBack(payload);
                outPacket->addTag<CreationTimeTag>()->setCreationTime(simTime());

                sendOrSchedule(outPacket, delay);
            }
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

void ModbusTcpServerApp::refreshDisplay() const
{
    char buf[64];
    sprintf(buf, "rcvd: %ld pks %ld bytes\nsent: %ld pks %ld bytes", msgsRcvd, bytesRcvd, msgsSent, bytesSent);
    getDisplayString().setTagArg("t", 0, buf);
}

void ModbusTcpServerApp::finish()
{
    EV_INFO << getFullPath() << ": sent " << bytesSent << " bytes in " << msgsSent << " packets\n";
    EV_INFO << getFullPath() << ": received " << bytesRcvd << " bytes in " << msgsRcvd << " packets\n";
}

} // namespace inet
