/*
 * OperatorStationApp.cc
 *
 *  Created on: Sep 20, 2025
 *      Author: llw
 */

#include "OperatorStationApp.h"


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
    #define MSGKIND_SEND       1  // 定时器触发：发送List报文

    Define_Module(OperatorStationApp);

    OperatorStationApp::~OperatorStationApp()
    {
        // only cancel/delete self messages here; do NOT call socket.destroy() in the destructor
        cancelAndDelete(timeoutMsg);
        timeoutMsg = nullptr;
    }

    void OperatorStationApp::initialize(int stage)
    {
        TcpAppBase::initialize(stage);
        if (stage == INITSTAGE_LOCAL) {
            earlySend = false;

            // 读取基础时间参数
            startTime = par("startTime");
            stopTime = par("stopTime");
            interval = par("interval");

            // 时间参数校验
            if (stopTime >= SIMTIME_ZERO && stopTime < startTime)
                throw cRuntimeError("停止时间不能小于启动时间");

            timeoutMsg = new cMessage("timer");
        }
    }

    void OperatorStationApp::handleStartOperation(LifecycleOperation *operation)
    {
        simtime_t now = simTime();
        simtime_t start = std::max(startTime, now);

        // 调度首次连接事件
        if (timeoutMsg && ((stopTime < SIMTIME_ZERO) || (start < stopTime) || (start == stopTime && startTime == stopTime))) {
            timeoutMsg->setKind(MSGKIND_CONNECT);
            scheduleAt(start, timeoutMsg);
        }
    }

    void OperatorStationApp::handleStopOperation(LifecycleOperation *operation)
    {
        cancelEvent(timeoutMsg);
        if (socket.getState() == TcpSocket::CONNECTED || socket.getState() == TcpSocket::CONNECTING || socket.getState() == TcpSocket::PEER_CLOSED)
            close();
    }

    void OperatorStationApp::handleCrashOperation(LifecycleOperation *operation)
    {
        cancelEvent(timeoutMsg);
        if (operation->getRootModule() != getContainingNode(this))
            socket.destroy();
    }

    void OperatorStationApp::sendRequest()
    {
        const auto& header = makeShared<ListMsg>();
        Packet *packet = new Packet("data");
        header->setSequenceNumber(++sequenceNumber);
        header->setIfList(true);
        // Tag both the chunk and the Packet to maximize compatibility with dataAge(packetReceived)
        header->addTag<CreationTimeTag>()->setCreationTime(simTime());
        packet->addTag<CreationTimeTag>()->setCreationTime(simTime());
        packet->insertAtFront(header);
        // Only send if socket is fully connected. Avoid sending while CONNECTING or PEER_CLOSED
        // because queued messages may remain undisposed if connection never establishes.
        if (socket.getState() == TcpSocket::CONNECTED) {
            EV_INFO << "Sending data packet, id=" << packet->getId() << ", socket state=" << socket.getState() << "\n";
            try {
                sendPacket(packet);
            }
            catch (const cRuntimeError &e) {
                EV_ERROR << "sendPacket threw: " << e.what() << ", deleting packet to avoid leak" << endl;
                delete packet;
                throw;
            }
            catch (...) {
                EV_ERROR << "sendPacket threw unknown exception, deleting packet to avoid leak" << endl;
                delete packet;
                throw;
            }
        }
        else {
            EV_WARN << "Socket not CONNECTED (state=" << socket.getState() << "); dropping packet to avoid leak" << endl;
            delete packet;
        }
    }


    void OperatorStationApp::handleTimer(cMessage *msg)
    {
        switch (msg->getKind()) {
            case MSGKIND_CONNECT:
                connect();
                if (earlySend)
                    sendRequest();
                break;
            case MSGKIND_SEND:
                sendRequest();
                rescheduleAfterOrDeleteTimer(interval, MSGKIND_SEND);
                break;
            default:
                throw cRuntimeError("未知定时器类型: kind=%d", msg->getKind());
        }
    }

    void OperatorStationApp::socketEstablished(TcpSocket *socket)
    {
        TcpAppBase::socketEstablished(socket);

        if (!earlySend) {
            sendRequest();
            rescheduleAfterOrDeleteTimer(interval, MSGKIND_SEND);
        }
    }

    void OperatorStationApp::rescheduleAfterOrDeleteTimer(simtime_t d, short int msgKind)
    {
        if (stopTime < SIMTIME_ZERO || simTime() + d < stopTime) {
            timeoutMsg->setKind(msgKind);
            rescheduleAfter(d, timeoutMsg);
        }
        else {
            cancelAndDelete(timeoutMsg);
            timeoutMsg = nullptr;
        }
    }

    void OperatorStationApp::socketDataArrived(TcpSocket *socket, Packet *msg, bool urgent)
    {
        auto data = msg->peekDataAsBytes();
        const auto &bytes = data->getBytes();
        // 累积到接收缓冲区（处理分片/粘包）
        recvBuffer.insert(recvBuffer.end(), bytes.begin(), bytes.end());

        size_t consumed = 0;
        int messagesParsed = 0;
        // 可能存在多个消息连续到达，循环解析
        while (storage->tryDeserializeModbusStorage(storage, recvBuffer, consumed)) {
            messagesParsed++;
            // 移除已消费字节
            recvBuffer.erase(recvBuffer.begin(), recvBuffer.begin() + consumed);
            EV_INFO << "成功反序列化ModbusStorage消息，长度=" << consumed
                    << ", 剩余未处理字节=" << recvBuffer.size() << endl;
        }
        if (messagesParsed == 0) {
            EV_DEBUG << "当前缓冲区字节数=" << recvBuffer.size()
                     << "，尚未形成完整ModbusStorage消息，继续等待后续分片" << endl;
        }

        // Debug: check CreationTimeTag presence for endToEndDelay calculation
        if (auto ct = msg->findTag<CreationTimeTag>()) {
            EV_INFO << "[E2E DEBUG] Received packet creationTime=" << ct->getCreationTime() << ", age=" << (simTime() - ct->getCreationTime()) << endl;
        }
        else {
            EV_WARN << "[E2E DEBUG] Received packet WITHOUT CreationTimeTag" << endl;
        }
        TcpAppBase::socketDataArrived(socket, msg, urgent);
        EV_INFO << "收到服务器回复报文 (messagesParsed=" << messagesParsed << ")" << endl;
    }

    void OperatorStationApp::close()
    {
        TcpAppBase::close();
        cancelEvent(timeoutMsg);
    }

    void OperatorStationApp::socketClosed(TcpSocket *socket)
    {
        TcpAppBase::socketClosed(socket);
    }

    void OperatorStationApp::socketFailure(TcpSocket *socket, int code)
    {
        TcpAppBase::socketFailure(socket, code);
        if (timeoutMsg) {
            simtime_t d = par("reconnectInterval");
            rescheduleAfterOrDeleteTimer(d, MSGKIND_CONNECT);
        }
    }

    void OperatorStationApp::finish(){
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

        // save storage to json
        auto nodeName = this->getParentModule()->getFullName();
        std::string filePath = std::string("results/") + nodeName + std::string(".json");
        storage->saveToJson(filePath);

        // call base finish for stats
        TcpAppBase::finish();
    }

}