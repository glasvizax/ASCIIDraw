#include "Texture.h"

#include "stb_image.h"

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

Cubemap loadCubemap(std::string_view filenames[6], FilteringType filtering_type)
{
    Cubemap tex3d;
    stbi_uc* tex_datas[6];
    int tex_width, tex_height;
    
    for (uint i = 0; i < 6; ++i) 
    {
        std::string path = g_data_path + fs::path(filenames[i]).filename().string();

        int width, height, tex_channels;
        tex_datas[i] = stbi_load(path.c_str(), &width, &height, &tex_channels, 1);
        if (i == 0) 
        {
            tex_width = width;
            tex_height = height;
        }
        else 
        {
            if(tex_width != width)
            {
                assert(false && "texture3d received textures of not the same size");
            }
            else if(tex_height != height)
            {
                assert(false && "texture3d received textures of not the same size");
            }
        }

        if (!tex_datas[i])
        {
            assert(false && "couldn't load texture"); 
        }
    }

    tex3d.init(xm::ivec2(tex_width, tex_height), tex_datas, filtering_type);

    for (uint i = 0; i < 6; ++i)
    {
        stbi_image_free(tex_datas[i]);
    }
    
    return tex3d;
}

void Texture::initAsMipMap(const Texture& prev)
{
    m_size.x = prev.m_size.x / 2;
    m_size.y = prev.m_size.y / 2;

    m_texture_buffer.resize(m_size.x * m_size.y);

    for (int x = 0; x < m_size.x; ++x)
    {
        for (int y = 0; y < m_size.y; ++y)
        {
            xm::ivec2 idx(x, y);
            xm::ivec2 prev_idx = idx * 2;
            uint average = 0;
            for (int i = 0; i < 2; ++i)
            {
                for (int j = 0; j < 2; ++j)
                {
                    average += getSymbolIntensity(prev.getValue(prev_idx + xm::ivec2(i, j)));
                }
            }
            average /= 4;
            m_texture_buffer[getIndex(xm::ivec2(x, y))] = getIntensitySymbolUI(average);
        }
    }
}

void Texture::init(xm::ivec2 size, uchar* data, FilteringType filtering_type)
{
    m_size = size;
    m_texture_buffer.resize(m_size.x * m_size.y);
    m_filtering_type = filtering_type;
    if (data)
    {
        std::transform(data, data + m_texture_buffer.size(), m_texture_buffer.data(), getIntensitySymbolUC);
    }/*
    if(filtering_type == FilteringType::TRILINEAR)
    {
        int gen_levels = std::min(std::log2(m_size.x), std::log2(m_size.y)) - 1;
        m_mip_maps.reserve(gen_levels);
        const Texture* prev = this;
        for(int i = 0; i < gen_levels; ++i)
        {
            m_mip_maps.emplace_back();
            m_mip_maps.back().initAsMipMap(*prev);
            prev = &m_mip_maps.back();
        }
    }*/
}

char Texture::getValueUV(xm::vec2 uv) const
{
    xm::ivec2 pos;
    switch (m_filtering_type)
    {
    case FilteringType::NEAREST:
    {
        pos.y = uv.y * (m_size.y - 1);
        pos.x = uv.x * (m_size.x - 1);

        return getValue(pos);
    }
    case FilteringType::BILINEAR:
    {
        float yf = uv.y * (m_size.y - 1);
        float xf = uv.x * (m_size.x - 1);
        xm::ivec2 i = { static_cast<int>(xf), static_cast<int>(yf) };

        int c00 = getSymbolIntensity(getValue(i));
        int c10 = getSymbolIntensity(getValue(i + xm::ivec2{ 1,0 }));
        int c01 = getSymbolIntensity(getValue(i + xm::ivec2{ 0,1 }));
        int c11 = getSymbolIntensity(getValue(i + xm::ivec2{ 1,0 }));

        float ty = yf - i.y;
        float tx = xf - i.x;

        float a = c00 + (c10 - c00) * tx;
        float b = c01 + (c11 - c01) * tx;
        uint final = (a + (b - a) * ty) + 0.5f;
        return getIntensitySymbolUI(final);
    }
    /*
    case FilteringType::TRILINEAR:
    {
        pos.y = uv.y * (m_size.y - 1);
        pos.x = uv.x * (m_size.x - 1);

        return getValue(pos);
    }*/
    default:
    {
        break;
    }
    }
}

void Texture::setValueUV(xm::vec2 uv, char value)
{
    xm::ivec2 pos;

    pos.y = uv.y * (m_size.y - 1);
    pos.x = uv.x * (m_size.x - 1);
}

size_t Texture::getIndex(xm::ivec2 pos) const
{
    size_t y = m_size.y - 1 - pos.y;
    return y * m_size.x + pos.x;
}

void Texture::fillSymbol(char symbol, BroadcastExecutor& exec)
{
    exec.foreachSync(
        [
            symbol
        ]
        (char& elem, uint idx)
        {
            elem = symbol;
        }, std::span(m_texture_buffer));
}

void Texture::clear(char clear_value)
{
    std::memset(m_texture_buffer.data(), clear_value, m_texture_buffer.size());
}

char Texture::getValue(xm::ivec2 pos) const
{
    xm::ivec2 correct_pos = { pos.x % (m_size.x), pos.y % (m_size.y) };
    return m_texture_buffer[getIndex(correct_pos)];
}


void Cubemap::init(xm::ivec2 size, uchar* data[6], FilteringType filtering_type)
{
    m_size = size;
    m_filtering_type = filtering_type;
    for (uint i = 0; i < 6; ++i) 
    {
        m_textures[i].init(m_size, data[i], filtering_type);
    }
}

char Cubemap::getValueByCoords(xm::vec3 xyz) const
{
    float x_abs = std::abs(xyz.x);
    float y_abs = std::abs(xyz.y);
    float z_abs = std::abs(xyz.z);
    
    if (x_abs >= y_abs && x_abs >= z_abs)
    {
        float mult = 1.0f / x_abs;
        xyz.x = xyz.x > 0.0f ? 1.0f : -1.0f;
        xyz.y *= mult;
        xyz.z *= mult;

        if (xyz.x > 0.0f)
        {
            xm::vec2 uv((-xyz.z + 1.0f) / 2.0f, (xyz.y + 1.0f) / 2.0f);
            return m_textures[0].getValueUV(uv);
        }
        else
        {
            xm::vec2 uv((xyz.z + 1.0f) / 2.0f, (xyz.y + 1.0f) / 2.0f);
            return m_textures[1].getValueUV(uv);
        }

    }
    else if (y_abs >= x_abs && y_abs >= z_abs)
    {
        float mult = 1.0f / y_abs;
        xyz.x *= mult;
        xyz.z *= mult;

        if (xyz.y > 0.0f)
        {
            xm::vec2 uv((xyz.x + 1.0f) / 2.0f, (-xyz.z + 1.0f) / 2.0f);
            return m_textures[2].getValueUV(uv);
        }
        else
        {
            xm::vec2 uv((xyz.x + 1.0f) / 2.0f, (xyz.z + 1.0f) / 2.0f);
            return m_textures[3].getValueUV(uv);
        }
    }
    else
    {
        float mult = 1.0f / z_abs;
        xyz.x *= mult;
        xyz.y *= mult;
        if(xyz.z > 0.0f)
        {
            xm::vec2 uv((xyz.x + 1.0f) / 2.0f, (xyz.y + 1.0f) / 2.0f);
            return m_textures[4].getValueUV(uv);
        }
        else 
        {
            xm::vec2 uv((-xyz.x + 1.0f) / 2.0f, (xyz.y + 1.0f) / 2.0f);
            return m_textures[5].getValueUV(uv);
        }
    }
}
