#pragma once

#include <algorithm>
#include <string>
#include <vector>
#include <xm/xm.h>

#include "DataInfo.h"
#include "IntensityUtils.h"

enum class FilteringType : uint8_t
{
    NEAREST,
    BILINEAR
//  TRILINEAR  not implemented 
};

class Texture
{
    std::string m_texture_buffer;
    xm::ivec2 m_size;
    FilteringType m_filtering_type = FilteringType::BILINEAR;
    std::vector<Texture> m_mip_maps;

    void initAsMipMap(const Texture& prev)
    {
        m_size.x = prev.m_size.x / 2;
        m_size.y = prev.m_size.y / 2;

        m_texture_buffer.resize(m_size.x * m_size.y);
        
        for(int x = 0; x < m_size.x; ++x)
        {
            for (int y = 0; y < m_size.y; ++y)
            {
                xm::ivec2 idx(x, y);
                xm::ivec2 prev_idx = idx * 2;
                uint average = 0;
                for(int i = 0; i < 2; ++i)
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

public:

    void init(xm::ivec2 size, unsigned char* data, FilteringType filtering_type = FilteringType::NEAREST)
    {
        m_size = size;
        m_texture_buffer.resize(m_size.x * m_size.y);
        std::transform(data, data + m_texture_buffer.size(), m_texture_buffer.data(), getIntensitySymbolUC);
        /*
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

    char getValueUV(xm::vec2 uv) const
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
                int c11 = getSymbolIntensity(getValue(i + xm::ivec2{ 1,1 }));

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

    void setValueUV(xm::vec2 uv, char value)
    {
        xm::ivec2 pos;

        pos.y = uv.y * (m_size.y - 1);
        pos.x = uv.x * (m_size.x - 1);
    }

    size_t getIndex(xm::ivec2 pos) const
    {
        size_t y = m_size.y - 1 - pos.y;
        return y * m_size.x + pos.x;
    }

    void clear(char clear_value)
    {
        std::memset(m_texture_buffer.data(), clear_value, m_texture_buffer.size());
    }

private:

    char getValue(xm::ivec2 pos) const
    {
        xm::ivec2 correct_pos = { pos.x % (m_size.x), pos.y % (m_size.y) };
        return m_texture_buffer[getIndex(correct_pos)];
    }

    void setValue(xm::ivec2 pos, char value)
    {
        m_texture_buffer[getIndex(pos)] = value;
    }
};

Texture loadTexture(std::string_view filename, FilteringType filtering_type = FilteringType::NEAREST);

