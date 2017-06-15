#ifndef PLUGIN_INTL_H
#define PLUGIN_INTL_H

#include <glib/gi18n.h>

#define GETTEXT_PACKAGE "gimp-jxr"
#define LOCALEDIR "./share/locale"
#define HAVE_BIND_TEXTDOMAIN_CODESET

#ifndef HAVE_BIND_TEXTDOMAIN_CODESET
#define bind_textdomain_codeset(Domain, Codeset) (Domain)
#endif

#endif