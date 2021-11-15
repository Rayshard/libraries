#include "parser.h"
#include <iostream>

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

    Parser parser(eosAction, unknownAction);
    parser.lexer.AddPattern(Regex("\\s"));
    parser.lexer.AddPattern(Regex(":"), [](Stream& _stream, const LexerMatch& _match) { std::cout << "I spot a colon!" << std::endl; });
    parser.AddTerminal("let", Regex("let"), [](Stream& _stream, const LexerMatch& _match) { return std::any(Token::KW_LET(_match.position)); });
    parser.AddTerminal("=", Regex("="), [](Stream& _stream, const LexerMatch& _match) { return std::any(Token::SYM_EQ(_match.position)); });
    parser.AddTerminal(";", Regex(";"), [](Stream& _stream, const LexerMatch& _match) { return std::any(Token::SYM_SEMICOLON(_match.position)); });
    parser.AddTerminal("ID", Regex("(_|[a-zA-Z])(_|[a-zA-Z0-9])*"), [](Stream& _stream, const LexerMatch& _match) { return std::any(Token::ID(_match.value, _match.position)); });
    parser.AddTerminal("NUM", Regex("-?(0|[1-9][0-9]*)([.][0-9]+)?"), [](Stream& _stream, const LexerMatch& _match) { return std::any(Token::NUM(std::stod(_match.value), _match.position)); });
    parser.AddRule("DECL", { "let", "ID", "=", "EXPR", ";" }, [](TokenStream& _stream, const Parser::NTMatch& _match) { std::cout << "Found a declaration!" << std::endl; });
    parser.AddRule("DECL", { "let", "ID", "=", ";" }, [](TokenStream& _stream, const Parser::NTMatch& _match) { std::cout << "Found a declaration without expr!" << std::endl; });
    parser.AddRule("DECL", { "let", "ID", "=", Parser::SymbolID_AnyTerminal, ";" }, [](TokenStream& _stream, const Parser::NTMatch& _match)
        {
            auto error = _match[3];
            std::cout << error.GetPosition() << " Expected EXPR but found '" << error.GetMatchAsTerminal().value << "'" << std::endl;
        });
    parser.AddRule("DECL", { "let", "ID", "=", Parser::SymbolID_AnySymbol }, [](TokenStream& _stream, const Parser::NTMatch& _match)
        {
            auto error = _match[3];
            std::cout << error.GetPosition() << " Expected EXPR but found '" << error.GetID() << "'" << std::endl;
        });
    parser.AddRule("DECL", { "let", "ID", "=", Parser::SymbolID_AnyTerminal }, [](TokenStream& _stream, const Parser::NTMatch& _match)
        {
            auto error = _match[3];
            std::cout << error.GetPosition() << " Expected EXPR but found '" << error.GetMatchAsTerminal().value << "'" << std::endl;
        });
    parser.AddRule("EXPR", { "NUM" }, [](TokenStream& _stream, const Parser::NTMatch& _match) { return _match[0].GetValue(); });
    parser.AddRule("EXPR", { "ID" }, [](TokenStream& _stream, const Parser::NTMatch& _match) { return _match[0].GetValue(); });

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