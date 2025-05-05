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

#include <imgui/imgui.h>
#include <imgui/backends/imgui_impl_glfw.h>
#include <imgui/backends/imgui_impl_opengl3.h>

#define KB(x) (x * 1024)

typedef struct {
  aiVector3D *vertices;
  aiColor4D *albedo;
  aiVector3D *normals;
  unsigned int *indices;

  unsigned int vertexOffset;
  unsigned indexOffset;
} model_t;

void framebuffer_size_callback(GLFWwindow *window, int width, int height) {
  glViewport(0, 0, width, height);
}

int allocate_model(model_t *model) {
  model->indices = (unsigned int *)malloc(KB(10));
  if (model->indices == NULL) {
    printf("Failed to allocate memory for scene vertices.\n");
    glfwTerminate();
    return -1;
  }

  model->vertices = (aiVector3D*)malloc(KB(10));
  if (model->vertices == NULL) {
    printf("Failed to allocate buffer for vertices\n");
    glfwTerminate();
    return -1;
  }

  model->albedo = (aiColor4D*)malloc(KB(10));
  if (model->albedo == NULL) {
    printf("Failed to allocate buffer for albedo data\n");
    glfwTerminate();
    return -1;
  }

  model->normals = (aiVector3D*)malloc(KB(10));
  if (model->normals == NULL) {
    printf("Failed to allocate buffer for normals\n");
    glfwTerminate();
    return -1;
  }

  return 0;
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
      model->normals[model->vertexOffset + vertexIdx] = scene->mMeshes[meshId]->mNormals[vertexIdx];
    }

    model->vertexOffset += scene->mMeshes[meshId]->mNumVertices;
  }

  for (int childIdx = 0; childIdx < node->mNumChildren; childIdx++) {
    extract_indices(model, node->mChildren[childIdx], scene);
  }
}

const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 600;

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

  if (allocate_model(&cornellBox) < 0) {
    return -1;
  }

  extract_indices(&cornellBox, root, scene);

  // Load shader programs
  unsigned int shadowMapShader, renderShader;
  
  // Shadow mapping shader program
  unsigned int vertexShader, geomShader, fragShader;
  
  // Shadow depth vertex shader
  vertexShader = glCreateShader(GL_VERTEX_SHADER);
  char *shadowVertShaderCode = read_shader_from_file("shaders/depth_shader.vert");
  glShaderSource(vertexShader, 1, (const char *const *)&shadowVertShaderCode, NULL);
  glCompileShader(vertexShader);
  // Check for compilation errors
  int success;
  char infoLog[512];
  glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
  if(!success) {
    glGetShaderInfoLog(vertexShader, 512, NULL, infoLog);
    printf("ERROR::SHADER::VERTEX::COMPILATION_FAILED\n%s\n", infoLog);
  }
  
  // Shadow depth geometry shader
  geomShader = glCreateShader(GL_GEOMETRY_SHADER);
  char *shadowGeomShaderCode = read_shader_from_file("shaders/depth_shader.geom");
  glShaderSource(geomShader, 1, (const char *const *)&shadowGeomShaderCode, NULL);
  glCompileShader(geomShader);
  // Check for compilation errors
  glGetShaderiv(geomShader, GL_COMPILE_STATUS, &success);
  if(!success) {
    glGetShaderInfoLog(geomShader, 512, NULL, infoLog);
    printf("ERROR::SHADER::GEOMETRY::COMPILATION_FAILED\n%s\n", infoLog);
  }
  
  // Shadow depth fragment shader
  fragShader = glCreateShader(GL_FRAGMENT_SHADER);
  char *shadowFragShaderCode = read_shader_from_file("shaders/depth_shader.frag");
  glShaderSource(fragShader, 1, (const char *const *)&shadowFragShaderCode, NULL);
  glCompileShader(fragShader);
  // Check for compilation errors
  glGetShaderiv(fragShader, GL_COMPILE_STATUS, &success);
  if(!success) {
    glGetShaderInfoLog(fragShader, 512, NULL, infoLog);
    printf("ERROR::SHADER::FRAGMENT::COMPILATION_FAILED\n%s\n", infoLog);
  }
  
  // Link shadow mapping shader program
  shadowMapShader = glCreateProgram();
  glAttachShader(shadowMapShader, vertexShader);
  glAttachShader(shadowMapShader, geomShader);
  glAttachShader(shadowMapShader, fragShader);
  glLinkProgram(shadowMapShader);
  // Check for linking errors
  glGetProgramiv(shadowMapShader, GL_LINK_STATUS, &success);
  if(!success) {
    glGetProgramInfoLog(shadowMapShader, 512, NULL, infoLog);
    printf("ERROR::SHADER::PROGRAM::LINKING_FAILED\n%s\n", infoLog);
  }
  
  glDeleteShader(vertexShader);
  glDeleteShader(geomShader);
  glDeleteShader(fragShader);
  
  // Blinn-Phong rendering shader program
  vertexShader = glCreateShader(GL_VERTEX_SHADER);
  char *vertShaderCode = read_shader_from_file("shaders/point_shadows.vert");
  glShaderSource(vertexShader, 1, (const char *const *)&vertShaderCode, NULL);
  glCompileShader(vertexShader);
  // Check for compilation errors
  glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
  if(!success) {
    glGetShaderInfoLog(vertexShader, 512, NULL, infoLog);
    printf("ERROR::SHADER::VERTEX::COMPILATION_FAILED\n%s\n", infoLog);
  }

  fragShader = glCreateShader(GL_FRAGMENT_SHADER);
  char *fragShaderCode = read_shader_from_file("shaders/point_shadows.frag");
  glShaderSource(fragShader, 1, (const char *const *)&fragShaderCode, NULL);
  glCompileShader(fragShader);
  // Check for compilation errors
  glGetShaderiv(fragShader, GL_COMPILE_STATUS, &success);
  if(!success) {
    glGetShaderInfoLog(fragShader, 512, NULL, infoLog);
    printf("ERROR::SHADER::FRAGMENT::COMPILATION_FAILED\n%s\n", infoLog);
  }

  renderShader = glCreateProgram();
  glAttachShader(renderShader, vertexShader);
  glAttachShader(renderShader, fragShader);
  glLinkProgram(renderShader);
  // Check for linking errors
  glGetProgramiv(renderShader, GL_LINK_STATUS, &success);
  if(!success) {
    glGetProgramInfoLog(renderShader, 512, NULL, infoLog);
    printf("ERROR::SHADER::PROGRAM::LINKING_FAILED\n%s\n", infoLog);
  }

  glDeleteShader(vertexShader);
  glDeleteShader(fragShader);

  unsigned int VAO, EBO, positions, albedo, normals;
  glGenVertexArrays(1, &VAO);
  glGenBuffers(1, &positions);
  glGenBuffers(1, &albedo);
  glGenBuffers(1, &EBO);
  glGenBuffers(1, &normals);

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

  glBindBuffer(GL_ARRAY_BUFFER, normals);
  glBufferData(GL_ARRAY_BUFFER, sizeof(aiVector3D) * cornellBox.vertexOffset,
              cornellBox.normals, GL_STATIC_DRAW);
  glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(aiVector3D), (void *)0);
  glEnableVertexAttribArray(2);

  // Set up depth cubemap for point shadows
  const unsigned int SHADOW_WIDTH = 1024, SHADOW_HEIGHT = 1024;
  float aspect = (float)SHADOW_WIDTH/(float)SHADOW_HEIGHT;
  float near_plane = 0.1f;
  float far_plane = 25.0f;
  
  // Create depth cubemap framebuffer object
  unsigned int depthMapFBO;
  glGenFramebuffers(1, &depthMapFBO);
  
  // Create depth cubemap texture
  unsigned int depthCubeMap;
  glGenTextures(1, &depthCubeMap);
  glBindTexture(GL_TEXTURE_CUBE_MAP, depthCubeMap);
  
  for (unsigned int i = 0; i < 6; ++i) {
    glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_DEPTH_COMPONENT, 
                 SHADOW_WIDTH, SHADOW_HEIGHT, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
  }
  
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
  
  // Attach depth texture as FBO's depth buffer
  glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
  glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, depthCubeMap, 0);
  glDrawBuffer(GL_NONE);
  glReadBuffer(GL_NONE);
  
  if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
    printf("Framebuffer not complete!\n");
  }
  
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  
  // Setup shadow transform matrices for point light
  mat4 shadowProj;
  glm_perspective(glm_rad(90.0f), aspect, near_plane, far_plane, shadowProj);
  
  // Setup Dear ImGui context
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO &io = ImGui::GetIO();
  // Setup Platform/Renderer bindings
  ImGui_ImplGlfw_InitForOpenGL(window, true);
  ImGui_ImplOpenGL3_Init("#version 330");

  // Setup Dear ImGui style
  ImGui::StyleColorsDark();

  // Light parameters to be adjusted in UI
  vec3 lightPos = {2.78f, 5.2f, 2.796f};
  vec3 lightColor = {1.0f, 1.0f, 1.0f};
  float lightIntensity = 1.0f;

  // Add a bias
  float shadowBias = 0.05f;

  while (!glfwWindowShouldClose(window)) {
    glfwPollEvents();

    struct timespec time;
    clock_gettime(CLOCK_MONOTONIC, &time);
    double currentTime = time.tv_sec + time.tv_nsec / 1000000000.0f;

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    // ImGui UI
    ImGui::Begin("Light Controls");
    ImGui::SliderFloat3("Light Position", lightPos, -10.0f, 10.0f);
    ImGui::SliderFloat("Shadow Bias", &shadowBias, 0.0f, 0.1f);
    ImGui::End();

    // 1. First render to depth cubemap
    // ------------------------------
    // Configure view port to the size of the point shadow texture
    glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
    glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
    glClear(GL_DEPTH_BUFFER_BIT);
    
    // Setup shadow transform matrices
    mat4 shadowMatrices[6];
    
    // Right
    mat4 lookAt1, view1;
    vec3 center1 = {lightPos[0] + 1.0f, lightPos[1], lightPos[2]};
    vec3 up1 = {0.0f, -1.0f, 0.0f};
    glm_lookat(lightPos, center1, up1, view1);
    glm_mat4_mul(shadowProj, view1, shadowMatrices[0]);
    
    // Left
    mat4 lookAt2, view2;
    vec3 center2 = {lightPos[0] - 1.0f, lightPos[1], lightPos[2]};
    vec3 up2 = {0.0f, -1.0f, 0.0f};
    glm_lookat(lightPos, center2, up2, view2);
    glm_mat4_mul(shadowProj, view2, shadowMatrices[1]);
    
    // Top
    mat4 lookAt3, view3;
    vec3 center3 = {lightPos[0], lightPos[1] + 1.0f, lightPos[2]};
    vec3 up3 = {0.0f, 0.0f, 1.0f};
    glm_lookat(lightPos, center3, up3, view3);
    glm_mat4_mul(shadowProj, view3, shadowMatrices[2]);
    
    // Bottom
    mat4 lookAt4, view4;
    vec3 center4 = {lightPos[0], lightPos[1] - 1.0f, lightPos[2]};
    vec3 up4 = {0.0f, 0.0f, -1.0f};
    glm_lookat(lightPos, center4, up4, view4);
    glm_mat4_mul(shadowProj, view4, shadowMatrices[3]);
    
    // Back
    mat4 lookAt5, view5;
    vec3 center5 = {lightPos[0], lightPos[1], lightPos[2] + 1.0f};
    vec3 up5 = {0.0f, -1.0f, 0.0f};
    glm_lookat(lightPos, center5, up5, view5);
    glm_mat4_mul(shadowProj, view5, shadowMatrices[4]);
    
    // Front
    mat4 lookAt6, view6;
    vec3 center6 = {lightPos[0], lightPos[1], lightPos[2] - 1.0f};
    vec3 up6 = {0.0f, -1.0f, 0.0f};
    glm_lookat(lightPos, center6, up6, view6);
    glm_mat4_mul(shadowProj, view6, shadowMatrices[5]);
    
    // Render scene to depth cubemap
    glUseProgram(shadowMapShader);
    
    // Set model matrix
    mat4 model;
    glm_mat4_identity(model);
    unsigned int modelLoc = glGetUniformLocation(shadowMapShader, "model");
    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, &model[0][0]);
    
    // Set shadow matrices
    for (unsigned int i = 0; i < 6; ++i) {
        char name[32];
        sprintf(name, "shadowMatrices[%d]", i);
        unsigned int shadowMatricesLoc = glGetUniformLocation(shadowMapShader, name);
        glUniformMatrix4fv(shadowMatricesLoc, 1, GL_FALSE, &shadowMatrices[i][0][0]);
    }
    
    // Set light position and far plane
    unsigned int lightPosLoc = glGetUniformLocation(shadowMapShader, "lightPos");
    glUniform3fv(lightPosLoc, 1, lightPos);
    unsigned int farPlaneLoc = glGetUniformLocation(shadowMapShader, "far_plane");
    glUniform1f(farPlaneLoc, far_plane);
    
    // Render scene
    glBindVertexArray(VAO);
    glDrawElements(GL_TRIANGLES, cornellBox.indexOffset, GL_UNSIGNED_INT, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    
    // 2. Render scene as normal with shadow mapping
    // ------------------------------
    glViewport(0, 0, SCR_WIDTH, SCR_HEIGHT);
    glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    // Create camera view and projection matrices
    mat4 view, projection;
    glm_mat4_identity(view);
    glm_mat4_identity(projection);
    
    // Camera parameters
    vec3 eye = {3.0f, 3.0f, -8.0f};
    vec3 up = {0.0f, 1.0f, 0.0f};
    vec3 dir = {0.0f, 0.0f, 1.0f};
    
    // Calculate view and projection matrices
    glm_look(eye, dir, up, view);
    glm_perspective(glm_rad(45.0f), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 100.0f, projection);
    
    // Use the Blinn-Phong shader
    glUseProgram(renderShader);
    
    // Set uniforms for rendering
    modelLoc = glGetUniformLocation(renderShader, "model");
    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, &model[0][0]);
    
    unsigned int viewLoc = glGetUniformLocation(renderShader, "view");
    glUniformMatrix4fv(viewLoc, 1, GL_FALSE, &view[0][0]);
    
    unsigned int projectionLoc = glGetUniformLocation(renderShader, "projection");
    glUniformMatrix4fv(projectionLoc, 1, GL_FALSE, &projection[0][0]);
    
    unsigned int viewPosLoc = glGetUniformLocation(renderShader, "viewPos");
    glUniform3fv(viewPosLoc, 1, eye);
    
    lightPosLoc = glGetUniformLocation(renderShader, "lightPos");
    glUniform3fv(lightPosLoc, 1, lightPos);
    
    unsigned int lightColorLoc = glGetUniformLocation(renderShader, "lightColor");
    vec3 adjustedLightColor = {
        lightColor[0] * lightIntensity,
        lightColor[1] * lightIntensity,
        lightColor[2] * lightIntensity
    };
    glUniform3fv(lightColorLoc, 1, adjustedLightColor);
    
    farPlaneLoc = glGetUniformLocation(renderShader, "far_plane");
    glUniform1f(farPlaneLoc, far_plane);

    unsigned int shadowBiasLoc = glGetUniformLocation(renderShader, "shadowBias");
    glUniform1f(shadowBiasLoc, shadowBias);
    
    // Set shadow cubemap texture
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, depthCubeMap);
    unsigned int shadowMapLoc = glGetUniformLocation(renderShader, "shadowMap");
    glUniform1i(shadowMapLoc, 0);

    
    
    // Render the scene
    glBindVertexArray(VAO);
    glDrawElements(GL_TRIANGLES, cornellBox.indexOffset, GL_UNSIGNED_INT, 0);
    
    // Render ImGui
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    
    // Swap buffers
    glfwSwapBuffers(window);
  }

  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();
  
  // Delete OpenGL objects
  glDeleteVertexArrays(1, &VAO);
  glDeleteBuffers(1, &positions);
  glDeleteBuffers(1, &albedo);
  glDeleteBuffers(1, &normals);
  glDeleteBuffers(1, &EBO);
  glDeleteFramebuffers(1, &depthMapFBO);
  glDeleteTextures(1, &depthCubeMap);
  glDeleteProgram(shadowMapShader);
  glDeleteProgram(renderShader);
  
  // Free memory
  glfwTerminate();
  free(cornellBox.albedo);
  free(cornellBox.normals);
  free(cornellBox.vertices);
  free(cornellBox.indices);
  
  return 0;
}