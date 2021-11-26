//
// Created by Hoper on 2021/11/25.
//

#ifndef PYOTHERSIDE_MYDEFINES_H
#define PYOTHERSIDE_MYDEFINES_H



#include <QtCore/qglobal.h>

#if defined(MYDLL_PORT)
#   define MYDLL_EXPORT Q_DECL_EXPORT
#else
#   define MYDLL_EXPORT Q_DECL_IMPORT
#endif


#endif //PYOTHERSIDE_MYDEFINES_H
