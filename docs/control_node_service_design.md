# Control Node 服务化设计草案

## 1. 文档目的

这份文档专门回答 3.0 里的第一个关键工程问题：

**`control_node` 应该如何从一个命令行查询入口，演进成一个常驻运行的控制节点服务。**

这一步的目标不是一步做成最终的分布式控制中心，而是先把下面这些基础能力建立起来：

- 有长期运行的控制节点进程
- 有设备代理可以连接的控制面入口
- 有最小的注册、心跳、拉任务能力
- 为后续接入虚拟网络、任务调度和 Web/API 留好边界

## 2. 设计目标

当前阶段，`control_node` 服务化的目标有 4 个：

1. **常驻化**
   - `control_node` 不再只是一次性执行命令
   - 支持以服务模式长期运行

2. **控制面集中化**
   - 设备注册
   - 心跳更新
   - 任务查询
   - 系统摘要
   都通过控制节点统一对外暴露

3. **协议最小化**
   - 第一版不追求复杂 RPC
   - 先采用简单、可调试的文本 TCP 协议

4. **为后续演进预留接口**
   - 后续可以替换成 HTTP / RPC / ASIO / ZeroTier 直连
   - 但当前业务层不需要推翻

## 3. 非目标

当前这一步明确不做：

- 完整 Web 管理端
- 复杂认证鉴权
- 多主控制节点
- 去中心化元数据共识
- 节点间自动副本修复
- 复杂任务编排协议

也就是说，这一阶段的目标是：

**先把“控制节点长期运行 + 设备代理可接入”做出来。**

## 4. 总体结构

当前建议采用：

```text
+---------------------------+
|      control_node         |
|---------------------------|
| ControlNodeServer         |
| ControlPlaneService       |
| SystemRuntime             |
| SQLite Metadata Store     |
+-------------+-------------+
              ^
              |
      text TCP control protocol
              |
+-------------+-------------+
|        device_agent        |
|---------------------------|
| DeviceAgentService        |
| ControlNodeClient         |
| local runtime hooks       |
+---------------------------+
```

这意味着：

- `control_node` 负责控制面状态
- `device_agent` 负责和控制节点通信
- 后续 agent 本地执行备份/恢复/接收任务时，也沿用这套边界

## 5. 服务模式设计

### 5.1 control_node 新增运行模式

在保留原有查询命令的基础上，新增：

```text
control_node serve <db_path> [host] [port] [offline_after_seconds]
```

它的职责：

- 监听 TCP 端口
- 接收设备代理请求
- 调用已有的 `SystemRuntime`
- 返回结构化响应

### 5.2 第一版协议范围

当前第一版协议只做最小闭环：

- `PING`
- `REGISTER`
- `HEARTBEAT`
- `SUMMARY`
- `PULL_TASKS`
- `ACK_TASK`
- `REPORT_TASK`
- `SUBMIT_BACKUP`
- `SUBMIT_RESTORE`
- `SUBMIT_SHARE_RECEIVE`
- `RETRY_TASK`
- `RETRY_RESTORE`
- `RETRY_SHARE`

当前 `device_agent` 也已经有两种运行形态：

- `run-once`
  - 适合单轮任务调试
- `serve`
  - 适合 3.0 第一阶段的常驻轮询运行

到目前为止，3.0 第一阶段已经具备两条完整链路：

1. **执行链服务化**
   - agent 注册
   - agent 心跳
   - agent 拉任务
   - agent 签收
   - agent 回报结果

2. **控制动作服务化**
   - 远程提交 backup / restore / share_receive
   - 远程触发 task / restore / share retry

这样做的原因是：

- 足够把 agent 接进来
- 足够验证控制面常驻化
- 也足够为后续任务执行做准备

## 6. 第一版协议草案

### 6.1 请求形式

第一版采用简单的“单行文本命令”：

```text
PING
REGISTER <node_id> <device_name> <zt_ip> <epoch>
HEARTBEAT <node_id> <epoch>
SUMMARY <offline_after_seconds>
PULL_TASKS <node_id>
```

### 6.2 响应形式

响应也保持纯文本，便于调试：

```text
OK PONG
OK REGISTERED
OK HEARTBEAT
OK SUMMARY devices=2 online_devices=2 queued_tasks=1 ...
OK TASKS 1
TASK ...
END
ERR <message>
```

## 7. device_agent 第一版职责

当前阶段的 `device_agent` 先不做所有执行能力，只做骨架。

第一版建议具备：

- 本地节点信息建模
- 向控制节点注册
- 定期心跳
- 拉取待执行任务
- 打印或缓存待执行任务

也就是说，先把它做成：

**一个最小可运行的控制面客户端。**

## 8. 与当前系统的衔接方式

这一步不应该推翻已有的 2.0 代码，而应该复用：

- `SystemRuntime`
- `ControlPlaneService`
- `DeviceManager`
- `TaskScheduler`
- `MetadataStore`

这样服务化只是新增一层“接入方式”，不是重写控制面。

## 9. 当前阶段的实现建议

建议按下面顺序推进：

1. 新增 `ControlNodeServer`
2. 在 `control_node` 增加 `serve` 子命令
3. 新增 `ControlNodeClient`
4. 新增 `DeviceAgentService`
5. 新增 `device_agent` 入口程序

## 10. 一句话总结

`control_node` 服务化的本质，不是换个进程形式，而是：

**让 NodeDisk 从“本地命令驱动系统”，开始进入“中心控制节点 + 分布式 agent”协同工作的阶段。**
