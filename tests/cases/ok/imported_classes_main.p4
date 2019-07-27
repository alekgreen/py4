import imported_classes_models

def main() -> None:
    a: imported_classes_models.Point = imported_classes_models.Point(2, 3)
    print(a)
    print(a.sum())
