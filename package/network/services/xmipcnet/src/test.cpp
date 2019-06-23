#include <stdio.h>
#include <memory.h>
#include <unistd.h>
#include <fcntl.h> 
#include <signal.h>
#include <pthread.h>
#include <syslog.h>
#include <string.h>
#include <stdlib.h>
#include <glob.h>
#include <errno.h>
#include <sys/sysmacros.h>
#include <sys/utsname.h>
#include <sys/mount.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <linux/un.h>
#include <poll.h>
#include <assert.h>
#include <linux/if.h>
#include <linux/types.h>
#include <linux/wireless.h>

#include <iostream>
#include "netsdk.h"
#include "post.h"
#include "base64.h"

using namespace std;

FILE * g_pFile;
int  NetDataCallBack( long lRealHandle, long dwDataType, unsigned char *pBuffer,long lbufsize,long dwUser)
{	
	static int allsize = 0;
	allsize += lbufsize;
	
	printf("%d [%s] lbz:%ld\n",allsize, (char*)dwUser, lbufsize);
	
	BOOL bResult = TRUE;
	try
	{	
		//fwrite(pBuffer,1,lbufsize,g_pFile);
	}
	catch (...)
	{
		
	}

	// it must return TRUE if decode successfully,or the SDK will consider the decode is failed
	return bResult;

}
int  RealDataCallBack_V2(long lRealHandle, const PACKET_INFO_EX *pFrame, long dwUser)
{
	printf("time:%04d-%02d-%02d %02d:%02d:%02d\n",pFrame->nYear,pFrame->nMonth,pFrame->nDay,pFrame->nHour,pFrame->nMinute,pFrame->nSecond);
	BOOL bResult = TRUE;
	try
	{	
		//fwrite(pFrame->pPacketBuffer,1,pFrame->dwPacketSize,g_pFile);
	}
	catch (...)
	{

	}

	// it must return TRUE if decode successfully,or the SDK will consider the decode is failed
	return bResult;
}
void  TalkDataCallBack(LONG lTalkHandle, char *pDataBuf, long dwBufSize, char byAudioFlag, long dwUser)
{
	BOOL bResult = TRUE;
	try
	{
		printf("come TalkDataCallBack :%ld\n",dwBufSize);

		if(g_pFile)
		{
			//printf("come in fwrite");
			fwrite(pDataBuf,1,dwBufSize,g_pFile);
		}
		else
		{
			printf("###################################fWrite wrong!!!!!\n");
		}
		
	}
	catch (...)
	{
		
	}
	// it must return TRUE if decode successfully,or the SDK will consider the decode is failed
	return ;
}

long g_LoginID=0;
bool DevicCallBack(long lLoginID, char *pBuf,
				   unsigned long dwBufLen, long dwUser)
{
	printf("one devic is comeing!");
		//H264_DVR_ACTIVEREG_INFO *info=(H264_DVR_ACTIVEREG_INFO *)pBuf;
		//printf("das device : %s",info->deviceSarialID);
		g_LoginID=lLoginID;
}
#define POST_TASK
//#define SearchDevice
//#define  DAS
//#define Talk
//#define RealPlay
//#define Config
#define DOWNLD_BYNAME
//#define PLAYBACK_BYNAME
//#define PLAYBACK_BYNAME_V2
//#define PlayBack_BYTIME
//#define PlayBack_BYTIME_V2
void* START_ROUTINE(LPVOID lpThreadParameter)
{
	
	H264_DVR_CLIENTINFO playstru;

	playstru.nChannel = 0;
	playstru.nStream = 0;
	playstru.nMode = 0;
	long nPlayHandle = H264_DVR_RealPlay(g_LoginID,&playstru );
	printf("nPlayHandle=%ld\n",nPlayHandle);
	sleep(1000);
	//return 1;		
}
pthread_t id;
void* SearchDeviceThread(void *arr)
{
	SDK_CONFIG_NET_COMMON_V2 m_Device[100];
	int nRetLength = 0;
	cout<<"start"<<endl;
	bool bRet= H264_DVR_SearchDevice((char*)m_Device,sizeof(SDK_CONFIG_NET_COMMON_V2)*100,&nRetLength,5000);
	printf("bret = %d\r\n", bRet);
	if(bRet)
	{		
		cout<<"m_Device->HostIP"<<m_Device[0].HostName<<endl;
		printf("H264_DVR_SearchDevice ok number is [%d]\n",nRetLength/sizeof(SDK_CONFIG_NET_COMMON_V2));
		pthread_join(id, NULL);
	}
}
static int s_flag = 0;
void pfNetComFun(SDK_CONFIG_NET_COMMON_V2 *pNetCom, unsigned long userData)
{
	printf("data============[%d]\n",s_flag);
}

static int pt_pipe[2];
static int dl_end_flag = 0;
static char cur_post_file[256] = {0};
static int cur_file_size = 0;
static int cur_dl_size = 0;
static int cur_post_size = 0;
static int cur_dl_per = 0;
static int cur_post_pos = 0;
static int glb_fd = 0;
static int post_data_flag  = 0;

int _fDownLoadDataCallBack(long lRealHandle, long dwDataType, unsigned char *pBuffer, long lbufsize,long dwUser)
{
	//H264_DVR_FILE_DATA *hfd = (H264_DVR_FILE_DATA*)dwUser;
	
	cur_dl_size += lbufsize;
	putchar('.');
	if((cur_dl_size%102400) == 0)
		putchar('\n');
	if(cur_dl_size > cur_post_pos)
		write(pt_pipe[1],pBuffer,lbufsize);
	
	if(cur_file_size*1024 <= cur_dl_size) {
		printf("\n--dataend per %d%%, file %ldKB, dl-all %d\n", cur_dl_per, cur_file_size, cur_dl_size);\
		syslog(LOG_INFO, "\n--dataend per %d%%, file %ldKB, dl-all %d\n", cur_dl_per, cur_file_size, cur_dl_size);
		dl_end_flag = 1;
	}
	return TRUE;
}

int check_assoc(char *ifname)
{
	int socket_id, i;
	struct iwreq wrq;

	socket_id = socket(AF_INET, SOCK_DGRAM, 0);
	strcpy(wrq.ifr_ifrn.ifrn_name, ifname);
	ioctl(socket_id, SIOCGIWAP, &wrq);
	close(socket_id);

	for (i = 0; i < 6; i++)
		if(wrq.u.ap_addr.sa_data[i])
			return 1;
	return 0;
}


void* _post_handle(void *arg)
{
	printf("in post thread hd...\n");
	syslog(LOG_INFO, "in post thread hd...\n");
	
	size_t stack_size = 0;
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	int ret = pthread_attr_getstacksize(&attr, &stack_size);  
	printf("stack_size = %dB, %dk\n", stack_size, stack_size/1024);  
	
	int rdlen = 0;
	unsigned char data[65536] = {0};
	
	while((rdlen=read(pt_pipe[0], data, sizeof(data))) > 0) {
		//printf("rd %d\n", rdlen);
	_again:
		if(check_assoc("apcli0") == 0) {
			syslog(LOG_INFO,"post waiting wifi up ok...\n");
			sleep(3);
			goto _again;
		}
		ret = post_content(glb_fd, 1, cur_post_file, cur_post_pos, data, rdlen);
		if(ret > 0) {
			cur_post_pos = ret;
			cur_post_size += rdlen;
		} else if(ret == SOCK_ERR) {
		_reconn:
			close(glb_fd);
			sleep(1);
			printf("net connect try again...\n");
			glb_fd = post_start(1,cur_post_file,&cur_post_pos);
			if(glb_fd < 0) {
				if(glb_fd == SOCK_ERR)
					goto _reconn;
			} else {
				printf("continue to post last data...\n");
				goto _again;
			}
		} else {
			printf("sys error...\n");
			sleep(1);
			goto _again;
		}
		//continue to read pipe
	}
	printf("out post thread hd...\n");
	return NULL;
}

void _resp_mcu_heart()
{
	system("echo timer > /sys/class/leds/heart/trigger");
	system("echo 700 > /sys/class/leds/heart/delay_on");
	system("echo 300 > /sys/class/leds/heart/delay_off");
}

void _fDisConnect(long lLoginID, char *pchDVRIP, long nDVRPort, unsigned long dwUser)
{
	printf("---%d---netsdk disconnect------\n",lLoginID);
	syslog(LOG_INFO, "---%d---netsdk disconnect------\n",lLoginID);
	_resp_mcu_heart();
	exit(1);
}

void _sig_handle(int sig)
{
	syslog(LOG_INFO, "main exit by sig %d\n", sig);
	_resp_mcu_heart();
	exit(sig);
}

int  main(int argc, char **argv)
{
	int ret = 0;
	int connok = 0;
	if(argc < 5) {
		printf("$s devid ipaddr port -d\n",argv[0]);
		return -1;
	}
	
	g_pFile=NULL;
	signal(SIGPIPE, SIG_IGN);
	signal(SIGINT, _sig_handle);
	signal(SIGTERM, _sig_handle);

	openlog("ipcnet", LOG_CONS | LOG_PID, 0);
	//1-devid, 2-ipaddr, 3-port, 4: -d/-f
	syslog(LOG_INFO, "start %s %s %s %s\n",argv[0],argv[1],argv[2],argv[3]);

// != -c
if(strcmp("-c",argv[4])) {
	while(connok < 60) {
		if(check_assoc("apcli0"))
			connok++;
		else {
			connok = 0;
			syslog(LOG_INFO, "wifi up is disconnected");
		}
		sleep(2);
	}
}
	if(strcmp("-d",argv[4]) == 0) {
		daemon(0,0);
		setbuf(stdout, NULL);
	}

	H264_DVR_Init(_fDisConnect,NULL);
	printf("H264_DVR_Init\n");
	time_t curtime;
    time(&curtime);
	syslog(LOG_INFO, "==apcli0 OK. H264_DVR_Init at systime %s\n", ctime(&curtime));

#ifdef SearchDevice
	int ret = pthread_create(&id, NULL,SearchDeviceThread, NULL);
	if(ret)
	{
		printf("Create search pthread error\n");
	}
#endif
#ifdef SearchDeviceV2
		H264_DVR_SearchDevice_V2(pfNetComFun,0, 5000);
#endif

#ifdef DAS
	cout<<"start das"<<endl;
	bool ret =H264_DVR_StartActiveRigister(9400,DevicCallBack,1);
	cout<<"end das"<<endl;
	if(ret>0)
	{
		cout<<"********sleep 2 minit************"<<endl;
		sleep(120);
	}	
	else
	{
		printf("Call H264_DVR_StartActiveRigister**********wrong!");
	}
#else

	connok = 0;
_login:
	H264_DVR_DEVICEINFO OutDev;	
	memset(&OutDev,0,sizeof(OutDev));
	int nError = 0;			
	g_LoginID = H264_DVR_Login((char*)argv[2], atoi(argv[3]), (char*)"admin",(char*)"",(LPH264_DVR_DEVICEINFO)(&OutDev),&nError);	
	printf("g_LoginID=%d,nError:%d\n",g_LoginID,nError);
	syslog(LOG_INFO, "g_LoginID=%d,nError:%d\n",g_LoginID,nError);
	if(g_LoginID <= 0 || nError != 0) {
		syslog(LOG_INFO, "try login ipc again %d...\n", connok);
		sleep(1);
		if(connok++ < 2)
			goto _login; //will Segmentation fault ???
		else
		{
			syslog(LOG_INFO, "---login failed! to exit...\n");
			exit(1);
		}
	}

if(strcmp("-c",argv[4])) {
	
#ifdef POST_TASK
	if(pipe(pt_pipe) == -1) {
		perror("pipe");
	}
	pthread_t ptid;
	if(pthread_create(&ptid, NULL, _post_handle, NULL))
	{
		H264_DVR_Logout(g_LoginID);
		printf("Create post pthread error\n");
		syslog(LOG_INFO, "Create post pthread error\n");
		exit(1);
	}
	pthread_detach(ptid);
#endif

}
	
#endif //DAS
	
	tm *st = NULL;
	time_t last_tt = time(NULL);
	connok = 0;
_getagain:
	ret = post_last_time(1, argv[1]);
	if(ret <= 0) {
		syslog(LOG_INFO, "--dev %s post get last time error! %d\n", argv[1],ret);
		sleep(3);
		if(connok++ < 3)
			goto _getagain;

		st = localtime(&last_tt);
		//st->tm_mon--; //last month
		st->tm_hour = 0;
		st->tm_min = 0;
		st->tm_sec = 0;
	} else {
		last_tt = ret;
		st = localtime(&last_tt);
	}

	H264_DVR_FINDINFO findInfo;
	findInfo.nChannelN0=0;
	findInfo.nFileType = SDK_RECORD_ALARM; //文件类型, 见 SDK_File_Type, 0-all
	
	findInfo.startTime.dwYear = st->tm_year+1900;
	findInfo.startTime.dwMonth = st->tm_mon+1; 
	findInfo.startTime.dwDay = st->tm_mday;
	findInfo.startTime.dwHour = st->tm_hour;
	findInfo.startTime.dwMinute = st->tm_min;
	findInfo.startTime.dwSecond = st->tm_sec;

	time_t t = time(NULL);
	tm *now= localtime(&t);
	findInfo.endTime.dwYear = now->tm_year+1900;
	findInfo.endTime.dwMonth = now->tm_mon+1;
	findInfo.endTime.dwDay = now->tm_mday;
	findInfo.endTime.dwHour = now->tm_hour;
	findInfo.endTime.dwMinute = now->tm_min;
	findInfo.endTime.dwSecond = now->tm_sec;

	H264_DVR_FILE_DATA *pData = new H264_DVR_FILE_DATA[128];
	int nFindCount = 0;
	int i = 0;
	char cmd[512] = {0};
	int per = 0;
	int less_flag = 0;
	printf("to found from %04d-%02d-%02d %02d-%02d-%02d to %04d-%02d-%02d %02d-%02d-%02d\n",
		findInfo.startTime.dwYear,findInfo.startTime.dwMonth,findInfo.startTime.dwDay,
		findInfo.startTime.dwHour,findInfo.startTime.dwMinute,findInfo.startTime.dwSecond,
		findInfo.endTime.dwYear,findInfo.endTime.dwMonth,findInfo.endTime.dwDay,
		findInfo.endTime.dwHour,findInfo.endTime.dwMinute,findInfo.endTime.dwSecond);

	syslog(LOG_INFO, "to found from %04d-%02d-%02d %02d-%02d-%02d to %04d-%02d-%02d %02d-%02d-%02d\n",
		findInfo.startTime.dwYear,findInfo.startTime.dwMonth,findInfo.startTime.dwDay,
		findInfo.startTime.dwHour,findInfo.startTime.dwMinute,findInfo.startTime.dwSecond,
		findInfo.endTime.dwYear,findInfo.endTime.dwMonth,findInfo.endTime.dwDay,
		findInfo.endTime.dwHour,findInfo.endTime.dwMinute,findInfo.endTime.dwSecond);
		
	long lRet= H264_DVR_FindFile(g_LoginID, &findInfo, pData, 128, &nFindCount, 5000);
	if(lRet>0&&nFindCount>0)
	{
	   	printf("search success,playback file num=%d\n", nFindCount);
		syslog(LOG_INFO, "search success,playback file num=%d\n", nFindCount);
		
		if(strcmp("-c",argv[4]) == 0) {
			//xmipcnet devid ip port -c
			//for web cmd to check record file
			for(i=0; i < nFindCount; i++) {
				printf("%s_%04d%02d%02d%02d%02d%02d_%04d%02d%02d%02d%02d%02d_%02d.h264\n",argv[1],
				pData[i].stBeginTime.year, pData[i].stBeginTime.month, pData[i].stBeginTime.day,
				pData[i].stBeginTime.hour, pData[i].stBeginTime.minute, pData[i].stBeginTime.second,
				pData[i].stEndTime.year, pData[i].stEndTime.month, pData[i].stEndTime.day,
				pData[i].stEndTime.hour, pData[i].stEndTime.minute, pData[i].stEndTime.second, pData[i].ch);
			}
			H264_DVR_Logout(g_LoginID);
			H264_DVR_Cleanup();
			exit(1);
		}
	_next:
			per = 0;
			cur_dl_size = 0;
			cur_post_size = 0;
			sprintf(cur_post_file,"%s_%04d%02d%02d%02d%02d%02d_%04d%02d%02d%02d%02d%02d_%02d.h264",argv[1],
				pData[i].stBeginTime.year, pData[i].stBeginTime.month, pData[i].stBeginTime.day,
				pData[i].stBeginTime.hour, pData[i].stBeginTime.minute, pData[i].stBeginTime.second,
				pData[i].stEndTime.year, pData[i].stEndTime.month, pData[i].stEndTime.day,
				pData[i].stEndTime.hour, pData[i].stEndTime.minute, pData[i].stEndTime.second, pData[i].ch);
		
			cur_file_size = pData[i].size;
		//}
		sprintf(cmd,"/etc/%s",cur_post_file);
		if(access(cmd,F_OK) == 0) {
			printf("file [%d]%s had post OK!\n",cur_file_size*1024,cmd);
			if(++i <= (nFindCount-1))
				goto _next;
			else
				goto _other;
		}
#ifdef DOWNLD_BYNAME
		lRet = H264_DVR_GetFileByName(g_LoginID, &pData[i], 0, 0, (long)(&pData[i]), _fDownLoadDataCallBack);
		if(lRet >= 0) {
			printf("to post file [%dKB][%s]%s\n", cur_file_size, cur_post_file, pData[i].sFileName);
			syslog(LOG_INFO, "to post file [%dKB][%s]%s\n", cur_file_size, cur_post_file, pData[i].sFileName);
		_start:
			glb_fd = post_start(1,cur_post_file,&cur_post_pos);
			if(glb_fd < 0) {
				printf("network error!\n");
				if(glb_fd == SOCK_ERR)
					close(glb_fd);
				sleep(1);
				goto _start;
			}
			
			while(!dl_end_flag) {
				per = H264_DVR_GetDownloadPos(lRet);
				cur_dl_per = per;
				printf("\n===dl %d%%===\n", per);
				//syslog(LOG_INFO, "=====dl %d%%=====\n", per);
				if( (per == 100) && (less_flag++ > 3) ) {
					//if send currently recording file ,file size is increasing , read dl size < file size;
					printf("get per 100, but real dl size %d < file size %d\n",cur_dl_size,cur_file_size*1024);
					syslog(LOG_INFO, "get per 100, but real dl size %d < file size %d\n",cur_dl_size,cur_file_size*1024);
					break;
				}
				sleep(3);
			}
			less_flag = 0;
			printf("%d%%file [file %d,dl %d,post %d][%s] dl and post OK\n",per,cur_file_size*1024,cur_dl_size,cur_post_size,cur_post_file);
			syslog(LOG_INFO, "%d%%file [file %d,dl %d,post %d][%s] dl and post OK\n",per,cur_file_size*1024,cur_dl_size,cur_post_size,cur_post_file);
			dl_end_flag = 0;
			cur_dl_per = 0;

			//H264_DVR_StopGetFile(lRet);
			
			//wait post_content thread send over.
			sleep(5);
			post_finish(glb_fd, 1, cur_post_file, cur_post_size);
			cur_dl_size = 0;
			cur_post_size = 0;
			close(glb_fd);
			sprintf(cmd,"echo %d > /etc/%s",cur_file_size*1024,cur_post_file);
			system(cmd);
			sync();
			if(++i > (nFindCount-1))
				printf("dl and post all files %d! to stop dl.\n", nFindCount);
			else {
				printf("continue to post new file\n");
				goto _next;
			}
		} else {
			printf("H264_DVR_GetFileByName ret %d dl file error %d!\n",lRet, H264_DVR_GetLastError());
			syslog(LOG_INFO, "H264_DVR_GetFileByName ret %d dl file error %d!\n",lRet, H264_DVR_GetLastError());
		}
#endif
	_other:
#ifdef PLAYBACK_BYNAME
	 	lRet = H264_DVR_PlayBackByName(g_LoginID, &pData[0], NULL, NetDataCallBack, (long)cur_post_file);
		if(lRet>0)
		{
			sleep(300);
			printf("Play success\n");
		}else
		{
			printf("Play failed,lRet=%ld\n",lRet);
		}
#else

#ifdef PLAYBACK_BYNAME_V2
		lRet = H264_DVR_PlayBackByName_V2(g_LoginID, &pData[0], NULL,RealDataCallBack_V2, NULL);
		if(lRet>0)
		{
			sleep(10);
			printf("Play success\n");
		}else
		{
			printf("Play failed,lRet=%ld\n",lRet);
		}
#endif
#endif


	//»Ø·Å
		H264_DVR_FINDINFO info;
		memset(&info, 0, sizeof(info));
		info.nChannelN0=0;
		info.nFileType=0;
		info.startTime.dwYear = st->tm_year+1900;
		info.startTime.dwMonth = st->tm_mon+1;
		info.startTime.dwDay = st->tm_mday;
		info.startTime.dwHour = 0;
		info.startTime.dwMinute = 0;
		info.startTime.dwSecond = 0;
		
		info.endTime.dwYear = now->tm_year+1900;
		info.endTime.dwMonth = now->tm_mon+1;
		info.endTime.dwDay = now->tm_mday;
		info.endTime.dwHour = 23;
		info.endTime.dwMinute = 59;
		info.endTime.dwSecond = 59;
		g_pFile = fopen("testPlayBack", "wb+");
		long ret=0;
				 
#ifdef PlayBack_BYTIME
				 	ret=H264_DVR_PlayBackByTime(g_LoginID,&info,NULL,NetDataCallBack,1);
					if(ret)
				 	{
				 		printf("######playBackByTime#########\n");
				 		sleep(10);	

				 	}
				 	else
				 	{
				 		printf("#############playbackWrong#####:%ld\n",ret);
				 	}
#else
#ifdef PlayBack_BYTIME_V2
					ret=H264_DVR_PlayBackByTime_V2(g_LoginID,&info,RealDataCallBack_V2,1,NULL,NULL);
					if(ret)
				 	{
				 		printf("######playBackByTime#########\n");
				 		sleep(10);	

				 	}
				 	else
				 	{
				 		printf("#############playbackWrong#####:%ld\n",ret);
				 	}
	
				 	
#endif
#endif	
	
	} else {
		printf("do NOT found any record file %d\n",nFindCount);
		syslog(LOG_INFO, "do NOT found any record file %d\n",nFindCount);
	}
	
	if(strcmp("-c",argv[4])) {
		printf("---resp mcu ,heart 300H-700L ms\n");
		syslog(LOG_INFO, "---resp mcu ,heart 300H-700L ms\n");
		_resp_mcu_heart();
	}
	
 	if(g_LoginID>0)
 	{
				//¶Ôœ²
#ifdef Talk
 		g_pFile = fopen("TestTalk", "wb+");			
 		long lHandle = H264_DVR_StartVoiceCom_MR(g_LoginID, TalkDataCallBack, 0);
 		if ( lHandle <= 0 )
 		{
 			printf("start talk wrong");
 		}
 		else
 		{						
 			printf("start talk ok\n");			
 			sleep(6);
 			if(H264_DVR_StopVoiceCom(lHandle))
 			{
 				printf("stop talk ok\n");
 			}
 			else
 			{
 				printf("stop wrong!");
 			}			
 		}
#endif
		//ÊµÊ±ŒàÊÓ


#ifdef RealPlay
		H264_DVR_CLIENTINFO playstru;

		playstru.nChannel = 0;
		playstru.nStream = 0;
		playstru.nMode = 0;
		long m_iPlayhandle = H264_DVR_RealPlay(g_LoginID, &playstru);	
		if(m_iPlayhandle == 0 )
		{
			printf("start RealPlay wrong!\n");
		}
		else
		{
			g_pFile = fopen("TestRealPlay.h264", "wb+");
			H264_DVR_SetRealDataCallBack_V2(m_iPlayhandle, RealDataCallBack_V2, 0);
			printf("start RealPlay ok!");
			sleep(10);
			if(H264_DVR_StopRealPlay(m_iPlayhandle))
			{
				printf("stop realPlay ok\n");
			}
			else
			{
				printf("stop realPlay Wrong\n");
			}

		}
#endif
		//ÍøÂçÅäÖÃ
#ifdef Config
 		DWORD dwRetLen = 0;
 		int nWaitTime = 10000;
 		SDK_CONFIG_NET_COMMON NetWorkCfg;
 		BOOL bReboot = FALSE;
 		BOOL bSuccess = H264_DVR_GetDevConfig(g_LoginID,E_SDK_CONFIG_SYSNET,0,
 			(char *)&NetWorkCfg,sizeof(SDK_CONFIG_NET_COMMON),&dwRetLen,nWaitTime);
 		if (bSuccess && dwRetLen == sizeof (SDK_CONFIG_NET_COMMON))
 		{
 			printf("TCPPort:%d\n",NetWorkCfg.SSLPort);			
 			NetWorkCfg.SSLPort=34567;
 			bSuccess = H264_DVR_SetDevConfig(g_LoginID,E_SDK_CONFIG_SYSNET,0,
 				(char *)&NetWorkCfg,sizeof(SDK_CONFIG_NET_COMMON),nWaitTime);
 			if (bSuccess)
 			{
 				printf("setconfig ok\n");
 			}
 			else
 			{
 				printf("setconfig wrong\n");
 			}
 
 		}
 		else
 		{
 			int len=sizeof (SDK_CONFIG_NET_COMMON);
 			printf("GetConfig Wrong:%d,RetLen:%ld  !=  %d\n",bSuccess,dwRetLen,len);
 		}
	//ÆÕÍšÅäÖÃ
 		dwRetLen = 0;
 		nWaitTime = 10000;
 		SDK_CONFIG_NORMAL NormalConfig = {0};
 		bSuccess = H264_DVR_GetDevConfig(g_LoginID, E_SDK_CONFIG_SYSNORMAL ,0, 
 			(char *)&NormalConfig ,sizeof(SDK_CONFIG_NORMAL), &dwRetLen,nWaitTime);
 		if ( bSuccess && dwRetLen == sizeof(SDK_CONFIG_NORMAL))
 		{
 			printf("############language: %d#############\n",NormalConfig.iLanguage);
 			NormalConfig.iLanguage=2;
 			BOOL bSuccess = H264_DVR_SetDevConfig(g_LoginID,E_SDK_CONFIG_SYSNORMAL,0,(char *)&NormalConfig,sizeof(SDK_CONFIG_NORMAL),nWaitTime);
 			if ( bSuccess == H264_DVR_OPT_REBOOT )
 			{
 				printf("####need reboot####\n");
 			}
 			else if(bSuccess>0)
 			{
 				printf("#####setconfig ok####\n");
 			}
 			else
 			{
 				printf("#####setconfig wrong####\n");
 			}
 		}

#endif

 	}
 	else
 	{
 		printf("**************login wrong************\n");	
 	}

#ifdef DAS
	cout<<"H264_DVR_StopActiveRigister1"<<endl;
	H264_DVR_StopActiveRigister();
	cout<<"H264_DVR_StopActiveRigister"<<endl;
#endif

	if(g_LoginID>0)
	{
		H264_DVR_Logout(g_LoginID);
		printf("Logout success!!!\n");
		syslog(LOG_INFO, "Logout success!!!\n");
	}
	sleep(1);
	H264_DVR_Cleanup();
	printf("*******H264_DVR_Cleanup*******\n");
	syslog(LOG_INFO, "*******H264_DVR_Cleanup*******\n");

	if(g_pFile)
	{
		fclose(g_pFile);
	}
	printf("**************OVER************\n");
	return 0;
}
