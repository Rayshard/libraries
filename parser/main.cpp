#include "parser.h"
#include <iostream>
#include <sstream>
#include <any>
#include <assert.h>
#include <unordered_map>

namespace parser
{
    class Parser
    {
    public:
        typedef std::string SymbolID;
        typedef Lexer::PatternID Terminal;

        struct NonTerminal
        {
            typedef void(*Procedure)(Stream&, const std::vector<std::any>&);
            typedef std::any(*Function)(Stream&, const std::vector<std::any>&);
            typedef std::variant<Procedure, Function, std::monostate> Action;
            typedef std::variant<SymbolID, std::regex> Component;

            struct Rule
            {
                std::vector<Component> components;
                Action action;
            };

            std::vector<Rule> rules;
        };

        struct Result
        {
            SymbolID symbolID;
            std::any value;
        };

    private:
        typedef std::variant<Terminal, NonTerminal> Symbol;
        std::unordered_map<SymbolID, Symbol> symbols;

        Lexer lexer;
        bool validated;
    public:
        Parser(Lexer::Action _onEOF, Lexer::Action _onUnknown) : lexer(_onEOF, _onUnknown), validated(true) { }
    private:
        void AddSymbol(SymbolID _id, Symbol _symbol)
        {
            if (_id.empty()) { throw std::runtime_error("Symbol id must be non-empty!"); }
            else if (!symbols.emplace(_id, _symbol).second) { throw std::runtime_error("Symbol ID '" + _id + "' is already in use."); }
        }

    public:
        void AddTerminal(SymbolID _id, Lexer::PatternID _patternID) { AddSymbol(_id, _patternID); }

        void AddRule(SymbolID _ntID, std::initializer_list<NonTerminal::Component> _components, NonTerminal::Action _action = std::monostate())
        {
            auto symbolSearch = symbols.find(_ntID);
            if (symbolSearch == symbols.end()) { AddSymbol(_ntID, NonTerminal()); }
            else if (std::get_if<Terminal>(&symbolSearch->second)) { throw std::runtime_error("'" + _ntID + "' does not refer to a non-terminal!"); }

            //Add regex components to lexer so that they get lexed like terminals and return a token (the matched value)
            for (auto& component : _components)
            {
                if (auto regex = std::get_if<std::regex>(&component))
                    lexer.AddPattern(*regex, [](Stream& _stream, const Lexer::Match& _match) { return std::any(_match.value); });
            }

            std::get<NonTerminal>(symbols.at(_ntID)).rules.push_back(NonTerminal::Rule{ .components = _components, .action = _action }); //Add rule to non-terminal
            validated = false;
        }

        void Validate()
        {
            for (auto& [symbolID, symbol] : symbols)
            {
                if (auto nt = std::get_if<NonTerminal>(&symbol))
                {
                    for (auto& rule : nt->rules)
                    {
                        for (auto& component : rule.components)
                        {
                            if (auto id = std::get_if<SymbolID>(&component))
                            {
                                auto symbolSearch = symbols.find(*id);
                                if (symbolSearch == symbols.end())
                                    throw std::runtime_error("Nonterminal '" + symbolID + "' references a symbol that does not exist: '" + *id + "'");
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

            auto symbolSearch = symbols.find(_symbolID);
            if (symbolSearch == symbols.end())
                throw std::runtime_error("Symbol with id '" + _symbolID + "' does not exist!");

            if (std::get_if<Terminal>(&symbolSearch->second))
            {
                auto peek = lexer.Lex(_stream);

                return Result();
            }
            else
            {
                NonTerminal& nt = std::get<NonTerminal>(symbolSearch->second);
                return Result();
            }
        }
    };
}

using namespace parser;

enum class TokenType
{
    ID, NUM, INVALID, END_OF_FILE
};

class Token
{
    TokenType type;
    std::optional<std::any> value;
    Position position;

    Token(TokenType _type, Position _pos) : type(_type), value(), position(_pos) {}
    Token(TokenType _type, std::any _value, Position _pos) : type(_type), value(_value), position(_pos) {}

public:
    const TokenType& GetType() const { return type; }
    const Position& GetPosition() const { return position; }

    template<typename T>
    const T GetValue() const { return std::any_cast<T>(value.value()); }

    static Token END_OF_FILE(Position _pos) { return Token(TokenType::END_OF_FILE, _pos); }
    static Token INVALID(const std::string& _text, Position _pos) { return Token(TokenType::INVALID, _text, _pos); }
    static Token ID(const std::string& _id, Position _pos) { return Token(TokenType::ID, _id, _pos); }
    static Token NUM(double _value, Position _pos) { return Token(TokenType::NUM, _value, _pos); }
};

int main()
{
    std::ifstream file("test.txt");
    Stream input(file);

    auto eofAction = [](Stream& _stream, const Lexer::Match& _match) { return std::any(Token::END_OF_FILE(_match.position)); };
    auto unknownAction = [](Stream& _stream, const Lexer::Match& _match) { return std::any(Token::INVALID(_match.value, _match.position)); };
    Lexer lexer(eofAction, unknownAction);
    lexer.AddPattern(std::regex("(_|[a-zA-Z])(_|[a-zA-Z0-9])*"), [](Stream& _stream, const Lexer::Match& _match) { return std::any(Token::ID(_match.value, _match.position)); });
    lexer.AddPattern(std::regex("-?(0|[1-9][0-9]*)([.][0-9]+)?"), [](Stream& _stream, const Lexer::Match& _match) { return std::any(Token::NUM(std::stod(_match.value), _match.position)); });
    lexer.AddPattern(std::regex("\\s"));
    lexer.AddPattern(std::regex(":"), [](Stream& _stream, const Lexer::Match& _match) { std::cout << "I spot a colon!" << std::endl; });

    while (true)
    {
        Lexer::Result result = lexer.Lex(input);
        Token token = std::any_cast<Token>(result.value);
        std::cout << token.GetPosition() << " ";

        switch (token.GetType())
        {
        case TokenType::ID: std::cout << "ID: " << token.GetValue<std::string>() << std::endl; break;
        case TokenType::NUM: std::cout << "NUM: " << token.GetValue<double>() << std::endl; break;
        case TokenType::INVALID: std::cout << "INVALID: " << token.GetValue<std::string>() << std::endl; break;
        case TokenType::END_OF_FILE: std::cout << "EOF" << std::endl; break;
        default: assert(false && "Case not handled!");
        }

        if (result.patternID == Lexer::EOF_PATTERN_ID)
            break;
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