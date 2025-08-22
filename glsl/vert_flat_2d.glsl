#version 420
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec2 aUV;

layout (std140, binding = 0) uniform GlobalUB
{
  mat4 projectionView;
};

layout (std140, binding = 1) uniform LocalUB
{
  mat4 model;
  vec4 color;
};

out vec2 iUV;
out vec4 iColor;

void main()
{
  gl_Position = projectionView * model * vec4(aPos, 0.0, 1.0);
  iUV = aUV;
  iColor = color;
}