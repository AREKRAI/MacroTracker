#include "UI.h"

void UiSize_copy(UiSize_t *dst, UiSize_t *src) {
  memcpy(dst->dimentions, src->dimentions, sizeof(dst->dimentions));
  dst->flag = src->flag;
}

void __UI_calculateMatrix(UI_t *self) {
  glm_mat4_identity(self->_matrix);
  
  glm_translate(self->_matrix, (vec3){
      self->_globalPos[0],
      self->_globalPos[1],
      0.f
    }
  );

  float width = self->_size.width;
  float height = self->_size.height;

  if (self->parent != NULL &&
      self->_size.flag & UI_SIZE_FLAG_FILL_HEIGHT) {
    height = self->parent->_size.height;
  }
  if (self->parent != NULL &&
      self->_size.flag & UI_SIZE_FLAG_FILL_WIDTH) {
    width = self->parent->_size.width;
  }

  glm_scale(self->_matrix,
    (vec3) { width, height, 1.0f }
  );
}

void __UI_updateMatrix(UI_t *self) {
  // Update globals to locals
  memcpy(self->_globalPos, self->_pos, sizeof(self->_globalPos));

  if (self->parent != NULL) {
    glm_vec2_add(self->parent->_globalPos, self->_globalPos, self->_globalPos);
  }

  __UI_calculateMatrix(self);
  for (UI_t *child = self->children;
    child < &self->children[self->childCount]; child++) {
      __UI_updateMatrix(child);
  }
}

void UI_setPosition(UI_t *self, vec2 newPosition) {
  memcpy(self->_pos, newPosition, sizeof(vec2));
  __UI_updateMatrix(self);
}

void UI_setSize(UI_t *self, UiSize_t newSize) {
  DEBUG_ASSERT(
    newSize.width < 1.01f && newSize.height < 1.01f &&
    newSize.width > 0.f && newSize.height > 0.f,
    "Size cannot exceed 1.f or be less than 0.f"
  );

  UiSize_copy(&self->_size, &newSize);
  __UI_updateMatrix(self);
}

Result_t UI_init(UI_t *self, UiInfo_t *info) {
  self->childCap = DEFAULT_CHILD_CAP;
  self->children = malloc(self->childCap);
  self->childCount = 0;

  self->_type = info->type;
  self->parent = info->parent;
  self->hide = info->hide;

  memcpy(self->_pos, info->position, sizeof(vec2));
  UiSize_copy(&self->_size, &info->size);
  memcpy(self->color, info->color, sizeof(vec4));
  memcpy(self->_color, info->color, sizeof(vec4));

  __UI_updateMatrix(self);
  return EXIT_SUCCESS;
}

size_t UI_addChild(UI_t *self, UiInfo_t *info) {
  self->childCount++;

  while (self->childCount * sizeof(UI_t) > self->childCap) {
    self->childCap <<= 1;
  }
  self->children = realloc(self->children, self->childCap);

  UI_t *child = &self->children[self->childCount - 1];
  info->parent = self;
  UI_init(child, info);

  return self->childCount - 1;
}

void UI_destroy(UI_t *self) {
  for (UI_t *child = self->children;
    child < &self->children[self->childCount]; child++) {
      UI_destroy(child);
  }

  switch (self->_type) {
    case UI_EL_TYPE_TEXT:
      UStr_destroy(&((UiText_t *)self->_unique)->str);
    case UI_EL_TYPE_BUTTON:
    case UI_EL_TYPE_CONTAINER:
      free(self->_unique);
      self->_unique = NULL;
      break;
    default:
      break;
  }

  free(self->children);
}

void __UI_initContainer(UI_t *self, UiContainerInfo_t *specInfo) {
  UiContainer_t *unique = (self->_unique = malloc(sizeof(UiContainer_t)));
  *unique = (UiContainer_t) {
    .flags = specInfo->flags
  };
}

size_t UI_addChildContainer(UI_t *self, UiContainerInfo_t *specInfo) {
  UiInfo_t genInfo = {
    .type = UI_EL_TYPE_CONTAINER,
    .parent = self,
  };

  DEBUG_ASSERT(
    sizeof(genInfo.color) == sizeof(specInfo->color), 
    "genInfo.color and specInfo->color must be of same type"
  );
  memcpy(genInfo.color, specInfo->color, sizeof(genInfo.color));

  DEBUG_ASSERT(
    sizeof(genInfo.position) == sizeof(specInfo->position), 
    "genInfo.position and specInfo->position must be of same type"
  );
  memcpy(genInfo.position, specInfo->position, sizeof(genInfo.position));

  DEBUG_ASSERT(
    sizeof(genInfo.size) == sizeof(specInfo->size), 
    "genInfo.size and specInfo->size must be of same type"
  );
  UiSize_copy(&genInfo.size, &specInfo->size);

  size_t child = UI_addChild(self, &genInfo);
  __UI_initContainer(&self->children[child], specInfo);

  return child;
}

void __UI_initButton(UI_t *self, UiButtonInfo_t *specInfo) {
  UiButton_t *unique = (self->_unique = malloc(sizeof(UiButton_t)));
  *unique = (UiButton_t) {
    .onClick = specInfo->onClick,
  };

  DEBUG_ASSERT(
    sizeof(unique->onHoverColor) == sizeof(specInfo->onHoverColor),
    "UiButton_t.onHoverColor must be of same type as UiButtonInfo_t.onHoverColor - vec4(cglm)"
  );
  memcpy(unique->onHoverColor, specInfo->onHoverColor, sizeof(unique->onHoverColor));
}

size_t UI_addChildButton(UI_t *self, UiButtonInfo_t *specInfo) {
  UiInfo_t genInfo = {
    .type = UI_EL_TYPE_BUTTON,
    .hide = false,
    .parent = self,
  };

  DEBUG_ASSERT(
    sizeof(genInfo.color) == sizeof(specInfo->color), 
    "genInfo.color and specInfo->color must be of same type"
  );
  memcpy(genInfo.color, specInfo->color, sizeof(genInfo.color));

  DEBUG_ASSERT(
    sizeof(genInfo.position) == sizeof(specInfo->position), 
    "genInfo.position and specInfo->position must be of same type"
  );
  memcpy(genInfo.position, specInfo->position, sizeof(genInfo.position));

  DEBUG_ASSERT(
    sizeof(genInfo.size) == sizeof(specInfo->size), 
    "genInfo.size and specInfo->size must be of same type"
  );
  UiSize_copy(&genInfo.size, &specInfo->size);

  size_t child = UI_addChild(self, &genInfo);
  __UI_initButton(&self->children[child], specInfo);

  return child;
}

bool UI_isHovered(UI_t* self, vec2 mouseWorldPos) {
  vec2 bottomLeft = {0};
  vec2 topRight = {0};
  glm_vec2_copy(self->_globalPos, bottomLeft);
  glm_vec2_copy(self->_globalPos, topRight);

  float halfWidth = self->_size.width / 2.f;
  float halfHeight = self->_size.height / 2.f;

  topRight[0] += halfWidth;
  bottomLeft[0] -= halfWidth;

  bottomLeft[1] -= halfHeight;
  topRight[1] += halfHeight;

  bool hovered = mouseWorldPos[0] < topRight[0] &&
    mouseWorldPos[0] > bottomLeft[0];
  hovered = hovered && 
    (mouseWorldPos[1] < topRight[1] && mouseWorldPos[1] > bottomLeft[1]);

  return hovered;
}

void UI_processMouseInput(UI_t* self, vec2 mouseWorldPos) {
  for (UI_t *child = self->children;
    child < &self->children[self->childCount]; child++) {
      UI_processMouseInput(child, mouseWorldPos);
  }

  bool hovered = UI_isHovered(self, mouseWorldPos);

  switch (self->_type) {
    case UI_EL_TYPE_BUTTON: {
      UiButton_t *unique = self->_unique;

      if (hovered) {
        memcpy(self->_color, unique->onHoverColor, sizeof(self->_color));
      } else {
        memcpy(self->_color, self->color, sizeof(self->_color));
      }
      
      break;
    }
    default:
      break;
  }
}

void __UI_initText(UI_t *self, UiTextInfo_t *specInfo) {
  UiText_t *unique = (self->_unique = malloc(sizeof(UiText_t)));
  UStr_init(&unique->str, specInfo->str);
}

size_t UI_addChildText(UI_t *self, UiTextInfo_t *specInfo) {
  UiInfo_t genInfo = {
    .type = UI_EL_TYPE_TEXT,
    .hide = false,
    .parent = self,
  };

  DEBUG_ASSERT(
    sizeof(genInfo.color) == sizeof(specInfo->color), 
    "genInfo.color and specInfo->color must be of same type"
  );
  memcpy(genInfo.color, specInfo->color, sizeof(genInfo.color));

  DEBUG_ASSERT(
    sizeof(genInfo.position) == sizeof(specInfo->position), 
    "genInfo.position and specInfo->position must be of same type"
  );
  memcpy(genInfo.position, specInfo->position, sizeof(genInfo.position));

  DEBUG_ASSERT(
    sizeof(genInfo.size) == sizeof(specInfo->size), 
    "genInfo.size and specInfo->size must be of same type"
  );
  UiSize_copy(&genInfo.size, &specInfo->size);

  size_t child = UI_addChild(self, &genInfo);
  __UI_initText(&self->children[child], specInfo);

  return child;
}