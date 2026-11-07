#include "engine.h"
#include "payloads.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
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
    int           ran;   /* 1 depois de executado */
} testcase_t;

typedef struct {
    testcase_t *cases;
    size_t      count;
    size_t      cap;
} caselist_t;

/* ---------- construcao da lista de casos ---------- */

static void cl_init(caselist_t *cl) {
    cl->cases = NULL;
    cl->count = 0;
    cl->cap   = 0;
}

static testcase_t *cl_new(caselist_t *cl) {
    if (cl->count == cl->cap) {
        size_t newcap = cl->cap ? cl->cap * 2 : 128;
        testcase_t *tmp = realloc(cl->cases, newcap * sizeof(testcase_t));
        if (!tmp) {
            fprintf(stderr, "sem memoria\n");
            exit(1);
        }
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

/*
 * Divide a URL em base ("https://host:port") e path ("/algo?x=1").
 * `base` e `path` devem ter espaco suficiente (tamanho da url basta).
 */
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

/* Versao do path com a caixa invertida de cada letra. */
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

/* Substitui todas as ocorrencias de `needle` por `rep` (bounded).
 * Nao usa printf, entao e seguro com templates vindos de wordlist. */
static void replace_all(const char *src, const char *needle, const char *rep,
                        char *out, size_t n) {
    size_t nl = strlen(needle);
    size_t o = 0;
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
    const char *ip = o->ip_value ? o->ip_value : NULL;

    /* --- verb tampering --- */
    if (o->tech_mask & TECH_VERBS) {
        for (size_t i = 0; i < PL_METHODS_N; i++) {
            testcase_t *tc = cl_new(cl);
            snprintf(tc->technique, sizeof(tc->technique), "verb");
            snprintf(tc->method, sizeof(tc->method), "%s", PL_METHODS[i]);
            snprintf(tc->url, sizeof(tc->url), "%s%s", base, path);
            snprintf(tc->desc, sizeof(tc->desc), "metodo %s", PL_METHODS[i]);
        }
    }

    /* --- headers de IP interno --- */
    if (o->tech_mask & TECH_FAKE_IP) {
        for (size_t h = 0; h < PL_FAKE_IP_HEADERS_N; h++) {
            if (ip) {
                testcase_t *tc = cl_new(cl);
                snprintf(tc->technique, sizeof(tc->technique), "header-ip");
                snprintf(tc->method, sizeof(tc->method), "GET");
                snprintf(tc->url, sizeof(tc->url), "%s%s", base, path);
                tc_add_header(tc, PL_FAKE_IP_HEADERS[h], ip);
                snprintf(tc->desc, sizeof(tc->desc), "%s: %s",
                         PL_FAKE_IP_HEADERS[h], ip);
            } else {
                for (size_t v = 0; v < PL_FAKE_IP_VALUES_N; v++) {
                    testcase_t *tc = cl_new(cl);
                    snprintf(tc->technique, sizeof(tc->technique), "header-ip");
                    snprintf(tc->method, sizeof(tc->method), "GET");
                    snprintf(tc->url, sizeof(tc->url), "%s%s", base, path);
                    tc_add_header(tc, PL_FAKE_IP_HEADERS[h],
                                  PL_FAKE_IP_VALUES[v]);
                    snprintf(tc->desc, sizeof(tc->desc), "%s: %s",
                             PL_FAKE_IP_HEADERS[h], PL_FAKE_IP_VALUES[v]);
                }
            }
        }
    }

    /* --- headers de IP extra (wordlist --headers) --- */
    if ((o->tech_mask & TECH_FAKE_IP) && o->n_extra_headers > 0) {
        const char *val = ip ? ip : "127.0.0.1";
        for (int e = 0; e < o->n_extra_headers; e++) {
            testcase_t *tc = cl_new(cl);
            snprintf(tc->technique, sizeof(tc->technique), "header-ip");
            snprintf(tc->method, sizeof(tc->method), "GET");
            snprintf(tc->url, sizeof(tc->url), "%s%s", base, path);
            tc_add_header(tc, o->extra_headers[e], val);
            snprintf(tc->desc, sizeof(tc->desc), "%s: %s",
                     o->extra_headers[e], val);
        }
    }

    /* --- reescrita de URL via header --- */
    if (o->tech_mask & TECH_URL_REWRITE) {
        for (size_t h = 0; h < PL_URL_REWRITE_HEADERS_N; h++) {
            testcase_t *tc = cl_new(cl);
            snprintf(tc->technique, sizeof(tc->technique), "url-rewrite");
            snprintf(tc->method, sizeof(tc->method), "GET");
            snprintf(tc->url, sizeof(tc->url), "%s/", base);
            tc_add_header(tc, PL_URL_REWRITE_HEADERS[h], path);
            snprintf(tc->desc, sizeof(tc->desc), "/ + %s: %s",
                     PL_URL_REWRITE_HEADERS[h], path);
        }
    }

    /* --- mutacoes de path --- */
    if (o->tech_mask & TECH_PATH) {
        for (size_t m = 0; m < PL_PATH_MUTATIONS_N; m++) {
            char mutated[8192];
            /* o formato pode referenciar o path uma ou duas vezes */
            snprintf(mutated, sizeof(mutated), PL_PATH_MUTATIONS[m],
                     path, path);
            testcase_t *tc = cl_new(cl);
            snprintf(tc->technique, sizeof(tc->technique), "path");
            snprintf(tc->method, sizeof(tc->method), "GET");
            snprintf(tc->url, sizeof(tc->url), "%s%s", base, mutated);
            snprintf(tc->desc, sizeof(tc->desc), "%s", mutated);
        }
        for (size_t s = 0; s < PL_PATH_SUFFIXES_N; s++) {
            testcase_t *tc = cl_new(cl);
            snprintf(tc->technique, sizeof(tc->technique), "path");
            snprintf(tc->method, sizeof(tc->method), "GET");
            snprintf(tc->url, sizeof(tc->url), "%s%s%s", base, path,
                     PL_PATH_SUFFIXES[s]);
            snprintf(tc->desc, sizeof(tc->desc), "%s%s", path,
                     PL_PATH_SUFFIXES[s]);
        }
    }

    /* --- mutacoes de path extra (wordlist --paths) --- */
    if ((o->tech_mask & TECH_PATH) && o->n_extra_paths > 0) {
        for (int e = 0; e < o->n_extra_paths; e++) {
            char mutated[8192];
            if (strstr(o->extra_paths[e], "%s"))
                replace_all(o->extra_paths[e], "%s", path,
                            mutated, sizeof(mutated));
            else
                snprintf(mutated, sizeof(mutated), "%s%s", path,
                         o->extra_paths[e]);
            testcase_t *tc = cl_new(cl);
            snprintf(tc->technique, sizeof(tc->technique), "path");
            snprintf(tc->method, sizeof(tc->method), "GET");
            snprintf(tc->url, sizeof(tc->url), "%s%s", base, mutated);
            snprintf(tc->desc, sizeof(tc->desc), "%s", mutated);
        }
    }

    /* --- troca de caixa --- */
    if (o->tech_mask & TECH_CASE) {
        char swapped[8192];
        swapcase(path, swapped, sizeof(swapped));
        if (strcmp(swapped, path) != 0) {
            testcase_t *tc = cl_new(cl);
            snprintf(tc->technique, sizeof(tc->technique), "case");
            snprintf(tc->method, sizeof(tc->method), "GET");
            snprintf(tc->url, sizeof(tc->url), "%s%s", base, swapped);
            snprintf(tc->desc, sizeof(tc->desc), "%s", swapped);
        }
    }
}

/* ---------- execucao paralela ---------- */

typedef struct {
    caselist_t          *cl;
    const engine_opts_t *opts;
    http_result_t        baseline;
    size_t               next;   /* proximo indice a processar */
    size_t               hits;   /* bypasses provaveis         */
    pthread_mutex_t      mu_idx;
    pthread_mutex_t      mu_out;
} shared_t;

/* Heuristica: o caso e "interessante" comparado a baseline? */
static int is_hit(const http_result_t *base, const http_result_t *r) {
    if (r->status == 0) return 0;              /* erro de transporte */
    if (r->status != base->status) {
        /* mudou o status: so e promissor se saiu do 4xx */
        if (r->status < 400 || r->status >= 500) return 1;
        return 0;
    }
    /* mesmo status: corpo bem diferente pode indicar conteudo liberado */
    long d = (long)r->body_len - (long)base->body_len;
    if (d < 0) d = -d;
    if (base->body_len > 0 && d > (long)(base->body_len / 10) && d > 32)
        return 1;
    return 0;
}

static const char *status_color(long s) {
    if (s >= 200 && s < 300) return C_GREEN;
    if (s >= 300 && s < 400) return C_CYAN;
    if (s >= 400 && s < 500) return C_RED;
    return C_YELLOW;
}

static void print_case(shared_t *sh, const testcase_t *tc, int hit) {
    const http_result_t *r = &tc->result;
    pthread_mutex_lock(&sh->mu_out);
    if (r->status == 0) {
        if (sh->opts->show_all)
            fprintf(stderr, C_DIM "  [err] %-11s %-7s %-28.28s  %s\n" C_RESET,
                    tc->technique, tc->method, tc->desc, r->error);
    } else {
        const char *mark = hit ? C_BOLD C_GREEN " <== BYPASS?" C_RESET : "";
        printf("%s%3ld%s  %-11s %-7s  %6zuB %5zuw  %-30.30s%s\n",
               status_color(r->status), r->status, C_RESET,
               tc->technique, tc->method, r->body_len, r->word_count,
               tc->desc, mark);
    }
    pthread_mutex_unlock(&sh->mu_out);
}

static void *worker(void *arg) {
    shared_t *sh = (shared_t *)arg;
    for (;;) {
        pthread_mutex_lock(&sh->mu_idx);
        size_t i = sh->next;
        if (i < sh->cl->count) sh->next++;
        pthread_mutex_unlock(&sh->mu_idx);
        if (i >= sh->cl->count) break;

        testcase_t *tc = &sh->cl->cases[i];
        char *hdrs[MAX_HEADERS];
        for (int h = 0; h < tc->nheaders; h++) hdrs[h] = tc->headers[h];

        http_request(tc->method, tc->url, hdrs, tc->nheaders,
                     &sh->opts->http, &tc->result);
        tc->ran = 1;

        int hit = is_hit(&sh->baseline, &tc->result);
        if (hit) {
            pthread_mutex_lock(&sh->mu_idx);
            sh->hits++;
            pthread_mutex_unlock(&sh->mu_idx);
        }
        if (hit || sh->opts->show_all) print_case(sh, tc, hit);

        if (sh->opts->http.delay_ms > 0) {
            struct timespec ts = {
                sh->opts->http.delay_ms / 1000,
                (sh->opts->http.delay_ms % 1000) * 1000000L
            };
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
        fputs("target,technique,method,url,headers,status,length,words,ms,hit\n",
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
                     const char *headers, const http_result_t *r, int hit) {
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
                "\"ms\":%.1f,\"hit\":%s}",
                r->status, r->body_len, r->word_count, r->elapsed_ms,
                hit ? "true" : "false");
    } else { /* CSV */
        csv_field(s->fp, target);  fputc(',', s->fp);
        csv_field(s->fp, tech);    fputc(',', s->fp);
        csv_field(s->fp, method);  fputc(',', s->fp);
        csv_field(s->fp, testurl); fputc(',', s->fp);
        csv_field(s->fp, headers); fputc(',', s->fp);
        fprintf(s->fp, "%ld,%zu,%zu,%.1f,%d\n",
                r->status, r->body_len, r->word_count, r->elapsed_ms,
                hit ? 1 : 0);
    }
    s->count++;
}

int engine_run(const char *url, const engine_opts_t *opts, sink_t *sink) {
    char base[8192], path[8192];
    if (split_url(url, base, path, sizeof(base)) != 0) {
        fprintf(stderr, "URL invalida: %s\n", url);
        return -1;
    }

    /* baseline: a requisicao "normal" que deve dar 403/401 */
    http_result_t baseline;
    char *nohdr[1];
    if (http_request("GET", url, nohdr, 0, &opts->http, &baseline) != 0) {
        fprintf(stderr, "falha na baseline: %s\n", baseline.error);
        return -1;
    }

    printf(C_BOLD "\n  alvo    " C_RESET "%s\n", url);
    printf(C_BOLD "  base    " C_RESET "%s%s\n", base, path);
    printf(C_BOLD "  baseline" C_RESET " status=%s%ld%s  %zuB  %zuw\n\n",
           status_color(baseline.status), baseline.status, C_RESET,
           baseline.body_len, baseline.word_count);
    printf(C_DIM "  cod  tecnica     metodo    tamanho  palavras  payload"
           C_RESET "\n");
    printf(C_DIM "  ---------------------------------------------------------"
           C_RESET "\n");

    caselist_t cl;
    cl_init(&cl);
    build_cases(&cl, base, path, opts);

    shared_t sh;
    sh.cl       = &cl;
    sh.opts     = opts;
    sh.baseline = baseline;
    sh.next     = 0;
    sh.hits     = 0;
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

    printf("\n" C_BOLD "  %zu testes executados, %zu bypass(es) provavel(is)"
           C_RESET "\n\n", cl.count, sh.hits);

    /* saida estruturada opcional */
    if (sink && sink->fp) {
        sink_row(sink, url, "baseline", "GET", url, "", &baseline, 0);
        for (size_t i = 0; i < cl.count; i++) {
            testcase_t *tc = &cl.cases[i];
            if (!tc->ran) continue;
            char hjoin[1024] = "";
            for (int h = 0; h < tc->nheaders; h++) {
                if (h) strncat(hjoin, "; ",
                               sizeof(hjoin) - strlen(hjoin) - 1);
                strncat(hjoin, tc->headers[h],
                        sizeof(hjoin) - strlen(hjoin) - 1);
            }
            int hit = is_hit(&baseline, &tc->result);
            sink_row(sink, url, tc->technique, tc->method, tc->url,
                     hjoin, &tc->result, hit);
        }
    }

    free(tids);
    free(cl.cases);
    pthread_mutex_destroy(&sh.mu_idx);
    pthread_mutex_destroy(&sh.mu_out);
    return 0;
}
