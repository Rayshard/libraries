// #include "lpc.h"
// #include <iostream>
// #include <fstream>

// #include <unordered_map>
// #include <vector>

// using namespace lpc;
// using namespace lpc::parsers;
// using namespace lpc::parsers::lexing;

// struct JSONValue;

// typedef std::string JSONString;
// typedef double JSONNumber;
// typedef bool JSONBool;
// typedef std::monostate JSONNull;

// struct JSONObject
// {
//     std::unordered_map<std::string, JSONValue*> pairs;

//     JSONObject();
//     JSONObject(const JSONObject& _other);

//     ~JSONObject();

//     JSONObject& operator=(const JSONObject& _other);
// };

// struct JSONArray
// {
//     std::vector<JSONValue*> values;

//     JSONArray();
//     JSONArray(const JSONArray& _other);

//     ~JSONArray();

//     JSONArray& operator=(const JSONArray& _other);
// };

// #define INDENT(indent) std::string((indent) * 4, ' ')

// class JSONValue
// {
//     typedef std::variant<JSONString, JSONNumber, JSONArray, JSONObject, JSONBool, JSONNull> type;
//     type value;

// public:
//     JSONValue() : value(JSONNull()) {}
//     JSONValue(JSONNull _n) : value(_n) { }
//     JSONValue(JSONNumber _n) : value(_n) { }
//     JSONValue(bool _b) : value(_b) { }
//     JSONValue(const JSONString& _s) : value(_s) { }
//     JSONValue(JSONString&& _s) : value(std::move(_s)) { }
//     JSONValue(const JSONArray& _a) : value(_a) { }
//     JSONValue(JSONArray&& _a) : value(std::move(_a)) { }
//     JSONValue(const JSONObject& _o) : value(_o) { }
//     JSONValue(JSONObject&& _o) : value(std::move(_o)) { }

//     bool IsString() { return std::get_if<JSONString>(&value); }
//     bool IsNumber() { return std::get_if<JSONNumber>(&value); }
//     bool IsObject() { return std::get_if<JSONObject>(&value); }
//     bool IsArray() { return std::get_if<JSONArray>(&value); }
//     bool IsNull() { return std::get_if<std::monostate>(&value); }

//     bool IsTrue()
//     {
//         const JSONBool* b = std::get_if<JSONBool>(&value);
//         return b && *b;
//     }

//     bool IsFalse()
//     {
//         const JSONBool* b = std::get_if<JSONBool>(&value);
//         return b && !*b;
//     }

//     JSONString* AsString() { return std::get_if<JSONString>(&value); }
//     JSONNumber* AsNumber() { return std::get_if<JSONNumber>(&value); }
//     JSONObject* AsObject() { return std::get_if<JSONObject>(&value); }
//     JSONArray* AsArray() { return std::get_if<JSONArray>(&value); }
//     JSONBool* AsBool() { return std::get_if<JSONBool>(&value); }

//     static void Output(JSONValue* _value, std::ostream& _stream, size_t _indent) //make _value const
//     {
//         if (_value->IsNull()) { _stream << "null"; }
//         else if (_value->IsTrue()) { _stream << "true"; }
//         else if (_value->IsFalse()) { _stream << "false"; }
//         else if (JSONString* s = _value->AsString()) { _stream << '"' << *s << '"'; }
//         else if (JSONNumber* n = _value->AsNumber()) { _stream << *n; }
//         else if (JSONObject* o = _value->AsObject())
//         {
//             _stream << "{\n";

//             size_t count = 0;
//             for (auto& [n, v] : o->pairs)
//             {
//                 _stream << INDENT(_indent + 1) << "\"" << n << "\" : ";
//                 Output(v, _stream, _indent + 1);

//                 if (++count != o->pairs.size())
//                     _stream << ",\n";
//             }

//             _stream << '\n' << INDENT(_indent) << "}";
//         }
//         else if (JSONArray* a = _value->AsArray())
//         {
//             _stream << "[\n";

//             size_t count = 0;
//             for (auto& v : a->values)
//             {
//                 _stream << INDENT(_indent + 1);
//                 Output(v, _stream, _indent + 1);

//                 if (++count != a->values.size())
//                     _stream << ",\n";
//             }

//             _stream << '\n' << INDENT(_indent) << "]";
//         }
//     }
// };

// #undef INDENT

// JSONObject::JSONObject() : pairs() {}

// JSONObject::JSONObject(const JSONObject& _other)
// {
//     for (auto& [key, value] : _other.pairs)
//         pairs[key] = new JSONValue(*value);
// }

// JSONObject& JSONObject::operator=(const JSONObject& _other)
// {
//     for (auto& [key, value] : _other.pairs)
//         pairs[key] = new JSONValue(*value);

//     return *this;
// }

// JSONObject::~JSONObject()
// {
//     for (auto& [_, value] : pairs)
//         delete value;

//     pairs.clear();
// }

// JSONArray::JSONArray() : values() {}

// JSONArray::JSONArray(const JSONArray& _other)
// {
//     for (auto& value : _other.values)
//         values.push_back(new JSONValue(*value));
// }

// JSONArray& JSONArray::operator=(const JSONArray& _other)
// {
//     for (auto& value : _other.values)
//         values.push_back(new JSONValue(*value));

//     return *this;
// }

// JSONArray::~JSONArray()
// {
//     for (auto& value : values)
//         delete value;

//     values.clear();
// }

// Variant<JSONString, JSONNumber> JSON()
// {
//     OptionalParser<std::string> WS = Optional(Whitespaces());

//     Recursive<JSONValue> VALUE;
//     Parser<JSONString> STRING = Map<std::string, JSONString>(Lexeme(Regex("/^(\"(((?=\\\\)\\\\([\"\\\\\\/bfnrt]|u[0-9a-fA-F]{4}))|[^\"\\\\\\0-\\x1F\\x7F]+)*\")$/")),
//         [](ParseResult<std::string>& _result) { return JSONString(_result.value.substr(1, _result.value.size() - 2)); });
//     Parser<JSONNumber> NUMBER = Map<std::string, JSONNumber>(Lexeme(Regex("-?(?:0|[1-9]\\d*)(?:\\.\\d+)?(?:[eE][+-]?\\d+)?")),
//         [](ParseResult<std::string>& _result) { return JSONNumber(std::stod(_result.value)); });
//     // List<std::string, JSONValue> PAIR = WS >> STRING << WS << Char(':') & VALUE;

//     // Parser<JSONObject> OBJECT = Char('{') >> WS >> Map<QuantifiedValue<ListValue<std::string, JSONValue>>, JSONObject>(Separated(PAIR, Char(',')),
//     //     [](QuantifiedResult<ListValue<std::string, JSONValue>>& _result)
//     //     {
//     //         JSONObject object;

//     //         for (auto& pairResult : _result.value)
//     //         {
//     //             auto& [stringResult, valueResult] = pairResult.value;
//     //             object.pairs[stringResult.value] = new JSONValue(std::move(valueResult.value));
//     //         }

//     //         return object;
//     //     }) << Char('}');

//     // Parser<JSONArray> ARRAY = Char('[') >> WS >> Map<QuantifiedValue<JSONValue>, JSONArray>(Separated(VALUE, Char(',')),
//     //     [](QuantifiedResult<JSONValue>& _result)
//     //     {
//     //         JSONArray array;

//     //         for (auto& result : _result.value)
//     //             array.values.push_back(new JSONValue(std::move(result.value)));

//     //         return array;
//     //     }) << Char(']');

//     // VALUE.Set(WS >>
//     //     Map<JSONObject, JSONValue>(OBJECT, [](ParseResult<JSONObject>& _result) { return JSONValue(std::move(_result.value)); })
//     //     | Map<JSONArray, JSONValue>(ARRAY, [](ParseResult<JSONArray>& _result) { return JSONValue(std::move(_result.value)); })
//     //     | Map<JSONString, JSONValue>(STRING, [](ParseResult<JSONString>& _result) { return JSONValue(std::move(_result.value)); })
//     //     | Map<JSONNumber, JSONValue>(NUMBER, [](ParseResult<JSONNumber>& _result) { return JSONValue(_result.value); })
//     //     | Map<std::string, JSONValue>(Lexeme(Regex("true|false|null")), [](ParseResult<std::string>& _result)
//     //         {
//     //             if (_result.value == "true") { return JSONValue(true); }
//     //             else if (_result.value == "false") { return JSONValue(false); }
//     //             else { return JSONValue(JSONNull()); }
//     //         })
//     //     << WS);

//     return WS >> (Variant<JSONString, JSONNumber>::Create(STRING) || Variant<JSONString, JSONNumber>::Create(NUMBER));
// }

// int main()
// {
//     try
//     {
//         std::ifstream file("lpc/tests/json.json");
//         auto json = JSON().Parse(IStreamToString(file)).value;

//         //JSONValue value;

//         // if(auto* objectResult = Variant<JSONObject, JSONArray>::Extract<JSONObject>(json)) { value = std::move(objectResult->value); }
//         // else if(auto* arrayResult = Variant<JSONObject, JSONArray>::Extract<JSONArray>(json)) { value = std::move(arrayResult->value); }

//         //if(auto* result = Variant<JSONString, JSONNumber>::Extract<JSONString>(json)) { std::cout << result->value << std::endl; }
//         //else if(auto* result = Variant<JSONString, JSONNumber>::Extract<JSONNumber>(json)) { std::cout << result->value << std::endl; }

//         //JSONValue::Output(&value, std::cout, 0);
//     }
//     catch (const ParseError& e)
//     {
//         std::cerr << e.GetMessageWithTrace() << '\n';
//     }

//     return 0;
// }

#include <iostream>
#include "lpc.h"

using namespace lpc;
using namespace lpc::parsers;

struct MyParser : public Parser<int>
{

    Parser<int>* Clone() const override { return new MyParser(); }

protected:
    ParseResult<int> OnParse(const Position& _pos, StringStream& _stream) const override { return ParseResult<int>(_pos, 5); }
};

int main()
{
    std::cout << Map<int, int>(MyParser(), [](ParseResult<int>&& _input) { return 6; }).Parse("Hello World").value << std::endl;


    return 0;
}