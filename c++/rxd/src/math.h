#pragma once
#include <array>
#include <cmath>
#include <string>

namespace rxd::math
{
    template<typename T>
    T Lerp(const T& _a, const T& _b, double _amt) { return _a + (_b - _a) * _amt; }

    template<size_t D, typename T>
        requires (D > 0)
    struct Vector
    {
        std::array<T, D> components;

        Vector() : components() { }
        Vector(T _value) { components.fill(_value); }
        Vector(const std::array<T, D>& _components) : components(_components) { }

        const T& operator[](size_t _i) const { return components[_i]; }
        T& operator[](size_t _i) { return components[_i]; }

        static Vector Zero() { return Vector(); }
        static Vector One() { return Vector(1); }
    };

    template<size_t D, typename T>
    Vector<D, T> operator+(const Vector<D, T>& _left, const Vector<D, T>& _right)
    {
        Vector<D, T> result;

        for (size_t i = 0; i < D; ++i)
            result[i] = _left[i] + _right[i];

        return result;
    }

    template<size_t D, typename T>
    Vector<D, T> operator+(const Vector<D, T>& _left, const T& _right)
    {
        Vector<D, T> result;

        for (size_t i = 0; i < D; ++i)
            result[i] = _left[i] + _right;

        return result;
    }

    template<size_t D, typename T>
    Vector<D, T> operator+(const T& _left, const Vector<D, T>& _right)
    {
        Vector<D, T> result;

        for (size_t i = 0; i < D; ++i)
            result[i] = _left + _right[i];

        return result;
    }

    template<size_t D, typename T>
    Vector<D, T> operator-(const Vector<D, T>& _left, const Vector<D, T>& _right)
    {
        Vector<D, T> result;

        for (size_t i = 0; i < D; ++i)
            result[i] = _left[i] - _right[i];

        return result;
    }

    template<size_t D, typename T>
    Vector<D, T> operator-(const Vector<D, T>& _left, const T& _right)
    {
        Vector<D, T> result;

        for (size_t i = 0; i < D; ++i)
            result[i] = _left[i] - _right;

        return result;
    }

    template<size_t D, typename T>
    Vector<D, T> operator-(const T& _left, const Vector<D, T>& _right)
    {
        Vector<D, T> result;

        for (size_t i = 0; i < D; ++i)
            result[i] = _left - _right[i];

        return result;
    }

    template<size_t D, typename T>
    Vector<D, T> operator*(const Vector<D, T>& _left, const Vector<D, T>& _right)
    {
        Vector<D, T> result;

        for (size_t i = 0; i < D; ++i)
            result[i] = _left[i] * _right[i];

        return result;
    }

    template<size_t D, typename T>
    Vector<D, T> operator*(const Vector<D, T>& _left, const T& _right)
    {
        Vector<D, T> result;

        for (size_t i = 0; i < D; ++i)
            result[i] = _left[i] * _right;

        return result;
    }

    template<size_t D, typename T>
    Vector<D, T> operator*(const T& _left, const Vector<D, T>& _right)
    {
        Vector<D, T> result;

        for (size_t i = 0; i < D; ++i)
            result[i] = _left * _right[i];

        return result;
    }

    template<size_t D, typename T>
    Vector<D, T> operator/(const Vector<D, T>& _left, const Vector<D, T>& _right)
    {
        Vector<D, T> result;

        for (size_t i = 0; i < D; ++i)
            result[i] = _left[i] / _right[i];

        return result;
    }

    template<size_t D, typename T>
    Vector<D, T> operator/(const Vector<D, T>& _left, const T& _right)
    {
        Vector<D, T> result;

        for (size_t i = 0; i < D; ++i)
            result[i] = _left[i] / _right;

        return result;
    }

    template<size_t D, typename T>
    Vector<D, T> operator/(const T& _left, const Vector<D, T>& _right)
    {
        Vector<D, T> result;

        for (size_t i = 0; i < D; ++i)
            result[i] = _left / _right[i];

        return result;
    }

    template<size_t D, typename T>
    double LengthSquared(const Vector<D, T>& _v)
    {
        double sum = 0;

        for (size_t i = 0; i < D; ++i)
        {
            const T& component = _v[i];
            sum += component * component;
        }

        return sum;
    }

    template<size_t D, typename T>
    double Length(const Vector<D, T>& _v) { return std::sqrt(LengthSquared(_v)); }

    template<size_t D, typename T>
    double Dot(const Vector<D, T>& _a, const Vector<D, T>& _b)
    {
        double result = 0;

        for (size_t i = 0; i < D; ++i)
            result += _a[i] * _b[i];

        return result;
    }

    template<size_t D, typename T>
    Vector<D, T> Normalize(const Vector<D, T>& _v) { return _v / Length(_v); }

    template<size_t D, typename T>
    Vector<D, T> Lerp(const Vector<D, T>& _a, const Vector<D, T>& _b, double _amt)
    {
        Vector<D, T> result;

        for (size_t i = 0; i < D; ++i)
            result[i] = Lerp(_a[i], _b[i], _amt);

        return result;
    }

    template<size_t D, typename T>
    std::string ToString(Vector<D, T> _v)
    {
        std::string result;
        result.reserve(3 + (D - 1) * 2); // Reserving the minimum amount of characters needed

        result += "(" + std::to_string(_v[0]);

        for (size_t i = 1; i < D; ++i)
            result += ", " + std::to_string(_v[i]);

        result += ")";
        return result;
    }

    template<size_t D, typename T>
    std::ostream& operator<<(std::ostream& _lhs, const Vector<D, T>& _rhs) { return _lhs << ToString(_rhs); }

    typedef Vector<2, double> Vec2F64;
    typedef Vector<3, double> Vec3F64;
    typedef Vector<4, double> Vec4F64;
}