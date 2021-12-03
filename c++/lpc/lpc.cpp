#include "lpc.h"

namespace lpc
{
    std::string Position::ToString() const { return "(" + std::to_string(line) + ", " + std::to_string(column) + ")"; }

    std::ostream& operator<<(std::ostream& _lhs, const Position& _rhs) { return _lhs << _rhs.ToString(); }

    std::string IStreamToString(std::istream& _stream)
    {
        auto start = _stream.tellg();
        auto string = std::string((std::istreambuf_iterator<char>(_stream)), std::istreambuf_iterator<char>());
        _stream.seekg(start);
        return string;
    }

    Regex::Regex() : std::regex(), string() { }
    Regex::Regex(const std::string& _string) : std::regex(_string), string(_string) { }

    const std::string& Regex::GetString() const { return string; }

    StringStream::StringStream(const std::string& _data, const Lexer& _lexer, const std::set<Lexer::PatternID>& _ignores)
        : offset(0), data(_data), lineStarts(), lexer(_lexer), tokens(), ignores(_ignores)
    {
        if (ignores.contains(Lexer::EOS_PATTERN_ID()))
            throw std::runtime_error(Lexer::EOS_PATTERN_ID() + " cannot be ignored!");

        //Get line starts
        lineStarts.push_back(0);

        for (size_t position = 0; position < data.size(); position++)
        {
            if (data[position] == '\n')
                lineStarts.push_back(position + 1);
        }
    }

    StringStream::StringStream(const std::string& _data) : StringStream(_data, Lexer()) { }

    char StringStream::PeekChar()
    {
        auto initOffset = offset;
        auto peek = GetChar();
        offset = initOffset;
        return peek;
    }

    char StringStream::GetChar() { return IsEOS() ? EOF : data[offset++]; }
    void StringStream::IgnoreChars(size_t _amt) { offset = std::min(data.size(), offset + _amt); }

    const Lexer::Token& StringStream::GetToken()
    {
        while (true)
        {
            Lexer::Token* token;
            auto search = tokens.find(offset);

            if (search != tokens.end())
            {
                auto& pair = search->second;

                token = &std::get<0>(pair);
                offset += pair.second;
            }
            else
            {
                auto start = offset;
                token = &tokens.emplace(start, std::pair{ lexer.Lex(*this), offset - start }).first->second.first;
            }

            if (!ignores.contains(token->patternID))
                return *token;
        }
    }

    const Lexer::Token& StringStream::PeekToken()
    {
        auto initOffset = offset;
        auto& peek = GetToken();
        offset = initOffset;
        return peek;
    }

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

    bool Lexer::Token::IsEOS() const { return patternID == EOS_PATTERN_ID(); }
    bool Lexer::Token::IsUnknown() const { return patternID == UNKNOWN_PATTERN_ID(); }

    Lexer::Lexer(Action _onEOS, Action _onUnknown) : patterns(), patternsMap()
    {
        patternEOS = { .id = EOS_PATTERN_ID(), .regex = Regex(), .action = _onEOS };
        patternUnknown = { .id = UNKNOWN_PATTERN_ID(), .regex = Regex(), .action = _onUnknown };

        patternsMap[patternEOS.id] = &patternEOS;
        patternsMap[patternUnknown.id] = &patternUnknown;
    }

    const Lexer::Pattern& Lexer::AddPattern(PatternID _id, Regex _regex, Action _action)
    {
        assert(!_id.empty() && "PatternID cannot be empty!");
        if (HasPattern(_id))
            throw std::runtime_error("Pattern with id '" + _id + "' already exists!");

        patterns.push_back(Pattern{ .id = _id, .regex = _regex, .action = _action });
        return *(patternsMap[patterns.back().id] = &patterns.back());
    }

    const Lexer::Pattern& Lexer::AddPattern(Regex _regex, Action _action) { return AddPattern("<Pattern: " + std::to_string(patterns.size()) + ">", _regex, _action); }

    Lexer::Token Lexer::Lex(StringStream& _stream) const
    {
        const Pattern* matchingPattern = nullptr;
        Position position = _stream.GetPosition();
        std::string matchValue;

        if (_stream.IsEOS()) { matchingPattern = &patternEOS; }
        else
        {
            size_t greatestPatternMatchLength = 0;

            for (auto& pattern : patterns)
            {
                const char* current = &*_stream.CCurrent();
                const char* end = &*_stream.CEnd();
                std::cmatch regexMatch;
                std::regex_search(current, end, regexMatch, pattern.regex, std::regex_constants::match_continuous);

                if (regexMatch.size() == 0)
                    continue;

                //If this is the first match or this match has a strictly greater length than
                //any previous match, set it as the match 
                if (!matchingPattern || regexMatch.length() > greatestPatternMatchLength)
                {
                    matchingPattern = &pattern;
                    matchValue = regexMatch.str();
                    greatestPatternMatchLength = regexMatch.length();
                }
            }

            if (!matchingPattern)
            {
                matchingPattern = &patternUnknown;
                matchValue = _stream.PeekChar();
            }
        }

        _stream.IgnoreChars(matchValue.size());

        Token token = {
            .patternID = matchingPattern->id,
            .position = position,
            .value = matchValue,
        };

        if (auto action = std::get_if<Function>(&matchingPattern->action)) { token.value = (*action)(_stream, matchValue); }
        else if (auto action = std::get_if<Procedure>(&matchingPattern->action)) { (*action)(_stream, matchValue); }

        return token;
    }

    Parser<std::string> Lexer::Pattern::AsTerminal(std::optional<std::string> _value) const { return Terminal(id, id, _value); }

    const Lexer::Pattern& Lexer::GetPattern(const PatternID& _id) const
    {
        if (!HasPattern(_id)) { throw std::runtime_error("Pattern with id '" + _id + "' does not exist!"); }
        else { return *(patternsMap.at(_id)); }
    }

    bool Lexer::HasPattern(const PatternID& _id) const { return patternsMap.contains(_id); }

    Parser<std::monostate> Error(const std::string& _name, const std::string& _message)
    {
        return Parser<std::monostate>(_name, [=](const Position& _pos, StringStream& _stream)
            {
                throw ParseError(_stream.GetPosition(), _message);
                return ParseResult<std::monostate>(_pos, std::monostate());
            });
    }
}

