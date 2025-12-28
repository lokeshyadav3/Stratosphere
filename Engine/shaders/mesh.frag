#version 450
layout(location = 0) in vec3 vNormal;
layout(location = 0) out vec4 outColor;

void main() {
    float ndotl = clamp(dot(normalize(vNormal), normalize(vec3(0.3, 0.7, 0.2))), 0.0, 1.0);
    outColor = vec4(vec3(0.2) + ndotl * vec3(0.8), 1.0);
}