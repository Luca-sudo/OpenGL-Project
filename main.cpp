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

struct ShaderDeclaration {
  const char *name, *vertPath, *fragPath;
  unsigned int program = 0;
};

typedef struct {
  aiVector3D *vertices;
  aiColor4D *albedo;
  aiVector3D *normals;
  unsigned int *indices;

  unsigned int vertexOffset;
  unsigned indexOffset;
} model_t;

typedef struct {
  vec3 point;    // A point on the plane
  vec3 normal;   // Plane normal (should point towards the viewer)
} reflection_plane_t;

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

  const C_STRUCT aiScene *scene = aiImportFile(
      "assets/cornell_box_v1.gltf", aiProcessPreset_TargetRealtime_MaxQuality);

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

  reflection_plane_t mirror_plane = {
    {3.440f, 1.650f, 2.714f},
    {-0.296f, 0.000f, -0.955f}
  };

  int selected_shader = 5;
  ShaderDeclaration SHADERS[] = {
    {"Flat", "shaders/flat.vert", "shaders/flat.frag"},
    {"Lambertian", "shaders/lambertian.vert", "shaders/lambertian.frag"},
    {"Phong", "shaders/phong.vert", "shaders/phong.frag"},
    {"Blinn-Phong", "shaders/blinn_phong.vert", "shaders/blinn_phong.frag"},
    {"Spotlight", "shaders/blinn_phong.vert", "shaders/spotlight.frag"},
    {"Reflection", "shaders/reflection.vert", "shaders/blinn_phong.frag"}
  };

  auto NUM_SHADERS = sizeof(SHADERS) / sizeof(SHADERS[0]);
  const char* SHADER_NAMES[NUM_SHADERS];

  for(int i = 0; i < NUM_SHADERS; i++){
    unsigned int vertexShader, fragShader, shaderProgram;
    vertexShader = glCreateShader(GL_VERTEX_SHADER);
    char *vertShaderCode = read_shader_from_file(SHADERS[i].vertPath);
    glShaderSource(vertexShader, 1, (const char *const *)&vertShaderCode, NULL);
    glCompileShader(vertexShader);

    fragShader = glCreateShader(GL_FRAGMENT_SHADER);
    char *fragShaderCode = read_shader_from_file(SHADERS[i].fragPath);
    glShaderSource(fragShader, 1, (const char *const *)&fragShaderCode, NULL);
    glCompileShader(fragShader);

    shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragShader);
    glLinkProgram(shaderProgram);

    glDeleteShader(vertexShader);
    glDeleteShader(fragShader);

    SHADERS[i].program = shaderProgram;
    SHADER_NAMES[i] = SHADERS[i].name;
  }

  unsigned int VAO, EBO, positions, albedo, normals;
  glGenVertexArrays(1, &VAO);
  glGenBuffers(1, &positions);
  glGenBuffers(1, &albedo);
  glGenBuffers(1, &EBO);
  glGenBuffers(1, &normals);

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

  unsigned int VAO_stencil, VBO_stencil;
  create_reflective_surface_stencil(&VAO_stencil, &VBO_stencil);

  // Setup Dear ImGui context
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO &io = ImGui::GetIO();
  ImGui_ImplGlfw_InitForOpenGL(window, true);
  ImGui_ImplOpenGL3_Init("#version 330");
  ImGui::StyleColorsDark();

  bool enable_reflection = true;

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

    vec3 lightPos = {2.78f, 5.48f, 2.796f};
    vec3 lightColor = {1.0f, 1.0f, 1.0f};
    vec3 lightDir;
    glm_vec3_negate_to(up, lightDir);
    float lightCutoffAngle = cos(0.2618f);
    float lightOuterCutoffAngle = cos(1.04f);

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
    glDrawElements(GL_TRIANGLES, cornellBox.indexOffset, GL_UNSIGNED_INT, 0);

    // Second pass: Create reflection if enabled
    if (enable_reflection) {
      // Step 1: Mark stencil buffer where mirror surface is visible
      glStencilFunc(GL_ALWAYS, 1, 0xFF);      
      glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE); 
      glStencilMask(0xFF);                     
      glDepthMask(GL_FALSE);                  
      glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE); 

      // Draw mirror quad (writes to stencil only)
      glBindVertexArray(VAO_stencil);
      glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);   

      // Restore write masks
      glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);   
      glDepthMask(GL_TRUE);     

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
    ImGui::Checkbox("Enable Reflection", &enable_reflection);
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

  return 0;
}