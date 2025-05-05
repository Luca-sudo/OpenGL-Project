#version 330 core
out vec4 FragColor;

in vec3 FragPos;
in vec3 Normal;
in vec3 Color;

uniform vec3 lightPos;
uniform vec3 viewPos;
uniform vec3 lightColor;
uniform float far_plane;
uniform samplerCube shadowMap;
uniform float shadowBias;

// Function to calculate shadow factor from cubemap
float ShadowCalculation(vec3 fragPos) {
    // Get vector between fragment position and light position
    vec3 fragToLight = fragPos - lightPos;
    
    // Use the fragment to light vector to sample from the depth map    
    float closestDepth = texture(shadowMap, fragToLight).r;
    
    // It is currently in linear range between [0,1]. Re-transform back to original depth value
    closestDepth *= far_plane;
    
    // Get current linear depth as the length between the fragment and light position
    float currentDepth = length(fragToLight);
    
    // Check whether current frag pos is in shadow
    float shadow = currentDepth - shadowBias > closestDepth ? 1.0 : 0.0;
    
    return shadow;
}

void main() {
    // Ambient
    float ambientStrength = 0.25;
    vec3 ambient = ambientStrength * lightColor;
    
    // Diffuse 
    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(lightPos - FragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * lightColor;
    
    // Specular - Blinn-Phong
    float specularStrength = 0.5;
    vec3 viewDir = normalize(viewPos - FragPos);
    vec3 halfwayDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(norm, halfwayDir), 0.0), 32.0);
    vec3 specular = specularStrength * spec * lightColor;
    
    // Calculate shadow
    float shadow = ShadowCalculation(FragPos);
    
    // Combine results
    vec3 result = (ambient + (1.0 - shadow) * (diffuse + specular)) * Color;
    FragColor = vec4(result, 1.0);
}