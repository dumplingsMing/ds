/*
 * FILE: rdt_sender.cc
 * DESCRIPTION: Reliable data transfer sender.
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
#include <map>
#include <iostream>

#include "rdt_struct.h"
#include "rdt_packet.h"
#include "rdt_sender.h"

#define TIMEOUT 0.3

using namespace std;

int next_frame_to_send;
int ack_expected;

map<int, packet> packets;

/* imply my own timer */
// the struct
typedef struct my_timer
{
    int seq;
    double start_time;
} Mytimer_t;
list<Mytimer_t> timers;

void print_buffer()
{
    map<int, struct packet>::iterator iter;
    for (iter = packets.begin(); iter != packets.end(); iter++)
        cout << iter->first << "\t" << GET_SEQ(iter->second) << endl;
}

void print_timers()
{
    list<struct my_timer>::iterator iter = timers.begin();
    for (;iter != timers.end();iter++)
        cout << iter->seq << " ";
    cout << endl;
}
// start a timer with seq number
void timer_start(int seq)
{
    Mytimer_t timer;
    timer.seq = seq;
    timer.start_time = GetSimulationTime();
    if (timers.size() == 0) {
        Sender_StartTimer(TIMEOUT);
    }
    timers.push_back(timer);
}

// stop a timer with a seq number
void timer_stop(int seq)
{
    list<struct my_timer>::iterator iter = timers.begin();
    for (; iter != timers.end(); iter++) {
        if (iter->seq == seq) {
            timers.erase(iter);
            iter--;
        }
    }
    if (timers.size() > 0)
        Sender_StartTimer(timers.front().start_time + TIMEOUT - GetSimulationTime());
    else
        Sender_StopTimer();
}

// a timer with seq number is time out
void timer_timeout(int seq)
{
    //cout << "timeout:" << seq << " in" << endl;
    //cout << "sender: send" << GET_SEQ(packets[seq]) << endl;
    //cout << "sender: send" << GET_SEQ(packets[seq]) << ", " << (int)GET_SIZE(packets[seq]) << endl;
    Sender_ToLowerLayer(&packets[seq]);
    timer_start(seq);
    //cout << "timeout:" << seq << " out" << endl;
}

/* sender initialization, called once at the very beginning */
void Sender_Init()
{
    fprintf(stdout, "At %.2fs: sender initializing ...\n", GetSimulationTime());
    next_frame_to_send = 0;
    ack_expected = 0;
    packets = map<int, struct packet>();
}

/* sender finalization, called once at the very end.
   you may find that you don't need it, in which case you can leave it blank.
   in certain cases, you might want to take this opportunity to release some 
   memory you allocated in Sender_init(). */
void Sender_Final()
{
    fprintf(stdout, "At %.2fs: sender finalizing ...\n", GetSimulationTime());
}

pkt_cksum_t Sender_CalCksum(struct packet pkt)
{
    pkt_cksum_t cksum = 0;
    for (size_t i = 0; i < RDT_CKSUM_POS; i++)
        cksum += pkt.data[i];
    for (size_t i = RDT_DATA_POS; i < RDT_PKTSIZE; i++) {
        cksum += pkt.data[i];
    }
    return cksum;
}

/* event handler, called when a message is passed from the upper layer at the 
   sender */
void Sender_FromUpperLayer(struct message *msg)
{
    //cout << "Sender from upper layer " << msg->size << endl;
    /* maximum payload size */
    int maxpayload_size = RDT_DATASIZE;

    /* split the message if it is too big */

    /* reuse the same packet data structure */
    struct packet pkt;

    /* the cursor always points to the first unsent byte in the message */
    int cursor = 0;
    pkt_seq_t cur_frame_seq;
    while (msg->size-cursor > maxpayload_size) {
        
        /* current frame */
        cur_frame_seq = next_frame_to_send++;

        /* fill in the packet */
        GET_SIZE(pkt) = maxpayload_size;
        GET_SEQ(pkt) = cur_frame_seq;
        memcpy(pkt.data+RDT_DATA_POS, msg->data+cursor, maxpayload_size);
        GET_CKSUM(pkt) = Sender_CalCksum(pkt);

        /* send it out through the lower layer */
        //cout << "sender: send" << GET_SEQ(pkt) << ", " << (int)GET_SIZE(pkt) << endl;
        //cout << "sender: send" << GET_SEQ(pkt) << ", " << GET_SIZE(pkt) << endl;
        Sender_ToLowerLayer(&pkt);
        /* save the packet into the buffer for later resend */
        packets[cur_frame_seq] = pkt;
        /* start timer for timeout */
        timer_start(cur_frame_seq);

        /* move the cursor */
        cursor += maxpayload_size;
        //print_buffer();
    }

    /* send out the last packet(same as before) */
    if (msg->size > cursor) {
        int size = msg->size - cursor;

        /* update current frame */
        cur_frame_seq = next_frame_to_send++;

        /* fill in the packet */
        GET_SIZE(pkt) = size;
        GET_SEQ(pkt) = cur_frame_seq;
        memcpy(pkt.data+RDT_DATA_POS, msg->data+cursor, size);
        GET_CKSUM(pkt) = Sender_CalCksum(pkt);

        /* send it out through the lower layer */
        //cout << "sender: send" << GET_SEQ(pkt) << ", " << GET_SIZE(pkt) << endl;
        //cout << "sender: send" << GET_SEQ(pkt) << ", " << (int)GET_SIZE(pkt) << endl;
        Sender_ToLowerLayer(&pkt);
        /* save the packet into the buffer for later resend */
        packets[cur_frame_seq] = pkt;
        /* start timer for timeout */
        timer_start(cur_frame_seq);
        //print_buffer();
    }
}

/* event handler, called when a packet is passed from the lower layer at the 
   sender */
void Sender_FromLowerLayer(struct packet *pkt)
{
    /* ignore bad packets */
    if (Sender_CalCksum(*pkt) != GET_CKSUM(*pkt))
        return;

    /* parse the packet */
    pkt_type_t type = GET_TYPE(*pkt);
    pkt_seq_t seq = GET_SEQ(*pkt);
    //cout << "sender from lower layer: " << seq << endl;
    if (!packets.count(seq))
        return;
    struct packet *sender_pkt = &packets[seq];

    /* handle different types of reply */
    switch (type) {
        case RDT_ACK:
            /* stop timer and remove it from packets */
            //cout << "sender ack " << seq << endl;
            timer_stop(seq);
            packets.erase(seq);
            break;
        case RDT_NAK: case RDT_ERR:
            /* resend the packet and restart timer */
            timer_stop(seq);
            //cout << "sender nak " << seq << endl;
            //cout << "sender: send" << GET_SEQ(*pkt) << ", " << (int)GET_SIZE(*pkt) << endl;
            Sender_ToLowerLayer(sender_pkt);
            timer_start(seq);
            break;
    }
}

/* event handler, called when the timer expires */
void Sender_Timeout()
{
    /* handle the multi soft timers */
    //cout << "in sender timeout" << endl;
    
    /* no timer exists, might be something wrong */
    if (timers.size() == 0) {
        printf("something wrong with timer");
        return;
    }

    /* update timer queue*/
    Mytimer_t cur_timer = timers.front();
    timers.pop_front();
    /* start the new timer */
    if (timers.size() > 0)
        Sender_StartTimer(timers.front().start_time + TIMEOUT - GetSimulationTime());

    /* call timeout of current timer */
    timer_timeout(cur_timer.seq);
}
