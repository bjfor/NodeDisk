# Repository Release Guide

本文档说明如何把 `netdisk_v1` 作为独立仓库发布到 GitHub。

## 当前仓库根目录

```bash
/home/hjh/hjh_project/yun_cunchu/AI_YunCunChu/netdisk_v1
```

## 发布前检查

建议先确认：

1. `build/` 没有被纳入版本控制
2. 本地测试数据库和临时输出没有被纳入版本控制
3. `NOTICE.md` 中的来源说明已经确认
4. `README.md`、`ROADMAP.md`、`docs/` 内容与当前代码一致

## 查看待提交文件

```bash
git status
```

## 首次提交

```bash
git add .
git commit -m "feat: initialize netdisk_v1 repository"
```

## 添加 GitHub 远端

```bash
git remote add origin <your-github-repo-url>
```

例如：

```bash
git remote add origin git@github.com:<your-name>/<your-repo>.git
```

## 推送

```bash
git push -u origin main
```

## 建议的仓库名称

可以考虑：

- `netdisk-v1`
- `personal-distributed-netdisk`
- `cpp-distributed-netdisk`

