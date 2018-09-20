#include "yogidb.h"
#include "dbg.h"

int postgres_select_cycle(PGconn *db, char* query, handle_result res_function, void *arg)
{
    PGresult *res = PQexec(db, query);
    check(PQresultStatus(res) != PGRES_FATAL_ERROR, "Postgres: %s",  PQerrorMessage(db));
    res_function(arg, res);
    PQclear(res);
    return 1;
error:
    if (res) PQclear(res);
    return 0;
}

static PGconn *pg_open_nonblocking_conn(Connection *conn)
{
    PGconn *db;
    int ret;
    db = PQconnectStart((char*) conn->conn_info->data);
    check_mem(db);
    check(PQstatus(db) != CONNECTION_BAD, "Failed establishing db connection.")
    if (conn->conn_count++ > conn->cur_max) {
        conn->cur_max = conn->conn_count;
    }
    struct pollfd pfds;
    ret = PGRES_POLLING_WRITING;
    do {
        pfds.events = ret == PGRES_POLLING_READING ? (0 ^ POLLIN) : (0 ^ POLLOUT);
        pfds.fd = PQsocket(db);
        poll(&pfds, 1, 5000);
        check(ret != -1, "Error polling socket.");
        check(ret, "Poll timeout.");
        ret = PQconnectPoll(db);
        check(ret != PGRES_POLLING_FAILED, "Postgres: polling db conn failed.");
    } while (ret != PGRES_POLLING_OK);
    if (!PQisnonblocking(db)) {
        ret = PQsetnonblocking(db, 1);
        check(ret != -1, "Postgres: %s", PQerrorMessage(db));
    }
    return db;
error:
    if (db) {
        PQfinish(db);
        conn->conn_count--;
    }
    return NULL;
}

PGresult *postgres_select_concurrent(Connection *conn, char* query)
{
    int ret;
    PGconn *db = pg_open_nonblocking_conn(conn);
    check(db, "No open connection.");
    ret = PQsendQuery(db, query);
    check(ret, "Postgres: %s", PQerrorMessage(db));
    struct pollfd pfds;
    pfds.events =  0 ^ POLLIN;
    uint8_t i = 3;
    do {
        pfds.fd = PQsocket(db);
        ret = poll(&pfds, 1, 5000);
        i--;
        check(ret != -1, "Error polling socket.");
        if (!ret) {
            log_warn("Poll timeout, %d attempts left", i);
        } else {
            ret = pfds.revents;
            ret &= POLLIN;
            check(ret, "Poll indicated readable data but there wasn't any.");
            ret = PQconsumeInput(db);
            check(ret, "Postgres: %s", PQerrorMessage(db));
            ret = PQisBusy(db);
            while (ret) {
                sleep(1);
                ret = PQisBusy(db);
            }
            break;
        }
    } while (i);
    check(i, "Too many timeouts.");
    PGresult *res = PQgetResult(db);
    check(PQresultStatus(res) == PGRES_TUPLES_OK, "Postgres: %s", PQerrorMessage(db));
    PQfinish(db);
    conn->conn_count--;
    return res;
error:
    if (res) PQclear(res);
    if (db) {
        PQfinish(db);
        conn->conn_count--;
    }
    return NULL;
}

int postgres_insert_concurrent(Connection *conn, char* query)
{
    int ret;
    PGconn *db = pg_open_nonblocking_conn(conn);
    check(db, "No open connection.");
    PGresult *res = PQexec(db, "LISTEN Notifier;");
    check(PQresultStatus(res) == PGRES_COMMAND_OK, "Postgres: %s", PQerrorMessage(db));
    PQclear(res);
    PGnotify *notify;
    ret = PQsendQuery(db, query);
    check(ret, "Postgres: %s", PQerrorMessage(db));
    int sock;
    fd_set input_mask;
    do {
        sock = PQsocket(db);
        FD_ZERO(&input_mask);
        FD_SET(sock, &input_mask);
        check(sock >= 0, "This should not happen.");
        ret = select(sock + 1, &input_mask, NULL, NULL, NULL);
        check(ret >= 0, "select() failed");
        PQconsumeInput(db);
        notify = PQnotifies(db);
    } while (!notify);
    ret = strcmp(notify->extra, "err");
    if (!ret) {
        fprintf(stdout, "Unique violation, need another rnd_id");
        ret = -1;
    }
    PQfreemem(notify);
    PQfinish(db);
    conn->conn_count--;
    return ret;
error:
    if (res) PQclear(res);
    if (notify) PQfreemem(notify);
    if (db) {
        PQfinish(db);
        conn->conn_count--;
    }
    return 0;
}

Connection *open_conn()
{
    PGconn *db;
    bstring conn_info = bformat("%s?sslmode=require", (char*) getenv("DATABASE_URL"));
    db = PQconnectdb((char*)conn_info->data);
    check(db, "Database connection error.");
    Connection *conn = malloc(sizeof(Connection));
    check_mem(conn);
    conn->db = db;
    conn->conn_info = conn_info;
    conn->conn_count = 1;
    conn->max_conn = 15;
    conn->cur_max = 1;
    return conn;
error:
    PQfinish(db);
    return NULL;
}

void close_conn(Connection *conn)
{
    if (conn->db) {
        if (conn->conn_info) bdestroy(conn->conn_info);
        printf("max number of concurrent connections to the db was %d\n", conn->cur_max);
        PQfinish(conn->db);
    }
    free(conn);
}
