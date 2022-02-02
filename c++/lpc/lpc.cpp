#include "lpc.h"
#include <cassert>

namespace lpc
{
    bool Position::operator==(const Position& _other) const { return line == _other.line && column == _other.column; }
    bool Position::operator!=(const Position& _other) const { return !(*this == _other); }

    std::string Position::ToString() const { return "(" + std::to_string(line) + ", " + std::to_string(column) + ")"; }

    std::ostream& operator<<(std::ostream& _lhs, const Position& _rhs) { return _lhs << _rhs.ToString(); }

    std::string IStreamToString(std::istream& _stream)
    {
        std::streampos start = _stream.tellg();
        std::string string = std::string((std::istreambuf_iterator<char>(_stream)), std::istreambuf_iterator<char>());
        _stream.seekg(start);
        return string;
    }

#pragma region StringStream
    StringStream::StringStream(const std::string& _data) : offset(0), data(_data), lineStarts()
    {
        //Get line starts
        lineStarts.push_back(0);

        for (size_t position = 0; position < data.size(); position++)
        {
            if (data[position] == '\n')
                lineStarts.push_back(position + 1);
        }
    }

    char StringStream::Peek()
    {
        size_t initOffset = offset;
        char peek = Get();
        offset = initOffset;
        return peek;
    }

    char StringStream::Get() { return IsEOS() ? EOF : data[offset++]; }
    void StringStream::Ignore(size_t _amt) { offset = std::min(data.size(), offset + _amt); }

    void StringStream::SetOffset(size_t _offset) { offset = std::min(_offset, data.size()); } //Recall that size_t is unsigned so no need to bound below
    size_t StringStream::GetOffset() const { return offset; }

    size_t StringStream::GetOffset(const Position& _pos) const
    {
        if (_pos.line > lineStarts.size() || _pos.line == 0 || _pos.column == 0)
            throw std::runtime_error("Invaild position: " + _pos.ToString());

        size_t lineStart = lineStarts[_pos.line - 1];
        size_t lineWidth = (_pos.line == lineStarts.size() ? data.size() : lineStarts[_pos.line]) - lineStart;

        if (_pos.column - 1 > lineWidth)
            throw std::runtime_error("Invaild position: " + _pos.ToString());

        return lineStart + _pos.column - 1;
    }

    std::string::iterator StringStream::Begin() { return data.begin(); }
    std::string::iterator StringStream::End() { return data.end(); }
    std::string::iterator StringStream::Current() { return data.begin() + offset; }
    std::string::const_iterator StringStream::CBegin() const { return data.cbegin(); }
    std::string::const_iterator StringStream::CEnd() const { return data.cend(); }
    std::string::const_iterator StringStream::CCurrent() const { return data.cbegin() + offset; }

    std::string StringStream::GetData(size_t _start, size_t _length) const
    {
        if (_start + _length > data.size())
            throw std::runtime_error("Parameters out of range of data!");

        return data.substr(_start, _length);
    }

    std::string StringStream::GetData(size_t _start) const { return GetData(_start, data.size() - _start); }

    Position StringStream::GetPosition(size_t _offset) const
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

    Position StringStream::GetPosition() const { return GetPosition(offset); }
    void StringStream::SetPosition(Position _pos) { offset = GetOffset(_pos); }
    bool StringStream::IsEOS() const { return offset >= data.size(); }
#pragma endregion

#pragma region Regex
    Regex::Regex() : std::regex(), string() { }
    Regex::Regex(const std::string& _string) : std::regex(_string), string(_string) { }

    const std::string& Regex::GetString() const { return string; }
#pragma endregion

#pragma region ParseError
    ParseError::ParseError(Position _pos, const std::string& _msg, const std::vector<ParseError>& _trace)
        : std::runtime_error("Error @ " + _pos.ToString() + ": " + _msg), position(_pos), message(_msg), trace(_trace) { }

    ParseError::ParseError(const ParseError& _e1, const ParseError& _e2)
        : ParseError(_e1.position, _e1.message)
    {
        trace.insert(trace.end(), _e1.trace.begin(), _e1.trace.end());
        trace.push_back(_e2);
    }

    ParseError::ParseError() : ParseError(Position(), "") { }

    std::string ParseError::GetMessageWithTrace() const
    {
        std::string traceMessage;

        for (const ParseError& e : trace)
            traceMessage += "\n\t" + std::regex_replace(e.GetMessageWithTrace(), std::regex("\\n"), "\n\t");

        return what() + traceMessage;
    }

    const Position& ParseError::GetPosition() const { return position; }
    const std::string& ParseError::GetMessage() const { return message; }
    const std::vector<ParseError>& ParseError::GetTrace() const { return trace; }

    bool ParseError::operator==(const ParseError& _other) const { return position == _other.position && message == _other.message && trace == _other.trace; }
    bool ParseError::operator!=(const ParseError& _other) const { return !(*this == _other); }


    ParseError ParseError::Expectation(const std::string& _expected, const std::string& _found, const Position& _pos)
    {
        return ParseError(_pos, "Expected " + _expected + ", but found " + _found);
    }
#pragma endregion

    namespace parsers
    {
        // namespace lexing
        // {
        //     //         Lexer::Lexer(Action _onEOS, Action _onUnknown)
        //     //             : patterns(), patternsMap(), patternEOS(CreatePattern(EOS_PATTERN_ID(), Regex(), _onEOS)), patternUnknown(CreatePattern(UNKNOWN_PATTERN_ID(), Regex("[\\S\\s]"), _onUnknown)) { }

        //     //         PatternID Lexer::AddPattern(const PatternID& _id, const Regex& _regex, Action _action)
        //     //         {
        //     //             if (_id.empty()) { throw std::runtime_error("_id cannot be empty!"); }
        //     //             else if (HasPattern(_id)) { throw std::runtime_error("Pattern with id '" + _id + "' already exists!"); }

        //     //             patterns.push_back(CreatePattern(_id, _regex, _action));
        //     //             patternsMap[_id] = patterns.size() - 1;
        //     //             return _id;
        //     //         }

        //     //         PatternID Lexer::AddPattern(Regex _regex, Action _action) { return AddPattern("<Pattern: " + std::to_string(patterns.size()) + ">", _regex, _action); }

        //     //         bool Lexer::HasPattern(const PatternID& _id) const { return _id == EOS_PATTERN_ID() || _id == UNKNOWN_PATTERN_ID() || patternsMap.contains(_id); }

        //     //         Parser<std::string> Lexer::CreateLexeme(const PatternID& _id, const std::set<PatternID>& _ignores, std::optional<std::string> _value) const
        //     //         {
        //     //             if (!HasPattern(_id)) { throw std::runtime_error("Pattern with id '" + _id + "' does not exist!"); }
        //     //             else { return Lexeme(*this, _id, _ignores, _value); }
        //     //         }

        //     //         void Lexer::OnLexUnknown(StringStream& _stream, const ParseResult<Token>& _result) { throw ParseError(_result.position, "Unrecognized token: '" + _result.value.value); }

        //     //         ParseResult<Token> Lexer::OnParse(const Position& _pos, StringStream& _stream)
        //     //         {
        //     //             ParseResult<PatternParseValue> patternParseResult;

        //     //             if (_stream.IsEOS()) { patternParseResult = patternEOS.Parse(_stream); }
        //     //             else
        //     //             {
        //     //                 Longest<PatternParseValue> longest(std::move(patterns));

        //     //                 try { patternParseResult = longest.OnParse(_stream.GetPosition(), _stream); }
        //     //                 catch (const ParseError& e) { patternParseResult = patternUnknown.Parse(_stream); }

        //     //                 patterns = std::move(longest.parsers);
        //     //             }

        //     //             auto& [token, patternAction] = patternParseResult.value;
        //     //             ParseResult<Token> result(std::move(patternParseResult.position), std::move(token));

        //     //             if (const lexing::Function* action = std::get_if<lexing::Function>(&patternAction)) { result.value.value = (*action)(_stream, result); }
        //     //             else if (const Procedure* action = std::get_if<Procedure>(&patternAction)) { (*action)(_stream, result); }

        //     //             return result;
        //     //         }

        //     //         Pattern Lexer::CreatePattern(const PatternID& _id, const Regex& _regex, Action _action)
        //     //         {
        //     //             return Map<std::string, PatternParseValue>(Lexeme(_regex), [=](ParseResult<std::string>& _result)
        //     //                 {
        //     //                     return PatternParseValue
        //     //                     {
        //     //                         .token = Token{.patternID = _id,
        //     //                         .value = _result.value}, .action = _action
        //     //                     };
        //     //                 });
        //     //         }
        //     //     }

        //     //     Parser<std::string> Lexeme(lexing::Lexer _lexer, const lexing::PatternID& _id, const std::set<lexing::PatternID>& _ignores, std::optional<std::string> _value)
        //     //     {
        //     //         return Parser<std::string>([=](const Position& _pos, StringStream& _stream)
        //     //             {
        //     //                 lexing::Lexer lexer = _lexer;
        //     //                 ParseResult<lexing::Token> token;

        //     //                 while (_ignores.contains((token = lexer.OnParse(_stream.GetPosition(), _stream)).value.patternID))
        //     //                     ;

        //     //                 auto& [patternID, tokenValue] = token.value;

        //     //                 if (patternID != _id)
        //     //                 {
        //     //                     std::string expected = "'" + _id + (_value.has_value() ? "(" + _value.value() + ")" : "") + "'";
        //     //                     std::string found = "'" + patternID + (tokenValue.empty() ? "" : "(" + tokenValue + ")") + "'";
        //     //                     throw ParseError::Expectation(expected, found, token.position);
        //     //                 }
        //     //                 else if (_value.has_value() && tokenValue != _value.value()) { throw ParseError::Expectation("'" + _value.value() + "'", "'" + tokenValue + "'", token.position); }

        //     //                 return ParseResult<std::string>(token.position, tokenValue);
        //     //             });
        // }

        Parser<std::string> Lexeme(const Regex& _regex, std::optional<std::string> _value)
        {
            return Parser<std::string>([=](const Position& _pos, StringStream& _stream)
                {
                    const char* current = &*_stream.CCurrent();
                    const char* end = &*_stream.CEnd();
                    std::cmatch regexMatch;
                    std::regex_search(current, end, regexMatch, _regex, std::regex_constants::match_continuous);

                    if (regexMatch.size() == 0)
                        throw ParseError(_pos, "No match found for regular expression: " + _regex.GetString());

                    std::string match = regexMatch.str();

                    if (_value.has_value() && match != _value.value())
                        throw ParseError::Expectation("'" + _value.value() + "'", "'" + match + "'", _pos);

                    _stream.Ignore(match.length());
                    return ParseResult<std::string>(_pos, match);
                });
        }

        Parser<std::string> Chars(std::optional<std::string> _value) { return Lexeme(Regex("[\\S\\s]+"), _value); }
        Parser<std::string> Letters(std::optional<std::string> _value) { return Lexeme(Regex("[a-zA-Z]+"), _value); }
        Parser<std::string> Digits(std::optional<std::string> _value) { return Lexeme(Regex("[0-9]+"), _value); }
        Parser<std::string> AlphaNums(std::optional<std::string> _value) { return Lexeme(Regex("[a-zA-Z0-9]+"), _value); }
        Parser<std::string> Whitespaces(std::optional<std::string> _value) { return Lexeme(Regex("\\s+"), _value); }

        Parser<char> Char(const Regex& _regex, std::optional<char> _value)
        {
            std::optional<std::string> value = _value.has_value() ? std::optional(std::string(1, _value.value())) : std::nullopt;
            return Map<std::string, char>(Lexeme(_regex, value), [](const ParseResult<std::string>& _result) { return _result.value[0]; });
        }

        Parser<char> Letter(std::optional<char> _value) { return Char(Regex("[a-zA-Z]"), _value); }
        Parser<char> Digit(std::optional<char> _value) { return Char(Regex("[0-9]"), _value); }
        Parser<char> AlphaNum(std::optional<char> _value) { return Char(Regex("[a-zA-Z0-9]"), _value); }
        Parser<char> Whitespace(std::optional<char> _value) { return Char(Regex("\\s"), _value); }
        Parser<char> Char(std::optional<char> _value) { return Char(Regex("[\\S\\s]"), _value); }

        // ParserPtr<std::monostate> EOS()
        // {
        //     return Function<std::monostate>([=](const Position& _pos, StringStream& _stream)
        //         {
        //             if (!_stream.IsEOS())
        //                 throw ParseError::Expectation("'" + lexing::EOS_PATTERN_ID() + "'", "'" + std::string(1, _stream.PeekChar()) + "'", _pos);

        //             return ParseResult(_pos, std::monostate());
        //         });
        // }

        // ParserPtr<std::monostate> Error(const std::string& _message)
        // {
        //     return Function<std::monostate>([=](const Position& _pos, StringStream& _stream)
        //         {
        //             throw ParseError(_stream.GetPosition(), _message);
        //             return ParseResult<std::monostate>(_pos, std::monostate());
        //         });
        // }
    }
}
