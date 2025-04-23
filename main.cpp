#include "assimp/color4.h"
#include "assimp/material.h"
#include "assimp/mesh.h"
#include "assimp/vector3.h"
#include "bits/types/struct_timeval.h"
#include "cglm/types.h"
#include "include/shader.h"
#include <assimp/cimport.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <cglm/cglm.h>
#include <glad/glad.h>
#include <math.h>
#include <stdio.h>
#include <time.h>

#include <GLFW/glfw3.h>
#include <string.h>

#define KB(x) (x * 1024)

typedef struct {
  aiVector3D *vertices;
  aiColor4D *albedo;
  unsigned int *indices;

  unsigned int vertexOffset;
  unsigned indexOffset;
} model_t;

void framebuffer_size_callback(GLFWwindow *window, int width, int height) {
  glViewport(0, 0, width, height);
}

void extract_indices(model_t *model, struct aiNode *node,
                     const struct aiScene *scene) {
  for (int meshIndex = 0; meshIndex < node->mNumMeshes; meshIndex++) {
    unsigned int meshId = node->mMeshes[meshIndex];
    unsigned int materialId = scene->mMeshes[meshId]->mMaterialIndex;
    for (int faceIdx = 0; faceIdx < scene->mMeshes[meshId]->mNumFaces;
         faceIdx++) {
      struct aiFace face = scene->mMeshes[meshId]->mFaces[faceIdx];
      for (int i = 0; i < face.mNumIndices; i++) {
        model->indices[model->indexOffset + i] =
            face.mIndices[i] + (model->vertexOffset);
      }
      model->indexOffset += face.mNumIndices;
    }

    aiColor4D albedo;
    if (AI_SUCCESS == aiGetMaterialColor(scene->mMaterials[materialId],
                                         AI_MATKEY_COLOR_DIFFUSE, &albedo)) {

    } else {
      printf("Failed to load albedo color!\n");
    }

    for (int vertexIdx = 0; vertexIdx < scene->mMeshes[meshId]->mNumVertices;
         vertexIdx++) {
      model->vertices[model->vertexOffset + vertexIdx] =
          scene->mMeshes[meshId]->mVertices[vertexIdx];
      model->albedo[model->vertexOffset + vertexIdx] = albedo;
    }

    model->vertexOffset += scene->mMeshes[meshId]->mNumVertices;
  }

  for (int childIdx = 0; childIdx < node->mNumChildren; childIdx++) {
    extract_indices(model, node->mChildren[childIdx], scene);
  }
}

float CUBE_VERTICES[] = {

    -0.5f, -0.5f, 0.5f, // front
    0.5f,  -0.5f, 0.5f,  0.5f,  0.5f,  0.5f,  -0.5f, -0.5f,
    0.5f,  0.5f,  0.5f,  0.5f,  -0.5f, 0.5f,  0.5f,

    -0.5f, -0.5f, -0.5f, // back
    0.5f,  -0.5f, -0.5f, 0.5f,  0.5f,  -0.5f, -0.5f, -0.5f,
    -0.5f, 0.5f,  0.5f,  -0.5f, -0.5f, 0.5f,  -0.5f,

    -0.5f, -0.5f, 0.5f, // left
    -0.5f, 0.5f,  0.5f,  -0.5f, -0.5f, -0.5f, -0.5f, -0.5f,
    -0.5f, -0.5f, 0.5f,  0.5f,  -0.5f, 0.5f,  -0.5f,

    0.5f,  -0.5f, 0.5f, // right
    0.5f,  0.5f,  0.5f,  0.5f,  -0.5f, -0.5f, 0.5f,  -0.5f,
    -0.5f, 0.5f,  0.5f,  0.5f,  0.5f,  0.5f,  -0.5f,
};

const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 600;

float CUBE_COLORS[] = {
    1.0f, 0.0f, 0.0f, // red
    0.0f, 1.0f, 0.0f, // green
    0.0f, 0.0f, 1.0f, // blue

    1.0f, 0.0f, 0.0f, // red
    0.0f, 1.0f, 0.0f, // green
    0.0f, 0.0f, 1.0f, // blue

    1.0f, 0.0f, 0.0f, // red
    0.0f, 1.0f, 0.0f, // green
    0.0f, 0.0f, 1.0f, // blue

    1.0f, 0.0f, 0.0f, // red
    0.0f, 1.0f, 0.0f, // green
    0.0f, 0.0f, 1.0f, // blue

    1.0f, 0.0f, 0.0f, // red
    0.0f, 1.0f, 0.0f, // green
    0.0f, 0.0f, 1.0f, // blue

    1.0f, 0.0f, 0.0f, // red
    0.0f, 1.0f, 0.0f, // green
    0.0f, 0.0f, 1.0f, // blue
};
int main() {

  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

  if (!glfwInit()) {
    printf("GLFW initialization failed\n");
    return -1;
  }

  GLFWwindow *window =
      glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Test", NULL, NULL);
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

  glEnable(GL_DEPTH_TEST);

  glViewport(0, 0, 800, 600);

  glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

  const C_STRUCT aiScene *scene = aiImportFile(
      "assets/cornell_box_v1.gltf", aiProcessPreset_TargetRealtime_MaxQuality);

  if (scene == NULL) {
    printf("Failed to load scene\n");
    glfwTerminate();
    return -1;
  }

  // FIXME: Uses the structure of example scene for now.
  struct aiNode *root = scene->mRootNode;

  model_t cornellBox = {};

  cornellBox.indices = (unsigned int *)malloc(KB(10));
  if (cornellBox.indices == NULL) {
    printf("Failed to allocate memory for scene vertices.\n");
    glfwTerminate();
    return -1;
  }

  cornellBox.vertices = (aiVector3D*)malloc(KB(10));
  if (cornellBox.vertices == NULL) {
    printf("Failed to allocate buffer for vertices\n");
    glfwTerminate();
    return -1;
  }

  cornellBox.albedo = (aiColor4D*)malloc(KB(10));
  if (cornellBox.albedo == NULL) {
    printf("Failed to allocate buffer for albedo data\n");
    glfwTerminate();
    return -1;
  }

  extract_indices(&cornellBox, root, scene);

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

  unsigned int VAO, EBO, positions, albedo;
  glGenVertexArrays(1, &VAO);
  glGenBuffers(1, &positions);
  glGenBuffers(1, &albedo);
  glGenBuffers(1, &EBO);

  // Configure VAO
  glBindVertexArray(VAO);

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER,
               sizeof(unsigned int) * cornellBox.indexOffset,
               cornellBox.indices, GL_STATIC_DRAW);

  glBindBuffer(GL_ARRAY_BUFFER, positions);
  glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 3 * cornellBox.vertexOffset,
               cornellBox.vertices, GL_STATIC_DRAW);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void *)0);
  glEnableVertexAttribArray(0);

  glBindBuffer(GL_ARRAY_BUFFER, albedo);
  glBufferData(GL_ARRAY_BUFFER,
               sizeof(aiColor4D) * cornellBox.vertexOffset,
               cornellBox.albedo, GL_STATIC_DRAW);
  glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(aiColor4D),
                        (void *)0);
  glEnableVertexAttribArray(1);

  while (!glfwWindowShouldClose(window)) {

    struct timespec time;

    clock_gettime(CLOCK_MONOTONIC, &time);

    double currentTime = time.tv_sec + time.tv_nsec / 1000000000.0f;

    glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // create transformations
    mat4 model, view, projection;

    glm_mat4_identity(model);
    glm_mat4_identity(view);
    glm_mat4_identity(projection);

    // vec3 eye = {3 * cos(currentTime), 0.0f, 3 * sin(currentTime)};
    vec3 eye = {3.0f, 3.0f, -8.0f};
    vec3 up = {0.0f, 1.0f, 0.0f};
    vec3 dir = {0.0f, 0.0f, 1.0f};

    // model = rotate(model, radians(-55.0f), vec3(1.0, 0.0, 0.0));
    // glm_rotate(model, glm_rad(-55.0f), (vec3){1.0f, 0.0f, 0.0f});

    // view = translate(view, vec3(0.0, 0.0, -3.0));
    glm_look(eye, dir, up, view);

    // projection = perspective(radians(45.0f), aspect, near, far)
    glm_perspective(glm_rad(45.0f), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f,
                    100.0f, projection);

    glUseProgram(shaderProgram);

    // set uniforms
    unsigned int modelLoc = glGetUniformLocation(shaderProgram, "model");
    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, &model[0][0]);
    unsigned int viewLoc = glGetUniformLocation(shaderProgram, "view");
    glUniformMatrix4fv(viewLoc, 1, GL_FALSE, &view[0][0]);
    unsigned int projectionLoc =
        glGetUniformLocation(shaderProgram, "projection");
    glUniformMatrix4fv(projectionLoc, 1, GL_FALSE, &projection[0][0]);

    glBindVertexArray(VAO);
    glDrawElements(GL_TRIANGLES, cornellBox.indexOffset, GL_UNSIGNED_INT, 0);

    glfwSwapBuffers(window);
    glfwPollEvents();
  }

  glfwTerminate();
  free(cornellBox.albedo);
  free(cornellBox.vertices);
  free(cornellBox.indices);

  return 0;
}
