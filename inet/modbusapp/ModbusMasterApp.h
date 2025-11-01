#ifndef MODBUSMASTERAPP_H_
#define MODBUSMASTERAPP_H_

#include "ModbusHeader_m.h"
#include "ModbusStorage.h"
#include "ModbusTcpAppBase.h"
#include "inet/applications/tcpapp/TcpBasicClientApp.h"

namespace inet {

class ModbusMasterApp : public ModbusTcpAppBase {

protected:
    cMessage *readTimer = nullptr;  // 定时读取定时器

    cMessage *sendNextTimer = nullptr;
    simtime_t readInterval;         // 读取间隔时间
    int transactionId = 1;          // 事务ID计数器
    int connectIndex = 0;
    uint16_t pretransactionId = 0;


    std::map<int, ChunkQueue> socketQueue;
    std::map<int, ChunkQueue> sendSocketQueue;

protected:
    virtual int numInitStages() const override { return NUM_INIT_STAGES; }
    virtual void initialize(int stage) override;
    virtual void handleTimer(cMessage *msg) override;
    virtual void socketDataArrived(TcpSocket *socket, Packet *msg, bool urgent) override;
    // 重写连接建立回调
    virtual void socketEstablished(TcpSocket *socket) override;



    virtual void handleStartOperation(LifecycleOperation *operation) override;
    virtual void handleStopOperation(LifecycleOperation *operation) override;
    virtual void handleCrashOperation(LifecycleOperation *operation) override;
    void generateQueryPacket(std::map<int, ChunkQueue>& sendSocketQueue);


public:
    ModbusMasterApp() {}
    virtual ~ModbusMasterApp() { cancelAndDelete(readTimer); }

    // 生成读请求报文
    virtual Packet* createRequest(uint8_t slaveId, uint8_t functionCode,
                                     uint16_t startAddress, uint16_t quantity);
    virtual Packet* createRequest(uint8_t slaveId, uint8_t functionCode,
                                     uint16_t startAddress, uint16_t quantity, const std::vector<uint8_t>& data);
    virtual void addPacketToQueue(Packet* pkt, int socketId);
    // 解析收到的响应报文并存储
    virtual void parseAndStoreResponse(TcpSocket *socket, const inet::Ptr<const BytesChunk>& requestPdu, const inet::Ptr<const ModbusHeader>& responseHeader, const inet::Ptr<const BytesChunk>& responsePdu);


};

} // namespace inet

#endif // MODBUSMASTERAPP_H_
