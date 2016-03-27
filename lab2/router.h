#ifndef ROUTER_H
#define ROUTER_H

#include <string>
#include <map>

#define MAX_BUF 8192
#define MAX_ID_LEN 16

/* the structs of a router table in local router */
typedef struct router_table_line
{
    std::string nexthop;
    float cost;
    int seq;
} rtb_entry_t;

typedef std::map<std::string, rtb_entry_t> rtb_t;

/* structs for router table data sended to other routers */
typedef struct message_line
{
    char dst_id[MAX_ID_LEN];
    float cost;
    int seq;
} msg_entry_t;

typedef struct message
{
    char id[MAX_ID_LEN];
    unsigned int len;
    msg_entry_t entries[0];
} msg_t;

#define GET_MSG_SZ(len) (sizeof(msg_t) + (len)*sizeof(msg_entry_t))

/* the structor for neighbor routers */
typedef struct neighbor_table_entry {
    float cost;
    int port;
} ntb_entry_t;
typedef std::map<std::string, ntb_entry_t> ntb_t;

/* functions used by main thread */
void init(std::string filename);
void* start_receiver(void *arg);
void* start_sender(void *arg);

#endif
