#include "module.h"
#include "bootstrap.h"

// #include "modules/heavyhitter/countmin.h"
// #include "modules/heavyhitter/countarray.h"

// #include "modules/heavyhitter/cuckoo.h"
// #include "modules/heavyhitter/cuckoo_bucket.h"
// #include "modules/heavyhitter/cuckoo_local.h"
// #include "modules/heavyhitter/cuckoo_local_ptr.h"
// #include "modules/heavyhitter/hashmap.h"
// #include "modules/heavyhitter/hashmap_linear.h"
// #include "modules/heavyhitter/hashmap_linear_ptr.h"
// #include "modules/heavyhitter/pqueue.h"
// #include "modules/heavyhitter/sharedmap.h"

// #include "modules/superspreader/cuckoo_bucket.h"
// #include "modules/superspreader/cuckoo_local.h"
// #include "modules/superspreader/hashmap.h"
// #include "modules/superspreader/hashmap_linear.h"

// #include "modules/randmod.h"



// #define SIMDBATCH_EXP


// #ifdef SIMDBATCH_EXP

#include "modules/simdbatch/cmsketch.h"
#include "modules/simdbatch/cmsketchsimd.h"

#include "modules/simdbatch/fmsketch.h"
#include "modules/simdbatch/fmsketchsimd.h"

#include "modules/simdbatch/flowradar.h"
#include "modules/simdbatch/flowradarsimd.h"
#include "modules/simdbatch/flowradarnosimd.h"

#include "modules/simdbatch/linearcounting.h"
#include "modules/simdbatch/linearcountingsimd.h"

#include "modules/simdbatch/mrac.h"
#include "modules/simdbatch/mracsimd.h"

#include "modules/simdbatch/spacesaving.h"
#include "modules/simdbatch/spacesavingsimd.h"


#include "modules/simdbatch/univmon.h"
#include "modules/simdbatch/univmonsimd.h"

#include "modules/simdbatch/revsketch.h"
#include "modules/simdbatch/revsketchsimd.h"

#include "modules/simdbatch/twolevel.h"
#include "modules/simdbatch/twolevelsimd.h"

#include "modules/simdbatch/vary-keysize/flowradarv.h"
#include "modules/simdbatch/vary-keysize/flowradarsimdv.h"

// #else

// #include "modules/heavychange/univmon.h"
// #include "modules/heavychange/revsketch.h"
// #include "modules/heavychange/minhash.h"
// #include "modules/heavychange/pkttest.h"

// #endif


void boostrap_register_modules(void) {



// #ifndef SIMDBATCH_EXP
    // REGISTER_MODULE("HeavyChange::UnivMon",                 heavychange_univmon);
    // REGISTER_MODULE("HeavyChange::RevSketch",               heavychange_revsketch);
    // REGISTER_MODULE("HeavyChange::MinHash",                 heavychange_minhash);
    // REGISTER_MODULE("HeavyChange::PktTest",                 heavychange_pkttest);

// #else

    REGISTER_MODULE("SimdBatch::FlowRadarSimdV",            simdbatch_flowradarsimdv);
    REGISTER_MODULE("SimdBatch::FlowRadarV",                simdbatch_flowradarv);
    
    REGISTER_MODULE("SimdBatch::TwoLevelSimd",              simdbatch_twolevelsimd);
    REGISTER_MODULE("SimdBatch::TwoLevel",                  simdbatch_twolevel);

    REGISTER_MODULE("SimdBatch::RevSketchSimd",             simdbatch_revsketchsimd);
    REGISTER_MODULE("SimdBatch::RevSketch",                 simdbatch_revsketch);

    REGISTER_MODULE("SimdBatch::UnivMonSimd",               simdbatch_univmonsimd);
    REGISTER_MODULE("SimdBatch::UnivMon",                   simdbatch_univmon);

    REGISTER_MODULE("SimdBatch::SpaceSavingSimd",           simdbatch_spacesavingsimd);
    REGISTER_MODULE("SimdBatch::SpaceSaving",               simdbatch_spacesaving);

    REGISTER_MODULE("SimdBatch::MRACSimd",                  simdbatch_mracsimd);
    REGISTER_MODULE("SimdBatch::MRAC",                      simdbatch_mrac);

    REGISTER_MODULE("SimdBatch::LinearCountingSimd",        simdbatch_linearcountingsimd);
    REGISTER_MODULE("SimdBatch::LinearCounting",            simdbatch_linearcounting);

    REGISTER_MODULE("SimdBatch::FlowRadarNoSimd",           simdbatch_flowradarnosimd);
    REGISTER_MODULE("SimdBatch::FlowRadarSimd",             simdbatch_flowradarsimd);
    REGISTER_MODULE("SimdBatch::FlowRadar",                 simdbatch_flowradar);

    REGISTER_MODULE("SimdBatch::FMSketchSimd",              simdbatch_fmsketchsimd);
    REGISTER_MODULE("SimdBatch::FMSketch",                  simdbatch_fmsketch);

    REGISTER_MODULE("SimdBatch::CMSketchSimd",              simdbatch_cmsketchsimd);
    REGISTER_MODULE("SimdBatch::CMSketch",                  simdbatch_cmsketch);

// #endif

    // REGISTER_MODULE("HeavyHitter::CountMin",                heavyhitter_countmin);
    // REGISTER_MODULE("HeavyHitter::CountArray",              heavyhitter_countarray);
    

    // REGISTER_MODULE("HeavyHitter::Cuckoo",                  heavyhitter_cuckoo);
    // REGISTER_MODULE("HeavyHitter::CuckooBucket",            heavyhitter_cuckoo_bucket);
    // REGISTER_MODULE("HeavyHitter::CuckooLocal",             heavyhitter_cuckoo_local);
    // REGISTER_MODULE("HeavyHitter::CuckooLocalPointer",      heavyhitter_cuckoo_local_ptr);
    // REGISTER_MODULE("HeavyHitter::Hashmap",                 heavyhitter_hashmap);
    // REGISTER_MODULE("HeavyHitter::HashmapLinear",           heavyhitter_hashmap_linear);
    // REGISTER_MODULE("HeavyHitter::HashmapLinearPointer",    heavyhitter_hashmap_linear_ptr);
    // REGISTER_MODULE("HeavyHitter::PQueue",                  heavyhitter_pqueue);
    // REGISTER_MODULE("HeavyHitter::Sharedmap",               heavyhitter_sharedmap);

    // REGISTER_MODULE("SuperSpreader::Hashmap",               superspreader_hashmap);
    // REGISTER_MODULE("SuperSpreader::HashmapLinear",         superspreader_hashmap_linear);
    // REGISTER_MODULE("SuperSpreader::CuckooLocal",           superspreader_cuckoo_local);
    // REGISTER_MODULE("SuperSpreader::CuckooBucket",          superspreader_cuckoo_bucket);

    // REGISTER_MODULE("Generic::RandMod",                     randmod);
}
