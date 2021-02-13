//
// The MIT License (MIT)
//
// Copyright 2020 Karolpg
//
// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), 
// to deal in the Software without restriction, including without limitation the rights to #use, copy, modify, merge, publish, distribute, sublicense, 
// and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR #COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, 
// WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
//

#pragma once

#include <cstdint>

namespace ImgUtils
{

enum DataType
{
    DT_Uint8,
    DT_Uint16,
    DT_Uint32,
    DT_Uint64,
    DT_Int8,
    DT_Int16,
    DT_Int32,
    DT_Int64,
    DT_Float32,
    DT_Float64,
};

enum DataOrder
{
    Pixel,         // e.g. [RGB, RGB, RGB]
    Component,     // e.g. [RRR, GGG, BBB]
    PixelTile,
    ComponentTile,
};

// The reason I haven't done it template is because I want to have it as simple header.
// Under the hood I back to normal types without DataType
void resize(uint32_t inW, uint32_t inH, uint32_t inC, const void* inData, DataType inType, DataOrder inOrder, uint32_t inTileSize,
            uint32_t outW, uint32_t outH, uint32_t outC, void* outData, DataType outType, DataOrder outOrder, uint32_t outTileSize,
            bool keepProportion,
            const void* outClearValue, uint32_t* outNewX, uint32_t* outNewY, uint32_t* outNewW, uint32_t* outNewH // function give new position of scaled data - important mainly when keepProportion == true
            );

}
