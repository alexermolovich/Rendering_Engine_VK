#include "camera.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

Camera::Camera()
{
    this->set_position(glm::vec3(0));
    this->set_direction(glm::vec3(0, 0, -1));
    this->set_world_up(glm::vec3(0, 1, 0));
    this->change_lens(100.f, 0.1f, 100000.f);
}

/* Update or retrieve the camera state. */
glm::mat4 Camera::get_view_matrix() const
{
    return this->b_view_matrix;
}

/* Update or retrieve the camera state. */
void Camera::update_view_matrix()
{
    this->b_view_matrix = glm::lookAt(this->pos, this->pos + this->dir, this->worldUp);
    hasChangedView = false;
}

/* Update or retrieve the camera state. */
glm::mat4 Camera::get_proj_matrix() const
{
    return this->b_proj_matrix;
}

/* Update or retrieve the camera state. */
void Camera::update_proj_matrix()
{
    // Vulkan's NDC depth range is [0, 1].  Using GLM's default [-1, 1]
    // projection makes reconstructed world positions disagree with the depth
    // buffer and, in turn, makes shadows appear attached to the camera.
    this->b_proj_matrix = glm::perspectiveRH_ZO(glm::radians(FOV), aspectRatio, nearPlane, farPlane);
    this->b_proj_matrix[1][1] *= -1.0f;
    hasChangedProj = false;
}

/* Update or retrieve the camera state. */
void Camera::set_position(glm::vec3 pos)
{
    this->pos = pos;
    hasChangedView = true;
}

/* Update or retrieve the camera state. */
void Camera::set_direction(glm::vec3 dir)
{
    this->dir = dir;
    this->right = glm::cross(dir, worldUp);
    hasChangedView = true;
}

/* Update or retrieve the camera state. */
void Camera::set_world_up(glm::vec3 worldUp)
{
    this->worldUp = worldUp;
    this->right = glm::cross(dir, worldUp);
    hasChangedView = true;
}

/* Update or retrieve the camera state. */
void Camera::change_lens(float FOV, float nearP, float farP)
{
    this->FOV = FOV;
    if (nearP > 0)
        nearPlane = nearP;
    if (farP > 0)
        farPlane = farP;
    hasChangedProj = true;
}

/* Update or retrieve the camera state. */
void Camera::update_aspect_ratio(float aspectRatio)
{
    this->aspectRatio = aspectRatio;
    hasChangedProj = true;
}

/* Update or retrieve the camera state. */
void Camera::serialize(std::ofstream &wf) const
{
    wf.write((char *)&pos, sizeof(pos));
    wf.write((char *)&dir, sizeof(dir));
    wf.write((char *)&worldUp, sizeof(worldUp));
    wf.write((char *)&FOV, sizeof(FOV));
    wf.write((char *)&nearPlane, sizeof(nearPlane));
    wf.write((char *)&farPlane, sizeof(farPlane));
}

/* Update or retrieve the camera state. */
void Camera::deserialize(std::ifstream &wf)
{
    wf.read((char *)&pos, sizeof(pos));
    wf.read((char *)&dir, sizeof(dir));
    wf.read((char *)&worldUp, sizeof(worldUp));
    wf.read((char *)&FOV, sizeof(FOV));
    wf.read((char *)&nearPlane, sizeof(nearPlane));
    wf.read((char *)&farPlane, sizeof(farPlane));
    this->right = glm::cross(dir, worldUp);
    hasChangedProj = true;
    hasChangedView = true;
}

/* Update or retrieve the camera state. */
void Camera::get_frustum_corners_world_space(float nearPlane, float farPlane, glm::vec3 corners[8]) const
{
    glm::mat4 proj = glm::perspectiveRH_ZO(glm::radians(FOV), aspectRatio, nearPlane, farPlane);

    // Vulkan clip space has an inverted Y axis.
    proj[1][1] *= -1.0f;

    glm::mat4 inv = glm::inverse(proj * get_view_matrix());

    int index = 0;

    for (int z = 0; z < 2; ++z)
    {
        for (int y = 0; y < 2; ++y)
        {
            for (int x = 0; x < 2; ++x)
            {
                glm::vec4 pt = inv * glm::vec4(x ? 1.0f : -1.0f, y ? 1.0f : -1.0f, z ? 1.0f : 0.0f, 1.0f);

                corners[index++] = glm::vec3(pt) / pt.w;
            }
        }
    }
}
