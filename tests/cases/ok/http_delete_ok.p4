import http

def main() -> None:
    (status, body): (int, str) = http.delete("http://127.0.0.1:18765/delete-ok")
    print(status)
    print(body)
