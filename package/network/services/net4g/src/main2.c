#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <signal.h>
#include <unistd.h>
#include <time.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>  
#include <arpa/inet.h>  
#include <termios.h>
#include <sys/un.h>

#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <errno.h>
#include <signal.h>
#include <pthread.h>
#include <sys/time.h>
/* for wait() */
#include <sys/wait.h>
#include <sys/ioctl.h>

#include "net_base.h"
#include "net_json.h"
#include "config.h"
#include "ota.h"

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#define CFG_FILE		"/tmp/cfg_wind"
#define TIMING_FILE    "/etc/timing_config"
#define BOARD_INFO_FILE "/tmp/board_info"

#define UCI_REMOTE_CONFIG "/etc/config/netest"
#define UCI_PPP_CONFIG	  "/etc/config/network"

#define WAITTING_TIME	10  //s
#define LOGIN_TIME		10   //s

#define LOCKFILE "/var/run/net4g.pid"
#define LOCKMODE (S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH)

#define WTD_TRIG	"/sys/class/leds/heart/trigger"
#define WTD_BRIG	"/sys/class/leds/heart/brightness"
#define WTD_CTRL	"/tmp/heartbeatctrl"

#if 1
#include "debug.h"
#else
#define DEBUG printf
#define NOTE printf
#define WARN printf
#define ERROR printf
#endif

enum {
		ERR = -1,
		WARNING = 0,
		AUTO = 1,
		MANUAL = 2,
		UNKNOW
	};
	
static char glb_auto_flag = 0;
static char exit_flag = 0;
static CONFIG_ST *glb_cfg;
static ST_UART uart_attr;
volatile int glb_remote_socket = 0;

int lockfile(int fd)
{
	struct flock fl;

	fl.l_type = F_WRLCK;
	fl.l_start = 0;
	fl.l_whence = SEEK_SET;
	fl.l_len = 0;
	fl.l_pid = getpid();
	return(fcntl(fd, F_SETLK, &fl));
}

int already_running(void)
{
	int		fd;
	char	buf[16];

	fd = open(LOCKFILE, O_RDWR|O_CREAT, LOCKMODE);
	if (fd < 0) {
		syslog(LOG_ERR, "can't open %s: %s", LOCKFILE, strerror(errno));
		exit(1);
	}
	if (lockfile(fd) < 0) {
		if (errno == EACCES || errno == EAGAIN) {
			close(fd);
			return(1);
		}
		syslog(LOG_ERR, "can't lock %s: %s", LOCKFILE, strerror(errno));
		exit(1);
	}
	ftruncate(fd, 0);
	sprintf(buf, "%ld", (long)getpid());
	write(fd, buf, strlen(buf)+1);
	return(0);
}

//{"respcode":"success","seqnum":"99","respmsg":"4g ok"}
char buf[] = "{\"result\":\"success\",\"errorCode\":1}";
char buf1[] = "{\"result\":\"failure\",\"errorCode\":-99}";
char buf2[] = "{\"result1\":\"failure\",\"errorCode\":99}";

void lua_call_func(const char *filename, const char *func,
				   const char *param,int len,char *ret)
{
	lua_State *L = lua_open();
	luaL_openlibs(L);
	if(luaL_loadfile(L,filename)|| lua_pcall(L,0,0,0)) {
		ERROR("luaL_loadfile err:%s\n",lua_tostring(L,-1));
		goto EXIT;
		//lua_error(L);
	}
	
	lua_getglobal(L,func);
	lua_pushlstring(L,param,len);
	
	if(lua_pcall(L,1,1,0)){
		ERROR("lua_pcall func '%s' err:%s\n",func,lua_tostring(L,-1));
		goto EXIT;
	}
	
	if(!lua_isstring(L,-1)) {
		ERROR("lua return type error:%s\n",lua_tostring(L,-1));
		goto EXIT;
	}
	
	strncpy(ret,lua_tostring(L,-1),127);
	lua_pop(L,1);
// 	printf("lua_call_func-ret[%ld]:%s\n",strlen(ret),ret);
EXIT:
 	lua_close(L);
}

void read_file(char *file,char *buf,int len)
{
	FILE *fp = fopen(file,"r");
	if(!fp)
		return;
	int ret = fread(buf,1,len,fp);
	if(ret != len)
		//WARN("Read %s file!acture size %d<%d\n",file,ret,len);
	fclose(fp);
}

void record2file(char *file,char *buf,int len)
{
	if(access(WTD_CTRL, F_OK) == 0) {
		if(strstr(file, WTD_BRIG) || strstr(file, WTD_BRIG)) {
			return;
		}
	}
	FILE *fp = fopen(file,"w+");
	if(!fp)
		return;
	int ret = fwrite(buf,1,len,fp);
	if(ret != len)
		WARN("WARN!write file %s!acture size %d<%d\n",file,ret,len);
	fclose(fp);
}

void touchfile(char *file)
{
	record2file(file, "none", 4);
}

void json_test()
{
	char *ptr = NULL;
	printf("net Json api testing...\n");
	
	printf("%s\n",ptr=create_json_msg_login("4G_info","3"));
	free(ptr);
	
	printf("%s\n",ptr=create_json_msg_rsp("success","99","4g ok","type-resp"));
	free(ptr);
	
	nvram_renew(BOARD_INFO_FILE);
	printf("%s\n",ptr=create_json_board_info("./board_info","9"));
	
	free(ptr);
	
	int ret = parse_json_ret(buf1,NULL);
 	printf("recv,code = %d\n",ret);
	
	nvram_buflist();
}

static void set_socket_keepalive(int socket)
{
	int keepalive = 1;
	int keepidle = 60; //def 7200s
	int keepinterval = 5;  //def 75s
	int keepcount = 3;  //def 9
	setsockopt(socket,SOL_SOCKET,SO_KEEPALIVE,(void*)&keepalive,sizeof(keepalive));
	setsockopt(socket,SOL_TCP,TCP_KEEPIDLE,(void*)&keepidle,sizeof(keepidle));
	setsockopt(socket,SOL_TCP,TCP_KEEPINTVL,(void*)&keepinterval,sizeof(keepinterval));
	setsockopt(socket,SOL_TCP,TCP_KEEPCNT,(void*)&keepcount,sizeof(keepcount));
}

void sleep_seconds_intr(int seconds)
{
    struct timeval tv;
    tv.tv_sec = seconds;
    tv.tv_usec = 0;
    int err;
    do {
       err = select(0,NULL,NULL,NULL,&tv);
    } while(err < 0); //while(err < 0 && errno == EINTR);
}

int init_connect(const char *ip_addr, int port, int keepalive)
{
	struct sockaddr_in client_addr;
	struct sockaddr_in server_addr;

	bzero(&client_addr,sizeof(client_addr));
    client_addr.sin_family = AF_INET;
    client_addr.sin_addr.s_addr = htons(INADDR_ANY);
    client_addr.sin_port = htons(0);
	
	int cli_fd = socket(AF_INET, SOCK_STREAM,0);
	if(cli_fd < 0){
        ERROR("Create Socket Failed!%d:%s\n",errno,strerror(errno));
        return -1;
    }

//   int nRecvBuf=50*1024;//设置为32K
//	setsockopt(cli_fd,SOL_SOCKET,SO_RCVBUF,(const char*)&nRecvBuf,sizeof(int));

    if(bind(cli_fd, (struct sockaddr*)&client_addr,sizeof(client_addr))) {
        ERROR("Client Bind Port Failed! %d:%s\n",errno,strerror(errno)); 
        goto EXIT;
    }
	
	bzero(&server_addr,sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    if(inet_aton(ip_addr, &server_addr.sin_addr) == 0){
        ERROR("inet_aton Server IP Address Error! %d:%s\n",errno,strerror(errno));
        goto EXIT;
    }
    server_addr.sin_port = htons(port);
	NOTE("Connecting %s:%d...\n",ip_addr,port);
	
#if 1
	int ret = 0;
	int flags = fcntl(cli_fd,F_GETFL,0);
    fcntl(cli_fd,F_SETFL,flags | O_NONBLOCK);
	
	int n = connect(cli_fd, (struct sockaddr*)&server_addr, sizeof(server_addr));
    if(n < 0)
	{
        if(errno != EINPROGRESS && errno != EWOULDBLOCK)
		{
			ERROR("connect is not EINPROGRESS! %d:%s\n",errno,strerror(errno));
			goto EXIT;
		}

        struct timeval tv;
        tv.tv_sec = 3;
        tv.tv_usec = 0;
		int con_error = 0;
        fd_set wset;
        FD_ZERO(&wset);
        FD_SET(cli_fd,&wset);
        n = select(cli_fd+1,NULL,&wset,NULL,&tv);
        if(n < 0) {
		ERROR("Connect select() error. %d:%s\n",errno,strerror(errno));
		goto EXIT;
        } else if (0 == n) {
            ERROR("Connect select time out.\n");
		goto EXIT;
        } else {
            socklen_t ilen = sizeof(con_error);
            ret = getsockopt(cli_fd, SOL_SOCKET, SO_ERROR, &con_error, &ilen);
            if(ret < 0 || con_error != 0)
                goto EXIT;
            else {
                NOTE("sock %d Connected OK %s:%d\n", cli_fd, ip_addr, port);
            }
        }
    }
    
    //fcntl(cli_fd,F_SETFL,flags & ~O_NONBLOCK);
#else
	//def connect timeout 75s
	if(connect(cli_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        ERROR("Can NOT connect!%d\n",errno);
        goto EXIT;
    }
    DEBUG("Connect OK!\n");
#endif

    return cli_fd;
EXIT:
	if(cli_fd >= 0) close(cli_fd);
	return -1;
}


void port_close(int seriafd)
{
	tcflush(seriafd, TCIOFLUSH);
	sleep_seconds_intr(1);
    close(seriafd);
}

int open_gps_com(char *device)
{
	int fd = open(device, O_RDONLY);
	if(fd <= 0) {
		ERROR("open %s error,%d:%s\n",device,errno,strerror(errno));
		return -1;
	}
	return fd;
}

char get_agps_char(int fd)
{
	struct timeval tv;
	tv.tv_sec = 5;
	tv.tv_usec = 0;
	fd_set rset;
	FD_ZERO(&rset);
	FD_SET(fd,&rset);
	int n = select(fd+1,&rset,NULL,NULL,&tv);
	if(n < 0) {
		if(errno == EINTR)
			return 0;
		ERROR("Agps select() error. %d:%s\n",errno,strerror(errno));
		return -1;
	} else if (0 == n) {
		//ERROR("Agps select time out.\n");
		return 0;
	} else {
		char ch = 0;
		if(read(fd,&ch,1) != 1) {
			ERROR("Read AGPS Com Error!%d:%s\n",errno,strerror(errno));
			return -1;
		}
		return ch;
	}
}

char get_char(int fd)
{
	char ch = 0;
	if(read(fd,&ch,1) != 1) {
		ERROR("Read GPS Com Error!%d:%s\n",errno,strerror(errno));
		return 0;
	}
	return ch;
}

int parse_gps(char *gpsinfo,int index,char *value)
{
	char val[512] = "";
	int i = 0;

	char *p = gpsinfo;
	char *st = val;
	while(*p != '\0'){
		if(*p == ',') {
			*st = '\0';
			if(i == index) {
				memset(value,0,31);
				strncpy(value,val,31);
			}
			i++;
			memset(val,0,sizeof(val));
			st = val;
		} else {
			*st = *p;
			st++;
		}
		p++;
	}
    return 0; 
}


int parse_var(char *oneline,char *file)
{
	//$GPRMC,032601.00,A,2952.502727,N,12138.414028,E,0.0,178.8,170718,4.3,W,A*23,
	static char gpsinfo[16][32] = {"","","","","","","","","","","","","","","",""};
	static	int limit = 0;

	if(limit++ < 50) {
		return -1;
	} else {
		limit = 0;
	}
	enum {
		NAME = 0,
		UTC,
		LAT_FLAG,
		LATITUDE,
		SN,
		LONGITUDE,
		WE,
		SPEED,
		GDIR,
		HIG_DATE,
		CIPJ,
		CIPJE,
		LOCT_MODE,
		CRC
	};
	
	if(strstr(oneline,"RMC")) {
		parse_gps(oneline,LAT_FLAG,gpsinfo[LAT_FLAG]);
		parse_gps(oneline,SN,gpsinfo[SN]);
		parse_gps(oneline,SPEED,gpsinfo[SPEED]);
		parse_gps(oneline,GDIR,gpsinfo[GDIR]);
		parse_gps(oneline,LATITUDE,gpsinfo[LATITUDE]);
		parse_gps(oneline,LONGITUDE,gpsinfo[LONGITUDE]);
	} else if (strstr(oneline,"GGA")) {
		//get height
		parse_gps(oneline,HIG_DATE,gpsinfo[HIG_DATE]);
	} else {
		//do not need other gps info
		return -1;
	}
	FILE *fp = fopen(file,"w+");
	if(!fp)
		return -1;
	char strbuf[128] = "Default\n";
	fwrite(strbuf,1,strlen(strbuf),fp);
	// A is Valid. V is invalid
	if(strcmp(gpsinfo[LAT_FLAG],"A") == 0) {
		sprintf(strbuf,"ns=%s\n",gpsinfo[SN]);
		fwrite(strbuf,1,strlen(strbuf),fp);
		sprintf(strbuf,"speed=%s\n",gpsinfo[SPEED]);
		fwrite(strbuf,1,strlen(strbuf),fp);
		sprintf(strbuf,"height=%s\n",gpsinfo[HIG_DATE]);
		fwrite(strbuf,1,strlen(strbuf),fp);
		sprintf(strbuf,"gpsdir=%s\n",gpsinfo[GDIR]);
		fwrite(strbuf,1,strlen(strbuf),fp);
		sprintf(strbuf,"latitude=%s\n",gpsinfo[LATITUDE]);
		fwrite(strbuf,1,strlen(strbuf),fp);
		sprintf(strbuf,"longitude=%s\n",gpsinfo[LONGITUDE]);
		fwrite(strbuf,1,strlen(strbuf),fp);
		record2file("/sys/class/leds/gps/brightness","1",1);
	} else {
		fwrite("speed=\n",1,strlen("speed=\n"),fp);
		fwrite("gpsdir=\n",1,strlen("gpsdir=\n"),fp);
		fwrite("latitude=\n",1,strlen("latitude=\n"),fp);
		fwrite("longitude=\n",1,strlen("longitude=\n"),fp);
		fwrite("ns=\n",1,strlen("ns=\n"),fp);
		fwrite("height=\n",1,strlen("height=\n"),fp);
		record2file("/sys/class/leds/gps/brightness","0",1);
	}
	fclose(fp);
#if debug
	int i = 0;
	for(i;i<CRC;i++)
		printf("[%s] ",gpsinfo[i]);
	printf("\n");
#endif
	return 0;
}

void* handle_gps(int *arg)
{
	char one_line[512] = {0};
//	int ret = 0;
//	unsigned int count = 0;
	char *p = one_line;
	char ch = 0;
	int valid_flag = 0;
	
	NOTE("GPS Thread start...\n");
	int fd = open_gps_com("/dev/ttyS0");
	while(!exit_flag) {
		ch = get_char(fd);
		if(ch <=0)
			goto RECONN;
		if(ch == '$') {
			memset(one_line,0,512);
			p = one_line;
			*p = ch;
			valid_flag = 1;
		} else if(ch == '\r' || ch == '\n') {
			if(valid_flag) {
				//valid data is between '$' to '\r' or '\n'
				//tail add ','
				*p = ',';
				p++;
				*p = '\0';
// 				DEBUG("[%d][%s]\n",++count,one_line);
				parse_var(one_line,"/tmp/gps_info"); 
				//the all gps info,can not sleep here
			}
			valid_flag = 0;
		}
		if(valid_flag) {
			*p = ch;
			p++;
		}
		continue;
	RECONN:
		ERROR("Read GPS Com Error!%d:%s\n",errno,strerror(errno));
		if(fd >= 0) port_close(fd);
		sleep_seconds_intr(10);
		fd = open_gps_com("/dev/ttyS0");
	}
	NOTE("GPS Thread exit!\n");
	if(fd >= 0) port_close(fd);
	system("/bin/echo GPS_thread_EXIT > /tmp/gps_status");
	return NULL;
}

void* handle_tty(int *arg)
{
	NOTE("tty thread start...\n");
	int fd  = 0;
	int len = 0;
	char buffer[1024] = {0};
	
	struct timeval now_tv;
	struct timeval last_tv;
	char sendbuf[10240] = {0};
	char filebuf[10000] = {0};
	
	int count = 0;
	FILE *fp = NULL;
	gettimeofday(&last_tv,NULL);
	memset(&uart_attr,0,sizeof(uart_attr));
	int ret = read_cont2buf("/etc/uart_cfg",(char*)&uart_attr,sizeof(uart_attr));
	if(ret != 0) {
		memset(&uart_attr,0,sizeof(uart_attr));
	}
	while(!exit_flag) {
		if(uart_attr.valid) {
			if(fd <= 0) {
				fd = init_com_dev("/dev/ttyS1",0,&uart_attr);
			}
			
			if(fd <= 0) {
				ERROR("Open ttyS1 error!\n");
				uart_attr.fd = -1;
				sleep_seconds_intr(60);
				continue;
			} else {
				NOTE("ttyS1 open success %d\n",fd);
				memset(buffer,0,sizeof(buffer));
				len = read(fd,buffer,sizeof(buffer)-1);
				if(len <= 0) {
					NOTE("ttyS1 read error! %d:%s\n",errno,strerror(errno));
					if(fd > 0) close(fd);
					fd = -1;
					continue;
				}
				if(uart_attr.workmode == 1) {
					gettimeofday(&last_tv,NULL);
					if(glb_remote_socket > 0) { 
						memset(sendbuf,0,sizeof(sendbuf));
						sprintf(sendbuf,"{\"type\":\"tc232up\",\"seqnum\":\"%d\",\"body\":{\"msg\":\"%s\"}}",
								count++,buffer);
						send(glb_remote_socket,sendbuf,strlen(sendbuf),0);
					}
				} else if(uart_attr.workmode == 0) {
					gettimeofday(&now_tv,NULL);
					
					if(now_tv.tv_sec - last_tv.tv_sec >= uart_attr.interval) {
						gettimeofday(&last_tv,NULL);
						if(fp) { fclose(fp); fp = NULL;};
						memset(filebuf,0,sizeof(filebuf));
						read_file(UART_BUF_FILE,filebuf,sizeof(filebuf)-1);
						memset(sendbuf,0,sizeof(sendbuf));
						sprintf(sendbuf,"{\"type\":\"tc232up\",\"seqnum\":\"%d\",\"body\":{\"msg\":\"%s\"}}",
						count++,filebuf);
						
						if(glb_remote_socket > 0) 
							send(glb_remote_socket,sendbuf,strlen(sendbuf),0);
						
					} else {
						if(fp) {
							fwrite(buffer,1,len,fp);
						} else {
							fp = fopen(UART_BUF_FILE,"w+");
							if(!fp){
								ERROR("Create uart buf file error!thread exit!\n");
								return NULL;
							}
						}
					}
				}
			}
		} else {
			if(fd>0) {
				close(fd);
				fd = -1;
				uart_attr.fd = -1;
			}
			sleep_seconds_intr(3);
		}
	}
	if(fd>0) close(fd);
	NOTE("TTY Thread exit...\n");
	return NULL;
}

void* handle_agps(int *arg)
{
	char ttydev[32] = {0};
	char atcfgfile[64] = {0};
	char one_line[512] = {0};
	char *p = one_line;
	char ch = 0;
	int valid_flag = 0;
    int count = 0;
	int fd = -1;
	
	NOTE("AGPS Thread start...\n");
WAIT_TYPE:
	read_file("/tmp/modem_type",one_line,64);
	if(strstr(one_line,"EC20")) {
		strcpy(ttydev,"/dev/ttyUSB1");
		strcpy(atcfgfile,"/etc/gcom/longsangagps.gcom");
	} else if(strstr(one_line,"LONGSUNG")) {
		strcpy(ttydev,"/dev/ttyUSB3");
		strcpy(atcfgfile,"/etc/gcom/ec20agps.gcom");
	} else {
		NOTE("AGPS Thread wait modem_type [%s], def EC20\n",one_line);
		//sleep_seconds_intr(3);
		//memset(one_line,0,sizeof(one_line));
		//goto WAIT_TYPE;
		strcpy(ttydev,"/dev/ttyUSB1");
		strcpy(atcfgfile,"/etc/gcom/longsangagps.gcom");
	}
	
	if(access(ttydev,F_OK) == 0) {
		NOTE("send AGPS Start AT CMD\n");
		sprintf(one_line,"/usr/bin/gcom -d %s -s %s > /tmp/agps_status",ttydev,atcfgfile);
		//system(one_line); // exec at 3g.sh
	}
	
	NOTE("AGPS dev is %s\n",ttydev);
	NOTE("AGPS AT cmd is %s\n",atcfgfile);
	while(!exit_flag) {
		if(access(ttydev,F_OK) != 0) {
			ERROR("APGS:DEV %s is not exist!\n",ttydev);
			if(fd > 0) port_close(fd);
			fd = -1;
			sleep_seconds_intr(120);
			continue;
		}
		
		if(fd < 0) {
			NOTE("APGS: Reopen GPS dev %s\n",ttydev);
			fd = open_gps_com(ttydev);
			if(fd < 0)
				goto RECONN;
		}
		ch = get_agps_char(fd);
		if(ch < 0)
			goto RECONN;
		if(ch == '$') {
			memset(one_line,0,512);
			p = one_line;
			*p = ch;
			valid_flag = 1;
			count = 0;
		} else if(ch == '\r' || ch == '\n' || count >510) {
			if(valid_flag) {
				//valid data is between '$' to '\r' or '\n'
				//add ',' tail
				*p = ',';
				p++;
				*p = '\0';
 				//printf("[%d][%s]\n",++count,one_line);
				parse_var(one_line,"/tmp/agps_info"); 
				//the all gps info,can not sleep here
			}
			valid_flag = 0;
			count = 0;
		}
		if(valid_flag) {
			*p = ch;
			p++;
			count++;
		}
		continue;
	RECONN:
		ERROR("Read AGPS Com Error!%d:%s\n",errno,strerror(errno));
		if(fd > 0) port_close(fd);
		fd = -1;
		sleep_seconds_intr(60);
	}
	NOTE("AGPS Thread exit! need to close AT ?\n");
	if(fd >0) port_close(fd);
	system("/bin/echo AGPS_thread_EXIT > /tmp/agps_status");
	return NULL;
}

static int sock_send(int sock, char *send_buf,int send_len)
{
	int ret = 0;
	int count = 2;
	while(count--) {
		ret = send(sock,send_buf,send_len,0);
		if(ret < 0) {
			if((errno == EINTR || errno == EWOULDBLOCK || errno == EAGAIN)) {
				usleep(100*1000);
				continue;
			}
			ERROR("socket send error!%d:%s\n",errno,strerror(errno));
			return -1;
		} else if(ret == 0) {
			ERROR("socket send sock had closed!\n");
			close(sock);
			return 0;
		} else
			break;
	}
	
	if(ret != send_len)
		WARN("Warning! socket send %d < %d\n",ret,send_len);

	return ret;
}

static int waiting_timeout(int socket,int second)
{
	struct timeval tv;
RECONN:
	tv.tv_sec = second;
	tv.tv_usec = 0;
	
	fd_set rset;
	FD_ZERO(&rset);
	FD_SET(socket,&rset);
	int n = select(socket+1,&rset,NULL,NULL,&tv);
	if(n < 0) {
		if(errno == EINTR)
			goto RECONN;
		ERROR("recv_timeout select() error=%d\n",errno);
		return -1;
	} else if (0 == n) {
		ERROR("recv_timeout select time out.\n");
		return -1;
	} else {
		DEBUG("socket Recved.\n");
	}
	if(FD_ISSET(socket, &rset) <= 0) {
		ERROR("something wrong while waiting for socket,error=%d\n",errno);
		return -1;
	}
	return 0;
}

static void renew_clock(const char *time)
{
	if(!time || strcmp(time,"") == 0) return;
	
	NOTE("system time update-%s\n",time);
	char cmd[128] = {0};
	sprintf(cmd,"/bin/date -s \"%s\"",time);
	system(cmd);
	// restart cron service
	system("/etc/init.d/cron restart");
}

/* return >0 ok */
int send_login(int socket,unsigned int seqnum)
{
	int ret = 0;
	char strnum[32] = {0};
	sprintf(strnum,"%d",seqnum);
	
	nvram_renew("/tmp/pub_info");
	nvram_renew(BOARD_INFO_FILE);
	char *ptr = create_json_msg_login(("./login_file"),strnum);
	ret = sock_send(socket,ptr,strlen(ptr));
	free(ptr);
	
	//wait ok!
	if(ret > 0) {
		char recvbuf[256] = {0};
		NOTE("Login Wait...\n");
		ret = waiting_timeout(socket,3); //wait for 3s
		if(ret < 0)
			return -1;
		
		ret = recv(socket,recvbuf,sizeof(recvbuf)-1,0);
		if(ret > 0) {
			char retmsg[128] = {0};
			if(0 == parse_json_ret(recvbuf,retmsg)) {
				//ok
				NOTE("Login OK. Server Time;%s:%s\n",recvbuf,retmsg);
				renew_clock(retmsg);
				return 1;
			} else {
				//error format
				ERROR("remote socket msg format error:%s\n",recvbuf);
				return -1;
			}
		} else {
			ERROR("remote socket recv error,%d:%s\n",errno,strerror(errno));
			return -1;
		}
	} else {
		return -2; // write socket error
	}
	return 0;
}

void read_timing_cfg(char *buffer,int len)
{
	read_file(TIMING_FILE,buffer,len);
}

void read_gpio11(char *value)
{
#define GPIO_PATH "/sys/class/leds/hame:red:gpioctrl/brightness"
	read_file(GPIO_PATH,value,1);
}

/* return >0 ok */
int send_board_info(int socket,unsigned int seqnum)
{
	int ret = 0;
	char strnum[32] = {0};
	sprintf(strnum,"%u",seqnum);

	nvram_renew(BOARD_INFO_FILE);
	nvram_renew("/tmp/pub_info");
	//nvram_renew("/tmp/gpio_info");
	system("/bin/echo Default > /tmp/signal");
	system("/bin/echo signal=$(cat /tmp/sig) >> /tmp/signal");
	system("/bin/cat /tmp/module_status_file >> /tmp/signal");
	nvram_renew("/tmp/signal");
	nvram_renew("/tmp/vcc");
	char *ptr = create_json_board_info("/tmp/board_file",strnum);
	ret = sock_send(socket,ptr,strlen(ptr));
	free(ptr);
	return ret;
}

/* return >0 ok */
int send_resp(int socket,const char *code,char *msg,int seqnum,char *type)
{
	int ret = 0;
	char strnum[32] = {0};
	sprintf(strnum,"%d",seqnum);
	
	char *ptr = create_json_msg_rsp(code,msg,strnum,type);
	ret = sock_send(socket,ptr,strlen(ptr));
	free(ptr);
	return ret;
}

void* handle_timing_gpio(void *arg)
{
	NOTE("GPIO handle_timing_gpio....\n");
	int auto_ok_flag = 0;	
//	int ret = -1;
	int debug = 0;
	unsigned int count = 0;
	char lua_ret[128] = {0};
	char ctrl_msg[1024] = "";

	read_file("/etc/automanual",ctrl_msg,1024);	
	lua_call_func("/etc/decode.lua","is_ctrlmsg_valid",
					ctrl_msg,strlen(ctrl_msg),lua_ret);
	if(strstr(lua_ret,"auto")) {
		glb_auto_flag = 1;
	}
	while(!exit_flag) {
		if(!glb_auto_flag) {
			NOTE("gpio ctrl waiting...\n");
			sleep_seconds_intr(60);
			continue;
		}
		memset(ctrl_msg,0,sizeof(ctrl_msg));
		read_timing_cfg(ctrl_msg,sizeof(ctrl_msg)-1);

		if(count++ % 6 == 0) debug = 1;else debug = 0;
		if(debug)	DEBUG("GPIO Timing handle by lua....\n");
		
		memset(lua_ret,0,sizeof(lua_ret));
		lua_call_func("/etc/decode.lua","detect_timing_flag",
						ctrl_msg,strlen(ctrl_msg),lua_ret);
		if(strstr(lua_ret,"OK") && strstr(lua_ret,"AUTO")) {
			if(debug)
				NOTE("GPIO Timing,Auto Ctrl OK\n");
			auto_ok_flag = 1;
		} else {
			//error or timeout ,nothing happend
			if(auto_ok_flag) {
				NOTE("GPIO Timing timeout,to reverse GPIO Val!\n");
				lua_call_func("/etc/decode.lua","gpio_reverse",
								ctrl_msg,strlen(ctrl_msg),lua_ret);
				auto_ok_flag = 0;
			}
		}
		if(debug)
			NOTE("GET Lua ret:[%s]\n",lua_ret);
		
		sleep_seconds_intr(15);
	}
	NOTE("handle_timing_gpio exit....\n");
	return NULL;
}

void renew_timing_cfg_file(char *buf,int len)
{
	record2file(TIMING_FILE,buf,len);
}

void renew_auto_manual_file(char *buf,int len)
{
	record2file("/etc/automanual",buf,len);
}

int read_cont2buf(char *file,char *buf,int len)
{
	if(!buf) return 1;
	FILE *fp = fopen(file,"r");
	if(!fp) {
		strcpy(buf,"null");
		return 1;
	}
	int ret = fread(buf,1,len,fp);
	if(ret > 0 && ret <= len) {
		buf[ret] = '\0';
	}
	fclose(fp);
	return 0;
}

void read_timing_cfg_file(char *buf,int len)
{
	read_cont2buf(TIMING_FILE,buf,len);
}

/**@internal
 * @brief Handles SIGCHLD signals to avoid zombie processes
 *
 * When a child process exits, it causes a SIGCHLD to be sent to the
 * parent process. This handler catches it and reaps the child process so it
 * can exit. Otherwise we'd get zombie processes.
 */
void
sigchld_handler(int s)
{
	int	status;
	pid_t rc;

	//NOTE("SIGCHLD handler: Trying to reap a child");

	rc = waitpid(-1, &status, WNOHANG | WUNTRACED);

	if(rc == -1) {
		if(errno == ECHILD) {
			NOTE("SIGCHLD handler: waitpid(): No child exists now.");
		} else {
			ERROR("SIGCHLD handler: Error reaping child (waitpid() returned -1): %s", strerror(errno));
		}
		return;
	}

	if(WIFEXITED(status)) {
		//DEBUG("SIGCHLD handler: Process PID %d exited normally, status %d", (int)rc, WEXITSTATUS(status));
		return;
	}

	if(WIFSIGNALED(status)) {
		NOTE("SIGCHLD handler: Process PID %d exited due to signal %d", (int)rc, WTERMSIG(status));
		return;
	}

	NOTE("SIGCHLD handler: Process PID %d changed state, status %d not exited, ignoring", (int)rc, status);
	return;
}

void termination_handler(int sig)
{
	WARN("\n-------Catch Signal %d, Main Process Exit.-------\n",sig);
	exit_flag = 1;
	if(glb_remote_socket > 0) close(glb_remote_socket);
	sync();
	exit(1);
}

void sigusr_handler(int sig)
{
	if(sig == SIGUSR1) {
		NOTE("---to stop heartbeat watchdog---\n");
		touchfile(WTD_CTRL);
	} else if(sig == SIGUSR2) {
		NOTE("---to resume heartbeat watchdog---\n");
		unlink(WTD_CTRL);
		record2file(WTD_TRIG,"none",4);
		record2file(WTD_BRIG,"0",1);
	}
}

/** @internal 
 * Registers all the signal handlers
 */
static void init_signals(void) {
	struct sigaction sa;

	NOTE("Setting SIGCHLD handler to sigchld_handler()");
	sa.sa_handler = sigchld_handler;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	if (sigaction(SIGCHLD, &sa, NULL) == -1) {
		ERROR("sigaction(): %s", strerror(errno));
		exit(1);
	}

	NOTE("Setting SIGUSR1/2  handler to stop/resume heartbeat watchdog\n");
	sa.sa_handler = sigusr_handler;
	sigemptyset(&sa.sa_mask);
	if (sigaction(SIGUSR1, &sa, NULL) == -1) {
		ERROR("sigaction(): %s\n", strerror(errno));
		exit(1);
	}
	if (sigaction(SIGUSR2, &sa, NULL) == -1) {
		ERROR("sigaction(): %s\n", strerror(errno));
		exit(1);
	}

	/* Trap SIGPIPE */
	/* This is done so that when libhttpd does a socket operation on
	* a disconnected socket (i.e.: Broken Pipes) we catch the signal
	* and do nothing. The alternative is to exit. SIGPIPE are harmless
	* if not desirable.
	*/
	NOTE("Setting SIGPIPE  handler to SIG_IGN\n");
	sa.sa_handler = SIG_IGN;
	if (sigaction(SIGPIPE, &sa, NULL) == -1) {
		ERROR("sigaction(): %s\n", strerror(errno));
		exit(1);
	}

	NOTE("Setting SIGTERM,SIGQUIT,SIGINT handlers to termination_handler()\n");
	sa.sa_handler = termination_handler;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;

	/* Trap SIGTERM */
	if (sigaction(SIGTERM, &sa, NULL) == -1) {
		ERROR("sigaction(): %s\n", strerror(errno));
		exit(1);
	}

	/* Trap SIGQUIT */
	if (sigaction(SIGQUIT, &sa, NULL) == -1) {
		ERROR("sigaction(): %s\n", strerror(errno));
		exit(1);
	}

	/* Trap SIGINT */
	if (sigaction(SIGINT, &sa, NULL) == -1) {
		ERROR("sigaction(): %s\n", strerror(errno));
		exit(1);
	}
}

void daemonize(const char *cmd)  
{  
    int         i, fd0, fd1, fd2;  
    pid_t           pid;  
    struct rlimit       rl;  
    struct sigaction    sa;  

    umask(0);   // Clear file creation mask. 

    if (getrlimit(RLIMIT_NOFILE, &rl) < 0)  {   // Get maximum number of file descriptors. 
        printf("%s: can't get file limit", cmd);  
    }  
    if ((pid = fork()) < 0)  {  //这一步fork保证进程不是进程组组长进程 
        printf("%s: can't fork", cmd);  
    }  
    else if (pid != 0) {    /* parent */  
        exit(0);  
    }  
    setsid();  // 创建一个回话，会话只包含子进程，且子进程是会话首进程 
    /* 
    会话首进程的退出会出发SIGHUP信号 
    默认此信号的操作会终止进程 
     */  
    sa.sa_handler = SIG_IGN;  
    sigemptyset(&sa.sa_mask);  
    sa.sa_flags = 0;  
    if (sigaction(SIGHUP, &sa, NULL) < 0)  {  
        printf("%s: can't ignore SIGHUP", cmd);  
    }  
    /* 
    再次创建子进程，退出父进程，保证守护进程不是会话首进程，这样open的时候就不会被分配终端 
    */  
    if ((pid = fork()) < 0)  {  
        printf("%s: can't fork", cmd);  
    }  
    else if (pid != 0) {  /* parent */  
        exit(0);  
    }  

    if (chdir("/") < 0)   {  // 改变当前工作路径为根目录
        printf("%s: can't change directory to /", cmd);  
    }  
    if (rl.rlim_max == RLIM_INFINITY)   {  //关闭所有打开的文件描述符
        rl.rlim_max = 1024;  
    }  
    for (i = 0; i < rl.rlim_max; i++)    {  
        close(i);  
    }  
    /* 
     因为前面关闭了所有的文件描述符，此时open返回的必定是最小的0，后面两次dup返回的依次是1、2，
     也就完成了对标准输入、标准输出、标准错误重定向至/dev/null的操作 
     */  
    fd0 = open("/dev/null", O_RDWR);  
    fd1 = dup(0);  
    fd2 = dup(0);  

    /* 
     * Initialize the log file. 
     */  
    openlog(cmd, LOG_CONS|LOG_NOWAIT, LOG_DAEMON);  
    if (fd0 != 0 || fd1 != 1 || fd2 != 2)   
    {  
        syslog(LOG_ERR, "unexpected file descriptors %d %d %d",fd0, fd1, fd2);  
        exit(1);  
    }  
}

#define SEND_RESP(sock,code,msg,seqnum,type) if(send_resp(sock,code,msg,seqnum,type) <= 0) { \
								ERROR("Send RSP error!%d:%s\n",errno,strerror(errno)); \
								return -1;} else {return 0;}


static int handle_gpiotiming(int socket, char *recv_buf)
{
	unsigned int seqnum = 0;
	char lua_ret[128] = {0};

	seqnum = parse_json_seqnum(recv_buf);
	lua_call_func("/etc/decode.lua","is_gpiotiming_valid",
				recv_buf,strlen(recv_buf),lua_ret);
	if(seqnum == -1 || strstr(lua_ret,"valid") == NULL) {
		//error format
		SEND_RESP(socket,"failure",lua_ret,99,"gpiotiming");
	} else {
		//record data to /etc/timing_config file
		renew_timing_cfg_file(recv_buf,strlen(recv_buf));
		SEND_RESP(socket,"success","Cfg gpiotiming OK",seqnum,"gpiotiming");
	}
	return 0;
}

static int handle_gpiotype(int socket, char *recv_buf)
{
	unsigned int seqnum = 0;
	char lua_ret[128] = {0};

	seqnum = parse_json_seqnum(recv_buf);
	lua_call_func("/etc/decode.lua","is_ctrlmsg_valid",
				recv_buf,strlen(recv_buf),lua_ret);
	if(strstr(lua_ret,"auto")) {
		glb_auto_flag = 1;
		renew_auto_manual_file(recv_buf,strlen(recv_buf));
		SEND_RESP(socket,"success","Ctrl GPIO OK:auto",seqnum,"gpiotype");
	} else if(strstr(lua_ret,"manual")) {
		glb_auto_flag = 0;
		renew_auto_manual_file(recv_buf,strlen(recv_buf));
		SEND_RESP(socket,"success","Ctrl GPIO OK:manual",seqnum,"gpiotype");
	} else {
		SEND_RESP(socket,"failure",lua_ret,seqnum,"gpiotype");
	}

	return 0;
}

static int handle_read_gpio_cfg(int socket, char *recv_buf)
{
	unsigned int seqnum = 0;
	seqnum = parse_json_seqnum(recv_buf);
	if(seqnum == -1) {
		//error format
		SEND_RESP(socket,"failure","Invalid readtiming MSG",99,"readtiming");
	} else {
		char info[1024] = "";
		read_timing_cfg_file(info,sizeof(info)-1);
		SEND_RESP(socket,"success",info,seqnum,"readtiming");
	}

	return 0;
}

static int handle_ota(int socket, char *recv_buf, int *exit_flag)
{
	char lua_ret[128] = "";
	char ota_ret[128] = "";
	unsigned int seqnum = 0;
	int ret = 0;
	OTA_ST ota_info;
	memset(&ota_info,0,sizeof(ota_info));

	seqnum = parse_json_seqnum(recv_buf);
	if(seqnum == -1) {
		//error format
		SEND_RESP(socket,"failure","Invalid OTA MSG",99,"ota");
	} else {
		if(0 == parse_json_ota_msg(recv_buf,&ota_info)) {
			//ok
			send_resp(socket,"success","Begin Download",seqnum,"ota");
			ret = do_ota(ota_info.url,ota_info.version,ota_ret,"/tmp/firmware.img");
			if(ret == 0) {
				memset(lua_ret,0,sizeof(lua_ret));
				NOTE("Get MD5:[%s]\n",ota_info.md5);
				read_file("/tmp/firmmd5",lua_ret,sizeof(lua_ret)-1);
		
				//lua_call_func("/etc/flashops.lua","action_flashops",
				//ota_info.md5,strlen(ota_info.md5),lua_ret);
				//if(strstr(lua_ret,"OK")) {
				if(strlen(ota_info.md5) == 32 && strstr(lua_ret,ota_info.md5)) {
					*exit_flag = 1;
					send_resp(socket,"success","Begin Flashing clear all",seqnum,"ota");
					system("/usr/bin/killall dropbear uhttpd crond");
					system("/sbin/sysupgrade -n /tmp/firmware.img");
					//exit_flag = 1;
				} else {
					ERROR("MD5 failed!%s\n",lua_ret);
					SEND_RESP(socket,"failure","md5 failed",seqnum,"ota");
				}
			} else {
				SEND_RESP(socket,"failure",ota_ret,seqnum,"ota");
			}
		} else {
			//error format
			SEND_RESP(socket,"failure","Invalid OTA MSG",seqnum,"ota");
		}
	}
	return 0;
}

static int handle_checktime(int socket, char *recv_buf)
{
	char timeinfo[128] = "";
	int ret = 0;
	int seqnum = 0;
	ret = parse_json_svrtime(recv_buf,&seqnum,timeinfo);
	if(ret != 0) {
		SEND_RESP(socket,"failure","Invalid Time MSG",seqnum,"checktime");
	} else {
		renew_clock(timeinfo);
		SEND_RESP(socket,"success","update Time OK",seqnum,"checktime");
	}
	return 0;
}

static int update_uci_remote_config(char *devid,char *ipaddr,char *port)
{
	char cmd[1024];
	sprintf(cmd,"/sbin/uci set netset.@netset[0].deviceid=%s && \
		/sbin/uci set netset.@netset[0].remote_ip=%s && \
		/sbin/uci set netset.@netset[0].remote_port=%s && /sbin/uci commit",
		devid,ipaddr,port);
	
	return system(cmd);
}

static int update_uci_ppp_config(char *dialtype,char *apn,char *user,char *pwd,char *dialnum)
{
	char cmd[1024];
	sprintf(cmd,"/sbin/uci set network.ppp.private_dial=%s && \
		/sbin/uci set network.ppp.apn=%s && \
		/sbin/uci set network.ppp.username=%s && \
		/sbin/uci set network.ppp.password=%s && \
		/sbin/uci set network.ppp.dialnumber=%s && \
		/sbin/uci commit",
		dialtype,apn,user,pwd,dialnum);
	
	return system(cmd);
}

/* return -1, socket send error
 0, socket send success,
 1, socket send ok ,then to exit */

static int handle_msg(int socket, char *recv_buf)
{
	int ret = 0;
	int seq = 0;
	if(strstr(recv_buf,"gpiotiming")) {
		NOTE("[Get Svr],gpiotiming msg!\n");
		if(handle_gpiotiming(socket,recv_buf) != 0)
			return -1;

	} else if(strstr(recv_buf,"gpiotype")) {
		NOTE("[Get Svr],gpio ctrl msg!\n");
		if(handle_gpiotype(socket,recv_buf) != 0)
			return -1;

	} else if(strstr(recv_buf,"readtiming")) {
		NOTE("[Get Svr],readtiming msg!\n");
		if(handle_read_gpio_cfg(socket,recv_buf) != 0)
			return -1;

	} else if(strstr(recv_buf,"\"type\":\"ota\"")) {
		NOTE("[Get Svr],ota msg!\n");
		int flag = 0;
		if(handle_ota(socket,recv_buf,&flag) != 0) {
			return -1;
		} else {
			if(flag) {
				return 1;
			}
		}
	} else if(strstr(recv_buf,"checktime")) {
		NOTE("[Get Svr],checktime msg!\n");
		if(handle_checktime(socket,recv_buf) != 0)
			return -1;

	} else if(strstr(recv_buf,"reboot")) {
		NOTE("[Get Svr],reboot msg!\n");
		ret = parse_json_seqnum(recv_buf);
		if(ret == -1) {
			SEND_RESP(socket,"failure","format error",99,"reboot");
		} else {
			send_resp(socket,"success","now to reboot",ret,"reboot");
			system("/sbin/reboot");
			return 1;
		}
	}  else if(strstr(recv_buf,"reset")) {
		NOTE("[Get Svr],reset msg!\n");
		ret = parse_json_seqnum(recv_buf);
		if(ret == -1) {
			SEND_RESP(socket,"failure","format error",99,"reset");
		} else {
			send_resp(socket,"success","now to reset",ret,"reset");
			system("/bin/echo \"y\" | /sbin/jffs2reset && /sbin/reboot");
			return 1;
		}
	} else if(strstr(recv_buf,"readlog")) {
		NOTE("[Get Svr],readlog msg!\n");
		char type[128] = "",logbuf[4096] = "";
		
		ret = parse_json_readlog(recv_buf,&seq,type);
		if(ret == -1) {
			SEND_RESP(socket,"failure","format error",99,"readlog");
		} else {
			if(strstr(type,"ETH")) {
				system("/sbin/logread | grep -i 'link changed' > /tmp/eth-log");
				read_cont2buf("/tmp/eth-log",logbuf,sizeof(logbuf)-1);
				SEND_RESP(socket,"success",logbuf,seq,"readlog");
				
			} else if(strstr(type,"PPP")) {
				system("/sbin/logread | grep -i 'ppp' > /tmp/pppd-log");
				read_cont2buf("/tmp/pppd-log",logbuf,sizeof(logbuf)-1);
				SEND_RESP(socket,"success",logbuf,seq,"readlog");
				
			} else if(strstr(type,"NET4G")) {
				system("/sbin/logread | grep -i 'net4g' > /tmp/net4g-log");
				read_cont2buf("/tmp/net4g-log",logbuf,sizeof(logbuf)-1);
				SEND_RESP(socket,"success",logbuf,seq,"readlog");
				
			} else if(strstr(type,"USB")) {
				system("/bin/dmesg | grep 'ttyUSB' > /tmp/ttyusb-log");
				read_cont2buf("/tmp/ttyusb-log",logbuf,sizeof(logbuf)-1);
				SEND_RESP(socket,"success",logbuf,seq,"readlog");
				
			} else if(strstr(type,"AT")) {
				read_cont2buf("/tmp/at_failed_ret",logbuf,sizeof(logbuf)-1);
				SEND_RESP(socket,"success",logbuf,seq,"readlog");
			} else {
				read_cont2buf("/tmp/remotecmd",logbuf,sizeof(logbuf)-1);
				SEND_RESP(socket,"success",logbuf,seq,"readlog");
			}
		}
	} else if(strstr(recv_buf,"runcmd")) {
		NOTE("[Get Svr],runcmd msg!\n");
		char do_cmd[256] = "";
		
		ret = parse_json_runcmd(recv_buf,&seq,do_cmd);
		if(ret == 0) {
			ret = system(do_cmd);
			DEBUG("do cmd %s ret=%d\n",do_cmd,ret);
			SEND_RESP(socket,"success","system runcmd",seq,"runcmd");
		} else {
			SEND_RESP(socket,"failure","format error",seq,"runcmd");
		}
	} else if(strstr(recv_buf,"setplatform")) {
		NOTE("[Get Svr],setplatform msg!\n");
		char devid[128] = "",ipaddr[128] = "", port[128] = "";
	
		ret = parse_json_remote_cfg(recv_buf,&seq,devid,ipaddr,port);
		//DEBUG("1-ret=%d,%s,%s,%s\n",seq,devid,ipaddr,port);
		if(ret == 0) {
			//write config to file
			update_uci_remote_config(devid,ipaddr,port);
			//resp to server
			SEND_RESP(socket,"success","OK",seq,"setplatform");
		} else {
			SEND_RESP(socket,"failure","format error",seq,"setplatform");
		}
	} else if(strstr(recv_buf,"getplatform")) {
		NOTE("[Get Svr],getplatform msg!\n");
		ret = parse_json_seqnum(recv_buf);
		if(ret == -1) {
			SEND_RESP(socket,"failure","format error",99,"getplatform");
		} else {
			char platinfo[512] = {0};
			sprintf(platinfo,"{\"id\":\"%s\",\"ip\":\"%s\",\"port\":\"%s\"}",
					nvram_get("deviceid"),nvram_get("remote_ip"),nvram_get("remote_port"));
			SEND_RESP(socket,"success",platinfo,ret,"getplatform");
		}
	} else if(strstr(recv_buf,"setdialparameters")) {
		NOTE("[Get Svr],setdialparameters msg!\n");
		char dialtype[128] = "",apn[128] = "",user[128] = "",pwd[128] = "", dialnum[128] = "";
		
		ret = parse_json_ppp_cfg(recv_buf,&seq,dialtype,apn,user,pwd,dialnum);
		//DEBUG("2-ret=%d,%s,%s,%s,%s,%s\n",seq,dialtype,apn,user,pwd,dialnum);
		if(ret == 0) {
			update_uci_ppp_config(dialtype,apn,user,pwd,dialnum);
			SEND_RESP(socket,"success","OK",seq,"setdialparameters");
		} else {
			SEND_RESP(socket,"failure","format error",seq,"setdialparameters");
		}
		
	} else if(strstr(recv_buf,"getdialparameters")) {
		NOTE("[Get Svr],getdialparameters msg!\n");
		ret = parse_json_seqnum(recv_buf);
		if(ret == -1) {
			SEND_RESP(socket,"failure","format error",99,"getdialparameters");
		} else {
			nvram_renew("/tmp/dialcfg");
			char platinfo[1024] = {0};
			sprintf(platinfo,"{\"network\":\"%s\",\"apn\":\"%s\",\"user\":\"%s\",\"password\":\"%s\",\"dialnumber\":\"%s\"}",
				nvram_get("4Gdial"),nvram_get("apn"),nvram_get("username"),nvram_get("password"),nvram_get("dialnumber"));
			SEND_RESP(socket,"success",platinfo,ret,"getdialparameters");
		}
	} else if(strstr(recv_buf,"tc232")) {
		// ctrl cmd
		char tty_cmd[1024] = "";
		ret = parse_json_tty_trans(recv_buf,&seq,tty_cmd);
		if(0 == ret) {
			if(uart_attr.fd > 0) {
				write(uart_attr.fd,tty_cmd,strlen(tty_cmd));
				SEND_RESP(socket,"success","ctrl uart ok",seq,"tc232");
			} else {
				SEND_RESP(socket,"failure","uart open failed",seq,"tc232");
			}
		} else {
			SEND_RESP(socket,"failure","format error",99,"tc232");
		}
	} else if(strstr(recv_buf,"set232parameters")) {
		//set uart attr
		memset(&uart_attr,0,sizeof(ST_UART));
		ret = parse_json_tty_cfg(recv_buf,&seq,&uart_attr);
		if(0 == ret) {
			uart_attr.valid = 1;
			record2file("/etc/uart_cfg",(char*)&uart_attr,sizeof(uart_attr));
			SEND_RESP(socket,"success","set uart ok",seq,"set232parameters");
		} else {
			uart_attr.valid = 0;
			SEND_RESP(socket,"failure","format error",99,"set232parameters");
		}
		
	} else if(strstr(recv_buf,"setwifiuplist")) {
		ret = parse_json_wifiup_cfg(recv_buf,&seq);
		if(ret > 0) {
			NOTE("send usr2 to ap_client ,renew wifi up list\n");
			system("pidof ap_client && kill -usr2 ap_client");
			SEND_RESP(socket,"success","set wifi up ok",seq,"setwifiuplist");
		}
		else {
			SEND_RESP(socket,"failure","format error",seq,"setwifiuplist");
		}

	} else if(strstr(recv_buf,"respcode")) {
		//ret = parse_json_seqnum(recv_buf);
		NOTE("[Get Svr],response msg,%s!\n",recv_buf);
		if(0 == parse_json_ret(recv_buf,NULL)) {
			//ok
		} else {
			//error format
		}
	} else {
		NOTE("[Get Svr]Invalid CMD,%s\n",recv_buf);
		ret = send_resp(socket,"failure","Invalid CTRL MSG",99,"Invalid Type");
		if(ret <= 0)
			return -1;
	}
	return 0;
}

#define LOGIN_INFO		"\"type\":\"login\""
#define OTA_INFO		"\"type\":\"ota\""
#define TIMIMGCFG_INFO	"\"type\":\"gpiotiming\""
#define GPIOCTRL_INFO	"\"type\":\"gpiotype\""	
#define RESP_INFO		"respcode"	

int main(int argc,char **argv)
{
	if(argc >=2 && strcmp(argv[1],"-d") == 0) {
		//daemon(0,0); //maybe cause SIGTOP tty Interrupt
		daemonize(argv[0]);
		syslog(LOG_INFO, "Starting as daemon\n");
	}
	/*
	 * Make sure only one copy of the daemon is running.
	 */
	if (already_running()) {
		syslog(LOG_ERR, "net4g daemon already running\n");
		exit(1);
	}
	
	// close timer on gpio
	record2file(WTD_TRIG,"none",4);
	record2file(WTD_BRIG,"0",1);
	
	glb_cfg = config_init();
	nvram_renew("/tmp/board_info");
	nvram_renew("/tmp/pub_info");
	
//	nvram_buflist();
	
	init_signals();
#if 0
	pthread_t pid = 0;
	if(pthread_create(&pid,NULL,(void*)handle_gps,NULL) < 0 ) {
		ERROR("create gps thread error!\n");
		exit(1);
	}
	pthread_detach(pid);
#else
	pthread_t tpid = 0;
	if(pthread_create(&tpid,NULL,(void*)handle_tty,NULL) < 0 ) {
		ERROR("create tty thread error!\n");
		exit(1);
	}
	pthread_detach(tpid);
#endif

#if 1
	pthread_t apid = 0;
	if(pthread_create(&apid,NULL,(void*)handle_agps,NULL) < 0 ) {
		ERROR("create Agps thread error!\n");
		exit(1);
	}
	pthread_detach(apid);
#endif
	
	pthread_t ptid = 0;
	if(pthread_create(&ptid,NULL,(void*)handle_timing_gpio,NULL) < 0 ) {
		ERROR("create GPIO thread error!\n");
		exit(1);
	}
	pthread_detach(ptid);
	
	int disconnected = 1;
	int faild_login = 1;
	int ret = 0;
	int socket = 0;
	struct timeval last_tv;
	struct timeval now_tv;
	unsigned int login_count = 1;
	unsigned int info_count = 1;
	unsigned int failcount = 0;
	char recv_buf[1024] = {0};
	fd_set fds;
	struct timeval tv;
		
	while(!exit_flag && failcount < 360) {
		usleep(10*1000);
		record2file(WTD_BRIG,"1",1);
		
		if(access("/dev/ttyUSB2",F_OK) != 0) {
			ERROR("waiting for detecting 4G modult;ttyUSB2\n");
			goto DISCONN;
		}
		
		//if(access("/tmp/dialok",F_OK) != 0) {
		//	ERROR("waiting for ppp dial OK\n");
		//	goto DISCONN;
		//}
		
		if(disconnected) {
			record2file("/tmp/onoffline","Offline",7);
			//system("/bin/echo Offline > /tmp/onoffline");
			faild_login = 1;
			nvram_renew("/tmp/board_info");
			socket = init_connect(nvram_get("remote_ip"),atoi(nvram_get("remote_port")),1);
			NOTE("remote socket = %d\n",socket);
			if(socket >= 0) {
				gettimeofday(&last_tv,NULL);
				disconnected = 0;
				set_socket_keepalive(socket);
				glb_remote_socket = socket;
				failcount = 0;
			} else {
				failcount++;
				glb_remote_socket = -1;
				record2file(WTD_BRIG,"0",1);
				sleep_seconds_intr(30);
				continue;
			}
		}
		// login
		if(faild_login) {
			record2file("/tmp/onoffline","Offline",7);
			//system("/bin/echo Offline > /tmp/onoffline");
			nvram_renew("/tmp/board_info");
			ret = send_login(socket,login_count++);
			if(ret > 0) {
				gettimeofday(&last_tv,NULL);
				faild_login = 0;
				login_count = 0;
				//system("/bin/echo Online > /tmp/onoffline");
				record2file("/tmp/onoffline","Online",6);
			} else if(ret == -2){
				//socket error
				ERROR("Error send login!\n");
				goto DISCONN;
			} else {
				//recv msg format error
				NOTE("To relogin!\n");
				failcount++;
				record2file(WTD_BRIG,"0",1);
				sleep_seconds_intr(30);
				continue;
			}
		}

		
		FD_ZERO(&fds);
		FD_SET(socket, &fds); 
		/* init socket timeout, set to 90 seconds , keepalive=90s,5,9count*/
		tv.tv_sec = 90;
		tv.tv_usec = 0;

		//server handle socket event
		if((ret = select(socket + 1, &fds, NULL, NULL, &tv)) < 0)
		{
			if(errno == EINTR) {
				//gettimeofday(&last_tv,NULL);
				NOTE("server socket select EINTR\n");
				record2file(WTD_BRIG,"0",1);
				continue;
			} else {
				ERROR("select error:%d\n",errno);
				goto DISCONN;
			}
		} else if(ret == 0) {
			ret = send_board_info(socket,info_count++);
			if(ret <= 0) {
				ERROR("Error send gpsinfo!\n");
				goto DISCONN;
			} else {
				if(failcount++ > 3) {
					ERROR("----Send-Q++, Recv-Q == 0---\n");
					goto DISCONN;
				}
				record2file(WTD_BRIG,"0",1);
				continue;
			}
		}
		if(FD_ISSET(socket, &fds) <= 0) {
			ERROR("something wrong while waiting for socket,error:%d\n",errno);
			goto DISCONN;
		}
		
		memset(recv_buf,0,sizeof(recv_buf));
		ret = recv(socket,recv_buf,1023,0);
		if(ret <= 0) {
			if((errno == EINTR || errno == EWOULDBLOCK || errno == EAGAIN))
				continue;
			ERROR("Error while recv socket:%d:%s\n",errno,strerror(errno));
			goto DISCONN;
		}
		NOTE("RECV:%s\n",recv_buf);
		ret = handle_msg(socket,recv_buf);
		if(ret < 0) {
			goto DISCONN;
		} else if(ret == 1) {
			exit_flag = 1;
			goto EXIT;
		} else {
			//ok
		}
		failcount = 0;
		record2file(WTD_BRIG,"0",1);
		gettimeofday(&last_tv,NULL);
		continue;
		
	DISCONN:
		failcount++;
		glb_remote_socket = -1;
		disconnected = 1;
		faild_login = 1;
		if(socket > 0) close(socket);
		socket = -1;
		sleep_seconds_intr(25);
		record2file(WTD_BRIG,"0",1);
		sleep_seconds_intr(1);
	} //end while(1)
EXIT:
	exit_flag = 1;
	if(socket > 0) close(socket);
	NOTE("net4g process exit!!\n");
	config_close(glb_cfg);
	closelog();
	return 0;
}

