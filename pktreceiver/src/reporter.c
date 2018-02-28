#include <string.h>

#include "rte_malloc.h"
#include "rte_memcpy.h"

#include "common.h"

#include "reporter.h"

ReporterPtr reporter_init( unsigned size, unsigned rowsize, unsigned socket, char const *fname_format) {
    ReporterPtr reporter = rte_zmalloc_socket(
            0, sizeof(struct Reporter), 64, socket);

// printf("11\n");fflush(stdout);
    // printf("rowsize * size = %d\n", rowsize * size);

    // printf("%d\n", socket);
    reporter->ptr1    = rte_zmalloc_socket(0, rowsize * size, 64, socket);

    // printf("rowsize * size = %d\n", rowsize * size);

    reporter->ptr2    = rte_zmalloc_socket(0, rowsize * size, 64, socket);

// printf("12\n");fflush(stdout);  
    
    reporter->active  = reporter->ptr1;
    reporter->offline = reporter->ptr2;
    reporter->size    = size;
    reporter->socket  = socket;
    reporter->rowsize = rowsize;
    reporter->idx     = 0;
    reporter->version = 0;
    strncpy(reporter->fname_format, fname_format, 128);

// printf("13\n");fflush(stdout);
    return reporter;
}

inline void
reporter_add_entry(ReporterPtr rep, void const *entry) {
    if (rep->idx >= rep->size) return;
    rte_memcpy(rep->active + (rep->rowsize * rep->idx), entry, rep->rowsize);
    rep->idx++;
}

void reporter_swap(ReporterPtr rep) {
    rep->offline_idx = rep->idx;
    rep->idx = 0;

    if (rep->active == rep->ptr2) {
        rep->active = rep->ptr1;
        rep->offline = rep->ptr2;
    } else if (rep->active == rep->ptr1) {
        rep->active = rep->ptr2;
        rep->offline = rep->ptr1;
    }

    rep->version++;
}

static inline void
_rsave(FILE *fp, void *data, unsigned unused) {
    (void)(unused);
    unsigned char *key = (unsigned char*)(data);
    fprintf(fp, "%u.%u.%u.%u/%u.%u.%u.%u %u\n",
            *(key+0), *(key+1), *(key+2), *(key+3),
            *(key+4), *(key+5), *(key+6), *(key+7),
            HEAVYHITTER_THRESHOLD);
}

void reporter_tick(ReporterPtr ptr) {
    uint32_t version = reporter_version(ptr);
    char buf[255] = {0};
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
    snprintf(buf, 255, ptr->fname_format, version);
#pragma GCC diagnostic pop
    reporter_swap(ptr);
    reporter_save(ptr, buf, _rsave);
    reporter_reset(ptr);
}

uint8_t *reporter_begin(ReporterPtr rep) {
    return rep->offline;
}

uint8_t *reporter_end(ReporterPtr rep) {
    return rep->offline + (rep->rowsize * rep->offline_idx);
}

uint8_t *reporter_next(ReporterPtr rep, void *ptr) {
    return ((uint8_t*)ptr) + (rep->rowsize);
}

unsigned reporter_version(ReporterPtr rep) { return rep->version; }

void reporter_save(ReporterPtr rep, const char *fname, 
        void (*func)(FILE *, void *, unsigned)) {
    FILE *fp = fopen(fname, "w+");
    uint8_t *ptr = reporter_begin(rep);
    uint8_t *end = reporter_end(rep);
    while (ptr < end) {
        func(fp, ptr, 0);
        ptr = reporter_next(rep, ptr);
    }
    fclose(fp);
}

void reporter_reset(ReporterPtr rep) {
    memset(rep->offline, 0, rep->rowsize * rep->offline_idx);
}

void reporter_free(ReporterPtr rep) {
    rte_free(rep->ptr1);
    rte_free(rep->ptr2);
    rte_free(rep);
}
