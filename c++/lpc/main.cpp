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
    auto KW_LET = keyword.AsTerminal("let").Satisfy([](auto result) { return result.value == "let"; }, [](auto result) { return "Expected 'le' but found '" + result.value + "'"; });

    auto symbol = lexer.AddPattern("SYMBOL", Regex("=|;|,"));
    auto SYM_EQ = symbol.AsTerminal("=");
    auto SYM_SEMICOLON = symbol.AsTerminal(";");
    auto SYM_COMMA = symbol.AsTerminal(",");

    auto ID = lexer.AddPattern("ID", Regex("(_|[a-zA-Z])(_|[a-zA-Z0-9])*")).AsTerminal();
    auto NUMBER = lexer.AddPattern("NUMBER", Regex("-?(0|[1-9][0-9]*)([.][0-9]+)?")).AsTerminal();

    Parser EXPR = ID | NUMBER | KW_LET;

    auto IDs = Recursive<std::vector<decltype(ID)::Result>>("IDs");

    Parser p = ID || Parser(ID & IDs).Map<std::vector<decltype(ID)::Result>>([](auto _result)
        {
            //std::vector<decltype(ID)::Result> tail = std::get<1>(_result.value).value;
            std::vector<decltype(ID)::Result> tail = std::get<1>(_result.value).value;
            decltype(tail) result = { std::get<0>(_result.value) };
            result.insert(result.end(), tail.begin(), tail.end());
            return result;
        });

    IDs.Set(p.Map<std::vector<decltype(ID)::Result>>([](auto _result)
        {
            return _result.value.template Is<1>() ? _result.value.template Get<1>() : std::vector<decltype(ID)::Result>({ decltype(ID)::Result(_result.position, _result.value.template Get<0>()) });
        }));

    auto DECL = Parser("Declaration", KW_LET & ID & SYM_EQ >> Try(EXPR, 15) << SYM_SEMICOLON);
    auto DECLS = ManyOrOne("Declarations", DECL);
    auto NUMBER_LIST = Separated("Numbers", NUMBER, SYM_COMMA, 1);
    Parser ID_LIST = ID + ID + ID + ID;

    auto parser = LPC(lexer, DECLS, { "WS" });

    try
    {
        std::cout << Between("Between", SYM_COMMA, NUMBER, SYM_SEMICOLON).Parse(", 123 ;", lexer, {"WS"}).value << std::endl;

        // auto numbers = NUMBER_LIST.Parse(IStreamToString(file), lexer, {"WS"}).value;

        // for(auto& number : numbers)
        //     std::cout << number.value << ", ";

        // std::cout << std::endl;

        // auto ids_list = ID_LIST.Parse(IStreamToString(file), lexer, { "WS" }).value;

        // for (auto& id : ids_list)
        //     std::cout << id.value << ", ";

        // std::cout << std::endl;

        auto decls = parser.Parse(IStreamToString(file)).value;

        std::vector<std::string> errors;

        for (auto& [pos, decl] : decls)
        {
            auto& [_, id, expr] = decl;
            if (expr.value.IsSuccess()) { std::cout << pos << " let " << id.value << " = " << expr.value.GetSuccess() << ";" << std::endl; }
            else
            {
                std::cout << pos << " let " << id.value << " = <ERROR>;" << std::endl;
                errors.push_back(expr.value.GetParseError().what());
            }
        }

        for (auto& error : errors)
            std::cout << error << std::endl;
    }
    catch (const ParseError& e) { std::cerr << e.what() << "\n\nDetails:\n" << e.GetDetails() << std::endl; }

    return 0;
}
