#ifndef IRUKA_I18N_H
#define IRUKA_I18N_H


#include <libintl.h>

#ifndef LOCALEDIR
#define LOCALEDIR "/usr/share/locale"
#endif

#define _(String) gettext(String)

#define N_(String) String

#endif
