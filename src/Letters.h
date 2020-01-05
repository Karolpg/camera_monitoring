#pragma once

#include <string>
#include <vector>
#include <cinttypes>

struct Letter{
  const uint32_t w;
  const uint32_t h;
  const std::vector<uint8_t> data; // binary data 0, / 1,
};


const Letter& getLetterData(char c);


void drawString(std::vector<uint8_t>& data, uint32_t w, uint32_t h, uint32_t c,
                uint32_t left, uint32_t up,
                const std::string& text, float letterScale, uint32_t letterColorRgba, bool adjustScale = true);
