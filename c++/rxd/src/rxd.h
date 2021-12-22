#pragma once

#include "SDL2/SDL.h"
#include <string>
#include <functional>
#include <thread>
#include <vector>
#include <concepts>
#include "math.h"
#include <iostream> //TODO: Remove

//TODO: Move to cpp file
#define CHECK_SDL(value, message) do { if (!(value)) throw std::runtime_error(std::string(message) + " Error:\n" + SDL_GetError()); } while(false)

namespace rxd
{
    using namespace math;

    class Runnable
    {
        bool running;

    public:
        Runnable();

        std::thread Start();
        void Quit();

        bool IsRunning() const;

        virtual void OnEvent(const SDL_Event& _event) = 0;

    protected:
        virtual void OnStart() = 0;
        virtual void OnRun() = 0;
        virtual void OnQuit() = 0;
    };

    void Init();
    void Run(Runnable& _runnable);
    void Quit();
    void CleanUp();

    namespace Utilities
    {
        struct Color
        {
            uint8_t a, r, g, b;

            Color(uint8_t _a, uint8_t _r, uint8_t _g, uint8_t _b) : a(_a), r(_r), g(_g), b(_b) { }
            Color(Vec4F64 _v) : Color(_v[0] * 255, _v[1] * 255, _v[2] * 255, _v[3] * 255) { }
            Color(uint32_t _value) : Color((_value >> 24) & 255, (_value >> 16) & 255, (_value >> 8) & 255, _value & 255) { }
            Color() : Color(255, 0, 0, 0) { }

            Vec4F64 ToVec4F64() { return Vec4F64({ a / 255.0, r / 255.0, g / 255.0, b / 255.0 }); }

            inline static Color Red() { return { 255, 255, 0, 0 }; }
            inline static Color Green() { return { 255, 0, 255, 0 }; }
            inline static Color Blue() { return { 255, 0, 0, 255 }; }
            inline static Color White() { return { 255, 255, 255, 255 }; }
            inline static Color Black() { return { 255, 0, 0, 0 }; }
            inline static Color Clear() { return { 0, 0, 0, 0 }; }
        };
    }

    struct Image
    {
        virtual ~Image() { }

        virtual Utilities::Color GetPixel(int64_t _x, int64_t _y) = 0;
        virtual void SetPixel(int64_t _x, int64_t _y, Utilities::Color _col) = 0;

        virtual uint64_t GetWidth() = 0;
        virtual uint64_t GetHeight() = 0;
    };

    class Bitmap : public Image
    {
    protected:
        SDL_Surface* surface;

        Bitmap(SDL_Surface* _surface);

    public:
        Bitmap();
        Bitmap(uint64_t _width, uint64_t _height);
        Bitmap(const std::string& _path);

        Bitmap(const Bitmap& _b);
        Bitmap(Bitmap&& _b) noexcept;

        virtual ~Bitmap();

        void Fill(Utilities::Color _color);
        void Blit(const Bitmap& _bitmap);

        Utilities::Color GetPixel(int64_t _x, int64_t _y) override;
        void SetPixel(int64_t _x, int64_t _y, Utilities::Color _col) override;

        Bitmap& operator=(const Bitmap& _b);
        Bitmap& operator=(Bitmap&& _b) noexcept;

        uint64_t GetWidth() override;
        uint64_t GetHeight() override;

        class Internal
        {
            friend class Window;
            friend class Screen;

            static SDL_Surface* GetSurface(const Bitmap& _bitmap);
        };
    };

    class Screen;

    class Window
    {
        SDL_Window* instance;
        SDL_Renderer* renderer;

    public:
        Window(std::string _title, uint64_t _x, uint64_t _y, uint64_t _width, uint64_t _height);
        virtual ~Window();

        void Show();
        void FlipScreen(const Screen& _screen);

        uint64_t GetWidth() const;
        uint64_t GetHeight() const;

        class Internal
        {
            friend class Screen;

            static SDL_Renderer* GetRenderer(const Window& _window) { return _window.renderer; }
        };
    };

    class Screen : public Bitmap
    {
        SDL_Texture* texture;

    public:
        Screen(Window& _window, uint64_t _width, uint64_t _height) : Bitmap(_width, _height)
        {
            texture = SDL_CreateTexture(Window::Internal::GetRenderer(_window), surface->format->format, SDL_TEXTUREACCESS_STREAMING, GetWidth(), GetHeight());
            CHECK_SDL(texture, "Could not create texture!");
        }

        Screen(const Screen&) = delete;
        Screen& operator= (const Screen&) = delete;

        virtual ~Screen() { SDL_DestroyTexture(texture); }

        void Update() { SDL_UpdateTexture(texture, NULL, surface->pixels, surface->pitch); }
        Bitmap Capture() { return Bitmap(*this); }

        class Internal
        {
            friend class Window;

            static SDL_Texture* GetTexture(const Screen& _screen) { return _screen.texture; }
        };
    };

    namespace Renderer
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

        template<typename T, size_t Offset, size_t Max>
        concept InRange = Offset * 8 <= Max * 8 - sizeof(T);

        template<size_t N>
            requires (N >= 2)
        struct Vertex
        {
            static constexpr size_t Size = N;

            using PixelShader = std::function<Utilities::Color(const Vertex&)>;

            template<typename Input>
            using VertexShader = std::function<Vertex(const Input&)>;

            Vertex(Vec2F64 _pos) { SetPosition(_pos); }

            const double& operator[](size_t _i) const
            {
                if (_i >= N)
                    throw std::out_of_range("Invalid component index: " + std::to_string(_i) + " > " + std::to_string(N - 1));

                return components[_i];
            }

            double& operator[](size_t _i)
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

            Vec2F64& GetPosition() { return GetComponent<Vec2F64, 0>(); }
            const Vec2F64& GetPosition() const { return GetComponent<Vec2F64, 0>(); }
            void SetPosition(const Vec2F64& _value) { return SetComponent<Vec2F64, 0>(_value); }

            const double& GetX() const { return GetPosition()[0]; };
            double& GetX() { return GetPosition()[0]; };
            void SetX(double _value) { GetPosition()[0] = _value; };

            const double& GetY() const { return GetPosition()[1]; };
            double& GetY() { return GetPosition()[1]; };
            void SetY(double _value) { GetPosition()[1] = _value; };

        private:
            double components[N];
        };

        template<typename Input, size_t Output>
        using VertexShader = std::function<Vertex<Output>(const Input&)>;

        template<size_t N>
        using PixelShader = std::function<Utilities::Color(const Vertex<N>&)>;

        template<typename V>
        struct Mesh
        {
            typedef std::array<V*, 3> Triangle;

            Mesh(std::vector<V>&& _vertices, const std::vector<std::array<size_t, 3>>& _triangleIndices) : vertices(_vertices), triangles(_vertices.size())
            {
                //TODO: Add checks that indices are valid
                for (auto& indices : _triangleIndices)
                    triangles.push_back({ &vertices[indices[0]], &vertices[indices[1]], &vertices[indices[2]] });
            }

            const std::vector<Triangle> GetTriangles() const { return triangles; }
        private:
            std::vector<V> vertices;
            std::vector<Triangle> triangles;
        };

        template<size_t N>
        class RasterizedEdge
        {
            Vertex<N> value;
            double step[N];

        public:
            RasterizedEdge(const Vertex<N>& _start, const Vertex<N>& _end, double _stepSize) : value(_start)
            {
                for (size_t i = 0; i < N; ++i)
                    step[i] = (_end[i] - _start[i]) * _stepSize;
            }

            void Step()
            {
                for (size_t i = 0; i < N; ++i)
                    value[i] += step[i];
            }

            const Vertex<N>& GetValue() const { return value; }
        };

        template<size_t N>
        void Rasterize(Target* _target, const Vertex<N>& _v1, const Vertex<N>& _v2, PixelShader<N> _ps)
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

        template<size_t N>
        void Rasterize(Target* _target, int64_t _yStart, int64_t _yEnd, RasterizedEdge<N>& _lEdge, RasterizedEdge<N>& _rEdge, PixelShader<N> _ps)
        {
            uint64_t targetWidth = _target->GetWidth();

            for (int64_t y = _yStart; y < _yEnd; ++y)
            {
                const Vertex<N>* vLeft = &_lEdge.GetValue(), * vRight = &_rEdge.GetValue();

                if (vRight->GetX() < vLeft->GetX())
                    std::swap(vLeft, vRight);

                int64_t xStart = std::ceil((vLeft->GetX() + 0.5) * targetWidth), xEnd = std::ceil((vRight->GetX() + 0.5) * targetWidth);
                auto scanLine = RasterizedEdge<N>(*vLeft, *vRight, 1.0 / (xEnd - xStart));

                for (int64_t x = xStart; x < xEnd; x++)
                {
                    _target->SetPixel(x, y, _ps(scanLine.GetValue()));
                    scanLine.Step();
                }

                _lEdge.Step();
                _rEdge.Step();
            }
        }

        template<size_t N>
        void Rasterize(Target* _target, const Vertex<N>& _v1, const Vertex<N>& _v2, const Vertex<N>& _v3, PixelShader<N> _ps, bool _wireframe = false)
        {
            //Sort vertices
            const Vertex<N>* top = &_v1, * middle = &_v2, * bottom = &_v3;

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
                auto xTB = RasterizedEdge<N>(*top, *bottom, 1.0 / (yEnd2 - y)), xTM = RasterizedEdge<N>(*top, *middle, 1.0 / (yEnd1 - y)), xMB = RasterizedEdge<N>(*middle, *bottom, 1.0 / (yEnd2 - yEnd1));

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

        template<size_t VertexSize>
        void Rasterize(Target* _target, const std::vector<std::array<Vertex<VertexSize>*, 3>>& _triangles, PixelShader<VertexSize> _ps, bool _wireframe = false)
        {
            for (auto& triangle : _triangles)
                Rasterize(_target, *triangle[0], *triangle[1], *triangle[2], _ps, _wireframe);
        }
    }
}