#ifndef _REPORTER_H_
#define _REPORTER_H_

typedef struct Reporter * ReporterPtr;

/* Not thread safe ... should only be called from the same thread.
 * Everything except add_entry should be called from the same thread.
 * Even though this is not thread-safe, add_entry can be called from
 * other threads ... but will cause minor errors? */

struct Reporter {
    unsigned size;
    unsigned rowsize;
    unsigned counter;
    unsigned socket;
    unsigned version;
    unsigned idx;
    unsigned offline_idx;
    char fname_format[128];

    uint8_t *active;
    uint8_t *offline;

    uint8_t *ptr1;
    uint8_t *ptr2;
};

ReporterPtr reporter_init(unsigned, unsigned, unsigned, char const *fname_format);
void reporter_add_entry(ReporterPtr, void const*);
void reporter_swap(ReporterPtr); 
void reporter_reset(ReporterPtr); 

uint8_t *reporter_begin(ReporterPtr); /* Always iterate over the offline pointer */
uint8_t *reporter_end(ReporterPtr);
uint8_t *reporter_next(ReporterPtr, void *);

unsigned reporter_version(ReporterPtr);
void reporter_tick(ReporterPtr);
void reporter_free(ReporterPtr);

void reporter_save(ReporterPtr rep, const char *fname, 
        void (*)(FILE *, void *, unsigned));

#endif // _REPORTER_H_
