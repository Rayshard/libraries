#include "lpc.h"
#include <iostream>

using namespace lpc;

typedef std::string JSON;

class JSONParser : public LPC<JSON>
{
public:
    JSONParser() : LPC(Create()) { }

    static Tuple Create()
    {
        Lexer lexer;
        auto WS = lexer.AddPattern("WS", Regex("\\s+"));

        auto NUMBER = lexer.AddPattern("NUMBER", Regex("-?(0|[1-9][0-9]*)([.][0-9]+)?")).AsTerminal();

        return Tuple(lexer, NUMBER, { WS.id });
    }
};

int main()
{
    std::ifstream file("test.txt");

    Lexer lexer;
    lexer.AddPattern("WS", Regex("\\s+"));

    auto keyword = lexer.AddPattern("KEYWORD", Regex("let"));
    auto KW_LET = keyword.AsTerminal("let");

    auto symbol = lexer.AddPattern("SYMBOL", Regex("=|;"));
    auto SYM_EQ = symbol.AsTerminal("=");
    auto SYM_SEMICOLON = symbol.AsTerminal(";");

    auto ID = lexer.AddPattern("ID", Regex("(_|[a-zA-Z])(_|[a-zA-Z0-9])*")).AsTerminal();
    auto NUMBER = lexer.AddPattern("NUMBER", Regex("-?(0|[1-9][0-9]*)([.][0-9]+)?")).AsTerminal();

    Parser EXPR = ID | NUMBER | KW_LET;

    auto DECL = Parser("Declaration", KW_LET >> ID & SYM_EQ >> EXPR << SYM_SEMICOLON);
    auto IDs = Recursive<std::vector<decltype(ID)::Result>>("IDs");

    Parser p = ID
        || Parser(ID & IDs).Map<std::vector<decltype(ID)::Result>>([](auto _result)
            {
                std::vector<decltype(ID)::Result> tail = std::get<1>(_result.value).value;
                decltype(tail) result = { std::get<0>(_result.value) };
                result.insert(result.end(), tail.begin(), tail.end());
                return result;
            });

    IDs.Set(p.Map<std::vector<decltype(ID)::Result>>([](auto _result) { return _result.value.index() == 1 ? std::get<1>(_result.value) : std::vector<decltype(ID)::Result>({ decltype(ID)::Result{.position = _result.position, .value = std::get<0>(_result.value)} }); }));

    auto parser = LPC(lexer, IDs, { "WS" });

    try
    {
        auto ids = parser.Parse(IStreamToString(file)).value;

        for (auto id : ids)
            std::cout << id.value << std::endl;
    }
    catch (const ParseError& e) { std::cerr << e.what() << std::endl; }

    return 0;
}
