native def get(url: str) -> (int, str)
native def get(url: str, timeout_ms: int) -> (int, str)
native def post(url: str, body: str) -> (int, str)
native def post(url: str, body: str, timeout_ms: int) -> (int, str)
