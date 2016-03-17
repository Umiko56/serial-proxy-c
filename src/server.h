#ifndef __SPROXY_H
#define __SPROXY_H

#include "ae.h"

#include <sys/types.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* Error codes */
#define C_OK  0
#define C_ERR -1

/* Static server configuration */
#define LOG_MAX_LEN 1024
#define CONFIG_DEFAULT_HZ 10
#define CONFIG_MIN_HZ     1
#define CONFIG_MAX_HZ     500
#define CONFIG_DEFAULT_PID_FILE "/var/run/sproxyd.pid"
#define CONFIG_DEFAULT_DAEMONIZE 0
#define CONFIG_DEFAULT_LOGFILE ""
#define CONFIG_DEFAULT_SYSLOG_ENABLED 0
#define CONFIG_SP_MAX 4
#define CONFIG_DEFAULT_MAX_CLIENTS 1000
#define CONFIG_DEFAULT_SERIAL_CONFIG_FILE "serial.ini"
#define CONFIG_MAX_LINE 1024

/* Log levels */
#define LL_DEBUG 0
#define LL_VERBOSE 1
#define LL_NOTICE 2
#define LL_WARNING 3
#define LL_ERROR 4
#define CONFIG_DEFAULT_VERBOSITY LL_DEBUG

#define run_with_period(_ms_) if ((_ms_ <= 1000/server.hz) || !(server.cronloops%((_ms_)/(1000/server.hz))))

struct sproxyServer {
    pid_t pid;                  /* Main process pid.*/
    char *pidfile;              /* PID file path */
    char *logfile;              /* Log file */
    char *configfile;           /* System config file */
    int shutdown;               /* Signal to shutdown */
    int daemonize;              /* True if running as a daemon */
    int verbosity;              /* Logging level */
    int syslog;                 /* Is syslog enabled? */
    int maxclients;             /* Max concurrent clients */
    int cronloops;              /* Number of times the cron function run */
    long long cron_event_id;    /* Cron task id */
    aeEventLoop *el;
    int hz;                     /* Timer event frequency */
    char *serial_configfile;    /* Serial config file */
    struct serialState *serial; /* State of serial devices */
};

/* Extern declarations */
extern struct sproxyServer server;

/* Core functions */
void serverLog(int level, const char *fmt, ...);
void serverLogRaw(int level, const char *msg);
void loadServerConfig(const char *filename);
void usage(void);

/* Serial */
void serialInit(void);
void serialTerm(void);
void serialBeforeSleep(void);
void serialCron(void);
void loadSerialConfig(const char *filename);

#endif
