import io
import json


class Record:
    name: str
    payload: json.Value
    file: io.File


def main() -> None:
    path = "/tmp/py4_class_native_fields.txt"
    file = io.open(path, "w")
    record = Record("demo", json.parse("{\"ok\":true}"), file)

    print(record)
    io.write(record.file, "hello")
    print(json.stringify(record.payload))

    io.close(file)
