#ifndef _DEBUG_H_
#define _DEBUG_H_

#define sample_time(var, rate) if ((var & (rate-1)) == (rate-1))
#define sample_done(var, rate) {\
    var++; \
    var=(var & (rate-1)); \
}

#endif // _DEBUG_H_
