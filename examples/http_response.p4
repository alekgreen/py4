import http

def main() -> None:
    response: http.Response = http.get("https://example.com")
    print(response.status)
    print(response.body)
    print(response.headers)
