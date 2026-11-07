#include "http.h"

#include <curl/curl.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

/* Buffer de resposta que vai acumulando o corpo. */
typedef struct {
    size_t len;
    size_t cap;
    char  *data;
} resp_buf_t;

/* Teto para o corpo que guardamos (nao precisamos do conteudo inteiro,
 * so o suficiente para comparar tamanho/palavras de forma estavel). */
#define TRESPASS_MAX_BODY (256 * 1024)

static size_t write_cb(char *ptr, size_t size, size_t nmemb, void *userdata) {
    resp_buf_t *buf = (resp_buf_t *)userdata;
    size_t incoming = size * nmemb;

    /* Mesmo que paremos de armazenar, informamos ao curl que "consumimos"
     * tudo, para que ele nao aborte o download. */
    if (buf->len >= TRESPASS_MAX_BODY) {
        buf->len += incoming;
        return incoming;
    }

    size_t storable = incoming;
    if (buf->len + storable > TRESPASS_MAX_BODY) {
        storable = TRESPASS_MAX_BODY - buf->len;
    }

    if (buf->len + storable + 1 > buf->cap) {
        size_t newcap = buf->cap ? buf->cap * 2 : 8192;
        while (newcap < buf->len + storable + 1) newcap *= 2;
        char *tmp = realloc(buf->data, newcap);
        if (!tmp) return 0; /* sinaliza erro ao curl */
        buf->data = tmp;
        buf->cap  = newcap;
    }

    memcpy(buf->data + buf->len, ptr, storable);
    buf->len += incoming; /* contamos o total recebido, nao so o guardado */
    return incoming;
}

static size_t count_words(const char *s, size_t n) {
    size_t words = 0;
    int in_word = 0;
    for (size_t i = 0; i < n; i++) {
        if (isspace((unsigned char)s[i])) {
            in_word = 0;
        } else if (!in_word) {
            in_word = 1;
            words++;
        }
    }
    return words;
}

int http_global_init(void) {
    return curl_global_init(CURL_GLOBAL_DEFAULT) == CURLE_OK ? 0 : -1;
}

void http_global_cleanup(void) {
    curl_global_cleanup();
}

int http_request(const char *method,
                 const char *url,
                 char *const *headers,
                 int nheaders,
                 const http_config_t *cfg,
                 http_result_t *out) {
    memset(out, 0, sizeof(*out));

    CURL *curl = curl_easy_init();
    if (!curl) {
        snprintf(out->error, sizeof(out->error), "curl_easy_init falhou");
        return -1;
    }

    resp_buf_t buf = {0, 0, NULL};
    struct curl_slist *hlist = NULL;
    for (int i = 0; i < nheaders; i++) {
        struct curl_slist *tmp = curl_slist_append(hlist, headers[i]);
        if (!tmp) {
            curl_slist_free_all(hlist);
            curl_easy_cleanup(curl);
            free(buf.data);
            snprintf(out->error, sizeof(out->error), "falha ao montar headers");
            return -1;
        }
        hlist = tmp;
    }

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, method);
    curl_easy_setopt(curl, CURLOPT_NOBODY, 0L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buf);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, cfg->timeout_ms);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION,
                     cfg->follow_redirects ? 1L : 0L);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L); /* seguro em threads */

    if (!cfg->verify_tls) {
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    }
    if (cfg->user_agent)
        curl_easy_setopt(curl, CURLOPT_USERAGENT, cfg->user_agent);
    if (cfg->proxy)
        curl_easy_setopt(curl, CURLOPT_PROXY, cfg->proxy);
    if (hlist)
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, hlist);

    /* Nao deixa a URL ser "normalizada" pelo curl: queremos que
     * payloads como /..;/ cheguem exatamente como escritos. */
    curl_easy_setopt(curl, CURLOPT_PATH_AS_IS, 1L);

    CURLcode rc = curl_easy_perform(curl);

    int ret = 0;
    if (rc != CURLE_OK) {
        snprintf(out->error, sizeof(out->error), "%s",
                 curl_easy_strerror(rc));
        ret = -1;
    } else {
        long code = 0;
        double total = 0.0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);
        curl_easy_getinfo(curl, CURLINFO_TOTAL_TIME, &total);
        out->status     = code;
        out->body_len   = buf.len;
        out->word_count = buf.data ? count_words(buf.data,
                            buf.len < TRESPASS_MAX_BODY ? buf.len
                                                        : TRESPASS_MAX_BODY)
                                   : 0;
        out->elapsed_ms = total * 1000.0;
    }

    curl_slist_free_all(hlist);
    curl_easy_cleanup(curl);
    free(buf.data);
    return ret;
}
