// main.cpp (FULL FILE - main room + portals + columns + rocks + chest + caves + underwater fog)
//
// ✅ Modifications included:
// 1) Loads Shark.obj (which references Shark.mtl) once.
// 2) Replaces Cave 1 obstacle cube with the shark model.
// 3) Replaces Cave 2 enemy A and enemy B cubes with sharks.
// 4) Loads statue.obj (which references statue.mtl) once.
// 5) Replaces BOTH cave coins (cave1 + cave2) with the statue model (still floats + spins).
// 6) Adds detailed comments throughout the file to help you read + learn.
//
// ⚠️ IMPORTANT ABOUT FILE NAMES / CASE:
// You said your files are named exactly:
//   Shark.obj / Shark.mtl
//   statue.obj / statue.mtl
//
// - This code loads: "Resources/Models/Shark.obj" and "Resources/Models/statue.obj"
// - Inside each OBJ, the mtllib line MUST match exactly (same capitalization):
//     Shark.obj  -> mtllib Shark.mtl
//     statue.obj -> mtllib statue.mtl
// If the mtllib name mismatches, the model may load but appear untextured/white.

#include "Graphics\\window.h"
#include "Camera\\camera.h"
#include "Shaders\\shader.h"
#include "Model Loading\\mesh.h"
#include "Model Loading\\texture.h"
#include "Model Loading\\meshLoaderObj.h"

#include <cmath>
#include <cstdlib>

// ===================== Helpers =====================

// Returns a random float in [a, b]
float randRange(float a, float b)
{
    return a + (b - a) * (rand() / (float)RAND_MAX);
}

// Checks if point p is inside an axis-aligned bounding box (AABB)
// - center: box center
// - halfSize: half extents (half width/height/depth)
bool pointInAABB(const glm::vec3& p, const glm::vec3& center, const glm::vec3& halfSize)
{
    return (std::abs(p.x - center.x) <= halfSize.x) &&
        (std::abs(p.y - center.y) <= halfSize.y) &&
        (std::abs(p.z - center.z) <= halfSize.z);
}

// Mirrors a position across X relative to an origin
// Used to keep Cave 2 enemies symmetrical
glm::vec3 mirrorXAroundOrigin(const glm::vec3& pos, const glm::vec3& origin)
{
    glm::vec3 d = pos - origin;
    d.x = -d.x;
    return origin + d;
}

// ===================== Floor alignment helpers =====================

// Main room floor plane Y position
const float MAIN_FLOOR_Y = -20.0f;

// "Half thickness" concept used for aligning objects on top of the floor
const float MAIN_FLOOR_HALF_THICKNESS = 0.5f;

// Returns the top Y of the main floor
inline float mainFloorTopY()
{
    return MAIN_FLOOR_Y + MAIN_FLOOR_HALF_THICKNESS;
}

// ===================== Global State =====================

void processKeyboardInput();

float deltaTime = 0.0f;
float lastFrame = 0.0f;

Window window("Game Engine", 800, 800);
Camera camera;

// Light (your shader uses lightPos, lightColor, viewPos)
glm::vec3 lightColor = glm::vec3(1.0f);
glm::vec3 lightPos = glm::vec3(-180.0f, 100.0f, -200.0f);

// Zones:
// 0 = main room
// 1 = cave 1
// 2 = cave 2
int currentZone = 0;

// Teleport cooldown
static float lastTeleportTime = -1000.0f;

// Where we return after coin pickup
glm::vec3 mainSpawnPos(0.0f, 0.0f, 100.0f);

// ===================== Main Room Stuff =====================

bool chestShaking = true;
glm::vec3 chestBasePos(0.0f, -18.0f, -140.0f);

glm::vec3 portalLeftPos(-100.0f, 15.0f, -80.0f);
glm::vec3 portalRightPos(100.0f, 15.0f, -80.0f);
glm::vec3 portalHalfSize(25.0f, 50.0f, 25.0f);

// Spawn points for caves
glm::vec3 caveSpawnLeft(0.0f, 0.0f, -500.0f);
glm::vec3 caveSpawnRight(50.0f, 0.0f, -500.0f);

// ===================== Cave 1 Stuff =====================

glm::vec3 cave1Origin;
glm::vec3 cave1ObstaclePos;
glm::vec3 cave1ObstacleTarget;
float     cave1NextTargetTime = 0.0f;

glm::vec3 cave1CoinBase;
bool      cave1CoinVisible = true;

// Cave terrain
float caveFloorY = -20.0f;
float caveFloorSize = 1200.0f;

// ===================== Cave 2 Stuff =====================

glm::vec3 cave2Origin;

glm::vec3 cave2EnemyA_Pos;
glm::vec3 cave2EnemyA_Target;
float     cave2EnemyA_NextTargetTime = 0.0f;

glm::vec3 cave2EnemyB_Pos;

glm::vec3 cave2CoinBase;
bool      cave2CoinVisible = true;

int main()
{
    // Background color (blue-ish underwater)
    glClearColor(0.2f, 0.8f, 1.0f, 1.0f);

    // Main shader + special sun shader
    Shader shader("Shaders/vertex_shader.glsl", "Shaders/fragment_shader.glsl");
    Shader sunShader("Shaders/sun_vertex_shader.glsl", "Shaders/sun_fragment_shader.glsl");

    // ===================== Textures =====================

    GLuint texWood = loadBMP("Resources/Textures/wood.bmp");
    GLuint texRock = loadBMP("Resources/Textures/rock.bmp");
    GLuint texOrange = loadBMP("Resources/Textures/orange.bmp");             // still used by other things if needed
    GLuint texUnderSand = loadBMP("Resources/Textures/underwater_sand.bmp");

    GLuint texColumn = loadBMP("Resources/Textures/987.bmp");
    if (texColumn == 0) texColumn = texRock;

    // Depth test is essential for 3D visibility
    glEnable(GL_DEPTH_TEST);

    // Deterministic random values each run
    srand(42);

    MeshLoaderObj loader;

    // Light sphere model
    Mesh sun = loader.loadObj("Resources/Models/sphere.obj");

    // Wood cube (used for portals)
    std::vector<Texture> woodTex(1);
    woodTex[0].id = texWood;
    woodTex[0].type = "texture_diffuse";
    Mesh box = loader.loadObj("Resources/Models/cube.obj", woodTex);

    // Floor plane
    std::vector<Texture> planeTex(1);
    planeTex[0].id = texUnderSand;
    planeTex[0].type = "texture_diffuse";
    Mesh plane = loader.loadObj("Resources/Models/plane.obj", planeTex);

    // Rock cube (used for rocks in main room; enemies were replaced by shark)
    std::vector<Texture> rockTex(1);
    rockTex[0].id = texRock;
    rockTex[0].type = "texture_diffuse";
    Mesh rockBox = loader.loadObj("Resources/Models/cube.obj", rockTex);

    // Orange cube coin mesh (NO LONGER DRAWN as the cave coin; now we use statue.obj)
    std::vector<Texture> coinTex(1);
    coinTex[0].id = texOrange;
    coinTex[0].type = "texture_diffuse";
    Mesh coinBox = loader.loadObj("Resources/Models/cube.obj", coinTex);

    // ===== CHEST OBJ =====
    Mesh chestMesh = loader.loadObj("Resources/Models/chest.obj");
    const float CHEST_OBJ_SCALE = 20.0f;

    // ===== COLUMN OBJ =====
    std::vector<Texture> colTex(1);
    colTex[0].id = texColumn;
    colTex[0].type = "texture_diffuse";
    Mesh columnMesh = loader.loadObj("Resources/Models/column.obj", colTex);

    // Column tuning
    const float COLUMN_SCALE = 0.80f;
    const float COLUMN_BASE_Y_FIX = -300.0f;

    // ==========================================================
    // ✅ NEW: SHARK MODEL (replaces enemy cubes in cave 1 & 2)
    // ==========================================================
    Mesh sharkMesh = loader.loadObj("Resources/Models/Shark.obj");
    const float SHARK_OBJ_SCALE = 8.0f;  // adjust if shark too big/small
    const float SHARK_Y_LIFT = 0.0f;      // adjust if shark sinks into floor

    // ==========================================================
    // ✅ NEW: STATUE MODEL (replaces the coin in cave 1 & 2)
    // ==========================================================
    Mesh statueMesh = loader.loadObj("Resources/Models/statue.obj");

    // Statue tuning (OBJ scale varies a lot between models)
    const float STATUE_OBJ_SCALE = 0.5f;  // adjust to fit your cave
    const float STATUE_Y_LIFT = 0.0f;     // adjust if floating too low/high

    // E edge detection
    bool ePrevDown = false;

    // ===================== Game Loop =====================
    while (!window.isPressed(GLFW_KEY_ESCAPE) &&
        glfwWindowShouldClose(window.getWindow()) == 0)
    {
        window.clear();

        // ----- Time step -----
        float currentFrame = glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        // ----- Movement input -----
        processKeyboardInput();

        // ----- "E just pressed" detection -----
        bool eDown = window.isPressed(GLFW_KEY_E);
        bool eJustPressed = (eDown && !ePrevDown);
        ePrevDown = eDown;

        float now = glfwGetTime();

        // ==========================================================
        // TELEPORT via portals (only when in MAIN ROOM)
        // ==========================================================
        if (currentZone == 0 && (now - lastTeleportTime > 0.8f))
        {
            glm::vec3 camPos = camera.getCameraPosition();

            // Left portal -> cave 1
            if (pointInAABB(camPos, portalLeftPos, portalHalfSize))
            {
                currentZone = 1;
                lastTeleportTime = now;

                cave1Origin = caveSpawnLeft;
                camera.setCameraPosition(cave1Origin + glm::vec3(0.0f));

                cave1ObstaclePos = cave1Origin + glm::vec3(0.0f, -10.0f, -220.0f);
                cave1ObstacleTarget = cave1ObstaclePos;
                cave1NextTargetTime = glfwGetTime() + 0.3f;

                cave1CoinBase = cave1Origin + glm::vec3(0.0f, 10.0f, -120.0f);
                cave1CoinVisible = true;
            }
            // Right portal -> cave 2
            else if (pointInAABB(camPos, portalRightPos, portalHalfSize))
            {
                currentZone = 2;
                lastTeleportTime = now;

                cave2Origin = caveSpawnRight;
                camera.setCameraPosition(cave2Origin + glm::vec3(0.0f));

                cave2EnemyA_Pos = cave2Origin + glm::vec3(120.0f, -10.0f, -220.0f);
                cave2EnemyA_Target = cave2EnemyA_Pos;
                cave2EnemyA_NextTargetTime = glfwGetTime() + 0.3f;

                cave2EnemyB_Pos = mirrorXAroundOrigin(cave2EnemyA_Pos, cave2Origin);

                cave2CoinBase = cave2Origin + glm::vec3(0.0f, 10.0f, -120.0f);
                cave2CoinVisible = true;
            }
        }

        // ==========================================================
        // MAIN ROOM interaction: stop chest shaking if E pressed near it
        // ==========================================================
        if (currentZone == 0)
        {
            if (eJustPressed)
            {
                float distToChest = glm::length(camera.getCameraPosition() - chestBasePos);
                if (distToChest < 200.0f)
                    chestShaking = false;
            }
        }

        // ==========================================================
        // CAVE 1 LOGIC: enemy movement + collision + coin pickup
        // ==========================================================
        if (currentZone == 1)
        {
            float nowT = glfwGetTime();

            // Pick a new random target for the moving enemy occasionally
            if (nowT > cave1NextTargetTime)
            {
                float rangeX = 220.0f;
                float rangeZ = 220.0f;

                glm::vec3 base = cave1Origin + glm::vec3(0.0f, -10.0f, -180.0f);

                cave1ObstacleTarget = base + glm::vec3(
                    randRange(-rangeX, rangeX),
                    0.0f,
                    randRange(-rangeZ, rangeZ)
                );

                cave1NextTargetTime = nowT + randRange(0.8f, 1.4f);
            }

            // Move enemy toward target
            glm::vec3 dir = cave1ObstacleTarget - cave1ObstaclePos;
            float len = glm::length(dir);
            if (len > 0.05f)
            {
                dir /= len;
                float speed = 130.0f;
                cave1ObstaclePos += dir * speed * deltaTime;
            }

            // Collision check vs enemy AABB (still same size logic)
            glm::vec3 enemyHalf(6.0f, 8.0f, 6.0f);
            if (pointInAABB(camera.getCameraPosition(), cave1ObstaclePos, enemyHalf))
            {
                camera.setCameraPosition(cave1Origin + glm::vec3(0.0f));
            }

            // Coin pickup (now it's a statue visually, but same gameplay)
            if (cave1CoinVisible && eJustPressed)
            {
                float tCoin = glfwGetTime();
                float floatY = std::sin(tCoin * 2.2f) * 3.0f;
                glm::vec3 coinPosNow = cave1CoinBase + glm::vec3(0.0f, floatY, 0.0f);

                float distToCoin = glm::length(camera.getCameraPosition() - coinPosNow);
                if (distToCoin < 35.0f)
                {
                    cave1CoinVisible = false;
                    currentZone = 0;
                    camera.setCameraPosition(mainSpawnPos);
                    lastTeleportTime = glfwGetTime();
                }
            }
        }

        // ==========================================================
        // CAVE 2 LOGIC: enemy A movement + enemy B mirror + collision + coin pickup
        // ==========================================================
        if (currentZone == 2)
        {
            float nowT = glfwGetTime();

            if (nowT > cave2EnemyA_NextTargetTime)
            {
                float rangeX = 240.0f;
                float rangeZ = 240.0f;

                glm::vec3 base = cave2Origin + glm::vec3(0.0f, -10.0f, -180.0f);

                cave2EnemyA_Target = base + glm::vec3(
                    randRange(0.0f, rangeX),
                    0.0f,
                    randRange(-rangeZ, rangeZ)
                );

                cave2EnemyA_NextTargetTime = nowT + randRange(0.6f, 1.1f);
            }

            glm::vec3 dirA = cave2EnemyA_Target - cave2EnemyA_Pos;
            float lenA = glm::length(dirA);
            if (lenA > 0.05f)
            {
                dirA /= lenA;
                float speedA = 190.0f;
                cave2EnemyA_Pos += dirA * speedA * deltaTime;
            }

            // Mirror A into B
            cave2EnemyB_Pos = mirrorXAroundOrigin(cave2EnemyA_Pos, cave2Origin);

            // Collision check with either enemy
            glm::vec3 enemyHalf(6.0f, 8.0f, 6.0f);
            glm::vec3 camPos = camera.getCameraPosition();

            if (pointInAABB(camPos, cave2EnemyA_Pos, enemyHalf) ||
                pointInAABB(camPos, cave2EnemyB_Pos, enemyHalf))
            {
                camera.setCameraPosition(cave2Origin + glm::vec3(0.0f));
            }

            // Coin pickup (visual = statue)
            if (cave2CoinVisible && eJustPressed)
            {
                float tCoin = glfwGetTime();
                float floatY = std::sin(tCoin * 2.2f) * 3.0f;
                glm::vec3 coinPosNow = cave2CoinBase + glm::vec3(0.0f, floatY, 0.0f);

                float distToCoin = glm::length(camera.getCameraPosition() - coinPosNow);
                if (distToCoin < 35.0f)
                {
                    cave2CoinVisible = false;
                    currentZone = 0;
                    camera.setCameraPosition(mainSpawnPos);
                    lastTeleportTime = glfwGetTime();
                }
            }
        }

        // ===================== Render setup =====================

        // ---- Sun pass ----
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

        glm::mat4 ModelMatrix = glm::mat4(1.0f);
        glm::mat4 MVP = glm::mat4(1.0f);

        // Draw light sphere where the light is
        ModelMatrix = glm::mat4(1.0f);
        ModelMatrix = glm::translate(ModelMatrix, lightPos);
        MVP = ProjectionMatrix * ViewMatrix * ModelMatrix;
        glUniformMatrix4fv(MatrixID, 1, GL_FALSE, &MVP[0][0]);
        sun.draw(sunShader);

        // ---- Main pass ----
        shader.use();

        GLuint MatrixID2 = glGetUniformLocation(shader.getId(), "MVP");
        GLuint ModelMatrixID = glGetUniformLocation(shader.getId(), "model");

        // Top light follows player (like a "headlamp")
        lightPos = camera.getCameraPosition() + glm::vec3(0.0f, 350.0f, 0.0f);
        lightColor = glm::vec3(0.8f, 0.8f, 0.8f);

        glUniform3f(glGetUniformLocation(shader.getId(), "lightColor"),
            lightColor.x, lightColor.y, lightColor.z);
        glUniform3f(glGetUniformLocation(shader.getId(), "lightPos"),
            lightPos.x, lightPos.y, lightPos.z);
        glUniform3f(glGetUniformLocation(shader.getId(), "viewPos"),
            camera.getCameraPosition().x,
            camera.getCameraPosition().y,
            camera.getCameraPosition().z);

        // Underwater fog
        glUniform1i(glGetUniformLocation(shader.getId(), "uUseFog"), 1);
        glUniform3f(glGetUniformLocation(shader.getId(), "uFogColor"), 0.05f, 0.35f, 0.55f);
        glUniform1f(glGetUniformLocation(shader.getId(), "uFogNear"), 40.0f);
        glUniform1f(glGetUniformLocation(shader.getId(), "uFogFar"), 420.0f);

        // Needed by some fog implementations
        glUniform1f(glGetUniformLocation(shader.getId(), "uNear"), 0.1f);
        glUniform1f(glGetUniformLocation(shader.getId(), "uFar"), 10000.0f);

        // Default: Not rendering portals
        glUniform1i(glGetUniformLocation(shader.getId(), "uIsPortal"), 0);
        glUniform1f(glGetUniformLocation(shader.getId(), "uPortalAlpha"), 1.0f);

        // ===================== DRAW MAIN ROOM =====================
        if (currentZone == 0)
        {
            // ---- floor ----
            ModelMatrix = glm::mat4(1.0f);
            ModelMatrix = glm::translate(ModelMatrix, glm::vec3(0.0f, MAIN_FLOOR_Y, 0.0f));
            ModelMatrix = glm::scale(ModelMatrix, glm::vec3(2000.0f, 1.0f, 2000.0f));
            MVP = ProjectionMatrix * ViewMatrix * ModelMatrix;
            glUniformMatrix4fv(MatrixID2, 1, GL_FALSE, &MVP[0][0]);
            glUniformMatrix4fv(ModelMatrixID, 1, GL_FALSE, &ModelMatrix[0][0]);
            plane.draw(shader);

            // ---- portals (transparent) ----
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

            glUniform1i(glGetUniformLocation(shader.getId(), "uIsPortal"), 1);
            glUniform1f(glGetUniformLocation(shader.getId(), "uPortalAlpha"), 0.35f);

            // left portal
            ModelMatrix = glm::translate(glm::mat4(1.0f), portalLeftPos);
            ModelMatrix = glm::scale(ModelMatrix, glm::vec3(8.0f, 18.0f, 0.5f));
            MVP = ProjectionMatrix * ViewMatrix * ModelMatrix;
            glUniformMatrix4fv(MatrixID2, 1, GL_FALSE, &MVP[0][0]);
            glUniformMatrix4fv(ModelMatrixID, 1, GL_FALSE, &ModelMatrix[0][0]);
            box.draw(shader);

            // right portal
            ModelMatrix = glm::translate(glm::mat4(1.0f), portalRightPos);
            ModelMatrix = glm::scale(ModelMatrix, glm::vec3(8.0f, 18.0f, 0.5f));
            MVP = ProjectionMatrix * ViewMatrix * ModelMatrix;
            glUniformMatrix4fv(MatrixID2, 1, GL_FALSE, &MVP[0][0]);
            glUniformMatrix4fv(ModelMatrixID, 1, GL_FALSE, &ModelMatrix[0][0]);
            box.draw(shader);

            // Back to normal drawing
            glUniform1i(glGetUniformLocation(shader.getId(), "uIsPortal"), 0);
            glUniform1f(glGetUniformLocation(shader.getId(), "uPortalAlpha"), 1.0f);
            glDisable(GL_BLEND);

            // ---- columns ----
            const int   NUM_COLS_PER_SIDE = 8;
            const float COL_X_LEFT = -260.0f;
            const float COL_X_RIGHT = 260.0f;
            const float COL_Z_START = -60.0f;
            const float COL_Z_STEP = 120.0f;

            // Helper lambda to draw a column at a position
            auto drawColumnOBJ = [&](float x, float z, bool isLeft)
                {
                    glm::mat4 M = glm::mat4(1.0f);

                    const float COLUMN_WORLD_LIFT = 240.0f;
                    M = glm::translate(M, glm::vec3(x, mainFloorTopY() + COLUMN_WORLD_LIFT, z));

                    float yaw = isLeft ? 222290.0f : -222290.0f;
                    M = glm::rotate(M, glm::radians(yaw), glm::vec3(0, 1, 0));

                    M = glm::scale(M, glm::vec3(COLUMN_SCALE));
                    M = glm::translate(M, glm::vec3(0.0f, COLUMN_BASE_Y_FIX, 0.0f));

                    glm::mat4 mvp = ProjectionMatrix * ViewMatrix * M;
                    glUniformMatrix4fv(MatrixID2, 1, GL_FALSE, &mvp[0][0]);
                    glUniformMatrix4fv(ModelMatrixID, 1, GL_FALSE, &M[0][0]);

                    columnMesh.draw(shader);
                };

            for (int i = 0; i < NUM_COLS_PER_SIDE; i++)
            {
                float z = COL_Z_START - i * COL_Z_STEP;
                drawColumnOBJ(COL_X_LEFT, z, true);
                drawColumnOBJ(COL_X_RIGHT, z, false);
            }

            // ---- rocks aligned to floor ----
            auto drawRock = [&](glm::vec3 pos, glm::vec3 scale)
                {
                    // Align bottom of cube to floor
                    pos.y = mainFloorTopY() + scale.y * 0.5f - 5.0f;

                    glm::mat4 M = glm::translate(glm::mat4(1.0f), pos);
                    M = glm::scale(M, scale);

                    glm::mat4 mvp = ProjectionMatrix * ViewMatrix * M;
                    glUniformMatrix4fv(MatrixID2, 1, GL_FALSE, &mvp[0][0]);
                    glUniformMatrix4fv(ModelMatrixID, 1, GL_FALSE, &M[0][0]);

                    rockBox.draw(shader);
                };

            drawRock(glm::vec3(-40.0f, 0.0f, -60.0f), glm::vec3(6.0f, 4.0f, 6.0f));
            drawRock(glm::vec3(20.0f, 0.0f, -90.0f), glm::vec3(4.0f, 3.0f, 5.0f));
            drawRock(glm::vec3(55.0f, 0.0f, -70.0f), glm::vec3(5.0f, 4.0f, 4.0f));
            drawRock(glm::vec3(-75.0f, 0.0f, -110.0f), glm::vec3(4.0f, 2.5f, 4.0f));
            drawRock(glm::vec3(90.0f, 0.0f, -130.0f), glm::vec3(7.0f, 3.5f, 5.0f));

            // ---- chest aligned to floor ----
            float t = glfwGetTime();
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

            glm::vec3 chestPos = chestBasePos;
            chestPos.y = mainFloorTopY();

            ModelMatrix = glm::mat4(1.0f);
            ModelMatrix = glm::translate(ModelMatrix, chestPos + shakeOffset);
            ModelMatrix = glm::scale(ModelMatrix, glm::vec3(CHEST_OBJ_SCALE));

            MVP = ProjectionMatrix * ViewMatrix * ModelMatrix;
            glUniformMatrix4fv(MatrixID2, 1, GL_FALSE, &MVP[0][0]);
            glUniformMatrix4fv(ModelMatrixID, 1, GL_FALSE, &ModelMatrix[0][0]);
            chestMesh.draw(shader);
        }

        // ===================== DRAW CAVE 1 =====================
        if (currentZone == 1)
        {
            // ---- cave floor ----
            ModelMatrix = glm::mat4(1.0f);
            ModelMatrix = glm::translate(ModelMatrix, cave1Origin + glm::vec3(0.0f, caveFloorY, 0.0f));
            ModelMatrix = glm::scale(ModelMatrix, glm::vec3(caveFloorSize, 1.0f, caveFloorSize));
            MVP = ProjectionMatrix * ViewMatrix * ModelMatrix;
            glUniformMatrix4fv(MatrixID2, 1, GL_FALSE, &MVP[0][0]);
            glUniformMatrix4fv(ModelMatrixID, 1, GL_FALSE, &ModelMatrix[0][0]);
            plane.draw(shader);

            // ---- Cave 1 enemy (Shark) ----
            ModelMatrix = glm::translate(glm::mat4(1.0f), cave1ObstaclePos + glm::vec3(0.0f, SHARK_Y_LIFT, 0.0f));
            ModelMatrix = glm::rotate(ModelMatrix, glm::radians(90.0f), glm::vec3(0, 1, 0)); // tweak if needed
            ModelMatrix = glm::scale(ModelMatrix, glm::vec3(SHARK_OBJ_SCALE));
            MVP = ProjectionMatrix * ViewMatrix * ModelMatrix;
            glUniformMatrix4fv(MatrixID2, 1, GL_FALSE, &MVP[0][0]);
            glUniformMatrix4fv(ModelMatrixID, 1, GL_FALSE, &ModelMatrix[0][0]);
            sharkMesh.draw(shader);

            // ---- Cave 1 coin (Statue) ----
            if (cave1CoinVisible)
            {
                float t2 = glfwGetTime();
                float floatY = std::sin(t2 * 2.2f) * 3.0f;

                // Position: base + float animation
                ModelMatrix = glm::translate(glm::mat4(1.0f),
                    cave1CoinBase + glm::vec3(0.0f, floatY + STATUE_Y_LIFT, 0.0f));

                // Spin the statue like a collectible
                ModelMatrix = glm::rotate(ModelMatrix, t2 * 1.6f, glm::vec3(0, 1, 0));

                // Scale it to a reasonable size
                ModelMatrix = glm::scale(ModelMatrix, glm::vec3(STATUE_OBJ_SCALE));

                MVP = ProjectionMatrix * ViewMatrix * ModelMatrix;
                glUniformMatrix4fv(MatrixID2, 1, GL_FALSE, &MVP[0][0]);
                glUniformMatrix4fv(ModelMatrixID, 1, GL_FALSE, &ModelMatrix[0][0]);
                statueMesh.draw(shader);
            }
        }

        // ===================== DRAW CAVE 2 =====================
        if (currentZone == 2)
        {
            // ---- cave floor ----
            ModelMatrix = glm::mat4(1.0f);
            ModelMatrix = glm::translate(ModelMatrix, cave2Origin + glm::vec3(0.0f, caveFloorY, 0.0f));
            ModelMatrix = glm::scale(ModelMatrix, glm::vec3(caveFloorSize, 1.0f, caveFloorSize));
            MVP = ProjectionMatrix * ViewMatrix * ModelMatrix;
            glUniformMatrix4fv(MatrixID2, 1, GL_FALSE, &MVP[0][0]);
            glUniformMatrix4fv(ModelMatrixID, 1, GL_FALSE, &ModelMatrix[0][0]);
            plane.draw(shader);

            // ---- Cave 2 enemy A (Shark) ----
            ModelMatrix = glm::translate(glm::mat4(1.0f), cave2EnemyA_Pos + glm::vec3(0.0f, SHARK_Y_LIFT, 0.0f));
            ModelMatrix = glm::rotate(ModelMatrix, glm::radians(90.0f), glm::vec3(0, 1, 0));
            ModelMatrix = glm::scale(ModelMatrix, glm::vec3(SHARK_OBJ_SCALE));
            MVP = ProjectionMatrix * ViewMatrix * ModelMatrix;
            glUniformMatrix4fv(MatrixID2, 1, GL_FALSE, &MVP[0][0]);
            glUniformMatrix4fv(ModelMatrixID, 1, GL_FALSE, &ModelMatrix[0][0]);
            sharkMesh.draw(shader);

            // ---- Cave 2 enemy B (Shark) ----
            ModelMatrix = glm::translate(glm::mat4(1.0f), cave2EnemyB_Pos + glm::vec3(0.0f, SHARK_Y_LIFT, 0.0f));
            ModelMatrix = glm::rotate(ModelMatrix, glm::radians(-90.0f), glm::vec3(0, 1, 0));
            ModelMatrix = glm::scale(ModelMatrix, glm::vec3(SHARK_OBJ_SCALE));
            MVP = ProjectionMatrix * ViewMatrix * ModelMatrix;
            glUniformMatrix4fv(MatrixID2, 1, GL_FALSE, &MVP[0][0]);
            glUniformMatrix4fv(ModelMatrixID, 1, GL_FALSE, &ModelMatrix[0][0]);
            sharkMesh.draw(shader);

            // ---- Cave 2 coin (Statue) ----
            if (cave2CoinVisible)
            {
                float t2 = glfwGetTime();
                float floatY = std::sin(t2 * 2.2f) * 3.0f;

                ModelMatrix = glm::translate(glm::mat4(1.0f),
                    cave2CoinBase + glm::vec3(0.0f, floatY + STATUE_Y_LIFT, 0.0f));

                ModelMatrix = glm::rotate(ModelMatrix, t2 * 1.6f, glm::vec3(0, 1, 0));
                ModelMatrix = glm::scale(ModelMatrix, glm::vec3(STATUE_OBJ_SCALE));

                MVP = ProjectionMatrix * ViewMatrix * ModelMatrix;
                glUniformMatrix4fv(MatrixID2, 1, GL_FALSE, &MVP[0][0]);
                glUniformMatrix4fv(ModelMatrixID, 1, GL_FALSE, &ModelMatrix[0][0]);
                statueMesh.draw(shader);
            }
        }

        // Swap buffers, poll events
        window.update();
    }

    return 0;
}


// ==========================================================
// Reads keyboard input each frame and moves/rotates the camera.
// Uses deltaTime so movement speed is stable across FPS.
// ==========================================================
void processKeyboardInput()
{
    float cameraSpeed = 30.0f * deltaTime;

    // Movement
    if (window.isPressed(GLFW_KEY_W)) camera.keyboardMoveFront(cameraSpeed);
    if (window.isPressed(GLFW_KEY_S)) camera.keyboardMoveBack(cameraSpeed);
    if (window.isPressed(GLFW_KEY_A)) camera.keyboardMoveLeft(cameraSpeed);
    if (window.isPressed(GLFW_KEY_D)) camera.keyboardMoveRight(cameraSpeed);
    if (window.isPressed(GLFW_KEY_R)) camera.keyboardMoveUp(cameraSpeed);
    if (window.isPressed(GLFW_KEY_F)) camera.keyboardMoveDown(cameraSpeed);

    // Rotation (arrows)
    if (window.isPressed(GLFW_KEY_LEFT))  camera.rotateOy(cameraSpeed);
    if (window.isPressed(GLFW_KEY_RIGHT)) camera.rotateOy(-cameraSpeed);
    if (window.isPressed(GLFW_KEY_UP))    camera.rotateOx(cameraSpeed);
    if (window.isPressed(GLFW_KEY_DOWN))  camera.rotateOx(-cameraSpeed);
}
