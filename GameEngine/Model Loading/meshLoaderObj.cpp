// meshLoaderObj.cpp
// ------------------------------------------------------------
// Loader simplu de OBJ (si optional MTL).
// Ce face:
//  - citeste v / vt / vn si f
//  - construieste un vector de Vertex + un vector de indices (triangulare pt faces cu >3 varfuri)
//  - daca OBJ nu are normale (vn), le genereaza din triunghiuri
//  - daca e loadMaterials=true, incearca sa citeasca mtllib + usemtl si sa incarce map_Kd (texture diffuse)
//
// Observatie: e un loader "practic", nu implementare completa a standardului OBJ/MTL.

#include "meshLoaderObj.h"
#include "stringTokenizer.h"
#include "texture.h"

#include <iostream>
#include <fstream>
#include <unordered_map>
#include <unordered_set>
#include <cmath>

#include <glm.hpp>

// ---------- path helpers ----------
// Avem nevoie de path helpers pentru ca MTL-ul si textura pot fi relative la folderul OBJ-ului.

static std::string getDir(const std::string& path)
{
    // cautam ultimul separator de director / sau \

    size_t p = path.find_last_of("/\\");
    if (p == std::string::npos) return "";
    return path.substr(0, p + 1);
}

static std::string joinPath(const std::string& dir, const std::string& rel)
{
    // daca rel e gol, ramanem cu dir
    if (rel.empty()) return dir;

    // Windows absolute path: C:\...
    if (rel.size() >= 2 && rel[1] == ':') return rel;

    // Absolute path (Unix /) sau path care incepe cu backslash
    if (!rel.empty() && (rel[0] == '/' || rel[0] == '\\')) return rel;

    // altfel e relativ la folderul OBJ
    return dir + rel;
}

static std::string replaceExtToBMP(const std::string& p)
{
    // proiectul foloseste BMP loader, deci convertim extensia in .bmp
    size_t dot = p.find_last_of('.');
    if (dot == std::string::npos) return p + ".bmp";
    return p.substr(0, dot) + ".bmp";
}

// ---------- tiny math helpers ----------
// Implementari mici ca sa evitam dependinte/complexitate in loader (strict ce ne trebuie).

static inline glm::vec3 cross3(const glm::vec3& a, const glm::vec3& b)
{
    return glm::vec3(
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    );
}

static inline float dot3(const glm::vec3& a, const glm::vec3& b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

static inline float len3(const glm::vec3& v)
{
    return std::sqrt(dot3(v, v));
}

static inline glm::vec3 normalize3(const glm::vec3& v)
{
    // normalizare safe (evitam impartirea la aproape-zero)
    float l = len3(v);
    if (l > 1e-6f) return v / l;
    return glm::vec3(0.0f, 1.0f, 0.0f);
}

// ---------- MTL parser (map_Kd) ----------
// Citim strict ce ne intereseaza: asocierea material -> map_Kd (diffuse texture).
// Pentru proiect e suficient, nu avem nevoie de specular/normal maps etc.

static std::unordered_map<std::string, std::string> parseMTL_MapKd(const std::string& mtlPath)
{
    std::unordered_map<std::string, std::string> matToMapKd;

    std::ifstream f(mtlPath);
    if (!f.good())
    {
        std::cout << "[MTL] Cannot open: " << mtlPath << "\n";
        return matToMapKd;
    }

    std::string line;
    std::string currentMat;

    while (std::getline(f, line))
    {
        std::vector<std::string> t;
        _stringTokenize(line, t);
        if (t.empty()) continue;

        // definirea materialului
        if (t[0] == "newmtl" && t.size() >= 2)
        {
            currentMat = t[1];
        }
        // textura difuza
        else if (t[0] == "map_Kd" && t.size() >= 2 && !currentMat.empty())
        {
            matToMapKd[currentMat] = t[1];
        }
    }

    return matToMapKd;
}

// ---------- generate normals (if OBJ has no vn) ----------
// Daca modelul nu are vn, generam normalele din triunghiuri.
// Facem:
//  1) setam normalele la 0
//  2) pt fiecare triunghi: calculam normal face (cross(e1,e2)) si o adunam la varfurile lui
//  3) normalizam la final fiecare vertex normal

static void generateNormalsFromIndices(
    std::vector<Vertex>& vertices,
    const std::vector<int>& indices,
    const std::vector<glm::vec3>& vpos)
{
    if (vertices.empty() || indices.size() < 3) return;
    if (vpos.size() != vertices.size()) return;

    for (auto& v : vertices)
        v.normals = glm::vec3(0.0f, 0.0f, 0.0f);

    for (size_t i = 0; i + 2 < indices.size(); i += 3)
    {
        int i0 = indices[i];
        int i1 = indices[i + 1];
        int i2 = indices[i + 2];

        // verificari simple de siguranta
        if (i0 < 0 || i1 < 0 || i2 < 0) continue;
        if (i0 >= (int)vertices.size() || i1 >= (int)vertices.size() || i2 >= (int)vertices.size()) continue;

        glm::vec3 p0 = vpos[i0];
        glm::vec3 p1 = vpos[i1];
        glm::vec3 p2 = vpos[i2];

        glm::vec3 e1 = p1 - p0;
        glm::vec3 e2 = p2 - p0;

        // normal face (orientarea depinde de winding)
        glm::vec3 fn = normalize3(cross3(e1, e2));

        // acumulam la fiecare vertex
        vertices[i0].normals += fn;
        vertices[i1].normals += fn;
        vertices[i2].normals += fn;
    }

    // normalizam rezultatul (media normalelor incident)
    for (auto& v : vertices)
        v.normals = normalize3(v.normals);
}

// ---------- class ----------

MeshLoaderObj::MeshLoaderObj() {}

Mesh MeshLoaderObj::loadObj(const std::string& filename)
{
    // versiunea "full": citeste si materiale (mtl) daca exista
    return loadObjInternal(filename, true);
}

Mesh MeshLoaderObj::loadObj(const std::string& filename, std::vector<Texture> textures)
{
    // versiunea "manual textures":
    // nu citim MTL, doar incarcam geometria si atasam texturile primite din afara
    Mesh mesh = loadObjInternal(filename, false);
    mesh.setTextures(textures);
    return mesh;
}

Mesh MeshLoaderObj::loadObjInternal(const std::string& filename, bool loadMaterials)
{
    std::vector<Vertex> vertices;
    std::vector<int> indices;
    std::vector<glm::vec3> vpos; // pastram pozitia pt fiecare vertex creat (util pt normale generate)

    std::ifstream file(filename.c_str(), std::ios::in | std::ios::binary);
    if (!file.good())
    {
        std::cout << "Obj model not found " << filename << std::endl;
        std::terminate();
    }

    // folderul de baza al OBJ-ului (pt mtllib / map_Kd relative)
    std::string baseDir = getDir(filename);

    std::string line;
    std::vector<std::string> tokens, facetokens;

    // listele clasice din OBJ: v, vn, vt
    std::vector<glm::vec3> positions; positions.reserve(1000);
    std::vector<glm::vec3> normals;   normals.reserve(1000);
    std::vector<glm::vec2> texcoords; texcoords.reserve(1000);

    bool anyNormalUsed = false; // daca am folosit vn din fisier, nu mai generam

    // materiale (doar daca loadMaterials)
    std::string mtlFile;
    std::unordered_set<std::string> usedMaterials;
    std::string currentMat = "None";

    while (std::getline(file, line))
    {
        _stringTokenize(line, tokens);
        if (tokens.empty()) continue;

        // comentarii in OBJ
        if (tokens[0].size() > 0 && tokens[0][0] == '#') continue;

        // mtllib: numele fisierului MTL
        if (loadMaterials && tokens[0] == "mtllib" && tokens.size() >= 2)
        {
            mtlFile = tokens[1];
            continue;
        }

        // usemtl: materialul folosit de urmatoarele face-uri
        if (loadMaterials && tokens[0] == "usemtl" && tokens.size() >= 2)
        {
            currentMat = tokens[1];
            usedMaterials.insert(currentMat);
            continue;
        }

        // v x y z
        if (tokens.size() > 3 && tokens[0] == "v")
            positions.push_back(glm::vec3(_stringToFloat(tokens[1]), _stringToFloat(tokens[2]), _stringToFloat(tokens[3])));

        // vn x y z
        if (tokens.size() > 3 && tokens[0] == "vn")
            normals.push_back(glm::vec3(_stringToFloat(tokens[1]), _stringToFloat(tokens[2]), _stringToFloat(tokens[3])));

        // vt u v
        if (tokens.size() > 2 && tokens[0] == "vt")
            texcoords.push_back(glm::vec2(_stringToFloat(tokens[1]), _stringToFloat(tokens[2])));

        // f ... (faces)
        if (tokens.size() >= 4 && tokens[0] == "f")
        {
            // incercam sa detectam formatul de face:
            //  1) v
            //  2) v/vt
            //  3) v//vn
            //  4) v/vt/vn
            unsigned int face_format = 0;
            if (tokens[1].find("//") != std::string::npos) face_format = 3; // v//vn

            _faceTokenize(tokens[1], facetokens);

            if (facetokens.size() == 3) face_format = 4;     // v/vt/vn
            else if (facetokens.size() == 2) face_format = 2; // v/vt
            else face_format = 1;                              // v

            // pt triangulare: retinem primul vertex al face-ului
            unsigned int index_of_first_vertex_of_face = (unsigned int)-1;

            for (unsigned int num_token = 1; num_token < tokens.size(); num_token++)
            {
                // in unele fisiere, dupa f pot aparea comentarii
                if (tokens[num_token].at(0) == '#') break;

                _faceTokenize(tokens[num_token], facetokens);

                glm::vec3 P(0.0f);
                glm::vec3 N(0.0f, 1.0f, 0.0f);
                glm::vec2 UV(0.0f);

                if (face_format == 1) // v
                {
                    int p_index = _stringToInt(facetokens[0]);
                    if (p_index > 0) p_index -= 1;
                    else p_index = (int)positions.size() + p_index;

                    P = positions[p_index];
                    vertices.push_back(Vertex(P.x, P.y, P.z));
                }
                else if (face_format == 2) // v/vt
                {
                    int p_index = _stringToInt(facetokens[0]);
                    if (p_index > 0) p_index -= 1;
                    else p_index = (int)positions.size() + p_index;

                    int t_index = _stringToInt(facetokens[1]);
                    if (t_index > 0) t_index -= 1;
                    else t_index = (int)texcoords.size() + t_index;

                    P = positions[p_index];
                    UV = texcoords[t_index];
                    vertices.push_back(Vertex(P.x, P.y, P.z, UV.x, UV.y));
                }
                else if (face_format == 3) // v//vn
                {
                    int p_index = _stringToInt(facetokens[0]);
                    if (p_index > 0) p_index -= 1;
                    else p_index = (int)positions.size() + p_index;

                    int n_index = _stringToInt(facetokens[1]);
                    if (n_index > 0) n_index -= 1;
                    else n_index = (int)normals.size() + n_index;

                    P = positions[p_index];
                    N = normals[n_index];
                    vertices.push_back(Vertex(P.x, P.y, P.z, N.x, N.y, N.z));
                    anyNormalUsed = true;
                }
                else // v/vt/vn
                {
                    int p_index = _stringToInt(facetokens[0]);
                    if (p_index > 0) p_index -= 1;
                    else p_index = (int)positions.size() + p_index;

                    int t_index = _stringToInt(facetokens[1]);
                    if (t_index > 0) t_index -= 1;
                    else t_index = (int)texcoords.size() + t_index;

                    int n_index = _stringToInt(facetokens[2]);
                    if (n_index > 0) n_index -= 1;
                    else n_index = (int)normals.size() + n_index;

                    P = positions[p_index];
                    UV = texcoords[t_index];
                    N = normals[n_index];

                    vertices.push_back(Vertex(P.x, P.y, P.z, N.x, N.y, N.z, UV.x, UV.y));
                    anyNormalUsed = true;
                }

                // vpos tine minte pozitia pentru vertex-ul creat (1:1 cu vertices)
                vpos.push_back(P);

                // Primele 3 vertex-uri ale face-ului formeaza primul triunghi
                if (num_token < 4)
                {
                    if (num_token == 1)
                        index_of_first_vertex_of_face = (unsigned int)(vertices.size() - 1);

                    indices.push_back((int)vertices.size() - 1);
                }
                else
                {
                    // Daca face-ul are mai mult de 3 vertex-uri, il triangulam tip "fan":
                    // (v0, v(i-1), v(i))
                    indices.push_back((int)index_of_first_vertex_of_face);
                    indices.push_back((int)vertices.size() - 2);
                    indices.push_back((int)vertices.size() - 1);
                }
            }
        }
    }

    // Daca fisierul nu a avut vn, generam noi normale per vertex
    if (!anyNormalUsed)
        generateNormalsFromIndices(vertices, indices, vpos);

    // --- materials only if requested ---
    if (loadMaterials)
    {
        std::vector<Texture> textures;

        if (!mtlFile.empty())
        {
            std::string mtlPath = joinPath(baseDir, mtlFile);
            auto matToMap = parseMTL_MapKd(mtlPath);

            // Alegem o textura din materialele folosite (prima gasita)
            std::string mapKd;
            for (const auto& m : usedMaterials)
            {
                auto it = matToMap.find(m);
                if (it != matToMap.end())
                {
                    mapKd = it->second;
                    break;
                }
            }
            // fallback: daca nu avem match direct, luam prima intrare
            if (mapKd.empty() && !matToMap.empty())
                mapKd = matToMap.begin()->second;

            if (!mapKd.empty())
            {
                std::string texPath = joinPath(baseDir, mapKd);
                std::string texBmp = replaceExtToBMP(texPath);

                GLuint tid = loadBMP(texBmp.c_str());
                if (tid != 0)
                {
                    Texture t;
                    t.id = tid;
                    t.type = "texture_diffuse";
                    textures.push_back(t);
                }
                else
                {
                    std::cout << "[MTL] Couldn't load BMP: " << texBmp << "\n";
                }
            }
        }

        std::cout << "Loading: " << filename << std::endl;

        // daca am incarcat texturi din mtl, construim Mesh cu textures
        if (!textures.empty())
            return Mesh(vertices, indices, textures);
    }

    // altfel, Mesh fara texturi
    std::cout << "Loading: " << filename << std::endl;
    return Mesh(vertices, indices);
}
