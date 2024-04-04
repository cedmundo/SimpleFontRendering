#version 330 core
out vec4 FragColor;

in vec3 vCol;
in vec2 vTex;

void main() {
  FragColor = vec4(vCol, 1.0); // * texture(tex0, vTex);
}
