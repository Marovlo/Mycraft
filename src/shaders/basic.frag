#version 450

layout(location = 0) in vec3 fragNormal;
layout(location = 1) in vec2 fragTexCoord;
layout(location = 2) in vec3 fragWorldPos;

layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 1) uniform sampler2D texSampler;

void main() {
    // Use texture for block colors
    vec4 texColor = texture(texSampler, fragTexCoord);

    // Simple directional light
    vec3 lightDir = normalize(vec3(0.3, 1.0, 0.5));
    vec3 norm = normalize(fragNormal);
    float diff = max(dot(norm, lightDir), 0.0);
    float ambient = 0.4;
    float lighting = ambient + diff * 0.6;

    // Add face-based shading (different faces get different brightness)
    if (abs(norm.y) > 0.5) {
        if (norm.y > 0.0) lighting *= 1.0;  // top face brightest
        else lighting *= 0.5;                // bottom face darkest
    } else if (abs(norm.x) > 0.5) {
        lighting *= 0.8;
    } else {
        lighting *= 0.7;
    }

    outColor = vec4(texColor.rgb * lighting, 1.0);
}
