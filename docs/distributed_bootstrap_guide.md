# NodeDisk 分布式启动指南（3.0 第一阶段）

这份文档对应 3.0 的第一批目标：

- 有一个常驻 `control_node`
- 有多台可执行任务的 `device_agent`
- 控制节点和 agent 可以建立最小 TCP 控制通道
- agent 可以被调度执行 `backup / restore / share_receive`

当前阶段的定位仍然是：

- 中心控制节点
- 分布式执行 agent
- 文本 TCP 控制协议
- 单控制库（SQLite）

它已经能开始建设真正的分布式系统，但还不是最终形态。

## 1. 推荐拓扑

建议先按下面这个最小拓扑验证：

```text
机器 A:
  control_node
  control database (SQLite)
  可选 mirror / FastDFS 后端

机器 B:
  device_agent
  本地 agent database (SQLite)
  本地待备份目录 / 恢复目录 / 接收目录

机器 C:
  device_agent
  本地 agent database (SQLite)
```

如果要做跨公网或复杂局域网环境，当前阶段推荐：

- 先使用 ZeroTier 官方客户端把机器拉进同一个虚拟局域网
- `control_node` 监听在机器 A 的 ZeroTier 虚拟 IP 上
- 各个 `device_agent` 用这个虚拟 IP 连接控制节点

当前阶段不建议一上来就嵌入 `libzt`。

## 2. 机器准备

所有节点都先执行：

```bash
git clone <your-node-disk-repo>
cd NodeDisk
cmake -S . -B build
cmake --build build -j
```

## 3. 启动控制节点

在机器 A 上执行：

```bash
cd NodeDisk
./scripts/run_control_node.sh /data/nodedisk/control.db 0.0.0.0 18770 300
```

参数含义：

- `control.db`：控制面 SQLite
- `0.0.0.0`：监听地址
- `18770`：控制端口
- `300`：设备离线判定秒数

如果你使用 ZeroTier，建议把第二个参数换成控制节点的虚拟 IP。

## 4. 启动设备代理

在机器 B 或机器 C 上执行：

```bash
cd NodeDisk
./scripts/run_device_agent.sh \
  /data/nodedisk/agent.db \
  <control_host> \
  18770 \
  node-b \
  laptop-b \
  <node_b_zt_ip> \
  mirror \
  /data/nodedisk/store \
  5 \
  0
```

它当前会按轮询周期持续执行：

- 注册
- 心跳
- 拉任务
- 执行任务
- 回报结果

也就是当前的 `serve` 模式。

## 5. 当前可验证的远程任务

当前阶段可以让 agent 远程执行：

- `backup`
- `restore`
- `share_receive`

本地已经验证通过的链路包括：

- agent 执行远程备份任务
- agent 执行远程恢复任务
- agent 执行远程共享接收任务
- 控制节点同步更新：
  - `scheduled_tasks`
  - `restore_requests`
  - `shared_library_recipients`

当前也已经可以通过 `control_node` 服务直接提交这些动作：

- 远程提交备份任务
- 远程提交恢复任务
- 远程提交共享接收任务
- 远程触发失败任务重试

## 6. 当前阶段的限制

这部分很重要，当前跨机部署时要明确：

1. `device_agent` 已经有 `serve` 常驻轮询模式，但还不是完整服务管理形态（例如 systemd、自恢复、自动拉起）。

2. 控制协议当前还是文本 TCP，适合 3.0 第一阶段验证，不是最终 RPC 形态。

3. 控制库当前仍是单 SQLite。
   这意味着：
   - 它适合作为当前单控制节点的元数据存储
   - 还不适合复杂并发写入和真正多主一致性

4. 任务提交的主要动作已经可以通过 `control_node` 服务接口完成，但当前仍然建议先在可信控制节点环境中使用，后续还要继续补更正式的认证和权限控制。

5. 当前 agent 执行共享接收时，文件会落到：

```text
/tmp/nodedisk_agent_materialized_<node_id>/
```

后续会继续演进成正式配置项。

## 7. 这一阶段的目标是什么

你可以把这一阶段定义成：

**“先让控制节点和设备代理真实分开部署，并能跨机完成任务协同。”**

也就是先证明：

- `control_node` 是真正的中心控制节点
- `device_agent` 是真正的分布式执行节点
- 节点之间已经可以围绕任务协同工作

而不是一开始就进入：

- 多主一致性
- 去中心化共识
- 全自动副本修复

## 8. 下一步推荐

完成这一步后，下一步最值得做的是：

1. 正式接入 ZeroTier 官方客户端环境
2. 再考虑更正式的 RPC/HTTP 控制接口
3. 把 agent 的常驻模式继续推进成更完整的服务管理（systemd、日志、自动恢复）
