/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2016 Omid Alipourfard <g@omid.io>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

#include <sys/types.h>  
#include <sys/socket.h>  
#include <netinet/in.h>  
#include <arpa/inet.h>  


#include "rte_common.h"
#include "rte_cycles.h"
#include "rte_eal.h"
#include "rte_ethdev.h"
#include "rte_errno.h"
#include "rte_mempool.h"


#include "common.h"
#include "debug.h"
#include "memory.h"
#include "module.h"

#include "net.h"

static struct rte_eth_conf port_conf = {
	.rxmode = {
		.split_hdr_size = 0,
		.header_split   = 0, /**< Header Split disabled */
		.hw_ip_checksum = 0, /**< IP checksum offload disabled */
		.hw_vlan_filter = 0, /**< VLAN filtering disabled */
		.jumbo_frame    = 0, /**< Jumbo Frame Support disabled */
		.hw_strip_crc   = 0, /**< CRC stripped by hardware */
        .mq_mode        = ETH_MQ_RX_RSS,
	},
    .rx_adv_conf = {
        .rss_conf = {
            //.rss_hf = ETH_RSS_TCP | ETH_RSS_UDP
            .rss_hf = ETH_RSS_IP,
        },
    },
	.txmode = {
		.mq_mode = ETH_MQ_TX_NONE,
	},
};

struct mbuf_rx_table {
    struct rte_mbuf     *rx_bufs[MAX_RX_BURST];
    uint32_t             rx_len;
};

struct mbuf_tx_table {
    struct rte_mbuf     *tx_bufs[MAX_PKT_BURST];
    uint32_t             tx_len;
};

struct RxQueue {
    uint16_t core_id;  /* Core that takes on processing the packets from this
                          queue */
    uint16_t port_id;  /* Port id of this queue */
    uint16_t queue_id; /* Queue id */
    struct mbuf_rx_table table;
};

struct TxQueue {
    uint16_t core_id;  /* Core that takes on processing the packets from this
                          queue */
    uint16_t port_id;  /* Port id of this queue */
    uint16_t queue_id; /* Queue id */
    struct mbuf_tx_table table;
};

struct PacketStats {
    uint64_t rx_packets;
    uint64_t rx_time;
    uint64_t rx_loop_count;
    uint64_t rx_histogram[HIST_SIZE];
};

struct Port {
    uint16_t    port_id;
    uint16_t    socket_id;

    uint8_t    nrxq; /* Num RX queues */
    uint8_t    ntxq; /* Num TX queues */

    uint16_t    rx_burst_size; /* Burst size of RX queue */
    uint16_t    tx_burst_size; /* Burst size of TX queue */

    struct rte_eth_conf *conf;  /* Ethernet config */
    struct rte_mempool  *pool[MAX_QUEUES];  /* DPDK pktmbuf pool */

    struct TxQueue tx_queues[MAX_QUEUES];
    struct RxQueue rx_queues[MAX_QUEUES];

    struct rte_eth_stats port_stats;      /* Port statistics */
    uint64_t             port_stats_time; /* Time when the port stat snapshot was taken */
    struct ether_addr    port_mac;

    ModulePtr rx_modules[MAX_MODULES];
    ModulePtr tx_modules[MAX_MODULES];

    uint8_t nrx_modules;
    uint8_t ntx_modules;
    uint8_t ticked;

    struct PacketStats pkt_stats;
};

static int
port_configure(PortPtr port) {
    RTE_LOG(INFO, APP,
        "Creating port with %d RX rings and %d TX rings\n", port->nrxq, port->ntxq);
    return rte_eth_dev_configure(port->port_id,
            port->nrxq, port->ntxq, port->conf);
}

static int
port_setup_mempool(PortPtr port) {
    unsigned i;
    for (i = 0; i < MAX_QUEUES; ++i) {
        char mempool_name[32] = {0};
        snprintf(mempool_name, 31, "port-%d-%d", port->port_id, i);

        /* Automatically garbage collected by EAL (?) */
        port->pool[i] = rte_pktmbuf_pool_create(
                mempool_name, PORT_PKTMBUF_POOL_SIZE,
                PORT_PKTMBUF_POOL_CACHE_SIZE, 0,
                RTE_MBUF_DEFAULT_BUF_SIZE, port->socket_id);

        if (!port->pool[i])
            return ENOMEM;
    }

    return 0;
}

static int
port_setup_core_queue_assignment(PortPtr port,
        struct CoreQueuePair *ass, uint32_t len) {
    unsigned i = 0;

    if (len > MAX_QUEUES) {
        return -1;
    }

    for (i = 0; i < len; ++i){
        port->tx_queues[i].core_id = ass[i].core_id;
        port->tx_queues[i].port_id = port->port_id;
        port->tx_queues[i].queue_id = i;

        port->rx_queues[i].core_id = ass[i].core_id;
        port->rx_queues[i].port_id = port->port_id;
        port->rx_queues[i].queue_id = i;
    }

    return 0;
}

static int
port_setup_rx_queues(PortPtr port) {
    int ret = 0; uint32_t rxq;
    for (rxq = 0; rxq < port->nrxq; ++rxq ) {
        ret = rte_eth_rx_queue_setup(
                port->port_id, rxq, RX_DESC_DEFAULT,
                port->socket_id, NULL, port->pool[rxq]);

        if (ret != 0)
            return ret;
    }
    return 0;
}

static int
port_setup_tx_queues(PortPtr port) {
    int ret = 0; uint32_t txq;
    for (txq = 0; txq < port->ntxq; ++txq ) {
        ret = rte_eth_tx_queue_setup(
                port->port_id, txq, TX_DESC_DEFAULT,
                port->socket_id, NULL);

        if (ret)
            return ret;
    }
    return 0;
}

int
port_start(PortPtr port) {
    int ret = 0;
    ret = rte_eth_dev_start(port->port_id);
    if (ret < 0) return ret;

    rte_eth_promiscuous_enable(port->port_id);
    return 0;
}

PortPtr
port_create(uint32_t port_id, struct CoreQueuePair *pairs, uint16_t len) {
    PortPtr port = NULL;

    int ret = mem_alloc(sizeof(struct Port), NULL, (void **)&port);
    if (ret) return 0;

    /* Set all the defaults to zero */
    memset(port, 0, sizeof(struct Port));

    port->port_id = port_id;
    port->socket_id = rte_eth_dev_socket_id(port_id);

    port->conf = &port_conf;
    port->nrxq = len;
    port->ntxq = len;

    port->rx_burst_size = MAX_PKT_BURST;
    port->tx_burst_size = MAX_PKT_BURST;

    ret = port_setup_core_queue_assignment(port, pairs, len);
    if (ret < 0) return 0;

    ret = port_configure(port);
    if (ret < 0) return 0;

    ret = port_setup_mempool(port);
    if (ret < 0) return 0;

    ret = port_setup_rx_queues(port);
    if (ret < 0) return 0;

    ret = port_setup_tx_queues(port);
    if (ret < 0) return 0;

    /* Get the port MAC address */
    rte_eth_macaddr_get(port->port_id, &port->port_mac);

    return port;
}

int
port_delete(PortPtr __attribute__((unused)) port) {
    /* Don't really need to do anything because the mem_alloc
     * takes care of it */
    return 0;
}

static void
port_send_burst(PortPtr port, uint16_t txq) {
    struct mbuf_tx_table *m_table = &port->tx_queues[txq].table;
    struct rte_mbuf   **mbufs   = m_table->tx_bufs;
    int idx = 0, nb_tx = 0;
    int port_id = port->port_id;

    if (m_table->tx_len > 0) {
        int len = m_table->tx_len;

        /* send all the pending packets out */
        while (len != 0) {
            nb_tx = rte_eth_tx_burst(port_id, txq, mbufs + idx, len);
            idx += nb_tx;
            len -= nb_tx;
        }

        m_table->tx_len = 0;
    }
}

static uint16_t
rx_queues_for_lcore(PortPtr port, struct RxQueue **rx_queues) {
    uint8_t i = 0, ptr = 0;
    unsigned core_id = rte_lcore_id();

    for (i = 0; i < port->nrxq; ++i) {
        if (port->rx_queues[i].core_id == core_id) {
            rx_queues[ptr++] = &port->rx_queues[i];

            /* Warn if the core and queue are not in the same NUMA domain */
            if (port->socket_id != rte_lcore_to_socket_id(core_id)) {
                RTE_LOG(WARNING, APP,
                        "Port and socket not on the same"
                        " core (port: %d, core: %d)\n",
                        port->port_id, core_id);
            }

        }
    }

    return ptr;
}

static uint16_t
tx_queues_for_lcore(PortPtr port, struct TxQueue **tx_queues) {
    uint8_t i = 0, ptr = 0;
    unsigned core_id = rte_lcore_id();

    for (i = 0; i < port->ntxq; ++i) {
        if (port->tx_queues[i].core_id == core_id) { 
            tx_queues[ptr++] = &port->tx_queues[i];

            /* Warn if the core and queue are not in the same NUMA domain */
            if (port->socket_id != rte_lcore_to_socket_id(core_id)) {
                RTE_LOG(WARNING, APP,
                        "Port and socket not on the same"
                        " core (port: %d, core: %d)\n",
                        port->port_id, core_id);
            }
        }
    }

    return ptr;
}

void
port_loop(PortPtr port, LoopRxFuncPtr rx_func, LoopIdleFuncPtr idle_func) {
    struct RxQueue *rx_queues[MAX_QUEUES];
    struct TxQueue *tx_queues[MAX_QUEUES];

    uint8_t nrxq = rx_queues_for_lcore(port, rx_queues);
    uint8_t ntxq = tx_queues_for_lcore(port, tx_queues);

    uint8_t rxq = 0, nb_rx = 0, port_id = port->port_id, txq = 0;
    uint16_t burst_size = port->rx_burst_size;

    struct PacketStats *pkt_stats = &port->pkt_stats;
    
    uint64_t rx_diff;
    uint16_t rx_bucket = 0;

    if (nrxq == 0 || ntxq == 0) {
        printf("Ignoring port ...\n");
        return;
    }

    printf("Running function on lcore: %d\n", rte_lcore_id());
    while (1) {
        /* Consume Receive Queues */
        for (rxq = 0; rxq < nrxq; ++rxq) {
            struct RxQueue *rx_queue = rx_queues[rxq];
            
            struct mbuf_rx_table *table = &rx_queue->table;
            struct rte_mbuf **pkts = table->rx_bufs + table->rx_len;

            /* Get a burst of packets */
            nb_rx = rte_eth_rx_burst(port_id,
                    rx_queue->queue_id,
                    pkts, burst_size);
            table->rx_len += nb_rx;

            /* Run the network function and count the cycles */

            if (table->rx_len >= MAX_RX_BURST/2) {
                /* Time the function */
                rx_diff = rte_get_tsc_cycles();
                rx_func(port, rx_queue->queue_id,
                        table->rx_bufs, table->rx_len); 
                rx_diff = rte_get_tsc_cycles() - rx_diff;

                // printf("%llu\n", rx_diff);

                /* Find the bucket */
                rx_bucket = rx_diff / (HIST_BUCKET_SIZE * table->rx_len);
                rx_bucket = (rx_bucket >= HIST_SIZE) ? HIST_SIZE - 1 : rx_bucket;

                /* Update the pkt_stats */
                pkt_stats->rx_packets += table->rx_len;
                pkt_stats->rx_time += rx_diff;
                pkt_stats->rx_histogram[rx_bucket] += table->rx_len;
                pkt_stats->rx_loop_count += 1;
                table->rx_len = 0;
            }
        }


        /* Drain Transmit Queues */
        for (txq = 0; txq < ntxq; ++txq) {
            struct TxQueue *tx_queue = tx_queues[txq];
            port_send_burst(port, tx_queue->queue_id);
        }

        idle_func(port);
    }
}

int
port_send_packet(PortPtr port, uint16_t txq, struct rte_mbuf *pkt) {
    struct mbuf_tx_table *m_table = &port->tx_queues[txq].table;
    struct rte_mbuf   **mbufs   = m_table->tx_bufs;

    int len = m_table->tx_len;

    /* Send a burst of packets when the local TX queue is full */
    if (unlikely(len == MAX_PKT_BURST)) {
        port_send_burst(port, txq);
        len = 0;
    }

    mbufs[len] = pkt;
    len++;
    m_table->tx_len = len;
    return 0;
}

inline uint32_t
port_id(PortPtr port) {return port->port_id; };

inline uint32_t
port_socket_id(PortPtr port) {return port->socket_id; };

inline uint32_t
port_queue_core_id(PortPtr port, uint8_t queue) { return port->tx_queues[queue].core_id; };

void
port_print_mac(PortPtr port) {
    struct ether_addr *mac = &port->port_mac;

    printf("Port (MAC: %d): %02X:%02X:%02X:%02X:%02X:%02X\n",
            port->port_id,
            mac->addr_bytes[0],
            mac->addr_bytes[1],
            mac->addr_bytes[2],
            mac->addr_bytes[3],
            mac->addr_bytes[4],
            mac->addr_bytes[5]);
}

static uint32_t
rx_percentile(uint64_t *rx_histogram, uint64_t rx_packets, double percentile) {
    double packet_count = (rx_packets * percentile)/ 100.0f;
    uint32_t i = 0;
    uint64_t total_packet = 0;
    for (i = 0; i < HIST_SIZE; ++i) {
        total_packet += rx_histogram[i];

        if (total_packet > packet_count)
            return (i) * HIST_BUCKET_SIZE;
    }

    return HIST_SIZE * HIST_BUCKET_SIZE;
}

static double
rx_average(uint64_t *rx_histogram, uint64_t rx_packets) {
    uint32_t i = 0;
    double total_packet = 0;
    for (i = 0; i < HIST_SIZE; ++i) {
        total_packet += (rx_histogram[i] * i * HIST_BUCKET_SIZE);
    }

    return total_packet/rx_packets + HIST_BUCKET_SIZE/2;
}

int last_miss = 0, now_miss = 1 << 30;

void
port_print_stats(PortPtr port, int client_sockfd) {
    struct rte_eth_stats *port_stats = &port->port_stats;

    uint64_t l_time     = port->port_stats_time;
    uint64_t l_opackets = port_stats->opackets;
    uint64_t l_ipackets = port_stats->ipackets;

    rte_eth_stats_get(port->port_id, port_stats);
    port->port_stats_time = rte_get_tsc_cycles();

    uint64_t l_hertz = rte_get_tsc_hz();
    double elapsed_time_s = (double)(port->port_stats_time - l_time) / (l_hertz);

    double o_rate = (port_stats->opackets - l_opackets) / (elapsed_time_s);
    double i_rate = (port_stats->ipackets - l_ipackets) / (elapsed_time_s);

    struct PacketStats *pkt_stats = &port->pkt_stats;
    uint64_t *rx_histogram = pkt_stats->rx_histogram;
    uint64_t rx_packets = pkt_stats->rx_packets;

    printf(
".------------------------------------------------------------------.\n\
| Port (%1d) - MAC [%02X:%02X:%02X:%02X:%2X:%02X]                               |\n\
|------------------------------------------------------------------|\n\
| Out: %15" PRIu64 " | Rate: %10.0f | Error: %15" PRIu64 " |\n\
|------------------------------------------------------------------|\n\
| In : %15" PRIu64 " | Rate: %10.0f | Missed: %14" PRIu64 " | Error: %15" PRIu64 " |\n\
|------------------------------------------------------------------|\n\
| Avg burst size   : %15.4f                               |\n\
|------------------------------------------------------------------|\n\
| Cycles per packet : %15.0f                              |\n\
| 99.9999 Percentile: %15.4f                              |\n\
| 99.999  Percentile: %15.4f                              |\n\
| 99.99   Percentile: %15.4f                              |\n\
| 99.9    Percentile: %15.4f                              |\n\
| 99.0    Percentile: %15.4f                              |\n\
| 95.0    Percentile: %15.4f                              |\n\
| 90.0    Percentile: %15.4f                              |\n\
| 50.0    Percentile: %15.4f                              |\n\
| Average Cycles pp.: %15.4f                              |\n\
*------------------------------------------------------------------*\n",
            port->port_id,
            port->port_mac.addr_bytes[0], port->port_mac.addr_bytes[1],
            port->port_mac.addr_bytes[2], port->port_mac.addr_bytes[3],
            port->port_mac.addr_bytes[4], port->port_mac.addr_bytes[5],
            port_stats->opackets, o_rate, port_stats->oerrors,
            port_stats->ipackets, i_rate, port_stats->imissed, port_stats->ierrors,
            (double)(pkt_stats->rx_packets)/(double)(pkt_stats->rx_loop_count),
            (double)(pkt_stats->rx_time)/(double)(pkt_stats->rx_packets),
            (double)rx_percentile(rx_histogram, rx_packets, 99.9999),
            (double)rx_percentile(rx_histogram, rx_packets, 99.999),
            (double)rx_percentile(rx_histogram, rx_packets, 99.99),
            (double)rx_percentile(rx_histogram, rx_packets, 99.9),
            (double)rx_percentile(rx_histogram, rx_packets, 99.0),
            (double)rx_percentile(rx_histogram, rx_packets, 95.0),
            (double)rx_percentile(rx_histogram, rx_packets, 90.0),
            (double)rx_percentile(rx_histogram, rx_packets, 50.0),
            (double)rx_average(rx_histogram, rx_packets)
            );


    // char buf[BUFSIZ]; 
    // sprintf(buf, "%lu", (uint64_t)(port_stats->imissed));
    // printf("%s",buf);
    // send(client_sockfd, buf, strlen(buf),0);
    uint64_t temp = port_stats->imissed + 1; 
    now_miss = temp;
    if(now_miss - last_miss > 1024)
    {
        // FILE * revsketch_res = fopen("revsketch_res.txt", "a");
        remove("revsketch_res.txt");
    }
    last_miss = now_miss;

    if(send(client_sockfd, (char*)&temp, 8, 0) == -1)
    {
        FILE * file_res = fopen("processinglatency.txt", "a");
        // fprintf(file_res, "rate = %lf\n", i_rate);
        fprintf(file_res, "avg latency = %lf\n", (double)rx_average(rx_histogram, rx_packets));
        fprintf(file_res, "99tail latency = %lf\n", (double)rx_percentile(rx_histogram, rx_packets, 99.0));
        fflush(file_res);
        fclose(file_res);

        rte_exit(0, "disconnected.");
    }


#ifdef DEBUG
    static struct rte_eth_xstats g_stats[1024];
    int n = 0, i = 0;
    n = rte_eth_xstats_get(port->port_id, g_stats, 1024);
    for (i = 0; i < n; ++i) {
        printf("%s: %" PRIu64 "\n", g_stats[i].name, g_stats[i].value);
    }
#endif
}

inline void
port_exec_rx_modules(PortPtr port, uint32_t __attribute__((unused)) queue,
        struct rte_mbuf **pkts, uint32_t count) {
    unsigned i;
    
    // printf("port->nrx_modules = %d\n", port->nrx_modules);

    for (i = 0; i < port->nrx_modules; ++i) {
        ModulePtr m = (ModulePtr)port->rx_modules[i];
        m->execute(m, port, pkts, count);
    }
}

inline void
port_add_rx_module(PortPtr port, void *module) {
    port->rx_modules[port->nrx_modules++] = (ModulePtr)module;
}

inline uint32_t
port_has_ticked(PortPtr port) {
    return port->ticked;
}

inline void
port_set_tick(PortPtr port) {
    port->ticked = 1;
}

inline void
port_clear_tick(PortPtr port) {
    port->ticked = 0;
}

inline int
port_queue_length(PortPtr port, uint32_t queue) {
    return rte_eth_rx_queue_count(port->port_id, queue);
}
