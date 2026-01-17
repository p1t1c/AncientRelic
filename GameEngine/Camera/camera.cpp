// camera.cpp
#include "camera.h"

Camera::Camera()
{
    cameraPosition = glm::vec3(0.0f, 0.0f, 100.0f);
    cameraViewDirection = glm::vec3(0.0f, 0.0f, -1.0f);
    cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);

    cameraRight = glm::normalize(glm::cross(cameraViewDirection, cameraUp));

    rotationOx = 0.0f;
    rotationOy = -90.0f;
}

Camera::Camera(glm::vec3 cameraPosition)
{
    this->cameraPosition = cameraPosition;
    cameraViewDirection = glm::vec3(0.0f, 0.0f, -1.0f);
    cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);

    cameraRight = glm::normalize(glm::cross(cameraViewDirection, cameraUp));

    rotationOx = 0.0f;
    rotationOy = -90.0f;
}

Camera::Camera(glm::vec3 cameraPosition, glm::vec3 cameraViewDirection, glm::vec3 cameraUp)
{
    this->cameraPosition = cameraPosition;
    this->cameraViewDirection = glm::normalize(cameraViewDirection);
    this->cameraUp = glm::normalize(cameraUp);

    this->cameraRight = glm::normalize(glm::cross(this->cameraViewDirection, this->cameraUp));

    rotationOx = 0.0f;
    rotationOy = -90.0f;
}

Camera::~Camera() {}

void Camera::keyboardMoveFront(float cameraSpeed)
{
    cameraPosition += cameraViewDirection * cameraSpeed;
}

void Camera::keyboardMoveBack(float cameraSpeed)
{
    cameraPosition -= cameraViewDirection * cameraSpeed;
}

void Camera::keyboardMoveLeft(float cameraSpeed)
{
    cameraPosition -= cameraRight * cameraSpeed;
}

void Camera::keyboardMoveRight(float cameraSpeed)
{
    cameraPosition += cameraRight * cameraSpeed;
}

void Camera::keyboardMoveUp(float cameraSpeed)
{
    cameraPosition += cameraUp * cameraSpeed;
}

void Camera::keyboardMoveDown(float cameraSpeed)
{
    cameraPosition -= cameraUp * cameraSpeed;
}

void Camera::rotateOx(float angle)
{
    // Rotate view direction around the RIGHT axis (Ox / pitch)
    cameraViewDirection = glm::normalize(glm::vec3(
        glm::rotate(glm::mat4(1.0f), angle, cameraRight) * glm::vec4(cameraViewDirection, 0.0f)
    ));

    // Recompute the other vectors (keep them orthonormal)
    cameraUp = glm::normalize(glm::cross(cameraRight, cameraViewDirection));
    cameraRight = glm::normalize(glm::cross(cameraViewDirection, cameraUp));
}

void Camera::rotateOy(float angle)
{
    // Rotate view direction around the UP axis (Oy / yaw)
    cameraViewDirection = glm::normalize(glm::vec3(
        glm::rotate(glm::mat4(1.0f), angle, cameraUp) * glm::vec4(cameraViewDirection, 0.0f)
    ));

    // Recompute the other vectors (keep them orthonormal)
    cameraRight = glm::normalize(glm::cross(cameraViewDirection, cameraUp));
    cameraUp = glm::normalize(glm::cross(cameraRight, cameraViewDirection));
}

glm::mat4 Camera::getViewMatrix()
{
    return glm::lookAt(cameraPosition, cameraPosition + cameraViewDirection, cameraUp);
}

glm::vec3 Camera::getCameraPosition()
{
    return cameraPosition;
}

glm::vec3 Camera::getCameraViewDirection()
{
    return cameraViewDirection;
}

glm::vec3 Camera::getCameraUp()
{
    return cameraUp;
}
