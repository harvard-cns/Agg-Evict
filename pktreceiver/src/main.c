#include <inttypes.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <sys/types.h>  
#include <sys/socket.h>  
#include <netinet/in.h>  
#include <arpa/inet.h>  


#include "rte_common.h"
#include "rte_cycles.h"
#include "rte_eal.h"
#include "rte_ethdev.h"
#include "rte_ip.h"
#include "rte_lcore.h"
#include "rte_malloc.h"
#include "rte_mbuf.h"

#include "assert.h"
#include "bootstrap.h"
#include "common.h"
#include "console.h"
#include "experiment.h"
#include "memory.h"
#include "module.h"
#include "net.h"
#include "pkt.h"
#include "reporter.h"

#include "dss/pqueue.h"
#include "dss/hashmap_cuckoo.h"

#ifdef IS_TEST_BUILD
#include "../tests/test.h"
#endif

#define PORT_COUNT 2

void rx_modules(PortPtr, uint32_t, struct rte_mbuf**, uint32_t);
void null_ptr(PortPtr);
void stats_ptr(PortPtr *, int);
void print_stats(PortPtr, int);
void initialize(void);
int core_loop(void *);
int cons_loop(void *);
int stats_loop(void *);
void cleanup(void);


char logname[100];


static ConsolePtr g_console = 0;

ExprsPtr g_exprs = 0;
PortPtr ports[PORT_COUNT] = {0};

inline void 
rx_modules(PortPtr port, 
        uint32_t queue,
        struct rte_mbuf ** __restrict__ pkts,
        uint32_t count) {
    uint16_t i = 0;
    for (i = 0; i < count; ++i) {
        rte_prefetch0(pkts[i]);
    }

    port_exec_rx_modules(port, queue, pkts, count);

    for (i = 0; i < count; ++i) { 
        uint8_t const *pkt = rte_pktmbuf_mtod(pkts[i], uint8_t const*);

        // unsigned char *src = pkt + 26;
        // unsigned char *dst = pkt + 30;

        // printf("%u\t%u\n", *(uint32_t *)(src), *(uint32_t *)(dst));

        // printf("%u\n", (uint32_t)*(uint8_t*)(pkt + 23));

        pkt += 38;
        uint32_t pktid = *((uint32_t const*)(pkt));


        // printf("pktid = %u\n", pktid);

        if (!(pktid & REPORT_THRESHOLD)) {
            // printf("pktid = %u\n", pktid);
#ifdef REPORT
            if(logname[0] != (char)0)
            {
                FILE * log_res = fopen(logname, "a");
                fprintf(log_res, "pktid = %d\n", pktid);
                fflush(log_res);
                fclose(log_res);
            }
#endif
            
            // FILE * revsketch_res = fopen("revsketch_res.txt", "a");
            // fprintf(revsketch_res, "pktid = %d\n", pktid);
            // fflush(revsketch_res);
            // fclose(revsketch_res);


            // FILE * minhash_res = fopen("minhash_res.txt", "a");
            // fprintf(minhash_res, "pktid = %d\n", pktid);
            // fflush(minhash_res);
            // fclose(minhash_res);


            port_set_tick(port);
        }

        rte_pktmbuf_free(pkts[i]);

    }
}

void print_stats(PortPtr port, int client_sockfd) {
    port_print_stats(port, client_sockfd);
}

int core_loop(void *ptr) {
    PortPtr port = (PortPtr)ptr;
    port_loop(port, rx_modules, null_ptr);
    return 0;
}
//char *sender_ip = "10.243.38.89";
char sender_ip[128];

int stats_loop(void *ptr) {
    PortPtr *ports = (PortPtr*)ptr;
    printf("Running stats function on lcore: %d\n", rte_lcore_id());


    int client_sockfd;

    struct sockaddr_in remote_addr;  
    memset(&remote_addr, 0, sizeof(remote_addr)); 
    remote_addr.sin_family = AF_INET; 
    remote_addr.sin_addr.s_addr = inet_addr(sender_ip); 
    remote_addr.sin_port = htons(64000);
    if((client_sockfd = socket(PF_INET, SOCK_STREAM, 0)) < 0)  
    {  
        rte_exit(0, "socket");
    }
    printf("connecting...");
    while(connect(client_sockfd, (struct sockaddr *)&remote_addr, sizeof(struct sockaddr)) < 0)  
    {  
        // rte_exit(0, "connect");
        printf(".");
    }  
    printf("connected to %s\n", sender_ip);



    while(1) {
        stats_ptr(ports, client_sockfd);
        expr_signal(g_exprs);
    }
    return 0;
}

inline void 
null_ptr(PortPtr __attribute__((unused)) port) {
}

inline void 
stats_ptr(PortPtr *ports, int client_sockfd) {
    unsigned i;
    if (unlikely(console_refresh(g_console))) {
        console_clear();
        for (i = 0; i < PORT_COUNT; ++i)
            if (ports[i])
                print_stats(ports[i], client_sockfd);
    }
}

void
initialize(void) {
    g_console = console_create(1000);
    boostrap_register_modules();
}

void
cleanup(void){
    expr_cleanup(g_exprs);
}

static void
cleanup_signal(int handler) {
    (void)(handler);
    FILE *f = fopen("stats.log", "w+");
    expr_stats_save(g_exprs, f);
    fclose(f);
    exit(0);
}

static void
init_modules(PortPtr port) {
    printf("Initializing modules on core: %d.\n", rte_lcore_id());
    expr_initialize(g_exprs, port);
    printf("Finished initializing modules.\n");
}

static int
run_port_at(uint32_t port_id, uint32_t core_id) {
    struct CoreQueuePair pairs0 [] = { { .core_id = 0 }, };
    pairs0[0].core_id = core_id;
    PortPtr port = ports[port_id] = port_create(port_id, pairs0,
            sizeof(pairs0)/sizeof(struct CoreQueuePair));
    if (!port) { port_delete(port); return EXIT_FAILURE; }
    port_start(port);
    port_print_mac(port);
    init_modules(port);
    rte_eal_remote_launch(core_loop, port, core_id);
    return 0;
}

int
main(int argc, char **argv) {
    int ret = 0;
    ret = rte_eal_init(argc, argv);

    if (ret < 0)
        rte_exit(EXIT_FAILURE, "Invalid EAL arguments.");

    argc -= ret;
    argv += ret;


    uint32_t start = 0;
    uint32_t end = 0;

    for(uint32_t i = 0; i < strlen(argv[1]); i++)
    {
        if(argv[1][i] == '/')
        {
            start = i;
        }
        if(argv[1][i] == '.')
        {
            end = i;
        }
    }
    strncpy(logname, argv[1] + start + 1, end - start - 1);
    if(logname[0] == 'r')
        strcpy(logname, "revsketch_res.txt");
    else if(logname[0] == 'm')
        strcpy(logname, "minhash_res.txt");
    else if(logname[0] == 'u')
        strcpy(logname, "univmon_res.txt");
    else
        strcat(logname, "_res.txt");
    
    strcpy(sender_ip, argv[2]);

    printf("%s\n", logname);

    FILE * file_res = fopen("processinglatency.txt", "a");
    fprintf(file_res, "%s\n", argv[1]);
    fflush(file_res);
    fclose(file_res);

    // if(argc == 3)
    // {
    //     strcpy(logname, argv[2]);
    //     printf("%d, %s\n", argc, argv[2]);

    // }

#ifdef IS_TEST_BUILD
    // Run tests if this is a test build
    exit(run_tests());
#endif

    g_exprs = expr_parse(argv[1]);

    int nb_ports = 0;
    nb_ports = rte_eth_dev_count();

    printf("Total number of ports: %d\n", nb_ports);

    initialize();
    atexit(cleanup);
    signal(SIGINT, cleanup_signal);

    if (run_port_at(0, 1) != 0) exit(-1);
    if (run_port_at(1, 3) != 0) exit(-1);

    rte_eal_remote_launch(stats_loop, ports, 5);
    rte_eal_mp_wait_lcore();

    return 0;
}
