from module_private_class_access_models import _Point

def main() -> None:
    p: _Point = _Point(1, 2)
    print(p.x)
