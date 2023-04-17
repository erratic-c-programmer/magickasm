"""
Parser for the magickasm assembly.
"""

from lex import TokenType, Token
from typing import Any


class Rule:
    def __init__(
        self,
        instruction: str,
        label: str | None,
        args: list[tuple[TokenType, Any, tuple[int, int]]],
        tok_offsets: list[tuple[int, int]],
    ):
        self.instruction = instruction
        self.label = label
        self.args = args[:]
        self.tok_offsets = tok_offsets

    def __copy__(self):
        return type(self)(
            self.instruction, self.label, self.args[:], self.tok_offsets[:]
        )

    def __repr__(self):
        return f'{ { "label": self.label, "instruction": self.instruction, "args": self.args } }'


class ParseError(Exception):
    """
    Class representing a parsing error.
    """

    def __init__(self, msg: str, where: int):
        super().__init__(msg)
        self.msg = msg
        self.where = where  # which token?


def parse_to_rule(toks: list[Token]) -> Rule | None:
    """
    Parses a lexed rule to a Rule data type.
    """
    if len(toks) == 0:
        return  # probably just a comment

    labelled = 0
    label = None
    if toks[0][0] == TokenType.Label:
        label = toks[0][1]
        labelled = 1

    if labelled and len(toks) == 1:
        toks.append((TokenType.Name, "nop", (0, 0)))

    if len(toks) - labelled < 1 or toks[labelled][0] != TokenType.Name:
        raise ParseError("missing mnemonic", labelled)

    return Rule(
        toks[labelled][1],
        label,
        toks[labelled + 1 :],
        ([toks[0][2]] if labelled else [])
        + [toks[labelled][2]]
        + [argtok[2] for argtok in toks[labelled + 1 :]],
    )
