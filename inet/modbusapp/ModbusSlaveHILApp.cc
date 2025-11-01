/*
 * ModbusSlaveHILApp.cc
 *
 *  Created on: Oct 26, 2025
 *      Author: llw
 */

#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#include "ModbusSlaveHILApp.h"
#include "ModbusHeaderSerializer.h"
#include "ModbusHeader_m.h"

#include "inet/common/packet/serializer/ChunkSerializerRegistry.h"
#include "inet/common/packet/serializer/BytesChunkSerializer.h"
#include "inet/common/ModuleAccess.h"
#include "inet/common/ProtocolTag_m.h"
#include "inet/common/TimeTag_m.h"
#include "inet/common/lifecycle/NodeStatus.h"
#include "inet/common/packet/Message.h"
#include "inet/common/packet/chunk/ByteCountChunk.h"
#include "inet/common/socket/SocketTag_m.h"
#include "inet/networklayer/common/L3AddressResolver.h"
#include "inet/transportlayer/contract/tcp/TcpCommand_m.h"

namespace inet {

Define_Module(ModbusSlaveHILApp);

void ModbusSlaveHILApp::initialize(int stage)
{
    cSimpleModule::initialize(stage);

    if (stage == INITSTAGE_LOCAL) {

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

        // 1. 创建TCP Socket
        client_fd = ::socket(AF_INET, SOCK_STREAM, 0);
        if (client_fd == -1) {
            throw cRuntimeError("创建Socket失败");
        }

        const char *remoteAddress = par("remoteAddress");
        int remotePort = par("remotePort");
        // 2. 连接服务器
        struct sockaddr_in server_addr;
        memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_family = AF_INET;
        server_addr.sin_addr.s_addr = inet_addr(remoteAddress); // 服务器IP
        server_addr.sin_port = htons(remotePort);                  // 服务器端口

        if (connect(client_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
            throw cRuntimeError("连接服务器失败");
            close(client_fd);
        }

        EV_INFO << "连接服务器成功" << endl;

        cModule *node = findContainingNode(this);
        NodeStatus *nodeStatus = node ? check_and_cast_nullable<NodeStatus *>(node->getSubmodule("status")) : nullptr;
        bool isOperational = (!nodeStatus) || nodeStatus->getState() == NodeStatus::UP;
        if (!isOperational)
            throw cRuntimeError("This module doesn't support starting in node DOWN state");
    }
}


void ModbusSlaveHILApp::sendBack(cMessage *msg)
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

void ModbusSlaveHILApp::handleMessage(cMessage *msg)
{
    if (msg->getKind() == TCP_I_PEER_CLOSED) {
        // we'll close too, but only after there's surely no message
        // pending to be sent back in this connection
        int connId = check_and_cast<Indication *>(msg)->getTag<SocketInd>()->getSocketId();
        delete msg;
        auto request = new Request("close", TCP_C_CLOSE);
        request->addTag<SocketReq>()->setSocketId(connId);
        sendBack(request);
    }
    else if (msg->getKind() == TCP_I_DATA || msg->getKind() == TCP_I_URGENT_DATA) {
        Packet *packet = check_and_cast<Packet *>(msg);
        int connId = packet->getTag<SocketInd>()->getSocketId();
        ChunkQueue& queue = socketQueue[connId];
        auto chunk = packet->peekDataAt(B(0), packet->getTotalLength());
        queue.push(chunk);
        emit(packetReceivedSignal, packet);

        while (queue.has<ModbusHeader>(b(-1))) {
            const auto& header = queue.pop<ModbusHeader>(b(-1));
            msgsRcvd++;
            bytesRcvd += B(header->getChunkLength()).get();

            // 验证协议标识（必须为0）
            if (header->getProtocolId() != 0) {
                EV_WARN << "Invalid Modbus protocol ID: " << header->getProtocolId() << endl;
                continue;
            }

            // 计算PDU长度（length = 从站ID(1字节) + PDU长度）
            uint16_t length = header->getLength();
            uint16_t pduLength = length - 1;

            // 验证PDU合法性（至少包含功能码1字节）
            if (pduLength < 1) {
                EV_WARN << "Invalid PDU length: " << pduLength << endl;
                continue;
            }

            // 检查PDU数据是否完整
            if (queue.getLength() < B(pduLength)) {
                // 数据不足，将头部放回队列
                queue.push(header);
                break;
            }

            // 提取PDU数据
            const auto& pduChunk = queue.pop<BytesChunk>(B(pduLength));
            const std::vector<uint8_t> pduData = pduChunk->getBytes();

            bytesRcvd += B(pduChunk->getChunkLength()).get();


            // 获取序列化器注册表
            ChunkSerializerRegistry& registry = ChunkSerializerRegistry::getInstance();

            // 获取ModbusHeader和BytesChunk的序列化器
            const ChunkSerializer* baseHeaderSerializer = registry.getSerializer(typeid(ModbusHeader));
            const ModbusHeaderSerializer* headerSerializer = dynamic_cast<const ModbusHeaderSerializer*>(baseHeaderSerializer);
            const ChunkSerializer* bytesSerializer = registry.getSerializer(typeid(BytesChunk));

            if (!headerSerializer || !bytesSerializer) {
                EV_ERROR << "未找到合适的序列化器" << endl;
                continue;
            }

            try {
                // 1. 序列化ModbusHeader
                MemoryOutputStream headerStream;
                headerSerializer->serialize(headerStream, header);
                std::vector<uint8_t> headerBytes = headerStream.getData();

                // 2. 序列化PDU数据
                MemoryOutputStream pduStream;
                bytesSerializer->serialize(pduStream, pduChunk, b(0), pduChunk->getChunkLength());
                std::vector<uint8_t> pduBytes = pduStream.getData();

                // 3. 组合完整报文并发送
                std::vector<uint8_t> sendData;
                sendData.insert(sendData.end(), headerBytes.begin(), headerBytes.end());
                sendData.insert(sendData.end(), pduBytes.begin(), pduBytes.end());

                ssize_t sent = ::send(client_fd, sendData.data(), sendData.size(), 0);
                if (sent <= 0) {
                    EV_ERROR << "发送数据失败: " << strerror(errno) << endl;
                    close(client_fd);
                    continue;
                }
                EV_INFO << "已发送 " << sent << " 字节到远程设备" << endl;

                // 4. 接收远程设备响应
                uint8_t recvBuf[2048];
                ssize_t recvLen = recv(client_fd, recvBuf, sizeof(recvBuf), 0);
                if (recvLen <= 0) {
                    EV_ERROR << "接收响应失败: " << strerror(errno) << endl;
                    close(client_fd);
                    continue;
                }
                EV_INFO << "收到 " << recvLen << " 字节响应" << endl;

                // 5. 反序列化响应数据
                MemoryInputStream recvStream(recvBuf, B(recvLen));

                // 反序列化ModbusHeader
                const Ptr<Chunk> headerChunk = headerSerializer->deserialize(recvStream);
                auto responseHeader = dynamicPtrCast<ModbusHeader>(headerChunk);
                if (!responseHeader) {
                    EV_ERROR << "反序列化响应头部失败" << endl;
                    continue;
                }

                // 反序列化响应PDU
                b remainingLength = B(recvLen) - responseHeader->getChunkLength();
                const Ptr<Chunk> pduResponseChunk = bytesSerializer->deserialize(recvStream, typeid(BytesChunk));
                auto responsePdu = dynamicPtrCast<BytesChunk>(pduResponseChunk);
                if (!responsePdu) {
                    EV_ERROR << "反序列化响应PDU失败" << endl;
                    continue;
                }

                // 6. 构建响应包并返回
                Packet *responsePacket = new Packet("ModbusResponse", TCP_C_SEND);
                responsePacket->addTag<SocketReq>()->setSocketId(connId);
                responsePacket->insertAtBack(responseHeader);
                responsePacket->insertAtBack(responsePdu);

                // 设置socket标识，确保正确路由
                sendBack(responsePacket);

            } catch (const std::exception& e) {
                EV_ERROR << "处理报文时发生错误: " << e.what() << endl;
                close(client_fd);
            }
        }
        delete msg;
    }
    else if (msg->getKind() == TCP_I_AVAILABLE)
        socket.processMessage(msg);
    else {
        // some indication -- ignore
        EV_WARN << "drop msg: " << msg->getName() << ", kind:" << msg->getKind() << "(" << cEnum::get("inet::TcpStatusInd")->getStringFor(msg->getKind()) << ")\n";
        delete msg;
    }
}

void ModbusSlaveHILApp::refreshDisplay() const
{
    char buf[64];
    sprintf(buf, "rcvd: %ld pks %ld bytes\nsent: %ld pks %ld bytes", msgsRcvd, bytesRcvd, msgsSent, bytesSent);
    getDisplayString().setTagArg("t", 0, buf);
}

void ModbusSlaveHILApp::finish()
{
    EV_INFO << getFullPath() << ": sent " << bytesSent << " bytes in " << msgsSent << " packets\n";
    EV_INFO << getFullPath() << ": received " << bytesRcvd << " bytes in " << msgsRcvd << " packets\n";
}

} // namespace inet





