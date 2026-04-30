#version 450

layout(location = 0) in vec3 fragNormal;
layout(location = 1) in vec2 fragTexCoord;
layout(location = 2) in vec3 fragWorldPos;
layout(location = 3) in float fragLight;
layout(location = 4) in vec3 fragColor;

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

layout(set = 0, binding = 1) uniform sampler2D texSampler;

// Water fog parameters (shared between surface-look-down and underwater)
const vec3  WATER_FOG_COLOR = vec3(0.02, 0.06, 0.22);
const float WATER_FOG_START = 2.0;
const float WATER_FOG_END   = 28.0;
const vec3  WATER_TINT      = vec3(0.4, 0.55, 1.0);

void main() {
    // --- Special mode: negative light encodes non-textured overlays ---
    // light in (-1.5, 0): solid-color wireframe (block highlight)
    if (fragLight < 0.0 && fragLight > -1.5) {
        float brightness = -fragLight;
        outColor = vec4(vec3(brightness), 0.4);
        return;
    }

    // light <= -1.5: water face. Actual light = -(fragLight + 2.0)
    bool isWaterFace = (fragLight <= -1.5);
    float lightVal = isWaterFace ? -(fragLight + 2.0) : fragLight;

    // light > 1.5: 生物受伤闪红效果。实际亮度 = lightVal - 2.0 (编码为 light = 2.0 + realLight)
    bool isHurt = (!isWaterFace && fragLight > 1.5);
    if (isHurt) {
        lightVal = fragLight - 2.0;
    }

    vec4 texColor = texture(texSampler, fragTexCoord);

    // 应用生物群系 tint 颜色（白色 = 不着色）
    texColor.rgb *= fragColor;

    // Alpha test for cutout rendering (destroy overlay, foliage, cross plants)
    // Skip for water faces (they use alpha blending instead)
    if (!isWaterFace && texColor.a < 0.5) discard;

    // MC-style face shading multiplier (directional ambient)
    vec3 norm = normalize(fragNormal);
    float faceShade;
    if (abs(norm.y) > 0.5) {
        faceShade = norm.y > 0.0 ? 1.0 : 0.5;
    } else if (abs(norm.x) > 0.5) {
        faceShade = 0.8;
    } else {
        faceShade = 0.7;
    }

    // Combine per-vertex light level with face shading
    vec3 litColor = texColor.rgb * lightVal * faceShade;

    // 受伤闪红：混合红色
    if (isHurt) {
        litColor = mix(litColor, vec3(1.0, 0.3, 0.3), 0.5);
    }

    // --- Fog computation ---
    vec3 fogCol   = ubo.fogColor.rgb;
    float fogStart = ubo.fogRange.x;
    float fogEnd   = ubo.fogRange.y;

    // Camera fully underwater: everything gets water fog
    if (ubo.underwater > 0.5) {
        fogCol   = WATER_FOG_COLOR;
        fogStart = WATER_FOG_START;
        fogEnd   = WATER_FOG_END;
        // Blue tint on everything underwater
        litColor = mix(litColor, litColor * WATER_TINT, 0.55);
    }

    // Standard distance fog
    float dist = length(fragWorldPos - ubo.viewPos.xyz);
    float fogFactor = clamp((fogEnd - dist) / (fogEnd - fogStart), 0.0, 1.0);
    vec3 finalColor = mix(fogCol, litColor, fogFactor);

    // --- Looking down into water from above ---
    // When camera is above water but the fragment is below the water surface,
    // apply additional water-depth fog based on how far the ray travels through water.
    if (ubo.underwater < 0.5 && !isWaterFace && fragWorldPos.y < ubo.waterSurfaceY) {
        // Compute the ray direction from camera to fragment
        vec3 rayDir = normalize(fragWorldPos - ubo.viewPos.xyz);

        // Find where the ray enters the water (intersect y = waterSurfaceY plane)
        float waterEntryDist = dist; // default: entire distance is in water
        if (abs(rayDir.y) > 0.001) {
            float t = (ubo.waterSurfaceY - ubo.viewPos.y) / rayDir.y;
            if (t > 0.0) {
                waterEntryDist = t;
            }
        }

        // Distance the ray travels through water
        float waterDist = max(dist - waterEntryDist, 0.0);

        // Apply water fog based on water travel distance
        float waterFog = clamp((WATER_FOG_END - waterDist) / (WATER_FOG_END - WATER_FOG_START), 0.0, 1.0);

        // Tint the submerged part blue
        vec3 tintedColor = mix(finalColor, finalColor * WATER_TINT, 0.35);
        finalColor = mix(WATER_FOG_COLOR, tintedColor, waterFog);
    }

    // Water faces: semi-transparent with deep blue tint
    float alpha = 1.0;
    if (isWaterFace) {
        alpha = 0.80;
        // Strong blue shift: heavily suppress red, moderate green, full blue
        finalColor *= vec3(0.25, 0.5, 1.0);
        // Blend in a solid deep blue base for consistent water color
        finalColor = mix(finalColor, vec3(0.08, 0.18, 0.42), 0.25);
    }

    outColor = vec4(finalColor, alpha);
}
