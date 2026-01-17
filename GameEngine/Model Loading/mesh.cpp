#include "mesh.h"
#include <vector>
#include <iostream>

Mesh::Mesh() {}

Mesh::Mesh(std::vector<Vertex> vertices, std::vector<int> indices)
{
    this->vertices = std::move(vertices);
    this->indices = std::move(indices);

    // IMPORTANT: NU folosi setup2() (fara UV). Vrei mereu layout 0/1/2.
    setup();
}

Mesh::Mesh(std::vector<Vertex> vertices, std::vector<int> indices, std::vector<Texture> textures)
{
    this->vertices = std::move(vertices);
    this->indices = std::move(indices);
    this->textures = std::move(textures);

    setup();
}

Mesh::~Mesh() {}

void Mesh::setTextures(std::vector<Texture> textures)
{
    this->textures = std::move(textures);

    // VAO are deja atributele ok; doar texturile s-au schimbat, nu e obligatoriu setup().
    // Dar e safe sa-l lasi asa. Daca vrei, poti comenta setup().
    // setup();
}

// render the mesh
void Mesh::draw(Shader shader)
{
    // IMPORTANT: uniformele se seteaza pe programul curent
    shader.use(); // sau glUseProgram(shader.getId());

    unsigned int diffuseNr = 1;
    unsigned int specularNr = 1;
    unsigned int normalNr = 1;
    unsigned int heightNr = 1;

    for (unsigned int i = 0; i < textures.size(); i++)
    {
        glActiveTexture(GL_TEXTURE0 + i);

        std::string number;
        std::string name = textures[i].type;

        if (name == "texture_diffuse")
            number = std::to_string(diffuseNr++);
        else if (name == "texture_specular")
            number = std::to_string(specularNr++);
        else if (name == "texture_normal")
            number = std::to_string(normalNr++);
        else if (name == "texture_height")
            number = std::to_string(heightNr++);

        std::string uniformName = name + number;

        int loc = glGetUniformLocation(shader.getId(), uniformName.c_str());
        if (loc == -1)
        {
            // Optional debug:
            // std::cout << "WARN: Uniform not found in shader: " << uniformName << "\n";
        }
        else
        {
            glUniform1i(loc, (int)i);
        }

        glBindTexture(GL_TEXTURE_2D, textures[i].id);
    }

    glBindVertexArray(vao);
    glDrawElements(GL_TRIANGLES, (GLsizei)indices.size(), GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);

    glActiveTexture(GL_TEXTURE0);
}

void Mesh::setup()
{
    // create buffers
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glGenBuffers(1, &ibo);

    // bind buffers
    glBindVertexArray(vao);

    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER,
        vertices.size() * sizeof(Vertex),
        vertices.data(),
        GL_STATIC_DRAW);

    // indices sunt vector<int>, dar desenam cu GL_UNSIGNED_INT -> convertim
    std::vector<unsigned int> uindices;
    uindices.reserve(indices.size());
    for (int idx : indices)
        uindices.push_back((unsigned int)idx);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
        uindices.size() * sizeof(unsigned int),
        uindices.data(),
        GL_STATIC_DRAW);

    // position
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);

    // normals
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normals));

    // texcoords
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, textureCoords));

    glBindVertexArray(0);
}
