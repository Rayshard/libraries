#include "lpc.h"

namespace lpc
{
    std::string Position::ToString() const { return "(" + std::to_string(line) + ", " + std::to_string(column) + ")"; }

    std::ostream& operator<<(std::ostream& _lhs, const Position& _rhs) { return _lhs << _rhs.ToString(); }

    std::string IStreamToString(std::istream& _stream) { return std::string((std::istreambuf_iterator<char>(_stream)), std::istreambuf_iterator<char>()); }

    Regex::Regex() : std::regex(), string() { }
    Regex::Regex(const std::string& _string) : std::regex(_string), string(_string) { }

    const std::string& Regex::GetString() const { return string; }

    template<typename T>
    Stream<T>::Stream(std::vector<T>&& _data) : data(std::move(_data)), offset(0) { }

    template<typename T>
    T Stream<T>::Get() { return IsEOS() ? T() : data[offset++]; }

    template<typename T>
    T Stream<T>::Peek()
    {
        auto initOffset = offset;
        auto peek = Get();
        offset = initOffset;
        return peek;
    }

    template<typename T>
    size_t Stream<T>::GetOffset() const { return offset; }

    template<typename T>
    void Stream<T>::SetOffset(size_t _offset) { offset = std::max<size_t>(0, std::min<size_t>(_offset, data.size())); }

    template<typename T>
    typename std::vector<T>::iterator Stream<T>::Begin() { return data.begin(); }

    template<typename T>
    typename std::vector<T>::iterator Stream<T>::End() { return data.end(); }

    template<typename T>
    typename std::vector<T>::iterator Stream<T>::Current() { return data.begin() + offset; }

    template<typename T>
    typename std::vector<T>::const_iterator Stream<T>::CBegin() const { return data.cbegin(); }

    template<typename T>
    typename std::vector<T>::const_iterator Stream<T>::CEnd() const { return data.cend(); }

    template<typename T>
    typename std::vector<T>::const_iterator Stream<T>::CCurrent() const { return data.cbegin() + offset; }

    StringStream::StringStream(const std::string& _data) : Stream(std::vector(_data.begin(), _data.end())), lineStarts()
    {
        //Get line starts
        lineStarts.push_back(0);

        for (size_t position = 0; position < _data.size(); position++)
        {
            if (_data[position] == '\n')
                lineStarts.push_back(position + 1);
        }
    }

    void StringStream::Ignore(size_t _amt)
    {
        for (size_t i = 0; i < _amt; i++)
            Get();
    }

    std::string StringStream::GetDataAsString(size_t _start, size_t _length) const
    {
        if (_start + _length > data.size())
            throw std::runtime_error("Parameters out of range of data!");

        auto start = data.begin() + _start;
        return std::string(start, start + _length);
    }

    std::string StringStream::GetDataAsString(size_t _start) const { return GetDataAsString(_start, data.size() - _start); }

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

    Position StringStream::GetPosition() const { return GetPosition(GetOffset()); }

    void StringStream::SetPosition(Position _pos)
    {
        if (_pos.line > lineStarts.size() || _pos.line == 0 || _pos.column == 0)
            throw std::runtime_error("Invaild position: " + _pos.ToString());

        size_t lineStart = lineStarts[_pos.line - 1];
        size_t lineWidth = (_pos.line == lineStarts.size() ? data.size() : lineStarts[_pos.line]) - lineStart;

        if (_pos.column - 1 > lineWidth)
            throw std::runtime_error("Invaild position: " + _pos.ToString());

        SetOffset(lineStart + _pos.column - 1);
    }

    bool StringStream::IsEOS() const { return GetOffset() >= data.size(); }

    bool Lexer::Token::IsEOS() const { return patternID == EOS_PATTERN_ID(); }
    bool Lexer::Token::IsUnknown() const { return patternID == UNKNOWN_PATTERN_ID(); }

    Lexer::Lexer(Action _onEOS, Action _onUnknown) : patterns()
    {
        patternEOS = { .id = EOS_PATTERN_ID(), .regex = Regex(), .action = _onEOS };
        patternUnknown = { .id = UNKNOWN_PATTERN_ID(), .regex = Regex(), .action = _onUnknown };
    }

    Lexer::PatternID Lexer::AddPattern(PatternID _id, Regex _regex, Action _action)
    {
        assert(!_id.empty() && "PatternID cannot be empty!");
        if (HasPatternID(_id))
            throw std::runtime_error("Pattern with id '" + _id + "' already exists!");

        patterns.push_back(Pattern{ .id = _id, .regex = _regex, .action = _action });
        return patterns.back().id;
    }

    Lexer::PatternID Lexer::AddPattern(Regex _regex, Action _action) { return AddPattern("<Pattern: " + std::to_string(patterns.size()) + ">", _regex, _action); }

    Lexer::Token Lexer::Lex(StringStream& _stream)
    {
        Pattern* matchingPattern = nullptr;
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
                matchValue = _stream.Peek();
            }
        }

        _stream.Ignore(matchValue.size());

        Token token = {
            .patternID = matchingPattern->id,
            .position = position,
            .value = matchValue,
        };

        if (auto action = std::get_if<Function>(&matchingPattern->action)) { token.value = (*action)(_stream, matchValue); }
        else if (auto action = std::get_if<Procedure>(&matchingPattern->action)) { (*action)(_stream, matchValue); }

        return token;
    }

    bool Lexer::HasPatternID(const PatternID& _id)
    {
        for (auto& pattern : patterns)
        {
            if (pattern.id == _id)
                return false;
        }

        return _id == EOS_PATTERN_ID() || _id == UNKNOWN_PATTERN_ID();
    }

    TokenStream::TokenStream(Lexer* _lexer, StringStream* _ss, const std::set<Lexer::PatternID>& _ignores)
        : Stream({}), lexer(_lexer), ss(_ss), ignores(_ignores)
    {
        if (ignores.contains(Lexer::EOS_PATTERN_ID()))
            throw std::runtime_error(Lexer::EOS_PATTERN_ID() + " cannot be ignored!");
    }

    Lexer::Token TokenStream::Get()
    {
        while (true)
        {
            while (GetOffset() >= data.size() && !IsEOS())
                data.push_back(lexer->Lex(*ss));

            if (IsEOS())
            {
                SetOffset(data.size());
                return data.back();
            }

            auto& token = data[GetOffset()];
            SetOffset(GetOffset() + 1);

            if (ignores.contains(token.patternID))
                continue;

            return token;
        }
    }

    Position TokenStream::GetPosition() { return Peek().position; }
    bool TokenStream::IsEOS() const { return !data.empty() && data[std::min(GetOffset(), data.size() - 1)].IsEOS(); }
}

