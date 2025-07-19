#version 330 core

in vec4 albedo;
in vec3 Normal;  
in vec3 FragPos;  

uniform vec3 lightPos; 
uniform vec3 viewPos; 
uniform vec3 lightColor;


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
    // ambient
    float ambientStrength = 0.2;
    vec3 ambient = ambientStrength * lightColor;
  	
    // diffuse 
    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(lightPos - FragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * lightColor;
    
    // specular
    float specularStrength = 0.5;
    vec3 viewDir = normalize(viewPos - FragPos);
    vec3 halfwayDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(viewDir, halfwayDir), 0.0), 64);
    vec3 specular = specularStrength * spec * lightColor;  

    // Calculate shadow
    float shadow = 0;
    if(enable_shadows){
        shadow = ShadowCalculation(FragPos);
    } 
        
    vec3 result = (ambient + (diffuse + specular) * (1 - shadow)) * albedo.rgb;
    FragColor = vec4(result, 1.0);
}
