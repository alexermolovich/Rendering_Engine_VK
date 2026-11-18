#version 460
#extension GL_EXT_ray_tracing : require

layout(location = 0) rayPayloadInEXT vec3 radiance;

void main()
{
    float skyBlend = clamp(normalize(gl_WorldRayDirectionEXT).y * 0.5 + 0.5, 0.0, 1.0);
    radiance = mix(vec3(0.75, 0.82, 0.95), vec3(0.12, 0.30, 0.65), skyBlend);
}
