from functools import reduce
from operator import add
from typing import cast


def lines(l: list[str]) -> str:
    return cast(str, reduce(add, map(lambda x: x + "\n", l)))


def indent(x: str, ch: str = "  "):
    return ch + x
