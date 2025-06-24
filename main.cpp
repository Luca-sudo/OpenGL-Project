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
#include <iostream>
#include <filesystem>

#include <GLFW/glfw3.h>
#include <string.h>

#include <imgui/imgui.h>
#include <imgui/backends/imgui_impl_glfw.h>
#include <imgui/backends/imgui_impl_opengl3.h>

#define STB_IMAGE_IMPLEMENTATION
#include "include/stb_image.h"

#define KB(x) (x * 1024)

struct ShaderDeclaration {
  const char *name, *vertPath, *fragPath;
  unsigned int program = 0;
};

typedef struct {
  aiVector3D *vertices;
  aiColor4D *albedo;
  aiVector3D *normals;
  aiVector2D *uvs;
  aiVector3D *tangents;
  aiVector3D *bitangents;
  unsigned int *indices;
  std::string normalMapPath;
  std::string diffuseMapPath;

  unsigned int vertexOffset;
  unsigned indexOffset;
} model_t;

void check_shader_compiling(unsigned int shader) {
    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        GLint logLength;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLength);
        char log[logLength];
        glGetShaderInfoLog(shader, logLength, nullptr, log);
        throw std::runtime_error("Shader compilation failed!\n" + std::string(log));
    }
}

void check_shader_linking(unsigned int shader) {
    GLint success;
    glGetShaderiv(shader, GL_LINK_STATUS, &success);
    if (!success) {
        GLint logLength;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLength);
        char log[logLength];
        glGetShaderInfoLog(shader, logLength, nullptr, log);
        throw std::runtime_error("Shader linking failed!\n" + std::string(log));
    }

}

void framebuffer_size_callback(GLFWwindow *window, int width, int height) {
  glViewport(0, 0, width, height);
}

//TODO: Coalesce into one allocation.
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

  model->uvs = (aiVector2D*)malloc(KB(10));
  if (model->uvs == NULL) {
    printf("Failed to allocate buffer for normals\n");
    glfwTerminate();
    return -1;
  }

  model->tangents = (aiVector3D*)malloc(KB(10));
  if (model->tangents == NULL) {
    printf("Failed to allocate buffer for tangents\n");
    glfwTerminate();
    return -1;
  }

  model->bitangents = (aiVector3D*)malloc(KB(10));
  if (model->bitangents == NULL) {
    printf("Failed to allocate buffer for tangents\n");
    glfwTerminate();
    return -1;
  }

  return 0;
}


void extract_textures(model_t *model, const struct aiScene *scene){
  for(int i = 0; i < scene->mNumMaterials; i++){
    aiMaterial* mat = scene->mMaterials[i];

    unsigned int normalMapCount = mat->GetTextureCount(aiTextureType_NORMALS);

    for(unsigned int j = 0; j < normalMapCount; j++){
      aiString path;
      mat->GetTexture(aiTextureType_NORMALS, 0, &path);
      model->normalMapPath = std::string("./") + std::string(path.C_Str());
    }

    unsigned int textureMapCount = mat->GetTextureCount(aiTextureType_DIFFUSE);

    for(unsigned int j = 0; j < textureMapCount; j++){
      aiString path;
      mat->GetTexture(aiTextureType_DIFFUSE, 0, &path);
      model->diffuseMapPath = std::string("./") + std::string(path.C_Str());
    }
  }
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

    aiString str;
    aiGetMaterialTexture(scene->mMaterials[materialId], aiTextureType_NORMALS, 0, &str);


    for (int vertexIdx = 0; vertexIdx < scene->mMeshes[meshId]->mNumVertices;
         vertexIdx++) {
      model->vertices[model->vertexOffset + vertexIdx] =
          scene->mMeshes[meshId]->mVertices[vertexIdx];
      model->albedo[model->vertexOffset + vertexIdx] = albedo;
      model->normals[model->vertexOffset + vertexIdx] = scene->mMeshes[meshId]->mNormals[vertexIdx];
      // Does the mesh contain vertex coordinates?
      if(scene->mMeshes[meshId]->mTextureCoords[0]){
        aiVector2D uv;
        uv.x = scene->mMeshes[meshId]->mTextureCoords[0][vertexIdx].x;
        uv.y = scene->mMeshes[meshId]->mTextureCoords[0][vertexIdx].y;
        model->uvs[model->vertexOffset + vertexIdx] = uv;
        model->tangents[model->vertexOffset + vertexIdx] = scene->mMeshes[meshId]->mTangents[vertexIdx];
        model->bitangents[model->vertexOffset + vertexIdx] = scene->mMeshes[meshId]->mBitangents[vertexIdx];
      }else {
        model->uvs[model->vertexOffset + vertexIdx] = (aiVector2D){-1.0f, -1.0f};
        model->tangents[model->vertexOffset + vertexIdx] = (aiVector3D){0.0f, 0.0f, 0.0f};
      }
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
      "assets/cornell_box_v2.gltf", aiProcessPreset_TargetRealtime_MaxQuality | aiProcess_CalcTangentSpace);

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
  extract_textures(&cornellBox, scene);


  unsigned int textures[2] = {0};
  glGenTextures(2, textures);
  unsigned int& normalMap = textures[0];
  unsigned int& diffuseMap = textures[1];


  // Define and instantiate all shaders.
  int selected_shader = 0;
  ShaderDeclaration SHADERS[] = {
    {"Flat", "shaders/flat.vert", "shaders/flat.frag"},
    {"Lambertian", "shaders/lambertian.vert", "shaders/lambertian.frag"},
    {"Phong", "shaders/phong.vert", "shaders/phong.frag"},
    {"Blinn-Phong", "shaders/blinn_phong.vert", "shaders/blinn_phong.frag"},
    {"Spotlight", "shaders/blinn_phong.vert", "shaders/spotlight.frag"},
    {"Texture", "shaders/texture.vert", "shaders/texture.frag"},
  };

  auto NUM_SHADERS = sizeof(SHADERS) / sizeof(SHADERS[0]);
  const char* SHADER_NAMES[NUM_SHADERS];
  std::cout << "Nr. of shaders: " << NUM_SHADERS << std::endl;

  for(int i = 0; i < NUM_SHADERS; i++){
    unsigned int vertexShader, fragShader, shaderProgram;
    vertexShader = glCreateShader(GL_VERTEX_SHADER);
    char *vertShaderCode = read_shader_from_file(SHADERS[i].vertPath);
    glShaderSource(vertexShader, 1, (const char *const *)&vertShaderCode, NULL);
    glCompileShader(vertexShader);

    check_shader_compiling(vertexShader);

    fragShader = glCreateShader(GL_FRAGMENT_SHADER);
    char *fragShaderCode = read_shader_from_file(SHADERS[i].fragPath);
    glShaderSource(fragShader, 1, (const char *const *)&fragShaderCode, NULL);
    glCompileShader(fragShader);

    check_shader_compiling(fragShader);

    shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragShader);
    glLinkProgram(shaderProgram);

    check_shader_linking(shaderProgram);

    glDeleteShader(vertexShader);
    glDeleteShader(fragShader);

    SHADERS[i].program = shaderProgram;
    SHADER_NAMES[i] = SHADERS[i].name;
  }

  unsigned int VAO;
  unsigned int vertexBuffers[7] = {0};

  glGenVertexArrays(1, &VAO);
  glGenBuffers(7, vertexBuffers);

  unsigned int &EBO = vertexBuffers[0];
  unsigned int &positions = vertexBuffers[1];
  unsigned int &albedo = vertexBuffers[2];
  unsigned int &normals = vertexBuffers[3];
  unsigned int &uvs = vertexBuffers[4];
  unsigned int &tangents = vertexBuffers[5];
  unsigned int &bitangents = vertexBuffers[6];

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
  glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(aiColor4D),
                        (void *)0);
  glEnableVertexAttribArray(1);

  glBindBuffer(GL_ARRAY_BUFFER, normals);
  glBufferData(GL_ARRAY_BUFFER, sizeof(aiVector3D) * cornellBox.vertexOffset,
              cornellBox.normals, GL_STATIC_DRAW);
  glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(aiVector3D), (void *)0);
  glEnableVertexAttribArray(2);

  glBindBuffer(GL_ARRAY_BUFFER, uvs);
  glBufferData(GL_ARRAY_BUFFER, sizeof(aiVector2D) * cornellBox.vertexOffset,
              cornellBox.uvs, GL_STATIC_DRAW);
  glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, sizeof(aiVector2D), (void *)0);
  glEnableVertexAttribArray(3);

  glBindBuffer(GL_ARRAY_BUFFER, tangents);
  glBufferData(GL_ARRAY_BUFFER, sizeof(aiVector3D) * cornellBox.vertexOffset,
               cornellBox.tangents, GL_STATIC_DRAW);
  glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, sizeof(aiVector3D), (void *)0);
  glEnableVertexAttribArray(4);

  glBindBuffer(GL_ARRAY_BUFFER, bitangents);
  glBufferData(GL_ARRAY_BUFFER, sizeof(aiVector3D) * cornellBox.vertexOffset,
               cornellBox.bitangents, GL_STATIC_DRAW);
  glVertexAttribPointer(5, 3, GL_FLOAT, GL_FALSE, sizeof(aiVector3D), (void *)0);
  glEnableVertexAttribArray(5);

  // Configuring the sampler, loading & configuring texture.
  glBindTexture(GL_TEXTURE_2D, diffuseMap);

  int width, height, nrComponents;
  unsigned char *textureData = stbi_load(cornellBox.diffuseMapPath.c_str(),&width, &height, &nrComponents, 0);
  if(textureData == NULL){
    std::cout << "Current working dir: " << std::filesystem::current_path() << std::endl;
    std::cout << stbi_failure_reason() << std::endl;
    throw std::runtime_error("Failed to load diffuse map.");
  }
  else{
    std::cout << "Found texture, now generate in GL." << std::endl;
    GLenum format;
    if(nrComponents == 3)
      format = GL_RGB;
    else if(nrComponents == 4)
      format = GL_RGBA;
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, format, GL_UNSIGNED_BYTE, textureData);
    glGenerateMipmap(GL_TEXTURE_2D);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  }
  free(textureData);

  glBindTexture(GL_TEXTURE_2D, normalMap);

  textureData = stbi_load(cornellBox.normalMapPath.c_str(), &width, &height, &nrComponents, 0);
  if(textureData == NULL){
    std::cout << "Current working dir: " << std::filesystem::current_path() << std::endl;
    std::cout << stbi_failure_reason() << std::endl;
    throw std::runtime_error("Failed to load normal map.");
  }
  else{
    std::cout << "Found texture, now generate in GL." << std::endl;
    GLenum format;
    if(nrComponents == 3)
      format = GL_RGB;
    else if(nrComponents == 4)
      format = GL_RGBA;
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, format, GL_UNSIGNED_BYTE, textureData);
    glGenerateMipmap(GL_TEXTURE_2D);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  }

  free(textureData);


  // Setup Dear ImGui context
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO &io = ImGui::GetIO();
  // Setup Platform/Renderer bindings
  ImGui_ImplGlfw_InitForOpenGL(window, true);
  ImGui_ImplOpenGL3_Init("#version 330");

  // Setup Dear ImGui style
  ImGui::StyleColorsDark();

  vec3 lightPos = {2.78f, 5.48f, 2.796f};

  while (!glfwWindowShouldClose(window)) {

    glfwPollEvents();


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

    vec3 lightColor = {1.0f, 1.0f, 1.0f};
    vec3 lightDir;
    glm_vec3_negate_to(up, lightDir);                   // lightDirection = down
    float lightCutoffAngle = cos(0.2618f);              // 15 degrees => around 0.2618 radians
    float lightOuterCutoffAngle = cos(1.04f);           // 60 degrees
    // model = rotate(model, radians(-55.0f), vec3(1.0, 0.0, 0.0));
    // glm_rotate(model, glm_rad(-25.0f), (vec3){0.0f, 1.0f, 0.0f});

    // view = translate(view, vec3(0.0, 0.0, -3.0));
    glm_look(eye, dir, up, view);

    // projection = perspective(radians(45.0f), aspect, near, far)
    glm_perspective(glm_rad(45.0f), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f,
                    100.0f, projection);

    unsigned int &shaderProgram = SHADERS[selected_shader].program;

    glUseProgram(shaderProgram);

    // set uniforms
    unsigned int modelLoc = glGetUniformLocation(shaderProgram, "model");
    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, &model[0][0]);
    unsigned int viewLoc = glGetUniformLocation(shaderProgram, "view");
    glUniformMatrix4fv(viewLoc, 1, GL_FALSE, &view[0][0]);
    unsigned int projectionLoc =
        glGetUniformLocation(shaderProgram, "projection");
    glUniformMatrix4fv(projectionLoc, 1, GL_FALSE, &projection[0][0]);
    unsigned int viewPosLoc = glGetUniformLocation(shaderProgram, "viewPos");
    glUniform3fv(viewPosLoc, 1, eye);
    unsigned int lightPosLoc = glGetUniformLocation(shaderProgram, "lightPos");
    glUniform3fv(lightPosLoc, 1, lightPos);
    unsigned int lightColorLoc = glGetUniformLocation(shaderProgram, "lightColor");
    glUniform3fv(lightColorLoc, 1, lightColor);
    unsigned int lightDirLoc = glGetUniformLocation(shaderProgram, "lightDir");
    glUniform3fv(lightDirLoc, 1, lightDir);
    unsigned int lightCutoffAngleLoc = glGetUniformLocation(shaderProgram, "lightCutoffAngle");
    glUniform1f(lightCutoffAngleLoc, lightCutoffAngle);
    unsigned int lightOuterCutoffAngleLoc = glGetUniformLocation(shaderProgram, "lightOuterCutoffAngle");
    glUniform1f(lightOuterCutoffAngleLoc, lightOuterCutoffAngle);

    glBindVertexArray(VAO);

    // Due to GLSL version 330, have to set uniform bind slots here.
    glUniform1i(glGetUniformLocation(shaderProgram, "diffuseMap"), 0);
    glUniform1i(glGetUniformLocation(shaderProgram, "normalMap"), 1);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, diffuseMap);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, normalMap);

    glDrawElements(GL_TRIANGLES, cornellBox.indexOffset, GL_UNSIGNED_INT, 0);

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    ImGui::Begin("Demo window");
    ImGui::Combo("Select a shader!", &selected_shader, SHADER_NAMES, IM_ARRAYSIZE(SHADER_NAMES));
    ImGui::SliderFloat("Light Position", &lightPos[1], 0.0f, 10.0f);
    ImGui::End();

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    glfwSwapBuffers(window);
  }

  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();

  glfwTerminate();
  free(cornellBox.albedo);
  free(cornellBox.normals);
  free(cornellBox.vertices);
  free(cornellBox.indices);
  free(cornellBox.uvs);

  return 0;
}
