import http

def main() -> None:
    headers: dict[str, str] = {"X-Test": "open-sesame", "X-Mode": "inspect"}
    response: http.Response = http.get("http://127.0.0.1:18765/needs-header", headers, 1000)
    print(response.status)
    print(response.body)
