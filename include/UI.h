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

typedef enum __UiFlag_t {
  UI_FLAG_NONE = 0,
  UI_FLAG_HIDE = 1,
  UI_FLAG_WIREFRAME = 2,
  UI_FLAG_FOCUS = 4,
  UI_FLAG_ORDER_VERTICAL = 8,
  UI_FLAG_ORDER_HORIZONTAL = 16
} UiFlag_t;

// TODO: GET BACK TO IDS
typedef uint32_t UiId_t;
#define NO_ID (-1)
#define ROOT_ID 0

#define DEFAULT_CHILD_CAP (1 << 8)

// TODO: implement atlas
// TODO: MAKE IT SAFE TO ADD UI IN ANY ORDER
// -> FIX THE ISSUE WHERE PARENT MEMORY GETS REALLOCED LOSING REFERENCE WITH CHILD POINTERS
// POSSIBLE FIXES -> ID SYSTEM, BUFFERING MEMORY REALLOCATION (HAVING TWO BUFFERS, ONE YOU SUBMIT CHANGES TO AND A FINAL ONE WHICH INTEGRATES THEM,
// KEEPING BUFFERS YOU HAVE A POINTER TO - NO WAY OF KNOWING IF THE POINTER'S BEEN DROPPED OUT OF SCOPE)

typedef enum __UI_SIZE_FLAG_T {
  UI_SIZE_FLAG_REAL = 0,
  UI_SIZE_FLAG_FILL_WIDTH = 1, // NOT SUPPORTED
  UI_SIZE_FLAG_FILL_HEIGHT = 2, // NOT SUPPORTED
  UI_SIZE_FLAG_FLEX_WIDTH = 4,
  UI_SIZE_FLAG_FLEX_HEIGHT = 8
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

  UiElType_t type;
  UiFlag_t flags;
  void *_unique;

  UiSize_t _size;
  vec2 _pos, _globalPos;

  vec4 color;
  vec4 _color;
  mat4 _matrix;

  UiId_t id;
} UI_t;

typedef struct __UiInfo_t {
  UiElType_t type;
  UiFlag_t flags;

  UiSize_t size;
  vec2 position;
  vec4 color;

  UI_t *parent;

  UiId_t parentId;
  UiId_t id;
} UiInfo_t;

void __UI_calculateMatrix(UI_t *self);
void __UI_updateMatrix(UI_t *self);
void UI_setPosition(UI_t *self, vec2 newPosition);
void UI_setSize(UI_t *self, UiSize_t newSize);
Result_t UI_init(UI_t *self, UiInfo_t *info);
UI_t *UI_addChild(UI_t *self, UiInfo_t *info);
void UI_destroy(UI_t *self);

// RETURNS: PARENT ID (0 = ROOT if parentId is not found in the tree or if )
UiId_t UI_addChildById(UI_t *root, UiInfo_t *info);
UI_t *UI_findById(UI_t *root, UiId_t id);

typedef struct __UiContainer_t {
  float _offsetAccumulation; // NOTE: UNUSED, 
} UiContainer_t;

typedef struct __UiContainerInfo_t {
  uint32_t NO_PARAMETER_SET_THIS_IS_A_PLACEHOLDER;
} UiContainerInfo_t;

void __UI_initContainer(UI_t *self, UiContainerInfo_t *specInfo);
UI_t *UI_addChildContainer(UI_t *self, UiInfo_t *info, UiContainerInfo_t *specInfo);
UiId_t UI_addChildContainerById(UI_t *self, UiInfo_t *info, UiContainerInfo_t *specInfo);

typedef void(*UiCBCK_t)(void *, UI_t *);

typedef struct __UiButton_t {
  vec4 onHoverColor;
  UiCBCK_t onClick;
} UiButton_t;

typedef struct __UiButtonInfo_t {
  vec4 onHoverColor;
  UiCBCK_t onClick;
} UiButtonInfo_t;

void __UI_initButton(UI_t *self, UiButtonInfo_t *specInfo);
UI_t *UI_addChildButton(UI_t *self, UiInfo_t *info, UiButtonInfo_t *specInfo);
UiId_t UI_addChildButtonById(UI_t *root, UiInfo_t *info, UiButtonInfo_t *specInfo);
bool UI_isHovered(UI_t* self, vec2 mouseWorldPos);

// TODO: get text and input to have an actual size in the rendering front end

typedef struct __UiText_t {
  UStr_t str;
} UiText_t;

typedef struct __UiTextInfo_t {
  const char *str;
} UiTextInfo_t;

void __UI_initText(UI_t *self, UiTextInfo_t *specInfo);
UI_t *UI_addChildText(UI_t *self, UiInfo_t *info, UiTextInfo_t *specInfo);
UiId_t UI_addChildTextById(UI_t *root, UiInfo_t *info, UiTextInfo_t *specInfo);

// REMOVE THIS IN THE FUTURE
// BY COPYING THE GLFW KEY MAPPING AND RENAMING IT
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

// TODO: EVENT SYSTEM
typedef enum __EVENT_TYPE_t {
  EVENT_TYPE_NONE = 0, // EVIL
  EVENT_TYPE_CLICK = 1,
  EVENT_TYPE_CHAR_INPUT = 2,
  EVENT_TYPE_MOUSE_MOVE = 3,
  EVENT_TYPE_FOCUS_SET = 4,
  EVENT_TYPE_KEY
} EVENT_TYPE_t;

typedef struct __Event_t {
  EVENT_TYPE_t type;
  union {
    vec2 position;
    UC_t character;
    struct {
      int glfwKey;
      int glfwAction;
    };
  };
} Event_t;

typedef struct __EventQueue_t {
  size_t count, cap;
  Event_t *events;
} EventQueue_t;

void EventQueue_init(EventQueue_t *self);
void EventQueue_cleanup(EventQueue_t *self);

void EventQueue_push(EventQueue_t *self, Event_t *ev);
bool EventQueue_pop(EventQueue_t *self, Event_t *ev);

// RETURNS: EVENT ABSORBED
bool UI_buttonProcessEvent(UI_t *self, void *ctx, Event_t *ev);

typedef struct __UiInput_t {
  UStr_t str;
} UiInput_t;

typedef struct __UiInputInfo_t {
  const char *str;
} UiInputInfo_t;

void __UI_initInput(UI_t *self, UiInputInfo_t *specInfo);
UI_t *UI_addChildInput(UI_t *self, UiInfo_t *info, UiInputInfo_t *specInfo);
UiId_t UI_addChildInputById(UI_t *root, UiInfo_t *info, UiInputInfo_t *specInfo);
bool UI_inputProcessEvent(UI_t *self, Event_t *ev);

#endif