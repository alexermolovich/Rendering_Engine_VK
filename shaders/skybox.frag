#version 450

layout(location = 0) in vec2 inUV;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 14) uniform texture2D Skybox_Image;
layout(set = 0, binding = 8) uniform sampler Skybox_Sampler;

layout(set = 1, binding = 0) uniform Ubo {
    mat4 projection;
    mat4 model;
    mat4 view;
    vec3 cameraPos;
    float nearPlane;
    float farPlane;
    mat4 invProjection;
} ubo;

void main()
{
    // Rotate the viewing ray into world space, ignoring camera translation.
    vec4 viewRay = ubo.invProjection * vec4(inUV * 2.0 - 1.0, 1.0, 1.0);
    vec3 direction = normalize(transpose(mat3(ubo.view)) * viewRay.xyz);

    // Map the world direction onto an equirectangular panorama.
    const float PI = 3.14159265359;
    vec2 skyUV = vec2(
        atan(direction.z, direction.x) / (2.0 * PI) + 0.5,
        acos(clamp(direction.y, -1.0, 1.0)) / PI);
    skyUV.x = 1.0 - skyUV.x;
    float halfTexelY = 0.5 / float(textureSize(sampler2D(Skybox_Image, Skybox_Sampler), 0).y);
    skyUV.y = clamp(skyUV.y, halfTexelY, 1.0 - halfTexelY);
    outColor = texture(sampler2D(Skybox_Image, Skybox_Sampler), skyUV);
}
