#version 450

layout(location = 0) in vec3 vNormal;
layout(location = 1) in vec2 vUV0;

layout(set = 1, binding = 0) uniform sampler2D uBaseColor;

layout(push_constant) uniform PushConstants
{
    mat4 model;
    vec4 baseColorFactor;
    vec4 materialParams; // x=alphaCutoff, y=alphaMode
} pc;

layout(location = 0) out vec4 outColor;

void main()
{
    vec3 n = normalize(vNormal);

    vec4 tex = texture(uBaseColor, vUV0);
    vec4 base = tex * pc.baseColorFactor;

    // alphaMode: 0=Opaque, 1=Mask, 2=Blend
    float alphaMode = pc.materialParams.y;
    float alphaCutoff = pc.materialParams.x;
    if (alphaMode > 0.5 && alphaMode < 1.5)
    {
        if (base.a < alphaCutoff)
            discard;
    }

    vec3 lightDir = normalize(vec3(0.3, 0.7, 0.2));
    float ndotl = clamp(dot(n, lightDir), 0.0, 1.0);
    vec3 ambient = vec3(0.2);
    vec3 lit = ambient + ndotl * vec3(0.8);

    outColor = vec4(base.rgb * lit, base.a);
}
