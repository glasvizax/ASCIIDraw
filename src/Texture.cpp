#include "Texture.h"

Texture loadTexture(std::string_view filename)
{
    Texture tex;
    std::string path = g_data_path + std::string(filename);

    int tex_width, tex_height, tex_channels;
    stbi_uc* tex_data = stbi_load(path.c_str(), &tex_width, &tex_height, &tex_channels, 1);

    if (tex_data)
    {
        tex.init(xm::ivec2(tex_width, tex_height), tex_data);
    }
    else
    {
        assert(false && "couldn't load texture");
    }

    stbi_image_free(tex_data);
    return tex;
}