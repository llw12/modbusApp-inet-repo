# ModbusApp-INET

基于 OMNeT++ 6.1 + INET 的 Modbus 应用套件，用于在仿真环境中构建主站/从站、运维工作站、转发中枢、以及与真实设备联动（HIL, Hardware-in-the-Loop）的端到端链路。本仓库的核心位于 modbusapp 目录，包含多种 App、Modbus 报文定义与序列化器、以及统一的存储配置组件。

提示
- 本 README 聚焦 modbusapp 目录下 App 的作用与用法，并给出可直接使用的参数与示例。
- 生成代码中的 opp_msgtool 版本为 6.1，建议使用 OMNeT++ 6.1 及与之匹配的 INET 版本。

--------------------------------------------------------------------------------

目录结构（modbusapp 关键文件）

应用模块（Apps）
- ModbusMasterApp.{cc,h,ned}：Modbus 主站，定时轮询多个从站，解析响应，写入 ModbusStorage。
- ModbusSlaveApp.{cc,h,ned}：Modbus 从站（纯仿真版），按配置响应各类功能码读写。
- ModbusSlaveHILApp.{cc,h,ned}：从站 HIL 版本，收到仿真内 TCP 报文后，转发到真实外部设备并将响应带回仿真。
- OperatorStationApp.{cc,h,ned}：运维工作站（简版），周期向 ModbusTcpServerApp 发送 ListMsg，接收并反序列化 ModbusStorage。
- OperatorStationApp2.{cc,h,ned}：运维工作站（增强版），按配置好的多条 Modbus 命令与发送时刻序列，生成 OperatorRequest 指令流。
- ModbusTcpServerApp.{cc,h,ned}：简单 TCP 服务器，接收 ListMsg 请求，返回主站的 ModbusStorage 快照（用于运维侧拉取现状）。
- TransitApp.{cc,h,ned}：转发中枢，接收 OperatorRequest，将其转换为主站请求加入队列，并把响应回送给指令来源。
- ModbusTcpAppBase.{cc,h}：主站基类，负责 JSON 配置解析、连接管理、发送/接收基础流程。

数据与序列化
- ModbusHeader.msg/.m.h/.m.cc + ModbusHeaderSerializer.{h,cc}：Modbus TCP MBAP 头部及序列化器。
- ModbusResponseHeader.msg/.m.h/.m.cc：响应头定义（如需）。
- OperatorRequest.msg/.m.h/.m.cc + OperatorRequestSerializer.{h,cc}：运维/转发通道使用的操作请求结构体与序列化器。
- ListMsg.msg/.m.h/.m.cc + ListMsgSerializer.{h,cc}：用于“列表/快照”请求的小消息类型（运维侧拉取用途）。

统一存储
- ModbusStorage.h：核心数据容器。统一管理 connect（服务器连接）、从站寄存器映射（线圈/离散输入/保持寄存器/输入寄存器）、序列化/反序列化（字节流与 JSON）。

--------------------------------------------------------------------------------

整体架构与典型拓扑

- 主站节点（Host-Master）
  - app[0]: ModbusMasterApp（必须为 app[0]，被其他模块按索引引用）
  - app[2]: TransitApp（必须为 app[2]，ModbusMasterApp 会按此索引查找它进行转发响应返回）

- 从站节点（Host-Slave）
  - app[0]: ModbusSlaveApp（仿真从站），或
  - app[0]: ModbusSlaveHILApp（与真实设备互通）

- 运维节点（Host-Operator）
  - 方案一：OperatorStationApp + ModbusTcpServerApp（在主站所在宿主上）
    - OperatorStationApp 连接 ModbusTcpServerApp，发送 ListMsg 拉取主站 ModbusStorage 快照
  - 方案二：OperatorStationApp2 + TransitApp（与主站共宿主）
    - OperatorStationApp2 发送 OperatorRequest 至 TransitApp
    - TransitApp 将请求组装为主站 Modbus 请求，加入发送队列，并把响应回送

注意：部分模块通过 findModuleByPath 使用固定 app 索引互相引用，务必按上面索引放置：
- ModbusTcpServerApp 通过 "^.app[0]" 获取 ModbusMasterApp
- ModbusMasterApp 通过 "^.app[2]" 获取 TransitApp

--------------------------------------------------------------------------------

各 App 作用与用法

1) ModbusMasterApp（主站）
- 作用
  - 定时轮询多个 Modbus 从站（功能码 0x01/0x02/0x03/0x04），接收响应、解析并写入 ModbusStorage。
  - 支持由 TransitApp 注入的请求进入发送队列。
- 关键参数（见 ModbusMasterApp.ned）
  - string configFile = "ModbusStorageConfig.json"：从站映射配置
  - int numConnect：Modbus 服务器连接条目数（需与 JSON connectArray 长度一致）
  - double readInterval：周期轮询间隔（例如 1s）
  - 网络/QoS：localAddress/localPort/timeToLive/dscp/tos
- 行为要点
  - 初始化时 parseConfigFile() 读取 JSON，connectAll() 建立到每个服务器（connectArray[i].ipAddress）的 TCP:502 连接，并记录 socketId。
  - generateQueryPacket() 周期生成所有读请求加入 sendSocketQueue；handleTimer() 对队列进行分发。
  - socketDataArrived() 将响应与等待队列匹配，parseAndStoreResponse() 写入 ModbusStorage。
  - 与 TransitApp 协作：TransitApp 注入写请求到队列；Master 按 transactionId 顺序发送并在收到响应后通过 TransitApp 回传。
- 示例 ini 片段
  - JSON 结构见“配置文件 ModbusStorageConfig.json”。

2) ModbusSlaveApp（从站，仿真）
- 作用
  - 监听 TCP:502，按配置存储响应读写请求（0x01/0x02/0x03/0x04/0x05/0x06/0x0F/0x10）。
- 关键参数（见 ModbusSlaveApp.ned）
  - string localAddress = ""；int localPort（NED 默认 1000）
  - string slavesConfigPath = "ModbusStorageConfig.json"
- 注意
  - 代码中使用的绑定端口为类成员 localPort=502（未从 par 提取），建议在部署时确保端口一致（把 NED 中 localPort 设置为 502，以避免困惑）。
- 行为要点
  - 从 slavesConfigPath JSON 中挑出与本机 IP 匹配的 connect 条目，装载各组寄存器到 ModbusStorage。
  - 收到请求后查找目标组并按 Modbus 协议构造响应。

3) ModbusSlaveHILApp（从站，HIL 联动）
- 作用
  - 在仿真内监听 TCP（供主站连接），同时创建本机 OS socket 主动连接真实设备（remoteAddress:remotePort）。
  - 收到仿真内 ModbusHeader+PDU 后，借助序列化器转为字节流，经 OS socket 发给真实设备；将真实响应反序列化后回送仿真内连接。
- 关键参数（见 .ned）
  - localAddress/localPort（仿真内监听）
  - remoteAddress/remotePort（真实设备）
- 要点
  - 需要已注册的 ModbusHeaderSerializer 与 BytesChunkSerializer（已在 cc 中 Register_Serializer）。

4) ModbusTcpServerApp（面向运维的快照服务）
- 作用
  - 监听 TCP，收到 ListMsg 后，从同宿主的 ModbusMasterApp（app[0]）获取 ModbusStorage 快照，序列化为 BytesChunk 回发。
- 关键参数（见 .ned）
  - localAddress/localPort（默认为 1000，建议按需调整）
  - replyDelay（可选）
- 拓扑要求
  - 必须与 ModbusMasterApp 同宿主，且 Master 位于 app[0] 索引（代码按此路径查找）。

5) OperatorStationApp（运维站，简版）
- 作用
  - 周期向 ModbusTcpServerApp 发送 ListMsg，收到返回的 ModbusStorage 字节流后，调用 storage->deserializeModbusStorage() 反序列化，并可保存结果到 results。
- 关键参数（见 .ned）
  - connectAddress/connectPort：连接 ModbusTcpServerApp
  - startTime/stopTime/interval：发送时序
- 用法
  - 与 ModbusTcpServerApp 对接，适合做“状态快照拉取”。

6) OperatorStationApp2（运维站，增强版）
- 作用
  - 支持按多条命令与独立发送时刻下发 OperatorRequest 给 TransitApp。
  - 命令格式通过参数 modbusRequest 和 sendTime 描述，并支持 seed 控制抖动的可重复性。
- 关键参数（见 .ned）
  - string modbusRequest：多条命令用分号分隔；每条命令形如
    - targetHostName slaveId functionCode startAddress number [data...]
    - 例如："client[0] 02 06 0014 0003 00 01 00 02 00 03; client[1] 01 10 0000 0002 00 0A 00 0B"
    - slaveId/functionCode 为 2 位十六进制，地址/数量为 4 位十六进制，data 为十六进制字节序列
  - string sendTime：与命令条数一致、严格递增的发送秒数列表（空格分隔），可配合 seed 产生小扰动
  - connectAddress/connectPort：连接 TransitApp 的地址与端口
- 工作流
  - 建连后按 actualTimes 调度发送每条 OperatorRequest。TransitApp 收到后进一步驱动主站下发真实 Modbus 报文。

7) TransitApp（运维到主站的转发中枢）
- 作用
  - 监听 TCP，接收 OperatorRequest。解析目标宿主名、从站地址、功能码、起止地址、数量，以及可能的数据段。
  - 通过 ModbusMasterApp::createRequest(...) 生成 Modbus 请求，加入主站的发送队列，并将响应匹配后回送 Operator 端。
- 关键参数（见 .ned）
  - localAddress/localPort（对接 OperatorStationApp2）
  - replyDelay（可选）
- 拓扑要求
  - 必须与 ModbusMasterApp 同宿主，且 Transit 位于 app[2] 索引（主站按此索引获取 Transit）。

--------------------------------------------------------------------------------

Modbus 报文与序列化

- ModbusHeader（MBAP 头）
  - 字段：transactionId, protocolId, length, slaveId
  - 序列化器：ModbusHeaderSerializer（大端序）
  - 主站/从站均基于该 Chunk 进行请求/响应的帧化；length = 1(slaveId) + PDU长度

- ModbusResponseHeader（可选）
  - 在需要单独携带 functionCode 的场景可使用

- OperatorRequest
  - 专用于运维/转发通道，包含 targetHostName、transactionId、protocolId、length、slaveId、functionCode、startAddress、quantity，以及可选 data（紧随其后）
  - 序列化器：OperatorRequestSerializer（字符串先长后内容，其余字段大端序）

- ListMsg
  - 轻量消息：sequenceNumber + ifList（bool），用于请求主站存储快照
  - 序列化器：ListMsgSerializer

--------------------------------------------------------------------------------

ModbusStorage（统一数据容器）

- 结构
  - connect[]：每个 Modbus 服务器连接，记录 socketId、ipAddress、numSlave、以及每个从站 MSMapping
  - MSMapping：含 slaveId、四类寄存器组数量与数据：
    - bitGroup（线圈，uint8_t）
    - inputBitGroup（离散输入，uint8_t）
    - registerGroup（保持寄存器，int16_t）
    - inputRegisterGroup（输入寄存器，int16_t）
  - RegisterGroup<T>：startAddress、number、data[]

- 能力
  - 字节流序列化/反序列化（用于网络传输/快照）
  - JSON 保存（results/节点名.json）
  - 实用查找：findConnectIndexBySocketId / findConnectIndexByIpAddress 等

- JSON 配置（示例骨架）
```json
{
  "connectArray": [
    {
      "ipAddress": "10.0.0.2",
      "numSlave": 1,
      "slaves": [
        {
          "slaveId": 1,
          "numBitGroup": 1,
          "numInputBitGroup": 0,
          "numRegisterGroup": 1,
          "numInputRegisterGroup": 0,
          "bitGroup": [
            { "startAddress": 0, "number": 8, "data": [0,0,0,0,0,0,0,0] }
          ],
          "registerGroup": [
            { "startAddress": 0, "number": 4, "data": [100,101,102,103] }
          ],
          "inputBitGroup": [],
          "inputRegisterGroup": []
        }
      ]
    }
  ]
}
```

--------------------------------------------------------------------------------

快速上手

1. 环境准备
- OMNeT++ 6.1，匹配的 INET 版本（确保 opp_msgtool 6.1）
- 载入 OMNeT++ 环境变量（Linux/macOS: source omnetpp/setenv）

2. 编译
- 在工程根目录执行 make（或通过 OMNeT++ IDE 导入工程后 Build）

3. 运行（示例）
- 主站 + 从站 + 运维（快照拉取）拓扑思路：
  - 在主站宿主添加 app[0]=ModbusMasterApp, app[1]=ModbusTcpServerApp, app[2]=TransitApp
  - 在从站宿主添加 ModbusSlaveApp（或 ModbusSlaveHILApp）
  - 在运维宿主添加 OperatorStationApp（或 OperatorStationApp2）

- 示例 omnetpp.ini 片段（主站）
```
*.master.app[0].typename = "ModbusMasterApp"
*.master.app[0].configFile = "ModbusStorageConfig.json"
*.master.app[0].numConnect = 2
*.master.app[0].readInterval = 1s

*.master.app[1].typename = "ModbusTcpServerApp"
*.master.app[1].localPort = 1000  # 运维快照服务端口

*.master.app[2].typename = "TransitApp"
*.master.app[2].localPort = 1001  # 运维命令转发端口
```

- 示例（从站）
```
*.slave.app[0].typename = "ModbusSlaveApp"
*.slave.app[0].slavesConfigPath = "ModbusStorageConfig.json"
# 注意：类中绑定端口固定为 502；建议 NED 也统一设置为 502
```

- 示例（运维-快照拉取）
```
*.operator.app[0].typename = "OperatorStationApp"
*.operator.app[0].connectAddress = "master"
*.operator.app[0].connectPort = 1000
*.operator.app[0].startTime = 1s
*.operator.app[0].interval = 5s
```

- 示例（运维-命令下发）
```
*.operator.app[0].typename = "OperatorStationApp2"
*.operator.app[0].connectAddress = "master"
*.operator.app[0].connectPort = 1001
*.operator.app[0].modbusRequest = "client[0] 01 03 0000 0002; client[1] 02 06 0014 0001 00 64"
*.operator.app[0].sendTime = "1.0 2.5"
*.operator.app[0].seed = 12345
```

--------------------------------------------------------------------------------

常见问题与排查

- 模块索引不匹配
  - Master 需位于 app[0]（被 TcpServer/Transit 查找），Transit 需位于 app[2]（被 Master 查找），否则 findModuleByPath 将失败。
- 端口配置不一致
  - 从站 NED 默认 localPort=1000，但 ModbusSlaveApp 代码绑定 502；请统一为 502，以符合 Modbus TCP。
- JSON 与 numConnect 不一致
  - numConnect 必须与配置文件 connectArray 长度一致，否则会有警告或连接缺失。
- HIL 场景失败
  - 检查 ModbusHeaderSerializer/BytesChunkSerializer 是否已注册（代码中 Register_Serializer 已处理），以及外部设备 IP/端口连通性。

--------------------------------------------------------------------------------

License 与致谢

- 详见源文件头部注释（大多采用 LGPL-3.0-or-later 样式标注）。
- 感谢 OMNeT++ 与 INET 社区提供的仿真框架支持。

--------------------------------------------------------------------------------

附：modbusapp 关键文件清单（分类）

- Apps
  - ModbusMasterApp.cc/.h/.ned
  - ModbusSlaveApp.cc/.h/.ned
  - ModbusSlaveHILApp.cc/.h/.ned
  - ModbusTcpServerApp.cc/.h/.ned
  - OperatorStationApp.cc/.h/.ned
  - OperatorStationApp2.cc/.h/.ned
  - TransitApp.cc/.h/.ned
  - ModbusTcpAppBase.cc/.h

- Messages & Serializers
  - ModbusHeader.msg / ModbusHeader_m.h / ModbusHeader_m.cc / ModbusHeaderSerializer.h / ModbusHeaderSerializer.cc
  - ModbusResponseHeader.msg / ModbusResponseHeader_m.h / ModbusResponseHeader_m.cc
  - OperatorRequest.msg / OperatorRequest_m.h / OperatorRequest_m.cc / OperatorRequestSerializer.h / OperatorRequestSerializer.cc
  - ListMsg.msg / ListMsg_m.h / ListMsg_m.cc / ListMsgSerializer.h / ListMsgSerializer.cc

- Storage
  - ModbusStorage.h
