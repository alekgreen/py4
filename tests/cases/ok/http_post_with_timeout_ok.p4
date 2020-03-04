import http

def main() -> None:
    (status, body): (int, str) = http.post_with_timeout("http://127.0.0.1:18765/echo", "payload", 1000)
    print(status)
    print(body)
