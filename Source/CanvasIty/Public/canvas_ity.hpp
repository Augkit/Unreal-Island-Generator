// canvas_ity v1.00 -- ISC license
// Copyright (c) 2022 Andrew Kensler
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice appear in all copies.
//
// THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
// WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
// ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
// WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
// ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
// OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

// ======== ABOUT ========
//
// This is a tiny, single-header C++ library for rasterizing immediate-mode
// 2D vector graphics, closely modeled on the basic W3C (not WHATWG) HTML5 2D
// canvas specification (https://www.w3.org/TR/2015/REC-2dcontext-20151119/).
//
// The priorities for this library are high-quality rendering, ease of use,
// and compact size.  Speed is important too, but secondary to the other
// priorities.  Notably, this library takes an opinionated approach and
// does not provide options for trading off quality for speed.
//
// Despite its small size, it supports nearly everything listed in the W3C
// HTML5 2D canvas specification, except for hit regions and getting certain
// properties.  The main differences lie in the surface-level API to make this
// easier for C++ use, while the underlying implementation is carefully based
// on the specification.  In particular, stroke, fill, gradient, pattern,
// image, and font styles are specified slightly differently (avoiding strings
// and auxiliary classes).  Nonetheless, the goal is that this library could
// produce a conforming HTML5 2D canvas implementation if wrapped in a thin
// layer of JavaScript bindings.  See the accompanying C++ automated test
// suite and its HTML5 port for a mapping between the APIs and a comparison
// of this library's rendering output against browser canvas implementations.

// ======== FEATURES ========
//
// High-quality rendering:
//
// - Trapezoidal area antialiasing provides very smooth antialiasing, even
//     when lines are nearly horizontal or vertical.
// - Gamma-correct blending, interpolation, and resampling are used
//     throughout.  It linearizes all colors and premultiplies alpha on
//     input and converts back to unpremultiplied sRGB on output.  This
//     reduces muddiness on many gradients (e.g., red to green), makes line
//     thicknesses more perceptually uniform, and avoids dark fringes when
//     interpolating opacity.
// - Bicubic convolution resampling is used whenever it needs to resample a
//     pattern or image.  This smoothly interpolates with less blockiness when
//     magnifying, and antialiases well when minifying.  It can simultaneously
//     magnify and minify along different axes.
// - Ordered dithering is used on output.  This reduces banding on subtle
//     gradients while still being compression-friendly.
// - High curvature is handled carefully in line joins.  Thick lines are
//     drawn correctly as though tracing with a wide pen nib, even where
//     the lines curve sharply.  (Simpler curve offsetting approaches tend
//     to show bite-like artifacts in these cases.)
//
// Ease of use:
//
// - Provided as a single-header library with no dependencies beside the
//     standard C++ library.  There is nothing to link, and it even includes
//     built-in binary parsing for TrueType font (TTF) files.  It is pure CPU
//     code and does not require a GPU or GPU context.
// - Has extensive Doxygen-style documentation comments for the public API.
// - Compiles cleanly at moderately high warning levels on most compilers.
// - Shares no internal pointers, nor holds any external pointers.  Newcomers
//     to C++ can have fun drawing with this library without worrying so much
//     about resource lifetimes or mutability.
// - Uses no static or global variables.  Threads may safely work with
//     different canvas instances concurrently without locking.
// - Allocates no dynamic memory after reaching the high-water mark.  Except
//     for the pixel buffer, flat std::vector instances embedded in the canvas
//     instance handle all dynamic memory.  This reduces fragmentation and
//     makes it easy to change the code to reserve memory up front or even
//     to use statically allocated vectors.
// - Works with exceptions and RTTI disabled.
// - Intentionally uses a plain C++03 style to make it as widely portable
//     as possible, easier to understand, and (with indexing preferred over
//     pointer arithmetic) easier to port natively to other languages.  The
//     accompanying test suite may also help with porting.
//
// Compact size:
//
// - The source code for the entire library consists of a bit over 2300 lines
//     (not counting comments or blanks), each no longer than 78 columns.
//     Alternately measured, it has fewer than 1300 semicolons.
// - The object code for the library can be less than 36 KiB on x86-64 with
//     appropriate compiler settings for size.
// - Due to the library's small size, the accompanying automated test suite
//     achieves 100% line coverage of the library in gcov and llvm-cov.

// ======== LIMITATIONS ========
//
// - Trapezoidal antialiasing overestimates coverage where paths self-
//     intersect within a single pixel.  Where inner joins are visible, this
//     can lead to a "grittier" appearance due to the extra windings used.
// - Clipping uses an antialiased sparse pixel mask rather than geometrically
//     intersecting paths.  Therefore, it is not subpixel-accurate.
// - Text rendering is extremely basic and mainly for convenience.  It only
//     supports left-to-right text, and does not do any hinting, kerning,
//     ligatures, text shaping, or text layout.  If you require any of those,
//     consider using another library to provide those and feed the results
//     to this library as either placed glyphs or raw paths.
// - TRUETYPE FONT PARSING IS NOT SECURE!  It does some basic validity
//     checking, but should only be used with known-good or sanitized fonts.
// - Parameter checking does not test for non-finite floating-point values.
// - Rendering is single-threaded, not explicitly vectorized, and not GPU-
//     accelerated.  It also copies data to avoid ownership issues.  If you
//     need the speed, you are better off using a more fully-featured library.
// - The library does no input or output on its own.  Instead, you must
//     provide it with buffers to copy into or out of.

// ======== USAGE ========
//
// This is a single-header library.  You may freely include it in any of
// your source files to declare the canvas_ity namespace and its members.
// However, to get the implementation, you must
//     #define CANVAS_ITY_IMPLEMENTATION
// in exactly one C++ file before including this header.
//
// Then, construct an instance of the canvas_ity::canvas class with the pixel
// dimensions that you want and draw into it using any of the various drawing
// functions.  You can then use the get_image_data() function to retrieve the
// currently drawn image at any time.
//
// See each of the public member function and data member (i.e., method
// and field) declarations for the full API documentation.  Also see the
// accompanying C++ automated test suite for examples of the usage of each
// public member, and the test suite's HTML5 port for how these map to the
// HTML5 canvas API.
#pragma once

#include <cstddef>
#include <vector>

namespace canvas_ity
{

// Public API enums
enum CANVASITY_API composite_operation {
    source_in = 1, source_copy, source_out, destination_in,
    destination_atop = 7, lighter = 10, destination_over, destination_out,
    source_atop, source_over, exclusive_or };
enum CANVASITY_API cap_style { butt, square, circle };
enum CANVASITY_API join_style { miter, bevel, rounded };
enum CANVASITY_API brush_type { fill_style, stroke_style };
enum CANVASITY_API repetition_style { repeat, repeat_x, repeat_y, no_repeat };
enum CANVASITY_API align_style { leftward, rightward, center, start = 0, ending };
enum CANVASITY_API baseline_style {
    alphabetic, top, middle, bottom, hanging, ideographic = 3 };

// Implementation details
struct CANVASITY_API xy { float x, y; xy(); xy( float, float ); };
struct CANVASITY_API rgba { float r, g, b, a; rgba(); rgba( float, float, float, float ); };
struct CANVASITY_API rgba_20 { 
    float r, g, b, a, d_a, d_b, d_c, d_d, d_e, d_f, d_g, d_h, d_i, d_j, d_k, d_l, d_m, d_n, d_o, d_p; 
    rgba_20(); 
    rgba_20( float, float, float, float,
          float, float, float, float,
          float, float, float, float,
          float, float, float, float,
          float, float, float, float );
};
struct CANVASITY_API affine_matrix { float a, b, c, d, e, f; };
struct CANVASITY_API paint_brush { enum types { color, linear, radial, pattern } type;
                     std::vector< rgba > colors; std::vector< float > stops;
                     xy start, end; float start_radius, end_radius;
                     int width, height; repetition_style repetition; };
struct CANVASITY_API paint_brush_20 { enum types { color, linear, radial, pattern } type;
                     std::vector< rgba_20 > colors; std::vector< float > stops;
                     xy start, end; float start_radius, end_radius;
                     int width, height; repetition_style repetition; };
struct CANVASITY_API font_face { std::vector< unsigned char > data;
                   int cmap, glyf, head, hhea, hmtx, loca, maxp, os_2;
                   float scale; };
struct CANVASITY_API subpath_data { size_t count; bool closed; };
struct CANVASITY_API bezier_path { std::vector< xy > points;
                     std::vector< subpath_data > subpaths; };
struct CANVASITY_API line_path { std::vector< xy > points;
                   std::vector< subpath_data > subpaths; };
struct CANVASITY_API pixel_run { unsigned short x, y; float delta; };
typedef std::vector< pixel_run > pixel_runs;

class CANVASITY_API canvas
{
public:

    // ======== LIFECYCLE ========

    /// @brief  Construct a new canvas.
    ///
    /// It will begin with all pixels set to transparent black.  Initially,
    /// the visible coordinates will run from (0, 0) in the upper-left to
    /// (width, height) in the lower-right and with pixel centers offset
    /// (0.5, 0.5) from the integer grid, though all this may be changed
    /// by transforms.  The sizes must be between 1 and 32768, inclusive.
    ///
    /// @param width  horizontal size of the new canvas in pixels
    /// @param height  vertical size of the new canvas in pixels
    ///
    canvas(
        int width,
        int height );

    /// @brief  Destroy the canvas and release all associated memory.
    ///
    ~canvas();

    // ======== TRANSFORMS ========

    /// @brief  Scale the current transform.
    ///
    /// Scaling may be non-uniform if the x and y scaling factors are
    /// different.  When a scale factor is less than one, things will be
    /// shrunk in that direction.  When a scale factor is greater than
    /// one, they will grow bigger.  Negative scaling factors will flip or
    /// mirror it in that direction.  The scaling factors must be non-zero.
    /// If either is zero, most drawing operations will do nothing.
    ///
    /// @param x  horizontal scaling factor
    /// @param y  vertical scaling factor
    ///
    void scale(
        float x,
        float y );

    /// @brief  Rotate the current transform.
    ///
    /// The rotation is applied clockwise in a direction around the origin.
    ///
    /// Tip: to rotate around another point, first translate that point to
    ///      the origin, then do the rotation, and then translate back.
    ///
    /// @param angle  clockwise angle in radians
    ///
    void rotate(
        float angle );

    /// @brief  Translate the current transform.
    ///
    /// By default, positive x values shift that many pixels to the right,
    /// while negative y values shift left, positive y values shift up, and
    /// negative y values shift down.
    ///
    /// @param x  amount to shift horizontally
    /// @param y  amount to shift vertically
    ///
    void translate(
        float x,
        float y );

    /// @brief  Add an arbitrary transform to the current transform.
    ///
    /// This takes six values for the upper two rows of a homogenous 3x3
    /// matrix (i.e., {{a, c, e}, {b, d, f}, {0.0, 0.0, 1.0}}) describing an
    /// arbitrary affine transform and appends it to the current transform.
    /// The values can represent any affine transform such as scaling,
    /// rotation, translation, or skew, or any composition of affine
    /// transforms.  The matrix must be invertible.  If it is not, most
    /// drawing operations will do nothing.
    ///
    /// @param a  horizontal scaling factor (m11)
    /// @param b  vertical skewing (m12)
    /// @param c  horizontal skewing (m21)
    /// @param d  vertical scaling factor (m22)
    /// @param e  horizontal translation (m31)
    /// @param f  vertical translation (m32)
    ///
    void transform(
        float a,
        float b,
        float c,
        float d,
        float e,
        float f );

    /// @brief  Replace the current transform.
    ///
    /// This takes six values for the upper two rows of a homogenous 3x3
    /// matrix (i.e., {{a, c, e}, {b, d, f}, {0.0, 0.0, 1.0}}) describing
    /// an arbitrary affine transform and replaces the current transform
    /// with it.  The values can represent any affine transform such as
    /// scaling, rotation, translation, or skew, or any composition of
    /// affine transforms.  The matrix must be invertible.  If it is not,
    /// most drawing operations will do nothing.
    ///
    /// Tip: to reset the current transform back to the default, use
    ///      1.0, 0.0, 0.0, 1.0, 0.0, 0.0.
    ///
    /// @param a  horizontal scaling factor (m11)
    /// @param b  vertical skewing (m12)
    /// @param c  horizontal skewing (m21)
    /// @param d  vertical scaling factor (m22)
    /// @param e  horizontal translation (m31)
    /// @param f  vertical translation (m32)
    ///
    void set_transform(
        float a,
        float b,
        float c,
        float d,
        float e,
        float f );

    // ======== COMPOSITING ========

    /// @brief  Set the degree of opacity applied to all drawing operations.
    ///
    /// If an operation already uses a transparent color, this can make it
    /// yet more transparent.  It must be in the range from 0.0 for fully
    /// transparent to 1.0 for fully opaque, inclusive.  If it is not, this
    /// does nothing.  Defaults to 1.0 (opaque).
    ///
    /// @param alpha  degree of opacity applied to all drawing operations
    ///
    void set_global_alpha(
        float alpha );

    /// @brief  Compositing operation for blending new drawing and old pixels.
    ///
    /// The source_copy, source_in, source_out, destination_atop, and
    /// destination_in operations may clear parts of the canvas outside the
    /// new drawing but within the clip region.  Defaults to source_over.
    ///
    /// source_atop:       Show new over old where old is opaque.
    /// source_copy:       Replace old with new.
    /// source_in:         Replace old with new where old was opaque.
    /// source_out:        Replace old with new where old was transparent.
    /// source_over:       Show new over old.
    /// destination_atop:  Show old over new where new is opaque.
    /// destination_in:    Clear old where new is transparent.
    /// destination_out:   Clear old where new is opaque.
    /// destination_over:  Show new under old.
    /// exclusive_or:      Show new and old but clear where both are opaque.
    /// lighter:           Sum the new with the old.
    ///
    composite_operation global_composite_operation;

    // ======== SHADOWS ========

    /// @brief  Set the color and opacity of the shadow.
    ///
    /// Most drawing operations can optionally draw a blurred drop shadow
    /// before doing the main drawing.  The shadow is modulated by the opacity
    /// of the drawing and will be blended into the existing pixels subject to
    /// the compositing settings and clipping region.  Shadows will only be
    /// drawn if the shadow color has any opacity and the shadow is either
    /// offset or blurred.  The color and opacity values will be clamped to
    /// the 0.0 to 1.0 range, inclusive.  Defaults to 0.0, 0.0, 0.0, 0.0
    /// (transparent black).
    ///
    /// @param red    sRGB red component of the shadow color
    /// @param green  sRGB green component of the shadow color
    /// @param blue   sRGB blue component of the shadow color
    /// @param alpha  opacity of the shadow (not premultiplied)
    ///
    void set_shadow_color(
        float red,
        float green,
        float blue,
        float alpha );

    /// @brief  Horizontal offset of the shadow in pixels.
    ///
    /// Negative shifts left, positive shifts right.  This is not affected
    /// by the current transform.  Defaults to 0.0 (no offset).
    ///
    float shadow_offset_x;

    /// @brief  Vertical offset of the shadow in pixels.
    ///
    /// Negative shifts up, positive shifts down.  This is not affected by
    /// the current transform.  Defaults to 0.0 (no offset).
    ///
    float shadow_offset_y;

    /// @brief  Set the level of Gaussian blurring on the shadow.
    ///
    /// Zero produces no blur, while larger values will blur the shadow
    /// more strongly.  This is not affected by the current transform.
    /// Must be non-negative.  If it is not, this does nothing.  Defaults to
    /// 0.0 (no blur).
    ///
    /// @param level  the level of Gaussian blurring on the shadow
    ///
    void set_shadow_blur(
        float level );

    // ======== LINE STYLES ========

    /// @brief  Set the width of the lines when stroking.
    ///
    /// Initially this is measured in pixels, though the current transform
    /// at the time of drawing can affect this.  Must be positive.  If it
    /// is not, this does nothing.  Defaults to 1.0.
    ///
    /// @param width  width of the lines when stroking
    ///
    void set_line_width(
        float width );

    /// @brief  Cap style for the ends of open subpaths and dash segments.
    ///
    /// The actual shape may be affected by the current transform at the time
    /// of drawing.  Only affects stroking.  Defaults to butt.
    ///
    /// butt:    Use a flat cap flush to the end of the line.
    /// square:  Use a half-square cap that extends past the end of the line.
    /// circle:  Use a semicircular cap.
    ///
    cap_style line_cap;

    /// @brief  Join style for connecting lines within the paths.
    ///
    /// The actual shape may be affected by the current transform at the time
    /// of drawing.  Only affects stroking.  Defaults to miter.
    ///
    /// miter:  Continue the ends until they intersect, if within miter limit.
    /// bevel:  Connect the ends with a flat triangle.
    /// round:  Join the ends with a circular arc.
    ///
    join_style line_join;

    /// @brief  Set the limit on maximum pointiness allowed for miter joins.
    ///
    /// If the distance from the point where the lines intersect to the
    /// point where the outside edges of the join intersect exceeds this
    /// ratio relative to the line width, then a bevel join will be used
    /// instead, and the miter will be lopped off.  Larger values allow
    /// pointier miters.  Only affects stroking and only when the line join
    /// style is miter.  Must be positive.  If it is not, this does nothing.
    /// Defaults to 10.0.
    ///
    /// @param limit  the limit on maximum pointiness allowed for miter joins
    ///
    void set_miter_limit(
        float limit );

    /// @brief  Offset where each subpath starts the dash pattern.
    ///
    /// Changing this shifts the location of the dashes along the path and
    /// animating it will produce a marching ants effect.  Only affects
    /// stroking and only when a dash pattern is set.  May be negative or
    /// exceed the length of the dash pattern, in which case it will wrap.
    /// Defaults to 0.0.
    ///
    float line_dash_offset;

    /// @brief  Set or clear the line dash pattern.
    ///
    /// Takes an array with entries alternately giving the lengths of dash
    /// and gap segments.  All must be non-negative; if any are not, this
    /// does nothing.  These will be used to draw with dashed lines when
    /// stroking, with each subpath starting at the length along the dash
    /// pattern indicated by the line dash offset.  Initially these lengths
    /// are measured in pixels, though the current transform at the time of
    /// drawing can affect this.  The count must be non-negative.  If it is
    /// odd, the array will be appended to itself to make an even count.  If
    /// it is zero, or the pointer is null, the dash pattern will be cleared
    /// and strokes will be drawn as solid lines.  The array is copied and
    /// it is safe to change or destroy it after this call.  Defaults to
    /// solid lines.
    ///
    /// @param segments  pointer to array for dash pattern
    /// @param count     number of entries in the array
    ///
    void set_line_dash(
        float const *segments,
        int count );

    // ======== FILL AND STROKE STYLES ========

    /// @brief  Set filling or stroking to use a constant color and opacity.
    ///
    /// The color and opacity values will be clamped to the 0.0 to 1.0 range,
    /// inclusive.  Filling and stroking defaults a constant color with 0.0,
    /// 0.0, 0.0, 1.0 (opaque black).
    ///
    /// @param type   whether to set the fill_style or stroke_style
    /// @param red    sRGB red component of the painting color
    /// @param green  sRGB green component of the painting color
    /// @param blue   sRGB blue component of the painting color
    /// @param alpha  opacity to paint with (not premultiplied)
    ///
    void set_color(
        brush_type type,
        float red,
        float green,
        float blue,
        float alpha );

    /// @brief  Set filling or stroking to use a linear gradient.
    ///
    /// Positions the start and end points of the gradient and clears all
    /// color stops to reset the gradient to transparent black.  Color stops
    /// can then be added again.  When drawing, pixels will be painted with
    /// the color of the gradient at the nearest point on the line segment
    /// between the start and end points.  This is affected by the current
    /// transform at the time of drawing.
    ///
    /// @param type     whether to set the fill_style or stroke_style
    /// @param start_x  horizontal coordinate of the start of the gradient
    /// @param start_y  vertical coordinate of the start of the gradient
    /// @param end_x    horizontal coordinate of the end of the gradient
    /// @param end_y    vertical coordinate of the end of the gradient
    ///
    void set_linear_gradient(
        brush_type type,
        float start_x,
        float start_y,
        float end_x,
        float end_y );

    /// @brief  Set filling or stroking to use a radial gradient.
    ///
    /// Positions the start and end circles of the gradient and clears all
    /// color stops to reset the gradient to transparent black.  Color stops
    /// can then be added again.  When drawing, pixels will be painted as
    /// though the starting circle moved and changed size linearly to match
    /// the ending circle, while sweeping through the colors of the gradient.
    /// Pixels not touched by the moving circle will be left transparent
    /// black.  The radial gradient is affected by the current transform
    /// at the time of drawing.  The radii must be non-negative.
    ///
    /// @param type          whether to set the fill_style or stroke_style
    /// @param start_x       horizontal starting coordinate of the circle
    /// @param start_y       vertical starting coordinate of the circle
    /// @param start_radius  starting radius of the circle
    /// @param end_x         horizontal ending coordinate of the circle
    /// @param end_y         vertical ending coordinate of the circle
    /// @param end_radius    ending radius of the circle
    ///
    void set_radial_gradient(
        brush_type type,
        float start_x,
        float start_y,
        float start_radius,
        float end_x,
        float end_y,
        float end_radius );

    /// @brief  Add a color stop to a linear or radial gradient.
    ///
    /// Each color stop has an offset which defines its position from 0.0 at
    /// the start of the gradient to 1.0 at the end.  Colors and opacity are
    /// linearly interpolated along the gradient between adjacent pairs of
    /// stops without premultiplying the alpha.  If more than one stop is
    /// added for a given offset, the first one added is considered closest
    /// to 0.0 and the last is closest to 1.0.  If no stop is at offset 0.0
    /// or 1.0, the stops with the closest offsets will be extended.  If no
    /// stops are added, the gradient will be fully transparent black.  Set a
    /// new linear or radial gradient to clear all the stops and redefine the
    /// gradient colors.  Color stops may be added to a gradient at any time.
    /// The color and opacity values will be clamped to the 0.0 to 1.0 range,
    /// inclusive.  The offset must be in the 0.0 to 1.0 range, inclusive.
    /// If it is not, or if chosen style type is not currently set to a
    /// gradient, this does nothing.
    ///
    /// @param type    whether to add to the fill_style or stroke_style
    /// @param offset  position of the color stop along the gradient
    /// @param red     sRGB red component of the color stop
    /// @param green   sRGB green component of the color stop
    /// @param blue    sRGB blue component of the color stop
    /// @param alpha   opacity of the color stop (not premultiplied)
    ///
    void add_color_stop(
        brush_type type,
        float offset,
        float red,
        float green,
        float blue,
        float alpha );

    /// @brief  Set filling or stroking to draw with an image pattern.
    ///
    /// Initially, pixels in the pattern correspond exactly to pixels on the
    /// canvas, with the pattern starting in the upper left.  The pattern
    /// is affected by the current transform at the time of drawing, and
    /// the pattern will be resampled as needed (with the filtering always
    /// wrapping regardless of the repetition setting).  The pattern can be
    /// repeated either horizontally, vertically, both, or neither, relative
    /// to the source image.  If the pattern is not repeated, then beyond it
    /// will be considered transparent black.  The pattern image, which should
    /// be in top to bottom rows of contiguous pixels from left to right,
    /// is copied and it is safe to change or destroy it after this call.
    /// The width and height must both be positive.  If either are not, or
    /// the image pointer is null, this does nothing.
    ///
    /// Tip: to use a small piece of a larger image, reduce the width and
    ///      height, and offset the image pointer while keeping the stride.
    ///
    /// @param type        whether to set the fill_style or stroke_style
    /// @param image       pointer to unpremultiplied sRGB RGBA8 image data
    /// @param width       width of the pattern image in pixels
    /// @param height      height of the pattern image in pixels
    /// @param stride      number of bytes between the start of each image row
    /// @param repetition  repeat, repeat_x, repeat_y, or no_repeat
    ///
    void set_pattern(
        brush_type type,
        unsigned char const *image,
        int width,
        int height,
        int stride,
        repetition_style repetition );

    // ======== BUILDING PATHS ========

    /// @brief  Reset the current path.
    ///
    /// The current path and all subpaths will be cleared after this, and a
    /// new path can be built.
    ///
    void begin_path();

    /// @brief  Create a new subpath.
    ///
    /// The given point will become the first point of the new subpath and
    /// is subject to the current transform at the time this is called.
    ///
    /// @param x  horizontal coordinate of the new first point
    /// @param y  vertical coordinate of the new first point
    ///
    void move_to(
        float x,
        float y );

    /// @brief  Close the current subpath.
    ///
    /// Adds a straight line from the end of the current subpath back to its
    /// first point and marks the subpath as closed so that this line will
    /// join with the beginning of the path at this point.  A new, empty
    /// subpath will be started beginning with the same first point.  If the
    /// current path is empty, this does nothing.
    ///
    void close_path();

    /// @brief  Extend the current subpath with a straight line.
    ///
    /// The line will go from the current end point (if the current path is
    /// not empty) to the given point, which will become the new end point.
    /// Its position is affected by the current transform at the time that
    /// this is called.  If the current path was empty, this is equivalent
    /// to just a move.
    ///
    /// @param x  horizontal coordinate of the new end point
    /// @param y  vertical coordinate of the new end point
    ///
    void line_to(
        float x,
        float y );

    /// @brief  Extend the current subpath with a quadratic Bezier curve.
    ///
    /// The curve will go from the current end point (or the control point
    /// if the current path is empty) to the given point, which will become
    /// the new end point.  The curve will be tangent toward the control
    /// point at both ends.  The current transform at the time that this
    /// is called will affect both points passed in.
    ///
    /// Tip: to make multiple curves join smoothly, ensure that each new end
    ///      point is on a line between the adjacent control points.
    ///
    /// @param control_x  horizontal coordinate of the control point
    /// @param control_y  vertical coordinate of the control point
    /// @param x          horizontal coordinate of the new end point
    /// @param y          vertical coordinate of the new end point
    ///
    void quadratic_curve_to(
        float control_x,
        float control_y,
        float x,
        float y );

    /// @brief  Extend the current subpath with a cubic Bezier curve.
    ///
    /// The curve will go from the current end point (or the first control
    /// point if the current path is empty) to the given point, which will
    /// become the new end point.  The curve will be tangent toward the first
    /// control point at the beginning and tangent toward the second control
    /// point at the end.  The current transform at the time that this is
    /// called will affect all points passed in.
    ///
    /// Tip: to make multiple curves join smoothly, ensure that each new end
    ///      point is on a line between the adjacent control points.
    ///
    /// @param control_1_x  horizontal coordinate of the first control point
    /// @param control_1_y  vertical coordinate of the first control point
    /// @param control_2_x  horizontal coordinate of the second control point
    /// @param control_2_y  vertical coordinate of the second control point
    /// @param x            horizontal coordinate of the new end point
    /// @param y            vertical coordinate of the new end point
    ///
    void bezier_curve_to(
        float control_1_x,
        float control_1_y,
        float control_2_x,
        float control_2_y,
        float x,
        float y );

    /// @brief  Extend the current subpath with an arc tangent to two lines.
    ///
    /// The arc is from the circle with the given radius tangent to both
    /// the line from the current end point to the vertex, and to the line
    /// from the vertex to the given point.  A straight line will be added
    /// from the current end point to the first tangent point (unless the
    /// current path is empty), then the shortest arc from the first to the
    /// second tangent points will be added.  The second tangent point will
    /// become the new end point.  If the radius is large, these tangent
    /// points may fall outside the line segments.  The current transform
    /// at the time that this is called will affect the passed in points
    /// and the arc.  If the current path was empty, this is equivalent to
    /// a move.  If the arc would be degenerate, it is equivalent to a line
    /// to the vertex point.  The radius must be non-negative.  If it is not,
    /// or if the current transform is not invertible, this does nothing.
    ///
    /// Tip: to draw a polygon with rounded corners, call this once for each
    ///      vertex and pass the midpoint of the adjacent edge as the second
    ///      point; this works especially well for rounded boxes.
    ///
    /// @param vertex_x  horizontal coordinate where the tangent lines meet
    /// @param vertex_y  vertical coordinate where the tangent lines meet
    /// @param x         a horizontal coordinate on the second tangent line
    /// @param y         a vertical coordinate on the second tangent line
    /// @param radius    radius of the circle containing the arc
    ///
    void arc_to(
        float vertex_x,
        float vertex_y,
        float x,
        float y,
        float radius );

    /// @brief  Extend the current subpath with an arc between two angles.
    ///
    /// The arc is from the circle centered at the given point and with the
    /// given radius.  A straight line will be added from the current end
    /// point to the starting point of the arc (unless the current path is
    /// empty), then the arc along the circle from the starting angle to the
    /// ending angle in the given direction will be added.  If they are more
    /// than two pi radians apart in the given direction, the arc will stop
    /// after one full circle.  The point at the ending angle will become
    /// the new end point of the path.  Initially, the angles are clockwise
    /// relative to the x-axis.  The current transform at the time that
    /// this is called will affect the passed in point, angles, and arc.
    /// The radius must be non-negative.
    ///
    /// @param x                  horizontal coordinate of the circle center
    /// @param y                  vertical coordinate of the circle center
    /// @param radius             radius of the circle containing the arc
    /// @param start_angle        radians clockwise from x-axis to begin
    /// @param end_angle          radians clockwise from x-axis to end
    /// @param counter_clockwise  true if the arc turns counter-clockwise
    ///
    void arc(
        float x,
        float y,
        float radius,
        float start_angle,
        float end_angle,
        bool counter_clockwise = false );

    /// @brief  Add a closed subpath in the shape of a rectangle.
    ///
    /// The rectangle has one corner at the given point and then goes in the
    /// direction along the width before going in the direction of the height
    /// towards the opposite corner.  The current transform at the time that
    /// this is called will affect the given point and rectangle.  The width
    /// and/or the height may be negative or zero, and this can affect the
    /// winding direction.
    ///
    /// @param x       horizontal coordinate of a rectangle corner
    /// @param y       vertical coordinate of a rectangle corner
    /// @param width   width of the rectangle
    /// @param height  height of the rectangle
    ///
    void rectangle(
        float x,
        float y,
        float width,
        float height );

    // ======== DRAWING PATHS ========

    /// @brief  Draw the interior of the current path using the fill style.
    ///
    /// Interior pixels are determined by the non-zero winding rule, with
    /// all open subpaths implicitly closed by a straight line beforehand.
    /// If shadows have been enabled by setting the shadow color with any
    /// opacity and either offsetting or blurring the shadows, then the
    /// shadows of the filled areas will be drawn first, followed by the
    /// filled areas themselves.  Both will be blended into the canvas
    /// according to the global alpha, the global compositing operation,
    /// and the clip region.  If the fill style is a gradient or a pattern,
    /// it will be affected by the current transform.  The current path is
    /// left unchanged by filling; begin a new path to clear it.  If the
    /// current transform is not invertible, this does nothing.
    ///
    void fill();

    /// @brief  Draw the edges of the current path using the stroke style.
    ///
    /// Edges of the path will be expanded into strokes according to the
    /// current dash pattern, dash offset, line width, line join style
    /// (and possibly miter limit), line cap, and transform.  If shadows
    /// have been enabled by setting the shadow color with any opacity and
    /// either offsetting or blurring the shadows, then the shadow will be
    /// drawn for the stroked lines first, then the stroked lines themselves.
    /// Both will be blended into the canvas according to the global alpha,
    /// the global compositing operation, and the clip region.  If the stroke
    /// style is a gradient or a pattern, it will be affected by the current
    /// transform.  The current path is left unchanged by stroking; begin a
    /// new path to clear it.  If the current transform is not invertible,
    /// this does nothing.
    ///
    /// Tip: to draw with a calligraphy-like angled brush effect, add a
    ///      non-uniform scale transform just before stroking.
    ///
    void stroke();

    /// @brief  Restrict the clip region by the current path.
    ///
    /// Intersects the current clip region with the interior of the current
    /// path (the region that would be filled), and replaces the current
    /// clip region with this intersection.  Subsequent calls to clip can
    /// only reduce this further.  When filling or stroking, only pixels
    /// within the current clip region will change.  The current path is
    /// left unchanged by updating the clip region; begin a new path to
    /// clear it.  Defaults to the entire canvas.
    ///
    /// Tip: to be able to reset the current clip region, save the canvas
    ///      state first before clipping then restore the state to reset it.
    ///
    void clip();

    /// @brief  Tests whether a point is in or on the current path.
    ///
    /// Interior areas are determined by the non-zero winding rule, with
    /// all open subpaths treated as implicitly closed by a straight line
    /// beforehand.  Points exactly on the boundary are also considered
    /// inside.  The point to test is interpreted without being affected by
    /// the current transform, nor is the clip region considered.  The current
    /// path is left unchanged by this test.
    ///
    /// @param x  horizontal coordinate of the point to test
    /// @param y  vertical coordinate of the point to test
    /// @return   true if the point is in or on the current path
    ///
    bool is_point_in_path(
        float x,
        float y );

    // ======== DRAWING RECTANGLES ========

    /// @brief  Clear a rectangular area back to transparent black.
    ///
    /// The clip region may limit the area cleared.  The current path is not
    /// affected by this clearing.  The current transform at the time that
    /// this is called will affect the given point and rectangle.  The width
    /// and/or the height may be negative or zero.  If either is zero, or the
    /// current transform is not invertible, this does nothing.
    ///
    /// @param x       horizontal coordinate of a rectangle corner
    /// @param y       vertical coordinate of a rectangle corner
    /// @param width   width of the rectangle
    /// @param height  height of the rectangle
    ///
    void clear_rectangle(
        float x,
        float y,
        float width,
        float height );

    /// @brief  Fill a rectangular area.
    ///
    /// This behaves as though the current path were reset to a single
    /// rectangle and then filled as usual.  However, the current path is
    /// not actually changed.  The current transform at the time that this is
    /// called will affect the given point and rectangle.  The width and/or
    /// the height may be negative but not zero.  If either is zero, or the
    /// current transform is not invertible, this does nothing.
    ///
    /// @param x       horizontal coordinate of a rectangle corner
    /// @param y       vertical coordinate of a rectangle corner
    /// @param width   width of the rectangle
    /// @param height  height of the rectangle
    ///
    void fill_rectangle(
        float x,
        float y,
        float width,
        float height );

    /// @brief  Stroke a rectangular area.
    ///
    /// This behaves as though the current path were reset to a single
    /// rectangle and then stroked as usual.  However, the current path is
    /// not actually changed.  The current transform at the time that this
    /// is called will affect the given point and rectangle.  The width
    /// and/or the height may be negative or zero.  If both are zero, or
    /// the current transform is not invertible, this does nothing.  If only
    /// one is zero, this behaves as though it strokes a single horizontal or
    /// vertical line.
    ///
    /// @param x       horizontal coordinate of a rectangle corner
    /// @param y       vertical coordinate of a rectangle corner
    /// @param width   width of the rectangle
    /// @param height  height of the rectangle
    ///
    void stroke_rectangle(
        float x,
        float y,
        float width,
        float height );

    // ======== DRAWING TEXT ========

    /// @brief  Horizontal position of the text relative to the anchor point.
    ///
    /// When drawing text, the positioning of the text relative to the anchor
    /// point includes the side bearings of the first and last glyphs.
    /// Defaults to leftward.
    ///
    /// leftward:   Draw the text's left edge at the anchor point.
    /// rightward:  Draw the text's right edge at the anchor point.
    /// center:     Draw the text's horizontal center at the anchor point.
    /// start:      This is a synonym for leftward.
    /// ending:     This is a synonym for rightward.
    ///
    align_style text_align;

    /// @brief  Vertical position of the text relative to the anchor point.
    ///
    /// Defaults to alphabetic.
    ///
    /// alphabetic:   Draw with the alphabetic baseline at the anchor point.
    /// top:          Draw the top of the em box at the anchor point.
    /// middle:       Draw the exact middle of the em box at the anchor point.
    /// bottom:       Draw the bottom of the em box at the anchor point.
    /// hanging:      Draw 60% of an em over the baseline at the anchor point.
    /// ideographic:  This is a synonym for bottom.
    ///
    baseline_style text_baseline;

    /// @brief  Set the font to use for text drawing.
    ///
    /// The font must be a TrueType font (TTF) file which has been loaded or
    /// mapped into memory.  Following some basic validation, the relevant
    /// sections of the font file contents are copied, and it is safe to
    /// change or destroy after this call.  As an optimization, calling this
    /// with either a null pointer or zero for the number of bytes will allow
    /// for changing the size of the previous font without recopying from
    /// the file.  Note that the font parsing is not meant to be secure;
    /// only use this with trusted TTF files!
    ///
    /// @param font   pointer to the contents of a TrueType font (TTF) file
    /// @param bytes  number of bytes in the font file
    /// @param size   size in pixels per em to draw at
    /// @return       true if the font was set successfully
    ///
    bool set_font(
        unsigned char const *font,
        int bytes,
        float size );

    /// @brief  Draw a line of text by filling its outline.
    ///
    /// This behaves as though the current path were reset to the outline
    /// of the given text in the current font and size, positioned relative
    /// to the given anchor point according to the current alignment and
    /// baseline, and then filled as usual.  However, the current path is
    /// not actually changed.  The current transform at the time that this
    /// is called will affect the given anchor point and the text outline.
    /// However, the comparison to the maximum width in pixels and the
    /// condensing if needed is done before applying the current transform.
    /// The maximum width (if given) must be positive.  If it is not, or
    /// the text pointer is null, or the font has not been set yet, or the
    /// last setting of it was unsuccessful, or the current transform is not
    /// invertible, this does nothing.
    ///
    /// @param text           null-terminated UTF-8 string of text to fill
    /// @param x              horizontal coordinate of the anchor point
    /// @param y              vertical coordinate of the anchor point
    /// @param maximum_width  horizontal width to condense wider text to
    ///
    void fill_text(
        char const *text,
        float x,
        float y,
        float maximum_width = 1.0e30f );

    /// @brief  Draw a line of text by stroking its outline.
    ///
    /// This behaves as though the current path were reset to the outline
    /// of the given text in the current font and size, positioned relative
    /// to the given anchor point according to the current alignment and
    /// baseline, and then stroked as usual.  However, the current path is
    /// not actually changed.  The current transform at the time that this
    /// is called will affect the given anchor point and the text outline.
    /// However, the comparison to the maximum width in pixels and the
    /// condensing if needed is done before applying the current transform.
    /// The maximum width (if given) must be positive.  If it is not, or
    /// the text pointer is null, or the font has not been set yet, or the
    /// last setting of it was unsuccessful, or the current transform is not
    /// invertible, this does nothing.
    ///
    /// @param text           null-terminated UTF-8 string of text to stroke
    /// @param x              horizontal coordinate of the anchor point
    /// @param y              vertical coordinate of the anchor point
    /// @param maximum_width  horizontal width to condense wider text to
    ///
    void stroke_text(
        char const *text,
        float x,
        float y,
        float maximum_width = 1.0e30f );

    /// @brief  Measure the width in pixels of a line of text.
    ///
    /// The measured width is the advance width, which includes the side
    /// bearings of the first and last glyphs.  However, text as drawn may
    /// go outside this (e.g., due to glyphs that spill beyond their nominal
    /// widths or stroked text with wide lines).  Measurements ignore the
    /// current transform.  If the text pointer is null, or the font has
    /// not been set yet, or the last setting of it was unsuccessful, this
    /// returns zero.
    ///
    /// @param text  null-terminated UTF-8 string of text to measure
    /// @return      width of the text in pixels
    ///
    float measure_text(
        char const *text );

    // ======== DRAWING IMAGES ========

    /// @brief  Draw an image onto the canvas.
    ///
    /// The position of the rectangle that the image is drawn to is affected
    /// by the current transform at the time of drawing, and the image will
    /// be resampled as needed (with the filtering always clamping to the
    /// edges of the image).  The drawing is also affected by the shadow,
    /// global alpha, global compositing operation settings, and by the
    /// clip region.  The current path is not affected by drawing an image.
    /// The image data, which should be in top to bottom rows of contiguous
    /// pixels from left to right, is not retained and it is safe to change
    /// or destroy it after this call.  The width and height must both be
    /// positive and the width and/or the height to scale to may be negative
    /// but not zero.  Otherwise, or if the image pointer is null, or the
    /// current transform is not invertible, this does nothing.
    ///
    /// Tip: to use a small piece of a larger image, reduce the width and
    ///      height, and offset the image pointer while keeping the stride.
    ///
    /// @param image      pointer to unpremultiplied sRGB RGBA8 image data
    /// @param width      width of the image in pixels
    /// @param height     height of the image in pixels
    /// @param stride     number of bytes between the start of each image row
    /// @param x          horizontal coordinate to draw the corner at
    /// @param y          vertical coordinate to draw the corner at
    /// @param to_width   width to scale the image to
    /// @param to_height  height to scale the image to
    ///
    void draw_image(
        unsigned char const *image,
        int width,
        int height,
        int stride,
        float x,
        float y,
        float to_width,
        float to_height );

    // ======== PIXEL MANIPULATION ========

    /// @brief  Fetch a rectangle of pixels from the canvas to an image.
    ///
    /// This call is akin to a direct pixel-for-pixel copy from the canvas
    /// buffer without resampling.  The position and size of the canvas
    /// rectangle is not affected by the current transform.  The image data
    /// is copied into, from top to bottom in rows of contiguous pixels from
    /// left to right, and it is safe to change or destroy it after this call.
    /// The requested rectangle may safely extend outside the bounds of the
    /// canvas; these pixels will be set to transparent black.  The width
    /// and height must be positive.  If not, or if the image pointer is
    /// null, this does nothing.
    ///
    /// Tip: to copy into a section of a larger image, reduce the width and
    ///      height, and offset the image pointer while keeping the stride.
    /// Tip: use this to get the rendered image from the canvas after drawing.
    ///
    /// @param image   pointer to unpremultiplied sRGB RGBA8 image data
    /// @param width   width of the image in pixels
    /// @param height  height of the image in pixels
    /// @param stride  number of bytes between the start of each image row
    /// @param x       horizontal coordinate of upper-left pixel to fetch
    /// @param y       vertical coordinate of upper-left pixel to fetch
    ///
    void get_image_data(
        unsigned char *image,
        int width,
        int height,
        int stride,
        int x,
        int y );

    /// @brief  Replace a rectangle of pixels on the canvas with an image.
    ///
    /// This call is akin to a direct pixel-for-pixel copy into the canvas
    /// buffer without resampling.  The position and size of the canvas
    /// rectangle is not affected by the current transform.  Nor is the
    /// result affected by the current settings for the global alpha, global
    /// compositing operation, shadows, or the clip region.  The image data,
    /// which should be in top to bottom rows of contiguous pixels from left
    /// to right, is copied from and it is safe to change or destroy it after
    /// this call.  The width and height must be positive.  If not, or if the
    /// image pointer is null, this does nothing.
    ///
    /// Tip: to copy from a section of a larger image, reduce the width and
    ///      height, and offset the image pointer while keeping the stride.
    /// Tip: use this to prepopulate the canvas before drawing.
    ///
    /// @param image   pointer to unpremultiplied sRGB RGBA8 image data
    /// @param width   width of the image in pixels
    /// @param height  height of the image in pixels
    /// @param stride  number of bytes between the start of each image row
    /// @param x       horizontal coordinate of upper-left pixel to replace
    /// @param y       vertical coordinate of upper-left pixel to replace
    ///
    void put_image_data(
        unsigned char const *image,
        int width,
        int height,
        int stride,
        int x,
        int y );

    // ======== CANVAS STATE ========

    /// @brief  Save the current state as though to a stack.
    ///
    /// The full state of the canvas is saved, except for the pixels in the
    /// canvas buffer, and the current path.
    ///
    /// Tip: to be able to reset the current clip region, save the canvas
    ///      state first before clipping then restore the state to reset it.
    ///
    void save();

    /// @brief  Restore a previously saved state as though from a stack.
    ///
    /// This does not affect the pixels in the canvas buffer or the current
    /// path.  If the stack of canvas states is empty, this does nothing.
    ///
    void restore();

    rgba* get_bitmap()
    {
        return bitmap;
    };

private:
    int size_x;
    int size_y;
    affine_matrix forward;
    affine_matrix inverse;
    float global_alpha;
    rgba shadow_color;
    float shadow_blur;
    std::vector< float > shadow;
    float line_width;
    float miter_limit;
    std::vector< float > line_dash;
    paint_brush fill_brush;
    paint_brush stroke_brush;
    paint_brush image_brush;
    bezier_path path;
    line_path lines;
    line_path scratch;
    pixel_runs runs;
    pixel_runs mask;
    font_face face;
    rgba *bitmap;
    canvas *saves;
    canvas( canvas const & );
    canvas &operator=( canvas const & );
    void add_tessellation( xy, xy, xy, xy, float, int );
    void add_bezier( xy, xy, xy, xy, float );
    void path_to_lines( bool );
    void add_glyph( int, float );
    int character_to_glyph( char const *, int & );
    void text_to_lines( char const *, xy, float, bool );
    void dash_lines();
    void add_half_stroke( size_t, size_t, bool );
    void stroke_lines();
    void add_runs( xy, xy );
    void lines_to_runs( xy, int );
    rgba paint_pixel( xy, paint_brush const & );
    void render_shadow( paint_brush const & );
    void render_main( paint_brush const & );
};
class CANVASITY_API canvas_20
{
public:

    // ======== LIFECYCLE ========

    /// @brief  Construct a new canvas_20.
    ///
    /// It will begin with all pixels set to transparent black.  Initially,
    /// the visible coordinates will run from (0, 0) in the upper-left to
    /// (width, height) in the lower-right and with pixel centers offset
    /// (0.5, 0.5) from the integer grid, though all this may be changed
    /// by transforms.  The sizes must be between 1 and 32768, inclusive.
    ///
    /// @param width  horizontal size of the new canvas_20 in pixels
    /// @param height  vertical size of the new canvas_20 in pixels
    ///
    canvas_20(
        int width,
        int height );

    /// @brief  Destroy the canvas_20 and release all associated memory.
    ///
    ~canvas_20();

    // ======== TRANSFORMS ========

    /// @brief  Scale the current transform.
    ///
    /// Scaling may be non-uniform if the x and y scaling factors are
    /// different.  When a scale factor is less than one, things will be
    /// shrunk in that direction.  When a scale factor is greater than
    /// one, they will grow bigger.  Negative scaling factors will flip or
    /// mirror it in that direction.  The scaling factors must be non-zero.
    /// If either is zero, most drawing operations will do nothing.
    ///
    /// @param x  horizontal scaling factor
    /// @param y  vertical scaling factor
    ///
    void scale(
        float x,
        float y );

    /// @brief  Rotate the current transform.
    ///
    /// The rotation is applied clockwise in a direction around the origin.
    ///
    /// Tip: to rotate around another point, first translate that point to
    ///      the origin, then do the rotation, and then translate back.
    ///
    /// @param angle  clockwise angle in radians
    ///
    void rotate(
        float angle );

    /// @brief  Translate the current transform.
    ///
    /// By default, positive x values shift that many pixels to the right,
    /// while negative y values shift left, positive y values shift up, and
    /// negative y values shift down.
    ///
    /// @param x  amount to shift horizontally
    /// @param y  amount to shift vertically
    ///
    void translate(
        float x,
        float y );

    /// @brief  Add an arbitrary transform to the current transform.
    ///
    /// This takes six values for the upper two rows of a homogenous 3x3
    /// matrix (i.e., {{a, c, e}, {b, d, f}, {0.0, 0.0, 1.0}}) describing an
    /// arbitrary affine transform and appends it to the current transform.
    /// The values can represent any affine transform such as scaling,
    /// rotation, translation, or skew, or any composition of affine
    /// transforms.  The matrix must be invertible.  If it is not, most
    /// drawing operations will do nothing.
    ///
    /// @param a  horizontal scaling factor (m11)
    /// @param b  vertical skewing (m12)
    /// @param c  horizontal skewing (m21)
    /// @param d  vertical scaling factor (m22)
    /// @param e  horizontal translation (m31)
    /// @param f  vertical translation (m32)
    ///
    void transform(
        float a,
        float b,
        float c,
        float d,
        float e,
        float f );

    /// @brief  Replace the current transform.
    ///
    /// This takes six values for the upper two rows of a homogenous 3x3
    /// matrix (i.e., {{a, c, e}, {b, d, f}, {0.0, 0.0, 1.0}}) describing
    /// an arbitrary affine transform and replaces the current transform
    /// with it.  The values can represent any affine transform such as
    /// scaling, rotation, translation, or skew, or any composition of
    /// affine transforms.  The matrix must be invertible.  If it is not,
    /// most drawing operations will do nothing.
    ///
    /// Tip: to reset the current transform back to the default, use
    ///      1.0, 0.0, 0.0, 1.0, 0.0, 0.0.
    ///
    /// @param a  horizontal scaling factor (m11)
    /// @param b  vertical skewing (m12)
    /// @param c  horizontal skewing (m21)
    /// @param d  vertical scaling factor (m22)
    /// @param e  horizontal translation (m31)
    /// @param f  vertical translation (m32)
    ///
    void set_transform(
        float a,
        float b,
        float c,
        float d,
        float e,
        float f );

    // ======== COMPOSITING ========

    /// @brief  Set the degree of opacity applied to all drawing operations.
    ///
    /// If an operation already uses a transparent color, this can make it
    /// yet more transparent.  It must be in the range from 0.0 for fully
    /// transparent to 1.0 for fully opaque, inclusive.  If it is not, this
    /// does nothing.  Defaults to 1.0 (opaque).
    ///
    /// @param alpha  degree of opacity applied to all drawing operations
    ///
    void set_global_alpha(
        float alpha );

    /// @brief  Compositing operation for blending new drawing and old pixels.
    ///
    /// The source_copy, source_in, source_out, destination_atop, and
    /// destination_in operations may clear parts of the canvas_20 outside the
    /// new drawing but within the clip region.  Defaults to source_over.
    ///
    /// source_atop:       Show new over old where old is opaque.
    /// source_copy:       Replace old with new.
    /// source_in:         Replace old with new where old was opaque.
    /// source_out:        Replace old with new where old was transparent.
    /// source_over:       Show new over old.
    /// destination_atop:  Show old over new where new is opaque.
    /// destination_in:    Clear old where new is transparent.
    /// destination_out:   Clear old where new is opaque.
    /// destination_over:  Show new under old.
    /// exclusive_or:      Show new and old but clear where both are opaque.
    /// lighter:           Sum the new with the old.
    ///
    composite_operation global_composite_operation;

    // ======== SHADOWS ========

    /// @brief  Set the color and opacity of the shadow.
    ///
    /// Most drawing operations can optionally draw a blurred drop shadow
    /// before doing the main drawing.  The shadow is modulated by the opacity
    /// of the drawing and will be blended into the existing pixels subject to
    /// the compositing settings and clipping region.  Shadows will only be
    /// drawn if the shadow color has any opacity and the shadow is either
    /// offset or blurred.  The color and opacity values will be clamped to
    /// the 0.0 to 1.0 range, inclusive.  Defaults to 0.0, 0.0, 0.0, 0.0
    /// (transparent black).
    ///
    /// @param red    sRGB red component of the shadow color
    /// @param green  sRGB green component of the shadow color
    /// @param blue   sRGB blue component of the shadow color
    /// @param alpha  opacity of the shadow (not premultiplied)
    ///
    void set_shadow_color(
        float red, float green, float blue, float alpha,
        float data_a, float data_b, float data_c, float data_d,
        float data_e, float data_f, float data_g, float data_h,
        float data_i, float data_j, float data_k, float data_l,
        float data_m, float data_n, float data_o, float data_p );

    /// @brief  Horizontal offset of the shadow in pixels.
    ///
    /// Negative shifts left, positive shifts right.  This is not affected
    /// by the current transform.  Defaults to 0.0 (no offset).
    ///
    float shadow_offset_x;

    /// @brief  Vertical offset of the shadow in pixels.
    ///
    /// Negative shifts up, positive shifts down.  This is not affected by
    /// the current transform.  Defaults to 0.0 (no offset).
    ///
    float shadow_offset_y;

    /// @brief  Set the level of Gaussian blurring on the shadow.
    ///
    /// Zero produces no blur, while larger values will blur the shadow
    /// more strongly.  This is not affected by the current transform.
    /// Must be non-negative.  If it is not, this does nothing.  Defaults to
    /// 0.0 (no blur).
    ///
    /// @param level  the level of Gaussian blurring on the shadow
    ///
    void set_shadow_blur(
        float level );

    // ======== LINE STYLES ========

    /// @brief  Set the width of the lines when stroking.
    ///
    /// Initially this is measured in pixels, though the current transform
    /// at the time of drawing can affect this.  Must be positive.  If it
    /// is not, this does nothing.  Defaults to 1.0.
    ///
    /// @param width  width of the lines when stroking
    ///
    void set_line_width(
        float width );

    /// @brief  Cap style for the ends of open subpaths and dash segments.
    ///
    /// The actual shape may be affected by the current transform at the time
    /// of drawing.  Only affects stroking.  Defaults to butt.
    ///
    /// butt:    Use a flat cap flush to the end of the line.
    /// square:  Use a half-square cap that extends past the end of the line.
    /// circle:  Use a semicircular cap.
    ///
    cap_style line_cap;

    /// @brief  Join style for connecting lines within the paths.
    ///
    /// The actual shape may be affected by the current transform at the time
    /// of drawing.  Only affects stroking.  Defaults to miter.
    ///
    /// miter:  Continue the ends until they intersect, if within miter limit.
    /// bevel:  Connect the ends with a flat triangle.
    /// round:  Join the ends with a circular arc.
    ///
    join_style line_join;

    /// @brief  Set the limit on maximum pointiness allowed for miter joins.
    ///
    /// If the distance from the point where the lines intersect to the
    /// point where the outside edges of the join intersect exceeds this
    /// ratio relative to the line width, then a bevel join will be used
    /// instead, and the miter will be lopped off.  Larger values allow
    /// pointier miters.  Only affects stroking and only when the line join
    /// style is miter.  Must be positive.  If it is not, this does nothing.
    /// Defaults to 10.0.
    ///
    /// @param limit  the limit on maximum pointiness allowed for miter joins
    ///
    void set_miter_limit(
        float limit );

    /// @brief  Offset where each subpath starts the dash pattern.
    ///
    /// Changing this shifts the location of the dashes along the path and
    /// animating it will produce a marching ants effect.  Only affects
    /// stroking and only when a dash pattern is set.  May be negative or
    /// exceed the length of the dash pattern, in which case it will wrap.
    /// Defaults to 0.0.
    ///
    float line_dash_offset;

    /// @brief  Set or clear the line dash pattern.
    ///
    /// Takes an array with entries alternately giving the lengths of dash
    /// and gap segments.  All must be non-negative; if any are not, this
    /// does nothing.  These will be used to draw with dashed lines when
    /// stroking, with each subpath starting at the length along the dash
    /// pattern indicated by the line dash offset.  Initially these lengths
    /// are measured in pixels, though the current transform at the time of
    /// drawing can affect this.  The count must be non-negative.  If it is
    /// odd, the array will be appended to itself to make an even count.  If
    /// it is zero, or the pointer is null, the dash pattern will be cleared
    /// and strokes will be drawn as solid lines.  The array is copied and
    /// it is safe to change or destroy it after this call.  Defaults to
    /// solid lines.
    ///
    /// @param segments  pointer to array for dash pattern
    /// @param count     number of entries in the array
    ///
    void set_line_dash(
        float const *segments,
        int count );

    // ======== FILL AND STROKE STYLES ========

    /// @brief  Set filling or stroking to use a constant color and opacity.
    ///
    /// The color and opacity values will be clamped to the 0.0 to 1.0 range,
    /// inclusive.  Filling and stroking defaults a constant color with 0.0,
    /// 0.0, 0.0, 1.0 (opaque black).
    ///
    /// @param type   whether to set the fill_style or stroke_style
    /// @param red    sRGB red component of the painting color
    /// @param green  sRGB green component of the painting color
    /// @param blue   sRGB blue component of the painting color
    /// @param alpha  opacity to paint with (not premultiplied)
    ///
    void set_color(
        brush_type type,
        float red, float green, float blue, float alpha,
        float data_a, float data_b, float data_c, float data_d,
        float data_e, float data_f, float data_g, float data_h,
        float data_i, float data_j, float data_k, float data_l,
        float data_m, float data_n, float data_o, float data_p );

    void set_data_color( brush_type type, const rgba_20& data );

    /// @brief  Set filling or stroking to use a linear gradient.
    ///
    /// Positions the start and end points of the gradient and clears all
    /// color stops to reset the gradient to transparent black.  Color stops
    /// can then be added again.  When drawing, pixels will be painted with
    /// the color of the gradient at the nearest point on the line segment
    /// between the start and end points.  This is affected by the current
    /// transform at the time of drawing.
    ///
    /// @param type     whether to set the fill_style or stroke_style
    /// @param start_x  horizontal coordinate of the start of the gradient
    /// @param start_y  vertical coordinate of the start of the gradient
    /// @param end_x    horizontal coordinate of the end of the gradient
    /// @param end_y    vertical coordinate of the end of the gradient
    ///
    void set_linear_gradient(
        brush_type type,
        float start_x,
        float start_y,
        float end_x,
        float end_y );

    /// @brief  Set filling or stroking to use a radial gradient.
    ///
    /// Positions the start and end circles of the gradient and clears all
    /// color stops to reset the gradient to transparent black.  Color stops
    /// can then be added again.  When drawing, pixels will be painted as
    /// though the starting circle moved and changed size linearly to match
    /// the ending circle, while sweeping through the colors of the gradient.
    /// Pixels not touched by the moving circle will be left transparent
    /// black.  The radial gradient is affected by the current transform
    /// at the time of drawing.  The radii must be non-negative.
    ///
    /// @param type          whether to set the fill_style or stroke_style
    /// @param start_x       horizontal starting coordinate of the circle
    /// @param start_y       vertical starting coordinate of the circle
    /// @param start_radius  starting radius of the circle
    /// @param end_x         horizontal ending coordinate of the circle
    /// @param end_y         vertical ending coordinate of the circle
    /// @param end_radius    ending radius of the circle
    ///
    void set_radial_gradient(
        brush_type type,
        float start_x,
        float start_y,
        float start_radius,
        float end_x,
        float end_y,
        float end_radius );

    /// @brief  Add a color stop to a linear or radial gradient.
    ///
    /// Each color stop has an offset which defines its position from 0.0 at
    /// the start of the gradient to 1.0 at the end.  Colors and opacity are
    /// linearly interpolated along the gradient between adjacent pairs of
    /// stops without premultiplying the alpha.  If more than one stop is
    /// added for a given offset, the first one added is considered closest
    /// to 0.0 and the last is closest to 1.0.  If no stop is at offset 0.0
    /// or 1.0, the stops with the closest offsets will be extended.  If no
    /// stops are added, the gradient will be fully transparent black.  Set a
    /// new linear or radial gradient to clear all the stops and redefine the
    /// gradient colors.  Color stops may be added to a gradient at any time.
    /// The color and opacity values will be clamped to the 0.0 to 1.0 range,
    /// inclusive.  The offset must be in the 0.0 to 1.0 range, inclusive.
    /// If it is not, or if chosen style type is not currently set to a
    /// gradient, this does nothing.
    ///
    /// @param type    whether to add to the fill_style or stroke_style
    /// @param offset  position of the color stop along the gradient
    /// @param red     sRGB red component of the color stop
    /// @param green   sRGB green component of the color stop
    /// @param blue    sRGB blue component of the color stop
    /// @param alpha   opacity of the color stop (not premultiplied)
    ///
    void add_color_stop(
        brush_type type,
        float offset,
        float red, float green, float blue, float alpha,
        float data_a, float data_b, float data_c, float data_d,
        float data_e, float data_f, float data_g, float data_h,
        float data_i, float data_j, float data_k, float data_l,
        float data_m, float data_n, float data_o, float data_p );

    /// @brief  Set filling or stroking to draw with an image pattern.
    ///
    /// Initially, pixels in the pattern correspond exactly to pixels on the
    /// canvas_20, with the pattern starting in the upper left.  The pattern
    /// is affected by the current transform at the time of drawing, and
    /// the pattern will be resampled as needed (with the filtering always
    /// wrapping regardless of the repetition setting).  The pattern can be
    /// repeated either horizontally, vertically, both, or neither, relative
    /// to the source image.  If the pattern is not repeated, then beyond it
    /// will be considered transparent black.  The pattern image, which should
    /// be in top to bottom rows of contiguous pixels from left to right,
    /// is copied and it is safe to change or destroy it after this call.
    /// The width and height must both be positive.  If either are not, or
    /// the image pointer is null, this does nothing.
    ///
    /// Tip: to use a small piece of a larger image, reduce the width and
    ///      height, and offset the image pointer while keeping the stride.
    ///
    /// @param type        whether to set the fill_style or stroke_style
    /// @param image       pointer to unpremultiplied sRGB RGBA8 image data
    /// @param width       width of the pattern image in pixels
    /// @param height      height of the pattern image in pixels
    /// @param stride      number of bytes between the start of each image row
    /// @param repetition  repeat, repeat_x, repeat_y, or no_repeat
    ///
    void set_pattern(
        brush_type type,
        unsigned char const *image,
        int width,
        int height,
        int stride,
        repetition_style repetition );

    // ======== BUILDING PATHS ========

    /// @brief  Reset the current path.
    ///
    /// The current path and all subpaths will be cleared after this, and a
    /// new path can be built.
    ///
    void begin_path();

    /// @brief  Create a new subpath.
    ///
    /// The given point will become the first point of the new subpath and
    /// is subject to the current transform at the time this is called.
    ///
    /// @param x  horizontal coordinate of the new first point
    /// @param y  vertical coordinate of the new first point
    ///
    void move_to(
        float x,
        float y );

    /// @brief  Close the current subpath.
    ///
    /// Adds a straight line from the end of the current subpath back to its
    /// first point and marks the subpath as closed so that this line will
    /// join with the beginning of the path at this point.  A new, empty
    /// subpath will be started beginning with the same first point.  If the
    /// current path is empty, this does nothing.
    ///
    void close_path();

    /// @brief  Extend the current subpath with a straight line.
    ///
    /// The line will go from the current end point (if the current path is
    /// not empty) to the given point, which will become the new end point.
    /// Its position is affected by the current transform at the time that
    /// this is called.  If the current path was empty, this is equivalent
    /// to just a move.
    ///
    /// @param x  horizontal coordinate of the new end point
    /// @param y  vertical coordinate of the new end point
    ///
    void line_to(
        float x,
        float y );

    /// @brief  Extend the current subpath with a quadratic Bezier curve.
    ///
    /// The curve will go from the current end point (or the control point
    /// if the current path is empty) to the given point, which will become
    /// the new end point.  The curve will be tangent toward the control
    /// point at both ends.  The current transform at the time that this
    /// is called will affect both points passed in.
    ///
    /// Tip: to make multiple curves join smoothly, ensure that each new end
    ///      point is on a line between the adjacent control points.
    ///
    /// @param control_x  horizontal coordinate of the control point
    /// @param control_y  vertical coordinate of the control point
    /// @param x          horizontal coordinate of the new end point
    /// @param y          vertical coordinate of the new end point
    ///
    void quadratic_curve_to(
        float control_x,
        float control_y,
        float x,
        float y );

    /// @brief  Extend the current subpath with a cubic Bezier curve.
    ///
    /// The curve will go from the current end point (or the first control
    /// point if the current path is empty) to the given point, which will
    /// become the new end point.  The curve will be tangent toward the first
    /// control point at the beginning and tangent toward the second control
    /// point at the end.  The current transform at the time that this is
    /// called will affect all points passed in.
    ///
    /// Tip: to make multiple curves join smoothly, ensure that each new end
    ///      point is on a line between the adjacent control points.
    ///
    /// @param control_1_x  horizontal coordinate of the first control point
    /// @param control_1_y  vertical coordinate of the first control point
    /// @param control_2_x  horizontal coordinate of the second control point
    /// @param control_2_y  vertical coordinate of the second control point
    /// @param x            horizontal coordinate of the new end point
    /// @param y            vertical coordinate of the new end point
    ///
    void bezier_curve_to(
        float control_1_x,
        float control_1_y,
        float control_2_x,
        float control_2_y,
        float x,
        float y );

    /// @brief  Extend the current subpath with an arc tangent to two lines.
    ///
    /// The arc is from the circle with the given radius tangent to both
    /// the line from the current end point to the vertex, and to the line
    /// from the vertex to the given point.  A straight line will be added
    /// from the current end point to the first tangent point (unless the
    /// current path is empty), then the shortest arc from the first to the
    /// second tangent points will be added.  The second tangent point will
    /// become the new end point.  If the radius is large, these tangent
    /// points may fall outside the line segments.  The current transform
    /// at the time that this is called will affect the passed in points
    /// and the arc.  If the current path was empty, this is equivalent to
    /// a move.  If the arc would be degenerate, it is equivalent to a line
    /// to the vertex point.  The radius must be non-negative.  If it is not,
    /// or if the current transform is not invertible, this does nothing.
    ///
    /// Tip: to draw a polygon with rounded corners, call this once for each
    ///      vertex and pass the midpoint of the adjacent edge as the second
    ///      point; this works especially well for rounded boxes.
    ///
    /// @param vertex_x  horizontal coordinate where the tangent lines meet
    /// @param vertex_y  vertical coordinate where the tangent lines meet
    /// @param x         a horizontal coordinate on the second tangent line
    /// @param y         a vertical coordinate on the second tangent line
    /// @param radius    radius of the circle containing the arc
    ///
    void arc_to(
        float vertex_x,
        float vertex_y,
        float x,
        float y,
        float radius );

    /// @brief  Extend the current subpath with an arc between two angles.
    ///
    /// The arc is from the circle centered at the given point and with the
    /// given radius.  A straight line will be added from the current end
    /// point to the starting point of the arc (unless the current path is
    /// empty), then the arc along the circle from the starting angle to the
    /// ending angle in the given direction will be added.  If they are more
    /// than two pi radians apart in the given direction, the arc will stop
    /// after one full circle.  The point at the ending angle will become
    /// the new end point of the path.  Initially, the angles are clockwise
    /// relative to the x-axis.  The current transform at the time that
    /// this is called will affect the passed in point, angles, and arc.
    /// The radius must be non-negative.
    ///
    /// @param x                  horizontal coordinate of the circle center
    /// @param y                  vertical coordinate of the circle center
    /// @param radius             radius of the circle containing the arc
    /// @param start_angle        radians clockwise from x-axis to begin
    /// @param end_angle          radians clockwise from x-axis to end
    /// @param counter_clockwise  true if the arc turns counter-clockwise
    ///
    void arc(
        float x,
        float y,
        float radius,
        float start_angle,
        float end_angle,
        bool counter_clockwise = false );

    /// @brief  Add a closed subpath in the shape of a rectangle.
    ///
    /// The rectangle has one corner at the given point and then goes in the
    /// direction along the width before going in the direction of the height
    /// towards the opposite corner.  The current transform at the time that
    /// this is called will affect the given point and rectangle.  The width
    /// and/or the height may be negative or zero, and this can affect the
    /// winding direction.
    ///
    /// @param x       horizontal coordinate of a rectangle corner
    /// @param y       vertical coordinate of a rectangle corner
    /// @param width   width of the rectangle
    /// @param height  height of the rectangle
    ///
    void rectangle(
        float x,
        float y,
        float width,
        float height );

    // ======== DRAWING PATHS ========

    /// @brief  Draw the interior of the current path using the fill style.
    ///
    /// Interior pixels are determined by the non-zero winding rule, with
    /// all open subpaths implicitly closed by a straight line beforehand.
    /// If shadows have been enabled by setting the shadow color with any
    /// opacity and either offsetting or blurring the shadows, then the
    /// shadows of the filled areas will be drawn first, followed by the
    /// filled areas themselves.  Both will be blended into the canvas_20
    /// according to the global alpha, the global compositing operation,
    /// and the clip region.  If the fill style is a gradient or a pattern,
    /// it will be affected by the current transform.  The current path is
    /// left unchanged by filling; begin a new path to clear it.  If the
    /// current transform is not invertible, this does nothing.
    ///
    void fill();

    /// @brief  Draw the edges of the current path using the stroke style.
    ///
    /// Edges of the path will be expanded into strokes according to the
    /// current dash pattern, dash offset, line width, line join style
    /// (and possibly miter limit), line cap, and transform.  If shadows
    /// have been enabled by setting the shadow color with any opacity and
    /// either offsetting or blurring the shadows, then the shadow will be
    /// drawn for the stroked lines first, then the stroked lines themselves.
    /// Both will be blended into the canvas_20 according to the global alpha,
    /// the global compositing operation, and the clip region.  If the stroke
    /// style is a gradient or a pattern, it will be affected by the current
    /// transform.  The current path is left unchanged by stroking; begin a
    /// new path to clear it.  If the current transform is not invertible,
    /// this does nothing.
    ///
    /// Tip: to draw with a calligraphy-like angled brush effect, add a
    ///      non-uniform scale transform just before stroking.
    ///
    void stroke();

    /// @brief  Restrict the clip region by the current path.
    ///
    /// Intersects the current clip region with the interior of the current
    /// path (the region that would be filled), and replaces the current
    /// clip region with this intersection.  Subsequent calls to clip can
    /// only reduce this further.  When filling or stroking, only pixels
    /// within the current clip region will change.  The current path is
    /// left unchanged by updating the clip region; begin a new path to
    /// clear it.  Defaults to the entire canvas_20.
    ///
    /// Tip: to be able to reset the current clip region, save the canvas_20
    ///      state first before clipping then restore the state to reset it.
    ///
    void clip();

    /// @brief  Tests whether a point is in or on the current path.
    ///
    /// Interior areas are determined by the non-zero winding rule, with
    /// all open subpaths treated as implicitly closed by a straight line
    /// beforehand.  Points exactly on the boundary are also considered
    /// inside.  The point to test is interpreted without being affected by
    /// the current transform, nor is the clip region considered.  The current
    /// path is left unchanged by this test.
    ///
    /// @param x  horizontal coordinate of the point to test
    /// @param y  vertical coordinate of the point to test
    /// @return   true if the point is in or on the current path
    ///
    bool is_point_in_path(
        float x,
        float y );

    // ======== DRAWING RECTANGLES ========

    /// @brief  Clear a rectangular area back to transparent black.
    ///
    /// The clip region may limit the area cleared.  The current path is not
    /// affected by this clearing.  The current transform at the time that
    /// this is called will affect the given point and rectangle.  The width
    /// and/or the height may be negative or zero.  If either is zero, or the
    /// current transform is not invertible, this does nothing.
    ///
    /// @param x       horizontal coordinate of a rectangle corner
    /// @param y       vertical coordinate of a rectangle corner
    /// @param width   width of the rectangle
    /// @param height  height of the rectangle
    ///
    void clear_rectangle(
        float x,
        float y,
        float width,
        float height );

    /// @brief  Fill a rectangular area.
    ///
    /// This behaves as though the current path were reset to a single
    /// rectangle and then filled as usual.  However, the current path is
    /// not actually changed.  The current transform at the time that this is
    /// called will affect the given point and rectangle.  The width and/or
    /// the height may be negative but not zero.  If either is zero, or the
    /// current transform is not invertible, this does nothing.
    ///
    /// @param x       horizontal coordinate of a rectangle corner
    /// @param y       vertical coordinate of a rectangle corner
    /// @param width   width of the rectangle
    /// @param height  height of the rectangle
    ///
    void fill_rectangle(
        float x,
        float y,
        float width,
        float height );

    /// @brief  Stroke a rectangular area.
    ///
    /// This behaves as though the current path were reset to a single
    /// rectangle and then stroked as usual.  However, the current path is
    /// not actually changed.  The current transform at the time that this
    /// is called will affect the given point and rectangle.  The width
    /// and/or the height may be negative or zero.  If both are zero, or
    /// the current transform is not invertible, this does nothing.  If only
    /// one is zero, this behaves as though it strokes a single horizontal or
    /// vertical line.
    ///
    /// @param x       horizontal coordinate of a rectangle corner
    /// @param y       vertical coordinate of a rectangle corner
    /// @param width   width of the rectangle
    /// @param height  height of the rectangle
    ///
    void stroke_rectangle(
        float x,
        float y,
        float width,
        float height );

    // ======== DRAWING TEXT ========

    /// @brief  Horizontal position of the text relative to the anchor point.
    ///
    /// When drawing text, the positioning of the text relative to the anchor
    /// point includes the side bearings of the first and last glyphs.
    /// Defaults to leftward.
    ///
    /// leftward:   Draw the text's left edge at the anchor point.
    /// rightward:  Draw the text's right edge at the anchor point.
    /// center:     Draw the text's horizontal center at the anchor point.
    /// start:      This is a synonym for leftward.
    /// ending:     This is a synonym for rightward.
    ///
    align_style text_align;

    /// @brief  Vertical position of the text relative to the anchor point.
    ///
    /// Defaults to alphabetic.
    ///
    /// alphabetic:   Draw with the alphabetic baseline at the anchor point.
    /// top:          Draw the top of the em box at the anchor point.
    /// middle:       Draw the exact middle of the em box at the anchor point.
    /// bottom:       Draw the bottom of the em box at the anchor point.
    /// hanging:      Draw 60% of an em over the baseline at the anchor point.
    /// ideographic:  This is a synonym for bottom.
    ///
    baseline_style text_baseline;

    /// @brief  Set the font to use for text drawing.
    ///
    /// The font must be a TrueType font (TTF) file which has been loaded or
    /// mapped into memory.  Following some basic validation, the relevant
    /// sections of the font file contents are copied, and it is safe to
    /// change or destroy after this call.  As an optimization, calling this
    /// with either a null pointer or zero for the number of bytes will allow
    /// for changing the size of the previous font without recopying from
    /// the file.  Note that the font parsing is not meant to be secure;
    /// only use this with trusted TTF files!
    ///
    /// @param font   pointer to the contents of a TrueType font (TTF) file
    /// @param bytes  number of bytes in the font file
    /// @param size   size in pixels per em to draw at
    /// @return       true if the font was set successfully
    ///
    bool set_font(
        unsigned char const *font,
        int bytes,
        float size );

    /// @brief  Draw a line of text by filling its outline.
    ///
    /// This behaves as though the current path were reset to the outline
    /// of the given text in the current font and size, positioned relative
    /// to the given anchor point according to the current alignment and
    /// baseline, and then filled as usual.  However, the current path is
    /// not actually changed.  The current transform at the time that this
    /// is called will affect the given anchor point and the text outline.
    /// However, the comparison to the maximum width in pixels and the
    /// condensing if needed is done before applying the current transform.
    /// The maximum width (if given) must be positive.  If it is not, or
    /// the text pointer is null, or the font has not been set yet, or the
    /// last setting of it was unsuccessful, or the current transform is not
    /// invertible, this does nothing.
    ///
    /// @param text           null-terminated UTF-8 string of text to fill
    /// @param x              horizontal coordinate of the anchor point
    /// @param y              vertical coordinate of the anchor point
    /// @param maximum_width  horizontal width to condense wider text to
    ///
    void fill_text(
        char const *text,
        float x,
        float y,
        float maximum_width = 1.0e30f );

    /// @brief  Draw a line of text by stroking its outline.
    ///
    /// This behaves as though the current path were reset to the outline
    /// of the given text in the current font and size, positioned relative
    /// to the given anchor point according to the current alignment and
    /// baseline, and then stroked as usual.  However, the current path is
    /// not actually changed.  The current transform at the time that this
    /// is called will affect the given anchor point and the text outline.
    /// However, the comparison to the maximum width in pixels and the
    /// condensing if needed is done before applying the current transform.
    /// The maximum width (if given) must be positive.  If it is not, or
    /// the text pointer is null, or the font has not been set yet, or the
    /// last setting of it was unsuccessful, or the current transform is not
    /// invertible, this does nothing.
    ///
    /// @param text           null-terminated UTF-8 string of text to stroke
    /// @param x              horizontal coordinate of the anchor point
    /// @param y              vertical coordinate of the anchor point
    /// @param maximum_width  horizontal width to condense wider text to
    ///
    void stroke_text(
        char const *text,
        float x,
        float y,
        float maximum_width = 1.0e30f );

    /// @brief  Measure the width in pixels of a line of text.
    ///
    /// The measured width is the advance width, which includes the side
    /// bearings of the first and last glyphs.  However, text as drawn may
    /// go outside this (e.g., due to glyphs that spill beyond their nominal
    /// widths or stroked text with wide lines).  Measurements ignore the
    /// current transform.  If the text pointer is null, or the font has
    /// not been set yet, or the last setting of it was unsuccessful, this
    /// returns zero.
    ///
    /// @param text  null-terminated UTF-8 string of text to measure
    /// @return      width of the text in pixels
    ///
    float measure_text(
        char const *text );

    // ======== DRAWING IMAGES ========

    /// @brief  Draw an image onto the canvas_20.
    ///
    /// The position of the rectangle that the image is drawn to is affected
    /// by the current transform at the time of drawing, and the image will
    /// be resampled as needed (with the filtering always clamping to the
    /// edges of the image).  The drawing is also affected by the shadow,
    /// global alpha, global compositing operation settings, and by the
    /// clip region.  The current path is not affected by drawing an image.
    /// The image data, which should be in top to bottom rows of contiguous
    /// pixels from left to right, is not retained and it is safe to change
    /// or destroy it after this call.  The width and height must both be
    /// positive and the width and/or the height to scale to may be negative
    /// but not zero.  Otherwise, or if the image pointer is null, or the
    /// current transform is not invertible, this does nothing.
    ///
    /// Tip: to use a small piece of a larger image, reduce the width and
    ///      height, and offset the image pointer while keeping the stride.
    ///
    /// @param image      pointer to unpremultiplied sRGB RGBA8 image data
    /// @param width      width of the image in pixels
    /// @param height     height of the image in pixels
    /// @param stride     number of bytes between the start of each image row
    /// @param x          horizontal coordinate to draw the corner at
    /// @param y          vertical coordinate to draw the corner at
    /// @param to_width   width to scale the image to
    /// @param to_height  height to scale the image to
    ///
    void draw_image(
        unsigned char const *image,
        int width,
        int height,
        int stride,
        float x,
        float y,
        float to_width,
        float to_height );

    // ======== PIXEL MANIPULATION ========

    /// @brief  Fetch a rectangle of pixels from the canvas_20 to an image.
    ///
    /// This call is akin to a direct pixel-for-pixel copy from the canvas_20
    /// buffer without resampling.  The position and size of the canvas_20
    /// rectangle is not affected by the current transform.  The image data
    /// is copied into, from top to bottom in rows of contiguous pixels from
    /// left to right, and it is safe to change or destroy it after this call.
    /// The requested rectangle may safely extend outside the bounds of the
    /// canvas_20; these pixels will be set to transparent black.  The width
    /// and height must be positive.  If not, or if the image pointer is
    /// null, this does nothing.
    ///
    /// Tip: to copy into a section of a larger image, reduce the width and
    ///      height, and offset the image pointer while keeping the stride.
    /// Tip: use this to get the rendered image from the canvas_20 after drawing.
    ///
    /// @param image   pointer to unpremultiplied sRGB RGBA8 image data
    /// @param width   width of the image in pixels
    /// @param height  height of the image in pixels
    /// @param stride  number of bytes between the start of each image row
    /// @param x       horizontal coordinate of upper-left pixel to fetch
    /// @param y       vertical coordinate of upper-left pixel to fetch
    ///
    void get_image_data(
        unsigned char *image,
        int width,
        int height,
        int stride,
        int x,
        int y );

    /// @brief  Replace a rectangle of pixels on the canvas_20 with an image.
    ///
    /// This call is akin to a direct pixel-for-pixel copy into the canvas_20
    /// buffer without resampling.  The position and size of the canvas_20
    /// rectangle is not affected by the current transform.  Nor is the
    /// result affected by the current settings for the global alpha, global
    /// compositing operation, shadows, or the clip region.  The image data,
    /// which should be in top to bottom rows of contiguous pixels from left
    /// to right, is copied from and it is safe to change or destroy it after
    /// this call.  The width and height must be positive.  If not, or if the
    /// image pointer is null, this does nothing.
    ///
    /// Tip: to copy from a section of a larger image, reduce the width and
    ///      height, and offset the image pointer while keeping the stride.
    /// Tip: use this to prepopulate the canvas_20 before drawing.
    ///
    /// @param image   pointer to unpremultiplied sRGB RGBA8 image data
    /// @param width   width of the image in pixels
    /// @param height  height of the image in pixels
    /// @param stride  number of bytes between the start of each image row
    /// @param x       horizontal coordinate of upper-left pixel to replace
    /// @param y       vertical coordinate of upper-left pixel to replace
    ///
    void put_image_data(
        unsigned char const *image,
        int width,
        int height,
        int stride,
        int x,
        int y );

    // ======== CANVAS STATE ========

    /// @brief  Save the current state as though to a stack.
    ///
    /// The full state of the canvas_20 is saved, except for the pixels in the
    /// canvas_20 buffer, and the current path.
    ///
    /// Tip: to be able to reset the current clip region, save the canvas_20
    ///      state first before clipping then restore the state to reset it.
    ///
    void save();

    /// @brief  Restore a previously saved state as though from a stack.
    ///
    /// This does not affect the pixels in the canvas_20 buffer or the current
    /// path.  If the stack of canvas_20 states is empty, this does nothing.
    ///
    void restore();
    
    rgba_20* get_bitmap()
    {
        return bitmap;
    }

private:
    int size_x;
    int size_y;
    affine_matrix forward;
    affine_matrix inverse;
    float global_alpha;
    rgba_20 shadow_color;
    float shadow_blur;
    std::vector< float > shadow;
    float line_width;
    float miter_limit;
    std::vector< float > line_dash;
    paint_brush_20 fill_brush;
    paint_brush_20 stroke_brush;
    paint_brush_20 image_brush;
    bezier_path path;
    line_path lines;
    line_path scratch;
    pixel_runs runs;
    pixel_runs mask;
    font_face face;
    rgba_20 *bitmap;
    canvas_20 *saves;
    canvas_20( canvas_20 const & );
    canvas_20 &operator=( canvas_20 const & );
    void add_tessellation( xy, xy, xy, xy, float, int );
    void add_bezier( xy, xy, xy, xy, float );
    void path_to_lines( bool );
    void add_glyph( int, float );
    int character_to_glyph( char const *, int & );
    void text_to_lines( char const *, xy, float, bool );
    void dash_lines();
    void add_half_stroke( size_t, size_t, bool );
    void stroke_lines();
    void add_runs( xy, xy );
    void lines_to_runs( xy, int );
    rgba_20 paint_pixel( xy, paint_brush_20 const & );
    void render_shadow( paint_brush_20 const & );
    void render_main( paint_brush_20 const & );
};

}
