#version 400

layout (location = 0) in vec3 pos;
layout (location = 1) in vec3 normals;
layout (location = 2) in vec2 texCoord;

out vec2 TexCoords;
out vec3 Normal;
out vec3 FragPos;

uniform mat4 MVP;
uniform mat4 model;

void main()
{
    TexCoords = texCoord;
    FragPos   = vec3(model * vec4(pos, 1.0));
    Normal    = mat3(transpose(inverse(model))) * normals;

    gl_Position = MVP * vec4(pos, 1.0);
}
