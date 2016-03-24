/*
 * FILE: rdt_receiver.cc
 * DESCRIPTION: Reliable data transfer receiver.
 * NOTE: This implementation assumes there is no packet loss, corruption, or 
 *       reordering.  You will need to enhance it to deal with all these 
 *       situations.  In this implementation, the packet format is laid out as 
 *       the following:
 *       
 *       |<-  1 byte  ->|<-             the rest            ->|
 *       | payload size |<-             payload             ->|
 *
 *       The first byte of each packet indicates the size of the payload
 *       (excluding this single-byte header)
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <list>
#include <set>
#include <iostream>

#include "rdt_struct.h"
#include "rdt_receiver.h"
#include "rdt_packet.h"

using namespace std;

/* the list of buffered packets, the first one is the one
 * after the first missing packet */
list<struct packet>buffer;

set<pkt_seq_t> missing_seqs;
pkt_seq_t next_pkt_expected;

/* receiver initialization, called once at the very beginning */
void Receiver_Init()
{
    fprintf(stdout, "At %.2fs: receiver initializing ...\n", GetSimulationTime());
    buffer = list<packet>();
    next_pkt_expected = 0;
    missing_seqs = set<pkt_seq_t>();
}

void print_buffer_1()
{
    list<packet>::iterator iter = buffer.begin();
    for (; iter != buffer.end(); iter++)
        cout << GET_SEQ(*iter) << " ";
    cout << endl;
}


/* receiver finalization, called once at the very end.
   you may find that you don't need it, in which case you can leave it blank.
   in certain cases, you might want to use this opportunity to release some 
   memory you allocated in Receiver_init(). */
void Receiver_Final()
{
    fprintf(stdout, "At %.2fs: receiver finalizing ...\n", GetSimulationTime());
    //print_buffer_1();
    set<pkt_seq_t>::iterator iter = missing_seqs.begin();
    /* 
    for (; iter != missing_seqs.end(); iter++)
        cout << *iter << " ";
    cout << endl;
    */

    //print_buffer_1();
}

/* insert a packet into the buffer at the right place */
void insert_packet(struct packet pkt)
{
    int seq = GET_SEQ(pkt);
    list<struct packet>::iterator iter;
    for(iter = buffer.begin(); iter != buffer.end() && GET_SEQ(*iter) < seq ; ++iter); 
    if (iter == buffer.end())
        buffer.push_back(pkt);
    else
        buffer.insert(iter, pkt);
}

pkt_cksum_t cal_cksum(struct packet pkt)
{
    pkt_cksum_t cksum = 0;
    for (size_t i = 0; i < RDT_CKSUM_POS; i++)
        cksum += pkt.data[i];
    for (size_t i = RDT_DATA_POS; i < RDT_PKTSIZE; i++)
        cksum += pkt.data[i];
    return cksum;
}

void receive_message(struct packet pkt)
{
    /* construct a message and deliver to the upper layer */
    struct message *msg = (struct message*) malloc(sizeof(struct message));
    ASSERT(msg!=NULL);

    msg->size = GET_SIZE(pkt);

    msg->data = (char*) malloc(msg->size);
    ASSERT(msg->data!=NULL);
    memcpy(msg->data, pkt.data+RDT_DATA_POS, msg->size);
    Receiver_ToUpperLayer(msg);

    /* don't forget to free the space */
    if (msg->data!=NULL) free(msg->data);
    if (msg!=NULL) free(msg);
}

/* try to commit buffered packets if without missing */
void try_commit()
{
    list<packet>::iterator iter;
    //cout << "try commit" << endl;
    /* commit all */
    if (missing_seqs.size() == 0) {
        for (iter = buffer.begin(); iter != buffer.end(); ++iter)
            receive_message(*iter);
    }

    /* commit until next not missed packet */
    else {
        pkt_seq_t first_missed = *(missing_seqs.begin());
        //cout << first_missed << endl;
        for (iter = buffer.begin(); iter != buffer.end() 
                && GET_SEQ(*iter) <= first_missed; ++iter) {
            //cout << "commit seq: " << GET_SEQ(*iter) << endl;
            receive_message(*iter);
        }
    }
    buffer.erase(buffer.begin(), iter);
}

/* send a reply packet of given type and seq */
void send_reply(pkt_type_t type, pkt_seq_t seq)
{
    struct packet pkt;
    GET_TYPE(pkt) = type;
    GET_SEQ(pkt) = seq;
    /* data for sender cksum check */
    for (size_t i = RDT_DATA_POS; i < RDT_PKTSIZE; i++)
        pkt.data[i] = (char)i;
    GET_CKSUM(pkt) = cal_cksum(pkt);
    //cout << "receiver send: " << GET_SEQ(pkt) << endl;
    Receiver_ToLowerLayer(&pkt);
}

/* event handler, called when a packet is passed from the lower layer at the 
   receiver */
/* who free pkt pointer? */
void Receiver_FromLowerLayer(struct packet *pkt)
{
    /* get the seq number */
    pkt_seq_t seq = GET_SEQ(*pkt);
    //cout << "receiver from lower layer: " << seq << ", "<< GET_CKSUM(*pkt) <<endl;

    /* check the cksum first, if corropted, abort the packet and send err */
    if (cal_cksum(*pkt) != GET_CKSUM(*pkt) || seq - next_pkt_expected > 10) {
        return;
    }

    /* receive the packet, send ack */
    send_reply(RDT_ACK, seq);

    /* the normal path */
    if (seq == next_pkt_expected) {
        next_pkt_expected++;
        if (missing_seqs.size() == 0)
            receive_message(*pkt);
        else {
            //cout << "push back" << endl;
            buffer.push_back(*pkt);
            //print_buffer_1();
        }
        return;
    }

    /* packet received is not expected one */
    /* if missed some packets */
    if (seq > next_pkt_expected) {
        /* recorded the missing packets and send nak*/
        for (pkt_seq_t i = next_pkt_expected; i < seq; i++) {
            missing_seqs.insert(i);
            send_reply(RDT_NAK, i);
        }
        /* buffer current packet and update expect number */
        buffer.push_back(*pkt);
        next_pkt_expected = seq + 1; 
    }
    else {
        set<pkt_seq_t>::iterator iter = missing_seqs.find(seq);
        /* received a packet not missing, return directly */
        if (iter == missing_seqs.end())
            return;
        /* else, update buffer and missing_seqs */
        insert_packet(*pkt);
        //cout << "after insert" << endl;
        //print_buffer_1();
        missing_seqs.erase(iter);

        /* try to commit good packets */
        try_commit();
    }
}
