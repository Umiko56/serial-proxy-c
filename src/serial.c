#include "server.h"
#include "serial.h"

#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <pty.h>
#include <sys/ioctl.h>
#include <linux/serial.h>

void serialBeforeSleep(void)
{
    int j, i;
    serialNode *node;

    for (j = 0; j < server.serial->size; j++) {
        node = server.serial->nodes[j];

        if (node->link) {
            node->link->recvbuflen = 0;
        }

        for (i = 0; i < node->numvirtuals; i++) {
            if (node->virtuals[i]->link) {
                node->virtuals[i]->link->recvbuflen = 0;
            }
        }
    }
}

serialLink *serialCreateLink(serialNode *node)
{
    serialLink *link;

    link = malloc(sizeof(*link));
    if (!link) {
        serverLog(LL_ERROR, "malloc failed");
        exit(1);
    }

    link->node = node;
    link->fd = -1;
    link->sfd = -1;
    link->recvbuflen = 0;

    return link;
}

void serialFreeLink(serialLink *link)
{
    if (link->fd != -1) {
        aeDeleteFileEvent(server.el, link->fd, AE_WRITABLE);
        aeDeleteFileEvent(server.el, link->fd, AE_READABLE);
    }

    if (link->node) {
        link->node->link = NULL;
    }

    if (link->fd != -1) {
        close(link->fd);
        link->fd = -1;
    }

    if (link->sfd != -1) {
        close(link->sfd);
        link->sfd = -1;
    }

    free(link);
    link = NULL;
}

void serialLinkIOError(serialLink *link)
{
    serialFreeLink(link);
}

serialNode *serialCreateNode(const char *nodename, int flags)
{
    serialNode *node;

    node = malloc(sizeof(*node));
    if (!node) {
        serverLog(LL_ERROR, "malloc failed");
        exit(1);
    }

    if (nodename) {
        memcpy(node->name, nodename, sizeof(node->name));
    }

    node->link = NULL;
    node->flags = flags;
    node->numvirtuals = 0;
    node->virtuals = NULL;
    node->virtualof = NULL;
    node->baudrate = 9600;

    return node;
}

void serialFreeNode(serialNode *n)
{
    int j;

    for (j = 0; j < n->numvirtuals; j++) {
        n->virtuals[j]->virtualof = NULL;
    }

    if (nodeIsVirtual(n)) {
        remove(n->name);

        if (n->virtualof) {
            serialNodeRemoveVirtual(n->virtualof, n);
        }
    }

    free(n->virtuals);
    n->virtuals = NULL;
    free(n);
    n = NULL;
}

int serialConnect(serialNode *node)
{
    struct termios ts;
    serialLink *link;
    int custom_baud = 0;

    link = serialCreateLink(node);

    /* Already opened */
    if (node->link) {
        goto err;
    }

    if (nodeIsMaster(node)) {
        link->fd = open(node->name, O_RDWR | O_NOCTTY);

        if (link->fd == -1) {
            serverLogErrno(LL_ERROR, "open");
            goto err;
        }

        if (isatty(link->fd) == -1) {
            serverLogErrno(LL_ERROR, "isatty");
            goto err;
        }
    } else if (nodeIsVirtual(node)) {
        if (openpty(&link->fd, &link->sfd, NULL, NULL, NULL) == -1) {
            serverLogErrno(LL_ERROR, "openpty");
            goto err;
        }

        remove(node->name);

        if (symlink(ttyname(link->sfd), node->name) == -1) {
            serverLogErrno(LL_ERROR, "symlink");
            goto err;
        }
    }

    if (tcgetattr(link->fd, &ts) == -1) {
        serverLogErrno(LL_ERROR, "tcgetattr");
        goto err;
    }

    if (nodeIsMaster(node)) {
        speed_t baud;
        switch (node->baudrate) {
        #ifdef B0
            case 0: baud = B0; break;
        #endif
        #ifdef B50
            case 50: baud = B50; break;
        #endif
        #ifdef B75
            case 75: baud = B75; break;
        #endif
        #ifdef B110
            case 110: baud = B110; break;
        #endif
        #ifdef B134
            case 134: baud = B134; break;
        #endif
        #ifdef B150
            case 150: baud = B150; break;
        #endif
        #ifdef B200
            case 200: baud = B200; break;
        #endif
        #ifdef B300
            case 300: baud = B300; break;
        #endif
        #ifdef B600
            case 600: baud = B600; break;
        #endif
        #ifdef B1200
            case 1200: baud = B1200; break;
        #endif
        #ifdef B1800
            case 1800: baud = B1800; break;
        #endif
        #ifdef B2400
            case 2400: baud = B2400; break;
        #endif
        #ifdef B4800
            case 4800: baud = B4800; break;
        #endif
        #ifdef B7200
            case 7200: baud = B7200; break;
        #endif
        #ifdef B9600
            case 9600: baud = B9600; break;
        #endif
        #ifdef B14400
            case 14400: baud = B14400; break;
        #endif
        #ifdef B19200
            case 19200: baud = B19200; break;
        #endif
        #ifdef B28800
            case 28800: baud = B28800; break;
        #endif
        #ifdef B57600
            case 57600: baud = B57600; break;
        #endif
        #ifdef B76800
            case 76800: baud = B76800; break;
        #endif
        #ifdef B38400
            case 38400: baud = B38400; break;
        #endif
        #ifdef B115200
            case 115200: baud = B115200; break;
        #endif
        #ifdef B128000
            case 128000: baud = B128000; break;
        #endif
        #ifdef B153600
            case 153600: baud = B153600; break;
        #endif
        #ifdef B230400
            case 230400: baud = B230400; break;
        #endif
        #ifdef B256000
            case 256000: baud = B256000; break;
        #endif
        #ifdef B460800
            case 460800: baud = B460800; break;
        #endif
        #ifdef B576000
            case 576000: baud = B576000; break;
        #endif
        #ifdef B921600
            case 921600: baud = B921600; break;
        #endif
        #ifdef B1000000
            case 1000000: baud = B1000000; break;
        #endif
        #ifdef B1152000
            case 1152000: baud = B1152000; break;
        #endif
        #ifdef B1500000
            case 1500000: baud = B1500000; break;
        #endif
        #ifdef B2000000
            case 2000000: baud = B2000000; break;
        #endif
        #ifdef B2500000
            case 2500000: baud = B2500000; break;
        #endif
        #ifdef B3000000
            case 3000000: baud = B3000000; break;
        #endif
        #ifdef B3500000
            case 3500000: baud = B3500000; break;
        #endif
        #ifdef B4000000
            case 4000000: baud = B4000000; break;
        #endif
            default:
                custom_baud = 1;
                struct serial_struct ser;

                if (ioctl(link->fd, TIOCGSERIAL, &ser) == -1) {
                    goto err;
                }

                // set custom divisor
                ser.custom_divisor = ser.baud_base / node->baudrate;
                // update flags
                ser.flags &= ~ASYNC_SPD_MASK;
                ser.flags |= ASYNC_SPD_CUST;

                if (ioctl(link->fd, TIOCSSERIAL, &ser) == -1) {
                    serverLogErrno(LL_ERROR, "ioctl");
                    goto err;
                }
        }

        if (!custom_baud) {
            cfsetispeed(&ts, baud);
            cfsetospeed(&ts, baud);
        }
    }

    cfmakeraw(&ts);

    if (tcsetattr(link->fd, TCSANOW, &ts) == -1) {
        serverLogErrno(LL_ERROR, "tcsetattr");
        goto err;
    }

    node->link = link;

    if (nodeIsMaster(node)) {
        aeCreateFileEvent(server.el, link->fd, AE_READABLE, serialReadHandler, link);
    } else if (nodeIsVirtual(node)) {
        aeCreateFileEvent(server.el, link->fd, AE_WRITABLE, serialWriteHandler, link);
    }

    return C_OK;
err:
    serialLinkIOError(link);
    return C_ERR;
}

void serialInit(void)
{
    server.serial = malloc(sizeof(serialState));
    if (!server.serial) {
        serverLog(LL_ERROR, "malloc failed");
        exit(1);
    }

    server.serial->size = 0;

    server.serial->nodes = malloc(sizeof(serialNode));
    if (!server.serial->nodes) {
        serverLog(LL_ERROR, "malloc failed");
        exit(1);
    }

    serialLoadConfig(server.serial_configfile);
}

void serialCron(void)
{
    int j, i;
    serialNode *node;
    serialNode *vnode;

    for (j = 0; j < server.serial->size; j++) {
        node = server.serial->nodes[j];

        if (node->link == NULL) {
            if (serialConnect(node) == C_ERR) {
                serverLog(LL_WARNING, "Problem reconnecting serial "
                          "device: %s", node->name);
                continue;
            } else {
                serverLog(LL_DEBUG, "Reconnected serial: %s", node->name);
            }
        }

        for (i = 0; i < node->numvirtuals; i++) {
            vnode = node->virtuals[i];

            if (vnode->link == NULL) {
                if (serialConnect(vnode) == C_ERR) {
                    serverLog(LL_WARNING, "Problem reconnecting virtual "
                              "serial device: %s", vnode->name);
                    continue;
                } else {
                    serverLog(LL_DEBUG, "Reconnected virtual: %s", vnode->name);
                }
            }
        }
    }
}

int serialNodeAddVirtual(serialNode *master, serialNode *virtual)
{
    int j;
    struct serialNode **tmp_virtuals;

    if (nodeIsVirtual(master)) {
        return C_ERR;
    }

    for (j = 0; j < master->numvirtuals; j++) {
        if (master->virtuals[j] == virtual) {
            return C_ERR;
        }
    }

    tmp_virtuals = realloc(master->virtuals,
                           sizeof(serialNode*)*(master->numvirtuals+1));
    if (!tmp_virtuals) {
        serverLog(LL_ERROR, "realloc failed");
        exit(1);
    }
    master->virtuals = tmp_virtuals;
    master->virtuals[master->numvirtuals] = virtual;
    master->numvirtuals++;
    virtual->virtualof = master;

    return C_OK;
}

int serialNodeRemoveVirtual(serialNode *master, serialNode *virtual)
{
    int j;
    int remaining_virtuals;

    if (nodeIsVirtual(master)) {
        return C_ERR;
    }

    for (j = 0; j < master->numvirtuals; j++) {
        if (master->virtuals[j] == virtual) {
            if ((j+1) < master->numvirtuals) {
                remaining_virtuals = (master->numvirtuals - j) - 1;
                memmove(master->virtuals+j,master->virtuals+(j+1),
                        (sizeof(*master->virtuals) * remaining_virtuals));
            }
            master->numvirtuals--;
            return C_OK;
        }
    }

    return C_ERR;
}

void serialSetNodeAsMaster(serialNode *n)
{
    if (nodeIsMaster(n)) {
        return;
    }

    if (n->virtualof) {
        serialNodeRemoveVirtual(n->virtualof, n);
    }

    n->flags &= ~SERIAL_NODE_VIRTUAL;
    n->flags |= SERIAL_NODE_MASTER;
    n->virtualof = NULL;
}

void serialWriteHandler(aeEventLoop *el, int fd, void *privdata, int mask)
{
    serialLink *link = (serialLink*) privdata;

    if (link && link->node && nodeIsVirtual(link->node)) {
        /* Read from master */
        serialLink *mlink = link->node->virtualof->link;

        /* Check if master is connected */
        if (mlink && mlink->recvbuflen > 0) {
            int numwritten = write(link->fd, mlink->recvbuf, mlink->recvbuflen);
            if (numwritten <= 0) {
                serverLogErrno(LL_DEBUG, "I/O error writing to %s node link",
                               link->node->name);
                serialLinkIOError(link);
            }
        }
    }
}

void serialReadHandler(aeEventLoop *el, int fd, void *privdata, int mask)
{
    serialLink *link = (serialLink*) privdata;
    int nread;

    nread = read(link->fd, &link->recvbuf, BUFSIZ);
    if (nread <= 0) {
        serverLogErrno(LL_DEBUG, "I/O error reading from %s node link: %s",
                       link->node->name);
        serialLinkIOError(link);
    }

    link->recvbuflen = nread;
}

int serialAddNode(serialNode *node)
{
    int j;
    struct serialNode **tmp_nodes;

    for (j = 0; j < server.serial->size; j++) {
        if (server.serial->nodes[j] == node) {
            return C_ERR;
        }
    }

    tmp_nodes = realloc(server.serial->nodes,
                        sizeof(serialNode*)*(server.serial->size+1));
    if (!tmp_nodes) {
        serverLog(LL_ERROR, "realloc failed");
        exit(1);
    }

    server.serial->nodes = tmp_nodes;
    server.serial->nodes[server.serial->size] = node;
    server.serial->size++;
    return C_OK;
}

int serialDelNode(serialNode *delnode)
{
    int j;

    for (j = 0; j < server.serial->size; j++) {
        if (server.serial->nodes[j] == delnode) {
            if ((j+1) < server.serial->size) {
                int remaining_nodes = (server.serial->size - j) - 1;
                memmove(server.serial->nodes+j, server.serial->nodes+(j+1),
                    (sizeof(*server.serial->nodes)*remaining_nodes));
            }
            server.serial->size--;
            return C_OK;
        }
    }

    return C_ERR;
}

void serialTerm(void)
{
    serialNode *node;
    serialNode *vnode;

    while (server.serial->size > 0) {
        node = server.serial->nodes[0];

        while (node->numvirtuals > 0) {
            vnode = node->virtuals[0];

            serverLog(LL_DEBUG, "Closing virtual: %s", vnode->name);

            if (vnode->link) {
                serialFreeLink(vnode->link);
            }

            serialFreeNode(vnode);
            vnode = NULL;
        }

        serverLog(LL_DEBUG, "Closing serial: %s", node->name);

        if (node->link) {
            serialFreeLink(node->link);
        }

        serialDelNode(node);
        serialFreeNode(node);
        node = NULL;
    }

    free(server.serial->nodes);
    server.serial->nodes = NULL;
    free(server.serial);
    server.serial = NULL;
}
