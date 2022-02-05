#pragma once
#include "rxd.h"
#include "graphics.h"

namespace rxd::renderer
{
    struct Renderable : public raytracing::Intersectable
    {
        virtual ~Renderable() { }

        virtual Color GetColor(const Vec3F64& _point) = 0;
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