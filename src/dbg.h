/* This header is almost identical to the "dbg.h" from Zed Shaw's "Learn C The Hard Way", expect for:

    1) Substitution of the GCC extension ##__VA_ARGS__ with the plain standard-compliant __VA_ARGS__ macro.
    2) Removal of check_mem(A), a specific memory-checking version of check(A, M, ...).
    3) Addition of error message templates.
    4) Some code reformatting.

Change 1) results in the variadic macros not accepting zero arguments for __VA_ARGS__. This is not an issue if a
format string with 1 or more arguments is used with them. */

#ifndef __dbg_h__
#define __dbg_h__

#include <stdio.h>
#include <errno.h>
#include <string.h>

#ifdef NDEBUG
    #define debug(M, ...)
#else
    #define debug(M, ...) fprintf(stderr,\
                "DEBUG %s:%d: " M "\n",\
                __FILE__, __LINE__, __VA_ARGS__)
#endif

#define clean_errno() (errno == 0 ? "None" : strerror(errno))

#define log_err(M, ...) fprintf(stderr,\
            "[ERROR] (%s:%d: errno: %s) " M "\n",\
            __FILE__, __LINE__,\
            clean_errno(), __VA_ARGS__)

#define log_warn(M, ...) fprintf(stderr,\
            "[WARN] (%s:%d: errno: %s) " M "\n",\
            __FILE__, __LINE__,\
            clean_errno(), __VA_ARGS__)

#define log_info(M, ...) fprintf(stderr,\
            "[INFO] (%s:%d) " M "\n",\
            __FILE__, __LINE__, __VA_ARGS__)

#define check(A, M, ...) if(!(A)) {\
            log_err(M, __VA_ARGS__);\
            errno=0;\
            goto error;\
        }

#define sentinel(M, ...) {\
            log_err(M, __VA_ARGS__);\
            errno=0;\
            goto error;\
        }

#define check_debug(A, M, ...) if(!(A)) {\
            debug(M, __VA_ARGS__);\
            errno=0;\
            goto error;\
        }

#define ERR_MEM "[%s]: Out of memory."
#define ERR_FAIL "[%s]: Failed %s."
#define ERR_FAIL_A "[%s]: Failed %s %s."
#define ERR_IMPRO "[%s]: Improper %s (got %d)."
#define ERR_INVAL "[%s]: Invalid %s (got %d)."
#define ERR_UNDEF "[%s]: Undefined %s (got %d)."
#define ERR_NALLOW "[%s] %s not allowed."
#define ERR_NALLOW_A "[%s] %s not allowed, use %s instead."


#endif
