#pragma once
#include <string>
#include <vector>

#include "mesh.h"
#include "texture.h"

class MeshLoaderObj
{
public:
    MeshLoaderObj();

    // Uses OBJ + (optional) MTL if present
    Mesh loadObj(const std::string& filename);

    // Uses OBJ geometry only (ignores MTL) and applies given textures
    Mesh loadObj(const std::string& filename, std::vector<Texture> textures);

private:
    // internal: load geometry; if loadMaterials=true, read mtllib/usemtl/map_Kd
    Mesh loadObjInternal(const std::string& filename, bool loadMaterials);
};
