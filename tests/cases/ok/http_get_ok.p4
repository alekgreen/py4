import http

def main() -> None:
    response: http.Response = http.get("http://127.0.0.1:18765/ok")
    print(response.status)
    print(response.body)
