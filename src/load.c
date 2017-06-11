#include "file-jxr.h"
#include <JXRGlue.h>
#include "utils.h"
#include <glib/gprintf.h>

static ERR jxrlib_load(const gchar* filename, gint* width, gint* height, gfloat* res_x, gfloat* res_y, PKPixelFormatGUID* pixel_format, gboolean* black_one, guchar** pixels, gchar** error_message);
static ERR get_target_pixel_format(const PKPixelFormatGUID* source, const PKPixelFormatGUID** target);
static void compact_stride(guchar* pixels, gint width, gint height, gint stride, gint bytes_per_pixel);

void load(gint nparams, const GimpParam* param, gint* nreturn_vals, GimpParam** return_vals)
{
    GimpParam*          ret_values;

    gchar*              filename;
    gchar*              error_message;
    ERR                 err;

    gint                width;
    gint                height;
    gfloat              res_x;
    gfloat              res_y;
    PKPixelFormatGUID   pixel_format;
    gboolean            black_one;
    guchar*             pixels = NULL;  

    GimpImageBaseType   base_type;
    GimpImageType       image_type;
    gint32              image_ID;
    gint32              layer_ID;
    GimpDrawable*       drawable;
    GimpPixelRgn        pixel_rgn;

    /*clock_t             time;
    gchar*              time_message;*/

/*#ifdef _DEBUG
    while (TRUE) { }
#endif*/

    filename = param[1].data.d_string;

    ret_values = g_new(GimpParam, 2);

    *nreturn_vals = 2;
    *return_vals = ret_values;  

    gimp_progress_init_printf(_("Loading '%s'"), gimp_filename_to_utf8(filename));

    /*time = clock();*/

    err = jxrlib_load(filename, &width, &height, &res_x, &res_y, &pixel_format, &black_one, &pixels, &error_message);

    if (Failed(err))
    {
        if (pixels)
            PKFreeAligned(&pixels);

        if (error_message == NULL)
        {
            switch (err)
            {
            case WMP_errFileIO:
                error_message = _("Error opening file.");
                break;
            case WMP_errOutOfMemory:
                error_message = _("Out of memory.");
                break;
            default:
                error_message = _("An error occurred during image loading.");
                break;
            }
        }

        ret_values[0].type          = GIMP_PDB_STATUS;
        ret_values[0].data.d_status = GIMP_PDB_EXECUTION_ERROR; 
        ret_values[1].type          = GIMP_PDB_STRING;
        ret_values[1].data.d_string = error_message;
        
        gimp_progress_end();
        return;
    }

    /*time = clock() - time;

    time_message = g_new(gchar, 128);
    g_sprintf(time_message, _("Elapsed time: %f ms."), (double)(time) / CLOCKS_PER_SEC);
    g_message(time_message);*/

    if (IsEqualGUID(&pixel_format, &GUID_PKPixelFormat24bppRGB))
    {
        base_type = GIMP_RGB;
        image_type = GIMP_RGB_IMAGE;
    }
    else if (IsEqualGUID(&pixel_format, &GUID_PKPixelFormat32bppRGBA))
    {
        base_type = GIMP_RGB;
        image_type = GIMP_RGBA_IMAGE;
    }
    else if (IsEqualGUID(&pixel_format, &GUID_PKPixelFormat8bppGray))
    {
        base_type = GIMP_GRAY;
        image_type = GIMP_GRAY_IMAGE;
    }
    else if (IsEqualGUID(&pixel_format, &GUID_PKPixelFormatBlackWhite))
    {
        base_type = GIMP_INDEXED;
        image_type = GIMP_INDEXED_IMAGE;
    }

    image_ID = gimp_image_new(width, height, base_type);
    
    gimp_image_set_filename(image_ID, filename);
    gimp_image_set_resolution(image_ID, res_x, res_y);
    
    if (IsEqualGUID(&pixel_format, &GUID_PKPixelFormatBlackWhite))
    {
        guchar colormap[] = { 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF };
        
        if (black_one)
        {
            colormap[0] = colormap[1] = colormap[2] = 0xFF;
            colormap[3] = colormap[4] = colormap[5] = 0x00;
        }

        gimp_image_set_colormap(image_ID, colormap, 2);
    }

    layer_ID = gimp_layer_new(image_ID, "Background", width, height, image_type, 100.0, GIMP_NORMAL_MODE);
    drawable = gimp_drawable_get(layer_ID);

    gimp_pixel_rgn_init(&pixel_rgn, drawable, 0, 0, width, height, TRUE, FALSE);
    gimp_pixel_rgn_set_rect(&pixel_rgn, pixels, 0, 0, width, height);

    gimp_drawable_update(layer_ID, 0, 0, width, height);
    gimp_image_add_layer(image_ID, layer_ID, 0);
    gimp_drawable_detach(drawable);

    PKFreeAligned(&pixels);

    ret_values[0].type          = GIMP_PDB_STATUS;
    ret_values[0].data.d_status = GIMP_PDB_SUCCESS;
    ret_values[1].type          = GIMP_PDB_IMAGE;
    ret_values[1].data.d_image  = image_ID;

    gimp_progress_end();
}

static ERR jxrlib_load(const gchar* filename, gint* width, gint* height, gfloat* res_x, gfloat* res_y, PKPixelFormatGUID* pixel_format, gboolean* black_one, guchar** pixels, gchar** error_message)
{
    ERR                 err;
    PKCodecFactory*     codec_factory = NULL;
    PKImageDecode*      decoder = NULL;
    PKPixelFormatGUID*  target_format;
    PKFormatConverter*  converter = NULL;
    guint               stride;
    PKRect              rect;
    
    *pixels = NULL;
    *error_message = NULL;

    Call(PKCreateCodecFactory(&codec_factory, WMP_SDK_VERSION));

    Call(codec_factory->CreateDecoderFromFile(filename, &decoder)); 

    Call(decoder->GetSize(decoder, width, height)); 
    Call(decoder->GetResolution(decoder, res_x, res_y));    
    Call(decoder->GetPixelFormat(decoder, pixel_format));

    *black_one = decoder->WMP.wmiSCP.bBlackWhite;

    Call(codec_factory->CreateFormatConverter(&converter));
    
    err = get_target_pixel_format(pixel_format, &target_format);    

    if (!Failed(err))
        err = converter->Initialize(converter, decoder, NULL, *target_format);
    
    if (Failed(err))
    {
        gchar* mnemonic = get_pixel_format_mnemonic(pixel_format);

        *error_message = g_new(gchar, 128);

        if (mnemonic != NULL)
            g_sprintf(*error_message, _("Image has an unsupported pixel format (%s)."), mnemonic);
        else
            g_sprintf(*error_message, _("Image has an unsupported pixel format (%08X-%04X-%04X-%02X%02X%02X%02X%02X%02X%02X%02X)."), 
                pixel_format->Data1, pixel_format->Data2, pixel_format->Data3, pixel_format->Data4[0],
                pixel_format->Data4[1], pixel_format->Data4[2], pixel_format->Data4[3], pixel_format->Data4[4],
                pixel_format->Data4[5], pixel_format->Data4[6], pixel_format->Data4[7]);

        goto Cleanup;
    }

    if (get_bits_per_pixel(target_format) < get_bits_per_pixel(pixel_format))
    {
        g_message(_("Warning:\n"
                    "The image you are loading has a pixel format that is not directly supported by GIMP. "
                    "In order to load this image it needs to be converted to a lower bit depth first. "
                    "Information will be lost because of this conversion."));
    }
    
    decoder->WMP.wmiSCP.uAlphaMode = 
        IsEqualGUID(target_format, &GUID_PKPixelFormat32bppRGBA) ? 2 : 0;

    stride = (*width * max(get_bits_per_pixel(pixel_format), get_bits_per_pixel(target_format)) + 7) / 8;

    Call(PKAllocAligned(pixels, stride * *height, 128));

    rect.X = 0;
    rect.Y = 0;
    rect.Width = *width;
    rect.Height = *height;

    Call(converter->Copy(converter, &rect, *pixels, stride)); 
    
    if (!IsEqualGUID(target_format, &GUID_PKPixelFormatBlackWhite) &&
        get_bits_per_pixel(pixel_format) > get_bits_per_pixel(target_format))
    {
        guint bytes_per_pixel = get_bits_per_pixel(target_format) / 8;      
        compact_stride(*pixels, *width, *height, stride, bytes_per_pixel);      
    }
    
    if (IsEqualGUID(target_format, &GUID_PKPixelFormatBlackWhite))
    {
        guchar* conv_pixels; 
        
        Call(convert_bw_indexed(*pixels, *width, *height, &conv_pixels)); // should pass stride here
                                                                          // if there were formats converted to BlackWhite

        Call(PKFreeAligned(pixels));
        *pixels = conv_pixels;
    }
    
    *pixel_format = *target_format;
        
Cleanup:
    if (Failed(err) && *pixels)
        PKFreeAligned(pixels);

    if (converter)
        converter->Release(&converter);

    if (decoder)
        decoder->Release(&decoder);
    
    if (codec_factory)
        codec_factory->Release(&codec_factory);

    return err;
}

static ERR get_target_pixel_format(const PKPixelFormatGUID* source, const PKPixelFormatGUID** target)
{ 
    ERR         err;
    PKPixelInfo pixel_info;   
    
    pixel_info.pGUIDPixFmt = source;
    
    Call(PixelFormatLookup(&pixel_info, LOOKUP_FORWARD));

    if (pixel_info.grBit & PK_pixfmtHasAlpha)
        *target = &GUID_PKPixelFormat32bppRGBA;
    else if (pixel_info.cfColorFormat == Y_ONLY)
        if (pixel_info.cbitUnit == 1)
            *target = &GUID_PKPixelFormatBlackWhite;
        else
            *target = &GUID_PKPixelFormat8bppGray;
    else
        *target = &GUID_PKPixelFormat24bppRGB;

Cleanup:
    return err;
}

static void compact_stride(guchar* pixels, gint width, gint height, gint stride, gint bytes_per_pixel)
{
    gint    y;
    guchar* src;
    guchar* dst;
    gint    new_stride = width * bytes_per_pixel;
    
    for (y = 1; y < height; y++)
    {
        src = pixels + y * stride;
        dst = pixels + y * new_stride;
        g_memmove(dst, src, new_stride);
    }
}
