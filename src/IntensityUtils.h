#pragma once

#include "Aliases.h"

extern char g_max_intensity_symbol;
extern char g_min_intensity_symbol;
extern char g_mid_intensity_symbol;

char getIntensitySymbolF(float intensity);
char getIntensitySymbolUC(uchar intensity);
char getIntensitySymbolUI(uint intensity);

char getNextIntensity();

uint getSymbolIntensity(char symbol);