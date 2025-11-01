//
// Copyright (C) 2004 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


#include "ModbusTcpAppBase.h"
#include "inet/networklayer/common/L3AddressResolver.h"
#include "inet/transportlayer/contract/tcp/TcpSocket.h"
#include "inet/common/INETUtils.h"
#include <fstream>
#include <stdexcept>

namespace inet {

simsignal_t ModbusTcpAppBase::connectSignal = registerSignal("connect");

void ModbusTcpAppBase::initialize(int stage)
{
    ApplicationBase::initialize(stage);

    if (stage == INITSTAGE_LOCAL) {
        WATCH(numSessions);
        WATCH(numBroken);
        WATCH(packetsSent);
        WATCH(packetsRcvd);
        WATCH(bytesSent);
        WATCH(bytesRcvd);

        // 获取配置文件名称和连接数量
        configFileName = par("configFile");
        int numConnect = par("numConnect");
        modbusStorage.setNumConnect(numConnect);
    }
    else if (stage == INITSTAGE_APPLICATION_LAYER) {
        // 解析配置文件
        parseConfigFile();

        // Connect to all destinations
        connectAll();
    }
}

void ModbusTcpAppBase::parseConfigFile()
{
    std::ifstream file(configFileName);
    if (!file.is_open()) {
        throw std::runtime_error("Could not open config file");
    }

    json j;
    file >> j;

    auto& connectArray = modbusStorage.getConnectArray();
    int connectIndex = 0;

    for (const auto& jsonConnect : j["connectArray"]) {
        if (connectIndex >= modbusStorage.getNumConnect()) {
            EV_WARN << "Config file has more connections than specified in numConnect parameter" << endl;
            break;
        }

        // 解析IP地址并转换为L3Address
        std::string ipStr = jsonConnect["ipAddress"];
        if(!connectArray[connectIndex].ipAddress.tryParse(ipStr.c_str())){
            throw cRuntimeError("ipAddress Parse failed");
        }

        int numSlave = jsonConnect["numSlave"];
        connectArray[connectIndex].numSlave = numSlave;
        connectArray[connectIndex].slaves = new MSMapping[numSlave]();

        int slaveIndex = 0;
        for (const auto& jsonSlave : jsonConnect["slaves"]) {
            if (slaveIndex >= numSlave) break;

            MSMapping& slave = connectArray[connectIndex].slaves[slaveIndex];
            slave.slaveId = jsonSlave["slaveId"];
            slave.numBitGroup = jsonSlave["numBitGroup"];
            slave.numInputBitGroup = jsonSlave["numInputBitGroup"];
            slave.numRegisterGroup = jsonSlave["numRegisterGroup"];
            slave.numInputRegisterGroup = jsonSlave["numInputRegisterGroup"];

            // 加载线圈组
            loadRegisterGroups<uint8_t>(jsonSlave["bitGroup"], slave.bitGroup, slave.numBitGroup);
            // 加载离散输入组
            loadRegisterGroups<uint8_t>(jsonSlave["inputBitGroup"], slave.inputBitGroup, slave.numInputBitGroup);
            // 加载保持寄存器组
            loadRegisterGroups<int16_t>(jsonSlave["registerGroup"], slave.registerGroup, slave.numRegisterGroup);
            // 加载输入寄存器组
            loadRegisterGroups<int16_t>(jsonSlave["inputRegisterGroup"], slave.inputRegisterGroup, slave.numInputRegisterGroup);

            slaveIndex++;
        }

        connectIndex++;
    }
}

template <typename ElementType>
void ModbusTcpAppBase::loadRegisterGroups(const json& jsonGroups, RegisterGroup<ElementType>*& groups, int count)
{
    if (count > 0 && !jsonGroups.empty()) {
        groups = new RegisterGroup<ElementType>[count];
        int groupIndex = 0;

        for (const auto& jsonGroup : jsonGroups) {
            if (groupIndex >= count) break;

            groups[groupIndex].startAddress = jsonGroup["startAddress"];
            groups[groupIndex].number = jsonGroup["number"];
            groups[groupIndex].data = new ElementType[groups[groupIndex].number];

            int dataIndex = 0;
            for (const auto& value : jsonGroup["data"]) {
                if (dataIndex >= groups[groupIndex].number) break;
                groups[groupIndex].data[dataIndex] = value.get<ElementType>();
                dataIndex++;
            }

            groupIndex++;
        }
    }
}

void ModbusTcpAppBase::handleMessageWhenUp(cMessage *msg)
{
    if (msg->isSelfMessage()) {
        handleTimer(msg);
    }
    else {
        // Find the corresponding socket for this message
        ISocket *socket = socketMap.findSocketFor(msg);
        if (!socket) {
            EV_WARN << "No socket found for message " << msg << ", deleting it" << endl;
            delete msg;
            return;
        }
        // Process message with the found socket
        check_and_cast<TcpSocket *>(socket)->processMessage(msg);
    }
}

void ModbusTcpAppBase::connectAll()
{
    const char *localAddress = par("localAddress");
    int localPort = par("localPort");

    for (size_t i = 0; i < modbusStorage.getConnectArray().size() ; i++) {
        const auto destAddr = modbusStorage.getConnect(i).ipAddress;
        // Create new socket for each destination
        TcpSocket *socket = new TcpSocket();
        socket->setCallback(this);
        socket->setOutputGate(gate("socketOut"));

        // Bind to local address/port if specified
        if (*localAddress || localPort != -1) {
            L3Address localAddr;
            if (*localAddress) {
                if(!L3AddressResolver().tryResolve(localAddress, localAddr)){
                    throw cRuntimeError("localAddress Resolve failed");
                }
            }
            socket->bind(localAddr, localPort);
        }

        // Set socket options
        int timeToLive = par("timeToLive");
        if (timeToLive != -1)
            socket->setTimeToLive(timeToLive);

        int dscp = par("dscp");
        if (dscp != -1)
            socket->setDscp(dscp);

        int tos = par("tos");
        if (tos != -1)
            socket->setTos(tos);

        // Connect to destination
        EV_INFO << "Connecting to " << destAddr << " port=" << serverPort << endl;
        socket->connect(destAddr, serverPort);

        // Add to socket map
        socketMap.addSocket(socket);
        numSessions++;

        // 设置ModbusStorage中的socketId和ipAddress
        if (i < modbusStorage.getNumConnect()) {
            auto& connect = modbusStorage.getConnect(i);
            connect.socketId = socket->getSocketId();
        }
    }
}

TcpSocket* ModbusTcpAppBase::connectTo(const L3Address& address)
{

    // 检查是否已存在到该地址的连接
    for (auto& elem : socketMap.getMap()) {
        TcpSocket* existingSocket = check_and_cast<TcpSocket*>(elem.second);
        if (existingSocket->getRemoteAddress() == address) {
            return existingSocket; // 返回已存在的连接
        }
    }
    // Create new socket
    TcpSocket *socket = new TcpSocket();
    socket->setCallback(this);
    socket->setOutputGate(gate("socketOut"));

    // Bind to local address/port if specified
    const char *localAddress = par("localAddress");
    int localPort = par("localPort");

    L3Address localAddr;
    if (*localAddress) {
        if(!L3AddressResolver().tryResolve(localAddress, localAddr)){
            throw cRuntimeError("localAddress Resolve failed");
        }
    }
    socket->bind(localAddr, localPort);

    // Set socket options
    int timeToLive = par("timeToLive");
    if (timeToLive != -1)
        socket->setTimeToLive(timeToLive);

    int dscp = par("dscp");
    if (dscp != -1)
        socket->setDscp(dscp);

    int tos = par("tos");
    if (tos != -1)
        socket->setTos(tos);

    // Connect to destination
    EV_INFO << "Connecting to " << address << " port=" << serverPort << endl;
    socket->connect(address, serverPort);

    // Add to socket map
    socketMap.addSocket(socket);
    numSessions++;
    emit(connectSignal, 1L);

    int index = modbusStorage.findConnectIndexByIpAddress(address);
    modbusStorage.getConnect(index).socketId = socket->getSocketId();


    return socket;
}


void ModbusTcpAppBase::closeAll()
{
    for (auto& elem : socketMap.getMap()) {
        TcpSocket *socket = check_and_cast<TcpSocket *>(elem.second);
        EV_INFO << "Closing socket to " << socket->getRemoteAddress() << endl;
        socket->close();
    }
}

void ModbusTcpAppBase::closeSocket(TcpSocket *socket)
{
      EV_INFO << "Closing socket to " << socket->getRemoteAddress() << endl;
      socket->close();
      emit(connectSignal, -1L);

}

void ModbusTcpAppBase::sendPacket(Packet *pkt, TcpSocket *socket)
{

    // 关键：切换到ModbusMasterApp的上下文，记录调试信息
    Enter_Method("sendPacket");

    // 关键：获取消息所有权（若消息来自TransitApp，其所有者可能是TransitApp）
    take(pkt); // 将消息所有者重新绑定为当前模块（ModbusMasterApp）



    auto chunk = pkt->peekDataAt(B(0), pkt->getTotalLength());
    waitProcessPacketSocketQueue[socket->getSocketId()].push(chunk);


    EV_INFO << "发送pkt，ID=" << pkt->getId() << endl;
    int numBytes = pkt->getByteLength();
    emit(packetSentSignal, pkt);
    socket->send(pkt);

    packetsSent++;
    bytesSent += numBytes;
}

void ModbusTcpAppBase::refreshDisplay() const
{
    ApplicationBase::refreshDisplay();
    std::stringstream out;
    out << "sockets: " << socketMap.size() << "\n";
    out << "sent: " << packetsSent << " pkts\n";
    out << "rcvd: " << packetsRcvd << " pkts";
    getDisplayString().setTagArg("t", 0, out.str().c_str());
}

void ModbusTcpAppBase::socketEstablished(TcpSocket *socket)
{
    EV_INFO << "Connection established to " << socket->getRemoteAddress() << ":"
            << socket->getRemotePort() << endl;
    emit(connectSignal, 1L);
}

void ModbusTcpAppBase::socketDataArrived(TcpSocket *socket, Packet *msg, bool)
{
    packetsRcvd++;
    bytesRcvd += msg->getByteLength();
    emit(packetReceivedSignal, msg);
    EV_INFO << "Received " << msg << " from " << socket->getRemoteAddress() << endl;
    // Subclasses should override this to process received data
    delete msg;
}

void ModbusTcpAppBase::socketPeerClosed(TcpSocket *socket)
{
    EV_INFO << "Remote closed connection to " << socket->getRemoteAddress() << endl;
    // Close our side too
    closeSocket(socket);
}

void ModbusTcpAppBase::socketClosed(TcpSocket *socket)
{
    EV_INFO << "Connection to " << socket->getRemoteAddress() << " closed" << endl;
}

void ModbusTcpAppBase::socketFailure(TcpSocket *socket, int code)
{
    EV_WARN << "Connection to " << socket->getRemoteAddress() << " failed, code=" << code << endl;
    numBroken++;
    // Remove socket from map
    socketMap.removeSocket(socket);
    delete socket;
}

void ModbusTcpAppBase::socketDeleted(TcpSocket *socket)
{
    EV_INFO << "Socket to " << socket->getRemoteAddress() << " deleted" << endl;
    socketMap.removeSocket(socket);
}

void ModbusTcpAppBase::finish()
{

    auto nodeName = this->getParentModule()->getFullName();
    std::string filePath = std::string("results/") + nodeName + std::string(".json");
    modbusStorage.saveToJson(filePath);


    std::string modulePath = getFullPath();

    EV_INFO << modulePath << ": opened " << numSessions << " sessions\n";
    EV_INFO << modulePath << ": " << numBroken << " sessions broken\n";
    EV_INFO << modulePath << ": sent " << bytesSent << " bytes in " << packetsSent << " packets\n";
    EV_INFO << modulePath << ": received " << bytesRcvd << " bytes in " << packetsRcvd << " packets\n";
}

// 实现 splitBySpace 函数
std::vector<std::string> ModbusTcpAppBase::splitBySpace(const std::string& str)
{
    std::vector<std::string> result;
    std::string current;

    for (char c : str) {
        if (isspace(c)) { // 遇到空格时，若当前有内容则加入结果
            if (!current.empty()) {
                result.push_back(current);
                current.clear();
            }
        }
        else { // 非空格字符累加
            current += c;
        }
    }

    // 处理最后一个非空字符串（避免末尾无空格的情况）
    if (!current.empty()) {
        result.push_back(current);
    }

    return result;
}

void ModbusTcpAppBase::handleTimer(cMessage *msg){

}


} // namespace inet
