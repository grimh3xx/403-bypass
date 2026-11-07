#ifndef TRESPASS_HTTP_H
#define TRESPASS_HTTP_H

#include <stddef.h>

/* Resultado de uma unica requisicao HTTP. */
typedef struct {
    long   status;        /* codigo HTTP (0 em caso de erro de transporte) */
    size_t body_len;      /* tamanho do corpo recebido em bytes            */
    size_t word_count;    /* numero aproximado de "palavras" no corpo      */
    double elapsed_ms;    /* tempo total da requisicao em ms               */
    char   error[128];    /* mensagem de erro, vazio se ok                 */
} http_result_t;

/* Configuracao global compartilhada por todas as requisicoes. */
typedef struct {
    long   timeout_ms;        /* timeout por requisicao        */
    int    follow_redirects;  /* seguir 3xx                    */
    int    verify_tls;        /* 1 = valida certificado        */
    const char *proxy;        /* ex: http://127.0.0.1:8080     */
    const char *user_agent;   /* User-Agent padrao             */
    long   delay_ms;          /* atraso apos cada req (rate)   */

    int    http_version;      /* 0=auto, 1=HTTP/1.0, 2=1.1, 3=2 */
    int    retries;           /* retries em erro de transporte  */
    char *const *extra_headers;  /* headers aplicados a TODA req */
    int    n_extra_headers;
} http_config_t;

/* Inicializa a camada HTTP global (chamar uma vez no inicio). */
int  http_global_init(void);
void http_global_cleanup(void);

/*
 * Executa uma requisicao.
 *   method  : verbo HTTP (GET, POST, ...)
 *   url     : URL completa
 *   headers : vetor de strings "Chave: valor" (pode ser NULL)
 *   nheaders: quantidade de headers
 *   cfg     : configuracao global
 *   out     : resultado preenchido pela funcao
 * Retorna 0 em sucesso, -1 em erro (out->error e preenchido).
 */
int http_request(const char *method,
                 const char *url,
                 char *const *headers,
                 int nheaders,
                 const http_config_t *cfg,
                 http_result_t *out);

#endif /* TRESPASS_HTTP_H */
