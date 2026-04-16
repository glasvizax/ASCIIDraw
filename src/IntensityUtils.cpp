
#include <algorithm>
#include "IntensityUtils.h"

char g_intensity_symbols[] = " .:!/r(l1Z4H9W8$@";
int g_intensity_symbols_size = sizeof(g_intensity_symbols) - 1;

char g_clear_symbol = g_intensity_symbols[1];
char g_max_intensity_symbol = g_intensity_symbols[g_intensity_symbols_size - 1];

char getIntensitySymbolF(float intensity)
{
    int idx = ((g_intensity_symbols_size - 1) * intensity) + 0.5f;
    return g_intensity_symbols[std::clamp(idx, 0, g_intensity_symbols_size - 1)];
}

char getIntensitySymbolUC(unsigned char intensity)
{
    int idx = (intensity * (g_intensity_symbols_size - 1)) / 255.0f + 0.5f;
    return g_intensity_symbols[idx];
}

char getNextIntensity()
{
    static int index = 0;
    int idx = index % (g_intensity_symbols_size);
    index++;
    return g_intensity_symbols[idx];
}
