#version 450

// 全屏三角形 — 不需要顶点输入
// 通过 gl_VertexIndex 生成覆盖全屏的三角形

layout(set = 0, binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
    vec4 fogColor;
    vec4 viewPos;
    vec2 fogRange;
    float underwater;
    float waterSurfaceY;
} ubo;

layout(location = 0) out vec3 fragViewDir;

void main() {
    // 生成全屏三角形的顶点（覆盖 [-1,1] NDC 空间）
    vec2 pos = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
    vec2 ndc = pos * 2.0 - 1.0;

    // 输出位置在最远深度（z=1.0），确保天空在所有物体后面
    gl_Position = vec4(ndc, 1.0, 1.0);

    // 计算视线方向：从 NDC 反投影到世界空间
    mat4 invProj = inverse(ubo.proj);
    mat4 invView = inverse(ubo.view);

    // NDC → 视图空间方向
    vec4 clipPos = vec4(ndc, 1.0, 1.0);
    vec4 viewDir4 = invProj * clipPos;
    viewDir4.w = 0.0;

    // 视图空间 → 世界空间方向
    fragViewDir = (invView * viewDir4).xyz;
}
