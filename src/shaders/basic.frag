#version 450

layout(location = 0) in vec3 fragNormal;
layout(location = 1) in vec2 fragTexCoord;
layout(location = 2) in vec3 fragWorldPos;

layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
    vec4 fogColor;
    vec4 viewPos;
    vec2 fogRange;
    vec2 padding;
} ubo;

layout(set = 0, binding = 1) uniform sampler2D texSampler;

void main() {
    vec4 texColor = texture(texSampler, fragTexCoord);

    // Simple directional light
    vec3 lightDir = normalize(vec3(0.3, 1.0, 0.5));
    vec3 norm = normalize(fragNormal);
    float diff = max(dot(norm, lightDir), 0.0);
    float ambient = 0.4;
    float lighting = ambient + diff * 0.6;

    // Face-based shading (MC-style)
    if (abs(norm.y) > 0.5) {
        if (norm.y > 0.0) lighting *= 1.0;
        else lighting *= 0.5;
    } else if (abs(norm.x) > 0.5) {
        lighting *= 0.8;
    } else {
        lighting *= 0.7;
    }

    vec3 litColor = texColor.rgb * lighting;

    // Distance fog
    float dist = length(fragWorldPos - ubo.viewPos.xyz);
    float fogFactor = clamp((ubo.fogRange.y - dist) / (ubo.fogRange.y - ubo.fogRange.x), 0.0, 1.0);
    vec3 finalColor = mix(ubo.fogColor.rgb, litColor, fogFactor);

    outColor = vec4(finalColor, texColor.a);
}
