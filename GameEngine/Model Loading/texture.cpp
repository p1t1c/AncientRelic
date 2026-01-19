#include "texture.h"
#include <iostream>
#include <vector>
#include <cstdint>

GLuint loadBMP(const char* imagepath)
{
    printf("Reading image %s\n", imagepath);

    unsigned char header[54];
    unsigned int dataPos = 0;
    unsigned int width = 0, height = 0;
    unsigned int imageSize = 0;

    FILE* file = nullptr;
    errno_t err = fopen_s(&file, imagepath, "rb");
    if (err || !file)
    {
        printf("%s could not be opened.\n", imagepath);
        return 0;
    }

    if (fread(header, 1, 54, file) != 54)
    {
        printf("Not a correct BMP file (header)\n");
        fclose(file);
        return 0;
    }

    if (header[0] != 'B' || header[1] != 'M')
    {
        printf("Not a correct BMP file (signature)\n");
        fclose(file);
        return 0;
    }

    // Doar BMP 24-bit necomprimat
    if (*(int*)&(header[0x1E]) != 0) { printf("Not a correct BMP file (compressed)\n"); fclose(file); return 0; }
    if (*(int*)&(header[0x1C]) != 24) { printf("Not a correct BMP file (not 24 bpp)\n"); fclose(file); return 0; }

    dataPos = *(int*)&(header[0x0A]);
    imageSize = *(int*)&(header[0x22]);
    width = *(int*)&(header[0x12]);
    height = *(int*)&(header[0x16]);

    if (dataPos == 0) dataPos = 54;

    // BMP rows sunt padded la multiplu de 4 bytes
    const unsigned int rowSizeNoPad = width * 3;
    const unsigned int rowSizePad = (rowSizeNoPad + 3) & ~3u;   // align to 4
    const unsigned int fileImageSize = rowSizePad * height;

    // Citim raw data (cu padding)
    std::vector<unsigned char> raw(fileImageSize);

    fseek(file, (long)dataPos, SEEK_SET);
    size_t readBytes = fread(raw.data(), 1, fileImageSize, file);
    fclose(file);

    if (readBytes != fileImageSize)
    {
        printf("BMP read failed / truncated\n");
        return 0;
    }

    // Scoatem padding-ul ca sa incarcam in OpenGL ca packed BGR
    std::vector<unsigned char> data(width * height * 3);
    for (unsigned int y = 0; y < height; y++)
    {
        memcpy(
            data.data() + y * rowSizeNoPad,
            raw.data() + y * rowSizePad,
            rowSizeNoPad
        );
    }

    GLuint textureID = 0;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);

    // IMPORTANT: 3 bytes/pixel => UNPACK_ALIGNMENT=1 ca sa nu apara artefacte
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, (GLsizei)width, (GLsizei)height, 0,
        GL_BGR, GL_UNSIGNED_BYTE, data.data());

    glGenerateMipmap(GL_TEXTURE_2D);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);

    // Anisotropic filtering (daca exista) -> reduce shimmer-ul pe podea
#ifdef GL_TEXTURE_MAX_ANISOTROPY_EXT
#ifdef GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT
    {
        GLfloat maxAniso = 0.0f;
        glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &maxAniso);
        if (maxAniso > 0.0f)
            glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, maxAniso);
    }
#endif
#endif

    glBindTexture(GL_TEXTURE_2D, 0);

    // Optional: revenim la default
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);

    return textureID;
}