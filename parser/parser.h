#pragma once
#include <regex>
#include <variant>
#include <map>
#include <optional>
#include <any>
#include <fstream>
#include <iostream> //TODO: remove

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

    template<typename T>
    class IStream
    {
    protected:
        std::vector<T> data;
        T eof;
    public:
        size_t offset;

        IStream(std::vector<T>&& _data, T _eof) : data(std::move(_data)), eof(_eof), offset(0) { }

        T Peek() { return IsEOF() ? eof : data[offset]; }
        T Get() { return IsEOF() ? eof : data[offset++]; }
        void Ignore(size_t _amt) { offset += _amt; }

        typename std::vector<T>::iterator Begin() { return data.begin(); }
        typename std::vector<T>::iterator End() { return data.end(); }
        typename std::vector<T>::iterator Current() { return data.begin() + offset; }

        typename std::vector<T>::iterator CBegin() const { return Begin(); }
        typename std::vector<T>::iterator CEnd() const { return End(); }
        typename std::vector<T>::iterator CCurrent() const { return Current(); }

        bool IsEOF() { return offset >= data.size(); }
    };

    class Stream : public IStream<char>
    {
        std::vector<size_t> lineStarts;

    public:
        Stream(std::string&& _data) : IStream(std::vector<char>(_data.begin(), _data.end()), (char)EOF)
        {
            //Get line starts
            lineStarts.push_back(0);

            for (size_t position = 0; position < _data.size(); position++)
            {
                if (_data[position] == '\n')
                    lineStarts.push_back(position + 1);
            }
        }

        Stream(std::istream& _stream) : Stream(std::string((std::istreambuf_iterator<char>(_stream)), std::istreambuf_iterator<char>())) { }

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

    // class Lexer
    // {
    // public:
    //     typedef size_t PatternID;
    //     inline static PatternID EOF_PATTERN_ID() { return 0; }
    //     inline static PatternID UNKNOWN_PATTERN_ID() { return 1; }

    //     struct Match
    //     {
    //         std::string value;
    //         Position position;
    //     };

    //     typedef void(*Procedure)(Stream&, const Match&);
    //     typedef std::any(*Tokenizer)(Stream&, const Match&);
    //     typedef std::variant<Procedure, Tokenizer, std::monostate> Action;

    //     struct Result
    //     {
    //         PatternID patternID;
    //         std::any value;
    //         Match match;
    //     };

    // private:
    //     struct Pattern
    //     {
    //         PatternID id;
    //         Regex regex;
    //         Action action;
    //     };

    //     std::vector<Pattern> patterns;
    //     Pattern eof, unknown;

    // public:
    //     Lexer(Action _onEOF, Action _onUnknown) : patterns()
    //     {
    //         eof = { .id = EOF_PATTERN_ID(), .regex = Regex(), .action = _onEOF };
    //         unknown = { .id = UNKNOWN_PATTERN_ID(), .regex = Regex(), .action = _onUnknown };
    //     }

    //     PatternID AddPattern(Regex _regex, Action _action = std::monostate())
    //     {
    //         patterns.push_back(Pattern{ .id = patterns.size() + 2, .regex = _regex, .action = _action });
    //         return patterns.back().id;
    //     }

    //     Result Lex(Stream& _stream)
    //     {
    //         Pattern* matchingPattern = nullptr;
    //         Match match{ .value = std::string(1, (char)_stream.Peek()), .position = _stream.GetPosition() };

    //         if (_stream.IsEOF()) { matchingPattern = &eof; }
    //         else
    //         {
    //             for (auto& pattern : patterns)
    //             {
    //                 const char* current = &*_stream.Current();
    //                 const char* end = &*_stream.End();
    //                 std::cmatch regexMatch;
    //                 std::regex_search(current, end, regexMatch, pattern.regex, std::regex_constants::match_continuous);

    //                 if (regexMatch.size() == 0)
    //                     continue;

    //                 //If this is the first match or this match has a strictly greater length than
    //                 //any previous match, set it as the match 
    //                 std::string matchStr = regexMatch.str();
    //                 if (!matchingPattern || matchStr.size() > match.value.size())
    //                 {
    //                     match.value = matchStr;
    //                     matchingPattern = &pattern;
    //                 }
    //             }

    //             if (!matchingPattern)
    //                 matchingPattern = &unknown;
    //         }

    //         _stream.Ignore(match.value.size()); //Advance stream past matched substring

    //         Result result = {
    //             .patternID = matchingPattern->id,
    //             .value = std::any(),
    //             .match = match
    //         };

    //         if (auto action = std::get_if<Tokenizer>(&matchingPattern->action)) { result.value = (*action)(_stream, match); }
    //         else if (auto action = std::get_if<Procedure>(&matchingPattern->action)) { (*action)(_stream, match); }

    //         return result;
    //     }
    // };

    template<typename Unit, typename PatternTemplate>
    class PatternMatcher
    {
    public:
        typedef std::optional<size_t>(*Matcher)(const IStream<Unit>&, const PatternTemplate&);

        struct Match { size_t start, size; };

        typedef size_t PatternID;
        inline static PatternID EOF_PATTERN_ID() { return 0; }
        inline static PatternID UNKNOWN_PATTERN_ID() { return 1; }

        typedef void(*Procedure)(IStream<Unit>&, const Match&);
        typedef std::any(*Function)(IStream<Unit>&, const Match&);
        typedef std::variant<Procedure, Function, std::monostate> Action;

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
            PatternTemplate pTemplate;
            Action action;
        };

        Matcher matcher;
        std::vector<Pattern> patterns;
        Pattern eof, unknown;

    public:
        PatternMatcher(Matcher _matcher, Action _onEOF, Action _onUnknown) : matcher(_matcher), patterns()
        {
            eof = { .id = EOF_PATTERN_ID(), .pTemplate = PatternTemplate(), .action = _onEOF };
            unknown = { .id = UNKNOWN_PATTERN_ID(), .pTemplate = PatternTemplate(), .action = _onUnknown };
        }

        PatternID AddPattern(PatternTemplate _template, Action _action = std::monostate())
        {
            patterns.push_back(Pattern{ .id = patterns.size() + 2, .pTemplate = _template, .action = _action });
            return patterns.back().id;
        }

        Result GetMatch(IStream<Unit>& _stream)
        {
            Pattern* matchingPattern = nullptr;
            Match match{ .start = _stream.offset, .size = 0 };

            if (_stream.IsEOF()) { matchingPattern = &eof; }
            else
            {
                for (auto& pattern : patterns)
                {
                    auto matchLength = matcher(_stream, pattern.pTemplate); 
                    if (!matchLength.has_value())
                        continue;

                    //If this is the first match or this match has a strictly greater length than
                    //any previous match, set it as the match 
                    if (!matchingPattern || matchLength > match.size)
                    {
                        match.size = matchLength;
                        matchingPattern = &pattern;
                    }
                }

                if (!matchingPattern)
                {
                    matchingPattern = &unknown;
                    match.size = 1;
                }
            }

            _stream.Ignore(match.size); //Advance stream past match

            Result result = {
                .patternID = matchingPattern->id,
                .value = std::any(),
                .match = match
            };

            if (auto action = std::get_if<Function>(&matchingPattern->action)) { result.value = (*action)(_stream, match); }
            else if (auto action = std::get_if<Function>(&matchingPattern->action)) { (*action)(_stream, match); }

            return result;
        }
    };

    class Lexer : public PatternMatcher<char, Regex> 
    {
        static std::optional<size_t> Matcher(const IStream<char>& _stream, const Regex& _regex)
        {
            const char* current = &*_stream.CCurrent();
            const char* end = &*_stream.CEnd();
            std::cmatch regexMatch;
            std::regex_search(current, end, regexMatch, _regex, std::regex_constants::match_continuous);

            return regexMatch.size() == 0 ? std::optional<size_t>() : regexMatch.length();
        }

        public:
        Lexer(Action _onEOF, Action _onUnknown) : PatternMatcher<char, Regex>(Matcher, _onEOF, _onUnknown) { }
    };
}