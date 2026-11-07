#include "engine.h"
#include "payloads.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <stdarg.h>
#include <unistd.h>
#include <pthread.h>

/* ---------- cores do terminal ---------- */
#define C_RESET  "\033[0m"
#define C_DIM    "\033[2m"
#define C_RED    "\033[31m"
#define C_GREEN  "\033[32m"
#define C_YELLOW "\033[33m"
#define C_CYAN   "\033[36m"
#define C_BOLD   "\033[1m"

#define MAX_HEADERS 4

typedef struct {
    char technique[24];
    char desc[320];
    char method[16];
    char url[8192];
    char headers[MAX_HEADERS][320];
    int  nheaders;

    http_result_t result;
    int           ran;
    int           hit;
} testcase_t;

typedef struct {
    testcase_t *cases;
    size_t      count;
    size_t      cap;
} caselist_t;

/* ---------- util ---------- */

static size_t app(char *out, size_t n, size_t o, const char *fmt, ...) {
    if (o >= n) return o;
    va_list ap;
    va_start(ap, fmt);
    int w = vsnprintf(out + o, n - o, fmt, ap);
    va_end(ap);
    if (w < 0) return o;
    o += (size_t)w;
    return o >= n ? n - 1 : o;
}

static int in_int(const int *a, int n, long v) {
    for (int i = 0; i < n; i++) if (a[i] == (int)v) return 1;
    return 0;
}
static int in_long(const long *a, int n, long v) {
    for (int i = 0; i < n; i++) if (a[i] == v) return 1;
    return 0;
}

/* ---------- construcao da lista de casos ---------- */

static void cl_init(caselist_t *cl) { cl->cases = NULL; cl->count = 0; cl->cap = 0; }

static testcase_t *cl_new(caselist_t *cl) {
    if (cl->count == cl->cap) {
        size_t newcap = cl->cap ? cl->cap * 2 : 128;
        testcase_t *tmp = realloc(cl->cases, newcap * sizeof(testcase_t));
        if (!tmp) { fprintf(stderr, "sem memoria\n"); exit(1); }
        cl->cases = tmp;
        cl->cap   = newcap;
    }
    testcase_t *tc = &cl->cases[cl->count++];
    memset(tc, 0, sizeof(*tc));
    return tc;
}

static void tc_add_header(testcase_t *tc, const char *name, const char *val) {
    if (tc->nheaders >= MAX_HEADERS) return;
    snprintf(tc->headers[tc->nheaders], sizeof(tc->headers[0]),
             "%s: %s", name, val);
    tc->nheaders++;
}

static testcase_t *mk(caselist_t *cl, const char *tech, const char *method,
                      const char *url) {
    testcase_t *tc = cl_new(cl);
    snprintf(tc->technique, sizeof(tc->technique), "%s", tech);
    snprintf(tc->method, sizeof(tc->method), "%s", method);
    snprintf(tc->url, sizeof(tc->url), "%s", url);
    return tc;
}

static int split_url(const char *url, char *base, char *path, size_t n) {
    const char *p = strstr(url, "://");
    if (!p) return -1;
    p += 3;
    const char *slash = strchr(p, '/');
    if (!slash) {
        snprintf(base, n, "%s", url);
        snprintf(path, n, "/");
        return 0;
    }
    size_t blen = (size_t)(slash - url);
    if (blen >= n) return -1;
    memcpy(base, url, blen);
    base[blen] = '\0';
    snprintf(path, n, "%s", slash);
    return 0;
}

static void swapcase(const char *in, char *out, size_t n) {
    size_t i = 0;
    for (; in[i] && i + 1 < n; i++) {
        unsigned char c = (unsigned char)in[i];
        if (islower(c))      out[i] = (char)toupper(c);
        else if (isupper(c)) out[i] = (char)tolower(c);
        else                 out[i] = (char)c;
    }
    out[i] = '\0';
}

static void replace_all(const char *src, const char *needle, const char *rep,
                        char *out, size_t n) {
    size_t nl = strlen(needle), o = 0;
    while (*src && o + 1 < n) {
        if (nl && strncmp(src, needle, nl) == 0) {
            for (const char *r = rep; *r && o + 1 < n; r++) out[o++] = *r;
            src += nl;
        } else {
            out[o++] = *src++;
        }
    }
    out[o] = '\0';
}

static void build_cases(caselist_t *cl, const char *base, const char *path,
                        const engine_opts_t *o) {
    const char *ip = o->ip_value;
    char url[8192];

    if (o->tech_mask & TECH_VERBS) {
        snprintf(url, sizeof(url), "%s%s", base, path);
        for (size_t i = 0; i < PL_METHODS_N; i++) {
            testcase_t *tc = mk(cl, "verb", PL_METHODS[i], url);
            snprintf(tc->desc, sizeof(tc->desc), "metodo %s", PL_METHODS[i]);
        }
    }

    if (o->tech_mask & TECH_FAKE_IP) {
        snprintf(url, sizeof(url), "%s%s", base, path);
        for (size_t h = 0; h < PL_FAKE_IP_HEADERS_N; h++) {
            if (ip) {
                testcase_t *tc = mk(cl, "header-ip", "GET", url);
                tc_add_header(tc, PL_FAKE_IP_HEADERS[h], ip);
                snprintf(tc->desc, sizeof(tc->desc), "%s: %s",
                         PL_FAKE_IP_HEADERS[h], ip);
            } else {
                for (size_t v = 0; v < PL_FAKE_IP_VALUES_N; v++) {
                    testcase_t *tc = mk(cl, "header-ip", "GET", url);
                    tc_add_header(tc, PL_FAKE_IP_HEADERS[h], PL_FAKE_IP_VALUES[v]);
                    snprintf(tc->desc, sizeof(tc->desc), "%s: %s",
                             PL_FAKE_IP_HEADERS[h], PL_FAKE_IP_VALUES[v]);
                }
            }
        }
        for (int e = 0; e < o->n_extra_headers; e++) {
            const char *val = ip ? ip : "127.0.0.1";
            testcase_t *tc = mk(cl, "header-ip", "GET", url);
            tc_add_header(tc, o->extra_headers[e], val);
            snprintf(tc->desc, sizeof(tc->desc), "%s: %s",
                     o->extra_headers[e], val);
        }
    }

    if (o->tech_mask & TECH_HOST) {
        snprintf(url, sizeof(url), "%s%s", base, path);
        for (size_t v = 0; v < PL_HOST_VALUES_N; v++) {
            testcase_t *tc = mk(cl, "host", "GET", url);
            tc_add_header(tc, "Host", PL_HOST_VALUES[v]);
            snprintf(tc->desc, sizeof(tc->desc), "Host: %s", PL_HOST_VALUES[v]);
        }
    }

    if (o->tech_mask & TECH_URL_REWRITE) {
        snprintf(url, sizeof(url), "%s/", base);
        for (size_t h = 0; h < PL_URL_REWRITE_HEADERS_N; h++) {
            testcase_t *tc = mk(cl, "url-rewrite", "GET", url);
            tc_add_header(tc, PL_URL_REWRITE_HEADERS[h], path);
            snprintf(tc->desc, sizeof(tc->desc), "/ + %s: %s",
                     PL_URL_REWRITE_HEADERS[h], path);
        }
    }

    if (o->tech_mask & TECH_PATH) {
        for (size_t m = 0; m < PL_PATH_MUTATIONS_N; m++) {
            char mutated[8192];
            snprintf(mutated, sizeof(mutated), PL_PATH_MUTATIONS[m], path, path);
            snprintf(url, sizeof(url), "%s%s", base, mutated);
            testcase_t *tc = mk(cl, "path", "GET", url);
            snprintf(tc->desc, sizeof(tc->desc), "%s", mutated);
        }
        for (size_t s = 0; s < PL_PATH_SUFFIXES_N; s++) {
            snprintf(url, sizeof(url), "%s%s%s", base, path, PL_PATH_SUFFIXES[s]);
            testcase_t *tc = mk(cl, "path", "GET", url);
            snprintf(tc->desc, sizeof(tc->desc), "%s%s", path, PL_PATH_SUFFIXES[s]);
        }
        for (int e = 0; e < o->n_extra_paths; e++) {
            char mutated[8192];
            if (strstr(o->extra_paths[e], "%s"))
                replace_all(o->extra_paths[e], "%s", path, mutated, sizeof(mutated));
            else
                snprintf(mutated, sizeof(mutated), "%s%s", path, o->extra_paths[e]);
            snprintf(url, sizeof(url), "%s%s", base, mutated);
            testcase_t *tc = mk(cl, "path", "GET", url);
            snprintf(tc->desc, sizeof(tc->desc), "%s", mutated);
        }
    }

    if (o->tech_mask & TECH_CASE) {
        char swapped[8192];
        swapcase(path, swapped, sizeof(swapped));
        if (strcmp(swapped, path) != 0) {
            snprintf(url, sizeof(url), "%s%s", base, swapped);
            testcase_t *tc = mk(cl, "case", "GET", url);
            snprintf(tc->desc, sizeof(tc->desc), "%s", swapped);
        }
    }

    /* --- tecnicas combinadas --- */
    if (o->combine && (o->tech_mask & TECH_PATH)) {
        const char *val = ip ? ip : "127.0.0.1";
        for (size_t m = 0; m < PL_PATH_MUTATIONS_N; m++) {
            char mutated[8192];
            snprintf(mutated, sizeof(mutated), PL_PATH_MUTATIONS[m], path, path);
            snprintf(url, sizeof(url), "%s%s", base, mutated);
            testcase_t *tc = mk(cl, "path+ip", "GET", url);
            tc_add_header(tc, "X-Forwarded-For", val);
            snprintf(tc->desc, sizeof(tc->desc), "%s + XFF:%s", mutated, val);
        }
        const char *verbs[] = {"POST", "PUT", "PATCH"};
        snprintf(url, sizeof(url), "%s%s/", base, path);
        for (size_t v = 0; v < sizeof(verbs) / sizeof(verbs[0]); v++) {
            testcase_t *tc = mk(cl, "verb+path", verbs[v], url);
            snprintf(tc->desc, sizeof(tc->desc), "%s %s/", verbs[v], path);
        }
    }
}

/* ---------- avaliacao de hit ---------- */

static int passes_filters(const engine_opts_t *o, const http_result_t *r) {
    if (in_int(o->fc, o->n_fc, r->status)) return 0;
    if (in_long(o->fs, o->n_fs, (long)r->body_len)) return 0;
    if (in_long(o->fw, o->n_fw, (long)r->word_count)) return 0;
    return 1;
}

static int is_hit(const engine_opts_t *o, const http_result_t *base,
                  const http_result_t *calib, const http_result_t *r) {
    if (r->status == 0) return 0;
    if (!passes_filters(o, r)) return 0;

    if (o->n_mc > 0)                       /* match codes explicitos */
        return in_int(o->mc, o->n_mc, r->status);

    int interesting = 0;
    if (r->status != base->status) {
        if (r->status < 400 || r->status >= 500) interesting = 1;
    } else {
        long d = (long)r->body_len - (long)base->body_len;
        if (d < 0) d = -d;
        if (base->body_len > 0 && d > (long)(base->body_len / 10) && d > 32)
            interesting = 1;
    }
    if (!interesting) return 0;

    /* calibracao: se bate com a resposta de path aleatorio, e ruido */
    if (o->calibrate && calib->status && calib->status == r->status) {
        long d = (long)r->body_len - (long)calib->body_len;
        if (d < 0) d = -d;
        if (d <= 32) return 0;
    }
    return 1;
}

/* curl reproduzivel do caso */
static void build_curl(const testcase_t *tc, const http_config_t *cfg,
                       char *out, size_t n) {
    size_t o = app(out, n, 0, "curl -s");
    if (!cfg->verify_tls)       o = app(out, n, o, "k");
    if (cfg->follow_redirects)  o = app(out, n, o, "L");
    if (cfg->http_version == 1) o = app(out, n, o, " --http1.0");
    else if (cfg->http_version == 3) o = app(out, n, o, " --http2");
    if (cfg->proxy)      o = app(out, n, o, " -x '%s'", cfg->proxy);
    if (cfg->user_agent) o = app(out, n, o, " -A '%s'", cfg->user_agent);
    o = app(out, n, o, " -X %s", tc->method);
    for (int h = 0; h < tc->nheaders; h++)
        o = app(out, n, o, " -H '%s'", tc->headers[h]);
    for (int e = 0; e < cfg->n_extra_headers; e++)
        o = app(out, n, o, " -H '%s'", cfg->extra_headers[e]);
    app(out, n, o, " '%s'", tc->url);
}

/* ---------- execucao paralela ---------- */

typedef struct {
    caselist_t          *cl;
    const engine_opts_t *opts;
    http_result_t        baseline;
    http_result_t        calib;
    size_t               next;
    size_t               done;
    size_t               hits;
    int                  progress;   /* 1 = barra no stderr */
    pthread_mutex_t      mu_idx;
    pthread_mutex_t      mu_out;
} shared_t;

static const char *status_color(long s) {
    if (s >= 200 && s < 300) return C_GREEN;
    if (s >= 300 && s < 400) return C_CYAN;
    if (s >= 400 && s < 500) return C_RED;
    return C_YELLOW;
}

static void clear_progress(shared_t *sh) {
    if (sh->progress) fprintf(stderr, "\r\033[K");
}

static void tick_progress(shared_t *sh, size_t done) {
    if (!sh->progress) return;
    size_t total = sh->cl->count;
    int pct = total ? (int)(done * 100 / total) : 100;
    fprintf(stderr, "\r" C_DIM "  [%zu/%zu] %d%%" C_RESET, done, total, pct);
    fflush(stderr);
}

static void print_case(shared_t *sh, const testcase_t *tc) {
    const http_result_t *r = &tc->result;
    const engine_opts_t *o = sh->opts;
    clear_progress(sh);
    const char *mark = tc->hit ? C_BOLD C_GREEN " <== BYPASS?" C_RESET : "";
    printf("%s%3ld%s  %-11s %-7s  %6zuB %5zuw  %-30.30s%s\n",
           status_color(r->status), r->status, C_RESET,
           tc->technique, tc->method, r->body_len, r->word_count,
           tc->desc, mark);
    if (tc->hit && o->show_curl) {
        char curl[2048];
        build_curl(tc, &o->http, curl, sizeof(curl));
        printf(C_DIM "      %s" C_RESET "\n", curl);
    }
}

static void *worker(void *arg) {
    shared_t *sh = (shared_t *)arg;
    const engine_opts_t *o = sh->opts;
    for (;;) {
        pthread_mutex_lock(&sh->mu_idx);
        size_t i = sh->next;
        if (i < sh->cl->count) sh->next++;
        pthread_mutex_unlock(&sh->mu_idx);
        if (i >= sh->cl->count) break;

        testcase_t *tc = &sh->cl->cases[i];
        char *hdrs[MAX_HEADERS + 1];
        int nh = tc->nheaders;
        for (int h = 0; h < nh; h++) hdrs[h] = tc->headers[h];
        char ua_hdr[320];
        if (o->rotate_ua) {
            snprintf(ua_hdr, sizeof(ua_hdr), "User-Agent: %s",
                     PL_USER_AGENTS[i % PL_USER_AGENTS_N]);
            hdrs[nh++] = ua_hdr;
        }

        http_request(tc->method, tc->url, hdrs, nh, &o->http, &tc->result);

        /* backoff em 429 (WAF/rate-limit) */
        int tries = 0;
        while (tc->result.status == 429 && tries < 3) {
            struct timespec ts = {0, (300L + 300L * tries) * 1000000L};
            nanosleep(&ts, NULL);
            http_request(tc->method, tc->url, hdrs, nh, &o->http, &tc->result);
            tries++;
        }
        tc->ran = 1;
        tc->hit = is_hit(o, &sh->baseline, &sh->calib, &tc->result);

        pthread_mutex_lock(&sh->mu_idx);
        if (tc->hit) sh->hits++;
        sh->done++;
        size_t done = sh->done;
        pthread_mutex_unlock(&sh->mu_idx);

        pthread_mutex_lock(&sh->mu_out);
        if (tc->hit || (o->show_all && !o->quiet))
            print_case(sh, tc);
        else if (tc->result.status == 0 && o->verbose) {
            clear_progress(sh);
            fprintf(stderr, C_DIM "  [err] %-11s %-7s %-28.28s  %s\n" C_RESET,
                    tc->technique, tc->method, tc->desc, tc->result.error);
        }
        tick_progress(sh, done);
        pthread_mutex_unlock(&sh->mu_out);

        if (o->http.delay_ms > 0) {
            struct timespec ts = {o->http.delay_ms / 1000,
                                  (o->http.delay_ms % 1000) * 1000000L};
            nanosleep(&ts, NULL);
        }
    }
    return NULL;
}

/* ---------- saida estruturada (JSON/CSV) ---------- */

static void json_str(FILE *fp, const char *s) {
    fputc('"', fp);
    for (; *s; s++) {
        unsigned char c = (unsigned char)*s;
        if (c == '"' || c == '\\') { fputc('\\', fp); fputc(c, fp); }
        else if (c == '\n') fputs("\\n", fp);
        else if (c == '\r') fputs("\\r", fp);
        else if (c == '\t') fputs("\\t", fp);
        else if (c < 0x20)  fprintf(fp, "\\u%04x", c);
        else fputc(c, fp);
    }
    fputc('"', fp);
}

static void csv_field(FILE *fp, const char *s) {
    fputc('"', fp);
    for (; *s; s++) { if (*s == '"') fputc('"', fp); fputc(*s, fp); }
    fputc('"', fp);
}

int sink_open(sink_t *s, const char *path, out_format_t fmt) {
    s->fp = strcmp(path, "-") == 0 ? stdout : fopen(path, "w");
    if (!s->fp) return -1;
    s->fmt = fmt;
    s->count = 0;
    if (fmt == OUT_JSON)
        fputc('[', s->fp);
    else if (fmt == OUT_CSV)
        fputs("target,technique,method,url,headers,status,length,words,ms,hit,curl\n",
              s->fp);
    return 0;
}

void sink_close(sink_t *s) {
    if (!s || !s->fp) return;
    if (s->fmt == OUT_JSON) fputs("\n]\n", s->fp);
    if (s->fp != stdout) fclose(s->fp);
    s->fp = NULL;
}

static void sink_row(sink_t *s, const char *target, const char *tech,
                     const char *method, const char *testurl,
                     const char *headers, const http_result_t *r, int hit,
                     const char *curl) {
    if (!s || !s->fp) return;
    if (s->fmt == OUT_JSON) {
        fputs(s->count ? ",\n  " : "\n  ", s->fp);
        fputc('{', s->fp);
        fputs("\"target\":",    s->fp); json_str(s->fp, target);
        fputs(",\"technique\":", s->fp); json_str(s->fp, tech);
        fputs(",\"method\":",   s->fp); json_str(s->fp, method);
        fputs(",\"url\":",      s->fp); json_str(s->fp, testurl);
        fputs(",\"headers\":",  s->fp); json_str(s->fp, headers);
        fprintf(s->fp,
                ",\"status\":%ld,\"length\":%zu,\"words\":%zu,"
                "\"ms\":%.1f,\"hit\":%s,\"curl\":",
                r->status, r->body_len, r->word_count, r->elapsed_ms,
                hit ? "true" : "false");
        json_str(s->fp, curl ? curl : "");
        fputc('}', s->fp);
    } else {
        csv_field(s->fp, target);  fputc(',', s->fp);
        csv_field(s->fp, tech);    fputc(',', s->fp);
        csv_field(s->fp, method);  fputc(',', s->fp);
        csv_field(s->fp, testurl); fputc(',', s->fp);
        csv_field(s->fp, headers); fputc(',', s->fp);
        fprintf(s->fp, "%ld,%zu,%zu,%.1f,%d,",
                r->status, r->body_len, r->word_count, r->elapsed_ms, hit ? 1 : 0);
        csv_field(s->fp, curl ? curl : "");
        fputc('\n', s->fp);
    }
    s->count++;
}

static void rand_path(char *out, size_t n) {
    static const char cs[] = "abcdefghijklmnopqrstuvwxyz0123456789";
    size_t i = 0;
    for (; i + 1 < n && i < 12; i++) out[i] = cs[rand() % (sizeof(cs) - 1)];
    out[i] = '\0';
}

int engine_run(const char *url, const engine_opts_t *opts, sink_t *sink) {
    char base[8192], path[8192];
    if (split_url(url, base, path, sizeof(base)) != 0) {
        fprintf(stderr, "URL invalida: %s\n", url);
        return -1;
    }

    http_result_t baseline;
    char *nohdr[1];
    if (http_request("GET", url, nohdr, 0, &opts->http, &baseline) != 0) {
        fprintf(stderr, "falha na baseline: %s\n", baseline.error);
        return -1;
    }

    /* calibracao: path aleatorio para aprender a resposta "generica" */
    http_result_t calib;
    memset(&calib, 0, sizeof(calib));
    if (opts->calibrate) {
        char rnd[16], cal_url[8192];
        rand_path(rnd, sizeof(rnd));
        snprintf(cal_url, sizeof(cal_url), "%s/%s", base, rnd);
        http_request("GET", cal_url, nohdr, 0, &opts->http, &calib);
    }

    if (!opts->quiet) {
        printf(C_BOLD "\n  alvo    " C_RESET "%s\n", url);
        printf(C_BOLD "  base    " C_RESET "%s%s\n", base, path);
        printf(C_BOLD "  baseline" C_RESET " status=%s%ld%s  %zuB  %zuw\n",
               status_color(baseline.status), baseline.status, C_RESET,
               baseline.body_len, baseline.word_count);
        if (opts->calibrate && calib.status) {
            printf(C_BOLD "  calib   " C_RESET "status=%s%ld%s  %zuB  (path aleatorio)\n",
                   status_color(calib.status), calib.status, C_RESET,
                   calib.body_len);
            if (calib.status >= 200 && calib.status < 300)
                printf(C_YELLOW "  [!] path inexistente retorna 2xx: "
                       "respostas por status sao pouco confiaveis\n" C_RESET);
        }
        printf(C_DIM "\n  cod  tecnica     metodo    tamanho  palavras  payload"
               C_RESET "\n");
        printf(C_DIM "  ---------------------------------------------------------"
               C_RESET "\n");
    }

    caselist_t cl;
    cl_init(&cl);
    build_cases(&cl, base, path, opts);

    shared_t sh;
    sh.cl       = &cl;
    sh.opts     = opts;
    sh.baseline = baseline;
    sh.calib    = calib;
    sh.next     = 0;
    sh.done     = 0;
    sh.hits     = 0;
    sh.progress = (!opts->quiet && isatty(STDERR_FILENO)) ? 1 : 0;
    pthread_mutex_init(&sh.mu_idx, NULL);
    pthread_mutex_init(&sh.mu_out, NULL);

    int nthreads = opts->threads > 0 ? opts->threads : 1;
    if ((size_t)nthreads > cl.count && cl.count > 0)
        nthreads = (int)cl.count;

    pthread_t *tids = calloc((size_t)nthreads, sizeof(pthread_t));
    for (int t = 0; t < nthreads; t++)
        pthread_create(&tids[t], NULL, worker, &sh);
    for (int t = 0; t < nthreads; t++)
        pthread_join(tids[t], NULL);

    clear_progress(&sh);
    if (!opts->quiet || sh.hits)
        printf("\n" C_BOLD "  %zu testes executados, %zu bypass(es) provavel(is)"
               C_RESET "\n\n", cl.count, sh.hits);

    if (sink && sink->fp) {
        char curl[2048];
        sink_row(sink, url, "baseline", "GET", url, "", &baseline, 0, "");
        for (size_t i = 0; i < cl.count; i++) {
            testcase_t *tc = &cl.cases[i];
            if (!tc->ran) continue;
            char hjoin[1024] = "";
            for (int h = 0; h < tc->nheaders; h++) {
                if (h) strncat(hjoin, "; ", sizeof(hjoin) - strlen(hjoin) - 1);
                strncat(hjoin, tc->headers[h], sizeof(hjoin) - strlen(hjoin) - 1);
            }
            build_curl(tc, &opts->http, curl, sizeof(curl));
            sink_row(sink, url, tc->technique, tc->method, tc->url,
                     hjoin, &tc->result, tc->hit, curl);
        }
    }

    free(tids);
    free(cl.cases);
    pthread_mutex_destroy(&sh.mu_idx);
    pthread_mutex_destroy(&sh.mu_out);
    return 0;
}
