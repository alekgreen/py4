def first_non_zero(xs: list[char]) -> char:
    for ch in xs:
        if ch != '0':
            return ch
    return '0'

def main() -> None:
    xs: list[char] = []
    xs.append('a')
    xs.append('b')
    print(len(xs))
    print(xs[0])
    xs[1] = 'z'
    print(xs[1])
    ys: list[char] = xs.copy()
    print(ys.pop())
    ys.clear()
    print(len(ys))
    zs: list[char] = ['0', 'k']
    print(first_non_zero(zs))
