import http

def main() -> None:
    print(http.get("http://127.0.0.1:18765/ok", "slow"))
