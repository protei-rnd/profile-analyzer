#ifndef __PSC__TYPEDEFS_H
#define __PSC__TYPEDEFS_H

#if defined(__GNUC__)
	#include <stdint.h>
	#include <inttypes.h>
#elif defined(_MSC_VER)
#if _MSC_VER >= 1600
	#include <stdint.h>
#else
	#include "msstdint.h"
	#include "msinttypes.h"
#endif 
#else
	#error "Unsupported compiler"
#endif

#endif // __PSC__TYPEDEFS_H
