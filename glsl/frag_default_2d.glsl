#version 420

layout(binding = 0) uniform sampler2D atlas;

in vec2 iUV;
in vec4 iColor;
out vec4 FragColor;

void main()
{
  vec4 sampled = vec4(1.0, 1.0, 1.0, texture(atlas, iUV).r);
  FragColor = sampled * iColor;
}