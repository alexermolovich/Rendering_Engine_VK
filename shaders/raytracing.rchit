#version 460
#extension GL_EXT_ray_tracing : require

layout(location = 0) rayPayloadInEXT vec3 radiance;
hitAttributeEXT vec2 barycentrics;

void main()
{
    // Debug colors expose triangle coverage before material buffers are connected.
    radiance = vec3(1.0 - barycentrics.x - barycentrics.y, barycentrics.x, barycentrics.y);
}
