# NodeDisk 2.0 设计推进说明

## 1. 文档目的

这份文档用于说明 2.0 当前优先推进的两条主线：

- 控制面持久化
- 备份中心增强

它不是重新推翻 1.0，而是在 1.0 已经打通的“备份”和“共享文件库”两条主链上，继续把系统做得更像一个可长期运行的产品。

---

## 2. 为什么 2.0 先做这两件事

1.0 的问题不是主链跑不通，而是很多控制面对象还是内存态：

- 设备信息
- 策略
- 调度任务
- 备份作业
- 恢复请求

这会导致一个问题：

**程序能跑，但系统状态不够稳定，也不够可查询。**

所以 2.0 第一阶段先做的是：

1. 把控制面状态落到 SQLite
2. 把备份中心从“能备份”提升到“能查历史、能提恢复请求”

---

## 3. 2.0 当前已经落下来的内容

### 3.1 控制面持久化

当前已经持久化到 SQLite 的对象包括：

- `node_records`
  - 设备注册和心跳后的节点信息
- `backup_policies`
  - 备份策略
- `transfer_policies`
  - 共享文件库策略
- `scheduled_tasks`
  - 备份、共享、恢复等调度任务
- `backup_jobs`
  - 备份作业
- `restore_requests`
  - 恢复请求
- `shared_library_entries`
  - 共享文件库条目
- `shared_library_recipients`
  - 每个共享条目的已接收节点
- `audit_events`
  - 备份、共享、恢复等操作产生的审计日志

对应的核心实现位于：

- [metadata_store.h](/home/hjh/hjh_project/yun_cunchu/AI_YunCunChu/netdisk_v1/include/netdisk/metadata/metadata_store.h)
- [metadata_store.cpp](/home/hjh/hjh_project/yun_cunchu/AI_YunCunChu/netdisk_v1/src/metadata/metadata_store.cpp)

### 3.2 控制面服务已接入持久化

下面几个服务已经不再依赖内存向量作为唯一状态源，而是直接接到元数据层：

- [device_manager.cpp](/home/hjh/hjh_project/yun_cunchu/AI_YunCunChu/netdisk_v1/src/control_plane/device_manager.cpp)
- [policy_manager.cpp](/home/hjh/hjh_project/yun_cunchu/AI_YunCunChu/netdisk_v1/src/control_plane/policy_manager.cpp)
- [task_scheduler.cpp](/home/hjh/hjh_project/yun_cunchu/AI_YunCunChu/netdisk_v1/src/control_plane/task_scheduler.cpp)
- [recovery_service.cpp](/home/hjh/hjh_project/yun_cunchu/AI_YunCunChu/netdisk_v1/src/backup/recovery_service.cpp)

这意味着：

- 设备重建 `SystemRuntime` 后仍然能从数据库里查到
- 任务状态不是进程退出就丢
- 备份策略和共享策略可以被统一检查
- 恢复请求可以被重新读取
- 审计日志可以跨进程、跨入口统一查询

---

## 4. 备份中心增强

### 4.1 当前新增能力

备份中心在 2.0 第一阶段新增了两类能力：

1. 备份作业持久化
2. 备份历史查询
3. 基于已有备份执行恢复

对应实现主要在：

- [backup_service.cpp](/home/hjh/hjh_project/yun_cunchu/AI_YunCunChu/netdisk_v1/src/backup/backup_service.cpp)
- [file_index_service.cpp](/home/hjh/hjh_project/yun_cunchu/AI_YunCunChu/netdisk_v1/src/backup/file_index_service.cpp)

### 4.2 新的查询语义

当前已经支持：

- 列出所有备份作业
- 按节点列出备份作业
- 按 `node_id + relative_path` 查看备份历史
- 按 `node_id + relative_path` 查最新备份
- 根据 `RestoreRequest` 执行恢复
- 根据 `node_id + relative_path` 直接恢复最新备份

这里延续了 1.0 的设计：

- `file_records` 代表内容实体
- `backup_records` 代表设备上的备份记录

所以现在的备份中心已经能回答类似问题：

- 某台设备上的 `docs/report.docx` 曾经备份过几次
- 它最新一次备份对应哪个内容实体
- 它现在落在什么存储后端
- 是否可以直接恢复到另一台设备上的目标路径

### 4.3 当前恢复执行链

这一轮已经把“恢复请求”从单纯登记推进到了“真实恢复”：

1. 根据恢复请求找到 `file_id`
2. 根据 `file_id` 找到 `stored_object`
3. 根据 `stored_object.backend_name` 选择存储后端
4. 从存储后端把文件恢复到目标路径
5. 按文件元数据重新计算 hash
6. 校验通过后把恢复任务标记为完成

当前状态：

- `mirror` 恢复已验证通过
- `fastdfs` 恢复已验证通过
  - 当前恢复链已切换为原生 `fdfs_download_file`
  - 不再依赖不稳定的 HTTP `curl` 下载
- 恢复请求现在已经带有明确状态：
  - `queued`
  - `running`
  - `completed`
  - `failed`
- 恢复请求同时保存：
  - 完成时间
  - 错误信息

### 4.4 恢复后的目标设备索引策略

这一轮补上的关键点是：

- 不再把“恢复后的目标文件”混进 `file_records` 这一层
- `file_records` 继续只表示内容实体
- 新增 `device_file_records`，专门记录“某台设备上真实存在的文件”

当前恢复成功后会额外写入一条目标设备文件映射，内容包括：

- `node_id`
  - 目标设备
- `file_id`
  - 对应的内容实体
- `absolute_path`
  - 恢复后的目标路径
- `relative_path`
  - 当前阶段用目标文件名作为设备侧相对路径
- `source_kind`
  - 当前恢复链写为 `restore`
- `source_ref_id`
  - 对应的 `restore_request_id`

这样做的意义是：

- 恢复完成后，系统可以明确知道“这个文件现在已经出现在目标设备上”
- 不会因为同一内容被恢复到新设备，就错误覆盖掉原来的内容实体索引
- 后续做设备文件视图、设备侧回收、设备侧同步决策时，会更清晰

这套 `device_file_records` 现在已经不只服务恢复链，而是扩展到了三条主要落盘链：

- `backup`
  - 记录源设备上的原始文件
- `share_receive`
  - 记录共享接收后在目标设备落盘的文件
- `restore`
  - 记录恢复后在目标设备落盘的文件

这意味着 2.0 现在已经具备了统一的设备文件视图：

- `file_records`
  - 仍然表示内容实体
- `device_file_records`
  - 表示“这份内容在哪台设备、哪个路径上真实存在”

---

## 5. 共享文件库持久化

2.0 这一轮也把共享文件库从“内存态条目”推进到了“数据库持久化条目”。

### 5.1 当前已经持久化的内容

- 共享条目本身
  - `entry_id`
  - `owner_node_id`
  - `source_path`
  - `file_id`
  - `display_name`
  - `note`
  - 创建时间、过期时间、已送达状态
- 接收节点关系
  - 哪些节点已经收到该共享条目

### 5.2 当前效果

这意味着现在共享文件库已经具备：

- 共享后重启进程，条目不会丢
- 接收节点不会只存在内存里
- `inspect` 可以直接看到共享条目和接收节点列表
- 可以列出活动条目和待接收条目
- 可以执行过期清理，并把共享库中的源文件清走
- 每个接收节点现在都有明确状态：
  - `pending`
  - `delivered`
  - `failed`
- 接收状态还会记录：
  - 实际接收路径
  - 错误信息
  - 最近更新时间

对应实现主要位于：

- [metadata_store.cpp](/home/hjh/hjh_project/yun_cunchu/AI_YunCunChu/netdisk_v1/src/metadata/metadata_store.cpp)
- [shared_library_service.cpp](/home/hjh/hjh_project/yun_cunchu/AI_YunCunChu/netdisk_v1/src/transfer/shared_library_service.cpp)
- [main.cpp](/home/hjh/hjh_project/yun_cunchu/AI_YunCunChu/netdisk_v1/apps/netdisk_cli/main.cpp)

---

## 6. CLI 层新增能力

为了让 2.0 当前阶段能直接被验证，CLI 也增加了这些命令：

```bash
./build/netdisk_cli share-list <db_path> <node_id> <scope>
./build/netdisk_cli share-cleanup <db_path> <now_epoch>
./build/netdisk_cli restore-submit <db_path> <request_id> <file_id> <source_node_id> <target_node_id> <destination_path>
./build/netdisk_cli restore-run <db_path> <request_id>
./build/netdisk_cli restore-latest <db_path> <source_node_id> <relative_path> <target_node_id> <destination_path>
./build/netdisk_cli backup-history <db_path> <node_id> <relative_path>
```

其中 `share-list` 的 `scope` 当前支持：

- `all`
- `active`
- `pending`

同时 `inspect` 的输出也增强了，现在除了文件和存储对象，还会输出：

- 节点
- 备份策略
- 共享策略
- 调度任务
- 备份作业
- 恢复请求
- 共享条目与接收节点

---

## 7. 2.0 当前状态怎么理解

到现在为止，2.0 还不是“新版本完全做完了”，而是：

**已经完成第一批最关键的基础强化。**

可以这样理解：

- 1.0 解决“主链能不能跑”
- 2.0 第一阶段解决“系统状态能不能长期保存和被查询”

这一步完成后，后续再做：

- 共享条目生命周期管理
- 更完整的恢复执行链
- 多设备更强协调

都会更顺。

---

## 8. 多设备协调当前进展

这一轮已经先落了一个“最小可用协调层”，重点不是做真正复杂的分布式调度，而是先把协调相关的基础观察和操作补齐。

当前已经支持：

- 基于设备心跳判断在线设备
- 从设备列表刷新在线 peer 视图
- 查询排队任务和可重试任务
- 对失败任务执行手动重试，重试后任务重新回到 `queued`

对应实现主要位于：

- [device_manager.cpp](/home/hjh/hjh_project/yun_cunchu/AI_YunCunChu/netdisk_v1/src/control_plane/device_manager.cpp)
- [task_scheduler.cpp](/home/hjh/hjh_project/yun_cunchu/AI_YunCunChu/netdisk_v1/src/control_plane/task_scheduler.cpp)
- [node_network_service.cpp](/home/hjh/hjh_project/yun_cunchu/AI_YunCunChu/netdisk_v1/src/security/node_network_service.cpp)
- [main.cpp](/home/hjh/hjh_project/yun_cunchu/AI_YunCunChu/netdisk_v1/apps/netdisk_cli/main.cpp)

---

## 9. 统一控制面入口当前进展

这一轮还新增了一个独立的控制面入口：

- [main.cpp](/home/hjh/hjh_project/yun_cunchu/AI_YunCunChu/netdisk_v1/apps/control_node/main.cpp)

它的目标不是替代业务 CLI，而是把已经分散的查询能力收拢成一个更像“控制节点”的入口。

当前已经支持：

- `summary`
  - 汇总设备、任务、备份、共享、恢复等系统状态
- `devices`
  - 统一查看设备和在线设备
- `tasks`
  - 统一查看全部任务、排队任务、失败任务
- `backups`
  - 查看备份作业
- `shared`
  - 查看某节点相关的共享条目
- `restores`
  - 查看恢复请求
- `audit`
  - 查看持久化审计日志

这一步的意义是：

- 2.0 不再只有很多零散的 CLI 子命令
- 控制面已经开始形成一个独立角色
- 后面如果要做轻量 Web 管理端，就有更清晰的后端聚合层可以接
- `control_node` 现在同时支持文本输出和 `--json` 结构化输出，已经具备给后续管理端/API 直接复用的基础
- `control_node` 的 `summary` 现在还能直接给出：
  - 运行中的恢复请求数
  - 失败的恢复请求数
  - 待处理共享投递数

### 当前审计能力

这一轮已经把审计日志也接入了 SQLite：

- `AuditLogger` 不再只保存在进程内
- `backup` / `transfer` / `restore` 产生的审计事件都会落库
- `control_node audit` 可以在新的进程里直接读取历史审计事件

这意味着控制面现在已经具备了“状态 + 审计”两条完整观察链。

### 当前运营动作能力

在这一轮里，2.0 还补齐了第一批“失败后可操作”的能力，而不只是停留在状态可见：

- `task-retry`
  - 对失败任务做手动重试
- `restore-retry`
  - 对失败恢复请求做手动重试
- `share-retry`
  - 对失败共享接收做手动重试

同时，共享链已经拆成了两个独立动作：

- `share-publish`
  - 只负责把文件发布到共享库
- `share-receive`
  - 只负责指定节点接收共享条目

这样做的意义是：

- 失败场景可以被独立制造和复现
- 共享接收失败后可以明确进入 `failed` 状态
- 后续控制面或轻量管理端可以直接复用这些动作，而不是依赖 demo 式的一次性命令

当前已经验证通过的失败路径有：

1. mirror 存储对象丢失导致恢复失败
   - 控制面可以看到恢复请求进入 `failed`
   - 补回对象后，`restore-retry` 可以把请求推进到 `completed`

2. 共享库源文件缺失导致共享接收失败
   - 控制面可以看到接收节点状态进入 `failed`
   - 补回共享源文件后，`share-retry` 可以把状态推进到 `delivered`

---

## 10. 下一步建议

2.0 接下来最合理的顺序是：

1. 控制面继续完善
   - 把查询和运营动作进一步统一成稳定接口
   - 为后续轻量管理端/API 统一查询层做准备

2. 恢复流程继续增强
   - 恢复结果落更完整审计
   - 设备文件映射继续扩展到共享接收和其他落盘链路

3. 共享文件库继续增强
   - 共享条目筛选和运营指标继续细化
   - 面向 UI/控制面的共享列表接口继续整理
   - 重复接收 / 幂等语义继续收口
