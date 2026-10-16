#include "HoT/MSDF/Validator.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>
#include <msdfgen.h>
#include <msdfgen-ext.h>

namespace HoT::MSDF::Validator {

static const double FLATTEN_EPS = 0.5;
static const double EPS = 1e-9;
static const double MAX_COORD = 1e9;
static const size_t MIN_CONTOUR_SIZE = 3;
static const int32_t MAX_CURVE_SAMPLES = 10;

struct Vec {
    double m_x = 0.0;
    double m_y = 0.0;

    Vec() = default;
    Vec(double x, double y) : m_x(x), m_y(y) {}

    double LengthSq() const { return this->m_x * this->m_x + this->m_y * this->m_y; }
};

static bool IsValidCoord(double v) {
    return !std::isnan(v) && !std::isinf(v) && std::abs(v) <= MAX_COORD;
}

static bool SamePoint(const Vec& a, const Vec& b) {
    return Vec(a.m_x - b.m_x, a.m_y - b.m_y).LengthSq() <= EPS * EPS;
}

static double DistPointToLine(const Vec& p, const Vec& a, const Vec& b) {
    const double vx = b.m_x - a.m_x;
    const double vy = b.m_y - a.m_y;
    const double wx = p.m_x - a.m_x;
    const double wy = p.m_y - a.m_y;
    const double c2 = vx * vx + vy * vy;

    if (c2 <= EPS) {
        return std::hypot(wx, wy);
    }

    const double t = std::clamp((wx * vx + wy * vy) / c2, 0.0, 1.0);

    return std::hypot(p.m_x - (a.m_x + t * vx), p.m_y - (a.m_y + t * vy));
}

static int32_t Orient(const Vec& a, const Vec& b, const Vec& c) {
    const double v = (b.m_x - a.m_x) * (c.m_y - a.m_y) - (b.m_y - a.m_y) * (c.m_x - a.m_x);

    return (v > EPS) - (v < -EPS);
}

static bool OnSegment(const Vec& a, const Vec& b, const Vec& p) {
    if (Orient(a, b, p) != 0) {
        return false;
    }

    return std::min(a.m_x, b.m_x) - EPS <= p.m_x && p.m_x <= std::max(a.m_x, b.m_x) + EPS && std::min(a.m_y, b.m_y) - EPS <= p.m_y && p.m_y <= std::max(a.m_y, b.m_y) + EPS;
}

static bool SegmentsIntersect(const Vec& a1, const Vec& a2, const Vec& b1, const Vec& b2) {
    const int32_t o1 = Orient(a1, a2, b1);
    const int32_t o2 = Orient(a1, a2, b2);
    const int32_t o3 = Orient(b1, b2, a1);
    const int32_t o4 = Orient(b1, b2, a2);

    if (o1 != o2 && o3 != o4) {
        return true;
    }

    return (o1 == 0 && OnSegment(a1, a2, b1)) || (o2 == 0 && OnSegment(a1, a2, b2)) || (o3 == 0 && OnSegment(b1, b2, a1)) || (o4 == 0 && OnSegment(b1, b2, a2));
}

static bool HasSelfIntersections(const std::vector<Vec>& pts) {
    const size_t n = pts.size();

    if (n < 4) {
        return false;
    }

    for (size_t i = 0; i < n; i++) {
        const Vec& a1 = pts[i];
        const Vec& a2 = pts[(i + 1) % n];

        if (SamePoint(a2, a1)) {
            continue;
        }

        for (size_t j = i + 2; j < n; j++) {
            if (i == 0 && j == n - 1) {
                continue;
            }

            const Vec& b1 = pts[j];
            const Vec& b2 = pts[(j + 1) % n];

            if (SamePoint(b2, b1)) {
                continue;
            }

            if (SegmentsIntersect(a1, a2, b1, b2)) {
                bool shareEndpoint = SamePoint(a1, b1) || SamePoint(a1, b2) || SamePoint(a2, b1) || SamePoint(a2, b2);

                if (!shareEndpoint) {
                    return true;
                }
            }
        }
    }

    return false;
}

static std::vector<Vec> FlattenContour(const msdfgen::Contour& contour, double tol) {
    std::vector<Vec> result;

    for (const msdfgen::EdgeHolder& edge : contour.edges) {
        if (!edge) {
            continue;
        }

        const msdfgen::Point2 start = edge->point(0);

        if (result.empty()) {
            result.emplace_back(start.x, start.y);
        }

        std::vector<Vec> edgePoints;
        edgePoints.emplace_back(start.x, start.y);

        for (int32_t j = 1; j <= MAX_CURVE_SAMPLES; j++) {
            const msdfgen::Point2 pt = edge->point(static_cast<double>(j) / MAX_CURVE_SAMPLES);
            edgePoints.emplace_back(pt.x, pt.y);
        }

        bool isLinear = true;
        const Vec first = edgePoints.front();
        const Vec last = edgePoints.back();

        for (size_t j = 1; j < edgePoints.size() - 1; j++) {
            if (DistPointToLine(edgePoints[j], first, last) > tol) {
                isLinear = false;
                break;
            }
        }

        if (isLinear) {
            result.push_back(edgePoints.back());
        } else {
            for (size_t j = 1; j < edgePoints.size(); j++) {
                result.push_back(edgePoints[j]);
            }
        }
    }

    return result;
}

static bool ValidateResolvedShape(const msdfgen::Shape& shape, double tol) {
    if (shape.contours.empty()) {
        return false;
    }

    for (const msdfgen::Contour& contour : shape.contours) {
        if (contour.edges.empty()) {
            return false;
        }

        std::vector<Vec> pts = FlattenContour(contour, tol);

        if (pts.size() < MIN_CONTOUR_SIZE) {
            return false;
        }

        for (const Vec& p : pts) {
            if (!IsValidCoord(p.m_x) || !IsValidCoord(p.m_y)) {
                return false;
            }
        }

        bool hasNonDegenerateEdge = false;

        for (size_t i = 0; i < pts.size(); i++) {
            if (!SamePoint(pts[(i + 1) % pts.size()], pts[i])) {
                hasNonDegenerateEdge = true;
                break;
            }
        }

        if (!hasNonDegenerateEdge || HasSelfIntersections(pts)) {
            return false;
        }
    }

    return true;
}

static bool IsGlyphValid(msdfgen::FontHandle* font, uint32_t codepoint) {
    msdfgen::Shape shape;

    if (!msdfgen::loadGlyph(shape, font, static_cast<msdfgen::unicode_t>(codepoint))) {
        return false;
    }

    if (shape.contours.empty()) {
        return true;
    }

    msdfgen::resolveShapeGeometry(shape);

    return ValidateResolvedShape(shape, FLATTEN_EPS);
}

bool IsFontCompatible(msdfgen::FontHandle* font) {
    if (!font) {
        return false;
    }

    for (uint32_t cp = 32; cp < 127; cp++) {
        if (!IsGlyphValid(font, cp)) {
            return false;
        }
    }

    return true;
}

}
