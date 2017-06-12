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

void convert_rgba_bgra(guchar* pixels, guint width, guint height)
{
    guint y;
    guint x;
    guchar* p;
    guchar tmp;

    p = pixels;

    for (y = 0; y < height; y++)
        for (x = 0; x < width; x++)
        {
            tmp = p[0];
            p[0] = p[2];
            p[2] = tmp;
            p += 4;
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

// the following metadata helper functions have been copied from jxrlib as they are missing in libjxr Debian packages

ERR _PKImageDecode_GetMetadata_WMP(PKImageDecode *pID, U32 uOffset, U32 uByteCount, U8 *pbGot, U32 *pcbGot)
{
    ERR err = WMP_errSuccess;

    if (pbGot && uOffset)
    {
        struct WMPStream* pWS = pID->pStream;
        size_t iCurrPos;

        FailIf(*pcbGot < uByteCount, WMP_errBufferOverflow);
        Call(pWS->GetPos(pWS, &iCurrPos));
        Call(pWS->SetPos(pWS, uOffset));
        Call(pWS->Read(pWS, pbGot, uByteCount));
        Call(pWS->SetPos(pWS, iCurrPos));
    }

Cleanup:
    if (Failed(err))
        *pcbGot = 0;
    else
        *pcbGot = uByteCount;

    return err;
}

ERR _PKImageDecode_GetXMPMetadata_WMP(PKImageDecode *pID, U8 *pbXMPMetadata, U32 *pcbXMPMetadata)
{
    return _PKImageDecode_GetMetadata_WMP(pID, pID->WMP.wmiDEMisc.uXMPMetadataOffset,
        pID->WMP.wmiDEMisc.uXMPMetadataByteCount, pbXMPMetadata, pcbXMPMetadata);
}

ERR _PKImageDecode_GetEXIFMetadata_WMP(PKImageDecode *pID, U8 *pbEXIFMetadata, U32 *pcbEXIFMetadata)
{
    return _PKImageDecode_GetMetadata_WMP(pID, pID->WMP.wmiDEMisc.uEXIFMetadataOffset,
        pID->WMP.wmiDEMisc.uEXIFMetadataByteCount, pbEXIFMetadata, pcbEXIFMetadata);
}

ERR _PKImageDecode_GetGPSInfoMetadata_WMP(PKImageDecode *pID, U8 *pbGPSInfoMetadata, U32 *pcbGPSInfoMetadata)
{
    return _PKImageDecode_GetMetadata_WMP(pID, pID->WMP.wmiDEMisc.uGPSInfoMetadataOffset,
        pID->WMP.wmiDEMisc.uGPSInfoMetadataByteCount, pbGPSInfoMetadata, pcbGPSInfoMetadata);
}

ERR _PKImageDecode_GetIPTCNAAMetadata_WMP(PKImageDecode *pID, U8 *pbIPTCNAAMetadata, U32 *pcbIPTCNAAMetadata)
{
    return _PKImageDecode_GetMetadata_WMP(pID, pID->WMP.wmiDEMisc.uIPTCNAAMetadataOffset,
        pID->WMP.wmiDEMisc.uIPTCNAAMetadataByteCount, pbIPTCNAAMetadata, pcbIPTCNAAMetadata);
}

ERR _PKImageDecode_GetPhotoshopMetadata_WMP(PKImageDecode *pID, U8 *pbPhotoshopMetadata, U32 *pcbPhotoshopMetadata)
{
    return _PKImageDecode_GetMetadata_WMP(pID, pID->WMP.wmiDEMisc.uPhotoshopMetadataOffset,
        pID->WMP.wmiDEMisc.uPhotoshopMetadataByteCount, pbPhotoshopMetadata, pcbPhotoshopMetadata);
}