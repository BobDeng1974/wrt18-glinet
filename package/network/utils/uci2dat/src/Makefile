#
CLIENT 		 = ap_client 
CLIENT_OBJ 	 = ap_client.o

UART_CFLAGS       = $(CFLAGS) -g -Wall -std=gnu99
UART_LDFLAGS      = $(LDFLAGS)

all: clean  $(CLIENT)
	
$(CLIENT):$(CLIENT_OBJ)
	$(CC) -o $@ $^ $(UART_LDFLAGS)

%.o : %.c
	$(CC) -c $< $(UART_CFLAGS) -o $@

# remove object files and executable when user executes "make clean"
clean:
	rm -rf *.[oa]  $(CLIENT)  *.~ *~
