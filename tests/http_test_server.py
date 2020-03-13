#!/usr/bin/env python3

from __future__ import annotations

import http.server
import socketserver
import sys
import time


class Handler(http.server.BaseHTTPRequestHandler):
    server_version = "py4-test"
    sys_version = ""

    def write_body(self, body: bytes) -> None:
        try:
            self.wfile.write(body)
        except (BrokenPipeError, ConnectionResetError):
            return

    def date_time_string(self, timestamp: float | None = None) -> str:
        return "Mon, 01 Jan 2001 00:00:00 GMT"

    def do_GET(self) -> None:  # noqa: N802
        if self.path == "/slow-needs-header":
            token = self.headers.get("X-Test", "")
            mode = self.headers.get("X-Mode", "")
            time.sleep(0.2)

            if token == "open-sesame" and mode == "inspect":
                body = b"slow header ok: open-sesame inspect"
                self.send_response(200)
            else:
                body = b"missing required headers"
                self.send_response(403)
            self.send_header("Content-Type", "text/plain; charset=utf-8")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.write_body(body)
            return

        if self.path == "/response-headers":
            body = b"headers route"
            self.send_response(200)
            self.send_header("X-Multi", "one")
            self.send_header("X-Multi", "two")
            self.send_header("X-Final", "yes")
            self.send_header("Content-Type", "text/plain; charset=utf-8")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.write_body(body)
            return

        if self.path == "/redirect-headers":
            self.send_response(302)
            self.send_header("Location", "/final-headers")
            self.send_header("X-Redirect", "discard-me")
            self.end_headers()
            return

        if self.path == "/final-headers":
            body = b"redirect final"
            self.send_response(200)
            self.send_header("X-Final", "final-one")
            self.send_header("X-Final", "final-two")
            self.send_header("Content-Type", "text/plain; charset=utf-8")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.write_body(body)
            return

        if self.path == "/needs-header":
            token = self.headers.get("X-Test", "")
            mode = self.headers.get("X-Mode", "")

            if token == "open-sesame" and mode == "inspect":
                body = b"header ok: open-sesame inspect"
                self.send_response(200)
            else:
                body = b"missing required headers"
                self.send_response(403)
            self.send_header("Content-Type", "text/plain; charset=utf-8")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.write_body(body)
            return

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

        if self.path == "/slow-post-needs-header":
            mode = self.headers.get("X-Post-Mode", "")
            time.sleep(0.2)

            if mode == "mirror":
                response = b"slow post header ok: " + body
                self.send_response(200)
            else:
                response = b"missing post header"
                self.send_response(403)
            self.send_header("Content-Type", "text/plain; charset=utf-8")
            self.send_header("Content-Length", str(len(response)))
            self.end_headers()
            self.write_body(response)
            return

        if self.path == "/post-response-headers":
            response = b"post headers: " + body
            self.send_response(200)
            self.send_header("X-Reply", "alpha")
            self.send_header("X-Reply", "beta")
            self.send_header("Content-Type", "text/plain; charset=utf-8")
            self.send_header("Content-Length", str(len(response)))
            self.end_headers()
            self.write_body(response)
            return

        if self.path == "/post-needs-header":
            mode = self.headers.get("X-Post-Mode", "")

            if mode == "mirror":
                response = b"post header ok: " + body
                self.send_response(200)
            else:
                response = b"missing post header"
                self.send_response(403)
            self.send_header("Content-Type", "text/plain; charset=utf-8")
            self.send_header("Content-Length", str(len(response)))
            self.end_headers()
            self.write_body(response)
            return

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

    def do_PUT(self) -> None:  # noqa: N802
        length_header = self.headers.get("Content-Length", "0")
        body = self.rfile.read(int(length_header))

        if self.path == "/put-echo":
            response = b"put: " + body
            self.send_response(200)
            self.send_header("Content-Type", "text/plain; charset=utf-8")
            self.send_header("Content-Length", str(len(response)))
            self.end_headers()
            self.write_body(response)
            return

        response = b"unknown put route"
        self.send_response(404)
        self.send_header("Content-Type", "text/plain; charset=utf-8")
        self.send_header("Content-Length", str(len(response)))
        self.end_headers()
        self.write_body(response)

    def do_DELETE(self) -> None:  # noqa: N802
        if self.path == "/delete-ok":
            response = b"delete ok"
            self.send_response(200)
            self.send_header("Content-Type", "text/plain; charset=utf-8")
            self.send_header("Content-Length", str(len(response)))
            self.end_headers()
            self.write_body(response)
            return

        response = b"unknown delete route"
        self.send_response(404)
        self.send_header("Content-Type", "text/plain; charset=utf-8")
        self.send_header("Content-Length", str(len(response)))
        self.end_headers()
        self.write_body(response)

    def do_PATCH(self) -> None:  # noqa: N802
        length_header = self.headers.get("Content-Length", "0")
        body = self.rfile.read(int(length_header))

        if self.path == "/patch-echo":
            response = b"patch: " + body
            self.send_response(200)
            self.send_header("Content-Type", "text/plain; charset=utf-8")
            self.send_header("Content-Length", str(len(response)))
            self.end_headers()
            self.write_body(response)
            return

        response = b"unknown patch route"
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
