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
