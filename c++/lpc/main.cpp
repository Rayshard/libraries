#include "lpc.h"
#include <iostream>
#include <fstream>

using namespace lpc;
using namespace lpc::parsers;

struct MyParser : public LPC<std::monostate>
{
    MyParser()
    {
        AddPattern("WS", Regex("\\s+"));
        AddPattern("ID", Regex("([a-zA-Z]|[_])+"));
        AddPattern("INTEGER", Regex("-?(0|[1-9][0-9]*)"));
        AddPattern("DECIMAL", Regex("-?(0|[1-9][0-9]*)([.][0-9]+)?"));
        AddPattern("SYMBOL", Regex(","));

        ignores.insert("WS");
    }

    Parser<std::string> ID() { return Named("ID", CreateLexeme("ID", ignores)); }
    Parser<int> INTEGER() { return Named("INTEGER", Mapped<std::string, int>(CreateLexeme("INTEGER", ignores), [](ParseResult<std::string>& _result) { return std::stoi(_result.value); })); }
    Parser<double> DECIMAL() { return Named("DECIMAL", Mapped<std::string, double>(CreateLexeme("DECIMAL", ignores), [](ParseResult<std::string>& _result) { return std::stod(_result.value); })); }
    Parser<std::monostate> COMMA() { return Named("COMMA", Mapped<std::string, std::monostate>(CreateLexeme("SYMBOL", ignores, ","), [](ParseResult<std::string>& _result) { return std::monostate(); })); }

protected:
    Result OnParse(const Position& _pos, StringStream& _stream) override
    {
        //typedef VariantParser<std::string, int, double> Element;

        // auto variantID = Element::Create(ID());
        // auto variantINTEGER = Element::Create(INTEGER());
        // auto variantDECIMAL = Element::Create(DECIMAL());

        // auto elements = Separated(variantID | variantINTEGER | variantDECIMAL, COMMA(), 2).Parse(_stream);

        // for (auto& elem : elements.value)
        // {
        //     if (ParseResult<std::string>* result = Element::Extract<std::string>(elem.value)) { std::cout << result->position << " : " << result->value << std::endl; }
        //     else if (ParseResult<int>* result = Element::Extract<int>(elem.value)) { std::cout << result->position << " : " << result->value << std::endl; }
        //     else if (ParseResult<double>* result = Element::Extract<double>(elem.value)) { std::cout << result->position << " : " << result->value << std::endl; }
        // }

        auto element = (CreateLexeme("ID", ignores) || CreateLexeme("DECIMAL", ignores) || CreateLexeme("INTEGER", ignores)).Parse(_stream).value;
        std::cout << element << std::endl;
        return Result(_pos, std::monostate());
    }
};

int main()
{
    try
    {
        std::ifstream file("lpc/test.txt");
        MyParser parser;
        Named("MyParser", parser).Parse(IStreamToString(file));
    }
    catch (const ParseError& e)
    {
        std::cerr << e.GetMessageWithTrace() << '\n';
    }

    return 0;
}
