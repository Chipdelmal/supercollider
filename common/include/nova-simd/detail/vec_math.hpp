//  genertic vectorized math functions
//  Copyright (C) 2010 Tim Blechmann
//
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation; either version 2 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; see the file COPYING.  If not, write to
//  the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
//  Boston, MA 02111-1307, USA.

#ifndef NOVA_SIMD_DETAIL_VECMATH_HPP
#define NOVA_SIMD_DETAIL_VECMATH_HPP

#include <cmath>

#if defined(__GNUC__) && defined(NDEBUG)
#define always_inline inline  __attribute__((always_inline))
#else
#define always_inline inline
#endif
#include <limits>

/* the approximation of mathematical functions should have a maximum relative error of below 5e-7f
 */

namespace nova
{
namespace detail
{

template <typename VecType>
always_inline VecType vec_sign(VecType const & arg)
{
    typedef VecType vec;
    const vec zero       = vec::gen_zero();
    const vec one        = vec::gen_one();
    const vec sign_mask  = vec::gen_sign_mask();

    const vec nonzero    = mask_neq(arg, zero);
    const vec sign       = arg & sign_mask;

    const vec abs_ret    = nonzero & one;
    return sign | abs_ret;
}

template <typename VecType>
always_inline VecType vec_round_float(VecType const & arg)
{
    typedef VecType vec;

    const vec sign    = arg & vec::gen_sign_mask();
    const vec abs_arg = sign ^ arg;
    const vec two_to_23_ps (0x1.0p23f);
    const vec rounded = (abs_arg + two_to_23_ps) - two_to_23_ps;

    return sign ^ rounded;
}

template <typename VecType>
always_inline VecType vec_floor_float(VecType const & arg)
{
    typedef VecType vec;

    const vec rounded = vec_round_float(arg);

    const vec rounded_larger = mask_gt(rounded, arg);
    const vec add            = rounded_larger & vec::gen_one();
    return rounded - add;
}

template <typename VecType>
always_inline VecType vec_ceil_float(VecType const & arg)
{
    typedef VecType vec;

    const vec rounded = vec_round_float(arg);

    const vec rounded_smaller = mask_lt(rounded, arg);
    const vec add             = rounded_smaller & vec::gen_one();
    return rounded + add;
}

template <typename VecFloat>
always_inline VecFloat ldexp_float(VecFloat const & x, typename VecFloat::int_vec const & n)
{
    typedef typename VecFloat::int_vec int_vec;

    const VecFloat exponent_mask = VecFloat::gen_exp_mask();
    const VecFloat exponent = exponent_mask & x;
    const VecFloat x_wo_x = andnot(exponent_mask, x);     // clear exponent

    int_vec new_exp = slli(n, 16+7) + int_vec(exponent);  // new exponent
    VecFloat new_exp_float(new_exp);
    VecFloat ret = x_wo_x | new_exp_float;
    return ret;
}

template <typename VecFloat>
always_inline VecFloat frexp_float(VecFloat const & x, typename VecFloat::int_vec & exp)
{
    typedef typename VecFloat::int_vec int_vec;

    const VecFloat exponent_mask = VecFloat::gen_exp_mask();
    const VecFloat exponent = exponent_mask & x;
    const VecFloat x_wo_x = andnot(exponent_mask, x);             // clear exponent

    const int_vec exp_int(exponent);

    exp = srli(exp_int, 16+7) - int_vec(126);
    return x_wo_x | VecFloat::gen_exp_mask_1();
}

/* adapted from cephes, approximation polynomial generated by sollya */
template <typename VecType>
always_inline VecType vec_exp_float(VecType const & arg)
{
    typedef typename VecType::int_vec int_vec;

    /* Express e**x = e**g 2**n
     *   = e**g e**( n loge(2) )
     *   = e**( g + n loge(2) )
     */

    // black magic
    VecType x = arg;
    VecType z = round(VecType(1.44269504088896341f) * x);
    int_vec n = z.truncate_to_int();
    x -= z*VecType(0.693359375f);
    x -= z*VecType(-2.12194440e-4f);

    /* Theoretical peak relative error in [-0.5, +0.5] is 3.5e-8. */
    VecType p = VecType(VecType::gen_one()) +
        x * (1.00000035762786865234375f +
        x * (0.4999996721744537353515625f +
        x * (0.16665561497211456298828125f +
        x * (4.167006909847259521484375e-2f +
        x * (8.420792408287525177001953125e-3f +
        x * 1.386119984090328216552734375e-3f)))));

    /* multiply by power of 2 */
    VecType approx = ldexp_float(p, n);

    /* handle min/max boundaries */
    const VecType maxlogf(88.72283905206835f);
    const VecType minlogf(-103.278929903431851103f);
    const VecType max_float(std::numeric_limits<float>::max());
    const VecType zero = VecType::gen_zero();

    VecType too_large = mask_gt(arg, maxlogf);
    VecType too_small = mask_lt(arg, minlogf);

    VecType ret = select(approx, max_float, too_large);
    ret = select(ret, zero, too_small);

    return ret;
}

/* adapted from cephes */
template <typename VecType>
always_inline VecType vec_log_float(VecType x)
{
    typedef typename VecType::int_vec int_vec;

    int_vec e;
    x = frexp_float( x, e );

    const VecType sqrt_05 = 0.707106781186547524f;
    const VecType x_smaller_sqrt_05 = mask_lt(x, sqrt_05);
    e = e + int_vec(x_smaller_sqrt_05);
    VecType x_add = x;
    x_add = x_add & x_smaller_sqrt_05;
    x += x_add - VecType(VecType::gen_one());

    VecType y =
    (((((((( 7.0376836292E-2 * x
    - 1.1514610310E-1) * x
    + 1.1676998740E-1) * x
    - 1.2420140846E-1) * x
    + 1.4249322787E-1) * x
    - 1.6668057665E-1) * x
    + 2.0000714765E-1) * x
    - 2.4999993993E-1) * x
    + 3.3333331174E-1) * x * x*x;

    VecType fe = e.convert_to_float();
    y += fe * -2.12194440e-4;

    y -= 0.5 * x*x;            /* y - 0.5 x^2 */
    VecType z  = x + y;        /* ... + x  */

    return z + 0.693359375 * fe;
}


/* exp function for vec_tanh_float. similar to vec_exp_tanh, but without boundary checks */
template <typename VecType>
always_inline VecType vec_exp_tanh_float(VecType const & arg)
{
    typedef typename VecType::int_vec int_vec;

    /* Express e**x = e**g 2**n
     *   = e**g e**( n loge(2) )
     *   = e**( g + n loge(2) )
     */

    // black magic
    VecType x = arg;
    VecType z = round(VecType(1.44269504088896341f) * x);
    int_vec n = z.truncate_to_int();
    x -= z*VecType(0.693359375f);
    x -= z*VecType(-2.12194440e-4f);

    /* Theoretical peak relative error in [-0.5, +0.5] is 3.5e-8. */
    VecType p = 1.f +
        x * (1.00000035762786865234375f +
        x * (0.4999996721744537353515625f +
        x * (0.16665561497211456298828125f +
        x * (4.167006909847259521484375e-2f +
        x * (8.420792408287525177001953125e-3f +
        x * 1.386119984090328216552734375e-3f)))));

    /* multiply by power of 2 */
    VecType approx = ldexp_float(p, n);

    return approx;
}


/* adapted from Julien Pommier's sse_mathfun.h, itself based on cephes */
template <typename VecType>
always_inline VecType vec_sin_float(VecType const & arg)
{
    typedef typename VecType::int_vec int_vec;

    const typename VecType::float_type four_over_pi = 1.27323954473516268615107010698011489627567716592367;

    VecType sign = arg & VecType::gen_sign_mask();
    VecType abs_arg = arg & VecType::gen_abs_mask();

    VecType y = abs_arg * four_over_pi;

    int_vec j = y.truncate_to_int();

    /* cephes: j=(j+1) & (~1) */
    j = (j + int_vec(1)) & int_vec(~1);
    y = j.convert_to_float();

    /* sign based on quadrant */
    VecType swap_sign_bit = slli(j & int_vec(4), 29);
    sign = sign ^ swap_sign_bit;

    /* polynomial mask */
    VecType poly_mask = VecType (mask_eq(j & int_vec(2), int_vec(0)));

    /* black magic */
    static float DP1 = 0.78515625;
    static float DP2 = 2.4187564849853515625e-4;
    static float DP3 = 3.77489497744594108e-8;
    VecType base = ((abs_arg - y * DP1) - y * DP2) - y * DP3;

    /* [0..pi/4] */
    VecType z = base * base;
    VecType p1 = ((  2.443315711809948E-005 * z
        - 1.388731625493765E-003) * z
        + 4.166664568298827E-002) * z * z
        -0.5f * z + 1
        ;

    /* [pi/4..pi/2] */
    VecType p2 = ((-1.9515295891E-4 * z
         + 8.3321608736E-3) * z
         - 1.6666654611E-1) * z * base + base;

    VecType approximation =  select(p1, p2, poly_mask);

    return approximation ^ sign;
}

/* adapted from Julien Pommier's sse_mathfun.h, itself based on cephes */
template <typename VecType>
always_inline VecType vec_cos_float(VecType const & arg)
{
    typedef typename VecType::int_vec int_vec;

    const typename VecType::float_type four_over_pi = 1.27323954473516268615107010698011489627567716592367;

    VecType abs_arg = arg & VecType::gen_abs_mask();

    VecType y = abs_arg * four_over_pi;

    int_vec j = y.truncate_to_int();

    /* cephes: j=(j+1) & (~1) */
    j = (j + int_vec(1)) & int_vec(~1);
    y = j.convert_to_float();

    /* sign based on quadrant */
    int_vec jm2 = j - int_vec(2);
    VecType sign = slli(andnot(jm2, int_vec(4)), 29);

    /* polynomial mask */
    VecType poly_mask = VecType (mask_eq(jm2 & int_vec(2), int_vec(0)));

    /* black magic */
    static float DP1 = 0.78515625;
    static float DP2 = 2.4187564849853515625e-4;
    static float DP3 = 3.77489497744594108e-8;
    VecType base = ((abs_arg - y * DP1) - y * DP2) - y * DP3;

    /* [0..pi/4] */
    VecType z = base * base;
    VecType p1 = ((  2.443315711809948E-005 * z
        - 1.388731625493765E-003) * z
        + 4.166664568298827E-002) * z * z
        -0.5f * z + 1
        ;

    /* [pi/4..pi/2] */
    VecType p2 = ((-1.9515295891E-4 * z
         + 8.3321608736E-3) * z
         - 1.6666654611E-1) * z * base + base;

    VecType approximation =  select(p1, p2, poly_mask);

    return approximation ^ sign;
}

/* adapted from cephes, approximation polynomial generted by sollya */
template <typename VecType>
always_inline VecType vec_tan_float(VecType const & arg)
{
    typedef typename VecType::int_vec int_vec;
    const typename VecType::float_type four_over_pi = 1.27323954473516268615107010698011489627567716592367;

    VecType sign = arg & VecType::gen_sign_mask();
    VecType abs_arg = arg & VecType::gen_abs_mask();

    VecType y = abs_arg * four_over_pi;
    int_vec j = y.truncate_to_int();

    /* cephes: j=(j+1) & (~1) */
    j = (j + int_vec(1)) & int_vec(~1);
    y = j.convert_to_float();

    /* approximation mask */
    VecType poly_mask = VecType (mask_eq(j & int_vec(2), int_vec(0)));

    /* black magic */
    static float DP1 = 0.78515625;
    static float DP2 = 2.4187564849853515625e-4;
    static float DP3 = 3.77489497744594108e-8;
    VecType base = ((abs_arg - y * DP1) - y * DP2) - y * DP3;

    VecType x = base; VecType x2 = x*x;

    // sollya: fpminimax(tan(x), [|3,5,7,9,11,13|], [|24...|], [-pi/4,pi/4], x);
    VecType approx =
        x + x * x2 * (0.3333315551280975341796875 + x2 * (0.1333882510662078857421875 + x2 * (5.3409568965435028076171875e-2 + x2 * (2.443529665470123291015625e-2 + x2 * (3.1127030961215496063232421875e-3 + x2 * 9.3892104923725128173828125e-3)))));

    //VecType recip = -reciprocal(approx);
    VecType recip = -1.0 / approx;

    VecType approximation = select(recip, approx, poly_mask);

    return approximation ^ sign;
}

/* adapted from cephes, approximation polynomial generted by sollya */
template <typename VecType>
always_inline VecType vec_asin_float(VecType const & arg)
{
    VecType abs_arg = arg & VecType::gen_abs_mask();
    VecType sign = arg & VecType::gen_sign_mask();
    VecType one = VecType::gen_one();
    VecType half = VecType::gen_05();
    VecType zero = VecType::gen_zero();

    // range redution: asin(x) = pi/2 - 2 asin( sqrt( (1-x)/2 ) ). for |arg| > 0.5
    VecType arg_greater_05 = mask_gt(abs_arg, 0.5);
    VecType arg_reduced_sqr = (one - abs_arg) * half;
    VecType arg_reduced = sqrt((one - abs_arg) * half);
    VecType approx_arg = select(abs_arg, arg_reduced, arg_greater_05);


    VecType z = select(abs_arg*abs_arg, arg_reduced_sqr, arg_greater_05);

    VecType x = approx_arg; VecType x2 = x*x;
    // sollya: fpminimax(asin(x), [|3,5,7,9,11|], [|24...|], [0.000000000000000000001,0.5], x);
    VecType approx_poly = x + x * x2 * (0.166667520999908447265625 + x2 * (7.4953101575374603271484375e-2 + x2 * (4.54690195620059967041015625e-2 + x2 * (2.418550290167331695556640625e-2 + x2 * 4.21570129692554473876953125e-2))));

    VecType approx_poly_reduced = 1.57079637050628662109375 - approx_poly - approx_poly;
    VecType approx = select(approx_poly, approx_poly_reduced, arg_greater_05);

    approx = approx ^ sign;
    // |arg| > 1: return 0
    VecType ret = select(approx, zero, mask_gt(abs_arg, one));
    return ret;
}

/* based on asin approximation:
 *
 * x < -0.5:        acos(x) = pi - 2.0 * asin( sqrt((1+x)/2) );
 * -0.5 < x < 0.5   acos(x) = pi/2 - asin(x)
 * x > 0.5          acos(x) =      2.0 * asin( sqrt((1-x)/2) ).
 *
 */
template <typename VecType>
always_inline VecType vec_acos_float(VecType const & arg)
{
    VecType abs_arg = arg & VecType::gen_abs_mask();
    VecType one = VecType::gen_one();
    VecType half = VecType::gen_05();
    VecType zero = VecType::gen_zero();

    VecType arg_greater_05 = mask_gt(abs_arg, half);
    VecType asin_arg_greater_05 = sqrt((one - abs_arg) * half);

    VecType asin_arg = select(arg, asin_arg_greater_05, arg_greater_05);

    VecType asin = vec_asin_float(asin_arg);
    VecType two_asin = asin + asin;

    VecType ret_m1_m05 = 3.1415927410125732421875 - two_asin;
    VecType ret_m05_05 = 1.57079637050628662109375 - asin;
    VecType ret_05_1 = two_asin;

    VecType ret_m05_1 = select(ret_m05_05, ret_05_1, mask_gt(arg, half));
    VecType ret = select(ret_m1_m05, ret_m05_1, mask_gt(arg, -0.5));

    // |arg| > 1: return 0
    ret = select(ret, zero, mask_gt(abs_arg, one));
    return ret;
}

/* adapted from cephes */
template <typename VecType>
always_inline VecType vec_atan_float(VecType const & arg)
{
    const VecType sign_arg = arg & VecType::gen_sign_mask();
    const VecType abs_arg  = arg & VecType::gen_abs_mask();
    const VecType one      = VecType::gen_one();
    VecType zero           = VecType::gen_zero();

    VecType arg_range0 = abs_arg;
    VecType arg_range1 = (abs_arg - one) / (abs_arg + one);
    VecType arg_range2 = -one / abs_arg;

    VecType offset_range0 = zero;
    VecType offset_range1 = 0.78539816339744830961566084581987572104929234984377;
    VecType offset_range2 = 1.57079632679489661923132169163975144209858469968754;

    VecType mask_range_01 = mask_gt(abs_arg, 0.41421356237309504880168872420969807856967187537695);
    VecType mask_range_12 = mask_gt(abs_arg, 2.41421356237309504880168872420969807856967187537698);

    VecType approx_arg = select(arg_range0,
                                select(arg_range1, arg_range2, mask_range_12),
                                mask_range_01);

    VecType approx_offset = select(offset_range0,
                                   select(offset_range1, offset_range2, mask_range_12),
                                   mask_range_01);


    VecType x = approx_arg;
    VecType x2 = x*x;

    VecType approx = approx_offset +
       x + x * x2 * (-0.333329498767852783203125 + x2 * (0.19977732002735137939453125 + x2 * (-0.1387787759304046630859375 + x2 * 8.054284751415252685546875e-2)));

    return approx ^ sign_arg;
}


template <typename VecType>
always_inline VecType vec_tanh_float(VecType const & arg)
{
    /* this order of computation (large->small->medium) seems to be the most efficient on sse*/

    const VecType sign_arg = arg & VecType::gen_sign_mask();
    const VecType abs_arg  = arg ^ sign_arg;
    const VecType one      = VecType::gen_one();
    const VecType two (2.f);
    const VecType maxlogf_2 (22.f);
    const VecType limit_small (0.625f);

    /* large values */
    const VecType abs_big          = mask_gt(abs_arg, maxlogf_2);
    const VecType result_limit_abs = one;

    /* small values */
    const VecType f1((float)-5.70498872745e-3);
    const VecType f2((float) 2.06390887954e-2);
    const VecType f3((float)-5.37397155531e-2);
    const VecType f4((float) 1.33314422036e-1);
    const VecType f5((float)-3.33332819422e-1);

    const VecType arg_sqr = abs_arg * abs_arg;
    const VecType result_small = ((((f1 * arg_sqr
                                     + f2) * arg_sqr
                                    + f3) * arg_sqr
                                   + f4) * arg_sqr
                                  + f5) * arg_sqr * arg
        + arg;

    const VecType abs_small = mask_lt(abs_arg, limit_small);

    /* medium values */
    const VecType result_medium_abs = one - two / (vec_exp_tanh_float(abs_arg + abs_arg) + one);

    /* select from large and medium branches and set sign */
    const VecType result_lm_abs = select(result_medium_abs, result_limit_abs, abs_big);
    const VecType result_lm = result_lm_abs | sign_arg;

    const VecType result = select(result_lm, result_small, abs_small);

    return result;
}

template <typename VecType>
always_inline VecType vec_signed_pow(VecType arg1, VecType arg2)
{
    const VecType sign_arg1 = arg1 & VecType::gen_sign_mask();
    const VecType abs_arg1  = arg1 ^ sign_arg1;

    const VecType result = pow(abs_arg1, arg2);

    return sign_arg1 | result;
}

/* compute pow using exp and log. seems to be faster than table-based algorithms */
template <typename VecType>
always_inline VecType vec_pow(VecType arg1, VecType arg2)
{
    const VecType result = exp(arg2 * log(arg1));
    return result;
}

template <typename VecType>
always_inline VecType vec_signed_sqrt(VecType arg)
{
    const VecType sign_arg1 = arg & VecType::gen_sign_mask();
    const VecType abs_arg1  = arg ^ sign_arg1;

    const VecType result = sqrt(abs_arg1);

    return sign_arg1 | result;
}

template <typename VecType>
always_inline VecType vec_log2(VecType arg)
{
    const double rlog2 = 1.0/std::log(2.0);
    return log(arg) * VecType((typename VecType::float_type)rlog2);
}

template <typename VecType>
always_inline VecType vec_log10(VecType arg)
{
    const double rlog10 = 1.0/std::log(10.0);
    return log(arg) * VecType((typename VecType::float_type)rlog10);
}

}
}

#undef always_inline

#endif /* NOVA_SIMD_DETAIL_VECMATH_HPP */
