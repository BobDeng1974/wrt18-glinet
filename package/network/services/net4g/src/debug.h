#ifndef DEBUG_H
#define DEBUG_H

#include <syslog.h>
#define DEBUG(fmt, ...)  syslog(LOG_DEBUG, fmt, ## __VA_ARGS__)
#define NOTE(fmt, ...) syslog(LOG_NOTICE, fmt, ## __VA_ARGS__)
#define WARN(fmt, ...) syslog(LOG_WARNING,fmt, ## __VA_ARGS__)
#define ERROR(fmt, ...)  syslog(LOG_ERR, fmt, ## __VA_ARGS__)

#endif