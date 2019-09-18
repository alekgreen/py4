def main() -> None:
    nums: list[int] = [1, 2, 3]
    assert len(nums) == 3
    assert nums[1] == 2, "middle element changed"
    print("ok")
