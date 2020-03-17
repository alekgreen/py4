import http

def main() -> None:
    response: http.Response = http.post("http://127.0.0.1:18765/echo", "payload", 1000)
    print(response.status)
    print(response.body)
