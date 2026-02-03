/**
 * @file hexgrid_math.hpp
 * @brief Hexagonal grid mathematics library with raylib integration
 *
 * Implementation based on Red Blob Games' excellent hexagonal grid guide:
 * https://www.redblobgames.com/grids/hexagons/
 *
 * Features:
 * - Cube coordinates (q, r, s) where q + r + s = 0
 * - Pointy-top and flat-top hex orientations
 * - Hex-to-pixel and pixel-to-hex conversions
 * - Grid shape generators (parallelogram, triangle, hexagon, rectangle)
 * - Raylib drawing functions
 */

#pragma once
#include <climits>
#include <cmath>
#include <cstdlib>
#include <raylib.h>
#include <raymath.h>

#include <vector>

using std::vector;

/// @brief Point type alias for raylib's Vector2
using Point = Vector2;

// ============================================================================
// HEX GRID MATH
// ============================================================================

/**
 * @brief Direction indices for hex neighbors
 *
 * For pointy-top hexes:
 * - HEX_E: East (right)
 * - HEX_NE: Northeast
 * - HEX_NW: Northwest
 * - HEX_W: West (left)
 * - HEX_SW: Southwest
 * - HEX_SE: Southeast
 */
enum HexDir { HEX_E = 0, HEX_NE, HEX_NW, HEX_W, HEX_SW, HEX_SE, HEX_DIR_COUNT };

/**
 * @brief Hex coordinate in cube coordinate system
 *
 * Uses cube coordinates where q + r + s = 0 always.
 * This constraint makes many hex algorithms simpler.
 */
struct Hex {
  int q; ///< Column coordinate (increases east/right)
  int r; ///< Row coordinate (increases southeast for pointy-top)
  int s; ///< Third coordinate, always equals -q - r

  /**
   * @brief Construct a hex with explicit q, r, s coordinates
   * @param q_ Column coordinate
   * @param r_ Row coordinate
   * @param s_ Third coordinate (must satisfy q + r + s = 0)
   */
  constexpr Hex(int q_, int r_, int s_) : q(q_), r(r_), s(s_) {}

  /**
   * @brief Construct a hex with q and r, computing s automatically
   * @param q_ Column coordinate
   * @param r_ Row coordinate
   */
  constexpr Hex(int q_, int r_) : q(q_), r(r_), s(-q_ - r_) {}

  constexpr bool operator==(const Hex& other) const {
    return q == other.q && r == other.r && s == other.s;
  }
  constexpr bool operator!=(const Hex& other) const { return !(*this == other); }
};

/**
 * @brief Fractional hex coordinate for intermediate calculations
 *
 * Used during pixel-to-hex conversion before rounding to integer coordinates.
 */
struct FractionalHex {
  double q; ///< Fractional column coordinate
  double r; ///< Fractional row coordinate
  double s; ///< Fractional third coordinate

  FractionalHex(double q_, double r_, double s_) : q(q_), r(r_), s(s_) {}
  FractionalHex(double q_, double r_) : q(q_), r(r_), s(-q_ - r_) {}
};

/**
 * @brief Add two hex coordinates
 * @param a First hex
 * @param b Second hex
 * @return Sum of the two hexes
 */
constexpr Hex hex_add(Hex a, Hex b) {
  return Hex(a.q + b.q, a.r + b.r, a.s + b.s);
}

/**
 * @brief Subtract two hex coordinates
 * @param a First hex
 * @param b Second hex (subtracted from a)
 * @return Difference a - b
 */
constexpr Hex hex_subtract(Hex a, Hex b) {
  return Hex(a.q - b.q, a.r - b.r, a.s - b.s);
}

/**
 * @brief Multiply hex coordinates by a scalar
 * @param a Hex to scale
 * @param k Scalar multiplier
 * @return Scaled hex
 */
constexpr Hex hex_multiply(Hex a, int k) {
  return Hex(a.q * k, a.r * k, a.s * k);
}

/**
 * @brief Calculate the length (distance from origin) of a hex
 * @param hex The hex coordinate
 * @return Manhattan distance from origin in hex space
 */
constexpr int hex_length(Hex hex) {
  int aq = hex.q < 0 ? -hex.q : hex.q;
  int ar = hex.r < 0 ? -hex.r : hex.r;
  int as = hex.s < 0 ? -hex.s : hex.s;
  return (aq + ar + as) / 2;
}

/**
 * @brief Calculate distance between two hexes
 * @param a First hex
 * @param b Second hex
 * @return Number of hex steps between a and b
 */
constexpr int hex_distance(Hex a, Hex b) {
  return hex_length(hex_subtract(a, b));
}

/// @brief Direction vectors for the 6 hex neighbors
const vector<Hex> hex_directions = {Hex(1, 0, -1), Hex(1, -1, 0), Hex(0, -1, 1),
                                    Hex(-1, 0, 1), Hex(-1, 1, 0), Hex(0, 1, -1)};

/**
 * @brief Get the direction vector for a given direction index
 * @param direction Direction index (0-5, use HexDir enum)
 * @return Hex offset for that direction
 */
inline Hex hex_direction(int direction /* 0 to 5 */) {
  return hex_directions[direction];
}

/**
 * @brief Get the neighboring hex in a given direction
 * @param hex Starting hex
 * @param direction Direction index (0-5, use HexDir enum)
 * @return The neighboring hex
 */
inline Hex hex_neighbor(Hex hex, int direction) {
  return hex_add(hex, hex_direction(direction));
}

/**
 * @brief Orientation matrices for hex-to-pixel and pixel-to-hex conversion
 *
 * Contains forward matrix (f0-f3) for hex-to-pixel and
 * backward matrix (b0-b3) for pixel-to-hex conversions.
 */
struct Orientation {
  const double f0, f1, f2, f3; ///< Forward matrix for hex-to-pixel
  const double b0, b1, b2, b3; ///< Backward matrix for pixel-to-hex
  const double start_angle;    ///< Starting angle in multiples of 60 degrees

  Orientation(double f0_, double f1_, double f2_, double f3_, double b0_, double b1_, double b2_,
              double b3_, double start_angle_)
      : f0(f0_), f1(f1_), f2(f2_), f3(f3_), b0(b0_), b1(b1_), b2(b2_), b3(b3_),
        start_angle(start_angle_) {}
};

/// @brief Orientation for pointy-top hexes (vertex pointing up)
const Orientation layout_pointy = Orientation(sqrt(3.0), sqrt(3.0) / 2.0, 0.0, 3.0 / 2.0,
                                              sqrt(3.0) / 3.0, -1.0 / 3.0, 0.0, 2.0 / 3.0, 0.5);

/// @brief Orientation for flat-top hexes (edge pointing up)
const Orientation layout_flat = Orientation(3.0 / 2.0, 0.0, sqrt(3.0) / 2.0, sqrt(3.0), 2.0 / 3.0,
                                            0.0, -1.0 / 3.0, sqrt(3.0) / 3.0, 0.0);

/**
 * @brief Layout configuration for rendering hexes
 *
 * Combines orientation, size, and origin to define how hexes
 * are positioned and scaled in screen space.
 */
/**
 * @brief Available grid shape types
 */
enum class GridShape {
  Parallelogram,   ///< Parallelogram-shaped grid
  TriangleDown,    ///< Downward-facing triangle
  TriangleUp,      ///< Upward-facing triangle
  Hexagon,         ///< Hexagonal grid with given radius
  RectanglePointy, ///< Rectangle for pointy-top hexes
  RectangleFlat    ///< Rectangle for flat-top hexes
};

/**
 * @brief Parameters for grid shape generation
 */
struct GridParams {
  int a = 0; ///< q1/left/size/radius depending on shape
  int b = 0; ///< q2/right
  int c = 0; ///< r1/top
  int d = 0; ///< r2/bottom
};

/**
 * @brief Layout configuration for rendering hexes
 *
 * Combines orientation, size, and origin to define how hexes
 * are positioned and scaled in screen space.
 */
struct Layout {
  Orientation orientation;
  Point hex_size = {0, 0};
  Point origin = {0, 0};
  GridShape shape = GridShape::Hexagon;
  GridParams params = {0, 0, 0, 0};
  uint n_hex = 10;

  Hex operator[](uint index) const;

  struct Iterator {
    const Layout* layout;
    uint index;
    Iterator(const Layout* l, uint i) : layout(l), index(i) {}
    Hex operator*() const { return (*layout)[index]; }
    Iterator& operator++() {
      ++index;
      return *this;
    }
    bool operator!=(const Iterator& other) const { return index != other.index; }
  };

  Iterator begin() const { return Iterator(this, 0); }
  Iterator end() const { return Iterator(this, n_hex); }
};
/**
 * @brief Convert hex coordinates to screen pixel position
 * @param layout The layout configuration
 * @param h The hex coordinate
 * @return Center position of the hex in screen coordinates
 */
inline Point hex_to_pixel(Layout layout, Hex h) {
  const Orientation& M = layout.orientation;
  double x = (M.f0 * h.q + M.f1 * h.r) * layout.hex_size.x;
  double y = (M.f2 * h.q + M.f3 * h.r) * layout.hex_size.y;
  return Point{(float)(x + layout.origin.x), (float)(y + layout.origin.y)};
}

/**
 * @brief Convert screen pixel position to fractional hex coordinates
 * @param layout The layout configuration
 * @param p Screen position in pixels
 * @return Fractional hex coordinate (needs rounding for integer hex)
 */
inline FractionalHex pixel_to_hex_fractional(Layout layout, Point p) {
  const Orientation& M = layout.orientation;
  float ptx = (p.x - layout.origin.x) / layout.hex_size.x;
  float pty = (p.y - layout.origin.y) / layout.hex_size.y;
  double q = M.b0 * ptx + M.b1 * pty;
  double r = M.b2 * ptx + M.b3 * pty;
  return FractionalHex(q, r, -q - r);
}

/**
 * @brief Get the offset from hex center to a corner
 * @param layout The layout configuration
 * @param corner Corner index (0-5)
 * @return Offset from center to the specified corner
 */
inline Point hex_corner_offset(Layout layout, int corner) {
  Point size = layout.hex_size;
  double angle = 2.0 * M_PI * (layout.orientation.start_angle + corner) / 6;
  return Point{(float)(size.x * cos(angle)), (float)(size.y * sin(angle))};
}

/**
 * @brief Get the bounding box size of a hex
 * @param layout The layout configuration
 * @return Width and height that fully contains a hex
 *
 * For pointy-top: width = sqrt(3) * size.x, height = 2 * size.y
 * For flat-top: width = 2 * size.x, height = sqrt(3) * size.y
 */
inline Point hex_bounding_size(Layout layout) {
  const float sqrt3 = 1.732050808f;
  Point size = layout.hex_size;
  // Pointy-top has start_angle 0.5, flat-top has 0.0
  if (layout.orientation.start_angle > 0.25f) {
    // Pointy-top
    return {sqrt3 * size.x, 2.0f * size.y};
  } else {
    // Flat-top
    return {2.0f * size.x, sqrt3 * size.y};
  }
}

/**
 * @brief Get all 6 corner positions of a hex in screen coordinates
 * @param layout The layout configuration
 * @param h The hex coordinate
 * @return Vector of 6 corner positions
 */
inline vector<Point> polygon_corners(Layout layout, Hex h) {
  vector<Point> corners = {};
  Point center = hex_to_pixel(layout, h);
  for (int i = 0; i < 6; i++) {
    Point offset = hex_corner_offset(layout, i);
    corners.push_back(Point{center.x + offset.x, center.y + offset.y});
  }
  return corners;
}

/**
 * @brief Draw a hex outline using raylib
 * @param layout The layout configuration
 * @param h The hex coordinate
 * @param color Line color
 */
inline void draw_hex(Layout layout, Hex h, Color color) {
  vector<Point> corners = polygon_corners(layout, h);
  for (int i = 0; i < 6; i++) {
    DrawLineV(corners[i], corners[(i + 1) % 6], color);
  }
}

/**
 * @brief Draw a filled hex using raylib
 * @param layout The layout configuration
 * @param h The hex coordinate
 * @param color Fill color
 */
inline void draw_hex_filled(Layout layout, Hex h, Color color) {
  vector<Point> corners = polygon_corners(layout, h);
  Point center = hex_to_pixel(layout, h);
  for (int i = 0; i < 6; i++) {
    DrawTriangle(center, corners[i], corners[(i + 1) % 6], color);
  }
}

// ============================================================================
// GRID SHAPE GENERATORS
// ============================================================================

/**
 * @brief Generate a parallelogram-shaped grid
 *
 * Creates a grid by iterating over q and r coordinates directly.
 * Works with either pointy-top or flat-top orientation.
 *
 * @param q1 Minimum q coordinate
 * @param q2 Maximum q coordinate
 * @param r1 Minimum r coordinate
 * @param r2 Maximum r coordinate
 * @return Vector of hex coordinates in the parallelogram
 */
inline vector<Hex> grid_parallelogram(int q1, int q2, int r1, int r2) {
  vector<Hex> hexes;
  for (int q = q1; q <= q2; q++) {
    for (int r = r1; r <= r2; r++) {
      hexes.push_back(Hex(q, r));
    }
  }
  return hexes;
}

/**
 * @brief Generate a downward-facing triangular grid
 *
 * For pointy-top: triangle points south
 * For flat-top: triangle points east
 *
 * @param size Size of the triangle (number of hexes on each edge - 1)
 * @return Vector of hex coordinates in the triangle
 */
inline vector<Hex> grid_triangle_down(int size) {
  vector<Hex> hexes;
  for (int q = 0; q <= size; q++) {
    for (int r = 0; r <= size - q; r++) {
      hexes.push_back(Hex(q, r));
    }
  }
  return hexes;
}

/**
 * @brief Generate an upward-facing triangular grid
 *
 * For pointy-top: triangle points north
 * For flat-top: triangle points west
 *
 * @param size Size of the triangle (number of hexes on each edge - 1)
 * @return Vector of hex coordinates in the triangle
 */
inline vector<Hex> grid_triangle_up(int size) {
  vector<Hex> hexes;
  for (int q = 0; q <= size; q++) {
    for (int r = size - q; r <= size; r++) {
      hexes.push_back(Hex(q, r));
    }
  }
  return hexes;
}

/**
 * @brief Generate a hexagonal-shaped grid
 *
 * Creates a hex-shaped region centered at the origin with the given radius.
 * Total hex count = 3*radius*(radius+1) + 1
 *
 * @param radius Distance from center to edge (0 = just center, 1 = 7 hexes, etc.)
 * @return Vector of hex coordinates in the hexagon
 */
inline vector<Hex> grid_hexagon(int radius) {
  vector<Hex> hexes;
  for (int q = -radius; q <= radius; q++) {
    int r1 = std::max(-radius, -q - radius);
    int r2 = std::min(radius, -q + radius);
    for (int r = r1; r <= r2; r++) {
      hexes.push_back(Hex(q, r));
    }
  }
  return hexes;
}

/**
 * @brief Generate a rectangular grid for pointy-top hexes
 *
 * Creates a visually rectangular grid when rendered with pointy-top orientation.
 * Parameters are in offset-like coordinates for intuitive rectangle definition.
 *
 * @param left Left bound (offset-style)
 * @param right Right bound (offset-style)
 * @param top Top bound (offset-style)
 * @param bottom Bottom bound (offset-style)
 * @return Vector of hex coordinates in the rectangle
 */
inline vector<Hex> grid_rectangle_pointy(int left, int right, int top, int bottom) {
  vector<Hex> hexes;
  for (int r = top; r <= bottom; r++) {
    int r_offset = (int)floor(r / 2.0);
    for (int q = left - r_offset; q <= right - r_offset; q++) {
      hexes.push_back(Hex(q, r));
    }
  }
  return hexes;
}

/**
 * @brief Generate a rectangular grid for flat-top hexes
 *
 * Creates a visually rectangular grid when rendered with flat-top orientation.
 * Parameters are in offset-like coordinates for intuitive rectangle definition.
 *
 * @param left Left bound (offset-style)
 * @param right Right bound (offset-style)
 * @param top Top bound (offset-style)
 * @param bottom Bottom bound (offset-style)
 * @return Vector of hex coordinates in the rectangle
 */
inline vector<Hex> grid_rectangle_flat(int left, int right, int top, int bottom) {
  vector<Hex> hexes;
  for (int q = left; q <= right; q++) {
    int q_offset = (int)floor(q / 2.0);
    for (int r = top - q_offset; r <= bottom - q_offset; r++) {
      hexes.push_back(Hex(q, r));
    }
  }
  return hexes;
}

// inline uint Layout::size() const {
//   switch (shape) {
//     case GridShape::Parallelogram:
//       return grid_parallelogram(params.a, params.b, params.c, params.d).size();
//     case GridShape::TriangleDown:
//       return grid_triangle_down(params.a).size();
//     case GridShape::TriangleUp:
//       return grid_triangle_up(params.a).size();
//     case GridShape::Hexagon:
//       return grid_hexagon(params.a).size();
//     case GridShape::RectanglePointy:
//       return grid_rectangle_pointy(params.a, params.b, params.c, params.d).size();
//     case GridShape::RectangleFlat:
//       return grid_rectangle_flat(params.a, params.b, params.c, params.d).size();
//   }
//   return 0;
// }

inline Hex Layout::operator[](uint index) const {
  vector<Hex> hexes;
  switch (shape) {
    case GridShape::Parallelogram:
      hexes = grid_parallelogram(params.a, params.b, params.c, params.d);
      break;
    case GridShape::TriangleDown:
      hexes = grid_triangle_down(params.a);
      break;
    case GridShape::TriangleUp:
      hexes = grid_triangle_up(params.a);
      break;
    case GridShape::Hexagon:
      hexes = grid_hexagon(params.a);
      break;
    case GridShape::RectanglePointy:
      hexes = grid_rectangle_pointy(params.a, params.b, params.c, params.d);
      break;
    case GridShape::RectangleFlat:
      hexes = grid_rectangle_flat(params.a, params.b, params.c, params.d);
      break;
  }
  if (index < hexes.size()) {
    return hexes[index];
  }
  return Hex(0, 0);
}

inline Vector3 hex_to_world(Layout layout, Hex h, float y = 0.0f) {
  Point p = hex_to_pixel(layout, h);
  return Vector3{p.x, y, p.y};
}

// Round fractional hex to nearest integer hex (cube coordinate rounding)
inline Hex hex_round(FractionalHex h) {
  int q = (int)round(h.q);
  int r = (int)round(h.r);
  int s = (int)round(h.s);
  double q_diff = abs(q - h.q);
  double r_diff = abs(r - h.r);
  double s_diff = abs(s - h.s);
  // Reset the component with largest rounding error
  if (q_diff > r_diff && q_diff > s_diff)
    q = -r - s;
  else if (r_diff > s_diff)
    r = -q - s;
  else
    s = -q - r;
  return Hex(q, r, s);
}

// Cast mouse ray onto XZ plane, return hex_id or UINT_MAX if missed
inline uint mouseray_hex(Layout layout, Camera3D camera) {
  /*
   uint hovered_id = mouseray_hex(this->hex_layout, camera);
    if (hovered_id != UINT_MAX && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
      for (auto& thing : things) {
        if (thing.hex_id == hovered_id) {
          higlighted_ref = thing.this_ref();
          break;
        }
      }
    }
 */

  Ray ray = GetScreenToWorldRay(GetMousePosition(), camera);
  // Intersect with y=0 plane
  if (fabsf(ray.direction.y) < 1e-6f)
    return UINT_MAX;
  float t = -ray.position.y / ray.direction.y;
  if (t < 0.0f)
    return UINT_MAX;
  // Hit point on XZ plane — maps to layout's 2D space as {x, z}
  Point hit = {
      ray.position.x + ray.direction.x * t,
      ray.position.z + ray.direction.z * t,
  };
  Hex h = hex_round(pixel_to_hex_fractional(layout, hit));
  // Find matching hex_id
  uint id = 0;
  for (auto valid : layout) {
    if (valid == h)
      return id;
    id++;
  }
  return UINT_MAX;
}

// ============================================================================
// DOCTEST - Unit and visual tests
// ============================================================================
#ifdef DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "imgui.h"
#include "rlImGui.h"
#include <doctest/doctest.h>

TEST_CASE("hex construction") {
  Hex h1(1, 2, -3);
  CHECK(h1.q == 1);
  CHECK(h1.r == 2);
  CHECK(h1.s == -3);

  // Two-arg constructor computes s automatically
  Hex h2(3, -1);
  CHECK(h2.s == -2); // s = -q - r = -3 - (-1) = -2
}

TEST_CASE("hex arithmetic") {
  Hex a(1, -3, 2);
  Hex b(3, -7, 4);

  Hex sum = hex_add(a, b);
  CHECK(sum.q == 4);
  CHECK(sum.r == -10);
  CHECK(sum.s == 6);

  Hex diff = hex_subtract(a, b);
  CHECK(diff.q == -2);
  CHECK(diff.r == 4);
  CHECK(diff.s == -2);

  Hex scaled = hex_multiply(a, 3);
  CHECK(scaled.q == 3);
  CHECK(scaled.r == -9);
  CHECK(scaled.s == 6);
}

TEST_CASE("hex equality") {
  Hex a(1, 2, -3);
  Hex b(1, 2, -3);
  Hex c(2, 1, -3);

  CHECK(a == b);
  CHECK(a != c);
}

TEST_CASE("hex length and distance") {
  CHECK(hex_length(Hex(0, 0, 0)) == 0);
  CHECK(hex_length(Hex(1, -1, 0)) == 1);
  CHECK(hex_length(Hex(3, -7, 4)) == 7);

  CHECK(hex_distance(Hex(0, 0, 0), Hex(0, 0, 0)) == 0);
  CHECK(hex_distance(Hex(0, 0, 0), Hex(3, -7, 4)) == 7);
}

TEST_CASE("hex directions and neighbors") {
  Hex origin(0, 0, 0);

  // Check all 6 directions
  CHECK(hex_neighbor(origin, HEX_E) == Hex(1, 0, -1));
  CHECK(hex_neighbor(origin, HEX_NE) == Hex(1, -1, 0));
  CHECK(hex_neighbor(origin, HEX_NW) == Hex(0, -1, 1));
  CHECK(hex_neighbor(origin, HEX_W) == Hex(-1, 0, 1));
  CHECK(hex_neighbor(origin, HEX_SW) == Hex(-1, 1, 0));
  CHECK(hex_neighbor(origin, HEX_SE) == Hex(0, 1, -1));
}

TEST_CASE("hex to pixel conversion") {
  Layout layout(layout_pointy, Point{30, 30}, Point{100, 100});
  Hex origin(0, 0, 0);

  Point p = hex_to_pixel(layout, origin);
  CHECK(p.x == doctest::Approx(100.0f));
  CHECK(p.y == doctest::Approx(100.0f));

  // Check that neighbors are roughly 30*sqrt(3) apart for pointy-top
  Hex east = Hex(1, 0, -1);
  Point pe = hex_to_pixel(layout, east);
  float dist = sqrt((pe.x - p.x) * (pe.x - p.x) + (pe.y - p.y) * (pe.y - p.y));
  CHECK(dist == doctest::Approx(30.0f * sqrt(3.0f)).epsilon(0.01));
}

TEST_CASE("pixel to hex roundtrip") {
  Layout layout_p(layout_pointy, Point{40, 40}, Point{200, 200});
  Layout layout_f(layout_flat, Point{40, 40}, Point{200, 200});

  for (int q = -3; q <= 3; q++) {
    for (int r = -3; r <= 3; r++) {
      if (abs(q + r) > 3)
        continue;

      Hex h(q, r);

      // Pointy layout roundtrip
      Point pp = hex_to_pixel(layout_p, h);
      FractionalHex fh_p = pixel_to_hex_fractional(layout_p, pp);
      Hex rp(static_cast<int>(round(fh_p.q)), static_cast<int>(round(fh_p.r)));
      CHECK(rp == h);

      // Flat layout roundtrip
      Point pf = hex_to_pixel(layout_f, h);
      FractionalHex fh_f = pixel_to_hex_fractional(layout_f, pf);
      Hex rf(static_cast<int>(round(fh_f.q)), static_cast<int>(round(fh_f.r)));
      CHECK(rf == h);
    }
  }
}

TEST_CASE("polygon corners") {
  Layout layout(layout_pointy, Point{30, 30}, Point{100, 100});
  Hex h(0, 0, 0);

  vector<Point> corners = polygon_corners(layout, h);
  CHECK(corners.size() == 6);

  // All corners should be 30 units from center (the size)
  Point center = hex_to_pixel(layout, h);
  for (const Point& c : corners) {
    float dist = sqrt((c.x - center.x) * (c.x - center.x) + (c.y - center.y) * (c.y - center.y));
    CHECK(dist == doctest::Approx(30.0f).epsilon(0.01));
  }
}

TEST_CASE("grid shape generators") {
  // Test parallelogram
  auto para = grid_parallelogram(0, 2, 0, 2);
  CHECK(para.size() == 9); // 3x3

  // Test triangle down
  auto tri_down = grid_triangle_down(2);
  CHECK(tri_down.size() == 6); // 1 + 2 + 3

  // Test triangle up
  auto tri_up = grid_triangle_up(2);
  CHECK(tri_up.size() == 6);

  // Test hexagon
  auto hex_grid = grid_hexagon(1);
  CHECK(hex_grid.size() == 7); // 1 center + 6 neighbors

  auto hex_grid_2 = grid_hexagon(2);
  CHECK(hex_grid_2.size() == 19); // 1 + 6 + 12

  // Test rectangles
  auto rect_p = grid_rectangle_pointy(0, 2, 0, 2);
  CHECK(rect_p.size() == 9);

  auto rect_f = grid_rectangle_flat(0, 2, 0, 2);
  CHECK(rect_f.size() == 9);
}

TEST_CASE("hex grid visual demo" * doctest::skip()) {
  // Run with: ./mylibs_tests --no-skip -tc="hex grid visual demo"

  const int screenWidth = 1280;
  const int screenHeight = 720;
  InitWindow(screenWidth, screenHeight, "Hex Grid Visual Test");
  SetTargetFPS(60);
  rlImGuiSetup(true);

  // Grid settings
  bool use_pointy = true;
  int current_shape = static_cast<int>(GridShape::Hexagon);
  int grid_size = 3;
  int rect_left = -2, rect_right = 2, rect_top = -2, rect_bottom = 2;
  bool show_coords = true;
  float hex_size = 30.0f;

  // Grid data
  vector<Hex> grid_hexes;
  bool needs_rebuild = true;

  Hex selected(0, 0);

  while (!WindowShouldClose()) {
    // Rebuild grid if settings changed
    if (needs_rebuild) {
      GridShape shape = static_cast<GridShape>(current_shape);
      switch (shape) {
        case GridShape::Parallelogram:
          grid_hexes = grid_parallelogram(-grid_size, grid_size, -grid_size, grid_size);
          break;
        case GridShape::TriangleDown:
          grid_hexes = grid_triangle_down(grid_size);
          break;
        case GridShape::TriangleUp:
          grid_hexes = grid_triangle_up(grid_size);
          break;
        case GridShape::Hexagon:
          grid_hexes = grid_hexagon(grid_size);
          break;
        case GridShape::RectanglePointy:
          grid_hexes = grid_rectangle_pointy(rect_left, rect_right, rect_top, rect_bottom);
          break;
        case GridShape::RectangleFlat:
          grid_hexes = grid_rectangle_flat(rect_left, rect_right, rect_top, rect_bottom);
          break;
      }
      needs_rebuild = false;
    }

    // Create layout based on current settings
    Layout layout(use_pointy ? layout_pointy : layout_flat, Point{hex_size, hex_size},
                  Point{screenWidth / 2.0f, screenHeight / 2.0f});

    // Mouse hover detection
    Point mouse = GetMousePosition();
    FractionalHex fh = pixel_to_hex_fractional(layout, mouse);
    Hex hovered(static_cast<int>(round(fh.q)), static_cast<int>(round(fh.r)));

    // Check if hovered hex is in the grid
    bool hovered_in_grid = false;
    for (const Hex& h : grid_hexes) {
      if (h == hovered) {
        hovered_in_grid = true;
        break;
      }
    }

    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && hovered_in_grid) {
      selected = hovered;
    }

    // Keyboard navigation
    if (IsKeyPressed(KEY_TAB)) {
      use_pointy = !use_pointy;
    }

    BeginDrawing();
    ClearBackground(DARKGRAY);

    // Draw all hexes in the grid
    for (const Hex& h : grid_hexes) {
      Color fill = DARKBLUE;
      if (h == selected)
        fill = YELLOW;
      else if (hovered_in_grid && h == hovered)
        fill = ColorBrightness(DARKBLUE, 0.3f);

      draw_hex_filled(layout, h, fill);
      draw_hex(layout, h, WHITE);

      if (show_coords) {
        Point center = hex_to_pixel(layout, h);
        DrawText(TextFormat("%d,%d", h.q, h.r), (int)center.x - 12, (int)center.y - 6, 10, WHITE);
      }
    }

    DrawText("Tab: toggle orientation | Click: select hex", 10, 10, 18, WHITE);
    DrawText(use_pointy ? "Pointy-top" : "Flat-top", 10, 35, 18, YELLOW);

    // ImGui controls
    rlImGuiBegin();
    if (ImGui::Begin("Grid Settings")) {
      ImGui::Text("FPS: %d", GetFPS());
      ImGui::Text("Hex count: %zu", grid_hexes.size());
      ImGui::Separator();

      // Orientation
      if (ImGui::Checkbox("Pointy-top", &use_pointy)) {
        // Orientation changed
      }

      // Hex size
      if (ImGui::SliderFloat("Hex Size", &hex_size, 15.0f, 60.0f)) {
        // Size changed
      }

      ImGui::Checkbox("Show Coordinates", &show_coords);
      ImGui::Separator();

      // Shape selector
      const char* shape_names[] = {"Parallelogram", "Triangle (Down)",    "Triangle (Up)",
                                   "Hexagon",       "Rectangle (Pointy)", "Rectangle (Flat)"};
      if (ImGui::Combo("Grid Shape", &current_shape, shape_names, IM_ARRAYSIZE(shape_names))) {
        needs_rebuild = true;
      }

      // Shape-specific parameters
      GridShape shape = static_cast<GridShape>(current_shape);
      if (shape == GridShape::RectanglePointy || shape == GridShape::RectangleFlat) {
        ImGui::Text("Rectangle bounds:");
        if (ImGui::SliderInt("Left", &rect_left, -5, 0))
          needs_rebuild = true;
        if (ImGui::SliderInt("Right", &rect_right, 0, 5))
          needs_rebuild = true;
        if (ImGui::SliderInt("Top", &rect_top, -5, 0))
          needs_rebuild = true;
        if (ImGui::SliderInt("Bottom", &rect_bottom, 0, 5))
          needs_rebuild = true;
      } else {
        if (ImGui::SliderInt("Grid Size", &grid_size, 1, 6)) {
          needs_rebuild = true;
        }
      }

      ImGui::Separator();
      ImGui::Text("Selected: (%d, %d, %d)", selected.q, selected.r, selected.s);
      ImGui::Text("Hovered: (%d, %d, %d)", hovered.q, hovered.r, hovered.s);
      if (hovered_in_grid) {
        ImGui::Text("Distance: %d", hex_distance(selected, hovered));
      }
    }
    ImGui::End();
    rlImGuiEnd();

    EndDrawing();
  }

  rlImGuiShutdown();
  CloseWindow();
  CHECK(true);
}

#endif
