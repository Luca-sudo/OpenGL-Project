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

/**
 * Frees the allocated storage for the model.
 * Since one chunk is allocated, starting at model->indices, it suffices to free that.
 */
void free_model(model_t &model) {
  free(model.indices);
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
      // The texture path has to be prefixed with "./",
      // because the path is relative. Otherwise throws an error.
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
  // Iterate over every mesh in the model.
  // What constitutes a mesh depends on how the model was constructed in Blender, for example.
  for (int meshIndex = 0; meshIndex < node->mNumMeshes; meshIndex++) {
    unsigned int meshId = node->mMeshes[meshIndex];
    unsigned int materialId = scene->mMeshes[meshId]->mMaterialIndex;
    // Faces are the triangles of our mesh.
    for (int faceIdx = 0; faceIdx < scene->mMeshes[meshId]->mNumFaces;
         faceIdx++) {
      struct aiFace face = scene->mMeshes[meshId]->mFaces[faceIdx];
      // Store only the associated index for now.
      // A later pass will extract the vertex data.
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


    // This pass extracts the concrete vertex data.
    // This comprises: position, color, normals, uvs, tangents, bitangents.
    for (int vertexIdx = 0; vertexIdx < scene->mMeshes[meshId]->mNumVertices;
         vertexIdx++) {
      model->vertices[model->vertexOffset + vertexIdx] =
          scene->mMeshes[meshId]->mVertices[vertexIdx];
      model->albedo[model->vertexOffset + vertexIdx] = albedo;
      model->normals[model->vertexOffset + vertexIdx] = scene->mMeshes[meshId]->mNormals[vertexIdx];
      // Does the mesh contain vertex coordinates? I.e. is this mesh textured at all?
      if(scene->mMeshes[meshId]->mTextureCoords[0]){
        aiVector2D uv;
        uv.x = scene->mMeshes[meshId]->mTextureCoords[0][vertexIdx].x;
        uv.y = scene->mMeshes[meshId]->mTextureCoords[0][vertexIdx].y;
        model->uvs[model->vertexOffset + vertexIdx] = uv;
        model->tangents[model->vertexOffset + vertexIdx] = scene->mMeshes[meshId]->mTangents[vertexIdx];
        model->bitangents[model->vertexOffset + vertexIdx] = scene->mMeshes[meshId]->mBitangents[vertexIdx];
      }else {
        // Since we don't separate rendering of un-textured and textured meshes,
        // we have to provide a designated uv value that our shader can detect.
        model->uvs[model->vertexOffset + vertexIdx] = (aiVector2D){-1.0f, -1.0f};
        model->tangents[model->vertexOffset + vertexIdx] = (aiVector3D){0.0f, 0.0f, 0.0f};
      }
    }

    model->vertexOffset += scene->mMeshes[meshId]->mNumVertices;
  }

  // Because assimp's aiScene has a graph structure,
  // there may be child nodes that describe missing parts of the scene.
  // Therefore, also recurse into these and extract.
  for (int childIdx = 0; childIdx < node->mNumChildren; childIdx++) {
    extract_indices(model, node->mChildren[childIdx], scene);
  }
}

//////////////////////////
// Reflection Functions //
//////////////////////////

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
  // If a shader should be added to the UI dropdown, then add an entry here.
  // Specify (display name, .vert path, .frag path)
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

  // This generates the shader for all the ones defined in SHADERS.
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

  ////////////////////////////
  // Shadow mapping shaders //
  ////////////////////////////
  
  unsigned int shadowMapShader;
  unsigned int vertexShader, geomShader, fragShader;
  
  // SHADOW MAPPING: Shadow depth vertex shader
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
  
  // SHADOW MAPPING: Shadow depth geometry shader
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
  
  // SHADOW MAPPING: Shadow depth fragment shader
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
  
  // SHADOW MAPPING: Link shadow mapping shader program
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


  ///////////////////////////////////////
  // VAO & VBO Creation and Population //
  ///////////////////////////////////////

  unsigned int VAO;
  unsigned int vertexBuffers[7] = {0};

  glGenVertexArrays(1, &VAO);

  // Generate 7 VBOs and create references to them.
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

    // Setup sampling. I.e. wrapping and filtering.
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

  ////////////////////////////////////////
  // Shadow mapping Cube map generation //
  ////////////////////////////////////////

  // SHADOW MAPPING: Set up depth cubemap for point shadows
  const unsigned int SHADOW_WIDTH = 1024, SHADOW_HEIGHT = 1024;
  float aspect = (float)SHADOW_WIDTH/(float)SHADOW_HEIGHT;
  float near_plane = 0.1f;
  float far_plane = 25.0f;
  
  // SHADOW MAPPING: Create depth cubemap framebuffer object
  unsigned int depthMapFBO;
  glGenFramebuffers(1, &depthMapFBO);
  
  // SHADOW MAPPING: Create depth cubemap texture
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
  
  // SHADOW MAPPING: Attach depth texture as FBO's depth buffer
  glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
  glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, depthCubeMap, 0);
  glDrawBuffer(GL_NONE);
  glReadBuffer(GL_NONE);
  
  if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
    printf("Framebuffer not complete!\n");
  }
  
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  
  // SHADOW MAPPING: Setup shadow transform matrices for point light
  mat4 shadowProj;
  glm_perspective(glm_rad(90.0f), aspect, near_plane, far_plane, shadowProj);

  // SHADOW MAPPING: Add a bias
  float shadowBias = 0.25f;


  // Setup Dear ImGui context
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO &io = ImGui::GetIO();
  ImGui_ImplGlfw_InitForOpenGL(window, true);
  ImGui_ImplOpenGL3_Init("#version 330");
  ImGui::StyleColorsDark();

  vec3 lightPos = {2.78f, 5.00f, 2.796f};

  bool enable_reflection = 0;
  bool enable_shadows = 1;

  float xPos = 3.0f;
  float yPos = 3.0f;
  float zPos = -8.0f;

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

    vec3 eye = {xPos, yPos, zPos};
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


    // SHADOW MAPPING: Conditional shadow pass - only render to shadow map if shadows are enabled
    if (enable_shadows) {
      // render to depth cubemap
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

      glBindVertexArray(VAO);
      glDrawElements(GL_TRIANGLES, cornellBox.indexOffset, GL_UNSIGNED_INT, 0);
      glBindFramebuffer(GL_FRAMEBUFFER, 0);
      // Reset viewport to screen dimensions
      glViewport(0, 0, SCR_WIDTH, SCR_HEIGHT);
    }


    /////////////////////////////////////
    // First pass: Draw scene normally //
    /////////////////////////////////////

    glUseProgram(shaderProgram);

    // Set Uniforms
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

    // SHADOW MAPPING: Set shadow-related uniforms conditionally
    if (enable_shadows) {
      unsigned int farPlaneLoc = glGetUniformLocation(shaderProgram, "far_plane");
      glUniform1f(farPlaneLoc, far_plane);
      unsigned int shadowBiasLoc = glGetUniformLocation(shaderProgram, "shadowBias");
      glUniform1f(shadowBiasLoc, shadowBias);
      unsigned int shadowMapLoc = glGetUniformLocation(shaderProgram, "shadowMap");
      glUniform1i(shadowMapLoc, 0);
      // SHADOW MAPPING: Enable shadows flag in shader
      unsigned int enable_shadowsLoc = glGetUniformLocation(shaderProgram, "enable_shadows");
      glUniform1i(enable_shadowsLoc, 1);
    } else {
      // SHADOW MAPPING: Disable shadows flag in shader
      unsigned int enable_shadowsLoc = glGetUniformLocation(shaderProgram, "enable_shadows");
      glUniform1i(enable_shadowsLoc, 0);
    }
    
    // Disable clipping for normal scene rendering
    unsigned int useClippingLoc = glGetUniformLocation(shaderProgram, "useClipping");
    glUniform1i(useClippingLoc, 0);

    glBindVertexArray(VAO);

    // Due to GLSL version 330, have to set uniform bind slots here.
    glUniform1i(glGetUniformLocation(shaderProgram, "diffuseMap"), 0);
    glUniform1i(glGetUniformLocation(shaderProgram, "normalMap"), 1);
    if (enable_shadows) {
      glUniform1i(glGetUniformLocation(shaderProgram, "shadowMap"), 2);
    }

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, diffuseMap);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, normalMap);

    // set shadow cubemap texture
    if (enable_shadows) {
      glActiveTexture(GL_TEXTURE2);
      glBindTexture(GL_TEXTURE_CUBE_MAP, depthCubeMap);
    }

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
      

      // VERSION 1. FLIP
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

      // Clipping plane must be 4d vector [a, b, c, d] such that (ax + by + cz + d = 0)
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
      // glUniform3fv(lightPosLoc, 1, reflected_light_pos);

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
    ImGui::SliderFloat("X Position", &xPos, 0.0f, 5.0f);
    ImGui::SliderFloat("Y Position", &yPos, 0.0f, 5.0f);
    ImGui::SliderFloat("Z Position", &zPos, -8.0f, 5.0f);
    ImGui::Checkbox("Enable Reflection", &enable_reflection);
    ImGui::Checkbox("Enable Shadows", &enable_shadows);
    if (enable_shadows) {
      ImGui::SliderFloat("Shadow Bias", &shadowBias, 0.0f, 0.3f);
    }
    ImGui::End();

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    glfwSwapBuffers(window);
  }

  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();

  glfwTerminate();
  free_model(cornellBox);

  return 0;
}
