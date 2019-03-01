#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <uci.h>

#define UCI_REMOTE_CONFIG "/etc/config/netset"
#define UCI_PPP_CONFIG	  "/etc/config/network"


void write_uci_option(char *section, char *option_path, char *value)
{
	char new_val[128] = {0};
	struct uci_context *set_ctx = uci_alloc_context();
	struct uci_ptr ptr;
	
	if (uci_lookup_ptr(set_ctx, &ptr, option_path, true) != UCI_OK) {
		goto cleanup;
	}
	if(ptr.value) {
		printf("write %s\n",ptr.value);
	}
	
	uci_set(set_ctx,&ptr);
	uci_commit(set_ctx, &ptr.p, false);

cleanup:
	uci_free_context(set_ctx);
}

static int update_uci_remote_config(char *devid,char *ipaddr,char *port)
{
	char cmd[1024];
	sprintf(cmd,"uci set netset.@netset[0].deviceid=%s && \
		uci set netset.@netset[0].remote_ip=%s && \
		uci set netset.@netset[0].remote_port=%s && uci commit",
		devid,ipaddr,port);
	
	system(cmd);
}

static int update_uci_ppp_config(char *dialtype,char *apn,char *user,char *pwd,char *dialnum)
{
	char cmd[1024];
	sprintf(cmd,"uci set network.ppp.private_dial=%s && \
		uci set network.ppp.apn=%s && \
		uci set network.ppp.username=%s && \
		uci set network.ppp.password && \
		uci set network.ppp.dialnumber && \
		uci commit",
		dialtype,apn,user,pwd,dialnum);
	
	system(cmd);
	
}

int main(int argc, char **argv)
{
	update_uci_ppp_config("0","3GNET","card","","#777");
	update_uci_remote_config("81699","43.252.231.89","23059");
	return 0;
}
