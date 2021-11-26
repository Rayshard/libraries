#include <variant>
#include <unordered_map>
#include <vector>
#include <unistd.h>
#include <sstream>
#include <optional>

#define INDENT(indent) std::string((indent) * 4, ' ')

namespace cpplib::json
{
    struct JSONValue;

    typedef std::unordered_map<std::string, JSONValue> JSONObject;
    typedef std::vector<JSONValue> JSONArray;
    typedef std::string JSONString;
    typedef double JSONNumber;
    typedef std::monostate JSONNull;

    void Output(std::ostream& _stream, JSONObject* _o, size_t _indent);
    void Output(std::ostream& _stream, JSONArray* _a, size_t _indent);
    void Output(std::ostream& _stream, JSONString* _s);
    void Output(std::ostream& _stream, JSONNumber* _n);

    class JSONValue
    {
        typedef std::variant<JSONString, JSONNumber, JSONArray, JSONObject, bool, JSONNull> type;
        type value;
    public:
        JSONValue(JSONNull _n) : value(_n) { }
        JSONValue(JSONString&& _s) : value(std::move(_s)) { }
        JSONValue(JSONNumber _n) : value(_n) { }
        JSONValue(JSONArray&& _a) : value(std::move(_a)) { }
        JSONValue(JSONObject&& _o) : value(std::move(_o)) { }
        JSONValue(bool _b) : value(_b) { }

        bool IsString() { return std::get_if<JSONString>(&value); }
        bool IsNumber() { return std::get_if<JSONNumber>(&value); }
        bool IsObject() { return std::get_if<JSONObject>(&value); }
        bool IsArray() { return std::get_if<JSONArray>(&value); }
        bool IsNull() { return std::get_if<std::monostate>(&value); }

        bool IsTrue()
        {
            const bool* b = std::get_if<bool>(&value);
            return b && *b;
        }

        bool IsFalse()
        {
            const bool* b = std::get_if<bool>(&value);
            return b && !*b;
        }

        JSONString* AsString() { return std::get_if<JSONString>(&value); }
        JSONNumber* AsNumber() { return std::get_if<JSONNumber>(&value); }
        JSONObject* AsObject() { return std::get_if<JSONObject>(&value); }
        JSONArray* AsArray() { return std::get_if<JSONArray>(&value); }

        void Output(std::ostream& _stream, size_t _indent)
        {
            if (IsNull()) { _stream << "null"; }
            else if (IsTrue()) { _stream << "true"; }
            else if (IsFalse()) { _stream << "false"; }
            else if (JSONString* s = AsString()) { json::Output(_stream, s); }
            else if (JSONNumber* n = AsNumber()) { json::Output(_stream, n); }
            else if (JSONObject* o = AsObject()) { json::Output(_stream, o, _indent); }
            else if (JSONArray* a = AsArray()) { json::Output(_stream, a, _indent); }
        }
    };

    class JSONReader
    {
    public:
        struct Position
        {
            size_t line, column;
            Position(size_t _line = 1, size_t _col = 1) : line(_line), column(_col) {}
        };

    private:
        static std::runtime_error CreateError(Position _pos, const std::string& _msg)
        {
            std::stringstream ss;
            ss << "(" << _pos.line << ", " << _pos.column << ") " << _msg;
            return std::runtime_error(ss.str());
        }

        std::istream* stream;
        Position position;

    public:
        JSONReader(std::istream* _stream) : stream(_stream), position() {}

        Position GetPosition() { return position; }

        int Peek() { return stream->peek(); }

        int Get()
        {
            int character = stream->get();
            position.column++;

            if (character == '\n')
            {
                position.line++;
                position.column = 1;
            }
            return character;
        }

        void SkipWhitespace()
        {
            while (std::isspace(Peek()))
                Get();
        }

        JSONNumber ReadNumber()
        {
            std::string str;
            Position pos;

            while (Peek() == '-')
                str += '-';

            bool hasDecimal = false;
            while (true)
            {
                int character = Peek();

                if (character == '.')
                {
                    if (hasDecimal || str.empty() || !std::isdigit(str.back())) { throw EXPECTATION(pos, "digit", "'.'"); }
                    else { hasDecimal = true; }
                }
                else if (!std::isdigit(character)) { break; }

                str += Get();
            }

            if (str.empty()) { throw EXPECTATION(pos, "number", Peek() == EOF ? "EOF" : std::string(1, Peek())); }
            else if (str.back() == '-' || str.back() == '.') { throw EXPECTATION(pos, "number", str); }
            else { return std::stod(str); }
        }

        JSONString ReadString()
        {
            JSONString result;
            Position pos = position;

            if (Peek() != '"') { throw EXPECTATION(pos, "\"", std::string(1, Peek())); }
            else { Get(); }

            while (true)
            {
                int character = Get();

                if (character == '\\')
                {
                    int escaped = Get();

                    switch (escaped)
                    {
                    case '"': result += '"'; break;
                    case '\\': result += '\\'; break;
                    case '/': result += '/'; break;
                    case 'b': result += '\b'; break;
                    case 'f': result += '\f'; break;
                    case 'n': result += '\n'; break;
                    case 'r': result += '\r'; break;
                    case 't': result += '\t'; break;
                    case 'u': result += "\\u"; break; //TODO: implement \uXXXX
                    default: throw EXPECTATION(pos, "\", \\, /, b, f, n, r, t, or uXXXX", escaped == EOF ? "EOF" : std::string(1, escaped));
                    }
                }
                else if (character == '"') { break; }
                else if (character == EOF) { throw EXPECTATION(pos, "\"", "EOF"); }
                else { result += character; }
            }

            return result;
        }

        void ReadKeyword(const std::string& _keyword)
        {
            Position pos = position;
            std::string remainingChars(_keyword.rbegin(), _keyword.rend());

            while (!remainingChars.empty() && Peek() == remainingChars.back())
            {
                Get();
                remainingChars.pop_back();
            }

            if (!remainingChars.empty())
                throw EXPECTATION(pos, _keyword, std::string(1, Peek()));
        }

        JSONNull ReadKWNull()
        {
            ReadKeyword("null");
            return JSONNull();
        }

        bool ReadKWTrue()
        {
            ReadKeyword("true");
            return true;
        }

        bool ReadKWFalse()
        {
            ReadKeyword("false");
            return false;
        }

        JSONValue ReadValue()
        {
            Position pos;

            switch (Peek())
            {
            case '{': return ReadObject();
            case '[': return ReadArray();
            case '"': return ReadString();
            case 't': return ReadKWTrue();
            case 'f': return ReadKWFalse();
            case 'n': return ReadKWNull();
            default: return ReadNumber();
            }
        }

        JSONObject ReadObject()
        {
            JSONObject result;
            Position pos;

            if (Peek() != '{') { throw EXPECTATION(pos, "{", std::string(1, Peek())); }
            else { Get(); }

            SkipWhitespace();

            if (Peek() != '}')
            {
                while (true)
                {
                    Position keyPos = position;
                    std::string name = ReadString();
                    if (result.find(name) != result.end())
                        throw DUPLICATE_KEY(keyPos, name);

                    SkipWhitespace();

                    if (Peek() != ':') { throw EXPECTATION(position, ":", std::string(1, Peek())); }
                    else { Get(); }

                    SkipWhitespace();
                    result.emplace(name, ReadValue());
                    SkipWhitespace();

                    if (Peek() != ',') { break; }
                    else { Get(); }

                    SkipWhitespace();
                }

                if (Peek() != '}') { throw EXPECTATION(position, "}", std::string(1, Peek())); }
                else { Get(); }
            }
            else { Get(); }

            return result;
        }

        JSONArray ReadArray()
        {
            JSONArray result;
            Position pos;

            if (Peek() != '[') { throw EXPECTATION(pos, "[", std::string(1, Peek())); }
            else { Get(); }

            SkipWhitespace();

            if (Peek() != ']')
            {
                while (true)
                {
                    result.push_back(ReadValue());
                    SkipWhitespace();

                    if (Peek() != ',') { break; }
                    else { Get(); }

                    SkipWhitespace();
                }

                if (Peek() != ']') { throw EXPECTATION(position, "]", std::string(1, Peek())); }
                else { Get(); }
            }
            else { Get(); }

            return result;
        }

        static std::runtime_error EXPECTATION(Position _pos, const std::string& _e, const std::string& _f) { return CreateError(_pos, "Expected " + _e + " but found " + _f + "."); }
        static std::runtime_error DUPLICATE_KEY(Position _pos, const std::string& _key) { return CreateError(_pos, "Duplicate key \"" + _key + "\" found."); }
    };

    void Output(std::ostream& _stream, JSONString* _s) { _stream << '"' << *_s << '"'; }
    void Output(std::ostream& _stream, JSONNumber* _n) { _stream << *_n; }

    void Output(std::ostream& _stream, JSONArray* _a, size_t _indent)
    {
        _stream << "[\n";

        size_t count = 0;
        for (auto& v : *_a)
        {
            _stream << INDENT(_indent + 1);
            v.Output(_stream, _indent + 1);

            if (++count != _a->size())
                _stream << ",\n";
        }

        _stream << '\n' << INDENT(_indent) << "]";
    }

    void Output(std::ostream& _stream, JSONObject* _o, size_t _indent)
    {
        _stream << "{\n";

        size_t count = 0;
        for (auto& [n, v] : *_o)
        {
            _stream << INDENT(_indent + 1) << "\"" << n << "\" : ";
            v.Output(_stream, _indent + 1);

            if (++count != _o->size())
                _stream << ",\n";
        }

        _stream << '\n' << INDENT(_indent) << "}";
    }

    typedef std::variant<JSONArray, JSONObject> JSONText;

    std::string ToString(JSONText&& _json)
    {
        std::stringstream ss;

        if (auto o = std::get_if<JSONObject>(&_json)) { Output(ss, o, 0); }
        else if (auto a = std::get_if<JSONArray>(&_json)) { Output(ss, a, 0); }

        return ss.str();
    }

    JSONText FromStream(std::istream& _stream)
    {
        JSONReader reader(&_stream);
        reader.SkipWhitespace();

        int character = reader.Peek();

        if (character == '{') { return reader.ReadObject(); }
        else if (character == '[') { return reader.ReadArray(); }

        throw JSONReader::EXPECTATION(reader.GetPosition(), "{ or [", std::string(1, character));
    }
}

#undef INDENT