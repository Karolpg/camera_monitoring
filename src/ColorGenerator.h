
#include <vector>
#include <cinttypes>

class ColorGenerator
{
public:
    //Alpha set to 255
    static std::vector<uint32_t> generateColorsRGB(uint32_t count);
private:
    static const uint8_t STEP_VISIBLE_FOR_EYE = 7;
    static const uint8_t START_VALUE = 100;
    static const uint8_t MAX_VALUE = 255;
};
