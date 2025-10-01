#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <glad/gl.h>

#include <cglm/cglm.h>
#include <cglm/struct.h>

#include <ft2build.h>
#include FT_FREETYPE_H

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <log.h>

#include "MacroDatabase.h"
#include "Common.h"
#include "UStr.h"
#include "UI.h"
#include "Draw.h"

typedef struct __AppInfo_t {
  const char *name;
  SizeVec2_t size;
} AppInfo_t;

#define DEFAULT_APP_NAME "macro"
#define DEFAULT_WINDOW_WIDTH 1024
#define DEFAULT_WINDOW_HEIGHT 1024

#define APP_INFO_INIT (AppInfo_t) {                          \
    .name = DEFAULT_APP_NAME,                                \
    .size = { DEFAULT_WINDOW_WIDTH, DEFAULT_WINDOW_HEIGHT }, \
  }

typedef struct __App_t {
  GLFWwindow *_wnd;
  bool _running;

  Draw_t draw;

  double _lastTime;
  double _deltaTime;

  bool _camMoving;
  vec2 camera, _camNew, _camStart;

  vec2 _mouseStart;
  vec2 _spritePosition;

  UStr_t input;
  size_t _inCount, _inCap;

  EventQueue_t _evQueue;
  UI_t _uiRoot;

  HANDLE _renderThread;
  AppInfo_t info;
} App_t;

#define FPS_LIMIT 144
#define FRAME_TIME (1.0 / FPS_LIMIT)

bool Draw_frameTimeElapsed(struct __Draw_t *draw) {
  double newTime = glfwGetTime();
  double delta = newTime - draw->_lastTime;
  if (delta < FRAME_TIME) {
    return false;
  }

  draw->_deltaTime = delta;
  draw->_lastTime = newTime;
  return true;
}

void Draw_timeInit(struct __Draw_t *draw) {
  draw->_lastTime = glfwGetTime();
  draw->_deltaTime = 0.0;
}

bool __App_frameTimeElapsed(App_t *app) {
  double newTime = glfwGetTime();
  double delta = newTime - app->_lastTime;
  if (delta < FRAME_TIME) {
    return false;
  }

  app->_deltaTime = delta;
  app->_lastTime = newTime;
  return true;
}

#define BASE_SPEED 2.f

void _App_wndCloseCBCK(GLFWwindow* window) {
  App_t *app = glfwGetWindowUserPointer(window);

  glfwSetWindowShouldClose(window, GLFW_TRUE);
  app->_running = false;
}

#define CAMERA_MOVE_STRENGTH 50.f

inline void _App_getMouseScreenPosition(App_t *app, vec2 result) {
  double my, mx;
  glfwGetCursorPos(app->_wnd, &mx, &my);

  int fbW = 0, fbH = 0;
  glfwGetFramebufferSize(app->_wnd, &fbW, &fbH);

  result[0] = (2.f * mx - fbW) / fbW ;
  result[1] = -(2.f * my) / fbH;
}

inline void _App_getMouseScreenNormalizedPosition(App_t *app, vec2 result) {
  int fbW = 0, fbH = 0;
  glfwGetFramebufferSize(app->_wnd, &fbW, &fbH);
  float aspect = fbW / (float)fbH;

  // Normalize for aspect
  _App_getMouseScreenPosition(app, result);
  result[0] *= aspect;
}

inline void _App_getMouseScreenNormalizedCentered(App_t *app, vec2 result) {
  int fbW = 0, fbH = 0;
  glfwGetFramebufferSize(app->_wnd, &fbW, &fbH);
  float aspect = fbW / (float)fbH;

  // Normalize for aspect
  _App_getMouseScreenPosition(app, result);
  result[0] *= aspect;
  result[1] += 1.0f;
}

inline void _App_getMouseWorldPosition(App_t *app, vec2 result) {
  int fbW = 0, fbH = 0;
  glfwGetFramebufferSize(app->_wnd, &fbW, &fbH);
  float aspect = fbW / (float)fbH;

  vec2 screenPosition = {0};
  // Normalize for aspect
  _App_getMouseScreenPosition(app, screenPosition);
  screenPosition[0] *= aspect;

  mat4 camInv = GLM_MAT4_IDENTITY_INIT;
  glm_mat4_inv(app->draw._view, camInv);

  vec3 cursorPos = {0, 0, 0};
  glm_mat4_mulv3(
    camInv,
    (vec3) {screenPosition[0], screenPosition[1], 0.f}, 1.f,
    cursorPos
  );

  result[0] = cursorPos[0];
  result[1] = cursorPos[1];
}

void  _App_updateCamera(App_t *app) {
  if (app->_camMoving) {
    vec2 cPos = {0};
    _App_getMouseWorldPosition(app, cPos);

    vec2 delta = {0};
    glm_vec2_sub(cPos, app->_mouseStart, delta);
    
    glm_vec2_scale(delta, -CAMERA_MOVE_STRENGTH * app->_deltaTime, delta);
    glm_vec2_sub(app->_camStart, delta, app->_camNew);
  }

  glm_vec2_lerp(
    (vec2) { app->camera[0], app->camera[1] },
    app->_camNew,
    app->_deltaTime,
    app->camera
  );
}

void _App_wndMouseBtnCBCK(GLFWwindow* window, int button, int action, int mods) {
  if (button != GLFW_MOUSE_BUTTON_1) {
    return;
  }

  App_t *app = glfwGetWindowUserPointer(window);
  if (action == GLFW_PRESS) {
    _App_getMouseWorldPosition(app, app->_mouseStart);

    memcpy(app->_camStart, app->camera, sizeof(vec2));
    memcpy(app->_camNew, app->camera, sizeof(vec2));
    app->_camMoving = true;
  } else {
    app->_camMoving = false;
  }

  if (button != GLFW_MOUSE_BUTTON_1)
    return;

  if (action != GLFW_RELEASE)
    return;

  Event_t payload = {
    .category = EVENT_CAT_INPUT,
    .type = EVENT_TYPE_CLICK,
    .position = {0}
  };
  _App_getMouseScreenNormalizedCentered(app, payload.position);
  EventQueue_push(&app->_evQueue, &payload);
  // _App_UI_onClick(app, &app->_uiRoot, cPos);
}

void _App_wndInputCBCK(GLFWwindow* window,
    int key, int scancode, int action, int mods) {
  App_t *app = glfwGetWindowUserPointer(window);

  Event_t payload = {
    .category = EVENT_CAT_INPUT,
    .type = EVENT_TYPE_KEY,

    .glfwAction = action,
    .glfwKey = key
  };
  EventQueue_push(&app->_evQueue, &payload);

  if (action != GLFW_PRESS && action != GLFW_REPEAT)
    return;

  if (key == GLFW_KEY_BACKSPACE) {
    UStr_trimEnd(&app->input, 1);
  }
  
  if (action != GLFW_PRESS)
    return;

  if (key != GLFW_KEY_ESCAPE)
    return;

  glfwSetWindowShouldClose(window, GLFW_TRUE);
  app->_running = false;
}

#define SHADER_DIR "glsl\\"
#define VERT_FILE_NAME "vert_default_2d.glsl"
#define FRAG_FILE_NAME "frag_default_2d.glsl"

#ifdef APP_DEBUG
void APIENTRY _App_OpenGL_debugMsgCallback(GLenum source, GLenum type, GLuint id,
  GLenum severity, GLsizei length, const GLchar *message, 
  const void *userParam) {
  // App_t *app = (App_t *)userParam;

  const char *sourceName = "API";
  switch (source) {
    case GL_DEBUG_SOURCE_OTHER:
      sourceName = "OTHER";
      break;
    case GL_DEBUG_SOURCE_APPLICATION:
      sourceName = "APP";
      break;
    case GL_DEBUG_SOURCE_THIRD_PARTY:
      sourceName = "THIRD PARTY";
      break;
    case GL_DEBUG_SOURCE_SHADER_COMPILER:
      sourceName = "SHADER";
      break;
    case GL_DEBUG_SOURCE_WINDOW_SYSTEM:
      sourceName = "WINDOW";
      break;
    case GL_DEBUG_SOURCE_API:
    default:
      break;
  }

  const char *typeName = "Unknown";
  switch (type) {
  case GL_DEBUG_TYPE_ERROR:
    typeName = "Error";
    break;
  case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR:
    typeName = "Deprecated";
    break;
  case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:
    typeName = "Undefined";
    break;
  case GL_DEBUG_TYPE_PORTABILITY:
    typeName = "Portability";
    break;
  case GL_DEBUG_TYPE_PERFORMANCE:
    typeName = "Performance";
    break;
  case GL_DEBUG_TYPE_MARKER:
    typeName = "Marker";
    break;
  case GL_DEBUG_TYPE_PUSH_GROUP:
    typeName = "Push Group";
    break;
  case GL_DEBUG_TYPE_POP_GROUP:
    typeName = "Pop Group";
    break;
  case GL_DEBUG_TYPE_OTHER:
  default:
    typeName = "Other";
    break;
  }

  switch (severity) {
    case GL_DEBUG_SEVERITY_HIGH:
      log_error("[OpenGL (%s) -> %s (id: %i)] %s" ENDL,
        typeName, sourceName, id, message
      );
      break;
    case GL_DEBUG_SEVERITY_MEDIUM:
      log_warn("[OpenGL (%s) -> %s (id: %i)] %s" ENDL,
        typeName, sourceName, id, message
      );
      break;
    case GL_DEBUG_SEVERITY_LOW:
    case GL_DEBUG_SEVERITY_NOTIFICATION:
      log_info("[OpenGL (%s) -> %s (id: %i)] %s" ENDL,
        typeName, sourceName, id, message
      );
    default:
      break;
  }

}
#endif

int32_t __intToNextPow2(int32_t val) {
  int32_t out = 1;
  
  while (out < val) {
    out <<= 1;
  }

  return out;
}

Result_t _OpenGL_validateAndLogError(GLuint shader, const char *shaderFileName,
  GLchar **p_charBuff, GLint *p_buffSize) {
  GLint shaderStatus = 0;

  glGetShaderiv(shader, GL_COMPILE_STATUS, &shaderStatus);
  if (shaderStatus != GL_TRUE) {
    GLint requiredLogLength = 0;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &requiredLogLength);

    GLint greaterSize = requiredLogLength > *p_buffSize ? 
      requiredLogLength : *p_buffSize;

    while (*p_buffSize < greaterSize) {
      *p_buffSize <<= 1;
    }
    
    *p_charBuff = realloc(*p_charBuff, *p_buffSize);

    glGetShaderInfoLog(shader, *p_buffSize, NULL, *p_charBuff);

    log_error("(%u : %s) compilation error: \"%s\"" ENDL,
      shader,
      shaderFileName,
      *p_charBuff
    );

    return RESULT_FAIL;
  }

  return RESULT_SUCCESS;
}

#ifdef APP_DEBUG
#define OPENGL_VALIDATE_AND_LOG_ERROR(sha, fname, p_cbuf, p_bufSize) _OpenGL_validateAndLogError(sha, fname, p_cbuf, p_bufSize)
#else
#define OPENGL_VALIDATE_AND_LOG_ERROR(sha, fname, p_cbuf, p_bufSize)
#endif

void _App_OpenGlCleanup(App_t *app) {
  glDeleteProgram(app->draw._texShader);
  glDeleteProgram(app->draw._flatShader);

  glDeleteBuffers(1, &app->draw._quadEBO);
  glDeleteBuffers(1, &app->draw._quadVBO);

  glDeleteVertexArrays(1, &app->draw._quadVAO);

  glDeleteBuffers(1, &app->draw._globalUB);
}

typedef struct _Vertex2D_t {
  vec2 position;
  vec2 tex;
} Vertex2D_t;

Result_t __Draw_loadShaderStringFromFiles(GLuint *p_program, const char *vert, const char *frag) {
  FILE *vertFile, *fragFile;
  GLint vertFileSize = 0, fragFileSize = 0;

  errno_t err = 0;
  if ((err = fopen_s(&vertFile, vert, "rb")) != 0 ||
    vertFile == NULL) {
    log_error(
      "Couldn't create/open the vertex shader file %s, error code: %i" ENDL,
      vert,
      err
    );

    return RESULT_FAIL;
  } else {
    log_info("Opened vertex shader file: %s" ENDL, vert);
  }
  
  if ((err = fopen_s(&fragFile, frag, "rb")) != 0 || 
    fragFile == NULL) {
    log_error(
      "Couldn't create/open the fragment shader file %s, error code: %i" ENDL,
      frag,
      err
    );

    fclose(vertFile);
    return RESULT_FAIL;
  } else {
    log_info("Opened fragment shader file: %s" ENDL, frag);
  }

  char *charBuff = NULL;

  fseek(vertFile, 0L, SEEK_END);
  vertFileSize = ftell(vertFile) + 1;

  fseek(fragFile, 0L, SEEK_END);
  fragFileSize = ftell(fragFile) + 1;

  GLint charCount = fragFileSize > vertFileSize ?
    fragFileSize : vertFileSize;

  charCount = __intToNextPow2(charCount);
  charBuff = malloc(charCount);
  memset(charBuff, 0, charCount);

  rewind(vertFile);
  
  GLint readFileSize = (GLint)fread(charBuff,
    sizeof(char), 
    vertFileSize - 1, 
    vertFile
  );
  charBuff[vertFileSize - 1] = '\0';

  GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
  glShaderSource(vertexShader, 1, &charBuff, &readFileSize);
  glCompileShader(vertexShader);
  fclose(vertFile);

#ifdef APP_DEBUG
  Result_t vertResult = OPENGL_VALIDATE_AND_LOG_ERROR(vertexShader, 
    vert, &charBuff, &charCount
  );

  if (!vertResult) {
    log_error("Failed to compile vertex shader (%u : %s)" ENDL,
      vertexShader,
      vert
    );

    return RESULT_FAIL;
  } else {
    log_info("Successfully loaded vertex shader (%u : %s)" ENDL,
      vertexShader,
      vert
    );
  }
#endif

  rewind(fragFile);
  // memset(charBuff, 0, charCount);
  readFileSize = (GLint)fread(charBuff,
    sizeof(char),
    fragFileSize - 1, 
    fragFile
  );
  charBuff[fragFileSize - 1] = '\0';

  GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
  glShaderSource(fragmentShader, 1, &charBuff, &readFileSize);
  glCompileShader(fragmentShader);
  fclose(fragFile);

#ifdef APP_DEBUG
  Result_t fragResult = OPENGL_VALIDATE_AND_LOG_ERROR(fragmentShader,
    frag, &charBuff, &charCount
  );

  if (!fragResult) {
    log_error("Failed to compile fragment shader (%u : %s)" ENDL,
      fragmentShader, frag
    );
    return RESULT_FAIL;
  } else {
    log_info("Successfully loaded fragment shader (%u : %s)" ENDL,
      fragmentShader, frag
    );
  }
#endif

  *p_program = glCreateProgram();
  glAttachShader(*p_program, vertexShader);
  glAttachShader(*p_program, fragmentShader);
  glLinkProgram(*p_program);

  glDeleteShader(vertexShader);
  glDeleteShader(fragmentShader);

#ifdef APP_DEBUG
  GLint linkStatus = GL_FALSE;
  glGetProgramiv(*p_program, GL_LINK_STATUS, &linkStatus);
  if (linkStatus != GL_TRUE) {
    GLint requiredLogLength = 0;
    glGetProgramiv(*p_program, GL_INFO_LOG_LENGTH,
      &requiredLogLength
    );

    GLint greaterSize = requiredLogLength > charCount ? 
      requiredLogLength : charCount;

    while (charCount < greaterSize) {
      charCount <<= 1;
    }
    
    charBuff = realloc(charBuff, charCount);

    glGetShaderInfoLog(*p_program, charCount, NULL,
      charBuff);

    log_error("(%u - %s : %s) link error: \"%s\"" ENDL,
      p_program,
      vert,
      frag,
      charBuff
    );

    return RESULT_FAIL;
  } else {
    log_info("Successfully linked shader program (%u - %s : %s)" ENDL,
      *p_program,
      vert,
      frag
    );
  }
#endif
  return RESULT_SUCCESS;
}

Result_t _App_initOpenGL(App_t *app) {
#ifdef APP_DEBUG
  glEnable(GL_DEBUG_OUTPUT);
  glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
  glDebugMessageCallback(_App_OpenGL_debugMsgCallback, NULL);
#endif

  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  Vertex2D_t vertices[] = {
    { .position = {-0.5f, -0.5f}, .tex = {0, 1} },  // bottom left
    { .position = { 0.5f, -0.5f}, .tex = {1, 1} },  // bottom right
    { .position = { 0.5f,  0.5f}, .tex = {1, 0} },  // top right
    { .position = {-0.5f,  0.5f}, .tex = {0, 0} }   // top left
  };
  
  unsigned int indices[] = {
      0, 1, 2,   // first triangle
      2, 3, 0    // second triangle
  };

  glCreateVertexArrays(1, &app->draw._quadVAO);
  
  glCreateBuffers(1, &app->draw._quadVBO);
  glNamedBufferData(app->draw._quadVBO, sizeof(vertices),
    vertices, GL_STATIC_DRAW
  );
  glVertexArrayVertexBuffer(app->draw._quadVAO, 0, app->draw._quadVBO,
    0, sizeof(Vertex2D_t)
  );

  glCreateBuffers(1, &app->draw._quadEBO);
  glNamedBufferData(app->draw._quadEBO, sizeof(indices),
    indices, GL_STATIC_DRAW
  );
  glVertexArrayElementBuffer(app->draw._quadVAO, app->draw._quadEBO);

  glVertexArrayAttribFormat(app->draw._quadVAO, 0, 3, GL_FLOAT,
    GL_FALSE, offsetof(Vertex2D_t, position)
  );
  glVertexArrayAttribBinding(app->draw._quadVAO, 0, 0);
  glEnableVertexArrayAttrib(app->draw._quadVAO, 0);

  glVertexArrayAttribFormat(app->draw._quadVAO, 1, 2, GL_FLOAT,
    GL_FALSE, offsetof(Vertex2D_t, tex)
  );
  glVertexArrayAttribBinding(app->draw._quadVAO, 1, 0);
  glEnableVertexArrayAttrib(app->draw._quadVAO, 1);

  __Draw_loadShaderStringFromFiles(
    &app->draw._texShader, 
    SHADER_DIR VERT_FILE_NAME, 
    SHADER_DIR FRAG_FILE_NAME
  );

  __Draw_loadShaderStringFromFiles(
    &app->draw._flatShader,
    SHADER_DIR "vert_flat_2d.glsl",
    SHADER_DIR "frag_flat_2d.glsl"
  );
  
  glCreateBuffers(1, &app->draw._globalUB);
  glNamedBufferData(app->draw._globalUB, sizeof(_GlobalUBData_t),
    &app->draw._globalUBData, GL_STATIC_DRAW
  );

  glCreateBuffers(1, &app->draw._localUB);
  glNamedBufferData(app->draw._localUB,
    sizeof(_LocalUBData2D_t), NULL, GL_STATIC_DRAW
  );

  return RESULT_SUCCESS;
}

#define FONT_PATH "fonts/NotoSans-SemiBold.ttf"
#define FONT_SIZE (1 << 7)
#define ATLAS_START_SIZE (SizeVec2_t) { 1 << 12, 1 << 12 }


_Glyph_t *_Draw_getGlyph(App_t *app, UC_t character) {
  if (character < app->draw._glyphCount) {
    _Glyph_t *sample = &app->draw._glyphs[character];

    if (sample->character == character) {
      return sample;
    }
  }

  size_t ucharInd = 0;
  for (;ucharInd < app->draw._glyphCount; ucharInd++) {
    _Glyph_t *iglyph = &app->draw._glyphs[ucharInd];
    if (iglyph->character == character) {
      return iglyph;
    }

    if (iglyph->character > character) {
      return NULL;
    }
  }

  return NULL;
}

_Glyph_t *_Draw_getGlyphOrLoad(App_t *app, UC_t character) {
  _Glyph_t *result = _Draw_getGlyph(app, character);
  if (result != NULL) {
    return result;
  }

  _Draw_loadGlyph(app, character);
  return _Draw_getGlyph(app, character);
}

Result_t _Draw_loadGlyph(App_t *app, UC_t character) {
    if (FT_Load_Char(app->draw._ftFace, character, FT_LOAD_RENDER)) {
      log_warn("Failed to load char %c of font " FONT_PATH ENDL);
      return RESULT_FAIL;
    }

    // generate texture
    GLuint texture = 0;
    if (app->draw._ftFace->glyph->bitmap.buffer == NULL)
      goto skip_glyph_texture_creation;

    glCreateTextures(GL_TEXTURE_2D, 1, &texture);
    glTextureStorage2D(texture, 1, GL_R8,
      app->draw._ftFace->glyph->bitmap.width, app->draw._ftFace->glyph->bitmap.rows
    );

    // set texture options
    glTextureParameteri(texture, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTextureParameteri(texture, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTextureParameteri(texture, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTextureParameteri(texture, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glTextureSubImage2D(texture, 0, 
      0, 0,
      app->draw._ftFace->glyph->bitmap.width, app->draw._ftFace->glyph->bitmap.rows,
      GL_RED, GL_UNSIGNED_BYTE, app->draw._ftFace->glyph->bitmap.buffer
    );

skip_glyph_texture_creation:
    _Glyph_t glyph = {
      .character = character,
      .glTextureHandle = texture,
      .isWhitespace = app->draw._ftFace->glyph->bitmap.buffer == NULL,
      .size = { 
        app->draw._ftFace->glyph->bitmap.width / (float)FONT_SIZE,
        app->draw._ftFace->glyph->bitmap.rows / (float)FONT_SIZE
      },
      .bearing = { 
        app->draw._ftFace->glyph->bitmap_left / (float)FONT_SIZE,
        app->draw._ftFace->glyph->bitmap_top / (float)FONT_SIZE
      },
      .advance = { 
        (uint32_t)(app->draw._ftFace->glyph->advance.x >> 6) / (float)FONT_SIZE,
        (uint32_t)(app->draw._ftFace->glyph->advance.y >> 6) / (float)FONT_SIZE
      }
    };

    app->draw._glyphCount++;

    while (app->draw._glyphCap < app->draw._glyphCount * sizeof(_Glyph_t)) {
      app->draw._glyphCap <<= 1;
    }
    app->draw._glyphs = realloc(app->draw._glyphs, app->draw._glyphCap);

    size_t ucharInd = 0;
    for (;ucharInd < app->draw._glyphCount - 1; ucharInd++) {
      _Glyph_t *iglyph = &app->draw._glyphs[ucharInd];
      if (iglyph->character == character) {
        log_error("Unicode character has already been loaded" ENDL);
        return RESULT_FAIL;
      }

      if (iglyph->character > character) {
        break;
      }
    }

    size_t spaceDiff = app->draw._glyphCount - ucharInd - 1;
    if (spaceDiff > 0) {
      memcpy(
        &app->draw._glyphs[ucharInd + 1],
        &app->draw._glyphs[ucharInd],
        sizeof(_Glyph_t)
      );
    }

    app->draw._glyphs[ucharInd] = glyph;

    return RESULT_SUCCESS;
}

#define ASCII_LOAD_LIMIT 128

Result_t _Draw_loadAsciiGlyphs(App_t *app) {
  if (FT_Init_FreeType(&app->draw._ft) != 0) {
    log_error("Failed to init/load freetype library" ENDL);
    return RESULT_FAIL;
  }

  if (FT_New_Face(app->draw._ft, FONT_PATH, 0, &app->draw._ftFace)) {
    log_error("Failed to load font at: " FONT_PATH ENDL);
    return RESULT_FAIL;
  }

  FT_Set_Pixel_Sizes(app->draw._ftFace, 0, FONT_SIZE);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

  // TODO:
  // Create texture atlas optimisations

  app->draw._glyphCount = 0,
  app->draw._glyphCap = DEFAULT_BUF_CAP,
  app->draw._glyphs = malloc(app->draw._glyphCap),

  memset(app->draw._glyphs, 0, app->draw._glyphCap);
  for (UC_t character = 0; character < ASCII_LOAD_LIMIT; character++) {
    _Draw_loadGlyph(app, character);
  }

  return RESULT_SUCCESS;
}

void _App_cleanupTextRenderer(App_t *app) {
  FT_Done_Face(app->draw._ftFace);
  FT_Done_FreeType(app->draw._ft);

  for (size_t glyphIndex = 0; glyphIndex < app->draw._glyphCount; glyphIndex++) {
    _Glyph_t *glyph = &app->draw._glyphs[glyphIndex];
    glDeleteTextures(1, &glyph->glTextureHandle);
  }
  free(app->draw._glyphs);
}

typedef struct __TextInfo_t {
  uint32_t fontSize;
  float horSpacing;
  float vertSpacing;
} TextInfo_t;

#define TEXT_INFO_INIT (TextInfo_t) { \
  .fontSize = 28,                     \
  .horSpacing = 0.f,                 \
  .vertSpacing = 0.f,                \
  }

void _App_wndCharCBCK(GLFWwindow* window, unsigned int character) {
  App_t *app = glfwGetWindowUserPointer(window);
  UStr_pushUC(&app->input, character);

  Event_t payload = {
    .category = EVENT_CAT_INPUT,
    .type = EVENT_TYPE_CHAR_INPUT,
    .character = character
  };
  EventQueue_push(&app->_evQueue, &payload);
}

void _Draw_text(App_t *app, UStr_t *str,
  const Transform_t transform, const TextInfo_t info) {
  glUseProgram(app->draw._texShader);
  glBindBufferBase(GL_UNIFORM_BUFFER, 0, app->draw._globalUB);
  glBindVertexArray(app->draw._quadVAO);

  _LocalUBData2D_t ubData = {
    .color = COLOR_RED,
    .model = GLM_MAT4_IDENTITY_INIT
  };
  vec2 lPos = { transform.position[0], transform.position[1] };

  int wwidth = 0, wheight = 0;
  glfwGetFramebufferSize(app->_wnd, &wwidth, &wheight);
  const float normalizationFactor = info.fontSize / 
    ((float)FONT_SIZE * wheight);
  // DOESN'T TAKE X/Y SPACING INTO ACCOUNT
  const float scaleX = transform.scale[0] * normalizationFactor;
  const float scaleY = transform.scale[1] * normalizationFactor;

  uint32_t peakYAdvance = 0;
  float peakYBearing = 0;

  for (UC_t *code_point = str->str; code_point < &str->str[str->count + 1]; code_point++) {
      _Glyph_t *glyph = _Draw_getGlyphOrLoad(app, *code_point);
      lPos[0] += (glyph->advance[0] / 2.f) * scaleX;

      if (glyph->isWhitespace || *code_point == '\n')
        goto skip_glyph_rendering;

      Transform_t trans = {
        .rotation = transform.rotation,
        .position = {
          lPos[0] + glyph->bearing[0] * scaleX,
          lPos[1] - (glyph->size[1] / 2.f - glyph->bearing[1]) * scaleY
        },
        .scale = {
          glyph->size[0] * scaleX,
          glyph->size[1] * scaleY
        }
      };

      Transform_toMat4(&trans, ubData.model);

      glNamedBufferSubData(app->draw._localUB, 0,
        sizeof(_LocalUBData2D_t), &ubData
      );
      glBindBufferBase(GL_UNIFORM_BUFFER, 1, app->draw._localUB);
      glBindTextureUnit(0, glyph->glTextureHandle);
      glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
      
skip_glyph_rendering:
      lPos[0] += (glyph->advance[0] / 2.f + info.horSpacing) * scaleX;
      
      peakYAdvance = glyph->advance[1] > peakYAdvance ?
        glyph->advance[1] : peakYAdvance;

      peakYBearing = glyph->bearing[1] > peakYBearing ?
        glyph->bearing[1] : peakYBearing;
      if (*code_point == '\n') {
        lPos[1] -= (peakYAdvance + peakYBearing + info.vertSpacing) * scaleY;
        lPos[0] = transform.position[0];
      }
  }
}

void _Draw_calculateUiTextSize(App_t* app, UI_t *ui, vec2 out) {
  UiText_t *text = ui->_unique;

  out[0] = 0;
  out[1] = 0;
  // TODO: add font size to the text struct

  uint32_t peakYAdvance = 0;
  float peakYBearing = 0;

  for (UC_t *code_point = text->str.str; code_point < &text->str.str[text->str.count + 1]; code_point++) {
    _Glyph_t *glyph = _Draw_getGlyphOrLoad(app, *code_point);
    out[0] += glyph->advance[0] / 2.f;
    out[0] += glyph->advance[0] / 2.f;
    
    peakYAdvance = glyph->advance[1] > peakYAdvance ?
      glyph->advance[1] : peakYAdvance;

    peakYBearing = glyph->bearing[1] > peakYBearing ?
      glyph->bearing[1] : peakYBearing;
    if (*code_point == '\n') {
      out[1] -= (peakYAdvance  + peakYBearing);
      out[0] = 0;
    }
  }
}

void _Draw_uiText(App_t* app, UI_t *ui) {
  UiText_t *text = ui->_unique;

  glUseProgram(app->draw._texShader);
  glBindBufferBase(GL_UNIFORM_BUFFER, 0, app->draw._globalUB);
  glBindVertexArray(app->draw._quadVAO);

  _LocalUBData2D_t ubData = {
    .color = COLOR_RED,
    .model = GLM_MAT4_IDENTITY_INIT
  };

  vec2 resultingSize = {0};
  _Draw_calculateUiTextSize(app, ui, resultingSize);

  vec2 normalizedScale = {0};
  glm_vec2_div(ui->parent->size.dimentions, resultingSize, normalizedScale);
  if (resultingSize[1] < 0.001f)
    normalizedScale[1] = 1.f;

  vec2 lPos = {0};

  uint32_t peakYAdvance = 0;
  float peakYBearing = 0;

  for (UC_t *code_point = text->str.str; code_point < &text->str.str[text->str.count + 1]; code_point++) {
      _Glyph_t *glyph = _Draw_getGlyphOrLoad(app, *code_point);
      lPos[0] += glyph->advance[0] / 2.f * normalizedScale[0];

      if (glyph->isWhitespace || *code_point == '\n')
        goto skip_ui_glyph_rendering;

      mat4 modelMatrix = GLM_MAT4_IDENTITY_INIT;
      glm_translate(modelMatrix, (vec3) {
        lPos[0] + glyph->bearing[0] * normalizedScale[0] - 0.5,
        lPos[1] - (glyph->size[1] / 2.f - glyph->bearing[1]) * normalizedScale[0],
        0.f
      });

      glm_scale(modelMatrix, (vec3) {
        glyph->size[0]  * normalizedScale[0],
        glyph->size[1] * normalizedScale[1]
      });

      _LocalUBData2D_t ubData = {0};
      glm_mat4_mul(ui->_matrix, modelMatrix, ubData.model);
      glm_vec4_copy(ui->_color, ubData.color);

      glNamedBufferSubData(app->draw._localUB, 0,
        sizeof(_LocalUBData2D_t), &ubData
      );
      glBindBufferBase(GL_UNIFORM_BUFFER, 1, app->draw._localUB);
      glBindTextureUnit(0, glyph->glTextureHandle);
      glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
      
skip_ui_glyph_rendering:
      lPos[0] += (glyph->advance[0] / 2.f) * normalizedScale[0];
      
      peakYAdvance = glyph->advance[1] > peakYAdvance ?
        glyph->advance[1] : peakYAdvance;

      peakYBearing = glyph->bearing[1] > peakYBearing ?
        glyph->bearing[1] : peakYBearing;
      if (*code_point == '\n') {
        lPos[1] -= (peakYAdvance + peakYBearing) * normalizedScale[1];
        lPos[0] = 0;
    }
  }
}

void _Draw_uiContainer(App_t* app, UI_t *ui) {
  glm_mat4_copy(app->draw._projection, app->draw._globalUBData.projectionView);
  glNamedBufferSubData(app->draw._globalUB, 0,
    sizeof(_GlobalUBData_t), &app->draw._globalUBData
  );

  _LocalUBData2D_t ubData = {0};
  glm_mat4_copy(ui->_matrix, ubData.model);
  glm_vec4_copy(ui->_color, ubData.color);

  glNamedBufferSubData(app->draw._localUB, 0,
    sizeof(_LocalUBData2D_t), &ubData
  );

  glUseProgram(app->draw._flatShader);
  glBindBufferBase(GL_UNIFORM_BUFFER, 0, app->draw._globalUB); 
  glBindBufferBase(GL_UNIFORM_BUFFER, 1, app->draw._localUB);
  glBindVertexArray(app->draw._quadVAO);
  glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
}

void _Draw_uiWireframe(App_t* app, UI_t *ui) {
  glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
  _Draw_uiContainer(app, ui);
  glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
}

void _Draw_UI(App_t* app, UI_t *ui) {
  if (ui->flags & UI_FLAG_WIREFRAME)
    _Draw_uiWireframe(app, ui);
  // HIDE CHILDREN TOO
  if (ui->flags & UI_FLAG_HIDE)
    return;

  switch (ui->type) {
    case UI_EL_TYPE_INPUT:
    case UI_EL_TYPE_TEXT: {
      _Draw_uiText(app, ui);
      break;
    }
    default:
      _Draw_uiContainer(app, ui);
      break;
  }

  for (UI_t *child = ui->children;
    child < &ui->children[ui->childCount]; child++) {
    _Draw_UI(app, child);
  }
}

void _Draw_loadCamera(App_t *app, vec2 cameraPosition) {
  glm_mat4_identity(app->draw._view);
  glm_translate(
    app->draw._view,
    (vec3) { 
      cameraPosition[0], 
      cameraPosition[1], 
      0.0 
    }
  );

  glm_mat4_mul(
    app->draw._projection,
    app->draw._view,
    app->draw._globalUBData.projectionView
  );

  glNamedBufferSubData(app->draw._globalUB, 0, 
    sizeof(_GlobalUBData_t), &app->draw._globalUBData
  );
}

void _App_render(App_t *app) {
  glfwMakeContextCurrent(app->_wnd);
  
  _Draw_loadCamera(app, app->camera);
  glClear(GL_COLOR_BUFFER_BIT);

  _LocalUBData2D_t spriteUB = {
    .color = {0.75, 0.0, 0.5, 1.0},
    .model = GLM_MAT4_IDENTITY_INIT
  };
  glm_translate(spriteUB.model, 
    (vec3) { app->_spritePosition[0], app->_spritePosition[1], 0 }
  );
  glm_scale(spriteUB.model, 
    (vec3) { 0.1f, 0.1f, 0 }
  );
  glNamedBufferSubData(app->draw._localUB, 0, 
    sizeof(_LocalUBData2D_t), &spriteUB
  );

  glUseProgram(app->draw._flatShader);
  glBindBufferBase(GL_UNIFORM_BUFFER, 0, app->draw._globalUB); 
  glBindBufferBase(GL_UNIFORM_BUFFER, 1, app->draw._localUB);
  glBindVertexArray(app->draw._quadVAO);
  glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

  spriteUB = (_LocalUBData2D_t) {
    .color = {0.75, 0.0, 0.5, 1.0},
    .model = GLM_MAT4_IDENTITY_INIT
  };

  // Drawing cursor
  vec3 cPos = {0};
  _App_getMouseWorldPosition(app, cPos);

  // INVESTIGATE
  cPos[1] += 1;

  glm_translate(spriteUB.model, cPos);
  glm_scale(spriteUB.model, (vec3) {0.025, 0.025, 1.f});

  glNamedBufferSubData(app->draw._localUB, 0, 
    sizeof(_LocalUBData2D_t), &spriteUB
  );
  glBindBufferBase(GL_UNIFORM_BUFFER, 0, app->draw._globalUB); 
  glBindBufferBase(GL_UNIFORM_BUFFER, 1, app->draw._localUB);
  glBindVertexArray(app->draw._quadVAO);
  glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

  char cPosBuffer[64];
  sprintf_s(cPosBuffer, sizeof(cPosBuffer) / sizeof(char), 
    "Cursor Pos: (%.1f, %.1f)", cPos[0], cPos[1]);

  UStr_t str = {0};
  UStr_init(&str, cPosBuffer);
  
  _Draw_text(app, &str, (Transform_t) {
      .position = {-0.5, -0.5},
      .rotation = 0.f,
      .scale = { 100.f, 100.f }
    },
    (TextInfo_t) {
      .fontSize = 100,
      .horSpacing = 0.25f,
      .vertSpacing = 1.f
    }
  );
  // Drawing cursor

  UStr_reset(&str);
  UStr_appendLiteral(&str, "This shit is Epic\n we goon to femboys twin");
  _Draw_text(app, &str, 
    (Transform_t) {
      .position = {0.5, 0.5},
      .rotation = -15.f,
      .scale = { 100.f, 100.f }
    },
    TEXT_INFO_INIT
  );

  UStr_reset(&str);
  UStr_appendLiteral(&str, "Това е тест");
  _Draw_text(app, &str, 
    (Transform_t) {
      .position = {-0.5, -0.5},
      .rotation = 90.f,
      .scale = { 20.f, 20.f }
    },
    TEXT_INFO_INIT
  );

  _Draw_text(app, &app->input, 
    (Transform_t) {
      .position = {0.f, 0.f},
      .rotation = 0.f,
      .scale = { 100.f, 100.f }
    },
    TEXT_INFO_INIT
  );

  char fpsBuffer[36];
  sprintf_s(fpsBuffer, sizeof(fpsBuffer) / sizeof(char), 
    "FPS: %.2f", (1.0 / app->_deltaTime));
  
  UStr_reset(&str);
  UStr_appendLiteral(&str, fpsBuffer);
  _Draw_text(app, &str, (Transform_t) {
      .position = {0.5, 0.5},
      .rotation = 0.f,
      .scale = { 50.f, 50.f }
    },
    (TextInfo_t) {
      .fontSize = 100,
      .horSpacing = 0.1f,
      .vertSpacing = 1.f
    }
  );

  _Draw_UI(app, &app->_uiRoot);
  glfwSwapBuffers(app->_wnd);
  UStr_destroy(&str);

  glfwMakeContextCurrent(NULL);
}

void __testButtonCallback(App_t  *app, UI_t *self) {
  vec2 newPosition = {0};
  glm_vec2_copy(self->_pos, newPosition);
  newPosition[1] += 0.01;
  UI_setPosition(self, newPosition);
}

Result_t _App_initUI(App_t *app) {
  UiInfo_t info = {
    .color = {1.f, 0.f, 0.f, 0.5f},
    .flags = UI_FLAG_NONE,
    .parent = NULL,
    .size = (UiSize_t) {
      .width = 2.f,
      .height = 2.f
    },
    .position = {0.f, 0.f},
    .type = UI_EL_TYPE_CONTAINER,
    .id = ROOT_ID
  };

  UI_init(&app->_uiRoot, &info);

  UiContainerInfo_t containerInfo = {0}; // BULLSHIT FOR NOW
  __UI_initContainer(&app->_uiRoot, &containerInfo);

  info = (UiInfo_t) {
    .flags = UI_FLAG_ORDER_VERTICAL,
    .color = COLOR_BASE,
    .size = (UiSize_t) {
      .flag = UI_SIZE_FLAG_REAL,
      .width = 2.f,
      .height = 0.9f
    },
    .position = {0.f, -1.f},
    .id = 1,
    .parentId = 0
  };
  UI_addChildContainerById(&app->_uiRoot, &info, &containerInfo);

  UiInputInfo_t inputInfo = {
    .str = "Default Input"
  };

  info = (UiInfo_t) {
    .flags = UI_FLAG_WIREFRAME,
    .color = COLOR_SECONDARY,
    .size = (UiSize_t) {
      .flag = UI_SIZE_FLAG_REAL,
      .width = 0.1f,
      .height = 0.1f
    },
    .position = {0.5f, 0.5f},
    .id = 2,
    .parentId = 1
  };
  UI_addChildInputById(&app->_uiRoot, &info, &inputInfo);
//
//  UiButtonInfo_t buttonInfo = {
//    .onHoverColor = COLOR_SECONDARY,
//    .onClick = __testButtonCallback,
//  };
//
//  info = (UiInfo_t) {
//    .flags = UI_FLAG_NONE,
//    .color = COLOR_PRIMARY,
//    .size = (UiSize_t) {
//      .flag = UI_SIZE_FLAG_REAL,
//      .width = 0.01f,
//      .height = 0.01f
//    },
//    .position = {0.f, 0.5f},
//    .id = 3,
//    .parentId = 1
//  };
//  UI_addChildButtonById(&app->_uiRoot, &info, &buttonInfo);
//  
//  buttonInfo = (UiButtonInfo_t) {
//    .onHoverColor = COLOR_SECONDARY,
//    .onClick = __testButtonCallback,
//  };
//
//  info = (UiInfo_t) {
//    .flags = UI_FLAG_NONE,
//    .color = COLOR_PRIMARY,
//    .size = (UiSize_t) {
//      .flag = UI_SIZE_FLAG_REAL,
//      .width = 0.01f,
//      .height = 0.01f
//    },
//    .position = {0.f, 0.f},
//    .id = 4,
//    .parentId = 1
//  };
//  UI_addChildButtonById(&app->_uiRoot, &info, &buttonInfo);
//
//  UiTextInfo_t textInfo = {
//    .str = "Add Macro"
//  };
//
//  info = (UiInfo_t) {
//    .flags = UI_FLAG_WIREFRAME,
//    .color = COLOR_BLACK,
//    .size = (UiSize_t) {
//      .flag = UI_SIZE_FLAG_REAL | UI_SIZE_FLAG_FILL_HEIGHT | UI_SIZE_FLAG_FILL_WIDTH,
//      .width = 0.01f,
//      .height = 0.01f
//    },
//    .position = {0.f, 0.f},
//    .id = 5,
//    .parentId = 4
//  };
//  UI_addChildTextById(&app->_uiRoot, &info, &textInfo);
  return RESULT_SUCCESS;
}

bool _App_UIprocessEvent(App_t *app, UI_t *ui, Event_t *ev) {
  // REVERSE ORDER, BECAUSE LAST DRAWN IS AT THE END OF THE ARRAY
  // TODO: MAYBE AN ACTIVE STATE THAT GETS DISABLED IF ANOTHER ELEMENT ABSORBS THE EVENT
  for (UI_t *child = &ui->children[ui->childCount]; 
    child >= ui->children; child--) {
    if (_App_UIprocessEvent(app, child, ev)) {
      return true;
    }
  }

  switch (ui->type) {
    case UI_EL_TYPE_BUTTON:
      return UI_buttonProcessEvent(ui, app, ev);
    case UI_EL_TYPE_INPUT:
      return UI_inputProcessEvent(ui, ev);
    default:
      return false;
  }
}

#define MOVEMENT_CUTOFF 0.5f

void _App_update(App_t *app)
{
  _App_updateCamera(app);

  vec2 offset = {0, 0};

  if (glfwGetKey(app->_wnd, GLFW_KEY_UP) == GLFW_PRESS)
    offset[1] += 1;

  if (glfwGetKey(app->_wnd, GLFW_KEY_DOWN) == GLFW_PRESS)
    offset[1] -= 1;

  if (glfwGetKey(app->_wnd, GLFW_KEY_LEFT) == GLFW_PRESS)
    offset[0] -= 1;
  
  if (glfwGetKey(app->_wnd, GLFW_KEY_RIGHT) == GLFW_PRESS)
    offset[0] += 1;

  float offsetMag = sqrtf(offset[0] * offset[0] + offset[1] * offset[1]);
  if (offsetMag > MOVEMENT_CUTOFF) {
    float offsetMagNorm = 1.f / offsetMag;
    float transformMultiplier = 
      (float)app->_deltaTime * offsetMagNorm;

    glm_vec2_scale(offset, transformMultiplier * BASE_SPEED, offset);
    glm_vec2_add(app->_spritePosition, offset, app->_spritePosition);
  }

  // vec2 newUiPos = {0, 0.1 * app->_deltaTime};
  // glm_vec2_add(app->_uiRoot.children[1]._pos, newUiPos, newUiPos);
  // UI_setPosition(
  //   &app->_uiRoot.children[1],
  //   newUiPos
  // );

  // TEMP//REMOVE
  vec2 cPos = {0};
  _App_getMouseScreenNormalizedCentered(app, cPos);
  
  Event_t ev = {0};
  while (EventQueue_pop(&app->_evQueue, &ev)) {
    _App_UIprocessEvent(app, &app->_uiRoot, &ev);
  }
}

void App_destroy(App_t *app) {
  DEBUG_ASSERT(app != NULL, "App is set to null on App_destroy");

  CloseHandle(app->_renderThread);

  UI_destroy(&app->_uiRoot);
  EventQueue_cleanup(&app->_evQueue);
  _App_cleanupTextRenderer(app);
  _App_OpenGlCleanup(app);
  
  DEBUG_ASSERT(app->_wnd != NULL, "GLFWwindow app->_wnd is set to NULL, not initialized");
  glfwDestroyWindow(app->_wnd);
  
  free(app);
}

inline void __App_calculateProjection(App_t *app) {
  int fbfW = 0, fbfH = 0;
  glfwGetFramebufferSize(app->_wnd, &fbfW, &fbfH);

  float aspect =  fbfW / (float)fbfH;
  glm_ortho(
    -aspect, aspect,
    -1.f, 1.f,
    -1.f, 1.f,
    app->draw._projection
  );
}

void _App_wndFbResizeCBCK(GLFWwindow *window, int width, int height) {
  App_t *app = glfwGetWindowUserPointer(window);

  glfwMakeContextCurrent(app->_wnd);
  glViewport(0, 0, width, height);
  
  __App_calculateProjection(app);

  int fbfW = 0, fbfH = 0;
  glfwGetFramebufferSize(app->_wnd, &fbfW, &fbfH);

  float aspect =  fbfW / (float)fbfH;
  app->_uiRoot.size.width = 2.f * aspect;
  __UI_updateMatrix(&app->_uiRoot);

  if (!__App_frameTimeElapsed(app))
    return;

  _App_update(app);
  _App_render(app);
}


Result_t App_run(App_t *app) {
  int winWidth, winHeight;
  glfwGetFramebufferSize(app->_wnd, &winWidth, &winHeight);
  _App_wndFbResizeCBCK(app->_wnd, winWidth, winHeight);

  _App_getMouseWorldPosition(app, app->_mouseStart);

  if (_App_initDrawThread(app) != RESULT_SUCCESS) {
    log_error("Failed to initialize theads");
    return RESULT_FAIL;
  }

  while (app->_running) {
    if (!__App_frameTimeElapsed(app)) {
      continue;
    }

    glfwPollEvents();
    _App_render(app);
    _App_update(app);
  }

  WaitForSingleObject(app->_renderThread, INFINITE);
  return RESULT_SUCCESS;
}

void _App_wndCursorPosCBCK(GLFWwindow* window, double xpos, double ypos) {
  App_t *app = glfwGetWindowUserPointer(window);
  Event_t payload = {
    .category = EVENT_CAT_INPUT,
    .type = EVENT_TYPE_MOUSE_MOVE,
    .position = {0}
  };

  _App_getMouseScreenNormalizedCentered(app, payload.position);
  EventQueue_push(&app->_evQueue, &payload);
}

DWORD WINAPI Draw_runWIN32(App_t *app) {
  while (app->_running) {
    if (!Draw_frameTimeElapsed(&app->draw)) {
      continue;
    }

  }

  return 0;
}

// Windows only for now
Result_t _App_initDrawThread(App_t *app) {
  app->_renderThread = CreateThread( 
    NULL,
    0,  
    Draw_runWIN32,
    app,
    0,
    app->_renderThread
  ); 

  if (app->_renderThread == NULL) {
    log_error("Failed to create render thread.");
    return RESULT_FAIL;
  }

  return RESULT_SUCCESS;
}

Result_t App_create(App_t** p_app, AppInfo_t info) {
  *p_app = malloc(sizeof(App_t));

  glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
  // glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);

#ifdef APP_DEBUG
  glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GL_TRUE);
#endif

  GLFWwindow *window;
  if (!(window = glfwCreateWindow(info.size.width, info.size.height, info.name, NULL, NULL))) {
    return RESULT_FAIL;
  }

  **p_app = (App_t) {
    ._wnd = window,
    ._running = true,

    .camera = {0, 0},
    ._camNew = {0, 0},
    ._camMoving = false,

    .draw = (Draw_t) {
      ._quadEBO = 0,
      ._quadVAO = 0,
      ._quadVBO = 0,

      ._texShader = 0,
      ._flatShader = 0,

      ._view = GLM_MAT4_IDENTITY_INIT,
      ._projection = GLM_MAT4_IDENTITY_INIT,

      ._globalUB = 0, ._localUB = 0,
      ._globalUBData = (_GlobalUBData_t) {
        .projectionView = GLM_MAT4_IDENTITY_INIT 
      },

      ._lastTime = 0.0,
      ._deltaTime = 0.0,
    },

    ._spritePosition = {0, 0},
    ._mouseStart = {0, 0},

    .info = info,
    ._inCap = DEFAULT_BUF_CAP,
    ._inCount = 0,

    ._lastTime = 0.0,
    ._deltaTime = 0.0,
  };

  UStr_init(&(*p_app)->input, "");

  __App_calculateProjection(*p_app);

  glfwSetWindowUserPointer(window, *p_app);
  glfwSetWindowCloseCallback(window, _App_wndCloseCBCK);
  glfwSetCursorPosCallback(window, _App_wndCursorPosCBCK);
  glfwSetKeyCallback(window, _App_wndInputCBCK);
  glfwSetMouseButtonCallback(window, _App_wndMouseBtnCBCK);
  glfwSetFramebufferSizeCallback(window, _App_wndFbResizeCBCK);
  glfwSetCharCallback(window, _App_wndCharCBCK);

  glfwMakeContextCurrent(window);
  int glVersion = 0;
  if (glVersion = gladLoadGL(glfwGetProcAddress)) {
    log_info("Loaded OpenGL %d.%d" ENDL, GLAD_VERSION_MAJOR(glVersion),
      GLAD_VERSION_MINOR(glVersion)
    );
  } else {
    log_error("Failed to initialize OpenGL context" ENDL);
    return RESULT_FAIL;
  }

  EventQueue_init(&(*p_app)->_evQueue);
  if (_App_initUI(*p_app) != RESULT_SUCCESS) {
    log_error("Failed to initialize app ui" ENDL);
    return RESULT_FAIL;
  }

  if (_App_initOpenGL(*p_app) != RESULT_SUCCESS) {
    log_error("Failed to setup OpenGL resources" ENDL);
    return RESULT_FAIL;
  }

  if (_Draw_loadAsciiGlyphs(*p_app) != RESULT_SUCCESS) {
    log_error("Failed to load glyphs" ENDL);
    return RESULT_FAIL;
  }

  Draw_timeInit(&(*p_app)->draw);

  glfwMakeContextCurrent(NULL);

  return RESULT_SUCCESS;
}

#define LOG_FILE_NAME "macro_runtime_log.txt"

int main(void) {
  FILE *logFile = NULL;

  errno_t err = 0;
  if ((err = fopen_s(&logFile, LOG_FILE_NAME, "w")) != 0 || logFile == NULL) {
    log_error(
      "Couldn't create/open the log file %s, error code: %i" ENDL,
      LOG_FILE_NAME,
      err
    );
  } else {
    log_add_fp(logFile, 0);
    log_info("Opened log file: %s" ENDL, LOG_FILE_NAME);
  }

  if (glfwInit() != GLFW_TRUE) {
    log_error("Failed to init GLFW");

    glfwTerminate();
    fclose(logFile);
    return -1;
  }

  App_t *app = NULL;

  Result_t initResult = App_create(&app, APP_INFO_INIT);
  if (initResult != RESULT_SUCCESS) {
    log_info("Macro failed at app creation" ENDL);
    goto macro_termination;
  }

  Result_t runtimeResult = App_run(app);
  if (runtimeResult == RESULT_FAIL) {
    log_error("Macro failed during the runtime");
    goto macro_termination;
  }

  log_info("Macro ended successfully" ENDL);

macro_termination:
  App_destroy(app);
  glfwTerminate();
  fclose(logFile);

  return (initResult == RESULT_SUCCESS && runtimeResult == RESULT_SUCCESS) ? 0 : -1;
}