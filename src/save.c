#include "file-jxr.h"
#include "utils.h"
#include <JXRGlue.h>
#include <libgimp/gimpui.h>
#include <gexiv2/gexiv2.h>

typedef enum
{
    OVERLAP_AUTO,
    OVERLAP_NONE,
    OVERLAP_ONE,
    OVERLAP_TWO
} OverlapSetting;

typedef enum
{
    SUBSAMPLING_YONLY,
    SUBSAMPLING_420,
    SUBSAMPLING_422,
    SUBSAMPLING_444
} SubsamplingSetting;

typedef enum
{
    TILING_NONE,
    TILING_256,
    TILING_512,
    TILING_1024
} TilingSetting;

typedef struct
{
    gint                image_quality;
    gint                alpha_quality;
    OverlapSetting      overlap;
    SubsamplingSetting  subsampling;
    TilingSetting       tiling; 
    gboolean            save_metadata;
    gboolean            embed_color_profile;
} SaveOptions;

typedef struct
{
    GtkWidget*  dialog;
    GtkWidget*  vbox;
    GtkWidget*  quality_table;
    GtkObject*  quality_entry;
    GtkObject*  alpha_quality_entry;
    GtkWidget*  advanced_expander;
    GtkWidget*  advanced_vbox;
    GtkWidget*  advanced_frame;
    GtkWidget*  advanced_table;
    GtkWidget*  overlap_label;
    GtkWidget*  overlap_combo_box;
    GtkWidget*  subsampling_label;
    GtkWidget*  subsampling_combo_box;
    GtkWidget*  tiling_label;
    GtkWidget*  tiling_combo_box;
    GtkWidget*  save_metadata_check_button;
    GtkWidget*  embed_color_profile_check_button;
    GtkWidget*  lossless_label;
    GtkWidget*  pixel_format_label;
    GtkWidget*  defaults_table;
    GtkWidget*  load_defaults_button;
    GtkWidget*  save_defaults_button;
} SaveGui;

static const SaveOptions DEFAULT_SAVE_OPTIONS = { 90, 100, OVERLAP_AUTO, SUBSAMPLING_444, TILING_NONE, TRUE, TRUE };

static ERR jxrlib_save(const gchar *filename, const Image* image, const SaveOptions* save_options);
static void apply_save_options(const SaveOptions* save_options, guint width, guint height, PKPixelFormatGUID pixel_format, gboolean black_one, CWMIStrCodecParam* wmiSCP, CWMIStrCodecParam* wmiSCP_Alpha);
static gboolean show_options(SaveOptions* save_options, gboolean alpha_enabled, gboolean subsampling_enabled, const PKPixelFormatGUID* pixel_format);
static void load_save_gui_defaults(const SaveGui* save_gui);
static void save_save_gui_defaults(const SaveGui* save_gui);
static void update_save_gui(const SaveGui* save_gui, const SaveOptions* save_options);
static void get_save_options(const SaveGui* save_gui, SaveOptions* save_options);
static gboolean load_save_gui_defaults_from_parasite(SaveOptions* save_options);
static void save_save_gui_defaults_to_parasite(const SaveOptions* save_options);

void save(gint nparams, const GimpParam* param, gint* nreturn_vals, GimpParam** return_vals)
{
    GimpParam*              ret_values;
    GimpExportCapabilities  capabilities;
    GimpExportReturn        export_return;

    SaveOptions             save_options;

    gchar*                  filename;
    Image                   image;
    gdouble                 res_x, res_y;

    GimpRunMode             run_mode;
    gint32                  image_ID;
    gint32                  drawable_ID;
    GimpPrecision           precision;
    GimpImageType           image_type;
    const Babl*             babl_pixel_format;
    gint                    bpp;

    GeglBuffer*             gegl_buffer;

    ERR                     err;

    gboolean                alpha_enabled;
    gboolean                subsampling_enabled;

    GimpParasite*           icc_parasite;
    GimpMetadata*           metadata;

/*#ifdef _DEBUG
    while (TRUE) { }
#endif*/
    
    run_mode    = (GimpRunMode)param[0].data.d_int32;
    image_ID    = param[1].data.d_int32;
    drawable_ID = param[2].data.d_int32;
    filename    = param[3].data.d_string;
    
    ret_values = g_new(GimpParam, 2);

    *nreturn_vals = 2;
    *return_vals = ret_values;
    ret_values[0].type = GIMP_PDB_STATUS;
    ret_values[0].data.d_status = GIMP_PDB_EXECUTION_ERROR;

    gimp_ui_init(PLUG_IN_BINARY, FALSE);

    memset(&image, 0, sizeof(image));
    
    capabilities = GIMP_EXPORT_CAN_HANDLE_RGB | GIMP_EXPORT_CAN_HANDLE_GRAY | 
        GIMP_EXPORT_CAN_HANDLE_INDEXED | GIMP_EXPORT_CAN_HANDLE_ALPHA;
            
Export:
    export_return = gimp_export_image(&image_ID, &drawable_ID, "JPEG XR", capabilities);

    if (export_return == GIMP_EXPORT_CANCEL)
    {
        ret_values[0].data.d_status = GIMP_PDB_CANCEL;
        return;
    }    

    image_type = gimp_drawable_type(drawable_ID);

    precision = gimp_image_get_precision(image_ID);    

    switch (image_type)
    {
    case GIMP_RGB_IMAGE:
    case GIMP_RGBA_IMAGE:
    case GIMP_GRAY_IMAGE:
        get_pixel_format_from_image_type_and_precision(image_type, precision, &image.pixel_format, &babl_pixel_format);
        break;
    case GIMP_GRAYA_IMAGE:
        /*ret_values[1].type          = GIMP_PDB_STRING;
        ret_values[1].data.d_string = _("Grayscale images with an alpha channel are not supported.");
        return;*/
        gimp_image_delete(image_ID);
        capabilities &= GIMP_EXPORT_CAN_HANDLE_RGB | GIMP_EXPORT_CAN_HANDLE_ALPHA;
        goto Export;
    case GIMP_INDEXED_IMAGE:        
        if (has_blackwhite_colormap(image_ID, &image.black_one))
        {
            image.pixel_format = GUID_PKPixelFormatBlackWhite;
            babl_pixel_format = gimp_drawable_get_format(drawable_ID);
        }
        else
        {
            /*ret_values[1].type          = GIMP_PDB_STRING;
            ret_values[1].data.d_string = _("Indexed images are not supported except for black-white colormaps.");
            return;*/
            gimp_image_delete(image_ID);
            capabilities &= GIMP_EXPORT_CAN_HANDLE_RGB | GIMP_EXPORT_CAN_HANDLE_GRAY | GIMP_EXPORT_CAN_HANDLE_ALPHA;
            goto Export;
        }
        break;
    case GIMP_INDEXEDA_IMAGE:
        /*ret_values[1].type          = GIMP_PDB_STRING;
        ret_values[1].data.d_string = _("Indexed images with an alpha channel are not supported.");
        return;*/
        gimp_image_delete(image_ID);
        capabilities &= GIMP_EXPORT_CAN_HANDLE_RGB | GIMP_EXPORT_CAN_HANDLE_GRAY | GIMP_EXPORT_CAN_HANDLE_ALPHA;
        goto Export;
    default:
        ret_values[1].type          = GIMP_PDB_STRING;
        ret_values[1].data.d_string = _("Image has an unsupported pixel format.");
        return;
    }

    if (!load_save_gui_defaults_from_parasite(&save_options))
        save_options = DEFAULT_SAVE_OPTIONS;
    
    switch (run_mode)
    {
    case GIMP_RUN_INTERACTIVE:
        gimp_get_data(SAVE_PROC, &save_options);
        alpha_enabled = has_pixel_format_alpha_channel(&image.pixel_format);
        subsampling_enabled = image_type == GIMP_RGB_IMAGE || image_type == GIMP_RGBA_IMAGE;
        if (show_options(&save_options, alpha_enabled, subsampling_enabled, &image.pixel_format))
        {
            gimp_set_data(SAVE_PROC, &save_options, sizeof(SaveOptions));
        }
        else
        {
            ret_values[0].data.d_status = GIMP_PDB_CANCEL;
            return;
        }
        break;

    case GIMP_RUN_NONINTERACTIVE:
        if (nparams == 12)
        {
            save_options.image_quality       = param[5].data.d_int32;
            save_options.alpha_quality       = param[6].data.d_int32;
            save_options.overlap             = param[7].data.d_int32;
            save_options.subsampling         = param[8].data.d_int32;
            save_options.tiling              = param[9].data.d_int32;
            save_options.save_metadata       = param[10].data.d_int32;
            save_options.embed_color_profile = param[11].data.d_int32;
            
            if (save_options.image_quality < 0       || save_options.image_quality > 100 ||
                save_options.alpha_quality < 0       || save_options.alpha_quality > 100 ||
                save_options.overlap < 0             || save_options.overlap > 3         ||
                save_options.subsampling < 0         || save_options.subsampling > 3     ||
                save_options.tiling < 0              || save_options.tiling > 3          ||
                save_options.save_metadata < 0       || save_options.save_metadata > 1   ||
                save_options.embed_color_profile < 0 || save_options.embed_color_profile > 1)
            {
                ret_values[0].data.d_status = GIMP_PDB_CALLING_ERROR;
                return;
            }
        }
        else
        {
            ret_values[0].data.d_status = GIMP_PDB_CALLING_ERROR;
            return;
        }   
        break;

    case GIMP_RUN_WITH_LAST_VALS:
        gimp_get_data(SAVE_PROC, &save_options);
        break;
    }
    
    gimp_progress_init_printf(_("Saving '%s'"), gimp_filename_to_utf8(filename));

    gegl_buffer = gimp_drawable_get_buffer(drawable_ID);

    image.width   = gegl_buffer_get_width(gegl_buffer);
    image.height  = gegl_buffer_get_height(gegl_buffer);

    if (!gimp_image_get_resolution(image_ID, &res_x, &res_y))
    {
        image.resolution_x = 72.0;
        image.resolution_y = 72.0;
    }
    else
    {
        image.resolution_x = (gfloat)res_x;
        image.resolution_y = (gfloat)res_y;
    }

    bpp = babl_format_get_bytes_per_pixel(babl_pixel_format);

    image.stride = image.width * bpp;

    err = PKAllocAligned(&image.pixels, image.stride * image.height, 128);
    
    if (Failed(err))
    {
        ret_values[1].type          = GIMP_PDB_STRING;
        ret_values[1].data.d_string = _("Out of memory.");
        return;
    }

    gegl_buffer_get(gegl_buffer, GEGL_RECTANGLE(0, 0, image.width, image.height), 1.0, babl_pixel_format, 
        image.pixels, image.stride, GEGL_ABYSS_NONE);

    g_object_unref(gegl_buffer);
    
    if (export_return == GIMP_EXPORT_EXPORT)
        gimp_image_delete(image_ID);
        
    if (IsEqualGUID(&image.pixel_format, &GUID_PKPixelFormatBlackWhite))
    {
        convert_indexed_bw(image.pixels, image.width, image.height);
        image.stride = (image.width + 7) / 8;
    }

    icc_parasite = gimp_image_parasite_find(image_ID, "icc-profile");

    if (icc_parasite != NULL)
    {
        image.color_context = (guchar*)gimp_parasite_data(icc_parasite);
        image.color_context_size = gimp_parasite_data_size(icc_parasite);
    }

    /*metadata = gimp_image_get_metadata(image_ID);

    if (metadata != NULL && gexiv2_metadata_has_xmp(metadata))
    {
        gchar** tags = gexiv2_metadata_get_xmp_tags(metadata);
        gchar* serialized = gimp_metadata_serialize(metadata);
        image.exif_metadata = gexiv2_metadata_get_xmp_packet(metadata);
        image.exif_metadata_size = strlen(image.exif_metadata) + 1;
    }*/

    err = jxrlib_save(filename, &image, &save_options);

    PKFreeAligned(&image.pixels); 

    if (icc_parasite != NULL)
    {
        gimp_parasite_free(icc_parasite);
    }

    if (!Failed(err))
    {
        *nreturn_vals = 1;
        ret_values[0].data.d_status = GIMP_PDB_SUCCESS;
    }
    else
    {
        ret_values[1].type          = GIMP_PDB_STRING;
        ret_values[1].data.d_string = _("An error occurred.");  
    }
    
    gimp_progress_end();
} 

static ERR jxrlib_save(const gchar *filename, const Image* image, const SaveOptions* save_options)
{
    ERR                 err;
    PKFactory*          factory = NULL;
    struct WMPStream*   stream;
    PKCodecFactory*     codec_factory = NULL;
    PKImageEncode*      encoder = NULL;
    CWMIStrCodecParam   wmiSCP;

    Call(PKCreateFactory(&factory, PK_SDK_VERSION));
    Call(PKCreateCodecFactory(&codec_factory, WMP_SDK_VERSION));    

    Call(factory->CreateStreamFromFilename(&stream, filename, "wb"));    

    Call(codec_factory->CreateCodec(&IID_PKImageWmpEncode, (void**)&encoder));
    
    apply_save_options(save_options, image->width, image->height, image->pixel_format, image->black_one, &wmiSCP, NULL);
    
    Call(encoder->Initialize(encoder, stream, &wmiSCP, sizeof(wmiSCP)));

    apply_save_options(save_options, image->width, image->height, image->pixel_format, image->black_one, &wmiSCP, &encoder->WMP.wmiSCP_Alpha);
    
    Call(encoder->SetPixelFormat(encoder, image->pixel_format));
    Call(encoder->SetSize(encoder, image->width, image->height));
    Call(encoder->SetResolution(encoder, image->resolution_x, image->resolution_y));
    
    if (save_options->embed_color_profile && image->color_context_size != 0)
    {
        Call(encoder->SetColorContext(encoder, image->color_context, image->color_context_size));
    }

    if (save_options->save_metadata)
    {
        if (image->exif_metadata_size != 0)
        {
            Call(PKImageEncode_SetEXIFMetadata_WMP(encoder, image->exif_metadata, image->exif_metadata_size));
        }

        if (image->xmp_metadata_size != 0)
        {
            Call(PKImageEncode_SetXMPMetadata_WMP(encoder, image->xmp_metadata, image->xmp_metadata_size));
        }

        if (image->iptc_metadata_size != 0)
        {
            Call(PKImageEncode_SetIPTCNAAMetadata_WMP(encoder, image->iptc_metadata, image->iptc_metadata_size));
        }
    }

    Call(encoder->WritePixels(encoder, image->height, image->pixels, image->stride));
    
Cleanup:
    if (encoder)
        encoder->Release(&encoder);
    
    if (codec_factory)
        codec_factory->Release(&codec_factory);
    
    if (factory)
        factory->Release(&factory);
    
    return err;
}

static int qp_table[12][6] = { // optimized for PSNR
    { 67, 79, 86, 72, 90, 98 },
    { 59, 74, 80, 64, 83, 89 },
    { 53, 68, 75, 57, 76, 83 },
    { 49, 64, 71, 53, 70, 77 },
    { 45, 60, 67, 48, 67, 74 },
    { 40, 56, 62, 42, 59, 66 },
    { 33, 49, 55, 35, 51, 58 },
    { 27, 44, 49, 28, 45, 50 },
    { 20, 36, 42, 20, 38, 44 },
    { 13, 27, 34, 13, 28, 34 },
    {  7, 17, 21,  8, 17, 21 },
    {  2,  5,  6,  2,  5,  6 }
};

/*static int qp_table[12][6] = { // optimized for SSIM
    { 67, 93, 98, 71, 98, 104 },
    { 59, 83, 88, 61, 89,  95 },
    { 50, 76, 81, 53, 85,  90 },
    { 46, 71, 77, 47, 79,  85 },
    { 41, 67, 71, 42, 75,  78 },
    { 34, 59, 65, 35, 66,  72 },
    { 30, 54, 60, 29, 60,  66 },
    { 24, 48, 53, 22, 53,  58 },
    { 18, 39, 45, 17, 43,  48 },
    { 13, 34, 38, 11, 35,  38 },
    {  8, 20, 24,  7, 22,  25 },
    {  2,  5,  6,  2,  5,   6 }
};*/

static void apply_save_options(const SaveOptions* save_options, guint width, guint height, PKPixelFormatGUID pixel_format, gboolean black_one, CWMIStrCodecParam* wmiSCP, CWMIStrCodecParam* wmiSCP_Alpha)
{
    gfloat iq_float;

    memset(wmiSCP, 0, sizeof(*wmiSCP));
    
    wmiSCP->bVerbose = FALSE;
    wmiSCP->bdBitDepth = BD_LONG;
    wmiSCP->bfBitstreamFormat = FREQUENCY;
    wmiSCP->bProgressiveMode = TRUE;    
    wmiSCP->sbSubband = SB_ALL;
    wmiSCP->uAlphaMode = has_pixel_format_alpha_channel(&pixel_format) ? 2 : 0;
    wmiSCP->bBlackWhite = black_one;    
    
    if (!has_pixel_format_color_channels(&pixel_format))
        wmiSCP->cfColorFormat = Y_ONLY;
    else
        wmiSCP->cfColorFormat = (COLORFORMAT)save_options->subsampling;
        
    iq_float = save_options->image_quality / 100.0f;
    
    if (iq_float == 1.0f)    
        wmiSCP->olOverlap = OL_NONE;
    else if (save_options->overlap == OVERLAP_AUTO)
        wmiSCP->olOverlap = iq_float >= 0.5f ? OL_ONE : OL_TWO;
    else
        wmiSCP->olOverlap = save_options->overlap - 1;
    
    if (iq_float == 1.0f)
        wmiSCP->uiDefaultQPIndex = 1;
    else
    {
        if (IsEqualGUID(&pixel_format, &GUID_PKPixelFormatBlackWhite))
            wmiSCP->uiDefaultQPIndex = (U8)(8 - 5.0f * iq_float + 0.5f);
        else
        {
            gfloat  iq;
            gint    qi;
            gint    *qp_row;
            float   qf;

            if (iq_float > 0.8f)
                iq = 0.8f + (iq_float - 0.8f) * 1.5f;
            else
                iq = iq_float;

            qi = (int)(10.0f * iq);
            qf = 10.0f * iq - (float)qi;
            
            qp_row = qp_table[qi];

            wmiSCP->uiDefaultQPIndex    = (U8)(0.5f + qp_row[0] * (1.0f - qf) + (qp_row + 6)[0] * qf);
            wmiSCP->uiDefaultQPIndexU   = (U8)(0.5f + qp_row[1] * (1.0f - qf) + (qp_row + 6)[1] * qf);
            wmiSCP->uiDefaultQPIndexV   = (U8)(0.5f + qp_row[2] * (1.0f - qf) + (qp_row + 6)[2] * qf);
            wmiSCP->uiDefaultQPIndexYHP = (U8)(0.5f + qp_row[3] * (1.0f - qf) + (qp_row + 6)[3] * qf);
            wmiSCP->uiDefaultQPIndexUHP = (U8)(0.5f + qp_row[4] * (1.0f - qf) + (qp_row + 6)[4] * qf);
            wmiSCP->uiDefaultQPIndexVHP = (U8)(0.5f + qp_row[5] * (1.0f - qf) + (qp_row + 6)[5] * qf);
        }
    }  

    if (has_pixel_format_alpha_channel(&pixel_format) && wmiSCP_Alpha != NULL)
    {
        gfloat aq_float = save_options->alpha_quality / 100.0f;
    
        if (aq_float == 1.0f)
            wmiSCP_Alpha->uiDefaultQPIndex = 1;
        else
        {
            gfloat  aq;
            int     qi;
            float   qf;
            
            if (aq_float > 0.8f)
                aq = 0.8f + (aq_float - 0.8f) * 1.5f;
            else
                aq = aq_float;

            qi = (int)(10.0f * aq);
            qf = 10.0f * aq - (float)qi;
            wmiSCP_Alpha->uiDefaultQPIndex = (U8)(0.5f + qp_table[qi][0] * (1.0f - qf) + qp_table[qi + 1][0] * qf);
        }

        wmiSCP->uiDefaultQPIndexAlpha = wmiSCP_Alpha->uiDefaultQPIndex;
    }
    
    if (save_options->tiling == TILING_NONE)
        wmiSCP->cNumOfSliceMinus1H = wmiSCP->cNumOfSliceMinus1V = 0;
    else
    {
        gint tile_size = 256 << (save_options->tiling - 1);
        
        gint i = 0;
        guint p = 0;
        
        for (i = 0; i < MAX_TILES; i++)
        {
            wmiSCP->uiTileY[i] = tile_size / 16;
            p += tile_size;
            if (p >= height)
                break;
        }
        
        wmiSCP->cNumOfSliceMinus1H = i;
        
        p = 0;
        
        for (i = 0; i < MAX_TILES; i++)
        {
            wmiSCP->uiTileX[i] = tile_size / 16;
            p += tile_size;
            if (p >= width)
                break;
        }
        
        wmiSCP->cNumOfSliceMinus1V = i;
    }
}

static gboolean show_options(SaveOptions* save_options, gboolean alpha_enabled, gboolean subsampling_enabled, const PKPixelFormatGUID* pixel_format)
{
    SaveGui     save_gui;
    gboolean    dialog_result;
    gchar*      text;
    
    save_gui.dialog = gimp_export_dialog_new(_("JPEG XR"), PLUG_IN_BINARY, NULL);

    g_signal_connect(save_gui.dialog, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    gtk_window_set_resizable(GTK_WINDOW(save_gui.dialog), FALSE);

    save_gui.vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_container_set_border_width(GTK_CONTAINER(save_gui.vbox), 12);
    gtk_box_pack_start(GTK_BOX(gimp_export_dialog_get_content_area(save_gui.dialog)), save_gui.vbox, TRUE, TRUE, 0);
    gtk_widget_show(save_gui.vbox); 

    save_gui.quality_table = gtk_table_new(2, 3, FALSE);
    gtk_table_set_col_spacings(GTK_TABLE(save_gui.quality_table), 6);
    gtk_table_set_row_spacings(GTK_TABLE(save_gui.quality_table), 6);
    gtk_box_pack_start(GTK_BOX(save_gui.vbox), save_gui.quality_table, FALSE, FALSE, 0);
    gtk_widget_show(save_gui.quality_table);

    save_gui.quality_entry = gimp_scale_entry_new(GTK_TABLE(save_gui.quality_table), 0, 0, _("_Quality:"), 125, 0, save_options->image_quality,
        0.0, 100.0, 1.0, 10.0, 0, TRUE, 0.0, 0.0,
        _("Image quality parameter. The higher the value the better the quality. Choose 100 to compress the image losslessly."),
        "file-jxr-save-quality");
        
    save_gui.alpha_quality_entry = gimp_scale_entry_new(GTK_TABLE(save_gui.quality_table), 0, 1, _("A_lpha quality:"), 125, 0, save_options->alpha_quality,
        0.0, 100.0, 1.0, 10.0, 0, TRUE, 0.0, 0.0,
        _("Alpha channel quality parameter. The higher the value the better the quality. Choose 100 to compress the alpha channel losslessly."),
        "file-jxr-save-quality");
        
    gimp_scale_entry_set_sensitive(save_gui.alpha_quality_entry, alpha_enabled);
    
    save_gui.lossless_label = gtk_label_new(_("A value of 100 guarantees lossless compression."));
    gtk_misc_set_alignment(GTK_MISC(save_gui.lossless_label), 0.0, 0.5);
    gtk_box_pack_start(GTK_BOX(save_gui.vbox), save_gui.lossless_label, FALSE, FALSE, 0);
    gtk_widget_show(save_gui.lossless_label);

    text = g_strdup_printf(_("Pixel format: %s"), get_pixel_format_mnemonic(pixel_format));
    save_gui.pixel_format_label = gtk_label_new(text);
    gtk_misc_set_alignment(GTK_MISC(save_gui.pixel_format_label), 0.0, 0.5);
    gtk_box_pack_start(GTK_BOX(save_gui.vbox), save_gui.pixel_format_label, FALSE, FALSE, 0);
    gtk_widget_show(save_gui.pixel_format_label);
    
    text = g_strdup_printf("<b>%s</b>", _("_Advanced Options"));
    save_gui.advanced_expander = gtk_expander_new_with_mnemonic(text);
    gtk_expander_set_use_markup(GTK_EXPANDER(save_gui.advanced_expander), TRUE);
    g_free(text);
    gtk_box_pack_start(GTK_BOX(save_gui.vbox), save_gui.advanced_expander, FALSE, FALSE, 0);
    gtk_widget_show(save_gui.advanced_expander);
    
    save_gui.advanced_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_container_add(GTK_CONTAINER(save_gui.advanced_expander), save_gui.advanced_vbox);
    gtk_widget_show(save_gui.advanced_vbox);

    save_gui.advanced_frame = gimp_frame_new("<expander>");
    gtk_box_pack_start(GTK_BOX(save_gui.advanced_vbox), save_gui.advanced_frame, FALSE, FALSE, 0);
    gtk_widget_show(save_gui.advanced_frame);
    
    save_gui.advanced_table = gtk_table_new(3, 3, FALSE);
    gtk_table_set_col_spacings(GTK_TABLE(save_gui.advanced_table), 6);
    gtk_table_set_col_spacing(GTK_TABLE(save_gui.advanced_table), 1, 50);
    gtk_table_set_row_spacings(GTK_TABLE(save_gui.advanced_table), 6);    
    gtk_container_add(GTK_CONTAINER(save_gui.advanced_frame), save_gui.advanced_table);
    gtk_widget_show(save_gui.advanced_table);
    
    save_gui.overlap_label = gtk_label_new_with_mnemonic(_("_Overlap:"));
    gtk_misc_set_alignment(GTK_MISC(save_gui.overlap_label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(save_gui.advanced_table), save_gui.overlap_label, 0, 1, 0, 1, GTK_FILL, (GtkAttachOptions)0, 0, 0);
    gtk_widget_show(save_gui.overlap_label);
    
    save_gui.overlap_combo_box = gtk_combo_box_new_text();
    gtk_combo_box_append_text(GTK_COMBO_BOX(save_gui.overlap_combo_box), _("Auto"));
    gtk_combo_box_append_text(GTK_COMBO_BOX(save_gui.overlap_combo_box), _("None"));
    gtk_combo_box_append_text(GTK_COMBO_BOX(save_gui.overlap_combo_box), _("One-level"));
    gtk_combo_box_append_text(GTK_COMBO_BOX(save_gui.overlap_combo_box), _("Two-level"));
    gtk_combo_box_set_active(GTK_COMBO_BOX(save_gui.overlap_combo_box), save_options->overlap);
    gtk_widget_set_tooltip_text(save_gui.overlap_combo_box, _("Higher levels reduce block artifacts but may introduce blurring and increase decoding time."));
    gtk_label_set_mnemonic_widget(GTK_LABEL(save_gui.overlap_label), save_gui.overlap_combo_box);
    gtk_table_attach(GTK_TABLE(save_gui.advanced_table), save_gui.overlap_combo_box, 1, 2, 0, 1, GTK_FILL, (GtkAttachOptions)0, 0, 0);
    gtk_widget_show(save_gui.overlap_combo_box);    
    
    save_gui.subsampling_label = gtk_label_new_with_mnemonic(_("Chroma _subsampling:"));
    gtk_misc_set_alignment(GTK_MISC(save_gui.subsampling_label), 0.0, 0.5);
    gtk_widget_set_sensitive(save_gui.subsampling_label, subsampling_enabled);
    gtk_table_attach(GTK_TABLE(save_gui.advanced_table), save_gui.subsampling_label, 0, 1, 1, 2, GTK_FILL, (GtkAttachOptions)0, 0, 0);
    gtk_widget_show(save_gui.subsampling_label);
    
    save_gui.subsampling_combo_box = gtk_combo_box_new_text();
    gtk_combo_box_append_text(GTK_COMBO_BOX(save_gui.subsampling_combo_box), _("Y-only"));
    gtk_combo_box_append_text(GTK_COMBO_BOX(save_gui.subsampling_combo_box), _("4:2:0"));
    gtk_combo_box_append_text(GTK_COMBO_BOX(save_gui.subsampling_combo_box), _("4:2:2"));
    gtk_combo_box_append_text(GTK_COMBO_BOX(save_gui.subsampling_combo_box), _("4:4:4"));
    gtk_combo_box_set_active(GTK_COMBO_BOX(save_gui.subsampling_combo_box), save_options->subsampling);
    gtk_widget_set_tooltip_text(save_gui.subsampling_combo_box, _("4:4:4 usually provides best size/quality tradeoff."));
    gtk_widget_set_sensitive(save_gui.subsampling_combo_box, subsampling_enabled);
    gtk_label_set_mnemonic_widget(GTK_LABEL(save_gui.subsampling_label), save_gui.subsampling_combo_box);
    gtk_table_attach(GTK_TABLE(save_gui.advanced_table), save_gui.subsampling_combo_box, 1, 2, 1, 2, GTK_FILL, (GtkAttachOptions)0, 0, 0);
    gtk_widget_show(save_gui.subsampling_combo_box);    
    
    save_gui.tiling_label = gtk_label_new_with_mnemonic(_("_Tiling:"));
    gtk_misc_set_alignment(GTK_MISC(save_gui.tiling_label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(save_gui.advanced_table), save_gui.tiling_label, 0, 1, 2, 3, GTK_FILL, (GtkAttachOptions)0, 0, 0);
    gtk_widget_show(save_gui.tiling_label);
    
    save_gui.tiling_combo_box = gtk_combo_box_new_text();
    gtk_combo_box_append_text(GTK_COMBO_BOX(save_gui.tiling_combo_box), _("None"));
    gtk_combo_box_append_text(GTK_COMBO_BOX(save_gui.tiling_combo_box), _("256 x 256"));
    gtk_combo_box_append_text(GTK_COMBO_BOX(save_gui.tiling_combo_box), _("512 x 512"));
    gtk_combo_box_append_text(GTK_COMBO_BOX(save_gui.tiling_combo_box), _("1024 x 1024"));
    gtk_combo_box_set_active(GTK_COMBO_BOX(save_gui.tiling_combo_box), save_options->tiling);
    gtk_widget_set_tooltip_text(save_gui.tiling_combo_box, _("Tiling optimizes the image for region decoding and is otherwise not needed."));
    gtk_label_set_mnemonic_widget(GTK_LABEL(save_gui.tiling_label), save_gui.tiling_combo_box);
    gtk_table_attach(GTK_TABLE(save_gui.advanced_table), save_gui.tiling_combo_box, 1, 2, 2, 3, GTK_FILL, (GtkAttachOptions)0, 0, 0);
    gtk_widget_show(save_gui.tiling_combo_box); 
    
    save_gui.save_metadata_check_button = gtk_check_button_new_with_mnemonic(_("Save _metadata"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(save_gui.save_metadata_check_button), save_options->save_metadata);
    gtk_misc_set_alignment(GTK_MISC(save_gui.save_metadata_check_button), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(save_gui.advanced_table), save_gui.save_metadata_check_button, 2, 3, 0, 1, GTK_FILL, (GtkAttachOptions) 0, 0, 0);
    gtk_widget_show(save_gui.save_metadata_check_button);
    
    save_gui.embed_color_profile_check_button = gtk_check_button_new_with_mnemonic(_("Embed _color profile"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(save_gui.embed_color_profile_check_button), save_options->embed_color_profile);
    gtk_misc_set_alignment(GTK_MISC(save_gui.embed_color_profile_check_button), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(save_gui.advanced_table), save_gui.embed_color_profile_check_button, 2, 3, 1, 2, GTK_FILL, (GtkAttachOptions) 0, 0, 0);
    gtk_widget_show(save_gui.embed_color_profile_check_button);

    save_gui.defaults_table = gtk_table_new(1, 3, FALSE);
    gtk_table_set_col_spacings(GTK_TABLE(save_gui.defaults_table), 6);
    gtk_box_pack_start(GTK_BOX(save_gui.vbox), save_gui.defaults_table, FALSE, FALSE, 0);
    gtk_widget_show(save_gui.defaults_table);

    save_gui.load_defaults_button = gtk_button_new_with_mnemonic(_("_Load Defaults"));
    gtk_table_attach(GTK_TABLE(save_gui.defaults_table), save_gui.load_defaults_button, 0, 1, 1, 2, GTK_FILL, (GtkAttachOptions) 0, 0, 0);
    gtk_widget_show(save_gui.load_defaults_button);

    g_signal_connect_swapped(save_gui.load_defaults_button, "clicked", G_CALLBACK(load_save_gui_defaults), &save_gui);

    save_gui.save_defaults_button = gtk_button_new_with_mnemonic(_("_Save Defaults"));
    gtk_table_attach(GTK_TABLE(save_gui.defaults_table), save_gui.save_defaults_button, 1, 2, 1, 2, GTK_FILL, (GtkAttachOptions) 0, 0, 0);
    gtk_widget_show(save_gui.save_defaults_button);

    g_signal_connect_swapped(save_gui.save_defaults_button, "clicked", G_CALLBACK(save_save_gui_defaults), &save_gui);
    
    gtk_widget_show(save_gui.dialog);

    dialog_result = gimp_dialog_run(GIMP_DIALOG(save_gui.dialog)) == GTK_RESPONSE_OK;

    get_save_options(&save_gui, save_options);

    gtk_widget_destroy(save_gui.dialog);

    return dialog_result;
}

static void load_save_gui_defaults(const SaveGui* save_gui)
{   
    SaveOptions save_options;

    if (!load_save_gui_defaults_from_parasite(&save_options))
        save_options = DEFAULT_SAVE_OPTIONS;

    update_save_gui(save_gui, &save_options);
}

static void save_save_gui_defaults(const SaveGui* save_gui)
{
    SaveOptions save_options;

    get_save_options(save_gui, &save_options);

    save_save_gui_defaults_to_parasite(&save_options);
}

static void update_save_gui(const SaveGui* save_gui, const SaveOptions* save_options)
{
    gtk_adjustment_set_value(GTK_ADJUSTMENT(save_gui->quality_entry), save_options->image_quality);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(save_gui->alpha_quality_entry), save_options->alpha_quality);
    gtk_combo_box_set_active(GTK_COMBO_BOX(save_gui->overlap_combo_box), save_options->overlap);
    gtk_combo_box_set_active(GTK_COMBO_BOX(save_gui->subsampling_combo_box), save_options->subsampling);
    gtk_combo_box_set_active(GTK_COMBO_BOX(save_gui->tiling_combo_box), save_options->tiling);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(save_gui->save_metadata_check_button), save_options->save_metadata);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(save_gui->embed_color_profile_check_button), save_options->embed_color_profile);
}

static void get_save_options(const SaveGui* save_gui, SaveOptions* save_options)
{
    save_options->image_quality = (gint) gtk_adjustment_get_value(GTK_ADJUSTMENT(save_gui->quality_entry));
    save_options->alpha_quality = (gint) gtk_adjustment_get_value(GTK_ADJUSTMENT(save_gui->alpha_quality_entry));
    save_options->overlap = gtk_combo_box_get_active(GTK_COMBO_BOX(save_gui->overlap_combo_box));
    save_options->subsampling = gtk_combo_box_get_active(GTK_COMBO_BOX(save_gui->subsampling_combo_box));
    save_options->tiling = gtk_combo_box_get_active(GTK_COMBO_BOX(save_gui->tiling_combo_box));
    save_options->save_metadata = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(save_gui->save_metadata_check_button));
    save_options->embed_color_profile = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(save_gui->embed_color_profile_check_button));
}

static gboolean load_save_gui_defaults_from_parasite(SaveOptions* save_options)
{
    GimpParasite *parasite;
    gchar        *parasite_data;
    gint          num_params_read;

    parasite = gimp_get_parasite("jxr-save-defaults");

    if (parasite == NULL)
        return FALSE;

    parasite_data = g_strndup(gimp_parasite_data(parasite), gimp_parasite_data_size(parasite));

    gimp_parasite_free(parasite);

    num_params_read = sscanf(parasite_data, "%d %d %d %d %d %d %d",
        &save_options->image_quality,
        &save_options->alpha_quality,
        &save_options->overlap,
        &save_options->subsampling,
        &save_options->tiling,
        &save_options->save_metadata,
        &save_options->embed_color_profile);

    g_free(parasite_data);

    return num_params_read == 7;
}

static void save_save_gui_defaults_to_parasite(const SaveOptions* save_options)
{
    GimpParasite *parasite;
    gchar        *parasite_data;

    parasite_data = g_strdup_printf("%d %d %d %d %d %d %d",
        save_options->image_quality,
        save_options->alpha_quality,
        save_options->overlap,
        save_options->subsampling,
        save_options->tiling,
        save_options->save_metadata,
        save_options->embed_color_profile);

    parasite = gimp_parasite_new("jxr-save-defaults", GIMP_PARASITE_PERSISTENT, strlen(parasite_data), parasite_data);

    gimp_attach_parasite(parasite);

    gimp_parasite_free(parasite);
    g_free(parasite_data);
}