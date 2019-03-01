#ifndef OTA_H
#define OTA_H

typedef struct ota_st {
	char url[256];
	char version[8];
	int size;
	int crc;
	char md5[33];
	char buildtime[32];
}OTA_ST;

int LOG(char *fmt, ...);
int do_ota(char *url,char *ver,char *resp,char *localfile);

#endif