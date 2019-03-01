#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <assert.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <sys/time.h>
#include <time.h>
#include "net_base.h"
#include "net_json.h"
#include "config.h"
#include "debug.h"

#define SUC_RET 		"success"
#define FAIL_RET		"failure"
#define OTA_MASK    6
#define STRCPY(key) strncpy(key,sub_json->valuestring,sizeof(key)-1)

#define ADD_KEY(obj,key) cJSON_AddItemToObject(obj, msg_key[key].name, \
							cJSON_CreateString(nvram_get(msg_key[key].name)))

#if 1
MSG_KEY msg_key[MAG_VAL_MAX+1] = {
	[MSG_TYPE] = { .name = "type", .inx = MSG_TYPE},
	[MSG_BODY] = {"body",MSG_BODY},
	
	[MSG_SEQNUM] = {"seqnum",MSG_SEQNUM},
	[MSG_OTA] = {"ota",MSG_OTA},
	[MSG_OTA_URL] = {"url",MSG_OTA_URL},
	[MSG_OTA_VER] = {"otaversion",MSG_OTA_VER},
	[MSG_OTA_SIZE] = {"size",MSG_OTA_SIZE},
	[MSG_OTA_CRC] = {"crc",MSG_OTA_CRC},
	[MSG_OTA_MD5] = {"md5",MSG_OTA_MD5},
	[MSG_OTA_BUILDTIME] = {"buildtime",MSG_OTA_BUILDTIME},
	
	[MSG_DEV_ID] = {"deviceid",MSG_DEV_ID},
	[MSG_MAC] = {"mac",MSG_MAC},
	[MSG_IMEI] = {"imei",MSG_IMEI},
	[MSG_IMSI] = {"imsi",MSG_IMSI},
	[MSG_VERSION] = {"version",MSG_VERSION},
	[MSG_CUR_TIME] = {"curtime",MSG_CUR_TIME},
	
	[MSG_GPS] = {"gpsinfo",MSG_GPS},
	[MSG_AGPS] = {"agpsinfo",MSG_AGPS},
	[MSG_GPS_LATI] = {"latitude",MSG_GPS_LATI},
	[MSG_GPS_LONGI] = {"longitude",MSG_GPS_LONGI},
	[MSG_GPS_DIR] = {"gpsdir",MSG_GPS_DIR},
	[MSG_GPS_SPEED] = {"speed",MSG_GPS_SPEED},
	[MSG_GPS_NS] = {"ns",MSG_GPS_NS},
	[MSG_GPS_HEIGHT] = {"height",MSG_GPS_HEIGHT},
	
	[MSG_NET] = {"netinfo",MSG_NET},
	[MSG_NET_SIG] = {"signal",MSG_NET_SIG},
	[MSG_NET_TYPE] = {"netstatus",MSG_NET_TYPE},
	
	[MSG_GPIO] = {"gpiostatus",MSG_GPIO},
	[MSG_GPIO_NUM] = {"gpionum",MSG_GPIO_NUM},
	[MSG_GPIO_NAME] = {"gpioname",MSG_GPIO_NAME},
	[MSG_GPIO_DIR] = {"gpiodir",MSG_GPIO_DIR},
	[MSG_GPIO_VAL] = {"gpioval",MSG_GPIO_VAL},
	[MSG_GPIO_MODE] = {"gpiomode",MSG_GPIO_MODE},
	
	
	[MSG_RSP_CODE] = {"respcode",MSG_RSP_CODE},
	[MSG_RSP_MSG] = {"respmsg",MSG_RSP_MSG},

	//add after 2016-10-15
	[MSG_CMD_INFO] = {"cmd",MSG_CMD_INFO},
	[MSG_PROTO_VER] = {"protocolversion",MSG_PROTO_VER},
	[MSG_REMOTE_ID] = {"id",MSG_REMOTE_ID},
	[MSG_REMOTE_IP] = {"ip",MSG_REMOTE_IP},
	[MSG_REMOTE_PORT] = {"port",MSG_REMOTE_PORT},
	[MSG_PPP_4GDIAL] = {"network",MSG_PPP_4GDIAL},
	[MSG_PPP_APN] = {"apn",MSG_PPP_APN},
	[MSG_PPP_USER] = {"user",MSG_PPP_USER},
	[MSG_PPP_PASD] = {"password",MSG_PPP_PASD},
	[MSG_PPP_DIALNUM] = {"dialnumber",MSG_PPP_DIALNUM},
	
	[MSG_SYS_LOG] = {"logtype",MSG_SYS_LOG},
	[MSG_TTY_CMDTP] = {"cmdtype",MSG_TTY_CMDTP},
	[MSG_TTY_MSG] = {"msg",MSG_TTY_MSG},
	[MSG_TTY_BAUD] = {"baudrate",MSG_TTY_BAUD},
	[MSG_TTY_DBT] = {"databit",MSG_TTY_DBT},
	[MSG_TTY_STPB] = {"stopbit",MSG_TTY_STPB},
	[MSG_TTY_PARI] = {"parity",MSG_TTY_PARI},
	[MSG_TTY_FLOWCT] = {"flowcontrol",MSG_TTY_FLOWCT},
	[MSG_TTY_WKMD] = {"workmode",MSG_TTY_WKMD},
	[MSG_TTY_TINTER] = {"timeinterval",MSG_TTY_TINTER},
	
	//after 2018-4-28 add vcc
	[MSG_INFO_OTHER] = {"other",MSG_INFO_OTHER},
	[MSG_INFO_VCC] = {"voltage",MSG_INFO_VCC},
	
	[MAG_VAL_MAX] = {"null",MAG_VAL_MAX}
};
#else
char msg_val[MAG_VAL_MAX][32] = {
	[MSG_MAGIC] = "magic",
	[MSG_TYPE] = "msgType3-char",
};
#endif

void get_cur_time(char *cur_time)
{
	time_t timer = time(NULL);
	struct tm *timeinfo = localtime(&timer);
	if(timeinfo) 
		sprintf(cur_time,"%4d-%02d-%02d %02d:%02d:%02d",1900+timeinfo->tm_year, 
                   1+timeinfo->tm_mon,timeinfo->tm_mday,timeinfo->tm_hour,
                    timeinfo->tm_min,timeinfo->tm_sec);
	else {
		ERROR("get current time error!\n");
		sprintf(cur_time,"%s","none");
	}
}

/* Need to release the returned pointer */
char* create_json_msg_login(const char *info_file,char *seqnum)
{

	cJSON *root = NULL;	/* declare a few. */
	char *out = NULL;
	root = create_root_json_obj();

	//type
	cJSON_AddItemToObject(root, msg_key[MSG_TYPE].name, cJSON_CreateString("login"));
	cJSON_AddItemToObject(root, msg_key[MSG_SEQNUM].name, cJSON_CreateString(seqnum));
	//body
	cJSON *obj_val = create_root_json_obj();
	ADD_KEY(obj_val,MSG_DEV_ID);
	ADD_KEY(obj_val,MSG_MAC);
	ADD_KEY(obj_val,MSG_IMEI);
	ADD_KEY(obj_val,MSG_IMSI);
	ADD_KEY(obj_val,MSG_VERSION);
	//ADD_KEY(obj_val,MSG_CUR_TIME);
	char cur_time[64] = "";
	get_cur_time(cur_time);
	cJSON_AddItemToObject(obj_val, msg_key[MSG_CUR_TIME].name, cJSON_CreateString(cur_time));
	cJSON_AddItemToObject(obj_val, msg_key[MSG_PROTO_VER].name, cJSON_CreateString("V2.7"));
	
	cJSON_AddItemToObject(root, msg_key[MSG_BODY].name, obj_val);

	//switch to string
	out = minimun_json(root);
	cJSON_Delete(root);

	assert(out);
	return out;
}

/* Need to release the returned pointer */
char* create_json_board_info(const char *board_info,char *seqnum)
{
	cJSON *root = create_root_json_obj();
	//type
	cJSON_AddItemToObject(root, msg_key[MSG_TYPE].name, cJSON_CreateString("boardinfo"));
	cJSON_AddItemToObject(root, msg_key[MSG_SEQNUM].name, cJSON_CreateString(seqnum));
	
	//body
	nvram_renew("/tmp/agps_info");
	cJSON *gps_obj = create_root_json_obj();
	ADD_KEY(gps_obj,MSG_GPS_LATI);
	ADD_KEY(gps_obj,MSG_GPS_LONGI);
	ADD_KEY(gps_obj,MSG_GPS_DIR);
	ADD_KEY(gps_obj,MSG_GPS_SPEED);
	ADD_KEY(gps_obj,MSG_GPS_NS);
	ADD_KEY(gps_obj,MSG_GPS_HEIGHT);
	
	//agpsinfo
//	nvram_renew("/tmp/agps_info");
//	cJSON *agps_obj = create_root_json_obj();
//	ADD_KEY(agps_obj,MSG_GPS_LATI);
//	ADD_KEY(agps_obj,MSG_GPS_LONGI);
//	ADD_KEY(agps_obj,MSG_GPS_DIR);
//	ADD_KEY(agps_obj,MSG_GPS_SPEED);
	
	cJSON *net_obj = create_root_json_obj();
	ADD_KEY(net_obj,MSG_NET_SIG);
	ADD_KEY(net_obj,MSG_NET_TYPE);
	
	//add vcc
	cJSON *vcc_obj = create_root_json_obj();
	ADD_KEY(vcc_obj,MSG_INFO_VCC);
	
//	cJSON *gpio_obj = create_root_json_obj();
//	ADD_KEY(gpio_obj,MSG_GPIO_NUM);
//	ADD_KEY(gpio_obj,MSG_GPIO_NAME);
//	ADD_KEY(gpio_obj,MSG_GPIO_DIR);
//	ADD_KEY(gpio_obj,MSG_GPIO_VAL);

//	cJSON *gpio_array = cJSON_CreateArray();
//	cJSON_AddItemToArray(gpio_array,gpio_obj);
// 	cJSON_AddItemToArray(gpio_array,gpio_obj2);
// 	cJSON_AddItemToArray(gpio_array,gpio_obj3);
	
	cJSON *body_obj = create_root_json_obj();
	ADD_KEY(body_obj,MSG_DEV_ID);
	char cur_time[64] = "";
	get_cur_time(cur_time);
	cJSON_AddItemToObject(body_obj, msg_key[MSG_CUR_TIME].name, cJSON_CreateString(cur_time));
	cJSON_AddItemToObject(body_obj, msg_key[MSG_GPS].name, gps_obj);
//	cJSON_AddItemToObject(body_obj, msg_key[MSG_AGPS].name, agps_obj);
	cJSON_AddItemToObject(body_obj, msg_key[MSG_NET].name, net_obj);
	cJSON_AddItemToObject(body_obj, msg_key[MSG_INFO_OTHER].name, vcc_obj);
	
//	cJSON_AddItemToObject(body_obj, msg_key[MSG_GPIO].name, gpio_array);
	
	cJSON_AddItemToObject(root, msg_key[MSG_BODY].name, body_obj);
	
	char *out = minimun_json(root);	
	cJSON_Delete(root);

	assert(out);
	return out;
}

static void replace_specfic_ascii(char *msg)
{
	char *p = msg;
	if(p == NULL) return;
	if(strlen(p) == 0) return;

	while(*p != '\0') {
		if(*p == '"')
			*p = '\'';
		p++;
	}
}

/* Need to release the returned pointer */
char* create_json_msg_rsp(const char *code,char *msg, const char *seqnum,const char *type)
{
	cJSON *root = create_root_json_obj();
	//type
	cJSON_AddItemToObject(root, msg_key[MSG_RSP_CODE].name, cJSON_CreateString(code));
	cJSON_AddItemToObject(root, msg_key[MSG_SEQNUM].name, cJSON_CreateString(seqnum));
	replace_specfic_ascii(msg);
	cJSON_AddItemToObject(root, msg_key[MSG_RSP_MSG].name, cJSON_CreateString(msg));
	cJSON_AddItemToObject(root, msg_key[MSG_TYPE].name, cJSON_CreateString(type));

	char *out = minimun_json(root);	
	cJSON_Delete(root);

	assert(out);
	return out;
}

int parse_json_ret(const char *text,char *retmsg)
{
	int ret = -1;
	cJSON *sub_json = NULL;
	cJSON *json_fmt = cJSON_Parse(text);
	if (!json_fmt) {
		ERROR("Error before: [%s]\n",cJSON_GetErrorPtr());
		return -1;
	} else {
		if((sub_json=GET_KEY(json_fmt,MSG_RSP_CODE))) {
			if(strcmp(sub_json->valuestring,SUC_RET)) {
				ERROR("[RSP] Failed!:[%s]\n",sub_json->valuestring);
				sub_json=GET_KEY(json_fmt,MSG_RSP_MSG);
				if(sub_json)
					ERROR("[RSP] Failed MSG:[%s]\n",sub_json->valuestring);
				goto EXIT;
			} else {
				if((sub_json=GET_KEY(json_fmt,MSG_SEQNUM))) {
					ret = atoi(sub_json->valuestring);
					DEBUG("[RSP] OK seqnum=[%d]\n",ret);
				}
				if((sub_json=GET_KEY(json_fmt,MSG_RSP_MSG)) && retmsg) {
					strncpy(retmsg,sub_json->valuestring,127);
				}	
				ret = 0;
			}
		}
	}
	cJSON_Delete(json_fmt);
	json_fmt = NULL;
	return ret;
EXIT:
	if(json_fmt) cJSON_Delete(json_fmt);
	return -1;
}

int parse_json_seqnum(const char *text)
{
	int ret = -1;
	cJSON *sub_json = NULL;
	cJSON *json_fmt = cJSON_Parse(text);
	if (!json_fmt) {
		ERROR("Error before: [%s]\n",cJSON_GetErrorPtr());
		return -1;
	} else {
		if(NULL == GET_KEY(json_fmt,MSG_TYPE)) {
			goto EXIT;
		}
		if(NULL == GET_KEY(json_fmt,MSG_BODY)) {
			goto EXIT;
		}
		if((sub_json=GET_KEY(json_fmt,MSG_SEQNUM))) {
			ret = atoi(sub_json->valuestring);
			DEBUG("[Get] seqnum=%d\n",ret);
		} else {
			goto EXIT;
		}
	}
	cJSON_Delete(json_fmt);
	json_fmt = NULL;
	return ret;
EXIT:
	if(json_fmt) cJSON_Delete(json_fmt);
	return -1;
}

int parse_json_svrtime(const char *text,int *seq,char *svrtime)
{
	int ret = -1;
	cJSON *sub_json = NULL;
	cJSON *json_fmt = cJSON_Parse(text);
	if (!json_fmt) {
		ERROR("Error before: [%s]\n",cJSON_GetErrorPtr());
		return -1;
	} else {
		if((sub_json=GET_KEY(json_fmt,MSG_SEQNUM)) && seq) {
			*seq = atoi(sub_json->valuestring);
			DEBUG("[Get] seqnum=%d\n",*seq);
		} else {
			goto EXIT;
		}
		
		if((sub_json=GET_KEY(json_fmt,MSG_BODY))) {
			if((sub_json=GET_KEY(sub_json,MSG_CUR_TIME))) {
				DEBUG("Server time=%s\n",sub_json->valuestring);
				if(svrtime)
					strncpy(svrtime,sub_json->valuestring,127);
				ret = 0;
			}
		}
	}
	cJSON_Delete(json_fmt);
	json_fmt = NULL;
	return ret;
	
EXIT:
	if(json_fmt) cJSON_Delete(json_fmt);
	return -1;
}

int parse_json_timing_gpio_mode(const char *text)
{
	int ret = -1;
	cJSON *sub_json = NULL;
	cJSON *json_fmt = cJSON_Parse(text);
	if (!json_fmt) {
		ERROR("Error before: [%s]\n",cJSON_GetErrorPtr());
		return -1;
	} else {
		if((sub_json=GET_KEY(json_fmt,MSG_BODY))) {
			if((sub_json=GET_KEY(sub_json,MSG_GPIO_MODE))) {
				DEBUG("GPIO Ctrl mode:%s\n",sub_json->valuestring);
				if(strcmp(sub_json->valuestring,"auto") == 0)
					ret = 1;
				else if(strcmp(sub_json->valuestring,"manual") == 0)
					ret = 2;
				else
					ret = 0;
			}
		}
	}
	cJSON_Delete(json_fmt);
	json_fmt = NULL;
	return ret;
}

int parse_json_ota_msg(const char* text,OTA_ST *ota_info)
{
	int parse_mask = 0;
	cJSON *sub_json = NULL, *body_json = NULL;
	cJSON *json_fmt = cJSON_Parse(text);
	if (!json_fmt) {
		ERROR("Error before: [%s]\n",cJSON_GetErrorPtr());
		return -1;
	} else {
		if(NULL == (body_json=GET_KEY(json_fmt,MSG_BODY)) ) {
			ERROR("OTA do NOT found body msg!\n");
			goto EXIT;
		}
		
		if((sub_json=GET_KEY(body_json,MSG_OTA_URL))) {
			DEBUG("[OTA] url:[%s]\n",sub_json->valuestring);
			parse_mask++;
			STRCPY(ota_info->url);
		}
		if((sub_json=GET_KEY(body_json,MSG_OTA_VER))) {
			DEBUG("[OTA] version:[%s]\n",sub_json->valuestring);
			parse_mask++;
			STRCPY(ota_info->version);
		}
		if((sub_json=GET_KEY(body_json,MSG_OTA_SIZE))) {
			DEBUG("[OTA] size:[%s]\n",sub_json->valuestring);
			parse_mask++;
			ota_info->size = strtol(sub_json->valuestring,NULL,16);
		}
		if((sub_json=GET_KEY(body_json,MSG_OTA_CRC))) {
			DEBUG("[OTA] crc:[%s]\n",sub_json->valuestring);
			parse_mask++;
			ota_info->crc = strtol(sub_json->valuestring,NULL,16);
		}
		if((sub_json=GET_KEY(body_json,MSG_OTA_MD5))) {
			DEBUG("[OTA] md5:[%s]\n",sub_json->valuestring);
			parse_mask++;
			STRCPY(ota_info->md5);
		}
		if((sub_json=GET_KEY(body_json,MSG_OTA_BUILDTIME))) {
			DEBUG("[OTA] buildtime:[%s]\n",sub_json->valuestring);
			parse_mask++;
			STRCPY(ota_info->buildtime);
		}
	}
	if(parse_mask != OTA_MASK) {
		ERROR("Error json format![%s]\n",text);
		goto EXIT;
	}
	cJSON_Delete(json_fmt);
	json_fmt = NULL;
	return 0;
EXIT:
	if(json_fmt) cJSON_Delete(json_fmt);
	return -1;
}

// add after 2016-10-15
int parse_json_readlog(const char *text,int *seq, char *type)
{
	int ret = -1;
	cJSON *sub_json = NULL;
	cJSON *json_fmt = cJSON_Parse(text);
	if (!json_fmt) {
		ERROR("Error before: [%s]\n",cJSON_GetErrorPtr());
		return -1;
	} else {
		if((sub_json=GET_KEY(json_fmt,MSG_SEQNUM))) {
			*seq = atoi(sub_json->valuestring);
			DEBUG("[Get] seqnum=%d\n",*seq);
		} else {
			goto EXIT;
		}

		if((sub_json=GET_KEY(json_fmt,MSG_BODY))) {
			if((sub_json=GET_KEY(sub_json,MSG_SYS_LOG))) {
				strncpy(type,sub_json->valuestring,127);
				DEBUG("get logtype=%s\n",type);
				ret = 0;
			}
		}
	}
EXIT:
	cJSON_Delete(json_fmt);
	json_fmt = NULL;
	return ret;
}


int parse_json_runcmd(const char *text,int *seq, char *cmd)
{
	int ret = -1;
	cJSON *sub_json = NULL;
	cJSON *json_fmt = cJSON_Parse(text);
	if (!json_fmt) {
		ERROR("Error before: [%s]\n",cJSON_GetErrorPtr());
		return -1;
	} else {
		if((sub_json=GET_KEY(json_fmt,MSG_SEQNUM))) {
			*seq = atoi(sub_json->valuestring);
			DEBUG("[Get] seqnum=%d\n",*seq);
		} else {
			goto EXIT;
		}

		if((sub_json=GET_KEY(json_fmt,MSG_BODY))) {
			if((sub_json=GET_KEY(sub_json,MSG_CMD_INFO))) {
				strncpy(cmd,sub_json->valuestring,255);
				DEBUG("get cmd=%s\n",cmd);
				//if(cmd)
					
				ret = 0;
			}
		}
	}
EXIT:
	cJSON_Delete(json_fmt);
	json_fmt = NULL;
	return ret;
}

int parse_json_remote_cfg(const char *text,int *seq,char *devid,char *ipaddr,char *port)
{
	int ret = -1;
	cJSON *sub_json = NULL, *body_json = NULL;
	cJSON *json_fmt = cJSON_Parse(text);
	if (!json_fmt) {
		ERROR("Error before: [%s]\n",cJSON_GetErrorPtr());
		return -1;
	} else {
		if((sub_json=GET_KEY(json_fmt,MSG_SEQNUM))) {
			*seq = atoi(sub_json->valuestring);
			DEBUG("[Get] seqnum=%d\n",*seq);
		} else {
			goto EXIT;
		}

		if((body_json=GET_KEY(json_fmt,MSG_BODY))) {

			if((sub_json=GET_KEY(body_json,MSG_REMOTE_ID))) {
				strncpy(devid,sub_json->valuestring,127);
				DEBUG("get cfg devid=%s\n",devid);
				//if(devid)
					
			} else {
				goto EXIT;
			}

			if((sub_json=GET_KEY(body_json,MSG_REMOTE_IP))) {
				strncpy(ipaddr,sub_json->valuestring,127);
				DEBUG("get cfg ip=%s\n",ipaddr);
				//if(ipaddr)
					
			} else {
				goto EXIT;
			}

			if((sub_json=GET_KEY(body_json,MSG_REMOTE_PORT))) {
				strncpy(port,sub_json->valuestring,127);
				DEBUG("get cfg port=%s\n",port);
				//if(port)
					
			} else {
				goto EXIT;
			}
			ret = 0;
		}
	}
EXIT:
	cJSON_Delete(json_fmt);
	json_fmt = NULL;
	return ret;
}

int parse_json_ppp_cfg(const char *text,int *seq,char *dialtype,char *apn,char *user,char *pwd,char *dialnum)
{
	int ret = -1;
	cJSON *sub_json = NULL,*body_json = NULL;
	cJSON *json_fmt = cJSON_Parse(text);
	if (!json_fmt) {
		ERROR("Error before: [%s]\n",cJSON_GetErrorPtr());
		return -1;
	} else {
		if((sub_json=GET_KEY(json_fmt,MSG_SEQNUM))) {
			*seq = atoi(sub_json->valuestring);
			DEBUG("[Get] seqnum=%d\n",*seq);
		} else {
			goto EXIT;
		}
		
		if((body_json=GET_KEY(json_fmt,MSG_BODY))) {

			if((sub_json=GET_KEY(body_json,MSG_PPP_4GDIAL))) {
				DEBUG("get ppp 4gdial=%s\n",sub_json->valuestring);
				if(dialtype)
					strncpy(dialtype,sub_json->valuestring,127);
			} else {
				goto EXIT;
			}

			if((sub_json=GET_KEY(body_json,MSG_PPP_APN))) {
				DEBUG("get ppp apn=%s\n",sub_json->valuestring);
				if(apn)
					strncpy(apn,sub_json->valuestring,127);
			} else {
				goto EXIT;
			}

			if((sub_json=GET_KEY(body_json,MSG_PPP_USER))) {
				DEBUG("get ppp user=%s\n",sub_json->valuestring);
				if(user)
					strncpy(user,sub_json->valuestring,127);
			} else {
				goto EXIT;
			}

			if((sub_json=GET_KEY(body_json,MSG_PPP_PASD))) {
				DEBUG("get ppp pwd=%s\n",sub_json->valuestring);
				if(pwd)
					strncpy(pwd,sub_json->valuestring,127);
			} else {
				goto EXIT;
			}

			if((sub_json=GET_KEY(body_json,MSG_PPP_DIALNUM))) {
				DEBUG("get ppp dialnum=%s\n",sub_json->valuestring);
				if(dialnum)
					strncpy(dialnum,sub_json->valuestring,127);
			} else {
				goto EXIT;
			}
			ret = 0;
		}
	}
EXIT:
	cJSON_Delete(json_fmt);
	json_fmt = NULL;
	return ret;
}

int parse_json_tty_trans(const char *text, int *seq, char *outcmd)
{
	int ret = -1;
	cJSON *sub_json = NULL,*body_json = NULL;
	cJSON *json_fmt = cJSON_Parse(text);
	if (!json_fmt) {
		ERROR("Error before: [%s]\n",cJSON_GetErrorPtr());
		return -1;
	} else {
		if((sub_json=GET_KEY(json_fmt,MSG_SEQNUM))) {
			*seq = atoi(sub_json->valuestring);
			DEBUG("[Get] seqnum=%d\n",*seq);
		} else {
			goto EXIT;
		}
		
		if((body_json=GET_KEY(json_fmt,MSG_BODY))) {

			if((sub_json=GET_KEY(body_json,MSG_CMD_INFO))) {
				DEBUG("get tty cmd=%s\n",sub_json->valuestring);
				if(outcmd)
					strncpy(outcmd,sub_json->valuestring,1023);
			} else {
				goto EXIT;
			}
			if((sub_json=GET_KEY(body_json,MSG_TTY_CMDTP))) {
				DEBUG("get tty cmd type=%s\n",sub_json->valuestring);
				
			} else {
				goto EXIT;
			}
			ret = 0;
		}
	}
EXIT:
	cJSON_Delete(json_fmt);
	json_fmt = NULL;
	return ret;
}

int parse_json_tty_cfg(const char *text,int *seq, ST_UART *uart_attr)
{
	int ret = -1;
	cJSON *sub_json = NULL,*body_json = NULL;
	cJSON *json_fmt = cJSON_Parse(text);
	if (!json_fmt) {
		ERROR("Error before: [%s]\n",cJSON_GetErrorPtr());
		return -1;
	} else {
		if((sub_json=GET_KEY(json_fmt,MSG_SEQNUM))) {
			*seq = atoi(sub_json->valuestring);
			DEBUG("[Get] seqnum=%d\n",*seq);
		} else {
			goto EXIT;
		}
		
		if((body_json=GET_KEY(json_fmt,MSG_BODY))) {
			if((sub_json=GET_KEY(body_json,MSG_TTY_BAUD))) {
				DEBUG("get tty baud=%s\n",sub_json->valuestring);
				uart_attr->baud = atoi(sub_json->valuestring);
			} else {
				goto EXIT;
			}
			if((sub_json=GET_KEY(body_json,MSG_TTY_DBT))) {
				DEBUG("get tty data bit=%s\n",sub_json->valuestring);
				uart_attr->databit = atoi(sub_json->valuestring);
			} else {
				goto EXIT;
			}
			if((sub_json=GET_KEY(body_json,MSG_TTY_STPB))) {
				DEBUG("get tty stop bit=%s\n",sub_json->valuestring);
				uart_attr->stopbit = atoi(sub_json->valuestring);
			} else {
				goto EXIT;
			}
			if((sub_json=GET_KEY(body_json,MSG_TTY_PARI))) {
				DEBUG("get tty crc=%s\n",sub_json->valuestring);
				uart_attr->parity = atoi(sub_json->valuestring);
			} else {
				goto EXIT;
			}
			if((sub_json=GET_KEY(body_json,MSG_TTY_FLOWCT))) {
				DEBUG("get tty flowcontrol=%s\n",sub_json->valuestring);
				uart_attr->flowct = atoi(sub_json->valuestring);
			} else {
				goto EXIT;
			}
			if((sub_json=GET_KEY(body_json,MSG_TTY_WKMD))) {
				DEBUG("get tty workmode=%s\n",sub_json->valuestring);
				uart_attr->workmode = atoi(sub_json->valuestring);
			} else {
				goto EXIT;
			}
			if((sub_json=GET_KEY(body_json,MSG_TTY_TINTER))) {
				DEBUG("get tty time interval=%s\n",sub_json->valuestring);
				uart_attr->interval = atoi(sub_json->valuestring);
			} else {
				goto EXIT;
			}
			
			ret = 0;
		}
	}
EXIT:
	cJSON_Delete(json_fmt);
	json_fmt = NULL;
	return ret;
}
