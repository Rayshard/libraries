from lpc import __version__
from lpc import lpc


def test_version():
    assert __version__ == '0.1.0'

def test_json():
    import json
    from examples import json_parser
    
    test = {
        "string": "Hello, \"\\\b\n\f\r\t\u0394World!",
        "number": 123.0,
        "true": True,
        "false": False,
        "null": None,
        "array": [1, "a", True, False, None]
    }
    test_str = json.dumps(test, indent=4, sort_keys=False, ensure_ascii=False)

    try:
        parser = json_parser.JSONParser()
        parse_result = parser.parse(test_str)
        assert test == parse_result
    except lpc.Parser.Error as e:
        print(e)