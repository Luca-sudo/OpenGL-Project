#version 330 core

in vec4 albedo;
in vec3 Normal;  
in vec3 FragPos;  

uniform vec3 lightPos; 
uniform vec3 viewPos; 
uniform vec3 lightColor;
uniform vec3 lightDir;
uniform float lightCutoffAngle; 
uniform float lightOuterCutoffAngle;

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
    vec3 ambient = ambientStrength * lightColor * albedo.rgb;

    // diffuse 
    vec3 norm = normalize(Normal);
    vec3 incomingLightAngle = normalize(lightPos - FragPos);
    float diff = max(dot(norm, incomingLightAngle), 0.0);
    vec3 diffuse = diff * lightColor * albedo.rgb;
    
    // specular
    float specularStrength = 0.7;
    vec3 viewDir = normalize(viewPos - FragPos);
    vec3 halfwayDir = normalize(incomingLightAngle + viewDir);
    float spec = pow(max(dot(viewDir, halfwayDir), 0.0), 64);
    vec3 specular = specularStrength * spec * albedo.rgb;  

    // spotlight
    float theta = dot(incomingLightAngle, normalize(-lightDir));
    float epsilon = lightCutoffAngle - lightOuterCutoffAngle;
    float intensity = clamp((theta - lightOuterCutoffAngle) / epsilon, 0.0, 1.0);
    diffuse *= intensity;
    specular *= intensity;

    // Calculate shadow
    float shadow = 0;
    if(enable_shadows){
        shadow = ShadowCalculation(FragPos);
    } 
    
    vec3 result = ambient + (diffuse + specular) * (1 - shadow);
    FragColor = vec4(result, 1.0);  
} 