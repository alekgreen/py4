import io


def main() -> None:
    f: io.File = io.open("/tmp/py4_native_file_print.txt", "w")
    print(f)
    io.close(f)
