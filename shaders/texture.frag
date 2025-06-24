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

out vec4 FragColor;

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
    vec3 result = text * (ambient + diffuse + specular) * albedo.rgb;
    FragColor = vec4(result, 1.0);
}
