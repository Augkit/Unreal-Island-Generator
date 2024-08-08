// ======== IMPLEMENTATION ========
//
// The general internal data flow (albeit not control flow!) for rendering
// a shape onto the canvas is as follows:
//
// 1. Construct a set of polybeziers representing the current path via the
//      public path building API (move_to, line_to, bezier_curve_to, etc.).
// 2. Adaptively tessellate the polybeziers into polylines (path_to_lines).
// 3. Maybe break the polylines into shorter polylines according to the dash
//      pattern (dash_lines).
// 4. Maybe stroke expand the polylines into new polylines that can be filled
//      to show the lines with width, joins, and caps (stroke_lines).
// 5. Scan-convert the polylines into a sparse representation of fractional
//      pixel coverage (lines_to_runs).
// 6. Maybe paint the covered pixel span alphas "offscreen", blur, color,
//      and blend them onto the canvas where not clipped (render_shadow).
// 7. Paint the covered pixel spans and blend them onto the canvas where not
//      clipped (render_main).

#pragma warning(disable:4800)

#include "canvas_ity.hpp"
#include <algorithm>
#include <cmath>
#include <numeric>

namespace canvas_ity
{

// 2D vector math operations
xy::xy() : x( 0.0f ), y( 0.0f ) {}
xy::xy( float new_x, float new_y ) : x( new_x ), y( new_y ) {}
static xy &operator+=( xy &left, xy right ) {
    left.x += right.x; left.y += right.y; return left; }
static xy &operator-=( xy &left, xy right ) {
    left.x -= right.x; left.y -= right.y; return left; }
static xy &operator*=( xy &left, float right ) {
    left.x *= right; left.y *= right; return left; }
static xy const operator+( xy left, xy right ) {
    return left += right; }
static xy const operator-( xy left, xy right ) {
    return left -= right; }
static xy const operator*( float left, xy right ) {
    return right *= left; }
static xy const operator*( affine_matrix const &left, xy right ) {
    return xy( left.a * right.x + left.c * right.y + left.e,
               left.b * right.x + left.d * right.y + left.f ); }
static float dot( xy left, xy right ) {
    return left.x * right.x + left.y * right.y; }
static float length( xy that ) {
    return sqrtf( dot( that, that ) ); }
static float direction( xy that ) {
    return atan2f( that.y, that.x ); }
static xy const normalized( xy that ) {
    return 1.0f / std::max( 1.0e-6f, length( that ) ) * that; }
static xy const perpendicular( xy that ) {
    return xy( -that.y, that.x ); }
static xy const lerp( xy from, xy to, float ratio ) {
    return from + ratio * ( to - from ); }

// RGBA color operations
rgba::rgba() : r( 0.0f ), g( 0.0f ), b( 0.0f ), a( 0.0f ) {}
rgba::rgba( float new_r, float new_g, float new_b, float new_a )
    : r( new_r ), g( new_g ), b( new_b ), a( new_a ) {}
static rgba &operator+=( rgba &left, rgba right ) {
    left.r += right.r; left.g += right.g; left.b += right.b;
    left.a += right.a; return left; }
static rgba &operator-=( rgba &left, rgba right ) {
    left.r -= right.r; left.g -= right.g; left.b -= right.b;
    left.a -= right.a; return left; }
static rgba &operator*=( rgba &left, float right ) {
    left.r *= right; left.g *= right; left.b *= right;
    left.a *= right; return left; }
static rgba const operator+( rgba left, rgba right ) {
    return left += right; }
static rgba const operator-( rgba left, rgba right ) {
    return left -= right; }
static rgba const operator*( float left, rgba right ) {
    return right *= left; }
static float linearized( float value ) {
    return value < 0.04045f ? value / 12.92f :
        powf( ( value + 0.055f ) / 1.055f, 2.4f ); }
static rgba const linearized( rgba that ) {
    return rgba( linearized( that.r ), linearized( that.g ),
                 linearized( that.b ), that.a ); }
static rgba const premultiplied( rgba that ) {
    return rgba( that.r * that.a, that.g * that.a,
                 that.b * that.a, that.a ); }
static float delinearized( float value ) {
    return value < 0.0031308f ? 12.92f * value :
        1.055f * powf( value, 1.0f / 2.4f ) - 0.055f; }
static rgba const delinearized( rgba that ) {
    return rgba( delinearized( that.r ), delinearized( that.g ),
                 delinearized( that.b ), that.a ); }
static rgba const unpremultiplied( rgba that ) {
    static float const threshold = 1.0f / 8160.0f;
    return that.a < threshold ? rgba( 0.0f, 0.0f, 0.0f, 0.0f ) :
        rgba( 1.0f / that.a * that.r, 1.0f / that.a * that.g,
              1.0f / that.a * that.b, that.a ); }
static rgba const clamped( rgba that ) {
    return rgba( std::min( std::max( that.r, 0.0f ), 1.0f ),
                 std::min( std::max( that.g, 0.0f ), 1.0f ),
                 std::min( std::max( that.b, 0.0f ), 1.0f ),
                 std::min( std::max( that.a, 0.0f ), 1.0f ) ); }

// RGBA color operations
rgba_20::rgba_20() : r( 0.0f ), g( 0.0f ), b( 0.0f ), a( 0.0f ), 
               d_a( 0.0f ), d_b( 0.0f ), d_c( 0.0f ), d_d( 0.0f ),
               d_e( 0.0f ), d_f( 0.0f ), d_g( 0.0f ), d_h( 0.0f ),
               d_i( 0.0f ), d_j( 0.0f ), d_k( 0.0f ), d_l( 0.0f ),
               d_m( 0.0f ), d_n( 0.0f ), d_o( 0.0f ), d_p( 0.0f ) {}
rgba_20::rgba_20( float new_r, float new_g, float new_b, float new_a, 
            float new_d_a, float new_d_b, float new_d_c, float new_d_d, 
            float new_d_e, float new_d_f, float new_d_g, float new_d_h, 
            float new_d_i, float new_d_j, float new_d_k, float new_d_l, 
            float new_d_m, float new_d_n, float new_d_o, float new_d_p )
    : r( new_r ), g( new_g ), b( new_b ), a( new_a ),
      d_a( new_d_a ), d_b( new_d_b ), d_c( new_d_c ), d_d( new_d_d ),
      d_e( new_d_e ), d_f( new_d_f ), d_g( new_d_g ), d_h( new_d_h ),
      d_i( new_d_i ), d_j( new_d_j ), d_k( new_d_k ), d_l( new_d_l ), 
      d_m( new_d_m ), d_n( new_d_n ), d_o( new_d_o ), d_p( new_d_p ) {}
static rgba_20 &operator+=( rgba_20 &left, rgba_20 right ) {
    left.r += right.r; left.g += right.g; left.b += right.b; left.a += right.a;
    left.d_a += right.d_a; left.d_b += right.d_b; left.d_c += right.d_c; left.d_d += right.d_d;
    left.d_e += right.d_e; left.d_f += right.d_f; left.d_g += right.d_g; left.d_h += right.d_h;
    left.d_i += right.d_i; left.d_j += right.d_j; left.d_k += right.d_k; left.d_l += right.d_l;
    left.d_m += right.d_m; left.d_n += right.d_n; left.d_o += right.d_o; left.d_p += right.d_p;
    return left; }
static rgba_20 &operator-=( rgba_20 &left, rgba_20 right ) {
    left.r -= right.r; left.g -= right.g; left.b -= right.b; left.a -= right.a;
    left.d_a -= right.d_a; left.d_b -= right.d_b; left.d_c -= right.d_c; left.d_d -= right.d_d;
    left.d_e -= right.d_e; left.d_f -= right.d_f; left.d_g -= right.d_g; left.d_h -= right.d_h;
    left.d_i -= right.d_i; left.d_j -= right.d_j; left.d_k -= right.d_k; left.d_l -= right.d_l;
    left.d_m -= right.d_m; left.d_n -= right.d_n; left.d_o -= right.d_o; left.d_p -= right.d_p;
    return left; }
static rgba_20 &operator*=( rgba_20 &left, float right ) {
    left.r *= right; left.g *= right; left.b *= right; left.a *= right;
    left.d_a *= right; left.d_b *= right; left.d_c *= right; left.d_d *= right;
    left.d_e *= right; left.d_f *= right; left.d_g *= right; left.d_h *= right;
    left.d_i *= right; left.d_j *= right; left.d_k *= right; left.d_l *= right;
    left.d_m *= right; left.d_n *= right; left.d_o *= right; left.d_p *= right;
    return left; }
static rgba_20 const operator+( rgba_20 left, rgba_20 right ) {
    return left += right; }
static rgba_20 const operator-( rgba_20 left, rgba_20 right ) {
    return left -= right; }
static rgba_20 const operator*( float left, rgba_20 right ) {
    return right *= left; }
static rgba_20 const zero() { 
    return rgba_20( 0.0f, 0.0f, 0.0f, 0.0f,
                 0.0f, 0.0f, 0.0f, 0.0f,
                 0.0f, 0.0f, 0.0f, 0.0f,
                 0.0f, 0.0f, 0.0f, 0.0f,
                 0.0f, 0.0f, 0.0f, 0.0f ); }

static rgba_20 const linearized( rgba_20 that ) {
    return rgba_20( linearized( that.r ), linearized( that.g ), linearized( that.b ), that.a,
                 linearized( that.d_a ), linearized( that.d_b ), linearized( that.d_c ), linearized( that.d_d ),
                 linearized( that.d_e ), linearized( that.d_f ), linearized( that.d_g ), linearized( that.d_h ),
                 linearized( that.d_i ), linearized( that.d_j ), linearized( that.d_k ), linearized( that.d_l ),
                 linearized( that.d_m ), linearized( that.d_n ), linearized( that.d_o ), linearized( that.d_p ) ); }
static rgba_20 const premultiplied( rgba_20 that ) {
    return rgba_20( that.r * that.a, that.g * that.a, that.b * that.a, that.a,
                 that.d_a * that.a, that.d_b * that.a, that.d_c * that.a, that.d_d * that.a,
                 that.d_e * that.a, that.d_f * that.a, that.d_g * that.a, that.d_h * that.a,
                 that.d_i * that.a, that.d_j * that.a, that.d_k * that.a, that.d_l * that.a,
                 that.d_m * that.a, that.d_n * that.a, that.d_o * that.a, that.d_p * that.a ); }
static rgba_20 const delinearized( rgba_20 that ) {
    return rgba_20( delinearized( that.r ), delinearized( that.g ), delinearized( that.b ), that.a,
                 delinearized( that.d_a ), delinearized( that.d_b ), delinearized( that.d_c ), delinearized( that.d_d ),
                 delinearized( that.d_e ), delinearized( that.d_f ), delinearized( that.d_g ), delinearized( that.d_h ),
                 delinearized( that.d_i ), delinearized( that.d_j ), delinearized( that.d_k ), delinearized( that.d_l ),
                 delinearized( that.d_m ), delinearized( that.d_n ), delinearized( that.d_o ), delinearized( that.d_p ) ); }
static rgba_20 const unpremultiplied( rgba_20 that ) {
    static float const threshold = 1.0f / 8160.0f;
    return that.a < threshold ? rgba_20() :
        rgba_20( 1.0f / that.a * that.r, 1.0f / that.a * that.g, 1.0f / that.a * that.b, that.a,
              1.0f / that.a * that.d_a, 1.0f / that.a * that.d_b, 1.0f / that.a * that.d_c, 1.0f / that.a * that.d_d,
              1.0f / that.a * that.d_e, 1.0f / that.a * that.d_f, 1.0f / that.a * that.d_g, 1.0f / that.a * that.d_h,
              1.0f / that.a * that.d_i, 1.0f / that.a * that.d_j, 1.0f / that.a * that.d_k, 1.0f / that.a * that.d_l,
              1.0f / that.a * that.d_m, 1.0f / that.a * that.d_n, 1.0f / that.a * that.d_o, 1.0f / that.a * that.d_p ); }
static rgba_20 const clamped( rgba_20 that ) {
    return rgba_20( std::min( std::max( that.r, 0.0f ), 1.0f ),
                 std::min( std::max( that.g, 0.0f ), 1.0f ),
                 std::min( std::max( that.b, 0.0f ), 1.0f ),
                 std::min( std::max( that.a, 0.0f ), 1.0f ),
                 std::min( std::max( that.d_a, 0.0f ), 1.0f ),
                 std::min( std::max( that.d_b, 0.0f ), 1.0f ),
                 std::min( std::max( that.d_c, 0.0f ), 1.0f ),
                 std::min( std::max( that.d_d, 0.0f ), 1.0f ),
                 std::min( std::max( that.d_e, 0.0f ), 1.0f ),
                 std::min( std::max( that.d_f, 0.0f ), 1.0f ),
                 std::min( std::max( that.d_g, 0.0f ), 1.0f ),
                 std::min( std::max( that.d_h, 0.0f ), 1.0f ),
                 std::min( std::max( that.d_i, 0.0f ), 1.0f ),
                 std::min( std::max( that.d_j, 0.0f ), 1.0f ),
                 std::min( std::max( that.d_k, 0.0f ), 1.0f ),
                 std::min( std::max( that.d_l, 0.0f ), 1.0f ),
                 std::min( std::max( that.d_m, 0.0f ), 1.0f ),
                 std::min( std::max( that.d_n, 0.0f ), 1.0f ),
                 std::min( std::max( that.d_o, 0.0f ), 1.0f ),
                 std::min( std::max( that.d_p, 0.0f ), 1.0f ) ); }

// Helpers for TTF file parsing
static int unsigned_8( std::vector< unsigned char > &data, int index ) {
    return data[ static_cast< size_t >( index ) ]; }
static int signed_8( std::vector< unsigned char > &data, int index ) {
    size_t place = static_cast< size_t >( index );
    return static_cast< signed char >( data[ place ] ); }
static int unsigned_16( std::vector< unsigned char > &data, int index ) {
    size_t place = static_cast< size_t >( index );
    return data[ place ] << 8 | data[ place + 1 ]; }
static int signed_16( std::vector< unsigned char > &data, int index ) {
    size_t place = static_cast< size_t >( index );
    return static_cast< short >( data[ place ] << 8 | data[ place + 1 ] ); }
static int signed_32( std::vector< unsigned char > &data, int index ) {
    size_t place = static_cast< size_t >( index );
    return ( data[ place + 0 ] << 24 | data[ place + 1 ] << 16 |
             data[ place + 2 ] <<  8 | data[ place + 3 ] <<  0 ); }

// Tessellate (at low-level) a cubic Bezier curve and add it to the polyline
// data.  This recursively splits the curve until two criteria are met
// (subject to a hard recursion depth limit).  First, the control points
// must not be farther from the line between the endpoints than the tolerance.
// By the Bezier convex hull property, this ensures that the distance between
// the true curve and the polyline approximation will be no more than the
// tolerance.  Secondly, it takes the cosine of an angular turn limit; the
// curve will be split until it turns less than this amount.  This is used
// for stroking, with the angular limit chosen such that the sagita of an arc
// with that angle and a half-stroke radius will be equal to the tolerance.
// This keeps expanded strokes approximately within tolerance.  Note that
// in the base case, it adds the control points as well as the end points.
// This way, stroke expansion infers the correct tangents from the ends of
// the polylines.
//
void canvas::add_tessellation(
    xy point_1,
    xy control_1,
    xy control_2,
    xy point_2,
    float angular,
    int limit )
{
    static float const tolerance = 0.125f;
    float flatness = tolerance * tolerance;
    xy edge_1 = control_1 - point_1;
    xy edge_2 = control_2 - control_1;
    xy edge_3 = point_2 - control_2;
    xy segment = point_2 - point_1;
    float squared_1 = dot( edge_1, edge_1 );
    float squared_2 = dot( edge_2, edge_2 );
    float squared_3 = dot( edge_3, edge_3 );
    static float const epsilon = 1.0e-4f;
    float length_squared = std::max( epsilon, dot( segment, segment ) );
    float projection_1 = dot( edge_1, segment ) / length_squared;
    float projection_2 = dot( edge_3, segment ) / length_squared;
    float clamped_1 = std::min( std::max( projection_1, 0.0f ), 1.0f );
    float clamped_2 = std::min( std::max( projection_2, 0.0f ), 1.0f );
    xy to_line_1 = point_1 + clamped_1 * segment - control_1;
    xy to_line_2 = point_2 - clamped_2 * segment - control_2;
    float cosine = 1.0f;
    if ( angular > -1.0f )
    {
        if ( squared_1 * squared_3 != 0.0f )
            cosine = dot( edge_1, edge_3 ) / sqrtf( squared_1 * squared_3 );
        else if ( squared_1 * squared_2 != 0.0f )
            cosine = dot( edge_1, edge_2 ) / sqrtf( squared_1 * squared_2 );
        else if ( squared_2 * squared_3 != 0.0f )
            cosine = dot( edge_2, edge_3 ) / sqrtf( squared_2 * squared_3 );
    }
    if ( ( dot( to_line_1, to_line_1 ) <= flatness &&
           dot( to_line_2, to_line_2 ) <= flatness &&
           cosine >= angular ) ||
         !limit )
    {
        if ( angular > -1.0f && squared_1 != 0.0f )
            lines.points.push_back( control_1 );
        if ( angular > -1.0f && squared_2 != 0.0f )
            lines.points.push_back( control_2 );
        if ( angular == -1.0f || squared_3 != 0.0f )
            lines.points.push_back( point_2 );
        return;
    }
    xy left_1 = lerp( point_1, control_1, 0.5f );
    xy middle = lerp( control_1, control_2, 0.5f );
    xy right_2 = lerp( control_2, point_2, 0.5f );
    xy left_2 = lerp( left_1, middle, 0.5f );
    xy right_1 = lerp( middle, right_2, 0.5f );
    xy split = lerp( left_2, right_1, 0.5f );
    add_tessellation( point_1, left_1, left_2, split, angular, limit - 1 );
    add_tessellation( split, right_1, right_2, point_2, angular, limit - 1 );
}

// Tessellate (at high-level) a cubic Bezier curve and add it to the polyline
// data.  This solves both for the extreme in curvature and for the horizontal
// and vertical extrema.  It then splits the curve into segments at these
// points and passes them off to the lower-level recursive tessellation.
// This preconditioning means the polyline exactly includes any cusps or
// ends of tight loops without the flatness test needing to locate it via
// bisection, and the angular limit test can use simple dot products without
// fear of curves turning more than 90 degrees.
//
void canvas::add_bezier(
    xy point_1,
    xy control_1,
    xy control_2,
    xy point_2,
    float angular )
{
    xy edge_1 = control_1 - point_1;
    xy edge_2 = control_2 - control_1;
    xy edge_3 = point_2 - control_2;
    if ( dot( edge_1, edge_1 ) == 0.0f &&
         dot( edge_3, edge_3 ) == 0.0f )
    {
        lines.points.push_back( point_2 );
        return;
    }
    float at[ 7 ] = { 0.0f, 1.0f };
    int cuts = 2;
    xy extrema_a = -9.0f * edge_2 + 3.0f * ( point_2 - point_1 );
    xy extrema_b = 6.0f * ( point_1 + control_2 ) - 12.0f * control_1;
    xy extrema_c = 3.0f * edge_1;
    static float const epsilon = 1.0e-4f;
    if ( fabsf( extrema_a.x ) > epsilon )
    {
        float discriminant =
            extrema_b.x * extrema_b.x - 4.0f * extrema_a.x * extrema_c.x;
        if ( discriminant >= 0.0f )
        {
            float sign = extrema_b.x > 0.0f ? 1.0f : -1.0f;
            float term = -extrema_b.x - sign * sqrtf( discriminant );
            float extremum_1 = term / ( 2.0f * extrema_a.x );
            at[ cuts++ ] = extremum_1;
            at[ cuts++ ] = extrema_c.x / ( extrema_a.x * extremum_1 );
        }
    }
    else if ( fabsf( extrema_b.x ) > epsilon )
        at[ cuts++ ] = -extrema_c.x / extrema_b.x;
    if ( fabsf( extrema_a.y ) > epsilon )
    {
        float discriminant =
            extrema_b.y * extrema_b.y - 4.0f * extrema_a.y * extrema_c.y;
        if ( discriminant >= 0.0f )
        {
            float sign = extrema_b.y > 0.0f ? 1.0f : -1.0f;
            float term = -extrema_b.y - sign * sqrtf( discriminant );
            float extremum_1 = term / ( 2.0f * extrema_a.y );
            at[ cuts++ ] = extremum_1;
            at[ cuts++ ] = extrema_c.y / ( extrema_a.y * extremum_1 );
        }
    }
    else if ( fabsf( extrema_b.y ) > epsilon )
        at[ cuts++ ] = -extrema_c.y / extrema_b.y;
    float determinant_1 = dot( perpendicular( edge_1 ), edge_2 );
    float determinant_2 = dot( perpendicular( edge_1 ), edge_3 );
    float determinant_3 = dot( perpendicular( edge_2 ), edge_3 );
    float curve_a = determinant_1 - determinant_2 + determinant_3;
    float curve_b = -2.0f * determinant_1 + determinant_2;
    if ( fabsf( curve_a ) > epsilon &&
         fabsf( curve_b ) > epsilon )
        at[ cuts++ ] = -0.5f * curve_b / curve_a;
    for ( int index = 1; index < cuts; ++index )
    {
        float value = at[ index ];
        int sorted = index - 1;
        for ( ; 0 <= sorted && value < at[ sorted ]; --sorted )
            at[ sorted + 1 ] = at[ sorted ];
        at[ sorted + 1 ] = value;
    }
    xy split_point_1 = point_1;
    for ( int index = 0; index < cuts - 1; ++index )
    {
        if ( !( 0.0f <= at[ index ] && at[ index + 1 ] <= 1.0f &&
                at[ index ] != at[ index + 1 ] ) )
            continue;
        float ratio = at[ index ] / at[ index + 1 ];
        xy partial_1 = lerp( point_1, control_1, at[ index + 1 ] );
        xy partial_2 = lerp( control_1, control_2, at[ index + 1 ] );
        xy partial_3 = lerp( control_2, point_2, at[ index + 1 ] );
        xy partial_4 = lerp( partial_1, partial_2, at[ index + 1 ] );
        xy partial_5 = lerp( partial_2, partial_3, at[ index + 1 ] );
        xy partial_6 = lerp( partial_1, partial_4, ratio );
        xy split_point_2 = lerp( partial_4, partial_5, at[ index + 1 ] );
        xy split_control_2 = lerp( partial_4, split_point_2, ratio );
        xy split_control_1 = lerp( partial_6, split_control_2, ratio );
        add_tessellation( split_point_1, split_control_1,
                          split_control_2, split_point_2,
                          angular, 20 );
        split_point_1 = split_point_2;
    }
}

// Convert the current path to a set of polylines.  This walks over the
// complete set of subpaths in the current path (stored as sets of cubic
// Beziers) and converts each Bezier curve segment to a polyline while
// preserving information about where subpaths begin and end and whether
// they are closed or open.  This replaces the previous polyline data.
//
void canvas::path_to_lines(
    bool stroking )
{
    static float const tolerance = 0.125f;
    float ratio = tolerance / std::max( 0.5f * line_width, tolerance );
    float angular = stroking ? ( ratio - 2.0f ) * ratio * 2.0f + 1.0f : -1.0f;
    lines.points.clear();
    lines.subpaths.clear();
    size_t index = 0;
    size_t ending = 0;
    for ( size_t subpath = 0; subpath < path.subpaths.size(); ++subpath )
    {
        ending += path.subpaths[ subpath ].count;
        size_t first = lines.points.size();
        xy point_1 = path.points[ index++ ];
        lines.points.push_back( point_1 );
        for ( ; index < ending; index += 3 )
        {
            xy control_1 = path.points[ index + 0 ];
            xy control_2 = path.points[ index + 1 ];
            xy point_2 = path.points[ index + 2 ];
            add_bezier( point_1, control_1, control_2, point_2, angular );
            point_1 = point_2;
        }
        subpath_data entry = {
            lines.points.size() - first,
            path.subpaths[ subpath ].closed };
        lines.subpaths.push_back( entry );
    }
}

// Add a text glyph directly to the polylines.  Given a glyph index, this
// parses the data for that glyph directly from the TTF glyph data table and
// immediately tessellates it to a set of a polyline subpaths which it adds
// to any subpaths already present.  It uses the current transform matrix to
// transform from the glyph's vertices in font units to the proper size and
// position on the canvas.
//
void canvas::add_glyph(
    int glyph,
    float angular )
{
    int loc_format = unsigned_16( face.data, face.head + 50 );
    int offset = face.glyf + ( loc_format ?
        signed_32( face.data, face.loca + glyph * 4 ) :
        unsigned_16( face.data, face.loca + glyph * 2 ) * 2 );
    int next = face.glyf + ( loc_format ?
        signed_32( face.data, face.loca + glyph * 4 + 4 ) :
        unsigned_16( face.data, face.loca + glyph * 2 + 2 ) * 2 );
    if ( offset == next )
        return;
    int contours = signed_16( face.data, offset );
    if ( contours < 0 )
    {
        offset += 10;
        for ( ; ; )
        {
            int flags = unsigned_16( face.data, offset );
            int component = unsigned_16( face.data, offset + 2 );
            if ( !( flags & 2 ) )
                return; // Matching points are not supported
            float e = static_cast< float >( flags & 1 ?
                signed_16( face.data, offset + 4 ) :
                signed_8( face.data, offset + 4 ) );
            float f = static_cast< float >( flags & 1 ?
                signed_16( face.data, offset + 6 ) :
                signed_8( face.data, offset + 5 ) );
            offset += flags & 1 ? 8 : 6;
            float a = flags & 200 ? static_cast< float >(
                signed_16( face.data, offset ) ) / 16384.0f : 1.0f;
            float b = flags & 128 ? static_cast< float >(
                signed_16( face.data, offset + 2 ) ) / 16384.0f : 0.0f;
            float c = flags & 128 ? static_cast< float >(
                signed_16( face.data, offset + 4 ) ) / 16384.0f : 0.0f;
            float d = flags & 8 ? a :
                flags & 64 ? static_cast< float >(
                    signed_16( face.data, offset + 2 ) ) / 16384.0f :
                flags & 128 ? static_cast< float >(
                    signed_16( face.data, offset + 6 ) ) / 16384.0f :
                1.0f;
            offset += flags & 8 ? 2 : flags & 64 ? 4 : flags & 128 ? 8 : 0;
            affine_matrix saved_forward = forward;
            affine_matrix saved_inverse = inverse;
            transform( a, b, c, d, e, f );
            add_glyph( component, angular );
            forward = saved_forward;
            inverse = saved_inverse;
            if ( !( flags & 32 ) )
                return;
        }
    }
    int hmetrics = unsigned_16( face.data, face.hhea + 34 );
    int left_side_bearing = glyph < hmetrics ?
        signed_16( face.data, face.hmtx + glyph * 4 + 2 ) :
        signed_16( face.data, face.hmtx + hmetrics * 2 + glyph * 2 );
    int x_min = signed_16( face.data, offset + 2 );
    int points = unsigned_16( face.data, offset + 8 + contours * 2 ) + 1;
    int instructions = unsigned_16( face.data, offset + 10 + contours * 2 );
    int flags_array = offset + 12 + contours * 2 + instructions;
    int flags_size = 0;
    int x_size = 0;
    for ( int index = 0; index < points; )
    {
        int flags = unsigned_8( face.data, flags_array + flags_size++ );
        int repeated = flags & 8 ?
            unsigned_8( face.data, flags_array + flags_size++ ) + 1 : 1;
        x_size += repeated * ( flags & 2 ? 1 : flags & 16 ? 0 : 2 );
        index += repeated;
    }
    int x_array = flags_array + flags_size;
    int y_array = x_array + x_size;
    int x = left_side_bearing - x_min;
    int y = 0;
    int flags = 0;
    int repeated = 0;
    int index = 0;
    for ( int contour = 0; contour < contours; ++contour )
    {
        int beginning = index;
        int ending = unsigned_16( face.data, offset + 10 + contour * 2 );
        xy begin_point = xy( 0.0f, 0.0f );
        bool begin_on = false;
        xy end_point = xy( 0.0f, 0.0f );
        bool end_on = false;
        size_t first = lines.points.size();
        for ( ; index <= ending; ++index )
        {
            if ( repeated )
                --repeated;
            else
            {
                flags = unsigned_8( face.data, flags_array++ );
                if ( flags & 8 )
                    repeated = unsigned_8( face.data, flags_array++ );
            }
            if ( flags & 2 )
                x += ( unsigned_8( face.data, x_array ) *
                       ( flags & 16 ? 1 : -1 ) );
            else if ( !( flags & 16 ) )
                x += signed_16( face.data, x_array );
            if ( flags & 4 )
                y += ( unsigned_8( face.data, y_array ) *
                       ( flags & 32 ? 1 : -1 ) );
            else if ( !( flags & 32 ) )
                y += signed_16( face.data, y_array );
            x_array += flags & 2 ? 1 : flags & 16 ? 0 : 2;
            y_array += flags & 4 ? 1 : flags & 32 ? 0 : 2;
            xy point = forward * xy( static_cast< float >( x ),
                                     static_cast< float >( y ) );
            int on_curve = flags & 1;
            if ( index == beginning )
            {
                begin_point = point;
                begin_on = on_curve;
                if ( on_curve )
                    lines.points.push_back( point );
            }
            else
            {
                xy point_2 = on_curve ? point :
                    lerp( end_point, point, 0.5f );
                if ( lines.points.size() == first ||
                     ( end_on && on_curve ) )
                    lines.points.push_back( point_2 );
                else if ( !end_on || on_curve )
                {
                    xy point_1 = lines.points.back();
                    xy control_1 = lerp( point_1, end_point, 2.0f / 3.0f );
                    xy control_2 = lerp( point_2, end_point, 2.0f / 3.0f );
                    add_bezier( point_1, control_1, control_2, point_2,
                                angular );
                }
            }
            end_point = point;
            end_on = on_curve;
        }
        if ( begin_on ^ end_on )
        {
            xy point_1 = lines.points.back();
            xy point_2 = lines.points[ first ];
            xy control = end_on ? begin_point : end_point;
            xy control_1 = lerp( point_1, control, 2.0f / 3.0f );
            xy control_2 = lerp( point_2, control, 2.0f / 3.0f );
            add_bezier( point_1, control_1, control_2, point_2, angular );
        }
        else if ( !begin_on && !end_on )
        {
            xy point_1 = lines.points.back();
            xy split = lerp( begin_point, end_point, 0.5f );
            xy point_2 = lines.points[ first ];
            xy left_1 = lerp( point_1, end_point, 2.0f / 3.0f );
            xy left_2 = lerp( split, end_point, 2.0f / 3.0f );
            xy right_1 = lerp( split, begin_point, 2.0f / 3.0f );
            xy right_2 = lerp( point_2, begin_point, 2.0f / 3.0f );
            add_bezier( point_1, left_1, left_2, split, angular );
            add_bezier( split, right_1, right_2, point_2, angular );
        }
        lines.points.push_back( lines.points[ first ] );
        subpath_data entry = { lines.points.size() - first, true };
        lines.subpaths.push_back( entry );
    }
}

// Decode the next codepoint from a null-terminated UTF-8 string to its glyph
// index within the font.  The index to the next codepoint in the string
// is advanced accordingly.  It checks for valid UTF-8 encoding, but not
// for valid unicode codepoints.  Where it finds an invalid encoding, it
// decodes it as the Unicode replacement character (U+FFFD) and advances to
// the invalid byte, per Unicode recommendation.  It also replaces low-ASCII
// whitespace characters with regular spaces.  After decoding the codepoint,
// it looks up the corresponding glyph index from the current font's character
// map table, returning a glyph index of 0 for the .notdef character (i.e.,
// "tofu") if the font lacks a glyph for that codepoint.
//
int canvas::character_to_glyph(
    char const *text,
    int &index )
{
    int bytes = ( ( text[ index ] & 0x80 ) == 0x00 ? 1 :
                  ( text[ index ] & 0xe0 ) == 0xc0 ? 2 :
                  ( text[ index ] & 0xf0 ) == 0xe0 ? 3 :
                  ( text[ index ] & 0xf8 ) == 0xf0 ? 4 :
                  0 );
    int const masks[] = { 0x0, 0x7f, 0x1f, 0x0f, 0x07 };
    int codepoint = bytes ? text[ index ] & masks[ bytes ] : 0xfffd;
    ++index;
    while ( --bytes > 0 )
        if ( ( text[ index ] & 0xc0 ) == 0x80 )
            codepoint = codepoint << 6 | ( text[ index++ ] & 0x3f );
        else
        {
            codepoint = 0xfffd;
            break;
        }
    if ( codepoint == '\t' || codepoint == '\v' || codepoint == '\f' ||
         codepoint == '\r' || codepoint == '\n' )
        codepoint = ' ';
    int tables = unsigned_16( face.data, face.cmap + 2 );
    int format_12 = 0;
    int format_4 = 0;
    int format_0 = 0;
    for ( int table = 0; table < tables; ++table )
    {
        int platform = unsigned_16( face.data, face.cmap + table * 8 + 4 );
        int encoding = unsigned_16( face.data, face.cmap + table * 8 + 6 );
        int offset = signed_32( face.data, face.cmap + table * 8 + 8 );
        int format = unsigned_16( face.data, face.cmap + offset );
        if ( platform == 3 && encoding == 10 && format == 12 )
            format_12 = face.cmap + offset;
        else if ( platform == 3 && encoding == 1 && format == 4 )
            format_4 = face.cmap + offset;
        else if ( format == 0 )
            format_0 = face.cmap + offset;
    }
    if ( format_12 )
    {
        int groups = signed_32( face.data, format_12 + 12 );
        for ( int group = 0; group < groups; ++group )
        {
            int start = signed_32( face.data, format_12 + 16 + group * 12 );
            int end = signed_32( face.data, format_12 + 20 + group * 12 );
            int glyph = signed_32( face.data, format_12 + 24 + group * 12 );
            if ( start <= codepoint && codepoint <= end )
                return codepoint - start + glyph;
        }
    }
    else if ( format_4 )
    {
        int segments = unsigned_16( face.data, format_4 + 6 );
        int end_array = format_4 + 14;
        int start_array = end_array + 2 + segments;
        int delta_array = start_array + segments;
        int range_array = delta_array + segments;
        for ( int segment = 0; segment < segments; segment += 2 )
        {
            int start = unsigned_16( face.data, start_array + segment );
            int end = unsigned_16( face.data, end_array + segment );
            int delta = signed_16( face.data, delta_array + segment );
            int range = unsigned_16( face.data, range_array + segment );
            if ( start <= codepoint && codepoint <= end )
                return range ?
                    unsigned_16( face.data, range_array + segment +
                                 ( codepoint - start ) * 2 + range ) :
                    ( codepoint + delta ) & 0xffff;
        }
    }
    else if ( format_0 && 0 <= codepoint && codepoint < 256 )
        return unsigned_8( face.data, format_0 + 6 + codepoint );
    return 0;
}

// Convert a text string to a set of polylines.  This works out the placement
// of the text string relative to the anchor position.  Then it walks through
// the string, sizing and placing each character by temporarily changing the
// current transform matrix to map from font units to canvas pixel coordinates
// before adding the glyph to the polylines.  This replaces the previous
// polyline data.
//
void canvas::text_to_lines(
    char const *text,
    xy position,
    float maximum_width,
    bool stroking )
{
    static float const tolerance = 0.125f;
    float ratio = tolerance / std::max( 0.5f * line_width, tolerance );
    float angular = stroking ? ( ratio - 2.0f ) * ratio * 2.0f + 1.0f : -1.0f;
    lines.points.clear();
    lines.subpaths.clear();
    if ( face.data.empty() || !text || maximum_width <= 0.0f )
        return;
    float width = maximum_width == 1.0e30f && text_align == leftward ? 0.0f :
        measure_text( text );
    float reduction = maximum_width / std::max( maximum_width, width );
    if ( text_align == rightward )
        position.x -= width * reduction;
    else if ( text_align == center )
        position.x -= 0.5f * width * reduction;
    xy scaling = face.scale * xy( reduction, 1.0f );
    float units_per_em = static_cast< float >(
        unsigned_16( face.data, face.head + 18 ) );
    float ascender = static_cast< float >(
        signed_16( face.data, face.os_2 + 68 ) );
    float descender = static_cast< float >(
        signed_16( face.data, face.os_2 + 70 ) );
    float normalize = face.scale * units_per_em / ( ascender - descender );
    if ( text_baseline == top )
        position.y += ascender * normalize;
    else if ( text_baseline == middle )
        position.y += ( ascender + descender ) * 0.5f * normalize;
    else if ( text_baseline == bottom )
        position.y += descender * normalize;
    else if ( text_baseline == hanging )
        position.y += 0.6f * face.scale * units_per_em;
    affine_matrix saved_forward = forward;
    affine_matrix saved_inverse = inverse;
    int hmetrics = unsigned_16( face.data, face.hhea + 34 );
    int place = 0;
    for ( int index = 0; text[ index ]; )
    {
        int glyph = character_to_glyph( text, index );
        forward = saved_forward;
        transform( scaling.x, 0.0f, 0.0f, -scaling.y,
                   position.x + static_cast< float >( place ) * scaling.x,
                   position.y );
        add_glyph( glyph, angular );
        int entry = std::min( glyph, hmetrics - 1 );
        place += unsigned_16( face.data, face.hmtx + entry * 4 );
    }
    forward = saved_forward;
    inverse = saved_inverse;
}

// Break the polylines into smaller pieces according to the dash settings.
// This walks along the polyline subpaths and dash pattern together, emitting
// new points via lerping where dash segments begin and end.  Each dash
// segment becomes a new open subpath in the polyline.  Some care is to
// taken to handle two special cases of closed subpaths.  First, those that
// are completely within the first dash segment should be emitted as-is and
// remain closed.  Secondly, those that start and end within a dash should
// have the two dashes merged together so that the lines join.  This replaces
// the previous polyline data.
//
void canvas::dash_lines()
{
    if ( line_dash.empty() )
        return;
    lines.points.swap( scratch.points );
    lines.points.clear();
    lines.subpaths.swap( scratch.subpaths );
    lines.subpaths.clear();
    float total = std::accumulate( line_dash.begin(), line_dash.end(), 0.0f );
    float offset = fmodf( line_dash_offset, total );
    if ( offset < 0.0f )
        offset += total;
    size_t start = 0;
    while ( offset >= line_dash[ start ] )
    {
        offset -= line_dash[ start ];
        start = start + 1 < line_dash.size() ? start + 1 : 0;
    }
    size_t ending = 0;
    for ( size_t subpath = 0; subpath < scratch.subpaths.size(); ++subpath )
    {
        size_t index = ending;
        ending += scratch.subpaths[ subpath ].count;
        size_t first = lines.points.size();
        size_t segment = start;
        bool emit = ~start & 1;
        size_t merge_point = lines.points.size();
        size_t merge_subpath = lines.subpaths.size();
        bool merge_emit = emit;
        float next = line_dash[ start ] - offset;
        for ( ; index + 1 < ending; ++index )
        {
            xy from = scratch.points[ index ];
            xy to = scratch.points[ index + 1 ];
            if ( emit )
                lines.points.push_back( from );
            float line = length( inverse * to - inverse * from );
            while ( next < line )
            {
                lines.points.push_back( lerp( from, to, next / line ) );
                if ( emit )
                {
                    subpath_data entry = {
                        lines.points.size() - first, false };
                    lines.subpaths.push_back( entry );
                    first = lines.points.size();
                }
                segment = segment + 1 < line_dash.size() ? segment + 1 : 0;
                emit = !emit;
                next += line_dash[ segment ];
            }
            next -= line;
        }
        if ( emit )
        {
            lines.points.push_back( scratch.points[ index ] );
            subpath_data entry = { lines.points.size() - first, false };
            lines.subpaths.push_back( entry );
            if ( scratch.subpaths[ subpath ].closed && merge_emit )
            {
                if ( lines.subpaths.size() == merge_subpath + 1 )
                    lines.subpaths.back().closed = true;
                else
                {
                    size_t count = lines.subpaths.back().count;
                    std::rotate( ( lines.points.begin() +
                                   static_cast< ptrdiff_t >( merge_point ) ),
                                 ( lines.points.end() -
                                   static_cast< ptrdiff_t >( count ) ),
                                 lines.points.end() );
                    lines.subpaths[ merge_subpath ].count += count;
                    lines.subpaths.pop_back();
                }
            }
        }
    }
}

// Trace along a series of points from a subpath in the scratch polylines
// and add new points to the main polylines with the stroke expansion on
// one side.  Calling this again with the ends reversed adds the other
// half of the stroke.  If the original subpath was closed, each pass
// adds a complete closed loop, with one adding the inside and one adding
// the outside.  The two will wind in opposite directions.  If the original
// subpath was open, each pass ends with one of the line caps and the two
// passes together form a single closed loop.  In either case, this handles
// adding line joins, including inner joins.  Care is taken to fill tight
// inside turns correctly by adding additional windings.  See Figure 10 of
// "Converting Stroked Primitives to Filled Primitives" by Diego Nehab, for
// the inspiration for these extra windings.
//
void canvas::add_half_stroke(
    size_t beginning,
    size_t ending,
    bool closed )
{
    float half = line_width * 0.5f;
    float ratio = miter_limit * miter_limit * half * half;
    xy in_direction = xy( 0.0f, 0.0f );
    float in_length = 0.0f;
    xy point = inverse * scratch.points[ beginning ];
    size_t finish = beginning;
    size_t index = beginning;
    do
    {
        xy next = inverse * scratch.points[ index ];
        xy out_direction = normalized( next - point );
        float out_length = length( next - point );
        static float const epsilon = 1.0e-4f;
        if ( in_length != 0.0f && out_length >= epsilon )
        {
            if ( closed && finish == beginning )
                finish = index;
            xy side_in = point + half * perpendicular( in_direction );
            xy side_out = point + half * perpendicular( out_direction );
            float turn = dot( perpendicular( in_direction ), out_direction );
            if ( fabsf( turn ) < epsilon )
                turn = 0.0f;
            xy offset = turn == 0.0f ? xy( 0.0f, 0.0f ) :
                half / turn * ( out_direction - in_direction );
            bool tight = ( dot( offset, in_direction ) < -in_length &&
                           dot( offset, out_direction ) > out_length );
            if ( turn > 0.0f && tight )
            {
                std::swap( side_in, side_out );
                std::swap( in_direction, out_direction );
                lines.points.push_back( forward * side_out );
                lines.points.push_back( forward * point );
                lines.points.push_back( forward * side_in );
            }
            if ( ( turn > 0.0f && !tight ) ||
                 ( turn != 0.0f && line_join == miter &&
                   dot( offset, offset ) <= ratio ) )
                lines.points.push_back( forward * ( point + offset ) );
            else if ( line_join == rounded )
            {
                float cosine = dot( in_direction, out_direction );
                float angle = acosf(
                    std::min( std::max( cosine, -1.0f ), 1.0f ) );
                float alpha = 4.0f / 3.0f * tanf( 0.25f * angle );
                lines.points.push_back( forward * side_in );
                add_bezier(
                    forward * side_in,
                    forward * ( side_in + alpha * half * in_direction ),
                    forward * ( side_out - alpha * half * out_direction ),
                    forward * side_out,
                    -1.0f );
            }
            else
            {
                lines.points.push_back( forward * side_in );
                lines.points.push_back( forward * side_out );
            }
            if ( turn > 0.0f && tight )
            {
                lines.points.push_back( forward * side_out );
                lines.points.push_back( forward * point );
                lines.points.push_back( forward * side_in );
                std::swap( in_direction, out_direction );
            }
        }
        if ( out_length >= epsilon )
        {
            in_direction = out_direction;
            in_length = out_length;
            point = next;
        }
        index = ( index == ending ? beginning :
                  ending > beginning ? index + 1 :
                  index - 1 );
    } while ( index != finish );
    if ( closed || in_length == 0.0f )
        return;
    xy ahead = half * in_direction;
    xy side = perpendicular( ahead );
    if ( line_cap == butt )
    {
        lines.points.push_back( forward * ( point + side ) );
        lines.points.push_back( forward * ( point - side ) );
    }
    else if ( line_cap == square )
    {
        lines.points.push_back( forward * ( point + ahead + side ) );
        lines.points.push_back( forward * ( point + ahead - side ) );
    }
    else if ( line_cap == circle )
    {
        static float const alpha = 0.55228475f; // 4/3*tan(pi/8)
        lines.points.push_back( forward * ( point + side ) );
        add_bezier( forward * ( point + side ),
                    forward * ( point + side + alpha * ahead ),
                    forward * ( point + ahead + alpha * side ),
                    forward * ( point + ahead ),
                    -1.0f );
        add_bezier( forward * ( point + ahead ),
                    forward * ( point + ahead - alpha * side ),
                    forward * ( point - side + alpha * ahead ),
                    forward * ( point - side ),
                    -1.0f );
    }
}

// Perform stroke expansion on the polylines.  After first breaking them up
// according to the dash pattern (if any), it then moves the polyline data to
// the scratch space.  Each subpath is then traced both forwards and backwards
// to add the points for a half stroke, which together create the points for
// one (if the original subpath was open) or two (if it was closed) new closed
// subpaths which, when filled, will draw the stroked lines.  While the lower
// level code this calls only adds the points of the half strokes, this
// adds subpath information for the strokes.  This replaces the previous
// polyline data.
//
void canvas::stroke_lines()
{
    if ( forward.a * forward.d - forward.b * forward.c == 0.0f )
        return;
    dash_lines();
    lines.points.swap( scratch.points );
    lines.points.clear();
    lines.subpaths.swap( scratch.subpaths );
    lines.subpaths.clear();
    size_t ending = 0;
    for ( size_t subpath = 0; subpath < scratch.subpaths.size(); ++subpath )
    {
        size_t beginning = ending;
        ending += scratch.subpaths[ subpath ].count;
        if ( ending - beginning < 2 )
            continue;
        size_t first = lines.points.size();
        add_half_stroke( beginning, ending - 1,
                         scratch.subpaths[ subpath ].closed );
        if ( scratch.subpaths[ subpath ].closed )
        {
            subpath_data entry = { lines.points.size() - first, true };
            lines.subpaths.push_back( entry );
            first = lines.points.size();
        }
        add_half_stroke( ending - 1, beginning,
                         scratch.subpaths[ subpath ].closed );
        subpath_data entry = { lines.points.size() - first, true };
        lines.subpaths.push_back( entry );
    }
}

// Scan-convert a single polyline segment.  This walks along the pixels that
// the segment touches in left-to-right order, using signed trapezoidal area
// to accumulate a list of changes in signed coverage at each visible pixel
// when processing them from left to right.  Each run of horizontal pixels
// ends with one final update to the right of the last pixel to bring the
// total up to date.  Note that this does not clip to the screen boundary.
//
void canvas::add_runs(
    xy from,
    xy to )
{
    static float const epsilon = 2.0e-5f;
    if ( fabsf( to.y - from.y ) < epsilon)
        return;
    float sign = to.y > from.y ? 1.0f : -1.0f;
    if ( from.x > to.x )
        std::swap( from, to );
    xy now = from;
    xy pixel = xy( floorf( now.x ), floorf( now.y ) );
    xy corner = pixel + xy( 1.0f, to.y > from.y ? 1.0f : 0.0f );
    xy slope = xy( ( to.x - from.x ) / ( to.y - from.y ),
                   ( to.y - from.y ) / ( to.x - from.x ) );
    xy next_x = ( to.x - from.x < epsilon ) ? to :
        xy( corner.x, now.y + ( corner.x - now.x ) * slope.y );
    xy next_y = xy( now.x + ( corner.y - now.y ) * slope.x, corner.y );
    if ( ( from.y < to.y && to.y < next_y.y ) ||
         ( from.y > to.y && to.y > next_y.y ) )
        next_y = to;
    float y_step = to.y > from.y ? 1.0f : -1.0f;
    do
    {
        float carry = 0.0f;
        while ( next_x.x < next_y.x )
        {
            float strip = std::min( std::max( ( next_x.y - now.y ) * y_step,
                                              0.0f ), 1.0f );
            float mid = ( next_x.x + now.x ) * 0.5f;
            float area = ( mid - pixel.x ) * strip;
            pixel_run piece = { static_cast< unsigned short >( pixel.x ),
                                static_cast< unsigned short >( pixel.y ),
                                ( carry + strip - area ) * sign };
            runs.push_back( piece );
            carry = area;
            now = next_x;
            next_x.x += 1.0f;
            next_x.y = ( next_x.x - from.x ) * slope.y + from.y;
            pixel.x += 1.0f;
        }
        float strip = std::min( std::max( ( next_y.y - now.y ) * y_step,
                                          0.0f ), 1.0f );
        float mid = ( next_y.x + now.x ) * 0.5f;
        float area = ( mid - pixel.x ) * strip;
        pixel_run piece_1 = { static_cast< unsigned short >( pixel.x ),
                              static_cast< unsigned short >( pixel.y ),
                              ( carry + strip - area ) * sign };
        pixel_run piece_2 = { static_cast< unsigned short >( pixel.x + 1.0f ),
                              static_cast< unsigned short >( pixel.y ),
                              area * sign };
        runs.push_back( piece_1 );
        runs.push_back( piece_2 );
        now = next_y;
        next_y.y += y_step;
        next_y.x = ( next_y.y - from.y ) * slope.x + from.x;
        pixel.y += y_step;
        if ( ( from.y < to.y && to.y < next_y.y ) ||
             ( from.y > to.y && to.y > next_y.y ) )
            next_y = to;
    } while ( now.y != to.y );
}

static bool operator<(
    pixel_run left,
    pixel_run right )
{
    return ( left.y < right.y ? true :
             left.y > right.y ? false :
             left.x < right.x ? true :
             left.x > right.x ? false :
             fabsf( left.delta ) < fabsf( right.delta ) );
}

// Scan-convert the polylines to prepare them for antialiased rendering.
// For each of the polyline loops, it first clips them to the screen.
// See "Reentrant Polygon Clipping" by Sutherland and Hodgman for details.
// Then it walks the polyline loop and scan-converts each line segment to
// produce a list of changes in signed pixel coverage when processed in
// left-to-right, top-to-bottom order.  The list of changes is then sorted
// into that order, and multiple changes to the same pixel are coalesced
// by summation.  The result is a sparse, run-length encoded description
// of the coverage of each pixel to be drawn.
//
void canvas::lines_to_runs(
    xy offset,
    int padding )
{
    runs.clear();
    float width = static_cast< float >( size_x + padding );
    float height = static_cast< float >( size_y + padding );
    size_t ending = 0;
    for ( size_t subpath = 0; subpath < lines.subpaths.size(); ++subpath )
    {
        size_t beginning = ending;
        ending += lines.subpaths[ subpath ].count;
        scratch.points.clear();
        for ( size_t index = beginning; index < ending; ++index )
            scratch.points.push_back( offset + lines.points[ index ] );
        for ( int edge = 0; edge < 4; ++edge )
        {
            xy normal = xy( edge == 0 ? 1.0f : edge == 2 ? -1.0f : 0.0f,
                            edge == 1 ? 1.0f : edge == 3 ? -1.0f : 0.0f );
            float place = edge == 2 ? width : edge == 3 ? height : 0.0f;
            size_t first = scratch.points.size();
            for ( size_t index = 0; index < first; ++index )
            {
                xy from = scratch.points[ ( index ? index : first ) - 1 ];
                xy to = scratch.points[ index ];
                float from_side = dot( from, normal ) + place;
                float to_side = dot( to, normal ) + place;
                if ( from_side * to_side < 0.0f )
                    scratch.points.push_back( lerp( from, to,
                        from_side / ( from_side - to_side ) ) );
                if ( to_side >= 0.0f )
                    scratch.points.push_back( to );
            }
            scratch.points.erase(
                scratch.points.begin(),
                scratch.points.begin() + static_cast< ptrdiff_t >( first ) );
        }
        size_t last = scratch.points.size();
        for ( size_t index = 0; index < last; ++index )
        {
            xy from = scratch.points[ ( index ? index : last ) - 1 ];
            xy to = scratch.points[ index ];
            add_runs( xy( std::min( std::max( from.x, 0.0f ), width ),
                          std::min( std::max( from.y, 0.0f ), height ) ),
                      xy( std::min( std::max( to.x, 0.0f ), width ),
                          std::min( std::max( to.y, 0.0f ), height ) ) );
        }
    }
    if ( runs.empty() )
        return;
    std::sort( runs.begin(), runs.end() );
    size_t to = 0;
    for ( size_t from = 1; from < runs.size(); ++from )
        if ( runs[ from ].x == runs[ to ].x &&
             runs[ from ].y == runs[ to ].y )
            runs[ to ].delta += runs[ from ].delta;
        else if ( runs[ from ].delta != 0.0f )
            runs[ ++to ] = runs[ from ];
    runs.erase( runs.begin() + static_cast< ptrdiff_t >( to ) + 1,
                runs.end() );
}

// Paint a pixel according to its point location and a paint style to produce
// a premultiplied, linearized RGBA color.  This handles all supported paint
// styles: solid colors, linear gradients, radial gradients, and patterns.
// For gradients and patterns, it takes into account the current transform.
// Patterns are resampled using a separable bicubic convolution filter,
// with edges handled according to the wrap mode.  See "Cubic Convolution
// Interpolation for Digital Image Processing" by Keys.  This filter is best
// known for magnification, but also works well for antialiased minification,
// since it's actually a Catmull-Rom spline approximation of Lanczos-2.
//
rgba canvas::paint_pixel(
    xy point,
    paint_brush const &brush )
{
    if ( brush.colors.empty() )
        return rgba( 0.0f, 0.0f, 0.0f, 0.0f );
    if ( brush.type == paint_brush::color )
        return brush.colors.front();
    point = inverse * point;
    if ( brush.type == paint_brush::pattern )
    {
        float width = static_cast< float >( brush.width );
        float height = static_cast< float >( brush.height );
        if ( ( ( brush.repetition & 2 ) &&
               ( point.x < 0.0f || width <= point.x ) ) ||
             ( ( brush.repetition & 1 ) &&
               ( point.y < 0.0f || height <= point.y ) ) )
            return rgba( 0.0f, 0.0f, 0.0f, 0.0f );
        float scale_x = fabsf( inverse.a ) + fabsf( inverse.c );
        float scale_y = fabsf( inverse.b ) + fabsf( inverse.d );
        scale_x = std::max( 1.0f, std::min( scale_x, width * 0.25f ) );
        scale_y = std::max( 1.0f, std::min( scale_y, height * 0.25f ) );
        float reciprocal_x = 1.0f / scale_x;
        float reciprocal_y = 1.0f / scale_y;
        point -= xy( 0.5f, 0.5f );
        int left = static_cast< int >( ceilf( point.x - scale_x * 2.0f ) );
        int top = static_cast< int >( ceilf( point.y - scale_y * 2.0f ) );
        int right = static_cast< int >( ceilf( point.x + scale_x * 2.0f ) );
        int bottom = static_cast< int >( ceilf( point.y + scale_y * 2.0f ) );
        rgba total_color = rgba( 0.0f, 0.0f, 0.0f, 0.0f );
        float total_weight = 0.0f;
        for ( int pattern_y = top; pattern_y < bottom; ++pattern_y )
        {
            float y = fabsf( reciprocal_y *
                ( static_cast< float >( pattern_y ) - point.y ) );
            float weight_y = ( y < 1.0f ?
                (    1.5f * y - 2.5f ) * y          * y + 1.0f :
                ( ( -0.5f * y + 2.5f ) * y - 4.0f ) * y + 2.0f );
            int wrapped_y = pattern_y % brush.height;
            if ( wrapped_y < 0 )
                wrapped_y += brush.height;
            if ( &brush == &image_brush )
                wrapped_y = std::min( std::max( pattern_y, 0 ),
                                      brush.height - 1 );
            for ( int pattern_x = left; pattern_x < right; ++pattern_x )
            {
                float x = fabsf( reciprocal_x *
                    ( static_cast< float >( pattern_x ) - point.x ) );
                float weight_x = ( x < 1.0f ?
                    (    1.5f * x - 2.5f ) * x          * x + 1.0f :
                    ( ( -0.5f * x + 2.5f ) * x - 4.0f ) * x + 2.0f );
                int wrapped_x = pattern_x % brush.width;
                if ( wrapped_x < 0 )
                    wrapped_x += brush.width;
                if ( &brush == &image_brush )
                    wrapped_x = std::min( std::max( pattern_x, 0 ),
                                          brush.width - 1 );
                float weight = weight_x * weight_y;
                size_t index = static_cast< size_t >(
                    wrapped_y * brush.width + wrapped_x );
                total_color += weight * brush.colors[ index ];
                total_weight += weight;
            }
        }
        return ( 1.0f / total_weight ) * total_color;
    }
    float offset;
    xy relative = point - brush.start;
    xy line = brush.end - brush.start;
    float gradient = dot( relative, line );
    float span = dot( line, line );
    if ( brush.type == paint_brush::linear )
    {
        if ( span == 0.0f )
            return rgba( 0.0f, 0.0f, 0.0f, 0.0f );
        offset = gradient / span;
    }
    else
    {
        float initial = brush.start_radius;
        float change = brush.end_radius - initial;
        float a = span - change * change;
        float b = -2.0f * ( gradient + initial * change );
        float c = dot( relative, relative ) - initial * initial;
        float discriminant = b * b - 4.0f * a * c;
        if ( discriminant < 0.0f ||
             ( span == 0.0f && change == 0.0f ) )
            return rgba( 0.0f, 0.0f, 0.0f, 0.0f );
        float root = sqrtf( discriminant );
        float reciprocal = 1.0f / ( 2.0f * a );
        float offset_1 = ( -b - root ) * reciprocal;
        float offset_2 = ( -b + root ) * reciprocal;
        float radius_1 = initial + change * offset_1;
        float radius_2 = initial + change * offset_2;
        if ( radius_2 >= 0.0f )
            offset = offset_2;
        else if ( radius_1 >= 0.0f )
            offset = offset_1;
        else
            return rgba( 0.0f, 0.0f, 0.0f, 0.0f );
    }
    size_t index = static_cast< size_t >(
        std::upper_bound( brush.stops.begin(), brush.stops.end(), offset ) -
        brush.stops.begin() );
    if ( index == 0 )
        return premultiplied( brush.colors.front() );
    if ( index == brush.stops.size() )
        return premultiplied( brush.colors.back() );
    float mix = ( ( offset - brush.stops[ index - 1 ] ) /
                  ( brush.stops[ index ] - brush.stops[ index - 1 ] ) );
    rgba delta = brush.colors[ index ] - brush.colors[ index - 1 ];
    return premultiplied( brush.colors[ index - 1 ] + mix * delta );
}

// Render the shadow of the polylines into the pixel buffer if needed.  After
// computing the border as the maximum distance that one pixel can affect
// another via the blur, it scan-converts the lines to runs with the shadow
// offset and that extra amount of border padding.  Then it bounds the scan
// converted shape, pads this with border, clips that to the extended canvas
// size, and rasterizes to fill a working area with the alpha.  Next, a fast
// approximation of a Gaussian blur is done using three passes of box blurs
// each in the rows and columns.  Note that these box blurs have a small extra
// weight on the tails to allow for fractional widths.  See "Theoretical
// Foundations of Gaussian Convolution by Extended Box Filtering" by Gwosdek
// et al. for details.  Finally, it colors the blurred alpha image with
// the shadow color and blends this into the pixel buffer according to the
// compositing settings and clip mask.  Note that it does not bother clearing
// outside the area of the alpha image when the compositing settings require
// clearing; that will be done on the subsequent main rendering pass.
//
void canvas::render_shadow(
    paint_brush const &brush )
{
    if ( shadow_color.a == 0.0f || ( shadow_blur == 0.0f &&
                                     shadow_offset_x == 0.0f &&
                                     shadow_offset_y == 0.0f ) )
        return;
    float sigma_squared = 0.25f * shadow_blur * shadow_blur;
    size_t radius = static_cast< size_t >(
        0.5f * sqrtf( 4.0f * sigma_squared + 1.0f ) - 0.5f );
    int border = 3 * ( static_cast< int >( radius ) + 1 );
    xy offset = xy( static_cast< float >( border ) + shadow_offset_x,
                    static_cast< float >( border ) + shadow_offset_y );
    lines_to_runs( offset, 2 * border );
    int left = size_x + 2 * border;
    int right = 0;
    int top = size_y + 2 * border;
    int bottom = 0;
    for ( size_t index = 0; index < runs.size(); ++index )
    {
        left = std::min( left, static_cast< int >( runs[ index ].x ) );
        right = std::max( right, static_cast< int >( runs[ index ].x ) );
        top = std::min( top, static_cast< int >( runs[ index ].y ) );
        bottom = std::max( bottom, static_cast< int >( runs[ index ].y ) );
    }
    left = std::max( left - border, 0 );
    right = std::min( right + border, size_x + 2 * border ) + 1;
    top = std::max( top - border, 0 );
    bottom = std::min( bottom + border, size_y + 2 * border );
    size_t width = static_cast< size_t >( std::max( right - left, 0 ) );
    size_t height = static_cast< size_t >( std::max( bottom - top, 0 ) );
    size_t working = width * height;
    shadow.clear();
    shadow.resize( working + std::max( width, height ) );
    static float const threshold = 1.0f / 8160.0f;
    {
        int x = -1;
        int y = -1;
        float sum = 0.0f;
        for ( size_t index = 0; index < runs.size(); ++index )
        {
            pixel_run next = runs[ index ];
            float coverage = std::min( fabsf( sum ), 1.0f );
            int to = next.y == y ? next.x : x + 1;
            if ( coverage >= threshold )
                for ( ; x < to; ++x )
                    shadow[ static_cast< size_t >( y - top ) * width +
                            static_cast< size_t >( x - left ) ] = coverage *
                        paint_pixel( xy( static_cast< float >( x ) + 0.5f,
                                         static_cast< float >( y ) + 0.5f ) -
                                     offset, brush ).a;
            if ( next.y != y )
                sum = 0.0f;
            x = next.x;
            y = next.y;
            sum += next.delta;
        }
    }
    float alpha = static_cast< float >( 2 * radius + 1 ) *
        ( static_cast< float >( radius * ( radius + 1 ) ) - sigma_squared ) /
        ( 2.0f * sigma_squared -
          static_cast< float >( 6 * ( radius + 1 ) * ( radius + 1 ) ) );
    float divisor = 2.0f * ( alpha + static_cast< float >( radius ) ) + 1.0f;
    float weight_1 = alpha / divisor;
    float weight_2 = ( 1.0f - alpha ) / divisor;
    for ( size_t y = 0; y < height; ++y )
        for ( int pass = 0; pass < 3; ++pass )
        {
            for ( size_t x = 0; x < width; ++x )
                shadow[ working + x ] = shadow[ y * width + x ];
            float running = weight_1 * shadow[ working + radius + 1 ];
            for ( size_t x = 0; x <= radius; ++x )
                running += ( weight_1 + weight_2 ) * shadow[ working + x ];
            shadow[ y * width ] = running;
            for ( size_t x = 1; x < width; ++x )
            {
                if ( x >= radius + 1 )
                    running -= weight_2 * shadow[ working + x - radius - 1 ];
                if ( x >= radius + 2 )
                    running -= weight_1 * shadow[ working + x - radius - 2 ];
                if ( x + radius < width )
                    running += weight_2 * shadow[ working + x + radius ];
                if ( x + radius + 1 < width )
                    running += weight_1 * shadow[ working + x + radius + 1 ];
                shadow[ y * width + x ] = running;
            }
        }
    for ( size_t x = 0; x < width; ++x )
        for ( int pass = 0; pass < 3; ++pass )
        {
            for ( size_t y = 0; y < height; ++y )
                shadow[ working + y ] = shadow[ y * width + x ];
            float running = weight_1 * shadow[ working + radius + 1 ];
            for ( size_t y = 0; y <= radius; ++y )
                running += ( weight_1 + weight_2 ) * shadow[ working + y ];
            shadow[ x ] = running;
            for ( size_t y = 1; y < height; ++y )
            {
                if ( y >= radius + 1 )
                    running -= weight_2 * shadow[ working + y - radius - 1 ];
                if ( y >= radius + 2 )
                    running -= weight_1 * shadow[ working + y - radius - 2 ];
                if ( y + radius < height )
                    running += weight_2 * shadow[ working + y + radius ];
                if ( y + radius + 1 < height )
                    running += weight_1 * shadow[ working + y + radius + 1 ];
                shadow[ y * width + x ] = running;
            }
        }
    int operation = global_composite_operation;
    int x = -1;
    int y = -1;
    float sum = 0.0f;
    for ( size_t index = 0; index < mask.size(); ++index )
    {
        pixel_run next = mask[ index ];
        float visibility = std::min( fabsf( sum ), 1.0f );
        int to = std::min( next.y == y ? next.x : x + 1, right - border );
        if ( visibility >= threshold &&
             top <= y + border && y + border < bottom )
            for ( ; x < to; ++x )
            {
                rgba &back = bitmap[ y * size_x + x ];
                rgba fore = global_alpha *
                    shadow[
                        static_cast< size_t >( y + border - top ) * width +
                        static_cast< size_t >( x + border - left ) ] *
                    shadow_color;
                float mix_fore = operation & 1 ? back.a : 0.0f;
                if ( operation & 2 )
                    mix_fore = 1.0f - mix_fore;
                float mix_back = operation & 4 ? fore.a : 0.0f;
                if ( operation & 8 )
                    mix_back = 1.0f - mix_back;
                rgba blend = mix_fore * fore + mix_back * back;
                blend.a = std::min( blend.a, 1.0f );
                back = visibility * blend + ( 1.0f - visibility ) * back;
            }
        if ( next.y != y )
            sum = 0.0f;
        x = std::max( static_cast< int >( next.x ), left - border );
        y = next.y;
        sum += next.delta;
    }
}

// Render the polylines into the pixel buffer.  It scan-converts the lines
// to runs which represent changes to the signed fractional coverage when
// read from left-to-right, top-to-bottom.  It scans through these to
// determine spans of pixels that need to be drawn, paints those pixels
// according to the brush, and then blends them into the buffer according
// to the current compositing settings.  This is slightly more complicated
// because it interleaves this with a simultaneous scan through a similar
// set of runs representing the current clip mask to determine which pixels
// it can composite into.  Note that shadows are always drawn first.
//
void canvas::render_main(
    paint_brush const &brush )
{
    if ( forward.a * forward.d - forward.b * forward.c == 0.0f )
        return;
    render_shadow( brush );
    lines_to_runs( xy( 0.0f, 0.0f ), 0 );
    int operation = global_composite_operation;
    int x = -1;
    int y = -1;
    float path_sum = 0.0f;
    float clip_sum = 0.0f;
    size_t path_index = 0;
    size_t clip_index = 0;
    while ( clip_index < mask.size() )
    {
        bool which = ( path_index < runs.size() &&
                       runs[ path_index ] < mask[ clip_index ] );
        pixel_run next = which ? runs[ path_index ] : mask[ clip_index ];
        float coverage = std::min( fabsf( path_sum ), 1.0f );
        float visibility = std::min( fabsf( clip_sum ), 1.0f );
        int to = next.y == y ? next.x : x + 1;
        static float const threshold = 1.0f / 8160.0f;
        if ( ( coverage >= threshold || ~operation & 8 ) &&
             visibility >= threshold )
            for ( ; x < to; ++x )
            {
                rgba &back = bitmap[ y * size_x + x ];
                rgba fore = coverage * global_alpha *
                    paint_pixel( xy( static_cast< float >( x ) + 0.5f,
                                     static_cast< float >( y ) + 0.5f ),
                                 brush );
                float mix_fore = operation & 1 ? back.a : 0.0f;
                if ( operation & 2 )
                    mix_fore = 1.0f - mix_fore;
                float mix_back = operation & 4 ? fore.a : 0.0f;
                if ( operation & 8 )
                    mix_back = 1.0f - mix_back;
                rgba blend = mix_fore * fore + mix_back * back;
                blend.a = std::min( blend.a, 1.0f );
                back = visibility * blend + ( 1.0f - visibility ) * back;
            }
        x = next.x;
        if ( next.y != y )
        {
            y = next.y;
            path_sum = 0.0f;
            clip_sum = 0.0f;
        }
        if ( which )
            path_sum += runs[ path_index++ ].delta;
        else
            clip_sum += mask[ clip_index++ ].delta;
    }
}

canvas::canvas(
    int width,
    int height )
    : global_composite_operation( source_over ),
      shadow_offset_x( 0.0f ),
      shadow_offset_y( 0.0f ),
      line_cap( butt ),
      line_join( miter ),
      line_dash_offset( 0.0f ),
      text_align( start ),
      text_baseline( alphabetic ),
      size_x( width ),
      size_y( height ),
      global_alpha( 1.0f ),
      shadow_blur( 0.0f ),
      line_width( 1.0f ),
      miter_limit( 10.0f ),
      fill_brush(),
      stroke_brush(),
      image_brush(),
      face(),
      bitmap( new rgba[ width * height ] ),
      saves( 0 )
{
    affine_matrix identity = { 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f };
    forward = identity;
    inverse = identity;
    set_color( fill_style, 0.0f, 0.0f, 0.0f, 1.0f );
    set_color( stroke_style, 0.0f, 0.0f, 0.0f, 1.0f );
    for ( unsigned short y = 0; y < size_y; ++y )
    {
        pixel_run piece_1 = { 0, y, 1.0f };
        pixel_run piece_2 = { static_cast< unsigned short >( size_x ), y,
                              -1.0f };
        mask.push_back( piece_1 );
        mask.push_back( piece_2 );
    }
}

canvas::~canvas()
{
    delete[] bitmap;
    while ( canvas *head = saves )
    {
        saves = head->saves;
        head->saves = 0;
        delete head;
    }
}

void canvas::scale(
    float x,
    float y )
{
    transform( x, 0.0f, 0.0f, y, 0.0f, 0.0f );
}

void canvas::rotate(
    float angle )
{
    float cosine = cosf( angle );
    float sine = sinf( angle );
    transform( cosine, sine, -sine, cosine, 0.0f, 0.0f );
}

void canvas::translate(
    float x,
    float y )
{
    transform( 1.0f, 0.0f, 0.0f, 1.0f, x, y );
}

void canvas::transform(
    float a,
    float b,
    float c,
    float d,
    float e,
    float f )
{
    set_transform( forward.a * a + forward.c * b,
                   forward.b * a + forward.d * b,
                   forward.a * c + forward.c * d,
                   forward.b * c + forward.d * d,
                   forward.a * e + forward.c * f + forward.e,
                   forward.b * e + forward.d * f + forward.f );
}

void canvas::set_transform(
    float a,
    float b,
    float c,
    float d,
    float e,
    float f )
{
    float determinant = a * d - b * c;
    float scaling = determinant != 0.0f ? 1.0f / determinant : 0.0f;
    affine_matrix new_forward = { a, b, c, d, e, f };
    affine_matrix new_inverse = {
        scaling * d, scaling * -b, scaling * -c, scaling * a,
        scaling * ( c * f - d * e ), scaling * ( b * e - a * f ) };
    forward = new_forward;
    inverse = new_inverse;
}

void canvas::set_global_alpha(
    float alpha )
{
    if ( 0.0f <= alpha && alpha <= 1.0f )
        global_alpha = alpha;
}

void canvas::set_shadow_color(
    float red,
    float green,
    float blue,
    float alpha )
{
    shadow_color = premultiplied( linearized( clamped(
        rgba( red, green, blue, alpha ) ) ) );
}

void canvas::set_shadow_blur(
    float level )
{
    if ( 0.0f <= level )
        shadow_blur = level;
}

void canvas::set_line_width(
    float width )
{
    if ( 0.0f < width )
        line_width = width;
}

void canvas::set_miter_limit(
    float limit )
{
    if ( 0.0f < limit )
        miter_limit = limit;
}

void canvas::set_line_dash(
    float const *segments,
    int count )
{
    for ( int index = 0; index < count; ++index )
        if ( segments && segments[ index ] < 0.0f )
            return;
    line_dash.clear();
    if ( !segments )
        return;
    for ( int index = 0; index < count; ++index )
        line_dash.push_back( segments[ index ] );
    if ( count & 1 )
        for ( int index = 0; index < count; ++index )
            line_dash.push_back( segments[ index ] );
}

void canvas::set_color(
    brush_type type,
    float red,
    float green,
    float blue,
    float alpha )
{
    paint_brush &brush = type == fill_style ? fill_brush : stroke_brush;
    brush.type = paint_brush::color;
    brush.colors.clear();
    brush.colors.push_back( premultiplied( linearized( clamped(
        rgba( red, green, blue, alpha ) ) ) ) );
}

void canvas::set_linear_gradient(
    brush_type type,
    float start_x,
    float start_y,
    float end_x,
    float end_y )
{
    paint_brush &brush = type == fill_style ? fill_brush : stroke_brush;
    brush.type = paint_brush::linear;
    brush.colors.clear();
    brush.stops.clear();
    brush.start = xy( start_x, start_y );
    brush.end = xy( end_x, end_y );
}

void canvas::set_radial_gradient(
    brush_type type,
    float start_x,
    float start_y,
    float start_radius,
    float end_x,
    float end_y,
    float end_radius )
{
    if ( start_radius < 0.0f || end_radius < 0.0f )
        return;
    paint_brush &brush = type == fill_style ? fill_brush : stroke_brush;
    brush.type = paint_brush::radial;
    brush.colors.clear();
    brush.stops.clear();
    brush.start = xy( start_x, start_y );
    brush.end = xy( end_x, end_y );
    brush.start_radius = start_radius;
    brush.end_radius = end_radius;
}

void canvas::add_color_stop(
    brush_type type,
    float offset,
    float red,
    float green,
    float blue,
    float alpha )
{
    paint_brush &brush = type == fill_style ? fill_brush : stroke_brush;
    if ( ( brush.type != paint_brush::linear &&
           brush.type != paint_brush::radial ) ||
         offset < 0.0f || 1.0f < offset )
        return;
    ptrdiff_t index = std::upper_bound(
        brush.stops.begin(), brush.stops.end(), offset ) -
        brush.stops.begin();
    rgba color = linearized( clamped( rgba( red, green, blue, alpha ) ) );
    brush.colors.insert( brush.colors.begin() + index, color );
    brush.stops.insert( brush.stops.begin() + index, offset );
}

void canvas::set_pattern(
    brush_type type,
    unsigned char const *image,
    int width,
    int height,
    int stride,
    repetition_style repetition )
{
    if ( !image || width <= 0 || height <= 0 )
        return;
    paint_brush &brush = type == fill_style ? fill_brush : stroke_brush;
    brush.type = paint_brush::pattern;
    brush.colors.clear();
    for ( int y = 0; y < height; ++y )
        for ( int x = 0; x < width; ++x )
        {
            int index = y * stride + x * 4;
            rgba color = rgba(
                image[ index + 0 ] / 255.0f, image[ index + 1 ] / 255.0f,
                image[ index + 2 ] / 255.0f, image[ index + 3 ] / 255.0f );
            brush.colors.push_back( premultiplied( linearized( color ) ) );
        }
    brush.width = width;
    brush.height = height;
    brush.repetition = repetition;
}

void canvas::begin_path()
{
    path.points.clear();
    path.subpaths.clear();
}

void canvas::move_to(
    float x,
    float y )
{
    if ( !path.subpaths.empty() && path.subpaths.back().count == 1 )
    {
        path.points.back() = forward * xy( x, y );
        return;
    }
    subpath_data subpath = { 1, false };
    path.points.push_back( forward * xy( x, y ) );
    path.subpaths.push_back( subpath );
}

void canvas::close_path()
{
    if ( path.subpaths.empty() )
        return;
    xy first = path.points[ path.points.size() - path.subpaths.back().count ];
    affine_matrix saved_forward = forward;
    affine_matrix identity = { 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f };
    forward = identity;
    line_to( first.x, first.y );
    path.subpaths.back().closed = true;
    move_to( first.x, first.y );
    forward = saved_forward;
}

void canvas::line_to(
    float x,
    float y )
{
    if ( path.subpaths.empty() )
    {
        move_to( x, y );
        return;
    }
    xy point_1 = path.points.back();
    xy point_2 = forward * xy( x, y );
    if ( dot( point_2 - point_1, point_2 - point_1 ) == 0.0f )
        return;
    path.points.push_back( point_1 );
    path.points.push_back( point_2 );
    path.points.push_back( point_2 );
    path.subpaths.back().count += 3;
}

void canvas::quadratic_curve_to(
    float control_x,
    float control_y,
    float x,
    float y )
{
    if ( path.subpaths.empty() )
        move_to( control_x, control_y );
    xy point_1 = path.points.back();
    xy control = forward * xy( control_x, control_y );
    xy point_2 = forward * xy( x, y );
    xy control_1 = lerp( point_1, control, 2.0f / 3.0f );
    xy control_2 = lerp( point_2, control, 2.0f / 3.0f );
    path.points.push_back( control_1 );
    path.points.push_back( control_2 );
    path.points.push_back( point_2 );
    path.subpaths.back().count += 3;
}

void canvas::bezier_curve_to(
    float control_1_x,
    float control_1_y,
    float control_2_x,
    float control_2_y,
    float x,
    float y )
{
    if ( path.subpaths.empty() )
        move_to( control_1_x, control_1_y );
    xy control_1 = forward * xy( control_1_x, control_1_y );
    xy control_2 = forward * xy( control_2_x, control_2_y );
    xy point_2 = forward * xy( x, y );
    path.points.push_back( control_1 );
    path.points.push_back( control_2 );
    path.points.push_back( point_2 );
    path.subpaths.back().count += 3;
}

void canvas::arc_to(
    float vertex_x,
    float vertex_y,
    float x,
    float y,
    float radius )
{
    if ( radius < 0.0f ||
         forward.a * forward.d - forward.b * forward.c == 0.0f )
        return;
    if ( path.subpaths.empty() )
        move_to( vertex_x, vertex_y );
    xy point_1 = inverse * path.points.back();
    xy vertex = xy( vertex_x, vertex_y );
    xy point_2 = xy( x, y );
    xy edge_1 = normalized( point_1 - vertex );
    xy edge_2 = normalized( point_2 - vertex );
    float sine = fabsf( dot( perpendicular( edge_1 ), edge_2 ) );
    static float const epsilon = 1.0e-4f;
    if ( sine < epsilon )
    {
        line_to( vertex_x, vertex_y );
        return;
    }
    xy offset = radius / sine * ( edge_1 + edge_2 );
    xy center = vertex + offset;
    float angle_1 = direction( dot( offset, edge_1 ) * edge_1 - offset );
    float angle_2 = direction( dot( offset, edge_2 ) * edge_2 - offset );
    bool reverse = static_cast< int >(
        floorf( ( angle_2 - angle_1 ) / 3.14159265f ) ) & 1;
    arc( center.x, center.y, radius, angle_1, angle_2, reverse );
}

void canvas::arc(
    float x,
    float y,
    float radius,
    float start_angle,
    float end_angle,
    bool counter_clockwise )
{
    if ( radius < 0.0f )
        return;
    static float const tau = 6.28318531f;
    float winding = counter_clockwise ? -1.0f : 1.0f;
    float from = fmodf( start_angle, tau );
    float span = fmodf( end_angle, tau ) - from;
    if ( ( end_angle - start_angle ) * winding >= tau )
        span = tau * winding;
    else if ( span * winding < 0.0f )
        span += tau * winding;
    xy centered_1 = radius * xy( cosf( from ), sinf( from ) );
    line_to( x + centered_1.x, y + centered_1.y );
    if ( span == 0.0f )
        return;
    int steps = static_cast< int >(
        std::max( 1.0f, roundf( 16.0f / tau * span * winding ) ) );
    float segment = span / static_cast< float >( steps );
    float alpha = 4.0f / 3.0f * tanf( 0.25f * segment );
    for ( int step = 0; step < steps; ++step )
    {
        float angle = from + static_cast< float >( step + 1 ) * segment;
        xy centered_2 = radius * xy( cosf( angle ), sinf( angle ) );
        xy point_1 = xy( x, y ) + centered_1;
        xy point_2 = xy( x, y ) + centered_2;
        xy control_1 = point_1 + alpha * perpendicular( centered_1 );
        xy control_2 = point_2 - alpha * perpendicular( centered_2 );
        bezier_curve_to( control_1.x, control_1.y,
                         control_2.x, control_2.y,
                         point_2.x, point_2.y );
        centered_1 = centered_2;
    }
}

void canvas::rectangle(
    float x,
    float y,
    float width,
    float height )
{
    move_to( x, y );
    line_to( x + width, y );
    line_to( x + width, y + height );
    line_to( x, y + height );
    close_path();
}

void canvas::fill()
{
    path_to_lines( false );
    render_main( fill_brush );
}

void canvas::stroke()
{
    path_to_lines( true );
    stroke_lines();
    render_main( stroke_brush );
}

void canvas::clip()
{
    path_to_lines( false );
    lines_to_runs( xy( 0.0f, 0.0f ), 0 );
    size_t part = runs.size();
    runs.insert( runs.end(), mask.begin(), mask.end() );
    mask.clear();
    int y = -1;
    float last = 0.0f;
    float sum_1 = 0.0f;
    float sum_2 = 0.0f;
    size_t index_1 = 0;
    size_t index_2 = part;
    while ( index_1 < part && index_2 < runs.size() )
    {
        bool which = runs[ index_1 ] < runs[ index_2 ];
        pixel_run next = which ? runs[ index_1 ] : runs[ index_2 ];
        if ( next.y != y )
        {
            y = next.y;
            last = 0.0f;
            sum_1 = 0.0f;
            sum_2 = 0.0f;
        }
        if ( which )
            sum_1 += runs[ index_1++ ].delta;
        else
            sum_2 += runs[ index_2++ ].delta;
        float visibility = ( std::min( fabsf( sum_1 ), 1.0f ) *
                             std::min( fabsf( sum_2 ), 1.0f ) );
        if ( visibility == last )
            continue;
        if ( !mask.empty() &&
             mask.back().x == next.x && mask.back().y == next.y )
            mask.back().delta += visibility - last;
        else
        {
            pixel_run piece = { next.x, next.y, visibility - last };
            mask.push_back( piece );
        }
        last = visibility;
    }
}

bool canvas::is_point_in_path(
    float x,
    float y )
{
    path_to_lines( false );
    int winding = 0;
    size_t subpath = 0;
    size_t beginning = 0;
    size_t ending = 0;
    for ( size_t index = 0; index < lines.points.size(); ++index )
    {
        while ( index >= ending )
        {
            beginning = ending;
            ending += lines.subpaths[ subpath++ ].count;
        }
        xy from = lines.points[ index ];
        xy to = lines.points[ index + 1 < ending ? index + 1 : beginning ];
        if ( ( from.y < y && y <= to.y ) || ( to.y < y && y <= from.y ) )
        {
            float side = dot( perpendicular( to - from ), xy( x, y ) - from );
            if ( side == 0.0f )
                return true;
            winding += side > 0.0f ? 1 : -1;
        }
        else if ( from.y == y && y == to.y &&
                  ( ( from.x <= x && x <= to.x ) ||
                    ( to.x <= x && x <= from.x ) ) )
            return true;
    }
    return winding;
}

void canvas::clear_rectangle(
    float x,
    float y,
    float width,
    float height )
{
    composite_operation saved_operation = global_composite_operation;
    float saved_global_alpha = global_alpha;
    float saved_alpha = shadow_color.a;
    paint_brush::types saved_type = fill_brush.type;
    global_composite_operation = destination_out;
    global_alpha = 1.0f;
    shadow_color.a = 0.0f;
    fill_brush.type = paint_brush::color;
    fill_rectangle( x, y, width, height );
    fill_brush.type = saved_type;
    shadow_color.a = saved_alpha;
    global_alpha = saved_global_alpha;
    global_composite_operation = saved_operation;
}

void canvas::fill_rectangle(
    float x,
    float y,
    float width,
    float height )
{
    if ( width == 0.0f || height == 0.0f )
        return;
    lines.points.clear();
    lines.subpaths.clear();
    lines.points.push_back( forward * xy( x, y ) );
    lines.points.push_back( forward * xy( x + width, y ) );
    lines.points.push_back( forward * xy( x + width, y + height ) );
    lines.points.push_back( forward * xy( x, y + height ) );
    subpath_data entry = { 4, true };
    lines.subpaths.push_back( entry );
    render_main( fill_brush );
}

void canvas::stroke_rectangle(
    float x,
    float y,
    float width,
    float height )
{
    if ( width == 0.0f && height == 0.0f )
        return;
    lines.points.clear();
    lines.subpaths.clear();
    if ( width == 0.0f || height == 0.0f )
    {
        lines.points.push_back( forward * xy( x, y ) );
        lines.points.push_back( forward * xy( x + width, y + height ) );
        subpath_data entry = { 2, false };
        lines.subpaths.push_back( entry );
    }
    else
    {
        lines.points.push_back( forward * xy( x, y ) );
        lines.points.push_back( forward * xy( x + width, y ) );
        lines.points.push_back( forward * xy( x + width, y + height ) );
        lines.points.push_back( forward * xy( x, y + height ) );
        lines.points.push_back( forward * xy( x, y ) );
        subpath_data entry = { 5, true };
        lines.subpaths.push_back( entry );
    }
    stroke_lines();
    render_main( stroke_brush );
}

bool canvas::set_font(
    unsigned char const *font,
    int bytes,
    float size )
{
    if ( font && bytes )
    {
        face.data.clear();
        face.cmap = 0;
        face.glyf = 0;
        face.head = 0;
        face.hhea = 0;
        face.hmtx = 0;
        face.loca = 0;
        face.maxp = 0;
        face.os_2 = 0;
        if ( bytes < 6 )
            return false;
        int version = ( font[ 0 ] << 24 | font[ 1 ] << 16 |
                        font[ 2 ] <<  8 | font[ 3 ] <<  0 );
        int tables = font[ 4 ] << 8 | font[ 5 ];
        if ( ( version != 0x00010000 && version != 0x74727565 ) ||
             bytes < tables * 16 + 12 )
            return false;
        face.data.insert( face.data.end(), font, font + tables * 16 + 12 );
        for ( int index = 0; index < tables; ++index )
        {
            int tag = signed_32( face.data, index * 16 + 12 );
            int offset = signed_32( face.data, index * 16 + 20 );
            int span = signed_32( face.data, index * 16 + 24 );
            if ( bytes < offset + span )
            {
                face.data.clear();
                return false;
            }
            int place = static_cast< int >( face.data.size() );
            if ( tag == 0x636d6170 )
                face.cmap = place;
            else if ( tag == 0x676c7966 )
                face.glyf = place;
            else if ( tag == 0x68656164 )
                face.head = place;
            else if ( tag == 0x68686561 )
                face.hhea = place;
            else if ( tag == 0x686d7478 )
                face.hmtx = place;
            else if ( tag == 0x6c6f6361 )
                face.loca = place;
            else if ( tag == 0x6d617870 )
                face.maxp = place;
            else if ( tag == 0x4f532f32 )
                face.os_2 = place;
            else
                continue;
            face.data.insert(
                face.data.end(), font + offset, font + offset + span );
        }
        if ( !face.cmap || !face.glyf || !face.head || !face.hhea ||
             !face.hmtx || !face.loca || !face.maxp || !face.os_2 )
        {
            face.data.clear();
            return false;
        }
    }
    if ( face.data.empty() )
        return false;
    int units_per_em = unsigned_16( face.data, face.head + 18 );
    face.scale = size / static_cast< float >( units_per_em );
    return true;
}

void canvas::fill_text(
    char const *text,
    float x,
    float y,
    float maximum_width )
{
    text_to_lines( text, xy( x, y ), maximum_width, false );
    render_main( fill_brush );
}

void canvas::stroke_text(
    char const *text,
    float x,
    float y,
    float maximum_width )
{
    text_to_lines( text, xy( x, y ), maximum_width, true );
    stroke_lines();
    render_main( stroke_brush );
}

float canvas::measure_text(
    char const *text )
{
    if ( face.data.empty() || !text )
        return 0.0f;
    int hmetrics = unsigned_16( face.data, face.hhea + 34 );
    int width = 0;
    for ( int index = 0; text[ index ]; )
    {
        int glyph = character_to_glyph( text, index );
        int entry = std::min( glyph, hmetrics - 1 );
        width += unsigned_16( face.data, face.hmtx + entry * 4 );
    }
    return static_cast< float >( width ) * face.scale;
}

void canvas::draw_image(
    unsigned char const *image,
    int width,
    int height,
    int stride,
    float x,
    float y,
    float to_width,
    float to_height )
{
    if ( !image || width <= 0 || height <= 0 ||
         to_width == 0.0f || to_height == 0.0f )
        return;
    std::swap( fill_brush, image_brush );
    set_pattern( fill_style, image, width, height, stride, repeat );
    std::swap( fill_brush, image_brush );
    lines.points.clear();
    lines.subpaths.clear();
    lines.points.push_back( forward * xy( x, y ) );
    lines.points.push_back( forward * xy( x + to_width, y ) );
    lines.points.push_back( forward * xy( x + to_width, y + to_height ) );
    lines.points.push_back( forward * xy( x, y + to_height ) );
    subpath_data entry = { 4, true };
    lines.subpaths.push_back( entry );
    affine_matrix saved_forward = forward;
    affine_matrix saved_inverse = inverse;
    translate( x + std::min( 0.0f, to_width ),
               y + std::min( 0.0f, to_height ) );
    scale( fabsf( to_width ) / static_cast< float >( width ),
           fabsf( to_height ) / static_cast< float >( height ) );
    render_main( image_brush );
    forward = saved_forward;
    inverse = saved_inverse;
}

void canvas::get_image_data(
    unsigned char *image,
    int width,
    int height,
    int stride,
    int x,
    int y )
{
    if ( !image )
        return;
    static float const bayer[][ 4 ] = {
        { 0.03125f, 0.53125f, 0.15625f, 0.65625f },
        { 0.78125f, 0.28125f, 0.90625f, 0.40625f },
        { 0.21875f, 0.71875f, 0.09375f, 0.59375f },
        { 0.96875f, 0.46875f, 0.84375f, 0.34375f } };
    for ( int image_y = 0; image_y < height; ++image_y )
        for ( int image_x = 0; image_x < width; ++image_x )
        {
            int index = image_y * stride + image_x * 4;
            int canvas_x = x + image_x;
            int canvas_y = y + image_y;
            rgba color = rgba( 0.0f, 0.0f, 0.0f, 0.0f );
            if ( 0 <= canvas_x && canvas_x < size_x &&
                 0 <= canvas_y && canvas_y < size_y )
                color = bitmap[ canvas_y * size_x + canvas_x ];
            float threshold = bayer[ canvas_y & 3 ][ canvas_x & 3 ];
            color = rgba( threshold, threshold, threshold, threshold ) +
                255.0f * delinearized( clamped( unpremultiplied( color ) ) );
            image[ index + 0 ] = static_cast< unsigned char >( color.r );
            image[ index + 1 ] = static_cast< unsigned char >( color.g );
            image[ index + 2 ] = static_cast< unsigned char >( color.b );
            image[ index + 3 ] = static_cast< unsigned char >( color.a );
        }
}

void canvas::put_image_data(
    unsigned char const *image,
    int width,
    int height,
    int stride,
    int x,
    int y )
{
    if ( !image )
        return;
    for ( int image_y = 0; image_y < height; ++image_y )
        for ( int image_x = 0; image_x < width; ++image_x )
        {
            int index = image_y * stride + image_x * 4;
            int canvas_x = x + image_x;
            int canvas_y = y + image_y;
            if ( canvas_x < 0 || size_x <= canvas_x ||
                 canvas_y < 0 || size_y <= canvas_y )
                continue;
            rgba color = rgba(
                image[ index + 0 ] / 255.0f, image[ index + 1 ] / 255.0f,
                image[ index + 2 ] / 255.0f, image[ index + 3 ] / 255.0f );
            bitmap[ canvas_y * size_x + canvas_x ] =
                premultiplied( linearized( color ) );
        }
}

void canvas::save()
{
    canvas *state = new canvas( 0, 0 );
    state->global_composite_operation = global_composite_operation;
    state->shadow_offset_x = shadow_offset_x;
    state->shadow_offset_y = shadow_offset_y;
    state->line_cap = line_cap;
    state->line_join = line_join;
    state->line_dash_offset = line_dash_offset;
    state->text_align = text_align;
    state->text_baseline = text_baseline;
    state->forward = forward;
    state->inverse = inverse;
    state->global_alpha = global_alpha;
    state->shadow_color = shadow_color;
    state->shadow_blur = shadow_blur;
    state->line_width = line_width;
    state->miter_limit = miter_limit;
    state->line_dash = line_dash;
    state->fill_brush = fill_brush;
    state->stroke_brush = stroke_brush;
    state->mask = mask;
    state->face = face;
    state->saves = saves;
    saves = state;
}

void canvas::restore()
{
    if ( !saves )
        return;
    canvas *state = saves;
    global_composite_operation = state->global_composite_operation;
    shadow_offset_x = state->shadow_offset_x;
    shadow_offset_y = state->shadow_offset_y;
    line_cap = state->line_cap;
    line_join = state->line_join;
    line_dash_offset = state->line_dash_offset;
    text_align = state->text_align;
    text_baseline = state->text_baseline;
    forward = state->forward;
    inverse = state->inverse;
    global_alpha = state->global_alpha;
    shadow_color = state->shadow_color;
    shadow_blur = state->shadow_blur;
    line_width = state->line_width;
    miter_limit = state->miter_limit;
    line_dash = state->line_dash;
    fill_brush = state->fill_brush;
    stroke_brush = state->stroke_brush;
    mask = state->mask;
    face = state->face;
    saves = state->saves;
    state->saves = 0;
    delete state;
}

// Tessellate (at low-level) a cubic Bezier curve and add it to the polyline
// data.  This recursively splits the curve until two criteria are met
// (subject to a hard recursion depth limit).  First, the control points
// must not be farther from the line between the endpoints than the tolerance.
// By the Bezier convex hull property, this ensures that the distance between
// the true curve and the polyline approximation will be no more than the
// tolerance.  Secondly, it takes the cosine of an angular turn limit; the
// curve will be split until it turns less than this amount.  This is used
// for stroking, with the angular limit chosen such that the sagita of an arc
// with that angle and a half-stroke radius will be equal to the tolerance.
// This keeps expanded strokes approximately within tolerance.  Note that
// in the base case, it adds the control points as well as the end points.
// This way, stroke expansion infers the correct tangents from the ends of
// the polylines.
//
void canvas_20::add_tessellation(
    xy point_1,
    xy control_1,
    xy control_2,
    xy point_2,
    float angular,
    int limit )
{
    static float const tolerance = 0.125f;
    float flatness = tolerance * tolerance;
    xy edge_1 = control_1 - point_1;
    xy edge_2 = control_2 - control_1;
    xy edge_3 = point_2 - control_2;
    xy segment = point_2 - point_1;
    float squared_1 = dot( edge_1, edge_1 );
    float squared_2 = dot( edge_2, edge_2 );
    float squared_3 = dot( edge_3, edge_3 );
    static float const epsilon = 1.0e-4f;
    float length_squared = std::max( epsilon, dot( segment, segment ) );
    float projection_1 = dot( edge_1, segment ) / length_squared;
    float projection_2 = dot( edge_3, segment ) / length_squared;
    float clamped_1 = std::min( std::max( projection_1, 0.0f ), 1.0f );
    float clamped_2 = std::min( std::max( projection_2, 0.0f ), 1.0f );
    xy to_line_1 = point_1 + clamped_1 * segment - control_1;
    xy to_line_2 = point_2 - clamped_2 * segment - control_2;
    float cosine = 1.0f;
    if ( angular > -1.0f )
    {
        if ( squared_1 * squared_3 != 0.0f )
            cosine = dot( edge_1, edge_3 ) / sqrtf( squared_1 * squared_3 );
        else if ( squared_1 * squared_2 != 0.0f )
            cosine = dot( edge_1, edge_2 ) / sqrtf( squared_1 * squared_2 );
        else if ( squared_2 * squared_3 != 0.0f )
            cosine = dot( edge_2, edge_3 ) / sqrtf( squared_2 * squared_3 );
    }
    if ( ( dot( to_line_1, to_line_1 ) <= flatness &&
           dot( to_line_2, to_line_2 ) <= flatness &&
           cosine >= angular ) ||
         !limit )
    {
        if ( angular > -1.0f && squared_1 != 0.0f )
            lines.points.push_back( control_1 );
        if ( angular > -1.0f && squared_2 != 0.0f )
            lines.points.push_back( control_2 );
        if ( angular == -1.0f || squared_3 != 0.0f )
            lines.points.push_back( point_2 );
        return;
    }
    xy left_1 = lerp( point_1, control_1, 0.5f );
    xy middle = lerp( control_1, control_2, 0.5f );
    xy right_2 = lerp( control_2, point_2, 0.5f );
    xy left_2 = lerp( left_1, middle, 0.5f );
    xy right_1 = lerp( middle, right_2, 0.5f );
    xy split = lerp( left_2, right_1, 0.5f );
    add_tessellation( point_1, left_1, left_2, split, angular, limit - 1 );
    add_tessellation( split, right_1, right_2, point_2, angular, limit - 1 );
}

// Tessellate (at high-level) a cubic Bezier curve and add it to the polyline
// data.  This solves both for the extreme in curvature and for the horizontal
// and vertical extrema.  It then splits the curve into segments at these
// points and passes them off to the lower-level recursive tessellation.
// This preconditioning means the polyline exactly includes any cusps or
// ends of tight loops without the flatness test needing to locate it via
// bisection, and the angular limit test can use simple dot products without
// fear of curves turning more than 90 degrees.
//
void canvas_20::add_bezier(
    xy point_1,
    xy control_1,
    xy control_2,
    xy point_2,
    float angular )
{
    xy edge_1 = control_1 - point_1;
    xy edge_2 = control_2 - control_1;
    xy edge_3 = point_2 - control_2;
    if ( dot( edge_1, edge_1 ) == 0.0f &&
         dot( edge_3, edge_3 ) == 0.0f )
    {
        lines.points.push_back( point_2 );
        return;
    }
    float at[ 7 ] = { 0.0f, 1.0f };
    int cuts = 2;
    xy extrema_a = -9.0f * edge_2 + 3.0f * ( point_2 - point_1 );
    xy extrema_b = 6.0f * ( point_1 + control_2 ) - 12.0f * control_1;
    xy extrema_c = 3.0f * edge_1;
    static float const epsilon = 1.0e-4f;
    if ( fabsf( extrema_a.x ) > epsilon )
    {
        float discriminant =
            extrema_b.x * extrema_b.x - 4.0f * extrema_a.x * extrema_c.x;
        if ( discriminant >= 0.0f )
        {
            float sign = extrema_b.x > 0.0f ? 1.0f : -1.0f;
            float term = -extrema_b.x - sign * sqrtf( discriminant );
            float extremum_1 = term / ( 2.0f * extrema_a.x );
            at[ cuts++ ] = extremum_1;
            at[ cuts++ ] = extrema_c.x / ( extrema_a.x * extremum_1 );
        }
    }
    else if ( fabsf( extrema_b.x ) > epsilon )
        at[ cuts++ ] = -extrema_c.x / extrema_b.x;
    if ( fabsf( extrema_a.y ) > epsilon )
    {
        float discriminant =
            extrema_b.y * extrema_b.y - 4.0f * extrema_a.y * extrema_c.y;
        if ( discriminant >= 0.0f )
        {
            float sign = extrema_b.y > 0.0f ? 1.0f : -1.0f;
            float term = -extrema_b.y - sign * sqrtf( discriminant );
            float extremum_1 = term / ( 2.0f * extrema_a.y );
            at[ cuts++ ] = extremum_1;
            at[ cuts++ ] = extrema_c.y / ( extrema_a.y * extremum_1 );
        }
    }
    else if ( fabsf( extrema_b.y ) > epsilon )
        at[ cuts++ ] = -extrema_c.y / extrema_b.y;
    float determinant_1 = dot( perpendicular( edge_1 ), edge_2 );
    float determinant_2 = dot( perpendicular( edge_1 ), edge_3 );
    float determinant_3 = dot( perpendicular( edge_2 ), edge_3 );
    float curve_a = determinant_1 - determinant_2 + determinant_3;
    float curve_b = -2.0f * determinant_1 + determinant_2;
    if ( fabsf( curve_a ) > epsilon &&
         fabsf( curve_b ) > epsilon )
        at[ cuts++ ] = -0.5f * curve_b / curve_a;
    for ( int index = 1; index < cuts; ++index )
    {
        float value = at[ index ];
        int sorted = index - 1;
        for ( ; 0 <= sorted && value < at[ sorted ]; --sorted )
            at[ sorted + 1 ] = at[ sorted ];
        at[ sorted + 1 ] = value;
    }
    xy split_point_1 = point_1;
    for ( int index = 0; index < cuts - 1; ++index )
    {
        if ( !( 0.0f <= at[ index ] && at[ index + 1 ] <= 1.0f &&
                at[ index ] != at[ index + 1 ] ) )
            continue;
        float ratio = at[ index ] / at[ index + 1 ];
        xy partial_1 = lerp( point_1, control_1, at[ index + 1 ] );
        xy partial_2 = lerp( control_1, control_2, at[ index + 1 ] );
        xy partial_3 = lerp( control_2, point_2, at[ index + 1 ] );
        xy partial_4 = lerp( partial_1, partial_2, at[ index + 1 ] );
        xy partial_5 = lerp( partial_2, partial_3, at[ index + 1 ] );
        xy partial_6 = lerp( partial_1, partial_4, ratio );
        xy split_point_2 = lerp( partial_4, partial_5, at[ index + 1 ] );
        xy split_control_2 = lerp( partial_4, split_point_2, ratio );
        xy split_control_1 = lerp( partial_6, split_control_2, ratio );
        add_tessellation( split_point_1, split_control_1,
                          split_control_2, split_point_2,
                          angular, 20 );
        split_point_1 = split_point_2;
    }
}

// Convert the current path to a set of polylines.  This walks over the
// complete set of subpaths in the current path (stored as sets of cubic
// Beziers) and converts each Bezier curve segment to a polyline while
// preserving information about where subpaths begin and end and whether
// they are closed or open.  This replaces the previous polyline data.
//
void canvas_20::path_to_lines(
    bool stroking )
{
    static float const tolerance = 0.125f;
    float ratio = tolerance / std::max( 0.5f * line_width, tolerance );
    float angular = stroking ? ( ratio - 2.0f ) * ratio * 2.0f + 1.0f : -1.0f;
    lines.points.clear();
    lines.subpaths.clear();
    size_t index = 0;
    size_t ending = 0;
    for ( size_t subpath = 0; subpath < path.subpaths.size(); ++subpath )
    {
        ending += path.subpaths[ subpath ].count;
        size_t first = lines.points.size();
        xy point_1 = path.points[ index++ ];
        lines.points.push_back( point_1 );
        for ( ; index < ending; index += 3 )
        {
            xy control_1 = path.points[ index + 0 ];
            xy control_2 = path.points[ index + 1 ];
            xy point_2 = path.points[ index + 2 ];
            add_bezier( point_1, control_1, control_2, point_2, angular );
            point_1 = point_2;
        }
        subpath_data entry = {
            lines.points.size() - first,
            path.subpaths[ subpath ].closed };
        lines.subpaths.push_back( entry );
    }
}

// Add a text glyph directly to the polylines.  Given a glyph index, this
// parses the data for that glyph directly from the TTF glyph data table and
// immediately tessellates it to a set of a polyline subpaths which it adds
// to any subpaths already present.  It uses the current transform matrix to
// transform from the glyph's vertices in font units to the proper size and
// position on the canvas_20.
//
void canvas_20::add_glyph(
    int glyph,
    float angular )
{
    int loc_format = unsigned_16( face.data, face.head + 50 );
    int offset = face.glyf + ( loc_format ?
        signed_32( face.data, face.loca + glyph * 4 ) :
        unsigned_16( face.data, face.loca + glyph * 2 ) * 2 );
    int next = face.glyf + ( loc_format ?
        signed_32( face.data, face.loca + glyph * 4 + 4 ) :
        unsigned_16( face.data, face.loca + glyph * 2 + 2 ) * 2 );
    if ( offset == next )
        return;
    int contours = signed_16( face.data, offset );
    if ( contours < 0 )
    {
        offset += 10;
        for ( ; ; )
        {
            int flags = unsigned_16( face.data, offset );
            int component = unsigned_16( face.data, offset + 2 );
            if ( !( flags & 2 ) )
                return; // Matching points are not supported
            float e = static_cast< float >( flags & 1 ?
                signed_16( face.data, offset + 4 ) :
                signed_8( face.data, offset + 4 ) );
            float f = static_cast< float >( flags & 1 ?
                signed_16( face.data, offset + 6 ) :
                signed_8( face.data, offset + 5 ) );
            offset += flags & 1 ? 8 : 6;
            float a = flags & 200 ? static_cast< float >(
                signed_16( face.data, offset ) ) / 16384.0f : 1.0f;
            float b = flags & 128 ? static_cast< float >(
                signed_16( face.data, offset + 2 ) ) / 16384.0f : 0.0f;
            float c = flags & 128 ? static_cast< float >(
                signed_16( face.data, offset + 4 ) ) / 16384.0f : 0.0f;
            float d = flags & 8 ? a :
                flags & 64 ? static_cast< float >(
                    signed_16( face.data, offset + 2 ) ) / 16384.0f :
                flags & 128 ? static_cast< float >(
                    signed_16( face.data, offset + 6 ) ) / 16384.0f :
                1.0f;
            offset += flags & 8 ? 2 : flags & 64 ? 4 : flags & 128 ? 8 : 0;
            affine_matrix saved_forward = forward;
            affine_matrix saved_inverse = inverse;
            transform( a, b, c, d, e, f );
            add_glyph( component, angular );
            forward = saved_forward;
            inverse = saved_inverse;
            if ( !( flags & 32 ) )
                return;
        }
    }
    int hmetrics = unsigned_16( face.data, face.hhea + 34 );
    int left_side_bearing = glyph < hmetrics ?
        signed_16( face.data, face.hmtx + glyph * 4 + 2 ) :
        signed_16( face.data, face.hmtx + hmetrics * 2 + glyph * 2 );
    int x_min = signed_16( face.data, offset + 2 );
    int points = unsigned_16( face.data, offset + 8 + contours * 2 ) + 1;
    int instructions = unsigned_16( face.data, offset + 10 + contours * 2 );
    int flags_array = offset + 12 + contours * 2 + instructions;
    int flags_size = 0;
    int x_size = 0;
    for ( int index = 0; index < points; )
    {
        int flags = unsigned_8( face.data, flags_array + flags_size++ );
        int repeated = flags & 8 ?
            unsigned_8( face.data, flags_array + flags_size++ ) + 1 : 1;
        x_size += repeated * ( flags & 2 ? 1 : flags & 16 ? 0 : 2 );
        index += repeated;
    }
    int x_array = flags_array + flags_size;
    int y_array = x_array + x_size;
    int x = left_side_bearing - x_min;
    int y = 0;
    int flags = 0;
    int repeated = 0;
    int index = 0;
    for ( int contour = 0; contour < contours; ++contour )
    {
        int beginning = index;
        int ending = unsigned_16( face.data, offset + 10 + contour * 2 );
        xy begin_point = xy( 0.0f, 0.0f );
        bool begin_on = false;
        xy end_point = xy( 0.0f, 0.0f );
        bool end_on = false;
        size_t first = lines.points.size();
        for ( ; index <= ending; ++index )
        {
            if ( repeated )
                --repeated;
            else
            {
                flags = unsigned_8( face.data, flags_array++ );
                if ( flags & 8 )
                    repeated = unsigned_8( face.data, flags_array++ );
            }
            if ( flags & 2 )
                x += ( unsigned_8( face.data, x_array ) *
                       ( flags & 16 ? 1 : -1 ) );
            else if ( !( flags & 16 ) )
                x += signed_16( face.data, x_array );
            if ( flags & 4 )
                y += ( unsigned_8( face.data, y_array ) *
                       ( flags & 32 ? 1 : -1 ) );
            else if ( !( flags & 32 ) )
                y += signed_16( face.data, y_array );
            x_array += flags & 2 ? 1 : flags & 16 ? 0 : 2;
            y_array += flags & 4 ? 1 : flags & 32 ? 0 : 2;
            xy point = forward * xy( static_cast< float >( x ),
                                     static_cast< float >( y ) );
            int on_curve = flags & 1;
            if ( index == beginning )
            {
                begin_point = point;
                begin_on = on_curve;
                if ( on_curve )
                    lines.points.push_back( point );
            }
            else
            {
                xy point_2 = on_curve ? point :
                    lerp( end_point, point, 0.5f );
                if ( lines.points.size() == first ||
                     ( end_on && on_curve ) )
                    lines.points.push_back( point_2 );
                else if ( !end_on || on_curve )
                {
                    xy point_1 = lines.points.back();
                    xy control_1 = lerp( point_1, end_point, 2.0f / 3.0f );
                    xy control_2 = lerp( point_2, end_point, 2.0f / 3.0f );
                    add_bezier( point_1, control_1, control_2, point_2,
                                angular );
                }
            }
            end_point = point;
            end_on = on_curve;
        }
        if ( begin_on ^ end_on )
        {
            xy point_1 = lines.points.back();
            xy point_2 = lines.points[ first ];
            xy control = end_on ? begin_point : end_point;
            xy control_1 = lerp( point_1, control, 2.0f / 3.0f );
            xy control_2 = lerp( point_2, control, 2.0f / 3.0f );
            add_bezier( point_1, control_1, control_2, point_2, angular );
        }
        else if ( !begin_on && !end_on )
        {
            xy point_1 = lines.points.back();
            xy split = lerp( begin_point, end_point, 0.5f );
            xy point_2 = lines.points[ first ];
            xy left_1 = lerp( point_1, end_point, 2.0f / 3.0f );
            xy left_2 = lerp( split, end_point, 2.0f / 3.0f );
            xy right_1 = lerp( split, begin_point, 2.0f / 3.0f );
            xy right_2 = lerp( point_2, begin_point, 2.0f / 3.0f );
            add_bezier( point_1, left_1, left_2, split, angular );
            add_bezier( split, right_1, right_2, point_2, angular );
        }
        lines.points.push_back( lines.points[ first ] );
        subpath_data entry = { lines.points.size() - first, true };
        lines.subpaths.push_back( entry );
    }
}

// Decode the next codepoint from a null-terminated UTF-8 string to its glyph
// index within the font.  The index to the next codepoint in the string
// is advanced accordingly.  It checks for valid UTF-8 encoding, but not
// for valid unicode codepoints.  Where it finds an invalid encoding, it
// decodes it as the Unicode replacement character (U+FFFD) and advances to
// the invalid byte, per Unicode recommendation.  It also replaces low-ASCII
// whitespace characters with regular spaces.  After decoding the codepoint,
// it looks up the corresponding glyph index from the current font's character
// map table, returning a glyph index of 0 for the .notdef character (i.e.,
// "tofu") if the font lacks a glyph for that codepoint.
//
int canvas_20::character_to_glyph(
    char const *text,
    int &index )
{
    int bytes = ( ( text[ index ] & 0x80 ) == 0x00 ? 1 :
                  ( text[ index ] & 0xe0 ) == 0xc0 ? 2 :
                  ( text[ index ] & 0xf0 ) == 0xe0 ? 3 :
                  ( text[ index ] & 0xf8 ) == 0xf0 ? 4 :
                  0 );
    int const masks[] = { 0x0, 0x7f, 0x1f, 0x0f, 0x07 };
    int codepoint = bytes ? text[ index ] & masks[ bytes ] : 0xfffd;
    ++index;
    while ( --bytes > 0 )
        if ( ( text[ index ] & 0xc0 ) == 0x80 )
            codepoint = codepoint << 6 | ( text[ index++ ] & 0x3f );
        else
        {
            codepoint = 0xfffd;
            break;
        }
    if ( codepoint == '\t' || codepoint == '\v' || codepoint == '\f' ||
         codepoint == '\r' || codepoint == '\n' )
        codepoint = ' ';
    int tables = unsigned_16( face.data, face.cmap + 2 );
    int format_12 = 0;
    int format_4 = 0;
    int format_0 = 0;
    for ( int table = 0; table < tables; ++table )
    {
        int platform = unsigned_16( face.data, face.cmap + table * 8 + 4 );
        int encoding = unsigned_16( face.data, face.cmap + table * 8 + 6 );
        int offset = signed_32( face.data, face.cmap + table * 8 + 8 );
        int format = unsigned_16( face.data, face.cmap + offset );
        if ( platform == 3 && encoding == 10 && format == 12 )
            format_12 = face.cmap + offset;
        else if ( platform == 3 && encoding == 1 && format == 4 )
            format_4 = face.cmap + offset;
        else if ( format == 0 )
            format_0 = face.cmap + offset;
    }
    if ( format_12 )
    {
        int groups = signed_32( face.data, format_12 + 12 );
        for ( int group = 0; group < groups; ++group )
        {
            int start = signed_32( face.data, format_12 + 16 + group * 12 );
            int end = signed_32( face.data, format_12 + 20 + group * 12 );
            int glyph = signed_32( face.data, format_12 + 24 + group * 12 );
            if ( start <= codepoint && codepoint <= end )
                return codepoint - start + glyph;
        }
    }
    else if ( format_4 )
    {
        int segments = unsigned_16( face.data, format_4 + 6 );
        int end_array = format_4 + 14;
        int start_array = end_array + 2 + segments;
        int delta_array = start_array + segments;
        int range_array = delta_array + segments;
        for ( int segment = 0; segment < segments; segment += 2 )
        {
            int start = unsigned_16( face.data, start_array + segment );
            int end = unsigned_16( face.data, end_array + segment );
            int delta = signed_16( face.data, delta_array + segment );
            int range = unsigned_16( face.data, range_array + segment );
            if ( start <= codepoint && codepoint <= end )
                return range ?
                    unsigned_16( face.data, range_array + segment +
                                 ( codepoint - start ) * 2 + range ) :
                    ( codepoint + delta ) & 0xffff;
        }
    }
    else if ( format_0 && 0 <= codepoint && codepoint < 256 )
        return unsigned_8( face.data, format_0 + 6 + codepoint );
    return 0;
}

// Convert a text string to a set of polylines.  This works out the placement
// of the text string relative to the anchor position.  Then it walks through
// the string, sizing and placing each character by temporarily changing the
// current transform matrix to map from font units to canvas_20 pixel coordinates
// before adding the glyph to the polylines.  This replaces the previous
// polyline data.
//
void canvas_20::text_to_lines(
    char const *text,
    xy position,
    float maximum_width,
    bool stroking )
{
    static float const tolerance = 0.125f;
    float ratio = tolerance / std::max( 0.5f * line_width, tolerance );
    float angular = stroking ? ( ratio - 2.0f ) * ratio * 2.0f + 1.0f : -1.0f;
    lines.points.clear();
    lines.subpaths.clear();
    if ( face.data.empty() || !text || maximum_width <= 0.0f )
        return;
    float width = maximum_width == 1.0e30f && text_align == leftward ? 0.0f :
        measure_text( text );
    float reduction = maximum_width / std::max( maximum_width, width );
    if ( text_align == rightward )
        position.x -= width * reduction;
    else if ( text_align == center )
        position.x -= 0.5f * width * reduction;
    xy scaling = face.scale * xy( reduction, 1.0f );
    float units_per_em = static_cast< float >(
        unsigned_16( face.data, face.head + 18 ) );
    float ascender = static_cast< float >(
        signed_16( face.data, face.os_2 + 68 ) );
    float descender = static_cast< float >(
        signed_16( face.data, face.os_2 + 70 ) );
    float normalize = face.scale * units_per_em / ( ascender - descender );
    if ( text_baseline == top )
        position.y += ascender * normalize;
    else if ( text_baseline == middle )
        position.y += ( ascender + descender ) * 0.5f * normalize;
    else if ( text_baseline == bottom )
        position.y += descender * normalize;
    else if ( text_baseline == hanging )
        position.y += 0.6f * face.scale * units_per_em;
    affine_matrix saved_forward = forward;
    affine_matrix saved_inverse = inverse;
    int hmetrics = unsigned_16( face.data, face.hhea + 34 );
    int place = 0;
    for ( int index = 0; text[ index ]; )
    {
        int glyph = character_to_glyph( text, index );
        forward = saved_forward;
        transform( scaling.x, 0.0f, 0.0f, -scaling.y,
                   position.x + static_cast< float >( place ) * scaling.x,
                   position.y );
        add_glyph( glyph, angular );
        int entry = std::min( glyph, hmetrics - 1 );
        place += unsigned_16( face.data, face.hmtx + entry * 4 );
    }
    forward = saved_forward;
    inverse = saved_inverse;
}

// Break the polylines into smaller pieces according to the dash settings.
// This walks along the polyline subpaths and dash pattern together, emitting
// new points via lerping where dash segments begin and end.  Each dash
// segment becomes a new open subpath in the polyline.  Some care is to
// taken to handle two special cases of closed subpaths.  First, those that
// are completely within the first dash segment should be emitted as-is and
// remain closed.  Secondly, those that start and end within a dash should
// have the two dashes merged together so that the lines join.  This replaces
// the previous polyline data.
//
void canvas_20::dash_lines()
{
    if ( line_dash.empty() )
        return;
    lines.points.swap( scratch.points );
    lines.points.clear();
    lines.subpaths.swap( scratch.subpaths );
    lines.subpaths.clear();
    float total = std::accumulate( line_dash.begin(), line_dash.end(), 0.0f );
    float offset = fmodf( line_dash_offset, total );
    if ( offset < 0.0f )
        offset += total;
    size_t start = 0;
    while ( offset >= line_dash[ start ] )
    {
        offset -= line_dash[ start ];
        start = start + 1 < line_dash.size() ? start + 1 : 0;
    }
    size_t ending = 0;
    for ( size_t subpath = 0; subpath < scratch.subpaths.size(); ++subpath )
    {
        size_t index = ending;
        ending += scratch.subpaths[ subpath ].count;
        size_t first = lines.points.size();
        size_t segment = start;
        bool emit = ~start & 1;
        size_t merge_point = lines.points.size();
        size_t merge_subpath = lines.subpaths.size();
        bool merge_emit = emit;
        float next = line_dash[ start ] - offset;
        for ( ; index + 1 < ending; ++index )
        {
            xy from = scratch.points[ index ];
            xy to = scratch.points[ index + 1 ];
            if ( emit )
                lines.points.push_back( from );
            float line = length( inverse * to - inverse * from );
            while ( next < line )
            {
                lines.points.push_back( lerp( from, to, next / line ) );
                if ( emit )
                {
                    subpath_data entry = {
                        lines.points.size() - first, false };
                    lines.subpaths.push_back( entry );
                    first = lines.points.size();
                }
                segment = segment + 1 < line_dash.size() ? segment + 1 : 0;
                emit = !emit;
                next += line_dash[ segment ];
            }
            next -= line;
        }
        if ( emit )
        {
            lines.points.push_back( scratch.points[ index ] );
            subpath_data entry = { lines.points.size() - first, false };
            lines.subpaths.push_back( entry );
            if ( scratch.subpaths[ subpath ].closed && merge_emit )
            {
                if ( lines.subpaths.size() == merge_subpath + 1 )
                    lines.subpaths.back().closed = true;
                else
                {
                    size_t count = lines.subpaths.back().count;
                    std::rotate( ( lines.points.begin() +
                                   static_cast< ptrdiff_t >( merge_point ) ),
                                 ( lines.points.end() -
                                   static_cast< ptrdiff_t >( count ) ),
                                 lines.points.end() );
                    lines.subpaths[ merge_subpath ].count += count;
                    lines.subpaths.pop_back();
                }
            }
        }
    }
}

// Trace along a series of points from a subpath in the scratch polylines
// and add new points to the main polylines with the stroke expansion on
// one side.  Calling this again with the ends reversed adds the other
// half of the stroke.  If the original subpath was closed, each pass
// adds a complete closed loop, with one adding the inside and one adding
// the outside.  The two will wind in opposite directions.  If the original
// subpath was open, each pass ends with one of the line caps and the two
// passes together form a single closed loop.  In either case, this handles
// adding line joins, including inner joins.  Care is taken to fill tight
// inside turns correctly by adding additional windings.  See Figure 10 of
// "Converting Stroked Primitives to Filled Primitives" by Diego Nehab, for
// the inspiration for these extra windings.
//
void canvas_20::add_half_stroke(
    size_t beginning,
    size_t ending,
    bool closed )
{
    float half = line_width * 0.5f;
    float ratio = miter_limit * miter_limit * half * half;
    xy in_direction = xy( 0.0f, 0.0f );
    float in_length = 0.0f;
    xy point = inverse * scratch.points[ beginning ];
    size_t finish = beginning;
    size_t index = beginning;
    do
    {
        xy next = inverse * scratch.points[ index ];
        xy out_direction = normalized( next - point );
        float out_length = length( next - point );
        static float const epsilon = 1.0e-4f;
        if ( in_length != 0.0f && out_length >= epsilon )
        {
            if ( closed && finish == beginning )
                finish = index;
            xy side_in = point + half * perpendicular( in_direction );
            xy side_out = point + half * perpendicular( out_direction );
            float turn = dot( perpendicular( in_direction ), out_direction );
            if ( fabsf( turn ) < epsilon )
                turn = 0.0f;
            xy offset = turn == 0.0f ? xy( 0.0f, 0.0f ) :
                half / turn * ( out_direction - in_direction );
            bool tight = ( dot( offset, in_direction ) < -in_length &&
                           dot( offset, out_direction ) > out_length );
            if ( turn > 0.0f && tight )
            {
                std::swap( side_in, side_out );
                std::swap( in_direction, out_direction );
                lines.points.push_back( forward * side_out );
                lines.points.push_back( forward * point );
                lines.points.push_back( forward * side_in );
            }
            if ( ( turn > 0.0f && !tight ) ||
                 ( turn != 0.0f && line_join == miter &&
                   dot( offset, offset ) <= ratio ) )
                lines.points.push_back( forward * ( point + offset ) );
            else if ( line_join == rounded )
            {
                float cosine = dot( in_direction, out_direction );
                float angle = acosf(
                    std::min( std::max( cosine, -1.0f ), 1.0f ) );
                float alpha = 4.0f / 3.0f * tanf( 0.25f * angle );
                lines.points.push_back( forward * side_in );
                add_bezier(
                    forward * side_in,
                    forward * ( side_in + alpha * half * in_direction ),
                    forward * ( side_out - alpha * half * out_direction ),
                    forward * side_out,
                    -1.0f );
            }
            else
            {
                lines.points.push_back( forward * side_in );
                lines.points.push_back( forward * side_out );
            }
            if ( turn > 0.0f && tight )
            {
                lines.points.push_back( forward * side_out );
                lines.points.push_back( forward * point );
                lines.points.push_back( forward * side_in );
                std::swap( in_direction, out_direction );
            }
        }
        if ( out_length >= epsilon )
        {
            in_direction = out_direction;
            in_length = out_length;
            point = next;
        }
        index = ( index == ending ? beginning :
                  ending > beginning ? index + 1 :
                  index - 1 );
    } while ( index != finish );
    if ( closed || in_length == 0.0f )
        return;
    xy ahead = half * in_direction;
    xy side = perpendicular( ahead );
    if ( line_cap == butt )
    {
        lines.points.push_back( forward * ( point + side ) );
        lines.points.push_back( forward * ( point - side ) );
    }
    else if ( line_cap == square )
    {
        lines.points.push_back( forward * ( point + ahead + side ) );
        lines.points.push_back( forward * ( point + ahead - side ) );
    }
    else if ( line_cap == circle )
    {
        static float const alpha = 0.55228475f; // 4/3*tan(pi/8)
        lines.points.push_back( forward * ( point + side ) );
        add_bezier( forward * ( point + side ),
                    forward * ( point + side + alpha * ahead ),
                    forward * ( point + ahead + alpha * side ),
                    forward * ( point + ahead ),
                    -1.0f );
        add_bezier( forward * ( point + ahead ),
                    forward * ( point + ahead - alpha * side ),
                    forward * ( point - side + alpha * ahead ),
                    forward * ( point - side ),
                    -1.0f );
    }
}

// Perform stroke expansion on the polylines.  After first breaking them up
// according to the dash pattern (if any), it then moves the polyline data to
// the scratch space.  Each subpath is then traced both forwards and backwards
// to add the points for a half stroke, which together create the points for
// one (if the original subpath was open) or two (if it was closed) new closed
// subpaths which, when filled, will draw the stroked lines.  While the lower
// level code this calls only adds the points of the half strokes, this
// adds subpath information for the strokes.  This replaces the previous
// polyline data.
//
void canvas_20::stroke_lines()
{
    if ( forward.a * forward.d - forward.b * forward.c == 0.0f )
        return;
    dash_lines();
    lines.points.swap( scratch.points );
    lines.points.clear();
    lines.subpaths.swap( scratch.subpaths );
    lines.subpaths.clear();
    size_t ending = 0;
    for ( size_t subpath = 0; subpath < scratch.subpaths.size(); ++subpath )
    {
        size_t beginning = ending;
        ending += scratch.subpaths[ subpath ].count;
        if ( ending - beginning < 2 )
            continue;
        size_t first = lines.points.size();
        add_half_stroke( beginning, ending - 1,
                         scratch.subpaths[ subpath ].closed );
        if ( scratch.subpaths[ subpath ].closed )
        {
            subpath_data entry = { lines.points.size() - first, true };
            lines.subpaths.push_back( entry );
            first = lines.points.size();
        }
        add_half_stroke( ending - 1, beginning,
                         scratch.subpaths[ subpath ].closed );
        subpath_data entry = { lines.points.size() - first, true };
        lines.subpaths.push_back( entry );
    }
}

// Scan-convert a single polyline segment.  This walks along the pixels that
// the segment touches in left-to-right order, using signed trapezoidal area
// to accumulate a list of changes in signed coverage at each visible pixel
// when processing them from left to right.  Each run of horizontal pixels
// ends with one final update to the right of the last pixel to bring the
// total up to date.  Note that this does not clip to the screen boundary.
//
void canvas_20::add_runs(
    xy from,
    xy to )
{
    static float const epsilon = 2.0e-5f;
    if ( fabsf( to.y - from.y ) < epsilon)
        return;
    float sign = to.y > from.y ? 1.0f : -1.0f;
    if ( from.x > to.x )
        std::swap( from, to );
    xy now = from;
    xy pixel = xy( floorf( now.x ), floorf( now.y ) );
    xy corner = pixel + xy( 1.0f, to.y > from.y ? 1.0f : 0.0f );
    xy slope = xy( ( to.x - from.x ) / ( to.y - from.y ),
                   ( to.y - from.y ) / ( to.x - from.x ) );
    xy next_x = ( to.x - from.x < epsilon ) ? to :
        xy( corner.x, now.y + ( corner.x - now.x ) * slope.y );
    xy next_y = xy( now.x + ( corner.y - now.y ) * slope.x, corner.y );
    if ( ( from.y < to.y && to.y < next_y.y ) ||
         ( from.y > to.y && to.y > next_y.y ) )
        next_y = to;
    float y_step = to.y > from.y ? 1.0f : -1.0f;
    do
    {
        float carry = 0.0f;
        while ( next_x.x < next_y.x )
        {
            float strip = std::min( std::max( ( next_x.y - now.y ) * y_step,
                                              0.0f ), 1.0f );
            float mid = ( next_x.x + now.x ) * 0.5f;
            float area = ( mid - pixel.x ) * strip;
            pixel_run piece = { static_cast< unsigned short >( pixel.x ),
                                static_cast< unsigned short >( pixel.y ),
                                ( carry + strip - area ) * sign };
            runs.push_back( piece );
            carry = area;
            now = next_x;
            next_x.x += 1.0f;
            next_x.y = ( next_x.x - from.x ) * slope.y + from.y;
            pixel.x += 1.0f;
        }
        float strip = std::min( std::max( ( next_y.y - now.y ) * y_step,
                                          0.0f ), 1.0f );
        float mid = ( next_y.x + now.x ) * 0.5f;
        float area = ( mid - pixel.x ) * strip;
        pixel_run piece_1 = { static_cast< unsigned short >( pixel.x ),
                              static_cast< unsigned short >( pixel.y ),
                              ( carry + strip - area ) * sign };
        pixel_run piece_2 = { static_cast< unsigned short >( pixel.x + 1.0f ),
                              static_cast< unsigned short >( pixel.y ),
                              area * sign };
        runs.push_back( piece_1 );
        runs.push_back( piece_2 );
        now = next_y;
        next_y.y += y_step;
        next_y.x = ( next_y.y - from.y ) * slope.x + from.x;
        pixel.y += y_step;
        if ( ( from.y < to.y && to.y < next_y.y ) ||
             ( from.y > to.y && to.y > next_y.y ) )
            next_y = to;
    } while ( now.y != to.y );
}

// Scan-convert the polylines to prepare them for antialiased rendering.
// For each of the polyline loops, it first clips them to the screen.
// See "Reentrant Polygon Clipping" by Sutherland and Hodgman for details.
// Then it walks the polyline loop and scan-converts each line segment to
// produce a list of changes in signed pixel coverage when processed in
// left-to-right, top-to-bottom order.  The list of changes is then sorted
// into that order, and multiple changes to the same pixel are coalesced
// by summation.  The result is a sparse, run-length encoded description
// of the coverage of each pixel to be drawn.
//
void canvas_20::lines_to_runs(
    xy offset,
    int padding )
{
    runs.clear();
    float width = static_cast< float >( size_x + padding );
    float height = static_cast< float >( size_y + padding );
    size_t ending = 0;
    for ( size_t subpath = 0; subpath < lines.subpaths.size(); ++subpath )
    {
        size_t beginning = ending;
        ending += lines.subpaths[ subpath ].count;
        scratch.points.clear();
        for ( size_t index = beginning; index < ending; ++index )
            scratch.points.push_back( offset + lines.points[ index ] );
        for ( int edge = 0; edge < 4; ++edge )
        {
            xy normal = xy( edge == 0 ? 1.0f : edge == 2 ? -1.0f : 0.0f,
                            edge == 1 ? 1.0f : edge == 3 ? -1.0f : 0.0f );
            float place = edge == 2 ? width : edge == 3 ? height : 0.0f;
            size_t first = scratch.points.size();
            for ( size_t index = 0; index < first; ++index )
            {
                xy from = scratch.points[ ( index ? index : first ) - 1 ];
                xy to = scratch.points[ index ];
                float from_side = dot( from, normal ) + place;
                float to_side = dot( to, normal ) + place;
                if ( from_side * to_side < 0.0f )
                    scratch.points.push_back( lerp( from, to,
                        from_side / ( from_side - to_side ) ) );
                if ( to_side >= 0.0f )
                    scratch.points.push_back( to );
            }
            scratch.points.erase(
                scratch.points.begin(),
                scratch.points.begin() + static_cast< ptrdiff_t >( first ) );
        }
        size_t last = scratch.points.size();
        for ( size_t index = 0; index < last; ++index )
        {
            xy from = scratch.points[ ( index ? index : last ) - 1 ];
            xy to = scratch.points[ index ];
            add_runs( xy( std::min( std::max( from.x, 0.0f ), width ),
                          std::min( std::max( from.y, 0.0f ), height ) ),
                      xy( std::min( std::max( to.x, 0.0f ), width ),
                          std::min( std::max( to.y, 0.0f ), height ) ) );
        }
    }
    if ( runs.empty() )
        return;
    std::sort( runs.begin(), runs.end() );
    size_t to = 0;
    for ( size_t from = 1; from < runs.size(); ++from )
        if ( runs[ from ].x == runs[ to ].x &&
             runs[ from ].y == runs[ to ].y )
            runs[ to ].delta += runs[ from ].delta;
        else if ( runs[ from ].delta != 0.0f )
            runs[ ++to ] = runs[ from ];
    runs.erase( runs.begin() + static_cast< ptrdiff_t >( to ) + 1,
                runs.end() );
}

// Paint a pixel according to its point location and a paint style to produce
// a premultiplied, linearized RGBA color.  This handles all supported paint
// styles: solid colors, linear gradients, radial gradients, and patterns.
// For gradients and patterns, it takes into account the current transform.
// Patterns are resampled using a separable bicubic convolution filter,
// with edges handled according to the wrap mode.  See "Cubic Convolution
// Interpolation for Digital Image Processing" by Keys.  This filter is best
// known for magnification, but also works well for antialiased minification,
// since it's actually a Catmull-Rom spline approximation of Lanczos-2.
//
rgba_20 canvas_20::paint_pixel(
    xy point,
    paint_brush_20 const &brush )
{
    if ( brush.colors.empty() )
        return zero();
    if ( brush.type == paint_brush_20::color )
        return brush.colors.front();
    point = inverse * point;
    if ( brush.type == paint_brush_20::pattern )
    {
        float width = static_cast< float >( brush.width );
        float height = static_cast< float >( brush.height );
        if ( ( ( brush.repetition & 2 ) &&
               ( point.x < 0.0f || width <= point.x ) ) ||
             ( ( brush.repetition & 1 ) &&
               ( point.y < 0.0f || height <= point.y ) ) )
            return zero();
        float scale_x = fabsf( inverse.a ) + fabsf( inverse.c );
        float scale_y = fabsf( inverse.b ) + fabsf( inverse.d );
        scale_x = std::max( 1.0f, std::min( scale_x, width * 0.25f ) );
        scale_y = std::max( 1.0f, std::min( scale_y, height * 0.25f ) );
        float reciprocal_x = 1.0f / scale_x;
        float reciprocal_y = 1.0f / scale_y;
        point -= xy( 0.5f, 0.5f );
        int left = static_cast< int >( ceilf( point.x - scale_x * 2.0f ) );
        int top = static_cast< int >( ceilf( point.y - scale_y * 2.0f ) );
        int right = static_cast< int >( ceilf( point.x + scale_x * 2.0f ) );
        int bottom = static_cast< int >( ceilf( point.y + scale_y * 2.0f ) );
        rgba_20 total_color = zero();
        float total_weight = 0.0f;
        for ( int pattern_y = top; pattern_y < bottom; ++pattern_y )
        {
            float y = fabsf( reciprocal_y *
                ( static_cast< float >( pattern_y ) - point.y ) );
            float weight_y = ( y < 1.0f ?
                (    1.5f * y - 2.5f ) * y          * y + 1.0f :
                ( ( -0.5f * y + 2.5f ) * y - 4.0f ) * y + 2.0f );
            int wrapped_y = pattern_y % brush.height;
            if ( wrapped_y < 0 )
                wrapped_y += brush.height;
            if ( &brush == &image_brush )
                wrapped_y = std::min( std::max( pattern_y, 0 ),
                                      brush.height - 1 );
            for ( int pattern_x = left; pattern_x < right; ++pattern_x )
            {
                float x = fabsf( reciprocal_x *
                    ( static_cast< float >( pattern_x ) - point.x ) );
                float weight_x = ( x < 1.0f ?
                    (    1.5f * x - 2.5f ) * x          * x + 1.0f :
                    ( ( -0.5f * x + 2.5f ) * x - 4.0f ) * x + 2.0f );
                int wrapped_x = pattern_x % brush.width;
                if ( wrapped_x < 0 )
                    wrapped_x += brush.width;
                if ( &brush == &image_brush )
                    wrapped_x = std::min( std::max( pattern_x, 0 ),
                                          brush.width - 1 );
                float weight = weight_x * weight_y;
                size_t index = static_cast< size_t >(
                    wrapped_y * brush.width + wrapped_x );
                total_color += weight * brush.colors[ index ];
                total_weight += weight;
            }
        }
        return ( 1.0f / total_weight ) * total_color;
    }
    float offset;
    xy relative = point - brush.start;
    xy line = brush.end - brush.start;
    float gradient = dot( relative, line );
    float span = dot( line, line );
    if ( brush.type == paint_brush_20::linear )
    {
        if ( span == 0.0f )
            return zero();
        offset = gradient / span;
    }
    else
    {
        float initial = brush.start_radius;
        float change = brush.end_radius - initial;
        float a = span - change * change;
        float b = -2.0f * ( gradient + initial * change );
        float c = dot( relative, relative ) - initial * initial;
        float discriminant = b * b - 4.0f * a * c;
        if ( discriminant < 0.0f ||
             ( span == 0.0f && change == 0.0f ) )
            return zero();
        float root = sqrtf( discriminant );
        float reciprocal = 1.0f / ( 2.0f * a );
        float offset_1 = ( -b - root ) * reciprocal;
        float offset_2 = ( -b + root ) * reciprocal;
        float radius_1 = initial + change * offset_1;
        float radius_2 = initial + change * offset_2;
        if ( radius_2 >= 0.0f )
            offset = offset_2;
        else if ( radius_1 >= 0.0f )
            offset = offset_1;
        else
            return zero();
    }
    size_t index = static_cast< size_t >(
        std::upper_bound( brush.stops.begin(), brush.stops.end(), offset ) -
        brush.stops.begin() );
    if ( index == 0 )
        return premultiplied( brush.colors.front() );
    if ( index == brush.stops.size() )
        return premultiplied( brush.colors.back() );
    float mix = ( ( offset - brush.stops[ index - 1 ] ) /
                  ( brush.stops[ index ] - brush.stops[ index - 1 ] ) );
    rgba_20 delta = brush.colors[ index ] - brush.colors[ index - 1 ];
    return premultiplied( brush.colors[ index - 1 ] + mix * delta );
}

// Render the shadow of the polylines into the pixel buffer if needed.  After
// computing the border as the maximum distance that one pixel can affect
// another via the blur, it scan-converts the lines to runs with the shadow
// offset and that extra amount of border padding.  Then it bounds the scan
// converted shape, pads this with border, clips that to the extended canvas_20
// size, and rasterizes to fill a working area with the alpha.  Next, a fast
// approximation of a Gaussian blur is done using three passes of box blurs
// each in the rows and columns.  Note that these box blurs have a small extra
// weight on the tails to allow for fractional widths.  See "Theoretical
// Foundations of Gaussian Convolution by Extended Box Filtering" by Gwosdek
// et al. for details.  Finally, it colors the blurred alpha image with
// the shadow color and blends this into the pixel buffer according to the
// compositing settings and clip mask.  Note that it does not bother clearing
// outside the area of the alpha image when the compositing settings require
// clearing; that will be done on the subsequent main rendering pass.
//
void canvas_20::render_shadow(
    paint_brush_20 const &brush )
{
    if ( shadow_color.a == 0.0f || ( shadow_blur == 0.0f &&
                                     shadow_offset_x == 0.0f &&
                                     shadow_offset_y == 0.0f ) )
        return;
    float sigma_squared = 0.25f * shadow_blur * shadow_blur;
    size_t radius = static_cast< size_t >(
        0.5f * sqrtf( 4.0f * sigma_squared + 1.0f ) - 0.5f );
    int border = 3 * ( static_cast< int >( radius ) + 1 );
    xy offset = xy( static_cast< float >( border ) + shadow_offset_x,
                    static_cast< float >( border ) + shadow_offset_y );
    lines_to_runs( offset, 2 * border );
    int left = size_x + 2 * border;
    int right = 0;
    int top = size_y + 2 * border;
    int bottom = 0;
    for ( size_t index = 0; index < runs.size(); ++index )
    {
        left = std::min( left, static_cast< int >( runs[ index ].x ) );
        right = std::max( right, static_cast< int >( runs[ index ].x ) );
        top = std::min( top, static_cast< int >( runs[ index ].y ) );
        bottom = std::max( bottom, static_cast< int >( runs[ index ].y ) );
    }
    left = std::max( left - border, 0 );
    right = std::min( right + border, size_x + 2 * border ) + 1;
    top = std::max( top - border, 0 );
    bottom = std::min( bottom + border, size_y + 2 * border );
    size_t width = static_cast< size_t >( std::max( right - left, 0 ) );
    size_t height = static_cast< size_t >( std::max( bottom - top, 0 ) );
    size_t working = width * height;
    shadow.clear();
    shadow.resize( working + std::max( width, height ) );
    static float const threshold = 1.0f / 8160.0f;
    {
        int x = -1;
        int y = -1;
        float sum = 0.0f;
        for ( size_t index = 0; index < runs.size(); ++index )
        {
            pixel_run next = runs[ index ];
            float coverage = std::min( fabsf( sum ), 1.0f );
            int to = next.y == y ? next.x : x + 1;
            if ( coverage >= threshold )
                for ( ; x < to; ++x )
                    shadow[ static_cast< size_t >( y - top ) * width +
                            static_cast< size_t >( x - left ) ] = coverage *
                        paint_pixel( xy( static_cast< float >( x ) + 0.5f,
                                         static_cast< float >( y ) + 0.5f ) -
                                     offset, brush ).a;
            if ( next.y != y )
                sum = 0.0f;
            x = next.x;
            y = next.y;
            sum += next.delta;
        }
    }
    float alpha = static_cast< float >( 2 * radius + 1 ) *
        ( static_cast< float >( radius * ( radius + 1 ) ) - sigma_squared ) /
        ( 2.0f * sigma_squared -
          static_cast< float >( 6 * ( radius + 1 ) * ( radius + 1 ) ) );
    float divisor = 2.0f * ( alpha + static_cast< float >( radius ) ) + 1.0f;
    float weight_1 = alpha / divisor;
    float weight_2 = ( 1.0f - alpha ) / divisor;
    for ( size_t y = 0; y < height; ++y )
        for ( int pass = 0; pass < 3; ++pass )
        {
            for ( size_t x = 0; x < width; ++x )
                shadow[ working + x ] = shadow[ y * width + x ];
            float running = weight_1 * shadow[ working + radius + 1 ];
            for ( size_t x = 0; x <= radius; ++x )
                running += ( weight_1 + weight_2 ) * shadow[ working + x ];
            shadow[ y * width ] = running;
            for ( size_t x = 1; x < width; ++x )
            {
                if ( x >= radius + 1 )
                    running -= weight_2 * shadow[ working + x - radius - 1 ];
                if ( x >= radius + 2 )
                    running -= weight_1 * shadow[ working + x - radius - 2 ];
                if ( x + radius < width )
                    running += weight_2 * shadow[ working + x + radius ];
                if ( x + radius + 1 < width )
                    running += weight_1 * shadow[ working + x + radius + 1 ];
                shadow[ y * width + x ] = running;
            }
        }
    for ( size_t x = 0; x < width; ++x )
        for ( int pass = 0; pass < 3; ++pass )
        {
            for ( size_t y = 0; y < height; ++y )
                shadow[ working + y ] = shadow[ y * width + x ];
            float running = weight_1 * shadow[ working + radius + 1 ];
            for ( size_t y = 0; y <= radius; ++y )
                running += ( weight_1 + weight_2 ) * shadow[ working + y ];
            shadow[ x ] = running;
            for ( size_t y = 1; y < height; ++y )
            {
                if ( y >= radius + 1 )
                    running -= weight_2 * shadow[ working + y - radius - 1 ];
                if ( y >= radius + 2 )
                    running -= weight_1 * shadow[ working + y - radius - 2 ];
                if ( y + radius < height )
                    running += weight_2 * shadow[ working + y + radius ];
                if ( y + radius + 1 < height )
                    running += weight_1 * shadow[ working + y + radius + 1 ];
                shadow[ y * width + x ] = running;
            }
        }
    int operation = global_composite_operation;
    int x = -1;
    int y = -1;
    float sum = 0.0f;
    for ( size_t index = 0; index < mask.size(); ++index )
    {
        pixel_run next = mask[ index ];
        float visibility = std::min( fabsf( sum ), 1.0f );
        int to = std::min( next.y == y ? next.x : x + 1, right - border );
        if ( visibility >= threshold &&
             top <= y + border && y + border < bottom )
            for ( ; x < to; ++x )
            {
                rgba_20 &back = bitmap[ y * size_x + x ];
                rgba_20 fore = global_alpha *
                    shadow[
                        static_cast< size_t >( y + border - top ) * width +
                        static_cast< size_t >( x + border - left ) ] *
                    shadow_color;
                float mix_fore = operation & 1 ? back.a : 0.0f;
                if ( operation & 2 )
                    mix_fore = 1.0f - mix_fore;
                float mix_back = operation & 4 ? fore.a : 0.0f;
                if ( operation & 8 )
                    mix_back = 1.0f - mix_back;
                rgba_20 blend = mix_fore * fore + mix_back * back;
                blend.a = std::min( blend.a, 1.0f );
                back = visibility * blend + ( 1.0f - visibility ) * back;
            }
        if ( next.y != y )
            sum = 0.0f;
        x = std::max( static_cast< int >( next.x ), left - border );
        y = next.y;
        sum += next.delta;
    }
}

// Render the polylines into the pixel buffer.  It scan-converts the lines
// to runs which represent changes to the signed fractional coverage when
// read from left-to-right, top-to-bottom.  It scans through these to
// determine spans of pixels that need to be drawn, paints those pixels
// according to the brush, and then blends them into the buffer according
// to the current compositing settings.  This is slightly more complicated
// because it interleaves this with a simultaneous scan through a similar
// set of runs representing the current clip mask to determine which pixels
// it can composite into.  Note that shadows are always drawn first.
//
void canvas_20::render_main(
    paint_brush_20 const &brush )
{
    if ( forward.a * forward.d - forward.b * forward.c == 0.0f )
        return;
    render_shadow( brush );
    lines_to_runs( xy( 0.0f, 0.0f ), 0 );
    int operation = global_composite_operation;
    int x = -1;
    int y = -1;
    float path_sum = 0.0f;
    float clip_sum = 0.0f;
    size_t path_index = 0;
    size_t clip_index = 0;
    while ( clip_index < mask.size() )
    {
        bool which = ( path_index < runs.size() &&
                       runs[ path_index ] < mask[ clip_index ] );
        pixel_run next = which ? runs[ path_index ] : mask[ clip_index ];
        float coverage = std::min( fabsf( path_sum ), 1.0f );
        float visibility = std::min( fabsf( clip_sum ), 1.0f );
        int to = next.y == y ? next.x : x + 1;
        static float const threshold = 1.0f / 8160.0f;
        if ( ( coverage >= threshold || ~operation & 8 ) &&
             visibility >= threshold )
            for ( ; x < to; ++x )
            {
                rgba_20 &back = bitmap[ y * size_x + x ];
                rgba_20 fore = coverage * global_alpha *
                    paint_pixel( xy( static_cast< float >( x ) + 0.5f,
                                     static_cast< float >( y ) + 0.5f ),
                                 brush );
                float mix_fore = operation & 1 ? back.a : 0.0f;
                if ( operation & 2 )
                    mix_fore = 1.0f - mix_fore;
                float mix_back = operation & 4 ? fore.a : 0.0f;
                if ( operation & 8 )
                    mix_back = 1.0f - mix_back;
                rgba_20 blend = mix_fore * fore + mix_back * back;
                blend.a = std::min( blend.a, 1.0f );
                back = visibility * blend + ( 1.0f - visibility ) * back;
            }
        x = next.x;
        if ( next.y != y )
        {
            y = next.y;
            path_sum = 0.0f;
            clip_sum = 0.0f;
        }
        if ( which )
            path_sum += runs[ path_index++ ].delta;
        else
            clip_sum += mask[ clip_index++ ].delta;
    }
}

canvas_20::canvas_20(
    int width,
    int height )
    : global_composite_operation( source_over ),
      shadow_offset_x( 0.0f ),
      shadow_offset_y( 0.0f ),
      line_cap( butt ),
      line_join( miter ),
      line_dash_offset( 0.0f ),
      text_align( start ),
      text_baseline( alphabetic ),
      size_x( width ),
      size_y( height ),
      global_alpha( 1.0f ),
      shadow_blur( 0.0f ),
      line_width( 1.0f ),
      miter_limit( 10.0f ),
      fill_brush(),
      stroke_brush(),
      image_brush(),
      face(),
      bitmap( new rgba_20[ width * height ] ),
      saves( 0 )
{
    affine_matrix identity = { 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f };
    forward = identity;
    inverse = identity;
    set_color( fill_style, 
               0.0f, 0.0f, 0.0f, 1.0f, 
               0.0f, 0.0f, 0.0f, 0.0f, 
               0.0f, 0.0f, 0.0f, 0.0f, 
               0.0f, 0.0f, 0.0f, 0.0f, 
               0.0f, 0.0f, 0.0f, 0.0f );
    set_color( stroke_style,
               0.0f, 0.0f, 0.0f, 1.0f, 
               0.0f, 0.0f, 0.0f, 0.0f, 
               0.0f, 0.0f, 0.0f, 0.0f, 
               0.0f, 0.0f, 0.0f, 0.0f, 
               0.0f, 0.0f, 0.0f, 0.0f );
    for ( unsigned short y = 0; y < size_y; ++y )
    {
        pixel_run piece_1 = { 0, y, 1.0f };
        pixel_run piece_2 = { static_cast< unsigned short >( size_x ), y,
                              -1.0f };
        mask.push_back( piece_1 );
        mask.push_back( piece_2 );
    }
}

canvas_20::~canvas_20()
{
    delete[] bitmap;
    while ( canvas_20 *head = saves )
    {
        saves = head->saves;
        head->saves = 0;
        delete head;
    }
}

void canvas_20::scale(
    float x,
    float y )
{
    transform( x, 0.0f, 0.0f, y, 0.0f, 0.0f );
}

void canvas_20::rotate(
    float angle )
{
    float cosine = cosf( angle );
    float sine = sinf( angle );
    transform( cosine, sine, -sine, cosine, 0.0f, 0.0f );
}

void canvas_20::translate(
    float x,
    float y )
{
    transform( 1.0f, 0.0f, 0.0f, 1.0f, x, y );
}

void canvas_20::transform(
    float a,
    float b,
    float c,
    float d,
    float e,
    float f )
{
    set_transform( forward.a * a + forward.c * b,
                   forward.b * a + forward.d * b,
                   forward.a * c + forward.c * d,
                   forward.b * c + forward.d * d,
                   forward.a * e + forward.c * f + forward.e,
                   forward.b * e + forward.d * f + forward.f );
}

void canvas_20::set_transform(
    float a,
    float b,
    float c,
    float d,
    float e,
    float f )
{
    float determinant = a * d - b * c;
    float scaling = determinant != 0.0f ? 1.0f / determinant : 0.0f;
    affine_matrix new_forward = { a, b, c, d, e, f };
    affine_matrix new_inverse = {
        scaling * d, scaling * -b, scaling * -c, scaling * a,
        scaling * ( c * f - d * e ), scaling * ( b * e - a * f ) };
    forward = new_forward;
    inverse = new_inverse;
}

void canvas_20::set_global_alpha(
    float alpha )
{
    if ( 0.0f <= alpha && alpha <= 1.0f )
        global_alpha = alpha;
}

void canvas_20::set_shadow_color(
    float red, float green, float blue, float alpha,
    float data_a, float data_b, float data_c, float data_d,
    float data_e, float data_f, float data_g, float data_h,
    float data_i, float data_j, float data_k, float data_l,
    float data_m, float data_n, float data_o, float data_p )
{
    shadow_color = premultiplied( linearized( clamped(
        rgba_20( red, green, blue, alpha,
              data_a, data_b, data_c, data_d,
              data_e, data_f, data_g, data_h,
              data_i, data_j, data_k, data_l,
              data_m, data_n, data_o, data_p )
        ) ) );
}

void canvas_20::set_shadow_blur(
    float level )
{
    if ( 0.0f <= level )
        shadow_blur = level;
}

void canvas_20::set_line_width(
    float width )
{
    if ( 0.0f < width )
        line_width = width;
}

void canvas_20::set_miter_limit(
    float limit )
{
    if ( 0.0f < limit )
        miter_limit = limit;
}

void canvas_20::set_line_dash(
    float const *segments,
    int count )
{
    for ( int index = 0; index < count; ++index )
        if ( segments && segments[ index ] < 0.0f )
            return;
    line_dash.clear();
    if ( !segments )
        return;
    for ( int index = 0; index < count; ++index )
        line_dash.push_back( segments[ index ] );
    if ( count & 1 )
        for ( int index = 0; index < count; ++index )
            line_dash.push_back( segments[ index ] );
}

void canvas_20::set_color(
    brush_type type,
    float red, float green, float blue, float alpha,
    float data_a, float data_b, float data_c, float data_d,
    float data_e, float data_f, float data_g, float data_h,
    float data_i, float data_j, float data_k, float data_l,
    float data_m, float data_n, float data_o, float data_p )
{
    paint_brush_20 &brush = type == fill_style ? fill_brush : stroke_brush;
    brush.type = paint_brush_20::color;
    brush.colors.clear();
    brush.colors.push_back( premultiplied( linearized( clamped(
        rgba_20( red, green, blue, alpha,
              data_a, data_b, data_c, data_d,
              data_e, data_f, data_g, data_h,
              data_i, data_j, data_k, data_l,
              data_m, data_n, data_o, data_p ) 
        ) ) ) );
}

void canvas_20::set_data_color( brush_type type, const rgba_20& data )
{
    paint_brush_20 &brush = type == fill_style ? fill_brush : stroke_brush;
    brush.type = paint_brush_20::color;
    brush.colors.clear();
    brush.colors.push_back(data);
};

void canvas_20::set_linear_gradient(
    brush_type type,
    float start_x,
    float start_y,
    float end_x,
    float end_y )
{
    paint_brush_20 &brush = type == fill_style ? fill_brush : stroke_brush;
    brush.type = paint_brush_20::linear;
    brush.colors.clear();
    brush.stops.clear();
    brush.start = xy( start_x, start_y );
    brush.end = xy( end_x, end_y );
}

void canvas_20::set_radial_gradient(
    brush_type type,
    float start_x,
    float start_y,
    float start_radius,
    float end_x,
    float end_y,
    float end_radius )
{
    if ( start_radius < 0.0f || end_radius < 0.0f )
        return;
    paint_brush_20 &brush = type == fill_style ? fill_brush : stroke_brush;
    brush.type = paint_brush_20::radial;
    brush.colors.clear();
    brush.stops.clear();
    brush.start = xy( start_x, start_y );
    brush.end = xy( end_x, end_y );
    brush.start_radius = start_radius;
    brush.end_radius = end_radius;
}

void canvas_20::add_color_stop(
    brush_type type,
    float offset,
    float red, float green, float blue, float alpha,
    float data_a, float data_b, float data_c, float data_d,
    float data_e, float data_f, float data_g, float data_h,
    float data_i, float data_j, float data_k, float data_l,
    float data_m, float data_n, float data_o, float data_p )
{
    paint_brush_20 &brush = type == fill_style ? fill_brush : stroke_brush;
    if ( ( brush.type != paint_brush_20::linear &&
           brush.type != paint_brush_20::radial ) ||
         offset < 0.0f || 1.0f < offset )
        return;
    ptrdiff_t index = std::upper_bound(
        brush.stops.begin(), brush.stops.end(), offset ) -
        brush.stops.begin();
    rgba_20 color = linearized( clamped( rgba_20( 
            red, green, blue, alpha,
            data_a, data_b, data_c, data_d,
            data_e, data_f, data_g, data_h,
            data_i, data_j, data_k, data_l,
            data_m, data_n, data_o, data_p
        ) ) );
    brush.colors.insert( brush.colors.begin() + index, color );
    brush.stops.insert( brush.stops.begin() + index, offset );
}

void canvas_20::set_pattern(
    brush_type type,
    unsigned char const *image,
    int width,
    int height,
    int stride,
    repetition_style repetition )
{
    if ( !image || width <= 0 || height <= 0 )
        return;
    paint_brush_20 &brush = type == fill_style ? fill_brush : stroke_brush;
    brush.type = paint_brush_20::pattern;
    brush.colors.clear();
    for ( int y = 0; y < height; ++y )
        for ( int x = 0; x < width; ++x )
        {
            int index = y * stride + x * 4;
            rgba_20 color = rgba_20(
                image[ index + 0 ] / 255.0f, image[ index + 1 ] / 255.0f,
                image[ index + 2 ] / 255.0f, image[ index + 3 ] / 255.0f,
                image[ index + 4 ] / 255.0f, image[ index + 5 ] / 255.0f,
                image[ index + 6 ] / 255.0f, image[ index + 7 ] / 255.0f,
                image[ index + 8 ] / 255.0f, image[ index + 9 ] / 255.0f,
                image[ index + 10 ] / 255.0f, image[ index + 11 ] / 255.0f,
                image[ index + 12 ] / 255.0f, image[ index + 13 ] / 255.0f,
                image[ index + 14 ] / 255.0f, image[ index + 15 ] / 255.0f,
                image[ index + 16 ] / 255.0f, image[ index + 17 ] / 255.0f,
                image[ index + 18 ] / 255.0f, image[ index + 19 ] / 255.0f );
            brush.colors.push_back( premultiplied( linearized( color ) ) );
        }
    brush.width = width;
    brush.height = height;
    brush.repetition = repetition;
}

void canvas_20::begin_path()
{
    path.points.clear();
    path.subpaths.clear();
}

void canvas_20::move_to(
    float x,
    float y )
{
    if ( !path.subpaths.empty() && path.subpaths.back().count == 1 )
    {
        path.points.back() = forward * xy( x, y );
        return;
    }
    subpath_data subpath = { 1, false };
    path.points.push_back( forward * xy( x, y ) );
    path.subpaths.push_back( subpath );
}

void canvas_20::close_path()
{
    if ( path.subpaths.empty() )
        return;
    xy first = path.points[ path.points.size() - path.subpaths.back().count ];
    affine_matrix saved_forward = forward;
    affine_matrix identity = { 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f };
    forward = identity;
    line_to( first.x, first.y );
    path.subpaths.back().closed = true;
    move_to( first.x, first.y );
    forward = saved_forward;
}

void canvas_20::line_to(
    float x,
    float y )
{
    if ( path.subpaths.empty() )
    {
        move_to( x, y );
        return;
    }
    xy point_1 = path.points.back();
    xy point_2 = forward * xy( x, y );
    if ( dot( point_2 - point_1, point_2 - point_1 ) == 0.0f )
        return;
    path.points.push_back( point_1 );
    path.points.push_back( point_2 );
    path.points.push_back( point_2 );
    path.subpaths.back().count += 3;
}

void canvas_20::quadratic_curve_to(
    float control_x,
    float control_y,
    float x,
    float y )
{
    if ( path.subpaths.empty() )
        move_to( control_x, control_y );
    xy point_1 = path.points.back();
    xy control = forward * xy( control_x, control_y );
    xy point_2 = forward * xy( x, y );
    xy control_1 = lerp( point_1, control, 2.0f / 3.0f );
    xy control_2 = lerp( point_2, control, 2.0f / 3.0f );
    path.points.push_back( control_1 );
    path.points.push_back( control_2 );
    path.points.push_back( point_2 );
    path.subpaths.back().count += 3;
}

void canvas_20::bezier_curve_to(
    float control_1_x,
    float control_1_y,
    float control_2_x,
    float control_2_y,
    float x,
    float y )
{
    if ( path.subpaths.empty() )
        move_to( control_1_x, control_1_y );
    xy control_1 = forward * xy( control_1_x, control_1_y );
    xy control_2 = forward * xy( control_2_x, control_2_y );
    xy point_2 = forward * xy( x, y );
    path.points.push_back( control_1 );
    path.points.push_back( control_2 );
    path.points.push_back( point_2 );
    path.subpaths.back().count += 3;
}

void canvas_20::arc_to(
    float vertex_x,
    float vertex_y,
    float x,
    float y,
    float radius )
{
    if ( radius < 0.0f ||
         forward.a * forward.d - forward.b * forward.c == 0.0f )
        return;
    if ( path.subpaths.empty() )
        move_to( vertex_x, vertex_y );
    xy point_1 = inverse * path.points.back();
    xy vertex = xy( vertex_x, vertex_y );
    xy point_2 = xy( x, y );
    xy edge_1 = normalized( point_1 - vertex );
    xy edge_2 = normalized( point_2 - vertex );
    float sine = fabsf( dot( perpendicular( edge_1 ), edge_2 ) );
    static float const epsilon = 1.0e-4f;
    if ( sine < epsilon )
    {
        line_to( vertex_x, vertex_y );
        return;
    }
    xy offset = radius / sine * ( edge_1 + edge_2 );
    xy center = vertex + offset;
    float angle_1 = direction( dot( offset, edge_1 ) * edge_1 - offset );
    float angle_2 = direction( dot( offset, edge_2 ) * edge_2 - offset );
    bool reverse = static_cast< int >(
        floorf( ( angle_2 - angle_1 ) / 3.14159265f ) ) & 1;
    arc( center.x, center.y, radius, angle_1, angle_2, reverse );
}

void canvas_20::arc(
    float x,
    float y,
    float radius,
    float start_angle,
    float end_angle,
    bool counter_clockwise )
{
    if ( radius < 0.0f )
        return;
    static float const tau = 6.28318531f;
    float winding = counter_clockwise ? -1.0f : 1.0f;
    float from = fmodf( start_angle, tau );
    float span = fmodf( end_angle, tau ) - from;
    if ( ( end_angle - start_angle ) * winding >= tau )
        span = tau * winding;
    else if ( span * winding < 0.0f )
        span += tau * winding;
    xy centered_1 = radius * xy( cosf( from ), sinf( from ) );
    line_to( x + centered_1.x, y + centered_1.y );
    if ( span == 0.0f )
        return;
    int steps = static_cast< int >(
        std::max( 1.0f, roundf( 16.0f / tau * span * winding ) ) );
    float segment = span / static_cast< float >( steps );
    float alpha = 4.0f / 3.0f * tanf( 0.25f * segment );
    for ( int step = 0; step < steps; ++step )
    {
        float angle = from + static_cast< float >( step + 1 ) * segment;
        xy centered_2 = radius * xy( cosf( angle ), sinf( angle ) );
        xy point_1 = xy( x, y ) + centered_1;
        xy point_2 = xy( x, y ) + centered_2;
        xy control_1 = point_1 + alpha * perpendicular( centered_1 );
        xy control_2 = point_2 - alpha * perpendicular( centered_2 );
        bezier_curve_to( control_1.x, control_1.y,
                         control_2.x, control_2.y,
                         point_2.x, point_2.y );
        centered_1 = centered_2;
    }
}

void canvas_20::rectangle(
    float x,
    float y,
    float width,
    float height )
{
    move_to( x, y );
    line_to( x + width, y );
    line_to( x + width, y + height );
    line_to( x, y + height );
    close_path();
}

void canvas_20::fill()
{
    path_to_lines( false );
    render_main( fill_brush );
}

void canvas_20::stroke()
{
    path_to_lines( true );
    stroke_lines();
    render_main( stroke_brush );
}

void canvas_20::clip()
{
    path_to_lines( false );
    lines_to_runs( xy( 0.0f, 0.0f ), 0 );
    size_t part = runs.size();
    runs.insert( runs.end(), mask.begin(), mask.end() );
    mask.clear();
    int y = -1;
    float last = 0.0f;
    float sum_1 = 0.0f;
    float sum_2 = 0.0f;
    size_t index_1 = 0;
    size_t index_2 = part;
    while ( index_1 < part && index_2 < runs.size() )
    {
        bool which = runs[ index_1 ] < runs[ index_2 ];
        pixel_run next = which ? runs[ index_1 ] : runs[ index_2 ];
        if ( next.y != y )
        {
            y = next.y;
            last = 0.0f;
            sum_1 = 0.0f;
            sum_2 = 0.0f;
        }
        if ( which )
            sum_1 += runs[ index_1++ ].delta;
        else
            sum_2 += runs[ index_2++ ].delta;
        float visibility = ( std::min( fabsf( sum_1 ), 1.0f ) *
                             std::min( fabsf( sum_2 ), 1.0f ) );
        if ( visibility == last )
            continue;
        if ( !mask.empty() &&
             mask.back().x == next.x && mask.back().y == next.y )
            mask.back().delta += visibility - last;
        else
        {
            pixel_run piece = { next.x, next.y, visibility - last };
            mask.push_back( piece );
        }
        last = visibility;
    }
}

bool canvas_20::is_point_in_path(
    float x,
    float y )
{
    path_to_lines( false );
    int winding = 0;
    size_t subpath = 0;
    size_t beginning = 0;
    size_t ending = 0;
    for ( size_t index = 0; index < lines.points.size(); ++index )
    {
        while ( index >= ending )
        {
            beginning = ending;
            ending += lines.subpaths[ subpath++ ].count;
        }
        xy from = lines.points[ index ];
        xy to = lines.points[ index + 1 < ending ? index + 1 : beginning ];
        if ( ( from.y < y && y <= to.y ) || ( to.y < y && y <= from.y ) )
        {
            float side = dot( perpendicular( to - from ), xy( x, y ) - from );
            if ( side == 0.0f )
                return true;
            winding += side > 0.0f ? 1 : -1;
        }
        else if ( from.y == y && y == to.y &&
                  ( ( from.x <= x && x <= to.x ) ||
                    ( to.x <= x && x <= from.x ) ) )
            return true;
    }
    return winding;
}

void canvas_20::clear_rectangle(
    float x,
    float y,
    float width,
    float height )
{
    composite_operation saved_operation = global_composite_operation;
    float saved_global_alpha = global_alpha;
    float saved_alpha = shadow_color.a;
    paint_brush_20::types saved_type = fill_brush.type;
    global_composite_operation = destination_out;
    global_alpha = 1.0f;
    shadow_color.a = 0.0f;
    fill_brush.type = paint_brush_20::color;
    fill_rectangle( x, y, width, height );
    fill_brush.type = saved_type;
    shadow_color.a = saved_alpha;
    global_alpha = saved_global_alpha;
    global_composite_operation = saved_operation;
}

void canvas_20::fill_rectangle(
    float x,
    float y,
    float width,
    float height )
{
    if ( width == 0.0f || height == 0.0f )
        return;
    lines.points.clear();
    lines.subpaths.clear();
    lines.points.push_back( forward * xy( x, y ) );
    lines.points.push_back( forward * xy( x + width, y ) );
    lines.points.push_back( forward * xy( x + width, y + height ) );
    lines.points.push_back( forward * xy( x, y + height ) );
    subpath_data entry = { 4, true };
    lines.subpaths.push_back( entry );
    render_main( fill_brush );
}

void canvas_20::stroke_rectangle(
    float x,
    float y,
    float width,
    float height )
{
    if ( width == 0.0f && height == 0.0f )
        return;
    lines.points.clear();
    lines.subpaths.clear();
    if ( width == 0.0f || height == 0.0f )
    {
        lines.points.push_back( forward * xy( x, y ) );
        lines.points.push_back( forward * xy( x + width, y + height ) );
        subpath_data entry = { 2, false };
        lines.subpaths.push_back( entry );
    }
    else
    {
        lines.points.push_back( forward * xy( x, y ) );
        lines.points.push_back( forward * xy( x + width, y ) );
        lines.points.push_back( forward * xy( x + width, y + height ) );
        lines.points.push_back( forward * xy( x, y + height ) );
        lines.points.push_back( forward * xy( x, y ) );
        subpath_data entry = { 5, true };
        lines.subpaths.push_back( entry );
    }
    stroke_lines();
    render_main( stroke_brush );
}

bool canvas_20::set_font(
    unsigned char const *font,
    int bytes,
    float size )
{
    if ( font && bytes )
    {
        face.data.clear();
        face.cmap = 0;
        face.glyf = 0;
        face.head = 0;
        face.hhea = 0;
        face.hmtx = 0;
        face.loca = 0;
        face.maxp = 0;
        face.os_2 = 0;
        if ( bytes < 6 )
            return false;
        int version = ( font[ 0 ] << 24 | font[ 1 ] << 16 |
                        font[ 2 ] <<  8 | font[ 3 ] <<  0 );
        int tables = font[ 4 ] << 8 | font[ 5 ];
        if ( ( version != 0x00010000 && version != 0x74727565 ) ||
             bytes < tables * 16 + 12 )
            return false;
        face.data.insert( face.data.end(), font, font + tables * 16 + 12 );
        for ( int index = 0; index < tables; ++index )
        {
            int tag = signed_32( face.data, index * 16 + 12 );
            int offset = signed_32( face.data, index * 16 + 20 );
            int span = signed_32( face.data, index * 16 + 24 );
            if ( bytes < offset + span )
            {
                face.data.clear();
                return false;
            }
            int place = static_cast< int >( face.data.size() );
            if ( tag == 0x636d6170 )
                face.cmap = place;
            else if ( tag == 0x676c7966 )
                face.glyf = place;
            else if ( tag == 0x68656164 )
                face.head = place;
            else if ( tag == 0x68686561 )
                face.hhea = place;
            else if ( tag == 0x686d7478 )
                face.hmtx = place;
            else if ( tag == 0x6c6f6361 )
                face.loca = place;
            else if ( tag == 0x6d617870 )
                face.maxp = place;
            else if ( tag == 0x4f532f32 )
                face.os_2 = place;
            else
                continue;
            face.data.insert(
                face.data.end(), font + offset, font + offset + span );
        }
        if ( !face.cmap || !face.glyf || !face.head || !face.hhea ||
             !face.hmtx || !face.loca || !face.maxp || !face.os_2 )
        {
            face.data.clear();
            return false;
        }
    }
    if ( face.data.empty() )
        return false;
    int units_per_em = unsigned_16( face.data, face.head + 18 );
    face.scale = size / static_cast< float >( units_per_em );
    return true;
}

void canvas_20::fill_text(
    char const *text,
    float x,
    float y,
    float maximum_width )
{
    text_to_lines( text, xy( x, y ), maximum_width, false );
    render_main( fill_brush );
}

void canvas_20::stroke_text(
    char const *text,
    float x,
    float y,
    float maximum_width )
{
    text_to_lines( text, xy( x, y ), maximum_width, true );
    stroke_lines();
    render_main( stroke_brush );
}

float canvas_20::measure_text(
    char const *text )
{
    if ( face.data.empty() || !text )
        return 0.0f;
    int hmetrics = unsigned_16( face.data, face.hhea + 34 );
    int width = 0;
    for ( int index = 0; text[ index ]; )
    {
        int glyph = character_to_glyph( text, index );
        int entry = std::min( glyph, hmetrics - 1 );
        width += unsigned_16( face.data, face.hmtx + entry * 4 );
    }
    return static_cast< float >( width ) * face.scale;
}

void canvas_20::draw_image(
    unsigned char const *image,
    int width,
    int height,
    int stride,
    float x,
    float y,
    float to_width,
    float to_height )
{
    if ( !image || width <= 0 || height <= 0 ||
         to_width == 0.0f || to_height == 0.0f )
        return;
    std::swap( fill_brush, image_brush );
    set_pattern( fill_style, image, width, height, stride, repeat );
    std::swap( fill_brush, image_brush );
    lines.points.clear();
    lines.subpaths.clear();
    lines.points.push_back( forward * xy( x, y ) );
    lines.points.push_back( forward * xy( x + to_width, y ) );
    lines.points.push_back( forward * xy( x + to_width, y + to_height ) );
    lines.points.push_back( forward * xy( x, y + to_height ) );
    subpath_data entry = { 4, true };
    lines.subpaths.push_back( entry );
    affine_matrix saved_forward = forward;
    affine_matrix saved_inverse = inverse;
    translate( x + std::min( 0.0f, to_width ),
               y + std::min( 0.0f, to_height ) );
    scale( fabsf( to_width ) / static_cast< float >( width ),
           fabsf( to_height ) / static_cast< float >( height ) );
    render_main( image_brush );
    forward = saved_forward;
    inverse = saved_inverse;
}

void canvas_20::get_image_data(
    unsigned char *image,
    int width,
    int height,
    int stride,
    int x,
    int y )
{
    if ( !image )
        return;
    static float const bayer[][ 4 ] = {
        { 0.03125f, 0.53125f, 0.15625f, 0.65625f },
        { 0.78125f, 0.28125f, 0.90625f, 0.40625f },
        { 0.21875f, 0.71875f, 0.09375f, 0.59375f },
        { 0.96875f, 0.46875f, 0.84375f, 0.34375f } };
    for ( int image_y = 0; image_y < height; ++image_y )
        for ( int image_x = 0; image_x < width; ++image_x )
        {
            int index = image_y * stride + image_x * 4;
            int canvas_x = x + image_x;
            int canvas_y = y + image_y;
            rgba_20 color = zero();
            if ( 0 <= canvas_x && canvas_x < size_x &&
                 0 <= canvas_y && canvas_y < size_y )
                color = bitmap[ canvas_y * size_x + canvas_x ];
            float threshold = bayer[ canvas_y & 3 ][ canvas_x & 3 ];
            color = rgba_20( threshold, threshold, threshold, threshold,
                          threshold, threshold, threshold, threshold,
                          threshold, threshold, threshold, threshold,
                          threshold, threshold, threshold, threshold,
                          threshold, threshold, threshold, threshold ) +
                255.0f * delinearized( clamped( unpremultiplied( color ) ) );
            image[ index + 0 ] = static_cast< unsigned char >( color.r );
            image[ index + 1 ] = static_cast< unsigned char >( color.g );
            image[ index + 2 ] = static_cast< unsigned char >( color.b );
            image[ index + 3 ] = static_cast< unsigned char >( color.a );
        }
}

void canvas_20::put_image_data(
    unsigned char const *image,
    int width,
    int height,
    int stride,
    int x,
    int y )
{
    if ( !image )
        return;
    for ( int image_y = 0; image_y < height; ++image_y )
        for ( int image_x = 0; image_x < width; ++image_x )
        {
            int index = image_y * stride + image_x * 4;
            int canvas_x = x + image_x;
            int canvas_y = y + image_y;
            if ( canvas_x < 0 || size_x <= canvas_x ||
                 canvas_y < 0 || size_y <= canvas_y )
                continue;
            rgba_20 color = rgba_20(
                image[ index + 0 ] / 255.0f, image[ index + 1 ] / 255.0f,
                image[ index + 2 ] / 255.0f, image[ index + 3 ] / 255.0f,
                image[ index + 4 ] / 255.0f, image[ index + 5 ] / 255.0f,
                image[ index + 6 ] / 255.0f, image[ index + 7 ] / 255.0f,
                image[ index + 8 ] / 255.0f, image[ index + 9 ] / 255.0f,
                image[ index + 10 ] / 255.0f, image[ index + 11 ] / 255.0f,
                image[ index + 12 ] / 255.0f, image[ index + 13 ] / 255.0f,
                image[ index + 14 ] / 255.0f, image[ index + 15 ] / 255.0f,
                image[ index + 16 ] / 255.0f, image[ index + 17 ] / 255.0f,
                image[ index + 18 ] / 255.0f, image[ index + 19 ] / 255.0f );
            bitmap[ canvas_y * size_x + canvas_x ] = premultiplied( linearized( color ) );
        }
}

void canvas_20::save()
{
    canvas_20 *state = new canvas_20( 0, 0 );
    state->global_composite_operation = global_composite_operation;
    state->shadow_offset_x = shadow_offset_x;
    state->shadow_offset_y = shadow_offset_y;
    state->line_cap = line_cap;
    state->line_join = line_join;
    state->line_dash_offset = line_dash_offset;
    state->text_align = text_align;
    state->text_baseline = text_baseline;
    state->forward = forward;
    state->inverse = inverse;
    state->global_alpha = global_alpha;
    state->shadow_color = shadow_color;
    state->shadow_blur = shadow_blur;
    state->line_width = line_width;
    state->miter_limit = miter_limit;
    state->line_dash = line_dash;
    state->fill_brush = fill_brush;
    state->stroke_brush = stroke_brush;
    state->mask = mask;
    state->face = face;
    state->saves = saves;
    saves = state;
}

void canvas_20::restore()
{
    if ( !saves )
        return;
    canvas_20 *state = saves;
    global_composite_operation = state->global_composite_operation;
    shadow_offset_x = state->shadow_offset_x;
    shadow_offset_y = state->shadow_offset_y;
    line_cap = state->line_cap;
    line_join = state->line_join;
    line_dash_offset = state->line_dash_offset;
    text_align = state->text_align;
    text_baseline = state->text_baseline;
    forward = state->forward;
    inverse = state->inverse;
    global_alpha = state->global_alpha;
    shadow_color = state->shadow_color;
    shadow_blur = state->shadow_blur;
    line_width = state->line_width;
    miter_limit = state->miter_limit;
    line_dash = state->line_dash;
    fill_brush = state->fill_brush;
    stroke_brush = state->stroke_brush;
    mask = state->mask;
    face = state->face;
    saves = state->saves;
    state->saves = 0;
    delete state;
}

}
