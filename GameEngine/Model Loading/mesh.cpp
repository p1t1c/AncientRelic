// mesh.cpp
// ------------------------------------------------------------
// Clasa Mesh:
//  - tine datele geometrice (vertices + indices) si optional texturi
//  - creeaza VAO/VBO/IBO in setup()
//  - in draw() seteaza sampler-ele pt texturi si apeleaza glDrawElements
//
// Observatie: layout-ul atributelor este:
//  0 -> position (vec3)
//  1 -> normals   (vec3)
//  2 -> texcoords (vec2)
// Asta trebuie sa fie consistent cu vertex shader-ul.

#include "mesh.h"
#include <vector>
#include <iostream>

Mesh::Mesh() {}

Mesh::Mesh(std::vector<Vertex> vertices, std::vector<int> indices)
{
    // mutam vectorii (evitam copieri)
    this->vertices = std::move(vertices);
    this->indices = std::move(indices);

    // Pregatim buffer-ele si atributele in VAO
    // Important: folosim setup() (are position + normal + uv).
    setup();
}

Mesh::Mesh(std::vector<Vertex> vertices, std::vector<int> indices, std::vector<Texture> textures)
{
    this->vertices = std::move(vertices);
    this->indices = std::move(indices);
    this->textures = std::move(textures);

    // La fel: pregatim VAO/VBO/IBO
    setup();
}

Mesh::~Mesh() {}

void Mesh::setTextures(std::vector<Texture> textures)
{
    // Setam textura/texurile pentru mesh.
    // VAO-ul nu depinde de texturi, deci nu e obligatoriu sa refacem setup().
    this->textures = std::move(textures);

    // setup();
}

// render the mesh
void Mesh::draw(Shader shader)
{
    // Asiguram ca shader-ul e activ atunci cand setam uniforme si desenam
    shader.use();

    unsigned int diffuseNr = 1;
    unsigned int specularNr = 1;
    unsigned int normalNr = 1;
    unsigned int heightNr = 1;

    // Pentru fiecare textura:
    //  - activam texture unit i
    //  - setam uniform-ul potrivit: texture_diffuse1, texture_diffuse2, etc.
    //  - bind textura
    for (unsigned int i = 0; i < textures.size(); i++)
    {
        glActiveTexture(GL_TEXTURE0 + i);

        std::string number;
        std::string name = textures[i].type;

        // numerotarea ajuta daca shader-ul suporta mai multe texturi de acelasi tip
        if (name == "texture_diffuse")
            number = std::to_string(diffuseNr++);
        else if (name == "texture_specular")
            number = std::to_string(specularNr++);
        else if (name == "texture_normal")
            number = std::to_string(normalNr++);
        else if (name == "texture_height")
            number = std::to_string(heightNr++);

        std::string uniformName = name + number;

        // cautam locatia uniform-ului in shader
        int loc = glGetUniformLocation(shader.getId(), uniformName.c_str());
        if (loc == -1)
        {
            // Daca uniform-ul nu exista in shader, nu e neaparat eroare (poate shader-ul nu foloseste tipul ala).
            // Putem lasa silent, sau log pentru debug.
            // std::cout << "WARN: Uniform not found in shader: " << uniformName << "\n";
        }
        else
        {
            // setam sampler-ul sa foloseasca texture unit i
            glUniform1i(loc, (int)i);
        }

        glBindTexture(GL_TEXTURE_2D, textures[i].id);
    }

    // desenare: VAO contine deja configurarea atributelor + IBO pentru indices
    glBindVertexArray(vao);
    glDrawElements(GL_TRIANGLES, (GLsizei)indices.size(), GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);

    // reset la default (optional, dar ajuta sa nu "scapam" pe alte draw-uri)
    glActiveTexture(GL_TEXTURE0);
}

void Mesh::setup()
{
    // Cream VAO/VBO/IBO pentru mesh
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glGenBuffers(1, &ibo);

    // Bind VAO (de acum incolo, atributele setate raman in acest VAO)
    glBindVertexArray(vao);

    // VBO: incarca array-ul de Vertex
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER,
        vertices.size() * sizeof(Vertex),
        vertices.data(),
        GL_STATIC_DRAW);

    // Indices sunt vector<int>, dar glDrawElements cu GL_UNSIGNED_INT.
    // Convertim intr-un vector<unsigned int> ca sa fie compatibil.
    std::vector<unsigned int> uindices;
    uindices.reserve(indices.size());
    for (int idx : indices)
        uindices.push_back((unsigned int)idx);

    // IBO/EBO: incarca indices
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
        uindices.size() * sizeof(unsigned int),
        uindices.data(),
        GL_STATIC_DRAW);

    // Atribute in VAO:
    // layout(0) position: vec3, la offset 0
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);

    // layout(1) normals: vec3, la offset-ul campului normals din struct Vertex
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normals));

    // layout(2) texcoords: vec2, la offset-ul campului textureCoords
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, textureCoords));

    // Unbind VAO ca sa nu modificam din greseala in alta parte
    glBindVertexArray(0);
}
