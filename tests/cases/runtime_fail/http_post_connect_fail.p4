import http

def main() -> None:
    result = http.post("http://127.0.0.1:18766/offline", "payload")
    print(result)
