#version 420

in vec2 iUV;
in vec4 iColor;
out vec4 FragColor;

void main()
{
  FragColor = iColor;
}