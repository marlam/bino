#ifndef GUICOMMON_H
#define GUICOMMON_H

#include "str.h"

#include "gettext.h"
// Qt requires strings from gettext to be in UTF-8 encoding.
#define _(string) (str::convert(gettext(string), str::localcharset(), "UTF-8").c_str())

#endif