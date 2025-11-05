/*
 * ModbusSlaveApp.cc
 *
 *  Created on: Oct 20, 2025
 *      Author: llw
 */

#include "ModbusSlaveApp.h"
#include "inet/common/ModuleAccess.h"
#include "inet/common/ProtocolTag_m.h"
#include "inet/common/packet/Message.h"
#include "inet/common/packet/chunk/ByteCountChunk.h"
#include "inet/common/socket/SocketTag_m.h"
#include "inet/networklayer/common/L3AddressResolver.h"
#include "inet/transportlayer/contract/tcp/TcpCommand_m.h"
#include <fstream>
#include "inet/common/lifecycle/NodeStatus.h"
#include "inet/networklayer/common/InterfaceTable.h"
#include "inet/common/TimeTag_m.h"

namespace inet {

Define_Module(ModbusSlaveApp);

void ModbusSlaveApp::initialize(int stage)
{
    cSimpleModule::initialize(stage);

    if (stage == INITSTAGE_LOCAL) {
        // 从ini文件获取配置路径
        slavesConfigPath = par("slavesConfigPath").stringValue();
        WATCH(requestsRcvd);
        WATCH(responsesSent);
        WATCH(bytesRcvd);
        WATCH(bytesSent);
    }
    else if (stage == INITSTAGE_APPLICATION_LAYER) {
        // 获取本地IP地址
        const char *localAddrStr = par("localAddress");
        localAddress = L3AddressResolver().resolve(localAddrStr);

        // 加载配置文件
        loadConfigFromJson();

        // 绑定到Modbus默认端口502并监听
        socket.setOutputGate(gate("socketOut"));
        socket.bind(localAddress, localPort);
        socket.listen();
        EV_INFO << "绑定到Modbus默认端口502并监听" << endl;

        // 检查节点状态
        cModule *node = findContainingNode(this);
        NodeStatus *nodeStatus = node ? check_and_cast_nullable<NodeStatus *>(node->getSubmodule("status")) : nullptr;
        bool isOperational = (!nodeStatus) || nodeStatus->getState() == NodeStatus::UP;
        if (!isOperational)
            throw cRuntimeError("This module doesn't support starting in node DOWN state");
    }
}

void ModbusSlaveApp::loadConfigFromJson()
{
    // 打开并解析JSON文件
    std::ifstream ifs(slavesConfigPath);
    if (!ifs.is_open())
        throw cRuntimeError("Failed to open config file: %s", slavesConfigPath.c_str());

    json j;
    try {
        ifs >> j;
        parseJsonConfig(j);
    }
    catch (const std::exception& e) {
        throw cRuntimeError("Error parsing JSON config: %s", e.what());
    }
}

void ModbusSlaveApp::parseJsonConfig(const json& j)
{
    // 查找与本地IP匹配的连接配置
    if (j.contains("connectArray") && j["connectArray"].is_array()) {
        for (const auto& connJson : j["connectArray"]) {
            if (connJson.contains("ipAddress") && matchLocalAddress(connJson["ipAddress"].get<std::string>())) {
                // 创建一个新连接
                connect newConn;
                newConn.ipAddress = L3AddressResolver().resolve(connJson["ipAddress"].get<std::string>().c_str());

                // 解析从站数量
                newConn.numSlave = connJson["numSlave"].get<int>();
                newConn.slaves = new MSMapping[newConn.numSlave]();

                int slaveIndex = 0;
                for (const auto& slaveJson  : connJson["slaves"]) {
                    if (slaveIndex >= newConn.numSlave) break;

                    MSMapping& slave = newConn.slaves[slaveIndex];
                    slave.slaveId = slaveJson ["slaveId"];
                    slave.numBitGroup = slaveJson ["numBitGroup"];
                    slave.numInputBitGroup = slaveJson ["numInputBitGroup"];
                    slave.numRegisterGroup = slaveJson ["numRegisterGroup"];
                    slave.numInputRegisterGroup = slaveJson ["numInputRegisterGroup"];

                    // 加载线圈组
                    loadRegisterGroups<uint8_t>(slaveJson ["bitGroup"], slave.bitGroup, slave.numBitGroup);
                    // 加载离散输入组
                    loadRegisterGroups<uint8_t>(slaveJson ["inputBitGroup"], slave.inputBitGroup, slave.numInputBitGroup);
                    // 加载保持寄存器组
                    loadRegisterGroups<int16_t>(slaveJson ["registerGroup"], slave.registerGroup, slave.numRegisterGroup);
                    // 加载输入寄存器组
                    loadRegisterGroups<int16_t>(slaveJson ["inputRegisterGroup"], slave.inputRegisterGroup, slave.numInputRegisterGroup);

                    slaveIndex++;
                }
                // 存储配置
                auto& connectArray = modbusStorage.getConnectArray();
                connectArray.push_back(newConn);
                modbusStorage.setNumConnect(connectArray.size());
                EV_INFO << "Successfully loaded config for IP: " << newConn.ipAddress << "numberConnect:" << modbusStorage.getNumConnect() << endl;

                return; // 找到匹配配置后退出
            }
        }
    }

    throw cRuntimeError("No matching IP configuration");
}

template <typename ElementType>
void ModbusSlaveApp::loadRegisterGroups(const json& jsonGroups, RegisterGroup<ElementType>*& groups, int count)
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

bool ModbusSlaveApp::matchLocalAddress(const std::string& configIp)
{
    L3Address configAddress = L3AddressResolver().resolve(configIp.c_str());
    InterfaceTable *interfaceTable = check_and_cast<InterfaceTable*>(findModuleByPath("^.interfaceTable"));
    if (!interfaceTable) {
        EV_ERROR << "interfaceTable module not found!" << endl;
        return false;
    }
    return interfaceTable->isLocalAddress(configAddress);
}

void ModbusSlaveApp::handleMessage(cMessage *msg)
{
    if (msg->getKind() == TCP_I_PEER_CLOSED) {
        // we'll close too, but only after there's surely no message
        // pending to be sent back in this connection
        int connId = check_and_cast<Indication *>(msg)->getTag<SocketInd>()->getSocketId();
        delete msg;
        auto request = new Request("close", TCP_C_CLOSE);
        request->addTag<SocketReq>()->setSocketId(connId);
        send(request, "socketOut");
    }
    else if (msg->getKind() == TCP_I_DATA || msg->getKind() == TCP_I_URGENT_DATA) {
        Packet *packet = check_and_cast<Packet *>(msg);
        int connId = packet->getTag<SocketInd>()->getSocketId();
        ChunkQueue& queue = socketQueue[connId];
        auto chunk = packet->peekDataAt(B(0), packet->getTotalLength());
        queue.push(chunk);
        emit(packetReceivedSignal, packet);

        while (queue.has<ModbusHeader>(b(-1))) {
            const auto& header = queue.pop<ModbusHeader>(b(-1));
            requestsRcvd++;
            bytesRcvd += B(header->getChunkLength()).get();

            // 验证协议标识（必须为0）
            if (header->getProtocolId() != 0) {
                EV_WARN << "Invalid Modbus protocol ID: " << header->getProtocolId() << endl;
                continue;
            }

            // 计算PDU长度（length = 从站ID(1字节) + PDU长度）
            uint16_t length = header->getLength();
            uint16_t pduLength = length - 1;

            // 验证PDU合法性（至少包含功能码1字节）
            if (pduLength < 1) {
                EV_WARN << "Invalid PDU length: " << pduLength << endl;
                continue;
            }

            // 检查PDU数据是否完整
            if (queue.getLength() < B(pduLength)) {
                // 数据不足，将头部放回队列
                queue.push(header);
                break;
            }

            // 提取PDU数据
            const auto& pduChunk = queue.pop<BytesChunk>(B(pduLength));
            const std::vector<uint8_t> pduData = pduChunk->getBytes();

            bytesRcvd += B(pduChunk->getChunkLength()).get();

            // 处理Modbus请求
            processModbusRequest(header, pduData.data(), pduLength, connId);



        }
        delete msg;


    }
    else if (msg->getKind() == TCP_I_AVAILABLE)
        socket.processMessage(msg);
    else {
        // some indication -- ignore
        EV_WARN << "drop msg: " << msg->getName() << ", kind:" << msg->getKind() << "(" << cEnum::get("inet::TcpStatusInd")->getStringFor(msg->getKind()) << ")\n";
        delete msg;
    }
}


void ModbusSlaveApp::processModbusRequest(const Ptr<const ModbusHeader>& requestHeader, const uint8_t* pduData, uint16_t pduLength, int connId)
{
    if (pduLength < 1) {
        sendExceptionResponse(requestHeader, 0x00, 0x03, connId); // 非法数据
        return;
    }

    uint8_t functionCode = pduData[0];
    switch (functionCode) {
        case 0x01: handleReadCoils(requestHeader, pduData, pduLength, connId); break;
        case 0x02: handleReadDiscreteInputs(requestHeader, pduData, pduLength, connId); break;
        case 0x03: handleReadHoldingRegisters(requestHeader, pduData, pduLength, connId); break;
        case 0x04: handleReadInputRegisters(requestHeader, pduData, pduLength, connId); break;
        case 0x05: handleWriteSingleCoil(requestHeader, pduData, pduLength, connId); break;
        case 0x06: handleWriteSingleRegister(requestHeader, pduData, pduLength, connId); break;
        case 0x0F: handleWriteMultipleCoils(requestHeader, pduData, pduLength, connId); break;
        case 0x10: handleWriteMultipleRegisters(requestHeader, pduData, pduLength, connId); break;
        default: sendExceptionResponse(requestHeader, functionCode, 0x01, connId); // 非法功能
    }
}

void ModbusSlaveApp::sendModbusResponse(const Ptr<const ModbusHeader>& requestHeader, const std::vector<uint8_t>& responsePdu, int connId)
{
    // 构建响应头部
    auto responseHeader = makeShared<ModbusHeader>();
    responseHeader->setTransactionId(requestHeader->getTransactionId());
    responseHeader->setProtocolId(0);
    responseHeader->setLength(1 + responsePdu.size()); // 1字节从站ID + PDU长度
    responseHeader->setSlaveId(requestHeader->getSlaveId());

    // 构建响应包
    Packet* responsePacket = new Packet("ModbusResponse", TCP_C_SEND);
    responsePacket->addTag<SocketReq>()->setSocketId(connId);
    responsePacket->insertAtBack(responseHeader);
    auto pduChunk = makeShared<BytesChunk>();
    pduChunk->setBytes(responsePdu);
    responsePacket->insertAtBack(pduChunk);
    responsePacket->addTag<CreationTimeTag>()->setCreationTime(simTime());

    auto& tags = check_and_cast<ITaggedObject *>(responsePacket)->getTags();
    tags.addTagIfAbsent<DispatchProtocolReq>()->setProtocol(&Protocol::tcp);

    // 发送响应
    send(responsePacket, "socketOut");
    bytesSent += responsePacket->getTotalLength().get();
    responsesSent++;
    emit(packetSentSignal, responsePacket);
}

void ModbusSlaveApp::sendExceptionResponse(const Ptr<const ModbusHeader>& requestHeader, uint8_t functionCode, uint8_t exceptionCode, int connId)
{
    std::vector<uint8_t> exceptionPdu;
    exceptionPdu.push_back(functionCode | 0x80); // 异常功能码（最高位置1）
    exceptionPdu.push_back(exceptionCode);       // 异常代码

    sendModbusResponse(requestHeader, exceptionPdu, connId);
}

MSMapping* ModbusSlaveApp::findSlave(uint8_t slaveId)
{
    const auto& connectArray = modbusStorage.getConnectArray();
    for (const auto& conn : connectArray) {
        for (int i = 0; i < conn.numSlave; i++) {
            if (conn.slaves[i].slaveId == slaveId) {
                return const_cast<MSMapping*>(&conn.slaves[i]);
            }
        }
    }
    EV_INFO <<"查找从站失败" << endl;
    return nullptr;
}

template <typename ElementType>
RegisterGroup<ElementType>* ModbusSlaveApp::findRegisterGroup(RegisterGroup<ElementType>* groups, int numGroups, uint16_t startAddress, uint16_t quantity)
{
    if (!groups || numGroups == 0) return nullptr;
    uint16_t endAddress = startAddress + quantity - 1;

    for (int i = 0; i < numGroups; i++) {
        auto& group = groups[i];
        uint16_t groupEnd = group.startAddress + group.number - 1;
        if (startAddress >= group.startAddress && endAddress <= groupEnd) {
            return &group;
        }
    }
    EV_INFO <<"查找寄存器组失败" << endl;
    return nullptr;
}

void ModbusSlaveApp::handleReadCoils(const Ptr<const ModbusHeader>& requestHeader, const uint8_t* pduData, uint16_t pduLength, int connId)
{
    // PDU格式：0x01 + 起始地址(2) + 数量(2) → 总长度5
    if (pduLength != 5) {
        sendExceptionResponse(requestHeader, 0x01, 0x03, connId);
        return;
    }

    uint16_t startAddr = ntohs(*(const uint16_t*)(pduData + 1));
    uint16_t quantity = ntohs(*(const uint16_t*)(pduData + 3));

    // 验证数量范围（1-2000）
    if (quantity < 1 || quantity > 2000) {
        sendExceptionResponse(requestHeader, 0x01, 0x03, connId);
        return;
    }

    // 查找从站和线圈组
    auto slave = findSlave(requestHeader->getSlaveId());
    if (!slave) {
        sendExceptionResponse(requestHeader, 0x01, 0x02, connId);
        return;
    }

    auto coilGroup = findRegisterGroup(slave->bitGroup, slave->numBitGroup, startAddr, quantity);
    if (!coilGroup) {
        sendExceptionResponse(requestHeader, 0x01, 0x02, connId);
        return;
    }

    // 构建响应数据
    std::vector<uint8_t> responsePdu;
    responsePdu.push_back(0x01);
    uint8_t byteCount = (quantity + 7) / 8;
    responsePdu.push_back(byteCount);

    uint8_t currentByte = 0;
    int bitPos = 0;
    for (uint16_t i = 0; i < quantity; i++) {
        uint16_t idx = startAddr + i - coilGroup->startAddress;
        currentByte |= (coilGroup->data[idx] & 0x01) << bitPos++;
        if (bitPos >= 8) {
            responsePdu.push_back(currentByte);
            currentByte = 0;
            bitPos = 0;
        }
    }
    if (bitPos > 0) responsePdu.push_back(currentByte);

    sendModbusResponse(requestHeader, responsePdu, connId);
}

void ModbusSlaveApp::handleReadDiscreteInputs(const Ptr<const ModbusHeader>& requestHeader, const uint8_t* pduData, uint16_t pduLength, int connId)
{
    // PDU格式：0x02 + 起始地址(2) + 数量(2) → 总长度5
    if (pduLength != 5) {
        sendExceptionResponse(requestHeader, 0x02, 0x03, connId);
        return;
    }

    uint16_t startAddr = ntohs(*(const uint16_t*)(pduData + 1));
    uint16_t quantity = ntohs(*(const uint16_t*)(pduData + 3));

    // 验证数量范围（1-2000）
    if (quantity < 1 || quantity > 2000) {
        sendExceptionResponse(requestHeader, 0x02, 0x03, connId);
        return;
    }

    // 查找从站和离散输入组
    auto slave = findSlave(requestHeader->getSlaveId());
    if (!slave) {
        sendExceptionResponse(requestHeader, 0x02, 0x02, connId);
        return;
    }

    auto inputGroup = findRegisterGroup(slave->inputBitGroup, slave->numInputBitGroup, startAddr, quantity);
    if (!inputGroup) {
        sendExceptionResponse(requestHeader, 0x02, 0x02, connId);
        return;
    }

    // 构建响应数据（与读线圈逻辑相同）
    std::vector<uint8_t> responsePdu;
    responsePdu.push_back(0x02);
    uint8_t byteCount = (quantity + 7) / 8;
    responsePdu.push_back(byteCount);

    uint8_t currentByte = 0;
    int bitPos = 0;
    for (uint16_t i = 0; i < quantity; i++) {
        uint16_t idx = startAddr + i - inputGroup->startAddress;
        currentByte |= (inputGroup->data[idx] & 0x01) << bitPos++;
        if (bitPos >= 8) {
            responsePdu.push_back(currentByte);
            currentByte = 0;
            bitPos = 0;
        }
    }
    if (bitPos > 0) responsePdu.push_back(currentByte);

    sendModbusResponse(requestHeader, responsePdu, connId);
}

void ModbusSlaveApp::handleReadHoldingRegisters(const Ptr<const ModbusHeader>& requestHeader, const uint8_t* pduData, uint16_t pduLength, int connId)
{
    // PDU格式：0x03 + 起始地址(2) + 数量(2) → 总长度5
    if (pduLength != 5) {
        sendExceptionResponse(requestHeader, 0x03, 0x03, connId);
        return;
    }

    uint16_t startAddr = ntohs(*(const uint16_t*)(pduData + 1));
    uint16_t quantity = ntohs(*(const uint16_t*)(pduData + 3));

    // 验证数量范围（1-125）
    if (quantity < 1 || quantity > 125) {
        sendExceptionResponse(requestHeader, 0x03, 0x03, connId);
        return;
    }

    // 查找从站和保持寄存器组
    auto slave = findSlave(requestHeader->getSlaveId());
    if (!slave) {
        sendExceptionResponse(requestHeader, 0x03, 0x02, connId);
        return;
    }

    auto regGroup = findRegisterGroup(slave->registerGroup, slave->numRegisterGroup, startAddr, quantity);
    if (!regGroup) {
        sendExceptionResponse(requestHeader, 0x03, 0x02, connId);
        return;
    }

    // 构建响应数据
    std::vector<uint8_t> responsePdu;
    responsePdu.push_back(0x03);
    responsePdu.push_back(quantity * 2); // 每个寄存器2字节

    for (uint16_t i = 0; i < quantity; i++) {
        uint16_t idx = startAddr + i - regGroup->startAddress;
        uint16_t value = regGroup->data[idx];
        responsePdu.push_back((value >> 8) & 0xFF);
        responsePdu.push_back(value & 0xFF);
    }

    sendModbusResponse(requestHeader, responsePdu, connId);
}

void ModbusSlaveApp::handleReadInputRegisters(const Ptr<const ModbusHeader>& requestHeader, const uint8_t* pduData, uint16_t pduLength, int connId)
{
    // PDU格式：0x04 + 起始地址(2) + 数量(2) → 总长度5
    if (pduLength != 5) {
        sendExceptionResponse(requestHeader, 0x04, 0x03, connId);
        return;
    }

    uint16_t startAddr = ntohs(*(const uint16_t*)(pduData + 1));
    uint16_t quantity = ntohs(*(const uint16_t*)(pduData + 3));

    // 验证数量范围（1-125）
    if (quantity < 1 || quantity > 125) {
        sendExceptionResponse(requestHeader, 0x04, 0x03, connId);
        return;
    }

    // 查找从站和输入寄存器组
    auto slave = findSlave(requestHeader->getSlaveId());
    if (!slave) {
        sendExceptionResponse(requestHeader, 0x04, 0x02, connId);
        return;
    }

    auto regGroup = findRegisterGroup(slave->inputRegisterGroup, slave->numInputRegisterGroup, startAddr, quantity);
    if (!regGroup) {
        sendExceptionResponse(requestHeader, 0x04, 0x02, connId);
        return;
    }

    // 构建响应数据（与读保持寄存器逻辑相同）
    std::vector<uint8_t> responsePdu;
    responsePdu.push_back(0x04);
    responsePdu.push_back(quantity * 2);

    for (uint16_t i = 0; i < quantity; i++) {
        uint16_t idx = startAddr + i - regGroup->startAddress;
        uint16_t value = regGroup->data[idx];
        responsePdu.push_back((value >> 8) & 0xFF);
        responsePdu.push_back(value & 0xFF);
    }

    sendModbusResponse(requestHeader, responsePdu, connId);
}

void ModbusSlaveApp::handleWriteSingleCoil(const Ptr<const ModbusHeader>& requestHeader, const uint8_t* pduData, uint16_t pduLength, int connId)
{
    // PDU格式：0x05 + 线圈地址(2) + 状态(2) → 总长度5
    if (pduLength != 5) {
        sendExceptionResponse(requestHeader, 0x05, 0x03, connId);
        return;
    }

    uint16_t coilAddr = ntohs(*(const uint16_t*)(pduData + 1));
    uint16_t value = ntohs(*(const uint16_t*)(pduData + 3));

    // 验证线圈状态（0xFF00=ON，0x0000=OFF）
    if (value != 0xFF00 && value != 0x0000) {
        sendExceptionResponse(requestHeader, 0x05, 0x03, connId);
        return;
    }

    // 查找从站和线圈组
    auto slave = findSlave(requestHeader->getSlaveId());
    if (!slave) {
        sendExceptionResponse(requestHeader, 0x05, 0x02, connId);
        return;
    }

    auto coilGroup = findRegisterGroup(slave->bitGroup, slave->numBitGroup, coilAddr, 1);
    if (!coilGroup) {
        sendExceptionResponse(requestHeader, 0x05, 0x02, connId);
        return;
    }

    // 写入线圈值
    uint16_t idx = coilAddr - coilGroup->startAddress;
    coilGroup->data[idx] = (value == 0xFF00) ? 1 : 0;

    // 响应PDU与请求PDU相同
    std::vector<uint8_t> responsePdu(pduData, pduData + pduLength);
    sendModbusResponse(requestHeader, responsePdu, connId);
}

void ModbusSlaveApp::handleWriteSingleRegister(const Ptr<const ModbusHeader>& requestHeader, const uint8_t* pduData, uint16_t pduLength, int connId)
{
    // PDU格式：0x06 + 寄存器地址(2) + 数值(2) → 总长度5
    if (pduLength != 5) {
        sendExceptionResponse(requestHeader, 0x06, 0x03, connId);
        return;
    }

    uint16_t regAddr = ntohs(*(const uint16_t*)(pduData + 1));
    uint16_t value = ntohs(*(const uint16_t*)(pduData + 3));

    // 查找从站和保持寄存器组
    auto slave = findSlave(requestHeader->getSlaveId());
    if (!slave) {
        sendExceptionResponse(requestHeader, 0x06, 0x02, connId);
        return;
    }

    auto regGroup = findRegisterGroup(slave->registerGroup, slave->numRegisterGroup, regAddr, 1);
    if (!regGroup) {
        sendExceptionResponse(requestHeader, 0x06, 0x02, connId);
        return;
    }

    // 写入寄存器值
    uint16_t idx = regAddr - regGroup->startAddress;
    regGroup->data[idx] = value;

    // 响应PDU与请求PDU相同
    std::vector<uint8_t> responsePdu(pduData, pduData + pduLength);
    sendModbusResponse(requestHeader, responsePdu, connId);
}

void ModbusSlaveApp::handleWriteMultipleCoils(const Ptr<const ModbusHeader>& requestHeader, const uint8_t* pduData, uint16_t pduLength, int connId)
{
    // PDU格式：0x0F + 起始地址(2) + 数量(2) + 字节数(1) + 数据(n)
    if (pduLength < 6) { // 最小长度：1+2+2+1=6
        sendExceptionResponse(requestHeader, 0x0F, 0x03, connId);
        return;
    }

    uint16_t startAddr = ntohs(*(const uint16_t*)(pduData + 1));
    uint16_t quantity = ntohs(*(const uint16_t*)(pduData + 3));
    uint8_t byteCount = pduData[5];

    // 验证数量范围（1-1968）和数据长度匹配
    if (quantity < 1 || quantity > 1968 || byteCount != (quantity + 7)/8) {
        sendExceptionResponse(requestHeader, 0x0F, 0x03, connId);
        return;
    }

    // 验证PDU总长度
    if (pduLength != 6 + byteCount) {
        sendExceptionResponse(requestHeader, 0x0F, 0x03, connId);
        return;
    }

    // 查找从站和线圈组
    auto slave = findSlave(requestHeader->getSlaveId());
    if (!slave) {
        sendExceptionResponse(requestHeader, 0x0F, 0x02, connId);
        return;
    }

    auto coilGroup = findRegisterGroup(slave->bitGroup, slave->numBitGroup, startAddr, quantity);
    if (!coilGroup) {
        sendExceptionResponse(requestHeader, 0x0F, 0x02, connId);
        return;
    }

    // 写入线圈值
    for (uint16_t i = 0; i < quantity; i++) {
        uint16_t idx = startAddr + i - coilGroup->startAddress;
        uint8_t bytePos = i / 8;
        uint8_t bitPos = i % 8;
        coilGroup->data[idx] = (pduData[6 + bytePos] >> bitPos) & 0x01;
    }

    // 响应PDU：功能码 + 起始地址 + 数量
    std::vector<uint8_t> responsePdu;
    responsePdu.push_back(0x0F);
    responsePdu.push_back((startAddr >> 8) & 0xFF);
    responsePdu.push_back(startAddr & 0xFF);
    responsePdu.push_back((quantity >> 8) & 0xFF);
    responsePdu.push_back(quantity & 0xFF);

    sendModbusResponse(requestHeader, responsePdu, connId);
}

void ModbusSlaveApp::handleWriteMultipleRegisters(const Ptr<const ModbusHeader>& requestHeader, const uint8_t* pduData, uint16_t pduLength, int connId)
{
    // PDU格式：0x10 + 起始地址(2) + 数量(2) + 字节数(1) + 数据(2*n)
    if (pduLength < 6) { // 最小长度：1+2+2+1=6
        sendExceptionResponse(requestHeader, 0x10, 0x03, connId);
        return;
    }

    uint16_t startAddr = ntohs(*(const uint16_t*)(pduData + 1));
    uint16_t quantity = ntohs(*(const uint16_t*)(pduData + 3));
    uint8_t byteCount = pduData[5];

    // 验证数量范围（1-123）和数据长度匹配
    if (quantity < 1 || quantity > 123 || byteCount != 2 * quantity) {
        sendExceptionResponse(requestHeader, 0x10, 0x03, connId);
        return;
    }

    // 验证PDU总长度
    if (pduLength != 6 + byteCount) {
        sendExceptionResponse(requestHeader, 0x10, 0x03, connId);
        return;
    }

    // 查找从站和保持寄存器组
    auto slave = findSlave(requestHeader->getSlaveId());
    if (!slave) {
        sendExceptionResponse(requestHeader, 0x10, 0x02, connId);
        return;
    }

    auto regGroup = findRegisterGroup(slave->registerGroup, slave->numRegisterGroup, startAddr, quantity);
    if (!regGroup) {
        sendExceptionResponse(requestHeader, 0x10, 0x02, connId);
        return;
    }

    // 写入寄存器值
    for (uint16_t i = 0; i < quantity; i++) {
        uint16_t idx = startAddr + i - regGroup->startAddress;
        uint16_t value = ntohs(*(const uint16_t*)(pduData + 6 + 2*i));
        regGroup->data[idx] = value;
    }

    // 响应PDU：功能码 + 起始地址 + 数量
    std::vector<uint8_t> responsePdu;
    responsePdu.push_back(0x10);
    responsePdu.push_back((startAddr >> 8) & 0xFF);
    responsePdu.push_back(startAddr & 0xFF);
    responsePdu.push_back((quantity >> 8) & 0xFF);
    responsePdu.push_back(quantity & 0xFF);

    sendModbusResponse(requestHeader, responsePdu, connId);
}

void ModbusSlaveApp::refreshDisplay() const
{
    char buf[128];
    sprintf(buf, "Requests: %ld, Responses: %ld\nBytes Rcvd: %ld, Sent: %ld",
            requestsRcvd, responsesSent, bytesRcvd, bytesSent);
    getDisplayString().setTagArg("t", 0, buf);
}

void ModbusSlaveApp::finish()
{
    auto nodeName = this->getParentModule()->getFullName();
    std::string filePath = std::string("results/") + nodeName + std::string(".json");
    modbusStorage.saveToJson(filePath);

    EV_INFO << getFullPath() << ": "
            << "Requests received: " << requestsRcvd << ", "
            << "Responses sent: " << responsesSent << endl;
    EV_INFO << "Bytes received: " << bytesRcvd << ", "
            << "Bytes sent: " << bytesSent << endl;
}

} // namespace inet
