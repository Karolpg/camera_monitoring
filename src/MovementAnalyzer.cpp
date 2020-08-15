#include "MovementAnalyzer.h"

MovementAnalyzer::MovementAnalyzer()
{
    double frameRate = 30.0/1.0; // [fps]
    double length = 3;// [s]
    m_cyclicBuffer.resize(static_cast<size_t>(frameRate*length));

    m_descr.components = 3;
    m_descr.width = 512;
    m_descr.height = 512;
    uint32_t ctr = 0;
    for (FrameF32& f : m_cyclicBuffer) {
        f.data.resize(m_descr.height*m_descr.width*m_descr.components, 0);
        f.bufferIdx = ctr++;
    }

}

void MovementAnalyzer::feedAnalyzer(const FrameU8 &frame, const FrameDescr &descr)
{
    ++m_itemCounter;
    auto newIdx = m_itemCounter % m_cyclicBuffer.size();
    auto prevIdx = (m_itemCounter - 1) % m_cyclicBuffer.size();
    FrameF32 &f = m_cyclicBuffer[newIdx];
    resize(frame, descr, f);
}

void MovementAnalyzer::resize(const FrameU8 &in, const FrameDescr &inDescr, FrameF32 &out)
{

}
