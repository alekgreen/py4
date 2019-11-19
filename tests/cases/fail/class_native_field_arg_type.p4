import io
import json


class Record:
    name: str
    payload: json.Value
    file: io.File


def main() -> None:
    path = "/tmp/py4_class_native_field_arg_type.txt"
    file = io.open(path, "w")
    record = Record("demo", "{\"ok\":true}", file)
    print(record)
