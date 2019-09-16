native type File

native def open(path: str, mode: str) -> File
native def write(file: File, data: str) -> None
native def read(file: File) -> str
native def read_line(file: File) -> str
native def flush(file: File) -> None
native def close(file: File) -> None

native def read_text(path: str) -> str
native def write_text(path: str, data: str) -> None
native def append_text(path: str, data: str) -> None
native def exists(path: str) -> bool
