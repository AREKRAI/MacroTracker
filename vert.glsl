#version 420
layout (location = 0) in vec3 aPos;

layout (std140, binding = 0) uniform GlobalUB
{
  mat4 projectionView;
};

void main()
{
   gl_Position = projectionView * vec4(aPos, 1.0);
}