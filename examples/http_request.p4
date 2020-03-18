import http

def main() -> None:
    response: http.Response = http.request("GET", "https://example.com")
    print(response.status)
    print(len(response.body))
    print(response.headers)
