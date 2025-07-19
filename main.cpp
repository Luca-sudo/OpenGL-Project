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

typedef struct {
  vec3 point;    // A point on the plane
  vec3 normal;   // Plane normal (should point towards the viewer)
} reflection_plane_t;

/**
 * Checks if the shader compiled properly. Throws an error and terminates the program otherwise.
 * Helpful because Open-GL does not report these errors.
 */
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

/**
 * Checks if the shader linked properly. Throws an error and terminates the program otherwise.
 */
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

/**
 * Allows resizing of the window. Otherwise the window dimension would not match that of the framebuffer.
 */
void framebuffer_size_callback(GLFWwindow *window, int width, int height) {
  glViewport(0, 0, width, height);
}

//TODO: This currently allocates 70 KB flat. Of course, this may be way too much.
// Instead, a precise amount of memory should be allocated, but this requires aggregating information about the model.
int allocate_model(model_t *model) {
  // Allocate one chunk of memory
  char* data = (char *)malloc(KB(70));
  if(data == NULL){
    printf("Failed to allocate memory for Scene.\n");
    glfwTerminate();
    return -1;
  }


  // Index into the chunk at 10 KB offsets each.
  // Therefore, each field gets 10 KB of memory.
  model->indices = (unsigned int*) data;
  model->vertices = (aiVector3D*) (data + KB(10));
  model->albedo = (aiColor4D*) (data + KB(20));
  model->normals = (aiVector3D*) (data + KB(30));
  model->uvs = (aiVector2D*) (data + KB(40));
  model->tangents = (aiVector3D*) (data + KB(50));
  model->bitangents = (aiVector3D*) (data + KB(60));

  return 0;
}


/*
** Fetch normal and diffuse maps from the model.
 */
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

/**
 * Loads all vertex data & indices from the .gltf file.
 */
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

void reflect_point_across_plane(vec3 result, vec3 point, vec3 plane_point, vec3 plane_normal) {
  vec3 n;
  glm_vec3_normalize_to(plane_normal, n);

  vec3 to_point;
  glm_vec3_sub(point, plane_point, to_point);

  float distance = glm_vec3_dot(to_point, n);

  vec3 reflection_offset;
  glm_vec3_scale(n, 2.0f * distance, reflection_offset);
  glm_vec3_sub(point, reflection_offset, result);
}

void reflect_direction_across_plane(vec3 dest, vec3 v, vec3 plane_normal) {
    vec3 temp;
    float dot = glm_vec3_dot(v, plane_normal);
    glm_vec3_scale(plane_normal, 2.0f * dot, temp);
    glm_vec3_sub(v, temp, dest);
}

void create_reflective_surface_stencil(unsigned int* VAO_stencil, unsigned int* VBO_stencil) {
  float center_point[] = {3.440f, 1.650f, 2.714f};
  float scale = 1.0f;

  float original_vertices[] = {
    2.650f, 3.300f, 2.959f,
    4.230f, 3.300f, 2.469f,
    4.230f, 0.000f, 2.469f,
    2.650f, 0.000f, 2.959f
  };

  float stencil_vertices[12];
  for (int i = 0; i < 4; ++i) {
    int j = i * 3;
    stencil_vertices[j + 0] = center_point[0] + scale * (original_vertices[j + 0] - center_point[0]);
    stencil_vertices[j + 1] = center_point[1] + scale * (original_vertices[j + 1] - center_point[1]);
    stencil_vertices[j + 2] = center_point[2] + scale * (original_vertices[j + 2] - center_point[2]);
  }

  unsigned int stencil_indices[] = {
    0, 1, 2,
    2, 3, 0
  };

  glGenVertexArrays(1, VAO_stencil);
  glGenBuffers(1, VBO_stencil);
  unsigned int EBO_stencil;
  glGenBuffers(1, &EBO_stencil);

  glBindVertexArray(*VAO_stencil);

  glBindBuffer(GL_ARRAY_BUFFER, *VBO_stencil);
  glBufferData(GL_ARRAY_BUFFER, sizeof(stencil_vertices), stencil_vertices, GL_STATIC_DRAW);

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO_stencil);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(stencil_indices), stencil_indices, GL_STATIC_DRAW);

  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
  glEnableVertexAttribArray(0);
}

const unsigned int SCR_WIDTH = 1200;
const unsigned int SCR_HEIGHT = 900;

int main() {

  /////////////////////////
  // GLFW Initialization //
  /////////////////////////

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
  glEnable(GL_STENCIL_TEST);

  glViewport(0, 0, 1200, 900);

  glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

  ///////////////////
  // Scene loading //
  ///////////////////

  const C_STRUCT aiScene *scene = aiImportFile(
      "assets/cornell_box_v2.gltf", aiProcessPreset_TargetRealtime_MaxQuality | aiProcess_CalcTangentSpace);

  if (scene == NULL) {
    printf("Failed to load scene\n");
    glfwTerminate();
    return -1;
  }

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


  reflection_plane_t mirror_plane = {
    {3.440f, 1.650f, 2.714f},
    {-0.296f, 0.000f, -0.955f}
  };

  ////////////////////
  // Shader loading //
  ////////////////////

  int selected_shader = 5;
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

  ///////////////////////////////////////
  // VAO & VBO Creation and Population //
  ///////////////////////////////////////

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

  ////////////////////////////
  // Setup diffuse texture  //
  ////////////////////////////

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

  //////////////////////////
  // Setup normal texture //
  //////////////////////////

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

  unsigned int VAO_stencil, VBO_stencil;
  create_reflective_surface_stencil(&VAO_stencil, &VBO_stencil);

  // Setup Dear ImGui context
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO &io = ImGui::GetIO();
  ImGui_ImplGlfw_InitForOpenGL(window, true);
  ImGui_ImplOpenGL3_Init("#version 330");
  ImGui::StyleColorsDark();

  vec3 lightPos = {2.78f, 5.48f, 2.796f};

  bool enable_reflection = 0;

  while (!glfwWindowShouldClose(window)) {
    glfwPollEvents();

    struct timespec time;
    clock_gettime(CLOCK_MONOTONIC, &time);
    double currentTime = time.tv_sec + time.tv_nsec / 1000000000.0f;

    glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    // Create transformations
    mat4 model, view, projection;
    glm_mat4_identity(model);
    glm_mat4_identity(view);
    glm_mat4_identity(projection);

    vec3 eye = {3.0f, 3.0f, -8.0f};
    vec3 up = {0.0f, 1.0f, 0.0f};
    vec3 dir = {0.0f, 0.0f, 1.0f};

    vec3 lightColor = {1.0f, 1.0f, 1.0f};
    vec3 lightDir;

    // Used for spotlight
    glm_vec3_negate_to(up, lightDir);                   // lightDirection = down
    float lightCutoffAngle = cos(0.2618f);              // 15 degrees => around 0.2618 radians
    float lightOuterCutoffAngle = cos(1.04f);           // 60 degrees

    glm_look(eye, dir, up, view);
    glm_perspective(glm_rad(45.0f), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f,
                    100.0f, projection);

    unsigned int &shaderProgram = SHADERS[selected_shader].program;

    // First pass: Draw scene normally
    glUseProgram(shaderProgram);

    unsigned int modelLoc = glGetUniformLocation(shaderProgram, "model");
    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, &model[0][0]);
    unsigned int viewLoc = glGetUniformLocation(shaderProgram, "view");
    glUniformMatrix4fv(viewLoc, 1, GL_FALSE, &view[0][0]);
    unsigned int projectionLoc = glGetUniformLocation(shaderProgram, "projection");
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
    
    // Disable clipping for normal scene rendering
    unsigned int useClippingLoc = glGetUniformLocation(shaderProgram, "useClipping");
    glUniform1i(useClippingLoc, 0);

    glBindVertexArray(VAO);

    // Due to GLSL version 330, have to set uniform bind slots here.
    glUniform1i(glGetUniformLocation(shaderProgram, "diffuseMap"), 0);
    glUniform1i(glGetUniformLocation(shaderProgram, "normalMap"), 1);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, diffuseMap);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, normalMap);

    glDrawElements(GL_TRIANGLES, cornellBox.indexOffset, GL_UNSIGNED_INT, 0);



    /////////////////////////////////
    // Reflection Pass, if enabled //
    /////////////////////////////////

    if (enable_reflection) {
      // Step 1: Mark stencil buffer where mirror surface is visible
      glStencilFunc(GL_ALWAYS, 1, 0xFF);      
      glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE); 
      glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE); 

      // Draw mirror quad (writes to stencil only)
      glBindVertexArray(VAO_stencil);
      glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);   

      // Restore write masks
      glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);   

      // Compute reflected view and light
      vec3 reflected_eye, reflected_dir, reflected_up;
      mat4 reflected_view;

      reflect_point_across_plane(reflected_eye, eye, mirror_plane.point, mirror_plane.normal);
      reflect_direction_across_plane(reflected_dir, dir, mirror_plane.normal);
      reflect_direction_across_plane(reflected_up, up, mirror_plane.normal);

      glm_look(reflected_eye, reflected_dir, reflected_up, reflected_view);

      mat4 flip_matrix;
      glm_mat4_identity(flip_matrix);
      flip_matrix[0][0] = -1.0f;
      glm_mat4_mul(flip_matrix, reflected_view, reflected_view);

      vec3 reflected_light_pos;
      reflect_point_across_plane(reflected_light_pos, lightPos, mirror_plane.point, mirror_plane.normal);

      // Set up clip plane
      glEnable(GL_CLIP_DISTANCE0);
      glUniform1i(useClippingLoc, 1);

      vec4 world_clip_plane;
      world_clip_plane[0] = mirror_plane.normal[0];  
      world_clip_plane[1] = mirror_plane.normal[1];  
      world_clip_plane[2] = mirror_plane.normal[2];  
      world_clip_plane[3] = -(mirror_plane.normal[0] * mirror_plane.point[0] + 
                              mirror_plane.normal[1] * mirror_plane.point[1] + 
                              mirror_plane.normal[2] * mirror_plane.point[2]);

      unsigned int world_clip_plane_loc = glGetUniformLocation(shaderProgram, "clipPlane");
      glUniform4fv(world_clip_plane_loc, 1, world_clip_plane);

      // Set reflection uniforms
      glUniformMatrix4fv(viewLoc, 1, GL_FALSE, &reflected_view[0][0]);
      glUniform3fv(viewPosLoc, 1, reflected_eye);
      glUniform3fv(lightPosLoc, 1, reflected_light_pos);

      // Adjust face culling for mirrored geometry
      glFrontFace(GL_CW);
      glEnable(GL_CULL_FACE);

      // Draw reflected scene inside stencil
      glClear(GL_DEPTH_BUFFER_BIT);
      glStencilFunc(GL_EQUAL, 1, 0xFF);
      glStencilMask(0x00);

      glBindVertexArray(VAO);
      glDrawElements(GL_TRIANGLES, cornellBox.indexOffset, GL_UNSIGNED_INT, 0);

      // Restore OpenGL state
      glDisable(GL_CLIP_DISTANCE0);
      glDisable(GL_CULL_FACE);
      glFrontFace(GL_CCW);
      glStencilMask(0xFF);
      glStencilFunc(GL_ALWAYS, 0, 0xFF);
      glUniformMatrix4fv(viewLoc, 1, GL_FALSE, &view[0][0]);
      glUniform3fv(viewPosLoc, 1, eye);
      glUniform3fv(lightPosLoc, 1, lightPos);
      glUniform1i(useClippingLoc, 0);
    }

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    ImGui::Begin("Demo window");
    ImGui::Combo("Select a shader!", &selected_shader, SHADER_NAMES, IM_ARRAYSIZE(SHADER_NAMES));
    ImGui::SliderFloat("Light Position", &lightPos[1], 0.0f, 10.0f);
    ImGui::Checkbox("Reflection", &enable_reflection);
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
