#include "server.h"
#include "ae.h"
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

#define DATETIME_BUF_SIZE (64)

struct sproxyServer server;

static void sigHandler(int sig)
{
    switch (sig) {
        case SIGINT:
        case SIGTERM:
            /* SIGINT is often delivered via Ctrl+C in an interactive session.
             * If we receive the signal the second time, we interpret this as
             * the user really wanting to quit ASAP without waiting to persist
             * on disk. */
            if (server.shutdown && sig == SIGINT) {
                exit(1); /* Exit with an error since this was not a clean shutdown. */
            }

            server.shutdown = 1;
            break;
        case SIGHUP:
            server.reload = 1;
            break;
        default:
            return;
    };
}

void setupSignalHandlers(void)
{
    struct sigaction act = {0};

    /* When the SA_SIGINFO flag is set in sa_flags then sa_sigaction is used.
     * Otherwise, sa_handler is used. */
    sigemptyset(&act.sa_mask);
    act.sa_flags = 0;
    act.sa_handler = sigHandler;

    if (sigaction(SIGTERM, &act, NULL) != 0) {
        serverLogErrno(LL_ERROR, "sigaction(SIGTERM) failed");
        exit(1);
    }

    if (sigaction(SIGINT, &act, NULL) != 0) {
        serverLogErrno(LL_ERROR, "sigaction(SIGINT) failed");
        exit(1);
    }

    if (sigaction(SIGHUP, &act, NULL) != 0) {
        serverLogErrno(LL_ERROR, "sigaction(SIGHUP) failed");
        exit(1);
    }
}

int prepareForShutdown()
{
    serverLog(LL_WARNING,"User requested shutdown...");

    /* Remove the pid file if possible and needed. */
    if (server.daemonize || server.pidfile) {
        unlink(server.pidfile);
    }

    return C_OK;
}

int serverCron(struct aeEventLoop *eventLoop, long long id, void *clientData)
{
    if (server.shutdown) {
        if (prepareForShutdown() == C_OK) {
            aeStop(eventLoop);
        } else {
            serverLog(LL_WARNING, "SIGTERM received but errors trying to shut "
                      "down the server, check the logs for more information");
            server.shutdown = 0;
        }
    }

    run_with_period(server.reconnect_interval) {
        serialCron();
    }

    server.cronloops++;
    return 1000/server.hz;
}

void serverInitConfig(void)
{
    server.pid = getpid();
    server.configfile = NULL;
    server.pidfile = NULL;
    server.shutdown = 0;
    server.cronloops = 0;
    server.daemonize = CONFIG_DEFAULT_DAEMONIZE;
    server.verbosity = CONFIG_DEFAULT_VERBOSITY;
    server.syslog = CONFIG_DEFAULT_SYSLOG_ENABLED;
    server.maxclients = CONFIG_DEFAULT_MAX_CLIENTS;
    server.el = aeCreateEventLoop(server.maxclients);
    server.cron_event_id = AE_ERR;
    server.hz = CONFIG_DEFAULT_HZ;
    server.reconnect_interval = CONFIG_DEFAULT_RECONNECT_INTERVAL_MS;

    server.logfile = strdup(CONFIG_DEFAULT_LOGFILE);
    if (!server.logfile) {
        serverLog(LL_ERROR, "strdup failed");
        exit(1);
    }

    server.serial_configfile = strdup(CONFIG_DEFAULT_SERIAL_CONFIG_FILE);
    if (!server.serial_configfile) {
        serverLog(LL_ERROR, "strdup failed");
        exit(1);
    }
}

void serverInit(void)
{
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

void serverTerm(void)
{
    serialTerm();

    free(server.logfile);
    server.logfile = NULL;
    free(server.pidfile);
    server.pidfile = NULL;
    free(server.configfile);
    server.configfile = NULL;
    free(server.serial_configfile);
    server.serial_configfile = NULL;

    if (aeDeleteTimeEvent(server.el, server.cron_event_id) == AE_ERR) {
        serverLog(LL_WARNING, "Failed removing event loop timers.");
    }

    server.cron_event_id = AE_ERR;
}

void version(void)
{
    printf("Sproxy server v=%s\n", SPROXY_VERSION);
    exit(0);
}

void daemonize(void)
{
    int fd;

    if (fork() != 0) {
        exit(0);
    }
    setsid();

    /* Every output goes to /dev/null. If sproxyd is daemonized but
     * the 'logfile' is set to 'stdout' in the configuration file
     * it will not log at all. */
    fd = open("/dev/null", O_RDWR, 0);
    if (fd != -1) {
        dup2(fd, STDIN_FILENO);
        dup2(fd, STDOUT_FILENO);
        dup2(fd, STDERR_FILENO);
        if (fd > STDERR_FILENO) {
            close(fd);
        }
    }
}

void createPidFile(void)
{
    FILE *fp = NULL;

    if (server.pidfile) {
        server.pidfile = strdup(CONFIG_DEFAULT_PID_FILE);
        if (!server.pidfile) {
            serverLog(LL_ERROR, "strdup failed");
            exit(1);
        }
    }

    /* Try to write the pid file in a best-effort way. */
    fp = fopen(server.pidfile, "w");
    if (fp) {
        fprintf(fp, "%d\n", (int)getpid());
        fclose(fp);
    }
}

void usage(void)
{
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

void serverBeforeSleep(struct aeEventLoop *eventLoop)
{
    serialBeforeSleep();
}

void serverLogRaw(int level, const char *msg)
{
    static const int syslogLevelMap[] = {
        LOG_DEBUG,
        LOG_INFO,
        LOG_NOTICE,
        LOG_WARNING,
        LOG_ERR
    };
    static const char* standardLevelMap[] = {
        "DEBUG",
        "INFO",
        "NOTICE",
        "WARN",
        "ERROR"
    };
    FILE *fp = NULL;
    char datetime_buf[DATETIME_BUF_SIZE] = {0};
    int offset;
    struct timeval tv = {0};

    if (level < server.verbosity) {
        return;
    }

    fp = !server.logfile[0] ? stdout : fopen(server.logfile, "a");
    if (!fp) {
        return;
    }

    if (gettimeofday(&tv, NULL) != 0) {
        return;
    }

    strftime(datetime_buf, sizeof(datetime_buf),
             "%Y-%m-%d %H:%M:%S", localtime(&tv.tv_sec));

    fprintf(fp, "%s %s %s\n", datetime_buf, standardLevelMap[level], msg);
    fflush(fp);

    if (server.logfile[0]) {
        fclose(fp);
    }

    if (server.syslog) {
        syslog(syslogLevelMap[level], "%s", msg);
    }
}

void serverLog(int level, const char *fmt, ...)
{
    va_list ap;
    char msg[LOG_MAX_LEN] = {0};

    if (level < server.verbosity) {
        return;
    }

    va_start(ap, fmt);
    vsnprintf(msg, sizeof(msg), fmt, ap);
    va_end(ap);

    serverLogRaw(level, msg);
}

void serverLogErrno(int level, const char *fmt, ...)
{
    va_list ap;
    char msg[LOG_MAX_LEN] = {0};
    const char *diag_fmt = "%s, Error: %s (%d)";

    if (level < server.verbosity) {
        return;
    }

    va_start(ap, fmt);
    vsnprintf(msg, sizeof(msg), fmt, ap);
    va_end(ap);

    serverLog(level, diag_fmt, msg, strerror(errno), errno);
}

int main(int argc, char *argv[])
{
    int c;

    serverInitConfig();

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
                break;
			case 'h':
            default:
                usage();
        }
    }

    serverLoadConfig(server.configfile);

    if (server.daemonize) {
        daemonize();

        if (server.pidfile) {
            createPidFile();
        }
    }

    serverInit();

    serverLog(LL_NOTICE,"Server started, sproxy version " SPROXY_VERSION);

    aeSetBeforeSleepProc(server.el, serverBeforeSleep);
    aeMain(server.el);
    serverTerm();
    aeDeleteEventLoop(server.el);

    return 0;
}
