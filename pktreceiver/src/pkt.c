#include "rte_mbuf.h"

#include "pkt.h"

void pkt_print(struct rte_mbuf *pkt) {
    uint32_t len = rte_pktmbuf_data_len(pkt);
    unsigned char *ptr = rte_pktmbuf_mtod(pkt, unsigned char *);
    uint32_t j = 0;
    uint32_t line = 0;

    printf("Received packet of length: %u\n", len);
    for (j = 0; j < len; ++j) {
        if (j % 12 == 0) printf("\n%04d: ", (line+=12));
        else if (j % 6 == 0) printf("- ");
        printf("%02X ", ptr[j]);
    }
    printf("\n");
}

void pkt_print_src_dst_ip(struct rte_mbuf *pkt) {
    unsigned char *ptr = rte_pktmbuf_mtod(pkt, unsigned char *);
    uint16_t *ethtype = (uint16_t*)(ptr + 12);
    unsigned char *src = ptr + 26;
    unsigned char *dst = ptr + 30;

    printf("%04X, %u.%u.%u.%u -> %u.%u.%u.%u\n", *ethtype,
            *(src), *(src+1), *(src+2), *(src+3), 
            *(dst), *(dst+1), *(dst+2), *(dst+3));
}

inline uint16_t
pkt_ethtype(struct rte_mbuf *pkt) {
    unsigned char *ptr = rte_pktmbuf_mtod(pkt, unsigned char *);
    uint16_t *ethtype = (uint16_t*)(ptr + 12);
    return (*ethtype);
}
