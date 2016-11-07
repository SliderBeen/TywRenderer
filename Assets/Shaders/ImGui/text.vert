#version 450

#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aUV;
layout(location = 2) in vec4 aColor;

layout(push_constant) uniform uPushConstant
{
    vec2 uScale;
    vec2 uTranslate;
} pc;

layout (location = 3) out block
{
    vec4 Color;
    vec2 UV;
} Out;

void main()
{
    Out.Color = aColor;
    Out.UV = aUV;
    gl_Position = vec4(aPos*pc.uScale+pc.uTranslate, 0, 1);
}