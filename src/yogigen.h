#ifndef __yogigen_h__
#define __yogigen_h__

#include <ctype.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/random.h>
#include "./dependencies/bstrlib.h"
#include "./dependencies/monocypher.h"
#include "dbg.h"
#include "yogiexpr.h"
#include "yogidb.h"

// The main YogiGen struct that is currently exposed
typedef struct yogigen {
    Expression *verbs;
    uint16_t verb_count;
    Expression *adjs;
    uint16_t adj_count;
    Expression *cepts;
    uint16_t cept_count;
    Expression *objs;
    uint16_t obj_count;
    Format_String *formats;
    uint16_t formats_count;
    Connection *conn;
} YogiGen;

// Representation of the db table "Generated" for YogiGen's output with a hash for user access
typedef struct generated {
    YogiGen *yogen_addr;
    uint64_t rnd_id;
    bstring str;
} Generated;

YogiGen *YogiGen_init();
uint8_t YogiGen_fetch_all(YogiGen *yogen);
bstring YogiGen_generate(YogiGen *yogen);
uint8_t YogiGen_insert_into_db(YogiGen *yogen, Generated *gen);
bstring YogiGen_get_by_id(YogiGen* yogen, bstring hash_str);
void YogiGen_close(YogiGen *yogen);

#endif
