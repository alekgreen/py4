import http

def main() -> None:
    result = http.get("http://127.0.0.1:18765/binary")
    print(result)
