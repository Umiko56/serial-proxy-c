#include "server.h"
#include "serial.h"

#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <pty.h>
#include <sys/ioctl.h>
#include <linux/serial.h>

void serialBeforeSleep(void) {
    /* Reset read buffer */
    int j;
    for (j = 0; j < server.serial->size; j++) {
        serialNode *node = server.serial->nodes[j];
        if (node->link) {
            node->link->recvbuflen = 0;
        }
        int i;
        for (i = 0; i < node->numvirtuals; i++) {
            if (node->virtuals[i]->link) {
                node->virtuals[i]->link->recvbuflen = 0;
            }
        }
    }
}

serialLink *createSerialLink(serialNode *node) {
    serialLink *link = malloc(sizeof(*link));
    link->node = node;
    link->fd = -1;
    link->sfd = -1;
    link->recvbuflen = 0;
    return link;
}

void freeSerialLink(serialLink *link) {
    if (link->fd != -1) {
        aeDeleteFileEvent(server.el, link->fd, AE_WRITABLE);
        aeDeleteFileEvent(server.el, link->fd, AE_READABLE);
    }
    if (link->node) {
        link->node->link = NULL;
    }
    if (link->fd != -1) {
        close(link->fd);
    }
    if (link->sfd != -1) {
        close(link->sfd);
    }
    free(link);
}

void handleLinkIOError(serialLink *link) {
    freeSerialLink(link);
}

int serialAcceptHandler(serialNode *node) {
    struct termios ts;
    serialLink *link = createSerialLink(node);
    int custom_baud = 0;

    /* Already opened */
    if (node->link) {
        goto err;
    }

    if (nodeIsMaster(node)) {
        link->fd = open(node->name, O_RDWR | O_NOCTTY);

        if (link->fd == -1) {
            goto err;
        }

        if (isatty(link->fd) == -1) {
            goto err;
        }
    } else if (nodeIsVirtual(node)) {
        if (openpty(&link->fd, &link->sfd, NULL, NULL, NULL) == -1) {
            goto err;
        }

        remove(node->name);

        if (symlink(ttyname(link->sfd), node->name) == -1) {
            goto err;
        }
    }

    if (tcgetattr(link->fd, &ts) == -1) {
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
    handleLinkIOError(link);
    return C_ERR;
}

void serialInit(void) {
    server.serial = malloc(sizeof(serialState));
    server.serial->size = 0;
    server.serial->nodes = malloc(sizeof(serialNode));

    loadSerialConfig(server.serial_configfile);
}

void serialCron(void) {
    int j, i;

    for (j = 0; j < server.serial->size; j++) {
        serialNode *node = server.serial->nodes[j];

        if (node->link == NULL) {
            if (serialAcceptHandler(node) == C_ERR) {
                serverLog(LL_WARNING, "Problem reconnecting serial "
                    "device: %s",
                    node->name);
                continue;
            } else {
                serverLog(LL_DEBUG, "Reconnected serial: %s", node->name);
            }
        }

        for (i = 0; i < node->numvirtuals; i++) {
            serialNode *vnode = node->virtuals[i];

            if (vnode->link == NULL) {
                if (serialAcceptHandler(vnode) == C_ERR) {
                    serverLog(LL_WARNING, "Problem reconnecting virtual "
                        "serial device: %s",
                        vnode->name);
                    continue;
                } else {
                    serverLog(LL_DEBUG, "Reconnected virtual: %s", vnode->name);
                }
            }
        }
    }
}

serialNode *createSerialNode(const char *nodename, int flags) {
    serialNode *node = malloc(sizeof(*node));

    if (nodename) {
        memcpy(node->name, nodename, SERIAL_NAMELEN);
    }

    node->link = NULL;
    node->flags = flags;
    node->numvirtuals = 0;
    node->virtuals = NULL;
    node->virtualof = NULL;
    node->baudrate = 9600;
    return node;
}

int serialNodeRemoveVirtual(serialNode *master, serialNode *virtual) {
    int j;

    for (j = 0; j < master->numvirtuals; j++) {
        if (master->virtuals[j] == virtual) {
            if ((j+1) < master->numvirtuals) {
                int remaining_virtuals = (master->numvirtuals - j) - 1;
                memmove(master->virtuals+j,master->virtuals+(j+1),
                        (sizeof(*master->virtuals) * remaining_virtuals));
            }
            master->numvirtuals--;
            /* TODO handle 0 virtuals */
            return C_OK;
        }
    }

    return C_ERR;
}

int serialNodeAddVirtual(serialNode *master, serialNode *virtual) {
    int j;

    for (j = 0; j < master->numvirtuals; j++) {
        if (master->virtuals[j] == virtual) {
            return C_ERR;
        }
    }

    master->virtuals = realloc(master->virtuals,
        sizeof(serialNode*)*(master->numvirtuals+1));
    master->virtuals[master->numvirtuals] = virtual;
    master->numvirtuals++;
    virtual->virtualof = master;

    return C_OK;
}

void freeSerialNode(serialNode *n) {
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
    free(n);
}

void serialSetNodeAsMaster(serialNode *n) {
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

void serialWriteHandler(aeEventLoop *el, int fd, void *privdata, int mask) {
    serialLink *link = (serialLink*) privdata;

    if (link->node && nodeIsVirtual(link->node)) {
        /* Read from master */
        serialLink *mlink = link->node->virtualof->link;

        /* Check if master is connected */
        if (mlink && mlink->recvbuflen > 0) {
            int numwritten = write(link->fd, mlink->recvbuf, mlink->recvbuflen);

            if (numwritten <= 0) {
                serverLog(LL_DEBUG,"I/O error writing to node link: %s",
                    strerror(errno));
                handleLinkIOError(link);
                return;
            }
        }
    }
}

void serialReadHandler(aeEventLoop *el, int fd, void *privdata, int mask) {
    serialLink *link = (serialLink*) privdata;
    int nread = read(link->fd, &link->recvbuf, BUFSIZ);

    if (nread <= 0) {
        serverLog(LL_DEBUG,"I/O error reading from node link: %s",
            (nread == 0) ? "connection closed" : strerror(errno));
        handleLinkIOError(link);
        return;
    }

    link->recvbuflen = nread;
}

int serialAddNode(serialNode *node) {
    int j;

    for (j = 0; j < server.serial->size; j++) {
        if (server.serial->nodes[j] == node) {
            return C_ERR;
        }
    }

    server.serial->nodes = realloc(server.serial->nodes,
        sizeof(serialNode*)*(server.serial->size+1));
    server.serial->nodes[server.serial->size] = node;
    server.serial->size++;
    return C_OK;
}

void serialDelNode(serialNode *delnode) {
    int j;

    for (j = 0; j < server.serial->size; j++) {
        if (server.serial->nodes[j] == delnode) {
            if ((j+1) < server.serial->size) {
                int remaining_nodes = (server.serial->size - j) - 1;
                memmove(server.serial->nodes+j, server.serial->nodes+(j+1),
                    (sizeof(*server.serial->nodes)*remaining_nodes));
            }
            server.serial->size--;
            return;
        }
    }
}

void serialTerm(void) {
    while (server.serial->size > 0) {
        serialNode *node = server.serial->nodes[0];

        while (node->numvirtuals > 0) {
            serialNode *vnode = node->virtuals[0];

            serverLog(LL_DEBUG, "Closing virtual: %s", vnode->name);

            if (vnode->link) {
                freeSerialLink(vnode->link);
            }

            freeSerialNode(vnode);
        }

        serverLog(LL_DEBUG, "Closing serial: %s", node->name);

        if (node->link) {
            freeSerialLink(node->link);
        }

        serialDelNode(node);
        freeSerialNode(node);
    }

    free(server.serial->nodes);
    free(server.serial);
}
