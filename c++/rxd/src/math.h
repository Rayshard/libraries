#pragma once
#include <array>
#include <cmath>
#include <string>
#include <cassert> // TODO: remove
#include <iostream> // TODO: remove

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
    bool operator==(const Vector<D, T>& _left, const Vector<D, T>& _right)
    {
        for (size_t i = 0; i < D; ++i)
            if (_left[i] != _right[i])
                return false;

        return true;
    }

    template<size_t D, typename T>
    bool operator!=(const Vector<D, T>& _left, const Vector<D, T>& _right)
    {
        for (size_t i = 0; i < D; ++i)
            if (_left[i] != _right[i])
                return true;

        return false;
    }

    template<size_t D, typename T>
    Vector<D, T> operator+(const Vector<D, T>& _left, const Vector<D, T>& _right)
    {
        Vector<D, T> result;

        for (size_t i = 0; i < D; ++i)
            result[i] = _left[i] + _right[i];

        return result;
    }

    template<size_t D, typename T>
    Vector<D, T>& operator+=(Vector<D, T>& _left, const Vector<D, T>& _right)
    {
        for (size_t i = 0; i < D; ++i)
            _left[i] += _right[i];

        return _left;
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
    Vector<D, T>& operator-=(Vector<D, T>& _left, const Vector<D, T>& _right)
    {
        for (size_t i = 0; i < D; ++i)
            _left[i] -= _right[i];

        return _left;
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

    typedef Vector<2, int32_t> Vec2I32;
    typedef Vector<2, uint32_t> Vec2UI32;
    typedef Vector<2, double> Vec2F64;
    typedef Vector<3, double> Vec3F64;
    typedef Vector<4, double> Vec4F64;

    struct Quaternion
    {
        double x, y, z, w;

        Quaternion(double _x, double _y, double _z, double _w) : x(_x), y(_y), z(_z), w(_w) { }
        Quaternion() : Quaternion(0.0, 0.0, 0.0, 0.0) { }

        static Quaternion FromAxisAngle(const Vec3F64& _axis, double _rad)
        {
            double sinHalfAngle = std::sin(_rad / 2), cosHalfAngle = std::cos(_rad / 2);

            return Quaternion(
                _axis[0] * sinHalfAngle,
                _axis[1] * sinHalfAngle,
                _axis[2] * sinHalfAngle,
                cosHalfAngle
            );
        }

        static Quaternion FromEulerAngles(double _yaw, double _pitch, double _roll)
        {
            double cy = std::cos(_yaw / 2), cp = std::cos(_pitch / 2), cr = std::cos(_roll / 2);
            double sy = std::sin(_yaw / 2), sp = std::sin(_pitch / 2), sr = std::sin(_roll / 2);

            return Quaternion(
                sr * cp * cy - cr * sp * sy,
                cr * sp * cy + sr * cp * sy,
                cr * cp * sy - sr * sp * cy,
                cr * cp * cy + sr * sp * sy
            );
        }

        static Vec3F64 Forward(const Quaternion& _q)
        {
            return Vec3F64({
                    2.0 * (_q.x * _q.z - _q.w * _q.y),
                    2.0 * (_q.y * _q.z + _q.w * _q.x),
                    1.0 - 2.0 * (_q.x * _q.x - _q.y * _q.y),
                });
        }

        static Vec3F64 Back(const Quaternion& _q)
        {
            return Vec3F64({
                    -2.0 * (_q.x * _q.z - _q.w * _q.y),
                    -2.0 * (_q.y * _q.z + _q.w * _q.x),
                    -1.0 + 2.0 * (_q.x * _q.x - _q.y * _q.y),
                });
        }

        static Vec3F64 Up(const Quaternion& _q)
        {
            return Vec3F64({
                    2.0 * (_q.x * _q.y + _q.w * _q.z),
                    1.0 - 2.0 * (_q.x * _q.x + _q.z * _q.z),
                    2.0 * (_q.y * _q.z - _q.w * _q.x),
                });
        }

        static Vec3F64 Down(const Quaternion& _q)
        {
            return Vec3F64({
                    -2.0 * (_q.x * _q.y + _q.w * _q.z),
                    -1.0 + 2.0 * (_q.x * _q.x + _q.z * _q.z),
                    -2.0 * (_q.y * _q.z - _q.w * _q.x),
                });
        }

        static Vec3F64 Right(const Quaternion& _q)
        {
            return Vec3F64({
                    1.0 - 2.0 * (_q.y * _q.y + _q.z * _q.z),
                    2.0 * (_q.x * _q.y - _q.w * _q.z),
                    2.0 * (_q.x * _q.z + _q.w * _q.y),
                });
        }

        static Vec3F64 Left(const Quaternion& _q)
        {
            return Vec3F64({
                    -1.0 + 2.0 * (_q.y * _q.y + _q.z * _q.z),
                    -2.0 * (_q.x * _q.y - _q.w * _q.z),
                    -2.0 * (_q.x * _q.z + _q.w * _q.y),
                });
        }
    };

    inline double LengthSquared(const Quaternion& _q) { return _q.x * _q.x + _q.y * _q.y + _q.z * _q.z + _q.w * _q.w; }
    inline double Length(const Quaternion& _q) { return std::sqrt(LengthSquared(_q)); }
    inline Quaternion Conjugate(const Quaternion& _q) { return Quaternion(-_q.x, -_q.y, -_q.z, _q.w); }

    inline Quaternion Normalize(const Quaternion& _q)
    {
        double length = Length(_q);
        return Quaternion(_q.x / length, _q.y / length, _q.z / length, _q.w / length);
    }

    inline Quaternion operator*(const Quaternion& _left, const Quaternion& _right)
    {
        return Quaternion(
            _left.x * _right.w + _left.w * _right.x + _left.y * _right.z - _left.z * _right.y,
            _left.y * _right.w + _left.w * _right.y + _left.z * _right.x - _left.x * _right.z,
            _left.z * _right.w + _left.w * _right.z + _left.x * _right.y - _left.y * _right.x,
            _left.w * _right.w - _left.x * _right.x - _left.y * _right.y - _left.z * _right.z
        );
    }

    inline Vec3F64 Rotate(const Vec3F64& _v, const Quaternion& _q)
    {
        Quaternion r = _q * Quaternion(_v[0], _v[1], _v[2], 0.0) * Conjugate(_q);
        return Vec3F64({ r.x, r.y, r.z });
    }
}