#!/usr/bin/env python3

from __future__ import annotations

import http.server
import socketserver
import sys
import time


class Handler(http.server.BaseHTTPRequestHandler):
    def write_body(self, body: bytes) -> None:
        try:
            self.wfile.write(body)
        except (BrokenPipeError, ConnectionResetError):
            return

    def do_GET(self) -> None:  # noqa: N802
        if self.path == "/ok":
            body = b"ok from server"
            self.send_response(200)
            self.send_header("Content-Type", "text/plain; charset=utf-8")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.write_body(body)
            return

        if self.path == "/missing":
            body = b"missing route"
            self.send_response(404)
            self.send_header("Content-Type", "text/plain; charset=utf-8")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.write_body(body)
            return

        if self.path == "/redirect":
            self.send_response(302)
            self.send_header("Location", "/ok")
            self.end_headers()
            return

        if self.path == "/binary":
            body = b"ab\x00cd"
            self.send_response(200)
            self.send_header("Content-Type", "application/octet-stream")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.write_body(body)
            return

        if self.path == "/slow":
            body = b"slow route"
            time.sleep(0.2)
            self.send_response(200)
            self.send_header("Content-Type", "text/plain; charset=utf-8")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.write_body(body)
            return

        body = b"unknown route"
        self.send_response(404)
        self.send_header("Content-Type", "text/plain; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.write_body(body)

    def log_message(self, format: str, *args: object) -> None:
        return

    def do_POST(self) -> None:  # noqa: N802
        length_header = self.headers.get("Content-Length", "0")
        body = self.rfile.read(int(length_header))

        if self.path == "/echo":
            response = b"echo: " + body
            self.send_response(200)
            self.send_header("Content-Type", "text/plain; charset=utf-8")
            self.send_header("Content-Length", str(len(response)))
            self.end_headers()
            self.write_body(response)
            return

        if self.path == "/post-missing":
            response = b"missing post route: " + body
            self.send_response(404)
            self.send_header("Content-Type", "text/plain; charset=utf-8")
            self.send_header("Content-Length", str(len(response)))
            self.end_headers()
            self.write_body(response)
            return

        if self.path == "/slow-echo":
            response = b"slow echo: " + body
            time.sleep(0.2)
            self.send_response(200)
            self.send_header("Content-Type", "text/plain; charset=utf-8")
            self.send_header("Content-Length", str(len(response)))
            self.end_headers()
            self.write_body(response)
            return

        response = b"unknown post route"
        self.send_response(404)
        self.send_header("Content-Type", "text/plain; charset=utf-8")
        self.send_header("Content-Length", str(len(response)))
        self.end_headers()
        self.write_body(response)


class ReusableTCPServer(socketserver.TCPServer):
    allow_reuse_address = True


def main() -> int:
    if len(sys.argv) != 2:
        print("usage: http_test_server.py <port>", file=sys.stderr)
        return 1

    port = int(sys.argv[1])
    with ReusableTCPServer(("127.0.0.1", port), Handler) as server:
        print("READY", flush=True)
        server.serve_forever()

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
