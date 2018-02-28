#include <string.h>
#include "module.h"

struct ModuleList _all_modules;

void _module_register(
        char const* name,
        ModulePtr (*init) (ModuleConfigPtr),
        void (*del)(ModulePtr),
        void (*execute)(ModulePtr, PortPtr, struct rte_mbuf **, uint32_t),
        void (*reset)(ModulePtr),
        void (*stats)(ModulePtr, FILE*)) {
    struct ModuleList *lst = malloc(sizeof(struct ModuleList));
    lst->next = _all_modules.next;
    lst->init = init;
    lst->del  = del;
    lst->execute = execute;
    lst->reset = reset;
    lst->stats = stats;
    strncpy(lst->name, name, 255);

    // printf("register name = %s\n", name);
   // fflush(stdout);
	
    _all_modules.next = lst;
}

static struct ModuleList *module_get(char const *name) {
    //printf("1\n");
    struct ModuleList *ptr = &_all_modules;
    
    //printf("2\n");
    while (ptr) {
	// printf("name = %s, ptr->name = %s\n", name, ptr->name);
       // fflush(stdout);

	if (strcasecmp(name, ptr->name) == 0)
            return ptr;
        ptr = ptr->next;
        //printf("3\n");
    }

    //printf("4\n");
    printf("Failed to find module: %s\n", name);
    exit(EXIT_FAILURE);
    return 0;
}

ModuleInit module_init_get(char const* name) {
 //   printf("9\n");fflush(stdout);
    return module_get(name)->init;
}

ModuleDelete module_delete_get(char const* name) {
    return module_get(name)->del;
}

ModuleExecute module_execute_get(char const* name) {
    return module_get(name)->execute;
}

ModuleReset module_reset_get(char const* name) {
    return module_get(name)->reset;
}

ModuleStats module_stats_get(char const* name) {
    return module_get(name)->stats;
}
