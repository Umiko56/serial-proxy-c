#include "server.h"
#include "serial.h"

#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <pty.h>
#include <sys/ioctl.h>
#include <linux/serial.h>

/* <device-path>.<virtual-suffix> */
#define SERIAL_VIRTUAL_FORMAT ("%s.%s")

/**
 * @brief Create and open a new connection link.
 *
 * @param[in] node - Serial node to assign connection link to
 *
 * @return Pointer to a newly allocated serial connection link
 */
static serialLink *_serialCreateLink(serialNode *node);

/**
 * @brief Close connection and release memory.
 *
 * @param[in] link - Serial connection link
 */
static void _serialFreeLink(serialLink *link);

/**
 * @brief Called when a connection link encounters an error. The connection
 *        will be closed.
 *
 * @param[in] link - Serial connection link
 */
static void _serialLinkIOError(serialLink *link);

/**
 * @brief Iterate through all master and virtual serial devices and reconnect
 *        devices which are disconnected.
 */
static void _serialReconnect(void);

/**
 * @brief Write data from fromlink to tolink.
 *
 * @param[in] tolink - Link to write to
 * @param[in[ fromlink - Link to read buffer
 *
 */
static void _serialWriteLink(serialLink *tolink, serialLink *fromlink);

/**
 * @brief Callback for read and writes.
 *
 * @param[in] el - Pointer to event loop
 * @param[in] fd - File descriptor of serialNode
 * @param[in] privdata - Pointer to serialLink
 * @param[in] mask - Event flags
 */
static void _serialEventHandler(aeEventLoop *el, int fd, void *privdata, int mask);

/**
 * @brief Write handle callback when data is ready to be written.
 *
 * @param[in] link - Communication link with a write event
 */
static void _serialWriteHandler(serialLink *link);

/**
 * @brief Read handle callback when data is ready to be read.
 *
 * @param[in] link - Communication link with a read event
 */
static void _serialReadHandler(serialLink *link);

/**
 * @brief Return the event flags a serial node should have (based on current
 *        configuration).
 *
 * @param[in] node - Serial node
 *
 * @return event flags
 */
static int _serialEventFlags(serialNode *node);

/**
 * @brief Return the event flags as a string.
 *
 * @param[in] node - Serial node
 *
 * @return Flags as a string
 */
static const char *_serialEventString(serialNode *node);

static serialLink *_serialCreateLink(serialNode *node)
{
    serialLink *link;
    struct termios ts;
    int custom_baud = 0;

    link = calloc(1, sizeof(*link));
    if (!link) {
        serverLog(LL_ERROR, "calloc failed");
        exit(1);
    }

    link->fd = -1;
    link->sfd = -1;

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
            {
                struct serial_struct ser;

                custom_baud = 1;

                if (ioctl(link->fd, TIOCGSERIAL, &ser) == -1) {
                    goto err;
                }

                ser.custom_divisor = ser.baud_base / node->baudrate;
                ser.flags &= ~ASYNC_SPD_MASK;
                ser.flags |= ASYNC_SPD_CUST;

                if (ioctl(link->fd, TIOCSSERIAL, &ser) == -1) {
                    serverLogErrno(LL_ERROR, "ioctl");
                    goto err;
                }
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

    aeCreateFileEvent(server.el,
                      link->fd,
                      _serialEventFlags(node),
                      _serialEventHandler,
                      link);

    node->link = link;
    link->node = node;
    goto done;
err:
    if (link) {
        _serialLinkIOError(link);
        link = NULL;
    }
done:
    return link;
}

static int _serialEventFlags(serialNode *node)
{
    int flags = 0;

    if (nodeIsMaster(node)) {
        flags = AE_READABLE;
        if (serialGetVirtualWriterNode(node)) {
            flags |= AE_WRITABLE;
        }
    } else if (nodeIsVirtual(node)) {
        flags = AE_WRITABLE;
        if (nodeIsWriter(node)) {
            flags |= AE_READABLE;
        }
    }

    return flags;
}

static const char *_serialEventString(serialNode *node)
{
    const char *str = NULL;
    int flags = _serialEventFlags(node);

    if (flags & (AE_READABLE | AE_WRITABLE)) {
        str = "rw";
    } else if (flags & AE_READABLE) {
        str = "r";
    } else if (flags & AE_READABLE) {
        str = "w";
    }

    return str;
}

static void _serialFreeLink(serialLink *link)
{
    if (link->fd != -1 && link->node) {
        aeDeleteFileEvent(server.el, link->fd, _serialEventFlags(link->node));
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

static void _serialLinkIOError(serialLink *link)
{
    _serialFreeLink(link);
}

static void _serialReconnect(void)
{
    serialNode *node = server.serial.master_head;
    serialNode *vnode;

    while (node) {
        int connected = 1;

        if (!node->link) {
            if (serialConnectNode(node) == C_ERR) {
                serverLog(LL_WARN, "Problem reconnecting serial device: %s",
                          node->name);
                connected = 0;
            } else {
                serverLog(LL_INFO, "Reconnected serial: %s (%d) [%s]",
                          node->name, node->link->fd, _serialEventString(node));
            }
        }

        if (connected) {
            vnode = node->virtual_head;

            while (vnode) {
                if (!vnode->link) {
                    if (serialConnectNode(vnode) == C_ERR) {
                        serverLog(LL_WARN, "Problem reconnecting virtual serial"
                                 " device: %s", vnode->name);
                    } else {
                        serverLog(LL_INFO, "Reconnected virtual: %s (%d) [%s]",
                                  vnode->name, vnode->link->fd,
                                  _serialEventString(vnode));
                    }
                }
                vnode = vnode->next;
            }
        }

        node = node->next;
    }
}

void serialBeforeSleep(void)
{
    serialNode *node = server.serial.master_head;
    serialNode *vnode;

    while (node) {
        if (node->link) {
            node->link->recvbuflen = 0;
        }

        vnode = node->virtual_head;
        while (vnode) {
            if (vnode->link) {
                vnode->link->recvbuflen = 0;
            }
            vnode = vnode->next;
        }
        node = node->next;
    }
    usleep(1000000);
}

serialNode *serialCreateNode(const char *nodename, uint32_t flags)
{
    serialNode *node = NULL;

    if (!nodename) {
        serverLog(LL_ERROR, "Cannot create a serial node without a name");
        goto done;
    }

    node = calloc(1, sizeof(*node));
    if (!node) {
        serverLog(LL_ERROR, "calloc failed");
        exit(1);
    }

    strlcpy(node->name, nodename, sizeof(node->name));
    node->flags = flags;
    node->baudrate = 9600;

done:
    return node;
}

void serialFreeNode(serialNode *n)
{
    int j;
    serialNode *cur = n->virtual_head;

    while (cur) {
        cur->virtualof = NULL;
        cur = cur->next;
    }

    if (nodeIsVirtual(n)) {
        remove(n->name);

        if (n->virtualof) {
            serialRemoveVirtualNode(n->virtualof, n);
        }
    }

    n->virtual_head = NULL;
    free(n);
    n = NULL;
}

int serialConnectNode(serialNode *node)
{
    int ret = C_ERR;
    struct serialLink *link;

    if (node->link) {
        goto done;
    }

    link = _serialCreateLink(node);
    if (!link) {
        goto done;
    }

    ret = C_OK;
done:
    return ret;
}

void serialInit(void)
{
    server.serial.master_head = NULL;
    serialLoadConfig(server.serial_configfile);
    _serialReconnect();
}

void serialAddVirtualNode(serialNode *master, serialNode *virtual)
{
    if (master->virtual_head) {
        virtual->next = master->virtual_head;
    }

    master->virtual_head = virtual;
    virtual->virtualof = master;
}

void serialRemoveVirtualNode(serialNode *master, serialNode *virtual)
{
    serialNode *prev = NULL;
    serialNode *cur = master->virtual_head;

    while (cur) {
        if (virtual == cur) {
            if (!prev) {
                master->virtual_head = NULL;
            } else {
                prev->next = virtual->next;
            }
            virtual->next = NULL;
            break;
        }
        cur = cur->next;
    }
}

static void _serialEventHandler(aeEventLoop *el, int fd, void *privdata, int mask)
{
    serialLink *link = (serialLink*)privdata;

    (void) el;
    (void) fd;

    /* Could be closed if an error occurred while processing a read event */
    if (!link) {
        return;
    }

    if (mask & AE_READABLE) {
        _serialReadHandler(link);
    }

    if (mask & AE_WRITABLE) {
        _serialWriteHandler(link);
    }
}

static void _serialWriteLink(serialLink *fromlink, serialLink *tolink)
{
    int nwrite;

    /*
    serverLog(LL_DEBUG, "%s (%d) -> %s (%d)",
              fromlink->node->name, fromlink->recvbuflen,
              tolink->node->name, tolink->recvbuflen);
*/
    if (fromlink && fromlink->recvbuflen > 0) {
        nwrite = write(tolink->fd, fromlink->recvbuf, fromlink->recvbuflen);
        if (nwrite <= 0) {
            serverLogErrno(LL_ERROR, "I/O error writing to %s (%d) node link",
                           tolink->node->name, tolink->fd);
            _serialLinkIOError(tolink);
            tolink = NULL;
        } else {
            serverLog(LL_DEBUG, "Wrote %d bytes from %s (%d) to %s (%d)",
                      nwrite,
                      fromlink->node->name, fromlink->fd,
                      tolink->node->name, tolink->fd);
        }
    }
}

static void _serialWriteHandler(serialLink *link)
{
    serialNode *virtual;

    if (nodeIsVirtual(link->node)) {
        /* Read from master */
        _serialWriteLink(link->node->virtualof->link, link);
    } else if (nodeIsMaster(link->node)) {
        /* Read from virtual */
        virtual = serialGetVirtualWriterNode(link->node);
        if (virtual) {
            _serialWriteLink(virtual->link, link);
        }
    }
}

static void _serialReadHandler(serialLink *link)
{
    int nread;

    nread = read(link->fd, &link->recvbuf, BUFSIZ);
    if (nread <= 0) {
        if (errno != EAGAIN) {
            serverLogErrno(LL_ERROR, "I/O error reading from %s (%d) node link",
                           link->node->name, link->fd);
            _serialLinkIOError(link);
            link = NULL;
        }
    } else {
        serverLog(LL_DEBUG, "Read %d bytes from %s (%d)",
                  nread, link->node->name, link->fd);
        link->recvbuflen = nread;
    }
}

void serialAddNode(serialNode *node)
{
    if (server.serial.master_head) {
        node->next = server.serial.master_head;
    }

    server.serial.master_head = node;
}

void serialDelNode(serialNode *node)
{
    serialNode *prev = NULL;
    serialNode *cur = server.serial.master_head;

    while (cur) {
        if (node == cur) {
            if (!prev) {
                server.serial.master_head = NULL;
            } else {
                prev->next = node->next;
            }
            node->next = NULL;
            break;
        }
        cur = cur->next;
    }
}

serialNode *serialGetNode(const char *nodename)
{
    serialNode *node = NULL;
    serialNode *cur = server.serial.master_head;

    while (cur) {
        if (nodename && strcmp(nodename, cur->name) == 0) {
            node = cur;
            break;
        }
        cur = cur->next;
    }

    return node;
}

serialNode *serialGetVirtualNode(serialNode *master, const char *nodename)
{
    serialNode *node = NULL;
    serialNode *cur = master->virtual_head;

    while (cur) {
        if (nodename && strcmp(nodename, cur->name) == 0) {
            node = cur;
            break;
        }
        cur = cur->next;
    }

    return node;
}

serialNode *serialGetVirtualWriterNode(serialNode *master)
{
    serialNode *node = NULL;
    serialNode *cur = master->virtual_head;

    while (cur) {
        if (nodeIsWriter(cur)) {
            node = cur;
            break;
        }
        cur = cur->next;
    }

    return node;
}

int serialVirtualName(const char *device, const char *suffix,
                      char *name, size_t name_size)
{
    int ret = 0;
    int n;

    n = snprintf(name, name_size, SERIAL_VIRTUAL_FORMAT, device, suffix);
    if (!(n > -1 && n < name_size)) {
        ret = -1;
    }

    return ret;
}

void serialCron(void)
{
    _serialReconnect();
}

void serialTerm(void)
{
    serialNode *node = server.serial.master_head;
    serialNode *vnode;

    while (node) {
        serialNode *tmp;
        vnode = node->virtual_head;

        while (vnode) {
            tmp = vnode;
            vnode = vnode->next;

            serverLog(LL_INFO, "Closing virtual: %s", tmp->name);

            if (tmp->link) {
                _serialFreeLink(tmp->link);
                tmp->link = NULL;
            }

            serialFreeNode(tmp);
            tmp = NULL;
        }

        tmp = node;
        node = node->next;

        serverLog(LL_INFO, "Closing serial: %s", tmp->name);

        if (tmp->link) {
            _serialFreeLink(tmp->link);
            tmp->link = NULL;
        }

        serialFreeNode(tmp);
        tmp = NULL;
    }
}
