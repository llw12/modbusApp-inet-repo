//
// Copyright (C) 2025 Your Name
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_MODBUSSTORAGE_H
#define __INET_MODBUSSTORAGE_H

#include <string>
#include <vector>
#include "inet/networklayer/common/L3Address.h"
#include <arpa/inet.h> // 用于htonl、htons等字节序转换函数
#include <nlohmann/json.hpp>
#include <fstream>
#include <iomanip>  // 用于std::setw
#include "nlohmann/ordered_map.hpp"  // 引入有序映射支持

// 定义有序JSON类型（确保字段顺序与写入顺序一致）
using ordered_json = nlohmann::basic_json<nlohmann::ordered_map>;

namespace inet {

// -----------------------------------------------------------------------------
// 1. 模板结构体：RegisterGroup（通用寄存器组，支持线圈/寄存器等不同数据类型）
// -----------------------------------------------------------------------------
template <typename ElementType>
struct RegisterGroup {
    uint16_t startAddress;  // 寄存器组起始地址（Modbus协议地址，如线圈地址0x0000）
    uint16_t number;        // 该组内元素总数（如10个线圈、20个保持寄存器）
    ElementType* data;      // 数据数组指针（动态分配，长度 = number，需手动释放）
};

// -----------------------------------------------------------------------------
// 2. 结构体：MSMapping（单个Modbus从站的寄存器映射配置）
// -----------------------------------------------------------------------------
struct MSMapping {
    uint8_t slaveId;                          // 从站地址（1~247，符合Modbus协议规范，0为广播地址）
    int numBitGroup;                          // 线圈组（BitGroup，离散输出）数量
    int numInputBitGroup;                     // 离散输入组（InputBitGroup）数量
    int numRegisterGroup;                     // 保持寄存器组（RegisterGroup，可读写）数量
    int numInputRegisterGroup;                // 输入寄存器组（InputRegisterGroup，只读）数量
    RegisterGroup<uint8_t>* bitGroup;         // 线圈组数组指针（长度 = numBitGroup）
    RegisterGroup<uint8_t>* inputBitGroup;    // 离散输入组数组指针（长度 = numInputBitGroup）
    RegisterGroup<int16_t>* registerGroup;    // 保持寄存器组数组指针（长度 = numRegisterGroup）
    RegisterGroup<int16_t>* inputRegisterGroup; // 输入寄存器组数组指针（长度 = numInputRegisterGroup）
};

// -----------------------------------------------------------------------------
// 3. 结构体：connect（单个Modbus服务器的连接配置，关联Socket与从站集合）
// -----------------------------------------------------------------------------
struct connect {
    int socketId;              // Socket唯一标识（关联TcpSocket/UdpSocket的socketId）
    L3Address  ipAddress;     // Modbus服务器IP地址（如"192.168.1.10"）
    int numSlave;              // 该服务器下挂载的从站总数
    MSMapping* slaves;         // 从站数组指针（动态分配，长度 = numSlave，需手动释放）
};

// -----------------------------------------------------------------------------
// 4. 核心类：ModbusStorage（管理Modbus服务器连接与从站配置的存储容器）
// -----------------------------------------------------------------------------
class ModbusStorage {
protected:
    int numconnect;                     // 配置的Modbus服务器连接总数（与connectArray.size()同步）
    std::vector<connect> connectArray;  // 动态连接数组（存储所有Modbus服务器连接配置）

public:
    // ------------------------------
    // 构造与析构（内存安全管理）
    // ------------------------------
    // 默认构造函数：初始化空连接集合
    ModbusStorage() : numconnect(0) {}

    // 带参构造函数：指定初始连接数
    explicit ModbusStorage(int initNumConnect) {
        setNumConnect(initNumConnect);  // 调用setter确保合法性检查
    }

    // 析构函数：逐层释放动态内存（避免内存泄漏）
    ~ModbusStorage() {
        clear();
    }

    // 新增：清理当前所有动态分配资源，安全复位
    void clear() {
        for (auto& conn : connectArray) {
            if (conn.slaves != nullptr) {
                for (int slaveIdx = 0; slaveIdx < conn.numSlave; slaveIdx++) {
                    MSMapping& currSlave = conn.slaves[slaveIdx];
                    // 释放线圈组
                    if (currSlave.bitGroup != nullptr) {
                        for (int i = 0; i < currSlave.numBitGroup; ++i) {
                            delete[] currSlave.bitGroup[i].data;
                        }
                        delete[] currSlave.bitGroup;
                        currSlave.bitGroup = nullptr;
                    }
                    // 释放离散输入组
                    if (currSlave.inputBitGroup != nullptr) {
                        for (int i = 0; i < currSlave.numInputBitGroup; ++i) {
                            delete[] currSlave.inputBitGroup[i].data;
                        }
                        delete[] currSlave.inputBitGroup;
                        currSlave.inputBitGroup = nullptr;
                    }
                    // 释放保持寄存器组
                    if (currSlave.registerGroup != nullptr) {
                        for (int i = 0; i < currSlave.numRegisterGroup; ++i) {
                            delete[] currSlave.registerGroup[i].data;
                        }
                        delete[] currSlave.registerGroup;
                        currSlave.registerGroup = nullptr;
                    }
                    // 释放输入寄存器组
                    if (currSlave.inputRegisterGroup != nullptr) {
                        for (int i = 0; i < currSlave.numInputRegisterGroup; ++i) {
                            delete[] currSlave.inputRegisterGroup[i].data;
                        }
                        delete[] currSlave.inputRegisterGroup;
                        currSlave.inputRegisterGroup = nullptr;
                    }
                }
                delete[] conn.slaves;
                conn.slaves = nullptr;
            }
        }
        connectArray.clear();
        numconnect = 0;
    }

    // ------------------------------
    // 核心功能：根据socketId查询连接索引
    // ------------------------------
    /**
     * 根据socketId查找对应的连接在connectArray中的索引
     * @param targetSocketId：待查询的Socket唯一标识
     * @return 找到则返回非负索引（0~numconnect-1），未找到返回-1
     */
    int findConnectIndexBySocketId(int targetSocketId) const {
        for (int idx = 0; idx < (int)connectArray.size(); idx++) {
            if (connectArray[idx].socketId == targetSocketId) {
                return idx;  // 找到匹配的连接，返回索引
            }
        }
        return -1;  // 未找到匹配的socketId
    }

    int findConnectIndexByIpAddress(L3Address targetIp) const {
        for (int idx = 0; idx < (int)connectArray.size(); idx++) {
            if (connectArray[idx].ipAddress == targetIp) {
                return idx;  // 找到匹配的连接，返回索引
            }
        }
        return -1;  // 未找到匹配的socketId
    }


    // ------------------------------
    // 连接管理：Setter/Getter（带合法性检查）
    // ------------------------------
    // 获取当前连接总数
    int getNumConnect() const { return numconnect; }

    // 设置连接总数并调整数组长度（清空原有数据，谨慎使用）
    void setNumConnect(int newNumConnect) {
        numconnect = newNumConnect;
        connectArray.resize(numconnect);  // 调整数组长度，新增元素为默认构造的connect
    }

    // 获取连接数组（只读，用于外部遍历/查询）
    const std::vector<connect>& getConnectArray() const { return connectArray; }

    // 获取连接数组（可写，用于外部修改连接配置）
    std::vector<connect>& getConnectArray() { return connectArray; }

    // 根据索引获取指定连接（带越界检查，返回可修改引用）
    connect& getConnect(int connIdx) {
        return connectArray[connIdx];
    }

    // 根据索引获取指定连接（只读，带越界检查）
    const connect& getConnect(int connIdx) const {
        return const_cast<ModbusStorage*>(this)->getConnect(connIdx);  // 复用非const版本的越界检查
    }

    // ------------------------------
    // 辅助方法：简化连接添加与从站初始化
    // ------------------------------
    // 添加一个新连接（自动更新numconnect）
    void addConnect(const connect& newConn) {
        connectArray.push_back(newConn);
        numconnect = connectArray.size();  // 同步更新连接总数
    }

    // 初始化指定连接的从站数组（动态分配内存，避免重复初始化）
    void initSlavesForConnect(int connIdx, int slaveCount, int socketId, const char * ip) {
        connect& targetConn = getConnect(connIdx);  // 触发越界检查
        // 配置连接基本信息
        targetConn.socketId = socketId;

        targetConn.ipAddress.tryParse(ip);

        // 分配从站数组
        targetConn.numSlave = slaveCount;
        targetConn.slaves = new MSMapping[slaveCount]();  // 默认初始化（成员设为0/nullptr）
    }

    // 从缓冲区读取uint16_t（网络字节序→主机字节序）
    uint16_t deserializeUint16(const std::vector<uint8_t>& buffer, size_t& offset) {
        // 检查缓冲区剩余字节是否足够（至少2字节）
        if (offset + sizeof(uint16_t) > buffer.size()) {
            throw cRuntimeError("deserializeUint16: Buffer underflow (need %zu, has %zu)",
                               sizeof(uint16_t), buffer.size() - offset);
        }
        // 从缓冲区读取2字节并转换为网络字节序的uint16_t
        uint16_t netValue;
        memcpy(&netValue, &buffer[offset], sizeof(netValue));
        offset += sizeof(netValue); // 更新偏移量
        return ntohs(netValue); // 转换为主机字节序
    }

    // 从缓冲区读取int（网络字节序→主机字节序）
    int deserializeInt(const std::vector<uint8_t>& buffer, size_t& offset) {
        if (offset + sizeof(int) > buffer.size()) {
            throw cRuntimeError("deserializeInt: Buffer underflow (need %zu, has %zu)",
                               sizeof(int), buffer.size() - offset);
        }
        int netValue;
        memcpy(&netValue, &buffer[offset], sizeof(netValue));
        offset += sizeof(netValue);
        return ntohl(netValue); // 转换为主机字节序
    }

    // 从缓冲区读取L3Address（支持IPv4/IPv6）
    L3Address deserializeL3Address(const std::vector<uint8_t>& buffer, size_t& offset) {
        if (offset + 1 > buffer.size()) {
            throw cRuntimeError("deserialize ipType: Buffer underflow (need 1, has %zu)",
            buffer.size() - offset);
        }
        uint8_t ipType;
        memcpy(&ipType, &buffer[offset], 1);
        offset += 1;
        if(ipType == 0) {
            // IPv4需要4字节
            if (offset + 4 > buffer.size()) {
                throw cRuntimeError("deserializeL3Address(IPv4): Buffer underflow (need 4, has %zu)",
                                   buffer.size() - offset);
            }
            uint32_t ipv4;
            memcpy(&ipv4, &buffer[offset], 4);
            offset += 4;
            return L3Address(Ipv4Address(ntohl(ipv4))); // 转换为主机字节序
        }
         else if(ipType == 1) {
            // IPv6需要16字节（4个32位字）
            if (offset + 16 > buffer.size()) {
                throw cRuntimeError("deserializeL3Address(IPv6): Buffer underflow (need 16, has %zu)",
                                   buffer.size() - offset);
            }
            uint32_t words[4];
            for (int i = 0; i < 4; i++) {
                memcpy(&words[i], &buffer[offset + i*4], 4);
                words[i] = ntohl(words[i]); // 每个字转换为主机字节序
            }
            offset += 16;
            return L3Address(Ipv6Address(words[0], words[1], words[2], words[3]));
            }
         else{
                throw cRuntimeError("deserializeL3Address: Unsupported address type");
        }
    }

    // 反序列化RegisterGroup（模板函数，对应不同元素类型）
    template <typename ElementType>
    void deserializeRegisterGroup(RegisterGroup<ElementType>& group, const std::vector<uint8_t>& buffer, size_t& offset) {
        // 读取起始地址和数量
        group.startAddress = deserializeUint16(buffer, offset);
        group.number = deserializeUint16(buffer, offset);

        // 读取冗余的数组长度（用于校验）
        uint16_t lengthCheck = deserializeUint16(buffer, offset);
        if (lengthCheck != group.number) {
            throw cRuntimeError("deserializeRegisterGroup: Length mismatch (expected %u, got %u)",
                               group.number, lengthCheck);
        }

        // 读取数据数组
        group.data = new ElementType[group.number]();
        for (uint16_t i = 0; i < group.number; i++) {
            if constexpr (std::is_same_v<ElementType, uint8_t>) {
                // uint8_t直接读取1字节
                if (offset + 1 > buffer.size()) {
                    throw cRuntimeError("deserializeRegisterGroup(uint8_t): Buffer underflow at index %u", i);
                }
                group.data[i] = buffer[offset++];
            }
            else if constexpr (std::is_same_v<ElementType, int16_t>) {
                // int16_t读取2字节并转换
                int16_t netValue;
                if (offset + sizeof(netValue) > buffer.size()) {
                    throw cRuntimeError("deserializeRegisterGroup(int16_t): Buffer underflow at index %u", i);
                }
                memcpy(&netValue, &buffer[offset], sizeof(netValue));
                offset += sizeof(netValue);
                group.data[i] = ntohs(netValue); // 转换为主机字节序
            }
        }
    }

    // 反序列化MSMapping（从站寄存器映射）
    void deserializeMSMapping(MSMapping& mapping, const std::vector<uint8_t>& buffer, size_t& offset) {
        // 读取从站ID（1字节）
        if (offset + 1 > buffer.size()) {
            throw cRuntimeError("deserializeMSMapping: Buffer underflow (slaveId)");
        }
        mapping.slaveId = buffer[offset++];

        // 读取4类寄存器组的数量
        mapping.numBitGroup = deserializeInt(buffer, offset);
        mapping.numInputBitGroup = deserializeInt(buffer, offset);
        mapping.numRegisterGroup = deserializeInt(buffer, offset);
        mapping.numInputRegisterGroup = deserializeInt(buffer, offset);

        // 反序列化线圈组（bitGroup）
        mapping.bitGroup = new RegisterGroup<uint8_t>[mapping.numBitGroup](); // 零初始化;
        for (int i = 0; i < mapping.numBitGroup; i++) {
            deserializeRegisterGroup<uint8_t>(mapping.bitGroup[i], buffer, offset);
        }

        // 反序列化离散输入组（inputBitGroup）
        mapping.inputBitGroup = new RegisterGroup<uint8_t>[mapping.numInputBitGroup](); // 零初始化;
        for (int i = 0; i < mapping.numInputBitGroup; i++) {
            deserializeRegisterGroup<uint8_t>(mapping.inputBitGroup[i], buffer, offset);
        }

        // 反序列化保持寄存器组（registerGroup）
        mapping.registerGroup = new RegisterGroup<int16_t>[mapping.numRegisterGroup](); // 零初始化;
        for (int i = 0; i < mapping.numRegisterGroup; i++) {
            deserializeRegisterGroup<int16_t>(mapping.registerGroup[i], buffer, offset);
        }

        // 反序列化输入寄存器组（inputRegisterGroup）
        mapping.inputRegisterGroup = new RegisterGroup<int16_t>[mapping.numInputRegisterGroup]();
        for (int i = 0; i < mapping.numInputRegisterGroup; i++) {
            deserializeRegisterGroup<int16_t>(mapping.inputRegisterGroup[i], buffer, offset);
        }
    }

    // 反序列化connect（服务器连接配置）
    void deserializeConnect(connect& conn, const std::vector<uint8_t>& buffer, size_t& offset) {
        // 读取socketId和IP地址（假设IP类型为IPv4，实际使用时需根据场景调整）
        conn.socketId = deserializeInt(buffer, offset);
        conn.ipAddress = deserializeL3Address(buffer, offset);

        // 读取从站数量和从站数组
        conn.numSlave = deserializeInt(buffer, offset);
        conn.slaves = new MSMapping[conn.numSlave]();
        for (int i = 0; i < conn.numSlave; i++) {
            deserializeMSMapping(conn.slaves[i], buffer, offset);
        }
    }

    // 反序列化ModbusStorage（顶层容器）
    void deserializeModbusStorage(ModbusStorage* storage, const std::vector<uint8_t>& buffer) {
        if (!storage) {
            throw cRuntimeError("deserializeModbusStorage: storage is null");
        }
        // 反序列化前先清理旧数据，避免内存泄漏
        storage->clear();
        size_t offset = 0; // 跟踪当前读取位置

        // 读取连接总数
        int numConnect = deserializeInt(buffer, offset);
        storage->setNumConnect(numConnect); // 假设ModbusStorage有设置连接数的方法

        // 读取每个连接并填充到storage
        auto& connectArray = storage->getConnectArray(); // 假设返回可修改的连接数组引用
        connectArray.resize(numConnect);
        for (int i = 0; i < numConnect; i++) {
            deserializeConnect(connectArray[i], buffer, offset);
        }

        // 校验是否读取完所有数据（可选，确保没有冗余字节）
        if (offset != buffer.size()) {
            EV_WARN << "deserializeModbusStorage: Extra bytes in buffer (read " << offset << ", total " << buffer.size() << ")" << endl;
        }
    }

    // 将uint16_t转换为网络字节序（大端）并写入缓冲区
    void serializeUint16(uint16_t value, std::vector<uint8_t>& buffer) {
        uint16_t netValue = htons(value); // 主机序→网络序
        buffer.insert(buffer.end(), reinterpret_cast<uint8_t*>(&netValue),
                     reinterpret_cast<uint8_t*>(&netValue) + sizeof(netValue));
    }

    // 将int转换为网络字节序并写入缓冲区
    void serializeInt(int value, std::vector<uint8_t>& buffer) {
        int netValue = htonl(value);
        buffer.insert(buffer.end(), reinterpret_cast<uint8_t*>(&netValue),
                     reinterpret_cast<uint8_t*>(&netValue) + sizeof(netValue));
    }

    void serializeL3Address(const L3Address& addr, std::vector<uint8_t>& buffer) {
        switch (addr.getType()) {
            case L3Address::IPv4: {
                const uint8_t ipType = 0;  //0:ipv4  1:ipv6
                buffer.push_back(ipType);
                auto ipv4Addr = addr.toIpv4();
                // Ipv4Address使用getInt()获取32位地址值，转换为网络字节序后拆分字节
                uint32_t ipv4 = htonl(ipv4Addr.getInt());
                const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&ipv4);
                buffer.insert(buffer.end(), bytes, bytes + 4);
                break;
            }
            case L3Address::IPv6: {
                const uint8_t ipType = 1;  //0:ipv4  1:ipv6
                buffer.push_back(ipType);
                auto ipv6Addr = addr.toIpv6();
                // Ipv6Address使用words()获取4个32位字，逐个转换为网络字节序后拆分字节
                const uint32_t* words = ipv6Addr.words();
                for (int i = 0; i < 4; ++i) {
                    uint32_t word = htonl(words[i]);
                    const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&word);
                    buffer.insert(buffer.end(), bytes, bytes + 4);
                }
                break;
            }
            // 建议添加默认情况处理未知类型
            default:
                // 可根据需求抛出异常或进行其他错误处理
                break;
        }
    }

    // 序列化RegisterGroup（通用模板）
    template <typename ElementType>
    void serializeRegisterGroup(const RegisterGroup<ElementType>& group, std::vector<uint8_t>& buffer) {
        // 序列化起始地址和数量
        serializeUint16(group.startAddress, buffer);
        serializeUint16(group.number, buffer);

        // 序列化数据数组（先写长度，再写每个元素）
        serializeUint16(group.number, buffer); // 再次确认数组长度（冗余校验）
        for (uint16_t i = 0; i < group.number; i++) {
            if constexpr (std::is_same_v<ElementType, uint8_t>) {
                buffer.push_back(group.data[i]); // uint8_t直接写入1字节
            }
            else if constexpr (std::is_same_v<ElementType, int16_t>) {
                int16_t netValue = htons(group.data[i]); // 转换为网络序
                buffer.insert(buffer.end(), reinterpret_cast<uint8_t*>(&netValue),
                             reinterpret_cast<uint8_t*>(&netValue) + sizeof(netValue));
            }
        }
    }

    void serializeMSMapping(const MSMapping& mapping, std::vector<uint8_t>& buffer) {
        // 序列化从站ID
        buffer.push_back(mapping.slaveId); // uint8_t占1字节

        // 序列化4类寄存器组的数量
        serializeInt(mapping.numBitGroup, buffer);
        serializeInt(mapping.numInputBitGroup, buffer);
        serializeInt(mapping.numRegisterGroup, buffer);
        serializeInt(mapping.numInputRegisterGroup, buffer);

        // 序列化线圈组（bitGroup）
        for (int i = 0; i < mapping.numBitGroup; i++) {
            serializeRegisterGroup(mapping.bitGroup[i], buffer);
        }

        // 序列化离散输入组（inputBitGroup）
        for (int i = 0; i < mapping.numInputBitGroup; i++) {
            serializeRegisterGroup(mapping.inputBitGroup[i], buffer);
        }

        // 序列化保持寄存器组（registerGroup）
        for (int i = 0; i < mapping.numRegisterGroup; i++) {
            serializeRegisterGroup(mapping.registerGroup[i], buffer);
        }

        // 序列化输入寄存器组（inputRegisterGroup）
        for (int i = 0; i < mapping.numInputRegisterGroup; i++) {
            serializeRegisterGroup(mapping.inputRegisterGroup[i], buffer);
        }
    }

    void serializeConnect(const connect& conn, std::vector<uint8_t>& buffer) {
        // 序列化socketId和IP地址
        serializeInt(conn.socketId, buffer);
        serializeL3Address(conn.ipAddress, buffer);

        // 序列化从站数量和从站数组
        serializeInt(conn.numSlave, buffer);
        for (int i = 0; i < conn.numSlave; i++) {
            serializeMSMapping(conn.slaves[i], buffer);
        }
    }


    std::vector<uint8_t> serializeModbusStorage(ModbusStorage* storage) {
        std::vector<uint8_t> buffer;

        if (!storage) {
            EV_ERROR << "ModbusStorage is null, cannot serialize" << endl;
            return buffer; // 返回空vector
        }

        // 序列化连接总数
        int numConnect = storage->getNumConnect();
        serializeInt(numConnect, buffer);

        // 序列化每个连接
        const auto& connectArray = storage->getConnectArray();
        for (const auto& conn : connectArray) {
            serializeConnect(conn, buffer);
        }

        return buffer;
    }

    // 新增：带总长度前缀的序列化（前4字节：totalLen，网络序）
    std::vector<uint8_t> serializeModbusStorageWithLength(ModbusStorage* storage) {
        std::vector<uint8_t> body = serializeModbusStorage(storage);
        std::vector<uint8_t> buffer;
        int totalLen = body.size();
        int netLen = htonl(totalLen);
        const uint8_t *lenBytes = reinterpret_cast<const uint8_t*>(&netLen);
        buffer.insert(buffer.end(), lenBytes, lenBytes + sizeof(netLen));
        buffer.insert(buffer.end(), body.begin(), body.end());
        return buffer;
    }

    // 新增：尝试按长度前缀解析一个完整ModbusStorage消息（支持增量接收）
    // buffer: 可能包含多个消息或半包；consumed 返回已消费字节数；成功返回true
    bool tryDeserializeModbusStorage(ModbusStorage* storage, const std::vector<uint8_t>& buffer, size_t& consumed) {
        consumed = 0;
        if (!storage)
            return false;
        // 需要至少4字节的长度前缀
        if (buffer.size() < sizeof(int))
            return false;
        int netLen;
        memcpy(&netLen, buffer.data(), sizeof(int));
        int totalLen = ntohl(netLen);
        if (totalLen < 0) {
            EV_WARN << "tryDeserializeModbusStorage: invalid totalLen=" << totalLen << endl;
            return false;
        }
        // 等待完整消息到达
        if (buffer.size() < sizeof(int) + static_cast<size_t>(totalLen))
            return false;
        // 提取消息体
        std::vector<uint8_t> body(buffer.begin() + sizeof(int), buffer.begin() + sizeof(int) + totalLen);
        // 反序列化（捕获缓冲区下溢异常，避免中断仿真）
        try {
            deserializeModbusStorage(storage, body);
        }
        catch (const cRuntimeError &e) {
            EV_ERROR << "tryDeserializeModbusStorage: deserialization failed: " << e.what() << endl;
            return false; // 不消费，等待外部处理或丢弃策略
        }
        consumed = sizeof(int) + totalLen;
        return true;
    }

    /**
     * 将ModbusStorage数据保存为JSON文件，格式与ModbusStorageConfig.json一致
     * @param filePath 保存路径
     */
    void saveToJson(const std::string& filePath) const {
        ordered_json j;  // 使用有序JSON
        ordered_json connectArrayJson = ordered_json::array();  // 数组也使用有序类型

        // 遍历所有连接
        for (const auto& conn : connectArray) {
            ordered_json connectJson;
            connectJson["ipAddress"] = conn.ipAddress.str();  // IP地址转为字符串
            connectJson["numSlave"] = conn.numSlave;

            ordered_json slavesJson = ordered_json::array();
            // 遍历每个从站
            for (int slaveIdx = 0; slaveIdx < conn.numSlave; ++slaveIdx) {
                const MSMapping& slave = conn.slaves[slaveIdx];
                ordered_json slaveJson;

                // 从站基本信息（严格按原配置顺序写入）
                slaveJson["slaveId"] = slave.slaveId;
                slaveJson["numBitGroup"] = slave.numBitGroup;
                slaveJson["numInputBitGroup"] = slave.numInputBitGroup;
                slaveJson["numRegisterGroup"] = slave.numRegisterGroup;
                slaveJson["numInputRegisterGroup"] = slave.numInputRegisterGroup;

                // 线圈组 (bitGroup)
                ordered_json bitGroupsJson = ordered_json::array();
                for (int groupIdx = 0; groupIdx < slave.numBitGroup; ++groupIdx) {
                    const auto& group = slave.bitGroup[groupIdx];
                    ordered_json groupJson;
                    groupJson["startAddress"] = group.startAddress;
                    groupJson["number"] = group.number;
                    groupJson["data"] = ordered_json::array();  // 数据数组
                    for (uint16_t dataIdx = 0; dataIdx < group.number; ++dataIdx) {
                        groupJson["data"].push_back(group.data[dataIdx]);
                    }
                    bitGroupsJson.push_back(groupJson);
                }
                slaveJson["bitGroup"] = bitGroupsJson;

                // 离散输入组 (inputBitGroup)
                ordered_json inputBitGroupsJson = ordered_json::array();
                for (int groupIdx = 0; groupIdx < slave.numInputBitGroup; ++groupIdx) {
                    const auto& group = slave.inputBitGroup[groupIdx];
                    ordered_json groupJson;
                    groupJson["startAddress"] = group.startAddress;
                    groupJson["number"] = group.number;
                    groupJson["data"] = ordered_json::array();
                    for (uint16_t dataIdx = 0; dataIdx < group.number; ++dataIdx) {
                        groupJson["data"].push_back(group.data[dataIdx]);
                    }
                    inputBitGroupsJson.push_back(groupJson);
                }
                slaveJson["inputBitGroup"] = inputBitGroupsJson;

                // 保持寄存器组 (registerGroup)
                ordered_json registerGroupsJson = ordered_json::array();
                for (int groupIdx = 0; groupIdx < slave.numRegisterGroup; ++groupIdx) {
                    const auto& group = slave.registerGroup[groupIdx];
                    ordered_json groupJson;
                    groupJson["startAddress"] = group.startAddress;
                    groupJson["number"] = group.number;
                    groupJson["data"] = ordered_json::array();
                    for (uint16_t dataIdx = 0; dataIdx < group.number; ++dataIdx) {
                        groupJson["data"].push_back(group.data[dataIdx]);
                    }
                    registerGroupsJson.push_back(groupJson);
                }
                slaveJson["registerGroup"] = registerGroupsJson;

                // 输入寄存器组 (inputRegisterGroup)
                ordered_json inputRegisterGroupsJson = ordered_json::array();
                for (int groupIdx = 0; groupIdx < slave.numInputRegisterGroup; ++groupIdx) {
                    const auto& group = slave.inputRegisterGroup[groupIdx];
                    ordered_json groupJson;
                    groupJson["startAddress"] = group.startAddress;
                    groupJson["number"] = group.number;
                    groupJson["data"] = ordered_json::array();
                    for (uint16_t dataIdx = 0; dataIdx < group.number; ++dataIdx) {
                        groupJson["data"].push_back(group.data[dataIdx]);
                    }
                    inputRegisterGroupsJson.push_back(groupJson);
                }
                slaveJson["inputRegisterGroup"] = inputRegisterGroupsJson;

                slavesJson.push_back(slaveJson);
            }

            connectJson["slaves"] = slavesJson;
            connectArrayJson.push_back(connectJson);
        }

        j["connectArray"] = connectArrayJson;

        // 写入文件（带缩进，增强可读性）
        std::ofstream file(filePath);
        if (!file.is_open()) {
            throw std::runtime_error("无法打开文件写入: " + filePath);
        }
        file << std::setw(4) << j << std::endl;  // 缩进4空格
    }

    };


} // namespace inet

#endif // __INET_MODBUSSTORAGE_H
