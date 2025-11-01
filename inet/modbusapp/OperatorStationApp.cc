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
        cancelAndDelete(timeoutMsg);
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
        header->addTag<CreationTimeTag>()->setCreationTime(simTime());
        packet->insertAtFront(header);
        sendPacket(packet);
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
        storage->deserializeModbusStorage(storage, data->getBytes());
        TcpAppBase::socketDataArrived(socket, msg, urgent);
        EV_INFO << "收到服务器回复报文" << endl;
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
        auto nodeName = this->getParentModule()->getFullName();
        std::string filePath = std::string("results/") + nodeName + std::string(".json");
        storage->saveToJson(filePath);
    }

}
