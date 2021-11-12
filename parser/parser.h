#pragma once
#include <regex>
#include <variant>
#include <map>
#include <optional>
#include <any>
#include <fstream>

namespace parser
{
    namespace Error
    {
        std::runtime_error UnknownSymbol(const std::string& _name) { return std::runtime_error("Unknown symbol: " + _name); }
        std::runtime_error DuplicateSymbolName(const std::string& _name) { return std::runtime_error("Duplicate symbol name: " + _name); }
        std::runtime_error CannotParseSymbol(const std::string& _name) { return std::runtime_error("Unable to parse symbol: " + _name); }
    }

    struct Position
    {
        size_t line, column;

        std::string ToString() const { return "(" + std::to_string(line) + ", " + std::to_string(column) + ")"; }
    };

    std::ostream& operator<<(std::ostream& _lhs, const Position& _rhs) {
        return _lhs << _rhs.ToString();
    }

    class Stream
    {
        std::string data;
        std::vector<size_t> lineStarts;

    public:
        size_t offset;

        Stream(std::string&& _data, size_t _offset = 0) : data(std::move(_data)), offset(_offset)
        {
            //Get line starts
            lineStarts.push_back(0);

            for (size_t position = 0; position < data.size(); position++)
            {
                if (data[position] == '\n')
                    lineStarts.push_back(position + 1);
            }
        }

        Stream(std::istream& _stream) : Stream(std::string((std::istreambuf_iterator<char>(_stream)), std::istreambuf_iterator<char>())) { }

        char Peek() { return IsEOF() ? (const char)EOF : data[offset]; }
        char Get() { return IsEOF() ? (const char)EOF : data[offset++]; }
        void Ignore(size_t _amt) { offset += _amt; }

        std::string::const_iterator Begin() { return data.begin(); }
        std::string::const_iterator End() { return data.end(); }
        std::string::const_iterator Current() { return data.begin() + offset; }
        bool IsEOF() { return offset >= data.size(); }

        Position GetPosition()
        {
            if (offset > data.size())
                throw std::runtime_error("Offset is out of range of data!");

            size_t line = 0, closestLineStart = 0;
            for (size_t lineStart : lineStarts)
            {
                if (lineStart > offset)
                    break;

                closestLineStart = lineStart;
                line++;
            }

            return Position{ line, offset - closestLineStart + 1 };
        }

        void SetPosition(Position _pos)
        {
            if (_pos.line > lineStarts.size() || _pos.line == 0 || _pos.column == 0)
                throw std::runtime_error("Invaild position: " + _pos.ToString());

            size_t lineStart = lineStarts[_pos.line - 1];
            size_t lineWidth = (_pos.line == lineStarts.size() ? data.size() : lineStarts[_pos.line]) - lineStart;

            if (_pos.column - 1 > lineWidth)
                throw std::runtime_error("Invaild position: " + _pos.ToString());

            offset = lineStart + _pos.column - 1;
        }
    };

    class Lexer
    {
    public:
        typedef size_t PatternID;
        static const PatternID EOF_PATTERN_ID = 0;
        static const PatternID INVALID_PATTERN_ID = 1;

        struct Match
        {
            std::string value;
            Position position;
        };

        typedef void(*Procedure)(Stream&, const Match&);
        typedef std::any(*Tokenizer)(Stream&, const Match&);
        typedef std::variant<Procedure, Tokenizer, std::monostate> Action;

        struct Result
        {
            PatternID patternID;
            std::any value;
            Match match;
        };

    private:
        struct Pattern
        {
            PatternID id;
            std::regex regex;
            Action action;
        };

        std::vector<Pattern> patterns;
        Pattern eof, unknown;

    public:
        Lexer(Action _onEOF, Action _onUnknown) : patterns()
        {
            eof = { .id = EOF_PATTERN_ID, .regex = std::regex(), .action = _onEOF };
            unknown = { .id = INVALID_PATTERN_ID, .regex = std::regex(), .action = _onUnknown };
        }

        PatternID AddPattern(std::regex _regex, Action _action = std::monostate())
        {
            patterns.push_back(Pattern{ .id = patterns.size() + 2, .regex = _regex, .action = _action });
            return patterns.back().id;
        }

        Result Lex(Stream& _stream, bool _skipNonTokens = true)
        {
            Pattern* matchingPattern = &unknown;
            Match match{ .value = "", .position = _stream.GetPosition() };

            if (_stream.IsEOF()) { matchingPattern = &eof; }
            else if (!patterns.empty())
            {
                auto itPattern = patterns.begin();
                while (itPattern != patterns.end())
                {
                    std::smatch regexMatch;
                    std::regex_search(_stream.Current(), _stream.End(), regexMatch, itPattern->regex, std::regex_constants::match_continuous);

                    if (regexMatch.size() == 0)
                    {
                        itPattern++;
                        continue;
                    }

                    match.value = regexMatch.str();
                    _stream.Ignore(regexMatch.length()); //Advance stream past matched substring

                    if (std::get_if<Tokenizer>(&itPattern->action))
                    {
                        matchingPattern = &*itPattern;
                        break;
                    }

                    if (auto procedure = std::get_if<Procedure>(&itPattern->action))
                        (*procedure)(_stream, match);

                    if (!_skipNonTokens)
                    {
                        matchingPattern = &*itPattern;
                        break;
                    }

                    match.position = _stream.GetPosition(); //Update match start position for next match

                    if (_stream.IsEOF())
                    {
                        matchingPattern = &eof;
                        break;
                    }

                    itPattern = patterns.begin();
                }
            }

            if (matchingPattern->id == EOF_PATTERN_ID) { match.value = std::string(1, (char)EOF); }
            else if (matchingPattern->id == INVALID_PATTERN_ID) { match.value = std::string(1, _stream.Get()); }

            Result result = {
                .patternID = matchingPattern->id,
                .value = std::any(),
                .match = match
            };

            if (auto tokenizer = std::get_if<Tokenizer>(&matchingPattern->action))
                result.value = (*std::get<Tokenizer>(matchingPattern->action))(_stream, match);

            return result;
        }
    };
}