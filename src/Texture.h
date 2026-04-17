#pragma once

#include <algorithm>
#include <string>

#include <xm/xm.h>

#include "DataInfo.h"
#include "IntensityUtils.h"

class Texture
{
    std::string m_texture_buffer;
    xm::ivec2 m_size;

public:

    void init(xm::ivec2 size, unsigned char* data)
    {
        m_size = size;
        m_texture_buffer.resize(m_size.x * m_size.y);
        std::transform(data, data + m_texture_buffer.size(), m_texture_buffer.data(), getIntensitySymbolUC);
    }

    char getValueUV(xm::vec2 uv)
    {
        xm::ivec2 pos;

        pos.y = uv.y * (m_size.y - 1);
        pos.x = uv.x * (m_size.x - 1);

        return getValue(pos);
    }

    void setValueUV(xm::vec2 uv, char value)
    {
        xm::ivec2 pos;

        pos.y = uv.y * (m_size.y - 1);
        pos.x = uv.x * (m_size.x - 1);

        return setValue(pos, value);
    }

    size_t getIndex(xm::ivec2 pos)
    {
        size_t y = m_size.y - 1 - pos.y;
        return y * m_size.x + pos.x;
    }

    void clear(char clear_value)
    {
        std::memset(m_texture_buffer.data(), clear_value, m_texture_buffer.size());
    }

private:

    char getValue(xm::ivec2 pos)
    {
        return m_texture_buffer[getIndex(pos)];
    }

    void setValue(xm::ivec2 pos, char value)
    {
        m_texture_buffer[getIndex(pos)] = value;
    }

};

Texture loadTexture(std::string_view filename);