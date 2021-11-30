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
#include <unordered_map>

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

    template<typename T>
    class Parser;

    class StringStream;

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

        struct Pattern
        {
            PatternID id;
            Regex regex;
            Action action;

            Parser<std::string> AsTerminal(std::optional<std::string> _value = std::nullopt) const;
        };

    private:
        std::vector<Pattern> patterns;
        std::unordered_map<PatternID, Pattern*> patternsMap;
        Pattern patternEOS, patternUnknown;

    public:
        Lexer(Action _onEOS = std::monostate(), Action _onUnknown = std::monostate());

        const Pattern& AddPattern(PatternID _id, Regex _regex, Action _action = NoAction());
        const Pattern& AddPattern(Regex _regex, Action _action = NoAction());
        
        Token Lex(StringStream& _stream) const;

        const Pattern& GetPattern(const PatternID& _id) const;
        bool HasPattern(const PatternID& _id) const;
    };

    class StringStream
    {
        size_t offset;
        std::string data;
        std::vector<size_t> lineStarts;
        
        const Lexer lexer;
        std::unordered_map<size_t, std::pair<Lexer::Token, size_t>> tokens;
        std::set<Lexer::PatternID> ignores;

    public:
        StringStream(const std::string& _data, const Lexer& _lexer, const std::set<Lexer::PatternID>& _ignores = {});
        StringStream(const std::string& _data);

        char GetChar();
        char PeekChar();
        void IgnoreChars(size_t _amt);

        const Lexer::Token& GetToken();
        const Lexer::Token& PeekToken();

        size_t GetOffset() const;
        Position GetPosition(size_t _offset) const;
        Position GetPosition() const;

        void SetOffset(size_t _offset);
        void SetPosition(Position _pos);
        
        std::string GetData(size_t _start) const;
        std::string GetData(size_t _start, size_t _length) const;

        std::string::iterator Begin();
        std::string::iterator End();
        std::string::iterator Current();

        std::string::const_iterator CBegin() const;
        std::string::const_iterator CEnd() const;
        std::string::const_iterator CCurrent() const;

        bool IsEOS() const;
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
        ParseError(const std::string& _msg)
            : std::runtime_error(_msg) { }

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
    struct ParserOperation
    {
        std::string name;

        ParserOperation(const std::string& _name) : name(_name) { }
        ~ParserOperation() { }

        virtual Parser<T> GetParser() const = 0;
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

        Parser(const ParserOperation<T>& _op) : Parser(_op.GetParser()) { }

        Parser(const std::string& _name, const ParserOperation<T>& _op) : Parser(_name, _op.GetParser()) { }

        Result Parse(TokenStream& _stream) const
        {
            auto streamStart = _stream.GetOffset();

            try { return function(_stream.GetPosition(), _stream); }
            catch (const ParseError& e)
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
        using Mapper = std::function<M(const Result&)>;

        template<typename M>
        Parser<M> Map(Mapper<M> _func) const { return Parser<M>(name, [=, function = function](const Position& _pos, TokenStream& _stream) { return typename Parser<M>::Result{ .position = _pos, .value = _func(function(_pos, _stream)) }; }); }

        const std::string& GetName() const { return name; }
    };

    template<typename T>
    using ParseResult = typename Parser<T>::Result;

    template<typename T>
    class Recursive : public Parser<T>
    {
        std::shared_ptr<Parser<T>> parser;

    public:
        Recursive(const std::string& _name) : Parser<T>(_name, [=](const Position& _pos, TokenStream& _stream) { return parser->Parse(_stream); }) {}

        void Set(const Parser<T>& _parser) { parser.reset(new Parser<T>(_parser)); }
    };

    template<typename T>
    using Empty = Parser<T>;

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

    template<typename T>
    static Parser<T> Quantified(const Parser<T>& _option1, const Parser<T>& _option2)
    {
        assert(false && "NOT IMPLEMENTED");
    }

    template<typename T>
    static Parser<T> Separated(const Parser<T>& _option1, const Parser<T>& _option2)
    {
        assert(false && "NOT IMPLEMENTED");
    }

    template<typename Keep, typename Discard>
    Parser<Keep> operator<<(const Parser<Keep>& _keep, const Parser<Discard>& _discard)
    {
        return Parser<Keep>("(" + _keep.GetName() + " << " + _discard.GetName() + ")", [keep = _keep, discard = _discard](const Position& _pos, TokenStream& _stream)
            {
                auto result = keep.Parse(_stream);
                discard.Parse(_stream);
                return result;
            });
    }

    template<typename Discard, typename Keep>
    Parser<Keep> operator>>(const Parser<Discard>& _discard, const Parser<Keep>& _keep)
    {
        return Parser<Keep>("(" + _discard.GetName() + " >> " + _keep.GetName() + ")", [keep = _keep, discard = _discard](const Position& _pos, TokenStream& _stream)
            {
                discard.Parse(_stream);
                return keep.Parse(_stream);
            });
    }

    template<typename... Ts>
    struct List : public ParserOperation<std::tuple<ParseResult<Ts>...>>
    {
        using Parsers = std::tuple<Parser<Ts>...>;
        using ParserT = Parser<std::tuple<ParseResult<Ts>...>>;
        using ParserTResultValue = std::tuple<ParseResult<Ts>...>;
        using ParserTResult = ParseResult<ParserTResultValue>;

        Parsers parsers;

        List(const std::string& _name, const Parsers& _parsers) : ParserOperation<ParserTResultValue>(_name), parsers(_parsers) { }

        Parser<ParserTResultValue> GetParser() const override
        {
            return ParserT(this->name, [parsers = parsers](const Position& _pos, TokenStream& _stream)
                {
                    return ParserTResult
                    {
                        .position = _pos,
                        .value = std::apply([&](auto &&... _args) { return std::make_tuple(_args.Parse(_stream)...); }, parsers)
                    };
                });
        }
    };

    template<typename First, typename Second>
    List<First, Second> operator&(const Parser<First>& _first, const Parser<Second>& _second)
    {
        return List<First, Second>("(" + _first.GetName() + " & " + _second.GetName() + ")", { _first, _second });
    }

    template<typename... Head, typename Appendage>
    List<Head..., Appendage> operator&(const List<Head...>& _head, const Parser<Appendage>& _appendage)
    {
        auto list = List<Head..., Appendage>(_head.name, std::tuple_cat(_head.parsers, std::make_tuple(_appendage)));
        list.name.replace(list.name.size() - 1, 1, " & " + _appendage.GetName() + ")");
        return list;
    }

    template<typename First, typename Second>
    List<First, Second> operator&(const ParserOperation<First>& _first, const Parser<Second>& _second) { return Parser(_first) & _second; }

    template<typename First, typename Second>
    List<First, Second> operator&(const Parser<First>& _first, const ParserOperation<Second>& _second) { return _first & Parser(_second); }

    template<typename First, typename Second>
    List<First, Second> operator&(const ParserOperation<First>& _first, const ParserOperation<Second>& _second) { return Parser(_first) & Parser(_second); }

    template<typename T>
    struct Choice : public ParserOperation<T>
    {
        std::vector<Parser<T>> parsers;

        Choice(const std::string& _name, const std::vector<Parser<T>>& _parsers) : ParserOperation<T>(_name), parsers(_parsers) { }

        Parser<T> GetParser() const override
        {
            assert(parsers.size() >= 2 && "Choice expects at least 2 options");

            return Parser<T>(this->name, [parsers = parsers](const Position& _pos, TokenStream& _stream)
                {
                    size_t streamStart = _stream.GetOffset(), greatestLength = 0;
                    std::optional<ParseResult<T>> result;
                    std::vector<ParseError> errors;

                    for (auto parser : parsers)
                    {
                        try
                        {
                            auto parseResult = parser.Parse(_stream);
                            auto length = _stream.GetOffset() - streamStart;

                            if (!result.has_value() || length > greatestLength)
                            {
                                result = parseResult;
                                greatestLength = length;
                                errors.clear();
                            }
                        }
                        catch (const ParseError& e)
                        {
                            if (!result.has_value())
                                errors.push_back(ParseError(std::regex_replace(e.what(), std::regex("\n"), "\n ")));
                        }

                        _stream.SetOffset(streamStart);
                    }

                    if (!result.has_value())
                    {
                        ParseError error = errors[0];

                        for (size_t i = 1; i < errors.size(); i++)
                            error = ParseError(error, errors[i]);

                        error = ParseError(std::regex_replace(error.what(), std::regex("\n(?! )"), "\n >> "));
                        error = ParseError(" >> " + std::regex_replace(error.what(), std::regex("\n(?! >>)"), "\n   "));
                        throw error;
                    }

                    _stream.SetOffset(streamStart + greatestLength);
                    return result.value();
                });
        }
    };

    template<typename T>
    Choice<T> operator|(const Parser<T>& _option1, const Parser<T>& _option2)
    {
        return Choice<T>("(" + _option1.GetName() + " | " + _option2.GetName() + ")", std::vector({ _option1, _option2 }));
    }

    template<typename T>
    Choice<T> operator|(const Choice<T>& _choices, const Parser<T>& _additional)
    {
        auto choice = _choices;
        choice.name.replace(choice.name.size() - 1, 1, " | " + _additional.GetName() + ")");
        choice.parsers.push_back(_additional);
        return choice;
    }

    template<typename T>
    Choice<T> operator|(const ParserOperation<T>& _option1, const Parser<T>& _option2) { return Parser(_option1) | _option2; }

    template<typename T>
    Choice<T> operator|(const Parser<T>& _option1, const ParserOperation<T>& _option2) { return _option1 | Parser(_option2); }

    template<typename T>
    Choice<T> operator|(const ParserOperation<T>& _option1, const ParserOperation<T>& _option2) { return Parser(_option1) | Parser(_option2); }

    template<typename... Ts>
    struct Variant : public ParserOperation<std::variant<Ts...>>
    {
        static constexpr size_t Size = sizeof...(Ts);
        using Parsers = std::tuple<Parser<Ts>...>;
        using MappedParserResultValue = std::variant<Ts...>;
        using MappedParsers = std::vector<Parser<MappedParserResultValue>>;

        Parsers parsers;

        Variant(const std::string& _name, Parsers _parsers) : ParserOperation<MappedParserResultValue>(_name), parsers(_parsers) { }

        Parser<std::variant<Ts...>> GetParser() const override
        {
            MappedParsers mappedParsers(Size);
            PopulateMappedParsers<Size - 1>(mappedParsers);
            return Choice<MappedParserResultValue>(this->name, mappedParsers).GetParser();
        }

        template <size_t Idx>
        void PopulateMappedParsers(MappedParsers& _parsers) const
        {
            using ArgType = std::variant_alternative_t<Idx, MappedParserResultValue>;

            _parsers[Idx] = std::get<Idx>(parsers).template Map<MappedParserResultValue>([](const ParseResult<ArgType>& _value)
                {
                    return MappedParserResultValue(std::in_place_index<Idx>, _value.value);
                });

            if constexpr (Idx != 0)
                PopulateMappedParsers<Idx - 1>(_parsers);
        }
    };

    template<typename Opt1, typename Opt2>
    Variant<Opt1, Opt2> operator||(const Parser<Opt1>& _option1, const Parser<Opt2>& _option2)
    {
        return Variant<Opt1, Opt2>("(" + _option1.GetName() + " | " + _option2.GetName() + ")", { _option1, _option2 });
    }

    template<typename... Initial, typename Extra>
    Variant<Initial..., Extra> operator||(Variant<Initial...> _initial, const Parser<Extra>& _extra)
    {
        auto variant = Variant<Initial..., Extra>(_initial.name, std::tuple_cat(_initial.parsers, std::make_tuple(_extra)));
        variant.name.replace(variant.name.size() - 1, 1, " | " + _extra.GetName() + ")");
        return variant;
    }

    template<typename Opt1, typename Opt2>
    Variant<Opt1, Opt2> operator||(const ParserOperation<Opt1>& _option1, const Parser<Opt2>& _option2) { return Parser(_option1) || _option2; }

    template<typename Opt1, typename Opt2>
    Variant<Opt1, Opt2> operator||(const Parser<Opt1>& _option1, const ParserOperation<Opt2>& _option2) { return _option1 || Parser(_option2); }

    template<typename Opt1, typename Opt2>
    Variant<Opt1, Opt2> operator||(const ParserOperation<Opt1>& _option1, const ParserOperation<Opt2>& _option2) { return Parser(_option1) || Parser(_option2); }

    template<typename T>
    class LPC
    {
    public:
        typedef std::tuple<Lexer, Parser<T>, std::set<Lexer::PatternID>> Tuple;

    private:
        Lexer lexer;
        Parser<T> parser;
        std::set<Lexer::PatternID> ignores;

    public:
        LPC(const Lexer& _lexer, const Parser<T>& _parser, const std::set<Lexer::PatternID>& _ignores = {}) : lexer(_lexer), parser(_parser), ignores(_ignores) { }
        LPC(const Tuple& _lpc) : LPC(std::get<0>(_lpc), std::get<1>(_lpc), std::get<2>(_lpc)) { }

        ParseResult<T> Parse(const std::string _input) { return parser.Parse(_input, lexer, ignores); }
    };
}