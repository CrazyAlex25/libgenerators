#ifndef GENERATORS_GLOBAL_H
#define GENERATORS_GLOBAL_H

#include <QtCore/qglobal.h>

#if defined(GENERATORS_LIBRARY)
# define GENERATORS_EXPORT Q_DECL_EXPORT
#else
#define GENERATORS_EXPORT Q_DECL_IMPORT
#endif

#endif // GENERATORS_GLOBAL_H
