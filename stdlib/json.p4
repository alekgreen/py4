native type Value

native def parse(text: str) -> Value
native def stringify(value: Value) -> str

native def is_null(value: Value) -> bool
native def is_bool(value: Value) -> bool
native def is_number(value: Value) -> bool
native def is_string(value: Value) -> bool
native def is_array(value: Value) -> bool
native def is_object(value: Value) -> bool

native def as_bool(value: Value) -> bool
native def as_float(value: Value) -> float
native def as_string(value: Value) -> str

native def get(value: Value, key: str) -> Value
