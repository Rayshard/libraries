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
#include <iostream> //TODO: remove

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

    template<typename T>
    class Parser
    {
    public:
        struct Result
        {
            Position position;
            T value;

            std::string ToString() const
            {
                std::stringstream ss;
                ss << "Result { position = " << position << ", value = \"" << value << "\" }";
                return ss.str();
            }
        };

        typedef std::function<Result(const Position& _pos, TokenStream& _stream)> Function;

    private:
        std::string name;
        Function function;

    public:
        Parser(const std::string& _name, Function _func) : name(_name), function(_func) { }

        Result Parse(TokenStream& _stream) { return function(_stream.GetPosition(), _stream); }

        Result Parse(const std::string& _input, Lexer& _lexer, const std::set<Lexer::PatternID>& _ignores = {})
        {
            StringStream ss(_input);
            TokenStream stream(&_lexer, &ss, _ignores);
            return Parse(stream);
        };

        const std::string& GetName() const { return name; }
    };

    template<typename T>
    using ParseResult = typename Parser<T>::Result;

    static Parser<std::string> Terminal(const std::string& _name, const Lexer::PatternID& _patternID, std::optional<std::string> _value = std::nullopt)
    {
        return Parser<std::string>(_name, [&](const Position& _pos, TokenStream& _stream)
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

                return ParseResult<std::string>{.position = _pos, .value = token.value };
            });
    }

    template<typename... Args>
    static Parser<std::tuple<ParseResult<Args>...>> Sequence(const std::string& _name, Parser<Args>... _parsers)
    {
        using ParserT = Parser<std::tuple<ParseResult<Args>...>>;
        return ParserT(_name, [&](const Position& _pos, TokenStream& _stream)
            {
                return typename ParserT::Result{ .position = _pos, .value = { _parsers.Parse(_stream)... } };
            });
    }

    template<typename... Ts>
    struct Choices
    {
        typedef std::tuple<Parser<Ts>...> Parsers;
        typedef std::variant<Ts...> Result;
        typedef std::vector<Result> Results;
        static constexpr size_t Size = std::tuple_size_v<Parsers>;

        template <size_t N>
        static void insert(Parsers& _parsers, Results& _results, TokenStream& _stream)
        {
            try
            {
                size_t streamStart = _stream.GetOffset();
                //_results.emplace_back(std::get<N>(_parsers).Parse(_stream));
                _stream.SetOffset(streamStart);
            }
            catch(const ParseError& e)
            {
                _results.emplace_back(Result());
            }
            
            
            insert<N - 1>(_results, _results);
        }

        template <>
        static void insert<0>(Parsers& _parsers, Results& _results, TokenStream& _stream)
        {
            try
            {
                size_t streamStart = _stream.GetOffset();
                //_results.emplace_back(std::get<0>(_parsers).Parse(_stream));
                _stream.SetOffset(streamStart);
            }
            catch(const ParseError& e)
            {
                _results.emplace_back(Result());
            }
        }

        static Results insert(Parsers& _parsers, TokenStream& _stream)
        {
            Results results;
            insert<Size - 1>(_parsers, results, _stream);
            return results;
        }
    };

    template<typename... Args>
    static Parser<std::variant<ParseResult<Args>...>> Choice(const std::string& _name, Parser<Args>... _parsers)
    {
        using Arg = std::variant<ParseResult<Args>...>;
        return Parser<Arg>(_name, [&](const Position& _pos, TokenStream& _stream)
            {
                Arg arg;
                size_t streamStart = _stream.GetOffset(), greatestArgLength = 0;

                using TChoices = Choices<Args...>;
                auto parsers = std::make_tuple(_parsers...);
                auto choices = TChoices::insert(parsers, _stream);
                // typename Choices<Parser<Args>...>::Container choices;
                // K::insert(parsers, choices);
                std::cout << choices.size() << std::endl;

                return typename Parser<Arg>::Result{ .position = _pos, .value = Arg() };
            });
    }

    // class LPC
    // {
    //     Lexer lexer;
    //     Parser parser;
    //     std::set<Lexer::PatternID> ignores;

    // public:
    //     LPC(Lexer _lexer, Parser _parser, const std::set<Lexer::PatternID>& _ignores = {})
    //         : lexer(_lexer), parser(_parser), ignores(_ignores) { }

    //     typename Parser::Result Parse(const std::string& _input) { return parser.Parse(_input, lexer, ignores); };
    // };
}