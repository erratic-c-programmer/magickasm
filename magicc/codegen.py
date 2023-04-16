"""
Bytecode codegen for the magickasm assembly.
"""

from copy import copy

from lex import TokenType
from parse import Rule


class CodegenError(Exception):
    """
    Class representing a codegen error.
    """

    def __init__(self, msg: str, where: tuple[int, int]):
        super().__init__(msg)
        self.msg = msg
        self.where = where  # (rule, token)


"""
Functions relating to label/literal handling.
"""


def resolve_labels(rules: list[Rule]) -> list[Rule]:
    """
    Removes all labels from a set of rules and
    replace them with absolute indices.
    """

    labels: dict = {}
    new_rules: list[Rule] = []
    for i, r in enumerate(rules):
        labels[r.label] = i + 1
        new_rules.append(copy(r))

    for i, r in enumerate(new_rules):
        for j, arg in enumerate(r.args):
            argtok: int = (r.label is not None) + 1 + j
            if arg[0] == TokenType.Label:
                try:
                    new_rules[i].args[j] = (
                        TokenType.Integer,
                        labels[arg[1]],
                        (i, argtok),
                    )
                except KeyError:
                    raise CodegenError("nonexistent label", (i, argtok))

    return new_rules


def resolve_strlit(rules: list[Rule]) -> tuple[list[Rule], bytes]:
    """
    Resolves strings to string table indices, generating the
    string literal table as a side effect. Also handles emptys
    (oops) by just making the strings with index 0.
    """

    strlit_tab: list[bytes] = []
    new_rules: list[Rule] = []
    for i, r in enumerate(rules):
        new_rules.append(copy(r))
        for j, arg in enumerate(r.args):
            argtok: int = (r.label is not None) + 1 + j
            if arg[0] == TokenType.String:
                strlit_tab.append(len(arg[1]).to_bytes(8, "little") + arg[1].encode())
                strlit_tab[-1] += b"\0" * (8 - len(strlit_tab[-1]) % 8) + (-1).to_bytes(
                    8, "little", signed=True
                )
                new_rules[-1].args[j] = (TokenType.String, len(strlit_tab), (i, argtok))
            elif arg[0] == TokenType.Empty:
                new_rules[-1].args[j] = (TokenType.String, 0, (i, argtok))

    return (
        new_rules,
        (len(strlit_tab) + 1).to_bytes(8, byteorder="little")  # length
        + (
            (0).to_bytes(8, "little")
            + (0).to_bytes(8, "little")
            + (-1).to_bytes(8, "little", signed=True)
        )  # empty
        + b"".join(strlit_tab),
    )


def resolve_intlit(rules: list[Rule]) -> list[Rule]:
    """
    Resolves integer literals.
    """

    new_rules: list[Rule] = []
    for i, r in enumerate(rules):
        new_rules.append(copy(r))
        for j, arg in enumerate(r.args):
            argtok: int = (r.label is not None) + 1 + j
            if arg[0] == TokenType.Integer:
                new_rules[-1].args[j] = (TokenType.Integer, int(arg[1]), (i, argtok))

    return new_rules


def resolve_reglit(rules: list[Rule]) -> list[Rule]:
    """
    Resolves register literals.
    """

    new_rules: list[Rule] = []
    for i, r in enumerate(rules):
        new_rules.append(copy(r))
        for j, arg in enumerate(r.args):
            argtok: int = (r.label is not None) + 1 + j
            if arg[0] == TokenType.Register:
                new_rules[-1].args[j] = (TokenType.Register, int(arg[1]), (i, argtok))

    return new_rules


def resolve_literals(rules: list[Rule]) -> tuple[list[Rule], bytes]:
    """
    Resolves all literals.
    second return value is string literal table.
    """

    strres, strlittab = resolve_strlit(rules)
    return (
        resolve_intlit(resolve_reglit(resolve_labels(strres))),
        strlittab,
    )


"""
Compiling to bytecode!
"""

# XXX DO NOT CHANGE
InstructionCodes = {
    name: code.to_bytes(6, "little")
    for code, name in enumerate(
        [
            "put",
            "cls",
            "st",
            "mst",
            "ld",
            "mld",
            "swp",
            "add",
            "sub",
            "neg",
            "mul",
            "div",
            "mod",
            "len",
            "ins",
            "cat",
            "sbs",
            "cmp",
            "casi",
            "iasc",
            "stoi",
            "itos",
            "jmp",
            "jb",
            "je",
            "ja",
            "nop",
            "type",
            "run",
        ]
    )
}


def rules2bytes(rules: list[Rule]) -> bytes:
    """
    Compiles a list of tokenised, resolved rules into bytecode.
    """

    code: list[bytes] = []

    for idx, r in enumerate(rules):
        args: list[bytes] = []

        if r.instruction not in InstructionCodes:
            raise CodegenError("nonexistent mnemonic", (idx, r.label is not None))

        # Handle arguments.
        argregfl: int = 0
        argtypfl: int = 0
        for j, arg in enumerate(r.args):
            if arg[0] == TokenType.Integer:
                args.append(arg[1].to_bytes(8, "little", signed=True))
            else:
                args.append(arg[1].to_bytes(8, "little"))
                if arg[0] == TokenType.Register:
                    argregfl |= 1 << j
                elif arg[0] == TokenType.String:
                    argtypfl |= 1 << j

        code.append(
            InstructionCodes[r.instruction]
            + ((argregfl << 4) | argtypfl).to_bytes(1, "little")
            + len(r.args).to_bytes(1, "little")
            + b"".join(args)
        )

    return len(code).to_bytes(8, "little") + b"".join(code)
