#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <assert.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "net_base.h"

cJSON * create_root_json_obj()
{
	cJSON *root = cJSON_CreateObject();
	assert(root);
	return root;
}

/* Need to release the returned pointer */
char* minimun_json(cJSON *root)
{
	return cJSON_PrintUnformatted(root);
}

/* Need to release the returned pointer */
char* format_json(cJSON *root)
{
	return cJSON_Print(root);
}

void output_type(cJSON *root)
{
	if(!root) {
		printf("Invalid cJson!\n");
		return;
	}
	switch(root->type) {
		case cJSON_False:
		case cJSON_True:
			printf("Type is False or True\n");
			break;
		case cJSON_Number:
			printf("Type is Number\n");
			break;
		case cJSON_String:
			printf("Type is String\n");
			break;
		case cJSON_Array:
			printf("Type is Array\n");
			break;
		case cJSON_Object:
			printf("Type is Object\n");
			break;
		default:
			printf("Unknow type!!!\n");
			break;
	}
}

char* json_strdup(const char* str)
{
      size_t len;
      char* copy = NULL;

      len = strlen(str) + 1;
      if (!(copy = (char*)malloc(len))) return 0;
      memcpy(copy,str,len);
      return copy;
}



