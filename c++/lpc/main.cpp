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

   auto mapper = [=](const ParseResult<std::string>& _value)
    {
        std::cout << _value.value << std::endl;
        return ParseResult<std::string>{_value.position, "Hello"};
    };
    
    //auto expr = Choice("EXPR", { Terminal("ID", "ID"), Terminal("NUMBER", "NUMBER") }).Map<std::string>(mapper);
    //auto expr = Choice("EXPR", { Terminal("ID", "ID"), Terminal("NUMBER", "NUMBER") });
    //auto expr = Terminal("NUMBER", "NUMBER");
    //auto expr = Map<std::string, std::string>(Choice("EXPR", { Terminal("ID", "ID"), Terminal("NUMBER", "NUMBER") }), mapper);
    auto expr = Variant("EXPR", Terminal("ID", "ID"), Terminal("NUMBER", "NUMBER"));
    
    // auto s = Sequence("Hi",
    //     Terminal("LET", "KEYWORD", "let"),
    //     Terminal("ID", "ID"),
    //     Terminal("EQ", "SYMBOL", "="),
    //     expr,
    //     Terminal("SEMICOLON", "SYMBOL", ";")
    // );

    auto s = Parser("MyParser", Terminal("ID", "ID"));
    // >> Sequence("Hi",
    //     Terminal("LET", "KEYWORD", "let"),
    //     Terminal("ID", "ID"),
    //     Terminal("EQ", "SYMBOL", "="),
    //     expr,
    //     Terminal("SEMICOLON", "SYMBOL", ";")
    // );

    try
    {
        auto r = s.Parse(IStreamToString(file), lexer, { "WS" }).value;
        //auto r = expr.Parse(IStreamToString(file), lexer, { "WS" }).value;
        //std::cout << std::get<1>(r) << std::endl;
        std::cout << r << std::endl;
        std::cout << 5 << std::endl;
    }
    catch (const ParseError& e)
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
