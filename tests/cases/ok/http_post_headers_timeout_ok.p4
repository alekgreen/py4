import http

def main() -> None:
    headers: dict[str, str] = {"X-Post-Mode": "mirror"}
    (status, body): (int, str) = http.post("http://127.0.0.1:18765/post-needs-header", "payload", headers, 1000)
    print(status)
    print(body)
