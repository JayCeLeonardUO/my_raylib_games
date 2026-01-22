#pragma once
#include <cmath>
#include <cstdlib>
#include <raylib.h>

// ============================================================================
// HEX GRID MATH (pointy-top hexagons)
// ============================================================================

struct HexGridConfig {
  int rows;
  int cols;
  Vector2 xy_bounds; // width, height of the area to fill
};

struct HexDims {
  float radius = 1;     // distance from center to vertex
  float width = 1.732f; // sqrt(3) * radius
  float height = 2.0f;  // 2 * radius
  float horiz_spacing;  // horizontal distance between hex centers
  float vert_spacing;   // vertical distance between hex centers
};

// Calculate hex dimensions to fit grid within bounds
inline HexDims hexDims_from_config(HexGridConfig config) {
  HexDims dims;

  // For pointy-top hexes:
  // - width = sqrt(3) * radius
  // - height = 2 * radius
  // - horiz spacing = width
  // - vert spacing = height * 0.75
  // - odd rows offset by width/2

  // Calculate radius to fit cols horizontally
  // Total width = (cols * width) + (width/2 for offset)
  // width = sqrt(3) * radius
  float width_for_cols = config.xy_bounds.x / (config.cols + 0.5f);
  float radius_from_width = width_for_cols / 1.732050808f;

  // Calculate radius to fit rows vertically
  // Total height = radius + (rows-1) * (1.5 * radius) + radius
  //              = 2*radius + (rows-1) * 1.5 * radius
  //              = radius * (2 + 1.5*(rows-1))
  //              = radius * (0.5 + 1.5*rows)
  float radius_from_height = config.xy_bounds.y / (0.5f + 1.5f * config.rows);

  // Use the smaller radius to fit both dimensions
  dims.radius = fminf(radius_from_width, radius_from_height);
  dims.width = 1.732050808f * dims.radius;
  dims.height = 2.0f * dims.radius;
  dims.horiz_spacing = dims.width;
  dims.vert_spacing = dims.height * 0.75f;

  return dims;
}

// Calculate hex center positions for a grid
// Returns array of Vector2[rows * cols] - caller must free()
inline Vector2* calculate_hexgrid(HexGridConfig config, HexDims dims) {
  int count = config.rows * config.cols;
  Vector2* positions = (Vector2*)malloc(count * sizeof(Vector2));
  if (!positions)
    return nullptr;

  // Center the grid within bounds
  float grid_width = (config.cols - 1) * dims.horiz_spacing + dims.width / 2.0f;
  float grid_height = (config.rows - 1) * dims.vert_spacing + dims.height;
  float offset_x = (config.xy_bounds.x - grid_width) / 2.0f + dims.radius;
  float offset_y = (config.xy_bounds.y - grid_height) / 2.0f + dims.radius;

  for (int row = 0; row < config.rows; row++) {
    for (int col = 0; col < config.cols; col++) {
      int idx = row * config.cols + col;

      float x = offset_x + col * dims.horiz_spacing;
      // Offset odd rows by half width
      if (row % 2 == 1) {
        x += dims.horiz_spacing / 2.0f;
      }
      float y = offset_y + row * dims.vert_spacing;

      positions[idx] = {x, y};
    }
  }

  return positions;
}

// Get index from row/col
inline int hex_index(int row, int col, int cols) {
  return row * cols + col;
}

// Get row/col from index
inline void hex_rowcol(int index, int cols, int* row, int* col) {
  *row = index / cols;
  *col = index % cols;
}

// Draw a pointy-top hexagon outline
inline void draw_hex(Vector2 center, float radius, Color color) {
  for (int i = 0; i < 6; i++) {
    float angle1 = (60.0f * i - 30.0f) * DEG2RAD;
    float angle2 = (60.0f * (i + 1) - 30.0f) * DEG2RAD;
    Vector2 p1 = {center.x + radius * cosf(angle1), center.y + radius * sinf(angle1)};
    Vector2 p2 = {center.x + radius * cosf(angle2), center.y + radius * sinf(angle2)};
    DrawLineV(p1, p2, color);
  }
}

// Draw a filled pointy-top hexagon
inline void draw_hex_filled(Vector2 center, float radius, Color color) {
  Vector2 points[6];
  for (int i = 0; i < 6; i++) {
    float angle = (60.0f * i - 30.0f) * DEG2RAD;
    points[i] = {center.x + radius * cosf(angle), center.y + radius * sinf(angle)};
  }
  // Draw as triangles from center
  for (int i = 0; i < 6; i++) {
    DrawTriangle(center, points[i], points[(i + 1) % 6], color);
  }
}
