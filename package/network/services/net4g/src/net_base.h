#ifndef NET_BASE_H
#define NET_BASE_H

#include "net_json.h"

cJSON * create_root_json_obj();
char* minimun_json(cJSON *root);
char* format_json(cJSON *root);
void output_type(cJSON *root);
char* json_strdup(const char* str);


#endif

