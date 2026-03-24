#include "netdisk/storage/fdfs_api.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "netdisk/common/config_loader.h"

static void trim_output(char *value) {
    int start = 0;
    int end = (int)strlen(value) - 1;

    while (value[start] == ' ' || value[start] == '\n' || value[start] == '\r' || value[start] == '\t') {
        ++start;
    }
    while (end >= start &&
           (value[end] == ' ' || value[end] == '\n' || value[end] == '\r' || value[end] == '\t')) {
        value[end] = '\0';
        --end;
    }

    if (start > 0) {
        memmove(value, value + start, strlen(value + start) + 1);
    }
}

static int run_upload_command(const char *filename, char *fileid) {
    char conf_path[256] = {0};
    const char *upload_command = getenv("SYNC_FDFS_UPLOAD_CMD");
    char command[1024] = {0};
    FILE *pipe = NULL;

    if (get_cfg_value(CFG_PATH, "dfs_path", "client", conf_path) != 0) {
        return -1;
    }

    if (upload_command == NULL || *upload_command == '\0') {
        upload_command = "fdfs_upload_file";
    }

    snprintf(command, sizeof(command), "%s %s %s 2>/dev/null", upload_command, conf_path, filename);
    pipe = popen(command, "r");
    if (pipe == NULL) {
        return -1;
    }

    if (fgets(fileid, 512, pipe) == NULL) {
        pclose(pipe);
        return -1;
    }
    pclose(pipe);

    trim_output(fileid);
    return (strlen(fileid) > 0) ? 0 : -1;
}

int fdfs_upload_file(const char *filename, char *fileid) {
    return run_upload_command(filename, fileid);
}

int fdfs_upload_file1(const char *filename, char *fileid, int size) {
    (void)size;
    return run_upload_command(filename, fileid);
}

int fdfs_make_file_url(const char *fileid, char *file_url) {
    char storage_ip[256] = {0};
    char storage_port[32] = {0};

    if (get_cfg_value(CFG_PATH, "storage_web_server", "ip", storage_ip) != 0) {
        return -1;
    }
    if (get_cfg_value(CFG_PATH, "storage_web_server", "port", storage_port) != 0) {
        return -1;
    }

    strcpy(file_url, "http://");
    strcat(file_url, storage_ip);
    strcat(file_url, ":");
    strcat(file_url, storage_port);
    strcat(file_url, "/");
    strcat(file_url, fileid);
    return 0;
}
