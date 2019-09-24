import io

def main() -> None:
    path: str = "/tmp/py4_with_existing_file.txt"
    file: io.File = io.open(path, "w")

    with file as handle:
        io.write(handle, "x")
        io.flush(handle)

    io.write(file, "y")
    io.close(file)

    with io.open(path, "r") as reader:
        print(io.read(reader))
