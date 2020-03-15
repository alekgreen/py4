import http

def main() -> None:
    (status, body, headers): (int, str, list[(str, str)]) = http.get_response_headers("https://example.com")
    response: http.Response = http.Response(status, body, headers)
    print(response.status)
    print(response.body)
    print(response.headers)
