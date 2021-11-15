#include "parser.h"
#include <iostream>
#include <sstream>
#include <any>
#include <assert.h>
#include <unordered_map>
#include <unordered_set>

namespace parser
{
    template<typename Unit, class TStream, typename PatternTemplate, typename Match>
        requires(std::is_base_of<IStream<Unit>, TStream>::value)
    class PatternMatcher
    {
    public:
        typedef size_t PatternID;
        inline static constexpr PatternID EOS_PATTERN_ID() { return 0; }
        inline static constexpr PatternID UNKNOWN_PATTERN_ID() { return 1; }

        typedef void(*Procedure)(TStream&, const Match&);
        typedef std::any(*Function)(TStream&, const Match&);
        typedef std::variant<Procedure, Function, std::monostate> Action;

        struct Result
        {
            PatternID patternID;
            std::any value;
            Match match;

            bool IsEOS() { return patternID == EOS_PATTERN_ID(); }
            bool IsUnknown() { return patternID == UNKNOWN_PATTERN_ID(); }
        };

    private:
        struct Pattern
        {
            PatternID id;
            PatternTemplate pTemplate;
            Action action;
        };

        std::vector<Pattern> patterns;
        Pattern eos, unknown;

    public:
        PatternMatcher(Action _onEOS, Action _onUnknown) : patterns()
        {
            eos = { .id = EOS_PATTERN_ID(), .pTemplate = PatternTemplate(), .action = _onEOS };
            unknown = { .id = UNKNOWN_PATTERN_ID(), .pTemplate = PatternTemplate(), .action = _onUnknown };
        }

        PatternID AddPattern(PatternTemplate _template, Action _action = std::monostate())
        {
            patterns.push_back(Pattern{ .id = patterns.size() + 2, .pTemplate = _template, .action = _action });
            return patterns.back().id;
        }

        Result GetMatch(TStream& _stream)
        {
            Pattern* matchingPattern = nullptr;
            size_t streamStart = _stream.offset;
            Match match;

            if (_stream.IsEOS())
            {
                matchingPattern = &eos;
                match = MatchEOS(_stream);
            }
            else
            {
                size_t greatestPatternMatchLength = 0;

                for (auto& pattern : patterns)
                {
                    auto patternMatch = MatchPatternTemplate(_stream, pattern.pTemplate); //Call to this function should update the stream position past the match
                    size_t patternMatchLength = _stream.offset - streamStart;

                    _stream.offset = streamStart; //Reset the stream's position to before match so that possible future calls to the matcher start at the same offset

                    if (!patternMatch.has_value())
                        continue;

                    //If this is the first match or this match has a strictly greater length than
                    //any previous match, set it as the match 
                    if (!matchingPattern || patternMatchLength > greatestPatternMatchLength)
                    {
                        matchingPattern = &pattern;
                        match = patternMatch.value();
                        greatestPatternMatchLength = patternMatchLength;
                    }
                }

                if (!matchingPattern)
                {
                    matchingPattern = &unknown;
                    match = MatchUnknown(_stream);
                }
                else { _stream.Ignore(greatestPatternMatchLength); }
            }

            Result result = {
                .patternID = matchingPattern->id,
                .value = std::any(),
                .match = match
            };

            if (auto action = std::get_if<Function>(&matchingPattern->action)) { result.value = (*action)(_stream, match); }
            else if (auto action = std::get_if<Procedure>(&matchingPattern->action)) { (*action)(_stream, match); }

            return result;
        }

        const std::vector<Pattern>& GetPatterns() const { return patterns; }

    private:
        virtual Match MatchEOS(TStream& _stream) = 0;
        virtual Match MatchUnknown(TStream& _stream) = 0;
        virtual std::optional<Match> MatchPatternTemplate(TStream&, const PatternTemplate&) = 0;
    };

    struct LexerMatch
    {
        Position position;
        std::string value;
    };

    class Lexer : public PatternMatcher<char, Stream, Regex, LexerMatch>
    {
    public:
        Lexer(Action _onEOS, Action _onUnknown) : PatternMatcher<char, Stream, Regex, LexerMatch>(_onEOS, _onUnknown) { }

    private:
        LexerMatch MatchEOS(Stream& _stream) override { return LexerMatch{ .position = _stream.GetPosition(), .value = std::string(1, (char)EOF) }; }
        LexerMatch MatchUnknown(Stream& _stream) override { return LexerMatch{ .position = _stream.GetPosition(), .value = std::string(1, _stream.Get()) }; }

        std::optional<LexerMatch> MatchPatternTemplate(Stream& _stream, const Regex& _regex) override
        {
            const char* current = &*_stream.CCurrent();
            const char* end = &*_stream.CEnd();
            std::cmatch regexMatch;
            std::regex_search(current, end, regexMatch, _regex, std::regex_constants::match_continuous);

            if (regexMatch.size() == 0)
                return std::optional<LexerMatch>();

            _stream.Ignore(regexMatch.length()); //Advance stream past match
            return LexerMatch{ .position = _stream.GetPosition(), .value = regexMatch.str() };
        }
    };

    class TokenStream : public IStream<Lexer::Result>
    {
        Lexer* lexer;
        Stream* ss;
    public:
        TokenStream(Lexer* _lexer, Stream* _ss) : IStream({ _lexer->GetMatch(*_ss) }), lexer(_lexer), ss(_ss) { }

        Lexer::Result Peek() override
        {
            assert(!data.empty() && "A token stream should be non-empty, containing at least the EOS token!");

            while (!data.back().IsEOS() && offset >= data.size() - 1)
                data.push_back(lexer->GetMatch(*ss));

            return IStream<Lexer::Result>::Peek();
        }

        Position GetPosition() { return Peek().match.position; }
    };

    class Parser
    {
    public:
        typedef std::string SymbolID;

        struct Result;

        typedef Lexer::PatternID Terminal;
        typedef std::monostate AnyTerminal;
        typedef std::variant<SymbolID, AnyTerminal> Component;
        typedef std::vector<Component> Rule;
        typedef LexerMatch TerminalMatch;
        typedef std::vector<Parser::Result> NTMatch;

        struct Result
        {
            SymbolID id;
            Position position;
            std::any value;
            std::variant<TerminalMatch, NTMatch> match;
        };

        class NonTerminal : public PatternMatcher<Lexer::Result, TokenStream, Rule, NTMatch>
        {
        public:
            Parser* parser;

            NonTerminal(Parser* _parser) : PatternMatcher<Lexer::Result, TokenStream, Rule, NTMatch>(std::monostate(), std::monostate()), parser(_parser) { }

        private:
            NTMatch MatchEOS(TokenStream& _stream) override { return { }; }

            NTMatch MatchUnknown(TokenStream& _stream) override
            {
                auto token = _stream.Get();
                return { Parser::Result{.position = token.match.position, .value = token.value } };
            }

            std::optional<NTMatch> MatchPatternTemplate(TokenStream& _stream, const Rule& _rule) override
            {
                NTMatch match;

                for (auto& component : _rule)
                {
                    try
                    {
                        if (auto symbolID = std::get_if<SymbolID>(&component)) { match.push_back(parser->ParseSymbol(_stream, *symbolID)); }
                        else { match.push_back(parser->ParseTerminal(_stream, {})); }
                    }
                    catch (const std::runtime_error& e) { return std::optional<NTMatch>(); }
                }

                return match;
            }
        };

    private:
        typedef std::variant<Terminal, NonTerminal> Symbol;
        std::unordered_map<SymbolID, Symbol> symbols;
        std::unordered_map<Terminal, SymbolID> terminals;

        bool validated;
    public:
        Lexer lexer;

        Parser(Lexer::Action _onEOS, Lexer::Action _onUnknown, SymbolID _invalidTerminalID) : lexer(_onEOS, _onUnknown), validated(true)
        {
            AddTerminal(_invalidTerminalID, Lexer::UNKNOWN_PATTERN_ID());
        }
    private:
        void AddSymbol(SymbolID _id, Symbol _symbol)
        {
            if (_id.empty()) { throw std::runtime_error("Symbol id must be non-empty!"); }
            else if (!symbols.emplace(_id, _symbol).second) { throw std::runtime_error("Symbol ID '" + _id + "' is already in use."); }

            if (auto terminal = std::get_if<Terminal>(&_symbol))
                terminals.emplace(*terminal, _id);
        }

        Result ParseTerminal(TokenStream& _stream, std::optional<SymbolID> _terminalSymbolID)
        {
            size_t parseStart = _stream.offset;
            Lexer::Result token;

            while (true)
            {
                token = _stream.Get();

                auto terminalSearch = terminals.find(token.patternID);
                if (terminalSearch == terminals.end())
                {
                    if (token.IsEOS())
                    {
                        _stream.offset = parseStart;
                        throw std::runtime_error("Expected " + _terminalSymbolID.value_or("any terminal") + " but lexer reached end of file.");
                    }
                    else { continue; }
                }

                if (!_terminalSymbolID.has_value() || terminalSearch->second == _terminalSymbolID.value())
                    return Result{ .id = terminalSearch->second, .position = token.match.position, .value = token.value, .match = token.match };

                _stream.offset = parseStart;
                throw std::runtime_error("Expected " + _terminalSymbolID.value() + " but found '" + terminalSearch->second + "'");
            }
        }

        Result ParseNonTerminal(TokenStream& _stream, SymbolID _ntSymbolID)
        {
            NonTerminal& nt = std::get<NonTerminal>(symbols.at(_ntSymbolID));
            Position position = _stream.GetPosition();
            auto matchedRule = nt.GetMatch(_stream);

            switch (matchedRule.patternID)
            {
            case NonTerminal::EOS_PATTERN_ID(): throw std::runtime_error("Unable to parse '" + _ntSymbolID + "'. Encountered end of stream!");
            case NonTerminal::UNKNOWN_PATTERN_ID(): throw std::runtime_error("Unable to parse '" + _ntSymbolID + "'. No rule matches the stream's tokens.");
            default: return Result{ .id = _ntSymbolID, .position = position, .value = matchedRule.value, .match = matchedRule.match };
            }
        }

        Result ParseSymbol(TokenStream& _stream, SymbolID _symbolID)
        {
            auto symbolSearch = symbols.find(_symbolID);
            if (symbolSearch == symbols.end())
                throw std::runtime_error("Symbol with id '" + _symbolID + "' does not exist!");

            return std::get_if<Terminal>(&symbolSearch->second) ? ParseTerminal(_stream, _symbolID) : ParseNonTerminal(_stream, _symbolID);
        }

    public:
        void AddTerminal(SymbolID _id, Lexer::PatternID _patternID) { AddSymbol(_id, _patternID); }
        void AddTerminal(SymbolID _id, Regex _regex, Lexer::Action _action = std::monostate()) { AddSymbol(_id, lexer.AddPattern(_regex, _action)); }

        void AddRule(SymbolID _ntID, std::initializer_list<Component> _components, NonTerminal::Action _action = std::monostate())
        {
            auto symbolSearch = symbols.find(_ntID);
            if (symbolSearch == symbols.end()) { AddSymbol(_ntID, NonTerminal(this)); }
            else if (std::get_if<Terminal>(&symbolSearch->second)) { throw std::runtime_error("'" + _ntID + "' refers to a terminal. Rules can only be added to non-terminals!"); }

            std::get<NonTerminal>(symbols.at(_ntID)).AddPattern(_components, _action);
            validated = false;
        }

        void Validate()
        {
            for (auto& [symbolID, symbol] : symbols)
            {
                if (auto nt = std::get_if<NonTerminal>(&symbol))
                {
                    for (auto& rule : nt->GetPatterns())
                    {
                        for (auto& component : rule.pTemplate)
                        {
                            if (auto componentAsSymbolID = std::get_if<SymbolID>(&component))
                            {
                                auto symbolSearch = symbols.find(*componentAsSymbolID);
                                if (symbolSearch == symbols.end())
                                    throw std::runtime_error("Nonterminal '" + symbolID + "' references a symbol that does not exist: '" + *componentAsSymbolID + "'");
                            }
                        }
                    }
                }
            }

            validated = true;
        }

        Result Parse(Stream& _stream, SymbolID _symbolID)
        {
            if (!validated)
                Validate();

            size_t streamStart = _stream.offset;

            try
            {
                TokenStream tStream(&lexer, &_stream);
                return ParseSymbol(tStream, _symbolID);
            }
            catch (const std::runtime_error& e)
            {
                _stream.offset = streamStart;
                throw e;
            }
        }
    };
}

using namespace parser;

enum class TokenType
{
    ID,
    NUM,
    KW_LET,
    SYM_EQ,
    SYM_SEMICOLON,
    INVALID,
    END_OF_FILE
};

class Token
{
    TokenType type;
    std::any value;
    Position position;

    Token(TokenType _type, Position _pos) : type(_type), value(), position(_pos) {}
    Token(TokenType _type, std::any _value, Position _pos) : type(_type), value(_value), position(_pos) {}

public:
    const TokenType& GetType() const { return type; }
    const Position& GetPosition() const { return position; }

    template<typename T>
    const T GetValue() const { return std::any_cast<T>(value); }

    static Token END_OF_FILE(Position _pos) { return Token(TokenType::END_OF_FILE, _pos); }
    static Token INVALID(const std::string& _text, Position _pos) { return Token(TokenType::INVALID, _text, _pos); }
    static Token ID(const std::string& _id, Position _pos) { return Token(TokenType::ID, _id, _pos); }
    static Token NUM(double _value, Position _pos) { return Token(TokenType::NUM, _value, _pos); }
    static Token KW_LET(Position _pos) { return Token(TokenType::KW_LET, _pos); }
    static Token SYM_EQ(Position _pos) { return Token(TokenType::SYM_EQ, _pos); }
    static Token SYM_SEMICOLON(Position _pos) { return Token(TokenType::SYM_SEMICOLON, _pos); }

};

int main()
{
    std::ifstream file("test.txt");
    Stream input(file);

    auto eosAction = [](Stream& _stream, const LexerMatch& _match) { return std::any(Token::END_OF_FILE(_match.position)); };
    auto unknownAction = [](Stream& _stream, const LexerMatch& _match) { return std::any(Token::INVALID(_match.value, _match.position)); };

    Parser parser(eosAction, unknownAction, "<INVALID>");
    parser.lexer.AddPattern(Regex("\\s"));
    parser.lexer.AddPattern(Regex(":"), [](Stream& _stream, const LexerMatch& _match) { std::cout << "I spot a colon!" << std::endl; });
    parser.AddTerminal("let", Regex("let"), [](Stream& _stream, const LexerMatch& _match) { return std::any(Token::KW_LET(_match.position)); });
    parser.AddTerminal("=", Regex("="), [](Stream& _stream, const LexerMatch& _match) { return std::any(Token::SYM_EQ(_match.position)); });
    parser.AddTerminal(";", Regex(";"), [](Stream& _stream, const LexerMatch& _match) { return std::any(Token::SYM_SEMICOLON(_match.position)); });
    parser.AddTerminal("ID", Regex("(_|[a-zA-Z])(_|[a-zA-Z0-9])*"), [](Stream& _stream, const LexerMatch& _match) { return std::any(Token::ID(_match.value, _match.position)); });
    parser.AddTerminal("NUM", Regex("-?(0|[1-9][0-9]*)([.][0-9]+)?"), [](Stream& _stream, const LexerMatch& _match) { return std::any(Token::NUM(std::stod(_match.value), _match.position)); });
    parser.AddRule("DECL", { "let", "ID", "=", "EXPR", ";" }, [](TokenStream& _stream, const Parser::NTMatch& _match) { std::cout << "Found a declaration!" << std::endl; });
    parser.AddRule("DECL", { "let", "ID", "=", ";" }, [](TokenStream& _stream, const Parser::NTMatch& _match) { std::cout << "Found a declaration without expr!" << std::endl; });
    parser.AddRule("DECL", { "let", "ID", "=", Parser::AnyTerminal(), ";" }, [](TokenStream& _stream, const Parser::NTMatch& _match)
        {
            auto error = _match[3];
            std::cout << error.position << " Expected EXPR but found '" << std::get<Parser::TerminalMatch>(error.match).value << "'" << std::endl;
        });
    parser.AddRule("EXPR", { "NUM" }, [](TokenStream& _stream, const Parser::NTMatch& _match) { return _match[0].value; });
    parser.AddRule("EXPR", { "ID" }, [](TokenStream& _stream, const Parser::NTMatch& _match) { return _match[0].value; });

    try
    {
        parser.Parse(input, "DECL");
    }
    catch (const std::runtime_error& e)
    {
        std::cerr << e.what() << '\n';
    }

    return 0;
}