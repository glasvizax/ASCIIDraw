
#include <algorithm>
#include <array>

#include "IntensityUtils.h"

constexpr char g_intensity_symbols[] = " .'`^\",:;Il!i><~+?][}{1)(|\\/tfjrxnuvczXYUJCLQ0OZmwqpdbkhao*#MW&8%B@$";
constexpr uint g_intensity_symbols_size = sizeof(g_intensity_symbols) - 1;

char g_min_intensity_symbol = g_intensity_symbols[0];
char g_max_intensity_symbol = g_intensity_symbols[g_intensity_symbols_size - 1];
char g_mid_intensity_symbol = g_intensity_symbols[g_intensity_symbols_size / 2];

char getIntensitySymbolF(float intensity)
{
    uint idx = ((g_intensity_symbols_size - 1) * intensity) + 0.5f;
    return g_intensity_symbols[std::clamp(idx, 0u, g_intensity_symbols_size - 1)];
}

char getIntensitySymbolUC(unsigned char intensity)
{
    uint idx = (intensity * (g_intensity_symbols_size - 1)) / 255.0f + 0.5f;
    return g_intensity_symbols[idx];
}

char getIntensitySymbolUI(uint intensity)
{
    return g_intensity_symbols[intensity];
}

consteval auto initIndexMap()
{
    std::array<uint, (sizeof(char) << 8)> index_map{};

    for (uint i = 0; i < g_intensity_symbols_size; ++i)
    {
        index_map[g_intensity_symbols[i]] = i;
    }

    return index_map;
}

constexpr auto g_index_map = initIndexMap();

uint getSymbolIntensity(char symbol)
{
    return g_index_map[symbol];
}

char getNextIntensity()
{
    static int index = 0;
    int idx = index % (g_intensity_symbols_size);
    index++;
    return g_intensity_symbols[idx];
}
