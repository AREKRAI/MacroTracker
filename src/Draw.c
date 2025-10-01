#include "Draw.h"

void Transform_toMat4(Transform_t *self, mat4 matrix) {
  glm_mat4_identity(matrix);

  glm_translate(matrix, self->position);

  glm_rotate(
    matrix, 
    (self->rotation / 180.f)* GLM_PI, 
    (vec3){0, 0, 1}
  );

  glm_scale(matrix, self->scale);

}

void Transform_copy(Transform_t *self, Transform_t other) {
  memcpy(self->position, other.position, sizeof(vec2));
  memcpy(self->scale, other.scale, sizeof(vec2));
  memcpy(&self->rotation, &other.rotation, sizeof(float));
}

Result_t Draw_init(Draw_t *self, GLFWwindow *wndHandle) {
  return RESULT_SUCCESS;
}