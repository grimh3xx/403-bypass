#!/usr/bin/env python3
"""Teste end-to-end do 403-bypass contra um mock server.

Sobe um servidor HTTP que devolve 403 em /secret, mas 200 quando:
  - o metodo e OPTIONS,
  - vem o header X-Forwarded-For: 127.0.0.1,
  - o path tem barra final (/secret/).

Roda o binario compilado e verifica que ele acha pelo menos um bypass (200).
Uso: python3 tests/test_cli.py [caminho-do-binario]
"""
import json
import subprocess
import sys
import tempfile
import threading
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path

BIN = sys.argv[1] if len(sys.argv) > 1 else str(Path(__file__).resolve().parent.parent / "403-bypass")


class Handler(BaseHTTPRequestHandler):
    def _reply(self):
        path = self.path
        xff = self.headers.get("X-Forwarded-For", "")
        if path == "/secret":
            granted = self.command == "OPTIONS" or xff == "127.0.0.1"
            code, body = (200, b"granted") if granted else (403, b"forbidden")
        elif path.startswith("/secret/") or path.startswith("/secret%2f"):
            code, body = 200, b"granted"
        else:
            code, body = 404, b"not found: this page does not exist here"
        self.send_response(code)
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        if self.command != "HEAD":
            self.wfile.write(body)

    def __getattr__(self, name):
        if name.startswith("do_"):
            return self._reply
        raise AttributeError(name)

    def log_message(self, *a):
        pass


def main():
    server = ThreadingHTTPServer(("127.0.0.1", 0), Handler)
    port = server.server_address[1]
    threading.Thread(target=server.serve_forever, daemon=True).start()
    url = f"http://127.0.0.1:{port}/secret"

    failures = []

    # 1) --version
    out = subprocess.run([BIN, "--version"], capture_output=True, text=True)
    if "403-bypass" not in out.stdout:
        failures.append(f"--version inesperado: {out.stdout!r}")

    # 2) varredura: deve achar bypass (200) via verb/ip/path
    with tempfile.NamedTemporaryFile("r", suffix=".json", delete=False) as tf:
        jpath = tf.name
    subprocess.run([BIN, "-u", url, "--only", "verb,ip,path",
                    "-s", "--no-curl", "-o", jpath, "-t", "10"],
                   capture_output=True, text=True, timeout=60)
    rows = json.load(open(jpath))
    hits = [r for r in rows if r["hit"]]
    got200 = [r for r in hits if r["status"] == 200]
    if not got200:
        failures.append(f"nenhum bypass 200 encontrado (hits={len(hits)})")
    techniques = {r["technique"] for r in got200}
    for want in ("verb", "header-ip", "path"):
        if want not in techniques:
            failures.append(f"tecnica '{want}' nao achou bypass (achou: {techniques})")

    # 3) match-code: -mc 200 so deve marcar os 200
    subprocess.run([BIN, "-u", url, "--only", "verb,ip,path", "-mc", "200",
                    "-s", "--no-curl", "-o", jpath], capture_output=True,
                   text=True, timeout=60)
    rows = json.load(open(jpath))
    bad = [r for r in rows if r["hit"] and r["status"] != 200]
    if bad:
        failures.append(f"-mc 200 marcou status != 200: {[r['status'] for r in bad]}")

    server.shutdown()
    if failures:
        print("FALHOU:")
        for f in failures:
            print("  -", f)
        sys.exit(1)
    print(f"OK: bypass detectado via {sorted(techniques)}; match-code respeitado")


if __name__ == "__main__":
    main()
