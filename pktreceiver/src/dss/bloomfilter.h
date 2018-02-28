#ifndef _DSS_BLOOMFILTER_
#define _DSS_BLOOMFILTER_

#define BF_SHIFT 10
#define BF_SIZE (1<<10)
#define BF_MASK (BF_SIZE-1)
#define BF_BUCKET_SHIFT (BF_SHIFT>>1)
#define BF_BUCKET_MASK ((1<<BF_BUCKET_SHIFT)-1)

struct BFProp {
    int reserved; // nbits later?
    int keylen;
};

// Implementation of a 1024 bit bloomfilter with 3 hash functions
typedef struct BFProp * BFPropPtr;

int bloomfilter_add_key(BFPropPtr bfp, void *bf, void const *key);
int bloomfilter_is_member(BFPropPtr bfp, void *bf, void const *key);
void bloomfilter_reset(BFPropPtr bfp, void *);

#endif
