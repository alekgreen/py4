import http

def main() -> None:
    (status, body): (int, str) = http.patch("http://127.0.0.1:18765/patch-echo", "payload")
    print(status)
    print(body)
