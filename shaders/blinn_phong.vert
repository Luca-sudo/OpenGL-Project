#version 330 core
layout (location = 0) in vec3 vPos;
layout (location = 1) in vec4 vAlbedo;
layout (location = 2) in vec3 aNormal;

out vec4 albedo;
out vec3 FragPos;
out vec3 Normal;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

uniform vec4 clipPlane; // Clipping plane in world space (ax + by + cz + d = 0)
uniform bool useClipping;

void main()
{
    albedo = vAlbedo;

    FragPos = vec3(model * vec4(vPos, 1.0));
    Normal = mat3(transpose(inverse(model))) * aNormal;  
    
    gl_Position = projection * view * vec4(FragPos, 1.0);

    if (useClipping)
        gl_ClipDistance[0] = dot(model * vec4(vPos, 1.0), clipPlane);
    else
        gl_ClipDistance[0] = 1.0; // Always keep it (positive means inside)
}
