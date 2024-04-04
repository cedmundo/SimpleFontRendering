#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// clang-format off
#include <glad/glad.h>
#include <GLFW/glfw3.h>
// clang-format on

#include <png.h>

#define WINDOW_TITLE "SimpleFontRendering"
#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 800

#define ASSETS_FONT_IMG "assets/cooper-hewitt-heavy.png"
#define ASSETS_FONT_TXT "assets/cooper-hewitt-heavy.txt"
#define ASSETS_GLYPH_VS "assets/glyph.vs.glsl"
#define ASSETS_GLYPH_FS "assets/glyph.fs.glsl"

typedef enum {
  LOG_INFO,
  LOG_WARN,
  LOG_ERROR,
} LogLevel;

typedef enum {
  SUCCESS,
  ERROR_CANNOT_LOAD_DESC_FILE,
  ERROR_CANNOT_LOAD_ATLAS_FILE,
  ERROR_CANNOT_LOAD_GLYPH_SHADER,
  ERROR_INVALID_DESCRIPTION,
} StatusCode;

typedef struct {
  float width;
  float height;
  float aspect;
} AppState;

typedef struct {
  unsigned vao;
  unsigned vbo;
  unsigned ebo;
} Mesh;

typedef struct {
  StatusCode status;
  float fontSize;
  float xos[256];
  float yos[256];
  float xas[256];
  Mesh quads[256];
  unsigned shaderProgramId;
  unsigned textureId;
} BitmapFont;

void Log(LogLevel level, const char *fmt, ...);
unsigned LoadShader(const char *vsFilename, const char *fsFilename);
unsigned LoadTexture(const char *filename);
BitmapFont LoadBitmapFont(const char *atlasFilename, const char *descFilename);
void UnloadBitmapFont(BitmapFont font);
char *ReadTextFile(const char *filename);
unsigned char *ReadPNGFile(const char *filename, unsigned *width,
                           unsigned *height);
bool HasPrefix(const char *line, const char *prefix);
int GetLineAttrInt(const char *line, unsigned attrId);
char *GetNextLine(char *line);
Mesh MakeGlyphQuad(float imgW, float imgH, float x, float y, float w, float h);
void RenderText(BitmapFont font, float xPos, float yPos, const char *text);
void WorldToViewport(float *vx, float *vy, float x, float y);
void MakeOrthoProj(float m[16], float l, float r, float t, float b, float n,
                   float f);
void MakeGlyphTransform(float m[16], float x, float y);

// warn: entire app state
static AppState globalState = {0};

int main() {
  GLFWwindow *window = NULL;
  int status = EXIT_SUCCESS;
  BitmapFont font = {0};

  if (!glfwInit()) {
    const char *msg = NULL;
    glfwGetError(&msg);
    Log(LOG_ERROR, "could not initialize GLFW: %s", msg);
    status = EXIT_FAILURE;
    goto terminate;
  }

  glfwInitHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
  glfwInitHint(GLFW_CONTEXT_VERSION_MINOR, 1);
  glfwInitHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

  window =
      glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, WINDOW_TITLE, NULL, NULL);
  if (window == NULL) {
    const char *msg = NULL;
    glfwGetError(&msg);
    Log(LOG_ERROR, "could not create window: %s", msg);
    status = EXIT_FAILURE;
    goto terminate;
  }

  glfwMakeContextCurrent(window);
  if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
    Log(LOG_ERROR, "could not initialize GL");
    status = EXIT_FAILURE;
    goto terminate;
  }

  // Load font and other resources
  {
    font = LoadBitmapFont(ASSETS_FONT_IMG, ASSETS_FONT_TXT);
    if (font.status != SUCCESS) {
      Log(LOG_ERROR, "could not load font");
      status = EXIT_FAILURE;
      goto terminate;
    }
  }

  // Color and depth setup
  glClearColor(0.7f, 0.7f, 0.7f, 1.0f);
  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_LESS);

  // Enable transparency
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  while (!glfwWindowShouldClose(window)) {
    // Prepare render
    int width = 0;
    int height = 0;
    glfwGetFramebufferSize(window, &width, &height);
    globalState.width = (float)width;
    globalState.height = (float)height;
    globalState.aspect = globalState.width / globalState.height;

    glViewport(0, 0, width, height);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    {
      // Render
      RenderText(font, 10.0f, 100.0f, "Medea china inutil");
    }
    // Finish render
    glfwPollEvents();
    glfwSwapBuffers(window);
  }

terminate:
  UnloadBitmapFont(font);

  if (window != NULL) {
    glfwDestroyWindow(window);
    glfwTerminate();
  }

  return status;
}

void Log(LogLevel level, const char *fmt, ...) {
  static const char *levels[] = {
      [LOG_INFO] = "INFO",
      [LOG_WARN] = "WARN",
      [LOG_ERROR] = "ERROR",
  };

  printf("%s  | ", levels[level]);
  va_list vaList;
  va_start(vaList, fmt);
  vprintf(fmt, vaList);
  va_end(vaList);
  printf("\n");
}

unsigned LoadShader(const char *vsFilename, const char *fsFilename) {
  unsigned shaderProgramId = 0;
  int shaderStatus = 0;
  char shaderLog[512] = {0};
  char *vsCode = NULL;
  char *fsCode = NULL;
  unsigned vShaderId = 0;
  unsigned fShaderId = 0;

  vsCode = ReadTextFile(vsFilename);
  if (vsCode == NULL) {
    Log(LOG_ERROR, "SHADER: could not load vertex shader: %s", vsFilename);
    goto terminate;
  }

  vShaderId = glCreateShader(GL_VERTEX_SHADER);
  glShaderSource(vShaderId, 1, (const char **)&vsCode, NULL);
  glCompileShader(vShaderId);

  glGetShaderiv(vShaderId, GL_COMPILE_STATUS, &shaderStatus);
  if (!shaderStatus) {
    glGetShaderInfoLog(vShaderId, 512, NULL, shaderLog);
    Log(LOG_ERROR, "SHADER: could not compile vertex shader: %s", shaderLog);
    goto terminate;
  }

  fsCode = ReadTextFile(fsFilename);
  if (fsCode == NULL) {
    Log(LOG_ERROR, "SHADER: could not load fragment shader: %s", fsFilename);
    goto terminate;
  }

  fShaderId = glCreateShader(GL_FRAGMENT_SHADER);
  glShaderSource(fShaderId, 1, (const char **)&fsCode, NULL);
  glCompileShader(fShaderId);

  glGetShaderiv(fShaderId, GL_COMPILE_STATUS, &shaderStatus);
  if (!shaderStatus) {
    glGetShaderInfoLog(fShaderId, 512, NULL, shaderLog);
    Log(LOG_ERROR, "SHADER: could not compile fragment shader: %s", shaderLog);
    goto terminate;
  }

  shaderProgramId = glCreateProgram();
  glAttachShader(shaderProgramId, vShaderId);
  glAttachShader(shaderProgramId, fShaderId);
  glLinkProgram(shaderProgramId);

  glGetShaderiv(shaderProgramId, GL_LINK_STATUS, &shaderStatus);
  if (!shaderStatus) {
    glGetProgramInfoLog(shaderProgramId, 512, NULL, shaderLog);
    Log(LOG_ERROR, "SHADER: could not link shader program: %s", shaderLog);
    goto terminate;
  }

terminate:
  if (vShaderId != 0) {
    glDeleteShader(vShaderId);
  }

  if (fShaderId != 0) {
    glDeleteShader(fShaderId);
  }

  if (vsCode != NULL) {
    free(vsCode);
  }

  if (fsCode != NULL) {
    free(fsCode);
  }

  return shaderProgramId;
}

unsigned LoadTexture(const char *filename) {
  unsigned textureId = 0;
  unsigned imageWidth = 0;
  unsigned imageHeight = 0;
  unsigned char *imageData = ReadPNGFile(filename, &imageWidth, &imageHeight);
  if (imageData == NULL) {
    return 0;
  }

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

  glGenTextures(1, &textureId);
  glBindTexture(GL_TEXTURE_2D, textureId);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, (int)imageWidth, (int)imageHeight, 0,
               GL_RGBA, GL_UNSIGNED_BYTE, imageData);
  glGenerateMipmap(GL_TEXTURE_2D);
  free(imageData);
  return textureId;
}

BitmapFont LoadBitmapFont(const char *atlasFilename, const char *descFilename) {
  char *fontDescData = NULL;
  char *line = NULL;
  BitmapFont font = {0};

  font.shaderProgramId = LoadShader(ASSETS_GLYPH_VS, ASSETS_GLYPH_FS);
  if (font.shaderProgramId == 0) {
    font.status = ERROR_CANNOT_LOAD_GLYPH_SHADER;
    goto terminate;
  }

  font.textureId = LoadTexture(atlasFilename);
  if (font.textureId == 0) {
    font.status = ERROR_CANNOT_LOAD_ATLAS_FILE;
    goto terminate;
  }

  fontDescData = ReadTextFile(descFilename);
  if (fontDescData == NULL) {
    font.status = ERROR_CANNOT_LOAD_DESC_FILE;
    goto terminate;
  }

  line = fontDescData;
  if (!HasPrefix(line, "info")) {
    Log(LOG_ERROR, "invalid info section");
    font.status = ERROR_INVALID_DESCRIPTION;
    goto terminate;
  }

  font.fontSize = (float)GetLineAttrInt(line, 1);
  line = GetNextLine(line);

  if (!HasPrefix(line, "common")) {
    Log(LOG_ERROR, "invalid common section");
    font.status = ERROR_INVALID_DESCRIPTION;
    goto terminate;
  }
  float scaleW = (float)GetLineAttrInt(line, 2);
  float scaleH = (float)GetLineAttrInt(line, 3);

  line = GetNextLine(line);
  line = GetNextLine(line);

  if (!HasPrefix(line, "chars")) {
    Log(LOG_ERROR, "invalid chars section");
    font.status = ERROR_INVALID_DESCRIPTION;
    goto terminate;
  }

  unsigned charCount = (unsigned)GetLineAttrInt(line, 0);
  for (unsigned i = 0; i < charCount; i++) {
    line = GetNextLine(line);
    if (!HasPrefix(line, "char")) {
      break;
    }

    unsigned id = GetLineAttrInt(line, 0);
    if (id > 255) {
      Log(LOG_WARN, "unsupported character outside range: %d", id);
      continue;
    }

    float x = (float)GetLineAttrInt(line, 1);
    float y = (float)GetLineAttrInt(line, 2);
    float w = (float)GetLineAttrInt(line, 3);
    float h = (float)GetLineAttrInt(line, 4);
    float xo = (float)GetLineAttrInt(line, 5);
    float yo = (float)GetLineAttrInt(line, 6);
    float xa = (float)GetLineAttrInt(line, 7);
    Mesh quad = MakeGlyphQuad(scaleW, scaleH, x, y, w, h);
    font.quads[id] = quad;
    font.xos[id] = xo;
    font.yos[id] = yo;
    font.xas[id] = xa;
  }

terminate:
  if (fontDescData != NULL) {
    free(fontDescData);
  }

  return font;
}

void UnloadBitmapFont(BitmapFont font) {
  if (font.shaderProgramId != 0) {
    glDeleteProgram(font.shaderProgramId);
  }

  for (unsigned i = 0; i < 256; i++) {
    Mesh quad = font.quads[i];
    if (quad.vao != 0) {
      glDeleteVertexArrays(1, &quad.vao);
    }

    if (quad.vbo != 0) {
      glDeleteBuffers(1, &quad.vbo);
    }

    if (quad.ebo != 0) {
      glDeleteBuffers(1, &quad.ebo);
    }
  }
}

char *ReadTextFile(const char *filename) {
  FILE *file = fopen(filename, "re");
  if (file == NULL) {
    Log(LOG_ERROR, "FILE: could not read file: %s", filename);
    return NULL;
  }

  fseek(file, 0, SEEK_END);
  size_t dataLen = ftell(file);
  fseek(file, 0, SEEK_SET);

  char *data = calloc(dataLen + 1, sizeof(char));
  if (data == NULL) {
    Log(LOG_ERROR, "FILE: could not allocate memory for file: %s", filename);
    fclose(file);
    return NULL;
  }

  fread(data, sizeof(char), dataLen, file);
  fclose(file);
  return data;
}

unsigned char *ReadPNGFile(const char *filename, unsigned *width,
                           unsigned *height) {
  unsigned char *imageData = NULL;
  FILE *file = NULL;
  png_byte header[8] = {0};
  png_structp pngHandler = NULL;
  png_bytepp rowPointers = NULL;
  png_infop pngInfo = NULL;
  size_t rowBytes;
  unsigned tmpHeight;

  file = fopen(filename, "rbe");
  if (file == NULL) {
    Log(LOG_ERROR, "PNG: could not open file: %s", filename);
    goto terminate;
  }

  // Read header
  if (fread(header, 1, sizeof(header), file) < 8) {
    Log(LOG_ERROR, "PNG: file is too short to be an PNG file: %s", filename);
    goto terminate;
  }

  if (!png_check_sig(header, 8)) {
    Log(LOG_ERROR, "PNG: not a valid PNG file: %s", filename);
    goto terminate;
  }

  // Create read struct
  pngHandler = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
  if (pngHandler == NULL) {
    Log(LOG_ERROR, "PNG: cannot initialize the library for file: %s", filename);
    goto terminate;
  }

  // Get info struct
  pngInfo = png_create_info_struct(pngHandler);
  if (pngInfo == NULL) {
    Log(LOG_ERROR, "PNG: cannot initialize pngInfo for file: %s", filename);
    goto terminate;
  }

  // Exception handling code
  if (setjmp(png_jmpbuf(pngHandler))) {
    Log(LOG_ERROR, "PNG: cannot initialize exceptions for file: %s", filename);
    goto terminate;
  }

  png_init_io(pngHandler, file);
  png_set_sig_bytes(pngHandler, sizeof(header)); // shift header
  png_read_info(pngHandler, pngInfo);            // read info

  // Get image width and height from info
  png_get_IHDR(pngHandler, pngInfo, width, height, NULL, NULL, NULL, NULL,
               NULL);
  png_read_update_info(pngHandler, pngInfo);
  Log(LOG_INFO, "PNG: %s: %u x %u", filename, *width, *height);

  // Row size in bytes
  rowBytes = png_get_rowbytes(pngHandler, pngInfo);
  tmpHeight = *height;

  // Allocate image block
  imageData = calloc(rowBytes * tmpHeight, sizeof(unsigned char));
  if (imageData == NULL) {
    Log(LOG_ERROR, "PNG: could not allocate memory for file: %s (%ld bytes)",
        filename, rowBytes * tmpHeight);
    goto terminate;
  }

  // Allocate memory for all rowPointers to help decode data
  rowPointers = calloc(tmpHeight, sizeof(png_bytep));
  if (rowPointers == NULL) {
    Log(LOG_ERROR, "PNG: could not allocate memory for file: %s (%ld bytes)",
        filename, tmpHeight * sizeof(png_bytep));
  }

  // Prepare rowPointers to point into imageData.
  for (unsigned i = 0; i < tmpHeight; i++) {
    rowPointers[tmpHeight - 1 - i] = imageData + i * rowBytes;
  }

  // Read full image into rowPointers (and thus, imageData)
  png_read_image(pngHandler, rowPointers);

terminate:
  if (file != NULL) {
    fclose(file);
  }

  if (pngHandler != NULL && pngInfo != NULL) {
    png_destroy_read_struct(&pngHandler, &pngInfo, NULL);
  }

  if (rowPointers != NULL) {
    free(rowPointers);
  }

  return imageData;
}

bool HasPrefix(const char *line, const char *prefix) {
  size_t headerPos;
  size_t prefixLen = strlen(prefix);

  for (headerPos = 0; headerPos < strlen(line); headerPos++) {
    if (line[headerPos] == ' ') {
      break;
    }
  }

  return headerPos == prefixLen && strncmp(line, prefix, headerPos) == 0;
}

int GetLineAttrInt(const char *line, unsigned attrId) {
  unsigned off = 0;
  for (unsigned i = 0; i < strlen(line); i++) {
    if (line[i] == '=') {
      if (off == attrId) {
        return (int)strtol(line + i + 1, NULL, 10);
      }

      off++;
    }
  }

  return 0;
}

char *GetNextLine(char *line) { return strchr(line, '\n') + 1; }

Mesh MakeGlyphQuad(float imgW, float imgH, float x, float y, float w, float h) {
  Mesh mesh = {0};
  float relW = w / imgW;
  float relH = h / imgH;
  // clang-format off
  float vertices[] = {
      // pos            // col            // uvs
      relW, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, (x + w) / imgW, 1.0f - (y / imgH),       // top right
      relW, relH, 0.0f, 1.0f, 1.0f, 1.0f, (x + w) / imgW, 1.0f - ((y + h) / imgH), // bottom right
      0.0f, relH, 0.0f, 1.0f, 1.0f, 1.0f, x/imgW,         1.0f - ((y + h) / imgH), // bottom left
      0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, x/imgW,         1.0f - (y / imgH),       // top left
  };
  // clang-format on
  const unsigned short indices[] = {
      0, 1, 3, // First triangle
      1, 2, 3, // Second triangle
  };

  glGenVertexArrays(1, &mesh.vao);
  glGenBuffers(1, &mesh.vbo);
  glGenBuffers(1, &mesh.ebo);
  glBindVertexArray(mesh.vao);

  glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo);
  glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.ebo);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices,
               GL_STATIC_DRAW);

  GLsizei vertexStride = 8 * sizeof(float);
  // Position
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, vertexStride,
                        (void *)(0 * sizeof(float)));
  glEnableVertexAttribArray(0);

  // Color
  glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, vertexStride,
                        (void *)(3 * sizeof(float)));
  glEnableVertexAttribArray(1);

  // UVs
  glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, vertexStride,
                        (void *)(6 * sizeof(float)));
  glEnableVertexAttribArray(2);
  return mesh;
}

void RenderText(BitmapFont font, float xPos, float yPos, const char *text) {
  size_t textLen = strlen(text);
  float xOffset = xPos;
  float yOffset = yPos;

  float proj[16] = {0};
  MakeOrthoProj(proj, -1.0f, 1.0f, -1.0f, 1.0f, 1.0f, 100.0f);

  for (size_t i = 0; i < textLen; i++) {
    unsigned id = (unsigned)text[i];
    unsigned vao = font.quads[id].vao;
    if (vao == 0) {
      Log(LOG_WARN, "TEXT: do not have a glyph for '%c' (%d)", id, (int)id);
      continue;
    }

    float model[16] = {0};
    float xRel = 0, yRel = 0;
    WorldToViewport(&xRel, &yRel, xOffset + font.xos[id],
                    yOffset + font.yos[id]);
    MakeGlyphTransform(model, xRel, yRel);

    unsigned pid = font.shaderProgramId;
    glUseProgram(pid);
    glBindTexture(GL_TEXTURE_2D, font.textureId);

    glBindVertexArray(vao);
    glUniformMatrix4fv(glGetUniformLocation(pid, "model"), 1, GL_TRUE, model);
    glUniformMatrix4fv(glGetUniformLocation(pid, "proj"), 1, GL_FALSE, proj);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, NULL);

    glBindVertexArray(0);
    glUseProgram(0);

    xOffset += font.xas[id];
  }
}

void MakeOrthoProj(float m[16], float l, float r, float t, float b, float n,
                   float f) {
  m[0] = 2 / (r - l);
  m[3] = -(r + l) / (r - l);
  m[5] = 2 / (t - b);
  m[7] = -(t + b) / (t - b);
  m[10] = -2 / (f - n);
  m[11] = -(f + n) / (f - n);
  m[15] = 1.0f;
}

void MakeGlyphTransform(float m[16], float x, float y) {
  m[0] = 1.0f;
  m[3] = x;
  m[5] = 1.0f;
  m[7] = y;
  m[10] = 1.0f;
  m[15] = 1.0f;
}

void WorldToViewport(float *vx, float *vy, float x, float y) {
  *vx = (x / WINDOW_WIDTH * 2.0f) - 1.0f;
  *vy = (-y / WINDOW_HEIGHT * 2.0f) + 1.0f;
}
