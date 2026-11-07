<img src="docs/banner.svg" alt="403-bypass" width="100%">

[![build](https://img.shields.io/github/actions/workflow/status/grimh3xx/403-bypass/build.yml?branch=master&style=flat-square&labelColor=111826&label=build)](https://github.com/grimh3xx/403-bypass/actions)
[![license](https://img.shields.io/github/license/grimh3xx/403-bypass?style=flat-square&labelColor=111826&color=22D3EE)](LICENSE)
![C](https://img.shields.io/badge/C-libcurl%20%2B%20pthreads-3B82F6?style=flat-square&labelColor=111826)
![HTTP](https://img.shields.io/badge/HTTP-403%2F401%20bypass-22D3EE?style=flat-square&labelColor=111826)

**Ferramenta rápida em C para testar bypasses de `403 Forbidden` e `401
Unauthorized`.** Inspirada em `nomore403`, porém reescrita do zero com
**libcurl + pthreads**: binário único, sem runtime, altamente concorrente.

<p align="center">
  <picture>
    <source media="(prefers-color-scheme: light)" srcset="docs/demo-light.svg">
    <img alt="403-bypass encontrando bypasses de verbo e path" src="docs/demo.svg" width="100%">
  </picture>
</p>

> ⚠️ **Uso autorizado apenas.** Rode somente contra sistemas que você tem
> permissão explícita para testar (pentest, bug bounty, CTF). O uso sem
> autorização é ilegal — a responsabilidade é inteiramente sua.

## Features

| | |
|---|---|
| **Verb tampering** | ~18 métodos HTTP: `GET`, `POST`, `PUT`, `PATCH`, `TRACE`, `PROPFIND`, `MKCOL`, `DEBUG`... |
| **Headers de origem** | `X-Forwarded-For`, `X-Real-IP`, `X-Custom-IP-Authorization`, `CF-Connecting-IP` e outros, com `127.0.0.1`, `localhost`, `::1`... |
| **Reescrita de URL** | `X-Original-URL`, `X-Rewrite-URL`, `Referer`... apontando para o caminho protegido |
| **Mutações de path** | `/path/`, `/path/..;/`, `/%2e/path`, troca de caixa, e **Unicode/double-encoding** (`%252f`, `%c0%af`, `%ef%bc%8f`) |
| **Baseline inteligente** | compara status + tamanho + nº de palavras com a requisição original e marca `<== BYPASS?` o que escapa do 4xx |
| **Concorrência** | pool de threads POSIX — binário único, sem dependências de runtime |
| **Batch & wordlists** | `-f urls.txt` (ou stdin), `--headers` e `--paths` para listas próprias |
| **Saída** | `JSON` ou `CSV` (`-o`) para integrar com outras ferramentas |
| **Rede** | proxy (Burp/ZAP), rate limit, timeout, TLS inseguro e seleção de técnicas |

## Quick start

Requer um compilador C11, `make` e `libcurl` com headers de dev.

```bash
# Debian/Ubuntu/Kali
sudo apt install build-essential libcurl4-openssl-dev
git clone https://github.com/grimh3xx/403-bypass && cd 403-bypass

make
./403-bypass -u https://alvo.tld/admin      # varredura completa
sudo make install                           # opcional: /usr/local/bin
```

## Uso

```text
uso: 403-bypass (-u <url> | -f <arquivo>) [opcoes]

  -u, --url <url>        URL alvo
  -f, --file <arquivo>   arquivo com uma URL por linha (batch; '-' = stdin)
  -t, --threads <n>      threads concorrentes (padrao 20)
  -T, --timeout <ms>     timeout por requisicao (padrao 7000)
      --delay <ms>       atraso apos cada requisicao (rate limit)
  -x, --proxy <url>      proxy (ex: http://127.0.0.1:8080)
  -A, --user-agent <ua>  User-Agent customizado
      --ip <valor>       IP/host unico para os headers de spoof
      --headers <arq>    wordlist de nomes de header extra
      --paths <arq>      wordlist de mutacoes de path ('%s' = path alvo)
  -o, --output <arq>     salva resultados em arquivo ('-' = stdout)
      --format <fmt>     json | csv (inferido da extensao de -o)
  -k, --insecure         nao validar certificado TLS
  -L, --location         seguir redirecionamentos
  -a, --all              mostrar todos os resultados (nao so os hits)
      --only <lista>     tecnicas: verb,ip,rewrite,path,case
```

### Exemplos

```bash
# Só verb tampering e headers de IP, passando pelo Burp
403-bypass -u https://alvo.tld/admin --only verb,ip -x http://127.0.0.1:8080

# Batch: várias URLs de um arquivo, salvando em JSON
403-bypass -f urls.txt -o resultado.json

# Wordlists próprias de headers e paths, saída CSV
403-bypass -u https://alvo.tld/x --headers hdrs.txt --paths paths.txt -o r.csv
```

O arquivo de `--paths` usa `%s` como marcador do caminho alvo, um por linha
(`%s/`, `%s%2e%2e%2f`, `%s/admin`). A saída CSV/JSON traz: `target, technique,
method, url, headers, status, length, words, ms, hit`.

### Lendo a saída

```text
cod  tecnica     metodo    tamanho  palavras  payload
200  verb        OPTIONS       0B     0w  metodo OPTIONS      <== BYPASS?
403  header-ip   GET           0B     0w  X-Forwarded-For: 127.0.0.1
```

- **cod** — status HTTP (verde = 2xx, ciano = 3xx, vermelho = 4xx).
- **tamanho/palavras** — do corpo da resposta, comparados com a baseline.
- **`<== BYPASS?`** — candidato promissor; **sempre valide manualmente**.

## Arte

O banner e o demo são gerados pela toolchain de marca do perfil:

```bash
python3 tools/gen_banner.py    # docs/banner.svg + social-preview.svg
python3 tools/gen_demo.py      # docs/demo.svg + docs/demo-light.svg
```

## Licença

MIT. Veja [LICENSE](LICENSE).
