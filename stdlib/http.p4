native def get(url: str) -> (int, str)
native def post(url: str, body: str) -> (int, str)
native def get_with_timeout(url: str, timeout_ms: int) -> (int, str)
native def post_with_timeout(url: str, body: str, timeout_ms: int) -> (int, str)
