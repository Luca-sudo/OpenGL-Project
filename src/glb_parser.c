#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cgltf/cgltf.h>
#include "glb_parser.h"

// Function to extract interleaved vertex data (position + color) from a GLB file
float* extract_vertex_data_from_glb(const char* filename, int* vertex_count) {
    *vertex_count = 0;

    // Parse the glTF file using cgltf
    cgltf_options options = {0};
    cgltf_data* data = NULL;
    cgltf_result result = cgltf_parse_file(&options, filename, &data);

    if (result != cgltf_result_success) {
        printf("Failed to parse GLB file: %s\n", filename);
        return NULL;
    }

    // Load binary data
    result = cgltf_load_buffers(&options, data, filename);
    if (result != cgltf_result_success) {
        printf("Failed to load GLB buffers\n");
        cgltf_free(data);
        return NULL;
    }

    // Count total vertices across all meshes
    size_t total_vertices = 0;
    for (size_t i = 0; i < data->meshes_count; i++) {
        cgltf_mesh* mesh = &data->meshes[i];

        for (size_t j = 0; j < mesh->primitives_count; j++) {
            cgltf_primitive* primitive = &mesh->primitives[j];

            // Find position accessor
            cgltf_accessor* position = NULL;
            for (size_t k = 0; k < primitive->attributes_count; k++) {
                if (primitive->attributes[k].type == cgltf_attribute_type_position) {
                    position = primitive->attributes[k].data;
                    break;
                }
            }

            if (position) {
                total_vertices += position->count;
            }
        }
    }
    
    if (total_vertices == 0) {
        printf("No vertices found in the GLB file\n");
        cgltf_free(data);
        return NULL;
    }
    
    // Allocate memory for interleaved vertex data (3 floats position + 3 floats color)
    float* vertex_data = (float*)malloc(total_vertices * 6 * sizeof(float));
    if (!vertex_data) {
        printf("Failed to allocate memory for vertex data\n");
        cgltf_free(data);
        return NULL;
    }
    
    // Extract vertex data from all meshes
    size_t vertex_index = 0;
    for (size_t i = 0; i < data->meshes_count; i++) {
        cgltf_mesh* mesh = &data->meshes[i];
        
        for (size_t j = 0; j < mesh->primitives_count; j++) {
            cgltf_primitive* primitive = &mesh->primitives[j];
            
            // Find position accessor
            cgltf_accessor* position = NULL;
            for (size_t k = 0; k < primitive->attributes_count; k++) {
                if (primitive->attributes[k].type == cgltf_attribute_type_position) {
                    position = primitive->attributes[k].data;
                    break;
                }
            }
            
            if (!position) continue;
            
            // Get material color
            float material_color[3] = {0.8f, 0.8f, 0.8f}; // Default color if no material
            if (primitive->material) {
                cgltf_material* material = primitive->material;
                material_color[0] = material->pbr_metallic_roughness.base_color_factor[0];
                material_color[1] = material->pbr_metallic_roughness.base_color_factor[1];
                material_color[2] = material->pbr_metallic_roughness.base_color_factor[2];
            }
            
            // Extract position data
            for (size_t v = 0; v < position->count; v++) {
                float pos[3];
                cgltf_accessor_read_float(position, v, pos, 3);
                
                // Set position
                vertex_data[vertex_index * 6] = pos[0];
                vertex_data[vertex_index * 6 + 1] = pos[1];
                vertex_data[vertex_index * 6 + 2] = pos[2];
                
                // Set color
                vertex_data[vertex_index * 6 + 3] = material_color[0];
                vertex_data[vertex_index * 6 + 4] = material_color[1];
                vertex_data[vertex_index * 6 + 5] = material_color[2];
                
                vertex_index++;
            }
        }
    }
    
    *vertex_count = (int)total_vertices;
    cgltf_free(data);
    return vertex_data;
}

// Function to get vertex count from a GLB file
int get_vertex_count_from_glb(const char* filename) {
    // Parse the glTF file using cgltf
    cgltf_options options = {0};
    cgltf_data* data = NULL;
    cgltf_result result = cgltf_parse_file(&options, filename, &data);
    
    if (result != cgltf_result_success) {
        return 0;
    }
    
    // Count total vertices across all meshes
    size_t total_vertices = 0;
    for (size_t i = 0; i < data->meshes_count; i++) {
        cgltf_mesh* mesh = &data->meshes[i];
        
        for (size_t j = 0; j < mesh->primitives_count; j++) {
            cgltf_primitive* primitive = &mesh->primitives[j];
            
            // Find position accessor
            cgltf_accessor* position = NULL;
            for (size_t k = 0; k < primitive->attributes_count; k++) {
                if (primitive->attributes[k].type == cgltf_attribute_type_position) {
                    position = primitive->attributes[k].data;
                    break;
                }
            }
            
            if (position) {
                total_vertices += position->count;
            }
        }
    }
    
    cgltf_free(data);
    return (int)total_vertices;
}
