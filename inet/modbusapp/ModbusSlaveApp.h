/*
 * ModbusSlaveApp.h
 *
 *  Created on: Oct 20, 2025
 *      Author: llw
 */

#ifndef INET_APPLICATIONS_TCPAPP_MODBUSSLAVEAPP_H_
#define INET_APPLICATIONS_TCPAPP_MODBUSSLAVEAPP_H_


#include "inet/common/lifecycle/LifecycleUnsupported.h"
#include "inet/common/packet/ChunkQueue.h"
#include "inet/transportlayer/contract/tcp/TcpSocket.h"
#include <nlohmann/json.hpp>
#include "ModbusHeader_m.h"
#include "ModbusStorage.h"

using json = nlohmann::json;

namespace inet {

class INET_API ModbusSlaveApp : public cSimpleModule, public LifecycleUnsupported
{
  protected:
    TcpSocket socket;
    ModbusStorage modbusStorage;  // 存储Modbus从站配置
    std::string slavesConfigPath;   // JSON配置文件路径
    L3Address localAddress;       // 本地IP地址
    int localPort = 502;          // Modbus默认端口

    // 统计信息
    long requestsRcvd = 0;
    long responsesSent = 0;
    long bytesRcvd = 0;
    long bytesSent = 0;

    std::map<int, ChunkQueue> socketQueue;  // 按连接ID管理数据队列

  protected:
    virtual void initialize(int stage) override;
    virtual int numInitStages() const override { return NUM_INIT_STAGES; }
    virtual void handleMessage(cMessage *msg) override;
    virtual void finish() override;
    virtual void refreshDisplay() const override;

    // 配置加载相关方法
    virtual void loadConfigFromJson();
    virtual void parseJsonConfig(const json& j);
    template <typename ElementType>
    void loadRegisterGroups(const json& jsonGroups, RegisterGroup<ElementType>*& groups, int count);
    virtual bool matchLocalAddress(const std::string& configIp);

    // Modbus消息处理核心方法
    virtual void processModbusRequest(const Ptr<const ModbusHeader>& requestHeader, const uint8_t* pduData, uint16_t pduLength, int connId);
    virtual void sendModbusResponse(const Ptr<const ModbusHeader>& requestHeader, const std::vector<uint8_t>& responsePdu, int connId);
    virtual void sendExceptionResponse(const Ptr<const ModbusHeader>& requestHeader, uint8_t functionCode, uint8_t exceptionCode, int connId);

    // 辅助查找方法
    virtual MSMapping* findSlave(uint8_t slaveId);
    template <typename ElementType>
    RegisterGroup<ElementType>* findRegisterGroup(RegisterGroup<ElementType>* groups, int numGroups, uint16_t startAddress, uint16_t quantity);

    // 功能码处理方法
    virtual void handleReadCoils(const Ptr<const ModbusHeader>& requestHeader, const uint8_t* pduData, uint16_t pduLength, int connId);
    virtual void handleReadDiscreteInputs(const Ptr<const ModbusHeader>& requestHeader, const uint8_t* pduData, uint16_t pduLength, int connId);
    virtual void handleReadHoldingRegisters(const Ptr<const ModbusHeader>& requestHeader, const uint8_t* pduData, uint16_t pduLength, int connId);
    virtual void handleReadInputRegisters(const Ptr<const ModbusHeader>& requestHeader, const uint8_t* pduData, uint16_t pduLength, int connId);
    virtual void handleWriteSingleCoil(const Ptr<const ModbusHeader>& requestHeader, const uint8_t* pduData, uint16_t pduLength, int connId);
    virtual void handleWriteSingleRegister(const Ptr<const ModbusHeader>& requestHeader, const uint8_t* pduData, uint16_t pduLength, int connId);
    virtual void handleWriteMultipleCoils(const Ptr<const ModbusHeader>& requestHeader, const uint8_t* pduData, uint16_t pduLength, int connId);
    virtual void handleWriteMultipleRegisters(const Ptr<const ModbusHeader>& requestHeader, const uint8_t* pduData, uint16_t pduLength, int connId);
    virtual void handleReadWriteMultipleRegisters(const Ptr<const ModbusHeader>& requestHeader, const uint8_t* pduData, uint16_t pduLength, int connId);
};

} // namespace inet

#endif /* INET_APPLICATIONS_TCPAPP_MODBUSSLAVEAPP_H_ */