

#ifndef INET_APPLICATIONS_MODBUSAPP_MODBUSHILBYIP_H_
#define INET_APPLICATIONS_MODBUSAPP_MODBUSHILBYIP_H_

#include "inet/common/lifecycle/LifecycleUnsupported.h"
#include "inet/common/packet/ChunkQueue.h"
#include "inet/transportlayer/contract/tcp/TcpSocket.h"

namespace inet {

class INET_API ModbusHILByIP : public cSimpleModule, public LifecycleUnsupported
{
  protected:
    TcpSocket serverSocket;     // listens for incoming Modbus/TCP
    TcpSocket hilSocket;        // connected to HIL device via IP/TCP

    long msgsRcvd = 0;
    long msgsSent = 0;
    long bytesRcvd = 0;
    long bytesSent = 0;

    // queue per inbound connection id
    std::map<int, ChunkQueue> socketQueue;
    // remember the last active inbound connection to route response back
    int lastInboundConnId = -1;

  protected:
    virtual void initialize(int stage) override;
    virtual int numInitStages() const override { return NUM_INIT_STAGES; }
    virtual void handleMessage(cMessage *msg) override;
    virtual void finish() override;
    virtual void refreshDisplay() const override;

    // helpers
    void sendToTcp(Packet *packet, int socketId);
};

} // namespace inet

#endif /* INET_APPLICATIONS_MODBUSAPP_MODBUSHILBYIP_H_ */
