#pragma once

#include "SDL2/SDL.h"
#include <string>
#include <functional>
#include <thread>
#include <tuple>
#include <vector>
#include <cmath>
#include <concepts>

//TODO: Move to cpp file
#define CHECK_SDL(value, message) do { if (!(value)) throw std::runtime_error(std::string(message) + " Error:\n" + SDL_GetError()); } while(false)

namespace rxd
{
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
        template<typename T>
        struct Vec2
        {
            T x, y;

            Vec2(T _x = 0, T _y = 0) : x(_x), y(_y) { }

            static double LengthSquared(const Vec2& _a, const Vec2& _b) { return (_b.x - _a.x) * (_b.x - _a.x) + (_b.y - _a.y) * (_b.y - _a.y); }
            static double Length(const Vec2& _a, const Vec2& _b) { return std::sqrt(LengthSquared(_a, _b)); }
            static Vec2 Lerp(const Vec2& _a, const Vec2& _b, double _amt) { return Vec2(_a.x + (_b.x - _a.x) * _amt, _a.y + (_b.y - _a.y) * _amt); }
        };

        template<typename T>
        Vec2<T> operator-(const Vec2<T>& _left, const Vec2<T>& _right) { return Vec2<T>(_left.x - _right.x, _left.y - _right.y); }

        template<typename T>
        std::string ToString(Vec2<T> _v) { return "(" + std::to_string(_v.x) + ", " + std::to_string(_v.y) + ")"; }

        template<typename T>
        struct Vec4
        {
            T x, y, z, w;

            Vec4(T _x = 0, T _y = 0, T _z = 0, T _w = 0) : x(_x), y(_y), z(_z), w(_w) { }

            static Vec4 Lerp(const Vec4& _a, const Vec4& _b, double _amt)
            {
                T x = _a.x + (_b.x - _a.x) * _amt;
                T y = _a.y + (_b.y - _a.y) * _amt;
                T z = _a.z + (_b.z - _a.z) * _amt;
                T w = _a.w + (_b.w - _a.w) * _amt;

                return Vec4(x, y, z, w);
            }
        };

        template<typename T>
        Vec4<T> operator-(const Vec4<T>& _left, const Vec4<T>& _right) { return Vec4<T>(_left.x - _right.x, _left.y - _right.y, _left.z - _right.z, _left.w - _right.w); }

        template<typename T>
        std::string ToString(Vec4<T> _v) { return "(" + std::to_string(_v.x) + ", " + std::to_string(_v.y) + ", " + std::to_string(_v.z) + ", " + std::to_string(_v.w) + ")"; }

        typedef Vec2<int64_t> Vec2I64;
        typedef Vec2<double> Vec2F64;
        typedef Vec4<int64_t> Vec4I64;
        typedef Vec4<double> Vec4F64;

        struct Color
        {
            uint8_t a, r, g, b;

            Color(uint8_t _a, uint8_t _r, uint8_t _g, uint8_t _b) : a(_a), r(_r), g(_g), b(_b) { }
            Color(Vec4F64 _v) : Color(_v.x * 255, _v.y * 255, _v.z * 255, _v.w * 255) { }
            Color(uint32_t _value) : Color((_value >> 24) & 255, (_value >> 16) & 255, (_value >> 8) & 255, _value & 255) { }
            Color() : Color(255, 0, 0, 0) { }

            Vec4F64 ToVec4F64() { return { a / 255.0, r / 255.0, g / 255.0, b / 255.0 }; }

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

            Vertex(Utilities::Vec2F64 _pos) { SetPosition(_pos); }

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

            Utilities::Vec2F64& GetPosition() { return GetComponent<Utilities::Vec2F64, 0>(); }
            const Utilities::Vec2F64& GetPosition() const { return GetComponent<Utilities::Vec2F64, 0>(); }
            void SetPosition(const Utilities::Vec2F64& _value) { return SetComponent<Utilities::Vec2F64, 0>(_value); }

            const double& GetX() const { return GetPosition().x; };
            double& GetX() { return GetPosition().x; };
            void SetX(double _value) { GetPosition().x = _value; };

            const double& GetY() const { return GetPosition().y; };
            double& GetY() { return GetPosition().y; };
            void SetY(double _value) { GetPosition().y = _value; };

        private:
            double components[N];
        };

        template<typename Input, size_t Output>
        using VertexShader = std::function<Vertex<Output>(const Input&)>;

        template<size_t N>
        using PixelShader = std::function<Utilities::Color(const Vertex<N>&)>;

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

        // template<typename VSIn, RasterizableVertex PSIn>
        // void Rasterize(Target* _target, const VSIn& _v1, const VSIn& _v2, VertexShader<VSIn, PSIn> _vs, PixelShader<PSIn> _ps)
        // {
        //     uint64_t targetWidth = _target->GetWidth(), targetHeight = _target->GetHeight();

        //     PSIn left = _vs(_v1), right = _vs(_v2);
        //     if (left.GetX() > right.GetX())
        //         std::swap(left, right);

        //     int64_t yStart = std::ceil(left.GetY() * targetHeight), yEnd = std::ceil(right.GetY() * targetHeight);

        //     if (yStart == yEnd) // Horizontal line
        //     {
        //         int64_t y = std::ceil(left.GetY() * targetHeight);
        //         int64_t xStart = std::ceil(left.GetX() * targetWidth), xEnd = std::ceil(right.GetX() * targetWidth);
        //         double scanLength = double(xEnd - xStart);

        //         for (int64_t x = xStart; x < xEnd; x++)
        //             _target->SetPixel(x, y, _ps(PSIn::Lerp(left, right, (x - xStart) / scanLength)));
        //     }
        //     else
        //     {
        //         auto edge = RasterizedEdge<PSIn>(left, right, 1.0 / (yStart - yEnd));
        //         double lastX = edge.GetValue().GetX(), distSquared = Utilities::Vec2F64::LengthSquared(right.GetPosition(), left.GetPosition());

        //         if (yStart < yEnd)
        //         {
        //             for (int64_t y = yStart; y < yEnd; y++)
        //             {
        //                 int64_t xStart = std::ceil(lastX * targetWidth);
        //                 int64_t xEnd = std::max(xStart + 1, (int64_t)std::ceil(edge.GetValue().GetX() * targetWidth));

        //                 for (int64_t x = xStart; x < xEnd; x++)
        //                 {
        //                     double t = Utilities::Vec2F64::LengthSquared({ x / double(targetWidth), y / double(targetHeight) }, left.GetPosition()) / distSquared;
        //                     _target->SetPixel(x, y, _ps(PSIn::Lerp(left, right, t)));
        //                 }

        //                 lastX = edge.GetValue().GetX();
        //                 edge.Step();
        //             }
        //         }
        //         else
        //         {
        //             for (int64_t y = yStart; y > yEnd; y--)
        //             {
        //                 int64_t xStart = std::ceil(lastX * targetWidth);
        //                 int64_t xEnd = std::max(xStart + 1, (int64_t)std::ceil(edge.GetValue().GetX() * targetWidth));

        //                 for (int64_t x = xStart; x < xEnd; x++)
        //                 {
        //                     double t = Utilities::Vec2F64::LengthSquared({ x / double(targetWidth), y / double(targetHeight) }, left.GetPosition()) / distSquared;
        //                     _target->SetPixel(x, y, _ps(PSIn::Lerp(left, right, t)));
        //                 }

        //                 lastX = edge.GetValue().GetX();
        //                 edge.Step();
        //             }
        //         }
        //     }
        // }

        template<size_t N>
        void Rasterize(Target* _target, int64_t _yStart, int64_t _yEnd, RasterizedEdge<N>& _lEdge, RasterizedEdge<N>& _rEdge, PixelShader<N> _ps)
        {
            uint64_t targetWidth = _target->GetWidth();

            for (int64_t y = _yStart; y < _yEnd; y++)
            {
                const Vertex<N>& vLeft = _lEdge.GetValue(), & vRight = _rEdge.GetValue();
                int64_t xStart = std::ceil(vLeft.GetX() * targetWidth), xEnd = std::ceil(vRight.GetX() * targetWidth);
                auto scanLine = RasterizedEdge<N>(vLeft, vRight, 1.0 / (xEnd - xStart));

                for (int64_t x = xStart; x < xEnd; x++)
                {
                    _target->SetPixel(x, y, _ps(scanLine.GetValue()));
                    scanLine.Step();
                }

                _lEdge.Step();
                _rEdge.Step();
            }
        }

        template<typename VSIn, size_t PSIn>
        void Rasterize(Target* _target, const VSIn& _v1, const VSIn& _v2, const VSIn& _v3, VertexShader<VSIn, PSIn> _vs, PixelShader<PSIn> _ps, bool _wireframe = false)
        {
            //Sort vertices
            Vertex<PSIn> top = _vs(_v1), middle = _vs(_v2), bottom = _vs(_v3);

            if (std::make_tuple(bottom.GetY(), bottom.GetX()) < std::make_tuple(middle.GetY(), middle.GetX())) { std::swap(bottom, middle); }
            if (std::make_tuple(middle.GetY(), middle.GetX()) < std::make_tuple(top.GetY(), top.GetX())) { std::swap(middle, top); }
            if (std::make_tuple(bottom.GetY(), bottom.GetX()) < std::make_tuple(middle.GetY(), middle.GetX())) { std::swap(bottom, middle); }

            //Rasterize
            if (_wireframe)
            {
                // Rasterize(_target, top, bottom, _vs, _ps);
                // Rasterize(_target, top, middle, _vs, _ps);
                // Rasterize(_target, middle, bottom, _vs, _ps);
            }
            else
            {
                auto topToBottom = bottom.GetPosition() - top.GetPosition(), topToMiddle = middle.GetPosition() - top.GetPosition();
                bool rightHanded = topToMiddle.x * topToBottom.y - topToBottom.x * topToMiddle.y >= 0;

                uint64_t targetHeight = _target->GetHeight();
                int64_t y = std::ceil(top.GetY() * targetHeight), yEnd1 = std::ceil(middle.GetY() * targetHeight), yEnd2 = std::ceil(bottom.GetY() * targetHeight);
                auto xTB = RasterizedEdge<PSIn>(top, bottom, 1.0 / (yEnd2 - y)), xTM = RasterizedEdge<PSIn>(top, middle, 1.0 / (yEnd1 - y)), xMB = RasterizedEdge<PSIn>(middle, bottom, 1.0 / (yEnd2 - yEnd1));

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
    }
}