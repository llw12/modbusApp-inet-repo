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
#include <random>

namespace inet{
    class INET_API OperatorStationApp2 : public TcpAppBase
    {
        protected:
          ModbusStorage* storage = new ModbusStorage;
          cMessage *connectMsg = nullptr;  // 负责连接的定时器
          cMessage *sendMsg = nullptr;     // 负责发送命令的定时器
          bool earlySend = false; // if true, don't wait with sendRequest() until established()
          uint32_t sequenceNumber = 0;
          std::string modbusRequest;  // 原始modbus请求字符串（可能包含多条，以;分隔）
          std::string sendTimeStr;    // 原始sendTime字符串（空格分隔）

          // 可重复性的随机数引擎
          uint64_t seed = 0;
          std::mt19937_64 rng;

          // 多条命令的结构与队列
          struct ModbusCommand {
              std::string targetHostName;
              uint8_t slaveId = 0;
              uint8_t functionCode = 0;
              uint16_t startAddress = 0;
              uint16_t quantity = 0;
              std::vector<uint8_t> data;
          };

          std::vector<ModbusCommand> commands;   // 解析后的命令列表
          std::vector<simtime_t> sendTimes;       // 配置的发送时间（单调递增）
          std::vector<simtime_t> actualTimes;     // 实际发送时间 = sendTime + 随机扰动
          std::vector<size_t> scheduleOrder;      // 按实际发送时间排序后的索引
          size_t nextSendIdx = 0;                 // 下一个要发送的scheduleOrder索引

          uint16_t transactionId = 0;  // Modbus事务ID

          virtual void sendModbusRequest(size_t index);  // 发送指定索引的OperatorRequest报文
          static std::vector<std::string> splitBySpace(const std::string& str);
          bool isHexChar(char c);
          uint8_t hexStringToUint8_t(std::string hexString);
          uint16_t hexStringToUint16_t(std::string hexString);  // 解析16位十六进制

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
          virtual void finish() override;

        public:
          OperatorStationApp2() {}
          virtual ~OperatorStationApp2();


    };

}





#endif /* INET_APPLICATIONS_TCPAPP_OPERATORSTATIONAPP2_H_ */