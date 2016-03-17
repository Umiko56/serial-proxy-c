#include "server.h"
#include "ae.h"
#include "cconfig.h"
#include "config.h"

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <signal.h>
#include <stdarg.h>
#include <syslog.h>
#include <sys/time.h>
#include <time.h>

/* Global vars */
struct sproxyServer server;

static void sigShutdownHandler(int sig) {
    switch (sig) {
        case SIGINT:
        case SIGTERM:
        default:
            break;
    };

    /* SIGINT is often delivered via Ctrl+C in an interactive session.
     * If we receive the signal the second time, we interpret this as
     * the user really wanting to quit ASAP without waiting to persist
     * on disk. */
    if (server.shutdown && sig == SIGINT) {
        exit(1); /* Exit with an error since this was not a clean shutdown. */
    }

    server.shutdown = 1;
}

void setupSignalHandlers(void) {
    struct sigaction act;

    /* When the SA_SIGINFO flag is set in sa_flags then sa_sigaction is used.
     * Otherwise, sa_handler is used. */
    sigemptyset(&act.sa_mask);
    act.sa_flags = 0;
    act.sa_handler = sigShutdownHandler;
    sigaction(SIGTERM, &act, NULL);
    sigaction(SIGINT, &act, NULL);
}

int prepareForShutdown() {
    serverLog(LL_WARNING,"User requested shutdown...");

    /* Remove the pid file if possible and needed. */
    if (server.daemonize || server.pidfile) {
        unlink(server.pidfile);
    }

    return C_OK;
}

int serverCron(struct aeEventLoop *eventLoop, long long id, void *clientData) {
    if (server.shutdown) {
        if (prepareForShutdown() == C_OK) {
            aeStop(eventLoop);
        } else {
            serverLog(LL_WARNING, "SIGTERM received but errors trying to shut down the server, check the logs for more information");
            server.shutdown = 0;
        }
    }

    run_with_period(100) {
        serialCron();
    }

    server.cronloops++;
    return 1000/server.hz;
}

void initServerConfig(void) {
    server.pid = getpid();
    server.configfile = NULL;
    server.pidfile = NULL;
    server.shutdown = 0;
    server.cronloops = 0;
    server.daemonize = CONFIG_DEFAULT_DAEMONIZE;
    server.verbosity = CONFIG_DEFAULT_VERBOSITY;
    server.syslog = CONFIG_DEFAULT_SYSLOG_ENABLED;
    server.logfile = strdup(CONFIG_DEFAULT_LOGFILE);
    server.maxclients = CONFIG_DEFAULT_MAX_CLIENTS;
    server.hz = CONFIG_DEFAULT_HZ;
    server.el = aeCreateEventLoop(server.maxclients);
    server.serial_configfile = strdup(CONFIG_DEFAULT_SERIAL_CONFIG_FILE);
    server.cron_event_id = AE_ERR;
}

void initServer(void) {
    signal(SIGHUP, SIG_IGN);
    signal(SIGPIPE, SIG_IGN);
    setupSignalHandlers();

    server.cron_event_id = aeCreateTimeEvent(server.el, 1, serverCron, NULL, NULL);

    if (server.cron_event_id == AE_ERR) {
        serverLog(LL_ERROR, "Can't create event loop timers.");
        exit(1);
    }

    serialInit();
}

void termServer(void) {
    serialTerm();

    if (server.logfile) free(server.logfile);
    if (server.pidfile) free(server.pidfile);
    if (server.configfile) free(server.configfile);
    if (server.serial_configfile) free(server.serial_configfile);

    if (aeDeleteTimeEvent(server.el, server.cron_event_id) == AE_ERR) {
        serverLog(LL_WARNING, "Failed removing event loop timers.");
    }
}

void version(void) {
    printf("Sproxy server v=%s\n", SPROXY_VERSION);
    exit(0);
}

void daemonize(void) {
    int fd;

    if (fork() != 0) exit(0); /* parent exits */
    setsid(); /* create a new session */

    /* Every output goes to /dev/null. If sproxyd is daemonized but
     * the 'logfile' is set to 'stdout' in the configuration file
     * it will not log at all. */
    if ((fd = open("/dev/null", O_RDWR, 0)) != -1) {
        dup2(fd, STDIN_FILENO);
        dup2(fd, STDOUT_FILENO);
        dup2(fd, STDERR_FILENO);
        if (fd > STDERR_FILENO) close(fd);
    }
}

void createPidFile(void) {
    if (server.pidfile) {
        server.pidfile = strdup(CONFIG_DEFAULT_PID_FILE); /* TODO memory */
    }
    /* Try to write the pid file in a best-effort way. */
    FILE *fp = fopen(server.pidfile, "w");
    if (fp) {
        fprintf(fp, "%d\n", (int)getpid());
        fclose(fp);
    }
}

void usage(void) {
    fprintf(stderr,
        "\n"
		"Usage: sproxyd [OPTIONS]\n\n"
		"OPTIONS\n\n"
        "-c\tConfig file\n"
        "-s\tSerial config file\n"
		"-d\tDaemonize\n"
		"-v\tProgram version\n"
		"-h\tUsage\n\n");
    exit(1);
}

void beforeSleep(struct aeEventLoop *eventLoop) {
    serialBeforeSleep();
}

void serverLogRaw(int level, const char *msg) {
    const int syslogLevelMap[] = { LOG_DEBUG, LOG_INFO, LOG_NOTICE, LOG_WARNING, LOG_ERR };
    const char* standardLevelMap[] = { "DEBUG", "INFO", "NOTICE", "WARN", "ERROR" };
    FILE *fp;
    char buf[64];
    int log_to_stdout = server.logfile[0] == '\0';

    level &= 0xff; /* clear flags */
    if (level < server.verbosity) return;

    fp = log_to_stdout ? stdout : fopen(server.logfile, "a");
    if (!fp) return;

    int off;
    struct timeval tv;
    gettimeofday(&tv,NULL);
    off = strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S.", localtime(&tv.tv_sec));
    snprintf(buf+off, sizeof(buf)-off, "%03d", (int)tv.tv_usec/1000);
    fprintf(fp, "%s %s %s\n", buf, standardLevelMap[level], msg);
    fflush(fp);

    if (!log_to_stdout) fclose(fp);
    if (server.syslog) syslog(syslogLevelMap[level], "%s", msg);
}

void serverLog(int level, const char *fmt, ...) {
    va_list ap;
    char msg[LOG_MAX_LEN];

    if ((level&0xff) < server.verbosity) return;

    va_start(ap, fmt);
    vsnprintf(msg, sizeof(msg), fmt, ap);
    va_end(ap);

    serverLogRaw(level, msg);
}

int main(int argc, char *argv[]) {
    initServerConfig();

    int c;
    while ((c = getopt(argc, argv, "c:dvh")) != -1) {
        switch (c) {
            case 'c':
                server.configfile = strdup(optarg);
                break;
            case 'd':
                server.daemonize = 1;
                break;
            case 'v':
                version();
			case 'h':
            default:
                usage();
        }
    }

    loadServerConfig(server.configfile);

    if (server.daemonize) {
        daemonize();

        if (server.pidfile) {
            createPidFile();
        }
    }

    initServer();

    serverLog(LL_NOTICE,"Server started, Sproxy version " SPROXY_VERSION);

    aeSetBeforeSleepProc(server.el, beforeSleep);
    aeMain(server.el);
    termServer();
    aeDeleteEventLoop(server.el);

    return 0;
}
