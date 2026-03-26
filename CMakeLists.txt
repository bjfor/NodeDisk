cmake_minimum_required(VERSION 3.16)

project(netdisk_v1 LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(SQLite3 REQUIRED)
find_package(Threads REQUIRED)

add_library(netdisk_common STATIC
    src/common/config_loader.c
    src/common/make_log.c
    src/common/md5.c
    third_party/cjson/cJSON.c
)

target_include_directories(netdisk_common
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/third_party
        ${PROJECT_SOURCE_DIR}/third_party/cjson
)

add_library(netdisk_metadata STATIC
    src/metadata/metadata_store.cpp
)

target_include_directories(netdisk_metadata
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(netdisk_metadata
    PUBLIC
        SQLite::SQLite3
)

add_library(netdisk_network STATIC
    src/network/protocol_codec.cpp
    src/network/transport.cpp
)

target_include_directories(netdisk_network
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)

add_library(netdisk_sync STATIC
    src/sync/chunk_reader.cpp
    src/sync/chunker.cpp
    src/sync/diff_engine.cpp
    src/sync/hash.cpp
    src/sync/scanner.cpp
    src/sync/sync_writer.cpp
)

target_include_directories(netdisk_sync
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(netdisk_sync
    PUBLIC
        netdisk_metadata
        netdisk_network
        netdisk_common
)

add_library(netdisk_storage STATIC
    src/storage/fdfs_api.c
    src/storage/storage_backend.cpp
    src/storage/replica_manager.cpp
    src/storage/integrity_checker.cpp
)

target_include_directories(netdisk_storage
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(netdisk_storage
    PUBLIC
        netdisk_common
)

add_library(netdisk_security STATIC
    src/security/audit_logger.cpp
    src/security/auth_service.cpp
    src/security/node_network_service.cpp
)

target_include_directories(netdisk_security
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)

add_library(netdisk_control_plane STATIC
    src/control_plane/control_node_server.cpp
    src/control_plane/device_agent_service.cpp
    src/control_plane/control_plane_service.cpp
    src/control_plane/device_manager.cpp
    src/control_plane/task_scheduler.cpp
    src/control_plane/policy_manager.cpp
)

target_include_directories(netdisk_control_plane
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(netdisk_control_plane
    PUBLIC
        netdisk_metadata
        netdisk_security
        Threads::Threads
)

add_library(netdisk_backup STATIC
    src/backup/backup_service.cpp
    src/backup/recovery_service.cpp
    src/backup/file_index_service.cpp
)

target_include_directories(netdisk_backup
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(netdisk_backup
    PUBLIC
        netdisk_control_plane
        netdisk_security
        netdisk_metadata
        netdisk_storage
        netdisk_sync
)

add_library(netdisk_transfer STATIC
    src/transfer/shared_library_service.cpp
)

target_include_directories(netdisk_transfer
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(netdisk_transfer
    PUBLIC
        netdisk_control_plane
        netdisk_security
)

add_library(netdisk_runtime STATIC
    src/control_plane/system_runtime.cpp
)

target_include_directories(netdisk_runtime
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(netdisk_runtime
    PUBLIC
        netdisk_backup
        netdisk_control_plane
        netdisk_metadata
        netdisk_security
        netdisk_storage
        netdisk_transfer
)

add_executable(sync_v1
    apps/sync_v1/main.cpp
)

target_include_directories(sync_v1
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(sync_v1
    PRIVATE
        netdisk_sync
        netdisk_storage
        netdisk_metadata
        netdisk_network
        netdisk_security
        netdisk_control_plane
        netdisk_backup
        netdisk_transfer
        netdisk_runtime
        netdisk_common
        Threads::Threads
)

add_executable(system_demo
    apps/system_demo/main.cpp
)

target_include_directories(system_demo
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(system_demo
    PRIVATE
        netdisk_runtime
        netdisk_backup
        netdisk_transfer
        netdisk_control_plane
        netdisk_security
        netdisk_metadata
)

add_executable(netdisk_cli
    apps/netdisk_cli/main.cpp
)

target_include_directories(netdisk_cli
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(netdisk_cli
    PRIVATE
        netdisk_runtime
        netdisk_backup
        netdisk_transfer
        netdisk_control_plane
        netdisk_security
        netdisk_storage
        netdisk_metadata
        netdisk_sync
        netdisk_common
        Threads::Threads
)

add_executable(control_node
    apps/control_node/main.cpp
)

target_include_directories(control_node
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(control_node
    PRIVATE
        netdisk_runtime
        netdisk_backup
        netdisk_transfer
        netdisk_control_plane
        netdisk_security
        netdisk_storage
        netdisk_metadata
        netdisk_sync
        netdisk_common
        Threads::Threads
)

add_executable(device_agent
    apps/device_agent/main.cpp
)

target_include_directories(device_agent
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(device_agent
    PRIVATE
        netdisk_control_plane
        netdisk_security
        netdisk_metadata
        netdisk_common
        netdisk_runtime
        Threads::Threads
)
