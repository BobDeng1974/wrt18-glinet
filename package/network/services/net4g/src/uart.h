#ifndef UART_H
#define UART_H

#define UART_BUF_FILE	"/tmp/tty_buf"

typedef struct _st_uart_attr {
	int valid;
	int fd;
	int baud;
	int databit;
	int stopbit;
	int parity;
	int flowct;
	int workmode;
	int interval;
}ST_UART;

int init_com_dev(const char *com_dev,int none_block,ST_UART *uart_attr);

#endif

