
-- Vertex

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aTexCoords;

out vec2 TexCoords;

void main()
{
    TexCoords = aTexCoords;
    gl_Position = vec4(aPos, 1.0);
}

-- Fragment

out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D gPosition;
uniform sampler2D gNormal;
uniform sampler2D gDiffuse;
uniform sampler2D gSpecular;
uniform sampler2D shadowMap;
uniform mat4 lightSpaceMatrix;
uniform float glossiness;

struct Light {
    vec3 Position;
    vec3 Color;
    
    float Linear;
    float Quadratic;
	float Radius;
};

uniform Light gLight;
uniform vec3 viewPos;

const int nsamples = 8;

uniform vec4 offset[nsamples] = { vec4(0.000000, 0.000000, 0.0, 0.0),
								  vec4(0.079821, 0.165750, 0.0, 0.0),
								  vec4(-0.331500, 0.159642, 0.0, 0.0),
								  vec4(-0.239463, -0.497250, 0.0, 0.0),
								  vec4(0.662999, -0.319284, 0.0, 0.0),
								  vec4(0.399104, 0.828749, 0.0, 0.0),
								  vec4(-0.994499, 0.478925, 0.0, 0.0),
								  vec4(-0.558746, -1.160249, 0.0, 0.0) };
								  
float getOcclusionCoef(vec4 shadowCoord, float bias)
{
	// get the stored depth
	float shadow_d = texture(shadowMap, shadowCoord.xy).r; 
	float diff = shadow_d - (shadowCoord.z + bias);
	return shadowCoord.z - bias > shadow_d  ? 0.0 : 1.0;  	
}

// using percent closer filtering technique
float percentCloserFilteredShadow (vec3 fragPos, vec3 normal)
{
	vec4 fragPosLightSpace = lightSpaceMatrix * vec4(fragPos, 1.0);
	// perform perspective divide
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
	// transform to [0,1] range
    projCoords = projCoords * 0.5 + 0.5;
	// calculate bias
	vec3 lightDir = normalize(gLight.Position - fragPos);
	float bias = max(0.05 * (1.0 - dot(normal, lightDir)), 0.005);
	float scale = 1.0 / textureSize(shadowMap, 0).x; 
	// sum shadow samples
	float shadowCoef = 0.0;
	for(int i=0; i<nsamples; i++)
	{
		shadowCoef += getOcclusionCoef(vec4(projCoords, 0.0) + scale*offset[i], bias);
	}
	shadowCoef /= nsamples;
	
	// keep the shadow at 1.0 when outside the zFar region of the light's frustum.
    if(projCoords.z > 1.0)
        shadowCoef = 1.0;
	
	return shadowCoef;
}

void main()
{             
    // retrieve data from gbuffer
    vec3 FragPos = texture(gPosition, TexCoords).rgb;
    vec3 Normal = texture(gNormal, TexCoords).rgb;
    vec3 Diffuse = texture(gDiffuse, TexCoords).rgb;
    vec4 Specular = texture(gSpecular, TexCoords);
	
	// do Phong lighting calculation
	vec3 ambient  = Diffuse * 0.2; // hard-coded ambient component
	vec3 viewDir  = normalize(viewPos - FragPos);
	
	// diffuse
	vec3 lightDir = normalize(gLight.Position - FragPos);
	vec3 diffuse = max(dot(Normal, lightDir), 0.0) * Diffuse * gLight.Color;
	// specular
	vec3 halfwayDir = normalize(lightDir + viewDir);  
	float spec = pow(max(dot(Normal, halfwayDir), 0.0), glossiness) * Specular.a;
	vec3 specular = gLight.Color * spec * Specular.rgb;
	// attenuation
	float distance = length(gLight.Position - FragPos);
	float attenuation = 1.0 / (1.0 + gLight.Linear * distance + gLight.Quadratic * distance * distance);
	diffuse *= attenuation;
	specular *= attenuation;
	// calculate shadow using PCF
	float shadow = percentCloserFilteredShadow(FragPos, Normal);
	
	vec3 result = ambient + (diffuse + specular) * shadow;
			
	FragColor = vec4(result, 1.0);
}