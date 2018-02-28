#ifndef TLOAD_H
#define TLOAD_H

#include <stdio.h>
#include "pm.h"

#define INVALID_LINE -1
#define END_LINE 0
#define VALID_LINE 1

// #define NB_MAX_PM 30000001
 // #define NB_MAX_PM (2 * 1024 * 1024 * 2 - 1)
#define NB_MAX_PM (2 * 1024 * 1024 * 14 - 1)
// #define NB_MAX_PM 29360127



#define NB_FIELD 5

int load_trace_line(FILE *fp, struct packet_model *pm);
int load_trace(const char *file, struct packet_model pms[]);


// int load_vxlan_trace_line(FILE *fp, struct packet_model *pm);
// int load_vxlan_trace(const char *file, struct packet_model pms[]);


#endif
