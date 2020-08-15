#pragma once

#include "Frame.h"

class MovementAnalyzer
{
public:
    MovementAnalyzer();

    void feedAnalyzer(const FrameU8 &frame, const FrameDescr& descr);
private:
    void resize(const FrameU8 &in, const FrameDescr& inDescr, FrameF32& out);


    std::vector<FrameF32> m_cyclicBuffer;
    FrameDescr m_descr;
    uint32_t m_itemCounter = 0;
};
