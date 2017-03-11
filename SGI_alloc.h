/*
 * Copyright (c) 1996-1997
 * Silicon Graphics Computer Systems, Inc.
 *
 * Permission to use, copy, modify, distribute and sell this software
 * and its documentation for any purpose is hereby granted without fee,
 * provided that the above copyright notice appear in all copies and
 * that both that copyright notice and this permission notice appear
 * in supporting documentation.  Silicon Graphics makes no
 * representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied warranty.
 */

/* NOTE: This is an internal header file, included by other STL headers.
 *   You should not attempt to use it directly.
 */

#ifndef __SGI_STL_INTERNAL_ALLOC_H
#define __SGI_STL_INTERNAL_ALLOC_H

#ifdef __SUNPRO_CC
#  define __PRIVATE public
   // Extra access restrictions prevent us from really making some things
   // private.
#else
#  define __PRIVATE private
#endif

#ifdef __STL_STATIC_TEMPLATE_MEMBER_BUG
#  define __USE_MALLOC
#endif


// This implements some standard node allocators.  These are
// NOT the same as the allocators in the C++ draft standard or in
// in the original STL.  They do not encapsulate different pointer
// types; indeed we assume that there is only one pointer type.
// The allocation primitives are intended to allocate individual objects,
// not larger arenas as with the original STL allocators.

#if 0
#   include <new>
#   define __THROW_BAD_ALLOC throw bad_alloc
#elif !defined(__THROW_BAD_ALLOC)
#   include <iostream.h>
#   define __THROW_BAD_ALLOC cerr << "out of memory" << endl; exit(1)
#endif

#ifndef __ALLOC
#   define __ALLOC alloc
#endif
#ifdef __STL_WIN32THREADS
#   include <windows.h>
#endif

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#ifndef __RESTRICT
#  define __RESTRICT
#endif

#if !defined(_PTHREADS) && !defined(_NOTHREADS) \
 && !defined(__STL_SGI_THREADS) && !defined(__STL_WIN32THREADS)
#   define _NOTHREADS
#endif

# ifdef _PTHREADS
    // POSIX Threads
    // This is dubious, since this is likely to be a high contention
    // lock.   Performance may not be adequate.
#   include <pthread.h>
#   define __NODE_ALLOCATOR_LOCK \
        if (threads) pthread_mutex_lock(&__node_allocator_lock)
#   define __NODE_ALLOCATOR_UNLOCK \
        if (threads) pthread_mutex_unlock(&__node_allocator_lock)
#   define __NODE_ALLOCATOR_THREADS true
#   define __VOLATILE volatile  // Needed at -O3 on SGI
# endif
# ifdef __STL_WIN32THREADS
    // The lock needs to be initialized by constructing an allocator
    // objects of the right type.  We do that here explicitly for alloc.
#   define __NODE_ALLOCATOR_LOCK \
        EnterCriticalSection(&__node_allocator_lock)
#   define __NODE_ALLOCATOR_UNLOCK \
        LeaveCriticalSection(&__node_allocator_lock)
#   define __NODE_ALLOCATOR_THREADS true
#   define __VOLATILE volatile  // may not be needed
# endif /* WIN32THREADS */
# ifdef __STL_SGI_THREADS
    // This should work without threads, with sproc threads, or with
    // pthreads.  It is suboptimal in all cases.
    // It is unlikely to even compile on nonSGI machines.

    extern "C" {
      extern int __us_rsthread_malloc;
    }
	// The above is copied from malloc.h.  Including <malloc.h>
	// would be cleaner but fails with certain levels of standard
	// conformance.
#   define __NODE_ALLOCATOR_LOCK if (threads && __us_rsthread_malloc) \
                { __lock(&__node_allocator_lock); }
#   define __NODE_ALLOCATOR_UNLOCK if (threads && __us_rsthread_malloc) \
                { __unlock(&__node_allocator_lock); }
#   define __NODE_ALLOCATOR_THREADS true
#   define __VOLATILE volatile  // Needed at -O3 on SGI
# endif
# ifdef _NOTHREADS
//  Thread-unsafe
#   define __NODE_ALLOCATOR_LOCK
#   define __NODE_ALLOCATOR_UNLOCK
#   define __NODE_ALLOCATOR_THREADS false
#   define __VOLATILE
# endif

__STL_BEGIN_NAMESPACE

#if defined(__sgi) && !defined(__GNUC__) && (_MIPS_SIM != _MIPS_SIM_ABI32)
#pragma set woff 1174
#endif

// Malloc-based allocator.  Typically slower than default alloc below.
// Typically thread-safe and more storage efficient.
#ifdef __STL_STATIC_TEMPLATE_MEMBER_BUG
# ifdef __DECLARE_GLOBALS_HERE
    void (* __malloc_alloc_oom_handler)() = 0;
    // g++ 2.7.2 does not handle static template data members.
# else
    extern void (* __malloc_alloc_oom_handler)();
# endif
#endif

template <int inst>
class __malloc_alloc_template {

private:

static void *oom_malloc(size_t);							//处理内存不够的函数

static void *oom_realloc(void *, size_t);

#ifndef __STL_STATIC_TEMPLATE_MEMBER_BUG
    static void (* __malloc_alloc_oom_handler)();			//处理内存不够的一个句柄
#endif

public:

static void * allocate(size_t n)
{
    void *result = malloc(n);								//进行分配n个内存空间
    if (0 == result)										//判断是否分配成功
		result = oom_malloc(n);								//内存分配不成功，进入OOMMalloc查看是否可以操作系统释放内存分配，如果无法分配，抛出异常。
    return result;											

}
//第一级空间配置器直接使用free
static void deallocate(void *p, size_t /* n */)
{
    free(p);
}

static void * reallocate(void *p, size_t /* old_sz */, size_t new_sz)
{
    void * result = realloc(p, new_sz);
    if (0 == result) result = oom_realloc(p, new_sz);
    return result;
}

//通过这个函数设置文件句柄，从而间接实现set_new_handler。
static void (* set_malloc_handler(void (*f)()))()
{
    void (* old)() = __malloc_alloc_oom_handler;
    __malloc_alloc_oom_handler = f;				//设置文件句柄
    return(old);								//返回旧的，操作系统惯用写法。
}

};

// malloc_alloc out-of-memory handling

#ifndef __STL_STATIC_TEMPLATE_MEMBER_BUG
template <int inst>
void (* __malloc_alloc_template<inst>::__malloc_alloc_oom_handler)() = 0;
#endif

template <int inst>
void * __malloc_alloc_template<inst>::oom_malloc(size_t n)
{
    void (* my_malloc_handler)();
    void *result;

    for (;;) {
        my_malloc_handler = __malloc_alloc_oom_handler;
        if (0 == my_malloc_handler) { __THROW_BAD_ALLOC; }		//看文件句柄是否为空，为空就抛出异常
        (*my_malloc_handler)();									//如果不为空的话，那么这个时候就会去进行处理set_new_handler机制
        result = malloc(n);										//在这里进行的就是set_new_handler机制，可以使得更多的空间变成可分配的空间。
																//这里可能就会分配出内存，达到最佳性能。
        if (result) return(result);								//如果分配成功，那么就返回，否则，继续进行循环查看
    }
}

template <int inst>
void * __malloc_alloc_template<inst>::oom_realloc(void *p, size_t n)
{
    void (* my_malloc_handler)();
    void *result;

    for (;;) {
        my_malloc_handler = __malloc_alloc_oom_handler;
        if (0 == my_malloc_handler) { __THROW_BAD_ALLOC; }
        (*my_malloc_handler)();
        result = realloc(p, n);
        if (result) return(result);
    }
}

typedef __malloc_alloc_template<0> malloc_alloc;

template<class T, class Alloc>
class simple_alloc {

public:
    static T *allocate(size_t n)
                { return 0 == n? 0 : (T*) Alloc::allocate(n * sizeof (T)); }
    static T *allocate(void)
                { return (T*) Alloc::allocate(sizeof (T)); }
    static void deallocate(T *p, size_t n)
                { if (0 != n) Alloc::deallocate(p, n * sizeof (T)); }
    static void deallocate(T *p)
                { Alloc::deallocate(p, sizeof (T)); }
};

// Allocator adaptor to check size arguments for debugging.
// Reports errors using assert.  Checking can be disabled with
// NDEBUG, but it's far better to just use the underlying allocator
// instead when no checking is desired.
// There is some evidence that this can confuse Purify.
template <class Alloc>
class debug_alloc {

private:

enum {extra = 8};       // Size of space used to store size.  Note
                        // that this must be large enough to preserve
                        // alignment.

public:

static void * allocate(size_t n)
{
    char *result = (char *)Alloc::allocate(n + extra);
    *(size_t *)result = n;
    return result + extra;
}

static void deallocate(void *p, size_t n)
{
    char * real_p = (char *)p - extra;
    assert(*(size_t *)real_p == n);
    Alloc::deallocate(real_p, n + extra);
}

static void * reallocate(void *p, size_t old_sz, size_t new_sz)
{
    char * real_p = (char *)p - extra;
    assert(*(size_t *)real_p == old_sz);
    char * result = (char *)
                  Alloc::reallocate(real_p, old_sz + extra, new_sz + extra);
    *(size_t *)result = new_sz;
    return result + extra;
}


};


# ifdef __USE_MALLOC

typedef malloc_alloc alloc;
typedef malloc_alloc single_client_alloc;

# else


// Default node allocator.
// With a reasonable compiler, this should be roughly as fast as the
// original STL class-specific allocators, but with less fragmentation.
// Default_alloc_template parameters are experimental and MAY
// DISAPPEAR in the future.  Clients should just use alloc for now.
//
// Important implementation properties:
// 1. If the client request an object of size > __MAX_BYTES, the resulting
//    object will be obtained directly from malloc.
// 2. In all other cases, we allocate an object of size exactly
//    ROUND_UP(requested_size).  Thus the client has enough size
//    information that we can return the object to the proper free list
//    without permanently losing part of the object.
//

// The first template parameter specifies whether more than one thread
// may use this allocator.  It is safe to allocate an object from
// one instance of a default_alloc and deallocate it with another
// one.  This effectively transfers its ownership to the second one.
// This may have undesirable effects on reference locality.
// The second parameter is unreferenced and serves only to allow the
// creation of multiple default_alloc instances.
// Node that containers built on different allocator instances have
// different types, limiting the utility of this approach.
#ifdef __SUNPRO_CC
// breaks if we make these template class members:
  enum {__ALIGN = 8};								//最大的对齐数
  enum {__MAX_BYTES = 128};							//最大字节数
  enum {__NFREELISTS = __MAX_BYTES/__ALIGN};		//自由链表的最大个数
#endif

template <bool threads, int inst>
class __default_alloc_template {

private:
  // Really we should use static const int x = N
  // instead of enum { x = N }, but few compilers accept the former.
# ifndef __SUNPRO_CC
    enum {__ALIGN = 8};
    enum {__MAX_BYTES = 128};
    enum {__NFREELISTS = __MAX_BYTES/__ALIGN};
# endif

	//得到一个bytes最近的一个8的整数倍整数
  static size_t ROUND_UP(size_t bytes) {
        return (((bytes) + __ALIGN-1) & ~(__ALIGN - 1));
  }
__PRIVATE:

  //内存对象结构
  union obj {
        union obj * free_list_link;					//理解为内存对象的指向下一个的指针，
        char client_data[1];    /* The client sees this.        */
  };


private:
# ifdef __SUNPRO_CC
    static obj * __VOLATILE free_list[]; 
        // Specifying a size results in duplicate def for 4.1
# else
	//声明自由链表，其实就是一个指针数组
    static obj * __VOLATILE free_list[__NFREELISTS]; 
# endif
	//巧妙实现查找到对应自由链表
  static  size_t FREELIST_INDEX(size_t bytes) {
        return (((bytes) + __ALIGN-1)/__ALIGN - 1);
  }

  // Returns an object of size n, and optionally adds to size n free list.
  //填充的函数
  static void *refill(size_t n);
  // Allocates a chunk for nobjs of size size.  nobjs may be reduced
  // if it is inconvenient to allocate the requested number.
  //分配大块内存的函数
  static char *chunk_alloc(size_t size, int &nobjs);

  // Chunk allocation state.
  static char *start_free;				//维护内存池的开始
  static char *end_free;				//维护内存池的结束
  static size_t heap_size;				//记录所有的从堆上开辟的内存的大小

# ifdef __STL_SGI_THREADS
    static volatile unsigned long __node_allocator_lock;
    static void __lock(volatile unsigned long *); 
    static inline void __unlock(volatile unsigned long *);
# endif

# ifdef _PTHREADS
    static pthread_mutex_t __node_allocator_lock;
# endif

# ifdef __STL_WIN32THREADS
    static CRITICAL_SECTION __node_allocator_lock;
    static bool __node_allocator_lock_initialized;

  public:
		
    __default_alloc_template() {
	// This assumes the first constructor is called before threads
	// are started.
        if (!__node_allocator_lock_initialized) {
            InitializeCriticalSection(&__node_allocator_lock);
            __node_allocator_lock_initialized = true;
        }
    }
  private:
# endif

    class lock {
        public:
            lock() { __NODE_ALLOCATOR_LOCK; }
            ~lock() { __NODE_ALLOCATOR_UNLOCK; }
    };
    friend class lock;

public:

  /* n must be > 0      */

	//分配内存函数
  static void * allocate(size_t n)
  {
    obj * __VOLATILE * my_free_list;			//指向一个自由链表节点，在这里一定要是在内存上取到的
    obj * __RESTRICT result;					//
	//当大于128个字节的时候，去调用第一级分配器
    if (n > (size_t) __MAX_BYTES) {
        return(malloc_alloc::allocate(n));
    }
    my_free_list = free_list + FREELIST_INDEX(n);			//寻找对应的自由链表
    // Acquire the lock here with a constructor call.
    // This ensures that it is released in exit or during stack
    // unwinding.
#       ifndef _NOTHREADS
        /*REFERENCED*/
        lock lock_instance;
#       endif
    result = *my_free_list;						
    if (result == 0) {							//如果result为NULL，这个时候向利用内存池进行填充处理。
        void *r = refill(ROUND_UP(n));
        return r;
    }
	//当获得了多块内存的，时候进行调整，把第一块返回，然后剩下的放到自由链表下面

    *my_free_list = result -> free_list_link;	
    return (result);
  };

  /* p may not be 0 */
  static void deallocate(void *p, size_t n)
  {
    obj *q = (obj *)p;					//
    obj * __VOLATILE * my_free_list;


	//如果大于128个字节，那么调用第一级分配器进行释放
    if (n > (size_t) __MAX_BYTES) {
        malloc_alloc::deallocate(p, n);
        return;
    }
    my_free_list = free_list + FREELIST_INDEX(n);		//寻找字节大小对应的自由链表
    // acquire lock
#       ifndef _NOTHREADS
        /*REFERENCED*/
        lock lock_instance;
#       endif /* _NOTHREADS */
    q -> free_list_link = *my_free_list;		//放回自由链表，采用的方式就是采用头插的方式。
    *my_free_list = q;			
    // lock is released here
  }

  static void * reallocate(void *p, size_t old_sz, size_t new_sz);

} ;

typedef __default_alloc_template<__NODE_ALLOCATOR_THREADS, 0> alloc;
typedef __default_alloc_template<false, 0> single_client_alloc;



/* We allocate memory in large chunks in order to avoid fragmenting     */
/* the malloc heap too much.                                            */
/* We assume that size is properly aligned.                             */
/* We hold the allocation lock.                                         */

template <bool threads, int inst>
char*
__default_alloc_template<threads, inst>::chunk_alloc(size_t size, int& nobjs)		//在这里注意返回的是引用
{
    char * result;
	size_t total_bytes = size * nobjs;					//需求大小
    size_t bytes_left = end_free - start_free;			//内存池剩余大小

    if (bytes_left >= total_bytes) {					//当你的内存池剩余大小大于需求大小
        result = start_free;							//这个时候让result指向内存池开始
        start_free += total_bytes;						//移动需求量的大小
        return(result);									//返回
    } else if (bytes_left >= size) {					//当你的内存池剩余大小能分配1——nobjs-1个。
        nobjs = bytes_left/size;						//计算你能分配多少个	
        total_bytes = size * nobjs;						//计算分配的字节大小
        result = start_free;							//调整result
        start_free += total_bytes;						//移动start_free
        return(result);									//返回内存块
    } 
	else {												//此时内存池中连一个大小都不能分配。
        size_t bytes_to_get = 2 * total_bytes + ROUND_UP(heap_size >> 4);	//得到一个你需要向系统申请的字节大小，2倍的需求量+n
        // Try to make use of the left-over piece.

		//为了把内存池当中的剩余的内存块放入自由链表当中，采用的是头插的方式。
        if (bytes_left > 0) {			
            obj * __VOLATILE * my_free_list =
                        free_list + FREELIST_INDEX(bytes_left);

            ((obj *)start_free) -> free_list_link = *my_free_list;
            *my_free_list = (obj *)start_free;
        }

		//进行内存分配申请
        start_free = (char *)malloc(bytes_to_get);
		
		//如果申请内存失败，说明此时堆上已没有内存空间
        if (0 == start_free) {
            int i;
            obj * __VOLATILE * my_free_list, *p;
            // Try to make do with what we have.  That can't
            // hurt.  We do not try smaller requests, since that tends
            // to result in disaster on multi-process machines.

			//寻找自由链表字节数size——128字节之间的自由链表是否还有内存块可用

            for (i = size; i <= __MAX_BYTES; i += __ALIGN) {
                my_free_list = free_list + FREELIST_INDEX(i);
                p = *my_free_list;								

                if (0 != p) {												//如果有可用的大块自由链表内存块
                    *my_free_list = p -> free_list_link;					//取出这块内存块。
                    start_free = (char *)p;									//放入内存池当中。
                    end_free = start_free + i;								//调整end_free.
                    return(chunk_alloc(size, nobjs));						//返回调用，调整nobjs
                    // Any leftover piece will eventually make it to the
                    // right free list.
                }
            }
	    end_free = 0;	// In case of exception.				//修正end_free,防止出现end_free和start_free不同在0的情况，出现你的内存池剩余大小出现很大问题。
            start_free = (char *)malloc_alloc::allocate(bytes_to_get);		//调用一级分配器的，利用oom_malloc的set_new_handler机制进行分配出内存空间
            // This should either throw an
            // exception or remedy the situation.  Thus we assume it
            // succeeded.
        }
        heap_size += bytes_to_get;				//分配出来以后，调整heap_size,记录总共向堆申请的内存空间。
        end_free = start_free + bytes_to_get;	//调整end_free,把开辟出来的内存放入内存池当中。
        return(chunk_alloc(size, nobjs));		//进行递归，进行分配调整nobjs。
    }
}


/* Returns an object of size n, and optionally adds to size n free list.*/
/* We assume that n is properly aligned.                                */
/* We hold the allocation lock.                                         */
template <bool threads, int inst>
void* __default_alloc_template<threads, inst>::refill(size_t n)
{
    int nobjs = 20;										//默认开20个对象内存大小
    char * chunk = chunk_alloc(n, nobjs);				//调用chunk_alloc进行分配内存。
    obj * __VOLATILE * my_free_list;					//
    obj * result;										//
    obj * current_obj, * next_obj;						//
    int i;

    if (1 == nobjs) return(chunk);						//如果开辟是一个内存块，那么直接返回，这个时候传引用。
    my_free_list = free_list + FREELIST_INDEX(n);		//找到对应的自由链表。

    /* Build free list in chunk */
      result = (obj *)chunk;							//result指向开辟出来的第一块内存对象块
      *my_free_list = next_obj = (obj *)(chunk + n);	//把第二块放到自由链表上，此时自由链表链19个内存块。
      for (i = 1; ; i++) {								//进行内存块之间的link修正。
        current_obj = next_obj;
        next_obj = (obj *)((char *)next_obj + n);		//取到下一块内存块
        if (nobjs - 1 == i) {							
            current_obj -> free_list_link = 0;			//如果是最后一块内存块，让它的link指向NULL
            break;
        } else {										
            current_obj -> free_list_link = next_obj;	//其他情况，让当前内存块的link指向下一个内存块。
        }
      }
    return(result);				//返回第一个内存块
}

template <bool threads, int inst>
void*
__default_alloc_template<threads, inst>::reallocate(void *p,
                                                    size_t old_sz,
                                                    size_t new_sz)
{
    void * result;
    size_t copy_sz;

    if (old_sz > (size_t) __MAX_BYTES && new_sz > (size_t) __MAX_BYTES) {
        return(realloc(p, new_sz));
    }
    if (ROUND_UP(old_sz) == ROUND_UP(new_sz)) return(p);
    result = allocate(new_sz);
    copy_sz = new_sz > old_sz? old_sz : new_sz;
    memcpy(result, p, copy_sz);
    deallocate(p, old_sz);
    return(result);
}

#ifdef _PTHREADS
    template <bool threads, int inst>
    pthread_mutex_t
    __default_alloc_template<threads, inst>::__node_allocator_lock
        = PTHREAD_MUTEX_INITIALIZER;
#endif

#ifdef __STL_WIN32THREADS
    template <bool threads, int inst> CRITICAL_SECTION
    __default_alloc_template<threads, inst>::__node_allocator_lock;

    template <bool threads, int inst> bool
    __default_alloc_template<threads, inst>::__node_allocator_lock_initialized
	= false;
#endif

#ifdef __STL_SGI_THREADS
__STL_END_NAMESPACE
#include <mutex.h>
#include <time.h>
__STL_BEGIN_NAMESPACE
// Somewhat generic lock implementations.  We need only test-and-set
// and some way to sleep.  These should work with both SGI pthreads
// and sproc threads.  They may be useful on other systems.
template <bool threads, int inst>
volatile unsigned long
__default_alloc_template<threads, inst>::__node_allocator_lock = 0;

#if __mips < 3 || !(defined (_ABIN32) || defined(_ABI64)) || defined(__GNUC__)
#   define __test_and_set(l,v) test_and_set(l,v)
#endif

template <bool threads, int inst>
void 
__default_alloc_template<threads, inst>::__lock(volatile unsigned long *lock)
{
    const unsigned low_spin_max = 30;  // spin cycles if we suspect uniprocessor
    const unsigned high_spin_max = 1000; // spin cycles for multiprocessor
    static unsigned spin_max = low_spin_max;
    unsigned my_spin_max;
    static unsigned last_spins = 0;
    unsigned my_last_spins;
    static struct timespec ts = {0, 1000};
    unsigned junk;
#   define __ALLOC_PAUSE junk *= junk; junk *= junk; junk *= junk; junk *= junk
    int i;

    if (!__test_and_set((unsigned long *)lock, 1)) {
        return;
    }
    my_spin_max = spin_max;
    my_last_spins = last_spins;
    for (i = 0; i < my_spin_max; i++) {
        if (i < my_last_spins/2 || *lock) {
            __ALLOC_PAUSE;
            continue;
        }
        if (!__test_and_set((unsigned long *)lock, 1)) {
            // got it!
            // Spinning worked.  Thus we're probably not being scheduled
            // against the other process with which we were contending.
            // Thus it makes sense to spin longer the next time.
            last_spins = i;
            spin_max = high_spin_max;
            return;
        }
    }
    // We are probably being scheduled against the other process.  Sleep.
    spin_max = low_spin_max;
    for (;;) {
        if (!__test_and_set((unsigned long *)lock, 1)) {
            return;
        }
        nanosleep(&ts, 0);
    }
}

template <bool threads, int inst>
inline void
__default_alloc_template<threads, inst>::__unlock(volatile unsigned long *lock)
{
#   if defined(__GNUC__) && __mips >= 3
        asm("sync");
        *lock = 0;
#   elif __mips >= 3 && (defined (_ABIN32) || defined(_ABI64))
        __lock_release(lock);
#   else 
        *lock = 0;
        // This is not sufficient on many multiprocessors, since
        // writes to protected variables and the lock may be reordered.
#   endif
}
#endif

template <bool threads, int inst>
char *__default_alloc_template<threads, inst>::start_free = 0;		//初始设定

template <bool threads, int inst>
char *__default_alloc_template<threads, inst>::end_free = 0;		//初始设定

template <bool threads, int inst>
size_t __default_alloc_template<threads, inst>::heap_size = 0;		//初始设定

template <bool threads, int inst>
__default_alloc_template<threads, inst>::obj * __VOLATILE
__default_alloc_template<threads, inst> ::free_list[
# ifdef __SUNPRO_CC
    __NFREELISTS
# else
    __default_alloc_template<threads, inst>::__NFREELISTS
# endif
] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, };				//初始化自由链表这个指针数组
// The 16 zeros are necessary to make version 4.1 of the SunPro
// compiler happy.  Otherwise it appears to allocate too little
// space for the array.

# ifdef __STL_WIN32THREADS
  // Create one to get critical section initialized.
  // We do this onece per file, but only the first constructor
  // does anything.
  static alloc __node_allocator_dummy_instance;
# endif

#endif /* ! __USE_MALLOC */

#if defined(__sgi) && !defined(__GNUC__) && (_MIPS_SIM != _MIPS_SIM_ABI32)
#pragma reset woff 1174
#endif

__STL_END_NAMESPACE

#undef __PRIVATE

#endif /* __SGI_STL_INTERNAL_ALLOC_H */

// Local Variables:
// mode:C++
// End:
