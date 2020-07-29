// Copyright 2019, 2020 Pokitec
// All rights reserved.

#include "Path.hpp"
#include "PixelMap.inl"
#include "BezierCurve.hpp"
#include "PixelMap.hpp"
#include "required.hpp"

namespace tt {

ssize_t Path::numberOfContours() const noexcept
{
    return nonstd::ssize(contourEndPoints);
}

ssize_t Path::numberOfLayers() const noexcept
{
    return nonstd::ssize(layerEndContours);
}

bool Path::hasLayers() const noexcept
{
    return numberOfLayers() > 0;
}

bool Path::allLayersHaveSameColor() const noexcept
{
    if (!hasLayers()) {
        return true;
    }

    ttlet &firstColor = layerEndContours.front().second;

    for (ttlet &[endContour, color] : layerEndContours) {
        if (color != firstColor) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] aarect Path::boundingBox() const noexcept
{
    if (nonstd::ssize(points) == 0) {
        return aarect{0.0, 0.0, 0.0, 0.0};
    }

    auto r = aarect::p0p3(points.front().p, points.front().p);

    for (ttlet &point: points) {
        r |= point.p;
    }

    return r;
}

void Path::tryRemoveLayers() noexcept
{
    if (!hasLayers()) {
        return;
    }

    if (!allLayersHaveSameColor()) {
        return;
    }

    layerEndContours.clear();
}

std::vector<BezierPoint>::const_iterator Path::beginContour(ssize_t contourNr) const noexcept
{
    return points.begin() + (contourNr == 0 ? 0 : contourEndPoints.at(contourNr - 1) + 1);
}

std::vector<BezierPoint>::const_iterator Path::endContour(ssize_t contourNr) const noexcept
{
    return points.begin() + contourEndPoints.at(contourNr) + 1;
}

ssize_t Path::beginLayer(ssize_t layerNr) const noexcept
{
    return layerNr == 0 ? 0 : layerEndContours.at(layerNr - 1).first + 1;
}

ssize_t Path::endLayer(ssize_t layerNr) const noexcept
{
    return layerEndContours.at(layerNr).first + 1;
}

vec Path::getColorOfLayer(ssize_t layerNr) const noexcept
{
    return layerEndContours.at(layerNr).second;
}

void Path::setColorOfLayer(ssize_t layerNr, vec fillColor) noexcept
{
    layerEndContours.at(layerNr).second = fillColor;
}

std::pair<Path,vec> Path::getLayer(ssize_t layerNr) const noexcept
{
    tt_assert(hasLayers());

    auto path = Path{};

    ttlet begin = beginLayer(layerNr);
    ttlet end = endLayer(layerNr);
    for (ssize_t contourNr = begin; contourNr != end; contourNr++) {
        path.addContour(beginContour(contourNr), endContour(contourNr));
    }

    return {path, getColorOfLayer(layerNr)};
}

void Path::optimizeLayers() noexcept
{
    if (nonstd::ssize(layerEndContours) == 0) {
        return;
    }

    decltype(layerEndContours) tmp;
    tmp.reserve(nonstd::ssize(layerEndContours));

    auto prev_i = layerEndContours.begin(); 
    for (auto i = prev_i + 1; i != layerEndContours.end(); ++i) {
        // Add the last layer of a contiguous color.
        if (prev_i->second != i->second) {
            tmp.push_back(*prev_i);
        }

        prev_i = i;
    }
    tmp.push_back(*prev_i);

    std::swap(layerEndContours, tmp);
}


std::vector<BezierPoint> Path::getBezierPointsOfContour(ssize_t subpathNr) const noexcept
{
    ttlet begin = points.begin() + (subpathNr == 0 ? 0 : contourEndPoints.at(subpathNr - 1) + 1);
    ttlet end = points.begin() + contourEndPoints.at(subpathNr) + 1;
    return std::vector(begin, end);
}

std::vector<BezierCurve> Path::getBeziersOfContour(ssize_t contourNr) const noexcept
{
    return makeContourFromPoints(beginContour(contourNr), endContour(contourNr));
}

std::vector<BezierCurve> Path::getBeziers() const noexcept
{
    tt_assert(!hasLayers());

    std::vector<BezierCurve> r;

    for (auto contourNr = 0; contourNr < numberOfContours(); contourNr++) {
        ttlet beziers = getBeziersOfContour(contourNr);
        r.insert(r.end(), beziers.begin(), beziers.end());
    }
    return r;
}

bool Path::isContourOpen() const noexcept
{
    if (points.size() == 0) {
        return false;
    } else if (contourEndPoints.size() == 0) {
        return true;
    } else {
        return contourEndPoints.back() != (nonstd::ssize(points) - 1);
    }
}

void Path::closeContour() noexcept
{
    if (isContourOpen()) {
        contourEndPoints.push_back(nonstd::ssize(points) - 1);
    }
}

bool Path::isLayerOpen() const noexcept
{
    if (points.size() == 0) {
        return false;
    } else if (isContourOpen()) {
        return true;
    } else if (layerEndContours.size() == 0) {
        return true;
    } else {
        return layerEndContours.back().first != (nonstd::ssize(contourEndPoints) - 1);
    }
}

void Path::closeLayer(vec fillColor) noexcept
{
    closeContour();
    if (isLayerOpen()) {
        layerEndContours.emplace_back(nonstd::ssize(contourEndPoints) - 1, fillColor);
    }
}

vec Path::currentPosition() const noexcept
{
    if (isContourOpen()) {
        return points.back().p;
    } else {
        return vec::point(0.0f, 0.0f);
    }
}

void Path::moveTo(vec position) noexcept
{
    tt_assume(position.is_point());
    closeContour();
    points.emplace_back(position, BezierPoint::Type::Anchor);
}

void Path::moveRelativeTo(vec direction) noexcept
{
    tt_assert(isContourOpen());
    tt_assume(direction.is_vector());

    ttlet lastPosition = currentPosition();
    closeContour();
    points.emplace_back(lastPosition + direction, BezierPoint::Type::Anchor);
}

void Path::lineTo(vec position) noexcept
{
    tt_assert(isContourOpen());
    tt_assume(position.is_point());

    points.emplace_back(position, BezierPoint::Type::Anchor);
}

void Path::lineRelativeTo(vec direction) noexcept
{
    tt_assert(isContourOpen());
    tt_assume(direction.is_vector());

    points.emplace_back(currentPosition() + direction, BezierPoint::Type::Anchor);
}

void Path::quadraticCurveTo(vec controlPosition, vec position) noexcept
{
    tt_assert(isContourOpen());
    tt_assume(controlPosition.is_point());
    tt_assume(position.is_point());

    points.emplace_back(controlPosition, BezierPoint::Type::QuadraticControl);
    points.emplace_back(position, BezierPoint::Type::Anchor);
}

void Path::quadraticCurveRelativeTo(vec controlDirection, vec direction) noexcept
{
    tt_assert(isContourOpen());
    tt_assume(controlDirection.is_vector());
    tt_assume(direction.is_vector());

    ttlet p = currentPosition();
    points.emplace_back(p + controlDirection, BezierPoint::Type::QuadraticControl);
    points.emplace_back(p + direction, BezierPoint::Type::Anchor);
}

void Path::cubicCurveTo(vec controlPosition1, vec controlPosition2, vec position) noexcept
{
    tt_assert(isContourOpen());
    tt_assume(controlPosition1.is_point());
    tt_assume(controlPosition2.is_point());
    tt_assume(position.is_point());

    points.emplace_back(controlPosition1, BezierPoint::Type::CubicControl1);
    points.emplace_back(controlPosition2, BezierPoint::Type::CubicControl2);
    points.emplace_back(position, BezierPoint::Type::Anchor);
}

void Path::cubicCurveRelativeTo(vec controlDirection1, vec controlDirection2, vec direction) noexcept
{
    tt_assert(isContourOpen());
    tt_assume(controlDirection1.is_vector());
    tt_assume(controlDirection2.is_vector());
    tt_assume(direction.is_vector());

    ttlet p = currentPosition();
    points.emplace_back(p + controlDirection1, BezierPoint::Type::CubicControl1);
    points.emplace_back(p + controlDirection2, BezierPoint::Type::CubicControl2);
    points.emplace_back(p + direction, BezierPoint::Type::Anchor);
}

void Path::arcTo(float radius, vec position) noexcept
{
    tt_assert(isContourOpen());
    tt_assume(position.is_point());

    ttlet r = std::abs(radius);
    ttlet P1 = currentPosition();
    ttlet P2 = position;
    ttlet Pm = midpoint(P1, P2);

    ttlet Vm2 = P2 - Pm;

    // Calculate the half angle between vectors P0 - C and P2 - C.
    ttlet alpha = std::asin(length(Vm2) / r);

    // Calculate the center point C. As the length of the normal of Vm2 at Pm.
    ttlet C = Pm + normal(Vm2) * vec{std::cos(alpha)} * vec{radius};

    // Calculate vectors from center to end points.
    ttlet VC1 = P1 - C;
    ttlet VC2 = P2 - C;

    ttlet q1 = length_squared(VC1);
    ttlet q2 = q1 + dot(VC1, VC2);
    ttlet k2 = (4.0f/3.0f) * (std::sqrt(2.0f * q1 * q2) - q2) / viktor_cross(VC1, VC2);

    // Calculate the control points.
    ttlet C1 = vec::point(
        (C.x() + VC1.x()) - k2 * VC1.y(),
        (C.y() + VC1.y()) + k2 * VC1.x()
    );
    ttlet C2 = vec::point(
        (C.x() + VC2.x()) + k2 * VC2.y(),
        (C.y() + VC2.y()) - k2 * VC2.x()
    );

    cubicCurveTo(C1, C2, P2);
}

void Path::addRectangle(aarect r, vec corners) noexcept
{
    tt_assert(!isContourOpen());

    ttlet radii = abs(corners);

    ttlet blc = r.corner<0>();
    ttlet brc = r.corner<1>();
    ttlet tlc = r.corner<2>();
    ttlet trc = r.corner<3>();

    ttlet blc1 = blc + vec{0.0f, radii.x()};
    ttlet blc2 = blc + vec{radii.x(), 0.0f};
    ttlet brc1 = brc + vec{-radii.y(), 0.0f};
    ttlet brc2 = brc + vec{0.0f, radii.y()};
    ttlet tlc1 = tlc + vec{radii.z(), 0.0f};
    ttlet tlc2 = tlc + vec{0.0f, -radii.z()};
    ttlet trc1 = trc + vec{0.0f, -radii.w()};
    ttlet trc2 = trc + vec{-radii.w(), 0.0f};

    moveTo(blc1);
    if (corners.x() > 0.0) {
        arcTo(radii.x(), blc2);
    } else if (corners.x() < 0.0) {
        lineTo(blc2);
    }

    lineTo(brc1);
    if (corners.y() > 0.0) {
        arcTo(radii.y(), brc2);
    } else if (corners.y() < 0.0) {
        lineTo(blc2);
    }

    lineTo(tlc1);
    if (corners.z() > 0.0) {
        arcTo(radii.z(), tlc2);
    } else if (corners.z() < 0.0) {
        lineTo(tlc2);
    }

    lineTo(trc1);
    if (corners.w() > 0.0) {
        arcTo(radii.w(), trc2);
    } else if (corners.w() < 0.0) {
        lineTo(trc2);
    }

    closeContour();
}

void Path::addCircle(vec position, float radius) noexcept
{
    tt_assert(!isContourOpen());
    tt_assume(position.is_point());

    moveTo(vec::point(position.x(), position.y() - radius));
    arcTo(radius, vec::point(position.x() + radius, position.y()));
    arcTo(radius, vec::point(position.x(), position.y() + radius));
    arcTo(radius, vec::point(position.x() - radius, position.y()));
    arcTo(radius, vec::point(position.x(), position.y() - radius));
    closeContour();
}

void Path::addContour(std::vector<BezierPoint>::const_iterator const &begin, std::vector<BezierPoint>::const_iterator const &end) noexcept
{
    tt_assert(!isContourOpen());
    points.insert(points.end(), begin, end);
    closeContour();
}

void Path::addContour(std::vector<BezierPoint> const &contour) noexcept
{
    addContour(contour.begin(), contour.end());
}

void Path::addContour(std::vector<BezierCurve> const &contour) noexcept
{
    tt_assert(!isContourOpen());

    for (ttlet &curve: contour) {
        // Don't emit the first point, the last point of the contour will wrap around.
        switch (curve.type) {
        case BezierCurve::Type::Linear:
            points.emplace_back(curve.P2, BezierPoint::Type::Anchor);
            break;
        case BezierCurve::Type::Quadratic:
            points.emplace_back(curve.C1, BezierPoint::Type::QuadraticControl);
            points.emplace_back(curve.P2, BezierPoint::Type::Anchor);
            break;
        case BezierCurve::Type::Cubic:
            points.emplace_back(curve.C1, BezierPoint::Type::CubicControl1);
            points.emplace_back(curve.C2, BezierPoint::Type::CubicControl2);
            points.emplace_back(curve.P2, BezierPoint::Type::Anchor);
            break;
        default:
            tt_no_default;
        }
    }

    closeContour();
}

void Path::addPath(Path const &path, vec fillColor) noexcept
{
    *this += path;
    closeLayer(fillColor);
}

void Path::addStroke(Path const &path, vec strokeColor, float strokeWidth, LineJoinStyle lineJoinStyle, float tolerance) noexcept
{
    *this += path.toStroke(strokeWidth, lineJoinStyle, tolerance);
    closeLayer(strokeColor);
}

Path Path::toStroke(float strokeWidth, LineJoinStyle lineJoinStyle, float tolerance) const noexcept
{
    tt_assert(!hasLayers());
    tt_assert(!isContourOpen());

    auto r = Path{};

    float starboardOffset = strokeWidth / 2;
    float portOffset = -starboardOffset;

    for (int i = 0; i < numberOfContours(); i++) {
        ttlet baseContour = getBeziersOfContour(i);

        ttlet starboardContour = makeParrallelContour(baseContour, starboardOffset, lineJoinStyle, tolerance);
        r.addContour(starboardContour);

        ttlet portContour = makeInverseContour(makeParrallelContour(baseContour, portOffset, lineJoinStyle, tolerance));
        r.addContour(portContour);
    }

    return r;
}

Path &Path::operator+=(Path const &rhs) noexcept
{
    tt_assert(!isContourOpen());
    tt_assert(!rhs.isContourOpen());

    // Left hand layer can only be open if the right hand side contains no layers.
    tt_assert(!rhs.hasLayers() || !isLayerOpen());

    ttlet pointOffset = nonstd::ssize(points);
    ttlet contourOffset = nonstd::ssize(contourEndPoints);

    layerEndContours.reserve(layerEndContours.size() + rhs.layerEndContours.size());
    for (ttlet &[x, fillColor]: rhs.layerEndContours) {
        layerEndContours.emplace_back(contourOffset + x, fillColor);
    }

    contourEndPoints.reserve(contourEndPoints.size() + rhs.contourEndPoints.size());
    for (ttlet x: rhs.contourEndPoints) {
        contourEndPoints.push_back(pointOffset + x);
    }

    points.insert(points.end(), rhs.points.begin(), rhs.points.end());
    return *this;
}

Path Path::centerScale(vec extent, float padding) const noexcept
{
    tt_assume(extent.is_vector());

    auto max_size = vec{
        std::max(1.0f, extent.x() - (padding * 2.0f)),
        std::max(1.0f, extent.y() - (padding * 2.0f))
    };

    auto bbox = boundingBox();
    if (bbox.width() <= 0.0 || bbox.height() <= 0.0) {
        return {};
    }

    ttlet scale = std::min(
        max_size.x() / bbox.width(),
        max_size.y() / bbox.height()
    );
    bbox *= scale;
    
    ttlet offset = -bbox.offset() + (extent - bbox.extent()) * vec{0.5f};

    return (mat::T(offset) * mat::S(scale, scale, 1.0f)) * *this;
}



void composit(PixelMap<R16G16B16A16SFloat>& dst, vec color, Path const &path) noexcept
{
    tt_assert(!path.hasLayers());
    tt_assert(!path.isContourOpen());

    auto mask = PixelMap<uint8_t>(dst.width, dst.height);
    fill(mask);

    ttlet curves = path.getBeziers();
    fill(mask, curves);

    composit(dst, color, mask);
}

void composit(PixelMap<R16G16B16A16SFloat>& dst, Path const &src) noexcept
{
    tt_assert(src.hasLayers() && !src.isLayerOpen());

    for (int layerNr = 0; layerNr < src.numberOfLayers(); layerNr++) {
        ttlet [layer, fillColor] = src.getLayer(layerNr);

        composit(dst, fillColor, layer);
    }
}

void fill(PixelMap<SDF8> &dst, Path const &path) noexcept
{
    fill(dst, path.getBeziers());
}

}