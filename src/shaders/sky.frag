#version 450

layout(location = 0) in vec3 fragViewDir;
layout(location = 0) out vec4 outColor;

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

// 天空参数通过 push constant 传入
layout(push_constant) uniform SkyParams {
    vec4 skyColorTop;      // 天顶颜色
    vec4 skyColorHorizon;  // 地平线颜色
    vec4 sunDir;           // 太阳方向 (xyz), w = sunSize
    vec4 moonDir;          // 月亮方向 (xyz), w = moonSize
    vec4 sunMoonAlpha;     // x = sunAlpha, y = moonAlpha, z = starAlpha, w = unused
} sky;

void main() {
    vec3 dir = normalize(fragViewDir);

    // === 天空渐变 ===
    // 根据视线方向的 y 分量计算天空颜色
    float t = clamp(dir.y * 2.0 + 0.1, 0.0, 1.0);  // y=0 地平线, y>0 天顶
    vec3 skyColor = mix(sky.skyColorHorizon.rgb, sky.skyColorTop.rgb, t);

    // 地平线以下稍微变暗
    if (dir.y < 0.0) {
        float below = clamp(-dir.y * 3.0, 0.0, 1.0);
        skyColor = mix(skyColor, sky.skyColorHorizon.rgb * 0.6, below);
    }

    // === 太阳 ===
    float sunDot = dot(dir, normalize(sky.sunDir.xyz));
    float sunSize = sky.sunDir.w;
    if (sunDot > (1.0 - sunSize) && sky.sunMoonAlpha.x > 0.01) {
        // 太阳光晕
        float halo = smoothstep(1.0 - sunSize * 3.0, 1.0 - sunSize, sunDot);
        skyColor += vec3(1.0, 0.8, 0.4) * halo * 0.3 * sky.sunMoonAlpha.x;

        // 太阳圆盘
        float disk = smoothstep(1.0 - sunSize * 0.5, 1.0 - sunSize * 0.2, sunDot);
        skyColor = mix(skyColor, vec3(1.0, 0.95, 0.8), disk * sky.sunMoonAlpha.x);
    }

    // === 月亮 ===
    float moonDot = dot(dir, normalize(sky.moonDir.xyz));
    float moonSize = sky.moonDir.w;
    if (moonDot > (1.0 - moonSize) && sky.sunMoonAlpha.y > 0.01) {
        float moonDisk = smoothstep(1.0 - moonSize * 0.5, 1.0 - moonSize * 0.2, moonDot);
        skyColor = mix(skyColor, vec3(0.85, 0.9, 1.0), moonDisk * sky.sunMoonAlpha.y);
    }

    // === 星空 ===
    if (sky.sunMoonAlpha.z > 0.01) {
        // 使用方向向量生成伪随机星点
        vec3 starDir = floor(dir * 80.0);  // 量化方向到网格
        float starHash = fract(sin(dot(starDir, vec3(12.9898, 78.233, 45.164))) * 43758.5453);
        if (starHash > 0.985) {  // 约 1.5% 的网格有星星
            float twinkle = fract(sin(dot(starDir, vec3(93.989, 67.345, 11.456))) * 23421.631);
            float brightness = 0.5 + 0.5 * twinkle;
            skyColor += vec3(brightness) * sky.sunMoonAlpha.z;
        }
    }

    // 水下时天空变为水雾色
    if (ubo.underwater > 0.5) {
        skyColor = vec3(0.02, 0.06, 0.22);
    }

    outColor = vec4(skyColor, 1.0);
}
