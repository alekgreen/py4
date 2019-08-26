import io

def main() -> None:
    path: str = "/tmp/py4_native_io_example.txt"
    io.write_text(path, "hello")
    io.append_text(path, " world")
    print(io.exists(path))
    print(io.read_text(path))
