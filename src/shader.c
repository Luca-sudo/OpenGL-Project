#include <shader.h>
#include <stdio.h>
#include <stdlib.h>

// Allocates memory for the code internally.
char *read_shader_from_file(const char *filepath) {
  FILE *file = fopen(filepath, "r");
  if (file == NULL) {
    fprintf(stderr, "Failed to open file @ %s\n", filepath);
    exit(EXIT_FAILURE);
  }

  fseek(file, 0L, SEEK_END);
  long fileSize = ftell(file);
  fseek(file, 0L, SEEK_SET);

  // Add one to accomodate null termination
  char *shader = malloc(fileSize + 1);
  if (shader == NULL) {
    fprintf(stderr, "Failed to allocate memory for shader code @ %s\n",
            filepath);
    fclose(file);
    exit(EXIT_FAILURE);
  }

  size_t bytesRead = fread(shader, sizeof(char), fileSize, file);
  if (bytesRead != fileSize) {
    fprintf(stderr, "Failed to read from shader file @ %s\n", filepath);
    fclose(file);
    free(shader);
    exit(EXIT_FAILURE);
  }

  shader[fileSize] = '\0';
  return shader;
}
