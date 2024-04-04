#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aCol;
layout (location = 2) in vec2 aTex;

out vec3 vCol;
out vec2 vTex;

uniform mat4 model;
uniform mat4 proj;

void main() {
  gl_Position = model * proj * vec4(aPos, 1.0);
  vCol = aCol;
  vTex = aTex;
}
