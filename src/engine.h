#ifndef TRESPASS_ENGINE_H
#define TRESPASS_ENGINE_H

#include <stdio.h>
#include "http.h"

/* Formato do arquivo de saida estruturada. */
typedef enum {
    OUT_NONE = 0,
    OUT_JSON,
    OUT_CSV,
} out_format_t;

/* Destino de saida compartilhado entre varias URLs (modo batch). */
typedef struct {
    FILE        *fp;
    out_format_t fmt;
    long         count;   /* linhas ja escritas (p/ virgulas do JSON) */
} sink_t;

/* Abre o sink e escreve o cabecalho (CSV) ou abre o array (JSON). */
int  sink_open(sink_t *s, const char *path, out_format_t fmt);
/* Fecha o array (JSON) e o arquivo. */
void sink_close(sink_t *s);

/* Quais tecnicas habilitar (bitmask). */
typedef enum {
    TECH_VERBS       = 1 << 0,  /* verb tampering              */
    TECH_FAKE_IP     = 1 << 1,  /* headers de IP interno       */
    TECH_URL_REWRITE = 1 << 2,  /* X-Original-URL e amigos     */
    TECH_PATH        = 1 << 3,  /* mutacoes de caminho         */
    TECH_CASE        = 1 << 4,  /* troca de caixa no path      */
    TECH_ALL         = 0x1F,
} tech_mask_t;

typedef struct {
    http_config_t http;
    int           threads;
    int           tech_mask;
    int           show_all;    /* imprime tambem resultados "sem graca" */
    const char   *ip_value;    /* sobrescreve o IP spoofado (opcional)  */

    /* wordlists externas (opcionais) */
    char *const  *extra_headers;   /* nomes de header extra p/ spoof    */
    int           n_extra_headers;
    char *const  *extra_paths;     /* mutacoes de path extra (com %s)   */
    int           n_extra_paths;
} engine_opts_t;

/*
 * Roda a varredura completa contra `url`.
 * `sink` pode ser NULL (sem saida estruturada) ou um sink ja aberto, que
 * sera alimentado com as linhas desta URL (permite agregar em modo batch).
 * Retorna 0 se executou (independente de achar bypass), -1 em erro fatal.
 */
int engine_run(const char *url, const engine_opts_t *opts, sink_t *sink);

#endif /* TRESPASS_ENGINE_H */
