/*
 * OperatorStationApp2.h
 *
 *  Created on: Oct 18, 2025
 *      Author: llw
 */

#ifndef INET_APPLICATIONS_TCPAPP_OPERATORSTATIONAPP2_H_
#define INET_APPLICATIONS_TCPAPP_OPERATORSTATIONAPP2_H_


#include "ModbusStorage.h"
#include "OperatorRequest_m.h"  // 引入OperatorRequest定义
#include "inet/applications/tcpapp/TcpAppBase.h"
#include "inet/common/lifecycle/ILifecycle.h"
#include "inet/common/lifecycle/NodeStatus.h"

namespace inet{
    class INET_API OperatorStationApp2 : public TcpAppBase
    {
        protected:
          ModbusStorage* storage = new ModbusStorage;
          cMessage *timeoutMsg = nullptr;
          bool earlySend = false; // if true, don't wait with sendRequest() until established()
          uint32_t sequenceNumber = 0;
          std::string modbusRequest;  // 存储modbus请求字符串
          simtime_t sendTime;         // 发送OperatorRequest的时间

          // 解析modbusRequest得到的参数
          std::string targetHostName;
          uint8_t targetSlaveId = 0;
          uint8_t functionCode = 0;
          uint16_t targetStartAddress = 0;
          uint16_t number = 0;
          std::vector<uint8_t> data;
          uint16_t transactionId = 0;  // Modbus事务ID

          virtual void sendModbusRequest();  // 新增：发送OperatorRequest报文
          static std::vector<std::string> splitBySpace(const std::string& str);
          bool isHexChar(char c);
          uint8_t hexStringToUint8_t(std::string hexString);
          uint16_t hexStringToUint16_t(std::string hexString);  // 新增：解析16位十六进制

          virtual int numInitStages() const override { return NUM_INIT_STAGES; }
          virtual void initialize(int stage) override;
          virtual void handleTimer(cMessage *msg) override;

          virtual void socketEstablished(TcpSocket *socket) override;
          virtual void socketDataArrived(TcpSocket *socket, Packet *msg, bool urgent) override;
          virtual void socketClosed(TcpSocket *socket) override;
          virtual void socketFailure(TcpSocket *socket, int code) override;

          virtual void handleStartOperation(LifecycleOperation *operation) override;
          virtual void handleStopOperation(LifecycleOperation *operation) override;
          virtual void handleCrashOperation(LifecycleOperation *operation) override;

          virtual void close() override;

        public:
          OperatorStationApp2() {}
          virtual ~OperatorStationApp2();


    };

}





#endif /* INET_APPLICATIONS_TCPAPP_OPERATORSTATIONAPP2_H_ */
