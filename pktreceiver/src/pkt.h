#ifndef _PKT_H_
#define _PKT_H_

/* Print the content of an rte_mbuf packet in a nice readable format (similar
 * to tcpdump */
void pkt_print(struct rte_mbuf *);
void pkt_print_src_dst_ip(struct rte_mbuf *);
inline uint16_t pkt_ethtype(struct rte_mbuf *);

#endif  // _PKT_H_
