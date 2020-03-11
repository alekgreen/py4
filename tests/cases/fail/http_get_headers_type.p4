import http

def main() -> None:
    headers = ["X-Test"]
    print(http.get("http://127.0.0.1:18765/ok", headers))
