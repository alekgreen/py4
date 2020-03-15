import http

def main() -> None:
    (status, body, headers): (int, str, list[(str, str)]) = http.get_response_headers("http://127.0.0.1:18765/response-headers")
    response: http.Response = http.Response(status, body, headers)
    print(response.status)
    print(response.body)
    print(response.headers)
