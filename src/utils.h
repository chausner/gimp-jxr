#ifndef UTILS_H
#define UTILS_H

#include "file-jxr.h"
#include <JXRGlue.h>

guint get_bits_per_pixel(const PKPixelFormatGUID* pixel_format);
ERR convert_bw_indexed(const guchar* pixels, guint width, guint height, guchar** conv_pixels);
void convert_indexed_bw(guchar* pixels, guint width, guint height);
gboolean has_blackwhite_colormap(gint32 image_ID, gboolean* black_one);
gchar* get_pixel_format_mnemonic(const PKPixelFormatGUID* pixel_format);
gboolean has_pixel_format_alpha_channel(const PKPixelFormatGUID* pixel_format);
gboolean has_pixel_format_color_channels(const PKPixelFormatGUID* pixel_format);
void get_pixel_format_from_image_type_and_precision(GimpImageType image_type, GimpPrecision precision, PKPixelFormatGUID* pixel_format, const Babl** babl_pixel_format);
gboolean get_image_type_and_precision_from_pixel_format(const PKPixelFormatGUID* pixel_format, GimpImageBaseType* base_type, GimpImageType* image_type, GimpPrecision* precision, const Babl** babl_pixel_format, const PKPixelFormatGUID** conv_pixel_format);

typedef struct
{
	guint             width;
	guint             height;
	guint             stride;
	gfloat            resolution_x;
	gfloat            resolution_y;
	PKPixelFormatGUID pixel_format;	
	guchar*           color_context;
    guint             color_context_size;
    guchar*           exif_metadata;
    guint             exif_metadata_size;
	guchar*           xmp_metadata;
	guint             xmp_metadata_size;
    guchar*           iptc_metadata;
    guint             iptc_metadata_size;
	gboolean          black_one;
	guchar*           pixels;
} Image;

#endif
