#ifndef NET_JSON_H
#define NET_JSON_H

#include "cjson.h"
#include "ota.h"
#include "uart.h"

enum {
	MSG_TYPE,
	MSG_BODY,
	MSG_SEQNUM,
	
	MSG_OTA,
	MSG_OTA_URL,
	MSG_OTA_VER,
	MSG_OTA_SIZE,
	MSG_OTA_CRC,
	MSG_OTA_MD5,
	MSG_OTA_BUILDTIME,
	
	MSG_DEV_ID,
	MSG_MAC,
	MSG_IMEI,
	MSG_IMSI,
	MSG_VERSION,
	MSG_CUR_TIME,
	
	MSG_GPS,
	MSG_AGPS,
	MSG_GPS_LATI,
	MSG_GPS_LONGI,
	MSG_GPS_DIR,
	MSG_GPS_SPEED,
	MSG_GPS_HEIGHT,
	MSG_GPS_NS,
	
	MSG_NET,
	MSG_NET_SIG,
	MSG_NET_TYPE,
	
	MSG_GPIO,
	MSG_GPIO_NUM,
	MSG_GPIO_NAME,
	MSG_GPIO_DIR,
	MSG_GPIO_VAL,
	MSG_GPIO_MODE,		
	MSG_RSP_CODE,
	MSG_RSP_MSG,

	//add after 2016-10-15
	MSG_CMD_INFO,
	MSG_PROTO_VER,
	MSG_REMOTE_ID,
	MSG_REMOTE_IP,
	MSG_REMOTE_PORT,

	MSG_PPP_4GDIAL,
	MSG_PPP_APN,
	MSG_PPP_USER,
	MSG_PPP_PASD,
	MSG_PPP_DIALNUM,
	
	MSG_SYS_LOG,
	MSG_TTY_CMDTP,
	MSG_TTY_MSG,
	MSG_TTY_BAUD,
	MSG_TTY_DBT,
	MSG_TTY_STPB,
	MSG_TTY_PARI,
	MSG_TTY_FLOWCT,
	MSG_TTY_WKMD,
	MSG_TTY_TINTER,
	//add gcc
	MSG_INFO_OTHER,
	MSG_INFO_VCC,
	//end
	MSG_INFO_WIFIUP,
	MSG_INFO_WIFIUP_SSID,
	MSG_INFO_WIFIUP_KEY,

	MAG_VAL_MAX
};

typedef struct msg_key_dsp {
	char *name;
	int inx;
}MSG_KEY;
extern MSG_KEY msg_key[MAG_VAL_MAX+1];

#define STR_OF(json,i) 	cJSON_GetArrayItemOfIndex(json,i)->valuestring
#define INT_OF(json,i) 	cJSON_GetArrayItemOfIndex(json,i)->valueint
#define DOU_OF(json,i) 	cJSON_GetArrayItemOfIndex(json,i)->valuedouble

#define GET_KEY(json,key)	cJSON_GetObjectItem(json,msg_key[key].name)

#define STR_OF_KEY(json,key) 		GET_KEY(json,key)->valuestring
#define INT_OF_KEY(json,key) 		GET_KEY(json,key)->valueint
#define DOU_OF_KEY(json,key) 		GET_KEY(json,key)->valuedouble


#define CHECK_SIZE(type,size,MIN)	 if(size < MIN) { \
								 printf("%s value is invalid! size=%d\n",type,size);\
								 return NULL;}

#define NET_RECORD_FILE	"./net_json_tb"

/* Need to release the returned pointer */
char* create_json_msg_login(const char *info_file,char *seqnum);

/* Need to release the returned pointer */
char* create_json_msg_rsp(const char *code,char *msg, const char *seqnum,const char *type);

/* Need to release the returned pointer */
char* create_json_board_info(const char *board_info,char *seqnum);

/*
 * return
 * 0 -- WARN neither auto nor manual
 * 1 -- auto
 * 2 -- manual
 * -1 -- format error
 */
int parse_json_timing_gpio_mode(const char *text);

int parse_json_svrtime(const char *text,int *seq,char *svrtime);
int parse_json_seqnum(const char *text);
int parse_json_ota_msg(const char* text,OTA_ST *ota_info);
int parse_json_ret(const char *text,char *retmsg);

int parse_json_readlog(const char *text,int *seq, char *type);
int parse_json_runcmd(const char *text,int *seq,char *cmd);
int parse_json_remote_cfg(const char *text,int *seq,char *devid,char *ipaddr,char *port);
int parse_json_ppp_cfg(const char *text,int *seq,char *dialtype,char *apn,char *user,char *pwd,char *dialnum);

int parse_json_tty_trans(const char *text, int *seq, char *outcmd);
int parse_json_tty_cfg(const char *text,int *seq, ST_UART *uart_attr);

int parse_json_wifiup_cfg(const char *text,int *seq);

#endif

