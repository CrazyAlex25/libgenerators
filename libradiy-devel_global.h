#ifndef LIBRADIYDEVEL_GLOBAL_H
#define LIBRADIYDEVEL_GLOBAL_H

#include <QtCore/qglobal.h>

#if defined(LIBRADIYDEVEL_LIBRARY)
#  define LIBRADIYDEVELSHARED_EXPORT Q_DECL_EXPORT
#else
#  define LIBRADIYDEVELSHARED_EXPORT Q_DECL_IMPORT
#endif

#endif // LIBRADIYDEVEL_GLOBAL_H
