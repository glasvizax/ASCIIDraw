#include "Texture.h"

#include "stb_image.h"
#include <filesystem>

namespace fs = std::filesystem;

Texture loadTexture(std::string_view filename, FilteringType filtering_type)
{
    Texture tex;
    std::string path = g_data_path + fs::path(filename).filename().string();

    int tex_width, tex_height, tex_channels;

    stbi_uc* tex_data = stbi_load(path.c_str(), &tex_width, &tex_height, &tex_channels, 1);

    if (tex_data)
    {
        tex.init(xm::ivec2(tex_width, tex_height), tex_data, filtering_type);
    }
    else
    {
        assert(false && "couldn't load texture");
    }

    stbi_image_free(tex_data);
    return tex;
}