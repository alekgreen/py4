import http

def main() -> None:
    headers: dict[str, str] = {"X-Post-Mode": "mirror"}
    response: http.Response = http.post("http://127.0.0.1:18765/post-needs-header", "payload", headers, 1000)
    print(response.status)
    print(response.body)
