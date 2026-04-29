#pragma once

#include <string>
#include <cstdlib>
#include <xm/xm.h>
#include <xm/math_helpers.h>

template<typename T>
class Framebuffer
{
    std::conditional_t<std::is_same_v<T, char>, std::string, std::vector<T>> m_buffer;
    xm::ivec2 m_size;
    uint32_t m_buff_size;

    friend void draw();

public:
    void init(xm::ivec2 size)
    {
        m_size = size;
        m_buff_size = m_size.x * m_size.y;
        m_buffer.resize(m_buff_size);
    }

    size_t getIndex(xm::ivec2 pos)
    {
        size_t y = m_size.y - 1 - pos.y;
        return y * m_size.x + pos.x;
    }

    T getValue(xm::ivec2 pos)
    {
        return m_buffer[getIndex(pos)];
    }

    void setValue(xm::ivec2 pos, T value)
    {
        m_buffer[getIndex(pos)] = value;
    }


    void clear(T clear_value)
    {
        if constexpr (sizeof(T) > 1)
        {
            std::fill(m_buffer.data(), m_buffer.data() + m_buff_size, clear_value);
        }
        else
        {
            std::memset(m_buffer.data(), clear_value, m_buff_size);
        }
    }
};

using CharFramebuffer = Framebuffer<char>;
using U8Framebuffer = Framebuffer<uint8_t>;
using FloatFramebuffer = Framebuffer<float>;

