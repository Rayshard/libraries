#pragma once
#include "rxd.h"

namespace rxd::renderer
{
    class Target : public Image
    {
        Image* image;
        std::vector<double> depthBuffer;

    public:
        Target(Image* _image, bool _hasDepthBuffer)
            : image(_image), depthBuffer(_hasDepthBuffer ? _image->GetWidth() * _image->GetHeight() : 0) { }

        Target(const Target&) = delete;
        Target& operator= (const Target&) = delete;

        virtual ~Target() { delete image; }

        Utilities::Color GetPixel(int64_t _x, int64_t _y) { return image->GetPixel(_x, _y); }
        void SetPixel(int64_t _x, int64_t _y, Utilities::Color _col) { image->SetPixel(_x, _y, _col); }

        Utilities::Color GetDepth(int64_t _x, int64_t _y) { return depthBuffer[_x + _y * GetWidth()]; }
        void SetDepth(int64_t _x, int64_t _y, double _value) { depthBuffer[_x + _y * GetWidth()] = _value; }
        void ClearDepthBuffer() { std::fill(depthBuffer.begin(), depthBuffer.end(), 0.0); }

        uint64_t GetWidth() { return image->GetWidth(); }
        uint64_t GetHeight() { return image->GetHeight(); };

        template<typename T>
            requires std::derived_from<T, Image>
        T& As() { return *(T*)image; }

        template<typename T>
            requires std::derived_from<T, Image>
        const T& As() const { return *(T*)image; }
    };

    struct IVertex
    {
        virtual ~IVertex() { };

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
    using PixelShader = std::function<Utilities::Color(const Input&)>;

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

    template<VertexType V>
    class RasterizedEdge
    {
        V value;
        std::vector<double> step;

    public:
        RasterizedEdge(const V& _start, const V& _end, double _stepSize) : value(_start), step(_start.GetSize())
        {
            for (size_t i = 0; i < step.size(); ++i)
                step[i] = (_end[i] - _start[i]) * _stepSize;
        }

        void Step()
        {
            for (size_t i = 0; i < step.size(); ++i)
                value[i] += step[i];
        }

        const V& GetValue() const { return value; }
    };

    class Renderer
    {
        
    public:
        template<VertexType V>
        void Rasterize(Target* _target, const V& _v1, const V& _v2, PixelShader<V> _ps)
        {
            // uint64_t targetWidth = _target->GetWidth(), targetHeight = _target->GetHeight();

            // Vertex<N>* left = _v1, * right = _v2;
            // if (right->GetX() < left->GetX())
            //     std::swap(left, right);

            // int64_t yStart = std::ceil(left->GetY() * targetHeight), yEnd = std::ceil(right->GetY() * targetHeight);

            // if (yStart == yEnd) // Horizontal line
            // {
            //     int64_t y = std::ceil(left->GetY() * targetHeight);
            //     int64_t xStart = std::ceil(left->GetX() * targetWidth), xEnd = std::ceil(right->GetX() * targetWidth);
            //     auto scanLine = RasterizedEdge<N>(*left, *right, 1.0 / (xEnd - xStart));

            //     for (int64_t x = xStart; x < xEnd; ++x)
            //     {
            //         _target->SetPixel(x, y, _ps(scanLine.GetValue()));
            //         scanLine.Step();
            //     }
            // }
            // else
            // {
            //     auto edge = RasterizedEdge<N>(left, right, 1.0 / (yStart - yEnd));
            //     double lastX = edge.GetValue().GetX(), distSquared = LengthSquared(right->GetPosition(), left->GetPosition());

            //     if (yStart < yEnd)
            //     {
            //         for (int64_t y = yStart; y < yEnd; ++y)
            //         {
            //             int64_t xStart = std::ceil(lastX * targetWidth);
            //             int64_t xEnd = std::max(xStart + 1, (int64_t)std::ceil(edge.GetValue().GetX() * targetWidth));

            //             for (int64_t x = xStart; x < xEnd; ++x)
            //             {
            //                 double t = LengthSquared(Vec2F64({ x / double(targetWidth), y / double(targetHeight) }), left->GetPosition()) / distSquared;
            //                 _target->SetPixel(x, y, _ps(PSIn::Lerp(left, right, t)));
            //             }

            //             lastX = edge.GetValue().GetX();
            //             edge.Step();
            //         }
            //     }
            //     else
            //     {
            //         for (int64_t y = yStart; y > yEnd; --y)
            //         {
            //             int64_t xStart = std::ceil(lastX * targetWidth);
            //             int64_t xEnd = std::max(xStart + 1, (int64_t)std::ceil(edge.GetValue().GetX() * targetWidth));

            //             for (int64_t x = xStart; x < xEnd; ++x)
            //             {
            //                 double t = Vec2F64::LengthSquared({ x / double(targetWidth), y / double(targetHeight) }, left->GetPosition()) / distSquared;
            //                 _target->SetPixel(x, y, _ps(PSIn::Lerp(left, right, t)));
            //             }

            //             lastX = edge.GetValue().GetX();
            //             edge.Step();
            //         }
            //     }
            // }
        }

        template<VertexType V>
        void Rasterize(Target* _target, int64_t _yStart, int64_t _yEnd, RasterizedEdge<V>& _lEdge, RasterizedEdge<V>& _rEdge, PixelShader<V> _ps)
        {
            uint64_t targetWidth = _target->GetWidth();

            for (int64_t y = _yStart; y < _yEnd; ++y)
            {
                const V* vLeft = &_lEdge.GetValue(), * vRight = &_rEdge.GetValue();

                if (vRight->GetX() < vLeft->GetX())
                    std::swap(vLeft, vRight);

                int64_t xStart = std::ceil((vLeft->GetX() + 0.5) * targetWidth), xEnd = std::ceil((vRight->GetX() + 0.5) * targetWidth);
                auto scanLine = RasterizedEdge<V>(*vLeft, *vRight, 1.0 / (xEnd - xStart));

                for (int64_t x = xStart; x < xEnd; x++)
                {
                    _target->SetPixel(x, y, _ps(scanLine.GetValue()));
                    scanLine.Step();
                }

                _lEdge.Step();
                _rEdge.Step();
            }
        }

        template<VertexType V>
        void Rasterize(Target* _target, const V& _v1, const V& _v2, const V& _v3, PixelShader<V> _ps, bool _wireframe = false)
        {
            //Sort vertices
            const V* top = &_v1, * middle = &_v2, * bottom = &_v3;

            if (bottom->GetY() < middle->GetY()) { std::swap(bottom, middle); }
            if (middle->GetY() < top->GetY()) { std::swap(middle, top); }
            if (bottom->GetY() < middle->GetY()) { std::swap(bottom, middle); }

            //Rasterize
            if (_wireframe)
            {
                // Rasterize(_target, top, bottom, _vs, _ps);
                // Rasterize(_target, top, middle, _vs, _ps);
                // Rasterize(_target, middle, bottom, _vs, _ps);
            }
            else
            {
                auto topToBottom = bottom->GetPosition() - top->GetPosition(), topToMiddle = middle->GetPosition() - top->GetPosition();
                bool rightHanded = topToMiddle[0] * topToBottom[1] - topToBottom[0] * topToMiddle[1] >= 0;

                uint64_t targetHeight = _target->GetHeight();
                int64_t y = std::ceil((top->GetY() + 0.5) * targetHeight), yEnd1 = std::ceil((middle->GetY() + 0.5) * targetHeight), yEnd2 = std::ceil((bottom->GetY() + 0.5) * targetHeight);
                auto xTB = RasterizedEdge<V>(*top, *bottom, 1.0 / (yEnd2 - y)), xTM = RasterizedEdge<V>(*top, *middle, 1.0 / (yEnd1 - y)), xMB = RasterizedEdge<V>(*middle, *bottom, 1.0 / (yEnd2 - yEnd1));

                if (rightHanded)
                {
                    Rasterize(_target, y, yEnd1, xTB, xTM, _ps);
                    Rasterize(_target, yEnd1, yEnd2, xTB, xMB, _ps);
                }
                else
                {
                    Rasterize(_target, y, yEnd1, xTM, xTB, _ps);
                    Rasterize(_target, yEnd1, yEnd2, xMB, xTB, _ps);
                }
            }
        }

        template<VertexType V>
        void Rasterize(Target* _target, const std::vector<std::array<V*, 3>>& _triangles, PixelShader<V> _ps, bool _wireframe = false)
        {
            for (auto& triangle : _triangles)
                Rasterize(_target, *triangle[0], *triangle[1], *triangle[2], _ps, _wireframe);
        }
    };
}