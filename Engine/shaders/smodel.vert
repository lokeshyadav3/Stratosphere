#version 450

// SModel v1 vertex layout (VertexPNTT):
// location 0: vec3 position
// location 1: vec3 normal
// location 2: vec2 uv0
// location 3: vec4 tangent
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV0;
layout(location = 3) in vec4 inTangent;

layout(set = 0, binding = 0) uniform CameraUBO {
    mat4 view;
    mat4 proj;
} cam;

layout(push_constant) uniform PushConstants
{
    mat4 model;
    vec4 baseColorFactor;
    vec4 materialParams;
} pc;

layout(location = 0) out vec3 vNormal;
layout(location = 1) out vec2 vUV0;

void main()
{
    vec4 worldPos = pc.model * vec4(inPosition, 1.0);
    mat3 normalMat = mat3(transpose(inverse(pc.model)));
    vNormal = normalize(normalMat * inNormal);
    vUV0 = inUV0;
    gl_Position = cam.proj * cam.view * worldPos;
}
