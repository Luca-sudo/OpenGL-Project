#version 330 core
layout (location = 0) in vec3 vPos;
layout (location = 1) in vec4 vAlbedo;

out vec4 albedo;
out vec3 FragPos;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main()
{
    albedo = vAlbedo;

    FragPos = vec3(model * vec4(vPos, 1.0));

    gl_Position = projection * view * vec4(FragPos, 1.0);
}
