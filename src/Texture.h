#pragma once

#include <algorithm>
#include <string>
#include <vector>
#include <xm/xm.h>

#include "DataInfo.h"
#include "IntensityUtils.h"
#include "BroadcastExecutor.h"

#include "Aliases.h"

enum class FilteringType : uint8
{
    NEAREST,
    BILINEAR
//  TRILINEAR  not implemented 
};

class Texture
{
    void initAsMipMap(const Texture& prev);

public:
    void init(xm::ivec2 size, uchar* data, FilteringType filtering_type = FilteringType::NEAREST);

    char getValueUV(xm::vec2 uv) const;
    void setValueUV(xm::vec2 uv, char value);

    size_t getIndex(xm::ivec2 pos) const;

    template<typename UVPred>
    void fillPattern(const UVPred& pred, BroadcastExecutor& exec);
    void fillSymbol(char symbol, BroadcastExecutor& exec);

    void setFilteringType(FilteringType type) 
    {
        m_filtering_type = type;
    }

    void clear(char clear_value);

private:
    char getValue(xm::ivec2 pos) const;

    void setValue(xm::ivec2 pos, char value)
    {
        m_texture_buffer[getIndex(pos)] = value;
    }

private:
    std::string m_texture_buffer;
    xm::ivec2 m_size;
    FilteringType m_filtering_type = FilteringType::BILINEAR;
    //std::vector<Texture> m_mip_maps;
};

Texture loadTexture(std::string_view filename, FilteringType filtering_type = FilteringType::NEAREST);

template<typename UVPred>
inline void Texture::fillPattern(const UVPred& pred, BroadcastExecutor& exec)
{
    exec.foreachSync(
        [
            pred, size = m_size
        ]
        (char& elem, uint idx)
        {
            xm::vec2 uv;
            uv.u = (idx % size.x) / static_cast<float>(size.x);
            uv.v = (idx / size.x) / static_cast<float>(size.y);
            elem = pred(uv);
        }, std::span(m_texture_buffer));
}

class Cubemap 
{
public:
    void init(xm::ivec2 size, uchar* data[6], FilteringType filtering_type = FilteringType::NEAREST);

    char getValueByCoords(xm::vec3 xyz) const;

    void setFilteringType(FilteringType type)
    {
        m_filtering_type = type;
    }

private:

    xm::ivec2 m_size;
    FilteringType m_filtering_type = FilteringType::BILINEAR;
    // 0 -> +x
    // 1 -> -x
    // 2 -> +y
    // 3 -> -y
    // 4 -> +z
    // 5 -> -z
    Texture m_textures[6];
};

Cubemap loadCubemap(std::string_view filenames[6], FilteringType filtering_type = FilteringType::NEAREST);
