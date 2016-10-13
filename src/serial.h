#ifndef SERIAL_H
#define SERIAL_H

#include <linux/limits.h>

/* Serial state */
#define SERIAL_OK      (0)  /* Everything looks ok */
#define SERIAL_FAIL    (1)  /* The serial device is not working */

/* Serial flags */
#define SERIAL_NODE_MASTER  (1) /* The node is a master */
#define SERIAL_NODE_VIRTUAL (2) /* The node is a virtual */
#define SERIAL_NODE_FAIL    (3) /* The node is believed to be malfunctioning */

#define nodeIsMaster(n) ((n)->flags & SERIAL_NODE_MASTER)
#define nodeIsVirtual(n) ((n)->flags & SERIAL_NODE_VIRTUAL)
#define nodeFailed(n) ((n)->flags & SERIAL_NODE_FAIL)

struct serialNode;

typedef struct serialLink {
    int fd;                       /* Serial file descriptor */
    int sfd;                      /* Slave serial file descriptor */
    char recvbuf[BUFSIZ];         /* Receive buffer */
    int recvbuflen;               /* Receive buffer size */
    struct serialNode *node;      /* Node related to this link if any, or NULL */
} serialLink;

typedef struct serialNode {
    char name[PATH_MAX];          /* Path to device (/dev/ttyS1) */
    int flags;
    struct serialNode **virtuals; /* Pointers to virtuals */
    struct serialNode *virtualof; /* Pointer to master (can be null) */
    int numvirtuals;              /* Number of virtuals, if this is a master */
    int baudrate;                 /* Baudrate of device */
    serialLink *link;             /* rs232 link with this node */
} serialNode;

typedef struct serialState {
    struct serialNode **nodes;    /* Pointer to masters */
    int size;                     /* Number of masters */
} serialState;

/**
 * @brief Allocate memory and initialize a new serial node.
 *
 * @param[in] nodename Serial device path
 * @param[in] flags Serial device responsibility
 *
 * @return Pointer to newly allocated serialNode object
 */
serialNode *serialCreateNode(const char *nodename, int flags);

/**
 * @brief Add serialNode to list and increment serial size. If the serialNode
 *        already exists in the list then this function does nothing.
 *
 * @param[in] node serialNode to add
 *
 * @return C_OK if serialNode was not already in the list and added, C_ERR if
 *         serialNode already exists.
 */
int serialAddNode(serialNode *node);

/**
 * @brief Remove serialNode from list and decrement serial size.
 *
 * @param[in] delnode serialNode to remove
 *
 * @return C_OK if serialNode was found and removed, C_ERR if serialNode was
 *         not found.
 */
int serialDelNode(serialNode *delnode);

/**
 * @brief Write handle callback when data is ready to be written.
 *
 * @param[in] el Pointer to event loop
 * @param[in] fd File descriptor of serialNode
 * @param[in] privdata Pointer to serialLink
 * @param[in] int mask Event flags
 */
void serialWriteHandler(aeEventLoop *el, int fd, void *privdata, int mask);

/**
 * @brief Read handle callback when data is ready to be read.
 *
 * @param[in] el Pointer to event loop
 * @param[in] fd File descriptor of serialNode
 * @param[in] privdata Pointer to serialLink
 * @param[in] int mask Event flags
 */
void serialReadHandler(aeEventLoop *el, int fd, void *privdata, int mask);

/**
 * @brief Associate a serialNode as a virtual of a master serialNode.
 *
 * @param[in] master serialNode master
 * @param[in] virtual serialNode virtual to associate with master
 *
 * @return C_OK if virtual was added to master, C_ERR if passed in master is
 *         not a master or if virtual already exists.
 */
int serialNodeAddVirtual(serialNode *master, serialNode *virtual);

/**
 * @brief Remove an associated virtual from a master serialNode.
 *
 * @param[in] master serialNode master
 * @param[in] virtual serialNode virtual to remove
 *
 * @return C_OK if virtual was removed from master, C_ERR if passed in master
 *         is not a master or if virtual does not exist.
 */
int serialNodeRemoveVirtual(serialNode *master, serialNode *virtual);

/**
 * @brief Set serialNode as master device. If the serialNode is already a
 *        master this method does nothing. If the serialNode is a virtual
 *        device of another serialNode, it will unlink itself from that
 *        serialNode.
 *
 * @param[in] n Pointer to serialNode
 */
void serialSetNodeAsMaster(serialNode *n);

/**
 * @brief Given an initialized serialNode, it will attempt to open
 *        and configure the connection. This function will also create file
 *        events for polling file descriptor events.
 *
 * @param[in] node serialNode to configure for communication
 *
 * @return C_OK if communication configured and opened, C_ERR otherwise
 */
int serialConnect(serialNode *node);

#endif
