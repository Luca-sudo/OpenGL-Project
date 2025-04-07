#include <stdio.h>
#include <stdlib.h>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <cgltf/cgltf.h>

#include <cglm/cglm.h>

#include "glb_parser.h"

void framebuffer_size_callback(GLFWwindow *window, int width, int height) {
  glViewport(0, 0, width, height);
}

const char *vertexShaderSource = "#version 330 core\n"
                                     "layout (location = 0) in vec3 aPos;\n"
                                     "layout (location = 1) in vec3 aColor;\n"
                                     "out vec3 color;\n"
                                     "uniform mat4 projection;\n"
                                     "uniform mat4 view;\n"
                                     "uniform mat4 model;\n"
                                     "void main() {\n"
                                     "   gl_Position = projection * view * model * vec4(aPos, 1.0);\n"
                                     "   color = aColor;\n"
                                     "}\0";

const char *fragmentShaderSource = "#version 330 core\n"
                                   "out vec4 FragColor;\n"
                                   "in vec3 color;\n"
                                   "void main() {\n"
                                   "   FragColor = vec4(color, 1.0f);\n"
                                   "}\n\0";

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
    printf("GLFW window creation failed (Error %d): %s\n", errorCode, errorDesc ? errorDesc : "Unknown error");
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

  // vertex shader
  unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER);
  glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
  glCompileShader(vertexShader);

  // check for vertex shader compile errors
  int success;
  char infoLog[512];
  glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
  if (!success) {
    glGetShaderInfoLog(vertexShader, 512, NULL, infoLog);
    printf("ERROR::SHADER::VERTEX::COMPILATION_FAILED\n%s\n", infoLog);
  }

  // fragment shader
  unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
  glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
  glCompileShader(fragmentShader);

  // check for fragment shader compile errors
  glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
  if (!success) {
    glGetShaderInfoLog(fragmentShader, 512, NULL, infoLog);
    printf("ERROR::SHADER::FRAGMENT::COMPILATION_FAILED\n%s\n", infoLog);
  }

  // link shaders
  unsigned int shaderProgram = glCreateProgram();
  glAttachShader(shaderProgram, vertexShader);
  glAttachShader(shaderProgram, fragmentShader);
  glLinkProgram(shaderProgram);

  // check for linking errors
  glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
  if (!success) {
    glGetProgramInfoLog(shaderProgram, 512, NULL, infoLog);
    printf("ERROR::SHADER::PROGRAM::LINKING_FAILED\n%s\n", infoLog);
  }

  // delete shaders as they are no longer needed
  glDeleteShader(vertexShader);
  glDeleteShader(fragmentShader);

  // extract vertices from glb file
  int vertex_count = 0;
  float* vertex_data = extract_vertex_data_from_glb("assets/cornell_box.glb", &vertex_count);

  if (!vertex_data) {
    printf("Failed to load vertex data from GLB file.\n");
    return -1;
  }

  // Create a Vertex Array Object (VAO) and Vertex Buffer Object (VBO)
  unsigned int VAO, VBO;
  glGenVertexArrays(1, &VAO);
  glGenBuffers(1, &VBO);

  glBindVertexArray(VAO);

  // Bind the VBO, upload vertex data
  glBindBuffer(GL_ARRAY_BUFFER, VBO);
  glBufferData(GL_ARRAY_BUFFER, vertex_count * 6 * sizeof(float), vertex_data, GL_STATIC_DRAW);

  // Position attribute
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
  glEnableVertexAttribArray(0);

  // Color attribute
  glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
  glEnableVertexAttribArray(1);

  free(vertex_data);

  // Set up projection, view, and model matrices
  mat4 projection, view, model;
  glm_perspective(glm_rad(45.0f), 800.0f / 600.0f, 0.1f, 100.0f, projection);
  glm_mat4_identity(view);  // Initialize view matrix to identity
  glm_translate(view, (vec3){0.0f, 0.0f, -3.0f});  // Camera at z = -3
  glm_mat4_identity(model);  // Identity matrix (no transformation)

  // Set up uniform locations
  int projectionLoc = glGetUniformLocation(shaderProgram, "projection");
  int viewLoc = glGetUniformLocation(shaderProgram, "view");
  int modelLoc = glGetUniformLocation(shaderProgram, "model");

  // render loop
  while (!glfwWindowShouldClose(window)) {

    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);  // Clear color (dark gray)
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Activate shader program
    glUseProgram(shaderProgram);

    // Pass matrices to shaders
    glUniformMatrix4fv(projectionLoc, 1, GL_FALSE, (const GLfloat*)projection);
    glUniformMatrix4fv(viewLoc, 1, GL_FALSE, (const GLfloat*)view);
    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, (const GLfloat*)model);

    // Draw the object
    glBindVertexArray(VAO);
    glDrawArrays(GL_TRIANGLES, 0, vertex_count);

    glfwSwapBuffers(window);
    glfwPollEvents();
  }

  glfwTerminate();

  return 0;
}

