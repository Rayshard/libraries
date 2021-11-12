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

    class Regex : public std::regex
    {
        std::string string;
    public:
        Regex() : std::regex(), string() { }
        Regex(const std::string& _string) : std::regex(_string), string(_string) { }

        const std::string& GetString() { return string; }
    };

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

        Position GetPosition(size_t _offset)
        {
            if (_offset > data.size())
                throw std::runtime_error("Offset is out of range of data!");

            size_t line = 0, closestLineStart = 0;
            for (size_t lineStart : lineStarts)
            {
                if (lineStart > _offset)
                    break;

                closestLineStart = lineStart;
                line++;
            }

            return Position{ line, _offset - closestLineStart + 1 };
        }

        Position GetPosition() { return GetPosition(offset); }

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
        inline static PatternID EOF_PATTERN_ID() { return 0; }
        inline static PatternID UNKNOWN_PATTERN_ID() { return 1; }

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
            Regex regex;
            Action action;
        };

        std::vector<Pattern> patterns;
        Pattern eof, unknown;

    public:
        Lexer(Action _onEOF, Action _onUnknown) : patterns()
        {
            eof = { .id = EOF_PATTERN_ID(), .regex = Regex(), .action = _onEOF };
            unknown = { .id = UNKNOWN_PATTERN_ID(), .regex = Regex(), .action = _onUnknown };
        }

        PatternID AddPattern(Regex _regex, Action _action = std::monostate())
        {
            patterns.push_back(Pattern{ .id = patterns.size() + 2, .regex = _regex, .action = _action });
            return patterns.back().id;
        }

        Result Lex(Stream& _stream, bool _skipNonTokens = true)
        {
            Pattern* matchingPattern = &unknown;
            Match match{ .value = "", .position = _stream.GetPosition() };

            if (_stream.IsEOF())
            {
                match.value = std::string(1, (char)EOF);
                matchingPattern = &eof;
            }
            else
            {
                for (auto& pattern : patterns)
                {
                    std::smatch regexMatch;
                    std::regex_search(_stream.Current(), _stream.End(), regexMatch, pattern.regex, std::regex_constants::match_continuous);

                    if (regexMatch.size() == 0)
                        continue;

                    match.value = regexMatch.str();
                    _stream.Ignore(regexMatch.length()); //Advance stream past matched substring
                    matchingPattern = &pattern;
                }
            }

            if (matchingPattern == &unknown)
                match.value = std::string(1, _stream.Get());

            Result result = {
                .patternID = matchingPattern->id,
                .value = std::any(),
                .match = match
            };

            if (auto action = std::get_if<Tokenizer>(&matchingPattern->action)) { result.value = (*action)(_stream, match); }
            else if (auto action = std::get_if<Procedure>(&matchingPattern->action)) { (*action)(_stream, match); }

            return result;
        }
    };
}