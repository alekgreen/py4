import http

def main() -> None:
    result = http.get_with_timeout("http://127.0.0.1:18765/ok", 0)
    print(result)
