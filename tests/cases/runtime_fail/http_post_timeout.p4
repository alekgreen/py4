import http

def main() -> None:
    result = http.post_with_timeout("http://127.0.0.1:18765/slow-echo", "payload", 50)
    print(result)
