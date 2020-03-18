import http

def main() -> None:
    headers: dict[str, str] = {
        "Accept": "text/html",
        "User-Agent": "py4-example"
    }
    response: http.Response = http.request("GET", "https://example.com", headers, 5000)
    print(response.status)
    print(len(response.body))
    print(response.headers)
