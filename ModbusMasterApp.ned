//
// Copyright (C) 2025 Your Name
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

package inet.applications.tcpapp;

import inet.applications.contract.IApp;

//
// Modbus TCP 主站应用模块，用于定时读取多个 Modbus 从站的寄存器/线圈数据。
// 基于 ModbusTcpAppBase 实现，支持多服务器连接管理（通过 SocketMap），从站配置从 JSON 文件加载，
// 兼容 IPv4 和 IPv6 网络，服务器端口默认遵循 Modbus TCP 标准（502）。
//
// 核心功能：
// 1. 从 JSON 配置文件（configFile）加载从站映射（线圈、离散输入、保持寄存器、输入寄存器）；
// 2. 按 readInterval 定时向所有从站发送读请求（功能码 0x01/0x02/0x03/0x04）；
// 3. 接收从站响应并更新 ModbusStorage 存储。
//
// 配置示例（omnetpp.ini）：
// <pre>
// **.modbusMaster.configFile = "ModbusStorageConfig.json"  # 从站配置文件路径
// **.modbusMaster.numConnect = 2                         # 连接的 Modbus 服务器数量（需与 JSON 中 connectArray 长度一致）
// </pre>
//
// @see ModbusTcpAppBase, ModbusStorage, ModbusHeader, ModbusStorageConfig.json
//
simple ModbusMasterApp like IApp
{
    parameters:
        // ------------------------------
        // 网络连接参数（参考 TcpBasicClientApp，适配 Modbus TCP 标准）
        // ------------------------------
        string localAddress = default("");  // 本地绑定地址（空串表示绑定所有网卡）
        int localPort = default(-1);        // 本地绑定端口（-1 表示自动分配）
        int connectPort = default(502);     // Modbus TCP 标准端口（默认 502，不建议修改）

        // ------------------------------
        // Modbus 特有配置参数
        // ------------------------------
        string configFile = default("ModbusStorageConfig.json");  // 从站配置 JSON 文件路径（必填）connectArray顺序与IP列表一致
        int numConnect = default(1);  // Modbus 服务器连接总数（需与 JSON 中 connectArray 长度一致）
        volatile double readInterval @unit(s) = default(1s);  // 定时读取间隔（如 1s 表示每秒读取一次）

        // ------------------------------
        // QoS 与生命周期参数
        // ------------------------------
        int timeToLive = default(-1);  // TTL（IPv4）/跳数限制（IPv6），-1 表示使用默认值
        int dscp = default(-1);        // DSCP 优先级，-1 表示不设置
        int tos = default(-1);         // TOS（IPv4）/流量类别（IPv6），-1 表示不设置
        @display("i=block/app");  // 界面图标（可自定义）
        @lifecycleSupport;  // 支持生命周期管理（启动/停止/崩溃处理）
        double stopOperationExtraTime @unit(s) = default(-1s);  // 停止操作额外等待时间
        double stopOperationTimeout @unit(s) = default(2s);    // 停止操作超时时间

        // ------------------------------
        // 信号与统计参数
        // ------------------------------
        // 基础信号（参考 TcpBasicClientApp）
        @signal[packetSent](type=inet::Packet);        // 发送数据包信号（含请求报文）
        @signal[packetReceived](type=inet::Packet);    // 接收数据包信号（含响应报文）
        @signal[connect](type=long);                 // 连接状态信号（1=建立，-1=关闭）

        // 基础统计（参考 TcpBasicClientApp）
        @statistic[packetReceived](title="接收数据包总数"; source=packetReceived; record=count,"sum(packetBytes)","vector(packetBytes)"; interpolationmode=none);
        @statistic[packetSent](title="发送数据包总数"; source=packetSent; record=count,"sum(packetBytes)","vector(packetBytes)"; interpolationmode=none);
        @statistic[endToEndDelay](title="端到端延迟"; source="dataAge(packetReceived)"; unit=s; record=histogram,weightedHistogram,vector; interpolationmode=none);
        @statistic[numActiveSessions](title="活跃连接数"; source=warmup(sum(connect)); record=max,timeavg,vector; interpolationmode=sample-hold; autoWarmupFilter=false);
        @statistic[numSessions](title="总连接数"; source="sum(connect+1)/2"; record=last);


    gates:
        // 与 TCP 层交互的闸门（完全参考 TcpBasicClientApp，确保协议命令路由正确）
        input socketIn @labels(TcpCommand/up);    // 接收 TCP 层指示（连接建立、数据到达、连接关闭）
        output socketOut @labels(TcpCommand/down); // 发送 TCP 层命令（打开连接、发送请求、关闭连接）
}
