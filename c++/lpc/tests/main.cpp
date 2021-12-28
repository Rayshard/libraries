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
    {"Function", []()
        {
            Function<int> parser = Function<int>([](const Position& _pos, StringStream& _stream) { return ParseResult<int>(_pos, 5); });
            ASSERT(parser.Parse("").value == 5);
        }
    },
    {"Reference", []()
        {
            Reference<char> parser;
            Function<char> function([=](const Position& _pos, StringStream& _stream) { return parser.Parse(_stream); });

            parser.Set(Char());
            ASSERT(parser.Parse("a") == function.Parse("a"));

            Reference<char> reference = parser;
            reference.Set(Char('b'));

            try
            {
                function.Parse("a");
                ASSERT(false);
            }
            catch(const ParseError& e) { }
        }
    },
    {"Map", []()
        {
            Function<int> parser = Function<int>([](const Position& _pos, StringStream& _stream) { return ParseResult<int>(_pos, 5); });
            ASSERT((Map<int, int>(parser, [](ParseResult<int>&& _input) { return 6; }).Parse("").value == 6));
        }
    },
    {"Chain", []()
        {
            Function<char> parser = Function<char>([](const Position& _pos, StringStream& _stream) { return ParseResult<char>(_pos, _stream.GetChar()); });
            Function<std::string> chain = Chain<char, std::string>(parser, [](ParseResult<char>&& _input)
            {
                return new Function<std::string>([=](const Position& _pos, StringStream& _stream) { return ParseResult<std::string>(_pos, std::string(1, _input.value)); });
            });

            ASSERT(chain.Parse("5").value == "5");
            ASSERT(chain.Parse("6").value == "6");
        }
    },
    {"Satisfy", []()
        {
            Function<int> parser = Function<int>([](const Position& _pos, StringStream& _stream) { return ParseResult<int>(_pos, 5); });
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
            Function<int> parser = Function<int>([](const Position& _pos, StringStream& _stream)
            {
                if (_stream.GetChar() == 'b')
                    throw ParseError(_pos, "");

                return ParseResult<int>(_pos, 5);
            });

            ASSERT(Success(parser, 6).Parse("a").value == 5);
            ASSERT(Success(parser, 6).Parse("b").value == 6);
        }
    },
    {"Failure", []()
        {
            Function<int> parser = Function<int>([](const Position& _pos, StringStream& _stream)
            {
                if (_stream.PeekChar() == 'b')
                    throw ParseError(_pos, "");

                return ParseResult<int>(_pos, 5);
            });

            try { Failure<int>(parser).Parse("b"); }
            catch (const ParseError& e) { ASSERT(false); }

            try
            {
                Failure<int>(parser).Parse("a");
                ASSERT(false);
            }
            catch (const ParseError& e) {}
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
    {"Error", []()
        {
            ASSERT(false);
        }
    },
    {"Try", []()
        {
            ASSERT(false);
        }
    },
    {"Count", []()
        {
            ASSERT(false);
        }
    },
    {"List", []()
        {
            Function<char> charParser = Char();
            auto [r1, r2, r3] = (List(std::tie(*(const Parser<char>*) & charParser, *(const Parser<char>*) & charParser, *(const Parser<char>*) & charParser)) << EOS()).Parse("abc").value;

            ASSERT(r1.value == 'a');
            ASSERT(r2.value == 'b');
            ASSERT(r3.value == 'c');
        }
    },
    {"&", []()
        {
            auto [r1, r2, r3, r4, r5, r6] = ((Char('a') & (((Char('b') & Char('c')) & Char('d')) & (Char('e') & Char('f')))) << EOS()).Parse("abcdef").value;

            ASSERT(r1.value == 'a');
            ASSERT(r2.value == 'b');
            ASSERT(r3.value == 'c');
            ASSERT(r4.value == 'd');
            ASSERT(r5.value == 'e');
            ASSERT(r6.value == 'f');
        }
    },
    {"Optional", []()
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
    {"Choice", []()
        {
            ASSERT(false);
        }
    },
    {"Named", []() { ASSERT((Named("MyParser", Char()) << EOS()).Parse("a").value == 'a'); } },
    {"Prefixed", []() { ASSERT((Prefixed(Digit(), Letter()) << EOS()).Parse("1b").value == 'b'); } },
    {"Suffixed", []() { ASSERT((Suffixed(Letter(), Digit()) << EOS()).Parse("b1").value == 'b'); } },
    {">>", []() { ASSERT((Digit() >> Letter() << EOS()).Parse("1b").value == 'b'); } },
    {"<<", []() { ASSERT((Letter() << Digit() << EOS()).Parse("b1").value == 'b'); } },
    {"+", []()
        {
            CountValue<char> results = ((Char('a') + (((Char('b') + Char('c')) + Char('d')) + (Char('e') + Char('f')))) << EOS()).Parse("abcdef").value;

            ASSERT(results.size() == 6);
            ASSERT(results[0].value == 'a');
            ASSERT(results[1].value == 'b');
            ASSERT(results[2].value == 'c');
            ASSERT(results[3].value == 'd');
            ASSERT(results[4].value == 'e');
            ASSERT(results[5].value == 'f');
        }
    },
    {"|", []()
        {
            ASSERT(false);
        }
    },
    {"Between", []() { ASSERT((Between(Char('a'), Char('b'), Char('c')) << EOS()).Parse("abc").value == 'b'); } },
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
