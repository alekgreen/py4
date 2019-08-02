import pkgdemo
import pkgdemo.math.ops
import pkgdemo.math.ops as ops
from pkgdemo.math.more import shift

def main() -> None:
    print(pkgdemo.package_seed)
    print(pkgdemo.math.ops.square(5))
    print(ops.square(4))
    print(shift(10))
