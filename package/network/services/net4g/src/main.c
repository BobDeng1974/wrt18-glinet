#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <signal.h>
#include <unistd.h>
#include <time.h>
#include <sys/types.h>  
#include <sys/socket.h>  
#include <arpa/inet.h>  
#include <termios.h>
#include <sys/un.h>
#include <syslog.h>
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

#include "net_base.h"
#include "net_json.h"
#include "config.h"

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#define CFG_FILE		"/etc/cfg_wind"
#define TIMING_FILE    "/etc/timing_config"
#define BOARD_INFO_FILE "/etc/board_info"

#define WAITTING_TIME	10  //s
#define LOGIN_TIME		10   //s

#if 1
#define DEBUG(fmt, ...)  syslog(LOG_DEBUG, fmt, ## __VA_ARGS__)
#define NOTE(fmt, ...) syslog(LOG_NOTICE, fmt, ## __VA_ARGS__)
#define WARN(fmt, ...) syslog(LOG_WARNING,fmt, ## __VA_ARGS__)
#define ERROR(fmt, ...)  syslog(LOG_ERR, fmt, ## __VA_ARGS__)
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
	
	strncpy(ret,lua_tostring(L,-1),255);
	lua_pop(L,1);
// 	printf("lua_call_func-ret[%ld]:%s\n",strlen(ret),ret);
EXIT:
 	lua_close(L);
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
        tv.tv_sec = 10;
        tv.tv_usec = 0;
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
            NOTE("Connectd.%s:%d\n",ip_addr,port);
        }
    }
    
    fcntl(cli_fd,F_SETFL,flags & ~O_NONBLOCK);
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

int parse_var(char *oneline,char *file)
{
	if(! strstr(oneline,"RMC")) {
		return -1;
	}
	enum {
		NAME,
		UTC,
		LAT_FLAG,
		LATITUDE,
		SN,
		LONGITUDE,
		WE,
		SPEED,
		DIR,
		DATE,
		CIPJ,
		CIPJE,
		LOCT_MODE,
		CRC
	};

	char gpsinfo[16][16] = {0};
	char *p = oneline;
	char *base = oneline;
	int count = 0;
	while(*p != '\0') {
		if(*p == ',') {
			*p = ' ';
			//printf("%s\n",base);
			base = ++p;
			count++;
			continue;
		} else {
			p++;
		}
	}
	//last key [x*crc]
	if(p = strchr(base,'*')) {
		*p = ' ';
		count++;
	}
	if(count != 13) {
		ERROR("Invalid MSG! ',' != 13, key != 14\n");
		return -1;
	}
	static unsigned long long check_interval = 0;
	if(!strstr(file,"agps")) {
		if(0 == (check_interval++ % 500))
			return 0;
	}
	//bug!!
	// if key = "" , then sscanf error!
	sscanf(oneline,"%s %s %s %s %s %s %s %s %s %s %s %s %s %s",
		gpsinfo[NAME],gpsinfo[UTC],gpsinfo[LAT_FLAG],gpsinfo[LATITUDE],
		gpsinfo[SN],gpsinfo[LONGITUDE],gpsinfo[WE],gpsinfo[SPEED],
		gpsinfo[DIR],gpsinfo[DATE],
		gpsinfo[CIPJ],gpsinfo[CIPJE],gpsinfo[LOCT_MODE],gpsinfo[CRC]);
	
	FILE *fp = fopen(file,"w+");
	if(!fp)
		return -1;
	char strbuf[128] = "Default\n";
	fwrite(strbuf,1,strlen(strbuf),fp);
	// A is Valid. V is invalid
	if(strcmp(gpsinfo[LAT_FLAG],"A") == 0) {
		sprintf(strbuf,"speed=%s\n",gpsinfo[SPEED]);
		fwrite(strbuf,1,strlen(strbuf),fp);
		sprintf(strbuf,"gpsdir=%s\n",gpsinfo[DIR]);
		fwrite(strbuf,1,strlen(strbuf),fp);
		sprintf(strbuf,"latitude=%s\n",gpsinfo[LATITUDE]);
		fwrite(strbuf,1,strlen(strbuf),fp);
		sprintf(strbuf,"longitude=%s\n",gpsinfo[LONGITUDE]);
		fwrite(strbuf,1,strlen(strbuf),fp);
	} else {
		fwrite("speed=\n",1,strlen("speed=\n"),fp);
		fwrite("gpsdir=\n",1,strlen("gpsdir=\n"),fp);
		fwrite("latitude=\n",1,strlen("latitude=\n"),fp);
		fwrite("longitude=\n",1,strlen("longitude=\n"),fp);
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
	int ret = 0;
	unsigned int count = 0;
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
	system("/bin/echo GPS_thread_EXIT > /tmp/gps_status");
	return NULL;
}

void* handle_agps(int *arg)
{
	char one_line[512] = {0};
	unsigned int count = 0;
	char *p = one_line;
	char ch = 0;
	int valid_flag = 0;
	
	NOTE("AGPS Thread start...\n");
	if(access("/dev/ttyUSB3",F_OK) == 0) {
		NOTE("send GPS Start AT CMD\n");
		system("/usr/bin/gcom -d /dev/ttyUSB2 -s /etc/gcom/startagps.gcom > /tmp/agps_status");
	}
	int fd = -1;
	while(!exit_flag) {
		if(access("/dev/ttyUSB3",F_OK) != 0) {
			ERROR("ttyUSB3 is not exist!\n");
			if(fd >= 0) port_close(fd);
			fd = -1;
			sleep_seconds_intr(120);
			continue;
		}
		
		if(fd < 0) {
			NOTE("Reopen ttyUSB3 dev\n");
			fd = open_gps_com("/dev/ttyUSB3");
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
		} else if(ch == '\r' || ch == '\n') {
			if(valid_flag) {
				//valid data is between '$' to '\r' or '\n'
				*p = '\0';
 				//DEBUG("[%d][%s]\n",++count,one_line);
				parse_var(one_line,"/tmp/agps_info"); 
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
		ERROR("Read AGPS Com Error!%d:%s\n",errno,strerror(errno));
		if(fd >=0) port_close(fd);
		fd = -1;
		sleep_seconds_intr(60);
	}
	NOTE("AGPS Thread exit!\n");
	system("/bin/echo AGPS_thread_EXIT > /tmp/agps_status");
	return NULL;
}

static int sock_send(int sock, char *send_buf,int send_len)
{
	int ret = 0;
	if((ret=send(sock,send_buf,send_len,0)) <= 0) {
		ERROR("socket send error!%d:%s\n",errno,strerror(errno));
		return -1;
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
	
	nvram_renew(BOARD_INFO_FILE);
	nvram_renew("/etc/pub_info");
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

void read_file(char *file,char *buf,int len)
{
	FILE *fp = fopen(file,"r");
	if(!fp)
		return;
	int ret = fread(buf,1,len,fp);
	if(ret != len)
		WARN("Read %s file!acture size %d<%d\n",file,ret,len);
	fclose(fp);
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
	sprintf(strnum,"%d",seqnum);
	
	nvram_renew(BOARD_INFO_FILE);
	nvram_renew("/etc/pub_info");
	nvram_renew("/tmp/gpio_info");
	system("/bin/echo Default > /tmp/signal");
	system("/bin/echo signal=$(cat /tmp/sig) >> /tmp/signal");
	system("/bin/cat /tmp/module_status_file >> /tmp/signal");
	nvram_renew("/tmp/signal");
	char val[4] = {0};
	read_gpio11(val);
	nvram_set("gpioval",val);
	nvram_set("gpioname","gpioctrl");
	nvram_set("gpiodir","out");
	nvram_set("gpionum","11");
	char *ptr = create_json_board_info(("/etc/board_file"),strnum);
	ret = sock_send(socket,ptr,strlen(ptr));
	free(ptr);
	return ret;
}

/* return >0 ok */
int send_resp(int socket,const char *code,const char *msg,int seqnum,char *type)
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
	int ret = -1;
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
			sleep_seconds_intr(3);
			continue;
		}
		memset(ctrl_msg,0,sizeof(ctrl_msg));
		read_timing_cfg(ctrl_msg,1024);

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
	NOTE("GPIO exit....\n");
	return NULL;
}

void record2file(char *file,char *buf,int len)
{
	FILE *fp = fopen(file,"w+");
	if(!fp)
		return;
	int ret = fwrite(buf,1,len,fp);
	if(ret != len)
		WARN("WARN!write file %s!acture size %d<%d\n",file,ret,len);
	fclose(fp);
}

void renew_timing_cfg_file(char *buf,int len)
{
	record2file(TIMING_FILE,buf,len);
}

void renew_auto_manual_file(char *buf,int len)
{
	record2file("/etc/automanual",buf,len);
}

void read_timing_cfg_file(char *buf,int len)
{
	if(!buf) return;
	FILE *fp = fopen(TIMING_FILE,"r");
	if(!fp) {
		strcpy(buf,"null");
		return;
	}
	int ret = fread(buf,1,len,fp);
	if(ret > 0) {
		buf[ret] = '\0';
	}
	fclose(fp);
}

#define SEND_RESP(sock,code,msg,seqnum,type) if(send_resp(sock,code,msg,seqnum,type) <= 0) { \
								ERROR("Send RSP error!%d:%s\n",errno,strerror(errno)); \
								goto DISCONN;}

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
	WARN("Catch Signal %d\n",sig);
	exit_flag = 1;
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

pid_t safe_fork(void)
{
	pid_t result;
	result = fork();

	if (result == -1) {
		ERROR("Failed to fork: %s.  Bailing out", strerror(errno));
		exit (1);
	} else if (result == 0) {
		/* I'm the child - do some cleanup */
		;
	}
	return result;
}

void init_daemon()
{
	int pid;
	int i;
	pid = fork();
	if(pid < 0)    
	    exit(1);
	else if( pid > 0)
	    exit(0);
	    
	setsid();
	pid = fork();
	if(pid > 0)
	    exit(0);
	else if( pid < 0)
	    exit(1);

	for(i = 0; i < 1024; i++)
		close(i);
	chdir("/");
	umask(0);
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
		NOTE("Starting as daemon, forking to background");
		init_daemon();
	}

	openlog("net4g",LOG_NOWAIT,LOG_DAEMON);
	glb_cfg = config_init();
	nvram_renew(CFG_FILE);
	nvram_renew("/etc/board_info");
	nvram_renew("/etc/pub_info");
	nvram_renew("/tmp/gpio_info");
	
//	nvram_buflist();
	
	init_signals();
	
	pthread_t pid = 0;
	if(pthread_create(&pid,NULL,handle_gps,NULL) < 0 ) {
		ERROR("create gps thread error!\n");
		exit(1);
	}
	pthread_detach(pid);
	
	pthread_t apid = 0;
	if(pthread_create(&apid,NULL,handle_agps,NULL) < 0 ) {
		ERROR("create Agps thread error!\n");
		exit(1);
	}
	pthread_detach(apid);
	
	pthread_t ptid = 0;
	if(pthread_create(&ptid,NULL,handle_timing_gpio,NULL) < 0 ) {
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
	unsigned int seqnum = 0;
	unsigned int login_count = 1;
	unsigned int info_count = 1;
	OTA_ST ota_info;
	memset(&ota_info,0,sizeof(ota_info));
	
	while(!exit_flag) {
		
		if(access("/dev/ttyUSB2",F_OK) != 0) {
			ERROR("waiting for detecting 4G modult;ttyUSBx\n");
			goto DISCONN;
		}
		
		if(disconnected) {
			record2file("/tmp/onoffline","Offline",7);
			//system("/bin/echo Offline > /tmp/onoffline");
			faild_login = 1;
			nvram_renew("/etc/board_info");
			socket = init_connect(nvram_get("remote_ip"),atoi(nvram_get("remote_port")),1);
			if(socket >= 0) {
				gettimeofday(&last_tv,NULL);
				disconnected = 0;
				set_socket_keepalive(socket);
			} else {
				sleep_seconds_intr(30);
				continue;
			}
		}
		// login
		if(faild_login) {
			record2file("/tmp/onoffline","Offline",7);
			//system("/bin/echo Offline > /tmp/onoffline");
			nvram_renew("/etc/board_info");
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
				sleep_seconds_intr(30);
				continue;
			}
		}
		
		gettimeofday(&now_tv,NULL);
		//printf("time;%ld:%ld\n",now_tv.tv_sec,last_tv.tv_sec);
		if(now_tv.tv_sec - last_tv.tv_sec >= 240) {
			gettimeofday(&last_tv,NULL);
			NOTE("net4g -- keepalive...%u\n",info_count);
		}
		
		fd_set fds;
		struct timeval tv;
		FD_ZERO(&fds);
		FD_SET(socket, &fds); 
		/* init socket timeout, set to 30 seconds */
		tv.tv_sec = 30;
		tv.tv_usec = 0;

		//server handle socket event
		if((ret = select(socket + 1, &fds, NULL, NULL, &tv)) < 0)
		{
			if(errno == EINTR) {
				//gettimeofday(&last_tv,NULL);
				NOTE("server socket select EINTR\n");
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
				continue;
			}
		}
		if(FD_ISSET(socket, &fds) <= 0) {
			ERROR("something wrong while waiting for socket,error:%d\n",errno);
			goto DISCONN;
		}
		//valid resp from server
		char recv_buf[1024] = {0};
		ret = recv(socket,recv_buf,1023,0);
		if(ret <= 0) {
			ERROR("Error while recv socket:%d:%s\n",errno,strerror(errno));
			goto DISCONN;
		}
		char lua_ret[128] = "";
		char ota_ret[128] = "";
		if(strstr(recv_buf,"gpiotiming")) {
			NOTE("[Get Svr],gpiotiming msg!\n");
			seqnum = parse_json_seqnum(recv_buf);
			lua_call_func("/etc/decode.lua","is_gpiotiming_valid",
						recv_buf,strlen(recv_buf),lua_ret);
			if(seqnum == -1 || strstr(lua_ret,"valid") == NULL) {
				//error format
				SEND_RESP(socket,"failure",lua_ret,seqnum,"gpiotiming");
			} else {
				//record data to /etc/timing_config file
				renew_timing_cfg_file(recv_buf,strlen(recv_buf));
				SEND_RESP(socket,"success","Cfg gpiotiming OK",seqnum,"gpiotiming");
			}
		} else if(strstr(recv_buf,"gpiotype")) {
			NOTE("[Get Svr],gpio ctrl msg!\n");
			memset(lua_ret,0,sizeof(lua_ret));
			seqnum = parse_json_seqnum(recv_buf);
			lua_call_func("/etc/decode.lua","is_ctrlmsg_valid",
						recv_buf,strlen(recv_buf),lua_ret);
			if(strstr(lua_ret,"auto")) {
				glb_auto_flag = 1;
				renew_auto_manual_file(recv_buf,strlen(recv_buf));
				SEND_RESP(socket,"success","Ctrl GPIO OK",seqnum,"gpiotype");
			} else if(strstr(lua_ret,"manual")) {
				glb_auto_flag = 0;
				renew_auto_manual_file(recv_buf,strlen(recv_buf));
				SEND_RESP(socket,"success","Ctrl GPIO OK",seqnum,"gpiotype");
			} else {
				SEND_RESP(socket,"failure",lua_ret,seqnum,"gpiotype");
			}
		} else if(strstr(recv_buf,"readtiming")) {
			NOTE("[Get Svr],readtiming msg!\n");
			seqnum = parse_json_seqnum(recv_buf);
			if(seqnum == -1) {
				//error format
				SEND_RESP(socket,"failure","Invalid readtiming MSG",99,"readtiming");
			} else {
				memset(recv_buf,0,sizeof(recv_buf));
				read_timing_cfg_file(recv_buf,sizeof(recv_buf));
				SEND_RESP(socket,"success",recv_buf,seqnum,"readtiming");
			}
		} else if(strstr(recv_buf,OTA_INFO)) {
			NOTE("[Get Svr],ota msg!\n");
			seqnum = parse_json_seqnum(recv_buf);
			if(seqnum == -1) {
				//error format
				SEND_RESP(socket,"failure","Invalid OTA MSG",99,"ota");
			} else {
				if(0 == parse_json_ota_msg(recv_buf,&ota_info)) {
					//ok
					SEND_RESP(socket,"success","Begin Download",seqnum,"ota");
					ret = ota(ota_info.url,ota_info.version,ota_ret,"/tmp/firmware.img");
					if(ret == 0) {
						memset(lua_ret,0,sizeof(lua_ret));
						NOTE("Get MD5:[%s]\n",ota_info.md5);
						lua_call_func("/etc/flashops.lua","action_flashops",
						ota_info.md5,strlen(ota_info.md5),lua_ret);
						if(strstr(lua_ret,"OK")) {
							SEND_RESP(socket,"success","Begin Flashing",seqnum,"ota");
							exit_flag = 1;
							goto EXIT;
						} else {
							SEND_RESP(socket,"failure",lua_ret,seqnum,"ota");
						}
					} else {
						SEND_RESP(socket,"failure",ota_ret,seqnum,"ota");
					}
				} else {
					//error format
					SEND_RESP(socket,"failure","Invalid OTA MSG",seqnum,"ota");
				}
			}
		} else if(strstr(recv_buf,"checktime")) {
			NOTE("[Get Svr],checktime msg!\n");
			ret = parse_json_svrtime(recv_buf,ota_ret);
			if(ret != 0) {
				SEND_RESP(socket,"failure","Invalid Time MSG",99,"checktime");
			} else {
				renew_clock(ota_ret);
				seqnum = parse_json_seqnum(recv_buf);
				SEND_RESP(socket,"success","Time OK",seqnum<0?99:seqnum,"checktime");
			}
		} else if(strstr(recv_buf,"reboot")) {
			NOTE("[Get Svr],reboot msg!\n");
			SEND_RESP(socket,"success","now to reboot",1,"reboot");
			system("/bin/reboot &");
			exit_flag = 1;
			goto EXIT;
		} else if(strstr(recv_buf,"respcode")) {
			seqnum = parse_json_seqnum(recv_buf);
			NOTE("[Get Svr],response msg,seqnum=%d,%s!\n",seqnum,recv_buf);
			if(0 == parse_json_ret(recv_buf,NULL)) {
				//ok
			} else {
				//error format
			}
		} else if(strstr(recv_buf,"firstboot")) {
			system("/bin/echo \"y\" | /sbin/jffs2reset && reboot &");
			exit_flag = 1;
			goto EXIT;
		} else {
			NOTE("[Get Svr]Invalid CMD,%s\n",recv_buf);
			SEND_RESP(socket,"failure","Invalid CTRL MSG",99,"Invalid Type");
		}

		gettimeofday(&last_tv,NULL);
		continue;
		
	DISCONN:
		disconnected = 1;
		faild_login = 1;
		if(socket > 0) close(socket);
		socket = -1;
		sleep_seconds_intr(30);
	} //end while(1)
EXIT:
	if(socket > 0) close(socket);
	NOTE("net4g process exit!!\n");
	config_close(glb_cfg);
	closelog();
	return 0;
}

