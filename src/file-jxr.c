#include "file-jxr.h"

static void query();
static void run(const gchar* name, gint nparams, const GimpParam* param, gint* nreturn_vals, GimpParam** return_vals);
void load(gint nparams, const GimpParam* param, gint* nreturn_vals, GimpParam** return_vals);
void save(gint nparams, const GimpParam* param, gint* nreturn_vals, GimpParam** return_vals);

const GimpPlugInInfo PLUG_IN_INFO =
{
    NULL, 
    NULL, 
    (GimpQueryProc)query,
    (GimpRunProc)run,
};

G_BEGIN_DECLS

MAIN()

static void query()
{
    static const GimpParamDef load_args[] =
    {
        { GIMP_PDB_INT32,   "run-mode",     "Interactive, non-interactive" },
        { GIMP_PDB_STRING,  "filename",     "The name of the file to load" },
        { GIMP_PDB_STRING,  "raw-filename", "The name entered" }
    };
    
    static const GimpParamDef load_return_vals[] =
    {
        { GIMP_PDB_IMAGE,   "image", "Output image" }
    };

    static const GimpParamDef save_args[] =
    {
        { GIMP_PDB_INT32,   "run-mode",         "Interactive, non-interactive" },
        { GIMP_PDB_IMAGE,   "image",            "Input image" },
        { GIMP_PDB_DRAWABLE,"drawable",         "Drawable to save" },
        { GIMP_PDB_STRING,  "filename",         "The name of the file to save the image in" },
        { GIMP_PDB_STRING,  "raw-filename",     "The name entered" },
        { GIMP_PDB_INT32,   "quality",          "Quality of saved image (0 <= quality <= 100, 100 = lossless)" },
        { GIMP_PDB_INT32,   "alpha-quality",    "Quality of alpha channel (0 <= quality <= 100, 100 = lossless)" }, 
        { GIMP_PDB_INT32,   "overlap",          "Overlap level (0 = auto, 1 = none, 2 = one level, 3 = two level)" },
        { GIMP_PDB_INT32,   "subsampling",      "Chroma subsampling (0 = Y-only, 1 = 4:2:0, 2 = 4:2:2, 3 = 4:4:4)" },
        { GIMP_PDB_INT32,   "tiling",           "Tiling (0 = none, 1 = 256 x 256, 2 = 512 x 512, 3 = 1024 x 1024)" },   
    };

    gimp_install_procedure(LOAD_PROC,
        "Loads JPEG XR images",
        "Loads JPEG XR image files.",
        "Christoph Hausner",
        "Christoph Hausner",
        "2013",
        N_("JPEG XR image"),
        NULL,
        GIMP_PLUGIN,
        G_N_ELEMENTS(load_args),
        G_N_ELEMENTS(load_return_vals),
        load_args, load_return_vals);

    gimp_register_file_handler_mime(LOAD_PROC, "image/vnd.ms-photo");
    gimp_register_magic_load_handler(LOAD_PROC, "jxr,wdp,hdp", "", "0,string,II\xBC");
    
    gimp_install_procedure(SAVE_PROC,
        "Saves JPEG XR images",
        "Saves JPEG XR image files.",
        "Christoph Hausner",
        "Christoph Hausner",
        "2013",
        N_("JPEG XR image"),
        "RGB*, GRAY, INDEXED",
        GIMP_PLUGIN,
        G_N_ELEMENTS(save_args), 0,
        save_args, 0);
    
    gimp_register_save_handler(SAVE_PROC, "jxr", "");
    gimp_register_file_handler_mime(SAVE_PROC, "image/vnd.ms-photo");
}

static void run(const gchar* name, gint nparams, const GimpParam* param, gint* nreturn_vals, GimpParam** return_vals)
{
    bindtextdomain(GETTEXT_PACKAGE, gimp_locale_directory());
    bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8");
    textdomain(GETTEXT_PACKAGE);
    
	gegl_init(NULL, NULL);

    if (strcmp(name, LOAD_PROC) == 0)
        load(nparams, param, nreturn_vals, return_vals);
    else if (strcmp(name, SAVE_PROC) == 0)
        save(nparams, param, nreturn_vals, return_vals);
}

G_END_DECLS