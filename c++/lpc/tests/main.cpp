#include "lpc.h"
#include <iostream>
#include <cassert>
#include <map>

using namespace lpc;
using namespace lpc::parsers;

using Test = std::function<void()>;

#define ASSERT(value) if (!(value)) throw std::runtime_error(std::string(__FILE__) + ":" + std::to_string(__LINE__))

std::map<std::string, Test> tests =
{
    {"StringStream", []()
        {
            ASSERT(false);
        }
    },
    {"Map", []()
        {
            auto parser = Parser<int>([](const Position& _pos, StringStream& _stream) { return ParseResult<int>(_pos, 5); });
            ASSERT((Map<int, int>(parser, [](ParseResult<int>&& _input) { return 6; }).Parse("").value == 6));
        }
    },
    {"Reference", []()
        {
            Reference<char> reference;
            auto function = Parser<char>([parser = Parser<char>(reference)](const Position& _pos, StringStream& _stream) { return parser.Parse(_stream); });

            reference.Set([](const Position& _pos, StringStream& _stream) {return ParseResult<char>(_pos, 'b'); });
            ASSERT(function.Parse("a").value == 'b');

            auto other = reference;
            other.Set([](const Position& _pos, StringStream& _stream) { throw ParseError(_pos, "Hiya"); return ParseResult<char>(_pos, 'c'); });

            try
            {
                function.Parse("a");
                ASSERT(false);
            }
            catch (const ParseError& e) {}
        }
    },
    {"Try", []()
        {
            ParseError expectedError(Position(100, 250), "The error!");

            auto parser = Try(Parser<int>([=](const Position& _pos, StringStream& _stream)
            {
                if (_stream.Peek() != 'q')
                    throw expectedError;

                return ParseResult<int>(_pos, 123);
            }));

            ASSERT(parser.Parse("q").value.IsSuccess());
            ASSERT(*parser.Parse("q").value.ExtractSuccess() == 123);

            ASSERT(parser.Parse("a").value.IsError());
            ASSERT(*parser.Parse("a").value.ExtractError() == expectedError);
        }
    },
    {"Count", []()
        {
            CountParser<char> parser = Count(Parser<char>([](const Position& _pos, StringStream& _stream)
            {
                if (!std::isalpha(_stream.Peek()))
                    throw ParseError::Expectation("a letter", "'" + std::string(1, _stream.Peek()) + "'", _stream.GetPosition());

                return ParseResult<char>(_pos, _stream.Get());
            }), 1, 3);

            StringStream stream("abcef g ");

            try
            {
                CountValue<char> value = parser.Parse(stream).value;
                ASSERT(value.size() == 3);
                ASSERT(value[0].position == Position(1, 1) && value[0].value == 'a');
                ASSERT(value[1].position == Position(1, 2) && value[1].value == 'b');
                ASSERT(value[2].position == Position(1, 3) && value[2].value == 'c');

                value = parser.Parse(stream).value;
                ASSERT(value.size() == 2);
                ASSERT(value[0].position == Position(1, 4) && value[0].value == 'e');
                ASSERT(value[1].position == Position(1, 5) && value[1].value == 'f');

                try
                {
                    parser.Parse(stream);
                    ASSERT(false);
                }
                catch (const ParseError& e) { stream.Ignore(1); }

                value = parser.Parse(stream).value;
                ASSERT(value.size() == 1);
                ASSERT(value[0].position == Position(1, 7) && value[0].value == 'g');
            }
            catch (const ParseError& e) { ASSERT(false); }
        }
    },
    {"ManyOrOne", []() { ASSERT(true); }}, //Derivative of Count
    {"ZeroOrOne", []() { ASSERT(true); }}, //Derivative of Count
    {"ZeroOrMore", []() { ASSERT(true); }}, //Derivative of Count
    {"Exactly", []() { ASSERT(true); }}, //Derivative of Count
    {"Seq", []()
        {
            StringStream input("abc");
            auto [r1, r2, r3] = Seq(Char('a'), Char('b'), Char('c')).Parse(input).value;

            ASSERT(r1.value == 'a');
            ASSERT(r2.value == 'b');
            ASSERT(r3.value == 'c');
            ASSERT(input.GetOffset() == 3);
        }
    },
    {"Optional", []()
        {
            StringStream input("123abc");
            auto parser = Optional(Digits());

            auto value = parser.Parse(input).value;
            ASSERT(value.has_value() && value.value() == "123");

            value = parser.Parse(input).value;
            ASSERT(!value.has_value());

            ASSERT(input.GetOffset() == 3);
        }
    },
    {"Longest", []()
        {
            StringStream input("123 abc 123abc");
            auto parser = Longest({Digits(), Letters(), Chars("123abc")});

            ASSERT(parser.Parse(input).value == "123");
            input.Ignore(1);
            ASSERT(parser.Parse(input).value == "abc");
            input.Ignore(1);
            ASSERT(parser.Parse(input).value == "123abc");
        }
    },
    {"FirstSuccess", []()
        {
            StringStream input("123abc abc 123abc");

            ASSERT(FirstSuccess({Letters(), Digits(), AlphaNums()}).Parse("123abc").value == "123");
            ASSERT(FirstSuccess({Letters(), AlphaNums(), Digits()}).Parse("123abc").value == "123abc");
            ASSERT(FirstSuccess({Letters(), AlphaNums(), Digits()}).Parse("qwe123abc").value == "qwe");
        }
    },
    {"Variant", []()
        {
            using V = Variant<std::string, char, int>;
            auto charParser = V::Create(Char());
            auto stringParser = V::Create(Chars());
            auto intParser = V::Create(Map<std::string, int>(Digits(),[](ParseResult<std::string>&& _result) { return std::stoi(_result.value); }));

            ASSERT(charParser.Parse("123").value.Extract<char>().value == '1');
            ASSERT(stringParser.Parse("123abc").value.Extract<std::string>().value == "123abc");
            ASSERT(intParser.Parse("123abc").value.Extract<int>().value == 123);
        }
    },
    {"Chain", []()
        {
            auto chain = Chain<std::string, int>(Digits(), [](ParseResult<std::string>&& _input)
            {
                return Parser<int>([=](const Position& _pos, StringStream& _stream)
                {
                    return ParseResult<int>(_pos, std::stoi(_input.value));
                });
            });

            ASSERT(chain.Parse("5").value == 5);
            ASSERT(chain.Parse("6").value == 6);
        }
    },
    {"Satisfy", []()
        {
            auto parser = Parser<int>([](const Position& _pos, StringStream& _stream) { return ParseResult<int>(_pos, 5); });
            auto onFail = [](const ParseResult<int>& _result) { return ParseError(_result.position, "Oh no!"); };

            ASSERT(Satisfy<int>(parser, 5, onFail).Parse("").value == 5);

            try
            {
                Satisfy<int>(parser, 6, onFail).Parse("");
                ASSERT(false);
            }
            catch (const ParseError& e) {}
        }
    },
    {"Success", []()
        {
            auto parser = Parser<int>([](const Position& _pos, StringStream& _stream)
            {
                if (_stream.Get() == 'b')
                    throw ParseError(_pos, "");

                return ParseResult<int>(_pos, 5);
            });

            ASSERT(Success(parser, 6).Parse("a").value == 5);
            ASSERT(Success(parser, 6).Parse("b").value == 6);
        }
    },
    {"Failure", []()
        {
            auto parser = Parser<int>([](const Position& _pos, StringStream& _stream)
            {
                if (_stream.Peek() == 'b')
                    throw ParseError(_pos, "");

                return ParseResult<int>(_pos, 5);
            });

            try { Failure(parser).Parse("b"); }
            catch (const ParseError& e) { ASSERT(false); }

            try
            {
                Failure(parser).Parse("a");
                ASSERT(false);
            }
            catch (const ParseError& e) {}
        }
    },
    {"Callback", []()
        {
            ASSERT(false);
        }
    },
    {"Lexeme", []()
        {
            ASSERT(false);
        }
    },
    {"Char", []()
        {
            ASSERT(false);
        }
    },
    {"Chars", []()
        {
            ASSERT(false);
        }
    },
    {"Letter", []()
        {
            ASSERT(false);
        }
    },
    {"Letters", []()
        {
            ASSERT(false);
        }
    },
    {"Digit", []()
        {
            ASSERT(false);
        }
    },
    {"Digits", []()
        {
            ASSERT(false);
        }
    },
    {"AlphaNum", []()
        {
            ASSERT(false);
        }
    },
    {"AlphaNums", []()
        {
            ASSERT(false);
        }
    },
    {"Whitespace", []()
        {
            ASSERT(false);
        }
    },
    {"Whitespaces", []()
        {
            ASSERT(false);
        }
    },
    {"EOS", []()
        {
            ASSERT(false);
        }
    },
    {"BinopChain", []()
        {
            ASSERT(false);
        }
    },
    {"Fold", []()
        {
            ASSERT(false);
        }
    },
    {"Named", []() { ASSERT(Named("MyParser", Char()).Parse("a").value == 'a'); } },
    {"Prefixed", []() { ASSERT(Prefixed(Digit(), Letter()).Parse("1b").value == 'b'); } },
    {"Suffixed", []() { ASSERT(Suffixed(Letter(), Digit()).Parse("b1").value == 'b'); } },
    {">>", []() { ASSERT((Digit() >> Letter()).Parse("1b").value == 'b'); } },
    {"<<", []() { ASSERT((Letter() << Digit()).Parse("b1").value == 'b'); } },
    {"Value", []()
        {
            StringStream input("gkyub");

            ASSERT(Value(123).Parse(input).value == 123);
            ASSERT(input.GetOffset() == 0);
        }
    },
    {"Between", []()
        {
            ASSERT(false);
        }
    },
    {"LookAhead", []()
        {
            ASSERT(false);
        }
    },
    {"Separate", []()
        {
            ASSERT(false);
        }
    },
};

int main()
{
    size_t numPassed = 0;

    std::cout << "Running " << tests.size() << " tests...\n\n";

    for (auto& [name, test] : tests)
    {
        std::optional<std::runtime_error> failure;

        try
        {
            test();
            numPassed++;
            std::cout << "\t(Passed) '" << name << "'\n";
        }
        catch (const std::runtime_error& e) { std::cout << "\t(Failed) '" << name << "' @ " << e.what() << '\n'; }
    }

    std::cout << "\nPassed " << numPassed << " tests.\nFailed " << tests.size() - numPassed << " tests." << std::endl;
    return 0;
}
