#ifndef TRESPASS_PAYLOADS_H
#define TRESPASS_PAYLOADS_H

#include <stddef.h>

/* Verbos HTTP testados na tecnica de "verb tampering". */
extern const char *const PL_METHODS[];
extern const size_t       PL_METHODS_N;

/*
 * Headers que simulam origem interna / confianca.
 * Cada entrada e "Nome-Do-Header", o valor e aplicado em tempo de execucao
 * (geralmente 127.0.0.1 ou localhost) pelo engine.
 */
extern const char *const PL_FAKE_IP_HEADERS[];
extern const size_t       PL_FAKE_IP_HEADERS_N;

/* Valores de IP/host usados junto dos headers acima. */
extern const char *const PL_FAKE_IP_VALUES[];
extern const size_t       PL_FAKE_IP_VALUES_N;

/*
 * Headers de reescrita de URL: recebem o PATH original como valor
 * (ex: "X-Original-URL: /admin"). O engine os combina com um request
 * para a raiz "/".
 */
extern const char *const PL_URL_REWRITE_HEADERS[];
extern const size_t       PL_URL_REWRITE_HEADERS_N;

/*
 * Mutacoes de caminho. "%s" e substituido pelo path alvo.
 * Ex: "%s/" , "/%%2e%s" , "%s..;/" ...
 * (use %% quando quiser um literal '%').
 */
extern const char *const PL_PATH_MUTATIONS[];
extern const size_t       PL_PATH_MUTATIONS_N;

/* Sufixos simples anexados ao final do path (ex: "%00", ".json"). */
extern const char *const PL_PATH_SUFFIXES[];
extern const size_t       PL_PATH_SUFFIXES_N;

#endif /* TRESPASS_PAYLOADS_H */
