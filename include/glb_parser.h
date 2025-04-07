#ifndef GLB_PARSER_H
#define GLB_PARSER_H

// Extracts interleaved position + color data
float* extract_vertex_data_from_glb(const char* filename, int* vertex_count);

// Gets number of vertices
int get_vertex_count_from_glb(const char* filename);

#endif
