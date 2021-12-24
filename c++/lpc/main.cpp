#include "lpc.h"
#include <iostream>

using namespace lpc;
using namespace lpc::parsers;

class MyParser : public IParser<std::monostate>
{
    Lexer lexer;

    Lexer::PatternID ID, INTEGER, DECIMAL, SYMBOL_COMMA;
    std::set<Lexer::PatternID> ignores;
public:
    MyParser() : lexer()
    {
        ignores.insert(lexer.AddPattern("WS", Regex("\\s+")).id);

        ID = lexer.AddPattern("ID", Regex("([a-zA-Z]|[_])+")).id;
        INTEGER = lexer.AddPattern("INTEGER", Regex("-?(0|[1-9][0-9]*)")).id;
        DECIMAL = lexer.AddPattern("DECIMAL", Regex("-?(0|[1-9][0-9]*)([.][0-9]+)?")).id;
        SYMBOL_COMMA = lexer.AddPattern("SYMBOL_COMMA", Regex(",")).id;
    }

protected:
    Result OnParse(const Position& _pos, StringStream& _stream) override
    {
        auto ids = Separated(Token(lexer, ID, ignores) | Token(lexer, INTEGER, ignores) | Token(lexer, DECIMAL, ignores), Token(lexer, SYMBOL_COMMA, ignores), 1).Parse(_stream);

        for(auto& id : ids.value)
            std::cout << id.position << " : " << id.value << std::endl;

        return Result(_pos, std::monostate());
    }
};

int main()
{
    try
    {
        MyParser parser;
        Named("MyParser", parser).Parse("123, World , 5.7");
    }
    catch (const ParseError& e)
    {
        std::cerr << e.GetMessageWithTrace() << '\n';
    }

    return 0;
}
