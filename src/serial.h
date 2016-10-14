#ifndef SERIAL_H
#define SERIAL_H

#include <linux/limits.h>
#include <stdint.h>
#include <stdio.h>

/* Serial state */
#define SERIAL_OK      (0)  /* Everything looks ok */
#define SERIAL_FAIL    (1)  /* The serial device is not working */

/* Serial flags */
enum {
    SERIAL_FLAG_MASTER    = 1, /* The node is a master */
    SERIAL_FLAG_VIRTUAL   = 2, /* The node is a virtual */
};

#define nodeIsMaster(n) ((n)->flags & SERIAL_FLAG_MASTER)
#define nodeIsVirtual(n) ((n)->flags & SERIAL_FLAG_VIRTUAL)

struct serialNode;

typedef struct serialLink {
    int fd;                          /* Serial file descriptor */
    int sfd;                         /* Slave serial file descriptor */
    char recvbuf[BUFSIZ];            /* Receive buffer */
    int recvbuflen;                  /* Number of bytes received */
    struct serialNode *node;         /* Node related to this link if any, or NULL */
} serialLink;

typedef struct serialNode {
    char name[PATH_MAX];             /* Path to device (/dev/ttyS1) */
    uint32_t flags;
    struct serialNode *virtual_head; /* Pointers to virtuals (if node is master) */
    struct serialNode *virtualof;    /* Pointer to master (if node is virtual) */
    int baudrate;                    /* Baudrate of device */
    serialLink *link;                /* rs232 link with this node */
    struct serialNode *next;         /* Pointer to next master in list (if any) */
} serialNode;

typedef struct serialState {
    struct serialNode *master_head;  /* Pointer to masters */
} serialState;

/**
 * @brief Allocate memory and initialize a new serial node.
 *
 * @param[in] nodename - Serial device path
 * @param[in] flags - Serial device responsibility and state
 *
 * @return Pointer to newly allocated serialNode object
 */
serialNode *serialCreateNode(const char *nodename, uint32_t flags);

/**
 * @brief Add serialNode to list.
 *
 * @param[in] node - serialNode to add
 */
void serialAddNode(serialNode *node);

/**
 * @brief Remove serialNode from list.
 *
 * @param[in] node - serialNode to remove
 */
void serialDelNode(serialNode *node);

/**
 * @brief Write handle callback when data is ready to be written.
 *
 * @param[in] el - Pointer to event loop
 * @param[in] fd - File descriptor of serialNode
 * @param[in] privdata - Pointer to serialLink
 * @param[in] mask - Event flags
 */
void serialWriteHandler(aeEventLoop *el, int fd, void *privdata, int mask);

/**
 * @brief Read handle callback when data is ready to be read.
 *
 * @param[in] el - Pointer to event loop
 * @param[in] fd - File descriptor of serialNode
 * @param[in] privdata - Pointer to serialLink
 * @param[in] mask - Event flags
 */
void serialReadHandler(aeEventLoop *el, int fd, void *privdata, int mask);

/**
 * @brief Associate a serialNode as a virtual of a master serialNode.
 *
 * @param[in] master - serialNode master
 * @param[in] virtual - serialNode virtual to associate with master
 *
 */
void serialAddVirtualNode(serialNode *master, serialNode *virtual);

/**
 * @brief Remove an associated virtual from a master serialNode.
 *
 * @param[in] master - serialNode master
 * @param[in] virtual - serialNode virtual to remove
 *
 */
void serialRemoveVirtualNode(serialNode *master, serialNode *virtual);

/**
 * @brief Given an initialized serialNode, it will attempt to open
 *        and configure the connection. This function will also create file
 *        events for polling file descriptor events.
 *
 * @param[in] node - serialNode to configure for communication
 */
int serialConnectNode(serialNode *node);

/**
 * @brief Return the node by given node name.
 *
 * @param[in] nodename - Name of the node (device name)
 *
 * @return Pointer to node if found or NULL if not found
 */
serialNode *serialGetNode(const char *nodename);

/**
 * @brief Return the virtual node of master by given node name.
 *
 * @param[in] master - Master node
 * @param[in] nodename - Name of the virtual node (device name)
 *
 * @return Pointer to node if found or NULL if not found
 */
serialNode *serialGetVirtualNode(serialNode *master, const char *nodename);

#endif
