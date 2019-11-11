import io
import json


def main() -> None:
    path = "/tmp/py4_native_containers.txt"
    file = io.open(path, "w")
    values: list[json.Value] = [json.parse("1"), json.parse("2")]
    handles: list[io.File] = [file]
    pair: (str, json.Value) = ("count", values[0])

    print(values)
    print(handles)
    print(pair)

    io.close(file)
