#ifndef _POST_H
#define _POST_H

#ifdef __cplusplus
extern "C" {
#endif

#define IP_ADDR         "122.227.179.90"
#define IP_PORT         (81)
#define PAGE_START     "/ws/MobileFileUpload.asmx/UploadStart"
#define PAGE_CONTENT    "/ws/MobileFileUpload.asmx/UploadContent"
#define PAGE_FINISH     "/ws/MobileFileUpload.asmx/UploadFinish"

#define FILE_NAME       "xmipc_20190528173000_20190528175030_01.h264"

#define MAXLINE 1024

#define MAX_BASE64      (1024*500)
#define MAX_CONTENT     ((MAX_BASE64)+1024)
#define MAX_INLEN		((MAX_BASE64/4-1)*3)

#define SOCK_ERR		(-9)

int post_start(int type, char *name, int *position);
int post_finish(int fd, int type, char *name, int size);
int post_content(int fd, int type, char *name, int position, unsigned char *data, int inlen);


#ifdef __cplusplus
}
#endif


#endif