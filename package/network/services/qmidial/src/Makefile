
CLIENT 		 = qmidial 
CLIENT_OBJ 	 = QmiWwanCM.o GobiNetCM.o main.o MPQMUX.o QMIThread.o util.o udhcpc.o

UART_CFLAGS       = $(CFLAGS) -Wall -s -std=gnu99
UART_LDFLAGS      = $(LDFLAGS) -ldl -lpthread

all: clean  $(CLIENT)
	
$(CLIENT):$(CLIENT_OBJ)
	$(CC) -o $@ $^ $(UART_LDFLAGS)

%.o : %.c
	$(CC) -c $< $(UART_CFLAGS) -o $@

# remove object files and executable when user executes "make clean"
clean:
	rm -rf *.[oa]  $(CLIENT)  *.~ *~





