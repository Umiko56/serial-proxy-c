#ifndef __SERIAL_H
#define __SERIAL_H

#define SERIAL_OK 0               /* Everything looks ok */
#define SERIAL_FAIL 1             /* The serial is not working */
#define SERIAL_NAMELEN 40

/* Serial flags */
#define SERIAL_NODE_MASTER 1      /* The node is a master */
#define SERIAL_NODE_VIRTUAL 2     /* The node is a virtual */
#define SERIAL_NODE_FAIL 3        /* The node is believed to be malfunctioning */

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
    char name[SERIAL_NAMELEN];    /* Path to device (/dev/ttyS1) */
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

serialNode *createSerialNode(const char *nodename, int flags);
int serialAddNode(serialNode *node);
void serialDelNode(serialNode *delnode);
void serialWriteHandler(aeEventLoop *el, int fd, void *privdata, int mask);
void serialReadHandler(aeEventLoop *el, int fd, void *privdata, int mask);
int serialNodeAddVirtual(serialNode *master, serialNode *virtual);
int serialNodeRemoveVirtual(serialNode *master, serialNode *virtual);
void serialSetMaster(serialNode *n);
void serialSetNodeAsMaster(serialNode *n);
int serialAcceptHandler(serialNode *node);

#endif
