#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace engine::world {

// single camera
struct Camera {
    glm::vec3 position{ 0.f, 0.f, 0.f };
    glm::vec3 look{ 0.f, 0.f, -1.f };
    glm::vec3 up{ 0.f, -1.f, 0.f };
    // field of view in degrees
    float fovDeg{ 60.f };

    glm::mat4 view() const noexcept {
        return glm::lookAt(position, position + glm::normalize(look), up);
    }

    glm::mat4 proj(float viewportHeight, float viewportWidth, float nearZ, float farZ) const noexcept {
        return glm::perspective(glm::radians(fovDeg), viewportWidth / viewportHeight, nearZ, farZ);
    }
};

}
