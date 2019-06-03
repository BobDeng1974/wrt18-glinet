
EXEC=xmipcnet

#CXX:=/opt/openwrt-sdk-ramips-mt76x8_gcc-7.3.0_musl.Linux-x86_64/staging_dir/toolchain-mipsel_24kc_gcc-7.3.0_musl/bin/mipsel-openwrt-linux-g++
#CC:=/opt/openwrt-sdk-ramips-mt76x8_gcc-7.3.0_musl.Linux-x86_64/staging_dir/toolchain-mipsel_24kc_gcc-7.3.0_musl/bin/mipsel-openwrt-linux-gcc

all: clean $(EXEC)

$(EXEC): base64.o post.o test.o 
	$(CXX) -o $@ $^ -lpthread -lxmnetsdk -L./

pctest: pc.o base64.o  post.o
	$(CC) -o $@ $^

test.o:test.cpp
	$(CXX) -c $^

pc.o:pc.c
	$(CC) -c $^

post.o:post.c
	$(CC) -c $^

base64.o:base64.c
	$(CC) -c $^

clean:
	rm -rf *.o $(EXEC) pctest
