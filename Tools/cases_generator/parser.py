"""Parser for bytecodes.inst."""

from dataclasses import dataclass

import lexer as lx


class Lexer:
    def __init__(self, src, filename=None):
        self.src = src
        self.filename = filename
        self.tokens = list(lx.tokenize(self.src, filename=filename))
        self.pos = 0

    def getpos(self):
        return self.pos

    def eof(self):
        return self.pos >= len(self.tokens)

    def reset(self, pos):
        assert 0 <= pos <= len(self.tokens), (pos, len(self.tokens))
        self.pos = pos

    def backup(self):
        assert self.pos > 0
        self.pos -= 1

    def next(self, raw=False):
        while self.pos < len(self.tokens):
            tok = self.tokens[self.pos]
            self.pos += 1
            if raw or tok.kind != "COMMENT":
                return tok
        return None

    def peek(self, raw=False):
        tok = self.next(raw=raw)
        self.backup()
        return tok

    def expect(self, kind):
        tkn = self.next()
        if tkn is not None and tkn.kind == kind:
            return tkn
        self.backup()
        return None

    def extract_line(self, line):
        lines = self.src.splitlines()
        if line > len(lines):
            return ""
        return lines[line - 1]

    def make_syntax_error(self, message, tkn=None):
        if tkn is None:
            tkn = self.peek()
        if tkn is None:
            tkn = self.tokens[-1]
        return lx.make_syntax_error(message,
            self.filename, tkn.line, tkn.column, self.extract_line(tkn.line))


@dataclass
class InstDef:
    name: str
    inputs: list[str]
    outputs: list[str]
    blob: list[lx.Token]


@dataclass
class Family:
    name: str
    members: list[str]


@dataclass
class Block:
    tokens: list[lx.Token]


class Parser(Lexer):
    # TODO: Make all functions reset the input pointer
    # when they return None, and raise when they know
    # that something is wrong.

    def inst_def(self):
        if header := self.inst_header():
            if block := self.block():
                header.blob = block.tokens
                return header
            raise self.make_syntax_error("Expected block")
        return None

    def block(self):
        if self.expect(lx.LBRACE):
            tokens = self.c_blob()
            if self.expect(lx.RBRACE):
                return Block(tokens)
            raise self.make_syntax_error("Expected '}'")
        return None

    def c_blob(self):
        tokens = []
        level = 0
        while True:
            tkn = self.next(raw=True)
            if tkn is None:
                break
            if tkn.kind in (lx.LBRACE, lx.LPAREN, lx.LBRACKET):
                level += 1
            elif tkn.kind in (lx.RBRACE, lx.RPAREN, lx.RBRACKET):
                level -= 1
                if level < 0:
                    self.backup()
                    break
            tokens.append(tkn)
        return tokens

    def inst_header(self):
        # inst(NAME, (inputs -- outputs))
        # TODO: Error out when there is something unexpected.
        here = self.getpos()
        # TODO: Make INST a keyword in the lexer.
        if (tkn := self.expect(lx.IDENTIFIER)) and tkn.text == "inst":
            if (self.expect(lx.LPAREN)
                    and (tkn := self.expect(lx.IDENTIFIER))):
                name = tkn.text
                if self.expect(lx.COMMA):
                    inp, outp = self.stack_effect()
                    if (self.expect(lx.RPAREN)
                            and self.peek().kind == lx.LBRACE):
                        return InstDef(name, inp, outp, [])
        self.reset(here)
        return None

    def stack_effect(self):
        # '(' [inputs] '--' [outputs] ')'
        if self.expect(lx.LPAREN):
            inp = self.inputs() or []
            if self.expect(lx.MINUSMINUS):
                outp = self.outputs() or []
                if self.expect(lx.RPAREN):
                    return inp, outp
        raise self.make_syntax_error("Expected stack effect")

    def inputs(self):
        # input (, input)*
        here = self.getpos()
        if inp := self.input():
            near = self.getpos()
            if self.expect(lx.COMMA):
                if rest := self.inputs():
                    return [inp] + rest
            self.reset(near)
            return [inp]
        self.reset(here)
        return None

    def input(self):
        # IDENTIFIER
        if (tkn := self.expect(lx.IDENTIFIER)):
            if self.expect(lx.LBRACKET):
                if arg := self.expect(lx.IDENTIFIER):
                    if self.expect(lx.RBRACKET):
                        return f"{tkn.text}[{arg.text}]"
                    if self.expect(lx.TIMES):
                        if num := self.expect(lx.NUMBER):
                            if self.expect(lx.RBRACKET):
                                return f"{tkn.text}[{arg.text}*{num.text}]"
                raise self.make_syntax_error("Expected argument in brackets", tkn)

            return tkn.text
        if self.expect(lx.CONDOP):
            while self.expect(lx.CONDOP):
                pass
            return "??"
        return None

    def outputs(self):
        # output (, output)*
        here = self.getpos()
        if outp := self.output():
            near = self.getpos()
            if self.expect(lx.COMMA):
                if rest := self.outputs():
                    return [outp] + rest
            self.reset(near)
            return [outp]
        self.reset(here)
        return None

    def output(self):
        return self.input()  # TODO: They're not quite the same.

    def family_def(self):
        here = self.getpos()
        if (tkn := self.expect(lx.IDENTIFIER)) and tkn.text == "family":
            if self.expect(lx.LPAREN):
                if (tkn := self.expect(lx.IDENTIFIER)):
                    name = tkn.text
                    if self.expect(lx.RPAREN):
                        if self.expect(lx.EQUALS):
                            if members := self.members():
                                if self.expect(lx.SEMI):
                                    return Family(name, members)
        self.reset(here)
        return None

    def members(self):
        here = self.getpos()
        if tkn := self.expect(lx.IDENTIFIER):
            near = self.getpos()
            if self.expect(lx.PLUS):
                if rest := self.members():
                    return [tkn.text] + rest
            self.reset(near)
            return [tkn.text]
        self.reset(here)
        return None
