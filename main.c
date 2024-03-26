#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// clang-format off
#include <glad/glad.h>
#include <GLFW/glfw3.h>
// clang-format on

#define WINDOW_TITLE "SimpleFontRendering"
#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 800

typedef enum {
  LOG_INFO,
  LOG_WARN,
  LOG_ERROR,
} LogLevel;

typedef struct {
  float width;
  float height;
  float aspect;
} AppState;

void Log(LogLevel level, const char *fmt, ...);

// warn: entire app state
static AppState globalState = {0};

int main() {
  GLFWwindow *window = NULL;
  int status = EXIT_SUCCESS;

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
      // RenderText(font, 0.0f, 0.0f, "Hello world");
    }
    // Finish render
    glfwPollEvents();
    glfwSwapBuffers(window);
  }

terminate:
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
