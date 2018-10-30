#include "yogiserver.h"

volatile sig_atomic_t terminate;

int log_message(const struct mg_connection *conn, const char *message)
{
	FILE *fp;
	fp = fopen(ERR_LOG, "w");
	if (!fp) {
		log_warn("Failed to open stream at path %s for logging, will log to stdout.", ERR_LOG);
		log_info("[CivetWeb] %s\n", message);
	} else {
		mg_cry(conn, "[CivetWeb] %s\n", message);
	}
	return 1;
}

static int get_sys_info(YogiServer *server)
{
	int req_bytes = (mg_get_system_info(NULL, 0)) * 1.2 + 1;
	server->sys_info = malloc(sizeof(char) * (req_bytes));
    check(server->sys_info, ERR_MEM, "YOGISERVER");
    mg_get_system_info(server->sys_info, req_bytes);
    return 1;
error:
    return 0;
}

static uint8_t randomize_button_text_idx()
{
    uint8_t rnd;
    ssize_t rnd_ret = getrandom(&rnd, sizeof(uint8_t), 0);
    if (rnd_ret == -1) {
        log_warn("Failed to source random bytes, fallback to default idx: %d", BTN_TXT_DEFAULT);
        rnd = BTN_TXT_DEFAULT;
    } else {
        rnd %= 3;
    }
    return rnd;
}

static struct mg_context *civet_init_start(YogiServer *server)
{
    mg_init_library(0);
    const char *options[] = {
        "document_root", DOCUMENT_ROOT,
        "listening_ports", PORT,
        "request_timeout_ms", REQUEST_TIMEOUT,
        "error_log_file", ERR_LOG,
#ifdef USE_WEBSOCKET
        "websocket_timeout_ms",
        WSOCK_TIMEOUT,
#endif
#ifdef USE_SSL
        "ssl_certificate",
        SSL_CERT,
        "ssl_protocol_version",
        SSL_VER,
        "ssl_cipher_list",
        SSL_CIPHER,
#endif
        "enable_auth_domain_check",
        AUTH_DOM_CHECK,
    0};
#ifdef USE_SSL
    server->callbacks.init_ssl = init_ssl;
#endif
    server->callbacks.log_message = log_message;
    struct mg_context *ctx;
    ctx = mg_start(&server->callbacks, 0, options);
    return ctx;
}

int frontpage_request_handler(struct mg_connection *conn, void *cbdata)
{
    int ret;
    const struct mg_request_info *req_info = mg_get_request_info(conn);
    ret = mg_strncasecmp(req_info->request_method, "GET", 3);
    check(!ret, ERR_NALLOW_A, "YOGISERVER", req_info->request_method, "GET");
	YogiServer *server = (YogiServer*) cbdata;
    uint8_t rnd = randomize_button_text_idx(server);
	mg_printf(conn, HTTP_RES_200);
    mg_printf(conn, bdata(server->pg_data.html_template_index), server->pg_data.btn_txts[rnd]);
    return 200;
error:
    if (ret) {
        mg_printf(conn, HTTP_RES_405);
        return 405;
    }
    mg_printf(conn, HTTP_RES_500);
    return 500;
}

int gen_request_handler(struct mg_connection *conn, void *cbdata)
{
    int ret;
	const struct mg_request_info *req_info = mg_get_request_info(conn);
    ret = mg_strncasecmp(req_info->request_method, "GET", 3);
    check(!ret, ERR_NALLOW_A, "YOGISERVER", req_info->request_method, "GET");
    YogiServer *server = (YogiServer*) cbdata;
    bstring gen_str = YogiGen_generate(server->yogen);
    check(gen_str, ERR_FAIL, "YOGISERVER", "YogiGen failed to generate requested string.");
    bstring gen_str_enc = bstrcpy(gen_str);
	// Modify literal apostrophes to the corresponding HTML entity.
	bstring find = bfromcstr("'");
	bstring replace = bfromcstr("&#8217");
	bfindreplace(gen_str_enc, find, replace, 0);
	bdestroy(find);
	bdestroy(replace);
	uint8_t rnd = randomize_button_text_idx(server);
	mg_printf(conn, HTTP_RES_200);
    mg_printf(conn, bdata(server->pg_data.html_template_gen),
		server->pg_data.btn_txts[rnd], bdata(gen_str),
		bdata(gen_str_enc)
	);
    bdestroy(gen_str);
    bdestroy(gen_str_enc);
    return 200;
error:
    if (ret) {
        mg_printf(conn, HTTP_RES_405);
        return 405;
    }
    mg_printf(conn, HTTP_RES_500);
    return 500;
}

int permalink_request_handler(struct mg_connection *conn, void *cbdata)
{
    int ret;
    const struct mg_request_info *req_info = mg_get_request_info(conn);
    ret = mg_strncasecmp(req_info->request_method, "GET", 3);
    check(!ret, ERR_NALLOW_A, "YOGISERVER", req_info->request_method, "GET");
    bstring query = bfromcstr(req_info->query_string);
    int pos = bstrchr(query, '=');
    bdelete(query, 0, pos + 1);
    bstring query_dec = bstrcpy(query);
	balloc(query_dec, query_dec->slen * 1.5);
    mg_url_decode(bdata(query), query->slen, bdata(query_dec), query_dec->mlen, 1);
    bdestroy(query);
    YogiServer *server = (YogiServer*) cbdata;
    uint64_t ins_id = YogiGen_insert_into_db(server->yogen, query_dec);
    check(ins_id != UINT64_MAX, ERR_FAIL_A, "YOGISERVER", "querying the database", "with INSERT");
    mg_printf(conn, HTTP_RES_200);
    mg_printf(conn, bdata(server->pg_data.html_template_permalink), HTTP_PREFIX,
        HOST, PORT, ins_id, "Generate another...");
    bdestroy(query_dec);
    return 200;
error:
    if (ret) {
        mg_printf(conn, HTTP_RES_405);
        return 405;
    }
    if (query_dec) bdestroy(query_dec);
	log_err("Failed to create permalink for %s", bdata(query_dec));
    mg_printf(conn, HTTP_RES_500);
    return 500;
}

int getbyid_request_handler(struct mg_connection *conn, void *cbdata)
{
    int ret;
    const struct mg_request_info *req_info = mg_get_request_info(conn);
    ret = mg_strncasecmp(req_info->request_method, "GET", 3);
    check(!ret, ERR_NALLOW_A, "YOGISERVER", req_info->request_method, "GET");
    bstring query = bfromcstr(req_info->query_string);
    for (uint8_t i = 0; i < query->slen; i++) {
        check(isxdigit(bdata(query)[i]), "Invalid URI:\n%s\nOnly hex digits allowed.", bdata(query));
    }
    YogiServer *server = (YogiServer*) cbdata;
    bstring ret_string = YogiGen_get_by_id((YogiGen*) server->yogen, query);
    check(ret_string, ERR_FAIL_A, "YOGISERVER", "querying the database", "with SELECT");
    mg_printf(conn, HTTP_RES_200);
    if (ret_string == query) {
        log_warn("Unable to find by the provided hash id %s, suggest the creation of a new string instead.", bdata(query));
        mg_printf(conn, bdata(server->pg_data.html_template_getbyid),
            "Sorry, there is nothing in the database matching this link!",
            "Maybe you could try getting a new one instead?");
    } else {
        mg_printf(conn, bdata(server->pg_data.html_template_getbyid),
            bdata(ret_string),
            "Get a new one");
    }
    bdestroy(query);
    if (ret_string) bdestroy(ret_string);
    return 200;
error:
    if (ret) {
        mg_printf(conn, HTTP_RES_405);
        return 405;
    }
    if (query) bdestroy(query);
    mg_printf(conn, HTTP_RES_500);
    return 500;
}

#ifndef HEROKU
int exit_request_handler(struct mg_connection *conn, void *cbdata)
{
    const struct mg_request_info *req_info = mg_get_request_info(conn);
    int ret = mg_strncasecmp(req_info->request_method, "GET", 3);
    check(!ret, ERR_NALLOW_A, "YOGISERVER", req_info->request_method, "GET");
    mg_printf(conn, "HTTP/1.1 200 OK\r\nContent-Type: "
              "text/plain\r\nConnection: close\r\n\r\n");
    mg_printf(conn, "Server will shut down.\n");
    terminate = 1;
    return 200;
error:
    mg_printf(conn, HTTP_RES_405);
    return 405;
}
#endif

int css_request_handler(struct mg_connection *conn, void *cbdata)
{
	const struct mg_request_info *req_info = mg_get_request_info(conn);
    int ret = mg_strncasecmp(req_info->request_method, "GET", 3);
    check(!ret, ERR_NALLOW_A, "YOGISERVER", req_info->request_method, "GET");
	mg_send_mime_file(conn, YOGIGEN_STYLE_CSS, "text/css");
	return 200;
error:
	mg_printf(conn, HTTP_RES_405);
	return 405;
}

int font_request_handler(struct mg_connection *conn, void *cbdata)
{
	const struct mg_request_info *req_info = mg_get_request_info(conn);
    int ret = mg_strncasecmp(req_info->request_method, "GET", 3);
    check(!ret, ERR_NALLOW_A, "YOGISERVER", req_info->request_method, "GET");
	bstring filepath = bfromcstr(YOGIGEN_STYLE_FONT);
	bstring local_uri = bfromcstr(req_info->local_uri);
	if (req_info->query_string) {
		bassignformat(filepath, YOGIGEN_STYLE_FONT, "eot");
		mg_send_mime_file(conn, bdata(filepath), "font/embedded-opentype");
	} else {
		int pos = bstrchr(local_uri, '.');
		if (bchar(local_uri, pos + 1) == 'e') {
			bassignformat(filepath, YOGIGEN_STYLE_FONT, "eot");
			mg_send_mime_file(conn, bdata(filepath), "font/eot");
		} else if (bchar(local_uri, pos + 1) == 't') {
			bassignformat(filepath, YOGIGEN_STYLE_FONT, "ttf");
			mg_send_mime_file(conn, bdata(filepath), "font/ttf");
		} else if (bchar(local_uri, pos + 4) == 'f') {
			if (!bchar(local_uri, pos + 5)) {
				bassignformat(filepath, YOGIGEN_STYLE_FONT, "woff");
				mg_send_mime_file(conn, bdata(filepath), "font/woff");
			} else {
				bassignformat(filepath, YOGIGEN_STYLE_FONT, "woff2");
				mg_send_mime_file(conn, bdata(filepath), "font/woff2");
			}
		} else {
			sentinel("404 for request to %s", bdata(local_uri));
		}
	}
	bdestroy(filepath);
	bdestroy(local_uri);
	return 200;
error:
	if (!ret) {
		mg_printf(conn, HTTP_RES_404);
		bdestroy(local_uri);
		bdestroy(filepath);
		return 404;
	}
	mg_printf(conn, HTTP_RES_405);
	return 405;
}

void dyno_signal_handler(int sig)
{
	log_info("Signaling event: %d", sig);
    if (sig == SIGTERM) {
        terminate = 1;
    }
}

#ifdef HEROKU
int acme_challenge_handler(struct mg_connection *conn, void *cbdata)
{
	char *challenge = getenv("LETS_ENCRYPT_CHALLENGE");
	if (challenge) {
		mg_printf(conn, HTTP_RES_200);
		mg_printf(conn, challenge);
		fprintf(stdout, "%s\n", challenge);
		return 200;
	} else {
		mg_printf(conn, HTTP_RES_404);
		return 404;
	}
}
#endif

YogiServer *YogiServer_init()
{
    YogiServer *server = malloc(sizeof(YogiServer));
    check(server, ERR_MEM, "YOGISERVER");

    server->yogen = YogiGen_init();
    check(server->yogen, ERR_FAIL, "YOGISERVER", "initializing YogiGen");
    int ret = YogiGen_fetch_all(server->yogen);
    check(ret, ERR_FAIL, "YOGISERVER", "fetching data for YogiGen");

    memset(&server->callbacks, 0, sizeof(struct mg_callbacks));
    memset(&server->ports, 0, sizeof(struct mg_server_ports) * 32);
    memset(&server->sigactor, 0, sizeof(struct sigaction));

    server->ctx = civet_init_start(server);
    check(server->ctx, ERR_FAIL, "YOGISERVER", "to initialize CivetWeb server");

    server->ports_count = mg_get_server_ports(server->ctx, 32, server->ports);
    check(server->ports_count >= 0, ERR_FAIL, "YOGISERVER", "fetching ports");

    server->sigactor.sa_handler = dyno_signal_handler;
    sigemptyset(&server->sigactor.sa_mask);
    server->sigactor.sa_flags = SA_RESTART;

    ret = get_sys_info(server);
    if (!ret) log_warn(ERR_FAIL, "YOGISERVER", "to obtain system information.");

	FILE *fp = fopen(YOGIGEN_INDEX_HTML, "r");
    check(fp, ERR_FAIL_A, "YOGISERVER", "opening file", YOGIGEN_INDEX_HTML);
    server->pg_data.html_template_index = bread((bNread) fread, fp);
    check(fp, ERR_FAIL_A, "YOGISERVER", "reading file", YOGIGEN_INDEX_HTML);
    fclose(fp);

    fp = fopen(YOGIGEN_GEN_HTML, "r");
    check(fp, ERR_FAIL_A, "YOGISERVER", "opening file", YOGIGEN_GEN_HTML);
    server->pg_data.html_template_gen = bread((bNread) fread, fp);
    check(fp, ERR_FAIL_A, "YOGISERVER", "reading file", YOGIGEN_GEN_HTML);
    fclose(fp);

    fp = fopen(YOGIGEN_PERMALINK_HTML, "r");
    check(fp, ERR_FAIL_A, "YOGISERVER", "opening file", YOGIGEN_PERMALINK_HTML);
    server->pg_data.html_template_permalink = bread((bNread) fread, fp);
    check(fp, ERR_FAIL_A, "YOGISERVER", "reading file", YOGIGEN_PERMALINK_HTML);
    fclose(fp);

    fp = fopen(YOGIGEN_GETBYID_HTML, "r");
    check(fp, ERR_FAIL_A, "YOGISERVER", "opening file", YOGIGEN_GETBYID_HTML);
    server->pg_data.html_template_getbyid = bread((bNread) fread, fp);
    check(fp, ERR_FAIL_A, "YOGISERVER", "reading file", YOGIGEN_GETBYID_HTML);
    fclose(fp);

    server->pg_data.btn_txts[0] = BTN_TXT_0; server->pg_data.btn_txts[1] = BTN_TXT_1;
    server->pg_data.btn_txts[2] = BTN_TXT_2; server->pg_data.btn_txts[3] = BTN_TXT_3;

    mg_set_request_handler(server->ctx, INDEX_URI, frontpage_request_handler, server);
    mg_set_request_handler(server->ctx, GEN_URI, gen_request_handler, server);
    mg_set_request_handler(server->ctx, PERMALINK_URI, permalink_request_handler, server);
    mg_set_request_handler(server->ctx, GETBYID_URI, getbyid_request_handler, server);
#ifndef HEROKU
    mg_set_request_handler(server->ctx, EXIT_URI, exit_request_handler, server);
#endif
	mg_set_request_handler(server->ctx, CSS_URI, css_request_handler, server);
	mg_set_request_handler(server->ctx, FONT_URI, font_request_handler, server);
#ifdef HEROKU
	mg_set_request_handler(server->ctx, ACME_URI, acme_challenge_handler, server);
#endif
    return server;
error:
    if (fp) fclose(fp);
    if (server) free(server);
    return NULL;
}

void YogiServer_close(YogiServer *server)
{
    if (server) {
        if (server->ctx) {
            mg_stop(server->ctx);
        }
        if (server->pg_data.html_template_index) {
            bdestroy(server->pg_data.html_template_index);
        }
		if (server->pg_data.html_template_gen) {
            bdestroy(server->pg_data.html_template_gen);
        }
        if (server->pg_data.html_template_permalink) {
            bdestroy(server->pg_data.html_template_permalink);
        }
        if (server->pg_data.html_template_getbyid) {
            bdestroy(server->pg_data.html_template_getbyid);
        }
        if (server->sys_info) {
            free(server->sys_info);
        }
        if (server->yogen) {
            YogiGen_close(server->yogen);
        }
        free(server);
    }
    mg_exit_library();
}

int main()
{
    terminate = 0;
    YogiServer *server;
    server = YogiServer_init();
    check(server, ERR_FAIL, "YOGISERVER", "to start, aborting process");
    int ret = sigaction(SIGTERM, &server->sigactor, NULL);
    check(ret == 0, ERR_FAIL, "YOGISERVER", "to set signal handler, aborting process");
    int i;
    for (i = 0; i < server->ports_count && i < 32; i++) {
		const char *protocol = get_protocol(server, i);
    	if ((server->ports[i].protocol & 1) == 1) {
            fprintf(stdout, "%s IPv4 connection on port %d\n", protocol, i);
    	}
    }

    while (!terminate) {
        sleeper(1);
    }

    YogiServer_close(server);
    fprintf(stdout, "Server stopped.\n");
    return EXIT_SUCCESS;
error:
    YogiServer_close(server);
    fprintf(stdout, "Server stopped.\n");
    return EXIT_FAILURE;
}
