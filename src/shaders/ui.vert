#version 450

layout(location = 0) in vec2 inPosition;  // screen-space pixels
layout(location = 1) in vec2 inTexCoord;
layout(location = 2) in vec4 inColor;

layout(push_constant) uniform PushConstants {
    vec2 screenSize;  // viewport width, height in pixels
} pc;

layout(location = 0) out vec2 fragTexCoord;
layout(location = 1) out vec4 fragColor;

void main() {
    // Convert pixel coords to NDC: [0,w] -> [-1,1], [0,h] -> [-1,1]
    // Y is flipped for Vulkan (top=0 in screen space → top=-1 in NDC)
    vec2 ndc = (inPosition / pc.screenSize) * 2.0 - 1.0;
    gl_Position = vec4(ndc, 0.0, 1.0);
    fragTexCoord = inTexCoord;
    fragColor = inColor;
}
