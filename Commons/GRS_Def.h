#pragma once

#include <atlexcept.h>

#ifndef GRS_THROW_IF_FAILED
#define GRS_THROW_IF_FAILED(hr) {HRESULT _hr = (hr);if (FAILED(_hr)){ ATLTRACE("Error: 0x%08x\n",_hr); AtlThrow(_hr); }}
#endif

//更简洁的向上边界对齐算法 内存管理中常用 请记住
#ifndef GRS_UPPER
#define GRS_UPPER(A,B) ((size_t)(((A)+((B)-1))&~((B) - 1)))
#endif

//上取整除法
#ifndef GRS_UPPER_DIV
#define GRS_UPPER_DIV(A,B) ((UINT)(((A)+((B)-1))/(B)))
#endif
