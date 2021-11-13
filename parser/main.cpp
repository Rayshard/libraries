#include "parser.h"
#include <iostream>
#include <sstream>
#include <any>
#include <assert.h>
#include <unordered_map>
#include <unordered_set>

namespace parser
{
    class Parser
    {
    public:
        typedef std::string SymbolID;

        struct Result
        {
            Position position;
            std::any value;
        };

        struct Rule
        {
            typedef Result Arg;
            typedef void(*Procedure)(Stream&, const std::vector<Arg>&);
            typedef std::any(*Function)(Stream&, const std::vector<Arg>&);
            typedef std::variant<Procedure, Function, std::monostate> Action;

            std::vector<SymbolID> components;
            Action action;
        };

        typedef Lexer::PatternID Terminal;
        typedef std::vector<Rule> NonTerminal;

    private:
        typedef std::variant<Terminal, NonTerminal> Symbol;
        std::unordered_map<SymbolID, Symbol> symbols;
        std::unordered_map<Terminal, SymbolID> terminals;

        bool validated;
    public:
        Lexer lexer;

        Parser(SymbolID _eofID, Lexer::Action _onEOF, Lexer::Action _onUnknown) : lexer(_onEOF, _onUnknown), validated(true) { }
    private:
        void AddSymbol(SymbolID _id, Symbol _symbol)
        {
            if (_id.empty()) { throw std::runtime_error("Symbol id must be non-empty!"); }
            else if (!symbols.emplace(_id, _symbol).second) { throw std::runtime_error("Symbol ID '" + _id + "' is already in use."); }

            if (auto terminal = std::get_if<Terminal>(&_symbol))
                terminals.emplace(*terminal, _id);
        }

    public:
        void AddTerminal(SymbolID _id, Lexer::PatternID _patternID) { AddSymbol(_id, _patternID); }
        void AddTerminal(SymbolID _id, Regex _regex, Lexer::Action _action = std::monostate()) { AddSymbol(_id, lexer.AddPattern(_regex, _action)); }

        void AddRule(SymbolID _ntID, std::initializer_list<SymbolID> _components, Rule::Action _action = std::monostate())
        {
            auto symbolSearch = symbols.find(_ntID);
            if (symbolSearch == symbols.end()) { AddSymbol(_ntID, NonTerminal()); }
            else if (std::get_if<Terminal>(&symbolSearch->second)) { throw std::runtime_error("'" + _ntID + "' refers to a terminal. Rules can only be added to non-terminals!"); }

            std::get<NonTerminal>(symbols.at(_ntID)).push_back(Rule{ .components = _components, .action = _action }); //Add rule to non-terminal
            validated = false;
        }

        void Validate()
        {
            for (auto& [symbolID, symbol] : symbols)
            {
                if (auto nt = std::get_if<NonTerminal>(&symbol))
                {
                    for (auto& rule : *nt)
                    {
                        for (auto& component : rule.components)
                        {
                            auto symbolSearch = symbols.find(component);
                            if (symbolSearch == symbols.end())
                                throw std::runtime_error("Nonterminal '" + symbolID + "' references a symbol that does not exist: '" + component + "'");
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

            auto symbolSearch = symbols.find(_symbolID);
            if (symbolSearch == symbols.end())
                throw std::runtime_error("Symbol with id '" + _symbolID + "' does not exist!");

            size_t parseStart = _stream.offset;

            if (auto terminal = std::get_if<Terminal>(&symbolSearch->second))
            {
                Lexer::Result peek;

                while (true)
                {
                    peek = lexer.GetMatch(_stream);

                    auto terminalSearch = terminals.find(peek.patternID);
                    if (terminalSearch == terminals.end())
                    {
                        if (peek.patternID == Lexer::EOF_PATTERN_ID())
                        {
                            _stream.offset = parseStart;
                            throw std::runtime_error("Expected '" + _symbolID + "' but lexer reached end of file.");
                        }
                        else { continue; }
                    }

                    if (terminalSearch->second == _symbolID)
                        break;

                    _stream.offset = parseStart;
                    throw std::runtime_error("Expected '" + _symbolID + "' but found '" + terminalSearch->second + "'");
                }

                return Result{ .position = _stream.GetPosition(peek.match.start), .value = peek.value };
            }
            else
            {
                NonTerminal& nt = std::get<NonTerminal>(symbolSearch->second);
                struct { size_t streamOffset; std::vector<SymbolID*> components; } furthestComponentsParsed;
                furthestComponentsParsed.streamOffset = parseStart;

                for (auto& rule : nt)
                {
                    std::vector<Rule::Arg> args;
                    bool parsed = true;

                    for (auto& component : rule.components)
                    {
                        if (_stream.offset > furthestComponentsParsed.streamOffset) { furthestComponentsParsed = { .streamOffset = _stream.offset, .components = { &component } }; }
                        else if (_stream.offset == furthestComponentsParsed.streamOffset || _stream.offset == parseStart) { furthestComponentsParsed.components.push_back(&component); }

                        try { args.push_back(Parse(_stream, component)); }
                        catch (const std::runtime_error& e)
                        {
                            parsed = false;
                            break;
                        }
                    }

                    if (!parsed)
                    {
                        _stream.offset = parseStart;
                        continue;
                    }

                    Result result = {
                        .position = args.size() == 0 ? _stream.GetPosition(parseStart) : args[0].position,
                        .value = std::any()
                    };

                    if (auto procedure = std::get_if<Rule::Procedure>(&rule.action)) { (*procedure)(_stream, args); }
                    else if (auto function = std::get_if<Rule::Function>(&rule.action)) { result.value = (*function)(_stream, args); }

                    return result;
                }

                //None of the rules were parsable
                _stream.offset = parseStart;
                
                //Throw error
                std::stringstream errorSS;
                errorSS << "Unable to parse '" << _symbolID << "'. Expected one of { ";

                std::unordered_set<std::string*> expecteds(furthestComponentsParsed.components.begin(), furthestComponentsParsed.components.end());
                for (auto& expected : expecteds)
                    errorSS << *expected << ",";

                if(!expecteds.empty())
                    errorSS.get(); //Remove trailing comma

                errorSS << " }.";
                throw std::runtime_error(errorSS.str());
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

    // auto eofAction = [](Stream& _stream, const LexerMatch& _match) { return std::any(Token::END_OF_FILE(_match.position)); };
    // auto unknownAction = [](Stream& _stream, const LexerMatch& _match) { return std::any(Token::INVALID(_match.value, _match.position)); };
    // Lexer lexer(eofAction, unknownAction);
    // lexer.AddPattern(Regex("\\s"));
    // lexer.AddPattern(Regex(":"), [](Stream& _stream, const LexerMatch& _match) { std::cout << "I spot a colon!" << std::endl; });

    // std::map<Lexer::PatternID, TokenType> tokens = {
    //     { Lexer::EOF_PATTERN_ID(), TokenType::END_OF_FILE },
    //     { Lexer::UNKNOWN_PATTERN_ID(), TokenType::INVALID },
    //     { lexer.AddPattern(Regex("(_|[a-zA-Z])(_|[a-zA-Z0-9])*"), [](Stream& _stream, const LexerMatch& _match) { return std::any(Token::ID(_match.value, _match.position)); }), TokenType::ID },
    //     { lexer.AddPattern(Regex("-?(0|[1-9][0-9]*)([.][0-9]+)?"), [](Stream& _stream, const LexerMatch& _match) { return std::any(Token::NUM(std::stod(_match.value), _match.position)); }), TokenType::NUM }
    // };

    // while (true)
    // {
    //     Lexer::Result result = lexer.Lex(input);
    //     auto tokenSearch = tokens.find(result.patternID);

    //     if (tokenSearch != tokens.end())
    //     {
    //         Token token = std::any_cast<Token>(result.value);
    //         std::cout << token.GetPosition() << " ";

    //         switch (token.GetType())
    //         {
    //         case TokenType::ID: std::cout << "ID: " << token.GetValue<std::string>() << std::endl; break;
    //         case TokenType::NUM: std::cout << "NUM: " << token.GetValue<double>() << std::endl; break;
    //         case TokenType::INVALID: std::cout << "INVALID: " << token.GetValue<std::string>() << std::endl; break;
    //         case TokenType::END_OF_FILE: std::cout << "EOF" << std::endl; break;
    //         default: assert(false && "Case not handled!");
    //         }
    //     }

    //     if (result.patternID == Lexer::EOF_PATTERN_ID())
    //         break;
    // }

    auto eofAction = [](Stream& _stream, const LexerMatch& _match) { return std::any(Token::END_OF_FILE(_match.GetPosition(_stream))); };
    auto unknownAction = [](Stream& _stream, const LexerMatch& _match) { return std::any(Token::INVALID(_match.GetValue(_stream), _match.GetPosition(_stream))); };

    Parser parser("EOF", eofAction, unknownAction);
    parser.lexer.AddPattern(Regex("\\s"));
    parser.lexer.AddPattern(Regex(":"), [](Stream& _stream, const LexerMatch& _match) { std::cout << "I spot a colon!" << std::endl; });
    parser.AddTerminal("let", Regex("let"), [](Stream& _stream, const LexerMatch& _match) { return std::any(Token::KW_LET(_match.GetPosition(_stream))); });
    parser.AddTerminal("=", Regex("="), [](Stream& _stream, const LexerMatch& _match) { return std::any(Token::SYM_EQ(_match.GetPosition(_stream))); });
    parser.AddTerminal(";", Regex(";"), [](Stream& _stream, const LexerMatch& _match) { return std::any(Token::SYM_SEMICOLON(_match.GetPosition(_stream))); });
    parser.AddTerminal("ID", Regex("(_|[a-zA-Z])(_|[a-zA-Z0-9])*"), [](Stream& _stream, const LexerMatch& _match) { return std::any(Token::ID(_match.GetValue(_stream), _match.GetPosition(_stream))); });
    parser.AddTerminal("NUM", Regex("-?(0|[1-9][0-9]*)([.][0-9]+)?"), [](Stream& _stream, const LexerMatch& _match) { return std::any(Token::NUM(std::stod(_match.GetValue(_stream)), _match.GetPosition(_stream))); });
    parser.AddRule("DECL", { "let", "ID", "=", "EXPR", ";" }, [](Stream& _stream, const std::vector<Parser::Rule::Arg>& _args) { std::cout << "Found a declaration!" << std::endl; });
    parser.AddRule("EXPR", { "NUM" }, [](Stream& _stream, const std::vector<Parser::Rule::Arg>& _args) { return _args[0].value; });
    parser.AddRule("EXPR", { "ID" }, [](Stream& _stream, const std::vector<Parser::Rule::Arg>& _args) { return _args[0].value; });

    try
    {
        parser.Parse(input, "DECL");
    }
    catch (const std::runtime_error& e)
    {
        std::cerr << e.what() << '\n';
    }

    return 0;

    // Grammar grammar({
    //     new Terminal<std::string>("ID", "abc", [](Stream& _stream, const std::string& _value) { return _value; }),
    //     new Terminal<int>("NUM", "123", [](Stream& _stream, const std::string& _value) { return std::stoi(_value); }),
    //     });


    // try
    // {
    //     std::cout << std::any_cast<std::string>(grammar.Parse("ID", input)) << std::endl;
    //     std::cout << std::any_cast<int>(grammar.Parse("NUM", input)) << std::endl;
    //     return 0;
    // }
    // catch (const std::runtime_error& e)
    // {
    //     std::cout << e.what() << std::endl;
    //     return -1;
    // }
}