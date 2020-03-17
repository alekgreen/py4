import http

def main() -> None:
    response: http.Response = http.request("GET", "http://127.0.0.1:18765/ok")
    print(response.status)
    print(response.body)
