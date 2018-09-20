#ifndef __yogiexpr_h__
#define __yogiexpr_h__

#include <stdint.h>
#include <stdlib.h>
#include "./dependencies/bstrlib.h"
#include "dbg.h"

// Representation of the db table "Expression":
typedef struct expression {
    uint16_t id;
    uint8_t type;
    uint8_t flags;
    bstring field_1;
    bstring field_2;
} Expression;

// Expression types:
#define VERB 0
#define ADJECTIVE 1
#define CONCEPT 2
#define OBJECT 3

// Expression flags currently used:
#define EXPR_NO_FLAGS 0
#define A_OFLAG_AN 1
#define VFLAG_NPOSTP_CEPT_OK 1
#define VFLAG_NPOSTP_ALL_OK 2
#define VFLAG_NPOSTP_OBJ_OK 3

// Representation of the db table "Format_String":
typedef struct format_str {
    bstring str;
    uint64_t data;
    uint8_t count;
} Format_String;

/* Field "data" in the format_str struct contains 8x8 bit (accessed as uint8_t) fields, of which a number indicated by count ([0,7], indexing from the little end) contain data for one substitution in the following manner:

bit no. 7  6  5 | 4  3 | 2  1 | 0
        flag     type   expr   expr
                 index  type   mode

The Format_String flags are enumerated below:
*/

#define FSTR_NO_FLAGS 0
#define FSTR_UNIQUE 1
#define FSTR_POSTPOS 2
#define FSTR_PREPOS 3 // NOT USED IN PRACTICE YET
#define FSTR_FLAG_4 4 // NOT IN USE
#define FSTR_INDEF_ART 5
#define FSTR_FLAG_6 6 // NOT IN USE
#define FSTR_FLAG_7 7 // NOT IN USE

// A macro to extract bytes cleanly from the data field by offset:
#define byte_of(x,y) (((uint8_t *)x)[(y)])

// A helper struct for string construction-time storage of expression substitution data
typedef struct substitution_data {
    bstring str;
    int8_t count;
    int16_t typewise_idx[4][4];
    uint8_t types[8];
    uint8_t modes[8];
    uint8_t flags[8];
    int16_t ins[8];
} Substitution_Data;

#endif
