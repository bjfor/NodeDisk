# NetDisk V1

`netdisk_v1` 是一个独立维护的 C++ 项目仓库，目标是围绕“个人分布式网盘”方向继续演进。

它已经不再只是原开源云盘项目的目录内重构分支，而是新的主开发线：

- 保留现代 C++ 工程结构
- 保留独立的同步、备份、共享、存储子系统
- 保留对原项目成熟能力的受控复用
- 为后续 2.0 演进做独立仓库准备

## 仓库定位

这样独立出来主要有 3 个好处：

1. 结构更清楚  
   新系统和老系统不再混着长，后续拆模块、拆子系统会轻松很多。

2. 复用更可控
   原项目里成熟的基础能力先复制进独立工作区，再按模块融入 `common`、`storage`、`third_party` 等正式目录，不会误伤老项目。

3. 演进更安全  
   如果后面同步内核、控制面、业务层都要重构，独立工作区更适合逐步演进。

## 当前目录结构

```text
netdisk_v1/
├── CMakeLists.txt       CMake 构建入口
├── Makefile             CMake 的轻量包装入口
├── .gitignore           仓库忽略规则
├── NOTICE.md            来源与发布说明
├── ROADMAP.md           版本路线图
├── apps/                可执行程序入口
├── configs/             运行配置
├── docs/                新系统设计文档
├── include/             对外公开头文件
├── src/                 核心实现
├── third_party/         第三方依赖目录
├── build/               out-of-source 构建目录
└── output/              后续运行产物预留目录
```

## 目录职责

`apps/`
- 放带有 `main()` 的可执行程序入口
- 当前入口是 `apps/sync_v1/main.cpp`

`include/netdisk/`
- 放公开头文件
- 头文件按子系统继续拆分

`src/`
- 放实际实现代码
- 目前已拆为 `sync`、`network`、`metadata`、`storage`、`control_plane`、`backup`、`transfer`、`security`

`third_party/`
- 放第三方依赖源码
- 当前已纳入 `cJSON`

## 这种拆法对性能的影响

从运行时角度看，这种拆法通常不会带来明显性能损失，前提是：

- 模块之间仍然是进程内调用
- 不要为了分层而引入多余的序列化
- 不要把本来一次内存调用拆成多次进程/网络调用

也就是说：

- **架构上解耦** 不等于 **运行时变慢**
- 真正会拖慢性能的，是不必要的 IPC、重复拷贝、重复 hash、重复落盘

如果我们后面保持：

- 同步内核是进程内组件
- 存储适配层是本地接口调用
- 元数据层避免重复扫描和重复计算

那么这个拆法大概率会：

- 提升维护效率
- 提升后续优化空间
- 对实际性能影响很小

从长期看，这种结构反而更容易做性能优化。

## 当前建议

从现在开始：

- 老项目保留，作为参考仓库和稳定能力来源
- 新架构开发优先放到本仓库
- 需要复用的原项目代码，已经按模块融入 `common`、`storage`、`third_party`
- 尽量不要再让新代码直接跨目录依赖旧的 `common/`、`include/`、`src_cgi/`

## 当前 1.0 代码状态

目前 1.0 所有核心子系统都已经进入代码骨架：

- `sync`：同步主链路已可运行
- `network`：协议和传输已可运行
- `metadata`：SQLite 元数据已可运行
- `storage`：存储适配已可运行，副本与完整性模块已建骨架
- `control_plane`：设备、策略、任务、系统运行时已建骨架
- `backup`：备份、恢复、文件索引已建骨架
- `transfer`：共享文件库已建骨架
- `security`：认证、审计、节点互联已建骨架

当前的真实完成度是：

- 底层同步/传输/存储能力最完整
- 控制层和业务层已经进入代码，并补上了首版可运行逻辑
- `system_demo` 可以演示设备注册、认证、备份任务、共享文件库、恢复请求和审计日志的最小闭环

## 当前 2.0 进展

2.0 当前已经进入第一批落地阶段，重点先做两件事：

- 控制面持久化
- 备份中心增强

已经落下来的内容包括：

- 设备信息持久化到 SQLite
- 备份策略和共享策略持久化到 SQLite
- 调度任务持久化到 SQLite
- 备份作业持久化到 SQLite
- 恢复请求持久化到 SQLite
- 共享文件库条目持久化到 SQLite
- 共享文件接收节点状态持久化到 SQLite
- 审计日志持久化到 SQLite
- 备份历史查询
- 按设备 + 相对路径查看最新备份
- 基于已有备份执行真实恢复
- 恢复请求状态持久化
- 共享接收按节点状态细化

这些能力主要体现在：

- `src/metadata/metadata_store.cpp`
- `src/control_plane/`
- `src/backup/`
- `apps/netdisk_cli/main.cpp`

## 发布前建议

在推送 GitHub 前，建议至少确认：

- `.gitignore` 已覆盖本地构建和测试产物
- `build/`、临时数据库、测试输出不会被提交
- `NOTICE.md` 中的来源说明和许可证问题已确认
- README 中描述的功能范围与当前代码状态一致

## 构建

推荐直接使用 CMake：

```bash
cmake -S . -B build
cmake --build build -j
```

生成：

```bash
./build/sync_v1
./build/system_demo
./build/netdisk_cli
./build/control_node
./build/device_agent
```

## 1.0 测试入口

推荐优先使用这三个入口：

- `./build/system_demo`
  适合快速看整体系统骨架是否能工作
- `./build/netdisk_cli`
  适合按命令验证备份、共享、恢复、历史查询
- `./build/control_node`
  适合统一查看设备、任务、共享和恢复等控制面状态
- `./build/device_agent`
  适合验证 3.0 第一阶段的控制节点/agent 协同链
- `bash scripts/test_v1.sh`
  适合做 1.0 一键回归测试

更完整的测试说明见：

- [docs/testing_v1.md](docs/testing_v1.md)
- [docs/distributed_bootstrap_guide.md](docs/distributed_bootstrap_guide.md)

## 3.0 第一阶段启动脚本

```bash
./scripts/run_control_node.sh [db_path] [host] [port] [offline_after_seconds]
./scripts/run_device_agent.sh [local_db_path] [control_host] [control_port] [node_id] [device_name] [zt_ip] [backend_name] [backend_arg] [poll_interval_seconds] [max_cycles]
```

推荐先按：

- 一台机器运行 `control_node`
- 多台机器运行 `device_agent`
- 使用 ZeroTier 官方客户端先打通虚拟网络

`run_device_agent.sh` 现在默认使用 `device_agent serve` 常驻轮询模式。

更完整的跨机准备说明见：

- [docs/distributed_bootstrap_guide.md](docs/distributed_bootstrap_guide.md)

## 常用 CLI

```bash
./build/netdisk_cli backup-run <db_path> <node_id> <source_path> <backend_name> [backend_arg]
./build/netdisk_cli share-publish <db_path> <owner_node_id> <entry_id> <source_file> <display_name> <library_root>
./build/netdisk_cli share-receive <db_path> <entry_id> <target_node_id> <receive_root>
./build/netdisk_cli share-run <db_path> <owner_node_id> <entry_id> <source_file> <display_name> <library_root> <target_node_id> <receive_root>
./build/netdisk_cli share-list <db_path> <node_id> <scope>
./build/netdisk_cli share-cleanup <db_path> <now_epoch>
./build/netdisk_cli coord-status <db_path> <now_epoch> <offline_after_seconds>
./build/netdisk_cli task-retry <db_path> <task_id> [detail]
./build/netdisk_cli restore-submit <db_path> <request_id> <file_id> <source_node_id> <target_node_id> <destination_path>
./build/netdisk_cli restore-run <db_path> <request_id>
./build/netdisk_cli restore-retry <db_path> <request_id>
./build/netdisk_cli restore-latest <db_path> <source_node_id> <relative_path> <target_node_id> <destination_path>
./build/netdisk_cli share-retry <db_path> <entry_id> <target_node_id> <receive_root>
./build/netdisk_cli backup-history <db_path> <node_id> <relative_path>
./build/netdisk_cli inspect <db_path>
```

`share-list` 的 `scope` 支持：

- `all`
- `active`
- `pending`

`inspect` 当前会输出：

- 节点
- 备份策略 / 共享策略
- 调度任务
- 备份作业
- 恢复请求
- 共享条目与接收节点
- 文件索引
- 设备文件映射
- 存储对象
- 备份记录

`coord-status` 当前会输出：

- 设备总数
- 在线设备数
- 在线 peers 数
- 排队中的任务数
- 可重试任务数

## control_node 入口

`control_node` 是 2.0 新增的统一控制面入口，当前支持：

```bash
./build/control_node submit-backup <host> <port> <job_id> <node_id> <source_path>
./build/control_node submit-restore <host> <port> <request_id> <file_id> <source_node_id> <target_node_id> <destination_path>
./build/control_node submit-share-receive <host> <port> <entry_id> <target_node_id>
./build/control_node retry-task-remote <host> <port> <task_id> [detail]
./build/control_node retry-restore-remote <host> <port> <request_id>
./build/control_node retry-share-remote <host> <port> <entry_id> <target_node_id>
./build/control_node summary <db_path> [offline_after_seconds] [--json]
./build/control_node devices <db_path> <all|online> [offline_after_seconds] [--json]
./build/control_node tasks <db_path> <all|queued|failed> [--json]
./build/control_node task-retry <db_path> <task_id> [detail]
./build/control_node backups <db_path> [node_id] [--json]
./build/control_node shared <db_path> <node_id> <all|active|pending> [--json]
./build/control_node share-retry <db_path> <entry_id> <target_node_id> <receive_root>
./build/control_node restores <db_path> [--json]
./build/control_node restore-retry <db_path> <request_id>
./build/control_node audit <db_path> [category] [--json]
```

追加 `--json` 后，`control_node` 会输出机器可读 JSON，便于后续轻量 Web 管理端或 API 层直接复用。

其中前 6 个命令属于 3.0 第一阶段新增的远程控制动作，用来通过常驻 `control_node` 服务提交任务和触发重试，而不是直接本机写控制库。

`summary` 现在还会额外给出：

- `device_files`
  - 已经在设备侧真实落盘并进入设备索引的文件数

当前 `device_files` 已经覆盖三条主要落盘链：

- `backup`
  - 备份源设备上的原始文件
- `share_receive`
  - 共享接收后在目标设备落盘的文件
- `restore`
  - 从备份恢复到目标设备的文件

## 2.0 当前已经补齐的运营动作

到目前为止，2.0 不只是“能看到状态”，还已经具备了第一批可操作能力：

- 失败任务可以手动重试
- 失败恢复请求可以从控制面直接重试
- 共享接收已经拆成“发布 / 接收”两步，方便独立执行和排障
- 失败的共享接收可以从控制面直接重试
- 恢复请求和共享接收状态都已经细化为可查询的阶段状态

当前已经验证过的失败链包括：

- mirror 存储对象缺失导致的恢复失败，然后通过 `restore-retry` 成功恢复
- 共享库源文件缺失导致的接收失败，然后通过 `share-retry` 成功补发

## FastDFS 备份与恢复说明

当前 `FastDFS` 后端已经具备真实的备份与恢复能力：

- 备份时通过 `fdfs_upload_file` 上传文件
- 恢复时通过 `fdfs_download_file` 下载文件

在 Docker 开发环境下，推荐这样设置：

```bash
export SYNC_FDFS_UPLOAD_CMD=/home/hjh/hjh_project/yun_cunchu/AI_YunCunChu/tools/fdfs_upload_file_docker.sh
export SYNC_FDFS_DOWNLOAD_CMD=/home/hjh/hjh_project/yun_cunchu/AI_YunCunChu/tools/fdfs_download_file_docker.sh
```

这样 `netdisk_cli` 在宿主机执行时，会自动借用 `tc_fcgi_app` 容器里的 FastDFS 客户端完成上传和下载。

如果你只是想图快，也可以直接：

```bash
make
```
