# 1.0 Testing Guide

本文档说明 `netdisk_v1` 在 1.0 阶段推荐怎么测试。

## 测试目标

1. 验证大文件备份主线
2. 验证共享文件库主线
3. 验证元数据和最终落盘结果

## 推荐入口

1. `system_demo`
   适合快速看整体系统是否还能工作
2. `netdisk_cli`
   适合按步骤验证 1.0 两条核心主线
3. `scripts/test_v1.sh`
   适合一键回归测试

## 构建

```bash
cd /home/hjh/hjh_project/yun_cunchu/AI_YunCunChu/netdisk_v1
cmake -S . -B build
cmake --build build -j
```

## 快速验证

```bash
./build/system_demo
```

预期可以看到：

- 设备数量
- 在线节点数量
- 备份策略和备份任务数量
- 共享条目和共享任务数量
- 已接收文件状态

## 命令行验证

### 1. 备份

```bash
./build/netdisk_cli backup-run /tmp/netdisk_v1_test/netdisk.db node-a /tmp/netdisk_v1_test/source_backup mirror /tmp/netdisk_v1_test/store_backup
```

预期：

- 返回 `backup_ok`
- 数据库中出现文件索引
- 数据库中出现对应设备的 `backup_records`
- `store_backup/` 下出现备份结果

### 2. 共享文件库

```bash
./build/netdisk_cli share-run /tmp/netdisk_v1_test/netdisk.db node-a entry-1 /tmp/netdisk_v1_test/source_backup/docs/handoff.txt handoff.txt /tmp/netdisk_v1_test/share_library node-b /tmp/netdisk_v1_test/share_receive
```

预期：

- 返回 `share_ok`
- `share_library/` 中出现共享库文件
- `share_receive/` 中出现接收后的文件

### 3. 检查数据库

```bash
./build/netdisk_cli inspect /tmp/netdisk_v1_test/netdisk.db
```

预期：

- 输出 `files=<n>`
- 输出 `stored_objects=<n>`
- 输出 `backup_records=<n>`
- 能看到文件相对路径和存储后端信息
- 能看到设备维度的备份记录

## 一键回归测试

```bash
bash scripts/test_v1.sh
```

预期最后输出：

```text
[ok] v1 test flow completed
```

## 当前 1.0 验收重点

- 大文件备份是否成功
- 备份文件是否真的落到目标存储目录
- 共享文件是否能发布到共享库
- 共享文件是否能从共享库接收到目标目录
- 元数据是否能通过 `inspect` 查到

## 当前阶段暂不作为 1.0 阻塞项

- 多副本自动调度
- 真正的去中心化多主一致性
- Android / 移动端
- 复杂冲突解决
- 重前端界面
