import http

def main() -> None:
    headers: dict[str, str] = {"X-Test": "open-sesame", "X-Mode": "inspect"}
    result = http.get("http://127.0.0.1:18765/needs-header", headers, 0)
    print(result)
