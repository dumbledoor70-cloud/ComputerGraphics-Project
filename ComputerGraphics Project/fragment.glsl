#version 330 core


in vec3 FragPos;
in vec3 FragNormal;
in vec2 outTextCoord;
out vec4 FragColor;

uniform vec3 outColor;
uniform vec3 lightColor;
uniform vec3 lightPos;
uniform vec3 ViewPos;

uniform sampler2D ourTexture;

void main()
{
	vec3 textureColor = texture(ourTexture,outTextCoord).rgb;

	vec3 Normal = normalize(FragNormal);
	vec3 lightDir = normalize(lightPos - FragPos);
	vec3 ViewDir = normalize(ViewPos - FragPos);


	float diffuseLight = max(dot(lightDir, Normal), 0);
	vec3 diffuse = diffuseLight * lightColor;
	
	float shininess = 512.0;
	vec3 reflectDir = normalize(reflect(-lightDir, Normal));
	float specularLight = diffuseLight > 0.0 ? max(dot(reflectDir, ViewDir), 0.0) : 0.0;
	specularLight = pow(specularLight, shininess);
	vec3 specular = specularLight * lightColor; 

	float ambientStrength = 0.3;
	vec3 ambient = ambientStrength * lightColor;
	
	vec3 result = ((diffuse + ambient + specular) * textureColor);

	FragColor = vec4(result, 1.0);
}