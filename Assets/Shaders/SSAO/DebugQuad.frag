#version 450

#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout (binding = 1) uniform   usampler2D  PosAndSpecularPacked;
layout (binding = 2) uniform   usampler2D  DiffuseNormalAndDepthPacked;
layout (binding = 3) uniform   sampler2D  ssaoImage; 

layout (location = 0) in vec3 outUV;
layout (location = 0) out vec4 outFragColor;


// c_precision of 128 fits within 7 base-10 digits
const float c_precision = 128.0;
const float c_precisionp1 = c_precision + 1.0;
/*
	param value 3-component encoded float
	returns normalized RGB value
*/
vec3 float2color(float value) 
{
    vec3 color;
    color.r = mod(value, c_precisionp1) / c_precision;
    color.b = mod(floor(value / c_precisionp1), c_precisionp1) / c_precision;
    color.g = floor(value / (c_precisionp1 * c_precisionp1)) / c_precision;
    return color;
}

/*
float LinearizeDepth(float depth)
{
  float n = 1.0; // camera z near
  float f = 128.0; // camera z far
  float z = depth;
  return (2.0 * n) / (f + n - z * (f - n));	
}
*/


vec3 VisFragment(int index)
{
	//Results
	vec2 P = (gl_FragCoord.xy / textureSize(DiffuseNormalAndDepthPacked, 0)) *2.0;
	vec3 result = vec3(1.0);

	if(index == 0)
	{
		uvec2 uvec2_PosAndSpecularPacked = textureLod(PosAndSpecularPacked, P, 0).rg;

		vec2 tempPosition0 = unpackHalf2x16(uvec2_PosAndSpecularPacked.x);
		vec2 tempPosAndSpec = unpackHalf2x16(uvec2_PosAndSpecularPacked.y);

		result = vec3(tempPosition0, tempPosAndSpec.x);
	}
	else if(index == 1)
	{
		uvec2 uvec2_DiffuseNormal = textureLod(DiffuseNormalAndDepthPacked, P, 0).rg;

		vec2 tempDiffuse0 = unpackHalf2x16(uvec2_DiffuseNormal.x);
		vec2 tempDiffAndNormal = unpackHalf2x16(uvec2_DiffuseNormal.y);

		result = vec3(tempDiffuse0.xy, tempDiffAndNormal.x);
	}
	else if(index == 2)
	{
		uvec2 uvec2_DiffuseNormal = textureLod(DiffuseNormalAndDepthPacked, P, 0).gb;

		vec2 tempDiffAndNormal = unpackHalf2x16(uvec2_DiffuseNormal.x);
		vec2 tempNormal = unpackHalf2x16(uvec2_DiffuseNormal.y);

		result = normalize(vec3(tempDiffAndNormal.y, tempNormal));
	}
	else if(index == 3)
	{
		uvec2 uvec2_PosAndSpecularPacked = textureLod(PosAndSpecularPacked, P, 0).gb;

		vec2 tempSpecularY = unpackHalf2x16(uvec2_PosAndSpecularPacked.x);
		vec2 tempSpecularXY = unpackHalf2x16(uvec2_PosAndSpecularPacked.y);

		result = vec3(tempSpecularY.y, tempSpecularXY);
	}
	else if(index == 4)
	{
		uint depth = textureLod(DiffuseNormalAndDepthPacked, P, 0).a;
		result = vec3(uintBitsToFloat(depth));
	}
	else if(index == 5)
	{
		float ssaoTexture = texture(ssaoImage,    outUV.st).r;
		result = vec3(ssaoTexture);
	}

	return result;
}


void main() 
{
	int index = int(outUV.z);
	outFragColor.rgb = VisFragment(index);
}