#ifndef CONFIG_H
#define CONFIG_H
#include "hash_nvram.h"

#define HEAD	   "#Begin Default;len<128;item<64"

typedef struct config_st {
	nvram_handle_t *hash_tb;
} CONFIG_ST;

CONFIG_ST* config_init();
void config_close(CONFIG_ST *cfg_st);
int config_save(const char *path);
int nvram_renew(char *fname);

const char* nvram_get(const char *name);
int nvram_set(const char *name, const char *value);
void nvram_buflist();

#endif

