#include "file-jxr.h"
#include <JXRGlue.h>
#include "utils.h"

#include <libgimp/gimpui.h>

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
    GtkWidget*  lossless_label;
    GtkWidget*  defaults_table;
    GtkWidget*  defaults_button;
} SaveGui;

static const SaveOptions DEFAULT_SAVE_OPTIONS = { 90, 100, OVERLAP_AUTO, SUBSAMPLING_444, TILING_NONE };

static ERR jxrlib_save(const gchar *filename, guint width, guint height, guint stride, gfloat res_x, gfloat res_y, PKPixelFormatGUID pixel_format, gboolean black_one, const guchar *pixels, const SaveOptions* save_options);
static void applySaveOptions(const SaveOptions* save_options, guint width, guint height, PKPixelFormatGUID pixel_format, gboolean black_one, CWMIStrCodecParam* wmiSCP, CWMIStrCodecParam* wmiSCP_Alpha);
static gboolean show_options(SaveOptions* save_options, gboolean alpha_enabled, gboolean subsampling_enabled);
static void load_save_gui_defaults(const SaveGui* save_gui);
static void open_help(const gchar* help_id, gpointer help_data);

void save(gint nparams, const GimpParam* param, gint* nreturn_vals, GimpParam** return_vals)
{
    GimpParam*              ret_values;
    GimpExportCapabilities  capabilities;
    GimpExportReturn        export_return;

    SaveOptions             save_options = DEFAULT_SAVE_OPTIONS;

    gint                    width;
    gint                    height;
    gchar*                  filename;
    gdouble                 res_x;
    gdouble                 res_y;

    GimpRunMode             run_mode;
    gint32                  image_ID;
    gint32                  drawable_ID;
    GimpImageType           image_type;
    
    gboolean                black_one;

    GimpPixelRgn            pixel_rgn;
    GimpDrawable*           drawable;
    guchar*                 pixels;
    guint                   stride;
    PKPixelFormatGUID       pixel_format;

    ERR                     err;

    gboolean                alpha_enabled;
    gboolean                subsampling_enabled;
    
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
        
    switch (image_type)
    {
    case GIMP_RGB_IMAGE:
        pixel_format = GUID_PKPixelFormat24bppRGB; 
        break;
    case GIMP_RGBA_IMAGE:
        pixel_format = GUID_PKPixelFormat32bppRGBA;
        break;
    case GIMP_GRAY_IMAGE:
        pixel_format = GUID_PKPixelFormat8bppGray;
        break;
    case GIMP_GRAYA_IMAGE:
        ret_values[1].type          = GIMP_PDB_STRING;
        ret_values[1].data.d_string = _("Grayscale images with an alpha channel are not supported.");
        return;
    case GIMP_INDEXED_IMAGE:        
        if (has_blackwhite_colormap(image_ID, &black_one))
        {
            pixel_format = GUID_PKPixelFormatBlackWhite;
        }
        else
        {
            ret_values[1].type          = GIMP_PDB_STRING;
            ret_values[1].data.d_string = _("Indexed images are not supported except for black-white colormaps.");
            return;
            /*gimp_image_delete(image_ID);
            capabilities &= GIMP_EXPORT_CAN_HANDLE_RGB | GIMP_EXPORT_CAN_HANDLE_GRAY | GIMP_EXPORT_CAN_HANDLE_ALPHA;
            goto Export;*/
        }
        break;
    case GIMP_INDEXEDA_IMAGE:
        ret_values[1].type          = GIMP_PDB_STRING;
        ret_values[1].data.d_string = _("Indexed images with an alpha channel are not supported.");
        return;
    default:
        ret_values[1].type          = GIMP_PDB_STRING;
        ret_values[1].data.d_string = _("Image has an unsupported pixel format.");
        return;
    }
    
    switch (run_mode)
    {
    case GIMP_RUN_INTERACTIVE:
        gimp_get_data(SAVE_PROC, &save_options);
        alpha_enabled = IsEqualGUID(&pixel_format, &GUID_PKPixelFormat32bppRGBA);
        subsampling_enabled = IsEqualGUID(&pixel_format, &GUID_PKPixelFormat24bppRGB) ||
            IsEqualGUID(&pixel_format, &GUID_PKPixelFormat32bppRGBA);
        if (show_options(&save_options, alpha_enabled, subsampling_enabled))
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
        if (nparams == 10)
        {
            save_options.image_quality = param[5].data.d_int32;
            save_options.alpha_quality = param[6].data.d_int32;
            save_options.overlap       = param[7].data.d_int32;
            save_options.subsampling   = param[8].data.d_int32;
            save_options.tiling        = param[9].data.d_int32;
            
            if (save_options.image_quality < 0 || save_options.image_quality > 100 ||
                save_options.alpha_quality < 0 || save_options.alpha_quality > 100 ||
                save_options.overlap < 0       || save_options.overlap > 3 ||
                save_options.subsampling < 0   || save_options.subsampling > 3 ||
                save_options.tiling < 0        || save_options.tiling > 3)
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

    drawable = gimp_drawable_get(drawable_ID);

    width   = drawable->width;
    height  = drawable->height;

    if (!gimp_image_get_resolution(image_ID, &res_x, &res_y))
    {
        res_x = 72.0;
        res_y = 72.0;
    }

    stride = width * drawable->bpp;

    err = PKAllocAligned(&pixels, stride * height, 128);
    
    if (Failed(err))
    {
        ret_values[1].type          = GIMP_PDB_STRING;
        ret_values[1].data.d_string = _("Out of memory.");
        return;
    }

    gimp_pixel_rgn_init(&pixel_rgn, drawable, 0, 0, width, height, FALSE, FALSE);
    gimp_pixel_rgn_get_rect(&pixel_rgn, pixels, 0, 0, width, height);

    gimp_drawable_detach(drawable);
    
    if (export_return == GIMP_EXPORT_EXPORT)
        gimp_image_delete(image_ID);
        
    if (IsEqualGUID(&pixel_format, &GUID_PKPixelFormatBlackWhite))
    {
        convert_indexed_bw(pixels, width, height);
        stride = (width + 7) / 8;
    }

    err = jxrlib_save(filename, width, height, stride, res_x, res_y, pixel_format, black_one, pixels, &save_options);

    PKFreeAligned(&pixels); 

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

static ERR jxrlib_save(const gchar *filename, guint width, guint height, guint stride, gfloat res_x, gfloat res_y, PKPixelFormatGUID pixel_format, gboolean black_one, const guchar *pixels, const SaveOptions* save_options)
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
    
    applySaveOptions(save_options, width, height, pixel_format, black_one, &wmiSCP, NULL);
    
    Call(encoder->Initialize(encoder, stream, &wmiSCP, sizeof(wmiSCP)));

    applySaveOptions(save_options, width, height, pixel_format, black_one, &wmiSCP, &encoder->WMP.wmiSCP_Alpha); 
    
    Call(encoder->SetPixelFormat(encoder, pixel_format));
    Call(encoder->SetSize(encoder, width, height));
    Call(encoder->SetResolution(encoder, res_x, res_y));

    Call(encoder->WritePixels(encoder, height, pixels, stride));
    
Cleanup:
    if (encoder)
        encoder->Release(&encoder);
    
    if (codec_factory)
        codec_factory->Release(&codec_factory);
    
    if (factory)
        factory->Release(&factory);
    
    return err;
}

static int qp_table[12][6] = {
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

static void applySaveOptions(const SaveOptions* save_options, guint width, guint height, PKPixelFormatGUID pixel_format, gboolean black_one, CWMIStrCodecParam* wmiSCP, CWMIStrCodecParam* wmiSCP_Alpha)
{
    gfloat iq_float;

    memset(wmiSCP, 0, sizeof(*wmiSCP));
    
    wmiSCP->bVerbose = FALSE;
    wmiSCP->bdBitDepth = BD_LONG;
    wmiSCP->bfBitstreamFormat = FREQUENCY;
    wmiSCP->bProgressiveMode = TRUE;    
    wmiSCP->sbSubband = SB_ALL;
    wmiSCP->uAlphaMode = IsEqualGUID(&pixel_format, &GUID_PKPixelFormat32bppRGBA) ? 2 : 0;
    wmiSCP->bBlackWhite = black_one;    
    
    if (IsEqualGUID(&pixel_format, &GUID_PKPixelFormatBlackWhite) ||
        IsEqualGUID(&pixel_format, &GUID_PKPixelFormat8bppGray))
        wmiSCP->cfColorFormat = Y_ONLY;
    else
        wmiSCP->cfColorFormat = (COLORFORMAT)save_options->subsampling;
        
    iq_float = save_options->image_quality / 100.0f;
    
    if (iq_float == 1.0f)    
        wmiSCP->olOverlap = OL_NONE;
    else if (save_options->overlap == OVERLAP_AUTO)
        wmiSCP->olOverlap = iq_float > 0.4f ? OL_ONE : OL_TWO;
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

    if (IsEqualGUID(&pixel_format, &GUID_PKPixelFormat32bppRGBA) && wmiSCP_Alpha != NULL)
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
        gint p = 0;
        
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

static gboolean show_options(SaveOptions* save_options, gboolean alpha_enabled, gboolean subsampling_enabled)
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
    
    save_gui.advanced_table = gtk_table_new(3, 2, FALSE);
    gtk_table_set_col_spacings(GTK_TABLE(save_gui.advanced_table), 6);
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
    gtk_combo_box_append_text(GTK_COMBO_BOX(save_gui.overlap_combo_box), _("One level"));
    gtk_combo_box_append_text(GTK_COMBO_BOX(save_gui.overlap_combo_box), _("Two level"));
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

    save_gui.defaults_table = gtk_table_new(1, 3, FALSE);
    gtk_table_set_col_spacings(GTK_TABLE(save_gui.defaults_table), 6);
    gtk_box_pack_start(GTK_BOX(save_gui.vbox), save_gui.defaults_table, FALSE, FALSE, 0);
    gtk_widget_show(save_gui.defaults_table);

    save_gui.defaults_button = gtk_button_new_with_mnemonic(_("_Load Defaults"));
    gtk_table_attach(GTK_TABLE(save_gui.defaults_table), save_gui.defaults_button, 0, 1, 1, 2, GTK_FILL, (GtkAttachOptions)0, 0, 0);
    gtk_widget_show(save_gui.defaults_button); 

    g_signal_connect_swapped(save_gui.defaults_button, "clicked", G_CALLBACK(load_save_gui_defaults), &save_gui);
    
    gtk_widget_show(save_gui.dialog);

    dialog_result = gimp_dialog_run(GIMP_DIALOG(save_gui.dialog)) == GTK_RESPONSE_OK;
    
    save_options->image_quality = gtk_adjustment_get_value(GTK_ADJUSTMENT(save_gui.quality_entry));
    save_options->alpha_quality = gtk_adjustment_get_value(GTK_ADJUSTMENT(save_gui.alpha_quality_entry));
    save_options->overlap = gtk_combo_box_get_active(GTK_COMBO_BOX(save_gui.overlap_combo_box));
    save_options->subsampling = gtk_combo_box_get_active(GTK_COMBO_BOX(save_gui.subsampling_combo_box));
    save_options->tiling = gtk_combo_box_get_active(GTK_COMBO_BOX(save_gui.tiling_combo_box));

    gtk_widget_destroy(save_gui.dialog);

    return dialog_result;
}

static void load_save_gui_defaults(const SaveGui* save_gui)
{   
    gtk_adjustment_set_value(GTK_ADJUSTMENT(save_gui->quality_entry), DEFAULT_SAVE_OPTIONS.image_quality);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(save_gui->alpha_quality_entry), DEFAULT_SAVE_OPTIONS.alpha_quality);
    gtk_combo_box_set_active(GTK_COMBO_BOX(save_gui->overlap_combo_box), DEFAULT_SAVE_OPTIONS.overlap);
    gtk_combo_box_set_active(GTK_COMBO_BOX(save_gui->subsampling_combo_box), DEFAULT_SAVE_OPTIONS.subsampling);
    gtk_combo_box_set_active(GTK_COMBO_BOX(save_gui->tiling_combo_box), DEFAULT_SAVE_OPTIONS.tiling);
}

/*static void open_help(const gchar* help_id, gpointer help_data)
{
#ifdef _WIN32
    ShellExecute(NULL, "open", "http://gimpjpegxrplugin.codeplex.com/", NULL, NULL, SW_SHOWNORMAL);
#else
    pid_t pid;
    char *args[3];

    args[0] = "/usr/bin/xdg-open";
    args[1] = "http://gimpjpegxrplugin.codeplex.com/";
    args[2] = NULL;
    
    pid = fork();
    
    if (!pid)
        execvp(args[0], args);
#endif
}*/
