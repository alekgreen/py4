import http

def main() -> None:
    (status, body): (int, str) = http.get("http://127.0.0.1:18765/ok", 1000)
    print(status)
    print(body)
