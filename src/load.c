#include "file-jxr.h"
#include <JXRGlue.h>
#include "utils.h"
#include <glib/gprintf.h>

static ERR jxrlib_load(const gchar* filename, Image* image, gchar** error_message);
static ERR get_target_pixel_format(const PKPixelFormatGUID* source, const PKPixelFormatGUID** target);
static void compact_stride(guchar* pixels, gint width, gint height, gint stride, gint bytes_per_pixel);

void load(gint nparams, const GimpParam* param, gint* nreturn_vals, GimpParam** return_vals)
{
    GimpParam*          ret_values;

    gchar*              filename;
    gchar*              error_message;
    ERR                 err;

    Image               image;

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

    err = jxrlib_load(filename, &image, &error_message);

    if (Failed(err))
    {
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

    if (IsEqualGUID(&image.pixel_format, &GUID_PKPixelFormat24bppRGB))
    {
        base_type = GIMP_RGB;
        image_type = GIMP_RGB_IMAGE;
    }
    else if (IsEqualGUID(&image.pixel_format, &GUID_PKPixelFormat32bppRGBA))
    {
        base_type = GIMP_RGB;
        image_type = GIMP_RGBA_IMAGE;
    }
    else if (IsEqualGUID(&image.pixel_format, &GUID_PKPixelFormat8bppGray))
    {
        base_type = GIMP_GRAY;
        image_type = GIMP_GRAY_IMAGE;
    }
    else if (IsEqualGUID(&image.pixel_format, &GUID_PKPixelFormatBlackWhite))
    {
        base_type = GIMP_INDEXED;
        image_type = GIMP_INDEXED_IMAGE;
    }

    image_ID = gimp_image_new(image.width, image.height, base_type);
    
    gimp_image_set_filename(image_ID, filename);
    gimp_image_set_resolution(image_ID, image.resolution_x, image.resolution_y);
    
    if (IsEqualGUID(&image.pixel_format, &GUID_PKPixelFormatBlackWhite))
    {
        guchar colormap[] = { 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF };
        
        if (image.black_one)
        {
            colormap[0] = colormap[1] = colormap[2] = 0xFF;
            colormap[3] = colormap[4] = colormap[5] = 0x00;
        }

        gimp_image_set_colormap(image_ID, colormap, 2);
    }

    layer_ID = gimp_layer_new(image_ID, "Background", image.width, image.height, image_type, 100.0, GIMP_NORMAL_MODE);
    drawable = gimp_drawable_get(layer_ID);

    gimp_pixel_rgn_init(&pixel_rgn, drawable, 0, 0, image.width, image.height, TRUE, FALSE);
    gimp_pixel_rgn_set_rect(&pixel_rgn, image.pixels, 0, 0, image.width, image.height);

    gimp_drawable_update(layer_ID, 0, 0, image.width, image.height);
    gimp_image_add_layer(image_ID, layer_ID, 0);
    gimp_drawable_detach(drawable);

    PKFreeAligned(&image.pixels);

    if (image.color_context_size != 0)
    {
        GimpParasite* parasite;
        parasite = gimp_parasite_new("icc-profile", GIMP_PARASITE_PERSISTENT | GIMP_PARASITE_UNDOABLE, image.color_context_size, image.color_context);
        gimp_image_attach_parasite(image_ID, parasite);        
        gimp_parasite_free(parasite);
        g_free(image.color_context);
    }

    ret_values[0].type          = GIMP_PDB_STATUS;
    ret_values[0].data.d_status = GIMP_PDB_SUCCESS;
    ret_values[1].type          = GIMP_PDB_IMAGE;
    ret_values[1].data.d_image  = image_ID;

    gimp_progress_end();
}

static ERR jxrlib_load(const gchar* filename, Image* image, gchar** error_message)
{
    ERR                 err;
    PKCodecFactory*     codec_factory = NULL;
    PKImageDecode*      decoder = NULL;
    PKPixelFormatGUID*  target_format;
    PKFormatConverter*  converter = NULL;
    PKRect              rect;
    
    memset(image, 0, sizeof(*image));

    *error_message = NULL;

    Call(PKCreateCodecFactory(&codec_factory, WMP_SDK_VERSION));

    Call(codec_factory->CreateDecoderFromFile(filename, &decoder)); 

    Call(decoder->GetSize(decoder, &image->width, &image->height)); 
    Call(decoder->GetResolution(decoder, &image->resolution_x, &image->resolution_y));    
    Call(decoder->GetPixelFormat(decoder, &image->pixel_format));

    Call(decoder->GetColorContext(decoder, NULL, &image->color_context_size));
    if (image->color_context_size != 0)
    {
        image->color_context = g_new(guchar, image->color_context_size);
        Call(decoder->GetColorContext(decoder, image->color_context, &image->color_context_size));
    }

    image->black_one = decoder->WMP.wmiSCP.bBlackWhite;

    Call(codec_factory->CreateFormatConverter(&converter));
    
    err = get_target_pixel_format(&image->pixel_format, &target_format);

    if (!Failed(err))
        err = converter->Initialize(converter, decoder, NULL, *target_format);
    
    if (Failed(err))
    {
        PKPixelFormatGUID* pf = &image->pixel_format;
        gchar* mnemonic = get_pixel_format_mnemonic(pf);

        *error_message = g_new(gchar, 128);

        if (mnemonic != NULL)
            g_sprintf(*error_message, _("Image has an unsupported pixel format (%s)."), mnemonic);
        else
            g_sprintf(*error_message, _("Image has an unsupported pixel format (%08X-%04X-%04X-%02X%02X%02X%02X%02X%02X%02X%02X)."), 
                pf->Data1, pf->Data2, pf->Data3, pf->Data4[0], pf->Data4[1], pf->Data4[2], pf->Data4[3], pf->Data4[4], pf->Data4[5], pf->Data4[6], pf->Data4[7]);

        goto Cleanup;
    }

    if (get_bits_per_pixel(target_format) < get_bits_per_pixel(&image->pixel_format))
    {
        g_message(_("Warning:\n"
                    "The image you are loading has a pixel format that is not directly supported by GIMP. "
                    "In order to load this image it needs to be converted to a lower bit depth first. "
                    "Information will be lost because of this conversion."));
    }
    
    decoder->WMP.wmiSCP.uAlphaMode = 
        IsEqualGUID(target_format, &GUID_PKPixelFormat32bppRGBA) ? 2 : 0;

    image->stride = (image->width * max(get_bits_per_pixel(&image->pixel_format), get_bits_per_pixel(target_format)) + 7) / 8;

    Call(PKAllocAligned(&image->pixels, image->stride * image->height, 128));

    rect.X = 0;
    rect.Y = 0;
    rect.Width = image->width;
    rect.Height = image->height;

    Call(converter->Copy(converter, &rect, image->pixels, image->stride)); 
    
    if (!IsEqualGUID(target_format, &GUID_PKPixelFormatBlackWhite) &&
        get_bits_per_pixel(&image->pixel_format) > get_bits_per_pixel(target_format))
    {
        guint bytes_per_pixel = get_bits_per_pixel(target_format) / 8;      
        compact_stride(image->pixels, image->width, image->height, image->stride, bytes_per_pixel);   
        image->stride = bytes_per_pixel * image->width;
    }
    
    if (IsEqualGUID(target_format, &GUID_PKPixelFormatBlackWhite))
    {
        guchar* conv_pixels; 
        
        Call(convert_bw_indexed(image->pixels, image->width, image->height, &conv_pixels)); // should pass stride here
                                                                                            // if there were formats converted to BlackWhite

        Call(PKFreeAligned(&image->pixels));
        image->pixels = conv_pixels;
    }
    
    image->pixel_format = *target_format;
        
Cleanup:
    if (Failed(err) && image->pixels)
        PKFreeAligned(&image->pixels);

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
