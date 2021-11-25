from abc import ABC, abstractmethod
from enum import Enum, auto
from io import StringIO
from os import error
import re
from typing import Any, Callable, Dict, List, NamedTuple, Optional, Pattern, Tuple, Union
from dataclasses import dataclass

@dataclass
class Position:
    line: int
    column: int

    def __str__(self) -> str:
        return f"({self.line}, {self.column})"

class StringStream:
    def __init__(self, string: str) -> None:
        self.__stream = StringIO(string)
        self.__size = len(string)

        # Get line starting offsets
        self.__lineStarts = [0]

        for line in self.__stream.readlines()[:-1]:
            self.__lineStarts.append(self.__lineStarts[-1] + len(line))

        self.__stream.seek(0) # reset stream to start

    def Peek(self) -> str:
        start = self.__stream.tell()
        value = self.__stream.read(1)
        self.__stream.seek(start)
        return value

    def Get(self) -> str:
        return self.__stream.read(1)

    def PeekRemainder(self) -> str:
        start = self.__stream.tell()
        value = self.__stream.read()
        self.__stream.seek(start)
        return value

    def Ignore(self, amt: int) -> None:
        self.__stream.read(amt)

    def GetOffset(self) -> int:
        return self.__stream.tell()

    def SetOffset(self, offset: int) -> None:
        self.__stream.seek(max(0, min(offset, self.__size)))

    def IsEOS(self) -> bool:
        return self.GetOffset() == self.__size

    def GetPosition(self) -> Position:
        line, closestLineStart, offset = 0, 0, self.GetOffset()

        for lineStart in self.__lineStarts:
            if lineStart > offset:
                break

            closestLineStart = lineStart
            line += 1

        return Position(line, offset - closestLineStart + 1)

    def SetPosition(self, pos: Position) -> None:
        if pos.line > len(self.__lineStarts) or pos.line == 0 or pos.column == 0:
            raise Exception(f"Invalid position: {pos}")

        lineStart = self.__lineStarts[pos.line - 1]
        lineWidth = (self.__size if pos.line == len(self.__lineStarts) else self.__lineStarts[pos.line]) - lineStart

        if pos.column - 1 > lineWidth:
            raise Exception(f"Invalid position: {pos}")

        self.SetOffset(lineStart + pos.column - 1)

class Lexer:
    PatternID = str
    Action = Union[None, Callable[['Lexer', str], str]]
    Pattern = NamedTuple('Pattern', [('id', PatternID), ('regex', re.Pattern), ('action', Action)])
    Token = NamedTuple('Token', [('patternID', PatternID), ('position', Position), ('value', str)])
    
    def CreatePatternIDFromIdx(idx: int) -> PatternID:
        return f"<Pattern: {idx}>"

    EOS_PATTERN_ID = "<EOS>"
    UNKNOWN_PATTERN_ID = "<UNKNOWN>"

    def __init__(self, onEOS: Action = None, onUnknown: Action = None) -> None:
        self.__patternEOS = Lexer.Pattern(Lexer.EOS_PATTERN_ID, re.compile(""), onEOS)
        self.__patternUnknown = Lexer.Pattern(Lexer.UNKNOWN_PATTERN_ID, re.compile(""), onUnknown)
        self.__patterns : List[Lexer.Pattern] = []

    def AddPattern(self, regex: str, action: Action = None, id: Optional[PatternID] = None) -> PatternID:
        patternID = Lexer.CreatePatternIDFromIdx(len(self.__patterns)) if id is None else id
        assert patternID not in [pattern.id for pattern in self.__patterns], f"Pattern with id '{patternID}' already exists!"

        self.__patterns.append(Lexer.Pattern(patternID, re.compile(f"^({regex})"), action))
        return self.__patterns[-1].id

    def Lex(self, stream: StringStream) -> Token:
        matchingPattern: Optional[Lexer.Pattern] = None
        streamPos = stream.GetPosition()
        matchValue = ""

        if stream.IsEOS():
            matchingPattern = self.__patternEOS
        else:
            string = stream.PeekRemainder()
            greatestPatternMatchLength = 0

            for pattern in self.__patterns:
                searchResult = pattern.regex.search(string)
                if searchResult is None:
                    continue
                
                match = searchResult[0]
                if matchingPattern is None or len(match) > greatestPatternMatchLength:
                    matchingPattern = pattern
                    matchValue = match
                    greatestPatternMatchLength = len(match)

            if matchingPattern is None:
                matchingPattern = self.__patternUnknown
                matchValue = stream.Peek()

        stream.Ignore(len(matchValue))

        value = matchValue if matchingPattern.action is None else matchingPattern.action(self, matchValue)
        assert isinstance(value, str), "Matching Pattern's action did not return a string!"

        return Lexer.Token(matchingPattern.id, streamPos, value)

class TokenStream:
    def __init__(self, lexer: Lexer, string: str, ignores: Optional[List[Lexer.PatternID]] = None) -> None:
        self.lexer = lexer
        self.__stream = StringStream(string)
        self.__offset : int = 0
        self.__tokens : List[Lexer.Token] = []
        self.__ignores = ignores if ignores is not None else []

        assert Lexer.EOS_PATTERN_ID not in self.__ignores, f"{Lexer.EOS_PATTERN_ID} cannot be ignored!"

    def Get(self) -> Lexer.Token:
        while True:
            while len(self.__tokens) <= self.__offset and not self.IsEOS():
                self.__tokens.append(self.lexer.Lex(self.__stream))

            token = self.__tokens[self.__offset]
            self.__offset = len(self.__tokens) - 1 if self.IsEOS() else self.__offset + 1

            if token.patternID in self.__ignores:
                continue

            return token

    def Peek(self) -> Lexer.Token:
        initOffset = self.__offset
        token = self.Get()
        self.__offset = initOffset
        return token

    def GetOffset(self) -> int:
        return self.__offset

    def SetOffset(self, offset: int) -> None:
        self.__offset = max(0, min(offset, len(self.__tokens) - 1))

    def IsEOS(self) -> bool:
        return len(self.__tokens) != 0 and self.__tokens[-1].patternID == Lexer.EOS_PATTERN_ID

    def GetPosition(self) -> Position:
        return self.Peek().position

class Parser(ABC):
    Result = NamedTuple('Result', [('position', Position), ('value', Any)])
    Transformer = Callable[[Any], Any]

    def __init__(self, name: str, transformer: Optional[Transformer] = None, ignores: Optional[List[Lexer.PatternID]] = None) -> None:
        super().__init__()
        self.__name = name
        self.__ignores = ignores if ignores is not None else []
        self.__transformer = (lambda value: value) if transformer is None else transformer

        assert Lexer.EOS_PATTERN_ID not in self.__ignores, f"{Lexer.EOS_PATTERN_ID} cannot be ignored!"

    def GetName(self) -> str:
        return self.__name

    @abstractmethod
    def _parse(self, start: Position, stream: TokenStream) -> Result:
        raise NotImplementedError()

    def parse(self, stream: TokenStream) -> Result:
        startOffset = stream.GetOffset()

        try:
            while stream.Peek().patternID in self.__ignores:
                stream.Get()

            result = self._parse(stream.GetPosition(), stream)
            return Parser.Result(result.position, self.__transformer(result.value))
        except Parser.Error as e:
            stream.SetOffset(startOffset)
            raise Parser.Error.Combine(Parser.Error(stream.GetPosition(), f"Unable to parse {self.GetName()}"), e)

    class Error(Exception):
        def __init__(self, position: Position, msg: str) -> None:
            super().__init__(position, msg)

        def __str__(self) -> str:
            return f"Error @ {self.args[0]}: {self.args[1]}"

        def Combine(error1: 'Parser.Error', error2: 'Parser.Error') -> 'Parser.Error':
            return Parser.Error(error1.args[0], f"{error1.args[1]}\n{error2}")

        def Expectation(expected: str, found: str, pos: Position) -> 'Parser.Error':
            return Parser.Error(pos, f"Expected {expected}, but found {found}")

class Terminal(Parser):
    def __init__(self, name: str, patternID: Lexer.PatternID, value: Optional[str] = None, transformer: Optional[Parser.Transformer] = None, ignores: Optional[List[Lexer.PatternID]] = None) -> None:
        super().__init__(name=name, transformer=transformer, ignores=ignores)
        self.__patternID = patternID
        self.__value = value

    def _parse(self, start: Position, stream: TokenStream) -> Parser.Result:
        token = stream.Get()
        
        if token.patternID != self.__patternID:
            expected = self.__patternID + f"({self.__value})" if self.__value is not None else ""
            found = token.patternID + (f"({token.value})" if token.value != "" else "")
            raise Parser.Error.Expectation(expected, found, token.position)
        elif self.__value is not None and token.value != self.__value:
            raise Parser.Error.Expectation(self.__value, token.value, token.position)

        return Parser.Result(token.position, token.value)

class Sequence(Parser):
    def __init__(self, name: str, parsers: List[Parser], transformer: Optional[Parser.Transformer] = None, ignores: Optional[List[Lexer.PatternID]] = None) -> None:
        super().__init__(name=name, transformer=transformer, ignores=ignores)
        self.__parsers = parsers

    def _parse(self, start: Position, stream: TokenStream) -> Parser.Result:
        results : List[Parser.Result] = []

        for parser in self.__parsers:
            results.append(parser.parse(stream))

        return Parser.Result(results[0].position if len(results) != 0 else start, results)

class Choice(Parser):
    def __init__(self, name: str, parsers: List[Parser], tag: bool, transformer: Optional[Parser.Transformer] = None, ignores: Optional[List[Lexer.PatternID]] = None) -> None:
        super().__init__(name=name, transformer=transformer, ignores=ignores)
        self.__parsers = parsers
        self.__tag = tag

    def _parse(self, start: Position, stream: TokenStream) -> Parser.Result:
        for parser in self.__parsers:
            try:
                result = parser.parse(stream)
                return Parser.Result(result.position, (parser.GetName(), result.value)) if self.__tag else result
            except Parser.Error:
                continue 
            
        raise Parser.Error(start, f"Expected one of [{', '.join([parser.GetName() for parser in self.__parsers])}]")

class Quantified(Parser):
    def __init__(self, name: str, parser: Parser, minimum: int = 0, maximum: Optional[int] = None, transformer: Optional[Parser.Transformer] = None, ignores: Optional[List[Lexer.PatternID]] = None) -> None:
        super().__init__(name=name, transformer=transformer, ignores=ignores)

        assert minimum >= 0, f"minimum={minimum} must be at least 0"
        assert maximum is None or minimum <= maximum, f"minimum={minimum} must be no more than maximum={maximum}"
        
        self.__parser = parser
        self.__minimum = minimum
        self.__maximum = maximum

    def _parse(self, start: Position,  stream: TokenStream) -> Parser.Result:
        results : List[Parser.Result] = []

        while self.__maximum is None or len(results) < self.__maximum:
            try:
                results.append(self.__parser.parse(stream))
            except Parser.Error as e:
                if len(results) < self.__minimum:
                    err = Parser.Error.Expectation(f"at least {self.__minimum} '{self.__parser.GetName()}'", f"only {len(results)}", stream.GetPosition())
                    raise Parser.Error.Combine(err, e)
                else:
                    break

        return Parser.Result(results[0].position if len(results) != 0 else start, results)

    def AtLeast1(name: str, parser: Parser, transformer: Optional[Parser.Transformer] = None, ignores: Optional[List[Lexer.PatternID]] = None) -> 'Quantified':
        return Quantified(name, parser, minimum=1, maximum=None, transformer=transformer, ignores=ignores)

    def AtMost1(name: str, parser: Parser, transformer: Optional[Parser.Transformer] = None, ignores: Optional[List[Lexer.PatternID]] = None) -> 'Quantified':
        return Quantified(name, parser, minimum=0, maximum=1, transformer=transformer, ignores=ignores)

    def AtLeast0(name: str, parser: Parser, transformer: Optional[Parser.Transformer] = None, ignores: Optional[List[Lexer.PatternID]] = None) -> 'Quantified':
        return Quantified(name, parser, minimum=0, maximum=None, transformer=transformer, ignores=ignores)

class Separated(Parser):
    def __init__(self, name: str, value: Parser, seperator: Parser, transformer: Optional[Parser.Transformer] = None, ignores: Optional[List[Lexer.PatternID]] = None) -> None:
        super().__init__(name=name, transformer=transformer, ignores=ignores)

        appendage = Sequence(name, [seperator, value], transformer=lambda result: result[1], ignores=ignores)
        tail = Quantified.AtLeast0(name, appendage, transformer=lambda result: [res.value for res in result], ignores=ignores)
        self.__parser = Sequence(name, [value, tail], transformer=lambda result: [result[0]] + result[1].value, ignores=ignores)

    def _parse(self, start: Position, stream: TokenStream) -> Parser.Result:
        try:
            return self.__parser.parse(stream)
        except Parser.Error:
            return Parser.Result(start, [])

class Lazy(Parser):
    def __init__(self, name: str, thunk: Callable[[], Parser]) -> None:
        super().__init__(name=name)
        self.__thunk = thunk
        
    def _parse(self, start: Position, stream: TokenStream) -> Parser.Result:
        assert False, "Should not be called. self.__thunk().parse() should override self.parse()"

    def parse(self, stream: TokenStream) -> Parser.Result:
        return self.__thunk().parse(stream)

if __name__ == "__main__":
    def escape_str(string: str) -> str:
        result = string
        result = result.replace("\\n", "\n")
        result = result.replace("\\t", "\t")
        result = result.replace("\\r", "\r")
        result = result.replace("\\f", "\f")
        result = result.replace("\\b", "\b")
        result = result.replace("\\\"", "\"")
        result = result.replace("\\\\", "\\")
        return result

    lexer = Lexer()
    lexer.AddPattern("\\s+", None, "WS")
    lexer.AddPattern("{|}|\\[|\\]|,|:", None, "SYMBOL")
    lexer.AddPattern("(true)|(false)|(null)", None, "KEYWORD")
    lexer.AddPattern("-?(?:0|[1-9]\d*)(?:\.\d+)(?:[eE][+-]?\d+)?", None, "decimal")
    lexer.AddPattern("-?(?:0|[1-9]\d*)", None, "integer")
    lexer.AddPattern("\"([^\\\"\\\\]|\\\\.)*\"", lambda _, value: escape_str(value[1:-1]), "string")

    parsers : Dict[str, Parser] = {}
    parsers["string"] = Terminal("string", "string")
    parsers["value"] = Choice("Value", [
        parsers["string"],
        Terminal("number", "integer", transformer=lambda value: int(value)),
        Terminal("number", "decimal", transformer=lambda value: float(value)),
        Terminal("true", "KEYWORD", "true", transformer=lambda _: True),
        Terminal("false", "KEYWORD", "false", transformer=lambda _: False),
        Terminal("null", "KEYWORD", "null", transformer=lambda _: None),
        Lazy("object", lambda: parsers["object"]),
        Lazy("array", lambda: parsers["array"]),
    ], tag=True)
    parsers["pair"] = Sequence("pair", [parsers["string"], Terminal(":", "SYMBOL", ":"), parsers["value"]])
    parsers["object"] = Sequence("object", [
        Terminal("{", "SYMBOL", "{"),
        Separated("pairs", parsers["pair"], Terminal(",", "SYMBOL", ",")),
        Terminal("}", "SYMBOL", "}"),
    ], transformer=lambda value: {res.value[0].value:res.value[2].value[1] for res in value[1].value})
    parsers["array"] = Sequence("array", [
        Terminal("[", "SYMBOL", "["),
        Separated("values", parsers["value"], Terminal(",", "SYMBOL", ",")),
        Terminal("]", "SYMBOL", "]"),
    ], transformer=lambda value: [res.value[1] for res in value[1].value])
    parsers["json"] = Choice("json", [parsers["array"], parsers["object"]], tag=False)

    import json
    
    test = {
        "string": "Hello, \"\\\b\n\f\r\t\u0394World!",
        "number": 123.0,
        "true": True,
        "false": False,
        "null": None,
        "array": [1, "a", True, False, None]
    }
    test_str = json.dumps(test, indent=4, sort_keys=False, ensure_ascii=False)
    stream = TokenStream(lexer, test_str, ["WS"])

    try:
        parse_result = parsers["json"].parse(stream)
        assert test == parse_result.value
    except Parser.Error as e:
        print(e)

