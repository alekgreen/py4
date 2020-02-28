import http

def main() -> None:
    (status, body): (int, str) = http.get("http://127.0.0.1:18765/missing")
    print(status)
    print(body)
