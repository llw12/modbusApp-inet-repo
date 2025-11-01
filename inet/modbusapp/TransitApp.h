//
// Copyright (C) 2025 llw
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_TRANSITAPP_H
#define __INET_TRANSITAPP_H

#include "ModbusStorage.h"
#include "inet/common/lifecycle/LifecycleUnsupported.h"
#include "inet/common/packet/ChunkQueue.h"
#include "inet/transportlayer/contract/tcp/TcpSocket.h"

namespace inet {

class INET_API TransitApp : public cSimpleModule, public LifecycleUnsupported
{
  protected:
    TcpSocket socket;
    simtime_t delay;
    simtime_t maxMsgDelay;
    int remoteSocketId;

    long msgsRcvd;
    long msgsSent;
    long bytesRcvd;
    long bytesSent;

    std::map<int, ChunkQueue> socketQueue;
    ChunkQueue* transitQueue = new ChunkQueue();

  protected:

    virtual void sendOrSchedule(cMessage *msg, simtime_t delay);

    virtual void initialize(int stage) override;
    virtual int numInitStages() const override { return NUM_INIT_STAGES; }
    virtual void handleMessage(cMessage *msg) override;
    virtual void finish() override;
    virtual void refreshDisplay() const override;

  public:
    ChunkQueue* getTransitQueue(){return transitQueue;}
    void sendBack(cMessage *msg);
//    int getSocketId(){return socket.getSocketId();}

};

} // namespace inet

#endif
