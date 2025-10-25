#include "ModbusMasterApp.h"
#include "inet/common/ProtocolTag_m.h"
#include "inet/common/packet/chunk/ByteCountChunk.h"

namespace inet {



Define_Module(ModbusMasterApp);

void ModbusMasterApp::initialize(int stage) {
    ModbusTcpAppBase::initialize(stage);
    if (stage == INITSTAGE_APPLICATION_LAYER) {
        // 从NED参数获取读取间隔
        readInterval = par("readInterval");
        readTimer = new cMessage("readTimer");

        scheduleAt(simTime(), readTimer);
    }
}

void ModbusMasterApp::handleTimer(cMessage *msg) {
    if (msg == readTimer) {
        // 遍历所有连接
        for (int connIdx = 0; connIdx < modbusStorage.getNumConnect(); connIdx++) {
            const auto& conn = modbusStorage.getConnect(connIdx);
            // 遍历连接下的所有从站
            for (int slaveIdx = 0; slaveIdx < conn.numSlave; slaveIdx++) {
                const auto& slave = conn.slaves[slaveIdx];
                // 读取线圈组（功能码0x01）
                for (int i = 0; i < slave.numBitGroup; i++) {
                    auto& group = slave.bitGroup[i];
                    auto pkt = createRequest(slave.slaveId, 0x01,
                                               group.startAddress, group.number);
                    auto socket = socketMap.getSocketById(conn.socketId);
                    if (socket) {
                        sendPacket(pkt, check_and_cast<TcpSocket*>(socket));
                    }
                }
                // 读取离散输入组（功能码0x02）
                for (int i = 0; i < slave.numInputBitGroup; i++) {
                    auto& group = slave.inputBitGroup[i];
                    auto pkt = createRequest(slave.slaveId, 0x02,
                                               group.startAddress, group.number);
                    auto socket = socketMap.getSocketById(conn.socketId);
                    if (socket) {
                        sendPacket(pkt, check_and_cast<TcpSocket*>(socket));
                    }
                }
                // 读取保持寄存器组（功能码0x03）
                for (int i = 0; i < slave.numRegisterGroup; i++) {
                    auto& group = slave.registerGroup[i];
                    auto pkt = createRequest(slave.slaveId, 0x03,
                                               group.startAddress, group.number);
                    auto socket = socketMap.getSocketById(conn.socketId);
                    if (socket) {
                        sendPacket(pkt, check_and_cast<TcpSocket*>(socket));
                    }
                }
                // 读取输入寄存器组（功能码0x04）
                for (int i = 0; i < slave.numInputRegisterGroup; i++) {
                    auto& group = slave.inputRegisterGroup[i];
                    auto pkt = createRequest(slave.slaveId, 0x04,
                                               group.startAddress, group.number);
                    auto socket = socketMap.getSocketById(conn.socketId);
                    if (socket) {
                        sendPacket(pkt, check_and_cast<TcpSocket*>(socket));
                    }
                }
            }
        }
        // 调度下一次读取
        scheduleAt(simTime() + readInterval, readTimer);
    }
    else {
        ModbusTcpAppBase::handleTimer(msg);
    }
}

void ModbusMasterApp::socketEstablished(TcpSocket *socket) {

    // 调用父类实现（确保基础逻辑执行）
    ModbusTcpAppBase::socketEstablished(socket);

}

void ModbusMasterApp::socketDataArrived(TcpSocket *socket, Packet *msg, bool urgent) {
    // 确保消息不为空
    if (!msg) {
        EV_ERROR << "Received null packet, ignoring." << endl;
        delete msg;
        return;
    }

    try {
        // 1. 获取连接标识并将收到的数据加入队列
        int socketId = socket->getSocketId();
        ChunkQueue& queue = socketQueue[socketId]; // 假设socketQueue已初始化

        // 提取报文中的数据并加入队列
        auto dataChunk = msg->peekDataAt(B(0), msg->getTotalLength());
        queue.push(dataChunk);
        EV_DEBUG << "Added " << msg->getTotalLength() << " bytes to queue for socket " << socketId << endl;

        // 2. 循环处理队列中完整的Modbus响应报文
        while (queue.getLength() >= B(7)) { // ModbusHeader至少7字节
            // 检查是否有完整的ModbusHeader
            if (!queue.has<ModbusHeader>()) {
                EV_DEBUG << "No complete ModbusHeader in queue, waiting for more data." << endl;
                break;
            }

            // 2.1 取出响应报文头部
            const auto& responseHeader = queue.pop<ModbusHeader>();
            uint16_t responseLength = responseHeader->getLength();
            B pduLength = B(responseLength - 1); // length字段包含后续字节数（slaveId + PDU）

            // 2.2 检查PDU部分是否完整
            if (queue.getLength() < pduLength) {
                EV_DEBUG << "Incomplete PDU in queue (need " << pduLength << ", has " << queue.getLength() << "), pushing back header and waiting." << endl;
                // 将头部放回队列（因PDU不完整，需重新处理）
                queue.push(responseHeader);
                break;
            }

            // 2.3 取出响应PDU部分
            const auto& responsePdu = queue.pop<BytesChunk>(pduLength);
            EV_DEBUG << "Extracted Modbus response: transactionId=" << responseHeader->getTransactionId()
                      << ", length=" << responseLength << endl;

            // 3. 获取对应的待处理请求报文
            if (!waitProcessPacketSocketQueue[socketId].has<ModbusHeader>()) {
                EV_DEBUG << "No complete ModbusHeader in waitProcessPacketSocketQueue, waiting for more data." << endl;
                break;
            }
            const auto& requestHeader = waitProcessPacketSocketQueue[socketId].pop<ModbusHeader>();
            const auto& requestPdu = waitProcessPacketSocketQueue[socketId].pop<BytesChunk>(B(requestHeader->getLength() - 1));

            // 5. 验证响应报文的正确性（简单示例：对比transactionId和slaveId）
            bool isResponseValid = (responseHeader->getTransactionId() == requestHeader->getTransactionId()) &&
                                   (responseHeader->getSlaveId() == requestHeader->getSlaveId());

            if (!isResponseValid) {
                EV_ERROR << "Invalid response: transactionId or slaveId mismatch with request." << endl;

//                delete requestHeader;
//                delete requestPdu;
                continue;
            }

            // 6. 处理正确的响应
            parseAndStoreResponse(socket, requestPdu, responseHeader, responsePdu);

        }
    }
    catch (const cRuntimeError& e) {
        EV_ERROR << "Error processing received data: " << e.what() << endl;
    }

    // 7. 调用父类方法处理统计和资源释放
    ModbusTcpAppBase::socketDataArrived(socket, msg, urgent);
}
Packet* ModbusMasterApp::createRequest(uint8_t slaveId, uint8_t functionCode,
                                          uint16_t startAddress, uint16_t quantity) {
    // 创建Modbus请求报文
    auto pkt = new Packet("ModbusRequest");
    auto header = makeShared<ModbusHeader>();

    // 设置Modbus头部字段
    header->setTransactionId(transactionId++);
    header->setProtocolId(0x0000); // Modbus TCP协议标识
    header->setLength(6); // 后续字节数（slaveId + functionCode + 4字节参数）
    header->setSlaveId(slaveId);

    std::vector<uint8_t> pdu;
    pdu.push_back(functionCode);
    uint8_t startAddressLow = startAddress & 0x00ff;
    uint8_t startAddressHigh = (startAddress >> 8) & 0x00ff;
    pdu.push_back(startAddressHigh);
    pdu.push_back(startAddressLow);
    uint8_t quantityLow = quantity & 0x00ff;
    uint8_t quantityHigh = (quantity >> 8) & 0x00ff;
    pdu.push_back(quantityHigh);
    pdu.push_back(quantityLow);

    auto pduChunk = makeShared<BytesChunk>();
    pduChunk->setBytes(pdu);


    pkt->insertAtFront(header);
    pkt->insertAtBack(pduChunk);



    return pkt;
}

Packet* ModbusMasterApp::createRequest(uint8_t slaveId, uint8_t functionCode,
                                       uint16_t startAddress, uint16_t quantity, const std::vector<uint8_t>& data) {
    Enter_Method("createRequest");
    if(data.empty()){
        throw cRuntimeError("data is empty when createRequest");
    }

    EV_INFO << "start create packet" << endl;
    auto pkt = new Packet("ModbusRequest");
    auto header = makeShared<ModbusHeader>();

    EV_INFO << "设置Modbus头部字段" << endl;
    header->setTransactionId(transactionId++);
    header->setProtocolId(0x0000);
    header->setSlaveId(slaveId);

    std::vector<uint8_t> pdu;  // 存储PDU字节流（元素为uint8_t）
    pdu.push_back(functionCode);
    uint8_t startAddressLow = startAddress & 0x00ff;
    uint8_t startAddressHigh = (startAddress >> 8) & 0x00ff;
    pdu.push_back(startAddressHigh);
    pdu.push_back(startAddressLow);

    ChunkPtr pduChunk;
    uint8_t dataLength;  // 注意：此处声明的变量作用域为整个函数，避免在case内重复定义
    auto tempChunk = makeShared<BytesChunk>();

    // 每个case用{}包裹，形成独立作用域，避免变量冲突和跳转错误
    switch(functionCode){
        case 0x05: {  // 用{}隔离作用域
            EV_INFO << "处理线圈操作（功能码0x" << std::hex << (int)functionCode << std::dec << "）数据" << endl;

            if (data.size() != quantity) {
                EV_ERROR << "线圈操作数据长度错误（功能码0x" << std::hex << (int)functionCode << std::dec
                        << "）：预期" << quantity << "个元素，实际" << data.size() << "个" << endl;
                throw cRuntimeError("Invalid data length for coil operation");
            }
            // 0x05功能码：写入单线圈，数据应为0xFF00（ON）或0x0000（OFF）
            pdu.push_back(data[0] ? 0xFF : 0x00);  // 高8位
            pdu.push_back(0x00);  // 低8位（固定为00）
            dataLength = 1;

            tempChunk->setBytes(pdu);
            pduChunk = tempChunk;
            break;
        }
        case 0x06: {  // 用{}隔离作用域
            EV_INFO << "处理寄存器操作（功能码0x" << std::hex << (int)functionCode << std::dec << "）数据" << endl;

            if (data.size() != quantity * 2) {  // 0x06写入单寄存器，quantity应为1，数据长度2字节
                EV_ERROR << "寄存器操作数据长度错误（功能码0x" << std::hex<< (int)functionCode << std::dec
                         << "）：预期" << quantity*2 << "字节，实际" << data.size() << "字节" << endl;
                throw cRuntimeError("Invalid data length for register operation");
            }

            for (uint8_t byte : data) {
                pdu.push_back(byte);
            }
            dataLength = quantity * 2;

            tempChunk->setBytes(pdu);
            pduChunk = tempChunk;
            break;
        }
        case 0x0F: {  // 用{}隔离作用域
            EV_INFO << "处理线圈操作（功能码0x" << std::hex << (int)functionCode << std::dec << "）数据" << endl;

            if (data.size() != quantity) {
                EV_ERROR << "线圈操作数据长度错误（功能码0x" << std::hex << (int)functionCode << std::dec
                        << "）：预期" << quantity << "个元素，实际" << data.size() << "个" << endl;
                throw cRuntimeError("Invalid data length for coil operation");
            }
            // 0x0F功能码：写入多线圈，需添加数量的高8位和低8位
            uint8_t quantityLow = quantity & 0x00ff;
            uint8_t quantityHigh = (quantity >> 8) & 0x00ff;
            pdu.push_back(quantityHigh);
            pdu.push_back(quantityLow);

            // 构建比特序列并补0
            std::vector<bool> bits;
            bits.reserve(quantity);
            for (uint8_t byte : data) {
                bits.push_back(byte != 0);
            }
            int currentBits = bits.size();
            int padding = (8 - (currentBits % 8)) % 8;
            if (padding > 0) {
                EV_INFO << "线圈数据比特数不足8的倍数，补充" << padding << "个0（总长度变为" << currentBits + padding << "）" << endl;
                bits.insert(bits.begin(), padding, false);
            }

            // 构建数据部分（包含字节计数和线圈数据）
            auto bitsChunk = makeShared<BitsChunk>();
            bitsChunk->setBits(bits);
            dataLength = bits.size() / 8;  // 字节数 = 总比特数 / 8
            pdu.push_back(dataLength);  // 添加字节计数（紧跟在数量后）

            // 合并PDU头部和线圈数据
            auto sequence = makeShared<SequenceChunk>();
            tempChunk->setBytes(pdu);

            sequence->insertAtBack(tempChunk);
            sequence->insertAtBack(bitsChunk);
            pduChunk = sequence;
            break;
        }
        case 0x10: {  // 用{}隔离作用域，避免与0x0F的quantityLow/High冲突
            EV_INFO << "处理寄存器操作（功能码0x" << std::hex << (int)functionCode << std::dec << "）数据" << endl;

            if (data.size() != quantity * 2) {  // 0x10写入多寄存器，每个寄存器2字节
                EV_ERROR << "寄存器操作数据长度错误（功能码0x" << std::hex<< (int)functionCode << std::dec
                         << "）：预期" << quantity*2 << "字节，实际" << data.size() << "字节" << endl;
                throw cRuntimeError("Invalid data length for register operation");
            }
            // 添加数量的高8位和低8位
            uint8_t quantityLow = quantity & 0x00ff;  // 作用域限制在当前case内，无重定义
            uint8_t quantityHigh = (quantity >> 8) & 0x00ff;
            pdu.push_back(quantityHigh);
            pdu.push_back(quantityLow);

            // 添加数据字节计数和数据本身
            dataLength = data.size();  // 数据总字节数（= quantity*2）
            pdu.push_back(dataLength);
            // 错误点：需逐个添加data中的字节，不能直接push_back整个vector
            for (uint8_t byte : data) {
                pdu.push_back(byte);
            }

            tempChunk->setBytes(pdu);
            pduChunk = tempChunk;
            break;
        }
        default: {  // 用{}隔离作用域
           EV_ERROR << "Unsupported function code: " << (int)functionCode << endl;
           delete pkt;  // 避免内存泄漏
           return nullptr;
        }
    }

    // 计算Modbus头部的长度字段（PDU总长度 = 功能码(1) + 后续字节数）
    header->setLength((pduChunk->getChunkLength().get())/8 + 1);  // 减去功能码的1字节

    pkt->insertAtFront(header);
    pkt->insertAtBack(pduChunk);

    EV_INFO << "packet总长度：" << pkt->getDataLength()  << endl;
    EV_INFO << "created packet ID：" << pkt->getId()  << endl;

    return pkt;
}

// 解析收到的响应报文并存储
void ModbusMasterApp::parseAndStoreResponse(TcpSocket *socket, const inet::Ptr<const inet::BytesChunk> &requestPdu, const inet::Ptr<const ModbusHeader> &responseHeader, const inet::Ptr<const inet::BytesChunk> &responsePdu) {
    // 1. 提取基础信息（socketId、slaveId）
    if (!socket || !requestPdu || !responseHeader || !responsePdu) {
        EV_ERROR << "parseAndStoreResponse: 输入参数为空" << endl;
        return;
    }
    int socketId = socket->getSocketId();
    auto modbusHeader = inet::dynamicPtrCast<const inet::ModbusHeader>(responseHeader);
    if (!modbusHeader) {
        EV_ERROR << "parseAndStoreResponse: 无效的Modbus头部" << endl;
        return;
    }
    uint8_t slaveId = modbusHeader->getSlaveId();
    const auto& requestBytes = requestPdu->getBytes();
    const auto& responseBytes = responsePdu->getBytes();

    // 2. 检查请求报文合法性并提取功能码
    if (requestBytes.empty()) {
        EV_ERROR << "parseAndStoreResponse: 请求PDU为空" << endl;
        return;
    }
    uint8_t reqFuncCode = requestBytes[0];
    uint16_t startAddress = 0;
    uint16_t quantity = 0;

    // 3. 从请求PDU提取起始地址和数量（根据功能码类型）
    try {
        if (reqFuncCode == 0x01 || reqFuncCode == 0x02 || reqFuncCode == 0x03 || reqFuncCode == 0x04) {
            // 读操作请求格式：[func(1)] + [start(2)] + [quantity(2)]
            if (requestBytes.size() < 5) throw std::invalid_argument("读操作请求格式不完整");
            startAddress = (requestBytes[1] << 8) | requestBytes[2];
            quantity = (requestBytes[3] << 8) | requestBytes[4];
        }
        else if (reqFuncCode == 0x05 || reqFuncCode == 0x06) {
            // 单个写操作请求格式：[func(1)] + [start(2)] + [data(2)]
            if (requestBytes.size() < 5) throw std::invalid_argument("单个写操作请求格式不完整");
            startAddress = (requestBytes[1] << 8) | requestBytes[2];
            quantity = 1; // 单个写固定数量为1
        }
        else if (reqFuncCode == 0x0F || reqFuncCode == 0x10) {
            // 多个写操作请求格式：[func(1)] + [start(2)] + [quantity(2)] + ...
            if (requestBytes.size() < 5) throw std::invalid_argument("多个写操作请求格式不完整");
            startAddress = (requestBytes[1] << 8) | requestBytes[2];
            quantity = (requestBytes[3] << 8) | requestBytes[4];
        }
        else {
            EV_WARN << "parseAndStoreResponse: 不支持的功能码 " << std::hex << (int)reqFuncCode << std::dec << endl;
            return;
        }
    }
    catch (const std::invalid_argument& e) {
        EV_ERROR << "parseAndStoreResponse: " << e.what() << endl;
        return;
    }

    // 4. 处理异常响应
    if (responseBytes.empty()) {
        EV_ERROR << "parseAndStoreResponse: 响应PDU为空" << endl;
        return;
    }
    uint8_t respFuncCode = responseBytes[0];
    if ((respFuncCode & 0x80) != 0) {
        // 异常响应格式：[func|0x80(1)] + [exceptionCode(1)]
        uint8_t exceptionCode = (responseBytes.size() >= 2) ? responseBytes[1] : 0xFF;
        EV_INFO << "Modbus异常响应 - 从站ID: " << (int)slaveId
                << ", 功能码: " << std::hex << (int)reqFuncCode
                << ", 异常码: " << (int)exceptionCode << std::dec << endl;
        return;
    }

    // 5. 验证响应功能码一致性
    if (respFuncCode != reqFuncCode) {
        EV_WARN << "parseAndStoreResponse: 功能码不匹配 - 请求: " << std::hex << (int)reqFuncCode
                << ", 响应: " << (int)respFuncCode << std::dec << endl;
        return;
    }

    // 6. 查找目标连接和从站
    int connIdx = modbusStorage.findConnectIndexBySocketId(socketId);
    if (connIdx == -1) {
        EV_WARN << "parseAndStoreResponse: 未找到socketId=" << socketId << "的连接" << endl;
        return;
    }
    inet::connect& targetConn = modbusStorage.getConnect(connIdx);
    inet::MSMapping* targetSlave = nullptr;
    for (int i = 0; i < targetConn.numSlave; i++) {
        if (targetConn.slaves[i].slaveId == slaveId) {
            targetSlave = &targetConn.slaves[i];
            break;
        }
    }
    if (!targetSlave) {
        EV_WARN << "parseAndStoreResponse: 未找到从站ID=" << (int)slaveId << "的配置" << endl;
        return;
    }

    // 7. 处理读操作响应（0x01/0x02/0x03/0x04）
    if (respFuncCode == 0x01 || respFuncCode == 0x02 || respFuncCode == 0x03 || respFuncCode == 0x04) {
        if (responseBytes.size() < 2) {
            EV_ERROR << "parseAndStoreResponse: 读操作响应缺少数据长度字段" << endl;
            return;
        }
        uint8_t dataLength = responseBytes[1];
        if (responseBytes.size() < 2 + dataLength) {
            EV_ERROR << "parseAndStoreResponse: 读操作响应数据不完整（期望" << (2 + dataLength)
                    << "字节，实际" << responseBytes.size() << "字节）" << endl;
            return;
        }
        const uint8_t* dataStart = &responseBytes[2];

        // 7.1 线圈/离散输入（位数据）处理
        if (respFuncCode == 0x01 || respFuncCode == 0x02) {
            std::vector<inet::RegisterGroup<uint8_t>*> bitGroups;
            if (respFuncCode == 0x01) {
                for (int i = 0; i < targetSlave->numBitGroup; i++) {
                    bitGroups.push_back(&targetSlave->bitGroup[i]); // 取对象地址
                }
            } else {
                for (int i = 0; i < targetSlave->numInputBitGroup; i++) {
                    bitGroups.push_back(&targetSlave->inputBitGroup[i]);
                }
            }

            for (uint16_t i = 0; i < quantity; i++) {
                uint16_t currAddr = startAddress + i;
                inet::RegisterGroup<uint8_t>* targetGroup = nullptr;
                for (auto group : bitGroups) {
                    if (currAddr >= group->startAddress && currAddr < group->startAddress + group->number) {
                        targetGroup = group;
                        break;
                    }
                }
                if (!targetGroup) {
                    EV_WARN << "parseAndStoreResponse: 地址" << currAddr << "不在任何位寄存器组中" << endl;
                    continue;
                }
                uint16_t offset = currAddr - targetGroup->startAddress;
                uint16_t byteIdx = i / 8;
                uint8_t bitIdx = 7 - (i % 8); // Modbus位存储高位在前
                targetGroup->data[offset] = (dataStart[byteIdx] >> bitIdx) & 0x01;
            }
        }
        // 7.2 保持寄存器/输入寄存器（16位数据）处理
        else {
            std::vector<inet::RegisterGroup<int16_t>*> regGroups;
            if (respFuncCode == 0x03) {
                for (int i = 0; i < targetSlave->numRegisterGroup; i++) {
                    regGroups.push_back(&targetSlave->registerGroup[i]);
                }
            } else {
                for (int i = 0; i < targetSlave->numInputRegisterGroup; i++) {
                    regGroups.push_back(&targetSlave->inputRegisterGroup[i]);
                }
            }

            for (uint16_t i = 0; i < quantity; i++) {
                uint16_t currAddr = startAddress + i;
                inet::RegisterGroup<int16_t>* targetGroup = nullptr;
                for (auto group : regGroups) {
                    if (currAddr >= group->startAddress && currAddr < group->startAddress + group->number) {
                        targetGroup = group;
                        break;
                    }
                }
                if (!targetGroup) {
                    EV_WARN << "parseAndStoreResponse: 地址" << currAddr << "不在任何16位寄存器组中" << endl;
                    continue;
                }
                uint16_t offset = currAddr - targetGroup->startAddress;
                uint16_t data = (dataStart[2*i] << 8) | dataStart[2*i + 1]; // 大端转主机序
                targetGroup->data[offset] = data;
            }
        }
    }
    // 8. 处理单个写操作响应（0x05/0x06）
    else if (respFuncCode == 0x05 || respFuncCode == 0x06) {
        if (responseBytes.size() < 5) {
            EV_ERROR << "parseAndStoreResponse: 单个写操作响应格式不完整" << endl;
            return;
        }
        uint16_t respStartAddr = (responseBytes[1] << 8) | responseBytes[2];
        uint16_t data = (responseBytes[3] << 8) | responseBytes[4];
        if (respStartAddr != startAddress) {
            EV_WARN << "parseAndStoreResponse: 单个写操作起始地址不匹配（请求" << startAddress << "，响应" << respStartAddr << "）" << endl;
            return;
        }

        // 8.1 单个线圈写入（0x05）
        if (respFuncCode == 0x05) {
            uint8_t bitValue = (data == 0xFF00) ? 1 : 0; // 0xFF00=ON, 0x0000=OFF
            for (int i = 0; i < targetSlave->numBitGroup; i++) {
                auto& group = targetSlave->bitGroup[i];
                if (startAddress >= group.startAddress && startAddress < group.startAddress + group.number) {
                    group.data[startAddress - group.startAddress] = bitValue;
                    break;
                }
            }
        }
        // 8.2 单个寄存器写入（0x06）
        else {
            for (int i = 0; i < targetSlave->numRegisterGroup; i++) {
                auto& group = targetSlave->registerGroup[i];
                if (startAddress >= group.startAddress && startAddress < group.startAddress + group.number) {
                    group.data[startAddress - group.startAddress] = data;
                    break;
                }
            }
        }
    }
    // 9. 处理多个写操作响应（0x0F/0x10）
    else if (respFuncCode == 0x0F || respFuncCode == 0x10) {
        if (responseBytes.size() < 6) {
            EV_ERROR << "parseAndStoreResponse: 多个写操作响应格式不完整" << endl;
            return;
        }
        uint16_t respStartAddr = (responseBytes[1] << 8) | responseBytes[2];
        uint16_t respQuantity = (responseBytes[3] << 8) | responseBytes[4];
        uint8_t dataLength = responseBytes[5];
        if (respStartAddr != startAddress || respQuantity != quantity) {
            EV_WARN << "parseAndStoreResponse: 多个写操作参数不匹配" << endl;
            return;
        }
        if (responseBytes.size() < 6 + dataLength) {
            EV_ERROR << "parseAndStoreResponse: 多个写操作数据不完整" << endl;
            return;
        }
        const uint8_t* dataStart = &responseBytes[6];

        // 9.1 多个线圈写入（0x0F）
        if (respFuncCode == 0x0F) {
            for (uint16_t i = 0; i < quantity; i++) {
                uint16_t currAddr = startAddress + i;
                for (int g = 0; g < targetSlave->numBitGroup; g++) {
                    auto& group = targetSlave->bitGroup[g];
                    if (currAddr >= group.startAddress && currAddr < group.startAddress + group.number) {
                        uint16_t offset = currAddr - group.startAddress;
                        uint16_t byteIdx = i / 8;
                        uint8_t bitIdx = 7 - (i % 8);
                        group.data[offset] = (dataStart[byteIdx] >> bitIdx) & 0x01;
                        break;
                    }
                }
            }
        }
        // 9.2 多个寄存器写入（0x10）
        else {
            if (dataLength != quantity * 2) {
                EV_WARN << "parseAndStoreResponse: 多个寄存器数据长度不匹配" << endl;
                return;
            }
            for (uint16_t i = 0; i < quantity; i++) {
                uint16_t currAddr = startAddress + i;
                for (int g = 0; g < targetSlave->numRegisterGroup; g++) {
                    auto& group = targetSlave->registerGroup[g];
                    if (currAddr >= group.startAddress && currAddr < group.startAddress + group.number) {
                        uint16_t offset = currAddr - group.startAddress;
                        uint16_t data = (dataStart[2*i] << 8) | dataStart[2*i + 1];
                        group.data[offset] = data;
                        break;
                    }
                }
            }
        }
    }

    EV_INFO << "parseAndStoreResponse: 成功处理响应 - 功能码: " << std::hex << (int)respFuncCode
            << ", 从站ID: " << (int)slaveId << std::dec << endl;
}

// 实现纯虚函数：启动操作
void ModbusMasterApp::handleStartOperation(LifecycleOperation *operation) {
    // 启动时开始定时读取
    if (readTimer && !readTimer->isScheduled()) {
        scheduleAt(simTime(), readTimer);
    }
}

// 实现纯虚函数：停止操作
void ModbusMasterApp::handleStopOperation(LifecycleOperation *operation) {
    // 停止时取消定时器
    if (readTimer && readTimer->isScheduled()) {
        cancelEvent(readTimer);
    }
    // 关闭所有连接
    closeAll();
}

// 实现纯虚函数：崩溃处理
void ModbusMasterApp::handleCrashOperation(LifecycleOperation *operation) {
    // 崩溃时取消定时器并清理资源
    if (readTimer) {
        cancelAndDelete(readTimer);
        readTimer = nullptr;
    }
    // 关闭所有连接
    closeAll();
}

} // namespace inet
