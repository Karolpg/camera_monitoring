#include "ImgUtils.h"
#include <math.h>
#include <assert.h>
#include <type_traits>
#include <type_traits>

//#include <iostream>
//#include <iomanip>

namespace ImgUtils
{

template<typename InData, typename OutData>
OutData avarageColor(uint32_t x, uint32_t y, uint32_t c,
                       const InData* data, uint32_t width, uint32_t height, uint32_t components,
                        uint32_t componentMove, uint32_t pixelMove,
                       double wAspect, double hAspect)
{
    assert(c <= components);
    if (wAspect >= 1.0) {
        if (hAspect >= 1.0) {
            uint32_t rawX = static_cast<uint32_t>(wAspect * x);
            uint32_t rawY = static_cast<uint32_t>(hAspect * y);


            // choose type:
            // - InData is float => float
            // - InData is double => double
            // - InData is int8 or int16 or int32 => int32  if bigger then take int64
            // - InData is uint8 or uint16 or uint32 => uint32  if bigger then take uint64
            using ACC_TYPE = typename std::conditional<std::is_floating_point<InData>::value,
                                                       typename std::conditional<(sizeof(InData) > sizeof(float)), double, float>::type,
                                                       typename std::conditional<std::is_unsigned<InData>::value,
                                                                                 typename std::conditional<(sizeof(InData) > sizeof(uint32_t)), uint64_t, uint32_t>::type,
                                                                                 typename std::conditional<(sizeof(InData) > sizeof(int32_t)),   int64_t,  int32_t>::type
                                                                                >::type
                                                      >::type;
            ACC_TYPE accColor = 0;

            uint32_t maskX = static_cast<uint32_t>(wAspect + 0.5);
            uint32_t maskY = static_cast<uint32_t>(hAspect + 0.5);
            uint32_t rawXEnd = std::min(rawX+maskX, width);
            uint32_t rawYEnd = std::min(rawY+maskY, height);
            uint32_t maskSize = (rawXEnd - rawX) * (rawYEnd - rawY);

            for (uint32_t imgY = rawY; imgY < rawYEnd; ++imgY) {
                for (uint32_t imgX = rawX; imgX < rawXEnd; ++imgX) {
                    accColor += static_cast<ACC_TYPE>(
                                    data[(imgY*width+imgX)*pixelMove + c*componentMove]
                                );
                }
            }

            // choose bigger or smaller floating point
            using RESULT_TYPE = typename std::conditional<std::is_floating_point<InData>::value,
                                                          typename std::conditional<(sizeof(InData) > sizeof(float)), double, float>::type,
                                                          typename std::conditional<(sizeof(InData) > sizeof(int32_t)), double, float>::type
                                                         >::type;
            RESULT_TYPE maxComponentDivider = RESULT_TYPE(1.0);
            if (!std::is_floating_point<InData>::value && std::is_floating_point<OutData>::value) {  // int -> float
                maxComponentDivider = RESULT_TYPE(size_t(std::numeric_limits<InData>::max()));
            }
            else if(std::is_floating_point<InData>::value && !std::is_floating_point<OutData>::value) { // float -> int
                //there could be a problem e.g. if user want to express Uint8 values as Uint32
                maxComponentDivider = RESULT_TYPE(1) / RESULT_TYPE(size_t(std::numeric_limits<OutData>::max()));
            }
            // Currently do nothing with integers signed/unsigned, scaling with the range etc.

            OutData outVal = OutData(RESULT_TYPE(accColor) / maskSize / maxComponentDivider);
            //std::cout << "outVal: " << std::fixed << std::setprecision(5) << outVal << " avg: " << RESULT_TYPE(accColor) / maskSize
            //          << " div: " << maxComponentDivider << " max:" << std::numeric_limits<InData>::max() << "\n";
            return outVal;
        }
        else {
            assert(!"Not implemented yet!");
        }
    }
    else {
        assert(!"Not implemented yet!");
    }
    return OutData(0);
}


template<typename InType, typename OutType>
void resizeTmpl(uint32_t inW, uint32_t inH, uint32_t inC, const InType *inData, ImgUtils::DataOrder inOrder, uint32_t inTileSize,
                      uint32_t outW, uint32_t outH, uint32_t outC, OutType *outData, ImgUtils::DataOrder outOrder, uint32_t outTileSize,
                      bool keepProportion,
                      const OutType* outClearValue, uint32_t* outNewX, uint32_t* outNewY, uint32_t* outNewW, uint32_t* outNewH)
{
    uint32_t newX = 0;
    uint32_t newY = 0;
    uint32_t newW = outW;
    uint32_t newH = outH;

    if (keepProportion) {
        if ((static_cast<float>(outW)/inW) < (static_cast<float>(outH)/inH)) {
            newW = outW;
            newH = (inH * outW)/inW;
            newX = 0;
            newY = (outH-newH)/2;
        } else {
            newH = outH;
            newW = (inW * outH)/inH;
            newX = (outW-newW)/2;
            newY = 0;
        }
    }

    if (outNewX) {
        *outNewX = newX;
    }
    if (outNewY) {
        *outNewY = newY;
    }
    if (outNewW) {
        *outNewW = newW;
    }
    if (outNewH) {
        *outNewH = newH;
    }


    const uint32_t cMax = std::min(inC, outC);

    double wAspect = static_cast<double>(inW) / static_cast<double>(newW);
    double hAspect = static_cast<double>(inH) / static_cast<double>(newH);

    bool inIsTiled = inOrder == PixelTile || inOrder == ComponentTile;
    bool outIsTiled = outOrder == PixelTile || outOrder == ComponentTile;


    if (!inIsTiled && !outIsTiled) {
        uint32_t inComponentMove = inOrder == Pixel ? 1 : inW*inH;
        uint32_t inPixelMove = inOrder == Pixel ? inC : 1;

        uint32_t outComponentMove = outOrder == Pixel ? 1 : outW*outH;
        uint32_t outPixelMove = outOrder == Pixel ? outC : 1;

        for (uint32_t component = 0; component < cMax; ++component) {
            #pragma omp parallel for
            for (uint32_t y = newY; y < newY + newH; ++y) {
                for (uint32_t x = newX; x < newX + newW; ++x) {
                    uint32_t idx = (y*outW + x)*outPixelMove + component*outComponentMove;
                    outData[idx] = avarageColor<InType, OutType>(x-newX, y-newY, component,
                                                                 inData, inW, inH, inC,
                                                                 inComponentMove, inPixelMove,
                                                                 wAspect, hAspect);
                }
            }

        }

        if (newX > 0 || newY > 0) {
            for (uint32_t component = 0; component < outC; ++component) {
                //clear left right site
                if (newX > 0) {
                    #pragma omp parallel for
                    for (uint32_t y = 0; y < newH; ++y) {
                        uint32_t idx = y*outW*outPixelMove + component*outComponentMove;
                        std::fill(&outData[idx], &outData[idx+newX*outPixelMove], outClearValue[component]); // fill left
                        std::fill(&outData[idx + (newX + newW)*outPixelMove], &outData[idx+outW*outPixelMove], outClearValue[component]); // fill right
                    }
                }
                else if (newY > 0) {
                    //clear up
                    #pragma omp parallel for
                    for (uint32_t y = 0; y < newY; ++y) {
                        uint32_t idx = y*outW*outPixelMove + component*outComponentMove;
                        std::fill(&outData[idx], &outData[idx+outW*outPixelMove], outClearValue[component]);
                    }

                    //clear bottom
                    #pragma omp parallel for
                    for (uint32_t y = newY + newH; y < outH; ++y) {
                        uint32_t idx = y*outW*outPixelMove + component*outComponentMove;
                        std::fill(&outData[idx], &outData[idx+outW*outPixelMove], outClearValue[component]);
                    }
                }
            }
        }
    }
    else {
        assert(!"Not implemented yet!");
    }
}



void resize(uint32_t inW, uint32_t inH, uint32_t inC, const void *inData, ImgUtils::DataType inType, ImgUtils::DataOrder inOrder, uint32_t inTileSize,
                      uint32_t outW, uint32_t outH, uint32_t outC, void *outData, ImgUtils::DataType outType, ImgUtils::DataOrder outOrder, uint32_t outTileSize,
                      bool keepProportion,
                      const void* outClearValue, uint32_t* outNewX, uint32_t* outNewY, uint32_t* outNewW, uint32_t* outNewH)
{
#define runResizeTempl(inDataT, outDataT, clearValT) \
    resizeTmpl(inW, inH, inC, inDataT, inOrder, inTileSize, \
               outW, outH, outC, outDataT, outOrder, outTileSize,\
               keepProportion,\
               clearValT, outNewX, outNewY, outNewW, outNewH);

#define takeCastAndRun(outType)\
    outType* outDataT = reinterpret_cast<outType*>(outData); \
    const outType* clearValT = reinterpret_cast<const outType*>(outClearValue); \
    runResizeTempl(inDataT, outDataT, clearValT);

#define castOutAndRun() \
    switch (outType) { \
        case DT_Uint8  : { takeCastAndRun(uint8_t ); break; }\
        case DT_Uint16 : { takeCastAndRun(uint16_t); break; }\
        case DT_Uint32 : { takeCastAndRun(uint32_t); break; }\
        case DT_Uint64 : { takeCastAndRun(uint64_t); break; }\
        case DT_Int8   : { takeCastAndRun(int8_t  ); break; }\
        case DT_Int16  : { takeCastAndRun(int16_t ); break; }\
        case DT_Int32  : { takeCastAndRun(int32_t ); break; }\
        case DT_Int64  : { takeCastAndRun(int64_t ); break; }\
        case DT_Float32: { takeCastAndRun(float   ); break; }\
        case DT_Float64: { takeCastAndRun(double  ); break; }\
    }

    switch (inType) {
        case DT_Uint8  : {
            const uint8_t* inDataT = reinterpret_cast<const uint8_t*>(inData);
            castOutAndRun();
            break;
        }
        case DT_Uint16 : {
            const uint16_t* inDataT = reinterpret_cast<const uint16_t*>(inData);
            castOutAndRun();
            break;
        }
        case DT_Uint32 : {
            const uint32_t* inDataT = reinterpret_cast<const uint32_t*>(inData);
            castOutAndRun();
            break;
        }
        case DT_Uint64 : {
            const uint64_t* inDataT = reinterpret_cast<const uint64_t*>(inData);
            castOutAndRun();
            break;
        }
        case DT_Int8 : {
            const int8_t* inDataT = reinterpret_cast<const int8_t*>(inData);
            castOutAndRun();
            break;
        }
        case DT_Int16 : {
            const int16_t* inDataT = reinterpret_cast<const int16_t*>(inData);
            castOutAndRun();
            break;
        }
        case DT_Int32 :  {
            const int32_t* inDataT = reinterpret_cast<const int32_t*>(inData);
            castOutAndRun();
            break;
        }
        case DT_Int64 : {
            const int64_t* inDataT = reinterpret_cast<const int64_t*>(inData);
            castOutAndRun();
            break;
        }
        case DT_Float32 : {
            const float* inDataT = reinterpret_cast<const float*>(inData);
            castOutAndRun();
            break;
        }
        case DT_Float64 : {
            const double* inDataT = reinterpret_cast<const double*>(inData);
            castOutAndRun();
            break;
        }
    }

#undef castOutAndRun
#undef takeCastAndRun
#undef runResizeTempl

}

}
// namespace ImgUtils
