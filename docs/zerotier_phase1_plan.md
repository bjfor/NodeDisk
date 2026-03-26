# NodeDisk ZeroTier 第一阶段接入方案

这份文档对应 3.0 当前最现实的一步：

**不急着嵌入 `libzt`，先复用 ZeroTier 官方客户端，把 `control_node + device_agent` 真正跑在两台机器上。**

## 1. 这一阶段的目标

第一阶段只验证 4 件事：

1. 两台机器加入同一个 ZeroTier 私有网络
2. `control_node` 能监听在控制节点的虚拟 IP 上
3. `device_agent` 能通过虚拟 IP 连接控制节点
4. 远程提交任务后，agent 能真实拉取并执行

这一步不追求：

- `libzt` 内嵌
- 节点自动发现
- P2P 文件直连优化
- 真正多主一致性

## 2. 为什么先用 ZeroTier 官方客户端

因为当前 3.0 的关键难点不在 NAT 穿透，而在：

- 控制节点与 agent 的角色划分
- 任务调度
- 状态回写
- 跨机运行稳定性

先用成熟虚拟网络把机器连起来，可以让你把精力放在系统主线，而不是底层组网。

## 3. 当前 NodeDisk 已经具备的前提

当前代码已经有：

- 常驻 `control_node`
- 常驻轮询 `device_agent`
- `backup / restore / share_receive` 远程执行链
- `submit-backup / submit-restore / submit-share-receive`
- 任务状态、恢复状态、共享接收状态回写

也就是说：

**现在 ZeroTier 主要是把“本机 127.0.0.1 调试”换成“跨机虚拟 IP 调试”。**

## 4. 推荐双机拓扑

### 机器 A

- 角色：控制节点
- 进程：`control_node`
- ZeroTier 虚拟 IP：例如 `10.147.17.10`

### 机器 B

- 角色：设备代理
- 进程：`device_agent`
- ZeroTier 虚拟 IP：例如 `10.147.17.11`

## 5. 准备步骤

### 5.1 两台机器都安装 ZeroTier 官方客户端

安装完成后：

```bash
zerotier-cli join <network_id>
```

然后在 ZeroTier 控制台里授权这两台设备。

### 5.2 确认虚拟 IP

两台机器分别执行：

```bash
zerotier-cli info
zerotier-cli listnetworks
```

记录各自的虚拟 IP。

## 6. 推荐配置文件

控制节点示例：

- [control_node.env.example](/home/hjh/hjh_project/yun_cunchu/AI_YunCunChu/netdisk_v1/configs/control_node.env.example)

设备代理示例：

- [device_agent.env.example](/home/hjh/hjh_project/yun_cunchu/AI_YunCunChu/netdisk_v1/configs/device_agent.env.example)

建议你复制成实际配置：

```bash
cp configs/control_node.env.example configs/control_node.env
cp configs/device_agent.env.example configs/device_agent.env
```

然后把：

- `NODEDISK_CONTROL_HOST`
- `NODEDISK_ZT_IP`
- `NODEDISK_NODE_ID`
- 数据库路径
- 存储路径

改成你自己的真实值。

## 7. 启动方式

### 7.1 控制节点

在机器 A 上：

```bash
./scripts/run_control_node_env.sh ./configs/control_node.env
```

### 7.2 设备代理

在机器 B 上：

```bash
./scripts/run_device_agent_env.sh ./configs/device_agent.env
```

## 8. 双机第一轮验证顺序

建议按这个顺序来：

1. 机器 A 启动 `control_node`
2. 机器 B 启动 `device_agent`
3. 在机器 A 上通过控制面提交任务：

```bash
./build/control_node submit-backup <control_zt_ip> 18770 job-z1 node-b /data/test-src
```

如果已经有共享条目或备份文件，也可以继续：

```bash
./build/control_node submit-restore <control_zt_ip> 18770 <request_id> <file_id> node-a node-b /data/restore/x.txt
./build/control_node submit-share-receive <control_zt_ip> 18770 <entry_id> node-b
```

4. 用控制面观察执行结果：

```bash
./build/control_node summary /data/nodedisk/control.db 300 --json
./build/control_node tasks /data/nodedisk/control.db all --json
./build/control_node restores /data/nodedisk/control.db --json
./build/control_node shared /data/nodedisk/control.db node-b all --json
```

## 9. 验收标准

第一阶段接入通过的标准建议定成：

- 两台机器 ZeroTier 网络互通
- `device_agent` 可以跨机连到 `control_node`
- 远程提交的备份任务能被 agent 执行
- `scheduled_tasks` 状态正确回写
- 恢复请求状态正确回写
- 共享接收状态正确回写

## 10. 当前阶段已知限制

当前 ZeroTier 第一阶段仍然有这些限制：

1. 控制协议还是文本 TCP
2. 任务提交虽然已服务化，但认证鉴权还很弱
3. `device_agent` 是常驻轮询，不是完整生产级服务管理
4. 还没有做节点到节点的直接文件块传输

所以这一步是：

**“先让系统在真实虚拟网络里跑起来”**

而不是：

**“一步做成最终分布式系统”**
