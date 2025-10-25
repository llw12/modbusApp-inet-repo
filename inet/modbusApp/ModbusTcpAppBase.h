//
// Copyright (C) 2004 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


#ifndef __INET_MODBUSTCPAPPBASE_H
#define __INET_MODBUSTCPAPPBASE_H

#include "inet/applications/base/ApplicationBase.h"
#include "inet/transportlayer/contract/tcp/TcpSocket.h"
#include "inet/common/socket/SocketMap.h"
#include "ModbusStorage.h"
#include <vector>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace inet {

class INET_API ModbusTcpAppBase : public ApplicationBase, public TcpSocket::ICallback
{
  protected:
    SocketMap socketMap;
    std::vector<L3Address> destinationAddresses;
    int serverPort = 502; // Modbus TCP standard port
    ModbusStorage modbusStorage;  // Modbus存储实例（protected 成员）
    const char *configFileName;   // 配置文件名称

    // statistics
    int numSessions = 0;
    int numBroken = 0;
    int packetsSent = 0;
    int packetsRcvd = 0;
    int bytesSent = 0;
    int bytesRcvd = 0;

    std::map<int, ChunkQueue> waitProcessPacketSocketQueue;

    // statistics:
    static simsignal_t connectSignal;

  protected:
    virtual void initialize(int stage) override;
    virtual int numInitStages() const override { return NUM_INIT_STAGES; }
    virtual void handleMessageWhenUp(cMessage *msg) override;
    virtual void finish() override;
    virtual void refreshDisplay() const override;



    virtual void handleTimer(cMessage *msg);

    /* TcpSocket::ICallback callback methods */
    virtual void socketDataArrived(TcpSocket *socket, Packet *msg, bool urgent) override;
    virtual void socketAvailable(TcpSocket *socket, TcpAvailableInfo *availableInfo) override { socket->accept(availableInfo->getNewSocketId()); }
    virtual void socketEstablished(TcpSocket *socket) override;
    virtual void socketPeerClosed(TcpSocket *socket) override;
    virtual void socketClosed(TcpSocket *socket) override;
    virtual void socketFailure(TcpSocket *socket, int code) override;
    virtual void socketStatusArrived(TcpSocket *socket, TcpStatusInfo *status) override {}
    virtual void socketDeleted(TcpSocket *socket) override;

  public:
    // --------------------------
    // 新增：获取 ModbusStorage 实例的公共方法
    // --------------------------
    /**
     * 非 const 版本：返回 ModbusStorage 引用，支持修改存储内容
     * 适用场景：需要修改从站配置、寄存器数据等
     */
    virtual ModbusStorage& getModbusStorage() { return modbusStorage; }

    /**
     * const 版本：返回 const ModbusStorage 引用，仅支持只读访问
     * 适用场景：仅查询存储内容（如统计从站数量、读取寄存器配置），避免意外修改
     */
    virtual const ModbusStorage& getModbusStorage() const { return modbusStorage; }

    virtual SocketMap& getSocketMap() { return socketMap; }

    /* Utility functions */
    virtual void connectAll();
    virtual TcpSocket* connectTo(const L3Address& address);
    virtual void closeAll();
    virtual void closeSocket(TcpSocket *socket);
    virtual void sendPacket(Packet *pkt, TcpSocket *socket);
    static std::vector<std::string> splitBySpace(const std::string& str);

    // 新增：解析JSON配置文件
    virtual void parseConfigFile();
    // 新增：从JSON对象加载寄存器组数据
    template <typename ElementType>
    void loadRegisterGroups(const json& jsonGroups, RegisterGroup<ElementType>*& groups, int count);
};


} // namespace inet

#endif
