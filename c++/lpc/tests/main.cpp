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
    {"Map", []()
        {
            auto parser = Parser<int>([](const Position& _pos, StringStream& _stream) { return ParseResult<int>(_pos, 5); });
            ASSERT((Map<int, int>(parser, [](ParseResult<int>&& _input) { return 6; }).Parse("").value == 6));
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
                    throw ParseError(_pos, "Expected a letter");

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

                stream.Ignore(1);

                value = parser.Parse(stream).value;
                ASSERT(value.size() == 1);
                ASSERT(value[0].position == Position(1, 7) && value[0].value == 'g');
            }
            catch (const ParseError& e) { ASSERT(false); }

            try
            {
                parser.Parse(stream);
                ASSERT(false);
            }
            catch (const ParseError& e) {}
        }
    },
    {"ManyOrOne", []() { ASSERT(true); }}, //Derivative of Count
    {"ZeroOrOne", []() { ASSERT(true); }}, //Derivative of Count
    {"ZeroOrMore", []() { ASSERT(true); }}, //Derivative of Count
    {"Exactly", []() { ASSERT(true); }}, //Derivative of Count
    {"List", []()
        {
            Parser<char> letter = Parser<char>([](const Position& _pos, StringStream& _stream)
            {
                if (!std::isalpha(_stream.Peek()))
                    throw ParseError(_pos, "Expected a letter");

                return ParseResult<char>(_pos, _stream.Get());
            });

            Parser<int> number = Parser<int>([](const Position& _pos, StringStream& _stream)
            {
                if (!std::isdigit(_stream.Peek()))
                    throw ParseError(_pos, "Expected a digit");

                return ParseResult<int>(_pos, int(_stream.Get() - '0'));
            });

            List<char, int, char> parser = List<char, int, char>(std::make_tuple(letter, number, letter));
            auto [r1, r2, r3] = parser.Parse("a1c").value;

            ASSERT(r1.value == 'a');
            ASSERT(r2.value == 'b');
            ASSERT(r3.value == 'c');
        }
    },
    {"&", []()
        {
            auto [r1, r2, r3, r4, r5, r6] = ((Char('a') & (((Char('b') & Char('c')) & Char('d')) & (Char('e') & Char('f')))) << EOS())->Parse("abcdef").value;

            ASSERT(r1.value == 'a');
            ASSERT(r2.value == 'b');
            ASSERT(r3.value == 'c');
            ASSERT(r4.value == 'd');
            ASSERT(r5.value == 'e');
            ASSERT(r6.value == 'f');
        }
    },
                    // {"Chain", []()
                    //     {
                    //         auto parser = Function<char>([](const Position& _pos, StringStream& _stream) { return ParseResult<char>(_pos, _stream.GetChar()); });
                    //         ParserPtr<std::string> chain = Chain<char, std::string>(parser, [](ParseResult<char>&& _input)
                    //         {
                    //             return Function<std::string>([=](const Position& _pos, StringStream& _stream)
                    //             {
                    //                 return ParseResult<std::string>(_pos, std::string(1, _input.value));
                    //             });
                    //         });

                    //         ASSERT(chain->Parse("5").value == "5");
                    //         ASSERT(chain->Parse("6").value == "6");
                    //     }
                    // },
                    // {"Satisfy", []()
                    //     {
                    //         auto parser = Function<int>([](const Position& _pos, StringStream& _stream) { return ParseResult<int>(_pos, 5); });
                    //         auto onFail = [](const ParseResult<int>& _result) { return ParseError(_result.position, "Oh no!"); };

                    //         ASSERT(Satisfy<int>(parser, 5, onFail)->Parse("").value == 5);

                    //         try
                    //         {
                    //             Satisfy<int>(parser, 6, onFail)->Parse("");
                    //             ASSERT(false);
                    //         }
                    //         catch (const ParseError& e) {}
                    //     }
                    // },
                    // {"Success", []()
                    //     {
                    //         ParserPtr<int> parser = Function<int>([](const Position& _pos, StringStream& _stream)
                    //         {
                    //             if (_stream.GetChar() == 'b')
                    //                 throw ParseError(_pos, "");

                    //             return ParseResult<int>(_pos, 5);
                    //         });

                    //         ASSERT(Success(parser, 6)->Parse("a").value == 5);
                    //         ASSERT(Success(parser, 6)->Parse("b").value == 6);
                    //     }
                    // },
                    // {"Failure", []()
                    //     {
                    //         ParserPtr<int> parser = Function<int>([](const Position& _pos, StringStream& _stream)
                    //         {
                    //             if (_stream.PeekChar() == 'b')
                    //                 throw ParseError(_pos, "");

                    //             return ParseResult<int>(_pos, 5);
                    //         });

                    //         try { Failure(parser)->Parse("b"); }
                    //         catch (const ParseError& e) { ASSERT(false); }

                    //         try
                    //         {
                    //             Failure(parser)->Parse("a");
                    //             ASSERT(false);
                    //         }
                    //         catch (const ParseError& e) {}
                    //     }
                    // },
                    // {"Lexeme", []()
                    //     {
                    //         ASSERT(false);
                    //     }
                    // },
                    // {"Char", []()
                    //     {
                    //         ASSERT(false);
                    //     }
                    // },
                    // {"Chars", []()
                    //     {
                    //         ASSERT(false);
                    //     }
                    // },
                    // {"Letter", []()
                    //     {
                    //         ASSERT(false);
                    //     }
                    // },
                    // {"Letters", []()
                    //     {
                    //         ASSERT(false);
                    //     }
                    // },
                    // {"Digit", []()
                    //     {
                    //         ASSERT(false);
                    //     }
                    // },
                    // {"Digits", []()
                    //     {
                    //         ASSERT(false);
                    //     }
                    // },
                    // {"AlphaNum", []()
                    //     {
                    //         ASSERT(false);
                    //     }
                    // },
                    // {"AlphaNums", []()
                    //     {
                    //         ASSERT(false);
                    //     }
                    // },
                    // {"Whitespace", []()
                    //     {
                    //         ASSERT(false);
                    //     }
                    // },
                    // {"Whitespaces", []()
                    //     {
                    //         ASSERT(false);
                    //     }
                    // },
                    // {"EOS", []()
                    //     {
                    //         ASSERT(false);
                    //     }
                    // },
                    // {"Error", []()
                    //     {
                    //         ASSERT(false);
                    //     }
                    // },
                    // {"Optional", []()
                    //     {
                    //         ASSERT(false);
                    //     }
                    // },
                    // {"BinopChain", []()
                    //     {
                    //         ASSERT(false);
                    //     }
                    // },
                    // {"Fold", []()
                    //     {
                    //         ASSERT(false);
                    //     }
                    // },
                    // {"Choice", []()
                    //     {
                    //         ASSERT(false);
                    //     }
                    // },
                    // {"Named", []() { ASSERT((Named("MyParser", Char()) << EOS())->Parse("a").value == 'a'); } },
                    // {"Prefixed", []() { ASSERT((Prefixed(Digit(), Letter()) << EOS())->Parse("1b").value == 'b'); } },
                    // {"Suffixed", []() { ASSERT((Suffixed(Letter(), Digit()) << EOS())->Parse("b1").value == 'b'); } },
                    // {">>", []() { ASSERT((Digit() >> Letter() << EOS())->Parse("1b").value == 'b'); } },
                    // {"<<", []() { ASSERT((Letter() << Digit() << EOS())->Parse("b1").value == 'b'); } },
                    // {"+", []()
                    //     {
                    //         CountValue<char> results = ((Char('a') + (((Char('b') + Char('c')) + Char('d')) + (Char('e') + Char('f')))) << EOS())->Parse("abcdef").value;

                    //         ASSERT(results.size() == 6);
                    //         ASSERT(results[0].value == 'a');
                    //         ASSERT(results[1].value == 'b');
                    //         ASSERT(results[2].value == 'c');
                    //         ASSERT(results[3].value == 'd');
                    //         ASSERT(results[4].value == 'e');
                    //         ASSERT(results[5].value == 'f');
                    //     }
                    // },
                    // {"|", []()
                    //     {
                    //         ASSERT(false);
                    //     }
                    // },
                    // {"||", []()
                    //     {
                    //         ASSERT(false);
                    //     }
                    // },
                    // {"Between", []() { ASSERT((Between(Char('a'), Char('b'), Char('c')) << EOS())->Parse("abc").value == 'b'); } },
                    // {"LookAhead", []()
                    //     {
                    //         ASSERT(false);
                    //     }
                    // },
                    // {"Separate", []()
                    //     {
                    //         ASSERT(false);
                    //     }
                    // },
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
