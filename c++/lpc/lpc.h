#pragma once
#include <string>
#include <regex>
#include <variant>
#include <optional>
#include <vector>
#include <set>
#include <functional>
#include <cassert>
#include <deque>

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

    template<typename T>
    struct Parser;

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
        std::unordered_map<PatternID, size_t> patternsMap;
        Pattern patternEOS, patternUnknown;

    public:
        Lexer(Action _onEOS = std::monostate(), Action _onUnknown = OnLexUnknown);

        const Pattern& AddPattern(PatternID _id, Regex _regex, Action _action = NoAction());
        const Pattern& AddPattern(Regex _regex, Action _action = NoAction());

        Token Lex(StringStream& _stream) const;

        const Pattern& GetPattern(const PatternID& _id) const;
        bool HasPattern(const PatternID& _id) const;

        Parser<std::string> CreateLexeme(const PatternID& _id, const std::set<PatternID>& _ignores = { }, std::optional<std::string> _value = std::nullopt) const;

        static void OnLexUnknown(StringStream& _stream, const Lexer::Token& _token);
    };

    class ParseError : public std::runtime_error
    {
        Position position;
        std::string message;
        std::vector<ParseError> trace;

    public:
        ParseError();
        ParseError(Position _pos, const std::string& _msg, const std::vector<ParseError>& _trace = { });
        ParseError(const ParseError& _e1, const ParseError& _e2);

        std::string GetMessageWithTrace() const;

        const Position& GetPosition() const;
        const std::string& GetMessage() const;
        const std::vector<ParseError>& GetTrace() const;

        static ParseError Expectation(const std::string& _expected, const std::string& _found, const Position& _pos);
    };

    template<typename T>
    struct ParseResult
    {
        Position position;
        T value;

        ParseResult(Position _pos, T _value)
            : position(_pos), value(_value) { }

        ParseResult()
            : ParseResult(Position(), T()) { }
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
            size_t streamStart = _stream.GetOffset();

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
        IParser() : Parser<T>([this](const Position& _pos, StringStream& _stream) { return OnParse(_pos, _stream); }) { }
        virtual ~IParser() { }

    protected:
        virtual Parser<T>::Result OnParse(const Position& _pos, StringStream& _stream) = 0;
    };

    template<typename T>
    class LPC : public IParser<T>, protected Lexer
    {
    protected:
        std::set<Lexer::PatternID> ignores;

    public:
        LPC(Lexer::Action _onLexEOS = std::monostate(), Lexer::Action _onLexUnknown = Lexer::OnLexUnknown) : Lexer(_onLexEOS, _onLexUnknown), ignores() { }

        virtual ~LPC() { }

        const Lexer& GetLexer() const { return *this; }
        const std::set<Lexer::PatternID>& GetIgnores() const { return ignores; }

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
        Parser<Out> Mapped(const Parser<In>& _parser, std::function<Out(ParseResult<In>&)> _map)
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
                    ParseResult<T> result = _parser.Parse(_stream);

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

#pragma region Try
        template<typename T>
        using TryValue = std::variant<T, ParseError>;

        template<typename T>
        using TryResult = ParseResult<TryValue<T>>;

        template<typename T>
        class TryParser : public IParser<TryValue<T>>
        {
            Parser<T> parser;

        public:
            TryParser(const Parser<T>& _parser) : parser(_parser) {}

            static bool IsSuccess(TryValue<T>& _value) { return _value.index() == 0; }
            static bool IsError(TryValue<T>& _value) { return _value.index() == 1; }

            static T* ExtractSuccess(TryValue<T>& _value) { return std::get_if<std::variant_alternative_t<0, TryValue<T>>>(&_value); }
            static ParseError* ExtractError(TryValue<T>& _value) { return std::get_if<std::variant_alternative_t<1, TryValue<T>>>(&_value); }

        protected:
            TryResult<T> OnParse(const Position& _pos, StringStream& _stream) override
            {
                try
                {
                    ParseResult<T> result = parser.Parse(_stream);
                    return TryResult<T>(result.position, result.value);
                }
                catch (const ParseError& e) { return TryResult<T>(e.GetPosition(), e); }
            }
        };
#pragma endregion

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

                                ParseError error = ParseError::Expectation("at least " + std::to_string(_min), "only " + std::to_string(results.size()), _stream.GetPosition());
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

#pragma region List
        template<typename... Ts>
        using ListValue = std::tuple<ParseResult<Ts>...>;

        template<typename... Ts>
        using ListResult = ParseResult<ListValue<Ts...>>;

        template<typename... Ts>
        class List : public IParser<ListValue<Ts...>>
        {
            std::tuple<Parser<Ts>...> parsers;

        public:
            List(const std::tuple<Parser<Ts>...>& _parsers) : parsers(_parsers) {}

            const std::tuple<Parser<Ts>...> GetParsers() const { return parsers; }

        protected:
            ListResult<Ts...> OnParse(const Position& _pos, StringStream& _stream) override
            {
                auto results = std::apply([&](auto &&... _args) { return std::tuple<ParseResult<Ts>...>{_args.Parse(_stream)...}; }, parsers);
                return ListResult<Ts...>(sizeof...(Ts) == 0 ? _pos : std::get<0>(results).position, results);
            }
        };

        template<typename First, typename Second>
        List<First, Second> operator&(const Parser<First>& _first, const Parser<Second>& _second) { return List<First, Second>({ _first, _second }); }

        template<typename... Head, typename Appendage>
        List<Head..., Appendage> operator&(const List<Head...>& _head, const Parser<Appendage>& _appendage) { return List<Head..., Appendage>(std::tuple_cat(_head.GetParsers(), std::tuple<Parser<Appendage>>{ _appendage })); }

        template<typename Appendage, typename... Tail>
        List<Appendage, Tail...> operator&(const Parser<Appendage>& _appendage, const List<Tail...>& _tail) { return List<Appendage, Tail...>(std::tuple_cat(std::tuple<Parser<Appendage>>{ _appendage }, _tail.GetParsers())); }

        template<typename... Head, typename... Tail>
        List<Head..., Tail...> operator&(const List<Head...>& _head, const List<Tail...>& _tail) { return List<Head..., Tail...>(std::tuple_cat(_head.GetParsers(), _tail.GetParsers())); }
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
                        ParseResult<T> result = _parser.Parse(_stream);
                        return OptionalResult<T>(result.position, result.value);
                    }
                    catch (const ParseError& e) { return OptionalResult<T>(_pos, std::nullopt); }
                });
        }
#pragma endregion

#pragma region BinopChain
        enum class BinopAssociativity { RIGHT, LEFT, NONE };

        template<typename ID>
        struct Binop
        {
            ID id;
            size_t precedence;
            BinopAssociativity associatvity;
        };

        template<typename T, typename B>
        using BinopChainCombiner = std::function<ParseResult<T>(const ParseResult<T>& _lhs, const ParseResult<Binop<B>>& _op, const ParseResult<T>& _rhs)>;

        template<typename T, typename B>
        ParseResult<T> BinopChainFunc(StringStream& _stream, const Parser<T>& _atom, const Parser<Binop<B>>& _op, BinopChainCombiner<T, B> _bcc, size_t _curPrecedence)
        {
            ParseResult<T> chain = _atom.Parse(_stream);

            while (true)
            {
                size_t streamStart = _stream.GetOffset();
                ParseResult<Binop<B>> op;

                try { op = _op.Parse(_stream); }
                catch (const ParseError& e) { break; }

                if (op.value.precedence < _curPrecedence)
                {
                    _stream.SetOffset(streamStart);
                    break;
                }

                ParseResult<T> rhs = BinopChainFunc(_stream, _atom, _op, _bcc, op.value.precedence + (op.value.associatvity == BinopAssociativity::RIGHT ? 0 : 1));
                chain = _bcc(chain, op, rhs);
            }

            return chain;
        }

        template<typename T, typename B>
        Parser<T> BinopChain(const Parser<T>& _atom, const Parser<Binop<B>>& _op, BinopChainCombiner<T, B> _bcc)
        {
            return Parser<T>([=](const Position& _pos, StringStream& _stream) { return BinopChainFunc(_stream, _atom, _op, _bcc, 0); });
        }
#pragma endregion

#pragma region Fold
        template<typename T, typename F>
        Parser<F> Fold(const Parser<T>& _parser, const F& _initial, std::function<void(F&, const ParseResult<T>&)> _func, bool _left)
        {
            return Parser<F>([=](const Position& _pos, StringStream& _stream)
                {
                    F value = _initial;
                    std::deque<ParseResult<T>> queue;

                    //Accumulate elements
                    while (true)
                    {
                        try { queue.push_back(_parser.Parse(_stream)); }
                        catch (const ParseError& e) { break; }
                    }

                    //Fold
                    if (_left)
                    {
                        while (!queue.empty())
                            _func(value, queue.pop_front());
                    }
                    else
                    {
                        while (!queue.empty())
                            _func(value, queue.pop_back());
                    }

                    return ParseResult<F>(_pos, value);
                });
        }

        template<typename T>
        Parser<T> FoldL(const Parser<T>& _parser, const T& _initial, std::function<void(T&, const ParseResult<T>&)> _func) { return Fold<T, T>(_parser, _initial, _func, true); }

        template<typename T>
        Parser<T> FoldR(const Parser<T>& _parser, const T& _initial, std::function<void(T&, const ParseResult<T>&)> _func) { return Fold<T, T>(_parser, _initial, _func, false); }
#pragma endregion

        template<typename T>
        Parser<T> Named(const std::string& _name, const Parser<T>& _parser)
        {
            return Parser<T>([=](const Position& _pos, StringStream& _stream)
                {
                    try { return _parser.Parse(_stream); }
                    catch (const ParseError& e) { throw ParseError(ParseError(e.GetPosition(), "Unable to parse " + _name), e); }
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
                    ParseResult<T> result = _parser.Parse(_stream);
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

#pragma region Choice
        template<typename T>
        class Longest : public IParser<T>
        {
            std::vector<Parser<T>> parsers;

        public:
            Longest(const std::vector<Parser<T>>& _parsers) : parsers(_parsers) {}

            const std::vector<Parser<T>> GetParsers() const { return parsers; }

        protected:
            ParseResult<T> OnParse(const Position& _pos, StringStream& _stream) override
            {
                size_t streamStart = _stream.GetOffset(), greatestLength = 0;
                std::optional<ParseResult<T>> result;
                std::vector<ParseError> errors;

                for (Parser<T>& parser : parsers)
                {
                    try
                    {
                        ParseResult<T> parseResult = parser.Parse(_stream);
                        size_t length = _stream.GetOffset() - streamStart;

                        if (!result.has_value() || length > greatestLength)
                        {
                            result = parseResult;
                            greatestLength = length;
                            errors.clear(); //We put this here to save memory
                        }
                    }
                    catch (const ParseError& e)
                    {
                        if (!result.has_value())
                        {
                            size_t eLength = _stream.GetOffset(e.GetPosition());
                            size_t errorsLength = errors.empty() ? 0 : _stream.GetOffset(errors.back().GetPosition());

                            if (eLength == errorsLength) { errors.push_back(e); }
                            else if (eLength > errorsLength) { errors = { e }; }
                        }
                    }

                    _stream.SetOffset(streamStart);
                }

                if (!result.has_value())
                    throw errors.size() == 1 ? errors.back() : ParseError(_pos, "No option parsed!", errors);

                _stream.SetOffset(streamStart + greatestLength);
                return result.value();
            }
        };

        template<typename T>
        Longest<T> operator|(const Parser<T>& _option1, const Parser<T>& _option2) { return Longest<T>({ _option1, _option2 }); }

        template<typename T>
        Longest<T> operator|(const Longest<T>& _firstOptions, const Parser<T>& _lastOption)
        {
            std::vector<Parser<T>> options = _firstOptions.GetParsers();
            options.push_back(_lastOption);
            return Longest<T>(std::move(options));
        }

        template<typename T>
        Longest<T> operator|(const Parser<T>& _firstOption, const Longest<T>& _lastOptions)
        {
            std::vector<Parser<T>> options = {_firstOption};
            const std::vector<Parser<T>>& lastParsers = _lastOptions.GetParsers();

            options.insert(options.end(), lastParsers.begin(), lastParsers.end());
            return Longest<T>(std::move(options));
        }

        template<typename T>
        Longest<T> operator|(const Longest<T>& _firstOptions, const Longest<T>& _lastOptions)
        {
            std::vector<Parser<T>> options = _firstOptions.GetParsers();
            const std::vector<Parser<T>>& lastParsers = _lastOptions.GetParsers();
            
            options.insert(options.end(), lastParsers.begin(), lastParsers.end());
            return Longest<T>(std::move(options));
        }

        template<typename T>
        class FirstSuccess : public IParser<T>
        {
            std::vector<Parser<T>> parsers;

        public:
            FirstSuccess(const std::vector<Parser<T>>& _parsers) : parsers(_parsers) {}

            const std::vector<Parser<T>> GetParsers() const { return parsers; }

        protected:
            ParseResult<T> OnParse(const Position& _pos, StringStream& _stream) override
            {
                size_t streamStart = _stream.GetOffset();
                std::vector<ParseError> errors;

                for (Parser<T>& parser : parsers)
                {
                    try { return parser.Parse(_stream); }
                    catch (const ParseError& e)
                    {
                        size_t eLength = _stream.GetOffset(e.GetPosition());
                        size_t errorsLength = errors.empty() ? 0 : _stream.GetOffset(errors.back().GetPosition());

                        if (eLength == errorsLength) { errors.push_back(e); }
                        else if (eLength > errorsLength) { errors = { e }; }
                    }

                    _stream.SetOffset(streamStart);
                }

                throw errors.size() == 1 ? errors.back() : ParseError(_pos, "No option parsed!", errors);
            }
        };

        template<typename T>
        FirstSuccess<T> operator||(const Parser<T>& _option1, const Parser<T>& _option2) { return FirstSuccess<T>({ _option1, _option2 }); }

        template<typename T>
        FirstSuccess<T> operator||(const FirstSuccess<T>& _firstOptions, const Parser<T>& _lastOption)
        {
            std::vector<Parser<T>> options = _firstOptions.GetParsers();
            options.push_back(_lastOption);
            return FirstSuccess<T>(std::move(options));
        }

        template<typename T>
        FirstSuccess<T> operator||(const Parser<T>& _firstOption, const FirstSuccess<T>& _lastOptions)
        {
            std::vector<Parser<T>> options = {_firstOption};
            const std::vector<Parser<T>>& lastParsers = _lastOptions.GetParsers();

            options.insert(options.end(), lastParsers.begin(), lastParsers.end());
            return FirstSuccess<T>(std::move(options));
        }

        template<typename T>
        FirstSuccess<T> operator||(const FirstSuccess<T>& _firstOptions, const FirstSuccess<T>& _lastOptions)
        {
            std::vector<Parser<T>> options = _firstOptions.GetParsers();
            const std::vector<Parser<T>>& lastParsers = _lastOptions.GetParsers();
            
            options.insert(options.end(), lastParsers.begin(), lastParsers.end());
            return FirstSuccess<T>(std::move(options));
        }
#pragma endregion

#pragma region Variant
        template <typename... TRest>
        struct unique_types;

        template <typename T1, typename T2, typename... TRest>
            requires (!std::is_same_v<T1, T2>)
        struct unique_types<T1, T2, TRest...>
            : unique_types<T1, TRest...>, unique_types<T2, TRest ...> {};

        template <typename T1, typename T2>
        struct unique_types<T1, T2> : std::integral_constant<bool, !std::is_same_v<T1, T2>> { };

        template <typename T>
        struct unique_types<T> : std::integral_constant<bool, true> { };

        template<typename... Ts>
            requires (unique_types<Ts...>::value)
        using VariantValue = std::variant<ParseResult<Ts>...>;

        template<typename... Ts>
            requires (unique_types<Ts...>::value)
        using VariantResult = ParseResult<VariantValue<Ts...>>;

        template<typename... Ts>
            requires (unique_types<Ts...>::value)
        class VariantParser : public IParser<VariantValue<Ts...>>
        {
            Parser<VariantValue<Ts...>> parser;

            VariantParser(const Parser<VariantValue<Ts...>>& _parser) : parser(_parser) {}

            template<typename T, std::size_t Idx = sizeof...(Ts) - 1>
                requires (std::is_same_v<T, Ts> || ...)
            static constexpr std::size_t GetIndex()
            {
                if constexpr (Idx == 0 || std::is_same_v<std::variant_alternative_t<Idx, std::variant<Ts...>>, T>) { return Idx; }
                else { return GetIndex<T, Idx - 1>(); }
            }

        public:
            template<typename T>
                requires (std::is_same_v<T, Ts> || ...)
            static VariantParser<Ts...> Create(const Parser<T>& _parser)
            {
                return Mapped<T, VariantValue<Ts...>>(_parser, [](ParseResult<T>& _result)
                    {
                        return VariantValue<Ts...>(std::in_place_index<GetIndex<T>()>, _result);
                    });
            }

            template<typename T>
                requires (std::is_same_v<T, Ts> || ...)
            static ParseResult<T>* Extract(VariantValue<Ts...>& _value) { return std::get_if<std::variant_alternative_t<GetIndex<T>(), VariantValue<Ts...>>>(&_value); }

        protected:
            VariantResult<Ts...> OnParse(const Position& _pos, StringStream& _stream) override { return parser.Parse(_stream); }
        };
#pragma endregion

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
                    return Mapped<OptionalValue<QuantifiedValue<T>>, QuantifiedValue<T>>(parser, [](OptionalResult<QuantifiedValue<T>>& _result)
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
                    ParseResult<T> result = _parser.Parse(_stream);

                    _stream.SetPosition(_pos);
                    return result;
                });
        }

        template<typename Prefix, typename T, typename Suffix>
        Parser<T> Between(const Parser<Prefix>& _prefix, const Parser<T>& _parser, const Parser<Suffix>& _suffix) { return _prefix >> _parser << _suffix; }

        Parser<std::string> Lexeme(const Lexer& _lexer, const Lexer::PatternID& _id, const std::set<Lexer::PatternID>& _ignores = {}, std::optional<std::string> _value = std::nullopt);
        Parser<std::string> Lexeme(const Regex& _regex, std::optional<std::string> _value = std::nullopt);
        Parser<char> Char(std::optional<char> _value = std::nullopt);
        Parser<std::string> Chars(std::optional<std::string> _value = std::nullopt);
        Parser<char> Letter(std::optional<char> _value = std::nullopt);
        Parser<std::string> Letters(std::optional<std::string> _value = std::nullopt);
        Parser<char> Digit(std::optional<char> _value = std::nullopt);
        Parser<std::string> Digits(std::optional<std::string> _value = std::nullopt);
        Parser<char> AlphaNum(std::optional<char> _value = std::nullopt);
        Parser<std::string> AlphaNums(std::optional<std::string> _value = std::nullopt);
        Parser<char> Whitespace(std::optional<std::string> _value = std::nullopt);
        Parser<std::string> Whitespaces(std::optional<std::string> _value = std::nullopt);
        Parser<std::monostate> EOS();
        Parser<std::monostate> Error(const std::string& _message);
    }
}