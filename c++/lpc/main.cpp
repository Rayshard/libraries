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

    auto mapper1 = [=](const ParseResult<std::string>& _value)
    {
        std::cout << _value.value << std::endl;
        return _value;
    };

    auto mapper = [=](const ParseResult<int>& _value)
    {
        std::cout << _value.value << std::endl;
        return _value;
    };

    //auto expr = Choice("EXPR", { Terminal("ID", "ID"), Terminal("NUMBER", "NUMBER") });
    Parser expr = Terminal("ID", "ID") | Terminal("NUMBER", "NUMBER") | Terminal("LET", "KEYWORD", "let");
    auto number = Terminal("NUMBER", "NUMBER").Map<int>([](auto _value) { return ParseResult<int>{.position = _value.position, .value = std::stoi(_value.value)}; });
    //auto expr2 = Variant("EXPR", Terminal("ID", "ID"), Terminal("NUMBER", "NUMBER"));
    Parser expr2 = Terminal("ID", "ID").Map<std::string>(mapper1) | number.Map<int>(mapper);

    // auto s = Sequence("Hi",
    //     Terminal("LET", "KEYWORD", "let"),
    //     Terminal("ID", "ID"),
    //     Terminal("EQ", "SYMBOL", "="),
    //     expr,
    //     Terminal("SEMICOLON", "SYMBOL", ";")
    // );

    auto s = Parser("MyParser",
        Terminal("LET", "KEYWORD", "let") >>
        Terminal("ID", "ID") &
        Terminal("EQ", "SYMBOL", "=") >>
        expr &
        Terminal("SEMICOLON", "SYMBOL", ";"));

    try
    {
        auto r = s.Parse(IStreamToString(file), lexer, { "WS" }).value;
        auto x = expr2.Parse("123", lexer, { "WS" }).value;
        std::cout << std::get<0>(r).value << " = " << std::get<1>(r).value << std::get<2>(r).value << std::endl;
        std::cout << std::get<0>(x) << std::endl;
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
