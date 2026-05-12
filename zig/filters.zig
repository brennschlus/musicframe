// filters.zig
//
// Comptime-generated lookup tables for all image filters.
// All arithmetic is resolved at compile time; runtime path is one memory
// access per channel per pixel.

// ---------------------------------------------------------------------------
// Pixel format: u32 = R | (G<<8) | (B<<16) | (A<<24)  (matches ImageBuffer)
// ---------------------------------------------------------------------------

inline fn unpackR(px: u32) u8 { return @truncate(px); }
inline fn unpackG(px: u32) u8 { return @truncate(px >> 8); }
inline fn unpackB(px: u32) u8 { return @truncate(px >> 16); }
inline fn unpackA(px: u32) u8 { return @truncate(px >> 24); }

inline fn pack(r: u8, g: u8, b: u8, a: u8) u32 {
    return @as(u32, r) | (@as(u32, g) << 8) | (@as(u32, b) << 16) | (@as(u32, a) << 24);
}

// ---------------------------------------------------------------------------
// Comptime LUT builders
// ---------------------------------------------------------------------------

fn buildLut(comptime transform: fn (u8) u8) [256]u8 {
    var lut: [256]u8 = undefined;
    for (&lut, 0..) |*v, i| {
        v.* = transform(@intCast(i));
    }
    return lut;
}

fn buildLut16(comptime transform: fn (u8) u16) [256]u16 {
    var lut: [256]u16 = undefined;
    for (&lut, 0..) |*v, i| {
        v.* = transform(@intCast(i));
    }
    return lut;
}

// ---------------------------------------------------------------------------
// Channel transforms (comptime)
// ---------------------------------------------------------------------------

fn warmR(x: u8) u8 {
    const v: i32 = @as(i32, x) + 25;
    return if (v > 255) 255 else @intCast(v);
}

fn warmB(x: u8) u8 {
    const v: i32 = @as(i32, x) - 20;
    return if (v < 0) 0 else @intCast(v);
}

// Returns a faded-channel transform for a given scale factor (out of 256).
// Result = 50 + (x * scale) >> 8, clamped to [0..255].
fn fadedChannel(comptime scale: u32) fn (u8) u8 {
    return struct {
        fn f(x: u8) u8 {
            const v: u32 = 50 + ((@as(u32, x) * scale) >> 8);
            return if (v > 255) 255 else @intCast(v);
        }
    }.f;
}

// BT.601 luminance partial contributions — u16 to preserve precision when summed.
// lum = (lum_r[r] + lum_g[g] + lum_b[b]) >> 8
// Max sum: 77*255 + 150*255 + 29*255 = 256*255 = 65280 — fits in u16.
fn lumR(x: u8) u16 { return @as(u16, 77) * x; }
fn lumG(x: u8) u16 { return @as(u16, 150) * x; }
fn lumB(x: u8) u16 { return @as(u16, 29) * x; }

// Sepia tint applied to luminance output.
fn sepiaR(x: u8) u8 {
    const v: u32 = @as(u32, x) + 40;
    return if (v > 255) 255 else @intCast(v);
}
fn sepiaG(x: u8) u8 {
    const v: u32 = @as(u32, x) + 20;
    return if (v > 255) 255 else @intCast(v);
}

// ---------------------------------------------------------------------------
// Lookup tables — stored in .rodata, computed at compile time
// ---------------------------------------------------------------------------

const warm_r: [256]u8  = buildLut(warmR);
const warm_b: [256]u8  = buildLut(warmB);

const faded_r: [256]u8 = buildLut(fadedChannel(180));
const faded_g: [256]u8 = buildLut(fadedChannel(170));
const faded_b: [256]u8 = buildLut(fadedChannel(160));

// u16 LUTs: avoids truncation error in (a>>8) + (b>>8) + (c>>8) vs (a+b+c)>>8
const lum_r: [256]u16  = buildLut16(lumR);
const lum_g: [256]u16  = buildLut16(lumG);
const lum_b: [256]u16  = buildLut16(lumB);

const sepia_r: [256]u8 = buildLut(sepiaR);
const sepia_g: [256]u8 = buildLut(sepiaG);

// ---------------------------------------------------------------------------
// Filter IDs — must match the C enum in image_filters.h
// ---------------------------------------------------------------------------

const FILTER_NONE      = 0;
const FILTER_GRAYSCALE = 1;
const FILTER_SEPIA     = 2;
const FILTER_WARM      = 3;
const FILTER_FADED     = 4;

// ---------------------------------------------------------------------------
// Per-filter pixel loops
// ---------------------------------------------------------------------------

fn applyWarm(src: [*]const u32, dst: [*]u32, n: u32) void {
    for (0..@as(usize, n)) |i| {
        const px = src[i];
        dst[i] = pack(warm_r[unpackR(px)], unpackG(px), warm_b[unpackB(px)], unpackA(px));
    }
}

fn applyFaded(src: [*]const u32, dst: [*]u32, n: u32) void {
    for (0..@as(usize, n)) |i| {
        const px = src[i];
        dst[i] = pack(faded_r[unpackR(px)], faded_g[unpackG(px)], faded_b[unpackB(px)], unpackA(px));
    }
}

fn applyGrayscale(src: [*]const u32, dst: [*]u32, n: u32) void {
    for (0..@as(usize, n)) |i| {
        const px = src[i];
        const gray: u8 = @intCast(
            (@as(u32, lum_r[unpackR(px)]) + lum_g[unpackG(px)] + lum_b[unpackB(px)]) >> 8
        );
        dst[i] = pack(gray, gray, gray, unpackA(px));
    }
}

fn applySepia(src: [*]const u32, dst: [*]u32, n: u32) void {
    for (0..@as(usize, n)) |i| {
        const px = src[i];
        const lum: u8 = @intCast(
            (@as(u32, lum_r[unpackR(px)]) + lum_g[unpackG(px)] + lum_b[unpackB(px)]) >> 8
        );
        dst[i] = pack(sepia_r[lum], sepia_g[lum], lum, unpackA(px));
    }
}

// ---------------------------------------------------------------------------
// Public C-ABI export
// ---------------------------------------------------------------------------

export fn image_filter_apply_zig(
    filter_id: c_int,
    src: [*]const u32,
    dst: [*]u32,
    pixel_count: u32,
) void {
    switch (filter_id) {
        FILTER_GRAYSCALE => applyGrayscale(src, dst, pixel_count),
        FILTER_SEPIA     => applySepia(src, dst, pixel_count),
        FILTER_WARM      => applyWarm(src, dst, pixel_count),
        FILTER_FADED     => applyFaded(src, dst, pixel_count),
        else             => @memcpy(dst[0..pixel_count], src[0..pixel_count]),
    }
}

// ---------------------------------------------------------------------------
// Tests — run on desktop with: zig test zig/filters.zig
// ---------------------------------------------------------------------------

test "warm: R saturates, G unchanged, B reduced" {
    const t = std.testing;
    const src = [_]u32{pack(255, 128, 100, 255)};
    var dst = [_]u32{0};
    image_filter_apply_zig(FILTER_WARM, &src, &dst, 1);
    try t.expectEqual(pack(255, 128, 80, 255), dst[0]);
}

test "warm: R increases, B clamps at zero" {
    const t = std.testing;
    const src = [_]u32{pack(10, 64, 15, 128)};
    var dst = [_]u32{0};
    image_filter_apply_zig(FILTER_WARM, &src, &dst, 1);
    try t.expectEqual(pack(35, 64, 0, 128), dst[0]);
}

test "faded: black becomes (50, 50, 50)" {
    const t = std.testing;
    const src = [_]u32{pack(0, 0, 0, 255)};
    var dst = [_]u32{0};
    image_filter_apply_zig(FILTER_FADED, &src, &dst, 1);
    try t.expectEqual(pack(50, 50, 50, 255), dst[0]);
}

test "faded: white is compressed" {
    const t = std.testing;
    // R: 50 + (255*180>>8) = 50 + 179 = 229
    // G: 50 + (255*170>>8) = 50 + 169 = 219
    // B: 50 + (255*160>>8) = 50 + 159 = 209
    const src = [_]u32{pack(255, 255, 255, 255)};
    var dst = [_]u32{0};
    image_filter_apply_zig(FILTER_FADED, &src, &dst, 1);
    try t.expectEqual(pack(229, 219, 209, 255), dst[0]);
}

test "grayscale: pure red → BT.601 luminance 76" {
    const t = std.testing;
    // lum_r[255] = 77*255 = 19635; >> 8 = 76
    const src = [_]u32{pack(255, 0, 0, 255)};
    var dst = [_]u32{0};
    image_filter_apply_zig(FILTER_GRAYSCALE, &src, &dst, 1);
    const gray = unpackR(dst[0]);
    try t.expectEqual(@as(u8, 76), gray);
    try t.expectEqual(gray, unpackG(dst[0]));
    try t.expectEqual(gray, unpackB(dst[0]));
}

test "grayscale: pure green → BT.601 luminance 149" {
    const t = std.testing;
    // lum_g[255] = 150*255 = 38250; >> 8 = 149
    const src = [_]u32{pack(0, 255, 0, 255)};
    var dst = [_]u32{0};
    image_filter_apply_zig(FILTER_GRAYSCALE, &src, &dst, 1);
    try t.expectEqual(@as(u8, 149), unpackR(dst[0]));
}

test "grayscale: alpha is preserved" {
    const t = std.testing;
    const src = [_]u32{pack(100, 100, 100, 77)};
    var dst = [_]u32{0};
    image_filter_apply_zig(FILTER_GRAYSCALE, &src, &dst, 1);
    try t.expectEqual(@as(u8, 77), unpackA(dst[0]));
}

test "sepia: mid gray gets warm tint" {
    const t = std.testing;
    // lum = (77*128 + 150*128 + 29*128) >> 8 = (256*128) >> 8 = 128
    // sepia_r = 128+40 = 168, sepia_g = 128+20 = 148, B = 128
    const src = [_]u32{pack(128, 128, 128, 255)};
    var dst = [_]u32{0};
    image_filter_apply_zig(FILTER_SEPIA, &src, &dst, 1);
    try t.expectEqual(@as(u8, 168), unpackR(dst[0]));
    try t.expectEqual(@as(u8, 148), unpackG(dst[0]));
    try t.expectEqual(@as(u8, 128), unpackB(dst[0]));
}

test "filter_none: output equals input" {
    const t = std.testing;
    const src = [_]u32{ pack(10, 20, 30, 40), pack(100, 150, 200, 255) };
    var dst = [_]u32{ 0, 0 };
    image_filter_apply_zig(FILTER_NONE, &src, &dst, 2);
    try t.expectEqual(src[0], dst[0]);
    try t.expectEqual(src[1], dst[1]);
}

// ---------------------------------------------------------------------------

pub fn panic(msg: []const u8, _: ?*std.builtin.StackTrace, _: ?usize) noreturn {
    _ = msg;
    while (true) {}
}

const std = @import("std");
