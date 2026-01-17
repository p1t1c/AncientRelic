// main.cpp (FULL FILE - main room + portals + columns + rocks + chest shake
// + cave1 simple plane + fast enemy + yellow coin pickup -> back to main
// + cave2 simple plane + TWO mirrored enemies (faster) + coin pickup -> back to main)

#include "Graphics\\window.h"
#include "Camera\\camera.h"
#include "Shaders\\shader.h"
#include "Model Loading\\mesh.h"
#include "Model Loading\\texture.h"
#include "Model Loading\\meshLoaderObj.h"

#include <cmath>
#include <cstdlib>

// ===================== Helpers =====================
float randRange(float a, float b)
{
    return a + (b - a) * (rand() / (float)RAND_MAX);
}

bool pointInAABB(const glm::vec3& p, const glm::vec3& center, const glm::vec3& halfSize)
{
    return (std::abs(p.x - center.x) <= halfSize.x) &&
        (std::abs(p.y - center.y) <= halfSize.y) &&
        (std::abs(p.z - center.z) <= halfSize.z);
}

// mirror a position across the cave origin on X axis (left/right mirror)
glm::vec3 mirrorXAroundOrigin(const glm::vec3& pos, const glm::vec3& origin)
{
    glm::vec3 d = pos - origin;
    d.x = -d.x;
    return origin + d;
}

// ===================== Global State =====================
void processKeyboardInput();

float deltaTime = 0.0f;
float lastFrame = 0.0f;

Window window("Game Engine", 800, 800);
Camera camera;

glm::vec3 lightColor = glm::vec3(1.0f);
glm::vec3 lightPos = glm::vec3(-180.0f, 100.0f, -200.0f);

// Zones: 0 = main room, 1 = cave1, 2 = cave2
int currentZone = 0;

// Teleport cooldown
static float lastTeleportTime = -1000.0f;

// Where we return after coin pickup (cave1 + cave2)
glm::vec3 mainSpawnPos(0.0f, 0.0f, 100.0f);

// ===================== Main Room Stuff =====================
bool chestShaking = true;
glm::vec3 chestBasePos(0.0f, -18.0f, -140.0f);

glm::vec3 portalLeftPos(-100.0f, -5.0f, -80.0f);
glm::vec3 portalRightPos(100.0f, -5.0f, -80.0f);
glm::vec3 portalHalfSize(25.0f, 50.0f, 25.0f);

glm::vec3 caveSpawnLeft(0.0f, 0.0f, -500.0f);
glm::vec3 caveSpawnRight(50.0f, 0.0f, -500.0f);

// ===================== Cave Floor Settings (USED IN CAVE 1 and 2) =====================
float caveFloorY = -20.0f;
float caveFloorSize = 900.0f;     // make bigger/smaller
float caveFloorCenterZ = -200.0f; // push plane forward so you don't see too much behind you

// ===================== Cave 1 Stuff (SIMPLE) =====================
glm::vec3 cave1Origin;
glm::vec3 cave1ObstaclePos;
glm::vec3 cave1ObstacleTarget;
float     cave1NextTargetTime = 0.0f;

glm::vec3 cave1CoinBase;
bool      cave1CoinVisible = true;

// ===================== Cave 2 Stuff (SIMPLE + 2 MIRROR ENEMIES + COIN) =====================
glm::vec3 cave2Origin;

// Enemy A (random mover)
glm::vec3 cave2EnemyA_Pos;
glm::vec3 cave2EnemyA_Target;
float     cave2EnemyA_NextTargetTime = 0.0f;

// Enemy B is MIRROR of A each frame
glm::vec3 cave2EnemyB_Pos;

// Cave2 coin
glm::vec3 cave2CoinBase;
bool      cave2CoinVisible = true;

int main()
{
    glClearColor(0.2f, 0.8f, 1.0f, 1.0f);

    Shader shader("Shaders/vertex_shader.glsl", "Shaders/fragment_shader.glsl");
    Shader sunShader("Shaders/sun_vertex_shader.glsl", "Shaders/sun_fragment_shader.glsl");

    // Textures
    GLuint texWood = loadBMP("Resources/Textures/wood.bmp");
    GLuint texRock = loadBMP("Resources/Textures/rock.bmp");
    GLuint texOrange = loadBMP("Resources/Textures/orange.bmp");

    // Coin texture (yellow) - fallback to orange if missing
    GLuint texYellow = loadBMP("Resources/Textures/orange.bmp");
    if (texYellow == 0)
        texYellow = texOrange;

    glEnable(GL_DEPTH_TEST);
    srand(42);

    MeshLoaderObj loader;
    Mesh sun = loader.loadObj("Resources/Models/sphere.obj");

    // wood cube
    std::vector<Texture> woodTex(1);
    woodTex[0].id = texWood;
    woodTex[0].type = "texture_diffuse";
    Mesh box = loader.loadObj("Resources/Models/cube.obj", woodTex);

    // MAIN plane (orange)
    std::vector<Texture> orangeTex(1);
    orangeTex[0].id = texOrange;
    orangeTex[0].type = "texture_diffuse";
    Mesh mainPlane = loader.loadObj("Resources/Models/plane.obj", orangeTex);

    // rock cube
    std::vector<Texture> rockTex(1);
    rockTex[0].id = texRock;
    rockTex[0].type = "texture_diffuse";
    Mesh rockBox = loader.loadObj("Resources/Models/cube.obj", rockTex);

    // CAVE plane (rock texture)
    Mesh cavePlane = loader.loadObj("Resources/Models/plane.obj", rockTex);

    // yellow coin cube
    std::vector<Texture> yellowTex(1);
    yellowTex[0].id = texYellow;
    yellowTex[0].type = "texture_diffuse";
    Mesh coinBox = loader.loadObj("Resources/Models/cube.obj", yellowTex);

    // E edge detection (ONE place)
    bool ePrevDown = false;

    while (!window.isPressed(GLFW_KEY_ESCAPE) &&
        glfwWindowShouldClose(window.getWindow()) == 0)
    {
        window.clear();

        float currentFrame = glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        processKeyboardInput();

        // ----- E edge detection -----
        bool eDown = window.isPressed(GLFW_KEY_E);
        bool eJustPressed = (eDown && !ePrevDown);
        ePrevDown = eDown;

        float now = glfwGetTime();

        // ===================== TELEPORT via portals (only when in MAIN) =====================
        if (currentZone == 0 && (now - lastTeleportTime > 0.8f))
        {
            glm::vec3 camPos = camera.getCameraPosition();

            // LEFT portal -> Cave 1
            if (pointInAABB(camPos, portalLeftPos, portalHalfSize))
            {
                currentZone = 1;
                lastTeleportTime = now;

                cave1Origin = caveSpawnLeft;

                camera.setCameraPosition(cave1Origin + glm::vec3(0.0f, 0.0f, 0.0f));

                cave1ObstaclePos = cave1Origin + glm::vec3(0.0f, -10.0f, -220.0f);
                cave1ObstacleTarget = cave1ObstaclePos;
                cave1NextTargetTime = glfwGetTime() + 0.3f;

                cave1CoinBase = cave1Origin + glm::vec3(0.0f, 10.0f, -120.0f);
                cave1CoinVisible = true;
            }
            // RIGHT portal -> Cave 2
            else if (pointInAABB(camPos, portalRightPos, portalHalfSize))
            {
                currentZone = 2;
                lastTeleportTime = now;

                cave2Origin = caveSpawnRight;

                // spawn player
                camera.setCameraPosition(cave2Origin + glm::vec3(0.0f, 0.0f, 0.0f));

                // Enemy A starts on +X side, farther
                cave2EnemyA_Pos = cave2Origin + glm::vec3(120.0f, -10.0f, -220.0f);
                cave2EnemyA_Target = cave2EnemyA_Pos;
                cave2EnemyA_NextTargetTime = glfwGetTime() + 0.3f;

                // Enemy B mirrors A
                cave2EnemyB_Pos = mirrorXAroundOrigin(cave2EnemyA_Pos, cave2Origin);

                // Coin in cave2
                cave2CoinBase = cave2Origin + glm::vec3(0.0f, 10.0f, -120.0f);
                cave2CoinVisible = true;
            }
        }

        // ===================== MAIN ROOM interactions (only zone 0) =====================
        if (currentZone == 0)
        {
            // Stop chest shaking with E near chest
            if (eJustPressed)
            {
                float distToChest = glm::length(camera.getCameraPosition() - chestBasePos);
                if (distToChest < 200.0f)
                    chestShaking = false;
            }
        }

        // ===================== CAVE 1 LOGIC (enemy + coin) =====================
        if (currentZone == 1)
        {
            float nowT = glfwGetTime();

            if (nowT > cave1NextTargetTime)
            {
                float rangeX = 260.0f;
                float rangeZ = 260.0f;

                glm::vec3 base = cave1Origin + glm::vec3(0.0f, -10.0f, -180.0f);

                cave1ObstacleTarget = base + glm::vec3(
                    randRange(-rangeX, rangeX),
                    0.0f,
                    randRange(-rangeZ, rangeZ)
                );

                cave1NextTargetTime = nowT + randRange(0.8f, 1.4f);
            }

            glm::vec3 dir = cave1ObstacleTarget - cave1ObstaclePos;
            float len = glm::length(dir);
            if (len > 0.05f)
            {
                dir /= len;
                float speed = 130.0f;
                cave1ObstaclePos += dir * speed * deltaTime;
            }

            glm::vec3 enemyHalf(6.0f, 8.0f, 6.0f);
            if (pointInAABB(camera.getCameraPosition(), cave1ObstaclePos, enemyHalf))
            {
                camera.setCameraPosition(cave1Origin + glm::vec3(0.0f, 0.0f, 0.0f));
            }

            // Coin pickup with E (use SAME float position as rendering)
            if (cave1CoinVisible && eJustPressed)
            {
                float tCoin = glfwGetTime();
                float floatY = std::sin(tCoin * 2.2f) * 3.0f;
                glm::vec3 coinPosNow = cave1CoinBase + glm::vec3(0.0f, floatY, 0.0f);

                float distToCoin = glm::length(camera.getCameraPosition() - coinPosNow);

                if (distToCoin < 35.0f)
                {
                    cave1CoinVisible = false;

                    // Return to main immediately
                    currentZone = 0;
                    camera.setCameraPosition(mainSpawnPos);
                    lastTeleportTime = glfwGetTime();
                }
            }
        }

        // ===================== CAVE 2 LOGIC (two mirrored enemies + coin) =====================
        if (currentZone == 2)
        {
            float nowT = glfwGetTime();

            // Choose a new target for Enemy A
            if (nowT > cave2EnemyA_NextTargetTime)
            {
                float rangeX = 280.0f;
                float rangeZ = 280.0f;

                glm::vec3 base = cave2Origin + glm::vec3(0.0f, -10.0f, -180.0f);

                cave2EnemyA_Target = base + glm::vec3(
                    randRange(0.0f, rangeX),        // keep A on +X half
                    0.0f,
                    randRange(-rangeZ, rangeZ)
                );

                cave2EnemyA_NextTargetTime = nowT + randRange(0.6f, 1.1f);
            }

            // Move Enemy A (FASTER)
            glm::vec3 dirA = cave2EnemyA_Target - cave2EnemyA_Pos;
            float lenA = glm::length(dirA);
            if (lenA > 0.05f)
            {
                dirA /= lenA;
                float speedA = 190.0f;
                cave2EnemyA_Pos += dirA * speedA * deltaTime;
            }

            // Enemy B mirrors A position
            cave2EnemyB_Pos = mirrorXAroundOrigin(cave2EnemyA_Pos, cave2Origin);

            // Collision with either enemy -> reset to cave2 spawn
            glm::vec3 enemyHalf(6.0f, 8.0f, 6.0f);
            glm::vec3 camPos = camera.getCameraPosition();

            if (pointInAABB(camPos, cave2EnemyA_Pos, enemyHalf) ||
                pointInAABB(camPos, cave2EnemyB_Pos, enemyHalf))
            {
                camera.setCameraPosition(cave2Origin + glm::vec3(0.0f, 0.0f, 0.0f));
            }

            // Coin pickup with E (same logic as cave1)
            if (cave2CoinVisible && eJustPressed)
            {
                float tCoin = glfwGetTime();
                float floatY = std::sin(tCoin * 2.2f) * 3.0f;
                glm::vec3 coinPosNow = cave2CoinBase + glm::vec3(0.0f, floatY, 0.0f);

                float distToCoin = glm::length(camera.getCameraPosition() - coinPosNow);

                if (distToCoin < 35.0f)
                {
                    cave2CoinVisible = false;

                    // Return to main immediately
                    currentZone = 0;
                    camera.setCameraPosition(mainSpawnPos);
                    lastTeleportTime = glfwGetTime();
                }
            }
        }

        // ===================== Render setup =====================
        sunShader.use();

        glm::mat4 ProjectionMatrix =
            glm::perspective(90.0f,
                window.getWidth() * 1.0f / window.getHeight(),
                0.1f, 10000.0f);

        glm::mat4 ViewMatrix =
            glm::lookAt(camera.getCameraPosition(),
                camera.getCameraPosition() + camera.getCameraViewDirection(),
                camera.getCameraUp());

        GLuint MatrixID = glGetUniformLocation(sunShader.getId(), "MVP");

        // light sphere
        glm::mat4 ModelMatrix = glm::mat4(1.0f);
        ModelMatrix = glm::translate(ModelMatrix, lightPos);
        glm::mat4 MVP = ProjectionMatrix * ViewMatrix * ModelMatrix;
        glUniformMatrix4fv(MatrixID, 1, GL_FALSE, &MVP[0][0]);
        sun.draw(sunShader);

        shader.use();

        GLuint MatrixID2 = glGetUniformLocation(shader.getId(), "MVP");
        GLuint ModelMatrixID = glGetUniformLocation(shader.getId(), "model");

        glUniform3f(glGetUniformLocation(shader.getId(), "lightColor"),
            lightColor.x, lightColor.y, lightColor.z);
        glUniform3f(glGetUniformLocation(shader.getId(), "lightPos"),
            lightPos.x, lightPos.y, lightPos.z);
        glUniform3f(glGetUniformLocation(shader.getId(), "viewPos"),
            camera.getCameraPosition().x,
            camera.getCameraPosition().y,
            camera.getCameraPosition().z);

        // ===================== DRAW MAIN ROOM (zone 0) =====================
        if (currentZone == 0)
        {
            // plane (bigger orange terrain)
            ModelMatrix = glm::mat4(1.0f);
            ModelMatrix = glm::translate(ModelMatrix, glm::vec3(0.0f, -20.0f, 0.0f));
            ModelMatrix = glm::scale(ModelMatrix, glm::vec3(2000.0f, 1.0f, 2000.0f));
            MVP = ProjectionMatrix * ViewMatrix * ModelMatrix;
            glUniformMatrix4fv(MatrixID2, 1, GL_FALSE, &MVP[0][0]);
            glUniformMatrix4fv(ModelMatrixID, 1, GL_FALSE, &ModelMatrix[0][0]);
            mainPlane.draw(shader);

            // portals
            ModelMatrix = glm::translate(glm::mat4(1.0f), portalLeftPos);
            ModelMatrix = glm::scale(ModelMatrix, glm::vec3(8.0f, 18.0f, 0.5f));
            MVP = ProjectionMatrix * ViewMatrix * ModelMatrix;
            glUniformMatrix4fv(MatrixID2, 1, GL_FALSE, &MVP[0][0]);
            glUniformMatrix4fv(ModelMatrixID, 1, GL_FALSE, &ModelMatrix[0][0]);
            box.draw(shader);

            ModelMatrix = glm::translate(glm::mat4(1.0f), portalRightPos);
            ModelMatrix = glm::scale(ModelMatrix, glm::vec3(8.0f, 18.0f, 0.5f));
            MVP = ProjectionMatrix * ViewMatrix * ModelMatrix;
            glUniformMatrix4fv(MatrixID2, 1, GL_FALSE, &MVP[0][0]);
            glUniformMatrix4fv(ModelMatrixID, 1, GL_FALSE, &ModelMatrix[0][0]);
            box.draw(shader);

            // ===== 8 Columns: 4 on LEFT, 4 on RIGHT =====
            float colXLeft = -180.0f;
            float colXRight = 180.0f;
            float colY = -5.0f;

            float zStart = -40.0f;
            float zStep = 120.0f;

            float z0 = zStart + 0 * -zStep;
            float z1 = zStart + 1 * -zStep;
            float z2 = zStart + 2 * -zStep;
            float z3 = zStart + 3 * -zStep;

            glm::vec3 colScale(12.0f, 45.0f, 12.0f);

            auto drawColumn = [&](float x, float z)
                {
                    ModelMatrix = glm::translate(glm::mat4(1.0f), glm::vec3(x, colY, z));
                    ModelMatrix = glm::scale(ModelMatrix, colScale);

                    MVP = ProjectionMatrix * ViewMatrix * ModelMatrix;
                    glUniformMatrix4fv(MatrixID2, 1, GL_FALSE, &MVP[0][0]);
                    glUniformMatrix4fv(ModelMatrixID, 1, GL_FALSE, &ModelMatrix[0][0]);

                    box.draw(shader);
                };

            drawColumn(colXLeft, z0);
            drawColumn(colXLeft, z1);
            drawColumn(colXLeft, z2);
            drawColumn(colXLeft, z3);

            drawColumn(colXRight, z0);
            drawColumn(colXRight, z1);
            drawColumn(colXRight, z2);
            drawColumn(colXRight, z3);

            // rocks
            auto drawRock = [&](glm::vec3 pos, glm::vec3 scale)
                {
                    ModelMatrix = glm::translate(glm::mat4(1.0f), pos);
                    ModelMatrix = glm::scale(ModelMatrix, scale);

                    MVP = ProjectionMatrix * ViewMatrix * ModelMatrix;
                    glUniformMatrix4fv(MatrixID2, 1, GL_FALSE, &MVP[0][0]);
                    glUniformMatrix4fv(ModelMatrixID, 1, GL_FALSE, &ModelMatrix[0][0]);

                    rockBox.draw(shader);
                };

            drawRock(glm::vec3(-40.0f, -18.0f, -60.0f), glm::vec3(6.0f, 4.0f, 6.0f));
            drawRock(glm::vec3(20.0f, -18.5f, -90.0f), glm::vec3(4.0f, 3.0f, 5.0f));
            drawRock(glm::vec3(55.0f, -18.0f, -70.0f), glm::vec3(5.0f, 4.0f, 4.0f));
            drawRock(glm::vec3(-75.0f, -18.5f, -110.0f), glm::vec3(4.0f, 2.5f, 4.0f));
            drawRock(glm::vec3(90.0f, -18.0f, -130.0f), glm::vec3(7.0f, 3.5f, 5.0f));

            // chest
            float t = glfwGetTime();
            glm::vec3 chestScale(12.0f, 8.0f, 8.0f);

            glm::vec3 shakeOffset(0.0f);
            if (chestShaking)
            {
                float shakeSpeed = 8.0f;
                shakeOffset = glm::vec3(
                    1.2f * std::sin(t * shakeSpeed),
                    0.4f * std::sin(t * shakeSpeed * 1.7f),
                    0.8f * std::cos(t * shakeSpeed)
                );
            }

            ModelMatrix = glm::translate(glm::mat4(1.0f), chestBasePos + shakeOffset);
            ModelMatrix = glm::scale(ModelMatrix, chestScale);

            MVP = ProjectionMatrix * ViewMatrix * ModelMatrix;
            glUniformMatrix4fv(MatrixID2, 1, GL_FALSE, &MVP[0][0]);
            glUniformMatrix4fv(ModelMatrixID, 1, GL_FALSE, &ModelMatrix[0][0]);
            box.draw(shader);
        }

        // ===================== DRAW CAVE 1 (zone 1) =====================
        if (currentZone == 1)
        {
            // Cave plane
            ModelMatrix = glm::mat4(1.0f);
            ModelMatrix = glm::translate(ModelMatrix, cave1Origin + glm::vec3(0.0f, caveFloorY, caveFloorCenterZ));
            ModelMatrix = glm::scale(ModelMatrix, glm::vec3(caveFloorSize, 1.0f, caveFloorSize));
            MVP = ProjectionMatrix * ViewMatrix * ModelMatrix;
            glUniformMatrix4fv(MatrixID2, 1, GL_FALSE, &MVP[0][0]);
            glUniformMatrix4fv(ModelMatrixID, 1, GL_FALSE, &ModelMatrix[0][0]);
            cavePlane.draw(shader);

            // Enemy
            ModelMatrix = glm::translate(glm::mat4(1.0f), cave1ObstaclePos);
            ModelMatrix = glm::scale(ModelMatrix, glm::vec3(18.0f, 18.0f, 18.0f));
            MVP = ProjectionMatrix * ViewMatrix * ModelMatrix;
            glUniformMatrix4fv(MatrixID2, 1, GL_FALSE, &MVP[0][0]);
            glUniformMatrix4fv(ModelMatrixID, 1, GL_FALSE, &ModelMatrix[0][0]);
            rockBox.draw(shader);

            // Coin
            if (cave1CoinVisible)
            {
                float t = glfwGetTime();
                float floatY = std::sin(t * 2.2f) * 3.0f;

                ModelMatrix = glm::translate(glm::mat4(1.0f), cave1CoinBase + glm::vec3(0.0f, floatY, 0.0f));
                ModelMatrix = glm::rotate(ModelMatrix, t * 4.0f, glm::vec3(0, 1, 0));
                ModelMatrix = glm::scale(ModelMatrix, glm::vec3(4.0f, 4.0f, 0.8f));

                MVP = ProjectionMatrix * ViewMatrix * ModelMatrix;
                glUniformMatrix4fv(MatrixID2, 1, GL_FALSE, &MVP[0][0]);
                glUniformMatrix4fv(ModelMatrixID, 1, GL_FALSE, &ModelMatrix[0][0]);
                coinBox.draw(shader);
            }
        }

        // ===================== DRAW CAVE 2 (zone 2) =====================
        if (currentZone == 2)
        {
            // Cave plane
            ModelMatrix = glm::mat4(1.0f);
            ModelMatrix = glm::translate(ModelMatrix, cave2Origin + glm::vec3(0.0f, caveFloorY, caveFloorCenterZ));
            ModelMatrix = glm::scale(ModelMatrix, glm::vec3(caveFloorSize, 1.0f, caveFloorSize));
            MVP = ProjectionMatrix * ViewMatrix * ModelMatrix;
            glUniformMatrix4fv(MatrixID2, 1, GL_FALSE, &MVP[0][0]);
            glUniformMatrix4fv(ModelMatrixID, 1, GL_FALSE, &ModelMatrix[0][0]);
            cavePlane.draw(shader);

            // Enemy A
            ModelMatrix = glm::translate(glm::mat4(1.0f), cave2EnemyA_Pos);
            ModelMatrix = glm::scale(ModelMatrix, glm::vec3(18.0f, 18.0f, 18.0f));
            MVP = ProjectionMatrix * ViewMatrix * ModelMatrix;
            glUniformMatrix4fv(MatrixID2, 1, GL_FALSE, &MVP[0][0]);
            glUniformMatrix4fv(ModelMatrixID, 1, GL_FALSE, &ModelMatrix[0][0]);
            rockBox.draw(shader);

            // Enemy B
            ModelMatrix = glm::translate(glm::mat4(1.0f), cave2EnemyB_Pos);
            ModelMatrix = glm::scale(ModelMatrix, glm::vec3(18.0f, 18.0f, 18.0f));
            MVP = ProjectionMatrix * ViewMatrix * ModelMatrix;
            glUniformMatrix4fv(MatrixID2, 1, GL_FALSE, &MVP[0][0]);
            glUniformMatrix4fv(ModelMatrixID, 1, GL_FALSE, &ModelMatrix[0][0]);
            rockBox.draw(shader);

            // Coin in cave2
            if (cave2CoinVisible)
            {
                float t = glfwGetTime();
                float floatY = std::sin(t * 2.2f) * 3.0f;

                ModelMatrix = glm::translate(glm::mat4(1.0f), cave2CoinBase + glm::vec3(0.0f, floatY, 0.0f));
                ModelMatrix = glm::rotate(ModelMatrix, t * 4.0f, glm::vec3(0, 1, 0));
                ModelMatrix = glm::scale(ModelMatrix, glm::vec3(4.0f, 4.0f, 0.8f));

                MVP = ProjectionMatrix * ViewMatrix * ModelMatrix;
                glUniformMatrix4fv(MatrixID2, 1, GL_FALSE, &MVP[0][0]);
                glUniformMatrix4fv(ModelMatrixID, 1, GL_FALSE, &ModelMatrix[0][0]);
                coinBox.draw(shader);
            }
        }

        window.update();
    }

    return 0;
}

void processKeyboardInput()
{
    float cameraSpeed = 30.0f * deltaTime;

    // translation
    if (window.isPressed(GLFW_KEY_W)) camera.keyboardMoveFront(cameraSpeed);
    if (window.isPressed(GLFW_KEY_S)) camera.keyboardMoveBack(cameraSpeed);
    if (window.isPressed(GLFW_KEY_A)) camera.keyboardMoveLeft(cameraSpeed);
    if (window.isPressed(GLFW_KEY_D)) camera.keyboardMoveRight(cameraSpeed);
    if (window.isPressed(GLFW_KEY_R)) camera.keyboardMoveUp(cameraSpeed);
    if (window.isPressed(GLFW_KEY_F)) camera.keyboardMoveDown(cameraSpeed);

    // rotation
    if (window.isPressed(GLFW_KEY_LEFT))  camera.rotateOy(cameraSpeed);
    if (window.isPressed(GLFW_KEY_RIGHT)) camera.rotateOy(-cameraSpeed);
    if (window.isPressed(GLFW_KEY_UP))    camera.rotateOx(cameraSpeed);
    if (window.isPressed(GLFW_KEY_DOWN))  camera.rotateOx(-cameraSpeed);
}
