#ifndef _VARY_KEYSIZE_COMMON_H
#define _VARY_KEYSIZE_COMMON_H


#define SIP
// #define DIP
// #define SD_PAIR
// #define FOUR_TUPLE
// #define FIVE_TUPLE

#define GRR
// #define LRU




// for flowradarv
struct CellV
{
#ifdef SIP
    uint32_t sip;
#endif
#ifdef DIP
    uint32_t dip;
#endif
#ifdef SD_PAIR
    uint32_t sip;
    uint32_t dip;
#endif
#ifdef FOUR_TUPLE
    uint32_t sip;
    uint32_t dip;
    uint32_t port;
#endif
#ifdef FIVE_TUPLE
    uint32_t sip;
    uint32_t dip;
    uint32_t port;
    uint32_t type;
#endif

    uint32_t flowc;
    uint32_t packetc;
};

typedef struct CellV* CellVPtr;




// for simd part storing the flowid;
struct SimdCell
{

#ifdef SIP

#endif 

#ifdef DIP

#endif

#ifdef SD_PAIR
	uint32_t flowid[2];	
#endif

#ifdef FOUR_TUPLE
	uint32_t flowid[3];	
#endif

#ifdef FIVE_TUPLE
	uint32_t flowid[4];		
#endif
};

typedef struct SimdCell * SimdCellPtr;


// for simd part
struct SimdV
{    
    uint32_t *key;
    uint32_t *value;

#ifdef SIP

#endif 

#ifdef DIP

#endif

#ifdef SD_PAIR
    SimdCellPtr simdcellptr;
#endif

#ifdef FOUR_TUPLE
    SimdCellPtr simdcellptr;
#endif

#ifdef FIVE_TUPLE
    SimdCellPtr simdcellptr;
#endif

    uint32_t *curpos;

#ifdef GRR
    uint32_t curkick;    
#endif

#ifdef LRU
    uint32_t time_now;
    uint32_t *myclock;
#endif


    uint32_t table[];
};

typedef struct SimdV* SimdVPtr;


#ifdef GRR
#ifdef SIP
#define SIMDV_SIZE (2 * SIMD_ROW_NUM * 16 * sizeof(uint32_t) + SIMD_ROW_NUM * sizeof(uint32_t))
#endif 

#ifdef DIP
#define SIMDV_SIZE (2 * SIMD_ROW_NUM * 16 * sizeof(uint32_t) + SIMD_ROW_NUM * sizeof(uint32_t))
#endif

#ifdef SD_PAIR
#define SIMDV_SIZE (4 * SIMD_ROW_NUM * 16 * sizeof(uint32_t) + SIMD_ROW_NUM * sizeof(uint32_t))
#endif

#ifdef FOUR_TUPLE
#define SIMDV_SIZE (5 * SIMD_ROW_NUM * 16 * sizeof(uint32_t) + SIMD_ROW_NUM * sizeof(uint32_t))
#endif

#ifdef FIVE_TUPLE
#define SIMDV_SIZE (6 * SIMD_ROW_NUM * 16 * sizeof(uint32_t) + SIMD_ROW_NUM * sizeof(uint32_t))
#endif
#endif

#ifdef LRU
#ifdef SIP
#define SIMDV_SIZE (3 * SIMD_ROW_NUM * 16 * sizeof(uint32_t) + SIMD_ROW_NUM * sizeof(uint32_t))
#endif 

#ifdef DIP
#define SIMDV_SIZE (3 * SIMD_ROW_NUM * 16 * sizeof(uint32_t) + SIMD_ROW_NUM * sizeof(uint32_t))
#endif

#ifdef SD_PAIR
#define SIMDV_SIZE (5 * SIMD_ROW_NUM * 16 * sizeof(uint32_t) + SIMD_ROW_NUM * sizeof(uint32_t))
#endif

#ifdef FOUR_TUPLE
#define SIMDV_SIZE (6 * SIMD_ROW_NUM * 16 * sizeof(uint32_t) + SIMD_ROW_NUM * sizeof(uint32_t))
#endif

#ifdef FIVE_TUPLE
#define SIMDV_SIZE (7 * SIMD_ROW_NUM * 16 * sizeof(uint32_t) + SIMD_ROW_NUM * sizeof(uint32_t))
#endif
#endif


#endif // _VARY_KEYSIZE_COMMON_H
