#include "config.h"
#include "server.h"
#include "serial.h"
#include "ini.h"

#include <linux/limits.h>

#define MATCH(s, n) (strcmp(section, s) == 0 && strcmp(name, n) == 0)
#define N_MATCH(n) (strcmp(name, n) == 0)

typedef struct configEnum {
    const char *name;
    const int val;
} configEnum;

configEnum loglevel_enum[] = {
    {"debug", LL_DEBUG},
    {"verbose", LL_VERBOSE},
    {"notice", LL_NOTICE},
    {"warning", LL_WARNING},
    {"error", LL_ERROR},
    {NULL, 0}
};

int yesnotoi(const char *s)
{
    if (!strcasecmp(s, "yes")) {
        return 1;
    } else if (!strcasecmp(s, "no")) {
        return 0;
    } else {
        return -1;
    }
}

int configEnumGetValue(configEnum *ce, const char *name)
{
    while(ce->name != NULL) {
        if (!strcasecmp(ce->name, name)) {
            return ce->val;
        }
        ce++;
    }
    return INT_MIN;
}

static int serverConfigHandler(void* user,
                               const char* section,
                               const char* name,
                               const char* value)
{
    struct sproxyServer *server = (struct sproxyServer*)user;

    if (MATCH("logging", "logfile")) {
        server->logfile = strdup(value);
        if (!server->logfile) {
            fprintf(stderr, "Can't set config file: %s\n", value);
            exit(1);
        }
    } else if (MATCH("logging", "syslog-enabled")) {
        server->syslog = yesnotoi(value);
    } else if (MATCH("logging", "loglevel")) {
        server->verbosity = configEnumGetValue(loglevel_enum, value);
    } else if (MATCH("system", "hz")) {
        server->hz = atoi(value);
        if (server->hz < CONFIG_MIN_HZ) server->hz = CONFIG_MIN_HZ;
        if (server->hz > CONFIG_MAX_HZ) server->hz = CONFIG_MAX_HZ;
    } else if (MATCH("system", "pidfile")) {
        server->pidfile = strdup(value);
        if (!server->pidfile) {
            fprintf(stderr, "Can't set pid file: %s\n", value);
            exit(1);
        }
    } else if (MATCH("system", "serial-configfile")) {
        /* if already assigned */
        if (server->serial_configfile) {
            free(server->serial_configfile);
            server->serial_configfile = NULL;
        }
        server->serial_configfile = strdup(value);
        if (!server->serial_configfile) {
            fprintf(stderr, "Can't set serial config file: %s\n", value);
            exit(1);
        }
    } else {
        return 0;
    }
    return 1;
}

void serverLoadConfig(const char *filename)
{
    if (filename && ini_parse(filename, serverConfigHandler, &server) < 0) {
        fprintf(stderr, "Can't load config file: %s\n", filename);
        exit(1);
    }
}

static int serialConfigHandler(void* user,
                               const char* section,
                               const char* name,
                               const char* value)
{
    struct sproxyServer *server = (struct sproxyServer*)user;
    serialNode *node = NULL;
    int j;

    /* check if serial port has been added */
    for (j = 0; j < server->serial->size; j++) {
        if (strcmp(server->serial->nodes[j]->name, section) == 0) {
            node = server->serial->nodes[j];
        }
    }

    /* not found, create */
    if (node == NULL) {
        node = serialCreateNode(section, SERIAL_NODE_MASTER);
        serialAddNode(node);
    }

    if (N_MATCH("baudrate")) {
        node->baudrate = atoi(value);
    } else if (N_MATCH("virtuals")) {
        char *str;
        char *token;
        char buf[PATH_MAX];
        serialNode *vnode;
        int n;

        str = strdup(value);
        if (!str) {
            fprintf(stderr, "Can't set virtuals: %s\n", value);
            exit(1);
        }

        token = strtok(str, " ");

        while (token != NULL) {
            n = snprintf(buf, sizeof(buf), "%s.%s", section, token);
            if (!(n > -1 && n < sizeof(buf))) {
                fprintf(stderr, "Can't set virtual: %s\n", token);
                exit(1);
            }

            vnode = serialCreateNode(buf, SERIAL_NODE_VIRTUAL);
            serialNodeAddVirtual(node, vnode);

            token = strtok(NULL, " ");
        }

        free(str);
        str = NULL;
    } else {
        return 0;
    }
    return 1;
}

void serialLoadConfig(const char *filename)
{
    if (!filename) {
        fprintf(stderr, "Serial config file must be given\n");
        exit(1);
    }

    if (ini_parse(filename, serialConfigHandler, &server) < 0) {
        fprintf(stderr, "Can't load serial config file: %s\n", filename);
        exit(1);
    }
}
