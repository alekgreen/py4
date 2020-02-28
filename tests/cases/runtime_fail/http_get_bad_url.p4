import http

def main() -> None:
    result = http.get("://bad")
    print(result)
