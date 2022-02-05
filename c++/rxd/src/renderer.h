#pragma once
#include "rxd.h"
#include "graphics.h"

using namespace rxd::math::raytracing;

namespace rxd::renderer
{
    struct Renderable : public raytracing::Intersectable
    {
        virtual ~Renderable() { }

        virtual Color GetColor(const Vec3F64& _point) = 0;
    };

    struct Sphere : public Renderable
    {
        Vec3F64 center;
        double radius;
        Color color;

        Sphere(const Vec3F64& _center, double _radius, const Color& _color) : center(_center), radius(_radius), color(_color) { }
        Sphere() : center(), radius(1), color(Color::White()) {}

        std::optional<double> GetIntersection(const Ray& _ray) override
        {
            Vec3F64 centerToRay = _ray.a - center, rayBMinusA = _ray.b - _ray.a;
            double a = Dot(rayBMinusA, rayBMinusA);
            double b = 2.0 * Dot(centerToRay, rayBMinusA);
            double c = Dot(centerToRay, centerToRay) - radius * radius;
            double discriminant = b * b - 4 * a * c;

            if (discriminant < 0.0)
                return std::nullopt;

            double sqrtDiscriminant = std::sqrt(discriminant), aTimes2 = a * 2;
            return std::min((-b + sqrtDiscriminant) / aTimes2, (-b - sqrtDiscriminant) / aTimes2);
        }

        Color GetColor(const Vec3F64& _point) override { return color; }
        Vec3F64 GetNormal(const Vec3F64& _point) override { return Normalize(_point - center); }
    };

    struct Plane : public Renderable
    {
        Vec3F64 normal;
        double distance;
        Color color;

        Plane(const Vec3F64& _normal, double _distance, const Color& _color) : normal(_normal), distance(_distance), color(_color) { }
        Plane() : normal(), distance(), color(Color::White()) {}

        std::optional<double> GetIntersection(const Ray& _ray) override
        {
            double normalDotRay = Dot(normal, _ray.b - _ray.a);

            if (normalDotRay >= 0) { return std::nullopt; } //Plane and ray are parallel or facing the same way
            else { return (distance - Dot(normal, _ray.a)) / normalDotRay; }
        }

        Color GetColor(const Vec3F64& _point) override { return color; }
        Vec3F64 GetNormal(const Vec3F64& _point) override { return normal; }

        Vec3F64 GetOrigin() { return normal * distance; }
    };

    struct IVertex
    {
        virtual ~IVertex() { }

        virtual const double& operator[](size_t _i) const = 0;
        virtual double& operator[](size_t _i) = 0;

        virtual Vec2F64& GetPosition() = 0;
        virtual const Vec2F64& GetPosition() const = 0;
        virtual void SetPosition(const Vec2F64& _value) = 0;

        virtual size_t GetSize() const = 0;

        const double& GetX() const { return GetPosition()[0]; };
        double& GetX() { return GetPosition()[0]; };
        void SetX(double _value) { GetPosition()[0] = _value; };

        const double& GetY() const { return GetPosition()[1]; };
        double& GetY() { return GetPosition()[1]; };
        void SetY(double _value) { GetPosition()[1] = _value; };
    };

    template<typename T>
    concept VertexType = std::derived_from<T, IVertex>;

    template<typename Input, VertexType Output>
    using VertexShader = std::function<Output(const Input&)>;

    template<VertexType Input>
    using PixelShader = std::function<Color(const Input&)>;

    template<typename T, size_t Offset, size_t Max>
    concept InRange = Offset * 8 <= Max * 8 - sizeof(T);

    template<size_t N>
        requires (N >= 2)
    struct Vertex : public IVertex
    {
        Vertex(Vec2F64 _pos) { SetPosition(_pos); }

        const double& operator[](size_t _i) const override
        {
            if (_i >= N)
                throw std::out_of_range("Invalid component index: " + std::to_string(_i) + " > " + std::to_string(N - 1));

            return components[_i];
        }

        double& operator[](size_t _i) override
        {
            if (_i >= N)
                throw std::out_of_range("Invalid component index: " + std::to_string(_i) + " > " + std::to_string(N - 1));

            return components[_i];
        }

        template<typename T, size_t Offset>
            requires InRange<T, Offset, N>
        T& GetComponent() { return *(T*)&components[Offset]; }

        template<typename T, size_t Offset>
            requires InRange<T, Offset, N>
        const T& GetComponent() const { return *(T*)&components[Offset]; }

        template<typename T, size_t Offset>
            requires InRange<T, Offset, N>
        void SetComponent(const T& _value) { *(T*)&components[Offset] = _value; }

        Vec2F64& GetPosition() override { return GetComponent<Vec2F64, 0>(); }
        const Vec2F64& GetPosition() const override { return GetComponent<Vec2F64, 0>(); }
        void SetPosition(const Vec2F64& _value) override { return SetComponent<Vec2F64, 0>(_value); }

        size_t GetSize() const override { return N; };

    private:
        double components[N];
    };

    struct Triangle : public Renderable
    {
        Vec3F64 p1, p2, p3;

        Triangle(const Vec3F64& _p1, const Vec3F64& _p2, const Vec3F64& _p3) : p1(_p1), p2(_p2), p3(_p3) { }
        Triangle() : p1(), p2(), p3() {}

        std::optional<double> GetIntersection(const Ray& _ray) override
        {
            // Check intersect supporting plane
            Vec3F64 normal = Normalize(Cross(p2 - p1, p3 - p1));
            Plane plane = Plane(normal, Dot(normal, p1), Color::Red());

            auto planeInteresctionT = plane.GetIntersection(_ray);
            if (!planeInteresctionT.has_value())
                return std::nullopt;

            // Check intersection is in triangle bounds
            Vec3F64 point = _ray.GetPoint(planeInteresctionT.value());
            double alpha = Dot(Cross(p2 - p1, point - p1), normal);
            double beta = Dot(Cross(p3 - p2, point - p2), normal);
            double gamma = Dot(Cross(p1 - p3, point - p3), normal);

            if (alpha < 0.0 || beta < 0.0 || gamma < 0.0) { return std::nullopt; }
            else { return planeInteresctionT.value(); }
        }

        Color GetColor(const Vec3F64& _point) override { return GetInterpolation(_point, Color::Red().ToVec4F64(), Color::Green().ToVec4F64(), Color::Blue().ToVec4F64()); }
        Vec3F64 GetNormal(const Vec3F64& _point) override { return Normalize(Cross(p2 - p1, p3 - p1)); }

        std::tuple<double, double, double> GetBarycentricCoords(const Vec3F64& _point)
        {
            Vec3F64 normal = Normalize(Cross(p2 - p1, p3 - p1));
            double denominator = Dot(Cross(p2 - p1, p3 - p1), normal);

            return {
                Dot(Cross(p3 - p2, _point - p2), normal) / denominator,
                Dot(Cross(p1 - p3, _point - p3), normal) / denominator,
                Dot(Cross(p2 - p1, _point - p1), normal) / denominator
            };
        }

        template<typename T>
        T GetInterpolation(const Vec3F64& _point, const T& _p1V, const T& _p2V, const T& _p3V)
        {
            auto [a, b, c] = GetBarycentricCoords(_point);
            return a * _p1V + b * _p2V + c * _p3V;
        }
    };

    template<VertexType V>
    struct Mesh
    {
        Mesh(std::vector<V>&& _vertices, std::vector<std::array<size_t, 3>>&& _triangleIndices) : vertices(std::move(_vertices)), triangleIndices(std::move(_triangleIndices))
        {
            size_t maxIndex = _vertices.size() - 1;

            for (auto& [i1, i2, i3] : _triangleIndices)
            {
                if (i1 > maxIndex || i2 > maxIndex || i3 > maxIndex)
                    throw std::runtime_error("Invalid triangle: (" + std::to_string(i1) + ", " + std::to_string(i2) + ", " + std::to_string(i3) + ")");
            }
        }

        const V& operator[](size_t _i) const { vertices.at(_i); }
        V& operator[](size_t _i) { vertices.at(_i); }

        const std::vector<std::array<size_t, 3>> GetTriangleIndices() const { return triangleIndices; }

        template<VertexType T>
        static Mesh<T> CreateFrom(const Mesh& _input, VertexShader<V, T> _vs)
        {
            std::vector<T> vertices(_input.vertices.size());
            auto triangleIndices = _input.triangleIndices;

            for (size_t i = 0; i < vertices.size(); ++i)
                vertices[i] = _vs(_input.vertices[i]);

            return Mesh<T>(std::move(vertices), std::move(triangleIndices));
        }

        std::vector<std::array<V*, 3>> GenerateTriangles()
        {
            std::vector<std::array<V*, 3>> triangles(triangleIndices.size());

            for (size_t i = 0; i < triangleIndices.size(); ++i)
            {
                auto& [i1, i2, i3] = triangleIndices[i];
                triangles[i] = { &vertices[i1], &vertices[i2], &vertices[i3] };
            }

            return triangles;
        }
    private:
        std::vector<V> vertices;
        std::vector<std::array<size_t, 3>> triangleIndices;
    };
}