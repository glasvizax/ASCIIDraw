#pragma once

#include <xm/xm.h>

class BroadcastExecutor;

void pushLineRaw(char symbol, xm::ivec2 a, xm::ivec2 b);
void pushTriangleScanlineRaw(char symbol, xm::ivec2 a, xm::ivec2 b, xm::ivec2 c);
void pushTriangleBarycenterRaw(char symbol, xm::ivec2 a, xm::ivec2 b, xm::ivec2 c);
void pushTriangleBarycenterRaw(char symbol, xm::ivec2 a, xm::ivec2 b, xm::ivec2 c, BroadcastExecutor& exec);

template <uint8_t N, typename T>
void swap(xm::vector<N, T>& a, xm::vector<N, T>& b)
{
    xm::vector<N, T> tmp = a;
    a = b;
    b = tmp;
}
