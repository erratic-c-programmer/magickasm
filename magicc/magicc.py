#! /usr/bin/env python

import argparse
from sys import exit

from lex import LexError, tokenize
from parse import ParseError, Rule, parse_to_rule
from codegen import CodegenError, resolve_literals, rules2bytes


class CompileError(Exception):
    """
    Class representing a general compile error.
    """

    def __init__(self, msg: str, src: str, where: tuple[int, int]):
        lineno: int = src[: where[0]].count(chr(10))
        charno: int = len(src[: where[0]].split("\n")[-1])
        super().__init__(
            f"<input>:{lineno}:{charno} error: {msg}\n"
            + f"  {lineno}\t|  {src.split(chr(10))[lineno - 1].strip()}\n"
            + f"\t|  "
            + " " * (charno)
            + "^" * (where[1] - where[0])
            + "\n"
            + f"Compilation failed."
        )


def compile_rules(rules_s_s: str) -> bytes:
    rules_s: list[str] = []
    rules: list[Rule] = []
    rule_char_map_raw: list[int] = []
    rule_char_map: list[int] = []

    c_n = 0
    for r in rules_s_s.split("\n"):
        c_n += len(r) + 1  # including the newline
        rules_s.append(r)
        rule_char_map_raw.append(c_n)

    r_n = 0
    try:
        for i, r in enumerate(rules_s):
            r_parsed = parse_to_rule(tokenize(r + " "))
            if r_parsed:
                rules.append(r_parsed)
                rule_char_map.append(rule_char_map_raw[i])
            r_n += 1
    except LexError as e:
        raise CompileError(
            e.msg,
            rules_s_s,
            (rule_char_map_raw[r_n] + e.where[0], rule_char_map_raw[r_n] + e.where[1]),
        )
    except ParseError as e:
        # If we even get to this stage, that means lexing was successful.
        rule_toks = tokenize(rules_s[r_n])
        raise CompileError(
            e.msg,
            rules_s_s,
            (
                rule_char_map[r_n] + rule_toks[e.where][2][0],
                rule_char_map[r_n] + rule_toks[e.where][2][1],
            ),
        )

    try:
        rules_res, str_lit_tab = resolve_literals(rules)
        return str_lit_tab + rules2bytes(rules_res)
    except CodegenError as e:
        raise CompileError(
            e.msg,
            rules_s_s,
            (
                rule_char_map[e.where[0]]
                + rules[e.where[0]].tok_offsets[e.where[1]][0],
                rule_char_map[e.where[0]]
                + rules[e.where[0]].tok_offsets[e.where[1]][1],
            ),
        )


# And finally, our interface.
if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description="The amazing Magickasm bytecode assembler"
    )
    parser.add_argument(
        "infile", metavar="infile", type=str, help="the assembly file to be processed"
    )
    parser.add_argument(
        "-o",
        "--output",
        dest="outfile_name",
        metavar="FILENAME",
        default="/dev/stdout",
        help="file to save produced bytecode in (defaults to stdout)",
    )
    args = parser.parse_args()

    with open(args.infile, "r") as codefile, open(args.outfile_name, "wb") as outfile:
        try:
            outfile.write(compile_rules(codefile.read()))
        except CompileError as e:
            print(str(e))
            exit(1)
