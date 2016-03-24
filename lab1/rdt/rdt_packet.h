#ifndef _RDT_PACKET_H_
#define _RDT_PACKET_H_

/* ther sender's packet */
/* the really data size of a packet */
#define RDT_HEADERSIZE (sizeof(pkt_size_t)+sizeof(pkt_seq_t)+sizeof(pkt_cksum_t))
#define RDT_DATASIZE (RDT_PKTSIZE - RDT_HEADERSIZE)

/* the type of fields */
#define pkt_size_t char
#define pkt_seq_t int
#define pkt_cksum_t short

/* the positions of the header fields of a packte */
#define RDT_SIZE_POS 0                                  /* packet size, char */
#define RDT_SEQ_POS  (RDT_SIZE_POS+sizeof(pkt_size_t))  /* seq number, int */
#define RDT_CKSUM_POS (RDT_SEQ_POS+sizeof(pkt_seq_t))   /* checksum, short */
#define RDT_DATA_POS (RDT_CKSUM_POS+sizeof(pkt_cksum_t))/* data, other parts of a packet */

/* the receiver's packet */
/* packet status */
#define RDT_ACK 0
#define RDT_NAK 1
#define RDT_ERR 2

/* packet fields */
/* the type of fields */
#define pkt_type_t char

#define RDT_TYPE_POS 0      /* the type of this reply */

/* helper micors to access fields */
#define GET_SIZE(pkt) (*((pkt_size_t *)&((pkt).data[RDT_SIZE_POS])))
#define GET_SEQ(pkt)  (*((pkt_seq_t *)&((pkt).data[RDT_SEQ_POS])))
#define GET_CKSUM(pkt) (*((pkt_cksum_t *)&((pkt).data[RDT_CKSUM_POS])))
#define GET_TYPE(pkt) (*((pkt_type_t *)&((pkt).data[RDT_TYPE_POS])))

#endif
