#include "llpreprocessor.h"

namespace ndMemoryPool
{
	LL_COMMON_API void startUp();
	LL_COMMON_API void tearDown();

	LL_COMMON_API void *malloc( size_t aSize, size_t aAlign );
	LL_COMMON_API void *realloc( void *ptr, size_t aSize, size_t aAlign );

	LL_COMMON_API void free( void* ptr );

	LL_COMMON_API void dumpStats( );
	LL_COMMON_API void tryShrink( );
}
