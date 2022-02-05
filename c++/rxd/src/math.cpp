#include "math.h"

namespace rxd::math
{
    namespace raytracing
    {
        std::optional<Intersection> TryIntersect(const Ray& _ray, const std::vector<Intersectable*>& _intersectables, bool _chooseClosest)
        {
            if (_chooseClosest)
            {
                Intersectable* closest = nullptr;
                double t = 1.0;

                for (auto intersectable : _intersectables)
                {
                    auto intersection = intersectable->GetIntersection(_ray);
                    if (intersection.has_value() && intersection.value() >= 0.0 && intersection.value() <= t)
                    {
                        closest = intersectable;
                        t = intersection.value();
                    }
                }

                if (closest) { return Intersection{ closest, _ray.GetPoint(t) }; }
                else { return std::nullopt; }
            }
            else
            {
                for (auto intersectable : _intersectables)
                {
                    auto intersection = intersectable->GetIntersection(_ray);
                    if (intersection.has_value() && intersection.value() >= 0.0 && intersection.value() <= 1.0)
                        return Intersection{ intersectable, _ray.GetPoint(intersection.value()) };
                }

                return std::nullopt;
            }
        }
    }
}