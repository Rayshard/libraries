#pragma once
#include <string>
#include <regex>
#include <vector>
#include <fstream>
#include <variant>
#include <optional>
#include <set>
#include <any>
#include <functional>
#include <sstream>
#include <tuple>

namespace lpc
{
    struct Position
    {
        size_t line, column;

        std::string ToString() const;
    };

    std::ostream& operator<<(std::ostream& _lhs, const Position& _rhs);
    std::string IStreamToString(std::istream& _stream);

    class Regex : public std::regex
    {
        std::string string;
    public:
        Regex();
        Regex(const std::string& _string);

        const std::string& GetString() const;
    };

    template<typename T>
    class Stream
    {
        size_t offset;

    protected:
        std::vector<T> data;

    public:
        Stream(std::vector<T>&& _data);

        virtual T Get();

        T Peek();
        size_t GetOffset() const;
        void SetOffset(size_t _offset);

        typename std::vector<T>::iterator Begin();
        typename std::vector<T>::iterator End();
        typename std::vector<T>::iterator Current();

        typename std::vector<T>::const_iterator CBegin() const;
        typename std::vector<T>::const_iterator CEnd() const;
        typename std::vector<T>::const_iterator CCurrent() const;

        virtual bool IsEOS() const = 0;
    };

    class StringStream : public Stream<char>
    {
        std::vector<size_t> lineStarts;

    public:
        StringStream(const std::string& _data);

        bool IsEOS() const override;

        void Ignore(size_t _amt);
        Position GetPosition(size_t _offset) const;
        Position GetPosition() const;
        void SetPosition(Position _pos);
        std::string GetDataAsString(size_t _start) const;
        std::string GetDataAsString(size_t _start, size_t _length) const;
    };

    class Lexer
    {
    public:
        typedef std::string PatternID;
        inline static PatternID EOS_PATTERN_ID() { return "<EOS>"; }
        inline static PatternID UNKNOWN_PATTERN_ID() { return "<UNKNOWN>"; }

        typedef std::monostate NoAction;
        typedef std::function<void(StringStream&, const std::string&)> Procedure;
        typedef std::function<std::string(StringStream&, const std::string&)> Function;
        typedef std::variant<Procedure, Function, NoAction> Action;

        struct Token
        {
            PatternID patternID;
            Position position;
            std::string value;

            bool IsEOS() const;
            bool IsUnknown() const;
        };

    private:
        struct Pattern
        {
            PatternID id;
            Regex regex;
            Action action;
        };

        std::vector<Pattern> patterns;
        Pattern patternEOS, patternUnknown;

    public:
        Lexer(Action _onEOS = std::monostate(), Action _onUnknown = std::monostate());

        PatternID AddPattern(PatternID _id, Regex _regex, Action _action = NoAction());
        PatternID AddPattern(Regex _regex, Action _action = NoAction());
        Token Lex(StringStream& _stream);

    private:
        bool HasPatternID(const PatternID& _id);
    };

    class TokenStream : public Stream<Lexer::Token>
    {
        Lexer* lexer;
        StringStream* ss;
        std::set<Lexer::PatternID> ignores;
    public:
        TokenStream(Lexer* _lexer, StringStream* _ss, const std::set<Lexer::PatternID>& _ignores = {});

        Lexer::Token Get() override;
        bool IsEOS() const override;
        Position GetPosition();
    };

    struct ParseError : public std::runtime_error
    {
        ParseError(Position _pos, const std::string& _msg)
            : std::runtime_error("Error @ " + _pos.ToString() + ": " + _msg) { }

        static ParseError Expectation(const std::string& _expected, const std::string& _found, const Position& _pos)
        {
            return ParseError(_pos, "Expected " + _expected + ", but found " + _found);
        }
    };

    class Parser
    {
    public:
        struct Result
        {
            Position position;
            std::any value;

            template<typename T>
            std::string ToString() const
            {
                std::stringstream ss;
                ss << "Result { position = " << position << ", value = \"" << GetValue<T>() << "\" }";
                return ss.str();
            }

            template<typename T>
            const T& GetValue() const { return std::any_cast<const T&>(value); }
        };

        typedef std::function<Result(const Position& _pos, TokenStream& _stream)> Function;

    private:
        std::string name;
        Function function;

    public:
        Parser(const std::string& _name, Function _func) : name(_name), function(_func) { }

        Result Parse(TokenStream& _stream)
        {
            return function(_stream.GetPosition(), _stream);
        }

        Result Parse(const std::string& _input, Lexer& _lexer, const std::set<Lexer::PatternID>& _ignores = {})
        {
            StringStream ss(_input);
            TokenStream stream(&_lexer, &ss, _ignores);
            return Parse(stream);
        };

        const std::string& GetName() const { return name; }
    };

    static Parser Terminal(const std::string& _name, const Lexer::PatternID& _patternID, std::optional<std::string> _value = std::nullopt)
    {
        return Parser(_name, [_patternID, _value](const Position& _pos, TokenStream& _stream)
            {
                auto token = _stream.Get();

                if (token.patternID != _patternID)
                {
                    auto expected = "'" + _patternID + (_value.has_value() ? "(" + _value.value() + ")" : "") + "'";
                    auto found = "'" + token.patternID + (token.value.empty() ? "" : "(" + token.value + ")") + "'";
                    throw ParseError::Expectation(expected, found, token.position);
                }
                else if (_value.has_value() && token.value != _value.value())
                    throw ParseError::Expectation("'" + _value.value() + "'", "'" + token.value + "'", token.position);

                return Parser::Result{ .position = _pos, .value = std::any(token.value) };
            });
    }

    static Parser Sequence(const std::string& _name, std::vector<Parser> _parsers)
    {
        return Parser(_name, [_parsers](const Position& _pos, TokenStream& _stream)
            {
                std::vector<Parser::Result> result;

                for (auto parser : _parsers)
                    result.push_back(parser.Parse(_stream));

                return typename Parser::Result{ .position = _pos, .value = std::any(result) };
            });
    }

    class LPC
    {
        Lexer lexer;
        Parser parser;
        std::set<Lexer::PatternID> ignores;

    public:
        LPC(Lexer _lexer, Parser _parser, const std::set<Lexer::PatternID>& _ignores = {})
            : lexer(_lexer), parser(_parser), ignores(_ignores) { }

        typename Parser::Result Parse(const std::string& _input) { return parser.Parse(_input, lexer, ignores); };
    };
}
