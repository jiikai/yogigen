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
        arr[i].str = bfromcstr(PQgetvalue(res_set, i, 0));
        arr[i].data = (uint64_t) atol(PQgetvalue(res_set, i, 1));
        arr[i].count = (uint8_t) atoi(PQgetvalue(res_set, i, 2));
    }
}

static void read_expr_result_set(void *arg, PGresult *res_set)
{
    Expression *arr = (Expression*) arg;
    uint16_t count = (uint16_t) PQntuples(res_set);
    for (uint16_t i = 0; i < count; i++) {
        arr[i].field_1 = bfromcstr(PQgetvalue(res_set, i, 0));
        arr[i].field_2 = bfromcstr(PQgetvalue(res_set, i, 1));
        arr[i].type = (uint8_t) atoi(PQgetvalue(res_set, i, 2));
        arr[i].flags = (uint8_t) atoi(PQgetvalue(res_set, i, 3));
    }
}

static uint8_t fetch_counts(YogiGen *yogen)
{
    int ret;
    ret = postgres_select_cycle(yogen->conn->db, SQL_EXPR_COUNT_PG,
        read_count_result_set, yogen);
    check(ret, ERR_EXTERN, "POSTGRES", PQerrorMessage(yogen->conn->db));
    ret = postgres_select_cycle(yogen->conn->db, SQL_FRMT_COUNT_PG,
        read_count_result_set, yogen);
    check(ret, ERR_EXTERN, "POSTGRES", PQerrorMessage(yogen->conn->db));
    return 1;
error:
    return 0;
}

static uint8_t fetch_formats(YogiGen *yogen)
{
    int ret;
    char *query = FRMT_SQL_SELECT_TEMPLATE;
    ret = postgres_select_cycle(yogen->conn->db, query, read_frmt_result_set, yogen->formats);
    check(ret, ERR_EXTERN, "POSTGRES", PQerrorMessage(yogen->conn->db));
    return 1;
error:
    return 0;
}

static uint8_t fetch_expressions(YogiGen *yogen)
{
    int ret;
    uint8_t i = 0;
    const char *template = EXPR_SQL_SELECT_TEMPLATE;
    Expression *arr_ptrs[4] = {yogen->verbs, yogen->adjs, yogen->cepts, yogen->objs};
    bstring query = bfromcstr(template);
    do {
        bassignformat(query, template, i);
        ret = postgres_select_cycle(yogen->conn->db, bdata(query),
        read_expr_result_set, arr_ptrs[i]);
        check(ret, ERR_EXTERN, "POSTGRES", PQerrorMessage(yogen->conn->db));
        i++;
    } while (i < 4);
    bdestroy(query);
    return 1;
error:
    if (query) bdestroy(query);
    return 0;
}


static Format_String *get_formats(YogiGen *yogen, uint16_t idx)
{
    check(idx < yogen->formats_count, ERR_INVAL, "YOGIGEN", "index", idx);
    return &yogen->formats[idx];
error:
    return NULL;
}

static Format_String *random_formats(YogiGen *yogen)
{
    uint16_t rnd;
    ssize_t ret = getrandom(&rnd, sizeof(uint16_t), 0);
    check(ret != -1, ERR_FAIL, "YOGIGEN", "sourcing random bytes");
    rnd %= yogen->formats_count;
    return get_formats(yogen, rnd);
error:
    return NULL;
}

static Expression *get_expression(YogiGen *yogen, uint16_t idx, uint16_t type)
{
    switch (type) {
        case VERB:
            check(idx < yogen->verb_count, ERR_INVAL, "YOGIGEN", "index", idx);
            return &yogen->verbs[idx];
        case ADJECTIVE:
            check(idx < yogen->adj_count, ERR_INVAL, "YOGIGEN", "index", idx);
            return &yogen->adjs[idx];
        case CONCEPT:
            check(idx < yogen->cept_count, ERR_INVAL, "YOGIGEN", "index", idx);
            return &yogen->cepts[idx];
        case OBJECT:
            check(idx < yogen->obj_count, ERR_INVAL, "YOGIGEN", "index", idx);
            return &yogen->objs[idx];
        default:
            sentinel(ERR_UNDEF, "YOGIGEN", "undefined expression type code", type);
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
            sentinel(ERR_UNDEF, "YOGIGEN", "expression type code", type);
    }
    ssize_t ret = getrandom(&res_id, sizeof(uint16_t), 0);
    check(ret != -1, ERR_FAIL, "YOGIGEN", "sourcing random bytes");
    res_id %= limit;
    // Flag check and handling:
    if (flag) {
        if (flag == FSTR_UNIQUE) {
            int16_t *used_ids = (int16_t*) ref_data;
            uint8_t i = 0;
            while (i < 4) {
                if (used_ids[i] >= 0 && used_ids[i] == res_id) {
                    ssize_t ret = getrandom(&res_id, sizeof(uint16_t), 0);
                    check(ret != -1, ERR_FAIL, "YOGIGEN", "sourcing random bytes");
                    res_id %= limit;
                    i = 0;
                } else {
                    i++;
                }
            }
        } else if (flag >= FSTR_POSTPOS_CEPT && flag <= FSTR_POSTPOS_OBJ) {
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
            check(ret != -1, ERR_FAIL, "YOGIGEN", "sourcing pseudorandom bytes");
            if (flag == FSTR_POSTPOS_OBJ) {
                limit -= pos_y;
                if (!limit) {
                    limit++;
                }
                res_id %= limit;
                res_id += pos_y;
            } else {
                pos_y -= pos_x;
                if (!pos_y) {
                    pos_y++;
                }
                res_id %= pos_y;
                res_id += pos_x;
            }
        } else if (flag == FSTR_INDEF_ART) {
            sentinel(ERR_IMPRO, "YOGIGEN", "format string flag for this function", flag);
        } else {
            sentinel(ERR_UNDEF, "YOGIGEN", "format string flag", flag);
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
    check(s_data, ERR_MEM, "YOGIGEN");
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
    check(s_data, ERR_MEM, "YOGIGEN");
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
                } else if (flag >= FSTR_POSTPOS_CEPT && flag <= FSTR_POSTPOS_OBJ) {
                    expr_i = random_expression_id(yogen, type, flag, NULL);
                } else if (flag == FSTR_INDEF_ART) {
                    expr_i = random_expression_id(yogen, type, 0, NULL);
                    s_data->flags[i] = FSTR_INDEF_ART; // handled later
                } else {
                    sentinel(ERR_UNDEF, "YOGIGEN", "format string flag", flag);
                }
            }
            check(expr_i != 0xFFFF, ERR_FAIL, "YOGIGEN", "retrieving resources");
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

static bstring substitute_and_prettify(YogiGen *yogen, Substitution_Data *s_data)
{
    int i = 0;
    Expression *expr;
    bstring out = bstrcpy(s_data->str);
    do {
        expr = get_expression(yogen, s_data->ins[i], s_data->types[i]);
        check(expr, ERR_FAIL, "YOGIGEN", "substituting expression");
        bstring insert = s_data->modes[i] == 0 ? expr->field_1 : expr->field_2;
        int pos = bstrchr(out, '%');
        // check for and handle possibly nonmatching indef article if the corresponding FSTR flag is present
        if (s_data->flags[i] == FSTR_INDEF_ART && expr->flags == A_OFLAG_AN) {
            bstring fill = bfromcstr("n ");
            binsert(insert, 0, fill, ' ');
            bdestroy(fill);
            pos--;
            bdelete(out, pos, 3);
        } else {
            bdelete(out, pos, 2);
        }
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
    check(yogen, ERR_MEM, "YOGIGEN");

    yogen->conn = open_conn();
    check(yogen->conn, ERR_FAIL, "YOGIGEN", "connecting to database");

    int8_t ret = fetch_counts(yogen);
    check(ret, ERR_FAIL, "YOGIGEN", "querying the database");

    Format_String *formats = malloc(sizeof(Format_String) * yogen->formats_count);
    check(formats, ERR_MEM, "YOGIGEN");
    yogen->formats = formats;

    Expression *verbs = malloc(sizeof(Expression) * yogen->verb_count);
    check(verbs, ERR_MEM, "YOGIGEN");
    yogen->verbs = verbs;

    Expression *adjs = malloc(sizeof(Expression) * yogen->adj_count);
    check(adjs, ERR_MEM, "YOGIGEN");
    yogen->adjs = adjs;

    Expression *cepts = malloc(sizeof(Expression) * yogen->cept_count);
    check(cepts, ERR_MEM, "YOGIGEN");
    yogen->cepts = cepts;

    Expression *objs = malloc(sizeof(Expression) * yogen->obj_count);
    check(objs, ERR_MEM, "YOGIGEN");
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
    check(ret, ERR_FAIL, "YOGIGEN", "querying the database");
    ret = fetch_formats(yogen);
    check(ret, ERR_FAIL, "YOGIGEN", "querying the database");
    close_nonblocking_conn(yogen->conn);
    return 1;
error:
    return 0;
}

bstring YogiGen_generate(YogiGen *yogen)
{
    Format_String *formats = random_formats(yogen);
    check(formats, ERR_FAIL, "YOGIGEN", "retrieving resources");
    Substitution_Data *s_data = process_formats(yogen, formats);
    check(s_data, ERR_FAIL_A, "YOGIGEN", "processing string", bdata(formats->str));
    bstring out = substitute_and_prettify(yogen, s_data);
    return out;
error:
    YogiGen_close(yogen);
    return NULL;
}

uint64_t YogiGen_insert_into_db(YogiGen *yogen, bstring gen_str)
{
    int ret;
    const char *template = GEN_SQL_INSERT_TEMPLATE;
    bstring query = bfromcstr(template);
    uint64_t rnd_id = 0;
    do {
        ssize_t rnd_ret = getrandom(&rnd_id, sizeof(uint64_t), 0);
        check(rnd_ret != -1, ERR_FAIL, "YOGIGEN", "sourcing pseudorandom bytes");
        if (rnd_id != UINT64_MAX) { // reserve UINT64_MAX for error
            bassignformat(query, template, rnd_id, bdata(gen_str));
            while (yogen->conn->conn_count == yogen->conn->max_conn) {
                sleep(1);
            }
            ret = postgres_insert_concurrent(yogen->conn, bdata(query));
            check(ret != 0, ERR_FAIL, "YOGIGEN", "querying the database");
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
    const char *template = GEN_SQL_UPDATE_RETURN_TEMPLATE;
    bstring query = bfromcstr(template);
    bassignformat(query, template, (bdata(id_str)));
    while (yogen->conn->conn_count == yogen->conn->max_conn) {
        sleep(1);
    }
    PGresult *res = postgres_select_concurrent(yogen->conn, bdata(query));
    bdestroy(query);
    check(res, ERR_FAIL, "YOGIGEN", "querying the database");
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

/* Diagnostics */

void print_fstr_flag_info(YogiGen *yogen)
{
    for (size_t i = 0; i < yogen->formats_count; i++) {
        Format_String fstr = yogen->formats[i];
        uint64_t data = fstr.data;
        int j = 0;
        do {
            uint8_t jth_byte = byte_of(&data, j);
            uint8_t flag = jth_byte >> 5;
            fprintf(stdout, "flag %d at substitution %d in string: %s\n", flag, j, bdata(fstr.str));
        } while (++j < fstr.count);
    }
}
