

#include <iostream>
#include <cstring>

#include "ModbusHILByIP.h"
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

Define_Module(ModbusHILByIP);

void ModbusHILByIP::initialize(int stage)
{
    cSimpleModule::initialize(stage);

    if (stage == INITSTAGE_LOCAL) {
        msgsRcvd = msgsSent = bytesRcvd = bytesSent = 0;
        WATCH(msgsRcvd);
        WATCH(msgsSent);
        WATCH(bytesRcvd);
        WATCH(bytesSent);
    }
    else if (stage == INITSTAGE_APPLICATION_LAYER) {
        // inbound server socket
        const char *localAddress = par("localAddress");
        int localPort = par("localPort");
        serverSocket.setOutputGate(gate("socketOut"));
        serverSocket.bind(localAddress[0] ? L3AddressResolver().resolve(localAddress) : L3Address(), localPort);
        serverSocket.listen();

        // active client socket to HIL device
        const char *remoteAddress = par("remoteAddress");
        int remotePort = par("remotePort");
        hilSocket.setOutputGate(gate("socketOut"));
        hilSocket.connect(L3AddressResolver().resolve(remoteAddress), remotePort);

        cModule *node = findContainingNode(this);
        NodeStatus *nodeStatus = node ? check_and_cast_nullable<NodeStatus *>(node->getSubmodule("status")) : nullptr;
        bool isOperational = (!nodeStatus) || nodeStatus->getState() == NodeStatus::UP;
        if (!isOperational)
            throw cRuntimeError("This module doesn't support starting in node DOWN state");
    }
}

void ModbusHILByIP::sendToTcp(Packet *packet, int socketId)
{
    msgsSent++;
    bytesSent += packet->getByteLength();
    emit(packetSentSignal, packet);

    auto &tags = packet->getTags();
    tags.addTagIfAbsent<SocketReq>()->setSocketId(socketId);
    tags.addTagIfAbsent<DispatchProtocolReq>()->setProtocol(&Protocol::tcp);
    send(packet, "socketOut");
}

void ModbusHILByIP::handleMessage(cMessage *msg)
{
    if (msg->isSelfMessage()) {
        // no timers
        delete msg;
        return;
    }

    if (msg->getKind() == TCP_I_PEER_CLOSED) {
        int connId = check_and_cast<Indication *>(msg)->getTag<SocketInd>()->getSocketId();
        delete msg;
        auto request = new Request("close", TCP_C_CLOSE);
        request->addTag<SocketReq>()->setSocketId(connId);
        auto &tags = request->getTags();
        tags.addTagIfAbsent<DispatchProtocolReq>()->setProtocol(&Protocol::tcp);
        send(request, "socketOut");
        return;
    }

    if (msg->getKind() == TCP_I_AVAILABLE) {
        // let TcpSocket state machines consume it, if any
        // we don't use callbacks, still process data indications explicitly below
        serverSocket.processMessage(msg);
        hilSocket.processMessage(msg);
        return;
    }

    if (msg->getKind() == TCP_I_DATA || msg->getKind() == TCP_I_URGENT_DATA) {
        Packet *packet = check_and_cast<Packet *>(msg);
        int connId = packet->getTag<SocketInd>()->getSocketId();
        emit(packetReceivedSignal, packet);

        // determine source: inbound client or HIL
        int hilConnId = hilSocket.getSocketId();
        if (connId != hilConnId) {
            // inbound from client -> forward to HIL
            lastInboundConnId = connId;
            ChunkQueue &queue = socketQueue[connId];
            auto chunk = packet->peekDataAt(B(0), packet->getTotalLength());
            queue.push(chunk);

            while (queue.has<ModbusHeader>(b(-1))) {
                const auto &header = queue.pop<ModbusHeader>(b(-1));
                msgsRcvd++;
                bytesRcvd += B(header->getChunkLength()).get();

                if (header->getProtocolId() != 0) {
                    EV_WARN << "Invalid Modbus protocol ID: " << header->getProtocolId() << endl;
                    continue;
                }

                uint16_t length = header->getLength();
                uint16_t pduLength = length - 1;
                if (pduLength < 1) {
                    EV_WARN << "Invalid PDU length: " << pduLength << endl;
                    continue;
                }
                if (queue.getLength() < B(pduLength)) {
                    queue.push(header);
                    break;
                }

                const auto &pduChunk = queue.pop<BytesChunk>(B(pduLength));
                bytesRcvd += B(pduChunk->getChunkLength()).get();

                // serialize header + pdu into a BytesChunk to send via TCP
                ChunkSerializerRegistry &registry = ChunkSerializerRegistry::getInstance();
                const ChunkSerializer *baseHeaderSerializer = registry.getSerializer(typeid(ModbusHeader));
                const ModbusHeaderSerializer *headerSerializer = dynamic_cast<const ModbusHeaderSerializer *>(baseHeaderSerializer);
                const ChunkSerializer *bytesSerializer = registry.getSerializer(typeid(BytesChunk));
                if (!headerSerializer || !bytesSerializer) {
                    EV_ERROR << "Serializer not found" << endl;
                    continue;
                }
                try {
                    MemoryOutputStream headerStream;
                    headerSerializer->serialize(headerStream, header);
                    std::vector<uint8_t> headerBytes = headerStream.getData();

                    MemoryOutputStream pduStream;
                    bytesSerializer->serialize(pduStream, pduChunk, b(0), pduChunk->getChunkLength());
                    std::vector<uint8_t> pduBytes = pduStream.getData();

                    auto payload = makeShared<BytesChunk>();
                    std::vector<uint8_t> sendData;
                    sendData.reserve(headerBytes.size() + pduBytes.size());
                    sendData.insert(sendData.end(), headerBytes.begin(), headerBytes.end());
                    sendData.insert(sendData.end(), pduBytes.begin(), pduBytes.end());
                    payload->setBytes(sendData);
                    //payload->addTag<CreationTimeTag>()->setCreationTime(simTime());

                    Packet *outPacket = new Packet("ModbusHILForward", TCP_C_SEND);
                    outPacket->insertAtBack(payload);
                    outPacket->addTag<CreationTimeTag>()->setCreationTime(simTime());

                    sendToTcp(outPacket, hilConnId);
                } catch (const std::exception &e) {
                    EV_ERROR << "Error serializing forward payload: " << e.what() << endl;
                }
            }
            delete packet;
            return;
        }
        else {
            // data from HIL -> relay back to last inbound connection
            ChunkQueue &queue = socketQueue[connId];
            auto chunk = packet->peekDataAt(B(0), packet->getTotalLength());
            queue.push(chunk);

            // Expect a full Modbus response (header + pdu)
            while (queue.has<ModbusHeader>(b(-1))) {
                auto respHeader = queue.pop<ModbusHeader>(b(-1));
                msgsRcvd++;
                bytesRcvd += B(respHeader->getChunkLength()).get();

                uint16_t length = respHeader->getLength();
                uint16_t pduLength = length - 1;
                if (pduLength < 1) {
                    EV_WARN << "Invalid response PDU length: " << pduLength << endl;
                    continue;
                }
                if (queue.getLength() < B(pduLength)) {
                    queue.push(respHeader);
                    break;
                }
                auto respPdu = queue.pop<BytesChunk>(B(pduLength));
                bytesRcvd += B(respPdu->getChunkLength()).get();

                // build packet to send back
                Packet *responsePacket = new Packet("ModbusHILResponse", TCP_C_SEND);
                responsePacket->insertAtBack(respHeader);
                responsePacket->insertAtBack(respPdu);
                responsePacket->addTag<CreationTimeTag>()->setCreationTime(simTime());

                if (lastInboundConnId >= 0)
                    sendToTcp(responsePacket, lastInboundConnId);
                else {
                    EV_WARN << "No inbound connection to send response to" << endl;
                    delete responsePacket;
                }
            }
            delete packet;
            return;
        }
    }

    // other indications: drop
    EV_WARN << "drop msg: " << msg->getName() << ", kind:" << msg->getKind() << "(" << cEnum::get("inet::TcpStatusInd")->getStringFor(msg->getKind()) << ")\n";
    delete msg;
}

void ModbusHILByIP::refreshDisplay() const
{
    char buf[64];
    sprintf(buf, "rcvd: %ld pks %ld bytes\nsent: %ld pks %ld bytes", msgsRcvd, bytesRcvd, msgsSent, bytesSent);
    getDisplayString().setTagArg("t", 0, buf);
}

void ModbusHILByIP::finish()
{
    EV_INFO << getFullPath() << ": sent " << bytesSent << " bytes in " << msgsSent << " packets\n";
    EV_INFO << getFullPath() << ": received " << bytesRcvd << " bytes in " << msgsRcvd << " packets\n";
}

} // namespace inet
