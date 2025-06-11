#version 330 core

in vec4 albedo;
in vec3 FragPos;

uniform vec3 lightPos;
uniform vec3 viewPos;
uniform vec3 lightColor;

out vec4 FragColor;

void main()
{
    // ambient
    float ambientStrength = 0.2;
    vec3 ambient = ambientStrength * lightColor;

    vec3 result = ambient * albedo.rgb;
    FragColor = vec4(result, 1.0);
}
