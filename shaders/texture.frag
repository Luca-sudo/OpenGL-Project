#version 330 core

in vec4 albedo;
in vec3 Normal;  
in vec3 FragPos;
in vec2 Uv;
in mat3 TBN;

uniform vec3 lightPos; 
uniform vec3 viewPos; 
uniform vec3 lightColor;

uniform sampler2D diffuseMap;
uniform sampler2D normalMap;

uniform bool enable_shadows;
uniform float far_plane;
uniform samplerCube shadowMap;
uniform float shadowBias;

out vec4 FragColor;

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

void main()
{
    // Normal mapping, if applicable
    vec3 text = vec3(1.0, 1.0, 1.0);
    vec3 normal = Normal;
    if(Uv.x != -1.0 || Uv.y != -1.0){
        text = texture(diffuseMap, Uv).rgb;
        vec3 modelNormal = texture(normalMap, Uv).rgb * 2.0 - 1.0;
        normal = TBN * modelNormal;
    }

    // Calculate shadow
    float shadow = 0;
    if(enable_shadows){
        shadow = ShadowCalculation(FragPos);
    } 
    

    // ambient
    float ambientStrength = 0.2;
    vec3 ambient = ambientStrength * lightColor;
  	
    // diffuse 
    vec3 norm = normalize(normal);
    vec3 lightDir = normalize(lightPos - FragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * lightColor;
    
    // specular
    float specularStrength = 0.5;
    vec3 viewDir = normalize(viewPos - FragPos);
    vec3 halfwayDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(viewDir, halfwayDir), 0.0), 16);
    vec3 specular = specularStrength * spec * lightColor;
    vec3 result = text * (ambient + (diffuse + specular) * (1.0 - shadow)) * albedo.rgb;
    FragColor = vec4(result, 1.0);
}
