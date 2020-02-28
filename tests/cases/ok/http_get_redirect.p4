import http

def main() -> None:
    (status, body): (int, str) = http.get("http://127.0.0.1:18765/redirect")
    print(status)
    print(body)
