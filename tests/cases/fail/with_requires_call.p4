import io

def main() -> None:
    path: str = "/tmp/py4_with_requires_call.txt"
    file: io.File = io.open(path, "w")
    with file as handle:
        io.write(handle, "x")
