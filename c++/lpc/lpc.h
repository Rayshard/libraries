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

        bool operator==(const Position& _other) const;
        bool operator!=(const Position& _other) const;

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

        char Get();
        char Peek();
        void Ignore(size_t _amt);

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

        bool operator==(const ParseError& _other) const;
        bool operator!=(const ParseError& _other) const;

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

        bool operator==(const ParseResult<T>& _other) const { return position == _other.position && value == _other.value; }
        bool operator!=(const ParseResult<T>& _other) const { return !(*this == _other); }
    };

    template<typename T>
    class Parser final
    {
        using Function = std::function<ParseResult<T>(const Position& _pos, StringStream& _stream)>;
        std::shared_ptr<Function> function;

    public:
        Parser()
            : function(new Function(nullptr)) { }

        template<typename Parsable>
        Parser(const Parsable& _parsable)
            : function(new Function(_parsable)) { }

        // template<typename Parsable>
        // Parser(std::shared_ptr<Parsable> _parsable)
        //     : function(std::make_shared<Function>([_parsable](const Position& _pos, StringStream& _stream) { return (*_parsable)(_pos, _stream); })) { }

        ParseResult<T> Parse(StringStream& _stream) const
        {
            size_t streamStart = _stream.GetOffset();

            try { return (*function)(_stream.GetPosition(), _stream); }
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

    namespace parsers
    {
        template<typename In, typename Out>
        Parser<Out> Map(const Parser<In>& _parser, std::function<Out(ParseResult<In>&&)> _map)
        {
            return Parser<Out>([=](const Position& _pos, StringStream& _stream)
                {
                    ParseResult<In> input = _parser.Parse(_stream);
                    Position position = input.position; //this is separated from the return statement because of the unknown order of argument evaluation 

                    return ParseResult<Out>(position, _map(std::move(input)));
                });
        }

        template<typename T>
        class Reference
        {
            std::shared_ptr<std::shared_ptr<Parser<T>>> reference;

        public:
            Reference() : reference(new std::shared_ptr<Parser<T>>(nullptr)) { }

            void Set(const Parser<T>& _parser) { (*reference).reset(new Parser<T>(_parser)); }

            ParseResult<T> operator() (const Position& _pos, StringStream& _stream) { return (*reference)->Parse(_stream); }
        };

#pragma region Try
        template<typename T>
        struct TryValue
        {
            using Variant = std::variant<T, ParseError>;
            Variant variant;

            TryValue() : variant() { }
            TryValue(const Variant& _t) : variant(_t) { }

            bool IsSuccess() { return variant.index() == 0; }
            bool IsError() { return variant.index() == 1; }

            T* ExtractSuccess() { return std::get_if<std::variant_alternative_t<0, Variant>>(&variant); }
            ParseError* ExtractError() { return std::get_if<std::variant_alternative_t<1, Variant>>(&variant); }

            static TryValue<T> CreateSuccess(const T& _value) { return TryValue<T>(Variant(std::in_place_index_t<0>(), _value)); }
            static TryValue<T> CreateError(const ParseError& _e) { return TryValue<T>(Variant(std::in_place_index_t<1>(), _e)); }
        };

        template<typename T>
        using TryResult = ParseResult<TryValue<T>>;

        template<typename T>
        using TryParser = Parser<TryValue<T>>;

        template<typename T>
        TryParser<T> Try(const Parser<T>& _parser)
        {
            return TryParser<T>([=](const Position& _pos, StringStream& _stream)
                {
                    try
                    {
                        ParseResult<T> result = _parser.Parse(_stream);
                        return TryResult<T>(result.position, TryValue<T>::CreateSuccess(result.value));
                    }
                    catch (const ParseError& e) { return TryResult<T>(e.GetPosition(), TryValue<T>::CreateError(e)); }
                });
        }
#pragma endregion

#pragma region Count
        template<typename T>
        using CountValue = std::vector<ParseResult<T>>;

        template<typename T>
        using CountResult = ParseResult<CountValue<T>>;

        template<typename T>
        using CountParser = Parser<CountValue<T>>;

        template<typename T>
        CountParser<T> Count(const Parser<T>& _parser, size_t _min, size_t _max)
        {
            if (_max < _min)
                throw std::runtime_error("_max must be at least _min: " + std::to_string(_max) + " < " + std::to_string(_min));

            return CountParser<T>([=](const Position& _pos, StringStream& _stream)
                {
                    CountValue<T> results;

                    if (_max != 0)
                    {
                        while (results.size() < _max)
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
                    return CountResult<T>(position, std::move(results));
                });
        }

        template<typename T>
        CountParser<T> ManyOrOne(const Parser<T>& _parser) { return Count<T>(_parser, 1, -1); }

        template<typename T>
        CountParser<T> ZeroOrOne(const Parser<T>& _parser) { return Count<T>(_parser, 0, 1); }

        template<typename T>
        CountParser<T> ZeroOrMore(const Parser<T>& _parser) { return Count<T>(_parser, 0, -1); }

        template<typename T>
        CountParser<T> Exactly(const Parser<T>& _parser, size_t _n) { return Count<T>(_parser, _n, _n); }
#pragma endregion

#pragma region Seq
        template<typename... Ts>
        using SeqValue = std::tuple<ParseResult<Ts>...>;

        template<typename... Ts>
        using SeqResult = ParseResult<SeqValue<Ts...>>;

        template<typename... Ts>
        using SeqParser = Parser<SeqValue<Ts...>>;

        template<typename... Ts>
        SeqParser<Ts...> Seq(Parser<Ts>... _parsers)
        {
            return SeqParser<Ts...>([parsers = std::make_tuple(_parsers...)](const Position& _pos, StringStream& _stream)
            {
                SeqValue<Ts...> results = std::apply([&](auto &&... _parsers) { return SeqValue<Ts...>{_parsers.Parse(_stream)...}; }, parsers);
                Position position = sizeof...(Ts) == 0 ? _pos : std::get<0>(results).position; //this is separated from the return statement because of the unknown order of argument evaluation 

                return SeqResult<Ts...>(position, std::move(results));
            });
        }
#pragma endregion

#pragma region Optional
        template<typename T>
        using OptionalValue = std::optional<T>;

        template<typename T>
        using OptionalResult = ParseResult<OptionalValue<T>>;

        template<typename T>
        using OptionalParser = Parser<OptionalValue<T>>;

        template<typename T>
        OptionalParser<T> Optional(const Parser<T>& _parser)
        {
            return OptionalParser<T>([=](const Position& _pos, StringStream& _stream)
                {
                    try
                    {
                        ParseResult<T> result = _parser.Parse(_stream);
                        return OptionalResult<T>(result.position, OptionalValue<T>(result.value));
                    }
                    catch (const ParseError& e) { return OptionalResult<T>(_pos, std::nullopt); }
                });
        }
#pragma endregion

        // #pragma region BinopChain
        //         enum class BinopAssociativity { RIGHT, LEFT, NONE };

        //         template<typename ID>
        //         struct Binop
        //         {
        //             ID id;
        //             size_t precedence;
        //             BinopAssociativity associatvity;
        //         };

        //         template<typename T, typename B>
        //         using BinopChainCombiner = std::function<ParseResult<T>(ParseResult<T>&& _lhs, ParseResult<Binop<B>>&& _op, ParseResult<T>&& _rhs)>;

        //         template<typename T, typename B>
        //         ParseResult<T> BinopChainFunc(StringStream& _stream, Parser<T> _atom, Parser<Binop<B>> _op, BinopChainCombiner<T, B> _bcc, size_t _curPrecedence)
        //         {
        //             ParseResult<T> chain = _atom.Parse(_stream);

        //             while (true)
        //             {
        //                 size_t streamStart = _stream.GetOffset();
        //                 ParseResult<Binop<B>> op;

        //                 try { op = _op.Parse(_stream); }
        //                 catch (const ParseError& e) { break; }

        //                 if (op.value.precedence < _curPrecedence)
        //                 {
        //                     _stream.SetOffset(streamStart);
        //                     break;
        //                 }

        //                 ParseResult<T> rhs = BinopChainFunc(_stream, _atom, _op, _bcc, op.value.precedence + (op.value.associatvity == BinopAssociativity::RIGHT ? 0 : 1));
        //                 chain = _bcc(std::move(chain), std::move(op), std::move(rhs));
        //             }

        //             return chain;
        //         }

        //         template<typename T, typename B>
        //         Parser<T> BinopChain(Parser<T> _atom, Parser<Binop<B>> _op, BinopChainCombiner<T, B> _bcc)
        //         {
        //             return Parser<T>([=](const Position& _pos, StringStream& _stream)
        //                 {
        //                     return BinopChainFunc(_stream, _atom, _op, _bcc, 0);
        //                 });
        //         }
        // #pragma endregion

        template<typename T>
        Parser<T> Longest(std::initializer_list<Parser<T>> _parsers)
        {
            return Parser<T>([parsers = std::vector<Parser<T>>(_parsers)](const Position& _pos, StringStream& _stream)
            {
                size_t streamStart = _stream.GetOffset(), greatestLength = 0;
                std::optional<ParseResult<T>> result;
                std::vector<ParseError> errors;

                for (const Parser<T>& parser : parsers)
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
            });
        }

        template<typename T>
        Parser<T> FirstSuccess(std::initializer_list<Parser<T>> _parsers)
        {
            return Parser<T>([parsers = std::vector<Parser<T>>(_parsers)](const Position& _pos, StringStream& _stream)
            {
                size_t streamStart = _stream.GetOffset();
                std::vector<ParseError> errors;

                for (const Parser<T>& parser : parsers)
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
            });
        }

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
        class VariantValue
        {
            std::variant<ParseResult<Ts>...> value;

        public:
            template<typename T>
                requires (std::is_same_v<T, Ts> || ...)
            VariantValue(const ParseResult<T>& _value) : value(_value) { }

            template<typename T>
                requires (std::is_same_v<T, Ts> || ...)
            const ParseResult<T>& Extract() const { return *std::get_if<std::variant_alternative_t<GetIndex<T>(), std::variant<ParseResult<Ts>...>>>(&value); }

            template<typename T, std::size_t Idx = sizeof...(Ts) - 1>
                requires (std::is_same_v<T, Ts> || ...)
            static constexpr std::size_t GetIndex()
            {
                if constexpr (Idx == 0 || std::is_same_v<std::variant_alternative_t<Idx, std::variant<Ts...>>, T>) { return Idx; }
                else { return GetIndex<T, Idx - 1>(); }
            }
        };

        template<typename... Ts>
        using VariantResult = ParseResult<VariantValue<Ts...>>;

        template<typename... Ts>
        using VariantParser = Parser<VariantValue<Ts...>>;

        template<typename... Ts>
            requires (unique_types<Ts...>::value)
        class Variant
        {
        public:
            template<typename T>
            static VariantParser<Ts...> Create(const Parser<T>& _parser)
            {
                return Map<T, VariantValue<Ts...>>(_parser, [](ParseResult<T>&& _result) { return VariantValue<Ts...>(_result); });
            }
        };
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
        Parser<T> Value(const T& _value) { return Parser<T>([=](const Position& _pos, StringStream& _stream) { return ParseResult<T>(_pos, _value); }); }

        template<typename T, typename S>
        CountParser<T> Separate(const Parser<T>& _parser, const Parser<S>& _sep, size_t _min = 0, size_t _max = SIZE_MAX)
        {
            if (_max < _min)
                throw std::runtime_error("_max must be at least _min: " + std::to_string(_max) + " < " + std::to_string(_min));

            if (_max == 0) { return Exactly<T>(_parser, 0); }
            else if (_max == 1)
            {
                if (_min == 1) { return Exactly<T>(_parser, 1); }
                else { return ZeroOrOne<T>(_parser); }
            }
            else if (_min == 0)
            {
                CountParser<T> p = Map<SeqValue<T, CountValue<T>>, CountValue<T>>(Seq(_parser, Count<T>(_sep >> _parser, 0, _max - 1)),
                    [](SeqResult<T, CountValue<T>>&& _result)
                    {
                        auto& [a, b] = _result.value;
                        b.position = a.position;
                        b.value.push_front(a);
                        return b;
                    });

                OptionalParser<CountValue<T>> parser = Optional<CountValue<T>>(p);
                return Map<OptionalValue<CountValue<T>>, CountValue<T>>(parser, [](OptionalResult<CountValue<T>>&& _result)
                    {
                        return _result.value.value_or(CountValue<T>());
                    });
            }
            else
            {
                return Map<SeqValue<T, CountValue<T>>, CountValue<T>>(Seq(_parser, Count<T>(_sep >> _parser, _min - 1, _max - 1)),
                    [](SeqResult<T, CountValue<T>>&& _result)
                    {
                        auto& [a, b] = _result.value;
                        b.position = a.position;
                        b.value.push_front(a);
                        return b;
                    });
            }
        }

        template<typename T, typename F>
        Parser<F> Fold(const Parser<T>& _parser, const F& _initial, std::function<void(F&, ParseResult<T>&&)> _func, bool _left)
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
        Parser<T> LookAhead(const Parser<T>& _parser)
        {
            return Parser<T>([=](const Position& _pos, StringStream& _stream)
                {
                    ParseResult<T> result = _parser.Parse(_stream);

                    _stream.SetPosition(_pos);
                    return result;
                });
        }

        template<typename P, typename T, typename S>
        Parser<T> Between(const Parser<P>& _prefix, const Parser<T>& _parser, const Parser<S>& _suffix) { return _prefix >> _parser << _suffix; }

        template<typename In, typename Out>
        Parser<Out> Chain(const Parser<In>& _parser, std::function<Parser<Out>(ParseResult<In>&&)> _func)
        {
            return Parser<Out>([=](const Position& _pos, StringStream& _stream)
                {
                    return _func(_parser.Parse(_stream)).Parse(_stream);
                });
        }

        template<typename T>
        Parser<T> Satisfy(const Parser<T>& _parser, std::function<bool(const ParseResult<T>&)> _predicate, std::function<ParseError(const ParseResult<T>&)> _onFail = nullptr)
        {
            return Parser<T>([=](const Position& _pos, StringStream& _stream)
                {
                    ParseResult<T> result = _parser.Parse(_stream);

                    if (_predicate(result)) { return result; }
                    else { throw _onFail ? _onFail(result) : ParseError(_pos, "Predicate not satisfied!"); }
                });
        }

        template<typename T>
        Parser<T> Satisfy(const Parser<T>& _parser, const T& _value, std::function<ParseError(const ParseResult<T>&)> _onFail = nullptr)
        {
            return Satisfy<T>(_parser, [=](const ParseResult<T>& _result) { return _result.value == _value; }, _onFail);
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
            return Parser<ParseError>([=](const Position& _pos, StringStream& _stream)
                {
                    std::optional<ParseError> error;

                    try { _parser.Parse(_stream); }
                    catch (const ParseError& e) { error = e; }

                    if (!error.has_value())
                        throw ParseError(_pos, "Unexpected success!");

                    return ParseResult<ParseError>(_pos, error.value());
                });
        }

        template<typename T>
        Parser<T> Callback(const Parser<T>& _parser, std::function<void(const ParseResult<T>&)> _func)
        {
            return Parser<T>([=](const Position& _pos, StringStream& _stream)
                {
                    ParseResult<T> result = _parser.Parse(_stream);

                    if (_func)
                        _func(result);

                    return result;
                });
        }

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

        //         namespace lexing
        //         {
        //             typedef std::string PatternID;

        //             inline static PatternID EOS_PATTERN_ID() { return "<EOS>"; }
        //             inline static PatternID UNKNOWN_PATTERN_ID() { return "<UNKNOWN>"; }

        //             struct Token
        //             {
        //                 PatternID patternID;
        //                 std::string value;

        //                 bool IsEOS() const;
        //                 bool IsUnknown() const;
        //             };

        //             typedef std::function<std::string(StringStream&, const ParseResult<Token>&)> Action;

        //             struct Pattern
        //             {
        //                 PatternID patternID;
        //                 Regex regex;
        //                 std::optional<Action> action;
        //             };

        //             // class Pattern : public Parser<Token>
        //             // {
        //             //     PatternID id;
        //             //     Function<Token> parser;

        //             // public:
        //             //     Pattern(const PatternID& _id, const Regex& _regex, const std::set<lexing::PatternID>& _ignores = {}) : id(_id), parser(nullptr)
        //             //     {
        //             //         parser = Named(_id, Function<Token>([_id, _ignores, lexeme=Lexeme(_regex)](const Position& _pos, StringStream& _stream)
        //             //         {
        //             //             ParseResult<std::string> result = parser.Parse(_stream);
        //             //             return ParseResult<Token>(result.position, Token{ .patternID = _id, .value = result.value });
        //             //         }));
        //             //     }

        //             //     void Set(Parser<T> _parser) { reference->reset(_parser.Clone()); }

        //             //     Parser<T>* Clone() const override
        //             //     {
        //             //         Reference<T>* clone = new Reference<T>();
        //             //         clone->reference = reference;

        //             //         return clone;
        //             //     }

        //             // protected:
        //             //     ParseResult<T> OnParse(const Position& _pos, StringStream& _stream) const override { return (*reference).Parse(_stream); }
        //             // };

        //             // class Lexer : public Parser<Token>
        //             // {
        //             //     std::vector<Function<Token>> patterns;
        //             //     std::unordered_map<PatternID, size_t> patternsMap;

        //             // public:
        //             //     Lexer(const std::vector<Pattern>& _patterns) : patterns(), patternsMap()
        //             //     {
        //             //         for (auto& [id, regex, action] : _patterns)
        //             //         {
        //             //             if (id.empty()) { throw std::runtime_error("Pattern's id cannot be empty!"); }
        //             //             else if (patternsMap.contains(id)) { throw std::runtime_error("Duplicate pattern id: '" + id + "' !"); }

        //             //             patternsMap[id] = patterns.size();
        //             //             patterns.push_back(Named(id, Function<Token>([id, lexeme = Lexeme(_regex)](const Position& _pos, StringStream& _stream)
        //             //             {
        //             //                 ParseResult<std::string> result = parser.Parse(_stream);
        //             //                 return ParseResult<Token>(result.position, Token{ .patternID = _id, .value = result.value });
        //             //             })));
        //             //         }
        //             //     }

        //             //     void Set(Parser<T> _parser) { reference->reset(_parser.Clone()); }

        //             //     Parser<T>* Clone() const override
        //             //     {
        //             //         Reference<T>* clone = new Reference<T>();
        //             //         clone->reference = reference;

        //             //         return clone;
        //             //     }

        //             // protected:
        //             //     ParseResult<Token> OnParse(const Position& _pos, StringStream& _stream) const override { return ParseResult<Token>(); }
        //             // };

        //             //     class Lexer
        //             //     {
        //             //         std::vector<Pattern> patterns;
        //             //         std::unordered_map<PatternID, size_t> patternsMap;
        //             //         Pattern patternEOS, patternUnknown;

        //             //     public:
        //             //         Lexer(Action _onEOS = std::monostate(), Action _onUnknown = OnLexUnknown);

        //             //         PatternID AddPattern(const PatternID& _id, const Regex& _regex, Action _action = NoAction());
        //             //         PatternID AddPattern(Regex _regex, Action _action = NoAction());

        //             //         bool HasPattern(const PatternID& _id) const;
        //             //         Parser<std::string> CreateLexeme(const PatternID& _id, const std::set<PatternID>& _ignores = { }, std::optional<std::string> _value = std::nullopt) const;

        //             //         ParseResult<Token> OnParse(const Position& _pos, StringStream& _stream);

        //             //         static void OnLexUnknown(StringStream& _stream, const ParseResult<Token>& _result);

        //             //     private:
        //             //         static Pattern CreatePattern(const PatternID& _id, const Regex& _regex, Action _action);
        //             //     };

        //             //     template<typename T>
        //             //     class LPC
        //             //     {
        //             //     protected:
        //             //         Lexer lexer;
        //             //         std::set<PatternID> ignores;

        //             //     public:
        //             //         LPC(Action _onLexEOS = std::monostate(), Action _onLexUnknown = Lexer::OnLexUnknown) : lexer(_onLexEOS, _onLexUnknown), ignores() { }

        //             //         virtual ~LPC() { }

        //             //         const Lexer& GetLexer() const { return lexer; }
        //             //         const std::set<PatternID>& GetIgnores() const { return ignores; }

        //             //         virtual ParseResult<T> OnParse(const Position& _pos, StringStream& _stream) = 0;
        //             //     };
        //         }

    }
}