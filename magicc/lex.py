"""
Lexer for the magickasm assembly.
"""


from enum import Enum
from typing import cast


class TokenType(Enum):
    """
    Enumaration of every type of valid token.
    """

    Rule = 1
    Empty = 2
    Label = 3
    Name = 4
    Integer = 5
    String = 6
    Register = 7
    Arguments = 8


Token = tuple[TokenType, str, tuple[int, int]]


class LexError(Exception):
    """
    Class representing a lexing error.
    """

    def __init__(self, msg: str, where: tuple[int, int]):
        super().__init__(msg)
        self.msg = msg
        self.where = where  # (start, end)


def tokenize(rule: str) -> list[Token]:
    """
    A very simple lexer state machine to lex a rule.
    """
    rule += " "  # force end with space

    ctx: TokenType | None = None
    buf: list[str] = []
    toks: list[Token] = []  # will never actually be None
    strfl: str = " "  # for string token processing

    tok_start: int = 0

    def pushtok(i: int):
        nonlocal toks, buf, ctx, tok_start
        if ctx is not None:
            toks.append((ctx, "".join(buf), (tok_start, i)))
        buf.clear()
        ctx = None

    err_msg: str | None = None
    for i, c in enumerate(rule):
        # Identify the type of token that is to come.
        if ctx is None:
            # Check for any lex errors after every token.
            if err_msg is not None:
                raise LexError(err_msg, (tok_start, i - 1))
            tok_start = i

            if c == "!":
                ctx = TokenType.Label
            elif c.islower():
                # NOTE since this consumes the first character, it should also be part of the buffer.
                buf.append(c)
                ctx = TokenType.Name
            elif c == "/":
                ctx = TokenType.Empty
            elif c == "#":
                ctx = TokenType.Integer
            elif c == '"':
                ctx = TokenType.String
            elif c == "$":
                ctx = TokenType.Register
            elif c.isspace():
                pass
            else:
                raise LexError("malformed token", (i, i))

        elif ctx == TokenType.Label:
            if c.islower():
                buf.append(c)
            elif c.isspace():
                pushtok(i)
            else:
                err_msg = "malformed label"

        elif ctx == TokenType.Name:
            if c.islower():
                buf.append(c)
            elif c.isspace():
                pushtok(i)
            else:
                err_msg = "malformed mnemonic"

        elif ctx == TokenType.Empty:
            if c.isspace():
                pushtok(i)
            else:
                err_msg = "empty symbol not followed by whitespace"

        elif ctx == TokenType.Integer:
            if c == "-":
                if len(buf) > 0:
                    err_msg = "malformed integer"
                buf.append(c)
            elif c.isdigit():
                buf.append(c)
            elif c.isspace():
                pushtok(i)
            else:
                err_msg = "malformed integer"

        elif ctx == TokenType.String:
            if strfl == "E":
                if c == "x":
                    strfl = "1"  # Want 1st hex digit
                else:
                    strfl = " "
                    esccdict = {
                        "a": 7,
                        "b": 8,
                        "e": 27,
                        "f": 12,
                        "n": 10,
                        "r": 13,
                        "t": 9,
                        "v": 11,
                        "\\": 92,
                        '"': 34,
                    }
                    try:
                        buf.append(chr(esccdict[c]))
                    except KeyError:
                        err_msg = "malformed escape code"
            # (Messily) handle hex codes.
            elif strfl == "1":
                strfl = "2"
                buf.append(c)
            elif strfl == "2":
                strfl = " "
                buf.append(c)
                hexchr = 0
                try:
                    hexchr = int("".join(buf[-2:]), 16)
                except ValueError:
                    err_msg = "malformed hexcode"
                buf.pop()
                buf.pop()
                buf.append(chr(hexchr))
            else:
                if c == "\\":
                    strfl = "E"
                elif c == '"':
                    pushtok(i)
                else:
                    buf.append(c)

        elif ctx == TokenType.Register:
            if c.isdigit():
                buf.append(c)
            elif c.isspace():
                pushtok(i)
            else:
                err_msg = "malformed register"

        else:
            err_msg = "malformed token"

    if ctx is not None:
        raise LexError("incomplete input", (len(rule) - 1, len(rule) - 1))

    return cast(list[Token], [tok for tok in toks if tok[0] is not None])
