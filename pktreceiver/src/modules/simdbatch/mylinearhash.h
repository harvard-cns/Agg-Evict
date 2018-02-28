#ifndef _MY_LINEAR_HASH_H
#define _MY_LINEAR_HASH_H


#define EMPTY 0
#define DELETE 1
#define OCCUPY 2

struct hashcell
{
   uint32_t key;
   uint32_t value;
   uint32_t status;
};

typedef struct hashcell * hashcell_p;

struct l_hash
{
   uint32_t size;
   hashcell_p array;

   uint32_t table[];
};

typedef struct l_hash * l_hash_p;

inline l_hash_p l_hash_create(uint32_t size);
inline void l_hash_insert(l_hash_p ptr, uint32_t key, uint32_t value);
inline void l_hash_delete(l_hash_p ptr, uint32_t key);
inline uint32_t l_hash_find(l_hash_p ptr, uint32_t key);
inline uint32_t * l_hash_find_pointer(l_hash_p ptr, uint32_t key);
inline void l_hash_reset(l_hash_p ptr);
inline void l_hash_des(l_hash_p ptr);
#endif