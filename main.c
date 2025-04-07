#include "include/shader.h"
#include <glad/glad.h>

#include <GLFW/glfw3.h>
#include <stdio.h>

void framebuffer_size_callback(GLFWwindow *window, int width, int height) {
  glViewport(0, 0, width, height);
}

float TRIANGLE_VERTICES[] = {-0.5f, -0.5f, 0.0f, 0.5f, -0.5f,
                             0.0f,  0.0f,  0.5f, 0.0f};

int main() {

  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

  if (!glfwInit()) {
    printf("GLFW initialization failed\n");
    return -1;
  }

  GLFWwindow *window = glfwCreateWindow(800, 600, "Test", NULL, NULL);
  if (!window) {
    const char *errorDesc;
    int errorCode = glfwGetError(&errorDesc);
    printf("GLFW window creation failed (Error %d): %s\n", errorCode,
           errorDesc ? errorDesc : "Unknown error");
    glfwTerminate();
    return -1;
  }

  glfwMakeContextCurrent(window);

  if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
    printf("Failed to initialize GLAD");
    return -1;
  }

  glViewport(0, 0, 800, 600);

  glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

  unsigned int vertexShader, fragShader, shaderProgram;
  vertexShader = glCreateShader(GL_VERTEX_SHADER);
  char *vertShaderCode = read_shader_from_file("shaders/triangle.vert");
  glShaderSource(vertexShader, 1, (const char *const *)&vertShaderCode, NULL);
  glCompileShader(vertexShader);

  fragShader = glCreateShader(GL_FRAGMENT_SHADER);
  char *fragShaderCode = read_shader_from_file("shaders/triangle.frag");
  glShaderSource(fragShader, 1, (const char *const *)&fragShaderCode, NULL);
  glCompileShader(fragShader);

  shaderProgram = glCreateProgram();
  glAttachShader(shaderProgram, vertexShader);
  glAttachShader(shaderProgram, fragShader);
  glLinkProgram(shaderProgram);

  glDeleteShader(vertexShader);
  glDeleteShader(fragShader);

  unsigned int VAO, VBO;
  glGenVertexArrays(1, &VAO);
  glGenBuffers(1, &VBO);

  // Configure VAO
  glBindVertexArray(VAO);
  glBindBuffer(GL_ARRAY_BUFFER, VBO);
  glBufferData(GL_ARRAY_BUFFER, sizeof(TRIANGLE_VERTICES), TRIANGLE_VERTICES,
               GL_STATIC_DRAW);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void *)0);
  glEnableVertexAttribArray(0);

  while (!glfwWindowShouldClose(window)) {
    glfwSwapBuffers(window);
    glfwPollEvents();

    glUseProgram(shaderProgram);
    glBindVertexArray(VAO);
    glDrawArrays(GL_TRIANGLES, 0, 3);
  }

  glfwTerminate();

  return 0;
}
