// Copyright 2020 Pokitec
// All rights reserved.

#pragma once

#include "TTauri/Foundation/required.hpp"
#include "TTauri/Foundation/numeric_cast.hpp"
#include "TTauri/Foundation/strings.hpp"
#include "TTauri/Foundation/exceptions.hpp"
#include "TTauri/Foundation/float16.hpp"
#include <fmt/format.h>
#include <xmmintrin.h>
#include <immintrin.h>
#include <smmintrin.h>
#include <cstdint>
#include <stdexcept>
#include <array>
#include <ostream>

namespace tt {

/** A 4D vector.
 *
 * If you need a 2D or 3D vector, point or color, you can use this vector class
 * as a homogenious coordinate.
 *
 * This class supports swizzeling. Swizzeling is done using member functions which
 * will return a `vec`. The name of the member function consists of 2 to 4 of the
 * following characters: 'x', 'y', 'z', 'w', 'r', 'g', 'b', 'a', '0' & '1'.
 * If the swizzle member function name would start with a '0' or '1' character it
 * will be prefixed with an underscore '_'.
 *
 * Since swizzle member functions always return a 4D vec, the third and forth
 * element will default to '0' and 'w'. This allows a 2D vector to maintain its
 * homogeniousness, or a color to maintain its alpha value.
 */ 
class vec {
    /* Intrinsic value of the vec.
     * Since the __m64 data type is not supported by MSVC on x64 it does
     * not yield a performance improvement to create a seperate 2D vector
     * class.
     *
     * The elements in __m128 are assigned as follows.
     *  - [127:96] w, alpha
     *  - [95:64] z, blue
     *  - [63:32] y, green
     *  - [31:0] x, red
     */
    __m128 v;

public:
    /* Create a zeroed out vec.
     */
    tt_force_inline vec() noexcept : vec(_mm_setzero_ps()) {}
    tt_force_inline vec(vec const &rhs) noexcept = default;
    tt_force_inline vec &operator=(vec const &rhs) noexcept = default;
    tt_force_inline vec(vec &&rhs) noexcept = default;
    tt_force_inline vec &operator=(vec &&rhs) noexcept = default;

    tt_force_inline vec(__m128 rhs) noexcept :
        v(rhs) {}

    tt_force_inline vec &operator=(__m128 rhs) noexcept {
        v = rhs;
        return *this;
    }

    tt_force_inline operator __m128 () const noexcept {
        return v;
    }

    explicit tt_force_inline vec(std::array<float,4> const &rhs) noexcept :
        v(_mm_loadu_ps(rhs.data())) {}

    tt_force_inline vec &operator=(std::array<float,4> const &rhs) noexcept {
        v = _mm_loadu_ps(rhs.data());
        return *this;
    }

    explicit tt_force_inline operator std::array<float,4> () const noexcept {
        std::array<float,4> r;
        _mm_storeu_ps(r.data(), v);
        return r;
    }

    explicit tt_force_inline vec(std::array<float16,4> const &rhs) noexcept :
        v(_mm_cvtph_ps(_mm_loadu_si64(rhs.data()))) {}

    tt_force_inline vec& operator=(std::array<float16,4> const &rhs) noexcept {
        v = _mm_cvtph_ps(_mm_loadu_si64(rhs.data()));
        return *this;
    }

    explicit tt_force_inline operator std::array<float16,4> () const noexcept {
        std::array<float16,4> r;
        _mm_storeu_si64(r.data(), _mm_cvtps_ph(v, _MM_FROUND_CUR_DIRECTION));
        return r;
    }

    /** Initialize a vec with all elements set to a value.
     * Useful as a scalar converter, when combined with an
     * arithmatic operator.
     */
    template<typename T, std::enable_if_t<std::is_arithmetic_v<T>,int> = 0>
    explicit tt_force_inline vec(T rhs) noexcept:
        vec(_mm_set_ps1(numeric_cast<float>(rhs))) {}

    /** Initialize a vec with all elements set to a value.
     * Useful as a scalar converter, when combined with an
     * arithmatic operator.
     */
    tt_force_inline vec &operator=(float rhs) noexcept {
        return *this = _mm_set_ps1(rhs);
    }

    /** Create a vec out of 2 to 4 values.
    * This vector is used as a homogenious coordinate, meaning:
    *  - vectors have w=0.0 (A direction and distance)
    *  - points have w=1.0 (A position in space)
    *
    * When this vector is used for color then:
    *  - x=Red, y=Green, z=Blue, w=Alpha
    *
    */
    template<typename T, typename U, typename V=float, typename W=float,
        std::enable_if_t<std::is_arithmetic_v<T> && std::is_arithmetic_v<U> && std::is_arithmetic_v<V> && std::is_arithmetic_v<W>,int> = 0>
    tt_force_inline vec(T x, U y, V z=0.0f, W w=0.0f) noexcept :
        vec(_mm_set_ps(
            numeric_cast<float>(w),
            numeric_cast<float>(z),
            numeric_cast<float>(y),
            numeric_cast<float>(x)
        )) {}

    template<typename T, std::enable_if_t<std::is_arithmetic_v<T>,int> = 0>
    tt_force_inline static vec make_x(T x) noexcept {
        return vec{_mm_set_ss(numeric_cast<float>(x))};
    }

    template<typename T, std::enable_if_t<std::is_arithmetic_v<T>,int> = 0>
    tt_force_inline static vec make_y(T y) noexcept {
        return vec{_mm_permute_ps(_mm_set_ss(numeric_cast<float>(y)), _MM_SHUFFLE(1,1,0,1))};
    }

    template<typename T, std::enable_if_t<std::is_arithmetic_v<T>,int> = 0>
    tt_force_inline static vec make_z(T z) noexcept {
        return vec{_mm_permute_ps(_mm_set_ss(numeric_cast<float>(z)), _MM_SHUFFLE(1,0,1,1))};
    }

    template<typename T, std::enable_if_t<std::is_arithmetic_v<T>,int> = 0>
    tt_force_inline static vec make_w(T w) noexcept {
        return vec{_mm_permute_ps(_mm_set_ss(numeric_cast<float>(w)), _MM_SHUFFLE(0,1,1,1))};
    }

    /** Create a point out of 2 to 4 values.
    * This vector is used as a homogeneous coordinate, meaning:
    *  - vectors have w=0.0 (A direction and distance)
    *  - points have w=1.0 (A position in space)
    *
    * When this vector is used for color then:
    *  - x=Red, y=Green, z=Blue, w=Alpha
    *
    */
    template<typename T=float, typename U=float, typename V=float, typename W=float,
    std::enable_if_t<std::is_arithmetic_v<T> && std::is_arithmetic_v<U> && std::is_arithmetic_v<V>,int> = 0>
    [[nodiscard]] tt_force_inline static vec point(T x=0.0f, U y=0.0f, V z=0.0f) noexcept {
        return vec{x, y, z, 1.0f};
    }

    /** Create a point out of 2 to 4 values.
    * This vector is used as a homogeneous coordinate, meaning:
    *  - vectors have w=0.0 (A direction and distance)
    *  - points have w=1.0 (A position in space)
    *
    * When this vector is used for color then:
    *  - x=Red, y=Green, z=Blue, w=Alpha
    *
    */
    [[nodiscard]] tt_force_inline static vec point(vec rhs) noexcept {
        return rhs.xyz1();
    }

    /** Get a origin vector(0.0, 0.0, 0.0, 1.0).
     * The origin of a window or image are in the left-bottom corner. The
     * center of the first pixel in the left-bottom corner is at coordinate
     * (0.5, 0.5). The origin of a glyph lies on the crossing of the
     * baseline and left-side-bearing. Paths have a specific location of the
     * origin.
     */
    [[nodiscard]] tt_force_inline static vec origin() noexcept {
        return vec{_mm_permute_ps(_mm_set_ss(1.0f), 0b00'01'10'11)};
    }

    /** Create a color out of 3 to 4 values.
     * If you use this vector as a color:
     *  - Red = x, Green = y, Blue = z, Alpha = w.
     *  - Alpha is linear: 0.0 is transparent, 1.0 is opaque.
     *    The Red/Green/Blue are not pre-multiplied with the alpha.
     *  - Red/Green/Blue are based on the linear-extended-sRGB floating point format:
     *    values between 0.0 and 1.0 is equal to linear-sRGB (no gamma curve).
     *    1.0,1.0,1.0 equals 80 cd/m2 and should be the maximum value for
     *    user interfaces. Values above 1.0 would cause brighter colors on
     *    HDR (high dynamic range) displays.
     *    Values below 0.0 will cause colours outside the sRGB color gamut
     *    for use with high-gamut displays
     */
    template<typename R, typename G, typename B, typename A=float,
        std::enable_if_t<std::is_floating_point_v<R> && std::is_floating_point_v<G> && std::is_floating_point_v<B> && std::is_floating_point_v<A>,int> = 0>
        [[nodiscard]] tt_force_inline static vec color(R r, G g, B b, A a=1.0f) noexcept {
        return vec{r, g, b, a};
    }

    [[nodiscard]] static vec colorFromSRGB(float r, float g, float b, float a=1.0f) noexcept;

    [[nodiscard]] static vec colorFromSRGB(uint8_t r, uint8_t g, uint8_t b, uint8_t a=255) noexcept;

    [[nodiscard]] static vec colorFromSRGB(std::string_view str);

    template<size_t I>
    tt_force_inline vec &set(float rhs) noexcept {
        static_assert(I <= 3);
        ttlet tmp = _mm_set_ss(rhs);
        return *this = _mm_insert_ps(*this, tmp, I << 4);
    }

    template<size_t I>
    tt_force_inline float get() const noexcept {
        static_assert(I <= 3);
        ttlet tmp = _mm_permute_ps(*this, I);
        return _mm_cvtss_f32(tmp);
    }

    tt_force_inline bool is_point() const noexcept {
        return w() == 1.0f;
    }

    tt_force_inline bool is_vector() const noexcept {
        return w() == 0.0f;
    }

    tt_force_inline bool is_opaque() const noexcept {
        return a() == 1.0f;
    }

    tt_force_inline bool is_transparent() const noexcept {
        return a() == 0.0f;
    }

    constexpr size_t size() const noexcept {
        return 4;
    }

    tt_force_inline float operator[](size_t i) const noexcept {
        tt_assume(i <= 3);
        ttlet i_ = _mm_set1_epi32(static_cast<uint32_t>(i));
        ttlet tmp = _mm_permutevar_ps(*this, i_);
        return _mm_cvtss_f32(tmp);
    }

    tt_force_inline vec &x(float rhs) noexcept { return set<0>(rhs); }
    tt_force_inline vec &y(float rhs) noexcept { return set<1>(rhs); }
    tt_force_inline vec &z(float rhs) noexcept { return set<2>(rhs); }
    tt_force_inline vec &w(float rhs) noexcept { return set<3>(rhs); }
    tt_force_inline vec &r(float rhs) noexcept { return set<0>(rhs); }
    tt_force_inline vec &g(float rhs) noexcept { return set<1>(rhs); }
    tt_force_inline vec &b(float rhs) noexcept { return set<2>(rhs); }
    tt_force_inline vec &a(float rhs) noexcept { return set<3>(rhs); }
    tt_force_inline vec &width(float rhs) noexcept { return set<0>(rhs); }
    tt_force_inline vec &height(float rhs) noexcept { return set<1>(rhs); }
    tt_force_inline vec &depth(float rhs) noexcept { return set<2>(rhs); }
    tt_force_inline float x() const noexcept { return get<0>(); }
    tt_force_inline float y() const noexcept { return get<1>(); }
    tt_force_inline float z() const noexcept { return get<2>(); }
    tt_force_inline float w() const noexcept { return get<3>(); }
    tt_force_inline float r() const noexcept { return get<0>(); }
    tt_force_inline float g() const noexcept { return get<1>(); }
    tt_force_inline float b() const noexcept { return get<2>(); }
    tt_force_inline float a() const noexcept { return get<3>(); }
    tt_force_inline float width() const noexcept { return get<0>(); }
    tt_force_inline float height() const noexcept { return get<1>(); }
    tt_force_inline float depth() const noexcept { return get<2>(); }


    tt_force_inline vec &operator+=(vec const &rhs) noexcept {
        return *this = _mm_add_ps(*this, rhs);
    }

    tt_force_inline vec &operator-=(vec const &rhs) noexcept {
        return *this = _mm_sub_ps(*this, rhs);
    }

    tt_force_inline vec &operator*=(vec const &rhs) noexcept {
        return *this = _mm_mul_ps(*this, rhs);
    }

    tt_force_inline vec &operator/=(vec const &rhs) noexcept {
        return *this = _mm_div_ps(*this, rhs);
    }

    [[nodiscard]] tt_force_inline friend vec operator-(vec const &rhs) noexcept {
        return _mm_sub_ps(_mm_setzero_ps(), rhs);
    }

    [[nodiscard]] tt_force_inline friend vec operator+(vec const &lhs, vec const &rhs) noexcept {
        return _mm_add_ps(lhs, rhs);
    }

    [[nodiscard]] tt_force_inline friend vec operator-(vec const &lhs, vec const &rhs) noexcept {
        return _mm_sub_ps(lhs, rhs);
    }

    [[nodiscard]] tt_force_inline friend vec operator*(vec const &lhs, vec const &rhs) noexcept {
        return _mm_mul_ps(lhs, rhs);
    }

    template<typename T, std::enable_if_t<std::is_arithmetic_v<T>, int> = 0>
    [[nodiscard]] tt_force_inline friend vec operator*(vec const &lhs, T const &rhs) noexcept {
        return lhs * vec{rhs};
    }

    template<typename T, std::enable_if_t<std::is_arithmetic_v<T>, int> = 0>
    [[nodiscard]] tt_force_inline friend vec operator*(T const &lhs, vec const &rhs) noexcept {
        return vec{lhs} * rhs;
    }

    [[nodiscard]] tt_force_inline friend vec operator/(vec const &lhs, vec const &rhs) noexcept {
        return _mm_div_ps(lhs, rhs);
    }

    [[nodiscard]] tt_force_inline friend vec max(vec const &lhs, vec const &rhs) noexcept {
        return _mm_max_ps(lhs, rhs);
    }

    [[nodiscard]] tt_force_inline friend vec min(vec const &lhs, vec const &rhs) noexcept {
        return _mm_min_ps(lhs, rhs);
    }

    [[nodiscard]] tt_force_inline friend vec abs(vec const &rhs) noexcept {
        return max(rhs, -rhs);
    }

    [[nodiscard]] tt_force_inline friend bool operator==(vec const &lhs, vec const &rhs) noexcept {
        ttlet tmp2 = _mm_movemask_ps(_mm_cmpeq_ps(lhs, rhs));
        return tmp2 == 0b1111;
    }

    [[nodiscard]] tt_force_inline friend bool operator!=(vec const &lhs, vec const &rhs) noexcept {
        return !(lhs == rhs);
    }

    /** Equal to.
    * @return boolean bit field, bit 0=x, 1=y, 2=z, 3=w.
    */
    [[nodiscard]] tt_force_inline friend int eq(vec const &lhs, vec const &rhs) noexcept {
        return _mm_movemask_ps(_mm_cmpeq_ps(lhs, rhs));
    }

    /** Not equal to.
    * @return boolean bit field, bit 0=x, 1=y, 2=z, 3=w.
    */
    [[nodiscard]] tt_force_inline friend int ne(vec const &lhs, vec const &rhs) noexcept {
        return _mm_movemask_ps(_mm_cmpneq_ps(lhs, rhs));
    }

    /** Less than.
     * @return boolean bit field, bit 0=x, 1=y, 2=z, 3=w.
     */
    [[nodiscard]] tt_force_inline friend int operator<(vec const &lhs, vec const &rhs) noexcept {
        return _mm_movemask_ps(_mm_cmplt_ps(lhs, rhs));
    }

    /** Less than or equal.
    * @return boolean bit field, bit 0=x, 1=y, 2=z, 3=w.
    */
    [[nodiscard]] tt_force_inline friend int operator<=(vec const &lhs, vec const &rhs) noexcept {
        return _mm_movemask_ps(_mm_cmple_ps(lhs, rhs));
    }

    /** Greater than.
    * @return boolean bit field, bit 0=x, 1=y, 2=z, 3=w.
    */
    [[nodiscard]] tt_force_inline friend int operator>(vec const &lhs, vec const &rhs) noexcept {
        return _mm_movemask_ps(_mm_cmpgt_ps(lhs, rhs));
    }

    /** Greater than or equal.
    * @return boolean bit field, bit 0=x, 1=y, 2=z, 3=w.
    */
    [[nodiscard]] tt_force_inline friend int operator>=(vec const &lhs, vec const &rhs) noexcept {
        return _mm_movemask_ps(_mm_cmpge_ps(lhs, rhs));
    }

    [[nodiscard]] tt_force_inline friend __m128 _length_squared(vec const &rhs) noexcept {
        ttlet tmp1 = _mm_mul_ps(rhs, rhs);
        ttlet tmp2 = _mm_hadd_ps(tmp1, tmp1);
        return _mm_hadd_ps(tmp2, tmp2);
    }

    [[nodiscard]] tt_force_inline friend float length_squared(vec const &rhs) noexcept {
        return _mm_cvtss_f32(_length_squared(rhs));
    }

    [[nodiscard]] tt_force_inline friend float length(vec const &rhs) noexcept {
        ttlet tmp = _mm_sqrt_ps(_length_squared(rhs));
        return _mm_cvtss_f32(tmp);
    }

    [[nodiscard]] tt_force_inline friend vec normalize(vec const &rhs) noexcept {
        auto ___l = _length_squared(rhs);
        auto llll = _mm_permute_ps(___l, _MM_SHUFFLE(0,0,0,0));
        auto iiii = _mm_rsqrt_ps(llll);
        return _mm_mul_ps(rhs, iiii);
    }

    /** Resize an extent while retaining aspect ratio.
    * @param rhs The extent to match.
    * @return The extent resized to match rhs, while retaining aspect ratio.
    */
    [[nodiscard]] vec resize2DRetainingAspectRatio(vec const &rhs) noexcept {
        ttlet ratio2D = rhs / *this;
        ttlet ratio = std::min(ratio2D.x(), ratio2D.y());
        return *this * ratio;
    }

    [[nodiscard]] tt_force_inline friend vec homogeneous_divide(vec const &rhs) noexcept {
        auto wwww = _mm_permute_ps(rhs, _MM_SHUFFLE(3,3,3,3));
        auto rcp_wwww = _mm_rcp_ps(wwww);
        return _mm_mul_ps(rhs, rcp_wwww);
    }

    [[nodiscard]] tt_force_inline friend float dot(vec const &lhs, vec const &rhs) noexcept {
        ttlet tmp1 = _mm_mul_ps(lhs, rhs);
        ttlet tmp2 = _mm_hadd_ps(tmp1, tmp1);
        ttlet tmp3 = _mm_hadd_ps(tmp2, tmp2);
        return _mm_cvtss_f32(tmp3);
    }

    [[nodiscard]] tt_force_inline friend vec reciprocal(vec const &rhs) noexcept {
        return _mm_rcp_ps(rhs);
    }

    template<bool nx, bool ny, bool nz, bool nw>
    friend vec neg(vec const &rhs) noexcept;

    [[nodiscard]] tt_force_inline friend vec hadd(vec const &lhs, vec const &rhs) noexcept {
        return _mm_hadd_ps(lhs, rhs);
    }

    [[nodiscard]] tt_force_inline friend vec hsub(vec const &lhs, vec const &rhs) noexcept {
        return _mm_hsub_ps(lhs, rhs);
    }

    [[nodiscard]] tt_force_inline friend float viktor_cross(vec const &lhs, vec const &rhs) noexcept {
        // a.x * b.y - a.y * b.x
        ttlet tmp1 = _mm_permute_ps(rhs, _MM_SHUFFLE(2,3,0,1));
        ttlet tmp2 = _mm_mul_ps(lhs, tmp1);
        ttlet tmp3 = _mm_hsub_ps(tmp2, tmp2);
        return _mm_cvtss_f32(tmp3);
    }

    // x=a.y*b.z - a.z*b.y
    // y=a.z*b.x - a.x*b.z
    // z=a.x*b.y - a.y*b.x
    // w=a.w*b.w - a.w*b.w
    [[nodiscard]] friend vec cross(vec const &lhs, vec const &rhs) noexcept {
        ttlet a_left = _mm_permute_ps(lhs, _MM_SHUFFLE(3,0,2,1));
        ttlet b_left = _mm_permute_ps(rhs, _MM_SHUFFLE(3,1,0,2));
        ttlet left = _mm_mul_ps(a_left, b_left);

        ttlet a_right = _mm_permute_ps(lhs, _MM_SHUFFLE(3,1,0,2));
        ttlet b_right = _mm_permute_ps(rhs, _MM_SHUFFLE(3,0,2,1));
        ttlet right = _mm_mul_ps(a_right, b_right);
        return _mm_sub_ps(left, right);
    }

    /** Calculate the 2D normal on a 2D vector.
     */
    [[nodiscard]] tt_force_inline friend vec normal(vec const &rhs) noexcept {
        tt_assume(rhs.z() == 0.0f && rhs.w() == 0.0f);
        return normalize(vec{-rhs.y(), rhs.x()});
    }

    [[nodiscard]] friend vec ceil(vec const &rhs) noexcept {
        return _mm_ceil_ps(rhs);
    }

    [[nodiscard]] friend vec floor(vec const &rhs) noexcept {
        return _mm_floor_ps(rhs);
    }

    [[nodiscard]] friend vec round(vec const &rhs) noexcept {
        return _mm_round_ps(rhs, _MM_FROUND_CUR_DIRECTION);
    }

    [[nodiscard]] friend std::array<vec,4> transpose(vec col0, vec col1, vec col2, vec col3) noexcept {
        _MM_TRANSPOSE4_PS(col0, col1, col2, col3);
        return { col0, col1, col2, col3 };
    }

    /** Find a point at the midpoint between two points.
     */
    [[nodiscard]] friend vec midpoint(vec const &p1, vec const &p2) noexcept {
        return (p1 + p2) * vec{0.5};
    }

    [[nodiscard]] friend vec desaturate(vec const &color, float brightness) noexcept {
        // Use luminance ratios and change the brightness.
        // luminance ratios according to BT.709.
        ttlet _0BGR = color * vec{0.2126, 0.7152, 0.0722} * vec{brightness};
        ttlet __SS = _mm_hadd_ps(_0BGR, _0BGR);
        ttlet ___L = _mm_hadd_ps(__SS, __SS);
        ttlet LLLL = _mm_permute_ps(___L, _MM_SHUFFLE(0,0,0,0));
        // grayscale, with original alpha. 
        return _mm_blend_ps(LLLL, color, 0b1000);
    }

    [[nodiscard]] friend vec composit(vec const &under, vec const &over) noexcept {
        if (over.is_transparent()) {
            return under;
        }
        if (over.is_opaque()) {
            return over;
        }

        ttlet over_alpha = over.wwww();
        ttlet under_alpha = under.wwww();

        ttlet over_color = over.xyz1();
        ttlet under_color = under.xyz1();

        ttlet output_color =
            over_color * over_alpha +
            under_color * under_alpha * (vec{1.0} - over_alpha);

        return output_color / output_color.www1();
    }

    /** Find the point on the other side and at the same distance of an anchor-point.
     */
    [[nodiscard]] friend vec reflect_point(vec const &p, vec const anchor) noexcept {
        return anchor - (p - anchor);
    }

    [[nodiscard]] friend std::string to_string(vec const &rhs) noexcept {
        return fmt::format("({}, {}, {}, {})", rhs.x(), rhs.y(), rhs.z(), rhs.w());
    }

    std::ostream friend &operator<<(std::ostream &lhs, vec const &rhs) noexcept {
        return lhs << to_string(rhs);
    }

    template<std::size_t I>
    [[nodiscard]] tt_force_inline friend float get(vec const &rhs) noexcept {
        return rhs.get<I>();
    }

    template<char a, char b, char c, char d>
    [[nodiscard]] constexpr static int swizzle_permute_mask() noexcept {
        int r = 0;
        switch (a) {
        case 'x': r |= 0b00'00'00'00; break;
        case 'y': r |= 0b00'00'00'01; break;
        case 'z': r |= 0b00'00'00'10; break;
        case 'w': r |= 0b00'00'00'11; break;
        case '0': r |= 0b00'00'00'00; break;
        case '1': r |= 0b00'00'00'00; break;
        }
        switch (b) {
        case 'x': r |= 0b00'00'00'00; break;
        case 'y': r |= 0b00'00'01'00; break;
        case 'z': r |= 0b00'00'10'00; break;
        case 'w': r |= 0b00'00'11'00; break;
        case '0': r |= 0b00'00'01'00; break;
        case '1': r |= 0b00'00'01'00; break;
        }
        switch (c) {
        case 'x': r |= 0b00'00'00'00; break;
        case 'y': r |= 0b00'01'00'00; break;
        case 'z': r |= 0b00'10'00'00; break;
        case 'w': r |= 0b00'11'00'00; break;
        case '0': r |= 0b00'10'00'00; break;
        case '1': r |= 0b00'10'00'00; break;
        }
        switch (d) {
        case 'x': r |= 0b00'00'00'00; break;
        case 'y': r |= 0b01'00'00'00; break;
        case 'z': r |= 0b10'00'00'00; break;
        case 'w': r |= 0b11'00'00'00; break;
        case '0': r |= 0b11'00'00'00; break;
        case '1': r |= 0b11'00'00'00; break;
        }
        return r;
    }

    template<char a, char b, char c, char d>
    [[nodiscard]] constexpr static int swizzle_zero_mask() noexcept {
        int r = 0;
        r |= (a == '1') ? 0 : 0b0001;
        r |= (b == '1') ? 0 : 0b0010;
        r |= (c == '1') ? 0 : 0b0100;
        r |= (d == '1') ? 0 : 0b1000;
        return r;
    }

    template<char a, char b, char c, char d>
    [[nodiscard]] constexpr static int swizzle_number_mask() noexcept {
        int r = 0;
        r |= (a == '0' || a == '1') ? 0b0001 : 0;
        r |= (b == '0' || b == '1') ? 0b0010 : 0;
        r |= (c == '0' || c == '1') ? 0b0100 : 0;
        r |= (d == '0' || d == '1') ? 0b1000 : 0;
        return r;
    }

    template<char a, char b, char c, char d>
    [[nodiscard]] tt_force_inline vec swizzle() const noexcept {
        constexpr int permute_mask = vec::swizzle_permute_mask<a,b,c,d>();
        constexpr int zero_mask = vec::swizzle_zero_mask<a,b,c,d>();
        constexpr int number_mask = vec::swizzle_number_mask<a,b,c,d>();

        __m128 swizzled;
        // Clang is able to optimize these intrinsics, MSVC is not.
        if constexpr (permute_mask != 0b11'10'01'00) {
            swizzled = _mm_permute_ps(*this, permute_mask);
        } else {
            swizzled = *this;
        }

        __m128 numbers;
        if constexpr (zero_mask == 0b0000) {
            numbers = _mm_set_ps1(1.0f);
        } else if constexpr (zero_mask == 0b1111) {
            numbers = _mm_setzero_ps();
        } else if constexpr (zero_mask == 0b1110) {
            numbers = _mm_set_ss(1.0f);
        } else {
            ttlet _1111 = _mm_set_ps1(1.0f);
            numbers = _mm_insert_ps(_1111, _1111, zero_mask);
        }

        __m128 result;
        if constexpr (number_mask == 0b0000) {
            result = swizzled;
        } else if constexpr (number_mask == 0b1111) {
            result = numbers;
        } else if constexpr (((zero_mask | ~number_mask) & 0b1111) == 0b1111) {
            result = _mm_insert_ps(swizzled, swizzled, number_mask);
        } else {
            result = _mm_blend_ps(swizzled, numbers, number_mask);
        }
        return result;
    }

#define SWIZZLE4(name, A, B, C, D)\
    [[nodiscard]] vec name() const noexcept {\
        return swizzle<A, B, C, D>();\
    }

#define SWIZZLE4_GEN3(name, A, B, C)\
    SWIZZLE4(name ## 0, A, B, C, '0')\
    SWIZZLE4(name ## 1, A, B, C, '1')\
    SWIZZLE4(name ## x, A, B, C, 'x')\
    SWIZZLE4(name ## y, A, B, C, 'y')\
    SWIZZLE4(name ## z, A, B, C, 'z')\
    SWIZZLE4(name ## w, A, B, C, 'w')

#define SWIZZLE4_GEN2(name, A, B)\
    SWIZZLE4_GEN3(name ## 0, A, B, '0')\
    SWIZZLE4_GEN3(name ## 1, A, B, '1')\
    SWIZZLE4_GEN3(name ## x, A, B, 'x')\
    SWIZZLE4_GEN3(name ## y, A, B, 'y')\
    SWIZZLE4_GEN3(name ## z, A, B, 'z')\
    SWIZZLE4_GEN3(name ## w, A, B, 'w')

#define SWIZZLE4_GEN1(name, A)\
    SWIZZLE4_GEN2(name ## 0, A, '0')\
    SWIZZLE4_GEN2(name ## 1, A, '1')\
    SWIZZLE4_GEN2(name ## x, A, 'x')\
    SWIZZLE4_GEN2(name ## y, A, 'y')\
    SWIZZLE4_GEN2(name ## z, A, 'z')\
    SWIZZLE4_GEN2(name ## w, A, 'w')

    SWIZZLE4_GEN1(_0, '0')
    SWIZZLE4_GEN1(_1, '1')
    SWIZZLE4_GEN1(x, 'x')
    SWIZZLE4_GEN1(y, 'y')
    SWIZZLE4_GEN1(z, 'z')
    SWIZZLE4_GEN1(w, 'w')

#define SWIZZLE3(name, A, B, C)\
    [[nodiscard]] vec name() const noexcept {\
        return swizzle<A,B,C,'w'>();\
    }

#define SWIZZLE3_GEN2(name, A, B)\
    SWIZZLE3(name ## 0, A, B, '0')\
    SWIZZLE3(name ## 1, A, B, '1')\
    SWIZZLE3(name ## x, A, B, 'x')\
    SWIZZLE3(name ## y, A, B, 'y')\
    SWIZZLE3(name ## z, A, B, 'z')\
    SWIZZLE3(name ## w, A, B, 'w')

#define SWIZZLE3_GEN1(name, A)\
    SWIZZLE3_GEN2(name ## 0, A, '0')\
    SWIZZLE3_GEN2(name ## 1, A, '1')\
    SWIZZLE3_GEN2(name ## x, A, 'x')\
    SWIZZLE3_GEN2(name ## y, A, 'y')\
    SWIZZLE3_GEN2(name ## z, A, 'z')\
    SWIZZLE3_GEN2(name ## w, A, 'w')

    SWIZZLE3_GEN1(_0, '0')
    SWIZZLE3_GEN1(_1, '1')
    SWIZZLE3_GEN1(x, 'x')
    SWIZZLE3_GEN1(y, 'y')
    SWIZZLE3_GEN1(z, 'z')
    SWIZZLE3_GEN1(w, 'w')

#define SWIZZLE2(name, A, B)\
    [[nodiscard]] vec name() const noexcept {\
        return swizzle<A,B,'0','w'>();\
    }

#define SWIZZLE2_GEN1(name, A)\
    SWIZZLE2(name ## 0, A, '0')\
    SWIZZLE2(name ## 1, A, '1')\
    SWIZZLE2(name ## x, A, 'x')\
    SWIZZLE2(name ## y, A, 'y')\
    SWIZZLE2(name ## z, A, 'z')\
    SWIZZLE2(name ## w, A, 'w')

    SWIZZLE2_GEN1(_0, '0')
    SWIZZLE2_GEN1(_1, '1')
    SWIZZLE2_GEN1(x, 'x')
    SWIZZLE2_GEN1(y, 'y')
    SWIZZLE2_GEN1(z, 'z')
    SWIZZLE2_GEN1(w, 'w')
};

template<bool nx, bool ny, bool nz, bool nw>
[[nodiscard]] vec neg(vec const &rhs) noexcept {
    ttlet n_rhs = -rhs;

    __m128 tmp = rhs;

    if constexpr (nx) {
        tmp = _mm_insert_ps(tmp, n_rhs, 0b00'00'0000);
    }
    if constexpr (ny) {
        tmp = _mm_insert_ps(tmp, n_rhs, 0b01'01'0000);
    }
    if constexpr (nz) {
        tmp = _mm_insert_ps(tmp, n_rhs, 0b10'10'0000);
    }
    if constexpr (nw) {
        tmp = _mm_insert_ps(tmp, n_rhs, 0b11'11'0000);
    }

    return tmp;
}

}

#undef SWIZZLE4
#undef SWIZZLE4_GEN1
#undef SWIZZLE4_GEN2
#undef SWIZZLE4_GEN3
#undef SWIZZLE3
#undef SWIZZLE3_GEN1
#undef SWIZZLE3_GEN2
#undef SWIZZLE2
#undef SWIZZLE2_GEN1