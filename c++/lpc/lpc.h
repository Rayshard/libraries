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

        bool operator==(const ParseResult<T>& _other) const { return position == _other.position && value == _other.value; }
        bool operator!=(const ParseResult<T>& _other) const { return !(*this == _other); }
    };

    template<typename T>
    struct Parser
    {
        virtual ~Parser() { }

        ParseResult<T> Parse(StringStream& _stream) const
        {
            size_t streamStart = _stream.GetOffset();

            try { return OnParse(_stream.GetPosition(), _stream); }
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

        virtual Parser<T>* Clone() const = 0;

    protected:
        virtual ParseResult<T> OnParse(const Position& _pos, StringStream& _stream) const = 0;
    };

    namespace parsers
    {
        template<typename T>
        struct Function : public Parser<T>
        {
            typedef std::function<ParseResult<T>(const Position& _pos, StringStream& _stream)> Type;
            Type function;

            Function(Type _func) : function(_func) { }

            Parser<T>* Clone() const override { return new Function<T>(function); }

        protected:
            ParseResult<T> OnParse(const Position& _pos, StringStream& _stream) const override { return function(_pos, _stream); };
        };

        namespace lexing
        {
            typedef std::string PatternID;

            inline static PatternID EOS_PATTERN_ID() { return "<EOS>"; }
            inline static PatternID UNKNOWN_PATTERN_ID() { return "<UNKNOWN>"; }

            struct Token
            {
                PatternID patternID;
                std::string value;

                bool IsEOS() const;
                bool IsUnknown() const;
            };

            typedef std::monostate NoAction;
            typedef std::function<void(StringStream&, const ParseResult<Token>&)> Procedure;
            typedef std::function<std::string(StringStream&, const ParseResult<Token>&)> Function;
            typedef std::variant<Procedure, Function, NoAction> Action;

            struct PatternParseValue
            {
                Token token;
                Action action;
            };

            typedef Parser<PatternParseValue> Pattern;

            //     class Lexer
            //     {
            //         std::vector<Pattern> patterns;
            //         std::unordered_map<PatternID, size_t> patternsMap;
            //         Pattern patternEOS, patternUnknown;

            //     public:
            //         Lexer(Action _onEOS = std::monostate(), Action _onUnknown = OnLexUnknown);

            //         PatternID AddPattern(const PatternID& _id, const Regex& _regex, Action _action = NoAction());
            //         PatternID AddPattern(Regex _regex, Action _action = NoAction());

            //         bool HasPattern(const PatternID& _id) const;
            //         Parser<std::string> CreateLexeme(const PatternID& _id, const std::set<PatternID>& _ignores = { }, std::optional<std::string> _value = std::nullopt) const;

            //         ParseResult<Token> OnParse(const Position& _pos, StringStream& _stream);

            //         static void OnLexUnknown(StringStream& _stream, const ParseResult<Token>& _result);

            //     private:
            //         static Pattern CreatePattern(const PatternID& _id, const Regex& _regex, Action _action);
            //     };

            //     template<typename T>
            //     class LPC
            //     {
            //     protected:
            //         Lexer lexer;
            //         std::set<PatternID> ignores;

            //     public:
            //         LPC(Action _onLexEOS = std::monostate(), Action _onLexUnknown = Lexer::OnLexUnknown) : lexer(_onLexEOS, _onLexUnknown), ignores() { }

            //         virtual ~LPC() { }

            //         const Lexer& GetLexer() const { return lexer; }
            //         const std::set<PatternID>& GetIgnores() const { return ignores; }

            //         virtual ParseResult<T> OnParse(const Position& _pos, StringStream& _stream) = 0;
            //     };
        }

        template<typename T>
        class Reference : public Parser<T>
        {
            std::shared_ptr<std::shared_ptr<Parser<T>>> reference;

        public:
            Reference() : reference(new std::shared_ptr<Parser<T>>()) { }

            void Set(const Parser<T>& _parser) { reference->reset(_parser.Clone()); }

            Parser<T>* Clone() const override
            {
                Reference<T>* clone = new Reference<T>();
                clone->reference = reference;

                return clone;
            }

        protected:
            ParseResult<T> OnParse(const Position& _pos, StringStream& _stream) const override { return (*reference)->Parse(_stream); }
        };

        template<typename In, typename Out>
        Function<Out> Map(const Parser<In>& _parser, std::function<Out(ParseResult<In>&&)> _map)
        {
            return Function<Out>([parser = std::shared_ptr<Parser<In>>(_parser.Clone()), _map](const Position& _pos, StringStream& _stream)
            {
                ParseResult<In> input = parser->Parse(_stream);
                Position position = input.position; //this is separated from the return statement because of the unknown order of argument evaluation 

                return ParseResult<Out>(position, _map(std::move(input)));
            });
        }

        template<typename In, typename Out>
        Function<Out> Chain(const Parser<In>& _parser, std::function<Parser<Out>* (ParseResult<In>&&)> _func)
        {
            return Function<Out>([parser = std::shared_ptr<Parser<In>>(_parser.Clone()), _func](const Position& _pos, StringStream& _stream)
            {
                Parser<Out>* nextParser = _func(parser->Parse(_stream));
                ParseResult result = nextParser->Parse(_stream);

                delete nextParser;
                return result;
            });
        }

        template<typename T>
        Function<T> Satisfy(const Parser<T>& _parser, std::function<bool(const ParseResult<T>&)> _predicate, std::function<ParseError(const ParseResult<T>&)> _onFail = nullptr)
        {
            return Function<T>([parser = std::shared_ptr<Parser<T>>(_parser.Clone()), _predicate, _onFail](const Position& _pos, StringStream& _stream)
            {
                ParseResult<T> result = parser->Parse(_stream);

                if (_predicate(result)) { return result; }
                else { throw _onFail ? _onFail(result) : ParseError(_pos, "Predicate not satisfied!"); }
            });
        }

        template<typename T>
        Function<T> Satisfy(const Parser<T>& _parser, const T& _value, std::function<ParseError(const ParseResult<T>&)> _onFail = nullptr)
        {
            return Satisfy<int>(_parser, [=](const ParseResult<T>& _result) { return _result.value == _value; }, _onFail);
        }

        template<typename T>
        Function<T> Success(const Parser<T>& _parser, const T& _default)
        {
            return Function<T>([parser = std::shared_ptr<Parser<T>>(_parser.Clone()), _default](const Position& _pos, StringStream& _stream)
            {
                try { return parser->Parse(_stream); }
                catch (const ParseError& e) { return ParseResult<T>(_pos, _default); }
            });
        }

        template<typename T>
        Function<ParseError> Failure(const Parser<T>& _parser)
        {
            return Function<ParseError>([parser = std::shared_ptr<Parser<T>>(_parser.Clone())](const Position& _pos, StringStream& _stream)
            {
                std::optional<ParseError> error;

                try { parser->Parse(_stream); }
                catch (const ParseError& e) { error = e; }

                if (!error.has_value())
                    throw ParseError(_pos, "Unexpected success!");

                return ParseResult<ParseError>(_pos, error.value());
            });
        }

#pragma region Try
        template<typename T>
        struct TryValue
        {
            std::variant<T, ParseError> variant;

            bool IsSuccess() { return variant.index() == 0; }
            bool IsError() { return variant.index() == 1; }

            T* ExtractSuccess() { return std::get_if<std::variant_alternative_t<0, TryValue<T>>>(&variant); }
            ParseError* ExtractError() { return std::get_if<std::variant_alternative_t<1, TryValue<T>>>(&variant); }
        };

        template<typename T>
        using TryResult = ParseResult<TryValue<T>>;

        template<typename T>
        using TryParser = Function<TryValue<T>>;

        template<typename T>
        TryParser<T> Try(const Parser<T>& _parser)
        {
            return TryParser<T>([parser = std::shared_ptr<Parser<T>>(_parser.Clone())](const Position& _pos, StringStream& _stream)
            {
                try
                {
                    ParseResult<T> result = parser->Parse(_stream);
                    return TryResult<T>(result.position, result.value);
                }
                catch (const ParseError& e) { return TryResult<T>(e.GetPosition(), e); }
            });
        }
#pragma endregion

#pragma region Count
        template<typename T>
        using CountValue = std::vector<ParseResult<T>>;

        template<typename T>
        using CountResult = ParseResult<CountValue<T>>;

        template<typename T>
        using CountParser = Function<CountValue<T>>;

        template<typename T>
        CountParser<T> Count(const Parser<T>& _parser, size_t _min, size_t _max)
        {
            if (_max < _min)
                throw std::runtime_error("_max must be at least _min: " + std::to_string(_max) + " < " + std::to_string(_min));

            return CountParser<T>([parser = std::shared_ptr<Parser<T>>(_parser.Clone()), _min, _max](const Position& _pos, StringStream& _stream)
            {
                CountValue<T> results;

                if (_max != 0)
                {
                    while (results.size() <= _max)
                    {
                        try { results.push_back(parser->Parse(_stream)); }
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

#pragma region List
        template<typename... Ts>
        using ListValue = std::tuple<ParseResult<Ts>...>;

        template<typename... Ts>
        using ListResult = ParseResult<ListValue<Ts...>>;

        template<typename... Ts>
        using ListParser = Function<ListValue<Ts...>>;

        template<typename... Ts>
        struct List : public Parser<ListValue<Ts...>>
        {
            using Parsers = std::tuple<Parser<Ts>*...>;
            Parsers parsers;

        public:
            List(const std::tuple<const Parser<Ts>&...>& _parsers) : parsers(std::apply([](auto &&... _ps) { return Parsers{ _ps.Clone()... }; }, _parsers)) {}

            List(const List<Ts...>& _other)
            {
                Free<sizeof...(Ts) - 1>();
                parsers = std::apply([](auto &&... _ps) { return Parsers{ _ps->Clone()... }; }, _other.parsers);
            }

            ~List() { Free<sizeof...(Ts) - 1>(); }

            List<Ts...>& operator=(const List<Ts...>& _other)
            {
                Free<sizeof...(Ts) - 1>();
                parsers = std::apply([](auto &&... _ps) { return Parsers{ _ps->Clone()... }; }, _other.parsers);
                return *this;
            }

            Parser<ListValue<Ts...>>* Clone() const override { return new List<Ts...>(GetParsers()); }

            std::tuple<const Parser<Ts>&...> GetParsers() const { return std::apply([](auto &&... _ps) { return std::tie(*_ps...); }, parsers); }
        protected:
            ListResult<Ts...> OnParse(const Position& _pos, StringStream& _stream) const override
            {
                ListValue<Ts...> results = std::apply([&](auto &&... _parsers) { return ListValue<Ts...>{_parsers->Parse(_stream)...}; }, parsers);
                Position position = sizeof...(Ts) == 0 ? _pos : std::get<0>(results).position; //this is separated from the return statement because of the unknown order of argument evaluation 

                return ListResult<Ts...>(position, std::move(results));
            }

        private:
            template<size_t I>
            void Free()
            {
                delete std::get<I>(parsers);

                if constexpr (I != 0)
                    Free<I - 1>();
            }
        };

        template<typename T1, typename T2>
        List<T1, T2> operator&(const Parser<T1>& _p1, const Parser<T2>& _p2) { return List<T1, T2>(std::tie(_p1, _p2)); }

        template<typename... Head, typename Appendage>
        List<Head..., Appendage> operator&(const List<Head...>& _head, const Parser<Appendage>& _appendage) { return List<Head..., Appendage>(std::tuple_cat(_head.GetParsers(), std::tie(_appendage))); }

        template<typename Appendage, typename... Tail>
        List<Appendage, Tail...> operator&(const Parser<Appendage>& _appendage, const List<Tail...>& _tail) { return List<Appendage, Tail...>(std::tuple_cat(std::tie(_appendage), _tail.GetParsers())); }

        template<typename... Head, typename... Tail>
        List<Head..., Tail...> operator&(const List<Head...>& _head, const List<Tail...>& _tail) { return List<Head..., Tail...>(std::tuple_cat(_head.GetParsers(), _tail.GetParsers())); }
#pragma endregion

#pragma region Optional
        template<typename T>
        using OptionalValue = std::optional<T>;

        template<typename T>
        using OptionalResult = ParseResult<OptionalValue<T>>;

        template<typename T>
        using OptionalParser = Function<OptionalValue<T>>;

        template<typename T>
        OptionalParser<T> Optional(const Parser<T>& _parser)
        {
            return OptionalParser<T>([parser = std::shared_ptr<Parser<T>>(_parser.Clone())](const Position& _pos, StringStream& _stream)
            {
                OptionalValue<T> value;

                try
                {
                    ParseResult<T> result = parser->Parse(_stream);
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
        using BinopChainCombiner = std::function<ParseResult<T>(ParseResult<T>&& _lhs, ParseResult<Binop<B>>&& _op, ParseResult<T>&& _rhs)>;

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
                chain = _bcc(std::move(chain), std::move(op), std::move(rhs));
            }

            return chain;
        }

        template<typename T, typename B>
        Function<T> BinopChain(const Parser<T>& _atom, const Parser<Binop<B>>& _op, BinopChainCombiner<T, B> _bcc)
        {
            return Function<T>([atom = std::shared_ptr<Parser<T>>(_atom.Clone()), op = std::shared_ptr<Parser<T>>(_op.Clone()), _bcc](const Position& _pos, StringStream& _stream)
            {
                return BinopChainFunc(_stream, *atom, *op, _bcc, 0);
            });
        }
#pragma endregion

#pragma region Fold
        template<typename T, typename F>
        Function<F> Fold(const Parser<T>& _parser, const F& _initial, std::function<void(F&, ParseResult<T>&&)> _func, bool _left)
        {
            return Function<F>([parser = std::shared_ptr<Parser<T>>(_parser.Clone()), _initial, _func, _left](const Position& _pos, StringStream& _stream)
            {
                F value = _initial;
                std::deque<ParseResult<T>> queue;

                //Accumulate elements
                while (true)
                {
                    try { queue.push_back(parser->Parse(_stream)); }
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
#pragma endregion

        template<typename T>
        Function<T> Named(const std::string& _name, const Parser<T>& _parser)
        {
            return Function<T>([_name, parser = std::shared_ptr<Parser<T>>(_parser.Clone())](const Position& _pos, StringStream& _stream)
            {
                try { return parser->Parse(_stream); }
                catch (const ParseError& e) { throw ParseError(ParseError(e.GetPosition(), "Unable to parse " + _name), e); }
            });
        }

        template<typename P, typename T>
        Function<T> Prefixed(const Parser<P>& _prefix, const Parser<T>& _parser)
        {
            return Function<T>([prefix = std::shared_ptr<Parser<P>>(_prefix.Clone()), parser = std::shared_ptr<Parser<T>>(_parser.Clone())](const Position& _pos, StringStream& _stream)
            {
                prefix->Parse(_stream);
                return parser->Parse(_stream);
            });
        }

        template<typename T, typename S>
        Function<T> Suffixed(const Parser<T>& _parser, const Parser<S>& _suffix)
        {
            return Function<T>([parser = std::shared_ptr<Parser<T>>(_parser.Clone()), suffix = std::shared_ptr<Parser<S>>(_suffix.Clone())](const Position& _pos, StringStream& _stream)
            {
                ParseResult<T> result = parser->Parse(_stream);
                suffix->Parse(_stream);
                return result;
            });
        }

        template<typename Keep, typename Discard>
        Function<Keep> operator<<(const Parser<Keep>& _keep, const Parser<Discard>& _discard) { return Suffixed(_keep, _discard); }

        template<typename Discard, typename Keep>
        Function<Keep> operator>>(const Parser<Discard>& _discard, const Parser<Keep>& _keep) { return Prefixed(_discard, _keep); }

        template<typename T>
        CountParser<T> operator+(const Parser<T>& _lhs, const Parser<T>& _rhs)
        {
            return CountParser<T>([lhs = std::shared_ptr<Parser<T>>(_lhs.Clone()), rhs = std::shared_ptr<Parser<T>>(_rhs.Clone())](const Position& _pos, StringStream& _stream)
            {
                CountValue<T> results = { lhs->Parse(_stream), rhs->Parse(_stream) };

                Position position = results.size() == 0 ? _pos : results[0].position; //this is separated from the return statement because of the unknown order of argument evaluation 
                return CountResult<T>(position, std::move(results));
            });
        }

        template<typename T>
        CountParser<T> operator+(const CountParser<T>& _lhs, const Parser<T>& _rhs)
        {
            return CountParser<T>([lhs = _lhs, rhs = std::shared_ptr<Parser<T>>(_rhs.Clone())](const Position& _pos, StringStream& _stream)
            {
                CountResult<T> result = lhs.Parse(_stream);
                result.value.push_back(rhs->Parse(_stream));

                return result;
            });
        }

        template<typename T>
        CountParser<T> operator+(const Parser<T>& _lhs, const CountParser<T>& _rhs)
        {
            return CountParser<T>([lhs = std::shared_ptr<Parser<T>>(_lhs.Clone()), rhs = _rhs](const Position& _pos, StringStream& _stream)
            {
                CountValue<T> results = { lhs->Parse(_stream) };
                CountValue<T> rhsResults = rhs.Parse(_stream).value;

                results.insert(results.end(), rhsResults.begin(), rhsResults.end());

                Position position = results[0].position; //this is separated from the return statement because of the unknown order of argument evaluation 
                return CountResult<T>(position, std::move(results));
            });
        }

        template<typename T>
        CountParser<T> operator+(const CountParser<T>& _lhs, const CountParser<T>& _rhs)
        {
            return CountParser<T>([=](const Position& _pos, StringStream& _stream)
                {
                    CountResult<T> result = _lhs.Parse(_stream);
                    CountValue<T> rhsResults = _rhs.Parse(_stream).value;

                    result.value.insert(result.value.end(), rhsResults.begin(), rhsResults.end());
                    return result;
                });
        }

#pragma region Longest
        enum class ChoiceType { LONGEST, FIRST };

        template<typename T>
        class Longest
        {
            std::vector<Parser<T>*> parsers;
            ChoiceType type;

        public:
            Longest(const std::vector<const Parser<T>&>& _parsers, ChoiceType _type) : parsers(_parsers.size()), type(_type)
            {
                std::transform(_parsers.cbegin(), _parsers.cend(), parsers.begin(), [](const Parser<T>& _parser) { return _parser.Clone(); });
            }

            Longest(const Longest<T>& _other)
            {
                for (auto& parser : parsers)
                    delete parser;

                parsers.resize(_other.parsers.size());
                std::transform(_other.parsers.cbegin(), _other.parsers.cend(), parsers.begin(), [](const Parser<T>& _parser) { return _parser.Clone(); });
            }

            ~Longest()
            {
                for (auto& parser : parsers)
                    delete parser;
            }

            Longest<T>& operator=(const Longest<T>& _other)
            {
                for (auto& parser : parsers)
                    delete parser;

                parsers.resize(_other.parsers.size());
                std::transform(_other.parsers.cbegin(), _other.parsers.cend(), parsers.begin(), [](const Parser<T>& _parser) { return _parser.Clone(); });
                return *this;
            }

            std::vector<const Parser<T>&> GetParsers() const
            {
                std::vector<const Parser<T>&> references(parsers.size());
                std::transform(parsers.cbegin(), parsers.cend(), references.begin(), [](const Parser<T>* _parser) { return *_parser; });

                return references;
            }

            ParseResult<T> OnParse(const Position& _pos, StringStream& _stream)
            {
                size_t streamStart = _stream.GetOffset(), greatestLength = 0;
                std::optional<ParseResult<T>> result;
                std::vector<ParseError> errors;

                for (const Parser<T>* parser : parsers)
                {
                    try
                    {
                        ParseResult<T> parseResult = parser->Parse(_stream);
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
            std::vector<const Parser<T>&> options = _firstOptions.GetParsers();
            options.push_back(_lastOption);
            return Longest<T>(std::move(options));
        }

        template<typename T>
        Longest<T> operator|(const Parser<T>& _firstOption, const Longest<T>& _lastOptions)
        {
            std::vector<const Parser<T>&> options = { _firstOption };
            std::vector<const Parser<T>&> lastParsers = _lastOptions.GetParsers();

            options.insert(options.end(), lastParsers.begin(), lastParsers.end());
            return Longest<T>(std::move(options));
        }

        template<typename T>
        Longest<T> operator|(const Longest<T>& _firstOptions, const Longest<T>& _lastOptions)
        {
            std::vector<const Parser<T>&> options = _firstOptions.GetParsers();
            std::vector<const Parser<T>&> lastParsers = _lastOptions.GetParsers();

            options.insert(options.end(), lastParsers.begin(), lastParsers.end());
            return Longest<T>(std::move(options));
        }
#pragma endregion

#pragma region First
        template<typename T>
        class First
        {
            std::vector<Parser<T>*> parsers;

        public:
            First(const std::vector<Parser<T>>& _parsers) : parsers(_parsers) {}

            ParseResult<T> OnParse(const Position& _pos, StringStream& _stream)
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
        First<T> operator||(const Parser<T>& _option1, const Parser<T>& _option2) { return First<T>({ _option1, _option2 }); }

        template<typename T>
        First<T> operator||(const First<T>& _firstOptions, const Parser<T>& _lastOption)
        {
            std::vector<Parser<T>> options = _firstOptions.parsers;
            options.push_back(_lastOption);
            return First<T>(std::move(options));
        }

        template<typename T>
        First<T> operator||(const Parser<T>& _firstOption, const First<T>& _lastOptions)
        {
            std::vector<Parser<T>> options = { _firstOption };
            const std::vector<Parser<T>>& lastParsers = _lastOptions.parsers;

            options.insert(options.end(), lastParsers.begin(), lastParsers.end());
            return First<T>(std::move(options));
        }

        template<typename T>
        First<T> operator||(const First<T>& _firstOptions, const First<T>& _lastOptions)
        {
            std::vector<Parser<T>> options = _firstOptions.parsers;
            const std::vector<Parser<T>>& lastParsers = _lastOptions.parsers;

            options.insert(options.end(), lastParsers.begin(), lastParsers.end());
            return First<T>(std::move(options));
        }
#pragma endregion

        // #pragma region Variant
        //         template <typename... TRest>
        //         struct unique_types;

        //         template <typename T1, typename T2, typename... TRest>
        //             requires (!std::is_same_v<T1, T2>)
        //         struct unique_types<T1, T2, TRest...>
        //             : unique_types<T1, TRest...>, unique_types<T2, TRest ...> {};

        //         template <typename T1, typename T2>
        //         struct unique_types<T1, T2> : std::integral_constant<bool, !std::is_same_v<T1, T2>> { };

        //         template <typename T>
        //         struct unique_types<T> : std::integral_constant<bool, true> { };

        //         template<typename... Ts>
        //             requires (unique_types<Ts...>::value)
        //         using VariantValue = std::variant<ParseResult<Ts>...>;

        //         template<typename... Ts>
        //             requires (unique_types<Ts...>::value)
        //         using VariantResult = ParseResult<VariantValue<Ts...>>;

        //         template<typename... Ts>
        //             requires (unique_types<Ts...>::value)
        //         struct Variant
        //         {
        //             Parser<VariantValue<Ts...>> parser;

        //             Variant(const Parser<VariantValue<Ts...>>& _parser) : parser(_parser) {}

        //             template<typename T>
        //                 requires (std::is_same_v<T, Ts> || ...)
        //             static Variant<Ts...> Create(const Parser<T>& _parser)
        //             {
        //                 return Map<T, VariantValue<Ts...>>(_parser, [](ParseResult<T>& _result)
        //                     {
        //                         return VariantValue<Ts...>(std::in_place_index<GetIndex<T>()>, _result);
        //                     });
        //             }

        //             template<typename T>
        //                 requires (std::is_same_v<T, Ts> || ...)
        //             static ParseResult<T>* Extract(VariantValue<Ts...>& _value) { return std::get_if<std::variant_alternative_t<GetIndex<T>(), VariantValue<Ts...>>>(&_value); }

        //             VariantResult<Ts...> OnParse(const Position& _pos, StringStream& _stream) { return parser.Parse(_stream); }

        //         private:
        //             template<typename T, std::size_t Idx = sizeof...(Ts) - 1>
        //                 requires (std::is_same_v<T, Ts> || ...)
        //             static constexpr std::size_t GetIndex()
        //             {
        //                 if constexpr (Idx == 0 || std::is_same_v<std::variant_alternative_t<Idx, std::variant<Ts...>>, T>) { return Idx; }
        //                 else { return GetIndex<T, Idx - 1>(); }
        //             }
        //         };
        // #pragma endregion

        template<typename T>
        Function<T> Value(const T& _value) { return Function<T>([=](const Position& _pos, StringStream& _stream) { return ParseResult<T>(_pos, _value); }); }

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
            else
            {
                if (_min == 0)
                {
                    OptionalParser<CountValue<T>> parser = Optional<CountValue<T>>(_parser + Count<T>(_sep >> _parser, 0, _max - 1));
                    return Map<OptionalValue<CountValue<T>>, CountValue<T>>(parser, [](OptionalResult<CountValue<T>>&& _result)
                        {
                            return _result.value.value_or(CountValue<T>());
                        });
                }
                else { return _parser + Count<T>(_sep >> _parser, _min - 1, _max - 1); }
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
        Function<T> Between(const Parser<Prefix>& _prefix, const Parser<T>& _parser, const Parser<Suffix>& _suffix) { return _prefix >> _parser << _suffix; }

        //         Parser<std::string> Lexeme(lexing::Lexer _lexer, const lexing::PatternID& _id, const std::set<lexing::PatternID>& _ignores = {}, std::optional<std::string> _value = std::nullopt);
        Function<std::string> Lexeme(const Regex& _regex, std::optional<std::string> _value = std::nullopt);
        Function<char> Char(std::optional<char> _value = std::nullopt);
        Function<std::string> Chars(std::optional<std::string> _value = std::nullopt);
        Function<char> Letter(std::optional<char> _value = std::nullopt);
        Function<std::string> Letters(std::optional<std::string> _value = std::nullopt);
        Function<char> Digit(std::optional<char> _value = std::nullopt);
        Function<std::string> Digits(std::optional<std::string> _value = std::nullopt);
        Function<char> AlphaNum(std::optional<char> _value = std::nullopt);
        Function<std::string> AlphaNums(std::optional<std::string> _value = std::nullopt);
        Function<char> Whitespace(std::optional<std::string> _value = std::nullopt);
        Function<std::string> Whitespaces(std::optional<std::string> _value = std::nullopt);
        Function<std::monostate> EOS();
        Function<std::monostate> Error(const std::string& _message);
    }
}