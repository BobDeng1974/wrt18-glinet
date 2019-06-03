#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/select.h>
#include <errno.h>
#include <sys/stat.h> 

#include "post.h"


int _http_connect(char *ip,int port)
{
    int sockfd = 0;
    struct sockaddr_in servaddr;

    if((sockfd = socket(AF_INET,SOCK_STREAM,0)) < 0) {
        perror("socket create err!");
        return -1;
    }
    
    bzero(&servaddr,sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(port);
    if(inet_pton(AF_INET,ip,&servaddr.sin_addr) <= 0) {
        perror("socket inet_pton err!");
        close(sockfd);
        return -1;
    }
    if(connect(sockfd,(struct sockaddr *)&servaddr,sizeof(servaddr)) < 0) {
        perror("socket connect err!\n");
        close(sockfd);
        return -1;
    }
    printf("socket connect OK %d\n", sockfd);
    return sockfd;
}

int _http_close(int fd)
{
    return close(fd);
}
/*
return val:
-1, sock err;
-2, sock timeout;
-3, http resp content err;
0, http resp OK;
*/
int _http_post(int sockfd,char *page,char *msg, int *rint)
{
    char recvline[MAXLINE] = {0};
    //char content[MAX_CONTENT] = {0};
	char *content = (char*)malloc(MAX_CONTENT);
	if(!content) {
		printf("post malloc error!\n");
		return -1;
	}
    memset(content,0,MAX_CONTENT);
    char *ptr = content;
    int ret = -1;

    ret = sprintf(ptr, "POST /%s HTTP/1.1\r\n",page);
    ptr += ret;
    ret = sprintf(ptr, "HOST: %s:%d\r\n",IP_ADDR,IP_PORT);
    ptr += ret;
    ret = sprintf(ptr, "Content-Type: application/x-www-form-urlencoded\r\n");
    ptr += ret;
    ret = sprintf(ptr, "Connection: keep-alive\r\n");
    ptr += ret;
    ret = sprintf(ptr, "Content-Length: %ld\r\n\r\n",strlen(msg));
    ptr += ret;
    ret = sprintf(ptr, "%s", msg);
    ptr += ret;
  //  sprintf(content,"%s%s%s%s%s",content_page,content_host,content_type,content_len,msg);

    ret = write(sockfd,content,strlen(content));
    //printf("http write %d, post all len %ld\n", ret, strlen(content));
    if(ret != strlen(content)) {
        perror("socket write err !\n");
		if(content) free(content);
        return SOCK_ERR;
    }
	if(content) free(content);
	
    fd_set   frd;
    struct timeval  tv;
    tv.tv_sec = 3;
    tv.tv_usec = 0;
	
	while(1) {
		FD_ZERO(&frd);
		FD_SET(sockfd, &frd);
		ret = select(sockfd+1, &frd, NULL, NULL, &tv);
		if(ret == 0) {
			printf("http get response timeout!\n");
			return SOCK_ERR;
		}
		else if(ret < 0) {
			if(errno == EINTR) {
				printf("###### [http] select interrupt by signal #####\n");
				continue;
			} else {
				printf("[http] read select error,%d:%s\n",errno,strerror(errno));
			}
 			return SOCK_ERR;  // error or closed
		}
		//run here! must be OK!
		ret = read(sockfd, recvline, MAXLINE);
        if(ret <= 0) {
            perror("socket read err!\n");
            return SOCK_ERR;
        }
        ret = ret > sizeof(recvline) ? sizeof(recvline)-1: ret;
        recvline[ret] = '\0';
        if(strstr(recvline, "HTTP/1.1 200 OK\r\n")) {
            ptr = strstr(recvline, "Content-Length:");
            if(ptr) {
                //printf("[http] response get\n%s\n", ptr);
                ptr = strstr(recvline, "<int xmlns=\"http://www.3gvs.net/\">");
                //printf("<---[http] response get\n\t%s\n", ptr);
                ret = 0;
                if(ptr=strstr(ptr, "\">")) {
                    ptr+=2;
                    while((*ptr!='\0') && (*ptr != '<')) {
                        recvline[ret++] = *ptr++;
                    }
                    recvline[ret] = '\0';
					ret = atoi(recvline);
                    //printf("<---[http] get int [%s]%d\n",recvline,ret);
                    if(rint)
                        *rint = ret;
					return 0;
                }
            }
        }
        printf("http get response error!\n%s\n",recvline);
        ret = -3;
        //or continue to read until timeout???
        break;
	}
_success:
    return ret;
}

int post_start(int type, char *name, int *position)
{
    int ret = 0;
    int pos = 0;
    int fd = 0;
    fd = _http_connect(IP_ADDR, IP_PORT);
    if(fd < 0)
        return SOCK_ERR;
	
	char msg[512] = {0};
	sprintf(msg, "fileType=%d&fileName=%s&password=12345",type,name);
	
    ret = _http_post(fd, PAGE_START, msg, &pos);
    printf("<---http post start ret %d, get pos %d\n", ret, pos);
	if(ret == 0) {
		if(position) *position = pos;
		return fd;
	} else
		return ret;
}

int post_finish(int fd, int type, char *name, int size)
{
    int ret = 0;
    int pos = 0;
 
	if(fd < 0)
		return -1;
	
	char msg[512] = {0};
	sprintf(msg, "fileType=%d&fileName=%s&fileSize=%d&password=12345",type,name,size);
    ret = _http_post(fd, PAGE_FINISH, msg, &pos);
    printf("<---http post finish ret %d ,pos %d\n", ret, pos);
    return ret;
}

int post_content(int fd, int type, char *name, int position, unsigned char *data, int inlen)
{
    int ret = 0;
    int new_pos = 0;
	if(fd < 0)
		return 0;
	
    char *msg = (char*)malloc(MAX_BASE64+512);
    char *base64 = (char*)malloc(MAX_BASE64);

	if(inlen >= MAX_INLEN) {
        printf("post_content data too large %d\n",inlen);
        return -1;
    }
	if(!msg || !base64) {
		printf("post_content malloc error!\n");
		return -1;
	}
    
    char *ptr = base64;
    int baselen = 0;
    int try_out = 0;

    memset(base64,0,MAX_CONTENT);
    baselen  = b64_encode(data, inlen, ptr, MAX_BASE64);
	if(baselen <= 0) {
		printf("http base64 error!\n");
		ret = -1;
		goto _exit;
	}
	//printf("b64 %d, b64 ct %d\n", baselen, strlen(ptr));
   // printf("[%d]64decode %d\n",baselen,b64_decode(ptr,dst,sizeof(dst)));
   // printf("64 compare %d\n",memcmp(buf,dst,sizeof(buf)));

	//printf("\n%s\n",ptr);
	//printf("[%d]64decode %d\n",baselen,b64_decode(ptr,dst,sizeof(dst)));
	memset(msg, 0, MAX_BASE64+512);
    
	sprintf(msg, "fileType=%d&fileName=%s&position=%d&base64=%s&password=12345",type,name,position,base64);

_AGAIN:
	if(++try_out > 3) {
		ret = -1;
		goto _exit;
	}
	ret = _http_post(fd, PAGE_CONTENT, msg, &new_pos);
	printf("http post at %d, resp %d, offset at %d\n", position, ret, new_pos);
	if(ret == 0) {
		if(new_pos > 0)
			ret = new_pos;
		else {
			//resp -1
			goto _AGAIN;
		}
	}
_exit:
	if(msg) free(msg);
	if(base64) free(base64);
	return ret;
}

int post_content2(int fd, int type, char *file, int position, char *data, int inlen)
{
    int ret = 0;
    int pos = 0;
    char msg[MAX_BASE64+256] = {0};
    char base64[MAX_BASE64] = {0};

    if(inlen >= MAX_BASE64) {
        printf("data too large %d\n",inlen);
        return -1;
    }
    
    char *ptr = base64;
    int baselen = 0;
    int i = 0;
    int try_out = 0;
    //for(i=0;i<1000;i++) {
	struct stat statbuf;  
    stat("./ipc.h264",&statbuf);  
    printf("file size %ld", statbuf.st_size);
	
    FILE *fp = fopen("./ipc.h264","rb");
    char buf[1024+1] = {0};
    char dst[1024+1] = {0};
    while((inlen=fread(buf, 1, sizeof(buf)-1, fp)) > 0) {
        printf("rd file len %d\n", inlen);
        baselen  = b64_encode(buf, inlen, ptr, sizeof(base64));
        if(baselen <= 0) {
            printf("http base64 error!\n");
            _http_close(fd);
            return -1;
        }
        //printf("b64 %d, b64 ct %d\n", baselen, strlen(ptr));
       // printf("[%d]64decode %d\n",baselen,b64_decode(ptr,dst,sizeof(dst)));
       // printf("64 compare %d\n",memcmp(buf,dst,sizeof(buf)));

        //printf("\n%s\n",ptr);
        //printf("[%d]64decode %d\n",baselen,b64_decode(ptr,dst,sizeof(dst)));
        //memset(msg, 0, sizeof(msg));
        sprintf(msg, "fileType=%d&fileName=%s&position=%d&base64=%s&password=12345",type,file,position,base64);

    _AGAIN:
        ret = _http_post(fd, PAGE_CONTENT, msg, &pos);
        printf("http post at %d, get content at %d\n", position, ret);
        if(ret >= 0) {
            position = ret;
        }
        else
        {
            printf("----try again %d---\n", try_out);
            if(++try_out > 2)
                break;
            goto _AGAIN;
        }
    }
	printf("rd finish %d\n", inlen);
    fclose(fp);
    _http_close(fd);
    return ret;
}

#if 0
int main()
{
    //char msg[] = "1313413413erqerqefdfarweqre";
    char *msg = malloc(204800);
    memset(msg,'2',204800);
    int i = 0;
    int ret = 0;
    int pos = 0;
    
    ret = post_start();
    if(ret >= 0)
        ret  = post_content(1,FILE_NAME, ret, msg,strlen(msg));
    else
    {
        printf("post end!\n");
    }
    return 0;
}
#endif
