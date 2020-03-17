import http

def main() -> None:
    response: http.Response = http.put("http://127.0.0.1:18765/put-echo", "payload")
    print(response.status)
    print(response.body)
