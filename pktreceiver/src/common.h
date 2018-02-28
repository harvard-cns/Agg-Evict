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

#ifndef _COMMON_H_
#define _COMMON_H_

#define HUGE_PAGE_SIZE ((2<<20) - 1)
#define PORT_PKTMBUF_POOL_SIZE ((2<<13)-1)

/* Advised to have a size where PORT_PKTMBUF_POOL_SIZE modulo num == 0 */
#define PORT_PKTMBUF_POOL_CACHE_SIZE ((250))  

#define MAX_NUM_SOCKETS 4    /* Total number of possible sockets in the system */
#define RX_DESC_DEFAULT 512  /* Mempool size for the RX queue */
#define TX_DESC_DEFAULT 512  /* Mempool size for the TX queue */

#define MAX_PKT_BURST   64   /* Maximum number of packets received in a burst * 2, eg: MAX_PKT_BURST=64 means batch size of 32*/
#define MAX_RX_BURST    64  /* The amount of packets to process at one time * 2*/

// #define MAX_RX_BURST    32   /* The amount of packets to process at one time */

#define MAX_RX_WAIT     4    /* Number of RX wait cycles */
#define MAX_QUEUES      4    /* Maximum number of queues */

#include "rte_mbuf.h"
#define MBUF_SIZE (1600 + sizeof(struct rte_mbuf) + RTE_PKTMBUF_HEADROOM)

#include "rte_log.h"
#define RTE_LOGTYPE_APP RTE_LOGTYPE_USER1

// Length of the name of the module
#define MAX_MODULE_NAME 32

// Histogram specification
#define HIST_SIZE 2048
#define HIST_BUCKET_SIZE  2

#define MAX_MODULES  4

// Max number of rings that consumer can read off
#define CONSUMER_MAX_RINGS 4

// Module params
#define COUNT_ARRAY_SIZE ((1<<19) - 1) // Size is 4 * 4 MB
#define SUPER_SPREADER_SIZE ((1<<20) - 1) // Size is 32 * 4 MB

// Report threshold
#define REPORT_THRESHOLD 0x1fffff
#define HEAVYHITTER_THRESHOLD 2048
#define SUPERSPREADER_THRESHOLD 128

// Cache line properties
#ifndef RTE_CACHE_LINE_MIN_SIZE
    #define RTE_CACHE_LINE_MIN_SIZE 64
#endif

//#define REPORT


#endif // _COMMON_H_
