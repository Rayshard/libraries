#include <variant>
#include <unordered_map>
#include <vector>
#include <unistd.h>
#include <sstream>
#include <optional>

namespace cpplib::json
{
    struct JSONValue;

    typedef std::unordered_map<std::string, JSONValue> JSONObject;
    typedef std::vector<JSONValue> JSONArray;
    typedef std::string JSONString;
    typedef int64_t JSONNumber;
    typedef std::variant<JSONString, JSONNumber, JSONArray, JSONObject, bool> JSONValueType;

    class JSONValue
    {
        JSONValueType value;

    public:
        JSONValue() : value() {}
        JSONValue(JSONString _value) : value(_value) {}
        JSONValue(JSONNumber _value) : value(_value) {}
        JSONValue(JSONArray _value) : value(_value) {}
        JSONValue(JSONObject _value) : value(_value) {}
        JSONValue(bool _value) : value(_value) {}

        bool IsString() { return std::get_if<JSONString>(&value); }
        bool IsNumber() { return std::get_if<JSONNumber>(&value); }
        bool IsObject() { return std::get_if<JSONObject>(&value); }
        bool IsArray() { return std::get_if<JSONArray>(&value); }
        bool IsNull() { return value.index() == std::variant_npos; }

        bool IsTrue()
        {
            const bool* b = std::get_if<bool>(&value);
            return b && *b;
        }

        bool IsFalse()
        {
            const bool* b = std::get_if<bool>(&value);
            return b && *b;
        }

        JSONString* AsString() { return std::get_if<JSONString>(&value); }
        JSONNumber* AsNumber() { return std::get_if<JSONNumber>(&value); }
        JSONObject* AsObject() { return std::get_if<JSONObject>(&value); }
        JSONArray* AsArray() { return std::get_if<JSONArray>(&value); }

    private:
        void Output(std::ostream& _stream)
        {
            if (IsNull()) { _stream << "null"; }
            else if (IsTrue()) { _stream << "true"; }
            else if (IsFalse()) { _stream << "false"; }
            else if (JSONString* s = AsString()) { _stream << '"' << *s << '"'; }
            else if (JSONNumber* n = AsNumber()) { _stream << *n; }
            else if (JSONObject* o = AsObject())
            {
                _stream << "{";

                size_t count = 0;
                for (auto& [n, v] : *o)
                {
                    _stream << " \"" << n << "\" : ";
                    v.Output(_stream);

                    if (++count != o->size())
                        _stream << ',';
                }

                _stream << " }";
            }
            else if (JSONArray* a = AsArray())
            {
                _stream << "[";

                size_t count = 0;
                for (auto& v : *a)
                {
                    _stream << ' ';
                    v.Output(_stream);

                    if (++count != a->size())
                        _stream << ',';
                }

                _stream << " ]";
            }
        }

    public:
        std::string ToString()
        {
            std::stringstream ss;
            Output(ss);
            return ss.str();
        }
    };

    class JSON
    {
        std::optional<JSONValue> text;

    public:
        JSON(JSONObject&& _o) : text(_o) {}
        JSON(JSONArray&& _a) : text(_a) {}

        std::string ToString()
        {
            if (text.has_value()) { return text.value().ToString(); }
            else { return ""; }
        }
    };
}