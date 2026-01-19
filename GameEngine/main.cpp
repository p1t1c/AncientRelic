// Ideea jocului (pe scurt, pt prezentare):
//  - Avem 3 "zone": main room (0) + cave1 (1) + cave2 (2)
//  - In main room sunt 2 portale (AABB). Cand intri in ele, te teleporteaza in cave-uri
//  - In cave-uri sunt rechini care se misca random; coliziunea e facuta cu Octree (mesh vs sfera player)
//  - In fiecare cave ai o "relic part" (statue). Apesi E aproape si o colectezi -> te intoarce in main room
//  - In main room ai un chest care se zguduie pana apesi E aproape de el (asta e "hidden part")
//  - Avem UI 2D in OpenGL CORE: quest checklist + stamina bar
//  - Avem iluminare mai complexa: directional + point lights + spot light (lanterna camerei)
//
// Observatie de proiect: fisierul e mare fiindca include si logica si randare si UI.
// Intr-un proiect mai curat, asta ar fi impartit pe module.

#include "Graphics\\window.h"
#include "Camera\\camera.h"
#include "Shaders\\shader.h"
#include "Model Loading\\mesh.h"
#include "Model Loading\\texture.h"
#include "Model Loading\\meshLoaderObj.h"

// Coliziune complexa: Octree construit din triunghiurile rechinului
#include "Model Loading\\SharkCollisionOctree.h"

#include <glm.hpp>

#include <cmath>
#include <cstdlib>
#include <vector>
#include <string>
#include <fstream>
#include <cstdint>

// ===================== GUI font (TTF) =====================
// stb_truetype: folosim un .ttf (Roboto) -> "bake" intr-un atlas bitmap -> textura OpenGL.
// Apoi desenam textul ca quads cu UV-uri in atlas.
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

// ===================== Helpers =====================

// Random float in interval [a, b]
float randRange(float a, float b)
{
    return a + (b - a) * (rand() / (float)RAND_MAX);
}

// Test de tip "trigger": verificam daca un punct se afla intr-un AABB.
// Il folosim pt portal (camera/player position intra in cutie -> teleport).
bool pointInAABB(const glm::vec3& p, const glm::vec3& center, const glm::vec3& halfSize)
{
    return (std::abs(p.x - center.x) <= halfSize.x) &&
        (std::abs(p.y - center.y) <= halfSize.y) &&
        (std::abs(p.z - center.z) <= halfSize.z);
}

// Functie mica pt simetrie in cave2: luam o pozitie si o oglindim fata de origin doar pe axa X.
// Astfel facem al doilea rechin "B" fara sa scriem logica separata de miscare.
glm::vec3 mirrorXAroundOrigin(const glm::vec3& pos, const glm::vec3& origin)
{
    glm::vec3 d = pos - origin;
    d.x = -d.x;
    return origin + d;
}

// ===================== Floor alignment helpers =====================
// Podeaua din main room e o placa la Y = -20.0.
// Ca sa pozitionam obiectele "pe podea", lucram cu topY = Y + halfThickness.

const float MAIN_FLOOR_Y = -20.0f;
const float MAIN_FLOOR_HALF_THICKNESS = 0.5f;

inline float mainFloorTopY()
{
    return MAIN_FLOOR_Y + MAIN_FLOOR_HALF_THICKNESS;
}

// ===================== Global State =====================

void processKeyboardInput();

// Timing per frame (deltaTime) pentru miscare/animatii independente de FPS
float deltaTime = 0.0f;
float lastFrame = 0.0f;

// ===================== STAMINA (Sprint) =====================
// stamina: [0..1]
// - cand tii SHIFT, creste viteza dar scade stamina
// - cand nu sprint-ezi, stamina se regenereaza
static float stamina = 1.0f;                 // 0..1
static const float STAMINA_DRAIN = 0.35f;    // scadere pe secunda cand sprint-ezi
static const float STAMINA_REGEN = 0.25f;    // regenerare pe secunda cand mergi normal
static const float SPRINT_MULT = 2.0f;       // multiplicator de viteza
static const float MIN_STAMINA_TO_SPRINT = 0.05f;
static bool isSprinting = false;

// Obiecte globale pt aplicatie
Window window("Game Engine", 800, 800);
Camera camera;

// Zone: 0 = main room, 1 = cave 1, 2 = cave 2
int currentZone = 0;

// Teleport cooldown: ca sa nu retrigger-uiesti portalul instant dupa spawn
static float lastTeleportTime = -1000.0f;

// Spawn-ul din main room unde te intorci dupa coin pickup
glm::vec3 mainSpawnPos(0.0f, 0.0f, 100.0f);

// ===================== Main Room Stuff =====================

// Chest-ul se zguduie pana il "interactionezi" cu E (si marcheaza hidden part)
bool chestShaking = true;
glm::vec3 chestBasePos(0.0f, -18.0f, -140.0f);

// Portale: sunt doar volume de trigger (AABB), nu coliziune mesh
glm::vec3 portalLeftPos(-100.0f, 15.0f, -80.0f);
glm::vec3 portalRightPos(100.0f, 15.0f, -80.0f);
glm::vec3 portalHalfSize(25.0f, 50.0f, 25.0f);

// Spawn points pentru caves
glm::vec3 caveSpawnLeft(0.0f, 0.0f, -500.0f);
glm::vec3 caveSpawnRight(50.0f, 0.0f, -500.0f);

// ===================== Cave 1 Stuff =====================

// Cave1 are un origin; toate pozitiile importante sunt relative la origin
glm::vec3 cave1Origin;

// Rechin in cave1: are pozitie curenta + un target spre care se misca
glm::vec3 cave1ObstaclePos;
glm::vec3 cave1ObstacleTarget;
float     cave1NextTargetTime = 0.0f;

// Relic part (vizibil/invizibil) + pozitie de baza
glm::vec3 cave1CoinBase;
bool      cave1CoinVisible = true;

// Podeaua si dimensiunea "pesterii"
float caveFloorY = -20.0f;
float caveFloorSize = 1200.0f;

// ===================== Cave 2 Stuff =====================

glm::vec3 cave2Origin;

// Cave2 are doi rechini: A e "principal", B e oglindit fata de origin
glm::vec3 cave2EnemyA_Pos;
glm::vec3 cave2EnemyA_Target;
float     cave2EnemyA_NextTargetTime = 0.0f;

glm::vec3 cave2EnemyB_Pos;

// Relic part din cave2
glm::vec3 cave2CoinBase;
bool      cave2CoinVisible = true;

// ==========================================================
// Shark collision settings
// ==========================================================

// Player-ul e aproximat ca o sfera cu raza fixa in world space.
// Cand testam coliziunea, transformam camera in local space al rechinului.
static const float PLAYER_RADIUS_WORLD = 6.0f;

// Cooldown ca sa nu "te omoare" in fiecare frame cand esti in contact
static float lastSharkHitTime = -1000.0f;
static const float SHARK_HIT_COOLDOWN = 0.8f;

// ==========================================================
// QUEST STATE (GUI tasks)
// ==========================================================

static bool qEnteredCave1 = false;
static bool qGotRelic1 = false;
static bool qEnteredCave2 = false;
static bool qGotRelic2 = false;
static bool qFoundHidden = false;

// ==========================================================
// Lighting helper setters (uniforms)
// ==========================================================
// Aici sunt functii care seteaza uniforme pentru shader-ul de fragment.
// Facem asta in C++ ca sa fie mai curat decat zeci de glUniform in main loop.

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
    // Array in shader: uPointLights[idx].<field>
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

    // Spotlight-ul vine din camera (pozitie + directie)
    glUniform3f(glGetUniformLocation(shader.getId(), "uSpotLight.position"), pos.x, pos.y, pos.z);
    glUniform3f(glGetUniformLocation(shader.getId(), "uSpotLight.direction"), dir.x, dir.y, dir.z);
    glUniform3f(glGetUniformLocation(shader.getId(), "uSpotLight.color"), color.x, color.y, color.z);
    glUniform1f(glGetUniformLocation(shader.getId(), "uSpotLight.intensity"), intensity);

    // cutoff-urile sunt trimise ca cos(angle) ca sa fie eficient in shader
    glUniform1f(glGetUniformLocation(shader.getId(), "uSpotLight.cutOff"), cutOffCos);
    glUniform1f(glGetUniformLocation(shader.getId(), "uSpotLight.outerCutOff"), outerCutOffCos);

    // parametri de atenuare (ca la point light)
    glUniform1f(glGetUniformLocation(shader.getId(), "uSpotLight.constant"), constant);
    glUniform1f(glGetUniformLocation(shader.getId(), "uSpotLight.linear"), linear);
    glUniform1f(glGetUniformLocation(shader.getId(), "uSpotLight.quadratic"), quadratic);
}

// ==========================================================
// GUI (CORE profile safe): UI shader + vbo/vao + stb_truetype
// ==========================================================
// In OpenGL CORE nu avem functii vechi gen glBegin/glEnd.
// Desenam UI ca niste triunghiuri/lines in screen space.
// Pentru text: fiecare litera devine 2 triunghiuri cu UV-uri intr-o textura atlas.

static GLuint uiVAO = 0, uiVBO = 0;
static GLuint textVAO = 0, textVBO = 0;

static GLuint uiWhiteTex = 0;  // textura 1x1 alba pt forme
static GLuint fontTex = 0;     // atlas font
static stbtt_bakedchar fontCData[96]; // ASCII 32..126

static const int FONT_TEX_W = 512;
static const int FONT_TEX_H = 512;
static const float FONT_PX_SIZE = 28.0f;
static const char* FONT_TTF_PATH = "Resources/Fonts/Roboto-Regular.ttf";

// citeste un fisier complet in memorie (folosit pt font .ttf)
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

// creeaza textura GL_R8 (1 channel) - potrivit pt atlas de font
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
    // VAO/VBO pentru forme UI (dreptunghiuri, linii)
    // Layout: (x,y,u,v) -> 4 floats / vertex
    glGenVertexArrays(1, &uiVAO);
    glGenBuffers(1, &uiVBO);
    glBindVertexArray(uiVAO);
    glBindBuffer(GL_ARRAY_BUFFER, uiVBO);

    // Alocam un buffer mare o data; apoi la fiecare draw facem glBufferSubData.
    glBufferData(GL_ARRAY_BUFFER, 1024 * 1024, nullptr, GL_DYNAMIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 4, (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 4, (void*)(sizeof(float) * 2));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);

    // VAO/VBO separat pentru text (aceeasi structura, dar logic separat)
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

    // Textura 1x1 alba: ne ajuta sa desenam forme fara atlas real
    {
        unsigned char white = 255;
        uiWhiteTex = makeTextureR8(1, 1, &white);
    }

    // Construim atlasul de font din fisierul TTF
    {
        std::vector<unsigned char> ttf;
        if (!readFileBytes(FONT_TTF_PATH, ttf))
        {
            // Daca lipseste fisierul font, UI-ul (forme) merge, dar textul nu.
            fontTex = uiWhiteTex;
        }
        else
        {
            std::vector<unsigned char> bitmap(FONT_TEX_W * FONT_TEX_H);

            // stbtt_BakeFontBitmap genereaza glyph-uri pt ASCII 32..126
            int res = stbtt_BakeFontBitmap(
                ttf.data(), 0,
                FONT_PX_SIZE,
                bitmap.data(), FONT_TEX_W, FONT_TEX_H,
                32, 96,
                fontCData
            );

            if (res <= 0)
            {
                // Daca bake a esuat, ramanem cu fallback
                fontTex = uiWhiteTex;
            }
            else
            {
                // Transformam bitmap-ul in textura OpenGL
                fontTex = makeTextureR8(FONT_TEX_W, FONT_TEX_H, bitmap.data());
            }
        }
    }
}

static void uiBindTexture(Shader& uiShader, GLuint tex)
{
    // UI shader-ul foloseste sampler2D uFont in texture unit 0
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
    // Functie "low level": incarcam vertecs in VBO si desenam cu glDrawArrays
    // screenW/screenH sunt trimise ca uniform ca sa facem conversia in shader (pixel coords -> NDC).
    uiShader.use();

    glUniform2f(glGetUniformLocation(uiShader.getId(), "uScreen"), (float)screenW, (float)screenH);
    glUniform4f(glGetUniformLocation(uiShader.getId(), "uColor"), r, g, b, a);

    uiBindTexture(uiShader, textureToUse);

    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);

    // Inlocuim doar partea folosita din buffer (fara realloc)
    glBufferSubData(GL_ARRAY_BUFFER, 0, vertCount * sizeof(float) * 4, vertsInterleavedPosUv);

    glDrawArrays(mode, 0, vertCount);
    glBindVertexArray(0);

    glBindTexture(GL_TEXTURE_2D, 0);
}

static void uiDrawRectFilled(Shader& uiShader, float x, float y, float w, float h,
    int screenW, int screenH, float r, float g, float b, float a)
{
    // Dreptunghi umplut = 2 triunghiuri (6 vertecsi).
    // UV-urile sunt 0 pentru ca folosim textura alba 1x1.
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
    // Contur dreptunghi (line strip). Inchidem forma repetand primul vertex la final.
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
    // Bifa: 2 segmente (4 puncte). Coordonate in UI (origine top-left).
    // Segment 1: din stanga-mijloc spre mijloc-jos
    // Segment 2: din mijloc-jos spre dreapta-sus
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

    // Daca fontTex e fallback (white), textul nu va avea glyph-uri reale.
    // Lasam totusi sa ruleze ca sa nu crape UI.
    if (fontTex == uiWhiteTex)
    {
        // optional: return;
    }

    // Construim un buffer de triunghiuri:
    // fiecare caracter -> 2 triunghiuri -> 6 vertecsi -> (x,y,u,v) per vertex
    static std::vector<float> tri;
    tri.clear();
    tri.reserve(strlen(txt) * 6 * 4);

    float xpos = x;
    float ypos = y;

    for (const char* p = txt; *p; ++p)
    {
        unsigned char c = (unsigned char)*p;

        // newline: reset pe X si coboram pe Y
        if (c == '\n')
        {
            xpos = x;
            ypos += FONT_PX_SIZE + 8.0f;
            continue;
        }

        // pastram doar ASCII-ul acoperit de atlas (32..126)
        if (c < 32 || c > 126) continue;

        stbtt_aligned_quad q;

        // stbtt_GetBakedQuad ne da:
        //  - coordonate de ecran (x0,y0,x1,y1)
        //  - coordonate UV (s0,t0,s1,t1) in atlas
        stbtt_GetBakedQuad(fontCData, FONT_TEX_W, FONT_TEX_H, c - 32, &xpos, &ypos, &q, 1);

        // Triunghi 1
        tri.push_back(q.x0); tri.push_back(q.y0); tri.push_back(q.s0); tri.push_back(q.t0);
        tri.push_back(q.x1); tri.push_back(q.y0); tri.push_back(q.s1); tri.push_back(q.t0);
        tri.push_back(q.x1); tri.push_back(q.y1); tri.push_back(q.s1); tri.push_back(q.t1);

        // Triunghi 2
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
    // UI se deseneaza peste scena 3D -> dezactivam depth test si folosim blending pt transparanta
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // cur = primul task "nefacut" (deci cate randuri afisam)
    int cur = -1;
    if (!qEnteredCave1) cur = 0;
    else if (!qGotRelic1) cur = 1;
    else if (!qEnteredCave2) cur = 2;
    else if (!qGotRelic2) cur = 3;
    else if (!qFoundHidden) cur = 4;

    // Daca toate sunt facute, afisam doar mesaj de final
    if (cur == -1)
    {
        float pw = 520.0f;
        float ph = 70.0f;
        float px = (screenW - pw) * 0.5f;
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

    // Task-urile (ordine fixa)
    const char* taskLabel[5] =
    {
        "Enter cave 1",
        "Collect relic part 1",
        "Enter cave 2",
        "Collect relic part 2",
        "Find the hidden part"
    };

    // Dimensiuni/pozitionare UI (stanga sus)
    float px = 20.0f;
    float py = 20.0f;
    float pw = 780.0f;

    float titleH = 34.0f;
    float rowH = 28.0f;
    float pad = 12.0f;

    // Afisam pana la cur inclusiv (task-uri facute + task curent)
    int rowsToShow = cur + 1;
    float ph = pad + titleH + 8.0f + rowsToShow * rowH + pad;

    // Fundal + contur (semi-transparent)
    uiDrawRectFilled(uiShader, px, py, pw, ph, screenW, screenH, 0, 0, 0, 0.35f);
    uiDrawRectOutline(uiShader, px, py, pw, ph, screenW, screenH, 1, 1, 1, 0.60f);

    // Titlu / instructiune
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

        // checkbox
        uiDrawRectOutline(uiShader, bx, by, box, box, screenW, screenH, 1, 1, 1, 0.90f);

        // daca e inainte de cur, inseamna ca e completat -> desenam check
        if (i < cur)
            uiDrawCheck(uiShader, bx, by, box, screenW, screenH);

        // text task
        uiDrawText(uiShader, bx + box + 10.0f, by + 14.0f, taskLabel[i],
            screenW, screenH, 1, 1, 1, 1.0f);
    }

    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
}

static void drawStaminaUI(Shader& uiShader, int screenW, int screenH)
{
    // Tot UI 2D (overlay), deci acelasi setup ca la quest
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // bara in dreapta sus
    float w = 220.0f;
    float h = 16.0f;
    float x = (float)screenW - w - 20.0f;
    float y = 20.0f;

    // panel mic (pt contrast)
    uiDrawRectFilled(uiShader, x - 10.0f, y - 10.0f, w + 20.0f, h + 34.0f,
        screenW, screenH, 0, 0, 0, 0.25f);
    uiDrawRectOutline(uiShader, x - 10.0f, y - 10.0f, w + 20.0f, h + 34.0f,
        screenW, screenH, 1, 1, 1, 0.25f);

    // background bara
    uiDrawRectFilled(uiShader, x, y, w, h, screenW, screenH, 0, 0, 0, 0.55f);

    // contur
    uiDrawRectOutline(uiShader, x, y, w, h, screenW, screenH, 1, 1, 1, 0.80f);

    // inner area (cu padding) ca sa nu se lipeasca fill de contur
    float innerPad = 2.0f;
    float innerW = w - innerPad * 2.0f;
    float innerH = h - innerPad * 2.0f;

    // latimea fill-ului depinde de stamina (0..1)
    float fillW = innerW * stamina;
    if (fillW < 0.0f) fillW = 0.0f;
    if (fillW > innerW) fillW = innerW;

    // culoare dinamica: low stamina -> rosu, mid -> galben, high -> cyan
    float rr, gg, bb;
    if (stamina < 0.25f) { rr = 1.0f; gg = 0.25f; bb = 0.25f; }
    else if (stamina < 0.5f) { rr = 1.0f; gg = 0.75f; bb = 0.20f; }
    else { rr = 0.20f; gg = 0.95f; bb = 1.00f; }

    // desenam fill-ul doar daca are latime nenula
    if (fillW > 0.0f)
    {
        uiDrawRectFilled(uiShader,
            x + innerPad, y + innerPad,
            fillW, innerH,
            screenW, screenH, rr, gg, bb, 0.95f);
    }

    // eticheta sub bara
    uiDrawText(uiShader, x, y + h + 18.0f, "STAMINA",
        screenW, screenH, 1, 1, 1, 0.90f);

    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
}

// ==========================================================
// MAIN
// ==========================================================

int main()
{
    // clear color = "apa/cer" (se vede si prin fog)
    glClearColor(0.2f, 0.8f, 1.0f, 1.0f);

    // Shader-ul principal: randare obiecte + lumini + fog
    Shader shader("Shaders/vertex_shader.glsl", "Shaders/fragment_shader.glsl");

    // Shader separat pt soare (doar un obiect mic, simplu)
    Shader sunShader("Shaders/sun_vertex_shader.glsl", "Shaders/sun_fragment_shader.glsl");

    // Shader UI 2D (text + shapes)
    Shader uiShader("Shaders/ui_vertex_shader.glsl", "Shaders/ui_fragment_shader.glsl");
    uiInit();

    // ===================== Textures =====================

    GLuint texWood = loadBMP("Resources/Textures/wood.bmp");
    GLuint texRock = loadBMP("Resources/Textures/rock.bmp");
    GLuint texOrange = loadBMP("Resources/Textures/orange.bmp");
    GLuint texUnderSand = loadBMP("Resources/Textures/underwater_sand.bmp");

    GLuint texColumn = loadBMP("Resources/Textures/987.bmp");
    if (texColumn == 0) texColumn = texRock; // fallback daca nu gasim textura

    glEnable(GL_DEPTH_TEST);

    // seed fix pt random (miscare repetabila, util pt testare)
    srand(42);

    MeshLoaderObj loader;

    // Soarele e o sfera
    Mesh sun = loader.loadObj("Resources/Models/sphere.obj");

    // Portals: folosim un cube cu textura de lemn (doar ca material vizual)
    std::vector<Texture> woodTex(1);
    woodTex[0].id = texWood;
    woodTex[0].type = "texture_diffuse";
    Mesh box = loader.loadObj("Resources/Models/cube.obj", woodTex);

    // Floor: plane cu textura underwater sand
    std::vector<Texture> planeTex(1);
    planeTex[0].id = texUnderSand;
    planeTex[0].type = "texture_diffuse";
    Mesh plane = loader.loadObj("Resources/Models/plane.obj", planeTex);

    // Rocks: cube cu textura rock (folosit pt "bolovani" simplificati)
    std::vector<Texture> rockTex(1);
    rockTex[0].id = texRock;
    rockTex[0].type = "texture_diffuse";
    Mesh rockBox = loader.loadObj("Resources/Models/cube.obj", rockTex);

    // Coin (vizual) - aici e incarcat dar in scena coin-ul real e statue
    std::vector<Texture> coinTex(1);
    coinTex[0].id = texOrange;
    coinTex[0].type = "texture_diffuse";
    Mesh coinBox = loader.loadObj("Resources/Models/cube.obj", coinTex);

    // Chest (OBJ cu material din MTL)
    Mesh chestMesh = loader.loadObj("Resources/Models/chest.obj");
    const float CHEST_OBJ_SCALE = 20.0f;

    // Columns (OBJ + textura separata)
    std::vector<Texture> colTex(1);
    colTex[0].id = texColumn;
    colTex[0].type = "texture_diffuse";
    Mesh columnMesh = loader.loadObj("Resources/Models/column.obj", colTex);

    const float COLUMN_SCALE = 0.80f;
    const float COLUMN_BASE_Y_FIX = -300.0f; // corectie pt pivotul/pozitia din model

    // Shark model (inamicul)
    Mesh sharkMesh = loader.loadObj("Resources/Models/Shark.obj");
    const float SHARK_OBJ_SCALE = 8.0f;
    const float SHARK_Y_LIFT = 0.0f;

    // Statue model (relic part)
    Mesh statueMesh = loader.loadObj("Resources/Models/statue.obj");
    const float STATUE_OBJ_SCALE = 0.5f;
    const float STATUE_Y_LIFT = 0.0f;

    // ==========================================================
    // BUILD OCTREE (o singura data)
    // ==========================================================
    // Construim octree din triunghiurile rechinului in spatiul local al modelului.
    // Apoi la runtime facem:
    //   world camera pos -> local shark pos (cu inverse model matrix)
    //   sphereHit(localPos, localRadius)
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

            // bounds pt fiecare triunghi (ajuta la testele din octree)
            t.bounds = triBounds(a, b, c);
            sharkTris.push_back(t);
        }

        // parametri tipici: max depth + max tris per node
        sharkOctree.build(sharkTris, 8, 30);
    }

    // Detectam "just pressed" pt E (ca sa nu fie true in fiecare frame cat tine apasat)
    bool ePrevDown = false;

    // ===================== Game Loop =====================
    while (!window.isPressed(GLFW_KEY_ESCAPE) &&
        glfwWindowShouldClose(window.getWindow()) == 0)
    {
        // stergem frame-ul (color + depth)
        window.clear();

        // calculam deltaTime pt miscari consistente
        float currentFrame = glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        // miscare camera + sprint state
        processKeyboardInput();

        // --- Update stamina ---
        // daca sprintam scade, altfel creste
        if (isSprinting)
        {
            stamina -= STAMINA_DRAIN * deltaTime;
            if (stamina < 0.0f) stamina = 0.0f;
        }
        else
        {
            stamina += STAMINA_REGEN * deltaTime;
            if (stamina > 1.0f) stamina = 1.0f;
        }

        // E just pressed (rising edge)
        bool eDown = window.isPressed(GLFW_KEY_E);
        bool eJustPressed = (eDown && !ePrevDown);
        ePrevDown = eDown;

        float now = glfwGetTime();

        // ==========================================================
        // TELEPORT via portals (doar in main room)
        // ==========================================================
        // Portalul e un trigger AABB. Daca player intra, schimbam zona si setam spawn-ul din cave.
        if (currentZone == 0 && (now - lastTeleportTime > 0.8f))
        {
            glm::vec3 camPos = camera.getCameraPosition();

            // portal stanga -> cave1
            if (pointInAABB(camPos, portalLeftPos, portalHalfSize))
            {
                currentZone = 1;
                qEnteredCave1 = true;
                lastTeleportTime = now;

                // setam origin-ul pesterii si mutam camera acolo
                cave1Origin = caveSpawnLeft;
                camera.setCameraPosition(cave1Origin + glm::vec3(0.0f));

                // rechin porneste in fata spawn-ului si primeste target initial
                cave1ObstaclePos = cave1Origin + glm::vec3(0.0f, 0.0f, -120.0f);
                cave1ObstacleTarget = cave1ObstaclePos;
                cave1NextTargetTime = glfwGetTime() + 0.3f;

                // relic part in apropiere, putin ridicata
                cave1CoinBase = cave1Origin + glm::vec3(0.0f, 10.0f, -120.0f);
                cave1CoinVisible = true;
            }
            // portal dreapta -> cave2
            else if (pointInAABB(camPos, portalRightPos, portalHalfSize))
            {
                currentZone = 2;
                qEnteredCave2 = true;
                lastTeleportTime = now;

                cave2Origin = caveSpawnRight;
                camera.setCameraPosition(cave2Origin + glm::vec3(0.0f));

                // rechin A porneste pe o parte, apoi se misca random; B e oglindit
                cave2EnemyA_Pos = cave2Origin + glm::vec3(120.0f, 0.0f, -220.0f);
                cave2EnemyA_Target = cave2EnemyA_Pos;
                cave2EnemyA_NextTargetTime = glfwGetTime() + 0.3f;

                cave2EnemyB_Pos = mirrorXAroundOrigin(cave2EnemyA_Pos, cave2Origin);

                cave2CoinBase = cave2Origin + glm::vec3(0.0f, 10.0f, -120.0f);
                cave2CoinVisible = true;
            }
        }

        // ==========================================================
        // MAIN ROOM: interaction cu chest (opreste shaking + marcheaza hidden part)
        // ==========================================================
        if (currentZone == 0 && eJustPressed)
        {
            // distanta camera -> chest
            float distToChest = glm::length(camera.getCameraPosition() - chestBasePos);

            // prag destul de mare (e un model mare)
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

            // La intervale random, alegem un nou target pt rechin
            if (nowT > cave1NextTargetTime)
            {
                float rangeX = 220.0f;
                float rangeZ = 220.0f;

                // baza zonei unde se misca (in fata spawn-ului)
                glm::vec3 base = cave1Origin + glm::vec3(0.0f, 0.0f, -180.0f);

                cave1ObstacleTarget = base + glm::vec3(
                    randRange(-rangeX, rangeX),
                    0.0f,
                    randRange(-rangeZ, rangeZ)
                );

                // urmatorul retarget peste 0.8..1.4 secunde
                cave1NextTargetTime = nowT + randRange(0.8f, 1.4f);
            }

            // miscare spre target (cu normalizare)
            glm::vec3 dir = cave1ObstacleTarget - cave1ObstaclePos;
            float len = glm::length(dir);
            if (len > 0.05f)
            {
                dir /= len;
                float speed = 130.0f;
                cave1ObstaclePos += dir * speed * deltaTime;
            }

            // Octree collision: player sphere vs rechin mesh (in local space)
            {
                glm::vec3 camPosW = camera.getCameraPosition();

                // Model matrix pt rechin: translate + rotate + scale
                glm::mat4 M = glm::mat4(1.0f);
                M = glm::translate(M, cave1ObstaclePos + glm::vec3(0.0f, SHARK_Y_LIFT, 0.0f));
                M = glm::rotate(M, glm::radians(90.0f), glm::vec3(0, 1, 0));
                M = glm::scale(M, glm::vec3(SHARK_OBJ_SCALE));

                // ca sa testam in octree (care e construit in local), transformam world->local
                glm::mat4 invM = glm::inverse(M);

                glm::vec3 camPosL = glm::vec3(invM * glm::vec4(camPosW, 1.0f));

                // raza in local: world radius / scale
                float rLocal = PLAYER_RADIUS_WORLD / SHARK_OBJ_SCALE;

                // daca sfera intersecteaza oricare triunghi (octree accelereaza cautarea)
                if (sharkOctree.sphereHit(camPosL, rLocal))
                {
                    float tNow = glfwGetTime();

                    // cooldown ca sa nu facem teleport in fiecare frame
                    if (tNow - lastSharkHitTime > SHARK_HIT_COOLDOWN)
                    {
                        lastSharkHitTime = tNow;

                        // "penalty": te trimite la spawn-ul pesterii
                        camera.setCameraPosition(cave1Origin + glm::vec3(0.0f));
                    }
                }
            }

            // relic part 1 pickup (E aproape de statue)
            if (cave1CoinVisible && eJustPressed)
            {
                // pozitie animata: statue pluteste (sin) pe Y
                float tCoin = glfwGetTime();
                float floatY = std::sin(tCoin * 2.2f) * 3.0f;
                glm::vec3 coinPosNow = cave1CoinBase + glm::vec3(0.0f, floatY, 0.0f);

                float distToCoin = glm::length(camera.getCameraPosition() - coinPosNow);

                // prag de colectare
                if (distToCoin < 35.0f)
                {
                    cave1CoinVisible = false;
                    qGotRelic1 = true;

                    // te intoarce in main room
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

            // retarget periodic pt rechin A
            if (nowT > cave2EnemyA_NextTargetTime)
            {
                float rangeX = 240.0f;
                float rangeZ = 240.0f;

                glm::vec3 base = cave2Origin + glm::vec3(0.0f, 0.0f, -180.0f);

                // in cave2 am restrictionat x (0..rangeX) ca sa fie mai "pe partea lui"
                cave2EnemyA_Target = base + glm::vec3(
                    randRange(0.0f, rangeX),
                    0.0f,
                    randRange(-rangeZ, rangeZ)
                );

                cave2EnemyA_NextTargetTime = nowT + randRange(0.6f, 1.1f);
            }

            // miscare rechin A
            glm::vec3 dirA = cave2EnemyA_Target - cave2EnemyA_Pos;
            float lenA = glm::length(dirA);
            if (lenA > 0.05f)
            {
                dirA /= lenA;
                float speedA = 190.0f;
                cave2EnemyA_Pos += dirA * speedA * deltaTime;
            }

            // rechin B e oglindit fata de origin (simetrie)
            cave2EnemyB_Pos = mirrorXAroundOrigin(cave2EnemyA_Pos, cave2Origin);

            // Octree collision pentru ambii rechini
            {
                glm::vec3 camPosW = camera.getCameraPosition();
                float rLocal = PLAYER_RADIUS_WORLD / SHARK_OBJ_SCALE;

                // functie lambda: construieste transformarea pt un rechin si testeaza in octree
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

                // daca ai lovit oricare, respawn la origin
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
        // Avem 2 "pase":
        //  1) soare (shader separat)
        //  2) scena principala (shader principal cu fog/lights/etc)

        // ---- Sun pass ----
        sunShader.use();

        // Proiectie si view din camera
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

        // pozitia soarelui e relativa la camera (deasupra ei), ca sa fie mereu vizibil
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

        // viewPos in shader (ne trebuie pt specular si spotlight)
        glUniform3f(glGetUniformLocation(shader.getId(), "viewPos"),
            camera.getCameraPosition().x,
            camera.getCameraPosition().y,
            camera.getCameraPosition().z);

        // Fog settings (subacvatic)
        glUniform1i(glGetUniformLocation(shader.getId(), "uUseFog"), 1);
        glUniform3f(glGetUniformLocation(shader.getId(), "uFogColor"), 0.05f, 0.35f, 0.55f);
        glUniform1f(glGetUniformLocation(shader.getId(), "uFogNear"), 40.0f);
        glUniform1f(glGetUniformLocation(shader.getId(), "uFogFar"), 420.0f);
        glUniform1f(glGetUniformLocation(shader.getId(), "uNear"), 0.1f);
        glUniform1f(glGetUniformLocation(shader.getId(), "uFar"), 10000.0f);

        // flag-uri pt portal (transparent / efect special)
        glUniform1i(glGetUniformLocation(shader.getId(), "uIsPortal"), 0);
        glUniform1f(glGetUniformLocation(shader.getId(), "uPortalAlpha"), 1.0f);

        // Material (valori default pt scena)
        setMaterialUniforms(shader, 0.20f, 0.35f, 64.0f);

        // Dir light (lumina ambientala generala)
        setDirLight(shader, true,
            glm::vec3(-0.2f, -1.0f, -0.3f),
            glm::vec3(0.25f, 0.40f, 0.55f),
            0.60f);

        // Point lights: folosim cateva lumini locale (portal, chest, relic)
        int pointCount = 0;

        // helper lambda: acelasi set de parametri de atenuare pt toate (arata consistent)
        auto setNicePoint = [&](int idx, const glm::vec3& pos, const glm::vec3& col, float intensity)
            {
                setPointLight(shader, idx, pos, col, intensity, 1.0f, 0.014f, 0.0007f);
            };

        // 2 lumini pt portale
        setNicePoint(pointCount++, portalLeftPos, glm::vec3(0.2f, 0.7f, 1.0f), 3.0f);
        setNicePoint(pointCount++, portalRightPos, glm::vec3(0.2f, 0.7f, 1.0f), 3.0f);

        // Chest shake: offset mic sinusoidal, doar in main room si doar cat timp nu e "rezolvat"
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

        // pozitia finala a chest-ului: baza + aliniere pe podea + shake
        glm::vec3 chestPos = chestBasePos;
        chestPos.y = mainFloorTopY();
        chestPos += shakeOffset;

        // lumina la chest, doar in main room
        if (currentZone == 0 && pointCount < 8)
            setNicePoint(pointCount++, chestPos + glm::vec3(0.0f, 60.0f, 0.0f), glm::vec3(1.0f, 0.8f, 0.4f), 2.0f);

        // lumina pt relic in cave1 (cand e vizibila)
        if (currentZone == 1 && cave1CoinVisible && pointCount < 8)
            setNicePoint(pointCount++, cave1CoinBase + glm::vec3(0.0f, 30.0f, 0.0f), glm::vec3(0.8f, 0.9f, 1.0f), 1.2f);

        // lumina pt relic in cave2 (cand e vizibila)
        if (currentZone == 2 && cave2CoinVisible && pointCount < 8)
            setNicePoint(pointCount++, cave2CoinBase + glm::vec3(0.0f, 30.0f, 0.0f), glm::vec3(0.8f, 0.9f, 1.0f), 1.2f);

        // shader-ul suporta max 8 point lights
        if (pointCount > 8) pointCount = 8;
        glUniform1i(glGetUniformLocation(shader.getId(), "uNumPointLights"), pointCount);

        // Spot light (lanterna): pozitia si directia camerei
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
            // floor: plane scalat mare
            ModelMatrix = glm::mat4(1.0f);
            ModelMatrix = glm::translate(ModelMatrix, glm::vec3(0.0f, MAIN_FLOOR_Y, 0.0f));
            ModelMatrix = glm::scale(ModelMatrix, glm::vec3(2000.0f, 1.0f, 2000.0f));
            MVP = ProjectionMatrix * ViewMatrix * ModelMatrix;
            glUniformMatrix4fv(MatrixID2, 1, GL_FALSE, &MVP[0][0]);
            glUniformMatrix4fv(ModelMatrixID, 1, GL_FALSE, &ModelMatrix[0][0]);
            plane.draw(shader);

            // portale: desenate transparent (blend)
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

            glUniform1i(glGetUniformLocation(shader.getId(), "uIsPortal"), 1);
            glUniform1f(glGetUniformLocation(shader.getId(), "uPortalAlpha"), 0.35f);

            // portal stanga (un "perete" subtire)
            ModelMatrix = glm::translate(glm::mat4(1.0f), portalLeftPos);
            ModelMatrix = glm::scale(ModelMatrix, glm::vec3(8.0f, 18.0f, 0.5f));
            MVP = ProjectionMatrix * ViewMatrix * ModelMatrix;
            glUniformMatrix4fv(MatrixID2, 1, GL_FALSE, &MVP[0][0]);
            glUniformMatrix4fv(ModelMatrixID, 1, GL_FALSE, &ModelMatrix[0][0]);
            box.draw(shader);

            // portal dreapta
            ModelMatrix = glm::translate(glm::mat4(1.0f), portalRightPos);
            ModelMatrix = glm::scale(ModelMatrix, glm::vec3(8.0f, 18.0f, 0.5f));
            MVP = ProjectionMatrix * ViewMatrix * ModelMatrix;
            glUniformMatrix4fv(MatrixID2, 1, GL_FALSE, &MVP[0][0]);
            glUniformMatrix4fv(ModelMatrixID, 1, GL_FALSE, &ModelMatrix[0][0]);
            box.draw(shader);

            // reset flag-uri portal
            glUniform1i(glGetUniformLocation(shader.getId(), "uIsPortal"), 0);
            glUniform1f(glGetUniformLocation(shader.getId(), "uPortalAlpha"), 1.0f);
            glDisable(GL_BLEND);

            // columns: desenam 2 siruri (stanga/dreapta) pe axa Z
            const int   NUM_COLS_PER_SIDE = 8;
            const float COL_X_LEFT = -260.0f;
            const float COL_X_RIGHT = 260.0f;
            const float COL_Z_START = -60.0f;
            const float COL_Z_STEP = 120.0f;

            // helper: construieste transformarea completa pt un column obj
            auto drawColumnOBJ = [&](float x, float z, bool isLeft)
                {
                    glm::mat4 M = glm::mat4(1.0f);

                    // ridicam coloana ca sa "cada" pe podea corect (model pivot diferit)
                    const float COLUMN_WORLD_LIFT = 240.0f;
                    M = glm::translate(M, glm::vec3(x, mainFloorTopY() + COLUMN_WORLD_LIFT, z));

                    // un yaw mare (rotatie) doar ca sa iasa orientarea cum trebuie in scena
                    float yaw = isLeft ? 222290.0f : -222290.0f;
                    M = glm::rotate(M, glm::radians(yaw), glm::vec3(0, 1, 0));

                    // scale uniform
                    M = glm::scale(M, glm::vec3(COLUMN_SCALE));

                    // corectie suplimentara pe Y in spatiul local al modelului
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

            // rocks: cuburi scalate, puse "pe podea"
            auto drawRock = [&](glm::vec3 pos, glm::vec3 scale)
                {
                    // aliniere: mutam centrul cubului astfel incat baza sa ajunga la podea
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

            // chest: pozitie + scale (cu shake aplicat mai sus)
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
            // floor cave1: plane scalat
            ModelMatrix = glm::mat4(1.0f);
            ModelMatrix = glm::translate(ModelMatrix, cave1Origin + glm::vec3(0.0f, caveFloorY, 0.0f));
            ModelMatrix = glm::scale(ModelMatrix, glm::vec3(caveFloorSize, 1.0f, caveFloorSize));
            MVP = ProjectionMatrix * ViewMatrix * ModelMatrix;
            glUniformMatrix4fv(MatrixID2, 1, GL_FALSE, &MVP[0][0]);
            glUniformMatrix4fv(ModelMatrixID, 1, GL_FALSE, &ModelMatrix[0][0]);
            plane.draw(shader);

            // shark cave1
            ModelMatrix = glm::translate(glm::mat4(1.0f), cave1ObstaclePos + glm::vec3(0.0f, SHARK_Y_LIFT, 0.0f));
            ModelMatrix = glm::rotate(ModelMatrix, glm::radians(90.0f), glm::vec3(0, 1, 0));
            ModelMatrix = glm::scale(ModelMatrix, glm::vec3(SHARK_OBJ_SCALE));
            MVP = ProjectionMatrix * ViewMatrix * ModelMatrix;
            glUniformMatrix4fv(MatrixID2, 1, GL_FALSE, &MVP[0][0]);
            glUniformMatrix4fv(ModelMatrixID, 1, GL_FALSE, &ModelMatrix[0][0]);
            sharkMesh.draw(shader);

            // statue (relic part) doar daca e vizibila
            if (cave1CoinVisible)
            {
                // animatie: pluteste + se roteste
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
            // floor cave2
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

            // shark B (oglindit, yaw invers)
            ModelMatrix = glm::translate(glm::mat4(1.0f), cave2EnemyB_Pos + glm::vec3(0.0f, SHARK_Y_LIFT, 0.0f));
            ModelMatrix = glm::rotate(ModelMatrix, glm::radians(-90.0f), glm::vec3(0, 1, 0));
            ModelMatrix = glm::scale(ModelMatrix, glm::vec3(SHARK_OBJ_SCALE));
            MVP = ProjectionMatrix * ViewMatrix * ModelMatrix;
            glUniformMatrix4fv(MatrixID2, 1, GL_FALSE, &MVP[0][0]);
            glUniformMatrix4fv(ModelMatrixID, 1, GL_FALSE, &ModelMatrix[0][0]);
            sharkMesh.draw(shader);

            // statue cave2
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
        // UI se deseneaza la final, peste tot.
        drawQuestUI(uiShader, window.getWidth(), window.getHeight());
        drawStaminaUI(uiShader, window.getWidth(), window.getHeight());

        // swap buffers + poll events
        window.update();
    }

    return 0;
}

// ==========================================================
// Keyboard input
// ==========================================================
void processKeyboardInput()
{
    // Sprint: SHIFT apasat + stamina suficienta
    bool shiftDown = window.isPressed(GLFW_KEY_LEFT_SHIFT) || window.isPressed(GLFW_KEY_RIGHT_SHIFT);
    isSprinting = (shiftDown && stamina > MIN_STAMINA_TO_SPRINT);

    // viteza de miscare: baza * (sprint? mult : 1) * deltaTime
    float baseMove = 30.0f;
    float moveSpeed = baseMove * (isSprinting ? SPRINT_MULT : 1.0f) * deltaTime;

    // miscari WASD + vertical R/F
    if (window.isPressed(GLFW_KEY_W)) camera.keyboardMoveFront(moveSpeed);
    if (window.isPressed(GLFW_KEY_S)) camera.keyboardMoveBack(moveSpeed);
    if (window.isPressed(GLFW_KEY_A)) camera.keyboardMoveLeft(moveSpeed);
    if (window.isPressed(GLFW_KEY_D)) camera.keyboardMoveRight(moveSpeed);
    if (window.isPressed(GLFW_KEY_R)) camera.keyboardMoveUp(moveSpeed);
    if (window.isPressed(GLFW_KEY_F)) camera.keyboardMoveDown(moveSpeed);

    // rotatia camerei: separata de sprint, ca sa nu sara sensitivity-ul
    float rotSpeed = 30.0f * deltaTime;

    if (window.isPressed(GLFW_KEY_LEFT))  camera.rotateOy(rotSpeed);
    if (window.isPressed(GLFW_KEY_RIGHT)) camera.rotateOy(-rotSpeed);
    if (window.isPressed(GLFW_KEY_UP))    camera.rotateOx(rotSpeed);
    if (window.isPressed(GLFW_KEY_DOWN))  camera.rotateOx(-rotSpeed);
}
