#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <assert.h>
#include <fcntl.h>
#include <unistd.h>

#include "config.h"


#define BUFSZ 		128

static nvram_handle_t *hash_tb = NULL;

void nvram_buflist()
{
	nvram_tuple_t *all = hash_nvram_getall(hash_tb);
    nvram_tuple_t *t = all,*next;
    while (t && (t->next != all )){
        printf("'%s'='%s'\n",t->name,t->value);
		next = t->next;
		free(t);
		t = next;
    }
}

CONFIG_ST* config_init()
{
	CONFIG_ST *cfg_st = (CONFIG_ST*)malloc(sizeof(CONFIG_ST));
	memset(cfg_st,0,sizeof(CONFIG_ST));
	cfg_st->hash_tb = (nvram_handle_t*)malloc(sizeof(nvram_handle_t));
	memset(cfg_st->hash_tb,0,sizeof(nvram_handle_t));
	hash_tb = cfg_st->hash_tb;
	return cfg_st;
}

void config_close(CONFIG_ST *cfg_st)
{
	if(cfg_st) {
		if(cfg_st->hash_tb) {
			hash_nvram_close(cfg_st->hash_tb);
			free(cfg_st->hash_tb);
		}
		free(cfg_st);
	}
}

int config_save(const char *path)
{
	FILE *fp;
	if (NULL == (fp = fopen(path, "w"))) {
		perror("fopen");
		return -1;
	}

	if(strlen(HEAD) != fwrite(HEAD,1,strlen(HEAD),fp)) {
		fprintf(stderr,"fwrite cfg '%s' file error\n",path);
		unlink(path);
		fclose(fp);
		return -1;
	}
	
	nvram_tuple_t *all = hash_nvram_getall(hash_tb);
    nvram_tuple_t *t = all,*next;
	char line[256] = {0};
	int ret = 0;
    while (t && (t->next != all )){
      //  printf("'%s'='%s'\n",t->name,t->value);
		sprintf(line,"%s=%s\n",t->name,t->value);
		ret = fwrite(line,1,strlen(line),fp);
		if(ret != strlen(line)) {
			fprintf(stderr,"fwrite cfg '%s' file error\n",path);
			unlink(path);
			fclose(fp);
			free(t);
			return -1;
		}
		next = t->next;
		free(t);
		t = next;
    }
    fclose(fp);
	return 0;
}

const char* nvram_get(const char *name)
{
	if(!name)
		return "";
	return hash_nvram_safe_get(hash_tb,name);
}

int nvram_set(const char *name, const char *value)
{
	if(!name)
		return -1;
	if(!value) {
		if(0 != hash_nvram_set(hash_tb,name,"")) {
			printf("Nvarm_set Hast set error!\n");
			return -1;
		}
	}
	if(0 != hash_nvram_set(hash_tb,name,value)) {
		printf("Nvarm_set Hast set error!\n");
		return -1;
	}
	return 0;
}

int nvram_renew(char *fname)
{	
	FILE *fp;
	char buf[BUFSZ], *p;
	int found = 0;

	if (NULL == (fp = fopen(fname, "ro"))) {
		printf("fopen %s error!\n",fname);
		return -1;
	}

	//find "Default" first
	while (NULL != fgets(buf, BUFSZ, fp)) {
		if (buf[0] == '\n' || buf[0] == '#')
			continue;
		if (!strncmp(buf, "Default\n", 8)) {
			found = 1;
			break;
		}
	}
	if (!found) {
		printf("%s file format error!\n",fname);
		fclose(fp);
		return -1;
	}

	while (NULL != fgets(buf, BUFSZ, fp)) {
		if (buf[0] == '\n' || buf[0] == '#' || buf[0] == ' ')
			continue;
		if (NULL == (p = strchr(buf, '='))) {
// 			printf("%s line [%s] format error!\n",fname, buf);
			continue;
		}
		buf[strlen(buf) - 1] = '\0'; //remove carriage return
		*p++ = '\0'; //seperate the string
		while(*p == ' ' && *p != '\0') p++;  //skip ' '
 
	//	printf("bufset'%s'='%s'\n", buf, p);
		if(strcmp(buf,"") != 0 && strlen(buf) <= 64) {
			if(0 != nvram_set(buf,p)) {
				printf("hash set error!\n");
				fclose(fp);
				return -1;
			}
		}
	}
	
	fclose(fp);
	return 0;
}

