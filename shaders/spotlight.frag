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

out vec4 FragColor;

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
    
    vec3 result = ambient + diffuse + specular;
    FragColor = vec4(result, 1.0);  
} 