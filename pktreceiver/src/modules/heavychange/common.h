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

#ifndef _HEAVYCHANGE_COMMON_H
#define _HEAVYCHANGE_COMMON_H

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



// for countheap
struct Pair
{
    uint32_t freq;
    uint64_t flowid;
};

typedef struct Pair * PairPtr;


// for minhash
#define MemRatio 0.2
#define P_SH 1
#define P_FS 1


// for revsketch
#define ODD_NUM 10007

#endif //_HEAVYCHANGE_COMMON_H
