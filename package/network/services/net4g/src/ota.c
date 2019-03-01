#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>
#include <syslog.h>
#include "ota.h"
#include "debug.h"

extern int check_image(char *image_file);

#define WRITE_LOG_FILE    "/tmp/OTA_LOG"

int LOG(char *fmt, ...)
{
	FILE *fp;
	va_list argptr;
	int cnt;
	fp = fopen(WRITE_LOG_FILE,"a+");
	if (fp == NULL) {
		fclose(fp);
		fprintf(stderr,"fopen file error\n");
	}
	va_start(argptr, fmt);
	//cnt = vsnprintf(buffer,bufsize ,fmt, argptr); //argptr had moved to last
	cnt = vfprintf(fp,fmt,argptr);
	//cnt = vprintf(fmt,argptr); //out to stdout
	vsyslog(LOG_NOTICE, fmt, argptr); // out to syslog
	va_end(argptr);
	fclose(fp);
	
	return(cnt);
}


int check_version(char *upver)
{
	if(upver == NULL)
		return -1;
	
	FILE *fp = fopen("/etc/Version","r");
	if(!fp)
		return -1;
	char curver[8] = {0};
	fread(curver,1,sizeof(curver),fp);
	if(strcmp(upver,curver) > 0) // upver > curver
	{
		fclose(fp);
		return 0;
	} else {
		fclose(fp);
		return -1;
	}
}

int do_ota(char *url,char *ver,char *resp,char *localfile)
{
	DEBUG("[do_ota] clear tmp firmware file!\n");
	system("/bin/rm -f /tmp/firmware.img");
	sleep(1);
	
	if(check_version(ver) != 0 && ver) {
		DEBUG("[OTA] Invalid upgrade Version!,%s\n",ver);
		LOG("[OTA] Invalid upgrade Version!,%s\n",ver);
		strcpy(resp,"Invalid upgrade Version!");
		return -1;
	}
	
	char getcmd[512] = "";
	sprintf(getcmd,"/usr/bin/wget %s -c -t 2 --timeout 10 -o /tmp/OTA_WGET_LOG -O /tmp/firmware.img", url);
	int ret = system(getcmd);
	
	DEBUG("[OTA] download finished,system ret=%d\n",ret);

	if(access("/tmp/firmware.img",F_OK) != 0) {
		ERROR("local firmware is not exist\n");
		strcpy(resp,"Download failed!");
		return -1;
	}
	ret = check_image(localfile);
	if (ret < 0) {
		DEBUG("[OTA] Not a valid Image kernel!\n");
		strcpy(resp,"Not a valid Image kernel!");
		return -2;
	}
	DEBUG("[OTA] image check success\n");
	LOG("[OTA] image check success\n");
	system("/usr/bin/md5sum /tmp/firmware.img > /tmp/firmmd5");
	
	return 0;
}


