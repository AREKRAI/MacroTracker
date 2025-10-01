#ifndef _H_DRAW_
#define _H_DRAW_

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <glad/gl.h>

#include <cglm/cglm.h>
#include <cglm/struct.h>

#include <ft2build.h>
#include FT_FREETYPE_H

#include <log.h>

#include "Common.h"
#include "UStr.h"

typedef struct __GlobalUBData_t {
  mat4 projectionView;
} _GlobalUBData_t;

typedef struct __Transform_t {
  vec2 position;
  vec2 scale;
  float rotation;
} Transform_t;

#define TRANSFORM_INIT (Transform_t) {   \
    .position = { 0.f, 0.f },            \
    .scale = {1.f, 1.f},                 \
    .rotation = 0.f                      \
  }

void Transform_toMat4(Transform_t *self, mat4 matrix);
void Transform_copy(Transform_t *self, Transform_t other);

typedef struct __LocalUBData2D_t {
  mat4 model;
  vec4 color;
} _LocalUBData2D_t;

typedef uint32_t u32vec2[2];

typedef struct __Glyph_t {
  UC_t character;
  vec2 size;
  vec2 bearing;
  vec2 advance;

  bool isWhitespace;

  GLuint glTextureHandle;
} _Glyph_t;

typedef union _SizeVec2_t {
  struct {
    size_t x, y;
  };

  struct {
    size_t width, height;
  };

  size_t ptr[2];
} SizeVec2_t;

typedef struct __Draw_t {
  FT_Library _ft;
  FT_Face _ftFace;

  // TODO: implement flatShader
  GLuint _texShader, _flatShader;
  GLuint _quadVAO, _quadVBO, _quadEBO;

  mat4 _projection, _view;

  GLuint _globalUB, _localUB;
  _GlobalUBData_t _globalUBData;

  // Currently linear search
  _Glyph_t *_glyphs;
  size_t _glyphCap, _glyphCount;

  double _lastTime;
  double _deltaTime;
} Draw_t;

Result_t Draw_init(Draw_t *self, GLFWwindow *wndHandle);
void Draw_cleanup(Draw_t *self);

#endif