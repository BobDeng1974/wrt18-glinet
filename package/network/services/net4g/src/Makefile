#build uart_zigbee executable when user executes "make"

CLI_UART = cli_uart

CLIENT 		 = net4g 
CLIENT_OBJ 	 = main2.o cjson.o net_json.o net_base.o config.o hash_nvram.o crc32.o ota.o image_head.o uart.o

UCITST	= uci_test
UCI_OBJ	= uci_test.o

SERVER 		 = websocket-server
SERVER_OBJ 	 = websocket-server.o cjson.o net_json.o  net_base.o net_tb_api.o utils.o

UART_CFLAGS       = $(CFLAGS) -g -Wall -std=gnu99
UART_LDFLAGS      = $(LDFLAGS) -lm -lpthread

all: clean  $(CLIENT)

$(SERVER):$(SERVER_OBJ)
	$(CC) -o $@ $^ $(UART_LDFLAGS)

$(UCITST):$(UCI_OBJ)
	$(CC) -o $@ $^ -luci
	
$(CLIENT):$(CLIENT_OBJ)
	$(CC) -o $@ $^ $(UART_LDFLAGS) -llua -lubox

$(CLI_UART):cli_uart.o
	$(CC) -o $@ $^ -lpthread
	
%.o : %.c
	$(CC) -c $< $(UART_CFLAGS) -o $@

# remove object files and executable when user executes "make clean"
clean:
	rm -rf *.[oa]  $(CLIENT) $(UCITST) $(SERVER) *.~ *~





