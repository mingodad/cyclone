#include <setjmp.h>
/* This is a C header file to be used by the output of the Cyclone to
   C translator.  The corresponding definitions are in file lib/runtime_cyc.c */
#ifndef _CYC_INCLUDE_H_
#define _CYC_INCLUDE_H_

/***********************************************************************/
/* Runtime Stack routines (runtime_stack.c).                           */
/***********************************************************************/

/* Need one of these per thread (we don't have threads)
   The runtime maintains a stack that contains either _handler_cons
   structs or _RegionHandle structs.  The tag is 0 for a handler_cons
   and 1 for a region handle.  */
struct _RuntimeStack {
  int tag; /* 0 for an exception handler, 1 for a region handle */
  struct _RuntimeStack *next;
  void (*cleanup)(struct _RuntimeStack *frame);
};

/***********************************************************************/
/* Low-level representations etc.                                      */
/***********************************************************************/

#ifndef offsetof
/* should be size_t, but int is fine. */
#define offsetof(t,n) ((int)(&(((t *)0)->n)))
#endif

/* Tagged arrays */
struct _dyneither_ptr {
  unsigned char *curr; 
  unsigned char *base; 
  unsigned char *last_plus_one; 
};  

/* Discriminated Unions */
struct _xtunion_struct { char *tag; };

/* Regions */
struct _RegionPage
#ifdef CYC_REGION_PROFILE
{ unsigned total_bytes;
  unsigned free_bytes;
  /* MWH: wish we didn't have to include the stuff below ... */
  struct _RegionPage *next;
  char data[1];
}
#endif
; // abstract -- defined in runtime_memory.c

struct _RegionHandle {
  struct _RuntimeStack s;
  struct _RegionPage *curr;
  char               *offset;
  char               *last_plus_one;
  struct _DynRegionHandle *sub_regions;
#ifdef CYC_REGION_PROFILE
  const char         *name;
#else
  unsigned used_bytes;
  unsigned wasted_bytes;
#endif
};

struct _DynRegionFrame {
  struct _RuntimeStack s;
  struct _DynRegionHandle *x;
};

// A dynamic region is just a region handle.  We have the
// wrapper struct for type abstraction reasons.
struct Cyc_Core_DynamicRegion {
  struct _RegionHandle h;
};

extern struct _RegionHandle _new_region(const char *);
extern void * _region_malloc(struct _RegionHandle *, unsigned);
extern void * _region_calloc(struct _RegionHandle *, unsigned t, unsigned n);
extern void   _free_region(struct _RegionHandle *);
extern struct _RegionHandle *_open_dynregion(struct _DynRegionFrame *f,
                                             struct _DynRegionHandle *h);
extern void   _pop_dynregion();

/* Exceptions */
struct _handler_cons {
  struct _RuntimeStack s;
  jmp_buf handler;
};
extern void _push_handler(struct _handler_cons *);
extern void _push_region(struct _RegionHandle *);
extern void _npop_handler(int);
extern void _pop_handler();
extern void _pop_region();

#ifndef _throw
extern void* _throw_null_fn(const char *filename, unsigned lineno);
extern void* _throw_arraybounds_fn(const char *filename, unsigned lineno);
extern void* _throw_badalloc_fn(const char *filename, unsigned lineno);
extern void* _throw_match_fn(const char *filename, unsigned lineno);
extern void* _throw_fn(void* e, const char *filename, unsigned lineno);
extern void* _rethrow(void* e);
#define _throw_null() (_throw_null_fn(__FILE__,__LINE__))
#define _throw_arraybounds() (_throw_arraybounds_fn(__FILE__,__LINE__))
#define _throw_badalloc() (_throw_badalloc_fn(__FILE__,__LINE__))
#define _throw_match() (_throw_match_fn(__FILE__,__LINE__))
#define _throw(e) (_throw_fn((e),__FILE__,__LINE__))
#endif

extern struct _xtunion_struct *_exn_thrown;

/* Built-in Exceptions */
struct Cyc_Null_Exception_exn_struct { char *tag; };
struct Cyc_Array_bounds_exn_struct { char *tag; };
struct Cyc_Match_Exception_exn_struct { char *tag; };
struct Cyc_Bad_alloc_exn_struct { char *tag; };
extern char Cyc_Null_Exception[];
extern char Cyc_Array_bounds[];
extern char Cyc_Match_Exception[];
extern char Cyc_Bad_alloc[];

/* Built-in Run-time Checks and company */
#ifdef CYC_ANSI_OUTPUT
#define _INLINE  
#else
#define _INLINE inline
#endif

#ifdef NO_CYC_NULL_CHECKS
#define _check_null(ptr) (ptr)
#else
#define _check_null(ptr) \
  ({ void*_cks_null = (void*)(ptr); \
     if (!_cks_null) _throw_null(); \
     _cks_null; })
#endif

#ifdef NO_CYC_BOUNDS_CHECKS
#define _check_known_subscript_null(ptr,bound,elt_sz,index)\
   ((char *)ptr) + (elt_sz)*(index))
#define _check_known_subscript_notnull(bound,index) (index)
#define _check_known_subscript_notnullX(bound,index)\
   ((char *)ptr) + (elt_sz)*(index))

#define _zero_arr_plus_char_fn(orig_x,orig_sz,orig_i,f,l) ((orig_x)+(orig_i))
#define _zero_arr_plus_short_fn(orig_x,orig_sz,orig_i,f,l) ((orig_x)+(orig_i))
#define _zero_arr_plus_int_fn(orig_x,orig_sz,orig_i,f,l) ((orig_x)+(orig_i))
#define _zero_arr_plus_float_fn(orig_x,orig_sz,orig_i,f,l) ((orig_x)+(orig_i))
#define _zero_arr_plus_double_fn(orig_x,orig_sz,orig_i,f,l) ((orig_x)+(orig_i))
#define _zero_arr_plus_longdouble_fn(orig_x,orig_sz,orig_i,f,l) ((orig_x)+(orig_i))
#define _zero_arr_plus_voidstar_fn(orig_x,orig_sz,orig_i,f,l) ((orig_x)+(orig_i))
#else
#define _check_known_subscript_null(ptr,bound,elt_sz,index) ({ \
  char*_cks_ptr = (char*)(ptr); \
  unsigned _cks_bound = (bound); \
  unsigned _cks_elt_sz = (elt_sz); \
  unsigned _cks_index = (index); \
  if (!_cks_ptr) _throw_null(); \
  if (_cks_index >= _cks_bound) _throw_arraybounds(); \
  (_cks_ptr) + _cks_elt_sz*_cks_index; })
#define _check_known_subscript_notnull(ptr,bound,elt_sz,index) ({ \
  char*_cks_ptr = (char*)(ptr); \
  unsigned _cks_bound = (bound); \
  unsigned _cks_elt_sz = (elt_sz); \
  unsigned _cks_index = (index); \
  if (_cks_index >= _cks_bound) _throw_arraybounds(); \
  (_cks_ptr) + _cks_elt_sz*_cks_index; })

/* Add i to zero-terminated pointer x.  Checks for x being null and
   ensures that x[0..i-1] are not 0. */
char * _zero_arr_plus_char_fn(char *orig_x, unsigned int orig_sz, int orig_i,const char *filename, unsigned lineno);
short * _zero_arr_plus_short_fn(short *orig_x, unsigned int orig_sz, int orig_i,const char *filename, unsigned lineno);
int * _zero_arr_plus_int_fn(int *orig_x, unsigned int orig_sz, int orig_i, const char *filename, unsigned lineno);
float * _zero_arr_plus_float_fn(float *orig_x, unsigned int orig_sz, int orig_i,const char *filename, unsigned lineno);
double * _zero_arr_plus_double_fn(double *orig_x, unsigned int orig_sz, int orig_i,const char *filename, unsigned lineno);
long double * _zero_arr_plus_longdouble_fn(long double *orig_x, unsigned int orig_sz, int orig_i, const char *filename, unsigned lineno);
void * _zero_arr_plus_voidstar_fn(void **orig_x, unsigned int orig_sz, int orig_i,const char *filename,unsigned lineno);
#endif

#define _zero_arr_plus_char(x,s,i) \
  (_zero_arr_plus_char_fn(x,s,i,__FILE__,__LINE__))
#define _zero_arr_plus_short(x,s,i) \
  (_zero_arr_plus_short_fn(x,s,i,__FILE__,__LINE__))
#define _zero_arr_plus_int(x,s,i) \
  (_zero_arr_plus_int_fn(x,s,i,__FILE__,__LINE__))
#define _zero_arr_plus_float(x,s,i) \
  (_zero_arr_plus_float_fn(x,s,i,__FILE__,__LINE__))
#define _zero_arr_plus_double(x,s,i) \
  (_zero_arr_plus_double_fn(x,s,i,__FILE__,__LINE__))
#define _zero_arr_plus_longdouble(x,s,i) \
  (_zero_arr_plus_longdouble_fn(x,s,i,__FILE__,__LINE__))
#define _zero_arr_plus_voidstar(x,s,i) \
  (_zero_arr_plus_voidstar_fn(x,s,i,__FILE__,__LINE__))

/* Calculates the number of elements in a zero-terminated, thin array.
   If non-null, the array is guaranteed to have orig_offset elements. */
int _get_zero_arr_size_char(const char *orig_x, unsigned int orig_offset);
int _get_zero_arr_size_short(const short *orig_x, unsigned int orig_offset);
int _get_zero_arr_size_int(const int *orig_x, unsigned int orig_offset);
int _get_zero_arr_size_float(const float *orig_x, unsigned int orig_offset);
int _get_zero_arr_size_double(const double *orig_x, unsigned int orig_offset);
int _get_zero_arr_size_longdouble(const long double *orig_x, unsigned int orig_offset);
int _get_zero_arr_size_voidstar(const void **orig_x, unsigned int orig_offset);

/* Does in-place addition of a zero-terminated pointer (x += e and ++x).  
   Note that this expands to call _zero_arr_plus_<type>_fn. */
char * _zero_arr_inplace_plus_char_fn(char **x, int orig_i,const char *filename,unsigned lineno);
short * _zero_arr_inplace_plus_short_fn(short **x, int orig_i,const char *filename,unsigned lineno);
int * _zero_arr_inplace_plus_int(int **x, int orig_i,const char *filename,unsigned lineno);
float * _zero_arr_inplace_plus_float_fn(float **x, int orig_i,const char *filename,unsigned lineno);
double * _zero_arr_inplace_plus_double_fn(double **x, int orig_i,const char *filename,unsigned lineno);
long double * _zero_arr_inplace_plus_longdouble_fn(long double **x, int orig_i,const char *filename,unsigned lineno);
void * _zero_arr_inplace_plus_voidstar_fn(void ***x, int orig_i,const char *filename,unsigned lineno);
#define _zero_arr_inplace_plus_char(x,i) \
  _zero_arr_inplace_plus_char_fn((char **)(x),i,__FILE__,__LINE__)
#define _zero_arr_inplace_plus_short(x,i) \
  _zero_arr_inplace_plus_short_fn((short **)(x),i,__FILE__,__LINE__)
#define _zero_arr_inplace_plus_int(x,i) \
  _zero_arr_inplace_plus_int_fn((int **)(x),i,__FILE__,__LINE__)
#define _zero_arr_inplace_plus_float(x,i) \
  _zero_arr_inplace_plus_float_fn((float **)(x),i,__FILE__,__LINE__)
#define _zero_arr_inplace_plus_double(x,i) \
  _zero_arr_inplace_plus_double_fn((double **)(x),i,__FILE__,__LINE__)
#define _zero_arr_inplace_plus_longdouble(x,i) \
  _zero_arr_inplace_plus_longdouble_fn((long double **)(x),i,__FILE__,__LINE__)
#define _zero_arr_inplace_plus_voidstar(x,i) \
  _zero_arr_inplace_plus_voidstar_fn((void ***)(x),i,__FILE__,__LINE__)

/* Does in-place increment of a zero-terminated pointer (e.g., x++). */
char * _zero_arr_inplace_plus_post_char_fn(char **x, int orig_i,const char *filename,unsigned lineno);
short * _zero_arr_inplace_plus_post_short_fn(short **x, int orig_i,const char *filename,unsigned lineno);
int * _zero_arr_inplace_plus_post_int_fn(int **x, int orig_i,const char *filename, unsigned lineno);
float * _zero_arr_inplace_plus_post_float_fn(float **x, int orig_i,const char *filename, unsigned lineno);
double * _zero_arr_inplace_plus_post_double_fn(double **x, int orig_i,const char *filename,unsigned lineno);
long double * _zero_arr_inplace_plus_post_longdouble_fn(long double **x, int orig_i,const char *filename,unsigned lineno);
void ** _zero_arr_inplace_plus_post_voidstar_fn(void ***x, int orig_i,const char *filename,unsigned lineno);
#define _zero_arr_inplace_plus_post_char(x,i) \
  _zero_arr_inplace_plus_post_char_fn((char **)(x),(i),__FILE__,__LINE__)
#define _zero_arr_inplace_plus_post_short(x,i) \
  _zero_arr_inplace_plus_post_short_fn((short **)(x),(i),__FILE__,__LINE__)
#define _zero_arr_inplace_plus_post_int(x,i) \
  _zero_arr_inplace_plus_post_int_fn((int **)(x),(i),__FILE__,__LINE__)
#define _zero_arr_inplace_plus_post_float(x,i) \
  _zero_arr_inplace_plus_post_float_fn((float **)(x),(i),__FILE__,__LINE__)
#define _zero_arr_inplace_plus_post_double(x,i) \
  _zero_arr_inplace_plus_post_double_fn((double **)(x),(i),__FILE__,__LINE__)
#define _zero_arr_inplace_plus_post_longdouble(x,i) \
  _zero_arr_inplace_plus_post_longdouble_fn((long double **)(x),(i),__FILE__,__LINE__)
#define _zero_arr_inplace_plus_post_voidstar(x,i) \
  _zero_arr_inplace_plus_post_voidstar_fn((void***)(x),(i),__FILE__,__LINE__)

/* functions for dealing with dynamically sized pointers */
#ifdef NO_CYC_BOUNDS_CHECKS
#define _check_dyneither_subscript(arr,elt_sz,index) ({ \
  struct _dyneither_ptr _cus_arr = (arr); \
  unsigned _cus_elt_sz = (elt_sz); \
  unsigned _cus_index = (index); \
  unsigned char *_cus_ans = _cus_arr.curr + _cus_elt_sz * _cus_index; \
  _cus_ans; })
#else
#define _check_dyneither_subscript(arr,elt_sz,index) ({ \
  struct _dyneither_ptr _cus_arr = (arr); \
  unsigned _cus_elt_sz = (elt_sz); \
  unsigned _cus_index = (index); \
  unsigned char *_cus_ans = _cus_arr.curr + _cus_elt_sz * _cus_index; \
  /* JGM: not needed! if (!_cus_arr.base) _throw_null();*/ \
  if (_cus_ans < _cus_arr.base || _cus_ans >= _cus_arr.last_plus_one) \
    _throw_arraybounds(); \
  _cus_ans; })
#endif

#define _tag_dyneither(tcurr,elt_sz,num_elts) ({ \
  struct _dyneither_ptr _tag_arr_ans; \
  _tag_arr_ans.base = _tag_arr_ans.curr = (void*)(tcurr); \
  /* JGM: if we're tagging NULL, ignore num_elts */ \
  _tag_arr_ans.last_plus_one = _tag_arr_ans.base ? (_tag_arr_ans.base + (elt_sz) * (num_elts)) : 0; \
  _tag_arr_ans; })

#ifdef NO_CYC_BOUNDS_CHECKS
#define _untag_dyneither_ptr(arr,elt_sz,num_elts) ((arr).curr)
#else
#define _untag_dyneither_ptr(arr,elt_sz,num_elts) ({ \
  struct _dyneither_ptr _arr = (arr); \
  unsigned char *_curr = _arr.curr; \
  if ((_curr < _arr.base || _curr + (elt_sz) * (num_elts) > _arr.last_plus_one) &&\
      _curr != (unsigned char *)0) \
    _throw_arraybounds(); \
  _curr; })
#endif

#define _get_dyneither_size(arr,elt_sz) \
  ({struct _dyneither_ptr _get_arr_size_temp = (arr); \
    unsigned char *_get_arr_size_curr=_get_arr_size_temp.curr; \
    unsigned char *_get_arr_size_last=_get_arr_size_temp.last_plus_one; \
    (_get_arr_size_curr < _get_arr_size_temp.base || \
     _get_arr_size_curr >= _get_arr_size_last) ? 0 : \
    ((_get_arr_size_last - _get_arr_size_curr) / (elt_sz));})

#define _dyneither_ptr_plus(arr,elt_sz,change) ({ \
  struct _dyneither_ptr _ans = (arr); \
  _ans.curr += ((int)(elt_sz))*(change); \
  _ans; })

#define _dyneither_ptr_inplace_plus(arr_ptr,elt_sz,change) ({ \
  struct _dyneither_ptr * _arr_ptr = (arr_ptr); \
  _arr_ptr->curr += ((int)(elt_sz))*(change); \
  *_arr_ptr; })

#define _dyneither_ptr_inplace_plus_post(arr_ptr,elt_sz,change) ({ \
  struct _dyneither_ptr * _arr_ptr = (arr_ptr); \
  struct _dyneither_ptr _ans = *_arr_ptr; \
  _arr_ptr->curr += ((int)(elt_sz))*(change); \
  _ans; })

/* Decrease the upper bound on a fat pointer by numelts where sz is
   the size of the pointer's type.  Note that this can't be a macro
   if we're to get initializers right. */
static struct
 _dyneither_ptr _dyneither_ptr_decrease_size(struct _dyneither_ptr x,
                                            unsigned int sz,
                                            unsigned int numelts) {
  unsigned delta = sz * numelts;
  /* Don't let someone decrease the size so much that it wraps around.
   * This is crucial to get NULL right. */
  if (x.last_plus_one - x.base >= delta)
    x.last_plus_one -= delta;
  else x.last_plus_one = x.base;
  return x; 
}

/* Allocation */
extern void* GC_malloc(int);
extern void* GC_malloc_atomic(int);
extern void* GC_calloc(unsigned,unsigned);
extern void* GC_calloc_atomic(unsigned,unsigned);
/* bound the allocation size to be less than MAX_ALLOC_SIZE,
   which is defined in runtime_memory.c
*/
extern void* _bounded_GC_malloc(int,const char *file,int lineno);
extern void* _bounded_GC_malloc_atomic(int,const char *file,int lineno);
extern void* _bounded_GC_calloc(unsigned n, unsigned s,
                                const char *file,int lineno);
extern void* _bounded_GC_calloc_atomic(unsigned n, unsigned s,
                                       const char *file,
                                       int lineno);
/* FIX?  Not sure if we want to pass filename and lineno in here... */
#ifndef CYC_REGION_PROFILE
#define _cycalloc(n) _bounded_GC_malloc(n,__FILE__,__LINE__)
#define _cycalloc_atomic(n) _bounded_GC_malloc_atomic(n,__FILE__,__LINE__)
#define _cyccalloc(n,s) _bounded_GC_calloc(n,s,__FILE__,__LINE__)
#define _cyccalloc_atomic(n,s) _bounded_GC_calloc_atomic(n,s,__FILE__,__LINE__)
#endif

#define MAX_MALLOC_SIZE (1 << 28)
static _INLINE unsigned int _check_times(unsigned x, unsigned y) {
  unsigned long long whole_ans = 
    ((unsigned long long) x)*((unsigned long long)y);
  unsigned word_ans = (unsigned)whole_ans;
  if(word_ans < whole_ans || word_ans > MAX_MALLOC_SIZE)
    _throw_badalloc();
  return word_ans;
}

#define _CYC_MAX_REGION_CONST 2
#define _CYC_MIN_ALIGNMENT (sizeof(double))

#ifdef CYC_REGION_PROFILE
extern int rgn_total_bytes;
#endif

static _INLINE void *_fast_region_malloc(struct _RegionHandle *r, unsigned orig_s) {  
  if (r > (struct _RegionHandle *)_CYC_MAX_REGION_CONST && r->curr != 0) { 
#ifdef CYC_NOALIGN
    unsigned s =  orig_s;
#else
    unsigned s =  (orig_s + _CYC_MIN_ALIGNMENT - 1) & (~(_CYC_MIN_ALIGNMENT -1)); 
#endif
    char *result; 
    result = r->offset; 
    if (s <= (r->last_plus_one - result)) {
      r->offset = result + s; 
#ifdef CYC_REGION_PROFILE
    r->curr->free_bytes = r->curr->free_bytes - s;
    rgn_total_bytes += s;
#endif
      return result;
    }
  } 
  return _region_malloc(r,orig_s); 
}

#if defined(CYC_REGION_PROFILE) 
extern void* _profile_GC_malloc(int,const char *file,const char *func,
                                int lineno);
extern void* _profile_GC_malloc_atomic(int,const char *file,
                                       const char *func,int lineno);
extern void* _profile_GC_calloc(unsigned n, unsigned s,
                                const char *file, const char *func, int lineno);
extern void* _profile_GC_calloc_atomic(unsigned n, unsigned s,
                                       const char *file, const char *func,
                                       int lineno);
extern void* _profile_region_malloc(struct _RegionHandle *, unsigned,
                                    const char *file,
                                    const char *func,
                                    int lineno);
extern void* _profile_region_calloc(struct _RegionHandle *, unsigned,
                                    unsigned,
                                    const char *file,
                                    const char *func,
                                    int lineno);
extern struct _RegionHandle _profile_new_region(const char *rgn_name,
						const char *file,
						const char *func,
                                                int lineno);
extern void _profile_free_region(struct _RegionHandle *,
				 const char *file,
                                 const char *func,
                                 int lineno);
#  if !defined(RUNTIME_CYC)
#define _new_region(n) _profile_new_region(n,__FILE__,__FUNCTION__,__LINE__)
#define _free_region(r) _profile_free_region(r,__FILE__,__FUNCTION__,__LINE__)
#define _region_malloc(rh,n) _profile_region_malloc(rh,n,__FILE__,__FUNCTION__,__LINE__)
#define _region_calloc(rh,n,t) _profile_region_calloc(rh,n,t,__FILE__,__FUNCTION__,__LINE__)
#  endif
#define _cycalloc(n) _profile_GC_malloc(n,__FILE__,__FUNCTION__,__LINE__)
#define _cycalloc_atomic(n) _profile_GC_malloc_atomic(n,__FILE__,__FUNCTION__,__LINE__)
#define _cyccalloc(n,s) _profile_GC_calloc(n,s,__FILE__,__FUNCTION__,__LINE__)
#define _cyccalloc_atomic(n,s) _profile_GC_calloc_atomic(n,s,__FILE__,__FUNCTION__,__LINE__)
#endif
#endif
 struct Cyc___cycFILE;struct Cyc_String_pa_PrintArg_struct{int tag;struct _dyneither_ptr f1;};struct Cyc_Int_pa_PrintArg_struct{int tag;unsigned long f1;};struct Cyc_Double_pa_PrintArg_struct{int tag;double f1;};struct Cyc_LongDouble_pa_PrintArg_struct{int tag;long double f1;};struct Cyc_ShortPtr_pa_PrintArg_struct{int tag;short*f1;};struct Cyc_IntPtr_pa_PrintArg_struct{int tag;unsigned long*f1;};struct Cyc_ShortPtr_sa_ScanfArg_struct{int tag;short*f1;};struct Cyc_UShortPtr_sa_ScanfArg_struct{int tag;unsigned short*f1;};struct Cyc_IntPtr_sa_ScanfArg_struct{int tag;int*f1;};struct Cyc_UIntPtr_sa_ScanfArg_struct{int tag;unsigned int*f1;};struct Cyc_StringPtr_sa_ScanfArg_struct{int tag;struct _dyneither_ptr f1;};struct Cyc_DoublePtr_sa_ScanfArg_struct{int tag;double*f1;};struct Cyc_FloatPtr_sa_ScanfArg_struct{int tag;float*f1;};struct Cyc_CharPtr_sa_ScanfArg_struct{int tag;struct _dyneither_ptr f1;};extern char Cyc_FileCloseError[15U];struct Cyc_FileCloseError_exn_struct{char*tag;};extern char Cyc_FileOpenError[14U];struct Cyc_FileOpenError_exn_struct{char*tag;struct _dyneither_ptr f1;};
# 310 "cycboot.h"
int toupper(int);struct Cyc_Core_Opt{void*v;};
# 97 "core.h"
struct _dyneither_ptr Cyc_Core_new_string(unsigned int);
# 101
struct _dyneither_ptr Cyc_Core_rnew_string(struct _RegionHandle*,unsigned int);extern char Cyc_Core_Invalid_argument[17U];struct Cyc_Core_Invalid_argument_exn_struct{char*tag;struct _dyneither_ptr f1;};extern char Cyc_Core_Failure[8U];struct Cyc_Core_Failure_exn_struct{char*tag;struct _dyneither_ptr f1;};extern char Cyc_Core_Impossible[11U];struct Cyc_Core_Impossible_exn_struct{char*tag;struct _dyneither_ptr f1;};extern char Cyc_Core_Not_found[10U];struct Cyc_Core_Not_found_exn_struct{char*tag;};extern char Cyc_Core_Unreachable[12U];struct Cyc_Core_Unreachable_exn_struct{char*tag;struct _dyneither_ptr f1;};
# 167
extern struct _RegionHandle*Cyc_Core_heap_region;
# 170
extern struct _RegionHandle*Cyc_Core_unique_region;struct Cyc_Core_DynamicRegion;struct Cyc_Core_NewDynamicRegion{struct Cyc_Core_DynamicRegion*key;};
# 317 "core.h"
struct _dyneither_ptr Cyc_Core_mkfat(void*arr,unsigned int s,unsigned int n);struct Cyc_List_List{void*hd;struct Cyc_List_List*tl;};
# 61 "list.h"
int Cyc_List_length(struct Cyc_List_List*x);extern char Cyc_List_List_mismatch[14U];struct Cyc_List_List_mismatch_exn_struct{char*tag;};extern char Cyc_List_Nth[4U];struct Cyc_List_Nth_exn_struct{char*tag;};
# 38 "string.h"
unsigned long Cyc_strlen(struct _dyneither_ptr s);
# 49 "string.h"
int Cyc_strcmp(struct _dyneither_ptr s1,struct _dyneither_ptr s2);
int Cyc_strptrcmp(struct _dyneither_ptr*s1,struct _dyneither_ptr*s2);
int Cyc_strncmp(struct _dyneither_ptr s1,struct _dyneither_ptr s2,unsigned long len);
int Cyc_zstrcmp(struct _dyneither_ptr,struct _dyneither_ptr);
int Cyc_zstrncmp(struct _dyneither_ptr s1,struct _dyneither_ptr s2,unsigned long n);
int Cyc_zstrptrcmp(struct _dyneither_ptr*,struct _dyneither_ptr*);
int Cyc_strcasecmp(struct _dyneither_ptr,struct _dyneither_ptr);
int Cyc_strncasecmp(struct _dyneither_ptr s1,struct _dyneither_ptr s2,unsigned long len);
# 60
struct _dyneither_ptr Cyc_strcat(struct _dyneither_ptr dest,struct _dyneither_ptr src);
# 62
struct _dyneither_ptr Cyc_strconcat(struct _dyneither_ptr,struct _dyneither_ptr);
struct _dyneither_ptr Cyc_rstrconcat(struct _RegionHandle*,struct _dyneither_ptr,struct _dyneither_ptr);
struct _dyneither_ptr Cyc_strconcat_l(struct Cyc_List_List*);
struct _dyneither_ptr Cyc_rstrconcat_l(struct _RegionHandle*,struct Cyc_List_List*);
struct _dyneither_ptr Cyc_str_sepstr(struct Cyc_List_List*,struct _dyneither_ptr);
struct _dyneither_ptr Cyc_rstr_sepstr(struct _RegionHandle*,struct Cyc_List_List*,struct _dyneither_ptr);
# 71
struct _dyneither_ptr Cyc_strcpy(struct _dyneither_ptr dest,struct _dyneither_ptr src);
struct _dyneither_ptr Cyc_strncpy(struct _dyneither_ptr,struct _dyneither_ptr,unsigned long);
struct _dyneither_ptr Cyc_zstrncpy(struct _dyneither_ptr,struct _dyneither_ptr,unsigned long);
# 76
struct _dyneither_ptr Cyc_realloc(struct _dyneither_ptr,unsigned long);
struct _dyneither_ptr Cyc_rrealloc(struct _RegionHandle*,struct _dyneither_ptr s,unsigned long sz);
# 81
struct _dyneither_ptr Cyc__memcpy(struct _dyneither_ptr d,struct _dyneither_ptr s,unsigned long,unsigned int);
struct _dyneither_ptr Cyc__memmove(struct _dyneither_ptr d,struct _dyneither_ptr s,unsigned long,unsigned int);
int Cyc_memcmp(struct _dyneither_ptr s1,struct _dyneither_ptr s2,unsigned long n);
struct _dyneither_ptr Cyc_memchr(struct _dyneither_ptr s,char c,unsigned long n);
struct _dyneither_ptr Cyc_mmemchr(struct _dyneither_ptr s,char c,unsigned long n);
struct _dyneither_ptr Cyc_memset(struct _dyneither_ptr s,char c,unsigned long n);
void Cyc_bzero(struct _dyneither_ptr s,unsigned long n);
void Cyc__bcopy(struct _dyneither_ptr src,struct _dyneither_ptr dst,unsigned long n,unsigned int sz);
# 99 "string.h"
struct _dyneither_ptr Cyc_expand(struct _dyneither_ptr s,unsigned long sz);
struct _dyneither_ptr Cyc_rexpand(struct _RegionHandle*,struct _dyneither_ptr s,unsigned long sz);
struct _dyneither_ptr Cyc_realloc_str(struct _dyneither_ptr str,unsigned long sz);
struct _dyneither_ptr Cyc_rrealloc_str(struct _RegionHandle*r,struct _dyneither_ptr str,unsigned long sz);
struct _dyneither_ptr Cyc_strdup(struct _dyneither_ptr src);
struct _dyneither_ptr Cyc_rstrdup(struct _RegionHandle*,struct _dyneither_ptr src);
# 108
struct _dyneither_ptr Cyc_substring(struct _dyneither_ptr,int ofs,unsigned long n);
struct _dyneither_ptr Cyc_rsubstring(struct _RegionHandle*,struct _dyneither_ptr,int ofs,unsigned long n);
# 113
struct _dyneither_ptr Cyc_replace_suffix(struct _dyneither_ptr,struct _dyneither_ptr,struct _dyneither_ptr);
struct _dyneither_ptr Cyc_rreplace_suffix(struct _RegionHandle*r,struct _dyneither_ptr src,struct _dyneither_ptr curr_suffix,struct _dyneither_ptr new_suffix);
# 119
struct _dyneither_ptr Cyc_strchr(struct _dyneither_ptr s,char c);
struct _dyneither_ptr Cyc_mstrchr(struct _dyneither_ptr s,char c);
struct _dyneither_ptr Cyc_mstrrchr(struct _dyneither_ptr s,char c);
struct _dyneither_ptr Cyc_strrchr(struct _dyneither_ptr s,char c);
struct _dyneither_ptr Cyc_mstrstr(struct _dyneither_ptr haystack,struct _dyneither_ptr needle);
struct _dyneither_ptr Cyc_strstr(struct _dyneither_ptr haystack,struct _dyneither_ptr needle);
struct _dyneither_ptr Cyc_strpbrk(struct _dyneither_ptr s,struct _dyneither_ptr accept);
struct _dyneither_ptr Cyc_mstrpbrk(struct _dyneither_ptr s,struct _dyneither_ptr accept);
unsigned long Cyc_strspn(struct _dyneither_ptr s,struct _dyneither_ptr accept);
unsigned long Cyc_strcspn(struct _dyneither_ptr s,struct _dyneither_ptr accept);
struct _dyneither_ptr Cyc_strtok(struct _dyneither_ptr s,struct _dyneither_ptr delim);
# 133
struct Cyc_List_List*Cyc_explode(struct _dyneither_ptr s);
struct Cyc_List_List*Cyc_rexplode(struct _RegionHandle*,struct _dyneither_ptr s);
struct _dyneither_ptr Cyc_implode(struct Cyc_List_List*c);
# 25 "strings.h"
int Cyc_strcasecmp(struct _dyneither_ptr,struct _dyneither_ptr);
int Cyc_strncasecmp(struct _dyneither_ptr s1,struct _dyneither_ptr s2,unsigned long len);
# 29 "assert.h"
void*Cyc___assert_fail(struct _dyneither_ptr assertion,struct _dyneither_ptr file,unsigned int line);
# 39 "string.cyc"
unsigned long Cyc_strlen(struct _dyneither_ptr s){
unsigned long i;
unsigned int sz=_get_dyneither_size(s,sizeof(char));
for(i=0U;i < sz;++ i){
if(*((const char*)_check_dyneither_subscript(s,sizeof(char),(int)i))== '\000')
return i;}
# 46
return i;}
# 52
int Cyc_strcmp(struct _dyneither_ptr s1,struct _dyneither_ptr s2){
if((char*)s1.curr == (char*)s2.curr)
return 0;{
int i=0;
unsigned int sz1=_get_dyneither_size(s1,sizeof(char));
unsigned int sz2=_get_dyneither_size(s2,sizeof(char));
unsigned int minsz=sz1 < sz2?sz1: sz2;
minsz <= _get_dyneither_size(s1,sizeof(char)) && minsz <= _get_dyneither_size(s2,sizeof(char))?0:({struct _dyneither_ptr _tmp7E=({const char*_tmp0="minsz <= numelts(s1) && minsz <= numelts(s2)";_tag_dyneither(_tmp0,sizeof(char),45U);});((int(*)(struct _dyneither_ptr assertion,struct _dyneither_ptr file,unsigned int line))Cyc___assert_fail)(_tmp7E,({const char*_tmp1="string.cyc";_tag_dyneither(_tmp1,sizeof(char),11U);}),59U);});
while(i < minsz){
char c1=*((const char*)_check_dyneither_subscript(s1,sizeof(char),i));
char c2=*((const char*)_check_dyneither_subscript(s2,sizeof(char),i));
if(c1 == '\000'){
if(c2 == '\000')return 0;else{
return - 1;}}else{
if(c2 == '\000')return 1;else{
# 68
int diff=c1 - c2;
if(diff != 0)return diff;}}
# 71
++ i;}
# 73
if(sz1 == sz2)return 0;
if(minsz < sz2){
if(*((const char*)_check_dyneither_subscript(s2,sizeof(char),i))== '\000')return 0;else{
return - 1;}}else{
# 78
if(*((const char*)_check_dyneither_subscript(s1,sizeof(char),i))== '\000')return 0;else{
return 1;}}};}
# 83
int Cyc_strptrcmp(struct _dyneither_ptr*s1,struct _dyneither_ptr*s2){
return Cyc_strcmp((struct _dyneither_ptr)*s1,(struct _dyneither_ptr)*s2);}
# 87
inline static int Cyc_ncmp(struct _dyneither_ptr s1,unsigned long len1,struct _dyneither_ptr s2,unsigned long len2,unsigned long n){
# 90
if(n <= 0)return 0;{
# 92
unsigned long min_len=len1 > len2?len2: len1;
unsigned long bound=min_len > n?n: min_len;
# 95
bound <= _get_dyneither_size(s1,sizeof(char)) && bound <= _get_dyneither_size(s2,sizeof(char))?0:({struct _dyneither_ptr _tmp7F=({const char*_tmp2="bound <= numelts(s1) && bound <= numelts(s2)";_tag_dyneither(_tmp2,sizeof(char),45U);});((int(*)(struct _dyneither_ptr assertion,struct _dyneither_ptr file,unsigned int line))Cyc___assert_fail)(_tmp7F,({const char*_tmp3="string.cyc";_tag_dyneither(_tmp3,sizeof(char),11U);}),95U);});
# 97
{int i=0;for(0;i < bound;++ i){
int retc;
if(({int _tmp81=({char _tmp80=*((const char*)_check_dyneither_subscript(s1,sizeof(char),i));_tmp80 - *((const char*)_check_dyneither_subscript(s2,sizeof(char),i));});retc=_tmp81;})!= 0)
return retc;}}
# 102
if(len1 < n  || len2 < n)
return(int)len1 - (int)len2;
return 0;};}
# 109
int Cyc_strncmp(struct _dyneither_ptr s1,struct _dyneither_ptr s2,unsigned long n){
unsigned long len1=Cyc_strlen(s1);
unsigned long len2=Cyc_strlen(s2);
return Cyc_ncmp(s1,len1,s2,len2,n);}
# 119
int Cyc_zstrcmp(struct _dyneither_ptr a,struct _dyneither_ptr b){
if((char*)a.curr == (char*)b.curr)
return 0;{
unsigned long as=_get_dyneither_size(a,sizeof(char));
unsigned long bs=_get_dyneither_size(b,sizeof(char));
unsigned long min_length=as < bs?as: bs;
int i=-1;
# 127
min_length <= _get_dyneither_size(a,sizeof(char)) && min_length <= _get_dyneither_size(b,sizeof(char))?0:({struct _dyneither_ptr _tmp82=({const char*_tmp4="min_length <= numelts(a) && min_length <= numelts(b)";_tag_dyneither(_tmp4,sizeof(char),53U);});((int(*)(struct _dyneither_ptr assertion,struct _dyneither_ptr file,unsigned int line))Cyc___assert_fail)(_tmp82,({const char*_tmp5="string.cyc";_tag_dyneither(_tmp5,sizeof(char),11U);}),127U);});
# 129
while((++ i,i < min_length)){
int diff=({int _tmp83=(int)*((const char*)_check_dyneither_subscript(a,sizeof(char),i));_tmp83 - (int)*((const char*)_check_dyneither_subscript(b,sizeof(char),i));});
if(diff != 0)
return diff;}
# 134
return(int)as - (int)bs;};}
# 137
int Cyc_zstrncmp(struct _dyneither_ptr s1,struct _dyneither_ptr s2,unsigned long n){
if(n <= 0)return 0;{
# 140
unsigned long s1size=_get_dyneither_size(s1,sizeof(char));
unsigned long s2size=_get_dyneither_size(s2,sizeof(char));
unsigned long min_size=s1size > s2size?s2size: s1size;
unsigned long bound=min_size > n?n: min_size;
# 145
bound <= _get_dyneither_size(s1,sizeof(char)) && bound <= _get_dyneither_size(s2,sizeof(char))?0:({struct _dyneither_ptr _tmp84=({const char*_tmp6="bound <= numelts(s1) && bound <= numelts(s2)";_tag_dyneither(_tmp6,sizeof(char),45U);});((int(*)(struct _dyneither_ptr assertion,struct _dyneither_ptr file,unsigned int line))Cyc___assert_fail)(_tmp84,({const char*_tmp7="string.cyc";_tag_dyneither(_tmp7,sizeof(char),11U);}),145U);});
# 147
{int i=0;for(0;i < bound;++ i){
if(({char _tmp85=*((const char*)_check_dyneither_subscript(s1,sizeof(char),i));_tmp85 < *((const char*)_check_dyneither_subscript(s2,sizeof(char),i));}))
return - 1;else{
if(({char _tmp86=*((const char*)_check_dyneither_subscript(s2,sizeof(char),i));_tmp86 < *((const char*)_check_dyneither_subscript(s1,sizeof(char),i));}))
return 1;}}}
# 153
if(min_size <= bound)
return 0;
if(s1size < s2size)
return - 1;else{
# 158
return 1;}};}
# 162
int Cyc_zstrptrcmp(struct _dyneither_ptr*a,struct _dyneither_ptr*b){
if((int)a == (int)b)return 0;
return Cyc_zstrcmp((struct _dyneither_ptr)*a,(struct _dyneither_ptr)*b);}
# 172
inline static struct _dyneither_ptr Cyc_int_strcato(struct _dyneither_ptr dest,struct _dyneither_ptr src){
int i;
unsigned long dsize;unsigned long slen;unsigned long dlen;
# 176
dsize=_get_dyneither_size(dest,sizeof(char));
({unsigned long _tmp87=Cyc_strlen((struct _dyneither_ptr)dest);dlen=_tmp87;});
({unsigned long _tmp88=Cyc_strlen(src);slen=_tmp88;});
# 180
if(slen + dlen <= dsize){
slen <= _get_dyneither_size(src,sizeof(char))?0:({struct _dyneither_ptr _tmp89=({const char*_tmp8="slen <= numelts(src)";_tag_dyneither(_tmp8,sizeof(char),21U);});((int(*)(struct _dyneither_ptr assertion,struct _dyneither_ptr file,unsigned int line))Cyc___assert_fail)(_tmp89,({const char*_tmp9="string.cyc";_tag_dyneither(_tmp9,sizeof(char),11U);}),181U);});
for(i=0;i < slen;++ i){
({struct _dyneither_ptr _tmpA=_dyneither_ptr_plus(dest,sizeof(char),(int)(i + dlen));char _tmpB=*((char*)_check_dyneither_subscript(_tmpA,sizeof(char),0U));char _tmpC=*((const char*)_check_dyneither_subscript(src,sizeof(char),i));if(_get_dyneither_size(_tmpA,sizeof(char))== 1U  && (_tmpB == '\000'  && _tmpC != '\000'))_throw_arraybounds();*((char*)_tmpA.curr)=_tmpC;});}
# 185
if(i + dlen < dsize)
({struct _dyneither_ptr _tmpD=_dyneither_ptr_plus(dest,sizeof(char),(int)(i + dlen));char _tmpE=*((char*)_check_dyneither_subscript(_tmpD,sizeof(char),0U));char _tmpF='\000';if(_get_dyneither_size(_tmpD,sizeof(char))== 1U  && (_tmpE == '\000'  && _tmpF != '\000'))_throw_arraybounds();*((char*)_tmpD.curr)=_tmpF;});}else{
# 189
(int)_throw((void*)({struct Cyc_Core_Invalid_argument_exn_struct*_tmp11=_cycalloc(sizeof(*_tmp11));_tmp11->tag=Cyc_Core_Invalid_argument,({struct _dyneither_ptr _tmp8A=({const char*_tmp10="strcat";_tag_dyneither(_tmp10,sizeof(char),7U);});_tmp11->f1=_tmp8A;});_tmp11;}));}
return dest;}
# 195
struct _dyneither_ptr Cyc_strcat(struct _dyneither_ptr dest,struct _dyneither_ptr src){
return Cyc_int_strcato(dest,src);}
# 200
struct _dyneither_ptr Cyc_rstrconcat(struct _RegionHandle*r,struct _dyneither_ptr a,struct _dyneither_ptr b){
unsigned long _tmp12=Cyc_strlen(a);
unsigned long _tmp13=Cyc_strlen(b);
struct _dyneither_ptr ans=Cyc_Core_rnew_string(r,(_tmp12 + _tmp13)+ 1);
int i;int j;
_tmp12 <= _get_dyneither_size(ans,sizeof(char)) && _tmp12 <= _get_dyneither_size(a,sizeof(char))?0:({struct _dyneither_ptr _tmp8B=({const char*_tmp14="alen <= numelts(ans) && alen <= numelts(a)";_tag_dyneither(_tmp14,sizeof(char),43U);});((int(*)(struct _dyneither_ptr assertion,struct _dyneither_ptr file,unsigned int line))Cyc___assert_fail)(_tmp8B,({const char*_tmp15="string.cyc";_tag_dyneither(_tmp15,sizeof(char),11U);}),205U);});
for(i=0;i < _tmp12;++ i){({struct _dyneither_ptr _tmp16=_dyneither_ptr_plus(ans,sizeof(char),i);char _tmp17=*((char*)_check_dyneither_subscript(_tmp16,sizeof(char),0U));char _tmp18=*((const char*)_check_dyneither_subscript(a,sizeof(char),i));if(_get_dyneither_size(_tmp16,sizeof(char))== 1U  && (_tmp17 == '\000'  && _tmp18 != '\000'))_throw_arraybounds();*((char*)_tmp16.curr)=_tmp18;});}
_tmp13 <= _get_dyneither_size(b,sizeof(char))?0:({struct _dyneither_ptr _tmp8C=({const char*_tmp19="blen <= numelts(b)";_tag_dyneither(_tmp19,sizeof(char),19U);});((int(*)(struct _dyneither_ptr assertion,struct _dyneither_ptr file,unsigned int line))Cyc___assert_fail)(_tmp8C,({const char*_tmp1A="string.cyc";_tag_dyneither(_tmp1A,sizeof(char),11U);}),207U);});
for(j=0;j < _tmp13;++ j){({struct _dyneither_ptr _tmp1B=_dyneither_ptr_plus(ans,sizeof(char),i + j);char _tmp1C=*((char*)_check_dyneither_subscript(_tmp1B,sizeof(char),0U));char _tmp1D=*((const char*)_check_dyneither_subscript(b,sizeof(char),j));if(_get_dyneither_size(_tmp1B,sizeof(char))== 1U  && (_tmp1C == '\000'  && _tmp1D != '\000'))_throw_arraybounds();*((char*)_tmp1B.curr)=_tmp1D;});}
return ans;}
# 212
struct _dyneither_ptr Cyc_strconcat(struct _dyneither_ptr a,struct _dyneither_ptr b){
return Cyc_rstrconcat(Cyc_Core_heap_region,a,b);}
# 217
struct _dyneither_ptr Cyc_rstrconcat_l(struct _RegionHandle*r,struct Cyc_List_List*strs){
# 219
unsigned long len;
unsigned long total_len=0U;
struct _dyneither_ptr ans;
struct _RegionHandle _tmp1E=_new_region("temp");struct _RegionHandle*temp=& _tmp1E;_push_region(temp);{
struct Cyc_List_List*lens=({struct Cyc_List_List*_tmp21=_region_malloc(temp,sizeof(*_tmp21));
_tmp21->hd=(void*)0U,_tmp21->tl=0;_tmp21;});
# 226
struct Cyc_List_List*end=lens;
{struct Cyc_List_List*p=strs;for(0;p != 0;p=p->tl){
({unsigned long _tmp8D=Cyc_strlen((struct _dyneither_ptr)*((struct _dyneither_ptr*)p->hd));len=_tmp8D;});
total_len +=len;
({struct Cyc_List_List*_tmp8E=({struct Cyc_List_List*_tmp1F=_region_malloc(temp,sizeof(*_tmp1F));_tmp1F->hd=(void*)len,_tmp1F->tl=0;_tmp1F;});((struct Cyc_List_List*)_check_null(end))->tl=_tmp8E;});
end=end->tl;}}
# 233
lens=lens->tl;
({struct _dyneither_ptr _tmp8F=Cyc_Core_rnew_string(r,total_len + 1);ans=_tmp8F;});{
unsigned long i=0U;
while(strs != 0){
struct _dyneither_ptr _tmp20=*((struct _dyneither_ptr*)strs->hd);
len=(unsigned long)((struct Cyc_List_List*)_check_null(lens))->hd;
({struct _dyneither_ptr _tmp91=_dyneither_ptr_decrease_size(_dyneither_ptr_plus(ans,sizeof(char),(int)i),sizeof(char),1U);struct _dyneither_ptr _tmp90=(struct _dyneither_ptr)_tmp20;Cyc_strncpy(_tmp91,_tmp90,len);});
i +=len;
strs=strs->tl;
lens=lens->tl;}};}{
# 245
struct _dyneither_ptr _tmp22=ans;_npop_handler(0U);return _tmp22;};
# 222
;_pop_region(temp);}
# 248
struct _dyneither_ptr Cyc_strconcat_l(struct Cyc_List_List*strs){
return Cyc_rstrconcat_l(Cyc_Core_heap_region,strs);}
# 254
struct _dyneither_ptr Cyc_rstr_sepstr(struct _RegionHandle*r,struct Cyc_List_List*strs,struct _dyneither_ptr separator){
if(strs == 0)return Cyc_Core_rnew_string(r,0U);
if(strs->tl == 0)return Cyc_rstrdup(r,(struct _dyneither_ptr)*((struct _dyneither_ptr*)strs->hd));{
struct Cyc_List_List*_tmp23=strs;
struct _RegionHandle _tmp24=_new_region("temp");struct _RegionHandle*temp=& _tmp24;_push_region(temp);
{struct Cyc_List_List*lens=({struct Cyc_List_List*_tmp28=_region_malloc(temp,sizeof(*_tmp28));_tmp28->hd=(void*)0U,_tmp28->tl=0;_tmp28;});
struct Cyc_List_List*end=lens;
unsigned long len;
unsigned long total_len=0U;
unsigned long list_len=0U;
for(0;_tmp23 != 0;_tmp23=_tmp23->tl){
({unsigned long _tmp92=Cyc_strlen((struct _dyneither_ptr)*((struct _dyneither_ptr*)_tmp23->hd));len=_tmp92;});
total_len +=len;
({struct Cyc_List_List*_tmp93=({struct Cyc_List_List*_tmp25=_region_malloc(temp,sizeof(*_tmp25));_tmp25->hd=(void*)len,_tmp25->tl=0;_tmp25;});((struct Cyc_List_List*)_check_null(end))->tl=_tmp93;});
end=end->tl;
++ list_len;}
# 271
lens=lens->tl;{
unsigned long seplen=Cyc_strlen(separator);
total_len +=(list_len - 1)* seplen;{
struct _dyneither_ptr ans=Cyc_Core_rnew_string(r,total_len + 1);
unsigned long i=0U;
while(((struct Cyc_List_List*)_check_null(strs))->tl != 0){
struct _dyneither_ptr _tmp26=*((struct _dyneither_ptr*)strs->hd);
len=(unsigned long)((struct Cyc_List_List*)_check_null(lens))->hd;
({struct _dyneither_ptr _tmp95=_dyneither_ptr_decrease_size(_dyneither_ptr_plus(ans,sizeof(char),(int)i),sizeof(char),1U);struct _dyneither_ptr _tmp94=(struct _dyneither_ptr)_tmp26;Cyc_strncpy(_tmp95,_tmp94,len);});
i +=len;
({struct _dyneither_ptr _tmp97=_dyneither_ptr_decrease_size(_dyneither_ptr_plus(ans,sizeof(char),(int)i),sizeof(char),1U);struct _dyneither_ptr _tmp96=separator;Cyc_strncpy(_tmp97,_tmp96,seplen);});
i +=seplen;
strs=strs->tl;
lens=lens->tl;}
# 286
({struct _dyneither_ptr _tmp99=_dyneither_ptr_decrease_size(_dyneither_ptr_plus(ans,sizeof(char),(int)i),sizeof(char),1U);struct _dyneither_ptr _tmp98=(struct _dyneither_ptr)*((struct _dyneither_ptr*)strs->hd);Cyc_strncpy(_tmp99,_tmp98,(unsigned long)((struct Cyc_List_List*)_check_null(lens))->hd);});{
struct _dyneither_ptr _tmp27=ans;_npop_handler(0U);return _tmp27;};};};}
# 259
;_pop_region(temp);};}
# 291
struct _dyneither_ptr Cyc_str_sepstr(struct Cyc_List_List*strs,struct _dyneither_ptr separator){
return Cyc_rstr_sepstr(Cyc_Core_heap_region,strs,separator);}
# 296
struct _dyneither_ptr Cyc_strncpy(struct _dyneither_ptr dest,struct _dyneither_ptr src,unsigned long n){
int i;
n <= _get_dyneither_size(src,sizeof(char)) && n <= _get_dyneither_size(dest,sizeof(char))?0:({struct _dyneither_ptr _tmp9A=({const char*_tmp29="n <= numelts(src) && n <= numelts(dest)";_tag_dyneither(_tmp29,sizeof(char),40U);});((int(*)(struct _dyneither_ptr assertion,struct _dyneither_ptr file,unsigned int line))Cyc___assert_fail)(_tmp9A,({const char*_tmp2A="string.cyc";_tag_dyneither(_tmp2A,sizeof(char),11U);}),298U);});
for(i=0;i < n;++ i){
char _tmp2B=*((const char*)_check_dyneither_subscript(src,sizeof(char),i));
if(_tmp2B == '\000')break;
*((char*)_check_dyneither_subscript(dest,sizeof(char),i))=_tmp2B;}
# 304
for(0;i < n;++ i){
*((char*)_check_dyneither_subscript(dest,sizeof(char),i))='\000';}
# 307
return dest;}
# 311
struct _dyneither_ptr Cyc_zstrncpy(struct _dyneither_ptr dest,struct _dyneither_ptr src,unsigned long n){
n <= _get_dyneither_size(dest,sizeof(char)) && n <= _get_dyneither_size(src,sizeof(char))?0:({struct _dyneither_ptr _tmp9B=({const char*_tmp2C="n <= numelts(dest) && n <= numelts(src)";_tag_dyneither(_tmp2C,sizeof(char),40U);});((int(*)(struct _dyneither_ptr assertion,struct _dyneither_ptr file,unsigned int line))Cyc___assert_fail)(_tmp9B,({const char*_tmp2D="string.cyc";_tag_dyneither(_tmp2D,sizeof(char),11U);}),312U);});{
int i;
for(i=0;i < n;++ i){
({char _tmp9C=*((const char*)_check_dyneither_subscript(src,sizeof(char),i));*((char*)_check_dyneither_subscript(dest,sizeof(char),i))=_tmp9C;});}
return dest;};}
# 319
struct _dyneither_ptr Cyc_strcpy(struct _dyneither_ptr dest,struct _dyneither_ptr src){
unsigned int ssz=_get_dyneither_size(src,sizeof(char));
unsigned int dsz=_get_dyneither_size(dest,sizeof(char));
if(ssz <= dsz){
unsigned int i;
for(i=0U;i < ssz;++ i){
char _tmp2E=*((const char*)_check_dyneither_subscript(src,sizeof(char),(int)i));
({struct _dyneither_ptr _tmp2F=_dyneither_ptr_plus(dest,sizeof(char),(int)i);char _tmp30=*((char*)_check_dyneither_subscript(_tmp2F,sizeof(char),0U));char _tmp31=_tmp2E;if(_get_dyneither_size(_tmp2F,sizeof(char))== 1U  && (_tmp30 == '\000'  && _tmp31 != '\000'))_throw_arraybounds();*((char*)_tmp2F.curr)=_tmp31;});
if(_tmp2E == '\000')break;}}else{
# 331
unsigned long len=Cyc_strlen(src);
({struct _dyneither_ptr _tmp9E=_dyneither_ptr_decrease_size(dest,sizeof(char),1U);struct _dyneither_ptr _tmp9D=src;Cyc_strncpy(_tmp9E,_tmp9D,len);});
if(len < _get_dyneither_size(dest,sizeof(char)))
({struct _dyneither_ptr _tmp32=_dyneither_ptr_plus(dest,sizeof(char),(int)len);char _tmp33=*((char*)_check_dyneither_subscript(_tmp32,sizeof(char),0U));char _tmp34='\000';if(_get_dyneither_size(_tmp32,sizeof(char))== 1U  && (_tmp33 == '\000'  && _tmp34 != '\000'))_throw_arraybounds();*((char*)_tmp32.curr)=_tmp34;});}
# 336
return dest;}
# 342
struct _dyneither_ptr Cyc_rstrdup(struct _RegionHandle*r,struct _dyneither_ptr src){
unsigned long len;
struct _dyneither_ptr temp;
# 346
({unsigned long _tmp9F=Cyc_strlen(src);len=_tmp9F;});
({struct _dyneither_ptr _tmpA0=Cyc_Core_rnew_string(r,len + 1);temp=_tmpA0;});
{struct _dyneither_ptr _tmp35=_dyneither_ptr_decrease_size(temp,sizeof(char),1U);
struct _dyneither_ptr _tmp36=_tmp35;struct _dyneither_ptr _tmp37;_LL1: _tmp37=_tmp36;_LL2:;
Cyc_strncpy(_tmp37,src,len);}
# 352
return temp;}
# 355
struct _dyneither_ptr Cyc_strdup(struct _dyneither_ptr src){
return Cyc_rstrdup(Cyc_Core_heap_region,src);}
# 359
struct _dyneither_ptr Cyc_rrealloc(struct _RegionHandle*r,struct _dyneither_ptr s,unsigned long sz){
struct _dyneither_ptr temp;
unsigned long slen;
# 363
slen=_get_dyneither_size(s,sizeof(char));
sz=sz > slen?sz: slen;
({struct _dyneither_ptr _tmpA1=({unsigned int _tmp38=sz;_tag_dyneither(_region_calloc(r,sizeof(char),_tmp38),sizeof(char),_tmp38);});temp=_tmpA1;});
# 367
{struct _dyneither_ptr _tmp39=temp;struct _dyneither_ptr _tmp3A;_LL1: _tmp3A=_tmp39;_LL2:;
Cyc_strncpy((struct _dyneither_ptr)_tmp3A,(struct _dyneither_ptr)s,slen);}
# 371
return temp;}
# 374
struct _dyneither_ptr Cyc_rexpand(struct _RegionHandle*r,struct _dyneither_ptr s,unsigned long sz){
struct _dyneither_ptr temp;
unsigned long slen;
# 378
({unsigned long _tmpA2=Cyc_strlen(s);slen=_tmpA2;});
sz=sz > slen?sz: slen;
({struct _dyneither_ptr _tmpA3=Cyc_Core_rnew_string(r,sz);temp=_tmpA3;});
# 382
{struct _dyneither_ptr _tmp3B=temp;struct _dyneither_ptr _tmp3C;_LL1: _tmp3C=_tmp3B;_LL2:;
Cyc_strncpy((struct _dyneither_ptr)_tmp3C,s,slen);}
# 386
if(slen != _get_dyneither_size(s,sizeof(char)))
({struct _dyneither_ptr _tmp3D=_dyneither_ptr_plus(temp,sizeof(char),(int)slen);char _tmp3E=*((char*)_check_dyneither_subscript(_tmp3D,sizeof(char),0U));char _tmp3F='\000';if(_get_dyneither_size(_tmp3D,sizeof(char))== 1U  && (_tmp3E == '\000'  && _tmp3F != '\000'))_throw_arraybounds();*((char*)_tmp3D.curr)=_tmp3F;});
# 389
return temp;}
# 392
struct _dyneither_ptr Cyc_expand(struct _dyneither_ptr s,unsigned long sz){
return Cyc_rexpand(Cyc_Core_heap_region,s,sz);}
# 396
struct _dyneither_ptr Cyc_rrealloc_str(struct _RegionHandle*r,struct _dyneither_ptr str,unsigned long sz){
# 398
unsigned long maxsizeP=_get_dyneither_size(str,sizeof(char));
# 400
if(maxsizeP == 0){
maxsizeP=30 > sz?30U: sz;
({struct _dyneither_ptr _tmpA4=Cyc_Core_rnew_string(r,maxsizeP);str=_tmpA4;});
({struct _dyneither_ptr _tmp40=_dyneither_ptr_plus(str,sizeof(char),0);char _tmp41=*((char*)_check_dyneither_subscript(_tmp40,sizeof(char),0U));char _tmp42='\000';if(_get_dyneither_size(_tmp40,sizeof(char))== 1U  && (_tmp41 == '\000'  && _tmp42 != '\000'))_throw_arraybounds();*((char*)_tmp40.curr)=_tmp42;});}else{
# 405
if(sz > maxsizeP){
maxsizeP=maxsizeP * 2 > (sz * 5)/ 4?maxsizeP * 2:(sz * 5)/ 4;{
struct _dyneither_ptr _tmp43=str;struct _dyneither_ptr _tmp44;_LL1: _tmp44=_tmp43;_LL2:;
({struct _dyneither_ptr _tmpA5=_dyneither_ptr_decrease_size(Cyc_rexpand(r,(struct _dyneither_ptr)_tmp44,maxsizeP),sizeof(char),1U);_tmp44=_tmpA5;});};}}
# 411
return str;}
# 414
struct _dyneither_ptr Cyc_realloc_str(struct _dyneither_ptr str,unsigned long sz){
return Cyc_rrealloc_str(Cyc_Core_heap_region,str,sz);}
# 423
struct _dyneither_ptr Cyc_rsubstring(struct _RegionHandle*r,struct _dyneither_ptr s,int start,unsigned long amt){
# 427
struct _dyneither_ptr ans=Cyc_Core_rnew_string(r,amt + 1);
({struct _dyneither_ptr _tmpA6=_dyneither_ptr_plus(s,sizeof(char),start);s=_tmpA6;});
amt < _get_dyneither_size(ans,sizeof(char)) && amt <= _get_dyneither_size(s,sizeof(char))?0:({struct _dyneither_ptr _tmpA7=({const char*_tmp45="amt < numelts(ans) && amt <= numelts(s)";_tag_dyneither(_tmp45,sizeof(char),40U);});((int(*)(struct _dyneither_ptr assertion,struct _dyneither_ptr file,unsigned int line))Cyc___assert_fail)(_tmpA7,({const char*_tmp46="string.cyc";_tag_dyneither(_tmp46,sizeof(char),11U);}),429U);});
{unsigned long i=0U;for(0;i < amt;++ i){
({struct _dyneither_ptr _tmp47=_dyneither_ptr_plus(ans,sizeof(char),(int)i);char _tmp48=*((char*)_check_dyneither_subscript(_tmp47,sizeof(char),0U));char _tmp49=*((const char*)_check_dyneither_subscript(s,sizeof(char),(int)i));if(_get_dyneither_size(_tmp47,sizeof(char))== 1U  && (_tmp48 == '\000'  && _tmp49 != '\000'))_throw_arraybounds();*((char*)_tmp47.curr)=_tmp49;});}}
({struct _dyneither_ptr _tmp4A=_dyneither_ptr_plus(ans,sizeof(char),(int)amt);char _tmp4B=*((char*)_check_dyneither_subscript(_tmp4A,sizeof(char),0U));char _tmp4C='\000';if(_get_dyneither_size(_tmp4A,sizeof(char))== 1U  && (_tmp4B == '\000'  && _tmp4C != '\000'))_throw_arraybounds();*((char*)_tmp4A.curr)=_tmp4C;});
return ans;}
# 436
struct _dyneither_ptr Cyc_substring(struct _dyneither_ptr s,int start,unsigned long amt){
return Cyc_rsubstring(Cyc_Core_heap_region,s,start,amt);}
# 442
struct _dyneither_ptr Cyc_rreplace_suffix(struct _RegionHandle*r,struct _dyneither_ptr src,struct _dyneither_ptr curr_suffix,struct _dyneither_ptr new_suffix){
unsigned long m=_get_dyneither_size(src,sizeof(char));
unsigned long n=_get_dyneither_size(curr_suffix,sizeof(char));
struct _dyneither_ptr err=({const char*_tmp4F="replace_suffix";_tag_dyneither(_tmp4F,sizeof(char),15U);});
if(m < n)
(int)_throw((void*)({struct Cyc_Core_Invalid_argument_exn_struct*_tmp4D=_cycalloc(sizeof(*_tmp4D));_tmp4D->tag=Cyc_Core_Invalid_argument,_tmp4D->f1=err;_tmp4D;}));
{unsigned long i=1U;for(0;i <= n;++ i){
if(({char _tmpA8=*((const char*)_check_dyneither_subscript(src,sizeof(char),(int)(m - i)));_tmpA8 != *((const char*)_check_dyneither_subscript(curr_suffix,sizeof(char),(int)(n - i)));}))
(int)_throw((void*)({struct Cyc_Core_Invalid_argument_exn_struct*_tmp4E=_cycalloc(sizeof(*_tmp4E));_tmp4E->tag=Cyc_Core_Invalid_argument,_tmp4E->f1=err;_tmp4E;}));}}{
struct _dyneither_ptr ans=Cyc_Core_rnew_string(r,((m - n)+ _get_dyneither_size(new_suffix,sizeof(char)))+ 1);
({struct _dyneither_ptr _tmpAA=_dyneither_ptr_decrease_size(ans,sizeof(char),1U);struct _dyneither_ptr _tmpA9=src;Cyc_strncpy(_tmpAA,_tmpA9,m - n);});
({struct _dyneither_ptr _tmpAC=_dyneither_ptr_decrease_size(_dyneither_ptr_plus(ans,sizeof(char),(int)(m - n)),sizeof(char),1U);struct _dyneither_ptr _tmpAB=new_suffix;Cyc_strncpy(_tmpAC,_tmpAB,_get_dyneither_size(new_suffix,sizeof(char)));});
return ans;};}
# 457
struct _dyneither_ptr Cyc_replace_suffix(struct _dyneither_ptr src,struct _dyneither_ptr curr_suffix,struct _dyneither_ptr new_suffix){
return Cyc_rreplace_suffix(Cyc_Core_heap_region,src,curr_suffix,new_suffix);}
# 464
struct _dyneither_ptr Cyc_strpbrk(struct _dyneither_ptr s,struct _dyneither_ptr accept){
int len=(int)_get_dyneither_size(s,sizeof(char));
unsigned int asize=_get_dyneither_size(accept,sizeof(char));
char c;
unsigned int i;
for(i=0U;i < len  && (c=*((const char*)_check_dyneither_subscript(s,sizeof(char),(int)i)))!= 0;++ i){
unsigned int j=0U;for(0;j < asize;++ j){
if(c == *((const char*)_check_dyneither_subscript(accept,sizeof(char),(int)j)))
return _dyneither_ptr_plus(s,sizeof(char),(int)i);}}
# 474
return _tag_dyneither(0,0,0);}
# 477
struct _dyneither_ptr Cyc_mstrpbrk(struct _dyneither_ptr s,struct _dyneither_ptr accept){
int len=(int)_get_dyneither_size(s,sizeof(char));
unsigned int asize=_get_dyneither_size(accept,sizeof(char));
char c;
unsigned int i;
for(i=0U;i < len  && (c=*((char*)_check_dyneither_subscript(s,sizeof(char),(int)i)))!= 0;++ i){
unsigned int j=0U;for(0;j < asize;++ j){
if(c == *((const char*)_check_dyneither_subscript(accept,sizeof(char),(int)j)))
return _dyneither_ptr_plus(s,sizeof(char),(int)i);}}
# 487
return _tag_dyneither(0,0,0);}
# 492
struct _dyneither_ptr Cyc_mstrchr(struct _dyneither_ptr s,char c){
int len=(int)_get_dyneither_size(s,sizeof(char));
char c2;
unsigned int i;
# 497
for(i=0U;i < len  && (c2=*((char*)_check_dyneither_subscript(s,sizeof(char),(int)i)))!= 0;++ i){
if(c2 == c)return _dyneither_ptr_plus(s,sizeof(char),(int)i);}
# 500
return _tag_dyneither(0,0,0);}
# 503
struct _dyneither_ptr Cyc_strchr(struct _dyneither_ptr s,char c){
int len=(int)_get_dyneither_size(s,sizeof(char));
char c2;
unsigned int i;
# 508
for(i=0U;i < len  && (c2=*((const char*)_check_dyneither_subscript(s,sizeof(char),(int)i)))!= 0;++ i){
if(c2 == c)return _dyneither_ptr_plus(s,sizeof(char),(int)i);}
# 511
return _tag_dyneither(0,0,0);}
# 516
struct _dyneither_ptr Cyc_strrchr(struct _dyneither_ptr s0,char c){
int len=(int)Cyc_strlen((struct _dyneither_ptr)s0);
int i=len - 1;
struct _dyneither_ptr s=s0;
_dyneither_ptr_inplace_plus(& s,sizeof(char),i);
# 522
for(0;i >= 0;(i --,_dyneither_ptr_inplace_plus_post(& s,sizeof(char),-1))){
if(*((const char*)_check_dyneither_subscript(s,sizeof(char),0U))== c)
return(struct _dyneither_ptr)s;}
# 526
return _tag_dyneither(0,0,0);}
# 529
struct _dyneither_ptr Cyc_mstrrchr(struct _dyneither_ptr s0,char c){
int len=(int)Cyc_strlen((struct _dyneither_ptr)s0);
int i=len - 1;
struct _dyneither_ptr s=s0;
_dyneither_ptr_inplace_plus(& s,sizeof(char),i);
# 535
for(0;i >= 0;(i --,_dyneither_ptr_inplace_plus_post(& s,sizeof(char),-1))){
if(*((char*)_check_dyneither_subscript(s,sizeof(char),0U))== c)
return(struct _dyneither_ptr)s;}
# 539
return _tag_dyneither(0,0,0);}
# 544
struct _dyneither_ptr Cyc_strstr(struct _dyneither_ptr haystack,struct _dyneither_ptr needle){
if(!((unsigned int)haystack.curr) || !((unsigned int)needle.curr))(int)_throw((void*)({struct Cyc_Core_Invalid_argument_exn_struct*_tmp51=_cycalloc(sizeof(*_tmp51));_tmp51->tag=Cyc_Core_Invalid_argument,({struct _dyneither_ptr _tmpAD=({const char*_tmp50="strstr";_tag_dyneither(_tmp50,sizeof(char),7U);});_tmp51->f1=_tmpAD;});_tmp51;}));
if(*((const char*)_check_dyneither_subscript(needle,sizeof(char),0U))== '\000')return haystack;{
# 548
int len=(int)Cyc_strlen((struct _dyneither_ptr)needle);
{struct _dyneither_ptr start=haystack;for(0;({
char*_tmpAF=(char*)({struct _dyneither_ptr _tmpAE=Cyc_strchr(start,*((const char*)_check_dyneither_subscript(needle,sizeof(char),0U)));start=_tmpAE;}).curr;_tmpAF != (char*)(_tag_dyneither(0,0,0)).curr;});({
struct _dyneither_ptr _tmpB1=({struct _dyneither_ptr _tmpB0=_dyneither_ptr_plus(start,sizeof(char),1);Cyc_strchr(_tmpB0,*((const char*)_check_dyneither_subscript(needle,sizeof(char),0U)));});start=_tmpB1;})){
if(Cyc_strncmp((struct _dyneither_ptr)start,(struct _dyneither_ptr)needle,(unsigned long)len)== 0)
return start;}}
# 555
return _tag_dyneither(0,0,0);};}
# 558
struct _dyneither_ptr Cyc_mstrstr(struct _dyneither_ptr haystack,struct _dyneither_ptr needle){
if(!((unsigned int)haystack.curr) || !((unsigned int)needle.curr))(int)_throw((void*)({struct Cyc_Core_Invalid_argument_exn_struct*_tmp53=_cycalloc(sizeof(*_tmp53));_tmp53->tag=Cyc_Core_Invalid_argument,({struct _dyneither_ptr _tmpB2=({const char*_tmp52="mstrstr";_tag_dyneither(_tmp52,sizeof(char),8U);});_tmp53->f1=_tmpB2;});_tmp53;}));
if(*((const char*)_check_dyneither_subscript(needle,sizeof(char),0U))== '\000')return haystack;{
# 562
int len=(int)Cyc_strlen((struct _dyneither_ptr)needle);
{struct _dyneither_ptr start=haystack;for(0;({
char*_tmpB4=(char*)({struct _dyneither_ptr _tmpB3=Cyc_mstrchr(start,*((const char*)_check_dyneither_subscript(needle,sizeof(char),0U)));start=_tmpB3;}).curr;_tmpB4 != (char*)(_tag_dyneither(0,0,0)).curr;});({
struct _dyneither_ptr _tmpB6=({struct _dyneither_ptr _tmpB5=_dyneither_ptr_plus(start,sizeof(char),1);Cyc_mstrchr(_tmpB5,*((const char*)_check_dyneither_subscript(needle,sizeof(char),0U)));});start=_tmpB6;})){
if(Cyc_strncmp((struct _dyneither_ptr)start,(struct _dyneither_ptr)needle,(unsigned long)len)== 0)
return start;}}
# 569
return _tag_dyneither(0,0,0);};}
# 575
unsigned long Cyc_strspn(struct _dyneither_ptr s,struct _dyneither_ptr accept){
unsigned long len=Cyc_strlen((struct _dyneither_ptr)s);
unsigned int asize=_get_dyneither_size(accept,sizeof(char));
# 579
len <= _get_dyneither_size(s,sizeof(char))?0:({struct _dyneither_ptr _tmpB7=({const char*_tmp54="len <= numelts(s)";_tag_dyneither(_tmp54,sizeof(char),18U);});((int(*)(struct _dyneither_ptr assertion,struct _dyneither_ptr file,unsigned int line))Cyc___assert_fail)(_tmpB7,({const char*_tmp55="string.cyc";_tag_dyneither(_tmp55,sizeof(char),11U);}),579U);});
{unsigned long i=0U;for(0;i < len;++ i){
int j;
for(j=0;j < asize;++ j){
if(({char _tmpB8=*((const char*)_check_dyneither_subscript(s,sizeof(char),(int)i));_tmpB8 == *((const char*)_check_dyneither_subscript(accept,sizeof(char),j));}))
break;}
if(j == asize)
return i;}}
# 589
return len;}
# 595
unsigned long Cyc_strcspn(struct _dyneither_ptr s,struct _dyneither_ptr accept){
unsigned long len=Cyc_strlen((struct _dyneither_ptr)s);
unsigned int asize=_get_dyneither_size(accept,sizeof(char));
# 599
len <= _get_dyneither_size(s,sizeof(char))?0:({struct _dyneither_ptr _tmpB9=({const char*_tmp56="len <= numelts(s)";_tag_dyneither(_tmp56,sizeof(char),18U);});((int(*)(struct _dyneither_ptr assertion,struct _dyneither_ptr file,unsigned int line))Cyc___assert_fail)(_tmpB9,({const char*_tmp57="string.cyc";_tag_dyneither(_tmp57,sizeof(char),11U);}),599U);});
{unsigned long i=0U;for(0;i < len;++ i){
int j;
for(j=0;j < asize;++ j){
if(({char _tmpBA=*((const char*)_check_dyneither_subscript(s,sizeof(char),(int)i));_tmpBA == *((const char*)_check_dyneither_subscript(accept,sizeof(char),j));}))return i;}}}
# 605
return len;}
# 612
struct _dyneither_ptr Cyc_strtok(struct _dyneither_ptr s,struct _dyneither_ptr delim){
# 614
static struct _dyneither_ptr olds={(void*)0,(void*)0,(void*)(0 + 0)};
struct _dyneither_ptr token;
# 617
if(({char*_tmpBB=(char*)s.curr;_tmpBB == (char*)(_tag_dyneither(0,0,0)).curr;})){
if(({char*_tmpBC=(char*)olds.curr;_tmpBC == (char*)(_tag_dyneither(0,0,0)).curr;}))
return _tag_dyneither(0,0,0);
s=olds;}{
# 624
unsigned long inc=Cyc_strspn((struct _dyneither_ptr)s,delim);
if(inc >= _get_dyneither_size(s,sizeof(char)) || *((char*)_check_dyneither_subscript(_dyneither_ptr_plus(s,sizeof(char),(int)inc),sizeof(char),0U))== '\000'){
# 627
({struct _dyneither_ptr _tmpBD=_tag_dyneither(0,0,0);olds=_tmpBD;});
return _tag_dyneither(0,0,0);}else{
# 631
_dyneither_ptr_inplace_plus(& s,sizeof(char),(int)inc);}
# 634
token=s;
({struct _dyneither_ptr _tmpBE=Cyc_mstrpbrk(token,(struct _dyneither_ptr)delim);s=_tmpBE;});
if(({char*_tmpBF=(char*)s.curr;_tmpBF == (char*)(_tag_dyneither(0,0,0)).curr;}))
# 638
({struct _dyneither_ptr _tmpC0=_tag_dyneither(0,0,0);olds=_tmpC0;});else{
# 642
({struct _dyneither_ptr _tmp58=s;char _tmp59=*((char*)_check_dyneither_subscript(_tmp58,sizeof(char),0U));char _tmp5A='\000';if(_get_dyneither_size(_tmp58,sizeof(char))== 1U  && (_tmp59 == '\000'  && _tmp5A != '\000'))_throw_arraybounds();*((char*)_tmp58.curr)=_tmp5A;});
({struct _dyneither_ptr _tmpC1=_dyneither_ptr_plus(s,sizeof(char),1);olds=_tmpC1;});}
# 645
return token;};}
# 649
struct Cyc_List_List*Cyc_rexplode(struct _RegionHandle*r,struct _dyneither_ptr s){
struct Cyc_List_List*result=0;
{int i=(int)(Cyc_strlen(s)- 1);for(0;i >= 0;-- i){
({struct Cyc_List_List*_tmpC2=({struct Cyc_List_List*_tmp5B=_region_malloc(r,sizeof(*_tmp5B));_tmp5B->hd=(void*)((int)*((const char*)_check_dyneither_subscript(s,sizeof(char),i))),_tmp5B->tl=result;_tmp5B;});result=_tmpC2;});}}
return result;}
# 656
struct Cyc_List_List*Cyc_explode(struct _dyneither_ptr s){
return Cyc_rexplode(Cyc_Core_heap_region,s);}
# 660
struct _dyneither_ptr Cyc_implode(struct Cyc_List_List*chars){
struct _dyneither_ptr s=Cyc_Core_new_string((unsigned int)(((int(*)(struct Cyc_List_List*x))Cyc_List_length)(chars)+ 1));
unsigned long i=0U;
while(chars != 0){
({struct _dyneither_ptr _tmp5C=_dyneither_ptr_plus(s,sizeof(char),(int)i ++);char _tmp5D=*((char*)_check_dyneither_subscript(_tmp5C,sizeof(char),0U));char _tmp5E=(char)((int)chars->hd);if(_get_dyneither_size(_tmp5C,sizeof(char))== 1U  && (_tmp5D == '\000'  && _tmp5E != '\000'))_throw_arraybounds();*((char*)_tmp5C.curr)=_tmp5E;});
chars=chars->tl;}
# 667
return s;}
# 671
inline static int Cyc_casecmp(struct _dyneither_ptr s1,unsigned long len1,struct _dyneither_ptr s2,unsigned long len2){
# 674
unsigned long min_length=len1 < len2?len1: len2;
# 676
min_length <= _get_dyneither_size(s1,sizeof(char)) && min_length <= _get_dyneither_size(s2,sizeof(char))?0:({struct _dyneither_ptr _tmpC3=({const char*_tmp5F="min_length <= numelts(s1) && min_length <= numelts(s2)";_tag_dyneither(_tmp5F,sizeof(char),55U);});((int(*)(struct _dyneither_ptr assertion,struct _dyneither_ptr file,unsigned int line))Cyc___assert_fail)(_tmpC3,({const char*_tmp60="string.cyc";_tag_dyneither(_tmp60,sizeof(char),11U);}),676U);});{
# 678
int i=-1;
while((++ i,i < min_length)){
int diff=({int _tmpC4= toupper((int)*((const char*)_check_dyneither_subscript(s1,sizeof(char),i)));_tmpC4 -  toupper((int)*((const char*)_check_dyneither_subscript(s2,sizeof(char),i)));});
if(diff != 0)
return diff;}
# 684
return(int)len1 - (int)len2;};}
# 687
int Cyc_strcasecmp(struct _dyneither_ptr s1,struct _dyneither_ptr s2){
if((char*)s1.curr == (char*)s2.curr)
return 0;{
unsigned long len1=Cyc_strlen(s1);
unsigned long len2=Cyc_strlen(s2);
return Cyc_casecmp(s1,len1,s2,len2);};}
# 695
inline static int Cyc_caseless_ncmp(struct _dyneither_ptr s1,unsigned long len1,struct _dyneither_ptr s2,unsigned long len2,unsigned long n){
# 698
if(n <= 0)return 0;{
# 700
unsigned long min_len=len1 > len2?len2: len1;
unsigned long bound=min_len > n?n: min_len;
# 703
bound <= _get_dyneither_size(s1,sizeof(char)) && bound <= _get_dyneither_size(s2,sizeof(char))?0:({struct _dyneither_ptr _tmpC5=({const char*_tmp61="bound <= numelts(s1) && bound <= numelts(s2)";_tag_dyneither(_tmp61,sizeof(char),45U);});((int(*)(struct _dyneither_ptr assertion,struct _dyneither_ptr file,unsigned int line))Cyc___assert_fail)(_tmpC5,({const char*_tmp62="string.cyc";_tag_dyneither(_tmp62,sizeof(char),11U);}),703U);});
# 705
{int i=0;for(0;i < bound;++ i){
int retc;
if(({int _tmpC7=({int _tmpC6= toupper((int)*((const char*)_check_dyneither_subscript(s1,sizeof(char),i)));_tmpC6 -  toupper((int)*((const char*)_check_dyneither_subscript(s2,sizeof(char),i)));});retc=_tmpC7;})!= 0)
return retc;}}
# 710
if(len1 < n  || len2 < n)
return(int)len1 - (int)len2;
return 0;};}
# 716
int Cyc_strncasecmp(struct _dyneither_ptr s1,struct _dyneither_ptr s2,unsigned long n){
unsigned long len1=Cyc_strlen(s1);
unsigned long len2=Cyc_strlen(s2);
return Cyc_caseless_ncmp(s1,len1,s2,len2,n);}
# 725
void* memcpy(void*,const void*,unsigned long n);
void* memmove(void*,const void*,unsigned long n);
int memcmp(const void*,const void*,unsigned long n);
char* memchr(const char*,char c,unsigned long n);
void* memset(void*,int c,unsigned long n);
void bcopy(const void*src,void*dest,unsigned long n);
void bzero(void*s,unsigned long n);
char* GC_realloc(char*,unsigned int n);
# 737
struct _dyneither_ptr Cyc_realloc(struct _dyneither_ptr s,unsigned long n){
unsigned long _tmp63=n;unsigned int _tmp65;_LL1: _tmp65=_tmp63;_LL2:;{
char*_tmp64= GC_realloc((char*)_untag_dyneither_ptr(s,sizeof(char),1U),_tmp65);
return((struct _dyneither_ptr(*)(char*arr,unsigned int s,unsigned int n))Cyc_Core_mkfat)((char*)_check_null(_tmp64),sizeof(char),_tmp65);};}
# 743
struct _dyneither_ptr Cyc__memcpy(struct _dyneither_ptr d,struct _dyneither_ptr s,unsigned long n,unsigned int sz){
if(((({void*_tmpC9=(void*)d.curr;_tmpC9 == (void*)(_tag_dyneither(0,0,0)).curr;}) || _get_dyneither_size(d,sizeof(void))< n) || ({void*_tmpC8=(void*)s.curr;_tmpC8 == (void*)(_tag_dyneither(0,0,0)).curr;})) || _get_dyneither_size(s,sizeof(void))< n)
(int)_throw((void*)({struct Cyc_Core_Invalid_argument_exn_struct*_tmp67=_cycalloc(sizeof(*_tmp67));_tmp67->tag=Cyc_Core_Invalid_argument,({struct _dyneither_ptr _tmpCA=({const char*_tmp66="memcpy";_tag_dyneither(_tmp66,sizeof(char),7U);});_tmp67->f1=_tmpCA;});_tmp67;}));
({void*_tmpCC=(void*)_untag_dyneither_ptr(d,sizeof(void),1U);const void*_tmpCB=(const void*)_untag_dyneither_ptr(s,sizeof(void),1U); memcpy(_tmpCC,_tmpCB,n * sz);});
return d;}
# 750
struct _dyneither_ptr Cyc__memmove(struct _dyneither_ptr d,struct _dyneither_ptr s,unsigned long n,unsigned int sz){
if(((({void*_tmpCE=(void*)d.curr;_tmpCE == (void*)(_tag_dyneither(0,0,0)).curr;}) || _get_dyneither_size(d,sizeof(void))< n) || ({void*_tmpCD=(void*)s.curr;_tmpCD == (void*)(_tag_dyneither(0,0,0)).curr;})) || _get_dyneither_size(s,sizeof(void))< n)
(int)_throw((void*)({struct Cyc_Core_Invalid_argument_exn_struct*_tmp69=_cycalloc(sizeof(*_tmp69));_tmp69->tag=Cyc_Core_Invalid_argument,({struct _dyneither_ptr _tmpCF=({const char*_tmp68="memove";_tag_dyneither(_tmp68,sizeof(char),7U);});_tmp69->f1=_tmpCF;});_tmp69;}));
({void*_tmpD1=(void*)_untag_dyneither_ptr(d,sizeof(void),1U);const void*_tmpD0=(const void*)_untag_dyneither_ptr(s,sizeof(void),1U); memmove(_tmpD1,_tmpD0,n * sz);});
return d;}
# 757
int Cyc_memcmp(struct _dyneither_ptr s1,struct _dyneither_ptr s2,unsigned long n){
if(((({char*_tmpD3=(char*)s1.curr;_tmpD3 == (char*)(_tag_dyneither(0,0,0)).curr;}) || ({char*_tmpD2=(char*)s2.curr;_tmpD2 == (char*)(_tag_dyneither(0,0,0)).curr;})) || _get_dyneither_size(s1,sizeof(char))< n) || _get_dyneither_size(s2,sizeof(char))< n)
(int)_throw((void*)({struct Cyc_Core_Invalid_argument_exn_struct*_tmp6B=_cycalloc(sizeof(*_tmp6B));_tmp6B->tag=Cyc_Core_Invalid_argument,({struct _dyneither_ptr _tmpD4=({const char*_tmp6A="memcmp";_tag_dyneither(_tmp6A,sizeof(char),7U);});_tmp6B->f1=_tmpD4;});_tmp6B;}));
return({const void*_tmpD6=(const void*)_untag_dyneither_ptr(s1,sizeof(char),1U);const void*_tmpD5=(const void*)_untag_dyneither_ptr(s2,sizeof(char),1U); memcmp(_tmpD6,_tmpD5,n);});}
# 763
struct _dyneither_ptr Cyc_memchr(struct _dyneither_ptr s,char c,unsigned long n){
unsigned int sz=_get_dyneither_size(s,sizeof(char));
if(({char*_tmpD7=(char*)s.curr;_tmpD7 == (char*)(_tag_dyneither(0,0,0)).curr;}) || n > sz)
(int)_throw((void*)({struct Cyc_Core_Invalid_argument_exn_struct*_tmp6D=_cycalloc(sizeof(*_tmp6D));_tmp6D->tag=Cyc_Core_Invalid_argument,({struct _dyneither_ptr _tmpD8=({const char*_tmp6C="memchr";_tag_dyneither(_tmp6C,sizeof(char),7U);});_tmp6D->f1=_tmpD8;});_tmp6D;}));{
char*_tmp6E= memchr((const char*)_untag_dyneither_ptr(s,sizeof(char),1U),c,n);
if(_tmp6E == 0)return _tag_dyneither(0,0,0);{
unsigned int _tmp6F=(unsigned int)((const char*)_untag_dyneither_ptr(s,sizeof(char),1U));
unsigned int _tmp70=(unsigned int)((const char*)_tmp6E);
unsigned int _tmp71=_tmp70 - _tmp6F;
return _dyneither_ptr_plus(s,sizeof(char),(int)_tmp71);};};}
# 775
struct _dyneither_ptr Cyc_mmemchr(struct _dyneither_ptr s,char c,unsigned long n){
unsigned int sz=_get_dyneither_size(s,sizeof(char));
if(({char*_tmpD9=(char*)s.curr;_tmpD9 == (char*)(_tag_dyneither(0,0,0)).curr;}) || n > sz)
(int)_throw((void*)({struct Cyc_Core_Invalid_argument_exn_struct*_tmp73=_cycalloc(sizeof(*_tmp73));_tmp73->tag=Cyc_Core_Invalid_argument,({struct _dyneither_ptr _tmpDA=({const char*_tmp72="mmemchr";_tag_dyneither(_tmp72,sizeof(char),8U);});_tmp73->f1=_tmpDA;});_tmp73;}));{
char*_tmp74= memchr((const char*)_untag_dyneither_ptr(s,sizeof(char),1U),c,n);
if(_tmp74 == 0)return _tag_dyneither(0,0,0);{
unsigned int _tmp75=(unsigned int)((const char*)_untag_dyneither_ptr(s,sizeof(char),1U));
unsigned int _tmp76=(unsigned int)_tmp74;
unsigned int _tmp77=_tmp76 - _tmp75;
return _dyneither_ptr_plus(s,sizeof(char),(int)_tmp77);};};}
# 787
struct _dyneither_ptr Cyc_memset(struct _dyneither_ptr s,char c,unsigned long n){
if(({char*_tmpDB=(char*)s.curr;_tmpDB == (char*)(_tag_dyneither(0,0,0)).curr;}) || n > _get_dyneither_size(s,sizeof(char)))
(int)_throw((void*)({struct Cyc_Core_Invalid_argument_exn_struct*_tmp79=_cycalloc(sizeof(*_tmp79));_tmp79->tag=Cyc_Core_Invalid_argument,({struct _dyneither_ptr _tmpDC=({const char*_tmp78="memset";_tag_dyneither(_tmp78,sizeof(char),7U);});_tmp79->f1=_tmpDC;});_tmp79;}));
 memset((void*)((char*)_untag_dyneither_ptr(s,sizeof(char),1U)),(int)c,n);
return s;}
# 794
void Cyc_bzero(struct _dyneither_ptr s,unsigned long n){
if(({char*_tmpDD=(char*)s.curr;_tmpDD == (char*)(_tag_dyneither(0,0,0)).curr;}) || _get_dyneither_size(s,sizeof(char))< n)
(int)_throw((void*)({struct Cyc_Core_Invalid_argument_exn_struct*_tmp7B=_cycalloc(sizeof(*_tmp7B));_tmp7B->tag=Cyc_Core_Invalid_argument,({struct _dyneither_ptr _tmpDE=({const char*_tmp7A="bzero";_tag_dyneither(_tmp7A,sizeof(char),6U);});_tmp7B->f1=_tmpDE;});_tmp7B;}));
((void(*)(char*s,unsigned long n)) bzero)((char*)_untag_dyneither_ptr(s,sizeof(char),1U),n);}
# 800
void Cyc__bcopy(struct _dyneither_ptr src,struct _dyneither_ptr dst,unsigned long n,unsigned int sz){
if(((({void*_tmpE0=(void*)src.curr;_tmpE0 == (void*)(_tag_dyneither(0,0,0)).curr;}) || _get_dyneither_size(src,sizeof(void))< n) || ({void*_tmpDF=(void*)dst.curr;_tmpDF == (void*)(_tag_dyneither(0,0,0)).curr;})) || _get_dyneither_size(dst,sizeof(void))< n)
(int)_throw((void*)({struct Cyc_Core_Invalid_argument_exn_struct*_tmp7D=_cycalloc(sizeof(*_tmp7D));_tmp7D->tag=Cyc_Core_Invalid_argument,({struct _dyneither_ptr _tmpE1=({const char*_tmp7C="bcopy";_tag_dyneither(_tmp7C,sizeof(char),6U);});_tmp7D->f1=_tmpE1;});_tmp7D;}));
({const void*_tmpE3=(const void*)_untag_dyneither_ptr(src,sizeof(void),1U);void*_tmpE2=(void*)_untag_dyneither_ptr(dst,sizeof(void),1U); bcopy(_tmpE3,_tmpE2,n * sz);});}
