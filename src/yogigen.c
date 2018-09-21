#include "yogigen.h"
#include "dbg.h"

static void read_count_result_set(void *arg, PGresult *res_set)
{
    YogiGen *yogen = (YogiGen*) arg;
    uint16_t count = (uint16_t) atoi(PQgetvalue(res_set, 0, 0));
    if (PQnfields(res_set) > 1) {
        yogen->verb_count = count;
        count = (uint16_t) atoi(PQgetvalue(res_set, 0, 1));
        yogen->adj_count = count;
        count = (uint16_t) atoi(PQgetvalue(res_set, 0, 2));
        yogen->cept_count = count;
        count = (uint16_t) atoi(PQgetvalue(res_set, 0, 3));
        yogen->obj_count = count;
    } else {
        yogen->formats_count = count;
    }
}

static void read_frmt_result_set(void *arg, PGresult *res_set)
{
    Format_String *arr = (Format_String*) arg;
    uint16_t count = (uint16_t) PQntuples(res_set);
    for (uint16_t i = 0; i < count; i++) {
        uint16_t id = (uint16_t) atoi(PQgetvalue(res_set, i, 0)) - 1;
        arr[id].str = bfromcstr(PQgetvalue(res_set, i, 1));
        arr[id].data = (uint64_t) atol(PQgetvalue(res_set, i, 2));
        arr[id].count = (uint8_t) atoi(PQgetvalue(res_set, i, 3));
    }
}

static void read_expr_result_set(void *arg, PGresult *res_set)
{
    Expression *arr = (Expression*) arg;
    uint16_t count = (uint16_t) PQntuples(res_set);
    for (uint16_t i = 0; i < count; i++) {
        uint16_t id = (uint16_t) atoi(PQgetvalue(res_set, i, 0));
        arr[id].id = id;
        arr[id].field_1 = bfromcstr(PQgetvalue(res_set, i, 1));
        arr[id].field_2 = bfromcstr(PQgetvalue(res_set, i, 2));
        arr[id].type = (uint8_t) atoi(PQgetvalue(res_set, i, 3));
        arr[id].flags = (uint8_t) atoi(PQgetvalue(res_set, i, 4));
    }
}


static uint8_t fetch_counts(YogiGen *yogen)
{
    int ret;
    ret = postgres_select_cycle(yogen->conn->db, SQL_EXPR_COUNT_PG,
       read_count_result_set, yogen);
    check(ret, "Postgres: %s", PQerrorMessage(yogen->conn->db));
    ret = postgres_select_cycle(yogen->conn->db, SQL_FRMT_COUNT_PG,
      read_count_result_set, yogen);
    check(ret, "Postgres: %s", PQerrorMessage(yogen->conn->db));
    return 1;
error:
    return 0;
}

static uint8_t fetch_formats(YogiGen *yogen)
{
    int ret;
    char *query = FRMT_SQL_SELECT_TEMPLATE;
    ret = postgres_select_cycle(yogen->conn->db, query, read_frmt_result_set, yogen->formats);
    check(ret, "Postgres: %s", PQerrorMessage(yogen->conn->db));
    return 1;
error:
    return 0;
}

static uint8_t fetch_expressions(YogiGen *yogen)
{
    int ret;
    uint8_t i = 0;
    uint16_t factor = 1;
    const char *template = EXPR_SQL_SELECT_TEMPLATE;
    Expression *arr_ptrs[4] = {yogen->verbs, yogen->adjs, yogen->cepts, yogen->objs};
    uint16_t arr_size[4] = {yogen->verb_count, yogen->adj_count, yogen->cept_count, yogen->obj_count};
    bstring query = bfromcstr(template);
    do {
        bassignformat(query, template, factor, i);
        ret = postgres_select_cycle(yogen->conn->db, bdata(query),
        read_expr_result_set, arr_ptrs[i]);
        check(ret, "Postgres: %s", PQerrorMessage(yogen->conn->db));
        factor += arr_size[i];
        i++;
    } while (i < 4);
    bdestroy(query);
    return 1;
error:
    if (query) bdestroy(query);
    return 0;
}


static Format_String *get_formats(YogiGen *yogen, uint16_t id)
{
    check(id < yogen->formats_count, "Invalid index %d", id);
    return &yogen->formats[id];
error:
    return NULL;
}

static Format_String *random_formats(YogiGen *yogen)
{
    uint16_t rnd;
    ssize_t ret = getrandom(&rnd, sizeof(uint16_t), 0);
    check(ret != -1, "Failed to source random bytes.");
    rnd %= yogen->formats_count;
    return get_formats(yogen, rnd);
error:
    return NULL;
}

static Expression *get_expression(YogiGen *yogen, uint16_t id, uint16_t type)
{
    switch (type) {
        case VERB:
            check(id < yogen->verb_count, "Invalid index %d", id);
            return &yogen->verbs[id];
        case ADJECTIVE:
            check(id < yogen->adj_count, "Invalid index %d", id);
            return &yogen->adjs[id];
        case CONCEPT:
            check(id < yogen->cept_count, "Invalid index %d", id);
            return &yogen->cepts[id];
        case OBJECT:
            check(id < yogen->obj_count, "Invalid index %d", id);
            return &yogen->objs[id];
        default:
            sentinel("Unknown type code %d\n", type);
error:      return NULL;
    }
}

static uint16_t random_expression_id(YogiGen *yogen, uint8_t type, uint8_t flag, void *ref_data)
{
    uint16_t limit;
    uint16_t res_id = 0;
    switch (type) {
        case VERB:
            limit = yogen->verb_count;
            break;
        case ADJECTIVE:
            limit = yogen->adj_count;
            break;
        case CONCEPT:
            limit = yogen->cept_count;
            break;
        case OBJECT:
            limit = yogen->obj_count;
            break;
        default:
            sentinel("Unknown type code %d\n", type);
    }
    ssize_t ret = getrandom(&res_id, sizeof(uint16_t), 0);
    check(ret != -1, "Failed to source random bytes.");
    res_id %= limit;
    // Flag check and handling:
    if (flag) {
        if (flag == FSTR_UNIQUE) {
            int16_t *used_ids = (int16_t*) ref_data;
            uint8_t i = 0;
            while (i < 4) {
                if (used_ids[i] >= 0 && used_ids[i] == res_id) {
                    ssize_t ret = getrandom(&res_id, sizeof(uint16_t), 0);
                    check(ret != -1, "Failed to source random bytes.");
                    res_id %= limit;
                    i = 0;
                } else {
                    i++;
                }
            }
        } else if (flag == FSTR_POSTPOS) {
            uint8_t *types = (uint8_t*) ref_data;
            uint8_t type_before = types[0];
            if (type_before >= 2) {
                log_warn("FSTR_POSTPOS should only occur after types %d and %d\n", CONCEPT, OBJECT);
                return res_id;
            }
            uint16_t pos_x;
            uint16_t pos_y;
            uint16_t i = 0;
            do {
                i++;
            } while (yogen->verbs[i].flags < VFLAG_NPOSTP_OBJ_OK);
            pos_x = i;
            i = limit;
            do {
                i--;
            } while (yogen->verbs[i].flags > VFLAG_NPOSTP_ALL_OK);
            pos_y = ++i;
            ssize_t ret = getrandom(&res_id, sizeof(uint16_t), 0);
            check(ret != -1, "Failed to source random bytes.");
            if (type_before == OBJECT) {
                res_id %= (limit - pos_y);
                res_id += pos_y;
            } else {
                res_id %= (pos_y - pos_x);
                res_id += pos_x;
            }
        } else if (flag == FSTR_PREPOS) {
            sentinel("Format string flag no %d: FSTR_PREPOS is not yet implemented.", flag);
        } else if (flag == FSTR_INDEF_ART) {
            sentinel("Format string flag no %d: FSTR_INDEF_ART shouldn't be passed onto this function at all.", flag);
        } else {
            sentinel("Format string flag no %d is undefined.", flag);
        }
    }
    return res_id;
error:
    res_id = UINT16_MAX;
    return res_id;
}

static Substitution_Data *s_data_build()
{
    Substitution_Data *s_data = malloc(sizeof(Substitution_Data));
    check_mem(s_data);
    memset(s_data->ins, -1, sizeof(int16_t) * 8);
    memset(s_data->modes, 0, sizeof(uint8_t) * 8);
    memset(s_data->types, 0, sizeof(uint8_t) * 8);
    memset(s_data->flags, 0, sizeof(uint8_t) * 8);
    memset(s_data->typewise_idx[0], -1, sizeof(int16_t) * 4);
    memset(s_data->typewise_idx[1], -1, sizeof(int16_t) * 4);
    memset(s_data->typewise_idx[2], -1, sizeof(int16_t) * 4);
    memset(s_data->typewise_idx[3], -1, sizeof(int16_t) * 4);
    return s_data;
error:
    return NULL;
}

static Substitution_Data *process_formats(YogiGen *yogen, Format_String *fstr)
{
    uint64_t data = fstr->data;
    uint8_t i = 0;
    Substitution_Data *s_data = s_data_build();
    check(s_data, "Failed to create a Substitution_Data template.");
    while (i < fstr->count) {
        uint8_t datum = byte_of(&data, i);
        uint8_t cpy = datum;
        uint8_t mask = 0x6;
        cpy &= mask;
        uint8_t type = cpy >> 1;
        s_data->types[i] = type;
        cpy = datum;
        mask = 0x18;
        cpy &= mask;
        uint8_t type_i = cpy >> 3;
        cpy = datum;
        mask = 0xE0;
        cpy &= mask;
        uint8_t flag = cpy >> 5;
        uint16_t expr_i;
        if (s_data->typewise_idx[type][type_i] == -1) {
            if (!flag) {
                expr_i = random_expression_id(yogen, type, 0, NULL);
            } else {
                if (flag == FSTR_UNIQUE) {
                    expr_i = random_expression_id(yogen, type, FSTR_UNIQUE, s_data->typewise_idx[type]);
                } else if (flag == FSTR_POSTPOS) {
                    expr_i = random_expression_id(yogen, type, flag, s_data->types);
                } else if (flag == FSTR_PREPOS) {
                    sentinel("Format string flag no %d: FSTR_PREPOS is not yet implemented.", flag);
                } else if (flag == FSTR_INDEF_ART) {
                    expr_i = random_expression_id(yogen, type, 0, NULL);
                    s_data->flags[i] = FSTR_INDEF_ART; // handled later
                } else {
                    sentinel("Format string flag no %d is undefined.", flag);
                }
            }
            check(expr_i != 0xFFFF, "random_expression_id() returned %d", UINT16_MAX);
            s_data->typewise_idx[type][type_i] = expr_i;
        } else {
            expr_i = s_data->typewise_idx[type][type_i];
        }
        s_data->ins[i] = expr_i;
        cpy = datum;
        mask = 0x1;
        cpy &= mask;
        s_data->modes[i] = cpy == 1 ? 1 : 0;
        i++;
    }
    s_data->str = fstr->str;
    s_data->count = i;
    return s_data;
error:
    return NULL;
}

static bstring prettify(YogiGen *yogen, Substitution_Data *s_data)
{
    int i = 0;
    Expression *expr;
    bstring out = bstrcpy(s_data->str);
    do {
        expr = get_expression(yogen, s_data->ins[i], s_data->types[i]);
        check(expr, "Error assigning an expression (id=%d) to template. (%s)", s_data->ins[i], s_data->str->data);
        bstring insert = s_data->modes[i] == 0 ? expr->field_1 : expr->field_2;
        int pos = bstrchr(out, '%');
        // handle possibly nonmatching indef article if the corresponding flag is present
        if (s_data->flags[i] == FSTR_INDEF_ART) {
            int an_pos = bstrrchrp(out, 'n', pos);
            an_pos = (pos - an_pos);
            if (an_pos != 2 && expr->flags == A_OFLAG_AN) {
                binsertch(out, pos - 1, 1, 'n');
            } else if (an_pos == 2 && expr->flags != A_OFLAG_AN) {
                bdelete(out, pos - 2, 1);
            }
        }
        bdelete(out, pos, 2);
        binsert(out, pos, insert, ' ');
        i++;
    } while (i < s_data->count);
    // correct possibly lowercase 1st letter
    if (islower(out->data[0])) {
        out->data[0] = toupper(out->data[0]);
    }
    // check for extra dots and the consequent need for capitalisation
    int dot_pos = bstrrchrp(out, '.', bstrrchr(out, '.') - 1);
    while (dot_pos != BSTR_ERR) {
        if (islower(out->data[dot_pos + 2])) {
            out->data[dot_pos + 2] = toupper(out->data[dot_pos + 2]);
        }
        dot_pos = bstrrchrp(out, '.', dot_pos - 1);
    }
    free(s_data);
    printf("%s\n", out->data);
    return out;
error:
    if (s_data) free(s_data);
    return NULL;
}

YogiGen *YogiGen_init()
{
    YogiGen *yogen = malloc(sizeof(YogiGen));
    check_mem(yogen);

    yogen->conn = open_conn();
    check(yogen->conn, "Failed to establish db connection.");

    int8_t ret = fetch_counts(yogen);
    check(ret, "Failed to fetch table row counts from database.");

    Format_String *formats = malloc(sizeof(Format_String) * yogen->formats_count);
    check_mem(formats);
    yogen->formats = formats;

    Expression *verbs = malloc(sizeof(Expression) * yogen->verb_count);
    check_mem(verbs);
    yogen->verbs = verbs;

    Expression *adjs = malloc(sizeof(Expression) * yogen->adj_count);
    check_mem(adjs);
    yogen->adjs = adjs;

    Expression *cepts = malloc(sizeof(Expression) * yogen->cept_count);
    check_mem(cepts);
    yogen->cepts = cepts;

    Expression *objs = malloc(sizeof(Expression) * yogen->obj_count);
    check_mem(objs);
    yogen->objs = objs;

    return yogen;

error:
    if (yogen) YogiGen_close(yogen);
    return NULL;
}

void YogiGen_close(YogiGen *yogen)
{
    if (yogen) {
        if (yogen->conn) {
            close_conn(yogen->conn);
        }
        if (yogen->formats) {
            for (size_t i = 0; i < yogen->formats_count; i++) {
                bdestroy(yogen->formats[i].str);
            }
            free(yogen->formats);
        }
        if (yogen->verbs) {
            for (size_t i = 0; i < yogen->verb_count; i++) {
                bdestroy(yogen->verbs[i].field_1);
                bdestroy(yogen->verbs[i].field_2);
            }
            free(yogen->verbs);
        }
        if (yogen->adjs) {
            for (size_t i = 0; i < yogen->adj_count; i++) {
                bdestroy(yogen->adjs[i].field_1);
                bdestroy(yogen->adjs[i].field_2);
            }
            free(yogen->adjs);
        }
        if (yogen->cepts) {
            for (size_t i = 0; i < yogen->cept_count; i++) {
                bdestroy(yogen->cepts[i].field_1);
                bdestroy(yogen->cepts[i].field_2);

            }
            free(yogen->cepts);
        }
        if (yogen->objs) {
            for (size_t i = 0; i < yogen->obj_count; i++) {
                bdestroy(yogen->objs[i].field_1);
                bdestroy(yogen->objs[i].field_2);
            }
            free(yogen->objs);
        }
        free(yogen);
    }
}

uint8_t YogiGen_fetch_all(YogiGen *yogen)
{
    uint8_t ret;
    ret = fetch_expressions(yogen);
    check(ret, "Failed to fetch expressions from database.");
    ret = fetch_formats(yogen);
    check(ret, "Failed to fetch format strings from database.");
    return 1;
error:
    return 0;
}

bstring YogiGen_generate(YogiGen *yogen)
{
    Format_String *formats = random_formats(yogen);
    check(formats, "Failed to randomize a format string.");
    Substitution_Data *s_data = process_formats(yogen, formats);
    check(s_data, "String processing failure for: %s", formats->str->data);
    bstring out = prettify(yogen, s_data);
    return out;
error:
    YogiGen_close(yogen);
    return NULL;
}

uint64_t YogiGen_insert_into_db(YogiGen *yogen, bstring gen_str)
{
    int ret;
    const char *template = GENS_SQL_INSERT_TEMPLATE;
    bstring query = bfromcstr(template);
    uint64_t rnd_id = 0;
    do {
        ssize_t rnd_ret = getrandom(&rnd_id, sizeof(uint64_t), 0);
        check(rnd_ret != -1, "Failed to source random bytes.");
        if (rnd_id != UINT64_MAX) { // reserve UINT64_MAX for error
            bassignformat(query, template, rnd_id, bdata(gen_str));
            while (yogen->conn->conn_count == yogen->conn->max_conn) {
                sleep(1);
            }
            ret = postgres_insert_concurrent(yogen->conn, bdata(query));
            check(ret != 0, "Insert was not succesful.");
        } else {
            ret = -1;
        }
    } while (ret == -1); // -1 usually indicates a uniqueness violation, need to roll a new id
    bdestroy(query);
    return rnd_id;
error:
    if (query) bdestroy(query);
    return UINT64_MAX;
}

bstring YogiGen_get_by_id(YogiGen *yogen, bstring id_str)
{
    const char *template = GENS_SQL_SELECT_TEMPLATE;
    bstring query = bfromcstr(template);
    bassignformat(query, template, (bdata(id_str)));
    while (yogen->conn->conn_count == yogen->conn->max_conn) {
        sleep(1);
    }
    PGresult *res = postgres_select_concurrent(yogen->conn, bdata(query));
    bdestroy(query);
    check(res, "Error obtaining result set from db.");
    if (!PQntuples(res)) {
        log_warn("No rows matching id, returning original input string %s.", id_str->data);
        PQclear(res);
        return id_str;
    }
    bstring ret_str = bfromcstr(PQgetvalue(res, 0, 0));
    PQclear(res);
    return ret_str;
error:
    if (query) bdestroy(query);
    return NULL;
}
