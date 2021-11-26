#pragma once
#include <functional>
#include <QVariant>
#include "mydefines.h"

typedef std::function<void(QVariant)> MYDLL_EXPORT NativeCallback;
