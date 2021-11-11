#pragma once
#include <regex>
#include <variant>
#include <map>
#include <optional>

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
        const std::string& data;
        std::vector<size_t> newLines;

    public:
        size_t offset;

        Stream(const std::string& _data, size_t _offset = 0) : data(_data), offset(_offset)
        {
            //Find new lines 
            newLines.push_back(0);
            size_t pos = _data.find("\n", 0);
            while (pos != std::string::npos)
            {
                newLines.push_back(pos);
                pos = _data.find("\n", pos + 1);
            }
        }

        char Peek() { return IsEOF() ? (const char)EOF : data[offset]; }
        char Get() { return IsEOF() ? (const char)EOF : data[offset++]; }
        void Ignore(size_t _amt) { offset += _amt; }

        std::string::const_iterator Begin() { return data.begin(); }
        std::string::const_iterator End() { return data.end(); }
        std::string::const_iterator Current() { return data.begin() + offset; }
        bool IsEOF() { return offset >= data.size(); }

        Position GetPosition()
        {
            Position position;

            for (size_t line = 0; line < newLines.size(); line++)
            {
                size_t newLineOffset = newLines[line];

                if (offset >= newLineOffset)
                    position = Position{ .line = line, .column = offset - newLineOffset };
            }

            return position;
        }
    };

    template<typename Token>
    class Lexer
    {
    public:
        struct Match
        {
            std::string value;
            Position position;
        };

        typedef void(*Procedure)(Stream&, const Match&);
        typedef Token(*Tokenizer)(Stream&, const Match&);
        typedef std::variant<Procedure, Tokenizer, std::monostate> Action;
        typedef std::string PatternID;

        struct Result
        {
            PatternID patternID;
            Token token;
        };
    private:
        struct Pattern
        {
            PatternID id;
            std::regex regex;
            Action action;
        };

        std::vector<Pattern> patterns;
        std::map<std::string, size_t> namedPatterns;
        Pattern eof, invalid;

    public:
        Lexer(Tokenizer _eofTokenizer, Tokenizer _invalidTokenizer, PatternID _eofPatternID = "<EOF>", PatternID _invalidPatternID = "<INVALID>")
            : patterns(), namedPatterns()
        {
            if (_eofPatternID.empty()) { throw std::runtime_error("EOF token pattern id must be non-empty!"); }
            else if (_invalidPatternID.empty()) { throw std::runtime_error("Invalid token pattern id must be non-empty!"); }
            else if (_eofPatternID == _invalidPatternID) { throw std::runtime_error("EOF and invalid token pattern ids must be distinct!"); }

            eof = { .id = _eofPatternID, .regex = std::regex(), .action = _eofTokenizer };
            invalid = { .id = _invalidPatternID, .regex = std::regex(), .action = _invalidTokenizer };
        }

        void AddPattern(std::regex _regex) { patterns.push_back(Pattern{ .id = "", .regex = _regex, .action = std::monostate() }); }
        void AddPattern(std::regex _regex, Procedure _prod) { patterns.push_back(Pattern{ .id = "", .regex = _regex, .action = _prod }); }

        void AddPattern(PatternID _id, std::regex _regex, Action _action)
        {
            if (_id.empty())
                throw std::runtime_error("ID must be non-empty!");

            auto search = namedPatterns.find(_id);
            if (search != namedPatterns.end() || _id == eof.id)
                throw std::runtime_error("Pattern with name '" + _id + "' already exists.");

            namedPatterns[_id] = patterns.size();
            patterns.push_back(Pattern{ .id = _id, .regex = _regex, .action = _action });
        }

        Result Lex(Stream& _stream)
        {
            Pattern* matchingPattern = &invalid;
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

                    if (itPattern->id.empty()) //Pattern does not define a token
                    {
                        if (auto procedure = std::get_if<Procedure>(&itPattern->action))
                            (*procedure)(_stream, match);

                        match.position = _stream.GetPosition(); //Update match start position for next match

                        if (_stream.IsEOF())
                        {
                            matchingPattern = &eof;
                            break;
                        }

                        itPattern = patterns.begin();
                        continue;
                    }

                    matchingPattern = &*itPattern;
                    break;
                }
            }

            if (matchingPattern->id == eof.id) { match.value = std::string(1, (char)EOF); }
            else if (matchingPattern->id == invalid.id) { match.value = std::string(1, _stream.Get()); }

            auto tokenizer = std::get<Tokenizer>(matchingPattern->action);
            return Result{ .patternID = matchingPattern->id, .token = (*tokenizer)(_stream, match) };
        }
    };
}