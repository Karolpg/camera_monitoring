#include "ColorGenerator.h"

std::vector<uint32_t> ColorGenerator::generateColorsRGB(uint32_t count) {
    std::vector<uint32_t> result(count);
    uint32_t SINGLE_PHASE_VALUES = (MAX_VALUE - START_VALUE) / STEP_VISIBLE_FOR_EYE;
    uint32_t ALL_PHASE_VALUES = SINGLE_PHASE_VALUES*3 + SINGLE_PHASE_VALUES*2*3 + SINGLE_PHASE_VALUES*3;
    if (count > ALL_PHASE_VALUES) {
        for (uint32_t i = 0; i < count; ++i) {
            result[i] = 0xff000000 | i;
        }
    }
    else {
        uint32_t step = ALL_PHASE_VALUES * STEP_VISIBLE_FOR_EYE / count;
        uint32_t prevPhase = static_cast<uint32_t>(-1);
        uint32_t phase = 0;
        uint32_t rest = 0;
        uint8_t r = 0;
        uint8_t g = 0;
        uint8_t b = 0;
        uint8_t* activeC = nullptr;
        for (uint32_t i = 0; i < count; ++i) {
            switch (phase) {
                //R
                case 0: {
                    if (prevPhase != phase) {
                        prevPhase = phase;
                        r = START_VALUE;
                        g = 0;
                        b = 0;
                    }
                    else {
                        r += step;
                    }

                    result[i] = 0xff000000u | ((0x000000ffu & b) << 16) | ((0x000000ffu & g) << 8) | ((0x000000ffu & r));

                    uint32_t nextVal = step + r;
                    if (nextVal > MAX_VALUE) {
                        phase += 1;
                        rest = nextVal - MAX_VALUE;
                    }
                    break;
                }
                //G
                case 1: {
                    if (prevPhase != phase) {
                        prevPhase = phase;
                        if (rest + START_VALUE < MAX_VALUE) {
                            r = 0;
                            g = START_VALUE + static_cast<uint8_t>(rest);
                            b = 0;
                            rest = 0;
                        }
                        else {
                            phase += 1;
                            rest = rest + START_VALUE - MAX_VALUE;
                            --i;//we know we have to retry this step
                            continue;
                        }
                    }
                    else {
                        g += step;
                    }

                    result[i] = 0xff000000u | ((0x000000ffu & b) << 16) | ((0x000000ffu & g) << 8) | ((0x000000ffu & r));

                    uint32_t nextVal = step + g;
                    if (nextVal > MAX_VALUE) {
                        phase += 1;
                        rest = nextVal - MAX_VALUE;
                    }
                    break;
                }
                //B
                case 2: {
                    if (prevPhase != phase) {
                        prevPhase = phase;
                        if (rest + START_VALUE < MAX_VALUE) {
                            r = 0;
                            g = 0;
                            b = START_VALUE + static_cast<uint8_t>(rest);
                            rest = 0;
                        }
                        else {
                            phase += 1;
                            rest = rest + START_VALUE - MAX_VALUE;
                            --i;//we know we have to retry this step
                            continue;
                        }
                    }
                    else {
                        b += step;
                    }

                    result[i] = 0xff000000u | ((0x000000ffu & b) << 16) | ((0x000000ffu & g) << 8) | ((0x000000ffu & r));

                    uint32_t nextVal = step + b;
                    if (nextVal > MAX_VALUE) {
                        phase += 1;
                        rest = nextVal - MAX_VALUE;
                    }
                    break;
                }

                //RG
                case 3: {
                    if (prevPhase != phase) {
                        prevPhase = phase;
                        if (rest + START_VALUE < MAX_VALUE) {
                            r = START_VALUE + static_cast<uint8_t>(rest);
                            g = START_VALUE;
                            b = 0;
                            activeC = &r;
                            rest = 0;
                        }
                        else if(rest + 2*START_VALUE < 2*MAX_VALUE) {
                            r = MAX_VALUE;
                            g = static_cast<uint8_t>(rest + 2*START_VALUE - 2*MAX_VALUE);
                            b = 0;
                            activeC = &g;
                            rest = 0;
                        }
                        else {
                            phase += 1;
                            rest = rest + 2*START_VALUE - 2*MAX_VALUE;
                            --i;//we know we have to retry this step
                            continue;
                        }
                    }
                    else {
                        *activeC += (rest ? rest : step);
                        rest = 0;
                    }

                    result[i] = 0xff000000u | ((0x000000ffu & b) << 16) | ((0x000000ffu & g) << 8) | ((0x000000ffu & r));

                    CHECK_NEXT_PHASE3:
                    uint32_t nextVal = (rest ? rest : step) + *activeC;
                    if (nextVal > 2*MAX_VALUE) {
                        phase += 1;
                        rest = nextVal - 2*MAX_VALUE;
                    }
                    else if(nextVal > MAX_VALUE) {
                        rest = nextVal - MAX_VALUE;
                        if (activeC == &r) {
                            activeC = &g;
                            if (rest)
                            goto CHECK_NEXT_PHASE3;
                        }
                        else {
                            phase += 1;
                        }
                    }
                    break;
                }

                //GB
                case 4: {
                    if (prevPhase != phase) {
                        prevPhase = phase;
                        if (rest + START_VALUE < MAX_VALUE) {
                            r = 0;
                            g = START_VALUE + static_cast<uint8_t>(rest);
                            b = START_VALUE;
                            activeC = &g;
                            rest = 0;
                        }
                        else if(rest + 2*START_VALUE < 2*MAX_VALUE) {
                            r = 0;
                            g = MAX_VALUE;
                            b = static_cast<uint8_t>(rest + 2*START_VALUE - 2*MAX_VALUE);
                            activeC = &b;
                            rest = 0;
                        }
                        else {
                            phase += 1;
                            rest = rest + 2*START_VALUE - 2*MAX_VALUE;
                            --i;//we know we have to retry this step
                            continue;
                        }
                    }
                    else {
                        *activeC += (rest ? rest : step);
                        rest = 0;
                    }

                    result[i] = 0xff000000u | ((0x000000ffu & b) << 16) | ((0x000000ffu & g) << 8) | ((0x000000ffu & r));

                    CHECK_NEXT_PHASE4:
                    uint32_t nextVal = (rest ? rest : step) + *activeC;
                    if (nextVal > 2*MAX_VALUE) {
                        phase += 1;
                        rest = nextVal - 2*MAX_VALUE;
                    }
                    else if(nextVal > MAX_VALUE) {
                        rest = nextVal - MAX_VALUE;
                        if (activeC == &g) {
                            activeC = &b;
                            if (rest)
                            goto CHECK_NEXT_PHASE4;
                        }
                        else {
                            phase += 1;
                        }
                    }
                    break;
                }

                //RB
                case 5: {
                    if (prevPhase != phase) {
                        prevPhase = phase;
                        if (rest + START_VALUE < MAX_VALUE) {
                            r = START_VALUE + static_cast<uint8_t>(rest);
                            g = 0;
                            b = START_VALUE;
                            activeC = &r;
                            rest = 0;
                        }
                        else if(rest + 2*START_VALUE < 2*MAX_VALUE) {
                            r = MAX_VALUE;
                            g = 0;
                            b = static_cast<uint8_t>(rest + 2*START_VALUE - 2*MAX_VALUE);
                            activeC = &b;
                            rest = 0;
                        }
                        else {
                            phase += 1;
                            rest = rest + 2*START_VALUE - 2*MAX_VALUE;
                            --i;//we know we have to retry this step
                            continue;
                        }
                    }
                    else {
                        *activeC += (rest ? rest : step);
                        rest = 0;
                    }

                    result[i] = 0xff000000u | ((0x000000ffu & b) << 16) | ((0x000000ffu & g) << 8) | ((0x000000ffu & r));

                    CHECK_NEXT_PHASE5:
                    uint32_t nextVal = (rest ? rest : step) + *activeC;
                    if (nextVal > 2*MAX_VALUE) {
                        phase += 1;
                        rest = nextVal - 2*MAX_VALUE;
                    }
                    else if(nextVal > MAX_VALUE) {
                        rest = nextVal - MAX_VALUE;
                        if (activeC == &r) {
                            activeC = &b;
                            if (rest)
                            goto CHECK_NEXT_PHASE5;
                        }
                        else {
                            phase += 1;
                        }
                    }
                    break;
                }

                //RGB
                case 6: {
                    if (prevPhase != phase) {
                        prevPhase = phase;
                        if (rest + START_VALUE < MAX_VALUE) {
                            r = START_VALUE + static_cast<uint8_t>(rest);
                            g = START_VALUE;
                            b = START_VALUE;
                            activeC = &r;
                            rest = 0;
                        }
                        else if(rest + 2*START_VALUE < 2*MAX_VALUE) {
                            r = MAX_VALUE;
                            g = static_cast<uint8_t>(rest + 2*START_VALUE - 2*MAX_VALUE);
                            b = START_VALUE;
                            activeC = &g;
                            rest = 0;
                        }
                        else if(rest + 3*START_VALUE < 3*MAX_VALUE) {
                            r = MAX_VALUE;
                            g = MAX_VALUE;
                            b = static_cast<uint8_t>(rest + 3*START_VALUE - 3*MAX_VALUE);
                            activeC = &b;
                            rest = 0;
                        }
                        else {
                            return result; //there is no more phases
                        }
                    }
                    else {
                        *activeC += (rest ? rest : step);
                        rest = 0;
                    }

                    result[i] = 0xff000000u | ((0x000000ffu & b) << 16) | ((0x000000ffu & g) << 8) | ((0x000000ffu & r));

                    CHECK_NEXT_PHASE6:
                    uint32_t nextVal = (rest ? rest : step) + *activeC;
                    if(nextVal > MAX_VALUE) {
                        rest = nextVal - MAX_VALUE;
                        if (activeC == &r) {
                            r = MAX_VALUE;
                            activeC = &g;
                            if (rest) {
                                goto CHECK_NEXT_PHASE6;
                            }
                        }
                        else if (activeC == &g) {
                            g = MAX_VALUE;
                            activeC = &b;
                            if (rest) {
                                goto CHECK_NEXT_PHASE6;
                            }
                        }
                        else {
                            return result;
                        }
                    }
                    break;
                }
            }
        }
    }
    return result;
}
