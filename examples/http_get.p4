import http

def main() -> None:
    (status, body): (int, str) = http.get("https://example.com")
    print(status)
    print(len(body))
