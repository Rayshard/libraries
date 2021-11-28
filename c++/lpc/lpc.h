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

        ParseError(Position _pos, const std::string& _msg, const ParseError& _appendage)
            : ParseError(_pos, _msg + "\n" + _appendage.what()) { }

        ParseError(const ParseError& _e1, const ParseError& _e2)
            : std::runtime_error(_e1.what() + std::string("\n") + _e2.what()) { }

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

        Parser() : Parser("", [](const Position& _pos, TokenStream& _stream) { return Result{ .position = _pos, .value = T() }; }) { }

        Parser(const std::string& _name, Parser<T> _parser) : Parser(_name, [=](const Position& _pos, TokenStream& _stream) { return _parser.Parse(_stream); }) { }

        Result Parse(TokenStream& _stream) const
        {
            auto streamStart = _stream.GetOffset();

            try { return function(_stream.GetPosition(), _stream); }
            catch(const ParseError& e)
            {
                _stream.SetOffset(streamStart);
                throw ParseError(_stream.GetPosition(), "Unable to parse " + name, e);
            }
        }

        Result Parse(const std::string& _input, Lexer& _lexer, const std::set<Lexer::PatternID>& _ignores = {}) const
        {
            StringStream ss(_input);
            TokenStream stream(&_lexer, &ss, _ignores);
            return Parse(stream);
        };

        template<typename M>
        using Mapper = std::function<typename Parser<M>::Result(const Result&)>;

        template<typename M>
        Parser<M> Map(Mapper<M> _func) { return Parser<M>(name, [=, function = function](const Position& _pos, TokenStream& _stream) { return _func(function(_pos, _stream)); }); }

        const std::string& GetName() const { return name; }
    };

    template<typename T>
    using ParseResult = typename Parser<T>::Result;

    static Parser<std::string> Terminal(const std::string& _name, const Lexer::PatternID& _patternID, std::optional<std::string> _value = std::nullopt)
    {
        return Parser<std::string>(_name, [=](const Position& _pos, TokenStream& _stream)
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

    template<typename T>
    static Parser<T> Choice(const std::string& _name, std::initializer_list<Parser<T>> _parsers)
    {
        return Parser<T>(_name, [parsers = std::vector(_parsers)](const Position& _pos, TokenStream& _stream)
        {
            size_t streamStart = _stream.GetOffset(), greatestLength = 0;
            std::optional<ParseResult<T>> result;

            for (auto parser : parsers)
            {
                try
                {
                    auto parseResult = parser.Parse(_stream);
                    auto length = _stream.GetOffset() - streamStart;

                    if (length > greatestLength)
                        result = parseResult;
                }
                catch (const ParseError& e) {}

                _stream.SetOffset(streamStart);
            }

            return result.has_value() ? result.value() : ParseResult<T>{ .position = _pos, .value = T() };
        });
    }

    template<typename... Ts>
    using VariantResultValue = std::variant<Ts...>;

    template<typename... Ts>
    using VariantResult = ParseResult<VariantResultValue<Ts...>>;

    template<typename... Ts>
    struct VariantParsers : public std::array<Parser<std::variant<Ts...>>, sizeof...(Ts)>
    {
        typedef std::tuple<Parser<Ts>...> Parsers;
        typedef VariantResultValue<Ts...> Value;

        VariantParsers(Parser<Ts>... _parsers)
        {
            auto parsers = std::make_tuple(_parsers...);
            Populate<sizeof...(Ts) - 1>(parsers);
        }

    private:
        template <size_t Idx>
        void Populate(Parsers& _parsers)
        {
            if constexpr (Idx != 0)
                Populate<Idx - 1>(_parsers);

            using ParserType = std::tuple_element_t<Idx, Parsers>;
            using ArgType = std::variant_alternative_t<Idx, Value>;

            this->operator[](Idx) = std::get<Idx>(_parsers).template Map<Value>([](const ParseResult<std::string>& _value)
                {
                    return ParseResult<Value>{.position = _value.position, .value = Value(std::in_place_index<Idx>, _value.value) };
                });
        }
    };

    template<typename... Ts>
    static Parser<VariantResultValue<Ts...>> Variant(const std::string& _name, Parser<Ts>... _parsers)
    {
        return Parser<VariantResultValue<Ts...>>(_name, [=](const Position& _pos, TokenStream& _stream)
            {
                auto parsers = VariantParsers<Ts...>(_parsers...);
                size_t streamStart = _stream.GetOffset(), greatestLength = 0;
                std::optional<VariantResult<Ts...>> result;

                for (auto parser : parsers)
                {
                    try
                    {
                        auto parseResult = parser.Parse(_stream);
                        auto length = _stream.GetOffset() - streamStart;

                        if (length > greatestLength)
                        {
                            result = parseResult;
                            greatestLength = length;
                        }
                    }
                    catch (const ParseError& e) {}

                    _stream.SetOffset(streamStart);
                }

                return result.has_value() ? result.value() : VariantResult<Ts...>{ .position = _pos, .value = VariantResultValue<Ts...>() };
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