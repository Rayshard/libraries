from typing import Any, Dict
from lpc import lpc

def format_and_escape_string(lexer: lpc.Lexer, value: str) -> str:
        result = value[1:-1]
        result = result.replace("\\n", "\n")
        result = result.replace("\\t", "\t")
        result = result.replace("\\r", "\r")
        result = result.replace("\\f", "\f")
        result = result.replace("\\b", "\b")
        result = result.replace("\\\"", "\"")
        result = result.replace("\\\\", "\\")
        return result

class JSONParser(lpc.LPC[Dict[Any, Any]]):
    def __init__(self) -> None:
        lexer = lpc.Lexer()
        lexer.AddPattern("\\s+", None, "WS")
        lexer.AddPattern("{|}|\\[|\\]|,|:", None, "SYMBOL")
        lexer.AddPattern("(true)|(false)|(null)", None, "KEYWORD")
        lexer.AddPattern("-?(?:0|[1-9]\\d*)(?:\\.\\d+)(?:[eE][+-]?\\d+)?", None, "decimal")
        lexer.AddPattern("-?(?:0|[1-9]\\d*)", None, "integer")
        lexer.AddPattern("\"([^\\\"\\\\]|\\\\.)*\"", format_and_escape_string, "string")

        parsers : Dict[str, lpc.Parser] = {}
        parsers["string"] = lpc.Terminal("string", "string")
        parsers["value"] = lpc.Choice("Value", [
            parsers["string"],
            lpc.Terminal("number", "integer", transformer=lambda value: int(value)),
            lpc.Terminal("number", "decimal", transformer=lambda value: float(value)),
            lpc.Terminal("true", "KEYWORD", "true", transformer=lambda _: True),
            lpc.Terminal("false", "KEYWORD", "false", transformer=lambda _: False),
            lpc.Terminal("null", "KEYWORD", "null", transformer=lambda _: None),
            lpc.Lazy("object", lambda: parsers["object"]),
            lpc.Lazy("array", lambda: parsers["array"]),
        ], tag=True)
        parsers["pair"] = lpc.Sequence("pair", [parsers["string"], lpc.Terminal(":", "SYMBOL", ":"), parsers["value"]])
        parsers["object"] = lpc.Sequence("object", [
            lpc.Terminal("{", "SYMBOL", "{"),
            lpc.Separated("pairs", parsers["pair"], lpc.Terminal(",", "SYMBOL", ",")),
            lpc.Terminal("}", "SYMBOL", "}"),
        ], transformer=lambda value: {res.value[0].value:res.value[2].value[1] for res in value[1].value})
        parsers["array"] = lpc.Sequence("array", [
            lpc.Terminal("[", "SYMBOL", "["),
            lpc.Separated("values", parsers["value"], lpc.Terminal(",", "SYMBOL", ",")),
            lpc.Terminal("]", "SYMBOL", "]"),
        ], transformer=lambda value: [res.value[1] for res in value[1].value])

        super().__init__(lexer, lpc.Choice("json", [parsers["array"], parsers["object"]], tag=False), ["WS"])
