// main.cpp (FULL FILE - main room + portals + columns + rocks + chest + caves + underwater fog)
//
// ✅ AABB portals + ✅ Octree shark collision
// ✅ Complex lighting (DirLight + PointLights + SpotLight) for the NEW fragment shader
// ✅ NEW: Quest GUI overlay (checkbox tasks + hidden part)
// ✅ FIX: Text works in CORE profile + NORMAL readable font via stb_truetype atlas (aPos+aUV + uFont)

#include "Graphics\\window.h"
#include "Camera\\camera.h"
#include "Shaders\\shader.h"
#include "Model Loading\\mesh.h"
#include "Model Loading\\texture.h"
#include "Model Loading\\meshLoaderObj.h"

// ✅ complex collision (Octree)
#include "Model Loading\\SharkCollisionOctree.h"

#include <glm.hpp>


#include <cmath>
#include <cstdlib>
#include <vector>
#include <string>
#include <fstream>
#include <cstdint>

// ===================== GUI font (TTF) =====================
// stb_truetype: bake TTF -> bitmap -> OpenGL texture atlas
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

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

glm::vec3 mirrorXAroundOrigin(const glm::vec3& pos, const glm::vec3& origin)
{
    glm::vec3 d = pos - origin;
    d.x = -d.x;
    return origin + d;
}

// ===================== Floor alignment helpers =====================

const float MAIN_FLOOR_Y = -20.0f;
const float MAIN_FLOOR_HALF_THICKNESS = 0.5f;

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

// Zones: 0 = main room, 1 = cave 1, 2 = cave 2
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

// ==========================================================
// ✅ Shark collision settings
// ==========================================================

static const float PLAYER_RADIUS_WORLD = 6.0f;
static float lastSharkHitTime = -1000.0f;
static const float SHARK_HIT_COOLDOWN = 0.8f;

// ==========================================================
// ✅ QUEST STATE (GUI tasks)
// ==========================================================

static bool qEnteredCave1 = false;
static bool qGotRelic1 = false;
static bool qEnteredCave2 = false;
static bool qGotRelic2 = false;
static bool qFoundHidden = false;

// ==========================================================
// ✅ Lighting helper setters for NEW fragment shader
// ==========================================================

static void setMaterialUniforms(Shader& shader, float ambient, float spec, float shininess)
{
    glUniform1f(glGetUniformLocation(shader.getId(), "uAmbientStrength"), ambient);
    glUniform1f(glGetUniformLocation(shader.getId(), "uSpecStrength"), spec);
    glUniform1f(glGetUniformLocation(shader.getId(), "uShininess"), shininess);
}

static void setDirLight(Shader& shader, bool enabled,
    const glm::vec3& dir, const glm::vec3& color, float intensity)
{
    glUniform1i(glGetUniformLocation(shader.getId(), "uUseDirLight"), enabled ? 1 : 0);
    if (!enabled) return;

    glUniform3f(glGetUniformLocation(shader.getId(), "uDirLight.direction"), dir.x, dir.y, dir.z);
    glUniform3f(glGetUniformLocation(shader.getId(), "uDirLight.color"), color.x, color.y, color.z);
    glUniform1f(glGetUniformLocation(shader.getId(), "uDirLight.intensity"), intensity);
}

static void setPointLight(Shader& shader, int idx,
    const glm::vec3& pos, const glm::vec3& color, float intensity,
    float constant, float linear, float quadratic)
{
    std::string base = "uPointLights[" + std::to_string(idx) + "].";

    glUniform3f(glGetUniformLocation(shader.getId(), (base + "position").c_str()), pos.x, pos.y, pos.z);
    glUniform3f(glGetUniformLocation(shader.getId(), (base + "color").c_str()), color.x, color.y, color.z);
    glUniform1f(glGetUniformLocation(shader.getId(), (base + "intensity").c_str()), intensity);

    glUniform1f(glGetUniformLocation(shader.getId(), (base + "constant").c_str()), constant);
    glUniform1f(glGetUniformLocation(shader.getId(), (base + "linear").c_str()), linear);
    glUniform1f(glGetUniformLocation(shader.getId(), (base + "quadratic").c_str()), quadratic);
}

static void setSpotLight(Shader& shader, bool enabled,
    const glm::vec3& pos, const glm::vec3& dir, const glm::vec3& color, float intensity,
    float cutOffCos, float outerCutOffCos,
    float constant, float linear, float quadratic)
{
    glUniform1i(glGetUniformLocation(shader.getId(), "uUseSpotLight"), enabled ? 1 : 0);
    if (!enabled) return;

    glUniform3f(glGetUniformLocation(shader.getId(), "uSpotLight.position"), pos.x, pos.y, pos.z);
    glUniform3f(glGetUniformLocation(shader.getId(), "uSpotLight.direction"), dir.x, dir.y, dir.z);
    glUniform3f(glGetUniformLocation(shader.getId(), "uSpotLight.color"), color.x, color.y, color.z);
    glUniform1f(glGetUniformLocation(shader.getId(), "uSpotLight.intensity"), intensity);

    glUniform1f(glGetUniformLocation(shader.getId(), "uSpotLight.cutOff"), cutOffCos);
    glUniform1f(glGetUniformLocation(shader.getId(), "uSpotLight.outerCutOff"), outerCutOffCos);

    glUniform1f(glGetUniformLocation(shader.getId(), "uSpotLight.constant"), constant);
    glUniform1f(glGetUniformLocation(shader.getId(), "uSpotLight.linear"), linear);
    glUniform1f(glGetUniformLocation(shader.getId(), "uSpotLight.quadratic"), quadratic);
}

// ==========================================================
// ✅ GUI (CORE profile safe): ui shader + vbo/vao + stb_truetype
//    Shader expects: layout(0)=aPos vec2, layout(1)=aUV vec2, sampler2D uFont
// ==========================================================

static GLuint uiVAO = 0, uiVBO = 0;
static GLuint textVAO = 0, textVBO = 0;

static GLuint uiWhiteTex = 0;  // 1x1 white -> alpha=1 for shapes
static GLuint fontTex = 0;     // baked atlas
static stbtt_bakedchar fontCData[96]; // ASCII 32..126

static const int FONT_TEX_W = 512;
static const int FONT_TEX_H = 512;
static const float FONT_PX_SIZE = 28.0f;
static const char* FONT_TTF_PATH = "Resources/Fonts/Roboto-Regular.ttf";

// read whole file
static bool readFileBytes(const char* path, std::vector<unsigned char>& out)
{
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    f.seekg(0, std::ios::end);
    size_t sz = (size_t)f.tellg();
    f.seekg(0, std::ios::beg);
    out.resize(sz);
    f.read((char*)out.data(), (std::streamsize)sz);
    return true;
}

static GLuint makeTextureR8(int w, int h, const unsigned char* pixels)
{
    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, w, h, 0, GL_RED, GL_UNSIGNED_BYTE, pixels);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glBindTexture(GL_TEXTURE_2D, 0);
    return tex;
}

static void uiInit()
{
    // Interleaved buffer per vertex: pos.xy + uv.xy (4 floats)
    glGenVertexArrays(1, &uiVAO);
    glGenBuffers(1, &uiVBO);
    glBindVertexArray(uiVAO);
    glBindBuffer(GL_ARRAY_BUFFER, uiVBO);
    glBufferData(GL_ARRAY_BUFFER, 1024 * 1024, nullptr, GL_DYNAMIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 4, (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 4, (void*)(sizeof(float) * 2));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);

    glGenVertexArrays(1, &textVAO);
    glGenBuffers(1, &textVBO);
    glBindVertexArray(textVAO);
    glBindBuffer(GL_ARRAY_BUFFER, textVBO);
    glBufferData(GL_ARRAY_BUFFER, 1024 * 1024, nullptr, GL_DYNAMIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 4, (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 4, (void*)(sizeof(float) * 2));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);

    // 1x1 white texture (R=255 => alpha=1 in your fragment shader)
    {
        unsigned char white = 255;
        uiWhiteTex = makeTextureR8(1, 1, &white);
    }

    // Build font atlas from TTF
    {
        std::vector<unsigned char> ttf;
        if (!readFileBytes(FONT_TTF_PATH, ttf))
        {
            // If missing, keep fontTex = white (so at least UI shapes render).
            // But text will not be readable without a TTF file.
            fontTex = uiWhiteTex;
        }
        else
        {
            std::vector<unsigned char> bitmap(FONT_TEX_W * FONT_TEX_H);
            int res = stbtt_BakeFontBitmap(
                ttf.data(), 0,
                FONT_PX_SIZE,
                bitmap.data(), FONT_TEX_W, FONT_TEX_H,
                32, 96,
                fontCData
            );

            if (res <= 0)
            {
                fontTex = uiWhiteTex;
            }
            else
            {
                fontTex = makeTextureR8(FONT_TEX_W, FONT_TEX_H, bitmap.data());
            }
        }
    }
}

static void uiBindTexture(Shader& uiShader, GLuint tex)
{
    uiShader.use();
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex);
    glUniform1i(glGetUniformLocation(uiShader.getId(), "uFont"), 0);
}

static void uiDrawVerts(Shader& uiShader, GLuint vao, GLuint vbo,
    const float* vertsInterleavedPosUv, int vertCount,
    int screenW, int screenH,
    float r, float g, float b, float a,
    GLenum mode,
    GLuint textureToUse)
{
    uiShader.use();

    glUniform2f(glGetUniformLocation(uiShader.getId(), "uScreen"), (float)screenW, (float)screenH);
    glUniform4f(glGetUniformLocation(uiShader.getId(), "uColor"), r, g, b, a);

    uiBindTexture(uiShader, textureToUse);

    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, vertCount * sizeof(float) * 4, vertsInterleavedPosUv);
    glDrawArrays(mode, 0, vertCount);
    glBindVertexArray(0);

    glBindTexture(GL_TEXTURE_2D, 0);
}

static void uiDrawRectFilled(Shader& uiShader, float x, float y, float w, float h,
    int screenW, int screenH, float r, float g, float b, float a)
{
    // pos.xy uv.xy ; use white texture so alpha=1
    float v[] = {
        x,     y,     0,0,
        x + w, y,     0,0,
        x + w, y + h, 0,0,

        x,     y,     0,0,
        x + w, y + h, 0,0,
        x,     y + h, 0,0
    };

    uiDrawVerts(uiShader, uiVAO, uiVBO, v, 6, screenW, screenH, r, g, b, a, GL_TRIANGLES, uiWhiteTex);
}

static void uiDrawRectOutline(Shader& uiShader, float x, float y, float w, float h,
    int screenW, int screenH, float r, float g, float b, float a)
{
    float v[] = {
        x,     y,     0,0,
        x + w, y,     0,0,
        x + w, y + h, 0,0,
        x,     y + h, 0,0,
        x,     y,     0,0
    };

    uiDrawVerts(uiShader, uiVAO, uiVBO, v, 5, screenW, screenH, r, g, b, a, GL_LINE_STRIP, uiWhiteTex);
}

static void uiDrawCheck(Shader& uiShader, float x, float y, float s, int screenW, int screenH)
{
    // In UI we use TOP-LEFT origin. Draw a check that looks correct in that space.
    // Two segments: (left-mid -> mid-low) and (mid-low -> right-high)

    float v[] = {
        x + s * 0.18f, y + s * 0.55f, 0,0,
        x + s * 0.40f, y + s * 0.78f, 0,0,

        x + s * 0.40f, y + s * 0.78f, 0,0,
        x + s * 0.82f, y + s * 0.22f, 0,0
    };

    uiDrawVerts(uiShader, uiVAO, uiVBO, v, 4, screenW, screenH,
        0.2f, 1.0f, 0.4f, 0.95f, GL_LINES, uiWhiteTex);
}


static void uiDrawText(Shader& uiShader, float x, float y, const char* txt,
    int screenW, int screenH, float r, float g, float b, float a)
{
    if (!txt || !txt[0]) return;

    // If fontTex fallback is white texture, it means no font loaded.
    // Text will not look like letters; so user must provide a TTF.
    if (fontTex == uiWhiteTex)
    {
        // draw a small warning line instead of invisible text (optional):
        // return;  // uncomment if you prefer nothing
    }

    // Build triangles: each glyph is 2 triangles => 6 vertices => 24 floats (pos+uv)
    static std::vector<float> tri;
    tri.clear();
    tri.reserve(strlen(txt) * 6 * 4);

    float xpos = x;
    float ypos = y;

    // stbtt_GetBakedQuad wants "y" as baseline-ish; but works fine with top-left style too.
    for (const char* p = txt; *p; ++p)
    {
        unsigned char c = (unsigned char)*p;
        if (c == '\n')
        {
            xpos = x;
            ypos += FONT_PX_SIZE + 8.0f;
            continue;
        }
        if (c < 32 || c > 126) continue;

        stbtt_aligned_quad q;
        stbtt_GetBakedQuad(fontCData, FONT_TEX_W, FONT_TEX_H, c - 32, &xpos, &ypos, &q, 1);

        // 2 triangles, each vertex: pos.xy uv.xy
        // tri1: (x0,y0)-(x1,y0)-(x1,y1)
        tri.push_back(q.x0); tri.push_back(q.y0); tri.push_back(q.s0); tri.push_back(q.t0);
        tri.push_back(q.x1); tri.push_back(q.y0); tri.push_back(q.s1); tri.push_back(q.t0);
        tri.push_back(q.x1); tri.push_back(q.y1); tri.push_back(q.s1); tri.push_back(q.t1);

        // tri2: (x0,y0)-(x1,y1)-(x0,y1)
        tri.push_back(q.x0); tri.push_back(q.y0); tri.push_back(q.s0); tri.push_back(q.t0);
        tri.push_back(q.x1); tri.push_back(q.y1); tri.push_back(q.s1); tri.push_back(q.t1);
        tri.push_back(q.x0); tri.push_back(q.y1); tri.push_back(q.s0); tri.push_back(q.t1);
    }

    if (tri.empty()) return;

    uiDrawVerts(uiShader, textVAO, textVBO,
        tri.data(), (int)(tri.size() / 4),
        screenW, screenH, r, g, b, a,
        GL_TRIANGLES, fontTex);
}

static void drawQuestUI(Shader& uiShader, int screenW, int screenH)
{
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    int cur = -1;
    if (!qEnteredCave1) cur = 0;
    else if (!qGotRelic1) cur = 1;
    else if (!qEnteredCave2) cur = 2;
    else if (!qGotRelic2) cur = 3;
    else if (!qFoundHidden) cur = 4;

    // ✅ FINISHED: draw ONLY this and exit
    if (cur == -1)
    {
        float pw = 520.0f;
        float ph = 70.0f;
        float px = (screenW - pw) * 0.5f;   // centered
        float py = 20.0f;

        uiDrawRectFilled(uiShader, px, py, pw, ph, screenW, screenH, 0, 0, 0, 0.40f);
        uiDrawRectOutline(uiShader, px, py, pw, ph, screenW, screenH, 1, 1, 1, 0.70f);

        uiDrawText(uiShader, px + 24, py + 44,
            "GAME FINISHED",
            screenW, screenH, 0.3f, 1.0f, 0.6f, 0.98f);

        glDisable(GL_BLEND);
        glEnable(GL_DEPTH_TEST);
        return;
    }

    // ---------------------------
    // de aici în jos: UI normal (title + stacked tasks)
    // ---------------------------

    const char* taskLabel[5] =
    {
        "Enter cave 1",
        "Collect relic part 1",
        "Enter cave 2",
        "Collect relic part 2",
        "Find the hidden part"
    };

    float px = 20.0f;
    float py = 20.0f;
    float pw = 780.0f;

    float titleH = 34.0f;
    float rowH = 28.0f;
    float pad = 12.0f;

    int rowsToShow = cur + 1;
    float ph = pad + titleH + 8.0f + rowsToShow * rowH + pad;

    uiDrawRectFilled(uiShader, px, py, pw, ph, screenW, screenH, 0, 0, 0, 0.35f);
    uiDrawRectOutline(uiShader, px, py, pw, ph, screenW, screenH, 1, 1, 1, 0.60f);

    uiDrawText(uiShader, px + pad, py + pad + 20.0f,
        "On your way to get all the missing parts of the relic you need to:",
        screenW, screenH, 1, 1, 1, 0.95f);

    float startY = py + pad + titleH + 8.0f;

    for (int i = 0; i <= cur; i++)
    {
        float y = startY + i * rowH;
        float box = 18.0f;
        float bx = px + pad;
        float by = y;

        uiDrawRectOutline(uiShader, bx, by, box, box, screenW, screenH, 1, 1, 1, 0.90f);

        if (i < cur)
            uiDrawCheck(uiShader, bx, by, box, screenW, screenH);

        uiDrawText(uiShader, bx + box + 10.0f, by + 14.0f, taskLabel[i],
            screenW, screenH, 1, 1, 1, 1.0f);
    }

    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
}


// ==========================================================
// MAIN
// ==========================================================

int main()
{
    glClearColor(0.2f, 0.8f, 1.0f, 1.0f);

    Shader shader("Shaders/vertex_shader.glsl", "Shaders/fragment_shader.glsl");
    Shader sunShader("Shaders/sun_vertex_shader.glsl", "Shaders/sun_fragment_shader.glsl");

    // ✅ UI shader (the one you posted)
    Shader uiShader("Shaders/ui_vertex_shader.glsl", "Shaders/ui_fragment_shader.glsl");
    uiInit();

    // ===================== Textures =====================

    GLuint texWood = loadBMP("Resources/Textures/wood.bmp");
    GLuint texRock = loadBMP("Resources/Textures/rock.bmp");
    GLuint texOrange = loadBMP("Resources/Textures/orange.bmp");
    GLuint texUnderSand = loadBMP("Resources/Textures/underwater_sand.bmp");

    GLuint texColumn = loadBMP("Resources/Textures/987.bmp");
    if (texColumn == 0) texColumn = texRock;

    glEnable(GL_DEPTH_TEST);
    srand(42);

    MeshLoaderObj loader;

    Mesh sun = loader.loadObj("Resources/Models/sphere.obj");

    // Portals
    std::vector<Texture> woodTex(1);
    woodTex[0].id = texWood;
    woodTex[0].type = "texture_diffuse";
    Mesh box = loader.loadObj("Resources/Models/cube.obj", woodTex);

    // Floor
    std::vector<Texture> planeTex(1);
    planeTex[0].id = texUnderSand;
    planeTex[0].type = "texture_diffuse";
    Mesh plane = loader.loadObj("Resources/Models/plane.obj", planeTex);

    // Rocks
    std::vector<Texture> rockTex(1);
    rockTex[0].id = texRock;
    rockTex[0].type = "texture_diffuse";
    Mesh rockBox = loader.loadObj("Resources/Models/cube.obj", rockTex);

    // Coin unused visual
    std::vector<Texture> coinTex(1);
    coinTex[0].id = texOrange;
    coinTex[0].type = "texture_diffuse";
    Mesh coinBox = loader.loadObj("Resources/Models/cube.obj", coinTex);

    // Chest
    Mesh chestMesh = loader.loadObj("Resources/Models/chest.obj");
    const float CHEST_OBJ_SCALE = 20.0f;

    // Column
    std::vector<Texture> colTex(1);
    colTex[0].id = texColumn;
    colTex[0].type = "texture_diffuse";
    Mesh columnMesh = loader.loadObj("Resources/Models/column.obj", colTex);

    const float COLUMN_SCALE = 0.80f;
    const float COLUMN_BASE_Y_FIX = -300.0f;

    // Shark
    Mesh sharkMesh = loader.loadObj("Resources/Models/Shark.obj");
    const float SHARK_OBJ_SCALE = 8.0f;
    const float SHARK_Y_LIFT = 0.0f;

    // Statue (relic part)
    Mesh statueMesh = loader.loadObj("Resources/Models/statue.obj");
    const float STATUE_OBJ_SCALE = 0.5f;
    const float STATUE_Y_LIFT = 0.0f;

    // ==========================================================
    // BUILD OCTREE
    // ==========================================================
    std::vector<Triangle> sharkTris;
    Octree sharkOctree;

    {
        const auto& V = sharkMesh.vertices;
        const auto& I = sharkMesh.indices;

        sharkTris.reserve(I.size() / 3);

        for (size_t k = 0; k + 2 < I.size(); k += 3)
        {
            glm::vec3 a = V[I[k + 0]].pos;
            glm::vec3 b = V[I[k + 1]].pos;
            glm::vec3 c = V[I[k + 2]].pos;

            Triangle t;
            t.a = a; t.b = b; t.c = c;
            t.bounds = triBounds(a, b, c);
            sharkTris.push_back(t);
        }

        sharkOctree.build(sharkTris, 8, 30);
    }

    bool ePrevDown = false;

    // ===================== Game Loop =====================
    while (!window.isPressed(GLFW_KEY_ESCAPE) &&
        glfwWindowShouldClose(window.getWindow()) == 0)
    {
        window.clear();

        float currentFrame = glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        processKeyboardInput();

        bool eDown = window.isPressed(GLFW_KEY_E);
        bool eJustPressed = (eDown && !ePrevDown);
        ePrevDown = eDown;

        float now = glfwGetTime();

        // ==========================================================
        // TELEPORT via portals (main room only)
        // ==========================================================
        if (currentZone == 0 && (now - lastTeleportTime > 0.8f))
        {
            glm::vec3 camPos = camera.getCameraPosition();

            if (pointInAABB(camPos, portalLeftPos, portalHalfSize))
            {
                currentZone = 1;
                qEnteredCave1 = true;
                lastTeleportTime = now;

                cave1Origin = caveSpawnLeft;
                camera.setCameraPosition(cave1Origin + glm::vec3(0.0f));

                cave1ObstaclePos = cave1Origin + glm::vec3(0.0f, 0.0f, -120.0f);
                cave1ObstacleTarget = cave1ObstaclePos;
                cave1NextTargetTime = glfwGetTime() + 0.3f;

                cave1CoinBase = cave1Origin + glm::vec3(0.0f, 10.0f, -120.0f);
                cave1CoinVisible = true;
            }
            else if (pointInAABB(camPos, portalRightPos, portalHalfSize))
            {
                currentZone = 2;
                qEnteredCave2 = true;
                lastTeleportTime = now;

                cave2Origin = caveSpawnRight;
                camera.setCameraPosition(cave2Origin + glm::vec3(0.0f));

                cave2EnemyA_Pos = cave2Origin + glm::vec3(120.0f, 0.0f, -220.0f);
                cave2EnemyA_Target = cave2EnemyA_Pos;
                cave2EnemyA_NextTargetTime = glfwGetTime() + 0.3f;

                cave2EnemyB_Pos = mirrorXAroundOrigin(cave2EnemyA_Pos, cave2Origin);

                cave2CoinBase = cave2Origin + glm::vec3(0.0f, 10.0f, -120.0f);
                cave2CoinVisible = true;
            }
        }

        // ==========================================================
        // MAIN ROOM: stop chest shaking + hidden part
        // ==========================================================
        if (currentZone == 0 && eJustPressed)
        {
            float distToChest = glm::length(camera.getCameraPosition() - chestBasePos);
            if (distToChest < 200.0f)
            {
                chestShaking = false;
                qFoundHidden = true;
            }
        }

        // ==========================================================
        // CAVE 1 LOGIC
        // ==========================================================
        if (currentZone == 1)
        {
            float nowT = glfwGetTime();

            if (nowT > cave1NextTargetTime)
            {
                float rangeX = 220.0f;
                float rangeZ = 220.0f;

                glm::vec3 base = cave1Origin + glm::vec3(0.0f, 0.0f, -180.0f);

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

            // Octree collision
            {
                glm::vec3 camPosW = camera.getCameraPosition();

                glm::mat4 M = glm::mat4(1.0f);
                M = glm::translate(M, cave1ObstaclePos + glm::vec3(0.0f, SHARK_Y_LIFT, 0.0f));
                M = glm::rotate(M, glm::radians(90.0f), glm::vec3(0, 1, 0));
                M = glm::scale(M, glm::vec3(SHARK_OBJ_SCALE));

                glm::mat4 invM = glm::inverse(M);

                glm::vec3 camPosL = glm::vec3(invM * glm::vec4(camPosW, 1.0f));
                float rLocal = PLAYER_RADIUS_WORLD / SHARK_OBJ_SCALE;

                if (sharkOctree.sphereHit(camPosL, rLocal))
                {
                    float tNow = glfwGetTime();
                    if (tNow - lastSharkHitTime > SHARK_HIT_COOLDOWN)
                    {
                        lastSharkHitTime = tNow;
                        camera.setCameraPosition(cave1Origin + glm::vec3(0.0f));
                    }
                }
            }

            // relic part 1 pickup
            if (cave1CoinVisible && eJustPressed)
            {
                float tCoin = glfwGetTime();
                float floatY = std::sin(tCoin * 2.2f) * 3.0f;
                glm::vec3 coinPosNow = cave1CoinBase + glm::vec3(0.0f, floatY, 0.0f);

                float distToCoin = glm::length(camera.getCameraPosition() - coinPosNow);
                if (distToCoin < 35.0f)
                {
                    cave1CoinVisible = false;
                    qGotRelic1 = true;
                    currentZone = 0;
                    camera.setCameraPosition(mainSpawnPos);
                    lastTeleportTime = glfwGetTime();
                }
            }
        }

        // ==========================================================
        // CAVE 2 LOGIC
        // ==========================================================
        if (currentZone == 2)
        {
            float nowT = glfwGetTime();

            if (nowT > cave2EnemyA_NextTargetTime)
            {
                float rangeX = 240.0f;
                float rangeZ = 240.0f;

                glm::vec3 base = cave2Origin + glm::vec3(0.0f, 0.0f, -180.0f);

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

            cave2EnemyB_Pos = mirrorXAroundOrigin(cave2EnemyA_Pos, cave2Origin);

            // Octree collision for both sharks
            {
                glm::vec3 camPosW = camera.getCameraPosition();
                float rLocal = PLAYER_RADIUS_WORLD / SHARK_OBJ_SCALE;

                auto hitSharkAt = [&](const glm::vec3& pos, float yawDeg) -> bool
                    {
                        glm::mat4 M = glm::mat4(1.0f);
                        M = glm::translate(M, pos + glm::vec3(0.0f, SHARK_Y_LIFT, 0.0f));
                        M = glm::rotate(M, glm::radians(yawDeg), glm::vec3(0, 1, 0));
                        M = glm::scale(M, glm::vec3(SHARK_OBJ_SCALE));

                        glm::mat4 invM = glm::inverse(M);
                        glm::vec3 camPosL = glm::vec3(invM * glm::vec4(camPosW, 1.0f));

                        return sharkOctree.sphereHit(camPosL, rLocal);
                    };

                bool hitA = hitSharkAt(cave2EnemyA_Pos, 90.0f);
                bool hitB = hitSharkAt(cave2EnemyB_Pos, -90.0f);

                if (hitA || hitB)
                {
                    float tNow = glfwGetTime();
                    if (tNow - lastSharkHitTime > SHARK_HIT_COOLDOWN)
                    {
                        lastSharkHitTime = tNow;
                        camera.setCameraPosition(cave2Origin + glm::vec3(0.0f));
                    }
                }
            }

            // relic part 2 pickup
            if (cave2CoinVisible && eJustPressed)
            {
                float tCoin = glfwGetTime();
                float floatY = std::sin(tCoin * 2.2f) * 3.0f;
                glm::vec3 coinPosNow = cave2CoinBase + glm::vec3(0.0f, floatY, 0.0f);

                float distToCoin = glm::length(camera.getCameraPosition() - coinPosNow);
                if (distToCoin < 35.0f)
                {
                    cave2CoinVisible = false;
                    qGotRelic2 = true;
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

        glm::vec3 sunPos = camera.getCameraPosition() + glm::vec3(0.0f, 350.0f, 0.0f);

        ModelMatrix = glm::mat4(1.0f);
        ModelMatrix = glm::translate(ModelMatrix, sunPos);
        MVP = ProjectionMatrix * ViewMatrix * ModelMatrix;
        glUniformMatrix4fv(MatrixID, 1, GL_FALSE, &MVP[0][0]);
        sun.draw(sunShader);

        // ---- Main pass ----
        shader.use();

        GLuint MatrixID2 = glGetUniformLocation(shader.getId(), "MVP");
        GLuint ModelMatrixID = glGetUniformLocation(shader.getId(), "model");

        glUniform3f(glGetUniformLocation(shader.getId(), "viewPos"),
            camera.getCameraPosition().x,
            camera.getCameraPosition().y,
            camera.getCameraPosition().z);

        // Fog
        glUniform1i(glGetUniformLocation(shader.getId(), "uUseFog"), 1);
        glUniform3f(glGetUniformLocation(shader.getId(), "uFogColor"), 0.05f, 0.35f, 0.55f);
        glUniform1f(glGetUniformLocation(shader.getId(), "uFogNear"), 40.0f);
        glUniform1f(glGetUniformLocation(shader.getId(), "uFogFar"), 420.0f);
        glUniform1f(glGetUniformLocation(shader.getId(), "uNear"), 0.1f);
        glUniform1f(glGetUniformLocation(shader.getId(), "uFar"), 10000.0f);

        glUniform1i(glGetUniformLocation(shader.getId(), "uIsPortal"), 0);
        glUniform1f(glGetUniformLocation(shader.getId(), "uPortalAlpha"), 1.0f);

        // Material
        setMaterialUniforms(shader, 0.20f, 0.35f, 64.0f);

        // Dir light
        setDirLight(shader, true,
            glm::vec3(-0.2f, -1.0f, -0.3f),
            glm::vec3(0.25f, 0.40f, 0.55f),
            0.60f);

        // Point lights
        int pointCount = 0;
        auto setNicePoint = [&](int idx, const glm::vec3& pos, const glm::vec3& col, float intensity)
            {
                setPointLight(shader, idx, pos, col, intensity, 1.0f, 0.014f, 0.0007f);
            };

        setNicePoint(pointCount++, portalLeftPos, glm::vec3(0.2f, 0.7f, 1.0f), 3.0f);
        setNicePoint(pointCount++, portalRightPos, glm::vec3(0.2f, 0.7f, 1.0f), 3.0f);

        float tChest = glfwGetTime();
        glm::vec3 shakeOffset(0.0f);
        if (currentZone == 0 && chestShaking)
        {
            float shakeSpeed = 8.0f;
            shakeOffset = glm::vec3(
                1.2f * std::sin(tChest * shakeSpeed),
                0.4f * std::sin(tChest * shakeSpeed * 1.7f),
                0.8f * std::cos(tChest * shakeSpeed)
            );
        }

        glm::vec3 chestPos = chestBasePos;
        chestPos.y = mainFloorTopY();
        chestPos += shakeOffset;

        if (currentZone == 0 && pointCount < 8)
            setNicePoint(pointCount++, chestPos + glm::vec3(0.0f, 60.0f, 0.0f), glm::vec3(1.0f, 0.8f, 0.4f), 2.0f);

        if (currentZone == 1 && cave1CoinVisible && pointCount < 8)
            setNicePoint(pointCount++, cave1CoinBase + glm::vec3(0.0f, 30.0f, 0.0f), glm::vec3(0.8f, 0.9f, 1.0f), 1.2f);

        if (currentZone == 2 && cave2CoinVisible && pointCount < 8)
            setNicePoint(pointCount++, cave2CoinBase + glm::vec3(0.0f, 30.0f, 0.0f), glm::vec3(0.8f, 0.9f, 1.0f), 1.2f);

        if (pointCount > 8) pointCount = 8;
        glUniform1i(glGetUniformLocation(shader.getId(), "uNumPointLights"), pointCount);

        // Spot light
        glm::vec3 cp = camera.getCameraPosition();
        glm::vec3 cd = camera.getCameraViewDirection();
        setSpotLight(shader, true,
            cp, cd,
            glm::vec3(0.8f, 0.9f, 1.0f),
            2.0f,
            glm::cos(glm::radians(12.5f)),
            glm::cos(glm::radians(18.0f)),
            1.0f, 0.02f, 0.001f);

        // ===================== DRAW MAIN ROOM =====================
        if (currentZone == 0)
        {
            // floor
            ModelMatrix = glm::mat4(1.0f);
            ModelMatrix = glm::translate(ModelMatrix, glm::vec3(0.0f, MAIN_FLOOR_Y, 0.0f));
            ModelMatrix = glm::scale(ModelMatrix, glm::vec3(2000.0f, 1.0f, 2000.0f));
            MVP = ProjectionMatrix * ViewMatrix * ModelMatrix;
            glUniformMatrix4fv(MatrixID2, 1, GL_FALSE, &MVP[0][0]);
            glUniformMatrix4fv(ModelMatrixID, 1, GL_FALSE, &ModelMatrix[0][0]);
            plane.draw(shader);

            // portals transparent
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

            glUniform1i(glGetUniformLocation(shader.getId(), "uIsPortal"), 1);
            glUniform1f(glGetUniformLocation(shader.getId(), "uPortalAlpha"), 0.35f);

            // left
            ModelMatrix = glm::translate(glm::mat4(1.0f), portalLeftPos);
            ModelMatrix = glm::scale(ModelMatrix, glm::vec3(8.0f, 18.0f, 0.5f));
            MVP = ProjectionMatrix * ViewMatrix * ModelMatrix;
            glUniformMatrix4fv(MatrixID2, 1, GL_FALSE, &MVP[0][0]);
            glUniformMatrix4fv(ModelMatrixID, 1, GL_FALSE, &ModelMatrix[0][0]);
            box.draw(shader);

            // right
            ModelMatrix = glm::translate(glm::mat4(1.0f), portalRightPos);
            ModelMatrix = glm::scale(ModelMatrix, glm::vec3(8.0f, 18.0f, 0.5f));
            MVP = ProjectionMatrix * ViewMatrix * ModelMatrix;
            glUniformMatrix4fv(MatrixID2, 1, GL_FALSE, &MVP[0][0]);
            glUniformMatrix4fv(ModelMatrixID, 1, GL_FALSE, &ModelMatrix[0][0]);
            box.draw(shader);

            glUniform1i(glGetUniformLocation(shader.getId(), "uIsPortal"), 0);
            glUniform1f(glGetUniformLocation(shader.getId(), "uPortalAlpha"), 1.0f);
            glDisable(GL_BLEND);

            // columns
            const int   NUM_COLS_PER_SIDE = 8;
            const float COL_X_LEFT = -260.0f;
            const float COL_X_RIGHT = 260.0f;
            const float COL_Z_START = -60.0f;
            const float COL_Z_STEP = 120.0f;

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

            // rocks
            auto drawRock = [&](glm::vec3 pos, glm::vec3 scale)
                {
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

            // chest
            ModelMatrix = glm::mat4(1.0f);
            ModelMatrix = glm::translate(ModelMatrix, chestPos);
            ModelMatrix = glm::scale(ModelMatrix, glm::vec3(CHEST_OBJ_SCALE));

            MVP = ProjectionMatrix * ViewMatrix * ModelMatrix;
            glUniformMatrix4fv(MatrixID2, 1, GL_FALSE, &MVP[0][0]);
            glUniformMatrix4fv(ModelMatrixID, 1, GL_FALSE, &ModelMatrix[0][0]);
            chestMesh.draw(shader);
        }

        // ===================== DRAW CAVE 1 =====================
        if (currentZone == 1)
        {
            ModelMatrix = glm::mat4(1.0f);
            ModelMatrix = glm::translate(ModelMatrix, cave1Origin + glm::vec3(0.0f, caveFloorY, 0.0f));
            ModelMatrix = glm::scale(ModelMatrix, glm::vec3(caveFloorSize, 1.0f, caveFloorSize));
            MVP = ProjectionMatrix * ViewMatrix * ModelMatrix;
            glUniformMatrix4fv(MatrixID2, 1, GL_FALSE, &MVP[0][0]);
            glUniformMatrix4fv(ModelMatrixID, 1, GL_FALSE, &ModelMatrix[0][0]);
            plane.draw(shader);

            // shark
            ModelMatrix = glm::translate(glm::mat4(1.0f), cave1ObstaclePos + glm::vec3(0.0f, SHARK_Y_LIFT, 0.0f));
            ModelMatrix = glm::rotate(ModelMatrix, glm::radians(90.0f), glm::vec3(0, 1, 0));
            ModelMatrix = glm::scale(ModelMatrix, glm::vec3(SHARK_OBJ_SCALE));
            MVP = ProjectionMatrix * ViewMatrix * ModelMatrix;
            glUniformMatrix4fv(MatrixID2, 1, GL_FALSE, &MVP[0][0]);
            glUniformMatrix4fv(ModelMatrixID, 1, GL_FALSE, &ModelMatrix[0][0]);
            sharkMesh.draw(shader);

            // statue
            if (cave1CoinVisible)
            {
                float t2 = glfwGetTime();
                float floatY = std::sin(t2 * 2.2f) * 3.0f;

                ModelMatrix = glm::translate(glm::mat4(1.0f),
                    cave1CoinBase + glm::vec3(0.0f, floatY + STATUE_Y_LIFT, 0.0f));
                ModelMatrix = glm::rotate(ModelMatrix, t2 * 1.6f, glm::vec3(0, 1, 0));
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
            ModelMatrix = glm::mat4(1.0f);
            ModelMatrix = glm::translate(ModelMatrix, cave2Origin + glm::vec3(0.0f, caveFloorY, 0.0f));
            ModelMatrix = glm::scale(ModelMatrix, glm::vec3(caveFloorSize, 1.0f, caveFloorSize));
            MVP = ProjectionMatrix * ViewMatrix * ModelMatrix;
            glUniformMatrix4fv(MatrixID2, 1, GL_FALSE, &MVP[0][0]);
            glUniformMatrix4fv(ModelMatrixID, 1, GL_FALSE, &ModelMatrix[0][0]);
            plane.draw(shader);

            // shark A
            ModelMatrix = glm::translate(glm::mat4(1.0f), cave2EnemyA_Pos + glm::vec3(0.0f, SHARK_Y_LIFT, 0.0f));
            ModelMatrix = glm::rotate(ModelMatrix, glm::radians(90.0f), glm::vec3(0, 1, 0));
            ModelMatrix = glm::scale(ModelMatrix, glm::vec3(SHARK_OBJ_SCALE));
            MVP = ProjectionMatrix * ViewMatrix * ModelMatrix;
            glUniformMatrix4fv(MatrixID2, 1, GL_FALSE, &MVP[0][0]);
            glUniformMatrix4fv(ModelMatrixID, 1, GL_FALSE, &ModelMatrix[0][0]);
            sharkMesh.draw(shader);

            // shark B
            ModelMatrix = glm::translate(glm::mat4(1.0f), cave2EnemyB_Pos + glm::vec3(0.0f, SHARK_Y_LIFT, 0.0f));
            ModelMatrix = glm::rotate(ModelMatrix, glm::radians(-90.0f), glm::vec3(0, 1, 0));
            ModelMatrix = glm::scale(ModelMatrix, glm::vec3(SHARK_OBJ_SCALE));
            MVP = ProjectionMatrix * ViewMatrix * ModelMatrix;
            glUniformMatrix4fv(MatrixID2, 1, GL_FALSE, &MVP[0][0]);
            glUniformMatrix4fv(ModelMatrixID, 1, GL_FALSE, &ModelMatrix[0][0]);
            sharkMesh.draw(shader);

            // statue
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

        // ===================== GUI Overlay =====================
        drawQuestUI(uiShader, window.getWidth(), window.getHeight());

        window.update();
    }

    return 0;
}

// ==========================================================
// Keyboard input
// ==========================================================
void processKeyboardInput()
{
    float cameraSpeed = 30.0f * deltaTime;

    if (window.isPressed(GLFW_KEY_W)) camera.keyboardMoveFront(cameraSpeed);
    if (window.isPressed(GLFW_KEY_S)) camera.keyboardMoveBack(cameraSpeed);
    if (window.isPressed(GLFW_KEY_A)) camera.keyboardMoveLeft(cameraSpeed);
    if (window.isPressed(GLFW_KEY_D)) camera.keyboardMoveRight(cameraSpeed);
    if (window.isPressed(GLFW_KEY_R)) camera.keyboardMoveUp(cameraSpeed);
    if (window.isPressed(GLFW_KEY_F)) camera.keyboardMoveDown(cameraSpeed);

    if (window.isPressed(GLFW_KEY_LEFT))  camera.rotateOy(cameraSpeed);
    if (window.isPressed(GLFW_KEY_RIGHT)) camera.rotateOy(-cameraSpeed);
    if (window.isPressed(GLFW_KEY_UP))    camera.rotateOx(cameraSpeed);
    if (window.isPressed(GLFW_KEY_DOWN))  camera.rotateOx(-cameraSpeed);
}
