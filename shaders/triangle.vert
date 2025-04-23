#version 330 core
layout (location = 0) in vec3 vPos;
layout (location = 1) in vec4 albedo;

out vec4 fColor;
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main() {
    fColor = albedo;
    gl_Position = projection * view * model * vec4(vPos, 1.0f);
}
