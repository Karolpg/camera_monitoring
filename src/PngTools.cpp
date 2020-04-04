#include "PngTools.h"

#include <png.h>
#include <assert.h>

namespace PngTools {

const uint32_t PNG_HEADER_SIZE = sizeof(png_struct) + sizeof(png_info);

// Taken from https://gist.github.com/niw/5963798

bool writePngFile(FILE *fileDescriptor, uint32_t width, uint32_t height, uint32_t components, const uint8_t* rawData)
{
    if(!fileDescriptor) return false;

    png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
    if (!png) return false;

    png_infop info = png_create_info_struct(png);
    if (!info) return false;

    if (setjmp(png_jmpbuf(png))) return false;

    png_init_io(png, fileDescriptor);

    int colorType = PNG_COLOR_TYPE_RGB;
    switch(components)
    {
    case 1: colorType = PNG_COLOR_TYPE_GRAY; break;
    case 2: colorType = PNG_COLOR_TYPE_GRAY_ALPHA; break;
    case 3: colorType = PNG_COLOR_TYPE_RGB; break;
    case 4: colorType = PNG_COLOR_TYPE_RGB_ALPHA; break;
    default: assert(!"Invalid components value"); return false;
    }

    // Output is 8bit depth, RGB format.
    png_set_IHDR(png,
                 info,
                 width, height,
                 8,
                 colorType,
                 PNG_INTERLACE_NONE,
                 PNG_COMPRESSION_TYPE_DEFAULT,
                 PNG_FILTER_TYPE_DEFAULT);
    png_write_info(png, info);

    // To remove the alpha channel for PNG_COLOR_TYPE_RGB format,
    // Use png_set_filler().
    //png_set_filler(png, 0, PNG_FILLER_AFTER);

    if (!rawData) {
        png_destroy_write_struct(&png, &info);
        return false;
    }

    //WHY someone from PNG lib want to have non const data here!!!!
    for(uint32_t h = 0; h < height; ++h) {
        png_write_row(png, const_cast<png_bytep>(&rawData[width*h*components]));
    }
    png_write_end(png, nullptr);

    png_destroy_write_struct(&png, &info);
    return true;
}

bool writePngFile(const char *filename, uint32_t width, uint32_t height, uint32_t components, const uint8_t* rawData)
{
    FILE *fp = fopen(filename, "wb");

    bool result = writePngFile(fp, width, height, components, rawData);
    fclose(fp);

    return result;
}

bool readPngFile(const char *filename, uint32_t& width, uint32_t& height, uint32_t& components, std::vector<uint8_t>& data)
{
    FILE *fp = fopen(filename, "rb");
    if(!fp) return false;

    png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
    if(!png) return false;

    png_infop info = png_create_info_struct(png);
    if(!info) return false;

    if(setjmp(png_jmpbuf(png))) return false;

    png_init_io(png, fp);

    png_read_info(png, info);

    width      = static_cast<uint32_t>(png_get_image_width(png, info));
    height     = static_cast<uint32_t>(png_get_image_height(png, info));
    auto color_type = png_get_color_type(png, info);
    auto bit_depth  = png_get_bit_depth(png, info);

    // Read any color_type into 8bit depth, RGBA format.
    // See http://www.libpng.org/pub/png/libpng-manual.txt

    if(bit_depth == 16)
        png_set_strip_16(png);

    if(color_type == PNG_COLOR_TYPE_PALETTE) {
        png_set_palette_to_rgb(png);
        components = 3;
    }

    // PNG_COLOR_TYPE_GRAY_ALPHA is always 8 or 16bit depth.
    if(color_type == PNG_COLOR_TYPE_GRAY) {
        components = 1;
        if (bit_depth < 8)
            png_set_expand_gray_1_2_4_to_8(png);
    }

    if(png_get_valid(png, info, PNG_INFO_tRNS))
        png_set_tRNS_to_alpha(png);

    if(color_type == PNG_COLOR_TYPE_GRAY_ALPHA) {
        components = 2;
    }
    else if(color_type == PNG_COLOR_TYPE_RGB) {
        components = 3;
    }
    else if(color_type == PNG_COLOR_TYPE_RGBA) {
        components = 4;
    }

    png_read_update_info(png, info);

    data.resize(width*height*components);

    for(uint32_t y = 0; y < height; y++) {
        png_read_row(png, &data[y*width*components], nullptr);
    }

    fclose(fp);

    png_destroy_read_struct(&png, &info, nullptr);

    return true;
}


} // namespace PngTools
