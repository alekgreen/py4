import http

def main() -> None:
    headers: dict[str, int] = {"X-Test": 1}
    print(http.get("http://127.0.0.1:18765/needs-header", headers))
