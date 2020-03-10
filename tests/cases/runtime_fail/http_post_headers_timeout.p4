import http

def main() -> None:
    headers: dict[str, str] = {"X-Post-Mode": "mirror"}
    result = http.post("http://127.0.0.1:18765/slow-post-needs-header", "payload", headers, 50)
    print(result)
