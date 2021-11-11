#include "parser.h"
#include <iostream>
#include <sstream>
#include <any>
#include <assert.h>

namespace parser
{
    class Grammar;

    class Symbol
    {
        std::string name;

    protected:
        Symbol(std::string _name) : name(_name) { }

    public:
        virtual ~Symbol() { }

        virtual std::any Parse(Stream& _stream, const Grammar* _grammar) = 0;

        const std::string& GetName() { return name; }
    };

    class Grammar
    {
        std::map<std::string, Symbol*> symbols;

    public:
        Grammar(const std::vector<Symbol*>& _symbols)
        {
            for (auto& symbol : _symbols)
            {
                if (!symbols.emplace(symbol->GetName(), symbol).second)
                    throw Error::DuplicateSymbolName(symbol->GetName());
            }
        }

        ~Grammar()
        {
            for (auto& [_, sym] : symbols)
                delete sym;
        }

        std::optional<Symbol*> GetSymbol(const std::string& _name)
        {
            auto symbolSearch = symbols.find(_name);
            return symbolSearch == symbols.end() ? std::optional<Symbol*>() : symbolSearch->second;
        }

        std::any Parse(const std::string& _symbolName, Stream& _stream)
        {
            auto symbol = GetSymbol(_symbolName);
            if (!symbol.has_value())
                throw Error::UnknownSymbol(_symbolName);

            return symbol.value()->Parse(_stream, this);
        }
    };

    template<typename T>
    class Terminal : public Symbol
    {
    public:
        typedef T(*Callback)(Stream& _stream, const std::string& _value);

    private:
        std::regex regex;
        Callback onMatch;

    public:
        Terminal(std::string _name, std::string _regex, Callback _onMatch) : Symbol(_name), regex(_regex), onMatch(_onMatch) { }

        std::any Parse(Stream& _stream, const Grammar* _grammar) override
        {
            std::smatch match;
            std::regex_search(_stream.Current(), _stream.End(), match, regex, std::regex_constants::match_continuous);

            if (match.size() == 0)
                throw Error::CannotParseSymbol(GetName());

            _stream.Ignore(match.length()); //Advance stream past matched substring
            return onMatch(_stream, match.str());
        }
    };

    // template<typename T>
    // class NonTerminal : public Symbol
    // {
    // public:
    //     typedef T(*Callback)(Stream& _stream, const std::vector<std::any>& _args);
    //     typedef std::variant<std::string, std::regex> Component;

    // private:
    //     std::vector<Component> components;
    //     Callback onMatch;

    // public:
    //     NonTerminal(std::string _name, const std::vector<Component>& _components, Callback _onMatch) : Symbol(_name), components(_components), onMatch(_onMatch) { }

    //     std::any Parse(Stream& _stream, const Grammar* _grammar) override
    //     {
    //         auto streamPos = _stream.position;
    //         std::vector<std::any> args;

    //         for (auto& component : components)
    //         {
    //             if (auto symbolName = std::get_if<std::string>(&component))
    //             {
    //                 auto symbol = _grammar->GetSymbol(*symbolName);
    //                 if (!symbol.has_value())
    //                     throw Error::UnknownSymbol(*symbolName);

    //                 args.push_back(symbol.value()->Parse(_stream, _grammar)));
    //             }
    //         }

    //         std::smatch match;
    //         std::regex_search(_stream.Current(), _stream.End(), match, regex, std::regex_constants::match_continuous);

    //         if (match.size() == 0)
    //             throw Error::CannotParseSymbol(GetName());

    //         _stream.Ignore(match.length()); //Advance stream past matched substring
    //         return onMatch(_stream, match.str());
    //     }
    // };
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
    while (true)
    {
        std::cout << "Enter text to lex: ";
        std::string inputLine;
        std::getline(std::cin, inputLine);

            if (inputLine.empty())
                break;

        std::cout << "Result:\n";
        Stream input(inputLine);

        auto eofAction = [](Stream& _stream, const Lexer<Token>::Match& _match) { return Token::END_OF_FILE(_match.position); };
        auto invalidAction = [](Stream& _stream, const Lexer<Token>::Match& _match) { return Token::INVALID(_match.value, _match.position); };
        Lexer<Token> lexer(eofAction, invalidAction);
        lexer.AddPattern("ID", std::regex("(_|[a-zA-Z])(_|[a-zA-Z0-9])*"), [](Stream& _stream, const Lexer<Token>::Match& _match) { return Token::ID(_match.value, _match.position); });
        lexer.AddPattern("NUM", std::regex("-?(0|[1-9][0-9]*)([.][0-9]+)?"), [](Stream& _stream, const Lexer<Token>::Match& _match) { return Token::NUM(std::stod(_match.value), _match.position); });
        lexer.AddPattern(std::regex(" "));
        lexer.AddPattern(std::regex(":"), [](Stream& _stream, const Lexer<Token>::Match& _match) { std::cout << "I spot a colon!" << std::endl; });

        while (true)
        {
            Token token = lexer.Lex(input).token;
            std::cout << token.GetPosition() << " ";

            switch (token.GetType())
            {
            case TokenType::ID: std::cout << "ID: " << token.GetValue<std::string>() << std::endl; break;
            case TokenType::NUM: std::cout << "NUM: " << token.GetValue<double>() << std::endl; break;
            case TokenType::INVALID: std::cout << "INVALID: " << token.GetValue<std::string>() << std::endl; break;
            case TokenType::END_OF_FILE: std::cout << "EOF" << std::endl; break;
            default: assert(false && "Case not handled!");
            }

            if (token.GetType() == TokenType::END_OF_FILE)
                break;
        }
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