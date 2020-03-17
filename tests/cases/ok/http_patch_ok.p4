import http

def main() -> None:
    response: http.Response = http.patch("http://127.0.0.1:18765/patch-echo", "payload")
    print(response.status)
    print(response.body)
