#pragma once
#include <fstream>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

/* Fly camera with Vulkan projection and cascade frustum helpers. */
class Camera
{
  public:
    Camera();

    glm::mat4 get_view_matrix() const;
    void update_view_matrix();

    glm::mat4 get_proj_matrix() const;
    void update_proj_matrix();

    void set_position(glm::vec3 pos);
    void set_direction(glm::vec3 dir);
    void set_world_up(glm::vec3 worldUp);

    void change_lens(float FOV, float nearP = -1.0f, float farP = -1.0f);
    void update_aspect_ratio(float aspectRatio);

    void serialize(std::ofstream &wf) const;
    void deserialize(std::ifstream &wf);

    void get_frustum_corners_world_space(float nearPlane, float farPlane,

                                         glm::vec3 corners[8]) const;
    glm::vec3 pos{};
    glm::vec3 dir{};
    glm::vec3 worldUp{};
    glm::vec3 right{};

    float FOV{};
    float nearPlane{};
    float farPlane{};
    float aspectRatio = 1.0f;

    bool hasChangedView = false;
    bool hasChangedProj = false;

  private:
    glm::mat4 b_view_matrix{};
    glm::mat4 b_proj_matrix{};
};
