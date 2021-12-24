#pragma once
#include <string>
#include <regex>
#include <variant>
#include <optional>
#include <vector>
#include <set>
#include <functional>
#include <cassert>

namespace lpc
{
    struct Position
    {
        size_t line, column;

        std::string ToString() const;
    };

    std::ostream& operator<<(std::ostream& _lhs, const Position& _rhs);

    std::string IStreamToString(std::istream& _stream);

    class StringStream
    {
        size_t offset;
        std::string data;
        std::vector<size_t> lineStarts;

    public:
        StringStream(const std::string& _data);

        char GetChar();
        char PeekChar();
        void IgnoreChars(size_t _amt);

        size_t GetOffset() const;
        size_t GetOffset(const Position& _pos) const;

        Position GetPosition() const;
        Position GetPosition(size_t _offset) const;

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

    class Regex : public std::regex
    {
        std::string string;
    public:
        Regex();
        Regex(const std::string& _string);

        const std::string& GetString() const;
    };

    class Lexer
    {
    public:
        typedef std::string PatternID;

        inline static PatternID EOS_PATTERN_ID() { return "<EOS>"; }
        inline static PatternID UNKNOWN_PATTERN_ID() { return "<UNKNOWN>"; }

        struct Token
        {
            PatternID patternID;
            Position position;
            std::string value;

            bool IsEOS() const;
            bool IsUnknown() const;
        };

        typedef std::monostate NoAction;
        typedef std::function<void(StringStream&, const Token&)> Procedure;
        typedef std::function<std::string(StringStream&, const Token&)> Function;
        typedef std::variant<Procedure, Function, NoAction> Action;

        struct Pattern
        {
            PatternID id;
            Regex regex;
            Action action;
        };

    private:
        std::vector<Pattern> patterns;
        std::unordered_map<PatternID, Pattern*> patternsMap;
        Pattern patternEOS, patternUnknown;

    public:
        Lexer(Action _onEOS = std::monostate(), Action _onUnknown = OnLexUnknown);

        const Pattern& AddPattern(PatternID _id, Regex _regex, Action _action = NoAction());
        const Pattern& AddPattern(Regex _regex, Action _action = NoAction());

        Token Lex(StringStream& _stream) const;

        const Pattern& GetPattern(const PatternID& _id) const;
        bool HasPattern(const PatternID& _id) const;

        static void OnLexUnknown(StringStream& _stream, const Lexer::Token& _token) { throw std::runtime_error("Unrecognized token: '" + _token.value + "' at " + _token.position.ToString()); }
    };

    class ParseError : public std::runtime_error
    {
        Position position;
        std::string message;
        std::vector<ParseError> trace;

    public:
        ParseError(Position _pos, const std::string& _msg)
            : std::runtime_error("Error @ " + _pos.ToString() + ": " + _msg), position(_pos), message(_msg), trace() { }

        ParseError(const ParseError& _e1, const ParseError& _e2)
            : ParseError(_e1.position, _e1.message)
        {
            trace.insert(trace.end(), _e1.trace.begin(), _e1.trace.end());
            trace.push_back(_e2);
        }

        ParseError(Position _pos, const std::string& _msg, std::initializer_list<ParseError> _trace)
            : ParseError(_pos, _msg)
        {
            trace.insert(trace.end(), _trace.begin(), _trace.end());
        }

        ParseError()
            : ParseError(Position(), "") { }

        std::string GetMessageWithTrace() const
        {
            std::string traceMessage;

            for (auto& e : trace)
                traceMessage += "\n" + std::regex_replace(e.GetMessageWithTrace(), std::regex("\\n"), "\n\t");

            return what() + traceMessage;
        }

        const Position& GetPosition() const { return position; }
        const std::string& GetMessage() const { return message; }
        const std::vector<ParseError>& GetTrace() const { return trace; }

        static ParseError Expectation(const std::string& _expected, const std::string& _found, const Position& _pos)
        {
            return ParseError(_pos, "Expected " + _expected + ", but found " + _found);
        }
    };

    template<typename T>
    struct ParseResult
    {
        Position position;
        T value;

        ParseResult(Position _pos, T _value)
            : position(_pos), value(_value) {}

        ParseResult()
            : ParseResult(Position(), T()) {}
    };

    template<typename T>
    using ParseFunction = std::function<ParseResult<T>(const Position& _pos, StringStream& _stream)>;

    template<typename T>
    struct Parser
    {
        typedef ParseFunction<T> Function;
        typedef ParseResult<T> Result;

    private:
        ParseFunction<T> function;

    public:
        Parser(ParseFunction<T> _func) : function(_func) { }

        virtual ~Parser() { }

        Result Parse(StringStream& _stream) const
        {
            auto streamStart = _stream.GetOffset();

            try { return function(_stream.GetPosition(), _stream); }
            catch (const ParseError& e)
            {
                _stream.SetOffset(streamStart);
                throw e;
            }
        }

        ParseResult<T> Parse(const std::string& _input) const
        {
            StringStream stream(_input);
            return Parse(stream);
        }
    };

    template<typename T>
    struct IParser : public Parser<T>
    {
        IParser() : Parser<T>([this](auto _pos, auto _stream) { return OnParse(_pos, _stream); }) { }
        virtual ~IParser() { }

    protected:
        virtual Parser<T>::Result OnParse(const Position& _pos, StringStream& _stream) = 0;
    };

    namespace parsers
    {
        template<typename T>
        class Recursive : public IParser<T>
        {
            std::shared_ptr<Parser<T>> parser;

        public:
            void Set(const Parser<T>& _parser) { parser.reset(new Parser<T>(_parser)); }

        protected:
            ParseResult<T> OnParse(const Position& _pos, StringStream& _stream) override { return parser->Parse(_stream); }
        };

        template<typename In, typename Out>
        Parser<Out> Mapped(const Parser<In>& _parser, std::function<Out(const ParseResult<In>&)> _map)
        {
            return Parser<Out>([=](const Position& _pos, StringStream& _stream)
                {
                    ParseResult<In> input = _parser.Parse(_stream);
                    return ParseResult<Out>(input.position, _map(input));
                });
        }

        template<typename In, typename Out>
        Parser<Out> Chained(const Parser<In>& _parser, std::function<Parser<Out>(ParseResult<In>&)> _chain)
        {
            return Parser<Out>([=](const Position& _pos, StringStream& _stream) { return _chainer(_parser.Parse(_stream)).Parse(_stream); });
        }

        template<typename T>
        Parser<T> Satisfied(const Parser<T>& _parser, std::function<bool(const ParseResult<T>&)> _predicate, std::function<ParseError(const ParseResult<T>&)> _onFail = nullptr)
        {
            return Parser<T>([=](const Position& _pos, StringStream& _stream)
                {
                    auto result = _parser.Parse(_stream);

                    if (_predicate(result)) { return result; }
                    else { throw _onFail ? _onFail(result) : ParseResult(_pos, "Predicate not satisfied!"); }
                });
        }

        template<typename T>
        Parser<T> Satisfied(const Parser<T>& _parser, const T& _value, std::function<ParseError(const ParseResult<T>&)> _onFail = nullptr)
        {
            return Satisfied(_parser, [=](const ParseResult<T>& _result) { return _result.value == _value; }, _onFail);
        }

        template<typename T>
        Parser<T> Success(const Parser<T>& _parser, const T& _default)
        {
            return Parser<T>([=](const Position& _pos, StringStream& _stream)
                {
                    try { return _parser.Parse(_stream); }
                    catch (const ParseError& e) { return ParseResult<T>(_pos, _default); }
                });
        }

        template<typename T>
        Parser<ParseError> Failure(const Parser<T>& _parser)
        {
            return Parser<T>([=](const Position& _pos, StringStream& _stream)
                {
                    try
                    {
                        _parser.Parse(_stream);
                        throw ParseError(_pos, "Unexpected success!");
                    }
                    catch (const ParseError& e) { return ParseResult<ParseError>(_pos, e); }
                });
        }

#pragma region Quantified
        template<typename T>
        using QuantifiedValue = std::vector<ParseResult<T>>;

        template<typename T>
        using QuantifiedParser = Parser<QuantifiedValue<T>>;

        template<typename T>
        using QuantifiedResult = ParseResult<QuantifiedValue<T>>;

        template<typename T>
        QuantifiedParser<T> Quantified(const Parser<T>& _parser, size_t _min, size_t _max)
        {
            assert(_max >= _min && "_max must be at least _min");

            return QuantifiedParser<T>([=](const Position& _pos, StringStream& _stream)
                {
                    QuantifiedValue<T> results;

                    if (_max != 0)
                    {
                        while (results.size() <= _max)
                        {
                            try { results.push_back(_parser.Parse(_stream)); }
                            catch (const ParseError& e)
                            {
                                if (results.size() >= _min)
                                    break;

                                auto error = ParseError::Expectation("at least " + std::to_string(_min), "only " + std::to_string(results.size()), _stream.GetPosition());
                                throw ParseError(e, error);
                            }
                        }
                    }

                    Position position = results.size() == 0 ? _pos : results[0].position; //this is separated from the return statement because of the unknown order of argument evaluation 
                    return QuantifiedResult<T>(position, std::move(results));
                });
        }

        template<typename T>
        QuantifiedParser<T> ManyOrOne(const Parser<T>& _parser) { return Quantified<T>(_parser, 1, -1); }

        template<typename T>
        QuantifiedParser<T> ZeroOrOne(const Parser<T>& _parser) { return Quantified<T>(_parser, 0, 1); }

        template<typename T>
        QuantifiedParser<T> ZeroOrMore(const Parser<T>& _parser) { return Quantified<T>(_parser, 0, -1); }

        template<typename T>
        QuantifiedParser<T> Exactly(const Parser<T>& _parser, size_t _n) { return Quantified<T>(_parser, _n, _n); }
#pragma endregion

#pragma region Optional
        template<typename T>
        using OptionalValue = std::optional<T>;

        template<typename T>
        using OptionalParser = Parser<OptionalValue<T>>;

        template<typename T>
        using OptionalResult = ParseResult<OptionalValue<T>>;

        template<typename T>
        OptionalParser<T> Optional(const Parser<T>& _parser)
        {
            return OptionalParser<T>([=](const Position& _pos, StringStream& _stream)
                {
                    OptionalValue<T> value;

                    try
                    {
                        auto result = _parser.Parse(_stream);
                        return OptionalResult<T>(result.position, result.value);
                    }
                    catch (const ParseError& e) { return OptionalResult<T>(_pos, std::nullopt); }
                });
        }
#pragma endregion

        template<typename T>
        Parser<T> Named(const std::string& _name, const Parser<T>& _parser)
        {
            return Parser<T>([=](const Position& _pos, StringStream& _stream)
                {
                    try { return _parser.Parse(_stream); }
                    catch (const ParseError& e) { throw ParseError(ParseError(_stream.GetPosition(), "Unable to parse " + _name), e); }
                });
        }

        template<typename P, typename T>
        Parser<T> Prefixed(const Parser<P>& _prefix, const Parser<T>& _parser)
        {
            return Parser<T>([=](const Position& _pos, StringStream& _stream)
                {
                    _prefix.Parse(_stream);
                    return _parser.Parse(_stream);
                });
        }

        template<typename T, typename S>
        Parser<T> Suffixed(const Parser<T>& _parser, const Parser<S>& _suffix)
        {
            return Parser<T>([=](const Position& _pos, StringStream& _stream)
                {
                    auto result = _parser.Parse(_stream);
                    _suffix.Parse(_stream);
                    return result;
                });
        }

        template<typename Keep, typename Discard>
        Parser<Keep> operator<<(const Parser<Keep>& _keep, const Parser<Discard>& _discard) { return Suffixed(_keep, _discard); }

        template<typename Discard, typename Keep>
        Parser<Keep> operator>>(const Parser<Discard>& _discard, const Parser<Keep>& _keep) { return Prefixed(_discard, _keep); }

        template<typename T>
        QuantifiedParser<T> operator+(const Parser<T>& _lhs, const Parser<T>& _rhs)
        {
            return QuantifiedParser<T>([=](const Position& _pos, StringStream& _stream)
                {
                    QuantifiedValue<T> results = { _lhs.Parse(_stream), _rhs.Parse(_stream) };

                    Position position = results.size() == 0 ? _pos : results[0].position; //this is separated from the return statement because of the unknown order of argument evaluation 
                    return QuantifiedResult<T>(position, std::move(results));
                });
        }

        template<typename T>
        QuantifiedParser<T> operator+(const QuantifiedParser<T>& _lhs, const Parser<T>& _rhs)
        {
            return QuantifiedParser<T>([=](const Position& _pos, StringStream& _stream)
                {
                    QuantifiedResult<T> result = _lhs.Parse(_stream);
                    result.value.pushback(_rhs.Parse(_stream));

                    return result;
                });
        }

        template<typename T>
        QuantifiedParser<T> operator+(const Parser<T>& _lhs, const QuantifiedParser<T>& _rhs)
        {
            return QuantifiedParser<T>([=](const Position& _pos, StringStream& _stream)
                {
                    QuantifiedValue<T> results = { _lhs.Parse(_stream) };
                    QuantifiedValue<T> rhsResults = _rhs.Parse(_stream).value;

                    results.insert(results.end(), rhsResults.begin(), rhsResults.end());

                    Position position = results[0].position; //this is separated from the return statement because of the unknown order of argument evaluation 
                    return QuantifiedResult<T>(position, std::move(results));
                });
        }

        template<typename T>
        QuantifiedParser<T> operator+(const QuantifiedParser<T>& _lhs, const QuantifiedParser<T>& _rhs)
        {
            return QuantifiedParser<T>([=](const Position& _pos, StringStream& _stream)
                {
                    QuantifiedResult<T> result = _lhs.Parse(_stream);
                    QuantifiedValue<T> rhsResults = _rhs.Parse(_stream).value;

                    result.value.insert(result.value.end(), rhsResults.begin(), rhsResults.end());
                    return result;
                });
        }

        template<typename T>
        Parser<T> operator|(const Parser<T>& _option1, const Parser<T>& _option2)
        {
            return Parser<T>([=](const Position& _pos, StringStream& _stream)
                {
                    size_t streamStart = _stream.GetOffset(), result1Length = 0, result2Length = 0;
                    std::variant<ParseResult<T>, ParseError> result1, result2;

                    try
                    {
                        result1 = _option1.Parse(_stream);
                        result1Length = _stream.GetOffset() - streamStart;
                    }
                    catch (const ParseError& e) { result1 = e; }

                    _stream.SetOffset(streamStart);

                    try
                    {
                        result2 = _option2.Parse(_stream);
                        result2Length = _stream.GetOffset() - streamStart;
                    }
                    catch (const ParseError& e) { result2 = e; }

                    if (result1.index() == 1 && result2.index() == 1) { throw ParseError(_pos, "No option parsed!", { std::get<1>(result1), std::get<1>(result2) }); }
                    else if (result1.index() == 0 && result2.index() == 1)
                    {
                        _stream.SetOffset(streamStart + result1Length);
                        return std::get<0>(result1);
                    }
                    else if (result2.index() == 0 && result1.index() == 1)
                    {
                        _stream.SetOffset(streamStart + result2Length);
                        return std::get<0>(result2);
                    }
                    else if (result1Length >= result2Length)
                    {
                        _stream.SetOffset(streamStart + result1Length);
                        return std::get<0>(result1);
                    }
                    else
                    {
                        _stream.SetOffset(streamStart + result2Length);
                        return std::get<0>(result2);
                    }
                });
        }

        template<typename T>
        Parser<T> Value(const T& _value) { return Parser<T>([=](const Position& _pos, StringStream& _stream) { return ParseResult<T>(_pos, _value); }); }

        template<typename T, typename S>
        QuantifiedParser<T> Separated(const Parser<T>& _parser, const Parser<S>& _sep, size_t _min = 0, size_t _max = SIZE_MAX)
        {
            assert(_max >= _min && "_max must be at least _min");

            if (_max == 0) { return Exactly<T>(_parser, 0); }
            else if (_max == 1)
            {
                if (_min == 1) { return Exactly<T>(_parser, 1); }
                else { return ZeroOrOne<T>(_parser); }
            }
            else
            {
                if (_min == 0)
                {
                    OptionalParser<QuantifiedValue<T>> parser = Optional<QuantifiedValue<T>>(_parser + Quantified<T>(_sep >> _parser, 0, _max - 1));
                    return Mapped<OptionalValue<QuantifiedValue<T>>, QuantifiedValue<T>>(parser, [](const OptionalResult<QuantifiedValue<T>>& _result)
                        {
                            return _result.value.has_value() ? _result.value.value() : QuantifiedValue<T>();
                        });
                }
                else { return _parser + Quantified<T>(_sep >> _parser, _min - 1, _max - 1); }
            }
        }

        template<typename T>
        Parser<T> LookAhead(const Parser<T>& _parser)
        {
            return Parser<T>([=](const Position& _pos, StringStream& _stream)
                {
                    auto result = _parser.Parse(_stream);

                    _stream.SetPosition(_pos);
                    return result;
                });
        }

        template<typename Prefix, typename T, typename Suffix>
        Parser<T> Between(const Parser<Prefix>& _prefix, const Parser<T>& _parser, const Parser<Suffix>& _suffix) { return _prefix >> _parser << _suffix; }

        Parser<std::string> Token(const Lexer& _lexer, const Lexer::PatternID& _id, const std::set<Lexer::PatternID>& _ignores = {}, std::optional<std::string> _value = std::nullopt);
        Parser<std::string> Token(const Regex& _regex, std::optional<std::string> _value = std::nullopt);
        Parser<std::string> Chars(std::optional<std::string> _value = std::nullopt);
        Parser<std::string> Letters(std::optional<std::string> _value = std::nullopt);
        Parser<std::string> Digits(std::optional<std::string> _value = std::nullopt);
        Parser<std::string> AlphaNums(std::optional<std::string> _value = std::nullopt);
        Parser<std::string> Whitespace(std::optional<std::string> _value = std::nullopt);
        Parser<char> Char(std::optional<char> _value = std::nullopt);
        Parser<std::monostate> EOS();
        Parser<std::monostate> Error(const std::string& _message);
    }
}