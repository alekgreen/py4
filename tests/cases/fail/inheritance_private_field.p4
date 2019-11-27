class Base:
    __secret: int

class Child(Base):
    extra: int

    def leak(self: Child) -> int:
        return self.__secret
