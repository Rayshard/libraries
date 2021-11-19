from io import StringIO
from os import linesep
import sys, json, re
from typing import Callable, Dict, List, NamedTuple, Optional, Pattern, Tuple, Union
from dataclasses import dataclass

@dataclass
class Position:
    line: int
    column: int

class StringStream:
    def __init__(self, string: str) -> None:
        self.__stream = StringIO(string)
        self.__size = len(string)

        # Get line starting offsets
        self.__lineStarts = [0]

        for line in self.__stream.readlines()[:-1]:
            self.__lineStarts.append(self.__lineStarts[-1] + len(line))

        self.__stream.seek(0) # reset stream to start
        print(self.__lineStarts)

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
        return self.__stream.seek(max(0, min(offset, self.__size)))

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
    PatternID = int
    Action = Union[None, Callable[['Lexer', str], str]]
    Pattern = NamedTuple('Pattern', [('id', int), ('regex', re.Pattern), ('action', Action)])
    Result = NamedTuple('Result', [('patternID', PatternID), ('position', Position), ('value', str)])
    
    EOS_PATTERN_ID = 0
    UNKNOWN_PATTERN_ID = 1

    def __init__(self, onEOS: Action = None, onUnknown: Action = None) -> None:
        self.__patternEOS = Lexer.Pattern(Lexer.EOS_PATTERN_ID, re.compile(""), onEOS)
        self.__patternUnknown = Lexer.Pattern(Lexer.UNKNOWN_PATTERN_ID, re.compile(""), onUnknown)
        self.__patterns : List[Lexer.Pattern] = []

    def AddPattern(self, regex: str, action: Action = None) -> PatternID:
        self.__patterns.append(Lexer.Pattern(len(self.__patterns) + 2, re.compile('^' + regex), action))
        return self.__patterns[-1].id

    def Lex(self, stream: StringStream) -> Result:
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

        return Lexer.Result(matchingPattern.id, streamPos, value)

@dataclass
class Decl:
    id: str
    expr: int

class Parser:
    class Callbacks:
        def OnLexStringLiteral(self, match: str) -> str:
            raise NotImplementedError()

        def OnParseStringLiteral(self, token: str) -> str:
            raise NotImplementedError()

        def OnParseNumber(self, token: str) -> int:
            raise NotImplementedError()

        def OnParseFullDecl(self, arg0: Position, arg1: Tuple[str, Position], arg2: Position, arg3: Tuple[int, Position], arg4: Position) -> int:
            raise NotImplementedError()

        def OnParseNoExprDecl(self, arg0: Position, arg1: Tuple[str, Position], arg2: Position) -> int:
            raise NotImplementedError()

    
    def __init__(self, callbacks: Callbacks) -> None:
        self.__lexer = Lexer(None, None)
        self.__lexer.AddPattern("\\s+")

        self.terminals : Dict[str, Lexer.PatternID] = {
            "=": lexer.AddPattern("-?(0|[1-9][0-9]*)([.][0-9]+)?"),
            ";": lexer.AddPattern("-?(0|[1-9][0-9]*)([.][0-9]+)?"),
            "let": lexer.AddPattern("-?(0|[1-9][0-9]*)([.][0-9]+)?"),
            "ID": lexer.AddPattern("(_|[a-zA-Z])(_|[a-zA-Z0-9])*"),
            "NUM": lexer.AddPattern("-?(0|[1-9][0-9]*)([.][0-9]+)?"),
            "STRING_LITERAL": lexer.AddPattern("\"", callbacks.OnLexStringLiteral),
        }

def CheckParserTemplateFormat(template):
    pass

if __name__ == "__main__":
    assert len(sys.argv) == 2, "Expected one path to parser template file!"

    parser_path = sys.argv[1]
    template = None

    with open(parser_path) as file:
        template = json.load(file)

    CheckParserTemplateFormat(template)

    lexer = Lexer()
    lexer.AddPattern("\\s+")
    id = lexer.AddPattern("[a-zA-Z]+")
    number = lexer.AddPattern("[0-9]+")

    stream = StringStream("Hello\nWorld123!")

    while not stream.IsEOS():
        print(lexer.Lex(stream))


