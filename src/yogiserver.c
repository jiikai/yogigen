#include "yogiserver.h"

volatile sig_atomic_t terminate;

// Log message callback for CivetWeb
int log_message(const struct mg_connection *conn, const char *message)
{
	fprintf(stdout,"%s\n", message);
	return 1;
}

static void encode_form_uri(bstring str)
{
    bstring find = bfromcstr(" ");
    bstring replace = bfromcstr("+");
    bfindreplace(str, find, replace, 0);
    bdestroy(find);
    bdestroy(replace);
}

static void decode_form_uri(bstring str)
{
    bstring find = bfromcstr("+");
    bstring replace = bfromcstr(" ");
    bfindreplace(str, find, replace, 0);
    bdestroy(find);
    bdestroy(replace);
}

static int get_sys_info(YogiServer *server)
{
    char *buf = malloc(sizeof(char) * 0xFF + 1);
    check_mem(buf);
    mg_get_system_info(buf, 0xFF);
    server->sys_info = bfromcstr(buf);
    free(buf);
    return 1;
error:
    return 0;
}

static uint8_t randomize_button_text_idx()
{
    uint8_t rnd;
    ssize_t rnd_ret = getrandom(&rnd, sizeof(uint8_t), 0);
    if (rnd_ret == -1) {
        log_warn("Failed to source random bytes, fallback to default text");
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
    check(!ret, YOGISERVER_405_NOT_ALLOWED, req_info->request_method);
	YogiServer *server = (YogiServer*) cbdata;
    uint8_t rnd = randomize_button_text_idx(server);
	long long content_len = (long long) (
		blength(server->html_template_index)
		 - 2 + sizeof(server->btn_txts[rnd]) // replace %s with a string
	);
	mg_send_http_ok(conn, "text/html", content_len);
    mg_printf(conn, bdata(server->html_template_index), server->btn_txts[rnd]);
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
    check(!ret, YOGISERVER_405_NOT_ALLOWED, req_info->request_method);
    YogiServer *server = (YogiServer*) cbdata;
    bstring gen_str = YogiGen_generate(server->yogen);
    check(gen_str, YOGISERVER_GEN_ERROR, "to generate requested string.");
    bstring gen_str_enc = bstrcpy(gen_str);
    encode_form_uri(gen_str_enc);
	uint8_t rnd = randomize_button_text_idx(server);
	long long content_len = (long long) (
		blength(server->html_template_gen)
		 - (2*3) // printf style format string placeholders
		+ sizeof(server->btn_txts[rnd]) // and their replacements
		+ sizeof(bdata(gen_str))
		+ sizeof(bdata(gen_str))
	);
	mg_send_http_ok(conn, "text/html", content_len);
    mg_printf(conn, bdata(server->html_template_gen), server->btn_txts[rnd], bdata(gen_str), bdata(gen_str_enc));
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
    check(!ret, YOGISERVER_405_NOT_ALLOWED, req_info->request_method);
    bstring query = bfromcstr(req_info->query_string);
    int pos = bstrchr(query, '=');
    bdelete(query, 0, pos + 1);
    bstring query_dec = bstrcpy(query);
    mg_url_decode(bdata(query), query->slen, bdata(query_dec), query_dec->slen, 0);
    decode_form_uri(query_dec);
    bdestroy(query);
    YogiServer *server = (YogiServer*) cbdata;
    Generated *gen = malloc(sizeof(Generated));
    check_mem(gen);
    gen->yogen_addr = server->yogen;
    gen->str = query_dec;
    int ins_ret = YogiGen_insert_into_db(server->yogen, gen);
    check(ins_ret, YOGISERVER_DB_ERROR, "inserting to");
    mg_printf(conn, HTTP_RES_200);
    mg_printf(conn, bdata(server->html_template_permalink), HTTP_PREFIX,
        HOST, PORT, gen->rnd_id, "Generate another...");
    bdestroy(gen->str);
    free(gen);
    return 200;
error:
    if (ret) {
        mg_printf(conn, HTTP_RES_405);
        return 405;
    }
    if (gen){
        bdestroy(gen->str);
        free(gen);
    }
	log_err("Failed to create permalink for %s", bdata(query_dec));
    mg_printf(conn, HTTP_RES_500);
    return 500;
}

int getbyid_request_handler(struct mg_connection *conn, void *cbdata)
{
    int ret;
    const struct mg_request_info *req_info = mg_get_request_info(conn);
    ret = mg_strncasecmp(req_info->request_method, "GET", 3);
    check(!ret, YOGISERVER_405_NOT_ALLOWED, req_info->request_method);
    bstring query = bfromcstr(req_info->query_string);
    for (uint8_t i = 0; i < query->slen; i++) {
        check(isxdigit(bdata(query)[i]), "Invalid URI:\n%s\nOnly hex digits allowed.", bdata(query));
    }
    YogiServer *server = (YogiServer*) cbdata;
    bstring ret_string = YogiGen_get_by_id((YogiGen*) server->yogen, query);
    check(ret_string, YOGISERVER_DB_ERROR, "SELECTing from");
    mg_printf(conn, HTTP_RES_200);
    if (ret_string == query) {
        log_warn("Unable to find by the provided hash id, suggest the creation of a new string instead.");
        mg_printf(conn, bdata(server->html_template_getbyid),
            "Sorry, there is nothing in the database matching this link!",
            "Maybe you could try getting a new one instead?");
    } else {
        mg_printf(conn, bdata(server->html_template_getbyid),
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
    check(!ret, YOGISERVER_405_NOT_ALLOWED, req_info->request_method);
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
    check(!ret, YOGISERVER_405_NOT_ALLOWED, req_info->request_method);
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
    check(!ret, YOGISERVER_405_NOT_ALLOWED, req_info->request_method);
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
	if (local_uri) {
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
    if (sig == SIGTERM) {
        fprintf(stdout, "SIGTERM\n");
        terminate = 1;
    }
}

YogiServer *YogiServer_init()
{
    YogiServer *server = malloc(sizeof(YogiServer));
    check_mem(server);

    server->yogen = YogiGen_init();
    check(server->yogen, YOGISERVER_STARTUP_FAILED, "initializing YogiGen");
    int ret = YogiGen_fetch_all(server->yogen);
    check(ret, YOGISERVER_STARTUP_FAILED, "fetching data for YogiGen");

    memset(&server->callbacks, 0, sizeof(struct mg_callbacks));
    memset(&server->ports, 0, sizeof(struct mg_server_ports) * 32);
    memset(&server->sigactor, 0, sizeof(struct sigaction));

    server->ctx = civet_init_start(server);
    check(server->ctx, YOGISERVER_STARTUP_FAILED, "to initialize CivetWeb server");

    server->ports_count = mg_get_server_ports(server->ctx, 32, server->ports);
    check(server->ports_count >= 0, YOGISERVER_STARTUP_FAILED, "fetching ports");

    server->sigactor.sa_handler = dyno_signal_handler;
    sigemptyset(&server->sigactor.sa_mask);
    server->sigactor.sa_flags = SA_RESTART;

    ret = get_sys_info(server);
    if (!ret) log_warn("Unable to collect system information\n");

	FILE *fp = fopen(YOGIGEN_INDEX_HTML, "r");
    check(fp, YOGISERVER_FILE_ERR, "opening", YOGIGEN_INDEX_HTML);
    server->html_template_index = bread((bNread) fread, fp);
    check(fp, YOGISERVER_FILE_ERR, "reading", YOGIGEN_INDEX_HTML);
    fclose(fp);

    fp = fopen(YOGIGEN_GEN_HTML, "r");
    check(fp, YOGISERVER_FILE_ERR, "opening", YOGIGEN_GEN_HTML);
    server->html_template_gen = bread((bNread) fread, fp);
    check(fp, YOGISERVER_FILE_ERR, "reading", YOGIGEN_GEN_HTML);
    fclose(fp);

    fp = fopen(YOGIGEN_PERMALINK_HTML, "r");
    check(fp, YOGISERVER_FILE_ERR, "opening", YOGIGEN_PERMALINK_HTML);
    server->html_template_permalink = bread((bNread) fread, fp);
    check(fp, YOGISERVER_FILE_ERR, "reading", YOGIGEN_PERMALINK_HTML);
    fclose(fp);

    fp = fopen(YOGIGEN_GETBYID_HTML, "r");
    check(fp, YOGISERVER_FILE_ERR, "opening", YOGIGEN_GETBYID_HTML);
    server->html_template_getbyid = bread((bNread) fread, fp);
    check(fp, YOGISERVER_FILE_ERR, "reading", YOGIGEN_GETBYID_HTML);
    fclose(fp);

    server->btn_txts[0] = BTN_TXT_0; server->btn_txts[1] = BTN_TXT_1;
    server->btn_txts[2] = BTN_TXT_2; server->btn_txts[3] = BTN_TXT_3;

    mg_set_request_handler(server->ctx, INDEX_URI, frontpage_request_handler, server);
    mg_set_request_handler(server->ctx, GEN_URI, gen_request_handler, server);
    mg_set_request_handler(server->ctx, PERMALINK_URI, permalink_request_handler, server);
    mg_set_request_handler(server->ctx, GETBYID_URI, getbyid_request_handler, server);
#ifndef HEROKU
    mg_set_request_handler(server->ctx, EXIT_URI, exit_request_handler, NULL);
#endif
	mg_set_request_handler(server->ctx, CSS_URI, css_request_handler, server);
	mg_set_request_handler(server->ctx, FONT_URI, font_request_handler, server);

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
        if (server->html_template_index) {
            bdestroy(server->html_template_index);
        }
		if (server->html_template_gen) {
            bdestroy(server->html_template_gen);
        }
        if (server->html_template_permalink) {
            bdestroy(server->html_template_permalink);
        }
        if (server->html_template_getbyid) {
            bdestroy(server->html_template_getbyid);
        }
        if (server->sys_info) {
            bdestroy(server->sys_info);
        }
        if (server->yogen) {
            YogiGen_close(server->yogen);
        }
        free(server);
    }
    mg_exit_library();
}

int main(int argc, char const *argv[])
{
    terminate = 0;
    YogiServer *server;
    server = YogiServer_init();
    check(server, YOGISERVER_STARTUP_FAILED, "to start, aborting process");
    int ret = sigaction(SIGTERM, &server->sigactor, NULL);
    check(ret == 0, YOGISERVER_STARTUP_FAILED, "to set signal handler, aborting process");

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
