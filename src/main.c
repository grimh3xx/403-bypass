#include "engine.h"
#include "http.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define VERSION "0.3.0"

static const char *BANNER =
"\033[36m"
"  _ _  ___ ____    _                                \n"
" | | || _ \\__ /   | |__ _  _ _ __  __ _ ___ ___     \n"
" |_  _||  _/|_ \\   | '_ \\ || | '_ \\/ _` (_-<(_-<     \n"
"   |_| |_| |___/   |_.__/\\_, | .__/\\__,_/__//__/     \n"
"                        |__/|_|   \033[0m\033[2mv" VERSION "\033[0m\n"
"\033[2m  bypass de 403/401 para testes autorizados\033[0m\n";

static void usage(const char *prog) {
    fprintf(stderr,
"\nuso: %s (-u <url> | -f <arquivo>) [opcoes]\n\n"
"alvo\n"
"  -u, --url <url>        URL alvo\n"
"  -f, --file <arquivo>   arquivo com uma URL por linha ('-' = stdin)\n\n"
"requisicao\n"
"  -H, --header <h>       header fixo em TODA req (ex: 'Cookie: a=b'); repetivel\n"
"  -A, --user-agent <ua>  User-Agent customizado\n"
"      --random-agent     rotaciona o User-Agent por requisicao\n"
"  -x, --proxy <url>      proxy (ex: http://127.0.0.1:8080)\n"
"      --http1.0 / --http1.1 / --http2   forca a versao de HTTP\n"
"      --ip <valor>       IP/host unico para os headers de spoof\n"
"  -k, --insecure         nao validar certificado TLS\n"
"  -L, --location         seguir redirecionamentos\n\n"
"desempenho\n"
"  -t, --threads <n>      threads concorrentes (padrao 20)\n"
"  -T, --timeout <ms>     timeout por requisicao (padrao 7000)\n"
"      --delay <ms>       atraso apos cada requisicao (rate limit)\n"
"      --retries <n>      retries em erro de transporte (padrao 0)\n\n"
"tecnicas\n"
"      --only <lista>     verb,ip,rewrite,path,case,host (padrao: todas)\n"
"      --combine          tambem testa tecnicas combinadas (header+path, verb+path)\n"
"      --headers <arq>    wordlist de nomes de header extra\n"
"      --paths <arq>      wordlist de mutacoes de path ('%%s' = path alvo)\n\n"
"match / filter (estilo ffuf)\n"
"      -mc <lista>        so conta como hit estes status (ex: 200,302)\n"
"      -fc <lista>        ignora estes status\n"
"      -fs <lista>        ignora estes tamanhos de corpo\n"
"      -fw <lista>        ignora estas contagens de palavras\n"
"      --no-calibrate     nao calibrar com path aleatorio (anti falso-positivo)\n\n"
"saida\n"
"  -o, --output <arq>     salva em arquivo ('-' = stdout)\n"
"      --format <fmt>     json | csv (inferido da extensao de -o)\n"
"  -a, --all              mostra todos os resultados, nao so os hits\n"
"  -s, --quiet            so hits e resumo (sem banner/tabela)\n"
"  -v, --verbose          mostra erros de transporte\n"
"      --no-curl          nao imprimir o curl reproduzivel\n"
"      --version          mostra a versao e sai\n"
"  -h, --help             esta ajuda\n\n"
"exemplos:\n"
"  %s -u https://alvo.tld/admin -H 'Cookie: session=...' -mc 200,302\n"
"  %s -f urls.txt --combine --random-agent -o saida.json\n\n",
        prog, prog, prog);
}

static int parse_tech(const char *s) {
    int mask = 0;
    char buf[256];
    snprintf(buf, sizeof(buf), "%s", s);
    for (char *tok = strtok(buf, ","); tok; tok = strtok(NULL, ",")) {
        if      (!strcmp(tok, "verb"))    mask |= TECH_VERBS;
        else if (!strcmp(tok, "ip"))      mask |= TECH_FAKE_IP;
        else if (!strcmp(tok, "rewrite")) mask |= TECH_URL_REWRITE;
        else if (!strcmp(tok, "path"))    mask |= TECH_PATH;
        else if (!strcmp(tok, "case"))    mask |= TECH_CASE;
        else if (!strcmp(tok, "host"))    mask |= TECH_HOST;
        else fprintf(stderr, "tecnica desconhecida: %s\n", tok);
    }
    return mask;
}

static int parse_ints(const char *s, int *out, int max) {
    int n = 0;
    char buf[512];
    snprintf(buf, sizeof(buf), "%s", s);
    for (char *t = strtok(buf, ","); t && n < max; t = strtok(NULL, ","))
        out[n++] = atoi(t);
    return n;
}
static int parse_longs(const char *s, long *out, int max) {
    int n = 0;
    char buf[512];
    snprintf(buf, sizeof(buf), "%s", s);
    for (char *t = strtok(buf, ","); t && n < max; t = strtok(NULL, ","))
        out[n++] = atol(t);
    return n;
}

static int arg_eq(const char *a, const char *s, const char *l) {
    return strcmp(a, s) == 0 || strcmp(a, l) == 0;
}

static char **read_lines(const char *path, int *count) {
    FILE *f = strcmp(path, "-") == 0 ? stdin : fopen(path, "r");
    if (!f) { perror(path); return NULL; }
    char **lines = NULL;
    int n = 0, cap = 0;
    char buf[8192];
    while (fgets(buf, sizeof(buf), f)) {
        size_t len = strlen(buf);
        while (len && (buf[len - 1] == '\n' || buf[len - 1] == '\r'))
            buf[--len] = '\0';
        if (len == 0 || buf[0] == '#') continue;
        if (n == cap) {
            cap = cap ? cap * 2 : 16;
            lines = realloc(lines, (size_t)cap * sizeof(char *));
        }
        lines[n++] = strdup(buf);
    }
    if (f != stdin) fclose(f);
    *count = n;
    return lines;
}

static void free_lines(char **l, int n) {
    for (int i = 0; i < n; i++) free(l[i]);
    free(l);
}

static out_format_t fmt_from_name(const char *name) {
    if (!name) return OUT_NONE;
    if (!strcmp(name, "json")) return OUT_JSON;
    if (!strcmp(name, "csv"))  return OUT_CSV;
    return OUT_NONE;
}
static out_format_t fmt_from_ext(const char *path) {
    const char *dot = strrchr(path, '.');
    if (dot && !strcmp(dot, ".csv")) return OUT_CSV;
    return OUT_JSON;
}

int main(int argc, char **argv) {
    const char *url = NULL, *url_file = NULL;
    const char *headers_file = NULL, *paths_file = NULL;
    const char *out_path = NULL, *out_fmt_name = NULL;
    char *hdr_opts[128];
    int  n_hdr_opts = 0;

    engine_opts_t opts;
    memset(&opts, 0, sizeof(opts));
    opts.threads               = 20;
    opts.tech_mask             = TECH_ALL;
    opts.calibrate             = 1;
    opts.show_curl             = 1;
    opts.http.timeout_ms       = 7000;
    opts.http.verify_tls       = 1;
    opts.http.follow_redirects = 0;
    opts.http.user_agent       = "403-bypass/" VERSION;

    for (int i = 1; i < argc; i++) {
        const char *a = argv[i];
        #define NEXT() (++i < argc ? argv[i] : (usage(argv[0]), exit(1), ""))
        if      (arg_eq(a, "-u", "--url"))        url = NEXT();
        else if (arg_eq(a, "-f", "--file"))       url_file = NEXT();
        else if (arg_eq(a, "-H", "--header"))   { if (n_hdr_opts < 128) hdr_opts[n_hdr_opts++] = (char *)NEXT(); }
        else if (arg_eq(a, "-t", "--threads"))    opts.threads = atoi(NEXT());
        else if (arg_eq(a, "-T", "--timeout"))    opts.http.timeout_ms = atol(NEXT());
        else if (!strcmp(a, "--delay"))           opts.http.delay_ms = atol(NEXT());
        else if (!strcmp(a, "--retries"))         opts.http.retries = atoi(NEXT());
        else if (arg_eq(a, "-x", "--proxy"))      opts.http.proxy = NEXT();
        else if (arg_eq(a, "-A", "--user-agent")) opts.http.user_agent = NEXT();
        else if (!strcmp(a, "--random-agent"))    opts.rotate_ua = 1;
        else if (!strcmp(a, "--http1.0"))         opts.http.http_version = 1;
        else if (!strcmp(a, "--http1.1"))         opts.http.http_version = 2;
        else if (!strcmp(a, "--http2"))           opts.http.http_version = 3;
        else if (!strcmp(a, "--ip"))              opts.ip_value = NEXT();
        else if (!strcmp(a, "--headers"))         headers_file = NEXT();
        else if (!strcmp(a, "--paths"))           paths_file = NEXT();
        else if (!strcmp(a, "--combine"))         opts.combine = 1;
        else if (!strcmp(a, "-mc") || !strcmp(a, "--match-code"))  opts.n_mc = parse_ints(NEXT(), opts.mc, MATCH_MAX);
        else if (!strcmp(a, "-fc") || !strcmp(a, "--filter-code")) opts.n_fc = parse_ints(NEXT(), opts.fc, MATCH_MAX);
        else if (!strcmp(a, "-fs") || !strcmp(a, "--filter-size")) opts.n_fs = parse_longs(NEXT(), opts.fs, MATCH_MAX);
        else if (!strcmp(a, "-fw") || !strcmp(a, "--filter-words"))opts.n_fw = parse_longs(NEXT(), opts.fw, MATCH_MAX);
        else if (!strcmp(a, "--no-calibrate"))    opts.calibrate = 0;
        else if (!strcmp(a, "--no-curl"))         opts.show_curl = 0;
        else if (arg_eq(a, "-o", "--output"))     out_path = NEXT();
        else if (!strcmp(a, "--format"))          out_fmt_name = NEXT();
        else if (arg_eq(a, "-k", "--insecure"))   opts.http.verify_tls = 0;
        else if (arg_eq(a, "-L", "--location"))   opts.http.follow_redirects = 1;
        else if (arg_eq(a, "-a", "--all"))        opts.show_all = 1;
        else if (arg_eq(a, "-s", "--quiet"))      opts.quiet = 1;
        else if (arg_eq(a, "-v", "--verbose"))    opts.verbose = 1;
        else if (!strcmp(a, "--only"))            opts.tech_mask = parse_tech(NEXT());
        else if (!strcmp(a, "--version"))       { printf("403-bypass %s\n", VERSION); return 0; }
        else if (arg_eq(a, "-h", "--help"))     { usage(argv[0]); return 0; }
        else { fprintf(stderr, "argumento desconhecido: %s\n", a);
               usage(argv[0]); return 1; }
        #undef NEXT
    }

    if (n_hdr_opts > 0) {
        opts.http.extra_headers   = hdr_opts;
        opts.http.n_extra_headers = n_hdr_opts;
    }

    if (!opts.quiet) fputs(BANNER, stderr);

    if (!url && !url_file) { usage(argv[0]); return 1; }
    if (opts.tech_mask == 0) { fprintf(stderr, "nenhuma tecnica selecionada\n"); return 1; }

    srand((unsigned)time(NULL));

    char **hdr_lines = NULL, **path_lines = NULL;
    if (headers_file)
        opts.extra_headers = hdr_lines = read_lines(headers_file, &opts.n_extra_headers);
    if (paths_file)
        opts.extra_paths = path_lines = read_lines(paths_file, &opts.n_extra_paths);

    char **file_targets = NULL;
    int n_file = 0;
    if (url_file) {
        file_targets = read_lines(url_file, &n_file);
        if (!file_targets && !url) return 1;
    }
    int ntargets = (url ? 1 : 0) + n_file;
    char **targets = calloc((size_t)(ntargets > 0 ? ntargets : 1), sizeof(char *));
    int ti = 0;
    if (url) targets[ti++] = (char *)url;
    for (int i = 0; i < n_file; i++) targets[ti++] = file_targets[i];
    if (ntargets == 0) { fprintf(stderr, "nenhuma URL para testar\n"); return 1; }

    if (http_global_init() != 0) {
        fprintf(stderr, "falha ao inicializar libcurl\n");
        return 1;
    }

    sink_t sink = {0};
    sink_t *sinkp = NULL;
    if (out_path) {
        out_format_t fmt = fmt_from_name(out_fmt_name);
        if (fmt == OUT_NONE) fmt = fmt_from_ext(out_path);
        if (sink_open(&sink, out_path, fmt) != 0) {
            fprintf(stderr, "nao foi possivel abrir %s para escrita\n", out_path);
            http_global_cleanup();
            return 1;
        }
        sinkp = &sink;
    }

    int rc = 0;
    for (int i = 0; i < ntargets; i++) {
        if (ntargets > 1 && !opts.quiet)
            fprintf(stderr, "\n\033[1m[%d/%d]\033[0m %s\n", i + 1, ntargets, targets[i]);
        if (engine_run(targets[i], &opts, sinkp) != 0) rc = 1;
    }

    if (sinkp) {
        sink_close(sinkp);
        if (strcmp(out_path, "-") != 0 && !opts.quiet)
            fprintf(stderr, "resultados salvos em %s\n", out_path);
    }

    http_global_cleanup();
    free(targets);
    free_lines(file_targets, n_file);
    free_lines(hdr_lines, opts.n_extra_headers);
    free_lines(path_lines, opts.n_extra_paths);
    return rc == 0 ? 0 : 1;
}
