#include "file-jxr.h"
#include "utils.h"
#include <JXRGlue.h>

static ERR jxrlib_load(const gchar* filename, Image* image, gchar** error_message);
static gboolean get_conv_pixel_format(const PKPixelFormatGUID* source, const PKPixelFormatGUID** target);

void load(gint nparams, const GimpParam* param, gint* nreturn_vals, GimpParam** return_vals)
{
    GimpParam*               ret_values;

    gchar*                   filename;
    gchar*                   error_message;
    ERR                      err;

    Image                    image;

    GimpImageBaseType        base_type;
    GimpImageType            image_type;
    gint32                   image_ID;
    gint32                   layer_ID;
    GeglBuffer*              gegl_buffer;

    GimpPrecision            precision;
    const Babl*              babl_pixel_format;
    const PKPixelFormatGUID* conv_pixel_format;

/*#ifdef _DEBUG
    while (TRUE) { }
#endif*/

    filename = param[1].data.d_string;

    ret_values = g_new(GimpParam, 2);

    *nreturn_vals = 2;
    *return_vals = ret_values;

    gimp_progress_init_printf(_("Loading '%s'"), gimp_filename_to_utf8(filename));

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

        ret_values[0].type = GIMP_PDB_STATUS;
        ret_values[0].data.d_status = GIMP_PDB_EXECUTION_ERROR;
        ret_values[1].type = GIMP_PDB_STRING;
        ret_values[1].data.d_string = error_message;

        gimp_progress_end();
        return;
    }

    if (!get_image_type_and_precision_from_pixel_format(&image.pixel_format, &base_type, &image_type, &precision, &babl_pixel_format,
        &conv_pixel_format))
    {
        return; // TODO: error!
    }

    image_ID = gimp_image_new_with_precision(image.width, image.height, base_type, precision);

    gimp_image_set_filename(image_ID, filename);
    gimp_image_set_resolution(image_ID, image.resolution_x, image.resolution_y);

    if (IsEqualGUID(&image.pixel_format, &GUID_PKPixelFormatBlackWhite))
    {
        guchar colormap [] = { 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF };

        if (image.black_one)
        {
            colormap[0] = colormap[1] = colormap[2] = 0xFF;
            colormap[3] = colormap[4] = colormap[5] = 0x00;
        }

        gimp_image_set_colormap(image_ID, colormap, 2);
    }

    layer_ID = gimp_layer_new(image_ID, "Background", image.width, image.height, image_type, 100.0, GIMP_NORMAL_MODE);

    gimp_image_insert_layer(image_ID, layer_ID, -1, 0);

    if (image_type == GIMP_INDEXED_IMAGE)
        babl_pixel_format = gimp_drawable_get_format(layer_ID);

    gegl_buffer = gimp_drawable_get_buffer(layer_ID);

    gegl_buffer_set(gegl_buffer, GEGL_RECTANGLE(0, 0, image.width, image.height), 0, babl_pixel_format, image.pixels, image.stride);

    g_object_unref(gegl_buffer);

    PKFreeAligned(&image.pixels);

    if (image.color_context_size != 0)
    {
        GimpParasite* parasite;
        parasite = gimp_parasite_new("icc-profile", GIMP_PARASITE_PERSISTENT | GIMP_PARASITE_UNDOABLE, image.color_context_size, image.color_context);
        gimp_image_attach_parasite(image_ID, parasite);
        gimp_parasite_free(parasite);
        g_free(image.color_context);
    }

    if (image.exif_metadata_size != 0 || image.xmp_metadata_size != 0)
    {
        GimpMetadata* metadata;
        GError*       gError = NULL;

        metadata = gimp_metadata_new();

        if (image.exif_metadata_size != 0)
        {
            guchar exifHeader[14] = {
                0x45, 0x78, 0x69, 0x66, 0x00, 0x00, 0x49, 0x49, 0x2A, 0x00, 0x08, 0x00, 0x00, 0x00
            };

            guchar* exifMetadataWithHeader = g_new(guchar, image.exif_metadata_size + sizeof(exifHeader));

            g_memmove(exifMetadataWithHeader, &exifHeader, sizeof(exifHeader));
            g_memmove(exifMetadataWithHeader + sizeof(exifHeader), image.exif_metadata, image.exif_metadata_size);

            if (gimp_metadata_set_from_exif(metadata, exifMetadataWithHeader, image.exif_metadata_size + sizeof(exifHeader), &gError))
            {
                gimp_image_set_metadata(image_ID, metadata);
            }

            g_free(exifMetadataWithHeader);            
        }

        if (image.xmp_metadata_size != 0)
        {
            if (gimp_metadata_set_from_xmp(metadata, image.xmp_metadata - 10, image.xmp_metadata_size + 10, &gError))
            {
                gimp_image_set_metadata(image_ID, metadata);
            }
        }

        if (image.iptc_metadata_size != 0)
        {
            // TODO
        }

        g_object_unref(metadata);
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
    PKPixelFormatGUID*  conv_pixel_format;
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

    Call(_PKImageDecode_GetEXIFMetadata_WMP(decoder, NULL, &image->exif_metadata_size));
    if (image->exif_metadata_size != 0)
    {
        image->exif_metadata = g_new(guchar, image->exif_metadata_size);
        Call(_PKImageDecode_GetEXIFMetadata_WMP(decoder, image->exif_metadata, &image->exif_metadata_size));
    }

    Call(_PKImageDecode_GetXMPMetadata_WMP(decoder, NULL, &image->xmp_metadata_size));
    if (image->xmp_metadata_size != 0)
    {
        image->xmp_metadata = g_new(guchar, image->xmp_metadata_size);
        Call(_PKImageDecode_GetXMPMetadata_WMP(decoder, image->xmp_metadata, &image->xmp_metadata_size));
    }

    image->black_one = decoder->WMP.wmiSCP.bBlackWhite;

    Call(codec_factory->CreateFormatConverter(&converter));
    
    err = get_conv_pixel_format(&image->pixel_format, &conv_pixel_format) ? WMP_errSuccess : WMP_errFail;

    if (!Failed(err))
        err = converter->Initialize(converter, decoder, NULL, *conv_pixel_format);
    
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
    
    decoder->WMP.wmiSCP.uAlphaMode = has_pixel_format_alpha_channel(conv_pixel_format) ? 2 : 0;

    image->stride = (image->width * max(get_bits_per_pixel(&image->pixel_format), get_bits_per_pixel(conv_pixel_format)) + 7) / 8;

    Call(PKAllocAligned(&image->pixels, image->stride * image->height, 128));

    rect.X = 0;
    rect.Y = 0;
    rect.Width = image->width;
    rect.Height = image->height;

    Call(converter->Copy(converter, &rect, image->pixels, image->stride)); 
    
    if (IsEqualGUID(conv_pixel_format, &GUID_PKPixelFormatBlackWhite))
    {
        guchar* conv_pixels; 
        
        Call(convert_bw_indexed(image->pixels, image->width, image->height, &conv_pixels));

        Call(PKFreeAligned(&image->pixels));

        image->pixels = conv_pixels;
        image->stride = image->width;
    }
    
    image->pixel_format = *conv_pixel_format;
        
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

static gboolean get_conv_pixel_format(const PKPixelFormatGUID* source, const PKPixelFormatGUID** target)
{ 
    GimpImageBaseType base_type;
    GimpImageType image_type;
    GimpPrecision precision;
    const Babl* babl_pixel_format;

    if (!get_image_type_and_precision_from_pixel_format(source, &base_type, &image_type, &precision, &babl_pixel_format, target))
        return FALSE;

    if (*target == NULL)
        *target = source;

    return TRUE;
}
