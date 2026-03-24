# Sync V1

`sync_v1` is the integrated 1.0 entrypoint for the new sync engine.

It keeps the new Phase 1 sync loop:
- directory scan
- chunk diff
- resumable pull
- atomic commit

It also bridges into the original project storage path:
- `mirror` backend: store synced files into a local backup directory
- `fastdfs` backend: reuse the original `cfg.json` and `fdfs_upload_file` command path

## Build

```bash
cmake -S . -B build
cmake --build build -j
```

Binary:

```bash
./build/sync_v1
```

## Usage

```bash
./build/sync_v1 <local_root> <local_db_path> <remote_root> <remote_db_path> [backend_name] [backend_arg]
```

Examples:

```bash
./build/sync_v1 /tmp/local /tmp/local.db /tmp/remote /tmp/remote.db
./build/sync_v1 /tmp/local /tmp/local.db /tmp/remote /tmp/remote.db mirror /tmp/store
./build/sync_v1 /tmp/local /tmp/local.db /tmp/remote /tmp/remote.db fastdfs
```

## Backend Notes

`mirror`
- default backend when omitted
- if `backend_arg` is omitted, files are mirrored into `<local_root>/.sync_store`

`fastdfs`
- reads `./configs/cfg.json`
- uses the same `dfs_path.client` and `storage_web_server` settings as the original project
- requires the `fdfs_upload_file` command to be available at runtime

If the host machine does not have `fdfs_upload_file`, you can reuse the project's Docker container:

```bash
export SYNC_FDFS_UPLOAD_CMD=/home/hjh/hjh_project/yun_cunchu/AI_YunCunChu/tools/fdfs_upload_file_docker.sh
```

The helper script will proxy uploads into `tc_fcgi_app` with:

```bash
docker exec tc_fcgi_app fdfs_upload_file /etc/fdfs/client.conf <filename>
```

## Metadata

The local SQLite database now stores:
- file records
- chunk records
- resumable sync task state
- completed chunk progress
- stored object bindings

`stored_objects` records the final backend location for each synced file.
