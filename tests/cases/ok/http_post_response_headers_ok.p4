import http

def main() -> None:
    (status, body, headers): (int, str, list[(str, str)]) = http.post_response_headers("http://127.0.0.1:18765/post-response-headers", "payload")
    print(status)
    print(body)
    print(headers)
