class Response:
    status: int
    body: str
    headers: list[(str, str)]

native def _request_raw(method: str, url: str) -> (int, str, list[(str, str)])
native def _request_raw(method: str, url: str, timeout_ms: int) -> (int, str, list[(str, str)])
native def _request_raw(method: str, url: str, headers: dict[str, str]) -> (int, str, list[(str, str)])
native def _request_raw(method: str, url: str, headers: dict[str, str], timeout_ms: int) -> (int, str, list[(str, str)])
native def _request_raw(method: str, url: str, body: str) -> (int, str, list[(str, str)])
native def _request_raw(method: str, url: str, body: str, timeout_ms: int) -> (int, str, list[(str, str)])
native def _request_raw(method: str, url: str, body: str, headers: dict[str, str]) -> (int, str, list[(str, str)])
native def _request_raw(method: str, url: str, body: str, headers: dict[str, str], timeout_ms: int) -> (int, str, list[(str, str)])

def _make_response(status: int, body: str, headers: list[(str, str)]) -> Response:
    return Response(status, body, headers)

def request(method: str, url: str) -> Response:
    raw: (int, str, list[(str, str)]) = _request_raw(method, url)
    return _make_response(raw[0], raw[1], raw[2])

def request(method: str, url: str, timeout_ms: int) -> Response:
    raw: (int, str, list[(str, str)]) = _request_raw(method, url, timeout_ms)
    return _make_response(raw[0], raw[1], raw[2])

def request(method: str, url: str, headers: dict[str, str]) -> Response:
    raw: (int, str, list[(str, str)]) = _request_raw(method, url, headers)
    return _make_response(raw[0], raw[1], raw[2])

def request(method: str, url: str, headers: dict[str, str], timeout_ms: int) -> Response:
    raw: (int, str, list[(str, str)]) = _request_raw(method, url, headers, timeout_ms)
    return _make_response(raw[0], raw[1], raw[2])

def request(method: str, url: str, body: str) -> Response:
    raw: (int, str, list[(str, str)]) = _request_raw(method, url, body)
    return _make_response(raw[0], raw[1], raw[2])

def request(method: str, url: str, body: str, timeout_ms: int) -> Response:
    raw: (int, str, list[(str, str)]) = _request_raw(method, url, body, timeout_ms)
    return _make_response(raw[0], raw[1], raw[2])

def request(method: str, url: str, body: str, headers: dict[str, str]) -> Response:
    raw: (int, str, list[(str, str)]) = _request_raw(method, url, body, headers)
    return _make_response(raw[0], raw[1], raw[2])

def request(method: str, url: str, body: str, headers: dict[str, str], timeout_ms: int) -> Response:
    raw: (int, str, list[(str, str)]) = _request_raw(method, url, body, headers, timeout_ms)
    return _make_response(raw[0], raw[1], raw[2])

def get(url: str) -> Response:
    return request("GET", url)

def get(url: str, timeout_ms: int) -> Response:
    return request("GET", url, timeout_ms)

def get(url: str, headers: dict[str, str]) -> Response:
    return request("GET", url, headers)

def get(url: str, headers: dict[str, str], timeout_ms: int) -> Response:
    return request("GET", url, headers, timeout_ms)

def post(url: str, body: str) -> Response:
    return request("POST", url, body)

def post(url: str, body: str, timeout_ms: int) -> Response:
    return request("POST", url, body, timeout_ms)

def post(url: str, body: str, headers: dict[str, str]) -> Response:
    return request("POST", url, body, headers)

def post(url: str, body: str, headers: dict[str, str], timeout_ms: int) -> Response:
    return request("POST", url, body, headers, timeout_ms)

def put(url: str, body: str) -> Response:
    return request("PUT", url, body)

def delete(url: str) -> Response:
    return request("DELETE", url)

def patch(url: str, body: str) -> Response:
    return request("PATCH", url, body)

def get_response_headers(url: str) -> (int, str, list[(str, str)]):
    return _request_raw("GET", url)

def post_response_headers(url: str, body: str) -> (int, str, list[(str, str)]):
    return _request_raw("POST", url, body)
