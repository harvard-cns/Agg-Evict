#include "rte_atomic.h"
#include "rte_memcpy.h"
#include "rte_cycles.h"
#include "rte_ethdev.h"
#include "rte_ip.h"
#include "rte_malloc.h"
#include "rte_mbuf.h"


#include <stdio.h>
#include <stdlib.h>


#include "../../../common.h"
#include "../../../experiment.h"
#include "../../../module.h"
#include "../../../net.h"
#include "../../../vendor/murmur3.h"
#include "../../../dss/hash.h"

#include "../../heavyhitter/common.h"

#include "flowradarsimdv.h"




inline static uint32_t flowradarsimdv_size(FlowRadarSimdVPtr ptr) 
{
    return ptr->size * (sizeof(struct CellV) + 4);
}

inline void flowradarsimdv_reset(FlowRadarSimdVPtr ptr) 
{
    rte_atomic32_set(&ptr->stats_search, 0);

    for(int i = 0; i < SIMD_ROW_NUM; i++)
    {
        uint32_t *p_key_bucket = (ptr->simdv_ptr->key + (i << 4));
        rte_prefetch0(p_key_bucket);

        uint32_t *p_value_bucket = (ptr->simdv_ptr->value + (i << 4));
        rte_prefetch0(p_value_bucket);
        
        for(int j = 0; j < 16; j++)
        {
            flowradarsimdv_inc_fr(ptr, (const void *)(p_key_bucket + j), *(p_value_bucket + j));
        }
    }

    memset(ptr->table, 0, flowradarsimdv_size(ptr));

    // init the simd part;
    memset(ptr->simdv_ptr->table, 0, SIMDV_SIZE);

#ifdef GRR
    ptr->simdv_ptr->curkick = 0;
#endif

#ifdef LRU
    ptr->simdv_ptr->time_now = 0;
#endif
}

FlowRadarSimdVPtr flowradarsimdv_create(uint32_t size, uint16_t keysize, uint16_t elsize, int socket) 
{
    /* Size up so that we are a power of two ... no wasted space */
    size = rte_align32pow2(size);
    // the number of cells in iblt of FR: size;
    // the number of bits in bf of FR: size * 32;


    FlowRadarSimdVPtr ptr = rte_zmalloc_socket(
        0, 
        sizeof(struct FlowRadarSimdV) + 
        (size) * (sizeof(struct CellV) + 4), /* Size of elements */
        64, socket);

    // not size - 1 !!!
    ptr->size = size;
    ptr->keysize = keysize;
    ptr->elsize = elsize;
    ptr->rowsize = ptr->elsize;

    ptr->rowbf = ptr->table;
    ptr->rowcell = (CellVPtr)(ptr->table + (size));


    // allocate memory for simd part;
    ptr->simdv_ptr = rte_zmalloc_socket(
        0, 
        sizeof(struct Simd) + SIMDV_SIZE, /* Size of elements */
        64, socket);

    ptr->simdv_ptr->key = ptr->simdv_ptr->table;
    ptr->simdv_ptr->value = ptr->simdv_ptr->table + (KEY_SIZE);



// curkick and time_now have already been set to zero when allocating memory;

#ifdef SIP
#ifdef LRU
    ptr->simdv_ptr->myclock = ptr->simdv_ptr->table + (KEY_SIZE * 2);
    ptr->simdv_ptr->curpos = ptr->simdv_ptr->table + (KEY_SIZE * 3);
#endif
#ifdef GRR
    ptr->simdv_ptr->curpos = ptr->simdv_ptr->table + (KEY_SIZE * 2);
#endif
#endif 

#ifdef DIP
#ifdef LRU
    ptr->simdv_ptr->myclock = ptr->simdv_ptr->table + (KEY_SIZE * 2);
    ptr->simdv_ptr->curpos = ptr->simdv_ptr->table + (KEY_SIZE * 3);
#endif
#ifdef GRR
    ptr->simdv_ptr->curpos = ptr->simdv_ptr->table + (KEY_SIZE * 2);
#endif
#endif

#ifdef SD_PAIR
    ptr->simdv_ptr->simdcellptr = (SimdCellPtr)(ptr->table + (KEY_SIZE * 2));
#ifdef LRU
    ptr->simdv_ptr->myclock = ptr->simdv_ptr->table + (KEY_SIZE * 4);
    ptr->simdv_ptr->curpos = ptr->simdv_ptr->table + (KEY_SIZE * 5);
#endif
#ifdef GRR
    ptr->simdv_ptr->curpos = ptr->simdv_ptr->table + (KEY_SIZE * 4);    
#endif
#endif

#ifdef FOUR_TUPLE
    ptr->simdv_ptr->simdcellptr = (SimdCellPtr)(ptr->table + (KEY_SIZE * 2));
#ifdef LRU
    ptr->simdv_ptr->myclock = ptr->simdv_ptr->table + (KEY_SIZE * 5);
    ptr->simdv_ptr->curpos = ptr->simdv_ptr->table + (KEY_SIZE * 6);
#endif
#ifdef GRR
    ptr->simdv_ptr->curpos = ptr->simdv_ptr->table + (KEY_SIZE * 5);    
#endif
#endif

#ifdef FIVE_TUPLE
    ptr->simdv_ptr->simdcellptr = (SimdCellPtr)(ptr->table + (KEY_SIZE * 2));
#ifdef LRU
    ptr->simdv_ptr->myclock = ptr->simdv_ptr->table + (KEY_SIZE * 6);
    ptr->simdv_ptr->curpos = ptr->simdv_ptr->table + (KEY_SIZE * 7);
#endif
#ifdef GRR
    ptr->simdv_ptr->curpos = ptr->simdv_ptr->table + (KEY_SIZE * 6);    
#endif
#endif


    return ptr;
}


inline int flowradarsimdv_inc_fr(FlowRadarSimdVPtr ptr, void const * kick_key, uint32_t kick_val)
{
    uint32_t bfsize = ((ptr->size) << 5); // 32 * size, bit;
    uint32_t ibltsize = ptr->size; // cells;

    uint32_t mykey[4];
    uint32_t mykeysize;

#ifdef SIP
    mykeysize = 1;
#endif 

#ifdef DIP
    mykeysize = 1;
#endif 

#ifdef SD_PAIR
    mykeysize = 2;
#endif 

#ifdef FOUR_TUPLE
    mykeysize = 3;
#endif 

#ifdef FIVE_TUPLE
    mykeysize = 4;
#endif 

    rte_memcpy(mykey, kick_key, mykeysize * sizeof(uint32_t));


    uint32_t h1 = dss_hash_with_seed((void const *)mykey, mykeysize, CMS_SH1) & (bfsize - 1);
    uint32_t *p1 = ptr->rowbf + (h1 >> 5);
    rte_prefetch0(p1);

    uint32_t h2 = dss_hash_with_seed((void const *)mykey, mykeysize, CMS_SH2 + CMS_SH1) & (bfsize - 1);
    uint32_t *p2 = ptr->rowbf + (h2 >> 5);
    rte_prefetch0(p2);

    uint32_t h3 = dss_hash_with_seed((void const *)mykey, mykeysize, CMS_SH3 + CMS_SH2) & (bfsize - 1);
    uint32_t *p3 = ptr->rowbf + (h3 >> 5);
    rte_prefetch0(p3);

    uint32_t h4 = dss_hash_with_seed((void const *)mykey, mykeysize, CMS_SH4 + CMS_SH3) & (bfsize - 1);
    uint32_t *p4 = ptr->rowbf + (h4 >> 5);
    rte_prefetch0(p4);


    int flag = 1;
    flag = (flag && (((*p1) >> (h1 & 0x1F)) & 1));
    (*p1) |= (1 << (h1 & 0x1F));

    flag = (flag && (((*p2) >> (h2 & 0x1F)) & 1));
    (*p2) |= (1 << (h2 & 0x1F));

    flag = (flag && (((*p3) >> (h3 & 0x1F)) & 1));
    (*p3) |= (1 << (h3 & 0x1F));

    flag = (flag && (((*p4) >> (h4 & 0x1F)) & 1));
    (*p4) |= (1 << (h4 & 0x1F));




    h1 = dss_hash_with_seed((void const *)mykey, mykeysize, CMS_SH5) & (ibltsize - 1);
    CellVPtr p1_cell = ptr->rowcell + h1;
    rte_prefetch0(p1_cell);

    h2 = dss_hash_with_seed((void const *)mykey, mykeysize, CMS_SH6 + CMS_SH5) & (ibltsize - 1);
    CellVPtr p2_cell = ptr->rowcell + h2;
    rte_prefetch0(p2_cell);

    h3 = dss_hash_with_seed((void const *)mykey, mykeysize, CMS_SH7 + CMS_SH5) & (ibltsize - 1);
    CellVPtr p3_cell = ptr->rowcell + h3;
    rte_prefetch0(p3_cell);

    h4 = dss_hash_with_seed((void const *)mykey, mykeysize, CMS_SH8 + CMS_SH5) & (ibltsize - 1);
    CellVPtr p4_cell = ptr->rowcell + h4;
    rte_prefetch0(p4_cell);


    // a new flow!
    if(flag == 0)
    {
#ifdef SIP
        p1_cell->sip ^= mykey[0];
        p2_cell->sip ^= mykey[0];
        p3_cell->sip ^= mykey[0];
        p4_cell->sip ^= mykey[0];
#endif

#ifdef DIP
        p1_cell->dip ^= mykey[0];
        p2_cell->dip ^= mykey[0];
        p3_cell->dip ^= mykey[0];
        p4_cell->dip ^= mykey[0];
#endif

#ifdef SD_PAIR
        p1_cell->sip ^= mykey[0];
        p1_cell->dip ^= mykey[1];

        p2_cell->sip ^= mykey[0];
        p2_cell->dip ^= mykey[1];

        p3_cell->sip ^= mykey[0];
        p3_cell->dip ^= mykey[1];
        
        p4_cell->sip ^= mykey[0];
        p4_cell->dip ^= mykey[1];
#endif

#ifdef FOUR_TUPLE
        p1_cell->sip ^= mykey[0];
        p1_cell->dip ^= mykey[1];
        p1_cell->port ^= mykey[2];

        p2_cell->sip ^= mykey[0];
        p2_cell->dip ^= mykey[1];
        p2_cell->port ^= mykey[2];

        p3_cell->sip ^= mykey[0];
        p3_cell->dip ^= mykey[1];
        p3_cell->port ^= mykey[2];

        p4_cell->sip ^= mykey[0];
        p4_cell->dip ^= mykey[1];
        p4_cell->port ^= mykey[2];
#endif

#ifdef FIVE_TUPLE
        p1_cell->sip ^= mykey[0];
        p1_cell->dip ^= mykey[1];
        p1_cell->port ^= mykey[2];
        p1_cell->type ^= mykey[3];

        p2_cell->sip ^= mykey[0];
        p2_cell->dip ^= mykey[1];
        p2_cell->port ^= mykey[2];
        p2_cell->type ^= mykey[3];

        p3_cell->sip ^= mykey[0];
        p3_cell->dip ^= mykey[1];
        p3_cell->port ^= mykey[2];
        p3_cell->type ^= mykey[3];
        
        p4_cell->sip ^= mykey[0];
        p4_cell->dip ^= mykey[1];
        p4_cell->port ^= mykey[2];
        p4_cell->type ^= mykey[3];
#endif
  
        p1_cell->flowc ++;
        p2_cell->flowc ++;
        p3_cell->flowc ++;
        p4_cell->flowc ++;
    }

    p1_cell->packetc += kick_val;
    p2_cell->packetc += kick_val;
    p3_cell->packetc += kick_val;
    p4_cell->packetc += kick_val;

    return 1;
}


inline int flowradarsimdv_inc(FlowRadarSimdVPtr ptr, void const *key) 
{
    rte_atomic32_inc(&ptr->stats_search);
#ifdef LRU
    ptr->simdv_ptr->time_now ++;
#endif
    








#ifdef SIP

    uint32_t flowid = *(uint32_t const*)key;
    uint32_t bucket_id = flowid % SIMD_ROW_NUM;

    uint32_t *p_key_bucket = (ptr->simdv_ptr->key + (bucket_id << 4));
    rte_prefetch0(p_key_bucket);

    uint32_t *p_value_bucket = (ptr->simdv_ptr->value + (bucket_id << 4));
    rte_prefetch0(p_value_bucket);

#ifdef LRU
    uint32_t *p_clock_bucket = (ptr->simdv_ptr->myclock + (bucket_id << 4));
    rte_prefetch0(p_clock_bucket);
#endif

    const __m128i item = _mm_set1_epi32((int)flowid);

    __m128i *keys_p = (__m128i *)p_key_bucket;


    __m128i a_comp = _mm_cmpeq_epi32(item, keys_p[0]);
    __m128i b_comp = _mm_cmpeq_epi32(item, keys_p[1]);
    __m128i c_comp = _mm_cmpeq_epi32(item, keys_p[2]);
    __m128i d_comp = _mm_cmpeq_epi32(item, keys_p[3]);

    a_comp = _mm_packs_epi32(a_comp, b_comp);
    c_comp = _mm_packs_epi32(c_comp, d_comp);
    a_comp = _mm_packs_epi32(a_comp, c_comp);

    int matched = _mm_movemask_epi8(a_comp);

    if(matched != 0)
    {
        //return 32 if input is zero;
        int matched_index = _tzcnt_u32((uint32_t)matched);
        (*(p_value_bucket + matched_index)) ++;  

#ifdef LRU
        p_clock_bucket[matched_index] = ptr->simdv_ptr->time_now;
#endif

        return 0;
    }

    uint32_t *p_curpos = (ptr->simdv_ptr->curpos + bucket_id);
    rte_prefetch0(p_curpos);
    int cur_pos_now = *(p_curpos);


    if(cur_pos_now != 16)
    {
        (*(p_key_bucket + cur_pos_now)) = flowid;
        (*(p_value_bucket + cur_pos_now)) = 1;

#ifdef LRU
        p_clock_bucket[cur_pos_now] = ptr->simdv_ptr->time_now;
#endif  

        (*(p_curpos)) ++;
        return 0;
    }

#ifdef GRR
    uint32_t curkick = ptr->simdv_ptr->curkick;
    uint32_t ret = flowradarsimdv_inc_fr(ptr, (const void *)(p_key_bucket + curkick), p_value_bucket[curkick]);

    p_key_bucket[curkick] = flowid;
    p_value_bucket[curkick] = 1;

    ptr->simdv_ptr->curkick = (curkick + 1) & 0xF;

    return ret;
#endif

#ifdef LRU
    uint32_t kick_index = 0;
    uint32_t min_time = (1 << 30);
    for(int i = 0; i < 16; i++)
    {
        if(p_clock_bucket[i] < min_time)
        {
            min_time = p_clock_bucket[i];
            kick_index = i;
        }
    }
    uint32_t ret = flowradarsimdv_inc_fr(ptr, (const void *)(p_key_bucket + kick_index), p_value_bucket[kick_index]);

    p_key_bucket[kick_index] = flowid;
    p_value_bucket[kick_index] = 1;

    p_clock_bucket[kick_index] = ptr->simdv_ptr->time_now;

    return ret;
#endif

#endif









#ifdef DIP

  
    uint32_t flowid = *((uint32_t const*)key + 1);
    uint32_t bucket_id = flowid % SIMD_ROW_NUM;

    uint32_t *p_key_bucket = (ptr->simdv_ptr->key + (bucket_id << 4));
    rte_prefetch0(p_key_bucket);

    uint32_t *p_value_bucket = (ptr->simdv_ptr->value + (bucket_id << 4));
    rte_prefetch0(p_value_bucket);

#ifdef LRU
    uint32_t *p_clock_bucket = (ptr->simdv_ptr->myclock + (bucket_id << 4));
    rte_prefetch0(p_clock_bucket);
#endif

    const __m128i item = _mm_set1_epi32((int)flowid);

    __m128i *keys_p = (__m128i *)p_key_bucket;


    __m128i a_comp = _mm_cmpeq_epi32(item, keys_p[0]);
    __m128i b_comp = _mm_cmpeq_epi32(item, keys_p[1]);
    __m128i c_comp = _mm_cmpeq_epi32(item, keys_p[2]);
    __m128i d_comp = _mm_cmpeq_epi32(item, keys_p[3]);

    a_comp = _mm_packs_epi32(a_comp, b_comp);
    c_comp = _mm_packs_epi32(c_comp, d_comp);
    a_comp = _mm_packs_epi32(a_comp, c_comp);

    int matched = _mm_movemask_epi8(a_comp);

    if(matched != 0)
    {
        //return 32 if input is zero;
        int matched_index = _tzcnt_u32((uint32_t)matched);
        (*(p_value_bucket + matched_index)) ++;  

#ifdef LRU
        p_clock_bucket[matched_index] = ptr->simdv_ptr->time_now;
#endif

        return 0;
    }

    uint32_t *p_curpos = (ptr->simdv_ptr->curpos + bucket_id);
    rte_prefetch0(p_curpos);
    int cur_pos_now = *(p_curpos);


    if(cur_pos_now != 16)
    {
        (*(p_key_bucket + cur_pos_now)) = flowid;
        (*(p_value_bucket + cur_pos_now)) = 1;

#ifdef LRU
        p_clock_bucket[cur_pos_now] = ptr->simdv_ptr->time_now;
#endif  

        (*(p_curpos)) ++;
        return 0;
    }

#ifdef GRR
    uint32_t curkick = ptr->simdv_ptr->curkick;
    uint32_t ret = flowradarsimdv_inc_fr(ptr, (const void *)(p_key_bucket + curkick), p_value_bucket[curkick]);

    p_key_bucket[curkick] = flowid;
    p_value_bucket[curkick] = 1;

    ptr->simdv_ptr->curkick = (curkick + 1) & 0xF;

    return ret;
#endif

#ifdef LRU
    uint32_t kick_index = 0;
    uint32_t min_time = (1 << 30);
    for(int i = 0; i < 16; i++)
    {
        if(p_clock_bucket[i] < min_time)
        {
            min_time = p_clock_bucket[i];
            kick_index = i;
        }
    }
    uint32_t ret = flowradarsimdv_inc_fr(ptr, (const void *)(p_key_bucket + kick_index), p_value_bucket[kick_index]);

    p_key_bucket[kick_index] = flowid;
    p_value_bucket[kick_index] = 1;

    p_clock_bucket[kick_index] = ptr->simdv_ptr->time_now;

    return ret;
#endif

#endif









#ifdef SD_PAIR

    uint32_t sip = (*(uint32_t const*)key);
    uint32_t dip = (*(((uint32_t const*)key) + 1));
    uint32_t fp = sip ^ dip;

    uint32_t bucket_id = fp % SIMD_ROW_NUM;

    uint32_t *p_key_bucket = (ptr->simdv_ptr->key + (bucket_id << 4));
    rte_prefetch0(p_key_bucket);

    uint32_t *p_value_bucket = (ptr->simdv_ptr->value + (bucket_id << 4));
    rte_prefetch0(p_value_bucket);

    SimdCellPtr p_simdcell_bucket = (ptr->simdv_ptr->simdcellptr + (bucket_id << 4));
    rte_prefetch0(p_simdcell_bucket);

#ifdef LRU
    uint32_t *p_clock_bucket = (ptr->simdv_ptr->myclock + (bucket_id << 4));
    rte_prefetch0(p_clock_bucket);
#endif

    const __m128i item = _mm_set1_epi32((int)fp);

    __m128i *keys_p = (__m128i *)p_key_bucket;


    __m128i a_comp = _mm_cmpeq_epi32(item, keys_p[0]);
    __m128i b_comp = _mm_cmpeq_epi32(item, keys_p[1]);
    __m128i c_comp = _mm_cmpeq_epi32(item, keys_p[2]);
    __m128i d_comp = _mm_cmpeq_epi32(item, keys_p[3]);

    a_comp = _mm_packs_epi32(a_comp, b_comp);
    c_comp = _mm_packs_epi32(c_comp, d_comp);
    a_comp = _mm_packs_epi32(a_comp, c_comp);

    int matched = _mm_movemask_epi8(a_comp);

    if(matched != 0)
    {
        //return 32 if input is zero;
        int matched_index = _tzcnt_u32((uint32_t)matched);

        if(p_simdcell_bucket[matched_index].flowid[0] == sip && p_simdcell_bucket[matched_index].flowid[1] == dip)
        {
            p_value_bucket[matched_index] ++;
        }
        else
        {
            flowradarsimdv_inc_fr(ptr, (const void *)((p_simdcell_bucket + matched_index)->flowid), p_value_bucket[matched_index]);
            p_simdcell_bucket[matched_index].flowid[0] = sip;
            p_simdcell_bucket[matched_index].flowid[1] = dip;

            p_value_bucket[matched_index] = 1;
        }

#ifdef LRU
        p_clock_bucket[matched_index] = ptr->simdv_ptr->time_now;
#endif
        return 0;
    }

    uint32_t *p_curpos = (ptr->simdv_ptr->curpos + bucket_id);
    rte_prefetch0(p_curpos);
    int cur_pos_now = *p_curpos;


    if(cur_pos_now != 16)
    {
        p_simdcell_bucket[cur_pos_now].flowid[0] = sip;
        p_simdcell_bucket[cur_pos_now].flowid[1] = dip;

        p_key_bucket[cur_pos_now] = fp;
        p_value_bucket[cur_pos_now] = 1;


#ifdef LRU
        p_clock_bucket[cur_pos_now] = ptr->simdv_ptr->time_now;
#endif  

        (*p_curpos) ++;

        return 0;
    }


#ifdef GRR
    uint32_t curkick = ptr->simdv_ptr->curkick;
    uint32_t ret = flowradarsimdv_inc_fr(ptr, (const void *)((p_simdcell_bucket + curkick)->flowid), p_value_bucket[curkick]);

    p_key_bucket[curkick] = fp;
    p_value_bucket[curkick] = 1;
    p_simdcell_bucket[curkick].flowid[0] = sip;
    p_simdcell_bucket[curkick].flowid[1] = dip;

    ptr->simdv_ptr->curkick = (curkick + 1) & 0xF;

    return ret;
#endif

#ifdef LRU
    uint32_t kick_index = 0;
    uint32_t min_time = (1 << 30);
    for(int i = 0; i < 16; i++)
    {
        if(p_clock_bucket[i] < min_time)
        {
            min_time = p_clock_bucket[i];
            kick_index = i;
        }
    }
    uint32_t ret = flowradarsimdv_inc_fr(ptr, (const void *)((p_simdcell_bucket + kick_index)->flowid), p_value_bucket[kick_index]);

    p_key_bucket[kick_index] = fp;
    p_value_bucket[kick_index] = 1;
    p_simdcell_bucket[kick_index].flowid[0] = sip;
    p_simdcell_bucket[kick_index].flowid[1] = dip;

    p_clock_bucket[kick_index] = ptr->simdv_ptr->time_now;

    return ret;
#endif

#endif








#ifdef FOUR_TUPLE

    uint32_t sip = (*(uint32_t const*)key);
    uint32_t dip = (*(((uint32_t const*)key) + 1));
    uint32_t port = (*(((uint32_t const*)key) + 2));
    uint32_t fp = sip ^ dip ^ port;

    uint32_t bucket_id = fp % SIMD_ROW_NUM;

    uint32_t *p_key_bucket = (ptr->simdv_ptr->key + (bucket_id << 4));
    rte_prefetch0(p_key_bucket);

    uint32_t *p_value_bucket = (ptr->simdv_ptr->value + (bucket_id << 4));
    rte_prefetch0(p_value_bucket);

    SimdCellPtr p_simdcell_bucket = (ptr->simdv_ptr->simdcellptr + (bucket_id << 4));
    rte_prefetch0(p_simdcell_bucket);

#ifdef LRU
    uint32_t *p_clock_bucket = (ptr->simdv_ptr->myclock + (bucket_id << 4));
    rte_prefetch0(p_clock_bucket);
#endif


    const __m128i item = _mm_set1_epi32((int)fp);

    __m128i *keys_p = (__m128i *)p_key_bucket;


    __m128i a_comp = _mm_cmpeq_epi32(item, keys_p[0]);
    __m128i b_comp = _mm_cmpeq_epi32(item, keys_p[1]);
    __m128i c_comp = _mm_cmpeq_epi32(item, keys_p[2]);
    __m128i d_comp = _mm_cmpeq_epi32(item, keys_p[3]);

    a_comp = _mm_packs_epi32(a_comp, b_comp);
    c_comp = _mm_packs_epi32(c_comp, d_comp);
    a_comp = _mm_packs_epi32(a_comp, c_comp);

    int matched = _mm_movemask_epi8(a_comp);

    if(matched != 0)
    {
        //return 32 if input is zero;
        int matched_index = _tzcnt_u32((uint32_t)matched);

        if(p_simdcell_bucket[matched_index].flowid[0] == sip && p_simdcell_bucket[matched_index].flowid[1] == dip
            && p_simdcell_bucket[matched_index].flowid[2] == port)
        {
            p_value_bucket[matched_index] ++;
        }
        else
        {
            flowradarsimdv_inc_fr(ptr, (const void *)((p_simdcell_bucket + matched_index)->flowid), p_value_bucket[matched_index]);
            p_simdcell_bucket[matched_index].flowid[0] = sip;
            p_simdcell_bucket[matched_index].flowid[1] = dip;
            p_simdcell_bucket[matched_index].flowid[2] = port;

            p_value_bucket[matched_index] = 1;
        }

#ifdef LRU
        p_clock_bucket[matched_index] = ptr->simdv_ptr->time_now;
#endif

        return 0;
    }

    uint32_t *p_curpos = (ptr->simdv_ptr->curpos + bucket_id);
    rte_prefetch0(p_curpos);
    int cur_pos_now = *p_curpos;


    if(cur_pos_now != 16)
    {
        p_simdcell_bucket[cur_pos_now].flowid[0] = sip;
        p_simdcell_bucket[cur_pos_now].flowid[1] = dip;
        p_simdcell_bucket[cur_pos_now].flowid[2] = port;

        p_key_bucket[cur_pos_now] = fp;
        p_value_bucket[cur_pos_now] = 1;

#ifdef LRU
        p_clock_bucket[cur_pos_now] = ptr->simdv_ptr->time_now;
#endif  

        (*p_curpos) ++;

        return 0;
    }


#ifdef GRR
    uint32_t curkick = ptr->simdv_ptr->curkick;
    uint32_t ret = flowradarsimdv_inc_fr(ptr, (const void *)((p_simdcell_bucket + curkick)->flowid), p_value_bucket[curkick]);

    p_key_bucket[curkick] = fp;
    p_value_bucket[curkick] = 1;
    p_simdcell_bucket[curkick].flowid[0] = sip;
    p_simdcell_bucket[curkick].flowid[1] = dip;
    p_simdcell_bucket[curkick].flowid[2] = port;

    ptr->simdv_ptr->curkick = (curkick + 1) & 0xF;

    return ret;
#endif

#ifdef LRU
    uint32_t kick_index = 0;
    uint32_t min_time = (1 << 30);
    for(int i = 0; i < 16; i++)
    {
        if(p_clock_bucket[i] < min_time)
        {
            min_time = p_clock_bucket[i];
            kick_index = i;
        }
    }
    uint32_t ret = flowradarsimdv_inc_fr(ptr, (const void *)((p_simdcell_bucket + kick_index)->flowid), p_value_bucket[kick_index]);

    p_key_bucket[kick_index] = fp;
    p_value_bucket[kick_index] = 1;
    p_simdcell_bucket[kick_index].flowid[0] = sip;
    p_simdcell_bucket[kick_index].flowid[1] = dip;
    p_simdcell_bucket[kick_index].flowid[2] = port;

    p_clock_bucket[kick_index] = ptr->simdv_ptr->time_now;

    return ret;
#endif

#endif












#ifdef FIVE_TUPLE

    uint32_t sip = (*(uint32_t const*)key);
    uint32_t dip = (*(((uint32_t const*)key) + 1));
    uint32_t port = (*(((uint32_t const*)key) + 2));
    uint32_t type = (*(((uint8_t const*)key) - 3));

    uint32_t fp = sip ^ dip ^ port ^ type;

    uint32_t bucket_id = fp % SIMD_ROW_NUM;

    uint32_t *p_key_bucket = (ptr->simdv_ptr->key + (bucket_id << 4));
    rte_prefetch0(p_key_bucket);

    uint32_t *p_value_bucket = (ptr->simdv_ptr->value + (bucket_id << 4));
    rte_prefetch0(p_value_bucket);

    SimdCellPtr p_simdcell_bucket = (ptr->simdv_ptr->simdcellptr + (bucket_id << 4));
    rte_prefetch0(p_simdcell_bucket);

#ifdef LRU
    uint32_t *p_clock_bucket = (ptr->simdv_ptr->myclock + (bucket_id << 4));
    rte_prefetch0(p_clock_bucket);
#endif

    const __m128i item = _mm_set1_epi32((int)fp);

    __m128i *keys_p = (__m128i *)p_key_bucket;


    __m128i a_comp = _mm_cmpeq_epi32(item, keys_p[0]);
    __m128i b_comp = _mm_cmpeq_epi32(item, keys_p[1]);
    __m128i c_comp = _mm_cmpeq_epi32(item, keys_p[2]);
    __m128i d_comp = _mm_cmpeq_epi32(item, keys_p[3]);

    a_comp = _mm_packs_epi32(a_comp, b_comp);
    c_comp = _mm_packs_epi32(c_comp, d_comp);
    a_comp = _mm_packs_epi32(a_comp, c_comp);

    int matched = _mm_movemask_epi8(a_comp);

    if(matched != 0)
    {
        //return 32 if input is zero;
        int matched_index = _tzcnt_u32((uint32_t)matched);

        if(p_simdcell_bucket[matched_index].flowid[0] == sip && p_simdcell_bucket[matched_index].flowid[1] == dip
            && p_simdcell_bucket[matched_index].flowid[2] == port && p_simdcell_bucket[matched_index].flowid[3] == type)
        {
            p_value_bucket[matched_index] ++;
        }
        else
        {
            flowradarsimdv_inc_fr(ptr, (const void *)((p_simdcell_bucket + matched_index)->flowid), p_value_bucket[matched_index]);
            p_simdcell_bucket[matched_index].flowid[0] = sip;
            p_simdcell_bucket[matched_index].flowid[1] = dip;
            p_simdcell_bucket[matched_index].flowid[2] = port;
            p_simdcell_bucket[matched_index].flowid[3] = type;

            p_value_bucket[matched_index] = 1;
        }

#ifdef LRU
        p_clock_bucket[matched_index] = ptr->simdv_ptr->time_now;
#endif
        return 0;
    }

    uint32_t *p_curpos = (ptr->simdv_ptr->curpos + bucket_id);
    rte_prefetch0(p_curpos);
    int cur_pos_now = *p_curpos;


    if(cur_pos_now != 16)
    {
        p_simdcell_bucket[cur_pos_now].flowid[0] = sip;
        p_simdcell_bucket[cur_pos_now].flowid[1] = dip;
        p_simdcell_bucket[cur_pos_now].flowid[2] = port;
        p_simdcell_bucket[cur_pos_now].flowid[3] = type;

        p_key_bucket[cur_pos_now] = fp;
        p_value_bucket[cur_pos_now] = 1;

#ifdef LRU
        p_clock_bucket[cur_pos_now] = ptr->simdv_ptr->time_now;
#endif  

        (*p_curpos) ++;

        return 0;
    }

#ifdef GRR
    uint32_t curkick = ptr->simdv_ptr->curkick;
    uint32_t ret = flowradarsimdv_inc_fr(ptr, (const void *)((p_simdcell_bucket + curkick)->flowid), p_value_bucket[curkick]);

    p_key_bucket[curkick] = fp;
    p_value_bucket[curkick] = 1;
    p_simdcell_bucket[curkick].flowid[0] = sip;
    p_simdcell_bucket[curkick].flowid[1] = dip;
    p_simdcell_bucket[curkick].flowid[2] = port;
    p_simdcell_bucket[curkick].flowid[3] = type;

    ptr->simdv_ptr->curkick = (curkick + 1) & 0xF;

    return ret;
#endif

#ifdef LRU
    uint32_t kick_index = 0;
    uint32_t min_time = (1 << 30);
    for(int i = 0; i < 16; i++)
    {
        if(p_clock_bucket[i] < min_time)
        {
            min_time = p_clock_bucket[i];
            kick_index = i;
        }
    }
    uint32_t ret = flowradarsimdv_inc_fr(ptr, (const void *)((p_simdcell_bucket + kick_index)->flowid), p_value_bucket[kick_index]);

    p_key_bucket[kick_index] = fp;
    p_value_bucket[kick_index] = 1;
    p_simdcell_bucket[kick_index].flowid[0] = sip;
    p_simdcell_bucket[kick_index].flowid[1] = dip;
    p_simdcell_bucket[kick_index].flowid[2] = port;
    p_simdcell_bucket[kick_index].flowid[3] = type;

    p_clock_bucket[kick_index] = ptr->simdv_ptr->time_now;

    return ret;
#endif
#endif
}

inline void flowradarsimdv_delete(FlowRadarSimdVPtr ptr) 
{
    rte_free(ptr->simdv_ptr);
    rte_free(ptr);
}

inline uint32_t flowradarsimdv_num_searches(FlowRadarSimdVPtr ptr)
{ 
    return (uint32_t)(rte_atomic32_read(&ptr->stats_search));
}







ModulePtr simdbatch_flowradarsimdv_init(ModuleConfigPtr conf) 
{
    uint32_t size    = mc_uint32_get(conf, "size");
    uint32_t keysize = mc_uint32_get(conf, "keysize");
    uint32_t elsize  = mc_uint32_get(conf, "valsize");
    uint32_t socket  = mc_uint32_get(conf, "socket");


    ModuleSimdBatchFlowRadarSimdVPtr module = rte_zmalloc_socket(0,
            sizeof(struct ModuleSimdBatchFlowRadarSimdV), 64, socket); 

    module->_m.execute = simdbatch_flowradarsimdv_execute;
    module->size  = size;
    module->keysize = keysize;
    module->elsize = elsize;
    module->socket = socket;

    module->flowradarsimdv_ptr1 = flowradarsimdv_create(size, keysize, elsize, socket);
    module->flowradarsimdv_ptr2 = flowradarsimdv_create(size, keysize, elsize, socket);

    module->flowradarsimdv = module->flowradarsimdv_ptr1;

    return (ModulePtr)module;
}

void simdbatch_flowradarsimdv_delete(ModulePtr module_) 
{
    ModuleSimdBatchFlowRadarSimdVPtr module = (ModuleSimdBatchFlowRadarSimdVPtr)module_;

    flowradarsimdv_delete(module->flowradarsimdv_ptr1);
    flowradarsimdv_delete(module->flowradarsimdv_ptr2);

    rte_free(module);
}

inline void simdbatch_flowradarsimdv_execute(
        ModulePtr module_,
        PortPtr port __attribute__((unused)),
        struct rte_mbuf ** __restrict__ pkts,
        uint32_t count) 
{
    
    (void)(port);

    ModuleSimdBatchFlowRadarSimdVPtr module = (ModuleSimdBatchFlowRadarSimdVPtr)module_;
    uint16_t i = 0;
    uint64_t timer = rte_get_tsc_cycles(); 
    (void)(timer);

    FlowRadarSimdVPtr flowradarsimdv = module->flowradarsimdv;

    /* Prefetch flowradarsimdv entries */
    for (i = 0; i < count; ++i) 
    {
        uint8_t const* pkt = rte_pktmbuf_mtod(pkts[i], uint8_t const*);
        flowradarsimdv_inc(flowradarsimdv, (pkt + 26));
    }
}

inline void simdbatch_flowradarsimdv_reset(ModulePtr module_) 
{
    ModuleSimdBatchFlowRadarSimdVPtr module = (ModuleSimdBatchFlowRadarSimdVPtr)module_;
    FlowRadarSimdVPtr prev = module->flowradarsimdv;

    if (module->flowradarsimdv == module->flowradarsimdv_ptr1) 
    {
        module->flowradarsimdv = module->flowradarsimdv_ptr2;
    } 
    else 
    {
        module->flowradarsimdv = module->flowradarsimdv_ptr1;
    }

    module->stats_search += flowradarsimdv_num_searches(prev);
    flowradarsimdv_reset(prev);
}

inline void simdbatch_flowradarsimdv_stats(ModulePtr module_, FILE *f) 
{
    ModuleSimdBatchFlowRadarSimdVPtr module = (ModuleSimdBatchFlowRadarSimdVPtr)module_;
    module->stats_search += flowradarsimdv_num_searches(module->flowradarsimdv);
    fprintf(f, "SimdBatch::FlowRadarSimdV::SearchLoad\t%u\n", module->stats_search);
}


