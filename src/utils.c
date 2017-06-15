#include "file-jxr.h"
#include <JXRGlue.h>

guint get_bits_per_pixel(const PKPixelFormatGUID* pixel_format)
{    
    PKPixelInfo pixel_info;
    
    pixel_info.pGUIDPixFmt = pixel_format;
    
    PixelFormatLookup(&pixel_info, LOOKUP_FORWARD);

    return pixel_info.cbitUnit;
}

ERR convert_bw_indexed(const guchar* pixels, guint width, guint height, guchar** conv_pixels)
{
    ERR             err;
    guint           stride;
    const guchar*   src;
    guchar*         dst;
    const guchar*   end;
    guint           n;

    err = PKAllocAligned(conv_pixels, width * height, 128);
    
    if (Failed(err))
        return err;
        
    stride = (width + 7) / 8;

    src = pixels;
    dst = *conv_pixels;    
    end = pixels + height * stride; 
    
    while (src < end)
    {
        const guchar* line_end = src + width / 8;
    
        while (src < line_end)
        {
            for (n = 0; n < 8; n++)
                *(dst++) = (*src >> (7 - n)) & 0x01;
            
            src++;
        }
        
        if (width % 8 != 0)
        {
            for (n = 0; n < width % 8; n++)
                *(dst++) = (*src >> (7 - n)) & 0x01;

            src++;
        }
    }

    return err;
}

void convert_indexed_bw(guchar* pixels, guint width, guint height)
{
    guint y;
    guint x;
    guint n;
    
    guchar* src = pixels;
    guchar* dst = pixels;
    
    for (y = 0; y < height; y++)
    {
        for (x = 0; x < width / 8; x++)
        {
            *dst = (*src << 7) | (*(src + 1) << 6) | (*(src + 2) << 5) | (*(src + 3) << 4) | 
                    (*(src + 4) << 3) | (*(src + 5) << 2) | (*(src + 6) << 1) | *(src + 7);
            src += 8;
            dst++;
        }
        
        if (width % 8 != 0)
        {
            *dst = 0x00;
            for (n = 0; n < width % 8; n++)
                *dst |= *(src++) << (7 - n);
            dst++;
        }
    }
}

gboolean has_blackwhite_colormap(gint32 image_ID, gboolean* black_one)
{
    guchar*     colormap;
    gint        num_colors;
    gboolean    bw = FALSE;

    colormap = gimp_image_get_colormap(image_ID, &num_colors);
    
    if (num_colors == 2)
    {
        if (memcmp(colormap, "\x00\x00\x00\xFF\xFF\xFF", 6) == 0)
        {
            bw = TRUE;
            *black_one = FALSE;
        }
        else if (memcmp(colormap, "\xFF\xFF\xFF\x00\x00\x00", 6) == 0)
        {
            bw = TRUE;
            *black_one = TRUE;
        }
    }
    
    g_free(colormap);
    
    return bw;
}

gchar* get_pixel_format_mnemonic(const PKPixelFormatGUID* pixel_format)
{
    if (memcmp(pixel_format, "\x24\xC3\xDD\x6F\x03\x4E\xFE\x4B\xB1\x85\x3D\x77\x76\x8D\xC9", 15) != 0)
        return NULL;

    switch (pixel_format->Data4[7])
    {
        case 0x0D:
            return "24bppRGB";
        case 0x0C:
            return "24bppBGR";
        case 0x0E:
            return "32bppBGR";
        case 0x15:
            return "48bppRGB";
        case 0x12:
            return "48bppRGBFixedPoint";
        case 0x3B:
            return "48bppRGBHalf";
        case 0x18:
            return "96bppRGBFixedPoint";
        case 0x40:
            return "64bppRGBFixedPoint";
        case 0x42:
            return "64bppRGBHalf";
        case 0x41:
            return "128bppRGBFixedPoint";
        case 0x1B:
            return "128bppRGBFloat";
        case 0x0F:
            return "32bppBGRA";
        case 0x16:
            return "64bppRGBA";
        case 0x1D:
            return "64bppRGBAFixedPoint";
        case 0x3A:
            return "64bppRGBAHalf";
        case 0x1E:
            return "128bppRGBAFixedPoint";
        case 0x19:
            return "128bppRGBAFloat";
        case 0x10:
            return "32bppPBGRA";
        case 0x17:
            return "64bppPRGBA";
        case 0x1A:
            return "128bppPRGBAFloat";
        case 0x1C:
            return "32bppCMYK";
        case 0x2C:
            return "40bppCMYKAlpha";
        case 0x1F:
            return "64bppCMYK";
        case 0x2D:
            return "80bppCMYKAlpha";
        case 0x20:
            return "24bpp3Channels";
        case 0x21:
            return "32bpp4Channels";
        case 0x22:
            return "40bpp5Channels";
        case 0x23:
            return "48bpp6Channels";
        case 0x24:
            return "56bpp7Channels";
        case 0x25:
            return "64bpp8Channels";
        case 0x2E:
            return "32bpp3ChannelsAlpha";
        case 0x2F:
            return "40bpp4ChannelsAlpha";
        case 0x30:
            return "48bpp5ChannelsAlpha";
        case 0x31:
            return "56bpp6ChannelsAlpha";
        case 0x32:
            return "64bpp7ChannelsAlpha";
        case 0x33:
            return "72bpp8ChannelsAlpha";
        case 0x26:
            return "48bpp3Channels";
        case 0x27:
            return "64bpp4Channels";
        case 0x28:
            return "80bpp5Channels";
        case 0x29:
            return "96bpp6Channels";
        case 0x2A:
            return "112bpp7Channels";
        case 0x2B:
            return "128bpp8Channels";
        case 0x34:
            return "64bpp3ChannelsAlpha";
        case 0x35:
            return "80bpp4ChannelsAlpha";
        case 0x36:
            return "96bpp5ChannelsAlpha";
        case 0x37:
            return "112bpp6ChannelsAlpha";
        case 0x38:
            return "128bpp7ChannelsAlpha";
        case 0x39:
            return "144bpp8ChannelsAlpha";
        case 0x08:
            return "8bppGray";
        case 0x0B:
            return "16bppGray";
        case 0x13:
            return "16bppGrayFixedPoint";
        case 0x3E:
            return "16bppGrayHalf";
        case 0x3F:
            return "32bppGrayFixedPoint";
        case 0x11:
            return "32bppGrayFloat";
        case 0x05:
            return "BlackWhite";
        case 0x09:
            return "16bppBGR555";
        case 0x0A:
            return "16bppBGR565";
        case 0x14:
            return "32bppBGR101010";
        case 0x3D:
            return "32bppRGBE";
        case 0x54:
            return "32bppCMYKDIRECT";
        case 0x55:
            return "64bppCMYKDIRECT";
        case 0x56:
            return "40bppCMYKDIRECTAlpha";
        case 0x43:
            return "80bppCMYKDIRECTAlpha";
        case 0x44:
            return "12bppYCC420";
        case 0x45:
            return "16bppYCC422";
        case 0x46:
            return "20bppYCC422";
        case 0x47:
            return "32bppYCC422";
        case 0x48:
            return "24bppYCC444";
        case 0x49:
            return "30bppYCC444";
        case 0x4A:
            return "48bppYCC444";
        case 0x4B:
            return "48bppYCC444FixedPoint";
        case 0x4C:
            return "20bppYCC420Alpha";
        case 0x4D:
            return "24bppYCC422Alpha";
        case 0x4E:
            return "30bppYCC422Alpha";
        case 0x4F:
            return "48bppYCC422Alpha";
        case 0x50:
            return "32bppYCC444Alpha";
        case 0x51:
            return "40bppYCC444Alpha";
        case 0x52:
            return "64bppYCC444Alpha";
        case 0x53:
            return "64bppYCC444AlphaFixedPoint";
        default:
            return NULL;        
    }
}

gboolean has_pixel_format_alpha_channel(const PKPixelFormatGUID* pixel_format)
{
    ERR         err;
    PKPixelInfo pixel_info;

    pixel_info.pGUIDPixFmt = pixel_format;

    Call(PixelFormatLookup(&pixel_info, LOOKUP_FORWARD));

    return pixel_info.grBit & PK_pixfmtHasAlpha;

Cleanup:
    return FALSE;
}

gboolean has_pixel_format_color_channels(const PKPixelFormatGUID* pixel_format)
{
    ERR         err;
    PKPixelInfo pixel_info;

    pixel_info.pGUIDPixFmt = pixel_format;

    Call(PixelFormatLookup(&pixel_info, LOOKUP_FORWARD));

    return pixel_info.cfColorFormat != Y_ONLY;

Cleanup:
    return TRUE;
}

void get_pixel_format_from_image_type_and_precision(GimpImageType image_type, GimpPrecision precision, PKPixelFormatGUID* pixel_format, const Babl** babl_pixel_format)
{
    GimpComponentType precision_type;
    GimpImageBaseType base_type;
    gboolean alpha;

    switch (precision)
    {
    case GIMP_PRECISION_U8_LINEAR:
    case GIMP_PRECISION_U8_GAMMA:
        precision_type = GIMP_COMPONENT_TYPE_U8;
        break;
    case GIMP_PRECISION_U16_LINEAR:
    case GIMP_PRECISION_U16_GAMMA:
        precision_type = GIMP_COMPONENT_TYPE_U16;
        break;
    case GIMP_PRECISION_U32_LINEAR:
    case GIMP_PRECISION_U32_GAMMA:
        precision_type = GIMP_COMPONENT_TYPE_U32;
        break;
    case GIMP_PRECISION_HALF_LINEAR:
    case GIMP_PRECISION_HALF_GAMMA:
        precision_type = GIMP_COMPONENT_TYPE_HALF;
        break;
    case GIMP_PRECISION_FLOAT_LINEAR:
    case GIMP_PRECISION_FLOAT_GAMMA:
        precision_type = GIMP_COMPONENT_TYPE_FLOAT;
        break;
    case GIMP_PRECISION_DOUBLE_LINEAR:
    case GIMP_PRECISION_DOUBLE_GAMMA:
        precision_type = GIMP_COMPONENT_TYPE_DOUBLE;
        break;
    }

    switch (image_type)
    {
    case GIMP_RGB_IMAGE:
        base_type = GIMP_RGB;
        alpha = FALSE;
        break;
    case GIMP_RGBA_IMAGE:
        base_type = GIMP_RGB;
        alpha = TRUE;
        break;
    case GIMP_GRAY_IMAGE:
        base_type = GIMP_GRAY;
        alpha = FALSE;
        break;
    case GIMP_GRAYA_IMAGE:
        base_type = GIMP_GRAY;
        alpha = TRUE;
        break;
    case GIMP_INDEXED_IMAGE:
        base_type = GIMP_INDEXED;
        alpha = FALSE;
        break;
    case GIMP_INDEXEDA_IMAGE:
        base_type = GIMP_INDEXED;
        alpha = TRUE;
        break;
    }

    if (base_type == GIMP_RGB && precision_type == GIMP_COMPONENT_TYPE_U8)
        if (!alpha)
        {
            *pixel_format = GUID_PKPixelFormat24bppRGB;
            *babl_pixel_format = babl_format("R'G'B' u8");
        }
        else
        {
            *pixel_format = GUID_PKPixelFormat32bppBGRA;
            *babl_pixel_format = babl_format_new(
                babl_model("R'G'B'A"),
                babl_type("u8"),
                babl_component("B'"),
                babl_component("G'"),
                babl_component("R'"),
                babl_component("A"),
                NULL);
        }
    else if (base_type == GIMP_RGB && (precision_type == GIMP_COMPONENT_TYPE_U16 || precision_type == GIMP_COMPONENT_TYPE_U32))
        if (!alpha)
        {
            *pixel_format = GUID_PKPixelFormat48bppRGB;
            *babl_pixel_format = babl_format("R'G'B' u16");
        }
        else
        {
            *pixel_format = GUID_PKPixelFormat64bppRGBA;
            *babl_pixel_format = babl_format("R'G'B'A u16");
        }
    else if (base_type == GIMP_RGB && precision_type == GIMP_COMPONENT_TYPE_HALF)
        if (!alpha)
        {
            *pixel_format = GUID_PKPixelFormat48bppRGBHalf;
            *babl_pixel_format = babl_format("RGB half");
        }
        else
        {
            *pixel_format = GUID_PKPixelFormat64bppRGBAHalf;
            *babl_pixel_format = babl_format("RGBA half");
        }
    else if (base_type == GIMP_RGB && (precision_type == GIMP_COMPONENT_TYPE_FLOAT || precision_type == GIMP_COMPONENT_TYPE_DOUBLE))
        if (!alpha)
        {
            *pixel_format = GUID_PKPixelFormat128bppRGBFloat;
            *babl_pixel_format = babl_format_new(
                babl_model("RGB"),
                babl_type("float"),
                babl_component("R"),
                babl_component("G"),
                babl_component("B"),
                babl_component("PAD"),
                NULL);
        }
        else
        {
            *pixel_format = GUID_PKPixelFormat128bppRGBAFloat;
            *babl_pixel_format = babl_format("RGBA float");
        }
    else if (base_type == GIMP_GRAY && precision_type == GIMP_COMPONENT_TYPE_U8)
    {
        *pixel_format = GUID_PKPixelFormat8bppGray;
        *babl_pixel_format = babl_format("Y' u8");
    }
    else if (base_type == GIMP_GRAY && (precision_type == GIMP_COMPONENT_TYPE_U16 || precision_type == GIMP_COMPONENT_TYPE_U32))
    {
        *pixel_format = GUID_PKPixelFormat16bppGray;
        *babl_pixel_format = babl_format("Y' u16");
    }
    else if (base_type == GIMP_GRAY && precision_type == GIMP_COMPONENT_TYPE_HALF)
    {
        *pixel_format = GUID_PKPixelFormat16bppGrayHalf;
        *babl_pixel_format = babl_format("Y half");
    }
    else if (base_type == GIMP_GRAY && (precision_type == GIMP_COMPONENT_TYPE_FLOAT || precision_type == GIMP_COMPONENT_TYPE_DOUBLE))
    {
        *pixel_format = GUID_PKPixelFormat32bppGrayFloat;
        *babl_pixel_format = babl_format("Y float");
    }
}

gboolean get_image_type_and_precision_from_pixel_format(const PKPixelFormatGUID* pixel_format, GimpImageBaseType* base_type, GimpImageType* image_type, GimpPrecision* precision, const Babl** babl_pixel_format, const PKPixelFormatGUID** conv_pixel_format)
{
    const PKPixelFormatGUID* final_pixel_format;

    if (IsEqualGUID(pixel_format, &GUID_PKPixelFormatBlackWhite))
    {
        *base_type = GIMP_INDEXED;
        *image_type = GIMP_INDEXED_IMAGE;
    }
    else if (!has_pixel_format_color_channels(pixel_format))
    {
        *base_type = GIMP_GRAY;
        *image_type = GIMP_GRAY_IMAGE;
    }
    else if (!has_pixel_format_alpha_channel(pixel_format))
    {
        *base_type = GIMP_RGB;
        *image_type = GIMP_RGB_IMAGE;
    }
    else
    {
        *base_type = GIMP_RGB;
        *image_type = GIMP_RGBA_IMAGE;
    }

    ERR         err;
    PKPixelInfo pixel_info;

    pixel_info.pGUIDPixFmt = pixel_format;

    Call(PixelFormatLookup(&pixel_info, LOOKUP_FORWARD));

    switch (pixel_info.bdBitDepth)
    {
    case BD_1:
    case BD_1alt:
    case BD_5:
    case BD_565:
    case BD_8:
        *precision = GIMP_PRECISION_U8_GAMMA;
        break;
    case BD_10:
    case BD_16:
        *precision = GIMP_PRECISION_U16_GAMMA;
        break;
    case BD_32:
        *precision = GIMP_PRECISION_U32_GAMMA;
        break;
    case BD_16F:
        *precision = GIMP_PRECISION_HALF_LINEAR;
        break;
    case BD_16S:
    case BD_32F:
    case BD_32S:
        *precision = GIMP_PRECISION_FLOAT_LINEAR;
        break;
    default:
        return FALSE;
    }

    switch (pixel_format->Data4[7])
    {
    //case 0x0C:
    //    *conv_pixel_format = &GUID_PKPixelFormat24bppRGB;
    //    break;
        //return "24bppBGR";
    //case 0x0E:
    //    *conv_pixel_format = &GUID_PKPixelFormat24bppRGB; 
    //    break;
        //return "32bppBGR";
    case 0x12:
        *conv_pixel_format = &GUID_PKPixelFormat96bppRGBFloat;
        break;
        //return "48bppRGBFixedPoint";
    case 0x18:
        *conv_pixel_format = &GUID_PKPixelFormat96bppRGBFloat;
        break;
        //return "96bppRGBFixedPoint";
    case 0x40:
        *conv_pixel_format = &GUID_PKPixelFormat96bppRGBFloat;
        break;
        //return "64bppRGBFixedPoint";
    //case 0x42:
    //    *conv_pixel_format = &GUID_PKPixelFormat48bppRGBHalf;
    //    break;
        //return "64bppRGBHalf";
    case 0x41:
        *conv_pixel_format = &GUID_PKPixelFormat96bppRGBFloat;
        break;
        //return "128bppRGBFixedPoint";
    //case 0x1B:
    //    *conv_pixel_format = &GUID_PKPixelFormat96bppRGBFloat;
    //    break;
        //return "128bppRGBFloat";
    //case 0x0F:
    //    *conv_pixel_format = &GUID_PKPixelFormat32bppRGBA; 
    //    break;
        //return "32bppBGRA";
    case 0x1D:
        *conv_pixel_format = &GUID_PKPixelFormat128bppRGBAFloat;
        break;
        //return "64bppRGBAFixedPoint";
    case 0x1E:
        *conv_pixel_format = &GUID_PKPixelFormat128bppRGBAFloat;
        break;
        //return "128bppRGBAFixedPoint";
    //case 0x10:
    //    *conv_pixel_format = &GUID_PKPixelFormat32bppRGBA;
    //    break;
        //return "32bppPBGRA";
    //case 0x17:
    //    *conv_pixel_format = &GUID_PKPixelFormat64bppRGBA;
    //    break;
        //return "64bppPRGBA";
    //case 0x1A:
    //    *conv_pixel_format = &GUID_PKPixelFormat128bppRGBAFloat;
    //    break;
        //return "128bppPRGBAFloat";
    case 0x13:
        *conv_pixel_format = &GUID_PKPixelFormat32bppGrayFloat; 
        break;
        //return "16bppGrayFixedPoint";
    case 0x3F:
        *conv_pixel_format = &GUID_PKPixelFormat32bppGrayFloat;
        break;
        //return "32bppGrayFixedPoint";
    case 0x09:
        *conv_pixel_format = &GUID_PKPixelFormat24bppRGB;
        break;
        //return "16bppBGR555";
    case 0x0A:
        *conv_pixel_format = &GUID_PKPixelFormat24bppRGB;
        break;
        //return "16bppBGR565";
    case 0x14:
        *conv_pixel_format = &GUID_PKPixelFormat48bppRGB;
        break;
        //return "32bppBGR101010";
    case 0x3D:
        *conv_pixel_format = &GUID_PKPixelFormat96bppRGBFloat;
        break;
        //return "32bppRGBE";
    default:
        *conv_pixel_format = NULL;
        break;
    }

    if (*conv_pixel_format != NULL)
        final_pixel_format = *conv_pixel_format;
    else
        final_pixel_format = pixel_format;

    switch (final_pixel_format->Data4[7])
    {
    case 0x0D:        
        *babl_pixel_format = babl_format("R'G'B' u8");
        break;
        //return "24bppRGB";
    case 0x0C:
        *babl_pixel_format = babl_format_new(
            babl_model("R'G'B'"),
            babl_type("u8"),
            babl_component("B'"),
            babl_component("G'"),
            babl_component("R'"),
            NULL);
        break;
        //return "24bppBGR";
    case 0x0E:
        *babl_pixel_format = babl_format_new( // test!
            babl_model("R'G'B'"),
            babl_type("u8"),
            babl_component("B'"),
            babl_component("G'"),
            babl_component("R'"),
            babl_component("PAD"),
            NULL);
        break;
        //return "32bppBGR";
    case 0x15:
        *babl_pixel_format = babl_format("R'G'B' u16");
        break;
        //return "48bppRGB";
    case 0x3B:
        *babl_pixel_format = babl_format("RGB half");
        break;
        //return "48bppRGBHalf";
    case 0x42:
        *babl_pixel_format = babl_format_new( // test!
            babl_model("RGB"),
            babl_type("half"),
            babl_component("R"),
            babl_component("G"),
            babl_component("B"),
            babl_component("PAD"),
            NULL);
        break;
        //return "64bppRGBHalf";
    case 0x27:
        *babl_pixel_format = babl_format("RGB float"); 
        break;
        //return "96bppRGBFloat";
    case 0x1B:
        *babl_pixel_format = babl_format_new( // test!
            babl_model("RGB"),
            babl_type("float"),
            babl_component("R"),
            babl_component("G"),
            babl_component("B"),
            babl_component("PAD"),
            NULL);
        break;
        //return "128bppRGBFloat";
    case 0x0F:
        *babl_pixel_format = babl_format_new( // test!
            babl_model("R'G'B'A"),
            babl_type("u8"),
            babl_component("B'"),
            babl_component("G'"),
            babl_component("R'"),
            babl_component("A"),
            NULL);
        break;
        //return "32bppBGRA";
    case 0x16:
        *babl_pixel_format = babl_format("R'G'B'A u16");
        break;
        //return "64bppRGBA";
    case 0x3A:
        *babl_pixel_format = babl_format("RGBA half"); // test!
        break;
        //return "64bppRGBAHalf";
    case 0x19:
        *babl_pixel_format = babl_format("RGBA float");
        break;
        //return "128bppRGBAFloat";
    case 0x10:
        *babl_pixel_format = babl_format_new( // test!
            babl_model("R'aG'aB'aA"),
            babl_type("u8"),
            babl_component("B'a"),
            babl_component("G'a"),
            babl_component("R'a"),
            babl_component("A"),
            NULL);
        break;
        //return "32bppPBGRA";
    case 0x17:
        *babl_pixel_format = babl_format_new( // test!
            babl_model("R'aG'aB'aA"),
            babl_type("u16"),
            babl_component("R'a"),
            babl_component("G'a"),
            babl_component("B'a"),
            babl_component("A"),
            NULL);
        break;
        //return "64bppPRGBA";
    case 0x1A:
        *babl_pixel_format = babl_format_new( // test!
            babl_model("RaGaBaA"),
            babl_type("float"),
            babl_component("Ra"),
            babl_component("Ga"),
            babl_component("Ba"),
            babl_component("A"),
            NULL);
        break;
        //return "128bppPRGBAFloat";
    case 0x1C:
        *babl_pixel_format = babl_format_new( // test!
            babl_model("CMYK"),
            babl_type("u8"),
            babl_component("C"),
            babl_component("M"),
            babl_component("Y"),
            babl_component("K"),
            NULL);
        break;
        //return "32bppCMYK";
    //case 0x2C:
    //    *babl_pixel_format = babl_format("CMYKA u8");
    //    break;
        //return "40bppCMYKAlpha";
    case 0x1F:
        *babl_pixel_format = babl_format_new( // test!
            babl_model("CMYK"),
            babl_type("u16"),
            babl_component("C"),
            babl_component("M"),
            babl_component("Y"),
            babl_component("K"),
            NULL);
        break;
        //return "64bppCMYK";
    //case 0x2D:
    //    *babl_pixel_format = babl_format("CMYKA u16");
    //    break;
        //return "80bppCMYKAlpha";
    case 0x08:
        *babl_pixel_format = babl_format("Y' u8");
        break;
        //return "8bppGray";
    case 0x0B:
        *babl_pixel_format = babl_format("Y' u16");
        break;
        //return "16bppGray";
    case 0x3E:
        *babl_pixel_format = babl_format("Y half"); // test!
        break;
        //return "16bppGrayHalf";
    case 0x11:
        *babl_pixel_format = babl_format("Y float");
        break;
        //return "32bppGrayFloat";
    case 0x05:
        *babl_pixel_format = NULL; // the correct pixel_format should be determined later by gimp_drawable_get_format here
        break;
        //return "BlackWhite";
    default:
        return FALSE;
    }

    return TRUE;

Cleanup:
    return FALSE;
}
