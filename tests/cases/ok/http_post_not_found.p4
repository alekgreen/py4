import http

def main() -> None:
    (status, body): (int, str) = http.post("http://127.0.0.1:18765/post-missing", "payload")
    print(status)
    print(body)
