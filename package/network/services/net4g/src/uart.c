#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/select.h>
#include <linux/fs.h>
#include <errno.h>
#include <termios.h>
#include <fcntl.h>
#include <assert.h>
#include <sys/time.h>
#include <time.h>

#include "uart.h"
#include "debug.h"

/* open COM dev
 *  115200 8N1
 *  NONBLOCK
 */
int setport(int fd, int baud, int databits, int stopbits, int parity)
{
	int    baudrate;
	struct termios newtio;   
	
	switch (baud)
	{
		case 300:
			baudrate = B300;
			break;
		case 600:
			baudrate = B600;
			break;
		case 1200:
			baudrate = B1200;
			break;
		case 2400:
			baudrate = B2400;
			break;
		case 4800:
			baudrate = B4800;
			break;
		case 9600:
			baudrate = B9600;
			break;
		case 19200:
			baudrate = B19200;
			break;
		case 38400:
			baudrate = B38400;
			break;
		case 57600:
			baudrate = B57600;
			break;
		case 115200:
			baudrate = B115200;
			break;
		default :
			return -1;  
			break;
	}
	
	tcgetattr(fd, &newtio);     
	bzero(&newtio, sizeof(newtio));    
	
	//must be sed firstly!
	newtio.c_cflag |= (CLOCAL | CREAD);
	newtio.c_cflag &= ~CSIZE; 
	
	switch (databits)
	{   
		case 7:  
			newtio.c_cflag |= CS7; 
			break;
		case 8:     
			newtio.c_cflag |= CS8; 
			break;   
		default:    
			return -1;
			break;    
	}
	
	switch (parity) 
	{   
		case 0:
		case 'n':
		case 'N':    
			newtio.c_cflag &= ~PARENB;   
			newtio.c_iflag &= ~INPCK;    
			break;
		case 1:
		case 'o':   
		case 'O':     
			newtio.c_cflag |= (PARODD | PARENB); 
			newtio.c_iflag |= INPCK;            
			break;
		case 2:
		case 'e':  
		case 'E':   
			newtio.c_cflag |= PARENB;     
			newtio.c_cflag &= ~PARODD;      
			newtio.c_iflag |= INPCK;       
			break;
		case 'S': 
		case 's':  
			newtio.c_cflag &= ~PARENB;
			newtio.c_cflag &= ~CSTOPB;
			break;  
		default:   
			return -1;    
			break;   
	} 
	
	switch (stopbits)
	{   
		case 1:    
			newtio.c_cflag &= ~CSTOPB;  
			break;  
		case 2:    
			newtio.c_cflag |= CSTOPB;  
			break;
		default:  
			return -1;
			break;  
	} 
	
	
	
	newtio.c_cc[VTIME] = 0;    
	newtio.c_cc[VMIN] = 1; 
#if 1
	newtio.c_oflag &= ~OPOST;
    newtio.c_oflag &= ~(ONLCR | OCRNL);
    newtio.c_iflag &= ~(ICRNL | INLCR);
    newtio.c_iflag &= ~(IXON | IXOFF | IXANY);
#endif
	newtio.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG); //raw mode
	cfsetispeed(&newtio, baudrate);   
	cfsetospeed(&newtio, baudrate);   
	tcflush(fd, TCIFLUSH); 
	
	if (tcsetattr(fd,TCSANOW, &newtio) != 0)   
	{ 
		ERROR("tcsetattr-error ,%d-%s\n",errno,strerror(errno));
		return -1;  
	}  
	
	return 0;
}

/*
 * open Com Dev
 * default 8 databits  1 stopbit
 * return 0 ok,otherwise failed
 */
int init_com_dev(const char *com_dev,int none_block,ST_UART *uart_attr)
{
	int fd = -1;
	
	//	const char *com_dev = "/dev/ttySAC2";
	if(none_block)
		fd = open(com_dev, O_RDWR|O_EXCL|O_NOCTTY|O_NONBLOCK);
	else
		fd = open(com_dev, O_RDWR|O_EXCL|O_NOCTTY);
	if (fd < 0) {
		ERROR("Open Comdev '%s' Error!\n",com_dev);
		return -1;
	}
	
	if(setport(fd,uart_attr->baud,uart_attr->databit,uart_attr->stopbit,uart_attr->parity) != 0) {
		ERROR("setport error!\n");
		return  -1;
	}
	uart_attr->fd = fd;
	return fd;
}

int get_one_package(int fd, ST_UART *uart)
{
	int ret=0, tty_len=0;
	unsigned int file_len=0;
	int count = 0;

    fd_set readfds;
    struct timeval tv;
    tv.tv_sec = uart->interval/1000;
    tv.tv_usec = 300000;
	
	FILE *fp = fopen(UART_BUF_FILE,"a+");
	if(!fp){
		ERROR("Create uart buf file error!\n");
		return -1;
	}
	struct timeval now_tv;
	struct timeval last_tv;
	char buffer[1024] = {0};
	char sendbuf[1200] = {0};
	
	while(1) {
		gettimeofday(&now_tv,NULL);
		
		if(now_tv.tv_sec - last_tv.tv_sec >= 30 && uart->workmode == 0) {
			memset(sendbuf,0,sizeof(sendbuf));
			sprintf(sendbuf,"{\"type\":\"tc232\",\"seqnum\":\"%d\",\"body\":{\"msg\":\"%s\"}}",
			count++,buffer);
			
		}
		FD_ZERO(&readfds);
		FD_SET(fd,&readfds);
		ret = select(fd+1,&readfds,NULL,NULL,&tv);
		if(ret == 0) {
			continue;
		}
		else if(ret < 0) {
			if(errno == EINTR) {
				NOTE("###### uart select interrupt by signal #####\n");
				continue;
			} else
				ERROR("uart select error,%d:%s\n",errno,strerror(errno));
 			return -1;  // error or closed
		}
		//run here! must be OK!
		memset(buffer,0,sizeof(buffer));
		tty_len = read(fd,buffer,sizeof(buffer)-1);
		ret = fwrite(buffer,1,tty_len,fp);
		if(ret != tty_len) {
			WARN("uart file write len %d < actual len %d!\n",ret,tty_len);
		}

		if(file_len > 1024*1024)
			rewind(fp);
		else
			file_len += ret;
	}
	
	fflush(fp);
	fclose(fp);
	return 0;
}
