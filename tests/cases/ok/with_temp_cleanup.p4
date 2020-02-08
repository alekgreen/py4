import io


def append_text(path: str, text: str) -> None:
    with io.open(path, "a") as handle:
        io.write(handle, text)
        io.flush(handle)


def main() -> None:
    path: str = "/tmp/py4_with_temp_cleanup.txt"

    with io.open(path, "w") as handle:
        io.write(handle, "a")
        io.flush(handle)

    append_text(path, "b")

    with io.open(path, "r") as reader:
        print(io.read(reader))
