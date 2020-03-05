import http

def main() -> None:
    result = http.get("http://127.0.0.1:18765/slow", 50)
    print(result)
