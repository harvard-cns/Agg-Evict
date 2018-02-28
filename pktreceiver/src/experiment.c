#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <yaml.h>

#include "module.h"
#include "net.h"

#include "experiment.h"

struct ExpModuleConfigParam {
    char *name;
    char *value;
};

struct ExpModuleConfig {
    char *name;
    ModulePtr   module;

    uint32_t     nparams;
    struct ExpModuleConfigParam *params[16];
};

struct ExpPortModules {
    uint32_t   port_id;
    uint32_t   nconfigs;
    struct ExpModuleConfig *configs[16];
    PortPtr     port;
};

struct Experiment {
    char   *name;
    struct ExpPortModules *port_modules[16];
    uint32_t nport_modules;
};

struct Experiments {
    struct Experiment *exprs[16];
    uint32_t nexprs;
};

typedef enum {
    ParserNodeRoot,
    ParserNodeExperiment,
    ParserNodePortModules,
    ParserNodeModuleConfig,
} ParserNodeEnum;

struct ParserNode {
    struct Experiments          *root;
    struct Experiment           *expr;
    struct ExpPortModules       *pm;
    struct ExpModuleConfig      *mc;

    ParserNodeEnum active;
    bool isValue;
    char lastVal[128];
};

static void
expr_print(ExprsPtr root) {
    uint32_t i = 0, j = 0, k = 0, l=0;
    for (i = 0; i < root->nexprs; ++i) {
        struct Experiment *expr = root->exprs[i];
        printf("> Experiment: %s\n", expr->name);
        for (j = 0; j < expr->nport_modules; ++j) {
            struct ExpPortModules *mods = expr->port_modules[j];
            printf("  > Port: %d\n", mods->port_id);
            for (k = 0; k < mods->nconfigs; ++k) {
                struct ExpModuleConfig *cfgs = mods->configs[k];
                printf("    > Module: %s\n", cfgs->name);
                for (l = 0; l < cfgs->nparams; ++l) {
                    printf("      > Params: %s -> %s\n", cfgs->params[l]->name, cfgs->params[l]->value);
                }
            }
        }
    }
}

static void
ParseScalar(struct ParserNode *pn, yaml_event_t *event) {
    char const* val = (char const *)event->data.scalar.value;

    if (pn->active == ParserNodeExperiment) {
        if (strcasecmp(pn->lastVal, "name") == 0) {
            pn->expr->name = malloc(strlen(val)+1);
            strcpy(pn->expr->name, val);
            strncpy(pn->lastVal, val, 128);
        } else {
            strncpy(pn->lastVal, val, 128);
        }

        return;
    }

    if (pn->active == ParserNodePortModules) {
        if (strcasecmp(pn->lastVal, "port-id") == 0) {
            pn->pm->port_id = atoi(val);
            strncpy(pn->lastVal, val, 128);
        } else {
            strncpy(pn->lastVal, val, 128);
        }

        return;
    }

    if (pn->active == ParserNodeModuleConfig) {
        if (!pn->isValue) {
            struct ExpModuleConfigParam *pair = calloc(1, sizeof(struct ExpModuleConfigParam));
            pn->mc->params[pn->mc->nparams] = pair;
            pair->name = malloc(strlen(val)+1);
            strcpy(pair->name , val);
            pn->isValue = true;
        } else {
            struct ExpModuleConfigParam *pair = pn->mc->params[pn->mc->nparams];
            pn->mc->nparams++;
            pair->value = malloc(strlen(val)+1);
            strcpy(pair->value, val);

            if (strcasecmp(pair->name, "name") == 0) {
                pn->mc->name = pair->value;
            }
            pn->isValue = false;
        }
    }
}

static void
ParseMappingStart(struct ParserNode *pn, yaml_event_t *event) {
    (void)(event);
    if (pn->active == ParserNodeRoot) {
        pn->active = ParserNodeExperiment;

        struct Experiment *expr = calloc(1, sizeof(struct Experiment));
        pn->root->exprs[pn->root->nexprs++] = expr;
        pn->expr = expr;
        return;
    }

    /* Mapping means nothing for ParserNodeExperiment we only expect sequences */

    if (pn->active == ParserNodePortModules) {
        struct ExpPortModules *epm = calloc(1, sizeof(struct ExpPortModules));
        pn->expr->port_modules[pn->expr->nport_modules++] = epm;
        pn->pm = epm;
        return;
    }

    if (pn->active == ParserNodeModuleConfig) {
        struct ExpModuleConfig *emc = calloc(1, sizeof(struct ExpModuleConfig));
        pn->pm->configs[pn->pm->nconfigs++] = emc;
        pn->mc = emc;
        pn->isValue = false;
        return;
    }
}

static void
ParseMappingEnd(struct ParserNode *pn, yaml_event_t *event) {
    (void)(pn);
    (void)(event);
    if (pn->active == ParserNodeExperiment) {
        pn->active = ParserNodeRoot;
        pn->expr = 0;
        return;
    }
}

static void
ParseSequenceStart(struct ParserNode *pn, yaml_event_t *event) {
    (void)(event);
    if (pn->active == ParserNodeExperiment) {
        if (strcasecmp(pn->lastVal, "ports") == 0) {
            pn->active = ParserNodePortModules;
        }
        return;
    }

    if (pn->active == ParserNodePortModules) {
        if (strcasecmp(pn->lastVal, "modules") == 0) {
            pn->active = ParserNodeModuleConfig;
        }
        return;
    }
}

static void
ParseSequenceEnd(struct ParserNode *pn, yaml_event_t *event) {
    (void)(event);
    if (pn->active == ParserNodeModuleConfig) {
        pn->active = ParserNodePortModules;
        pn->mc = 0;
        return;
    }

    if (pn->active == ParserNodePortModules) {
        pn->active = ParserNodeExperiment;
        pn->pm = 0;
        return;
    }

}


/* Sample experiment configuration:
 *
 * Experiment
 *      name:
 *      ports:
 *          - port-no: 1
 *            modules:
 *              - name: testing
 */
ExprsPtr expr_parse(char const *fname) {
    (void)(fname);
    FILE *fh = fopen(fname, "r");
    yaml_parser_t parser;
    yaml_event_t event;

    if (!yaml_parser_initialize(&parser)) {
        fputs("Failed to initialize the YAML parser!\n", stderr);
        exit(EXIT_FAILURE);
    }

    if (!fh) {
        fputs("Failed to open the file!\n", stderr);
        exit(EXIT_FAILURE);
    }

    yaml_parser_set_input_file(&parser, fh);

    ExprsPtr root = calloc(1, sizeof(struct Experiments));
    struct ParserNode pn = {
        .root = root,
        .active = ParserNodeRoot,
    };

    do {
        if (!yaml_parser_parse(&parser, &event)) {
            fputs("Failed to parse the yaml file.\n", stderr);
            exit(EXIT_FAILURE);
        }

        switch (event.type) {
            case YAML_NO_EVENT:           break;
            case YAML_STREAM_START_EVENT: break;
            case YAML_STREAM_END_EVENT:   break;
            case YAML_ALIAS_EVENT:        break;

            /* Block delimeters */
            case YAML_DOCUMENT_START_EVENT: break;
            case YAML_DOCUMENT_END_EVENT:   break;
            case YAML_SEQUENCE_START_EVENT: ParseSequenceStart(&pn, &event); break;
            case YAML_SEQUENCE_END_EVENT:   ParseSequenceEnd(&pn, &event);   break;
            case YAML_MAPPING_START_EVENT:  ParseMappingStart(&pn, &event);  break;
            case YAML_MAPPING_END_EVENT:    ParseMappingEnd(&pn, &event);    break;

            /* Data */
            case YAML_SCALAR_EVENT: ParseScalar(&pn, &event); break;
        }
        if(event.type != YAML_STREAM_END_EVENT) yaml_event_delete(&event);
    } while(event.type != YAML_STREAM_END_EVENT);

    yaml_event_delete(&event);
    yaml_parser_delete(&parser);
    fclose(fh);

    expr_print(root);
    return root;
}

void expr_initialize(ExprsPtr exprs, PortPtr port) {

    struct Experiment *expr = exprs->exprs[0];
    unsigned i = 0;
    unsigned pid = port_id(port);
    for (i = 0; i < expr->nport_modules; ++i) {
        struct ExpPortModules *pm = expr->port_modules[i];

        if (pid == pm->port_id) {
            pm->port = port;
            unsigned j = 0;

            for (j = 0; j < pm->nconfigs; ++j) {

                struct ExpModuleConfig *cfg = pm->configs[j];

                // Create the modules
                cfg->module = module_init_get(cfg->name)(cfg);


                // printf("fasfasfasdf\n");
                // Only support RX modules for now
                port_add_rx_module(port, cfg->module);
            }
        }
    }
}

void expr_cleanup(ExprsPtr exprs) {
    struct Experiment *expr = exprs->exprs[0];
    unsigned i = 0;
    for (i = 0; i < expr->nport_modules; ++i) {
        struct ExpPortModules *pm = expr->port_modules[i];
        unsigned j = 0;
        for (j = 0; j < pm->nconfigs; ++j) {
            struct ExpModuleConfig *cfg = pm->configs[j];

            if (cfg->module) module_delete_get(cfg->name)(cfg->module);
        }
    }
}

void expr_stats_save(ExprsPtr exprs, FILE *f) {
    struct Experiment *expr = exprs->exprs[0];
    unsigned i = 0;
    for (i = 0; i < expr->nport_modules; ++i) {
        struct ExpPortModules *pm = expr->port_modules[i];
        unsigned j = 0;
        for (j = 0; j < pm->nconfigs; ++j) {
            struct ExpModuleConfig *cfg = pm->configs[j];
            if (cfg->module) module_stats_get(cfg->name)(cfg->module, f);
        }
    }
}

void expr_signal(ExprsPtr exprs) {
    struct Experiment *expr = exprs->exprs[0];
    unsigned i = 0;
    for (i = 0; i < expr->nport_modules; ++i) {
        struct ExpPortModules *pm = expr->port_modules[i];
        unsigned j = 0;
        if (port_has_ticked(pm->port)) {
            port_clear_tick(pm->port);
            for (j = 0; j < pm->nconfigs; ++j) {
                struct ExpModuleConfig *cfg = pm->configs[j];

                if (cfg->module) module_reset_get(cfg->name)(cfg->module);
            }
        }
    }
}

inline static struct ExpModuleConfigParam *
find_key(ModuleConfigPtr ptr, char const *name) {
    unsigned i = 0;
    for (i = 0; i < ptr->nparams; ++i) {
        if (strcasecmp(ptr->params[i]->name, name) == 0)
            return ptr->params[i];
    }

    printf("Need to specify: %s for %s\n", name, ptr->name);
    exit(EXIT_FAILURE);
    return 0;
}

uint32_t mc_uint32_get(ModuleConfigPtr ptr, char const *n) {
    return strtoul(find_key(ptr, n)->value, 0, 0);
}

unsigned mc_unsigned_get(ModuleConfigPtr ptr, char const *n) {
    return mc_uint32_get(ptr, n);
}

char const *mc_string_get(ModuleConfigPtr ptr, char const *n) {
    return find_key(ptr, n)->value;
}
