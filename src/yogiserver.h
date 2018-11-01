#ifndef __yogiserver_h__
#define __yogiserver_h__

/* Server functionality for YogiGen. */

#define _XOPEN_SOURCE 700
#ifdef _WIN32
#include <windows.h>
#define sleeper(x) Sleep(x * 1000)
#else
#include <unistd.h>
#define sleeper(x) sleep(x)
#endif
#include <signal.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <pthread.h>
#include <sys/random.h>
#include "./dependencies/civetweb.h"
#include "./dependencies/bstrlib.h"
#include "dbg.h"
#include "yogigen.h"

// Options for the CivetWeb server
#define DOCUMENT_ROOT "../resources/html"
#ifdef HEROKU
    #define PORT ((char*) getenv("PORT"))
    #define HOST "yogigen.herokuapp.com"
#else
    #define PORT "8080"
    #define HOST "localhost"
#endif
#define REQUEST_TIMEOUT "10000"
#define ERR_LOG "../log/server.log"
#ifdef USE_WEBSOCKET
    #define WSOCK_TIMEOUT "3600000"
#endif
#ifdef USE_SSL
    #define HTTP_PREFIX "https://"
    #include "openssl/ssl.h"
    #define SSL_CERT "../resources/cert/server.pem"
    #define SSL_VER "4"
    #ifdef USE_SSL_DH
        #include "openssl/dh.h"
        #include "openssl/ec.h"
        #include "openssl/evp.h"
        #include "openssl/ecdsa.h"
        #define SSL_CIPHER "ECDHE-RSA-AES256-GCM-SHA384:DES-CBC3-SHA:AES128-SHA:AES128-GCM-SHA256"
    #else
        #define SSL_CIPHER "DES-CBC3-SHA:AES128-SHA:AES128-GCM-SHA256"
    #endif
/*
* Copyright (c) 2013-2017 the CivetWeb developers
* Copyright (c) 2013 No Face Press, LLC
* License http://opensource.org/licenses/mit-license.php MIT License
*/
    int init_ssl(void *ssl_context, void *user_data)
    {
    	/* Add application specific SSL initialization */
    	struct ssl_ctx_st *ctx = (struct ssl_ctx_st *)ssl_context;

    #ifdef USE_SSL_DH
    	/* example from https://github.com/civetweb/civetweb/issues/347 */
    	DH *dh = get_dh2236();
    	if (!dh)
    		return -1;
    	if (1 != SSL_CTX_set_tmp_dh(ctx, dh))
    		return -1;
    	DH_free(dh);

    	EC_KEY *ecdh = EC_KEY_new_by_curve_name(NID_X9_62_prime256v1);
    	if (!ecdh)
    		return -1;
    	if (1 != SSL_CTX_set_tmp_ecdh(ctx, ecdh))
    		return -1;
    	EC_KEY_free(ecdh);

    	printf("ECDH ciphers initialized\n");
    #endif
    	return 0;
    }
/* END COPYRIGHT */
#else
    #ifdef HEROKU
        #define HTTP_PREFIX "https://"
    #else
        #define HTTP_PREFIX "http://"
    #endif
#endif
#define AUTH_DOM_CHECK "no"

// Valid relative URIs
#define INDEX_URI "/"
#define GEN_URI "/gen"
#define PERMALINK_URI "/permalink"
#define GETBYID_URI "/getbyid"
#ifndef HEROKU
    #define EXIT_URI "/exit"
#endif
#ifdef HEROKU
    #define ACME_URI "/.well-known/acme-challenge"
#endif
// URIs for serving js, css and fonts
#define CSS_URI "/css/style.css"
#define FONT_URI "/fonts"
#define JS_URI "/js"

// HTML template paths (relative to the running binary)
#define YOGIGEN_INDEX_HTML "../resources/index.html"
#define YOGIGEN_GEN_HTML "../resources/gen.html"
#define YOGIGEN_PERMALINK_HTML "../resources/permalink.html"
#define YOGIGEN_GETBYID_HTML "../resources/getbyid.html"

// JavaScript paths
#define YOGIGEN_JS_CP2CB "../resources/js/cp2cb-async-fallback.js"

// CSS & font paths (relative to document root)
#define YOGIGEN_STYLE_CSS "../resources/css/style.css"
#define YOGIGEN_STYLE_FONT "../resources/fonts/TFArrow-Bold.%s"

// HTTP response headers currently used
#define HTTP_RES_200 "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nConnection: close\r\n\r\n"
#define HTTP_RES_404 "HTTP/1.1 404 Not Found\r\nConnection: close\r\n\r\n"
#define HTTP_RES_405 "HTTP/1.1 405 Method Not Allowed\r\nAllow: GET\r\nConnection: close\r\n\r\n"
#define HTTP_RES_500 "HTTP/1.1 500 Internal Server Error\r\nConnection: close\r\n\r\n"

// HTML Button text variations
#define BTN_TXT_0 "Generate"
#define BTN_TXT_1 "Chakras align"
#define BTN_TXT_2 "Feel the wisdom"
#define BTN_TXT_3 "Enlightenment is only a click away"
#define BTN_TXT_COUNT 4
#define BTN_TXT_DEFAULT 3

// Port protocol getter macro
#define get_protocol(server,i) server->ports[i].is_ssl ? "https" : "http"

// Wrapper struct for page substance
typedef struct pagedata {
    bstring html_template_index;
    bstring html_template_gen;
    bstring html_template_permalink;
    bstring html_template_getbyid;
    char *btn_txts[BTN_TXT_COUNT];
} PageData;

// Wrapper struct for the server
typedef struct yogiserver {
    struct sigaction sigactor;
    struct mg_context *ctx;
    struct mg_callbacks callbacks;
    struct mg_server_ports ports[32];
    int8_t ports_count;
    YogiGen *yogen;
    PageData pg_data;
    char *sys_info;
} YogiServer;

// Public access functions
YogiServer* YogiServer_init();
void YogiServer_close(YogiServer *server);

#endif
