/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2016 Yang Zhou <zhou.yang@pku.edu.cn>
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

#ifndef _SIMDBATCH_COMMON_H
#define _SIMDBATCH_COMMON_H

#include "../../dss/hash.h"
#define CMS_SH1 17093
#define CMS_SH2 30169
#define CMS_SH3 52757
#define CMS_SH4 83233


#define CMS_SH5 104147
#define CMS_SH6 137143
#define CMS_SH7 154823
#define CMS_SH8 174907


#define CMS_SH9 195047
#define CMS_SH10 218447
#define CMS_SH11 233323
#define CMS_SH12 253531


#define CMS_SH13 277829
#define CMS_SH14 296221
#define CMS_SH15 318569
#define CMS_SH16 338573




#define GRR_ALL
// #define LRU_ALL


struct Simd
{    
#ifdef GRR_ALL
    uint32_t curkick;
#endif

#ifdef LRU_ALL
    uint32_t time_now;
    uint32_t *myclock;
#endif

    uint32_t *key;
    uint32_t *value;
    uint32_t *curpos;

    uint32_t table[];
};

typedef struct Simd* SimdPtr;

// #define SIMD_ROW_NUM 5000

#define SIMD_ROW_NUM 2000



#ifdef GRR_ALL
#define SIMD_SIZE (2 * SIMD_ROW_NUM * 16 * sizeof(uint32_t) + SIMD_ROW_NUM * sizeof(uint32_t))
#endif

#ifdef LRU_ALL
#define SIMD_SIZE (3 * SIMD_ROW_NUM * 16 * sizeof(uint32_t) + SIMD_ROW_NUM * sizeof(uint32_t))
#endif


#define KEY_SIZE (SIMD_ROW_NUM * 16)


struct SimdSD
{    
#ifdef GRR_ALL
    uint32_t curkick;
#endif

#ifdef LRU_ALL
    uint32_t time_now;
    uint32_t *myclock;
#endif
 
    uint32_t *key;
    uint32_t *value;
    uint64_t *sdpair;

    uint32_t *curpos;

    uint32_t table[];
};

typedef struct SimdSD* SimdSDPtr;


#define SIMDSD_SIZE (SIMD_SIZE + 2 * SIMD_ROW_NUM * 16 * sizeof(uint32_t))



// for flowradar
struct Cell
{
    uint32_t flowx;
    uint32_t flowc;
    uint32_t packetc;
};

typedef struct Cell* CellPtr;


// for spacesaving
struct Node
{
    uint32_t flowid;
    uint32_t freq;
};

typedef struct Node* NodePtr;
// #define tail_node (ptr->nodes[0].prev)

// for countheap
struct Pair
{
    uint32_t freq;
    uint32_t flowid;
};

typedef struct Pair * PairPtr;


// for twolevel
struct BitCell
{
    uint32_t cell[3];
};

typedef struct BitCell * BitCellPtr;



#endif //_SIMDBATCH_COMMON_H
