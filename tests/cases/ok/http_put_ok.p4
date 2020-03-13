import http

def main() -> None:
    (status, body): (int, str) = http.put("http://127.0.0.1:18765/put-echo", "payload")
    print(status)
    print(body)
