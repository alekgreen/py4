import io
import json


class Meta:
    label: str


class User:
    name: str
    tags: list[str]
    meta: Meta | None


def main() -> None:
    path = "json_classes.tmp"
    io.write_text(path, "{\"name\":\"ann\",")
    io.append_text(path, "\"tags\":[\"x\",\"y\"],")
    io.append_text(path, "\"meta\":{\"label\":\"core\"}}")

    user = json.from_string[User](io.read_text(path))

    print(user)
