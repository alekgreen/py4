import http

def main() -> None:
    response: http.Response = http.delete("http://127.0.0.1:18765/delete-ok")
    print(response.status)
    print(response.body)
