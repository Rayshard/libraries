#include "lpc.h"
#include <iostream>

using namespace lpc;

int main()
{
    std::ifstream file("test.txt");
    // TestLPC parser("TestParser");

    // std::cout << parser.Parse(IStreamToString(file)).ToString() << std::endl;

    Lexer lexer;
    lexer.AddPattern("WS", Regex("\\s+"));
    lexer.AddPattern("KEYWORD", Regex("let"));
    lexer.AddPattern("SYMBOL", Regex("=|;"));
    lexer.AddPattern("ID", Regex("(_|[a-zA-Z])(_|[a-zA-Z0-9])*"));
    lexer.AddPattern("NUMBER", Regex("-?(0|[1-9][0-9]*)([.][0-9]+)?"));

    auto s = Sequence("Hi", {
        Terminal("LET", "KEYWORD", "let"),
        Terminal("ID", "ID"),
        Terminal("EQ", "SYMBOL", "="),
        Terminal("EXPR", "NUMBER"),
        Terminal("SEMICOLON", "SYMBOL", "=")
        });

    try
    {
        auto r = s.Parse(IStreamToString(file), lexer, { "WS" });
    }
    catch(const ParseError& e)
    {
        std::cerr << e.what() << std::endl;
    }
    // StringStream input(IStreamToString(file));
    // Lexer lexer = Lexer();

    //     TokenStream stream(&lexer, &input);

    // while (!stream.IsEOS())
    //     std::cout << stream.GetOffset() << ", " << stream.Get().value << std::endl;

    // stream.SetOffset(9);
    // std::cout << stream.GetOffset() << ", " << stream.Get().value << std::endl;
    // stream.SetOffset(14);
    // std::cout << stream.GetOffset() << ", " << stream.Get().value << std::endl;

    return 0;
}