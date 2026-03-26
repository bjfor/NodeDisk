#ifndef _FDFS_API_H
#define _FDFS_API_H

int fdfs_upload_file(const char* filename, char* fileid);
int fdfs_upload_file1(const char* filename, char* fileid, int size);
int fdfs_download_file(const char* fileid, const char* local_filename);
int fdfs_make_file_url(const char* fileid, char* file_url);

#endif
    
