
#ifndef _H_UI_
#define _H_UI_

#include <cglm/cglm.h>
#include <stdbool.h>

#include "Common.h"
#include "UStr.h"

typedef enum __UiElType_t {
  UI_EL_TYPE_CONTAINER = 0,
  UI_EL_TYPE_TEXT = 1,
  UI_EL_TYPE_BUTTON = 2,
  UI_EL_TYPE_INPUT = 3
} UiElType_t;

#define DEFAULT_CHILD_CAP ((size_t)1 << 7)

// TODO: implement atlas

typedef enum __UI_SIZE_FLAG_T {
  UI_SIZE_FLAG_REAL = 0,
  UI_SIZE_FLAG_FILL_WIDTH = 1, // NOT SUPPORTED
  UI_SIZE_FLAG_FILL_HEIGHT = 2 // NOT SUPPORTED
} UI_SIZE_FLAG_T;

typedef struct __UiSize_t {
  UI_SIZE_FLAG_T flag;

  union {
    vec2 dimentions;
    struct {
      float width, height;
    };
  };
} UiSize_t;

void UiSize_copy(UiSize_t *dst, UiSize_t *src);

typedef struct __UI_t {
  struct __UI_t *children, *parent;
  size_t childCount, childCap;

  UiElType_t _type;
  void *_unique;

  UiSize_t _size;
  vec2 _pos, _globalPos;

  vec4 color;
  vec4 _color;
  mat4 _matrix;
} UI_t;

typedef struct __UiInfo_t {
  UiElType_t type;

  UiSize_t size;
  vec2 position;
  vec4 color;

  UI_t *parent;
} UiInfo_t;

void __UI_calculateMatrix(UI_t *self);
void __UI_updateMatrix(UI_t *self);
void UI_setPosition(UI_t *self, vec2 newPosition);
void UI_setSize(UI_t *self, UiSize_t newSize);
Result_t UI_init(UI_t *self, UiInfo_t *info);
UI_t *UI_addChild(UI_t *self, UiInfo_t *info);
void UI_destroy(UI_t *self);

typedef enum __ContainerFlag_t {
  CONTAINER_FLAG_NONE = 0,
  CONTAINER_FLAG_ORDER_VERTICAL = 1,
  CONTAINER_FLAG_ORDER_HORIZONTAL = 2
} ContainerFlag_t;

typedef struct __UiContainer_t {
  ContainerFlag_t flags;
  // TODO: float _offsetAccumulation;
} UiContainer_t;

typedef struct __UiContainerInfo_t {
  ContainerFlag_t flags;
  vec4 color;

  UiSize_t size;
  vec2 position;
} UiContainerInfo_t;

void __UI_initContainer(UI_t *self, UiContainerInfo_t *specInfo);
UI_t *UI_addChildContainer(UI_t *self, UiContainerInfo_t *specInfo);

typedef void(*UiCBCK_t)(void *, UI_t *);

typedef struct __UiButton_t {
  vec4 onHoverColor;
  UiCBCK_t onClick;
} UiButton_t;

typedef struct __UiButtonInfo_t {
  vec4 color;
  vec4 onHoverColor;
  UiCBCK_t onClick;

  UiSize_t size;
  vec2 position;
} UiButtonInfo_t;

void __UI_initButton(UI_t *self, UiButtonInfo_t *specInfo);
UI_t *UI_addChildButton(UI_t *self, UiButtonInfo_t *specInfo);
bool UI_isHovered(UI_t* self, vec2 mouseWorldPos);

void UI_processMouseInput(UI_t* self, vec2 mouseWorldPos);

#endif