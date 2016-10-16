#include "config.h"
#include "server.h"
#include "serial.h"
#include "ini.h"

#include <limits.h>
#include <linux/limits.h>

#define MATCH(s, n) (strcasecmp(section, s) == 0 && strcasecmp(name, n) == 0)
#define N_MATCH(n) (strcasecmp(name, n) == 0)

/**
 * @brief Convert string "yes" to 1 and "no" to 0.
 *
 * @param[in] s - String to convert
 *
 * @return 1 if yes, 0 if no or -1 if neither
 */
static int _yesnotoi(const char *s);

/**
 * @brief Convert log name string to log level int.
 *
 * @param[in] name - Log name string
 *
 * @return Log level of name (if found) otherwise LL_ERROR
 */
static int _getLogLevel(const char *name);

/**
 * @brief Server device configuration file callback.
 *
 * @param[in] user - User data
 * @param[in] section - ini section
 * @param[in] name - configuration key
 * @param[in] value - configuration value
 *
 * @return 1 if configuration is valid, 0 if not
 */
static int _serverConfigHandler(void* user,
                                const char* section,
                                const char* name,
                                const char* value);

/**
 * @brief Serial device configuration file callback.
 *
 * @param[in] user - User data
 * @param[in] section - ini section (device name)
 * @param[in] name - configuration key
 * @param[in] value - configuration value
 *
 * @return 1 if configuration is valid, 0 if not
 */
static int _serialConfigHandler(void* user,
                                const char* section,
                                const char* name,
                                const char* value);

static int _yesnotoi(const char *s)
{
    if (!strcasecmp(s, "yes")) {
        return 1;
    } else if (!strcasecmp(s, "no")) {
        return 0;
    } else {
        return -1;
    }
}

static int _getLogLevel(const char *name)
{
    int level = LL_ERROR;

    if (!name) {
        goto done;
    }

    if (!strcasecmp(name, "debug")) {
        level = LL_DEBUG;
    } else if (!strcasecmp(name, "info")) {
        level = LL_INFO;
    } else if (!strcasecmp(name, "warn")) {
        level = LL_WARN;
    }
done:
    return level;
}

static int _serverConfigHandler(void* user,
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
        server->syslog = _yesnotoi(value);
    } else if (MATCH("logging", "loglevel")) {
        server->verbosity = _getLogLevel(value);
    } else if (MATCH("system", "hz")) {
        server->hz = atoi(value);
        if (server->hz < CONFIG_MIN_HZ) server->hz = CONFIG_MIN_HZ;
        if (server->hz > CONFIG_MAX_HZ) server->hz = CONFIG_MAX_HZ;
    } else if (MATCH("system", "reconnect-interval")) {
        server->reconnect_interval = atoi(value);
        if (server->reconnect_interval < CONFIG_MIN_RECONNECT_INTERVAL_MS) {
            server->reconnect_interval = CONFIG_MIN_RECONNECT_INTERVAL_MS;
        }
        if (server->reconnect_interval > CONFIG_MAX_RECONNECT_INTERVAL_MS) {
            server->reconnect_interval = CONFIG_MAX_RECONNECT_INTERVAL_MS;
        }
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
    if (filename && ini_parse(filename, _serverConfigHandler, &server) < 0) {
        fprintf(stderr, "Can't load config file: %s\n", filename);
        exit(1);
    }
}

static int _serialConfigHandler(void* user,
                                const char* section,
                                const char* name,
                                const char* value)
{
    struct sproxyServer *server = (struct sproxyServer*)user;
    serialNode *node;

    /* Check if serial port has been added, if not, create */
    node = serialGetNode(section);
    if (!node) {
        node = serialCreateNode(section, SERIAL_FLAG_MASTER);
        serialAddNode(node);
    }

    if (N_MATCH("baudrate")) {
        node->baudrate = atoi(value);
    } else if (N_MATCH("virtuals")) {
        char virtual_name[PATH_MAX];
        serialNode *vnode;
        char *str;
        char *token;

        str = strdup(value);
        if (!str) {
            fprintf(stderr, "Can't set virtuals: %s\n", value);
            exit(1);
        }

        token = strtok(str, " ");

        while (token) {
            if (serialVirtualName(section, token,
                                  virtual_name, sizeof(virtual_name)) != 0) {
                fprintf(stderr, "Can't create virtual name: %s\n", token);
                exit(1);
            }

            vnode = serialGetVirtualNode(node, virtual_name);
            if (!vnode) {
                vnode = serialCreateNode(virtual_name, SERIAL_FLAG_VIRTUAL);
                serialAddVirtualNode(node, vnode);
            }

            token = strtok(NULL, " ");
        }

        free(str);
        str = NULL;
    } else if (N_MATCH("writer")) {
        char virtual_name[PATH_MAX];
        serialNode *vnode;

        if (serialVirtualName(section, value,
                              virtual_name, sizeof(virtual_name)) != 0) {
            fprintf(stderr, "Can't create virtual name: %s\n", value);
            exit(1);
        }

        vnode = serialGetVirtualNode(node, virtual_name);
        if (vnode) {
            vnode->flags |= SERIAL_FLAG_WRITER;
        }
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

    if (ini_parse(filename, _serialConfigHandler, &server) < 0) {
        fprintf(stderr, "Can't load serial config file: %s\n", filename);
        exit(1);
    }
}
