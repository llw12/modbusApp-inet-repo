/*
 * ModbusSlaveHILApp.h
 *
 *  Created on: Oct 26, 2025
 *      Author: llw
 */

#ifndef INET_APPLICATIONS_MODBUSAPP_MODBUSSLAVEHILAPP_H_
#define INET_APPLICATIONS_MODBUSAPP_MODBUSSLAVEHILAPP_H_

#include "inet/common/lifecycle/LifecycleUnsupported.h"
#include "inet/common/packet/ChunkQueue.h"
#include "inet/transportlayer/contract/tcp/TcpSocket.h"

namespace inet {

/**
 * Generic server application. It serves requests coming in GenericAppMsg
 * request messages. Clients are usually subclassed from TcpAppBase.
 *
 * @see GenericAppMsg, TcpAppBase
 */
class INET_API ModbusSlaveHILApp : public cSimpleModule, public LifecycleUnsupported
{
  protected:

    int client_fd;
    TcpSocket socket;

    long msgsRcvd;
    long msgsSent;
    long bytesRcvd;
    long bytesSent;

    std::map<int, ChunkQueue> socketQueue;

  protected:
    virtual void sendBack(cMessage *msg);

    virtual void initialize(int stage) override;
    virtual int numInitStages() const override { return NUM_INIT_STAGES; }
    virtual void handleMessage(cMessage *msg) override;
    virtual void finish() override;
    virtual void refreshDisplay() const override;
};

} // namespace inet







#endif /* INET_APPLICATIONS_MODBUSAPP_MODBUSSLAVEHILAPP_H_ */
