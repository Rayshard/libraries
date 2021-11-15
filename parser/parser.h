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

    public:
        size_t offset;

        IStream(std::vector<T>&& _data) : data(std::move(_data)), offset(0)
        {
            assert(!data.empty() && "Data must be non-empty!");
        }

        IStream(std::vector<T>&& _data, T _eos) : data(std::move(_data)), offset(0)
        {
            data.push_back(_eos);
        }

        virtual T Peek() { return IsEOS() ? data.back() : data[offset]; }
        
        void Ignore(size_t _amt)
        {
            for(size_t i = 0; i < _amt; i++)
                Get();
        }

        T Get()
        {
            auto peek = Peek();
            offset++;
            return peek;
        }

        typename std::vector<T>::iterator Begin() { return data.begin(); }
        typename std::vector<T>::iterator End() { return data.end(); }
        typename std::vector<T>::iterator Current() { return data.begin() + offset; }

        typename std::vector<T>::const_iterator CBegin() const { return data.cbegin(); }
        typename std::vector<T>::const_iterator CEnd() const { return data.cend(); }
        typename std::vector<T>::const_iterator CCurrent() const { return data.cbegin() + offset; }

        bool IsEOS() { return offset >= data.size(); }
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

        std::string GetDataAsString(size_t _start, size_t _length) const
        {
            if (_start + _length >= data.size())
                throw std::runtime_error("Parameters out of range of data!");

            auto start = data.begin() + _start;
            return std::string(start, start + _length);
        }

        Position GetPosition(size_t _offset) const
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

        Position GetPosition() const { return GetPosition(offset); }

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
}