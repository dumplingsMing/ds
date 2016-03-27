#include <stdlib.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>
#include <string>
#include <fstream>
#include <iostream>
#include <set>
#include "router.h"

using namespace std;

/* global variables */
/* router tables */
rtb_t router_table;
ntb_t neighbor_table;
string router_id;
int number;

/* locks */
pthread_mutex_t router_table_lock;
pthread_mutex_t neighbor_table_lock;

void
print_message(msg_entry_t &entry)
{
    cout << string(entry.dst_id) << " " << entry.cost << " " << entry.seq << endl;
}

void  
get_socket_addr(char *ip, int port, struct sockaddr_in& addr)
{
    bzero((char *)&addr, sizeof(struct sockaddr_in));
    addr.sin_family = AF_INET;
    if (ip == NULL)
        addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    else
        addr.sin_addr.s_addr = inet_addr(ip);
    addr.sin_port = htons((unsigned short)port);
}

int
open_socket(char *ip, int port)
{
    /* create a fd for socket */
    int socketfd;
    if ((socketfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
        return -1;

    /* set SO_REUSERADDR to true for immiderate restart sercver */
    int optval = 1;
    if (setsockopt(socketfd, SOL_SOCKET, SO_REUSEADDR, 
		   (const void *)&optval , sizeof(int)) < 0)
	    return -1;

    /* create a socket address and bind it to listen fd */
    struct sockaddr_in addr;
    get_socket_addr(ip, port, addr);
    /* bind the address to receive fd */
    if (bind(socketfd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
        return -1;
    return socketfd;
}

void
read_routers(string filename)
{
    ifstream ift(filename.c_str());

    /* metadata */
    int count;
    string id;
    ift >> count >> id;
    router_id = id;

    int neighbor_port;
    string neighbor_id;
    float neighbor_cost;
    ntb_entry_t entry;

    ntb_t::iterator iter;

    pthread_mutex_lock(&neighbor_table_lock);
    /* read every line */
    for (int i = 0; i < count; i++) {
        ift >> neighbor_id >> neighbor_cost >> neighbor_port;
        iter = neighbor_table.find(neighbor_id);
        entry.cost = neighbor_cost;
        entry.port = neighbor_port;

        /* add a new entry into neighbor table */
        if (iter == neighbor_table.end()) {
            neighbor_table[neighbor_id] = entry;
            rtb_entry_t rtb_entry;
            rtb_entry.nexthop = neighbor_id;
            rtb_entry.cost = neighbor_cost;
            rtb_entry.seq = 0;
            router_table[neighbor_id] = rtb_entry;
        }

        /* update an existing entry */
        else if (iter->second.cost != neighbor_cost) {
            rtb_t::iterator rt_iter = router_table.begin();
            /* also update those in router_table and seq */
            for (;rt_iter != router_table.end(); ++rt_iter) {
                if (rt_iter->second.nexthop == neighbor_id) {
                    rt_iter->second.cost = (neighbor_cost < 0 ? 
                        -1 : rt_iter->second.cost + neighbor_cost - iter->second.cost);
                    rt_iter->second.seq++;
                }
            }
            iter->second.cost = neighbor_cost;
        }
    }
    pthread_mutex_unlock(&neighbor_table_lock);
}

void 
init(string filename)
{
    router_table = map<string, rtb_entry_t>();
    neighbor_table = map<string, ntb_entry_t>();
    
    read_routers(filename);

    ntb_t::iterator iter = neighbor_table.begin();
    rtb_entry_t entry;
    entry.nexthop = router_id;
    entry.cost = 0;
    entry.seq = 0;
    router_table[router_id] = entry;

	pthread_mutex_init(&router_table_lock, NULL);
	pthread_mutex_init(&neighbor_table_lock, NULL);
}

/* print router table as commanded (also for debug) */
bool 
print_router_table()
{
    cout << "## print-out number " << number << endl;
    number++;
    rtb_t::iterator iter = router_table.begin();
    for (; iter != router_table.end(); ++iter) {
        string dst_id = (iter->first);
        string next_hop = (iter->second).nexthop;
        int seq = (iter->second).seq;
        float cost = (iter->second).cost;
        cout << "shortest path to node " << dst_id
                << "(seq# " << seq << "): "
                << "the next hop is " << next_hop
                << " and the cost is " << cost << ", "
                << router_id << " -> " << dst_id << " : "
                << cost << endl; 
    }
}

/* function used in receiver to receive message at a specific port */
int
receive_msg(char *buf, int max, int fd)
{
    /* a blocked receive function for UDP protocol */
    int recv_len;
    //cout << "recvfd: " << fd << endl;
    while ((recv_len = recvfrom(fd, buf, max,
                    0, NULL, NULL)) < 0);
        //cout << "recv_len: " << recv_len << endl;
    //cout << "recv success" << endl;
    //cout << ((msg_t *)buf)->len << endl;
    
    return recv_len;
}

/* function used to update router_table according to received message */
void 
update_router_table(msg_t* pmsgs)
{
    int msgcnt = pmsgs->len;
    //cout << "udpate: " << msgcnt << endl;
    string sender_id = string(pmsgs->id);
    float router_cost = neighbor_table[sender_id].cost;

    /* loop the received message */
    msg_entry_t msg_entry;
    rtb_entry_t rtb_entry;
    int i;
    //cout << "receive: " << sender_id << endl;
    for (i = 0; i < msgcnt; i++) {
        msg_entry = pmsgs->entries[i];
        string dst_id = string(msg_entry.dst_id);

        float cost = router_cost + msg_entry.cost;
        if (router_cost < 0 || msg_entry.cost < 0 )
            cost = -1;
        int seq = msg_entry.seq;

        rtb_entry.nexthop = sender_id;
        rtb_entry.cost = cost;
        rtb_entry.seq = seq;

        //print_message(msg_entry);
        rtb_t::iterator iter = router_table.find(dst_id);
        /* not exist now, add to the router table */
        if (iter == router_table.end()) {
            //cout << ": a" << endl;
            router_table[dst_id] = rtb_entry; 
        }

        /* already exist*/
        /* with a bigger seq number */
        else if (seq > iter->second.seq){
            //cout << ": b" << endl;
            /* dst is current node: cur node is the newer one */
            if (dst_id == router_id)
                iter->second.seq = seq+1;
            /* else, cover current entry */
            else
                iter->second = rtb_entry;
        }

        /* with the same seq number: */
        else if (seq == iter->second.seq && cost > 0
                && (iter->second.cost > cost || iter->second.cost == -1)) {
            iter->second.cost = cost;
            iter->second.nexthop = sender_id;
        }

        /* with a lower seq number: ignore */
    }
}

/* the function of receive side */
void *
start_receiver(void *arg)
{
    int port = *(int *)arg;
    char buf[MAX_BUF];
    /* open fd for receive use */
    int recv_fd;
    while ((recv_fd = open_socket(NULL, port)) < 0)
        cerr << "error when opening receive fd";
    //cout << "port: " << port << ", open recv sock ok" << endl;

    for (;;) {
        /* receive message */
        receive_msg(buf, MAX_BUF, recv_fd);

        /* parse the messages and update router table */
        msg_t *pmsgs = (msg_t *)buf;
		pthread_mutex_lock(&router_table_lock);
        update_router_table(pmsgs);
		pthread_mutex_unlock(&router_table_lock);
    }
    /* close operation, may never reach here */
    close(recv_fd);
    return 0;
}

/* function needed to package current router table into a message struct */
void 
pack_send_messages(msg_t*& msg)
{
    int size = router_table.size();
    msg = (msg_t *)malloc(GET_MSG_SZ(size));

    msg->len = size;
    strcpy(msg->id, router_id.c_str());

    //cout << "send" << endl;
    /* copy all entries */
    int pos = 0;
    rtb_t::iterator iter = router_table.begin();
    for (; iter != router_table.end(); ++iter) {
        msg_entry_t entry;
        strcpy(entry.dst_id, iter->first.c_str());
        entry.cost = iter->second.cost;
        entry.seq = iter->second.seq;
        msg->entries[pos] = entry;
        //print_message(entry);
        ++pos;
    }
}

void 
send_message(msg_t *send_msg, int sendfd)
{
    ntb_t::iterator iter = neighbor_table.begin();

    for(; iter != neighbor_table.end(); ++iter) {
        if (iter->second.cost < 0)
            continue;
        struct sockaddr_in addr;
        get_socket_addr("127.0.0.1", iter->second.port, addr);
        sendto(sendfd, (char *)send_msg, GET_MSG_SZ(send_msg->len),
                0, (struct sockaddr *)&addr, sizeof(addr));
    }
}

/* the sender thread */
void *
start_sender(void *arg) {
    string filename = *(string *)arg;
    int sendfd;
    while ((sendfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
        cerr << "open send socket error: " << sendfd << endl;

    for (;;)  {
        /* update the neighbor table according to input file change */
        ntb_t old_neighbor_table = neighbor_table;
        map<string, float> diff_routers;

		pthread_mutex_lock(&router_table_lock);
        /* handle changed routers */
        read_routers(filename);

        /* print router path as commanded */
        print_router_table();
		pthread_mutex_unlock(&router_table_lock);

        /* package current router table and send */
        msg_t *send_msg;
		pthread_mutex_lock(&router_table_lock);
        pack_send_messages(send_msg);
		pthread_mutex_unlock(&router_table_lock);
        send_message(send_msg, sendfd);

        /* sleep for next send */
        sleep(1);
    }
    close(sendfd);
    return 0;
}
