#ifndef _SETJMP_H_
#define _SETJMP_H_
#ifndef _jmp_buf_def_
#define _jmp_buf_def_
typedef int jmp_buf[192];
#endif
extern int setjmp(jmp_buf);
#endif
/* This is a C header file to be used by the output of the Cyclone to
   C translator.  The corresponding definitions are in file
   lib/runtime_cyc.c
*/
#ifndef _CYC_INCLUDE_H_
#define _CYC_INCLUDE_H_

#ifdef NO_CYC_PREFIX
#define ADD_PREFIX(x) x
#else
#define ADD_PREFIX(x) Cyc_##x
#endif

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

/* Need one of these per thread (we don't have threads)
   The runtime maintains a stack that contains either _handler_cons
   structs or _RegionHandle structs.  The tag is 0 for a handler_cons
   and 1 for a region handle.  */
struct _RuntimeStack {
  int tag; /* 0 for an exception handler, 1 for a region handle */
  struct _RuntimeStack *next;
};

/* Regions */
struct _RegionPage {
#ifdef CYC_REGION_PROFILE
  unsigned total_bytes;
  unsigned free_bytes;
#endif
  struct _RegionPage *next;
  char data[1];  /*FJS: used to be size 0, but that's forbidden in ansi c*/
};

struct _RegionHandle {
  struct _RuntimeStack s;
  struct _RegionPage *curr;
  char               *offset;
  char               *last_plus_one;
  struct _DynRegionHandle *sub_regions;
#ifdef CYC_REGION_PROFILE
  const char         *name;
#endif
};

struct _DynRegionFrame {
  struct _RuntimeStack s;
  struct _DynRegionHandle *x;
};

extern struct _RegionHandle _new_region(const char *);
extern void * _region_malloc(struct _RegionHandle *, unsigned);
extern void * _region_calloc(struct _RegionHandle *, unsigned t, unsigned n);
extern void   _free_region(struct _RegionHandle *);
extern void   _reset_region(struct _RegionHandle *);
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
extern int _throw_null();
extern int _throw_arraybounds();
extern int _throw_badalloc();
extern int _throw(void* e);
#endif

extern struct _xtunion_struct *_exn_thrown;

/* Built-in Exceptions */
extern struct _xtunion_struct ADD_PREFIX(Null_Exception_struct);
extern struct _xtunion_struct * ADD_PREFIX(Null_Exception);
extern struct _xtunion_struct ADD_PREFIX(Array_bounds_struct);
extern struct _xtunion_struct * ADD_PREFIX(Array_bounds);
extern struct _xtunion_struct ADD_PREFIX(Match_Exception_struct);
extern struct _xtunion_struct * ADD_PREFIX(Match_Exception);
extern struct _xtunion_struct ADD_PREFIX(Bad_alloc_struct);
extern struct _xtunion_struct * ADD_PREFIX(Bad_alloc);

/* Built-in Run-time Checks and company */
#ifdef __APPLE__
#define _INLINE_FUNCTIONS
#endif

#ifdef CYC_ANSI_OUTPUT
#define _INLINE  
#define _INLINE_FUNCTIONS
#else
#define _INLINE inline
#endif

#ifdef VC_C
#define _CYC_U_LONG_LONG_T __int64
#else
#ifdef GCC_C
#define _CYC_U_LONG_LONG_T unsigned long long
#else
#define _CYC_U_LONG_LONG_T unsigned long long
#endif
#endif

#ifdef NO_CYC_NULL_CHECKS
#define _check_null(ptr) (ptr)
#else
#ifdef _INLINE_FUNCTIONS
static _INLINE void *
_check_null(void *ptr) {
  void*_check_null_temp = (void*)(ptr);
  if (!_check_null_temp) _throw_null();
  return _check_null_temp;
}
#else
#define _check_null(ptr) \
  ({ void*_check_null_temp = (void*)(ptr); \
     if (!_check_null_temp) _throw_null(); \
     _check_null_temp; })
#endif
#endif

#ifdef NO_CYC_BOUNDS_CHECKS
#define _check_known_subscript_null(ptr,bound,elt_sz,index) ({ \
  ((char *)ptr) + (elt_sz)*(index); })
#else
#ifdef _INLINE_FUNCTIONS
static _INLINE char *
_check_known_subscript_null(void *ptr, unsigned bound, unsigned elt_sz, unsigned index) {
  void*_cks_ptr = (void*)(ptr);
  unsigned _cks_bound = (bound);
  unsigned _cks_elt_sz = (elt_sz);
  unsigned _cks_index = (index);
  if (!_cks_ptr) _throw_null();
  if (_cks_index >= _cks_bound) _throw_arraybounds();
  return ((char *)_cks_ptr) + _cks_elt_sz*_cks_index;
}
#else
#define _check_known_subscript_null(ptr,bound,elt_sz,index) ({ \
  void*_cks_ptr = (void*)(ptr); \
  unsigned _cks_bound = (bound); \
  unsigned _cks_elt_sz = (elt_sz); \
  unsigned _cks_index = (index); \
  if (!_cks_ptr) _throw_null(); \
  if (_cks_index >= _cks_bound) _throw_arraybounds(); \
  ((char *)_cks_ptr) + _cks_elt_sz*_cks_index; })
#endif
#endif

#ifdef NO_CYC_BOUNDS_CHECKS
#define _check_known_subscript_notnull(bound,index) (index)
#else
#ifdef _INLINE_FUNCTIONS
static _INLINE unsigned
_check_known_subscript_notnull(unsigned bound,unsigned index) { 
  unsigned _cksnn_bound = (bound); 
  unsigned _cksnn_index = (index); 
  if (_cksnn_index >= _cksnn_bound) _throw_arraybounds(); 
  return _cksnn_index;
}
#else
#define _check_known_subscript_notnull(bound,index) ({ \
  unsigned _cksnn_bound = (bound); \
  unsigned _cksnn_index = (index); \
  if (_cksnn_index >= _cksnn_bound) _throw_arraybounds(); \
  _cksnn_index; })
#endif
#endif

/* Add i to zero-terminated pointer x.  Checks for x being null and
   ensures that x[0..i-1] are not 0. */
#ifdef NO_CYC_BOUNDS_CHECK
#define _zero_arr_plus_char(orig_x,orig_sz,orig_i) ((orig_x)+(orig_i))
#define _zero_arr_plus_short(orig_x,orig_sz,orig_i) ((orig_x)+(orig_i))
#define _zero_arr_plus_int(orig_x,orig_sz,orig_i) ((orig_x)+(orig_i))
#define _zero_arr_plus_float(orig_x,orig_sz,orig_i) ((orig_x)+(orig_i))
#define _zero_arr_plus_double(orig_x,orig_sz,orig_i) ((orig_x)+(orig_i))
#define _zero_arr_plus_longdouble(orig_x,orig_sz,orig_i) ((orig_x)+(orig_i))
#define _zero_arr_plus_voidstar(orig_x,orig_sz,orig_i) ((orig_x)+(orig_i))
#else
static _INLINE char *
_zero_arr_plus_char(char *orig_x, int orig_sz, int orig_i) {
  unsigned int _czs_temp;
  if ((orig_x) == 0) _throw_null();
  if (orig_i < 0) _throw_arraybounds();
  for (_czs_temp=orig_sz; _czs_temp < orig_i; _czs_temp++)
    if (orig_x[_czs_temp] == 0) _throw_arraybounds();
  return orig_x + orig_i;
}
static _INLINE short *
_zero_arr_plus_short(short *orig_x, int orig_sz, int orig_i) {
  unsigned int _czs_temp;
  if ((orig_x) == 0) _throw_null();
  if (orig_i < 0) _throw_arraybounds();
  for (_czs_temp=orig_sz; _czs_temp < orig_i; _czs_temp++)
    if (orig_x[_czs_temp] == 0) _throw_arraybounds();
  return orig_x + orig_i;
}
static _INLINE int *
_zero_arr_plus_int(int *orig_x, int orig_sz, int orig_i) {
  unsigned int _czs_temp;
  if ((orig_x) == 0) _throw_null();
  if (orig_i < 0) _throw_arraybounds();
  for (_czs_temp=orig_sz; _czs_temp < orig_i; _czs_temp++)
    if (orig_x[_czs_temp] == 0) _throw_arraybounds();
  return orig_x + orig_i;
}
static _INLINE float *
_zero_arr_plus_float(float *orig_x, int orig_sz, int orig_i) {
  unsigned int _czs_temp;
  if ((orig_x) == 0) _throw_null();
  if (orig_i < 0) _throw_arraybounds();
  for (_czs_temp=orig_sz; _czs_temp < orig_i; _czs_temp++)
    if (orig_x[_czs_temp] == 0) _throw_arraybounds();
  return orig_x + orig_i;
}
static _INLINE double *
_zero_arr_plus_double(double *orig_x, int orig_sz, int orig_i) {
  unsigned int _czs_temp;
  if ((orig_x) == 0) _throw_null();
  if (orig_i < 0) _throw_arraybounds();
  for (_czs_temp=orig_sz; _czs_temp < orig_i; _czs_temp++)
    if (orig_x[_czs_temp] == 0) _throw_arraybounds();
  return orig_x + orig_i;
}
static _INLINE long double *
_zero_arr_plus_longdouble(long double *orig_x, int orig_sz, int orig_i) {
  unsigned int _czs_temp;
  if ((orig_x) == 0) _throw_null();
  if (orig_i < 0) _throw_arraybounds();
  for (_czs_temp=orig_sz; _czs_temp < orig_i; _czs_temp++)
    if (orig_x[_czs_temp] == 0) _throw_arraybounds();
  return orig_x + orig_i;
}
static _INLINE void *
_zero_arr_plus_voidstar(void **orig_x, int orig_sz, int orig_i) {
  unsigned int _czs_temp;
  if ((orig_x) == 0) _throw_null();
  if (orig_i < 0) _throw_arraybounds();
  for (_czs_temp=orig_sz; _czs_temp < orig_i; _czs_temp++)
    if (orig_x[_czs_temp] == 0) _throw_arraybounds();
  return orig_x + orig_i;
}
#endif


/* Calculates the number of elements in a zero-terminated, thin array.
   If non-null, the array is guaranteed to have orig_offset elements. */
static _INLINE int
_get_zero_arr_size_char(const char *orig_x, unsigned int orig_offset) {
  const char *_gres_x = orig_x;
  unsigned int _gres = 0;
  if (_gres_x != 0) {
     _gres = orig_offset;
     _gres_x += orig_offset - 1;
     while (*_gres_x != 0) { _gres_x++; _gres++; }
  }
  return _gres; 
}
static _INLINE int
_get_zero_arr_size_short(const short *orig_x, unsigned int orig_offset) {
  const short *_gres_x = orig_x;
  unsigned int _gres = 0;
  if (_gres_x != 0) {
     _gres = orig_offset;
     _gres_x += orig_offset - 1;
     while (*_gres_x != 0) { _gres_x++; _gres++; }
  }
  return _gres; 
}
static _INLINE int
_get_zero_arr_size_int(const int *orig_x, unsigned int orig_offset) {
  const int *_gres_x = orig_x;
  unsigned int _gres = 0;
  if (_gres_x != 0) {
     _gres = orig_offset;
     _gres_x += orig_offset - 1;
     while (*_gres_x != 0) { _gres_x++; _gres++; }
  }
  return _gres; 
}
static _INLINE int
_get_zero_arr_size_float(const float *orig_x, unsigned int orig_offset) {
  const float *_gres_x = orig_x;
  unsigned int _gres = 0;
  if (_gres_x != 0) {
     _gres = orig_offset;
     _gres_x += orig_offset - 1;
     while (*_gres_x != 0) { _gres_x++; _gres++; }
  }
  return _gres; 
}
static _INLINE int
_get_zero_arr_size_double(const double *orig_x, unsigned int orig_offset) {
  const double *_gres_x = orig_x;
  unsigned int _gres = 0;
  if (_gres_x != 0) {
     _gres = orig_offset;
     _gres_x += orig_offset - 1;
     while (*_gres_x != 0) { _gres_x++; _gres++; }
  }
  return _gres; 
}
static _INLINE int
_get_zero_arr_size_longdouble(const long double *orig_x, unsigned int orig_offset) {
  const long double *_gres_x = orig_x;
  unsigned int _gres = 0;
  if (_gres_x != 0) {
     _gres = orig_offset;
     _gres_x += orig_offset - 1;
     while (*_gres_x != 0) { _gres_x++; _gres++; }
  }
  return _gres; 
}
static _INLINE int
_get_zero_arr_size_voidstar(const void **orig_x, unsigned int orig_offset) {
  const void **_gres_x = orig_x;
  unsigned int _gres = 0;
  if (_gres_x != 0) {
     _gres = orig_offset;
     _gres_x += orig_offset - 1;
     while (*_gres_x != 0) { _gres_x++; _gres++; }
  }
  return _gres; 
}


/* Does in-place addition of a zero-terminated pointer (x += e and ++x).  
   Note that this expands to call _zero_arr_plus. */
/*#define _zero_arr_inplace_plus(x,orig_i) ({ \
  typedef _zap_tx = (*x); \
  _zap_tx **_zap_x = &((_zap_tx*)x); \
  *_zap_x = _zero_arr_plus(*_zap_x,1,(orig_i)); })
  */
static _INLINE void 
_zero_arr_inplace_plus_char(char *x, int orig_i) {
  char **_zap_x = &x;
  *_zap_x = _zero_arr_plus_char(*_zap_x,1,orig_i);
}
static _INLINE void 
_zero_arr_inplace_plus_short(short *x, int orig_i) {
  short **_zap_x = &x;
  *_zap_x = _zero_arr_plus_short(*_zap_x,1,orig_i);
}
static _INLINE void 
_zero_arr_inplace_plus_int(int *x, int orig_i) {
  int **_zap_x = &x;
  *_zap_x = _zero_arr_plus_int(*_zap_x,1,orig_i);
}
static _INLINE void 
_zero_arr_inplace_plus_float(float *x, int orig_i) {
  float **_zap_x = &x;
  *_zap_x = _zero_arr_plus_float(*_zap_x,1,orig_i);
}
static _INLINE void 
_zero_arr_inplace_plus_double(double *x, int orig_i) {
  double **_zap_x = &x;
  *_zap_x = _zero_arr_plus_double(*_zap_x,1,orig_i);
}
static _INLINE void 
_zero_arr_inplace_plus_longdouble(long double *x, int orig_i) {
  long double **_zap_x = &x;
  *_zap_x = _zero_arr_plus_longdouble(*_zap_x,1,orig_i);
}
static _INLINE void 
_zero_arr_inplace_plus_voidstar(void **x, int orig_i) {
  void ***_zap_x = &x;
  *_zap_x = _zero_arr_plus_voidstar(*_zap_x,1,orig_i);
}




/* Does in-place increment of a zero-terminated pointer (e.g., x++).
   Note that this expands to call _zero_arr_plus. */
/*#define _zero_arr_inplace_plus_post(x,orig_i) ({ \
  typedef _zap_tx = (*x); \
  _zap_tx **_zap_x = &((_zap_tx*)x); \
  _zap_tx *_zap_res = *_zap_x; \
  *_zap_x = _zero_arr_plus(_zap_res,1,(orig_i)); \
  _zap_res; })*/
  
static _INLINE char *
_zero_arr_inplace_plus_post_char(char *x, int orig_i){
  char ** _zap_x = &x;
  char * _zap_res = *_zap_x;
  *_zap_x = _zero_arr_plus_char(_zap_res,1,orig_i);
  return _zap_res;
}
static _INLINE short *
_zero_arr_inplace_plus_post_short(short *x, int orig_i){
  short **_zap_x = &x;
  short * _zap_res = *_zap_x;
  *_zap_x = _zero_arr_plus_short(_zap_res,1,orig_i);
  return _zap_res;
}
static _INLINE int *
_zero_arr_inplace_plus_post_int(int *x, int orig_i){
  int **_zap_x = &x;
  int * _zap_res = *_zap_x;
  *_zap_x = _zero_arr_plus_int(_zap_res,1,orig_i);
  return _zap_res;
}
static _INLINE float *
_zero_arr_inplace_plus_post_float(float *x, int orig_i){
  float **_zap_x = &x;
  float * _zap_res = *_zap_x;
  *_zap_x = _zero_arr_plus_float(_zap_res,1,orig_i);
  return _zap_res;
}
static _INLINE double *
_zero_arr_inplace_plus_post_double(double *x, int orig_i){
  double **_zap_x = &x;
  double * _zap_res = *_zap_x;
  *_zap_x = _zero_arr_plus_double(_zap_res,1,orig_i);
  return _zap_res;
}
static _INLINE long double *
_zero_arr_inplace_plus_post_longdouble(long double *x, int orig_i){
  long double **_zap_x = &x;
  long double * _zap_res = *_zap_x;
  *_zap_x = _zero_arr_plus_longdouble(_zap_res,1,orig_i);
  return _zap_res;
}
static _INLINE void **
_zero_arr_inplace_plus_post_voidstar(void **x, int orig_i){
  void ***_zap_x = &x;
  void ** _zap_res = *_zap_x;
  *_zap_x = _zero_arr_plus_voidstar(_zap_res,1,orig_i);
  return _zap_res;
}



/* functions for dealing with dynamically sized pointers */
#ifdef NO_CYC_BOUNDS_CHECKS
#ifdef _INLINE_FUNCTIONS
static _INLINE unsigned char *
_check_dyneither_subscript(struct _dyneither_ptr arr,unsigned elt_sz,unsigned index) {
  struct _dyneither_ptr _cus_arr = (arr);
  unsigned _cus_elt_sz = (elt_sz);
  unsigned _cus_index = (index);
  unsigned char *_cus_ans = _cus_arr.curr + _cus_elt_sz * _cus_index;
  return _cus_ans;
}
#else
#define _check_dyneither_subscript(arr,elt_sz,index) ({ \
  struct _dyneither_ptr _cus_arr = (arr); \
  unsigned _cus_elt_sz = (elt_sz); \
  unsigned _cus_index = (index); \
  unsigned char *_cus_ans = _cus_arr.curr + _cus_elt_sz * _cus_index; \
  _cus_ans; })
#endif
#else
#ifdef _INLINE_FUNCTIONS
static _INLINE unsigned char *
_check_dyneither_subscript(struct _dyneither_ptr arr,unsigned elt_sz,unsigned index) {
  struct _dyneither_ptr _cus_arr = (arr);
  unsigned _cus_elt_sz = (elt_sz);
  unsigned _cus_index = (index);
  unsigned char *_cus_ans = _cus_arr.curr + _cus_elt_sz * _cus_index;
  if (!_cus_arr.base) _throw_null();
  if (_cus_ans < _cus_arr.base || _cus_ans >= _cus_arr.last_plus_one)
    _throw_arraybounds();
  return _cus_ans;
}
#else
#define _check_dyneither_subscript(arr,elt_sz,index) ({ \
  struct _dyneither_ptr _cus_arr = (arr); \
  unsigned _cus_elt_sz = (elt_sz); \
  unsigned _cus_index = (index); \
  unsigned char *_cus_ans = _cus_arr.curr + _cus_elt_sz * _cus_index; \
  if (!_cus_arr.base) _throw_null(); \
  if (_cus_ans < _cus_arr.base || _cus_ans >= _cus_arr.last_plus_one) \
    _throw_arraybounds(); \
  _cus_ans; })
#endif
#endif

#ifdef _INLINE_FUNCTIONS
static _INLINE struct _dyneither_ptr
_tag_dyneither(const void *tcurr,unsigned elt_sz,unsigned num_elts) {
  struct _dyneither_ptr _tag_arr_ans;
  _tag_arr_ans.base = _tag_arr_ans.curr = (void*)(tcurr);
  _tag_arr_ans.last_plus_one = _tag_arr_ans.base + (elt_sz) * (num_elts);
  return _tag_arr_ans;
}
#else
#define _tag_dyneither(tcurr,elt_sz,num_elts) ({ \
  struct _dyneither_ptr _tag_arr_ans; \
  _tag_arr_ans.base = _tag_arr_ans.curr = (void*)(tcurr); \
  _tag_arr_ans.last_plus_one = _tag_arr_ans.base + (elt_sz) * (num_elts); \
  _tag_arr_ans; })
#endif

#ifdef _INLINE_FUNCTIONS
static _INLINE struct _dyneither_ptr *
_init_dyneither_ptr(struct _dyneither_ptr *arr_ptr,
                    void *arr, unsigned elt_sz, unsigned num_elts) {
  struct _dyneither_ptr *_itarr_ptr = (arr_ptr);
  void* _itarr = (arr);
  _itarr_ptr->base = _itarr_ptr->curr = _itarr;
  _itarr_ptr->last_plus_one = ((unsigned char *)_itarr) + (elt_sz) * (num_elts);
  return _itarr_ptr;
}
#else
#define _init_dyneither_ptr(arr_ptr,arr,elt_sz,num_elts) ({ \
  struct _dyneither_ptr *_itarr_ptr = (arr_ptr); \
  void* _itarr = (arr); \
  _itarr_ptr->base = _itarr_ptr->curr = _itarr; \
  _itarr_ptr->last_plus_one = ((char *)_itarr) + (elt_sz) * (num_elts); \
  _itarr_ptr; })
#endif

#ifdef NO_CYC_BOUNDS_CHECKS
#define _untag_dyneither_ptr(arr,elt_sz,num_elts) ((arr).curr)
#else
#ifdef _INLINE_FUNCTIONS
static _INLINE unsigned char *
_untag_dyneither_ptr(struct _dyneither_ptr arr, 
                     unsigned elt_sz,unsigned num_elts) {
  struct _dyneither_ptr _arr = (arr);
  unsigned char *_curr = _arr.curr;
  if (_curr < _arr.base || _curr + (elt_sz) * (num_elts) > _arr.last_plus_one)
    _throw_arraybounds();
  return _curr;
}
#else
#define _untag_dyneither_ptr(arr,elt_sz,num_elts) ({ \
  struct _dyneither_ptr _arr = (arr); \
  unsigned char *_curr = _arr.curr; \
  if (_curr < _arr.base || _curr + (elt_sz) * (num_elts) > _arr.last_plus_one)\
    _throw_arraybounds(); \
  _curr; })
#endif
#endif

#ifdef _INLINE_FUNCTIONS
static _INLINE unsigned
_get_dyneither_size(struct _dyneither_ptr arr,unsigned elt_sz) {
  struct _dyneither_ptr _get_arr_size_temp = (arr);
  unsigned char *_get_arr_size_curr=_get_arr_size_temp.curr;
  unsigned char *_get_arr_size_last=_get_arr_size_temp.last_plus_one;
  return (_get_arr_size_curr < _get_arr_size_temp.base ||
          _get_arr_size_curr >= _get_arr_size_last) ? 0 :
    ((_get_arr_size_last - _get_arr_size_curr) / (elt_sz));
}
#else
#define _get_dyneither_size(arr,elt_sz) \
  ({struct _dyneither_ptr _get_arr_size_temp = (arr); \
    unsigned char *_get_arr_size_curr=_get_arr_size_temp.curr; \
    unsigned char *_get_arr_size_last=_get_arr_size_temp.last_plus_one; \
    (_get_arr_size_curr < _get_arr_size_temp.base || \
     _get_arr_size_curr >= _get_arr_size_last) ? 0 : \
    ((_get_arr_size_last - _get_arr_size_curr) / (elt_sz));})
#endif

#ifdef _INLINE_FUNCTIONS
static _INLINE struct _dyneither_ptr
_dyneither_ptr_plus(struct _dyneither_ptr arr,unsigned elt_sz,int change) {
  struct _dyneither_ptr _ans = (arr);
  _ans.curr += ((int)(elt_sz))*(change);
  return _ans;
}
#else
#define _dyneither_ptr_plus(arr,elt_sz,change) ({ \
  struct _dyneither_ptr _ans = (arr); \
  _ans.curr += ((int)(elt_sz))*(change); \
  _ans; })
#endif

#ifdef _INLINE_FUNCTIONS
static _INLINE struct _dyneither_ptr
_dyneither_ptr_inplace_plus(struct _dyneither_ptr *arr_ptr,unsigned elt_sz,
                            int change) {
  struct _dyneither_ptr * _arr_ptr = (arr_ptr);
  _arr_ptr->curr += ((int)(elt_sz))*(change);
  return *_arr_ptr;
}
#else
#define _dyneither_ptr_inplace_plus(arr_ptr,elt_sz,change) ({ \
  struct _dyneither_ptr * _arr_ptr = (arr_ptr); \
  _arr_ptr->curr += ((int)(elt_sz))*(change); \
  *_arr_ptr; })
#endif

#ifdef _INLINE_FUNCTIONS
static _INLINE struct _dyneither_ptr
_dyneither_ptr_inplace_plus_post(struct _dyneither_ptr *arr_ptr,unsigned elt_sz,int change) {
  struct _dyneither_ptr * _arr_ptr = (arr_ptr);
  struct _dyneither_ptr _ans = *_arr_ptr;
  _arr_ptr->curr += ((int)(elt_sz))*(change);
  return _ans;
}
#else
#define _dyneither_ptr_inplace_plus_post(arr_ptr,elt_sz,change) ({ \
  struct _dyneither_ptr * _arr_ptr = (arr_ptr); \
  struct _dyneither_ptr _ans = *_arr_ptr; \
  _arr_ptr->curr += ((int)(elt_sz))*(change); \
  _ans; })
#endif

/* Decrease the upper bound on a fat pointer by numelts where sz is
   the size of the pointer's type.  Note that this can't be a macro
   if we're to get initializers right. */
static struct 
_dyneither_ptr _dyneither_ptr_decrease_size(struct _dyneither_ptr x,
                                            unsigned int sz,
                                            unsigned int numelts) {
  x.last_plus_one -= sz * numelts; 
  return x; 
}

/* Allocation */
extern void* GC_malloc(int);
extern void* GC_malloc_atomic(int);
extern void* GC_calloc(unsigned,unsigned);
extern void* GC_calloc_atomic(unsigned,unsigned);

static _INLINE void* _cycalloc(int n) {
  void * ans = (void *)GC_malloc(n);
  if(!ans)
    _throw_badalloc();
  return ans;
}
static _INLINE void* _cycalloc_atomic(int n) {
  void * ans = (void *)GC_malloc_atomic(n);
  if(!ans)
    _throw_badalloc();
  return ans;
}
static _INLINE void* _cyccalloc(unsigned n, unsigned s) {
  void* ans = (void*)GC_calloc(n,s);
  if (!ans)
    _throw_badalloc();
  return ans;
}
static _INLINE void* _cyccalloc_atomic(unsigned n, unsigned s) {
  void* ans = (void*)GC_calloc_atomic(n,s);
  if (!ans)
    _throw_badalloc();
  return ans;
}
#define MAX_MALLOC_SIZE (1 << 28)
static _INLINE unsigned int _check_times(unsigned x, unsigned y) {
  _CYC_U_LONG_LONG_T whole_ans = 
    ((_CYC_U_LONG_LONG_T)x)*((_CYC_U_LONG_LONG_T)y);
  unsigned word_ans = (unsigned)whole_ans;
  if(word_ans < whole_ans || word_ans > MAX_MALLOC_SIZE)
    _throw_badalloc();
  return word_ans;
}

#if defined(CYC_REGION_PROFILE) 
extern void* _profile_GC_malloc(int,char *file,int lineno);
extern void* _profile_GC_malloc_atomic(int,char *file,int lineno);
extern void* _profile_region_malloc(struct _RegionHandle *, unsigned,
                                     char *file,int lineno);
extern struct _RegionHandle _profile_new_region(const char *rgn_name,
						char *file,int lineno);
extern void _profile_free_region(struct _RegionHandle *,
				 char *file,int lineno);
#  if !defined(RUNTIME_CYC)
#define _new_region(n) _profile_new_region(n,__FILE__ ":" __FUNCTION__,__LINE__)
#define _free_region(r) _profile_free_region(r,__FILE__ ":" __FUNCTION__,__LINE__)
#define _region_malloc(rh,n) _profile_region_malloc(rh,n,__FILE__ ":" __FUNCTION__,__LINE__)
#  endif
#define _cycalloc(n) _profile_GC_malloc(n,__FILE__ ":" __FUNCTION__,__LINE__)
#define _cycalloc_atomic(n) _profile_GC_malloc_atomic(n,__FILE__ ":" __FUNCTION__,__LINE__)
#endif
#endif

/* the next two routines swap [x] and [y]; not thread safe! */
static _INLINE void _swap_word(void *x, void *y) {
  unsigned long *lx = (unsigned long *)x, *ly = (unsigned long *)y, tmp;
  tmp = *lx;
  *lx = *ly;
  *ly = tmp;
}
static _INLINE void _swap_dyneither(struct _dyneither_ptr *x, 
				   struct _dyneither_ptr *y) {
  struct _dyneither_ptr tmp = *x;
  *x = *y;
  *y = tmp;
}
 void exit(int);void*abort();struct Cyc_Core_NewRegion{struct _DynRegionHandle*
dynregion;};struct Cyc_Core_Opt{void*v;};extern char Cyc_Core_Invalid_argument[21];
struct Cyc_Core_Invalid_argument_struct{char*tag;struct _dyneither_ptr f1;};extern
char Cyc_Core_Failure[12];struct Cyc_Core_Failure_struct{char*tag;struct
_dyneither_ptr f1;};extern char Cyc_Core_Impossible[15];struct Cyc_Core_Impossible_struct{
char*tag;struct _dyneither_ptr f1;};extern char Cyc_Core_Not_found[14];extern char Cyc_Core_Unreachable[
16];struct Cyc_Core_Unreachable_struct{char*tag;struct _dyneither_ptr f1;};extern
struct _RegionHandle*Cyc_Core_heap_region;extern char Cyc_Core_Open_Region[16];
extern char Cyc_Core_Free_Region[16];struct _dyneither_ptr Cstring_to_string(char*);
struct Cyc___cycFILE;extern struct Cyc___cycFILE*Cyc_stdout;extern struct Cyc___cycFILE*
Cyc_stderr;struct Cyc_Cstdio___abstractFILE;struct Cyc_String_pa_struct{int tag;
struct _dyneither_ptr f1;};struct Cyc_Int_pa_struct{int tag;unsigned long f1;};struct
Cyc_Double_pa_struct{int tag;double f1;};struct Cyc_LongDouble_pa_struct{int tag;
long double f1;};struct Cyc_ShortPtr_pa_struct{int tag;short*f1;};struct Cyc_IntPtr_pa_struct{
int tag;unsigned long*f1;};struct _dyneither_ptr Cyc_aprintf(struct _dyneither_ptr,
struct _dyneither_ptr);int Cyc_fclose(struct Cyc___cycFILE*);int Cyc_fflush(struct
Cyc___cycFILE*);struct Cyc___cycFILE*Cyc_fopen(const char*,const char*);int Cyc_fprintf(
struct Cyc___cycFILE*,struct _dyneither_ptr,struct _dyneither_ptr);int Cyc_fputc(int,
struct Cyc___cycFILE*);int Cyc_fputs(const char*,struct Cyc___cycFILE*);struct Cyc_ShortPtr_sa_struct{
int tag;short*f1;};struct Cyc_UShortPtr_sa_struct{int tag;unsigned short*f1;};
struct Cyc_IntPtr_sa_struct{int tag;int*f1;};struct Cyc_UIntPtr_sa_struct{int tag;
unsigned int*f1;};struct Cyc_StringPtr_sa_struct{int tag;struct _dyneither_ptr f1;};
struct Cyc_DoublePtr_sa_struct{int tag;double*f1;};struct Cyc_FloatPtr_sa_struct{
int tag;float*f1;};struct Cyc_CharPtr_sa_struct{int tag;struct _dyneither_ptr f1;};
int remove(const char*);int Cyc_vfprintf(struct Cyc___cycFILE*,struct _dyneither_ptr,
struct _dyneither_ptr);extern char Cyc_FileCloseError[19];extern char Cyc_FileOpenError[
18];struct Cyc_FileOpenError_struct{char*tag;struct _dyneither_ptr f1;};extern char
Cyc_Lexing_Error[10];struct Cyc_Lexing_Error_struct{char*tag;struct _dyneither_ptr
f1;};struct Cyc_Lexing_lexbuf{void(*refill_buff)(struct Cyc_Lexing_lexbuf*);void*
refill_state;struct _dyneither_ptr lex_buffer;int lex_buffer_len;int lex_abs_pos;int
lex_start_pos;int lex_curr_pos;int lex_last_pos;int lex_last_action;int
lex_eof_reached;};struct Cyc_Lexing_function_lexbuf_state{int(*read_fun)(struct
_dyneither_ptr,int,void*);void*read_fun_state;};struct Cyc_Lexing_lex_tables{
struct _dyneither_ptr lex_base;struct _dyneither_ptr lex_backtrk;struct
_dyneither_ptr lex_default;struct _dyneither_ptr lex_trans;struct _dyneither_ptr
lex_check;};struct Cyc_Lexing_lexbuf*Cyc_Lexing_from_file(struct Cyc___cycFILE*);
struct _dyneither_ptr Cyc_Lexing_lexeme(struct Cyc_Lexing_lexbuf*);char Cyc_Lexing_lexeme_char(
struct Cyc_Lexing_lexbuf*,int);int Cyc_Lexing_lexeme_start(struct Cyc_Lexing_lexbuf*);
int Cyc_Lexing_lexeme_end(struct Cyc_Lexing_lexbuf*);struct Cyc_List_List{void*hd;
struct Cyc_List_List*tl;};struct Cyc_List_List*Cyc_List_list(struct _dyneither_ptr);
extern char Cyc_List_List_mismatch[18];void Cyc_List_iter(void(*f)(void*),struct Cyc_List_List*
x);struct Cyc_List_List*Cyc_List_imp_rev(struct Cyc_List_List*x);struct Cyc_List_List*
Cyc_List_append(struct Cyc_List_List*x,struct Cyc_List_List*y);extern char Cyc_List_Nth[
8];int Cyc_List_mem(int(*compare)(void*,void*),struct Cyc_List_List*l,void*x);
struct Cyc_Iter_Iter{void*env;int(*next)(void*env,void*dest);};int Cyc_Iter_next(
struct Cyc_Iter_Iter,void*);struct Cyc_Set_Set;struct Cyc_Set_Set*Cyc_Set_empty(int(*
cmp)(void*,void*));struct Cyc_Set_Set*Cyc_Set_insert(struct Cyc_Set_Set*s,void*elt);
struct Cyc_Set_Set*Cyc_Set_union_two(struct Cyc_Set_Set*s1,struct Cyc_Set_Set*s2);
struct Cyc_Set_Set*Cyc_Set_diff(struct Cyc_Set_Set*s1,struct Cyc_Set_Set*s2);struct
Cyc_Set_Set*Cyc_Set_delete(struct Cyc_Set_Set*s,void*elt);int Cyc_Set_cardinality(
struct Cyc_Set_Set*s);int Cyc_Set_member(struct Cyc_Set_Set*s,void*elt);extern char
Cyc_Set_Absent[11];struct Cyc_Iter_Iter Cyc_Set_make_iter(struct _RegionHandle*rgn,
struct Cyc_Set_Set*s);unsigned long Cyc_strlen(struct _dyneither_ptr s);int Cyc_strptrcmp(
struct _dyneither_ptr*s1,struct _dyneither_ptr*s2);struct _dyneither_ptr Cyc_strconcat(
struct _dyneither_ptr,struct _dyneither_ptr);struct _dyneither_ptr Cyc_strdup(struct
_dyneither_ptr src);struct _dyneither_ptr Cyc_substring(struct _dyneither_ptr,int ofs,
unsigned long n);struct Cyc_Hashtable_Table;struct Cyc_Hashtable_Table*Cyc_Hashtable_create(
int sz,int(*cmp)(void*,void*),int(*hash)(void*));void Cyc_Hashtable_insert(struct
Cyc_Hashtable_Table*t,void*key,void*val);void*Cyc_Hashtable_lookup(struct Cyc_Hashtable_Table*
t,void*key);int Cyc_Hashtable_hash_stringptr(struct _dyneither_ptr*p);struct
_dyneither_ptr Cyc_Filename_concat(struct _dyneither_ptr,struct _dyneither_ptr);
struct _dyneither_ptr Cyc_Filename_chop_extension(struct _dyneither_ptr);struct
_dyneither_ptr Cyc_Filename_dirname(struct _dyneither_ptr);struct _dyneither_ptr Cyc_Filename_basename(
struct _dyneither_ptr);struct Cyc_Lineno_Pos{struct _dyneither_ptr logical_file;
struct _dyneither_ptr line;int line_no;int col;};extern char Cyc_Position_Exit[9];void
Cyc_Position_reset_position(struct _dyneither_ptr);struct Cyc_Position_Segment;
struct Cyc_Position_Error{struct _dyneither_ptr source;struct Cyc_Position_Segment*
seg;void*kind;struct _dyneither_ptr desc;};extern char Cyc_Position_Nocontext[14];
extern char Cyc_Arg_Bad[8];struct Cyc_Arg_Bad_struct{char*tag;struct _dyneither_ptr
f1;};extern char Cyc_Arg_Error[10];struct Cyc_Arg_Unit_spec_struct{int tag;void(*f1)();
};struct Cyc_Arg_Flag_spec_struct{int tag;void(*f1)(struct _dyneither_ptr);};struct
Cyc_Arg_FlagString_spec_struct{int tag;void(*f1)(struct _dyneither_ptr,struct
_dyneither_ptr);};struct Cyc_Arg_Set_spec_struct{int tag;int*f1;};struct Cyc_Arg_Clear_spec_struct{
int tag;int*f1;};struct Cyc_Arg_String_spec_struct{int tag;void(*f1)(struct
_dyneither_ptr);};struct Cyc_Arg_Int_spec_struct{int tag;void(*f1)(int);};struct
Cyc_Arg_Rest_spec_struct{int tag;void(*f1)(struct _dyneither_ptr);};void Cyc_Arg_usage(
struct Cyc_List_List*,struct _dyneither_ptr);void Cyc_Arg_parse(struct Cyc_List_List*
specs,void(*anonfun)(struct _dyneither_ptr),struct _dyneither_ptr errmsg,struct
_dyneither_ptr args);struct Cyc_Buffer_t;struct Cyc_Buffer_t*Cyc_Buffer_create(
unsigned int n);struct _dyneither_ptr Cyc_Buffer_contents(struct Cyc_Buffer_t*);void
Cyc_Buffer_add_char(struct Cyc_Buffer_t*,char);void Cyc_Buffer_add_string(struct
Cyc_Buffer_t*,struct _dyneither_ptr);struct Cyc_Absyn_Loc_n_struct{int tag;};struct
Cyc_Absyn_Rel_n_struct{int tag;struct Cyc_List_List*f1;};struct Cyc_Absyn_Abs_n_struct{
int tag;struct Cyc_List_List*f1;};union Cyc_Absyn_Nmspace_union{struct Cyc_Absyn_Loc_n_struct
Loc_n;struct Cyc_Absyn_Rel_n_struct Rel_n;struct Cyc_Absyn_Abs_n_struct Abs_n;};
struct _tuple0{union Cyc_Absyn_Nmspace_union f1;struct _dyneither_ptr*f2;};struct Cyc_Absyn_Conref;
struct Cyc_Absyn_Tqual{int print_const;int q_volatile;int q_restrict;int real_const;
struct Cyc_Position_Segment*loc;};struct Cyc_Absyn_Eq_constr_struct{int tag;void*f1;
};struct Cyc_Absyn_Forward_constr_struct{int tag;struct Cyc_Absyn_Conref*f1;};
struct Cyc_Absyn_No_constr_struct{int tag;};union Cyc_Absyn_Constraint_union{struct
Cyc_Absyn_Eq_constr_struct Eq_constr;struct Cyc_Absyn_Forward_constr_struct
Forward_constr;struct Cyc_Absyn_No_constr_struct No_constr;};struct Cyc_Absyn_Conref{
union Cyc_Absyn_Constraint_union v;};struct Cyc_Absyn_Eq_kb_struct{int tag;void*f1;}
;struct Cyc_Absyn_Unknown_kb_struct{int tag;struct Cyc_Core_Opt*f1;};struct Cyc_Absyn_Less_kb_struct{
int tag;struct Cyc_Core_Opt*f1;void*f2;};struct Cyc_Absyn_Tvar{struct _dyneither_ptr*
name;int identity;void*kind;};struct Cyc_Absyn_Upper_b_struct{int tag;struct Cyc_Absyn_Exp*
f1;};struct Cyc_Absyn_PtrLoc{struct Cyc_Position_Segment*ptr_loc;struct Cyc_Position_Segment*
rgn_loc;struct Cyc_Position_Segment*zt_loc;};struct Cyc_Absyn_PtrAtts{void*rgn;
struct Cyc_Absyn_Conref*nullable;struct Cyc_Absyn_Conref*bounds;struct Cyc_Absyn_Conref*
zero_term;struct Cyc_Absyn_PtrLoc*ptrloc;};struct Cyc_Absyn_PtrInfo{void*elt_typ;
struct Cyc_Absyn_Tqual elt_tq;struct Cyc_Absyn_PtrAtts ptr_atts;};struct Cyc_Absyn_VarargInfo{
struct Cyc_Core_Opt*name;struct Cyc_Absyn_Tqual tq;void*type;int inject;};struct Cyc_Absyn_FnInfo{
struct Cyc_List_List*tvars;struct Cyc_Core_Opt*effect;void*ret_typ;struct Cyc_List_List*
args;int c_varargs;struct Cyc_Absyn_VarargInfo*cyc_varargs;struct Cyc_List_List*
rgn_po;struct Cyc_List_List*attributes;};struct Cyc_Absyn_UnknownTunionInfo{struct
_tuple0*name;int is_xtunion;int is_flat;};struct Cyc_Absyn_UnknownTunion_struct{int
tag;struct Cyc_Absyn_UnknownTunionInfo f1;};struct Cyc_Absyn_KnownTunion_struct{int
tag;struct Cyc_Absyn_Tuniondecl**f1;};union Cyc_Absyn_TunionInfoU_union{struct Cyc_Absyn_UnknownTunion_struct
UnknownTunion;struct Cyc_Absyn_KnownTunion_struct KnownTunion;};struct Cyc_Absyn_TunionInfo{
union Cyc_Absyn_TunionInfoU_union tunion_info;struct Cyc_List_List*targs;struct Cyc_Core_Opt*
rgn;};struct Cyc_Absyn_UnknownTunionFieldInfo{struct _tuple0*tunion_name;struct
_tuple0*field_name;int is_xtunion;};struct Cyc_Absyn_UnknownTunionfield_struct{int
tag;struct Cyc_Absyn_UnknownTunionFieldInfo f1;};struct Cyc_Absyn_KnownTunionfield_struct{
int tag;struct Cyc_Absyn_Tuniondecl*f1;struct Cyc_Absyn_Tunionfield*f2;};union Cyc_Absyn_TunionFieldInfoU_union{
struct Cyc_Absyn_UnknownTunionfield_struct UnknownTunionfield;struct Cyc_Absyn_KnownTunionfield_struct
KnownTunionfield;};struct Cyc_Absyn_TunionFieldInfo{union Cyc_Absyn_TunionFieldInfoU_union
field_info;struct Cyc_List_List*targs;};struct Cyc_Absyn_UnknownAggr_struct{int tag;
void*f1;struct _tuple0*f2;};struct Cyc_Absyn_KnownAggr_struct{int tag;struct Cyc_Absyn_Aggrdecl**
f1;};union Cyc_Absyn_AggrInfoU_union{struct Cyc_Absyn_UnknownAggr_struct
UnknownAggr;struct Cyc_Absyn_KnownAggr_struct KnownAggr;};struct Cyc_Absyn_AggrInfo{
union Cyc_Absyn_AggrInfoU_union aggr_info;struct Cyc_List_List*targs;};struct Cyc_Absyn_ArrayInfo{
void*elt_type;struct Cyc_Absyn_Tqual tq;struct Cyc_Absyn_Exp*num_elts;struct Cyc_Absyn_Conref*
zero_term;struct Cyc_Position_Segment*zt_loc;};struct Cyc_Absyn_Evar_struct{int tag;
struct Cyc_Core_Opt*f1;struct Cyc_Core_Opt*f2;int f3;struct Cyc_Core_Opt*f4;};struct
Cyc_Absyn_VarType_struct{int tag;struct Cyc_Absyn_Tvar*f1;};struct Cyc_Absyn_TunionType_struct{
int tag;struct Cyc_Absyn_TunionInfo f1;};struct Cyc_Absyn_TunionFieldType_struct{int
tag;struct Cyc_Absyn_TunionFieldInfo f1;};struct Cyc_Absyn_PointerType_struct{int
tag;struct Cyc_Absyn_PtrInfo f1;};struct Cyc_Absyn_IntType_struct{int tag;void*f1;
void*f2;};struct Cyc_Absyn_DoubleType_struct{int tag;int f1;};struct Cyc_Absyn_ArrayType_struct{
int tag;struct Cyc_Absyn_ArrayInfo f1;};struct Cyc_Absyn_FnType_struct{int tag;struct
Cyc_Absyn_FnInfo f1;};struct Cyc_Absyn_TupleType_struct{int tag;struct Cyc_List_List*
f1;};struct Cyc_Absyn_AggrType_struct{int tag;struct Cyc_Absyn_AggrInfo f1;};struct
Cyc_Absyn_AnonAggrType_struct{int tag;void*f1;struct Cyc_List_List*f2;};struct Cyc_Absyn_EnumType_struct{
int tag;struct _tuple0*f1;struct Cyc_Absyn_Enumdecl*f2;};struct Cyc_Absyn_AnonEnumType_struct{
int tag;struct Cyc_List_List*f1;};struct Cyc_Absyn_RgnHandleType_struct{int tag;void*
f1;};struct Cyc_Absyn_DynRgnType_struct{int tag;void*f1;void*f2;};struct Cyc_Absyn_TypedefType_struct{
int tag;struct _tuple0*f1;struct Cyc_List_List*f2;struct Cyc_Absyn_Typedefdecl*f3;
void**f4;};struct Cyc_Absyn_ValueofType_struct{int tag;struct Cyc_Absyn_Exp*f1;};
struct Cyc_Absyn_TagType_struct{int tag;void*f1;};struct Cyc_Absyn_AccessEff_struct{
int tag;void*f1;};struct Cyc_Absyn_JoinEff_struct{int tag;struct Cyc_List_List*f1;};
struct Cyc_Absyn_RgnsEff_struct{int tag;void*f1;};struct Cyc_Absyn_NoTypes_struct{
int tag;struct Cyc_List_List*f1;struct Cyc_Position_Segment*f2;};struct Cyc_Absyn_WithTypes_struct{
int tag;struct Cyc_List_List*f1;int f2;struct Cyc_Absyn_VarargInfo*f3;struct Cyc_Core_Opt*
f4;struct Cyc_List_List*f5;};struct Cyc_Absyn_Regparm_att_struct{int tag;int f1;};
struct Cyc_Absyn_Aligned_att_struct{int tag;int f1;};struct Cyc_Absyn_Section_att_struct{
int tag;struct _dyneither_ptr f1;};struct Cyc_Absyn_Format_att_struct{int tag;void*f1;
int f2;int f3;};struct Cyc_Absyn_Initializes_att_struct{int tag;int f1;};struct Cyc_Absyn_Mode_att_struct{
int tag;struct _dyneither_ptr f1;};struct Cyc_Absyn_Carray_mod_struct{int tag;struct
Cyc_Absyn_Conref*f1;struct Cyc_Position_Segment*f2;};struct Cyc_Absyn_ConstArray_mod_struct{
int tag;struct Cyc_Absyn_Exp*f1;struct Cyc_Absyn_Conref*f2;struct Cyc_Position_Segment*
f3;};struct Cyc_Absyn_Pointer_mod_struct{int tag;struct Cyc_Absyn_PtrAtts f1;struct
Cyc_Absyn_Tqual f2;};struct Cyc_Absyn_Function_mod_struct{int tag;void*f1;};struct
Cyc_Absyn_TypeParams_mod_struct{int tag;struct Cyc_List_List*f1;struct Cyc_Position_Segment*
f2;int f3;};struct Cyc_Absyn_Attributes_mod_struct{int tag;struct Cyc_Position_Segment*
f1;struct Cyc_List_List*f2;};struct Cyc_Absyn_Char_c_struct{int tag;void*f1;char f2;
};struct Cyc_Absyn_Short_c_struct{int tag;void*f1;short f2;};struct Cyc_Absyn_Int_c_struct{
int tag;void*f1;int f2;};struct Cyc_Absyn_LongLong_c_struct{int tag;void*f1;
long long f2;};struct Cyc_Absyn_Float_c_struct{int tag;struct _dyneither_ptr f1;};
struct Cyc_Absyn_String_c_struct{int tag;struct _dyneither_ptr f1;};struct Cyc_Absyn_Null_c_struct{
int tag;};union Cyc_Absyn_Cnst_union{struct Cyc_Absyn_Char_c_struct Char_c;struct Cyc_Absyn_Short_c_struct
Short_c;struct Cyc_Absyn_Int_c_struct Int_c;struct Cyc_Absyn_LongLong_c_struct
LongLong_c;struct Cyc_Absyn_Float_c_struct Float_c;struct Cyc_Absyn_String_c_struct
String_c;struct Cyc_Absyn_Null_c_struct Null_c;};struct Cyc_Absyn_VarargCallInfo{
int num_varargs;struct Cyc_List_List*injectors;struct Cyc_Absyn_VarargInfo*vai;};
struct Cyc_Absyn_StructField_struct{int tag;struct _dyneither_ptr*f1;};struct Cyc_Absyn_TupleIndex_struct{
int tag;unsigned int f1;};struct Cyc_Absyn_MallocInfo{int is_calloc;struct Cyc_Absyn_Exp*
rgn;void**elt_type;struct Cyc_Absyn_Exp*num_elts;int fat_result;};struct Cyc_Absyn_Const_e_struct{
int tag;union Cyc_Absyn_Cnst_union f1;};struct Cyc_Absyn_Var_e_struct{int tag;struct
_tuple0*f1;void*f2;};struct Cyc_Absyn_UnknownId_e_struct{int tag;struct _tuple0*f1;
};struct Cyc_Absyn_Primop_e_struct{int tag;void*f1;struct Cyc_List_List*f2;};struct
Cyc_Absyn_AssignOp_e_struct{int tag;struct Cyc_Absyn_Exp*f1;struct Cyc_Core_Opt*f2;
struct Cyc_Absyn_Exp*f3;};struct Cyc_Absyn_Increment_e_struct{int tag;struct Cyc_Absyn_Exp*
f1;void*f2;};struct Cyc_Absyn_Conditional_e_struct{int tag;struct Cyc_Absyn_Exp*f1;
struct Cyc_Absyn_Exp*f2;struct Cyc_Absyn_Exp*f3;};struct Cyc_Absyn_And_e_struct{int
tag;struct Cyc_Absyn_Exp*f1;struct Cyc_Absyn_Exp*f2;};struct Cyc_Absyn_Or_e_struct{
int tag;struct Cyc_Absyn_Exp*f1;struct Cyc_Absyn_Exp*f2;};struct Cyc_Absyn_SeqExp_e_struct{
int tag;struct Cyc_Absyn_Exp*f1;struct Cyc_Absyn_Exp*f2;};struct Cyc_Absyn_UnknownCall_e_struct{
int tag;struct Cyc_Absyn_Exp*f1;struct Cyc_List_List*f2;};struct Cyc_Absyn_FnCall_e_struct{
int tag;struct Cyc_Absyn_Exp*f1;struct Cyc_List_List*f2;struct Cyc_Absyn_VarargCallInfo*
f3;};struct Cyc_Absyn_Throw_e_struct{int tag;struct Cyc_Absyn_Exp*f1;};struct Cyc_Absyn_NoInstantiate_e_struct{
int tag;struct Cyc_Absyn_Exp*f1;};struct Cyc_Absyn_Instantiate_e_struct{int tag;
struct Cyc_Absyn_Exp*f1;struct Cyc_List_List*f2;};struct Cyc_Absyn_Cast_e_struct{
int tag;void*f1;struct Cyc_Absyn_Exp*f2;int f3;void*f4;};struct Cyc_Absyn_Address_e_struct{
int tag;struct Cyc_Absyn_Exp*f1;};struct Cyc_Absyn_New_e_struct{int tag;struct Cyc_Absyn_Exp*
f1;struct Cyc_Absyn_Exp*f2;};struct Cyc_Absyn_Sizeoftyp_e_struct{int tag;void*f1;};
struct Cyc_Absyn_Sizeofexp_e_struct{int tag;struct Cyc_Absyn_Exp*f1;};struct Cyc_Absyn_Offsetof_e_struct{
int tag;void*f1;void*f2;};struct Cyc_Absyn_Gentyp_e_struct{int tag;struct Cyc_List_List*
f1;void*f2;};struct Cyc_Absyn_Deref_e_struct{int tag;struct Cyc_Absyn_Exp*f1;};
struct Cyc_Absyn_AggrMember_e_struct{int tag;struct Cyc_Absyn_Exp*f1;struct
_dyneither_ptr*f2;};struct Cyc_Absyn_AggrArrow_e_struct{int tag;struct Cyc_Absyn_Exp*
f1;struct _dyneither_ptr*f2;};struct Cyc_Absyn_Subscript_e_struct{int tag;struct Cyc_Absyn_Exp*
f1;struct Cyc_Absyn_Exp*f2;};struct Cyc_Absyn_Tuple_e_struct{int tag;struct Cyc_List_List*
f1;};struct _tuple1{struct Cyc_Core_Opt*f1;struct Cyc_Absyn_Tqual f2;void*f3;};
struct Cyc_Absyn_CompoundLit_e_struct{int tag;struct _tuple1*f1;struct Cyc_List_List*
f2;};struct Cyc_Absyn_Array_e_struct{int tag;struct Cyc_List_List*f1;};struct Cyc_Absyn_Comprehension_e_struct{
int tag;struct Cyc_Absyn_Vardecl*f1;struct Cyc_Absyn_Exp*f2;struct Cyc_Absyn_Exp*f3;
int f4;};struct Cyc_Absyn_Struct_e_struct{int tag;struct _tuple0*f1;struct Cyc_List_List*
f2;struct Cyc_List_List*f3;struct Cyc_Absyn_Aggrdecl*f4;};struct Cyc_Absyn_AnonStruct_e_struct{
int tag;void*f1;struct Cyc_List_List*f2;};struct Cyc_Absyn_Tunion_e_struct{int tag;
struct Cyc_List_List*f1;struct Cyc_Absyn_Tuniondecl*f2;struct Cyc_Absyn_Tunionfield*
f3;};struct Cyc_Absyn_Enum_e_struct{int tag;struct _tuple0*f1;struct Cyc_Absyn_Enumdecl*
f2;struct Cyc_Absyn_Enumfield*f3;};struct Cyc_Absyn_AnonEnum_e_struct{int tag;
struct _tuple0*f1;void*f2;struct Cyc_Absyn_Enumfield*f3;};struct Cyc_Absyn_Malloc_e_struct{
int tag;struct Cyc_Absyn_MallocInfo f1;};struct Cyc_Absyn_Swap_e_struct{int tag;
struct Cyc_Absyn_Exp*f1;struct Cyc_Absyn_Exp*f2;};struct Cyc_Absyn_UnresolvedMem_e_struct{
int tag;struct Cyc_Core_Opt*f1;struct Cyc_List_List*f2;};struct Cyc_Absyn_StmtExp_e_struct{
int tag;struct Cyc_Absyn_Stmt*f1;};struct Cyc_Absyn_Valueof_e_struct{int tag;void*f1;
};struct Cyc_Absyn_Exp{struct Cyc_Core_Opt*topt;void*r;struct Cyc_Position_Segment*
loc;void*annot;};struct Cyc_Absyn_Exp_s_struct{int tag;struct Cyc_Absyn_Exp*f1;};
struct Cyc_Absyn_Seq_s_struct{int tag;struct Cyc_Absyn_Stmt*f1;struct Cyc_Absyn_Stmt*
f2;};struct Cyc_Absyn_Return_s_struct{int tag;struct Cyc_Absyn_Exp*f1;};struct Cyc_Absyn_IfThenElse_s_struct{
int tag;struct Cyc_Absyn_Exp*f1;struct Cyc_Absyn_Stmt*f2;struct Cyc_Absyn_Stmt*f3;};
struct _tuple2{struct Cyc_Absyn_Exp*f1;struct Cyc_Absyn_Stmt*f2;};struct Cyc_Absyn_While_s_struct{
int tag;struct _tuple2 f1;struct Cyc_Absyn_Stmt*f2;};struct Cyc_Absyn_Break_s_struct{
int tag;struct Cyc_Absyn_Stmt*f1;};struct Cyc_Absyn_Continue_s_struct{int tag;struct
Cyc_Absyn_Stmt*f1;};struct Cyc_Absyn_Goto_s_struct{int tag;struct _dyneither_ptr*f1;
struct Cyc_Absyn_Stmt*f2;};struct Cyc_Absyn_For_s_struct{int tag;struct Cyc_Absyn_Exp*
f1;struct _tuple2 f2;struct _tuple2 f3;struct Cyc_Absyn_Stmt*f4;};struct Cyc_Absyn_Switch_s_struct{
int tag;struct Cyc_Absyn_Exp*f1;struct Cyc_List_List*f2;};struct Cyc_Absyn_Fallthru_s_struct{
int tag;struct Cyc_List_List*f1;struct Cyc_Absyn_Switch_clause**f2;};struct Cyc_Absyn_Decl_s_struct{
int tag;struct Cyc_Absyn_Decl*f1;struct Cyc_Absyn_Stmt*f2;};struct Cyc_Absyn_Label_s_struct{
int tag;struct _dyneither_ptr*f1;struct Cyc_Absyn_Stmt*f2;};struct Cyc_Absyn_Do_s_struct{
int tag;struct Cyc_Absyn_Stmt*f1;struct _tuple2 f2;};struct Cyc_Absyn_TryCatch_s_struct{
int tag;struct Cyc_Absyn_Stmt*f1;struct Cyc_List_List*f2;};struct Cyc_Absyn_Region_s_struct{
int tag;struct Cyc_Absyn_Tvar*f1;struct Cyc_Absyn_Vardecl*f2;int f3;struct Cyc_Absyn_Exp*
f4;struct Cyc_Absyn_Stmt*f5;};struct Cyc_Absyn_ResetRegion_s_struct{int tag;struct
Cyc_Absyn_Exp*f1;};struct Cyc_Absyn_Alias_s_struct{int tag;struct Cyc_Absyn_Exp*f1;
struct Cyc_Absyn_Tvar*f2;struct Cyc_Absyn_Vardecl*f3;struct Cyc_Absyn_Stmt*f4;};
struct Cyc_Absyn_Stmt{void*r;struct Cyc_Position_Segment*loc;struct Cyc_List_List*
non_local_preds;int try_depth;void*annot;};struct Cyc_Absyn_Var_p_struct{int tag;
struct Cyc_Absyn_Vardecl*f1;struct Cyc_Absyn_Pat*f2;};struct Cyc_Absyn_Reference_p_struct{
int tag;struct Cyc_Absyn_Vardecl*f1;struct Cyc_Absyn_Pat*f2;};struct Cyc_Absyn_TagInt_p_struct{
int tag;struct Cyc_Absyn_Tvar*f1;struct Cyc_Absyn_Vardecl*f2;};struct Cyc_Absyn_Tuple_p_struct{
int tag;struct Cyc_List_List*f1;int f2;};struct Cyc_Absyn_Pointer_p_struct{int tag;
struct Cyc_Absyn_Pat*f1;};struct Cyc_Absyn_Aggr_p_struct{int tag;struct Cyc_Absyn_AggrInfo
f1;struct Cyc_List_List*f2;struct Cyc_List_List*f3;int f4;};struct Cyc_Absyn_Tunion_p_struct{
int tag;struct Cyc_Absyn_Tuniondecl*f1;struct Cyc_Absyn_Tunionfield*f2;struct Cyc_List_List*
f3;int f4;};struct Cyc_Absyn_Int_p_struct{int tag;void*f1;int f2;};struct Cyc_Absyn_Char_p_struct{
int tag;char f1;};struct Cyc_Absyn_Float_p_struct{int tag;struct _dyneither_ptr f1;};
struct Cyc_Absyn_Enum_p_struct{int tag;struct Cyc_Absyn_Enumdecl*f1;struct Cyc_Absyn_Enumfield*
f2;};struct Cyc_Absyn_AnonEnum_p_struct{int tag;void*f1;struct Cyc_Absyn_Enumfield*
f2;};struct Cyc_Absyn_UnknownId_p_struct{int tag;struct _tuple0*f1;};struct Cyc_Absyn_UnknownCall_p_struct{
int tag;struct _tuple0*f1;struct Cyc_List_List*f2;int f3;};struct Cyc_Absyn_Exp_p_struct{
int tag;struct Cyc_Absyn_Exp*f1;};struct Cyc_Absyn_Pat{void*r;struct Cyc_Core_Opt*
topt;struct Cyc_Position_Segment*loc;};struct Cyc_Absyn_Switch_clause{struct Cyc_Absyn_Pat*
pattern;struct Cyc_Core_Opt*pat_vars;struct Cyc_Absyn_Exp*where_clause;struct Cyc_Absyn_Stmt*
body;struct Cyc_Position_Segment*loc;};struct Cyc_Absyn_Global_b_struct{int tag;
struct Cyc_Absyn_Vardecl*f1;};struct Cyc_Absyn_Funname_b_struct{int tag;struct Cyc_Absyn_Fndecl*
f1;};struct Cyc_Absyn_Param_b_struct{int tag;struct Cyc_Absyn_Vardecl*f1;};struct
Cyc_Absyn_Local_b_struct{int tag;struct Cyc_Absyn_Vardecl*f1;};struct Cyc_Absyn_Pat_b_struct{
int tag;struct Cyc_Absyn_Vardecl*f1;};struct Cyc_Absyn_Vardecl{void*sc;struct
_tuple0*name;struct Cyc_Absyn_Tqual tq;void*type;struct Cyc_Absyn_Exp*initializer;
struct Cyc_Core_Opt*rgn;struct Cyc_List_List*attributes;int escapes;};struct Cyc_Absyn_Fndecl{
void*sc;int is_inline;struct _tuple0*name;struct Cyc_List_List*tvs;struct Cyc_Core_Opt*
effect;void*ret_type;struct Cyc_List_List*args;int c_varargs;struct Cyc_Absyn_VarargInfo*
cyc_varargs;struct Cyc_List_List*rgn_po;struct Cyc_Absyn_Stmt*body;struct Cyc_Core_Opt*
cached_typ;struct Cyc_Core_Opt*param_vardecls;struct Cyc_Absyn_Vardecl*fn_vardecl;
struct Cyc_List_List*attributes;};struct Cyc_Absyn_Aggrfield{struct _dyneither_ptr*
name;struct Cyc_Absyn_Tqual tq;void*type;struct Cyc_Absyn_Exp*width;struct Cyc_List_List*
attributes;};struct Cyc_Absyn_AggrdeclImpl{struct Cyc_List_List*exist_vars;struct
Cyc_List_List*rgn_po;struct Cyc_List_List*fields;};struct Cyc_Absyn_Aggrdecl{void*
kind;void*sc;struct _tuple0*name;struct Cyc_List_List*tvs;struct Cyc_Absyn_AggrdeclImpl*
impl;struct Cyc_List_List*attributes;};struct Cyc_Absyn_Tunionfield{struct _tuple0*
name;struct Cyc_List_List*typs;struct Cyc_Position_Segment*loc;void*sc;};struct Cyc_Absyn_Tuniondecl{
void*sc;struct _tuple0*name;struct Cyc_List_List*tvs;struct Cyc_Core_Opt*fields;int
is_xtunion;int is_flat;};struct Cyc_Absyn_Enumfield{struct _tuple0*name;struct Cyc_Absyn_Exp*
tag;struct Cyc_Position_Segment*loc;};struct Cyc_Absyn_Enumdecl{void*sc;struct
_tuple0*name;struct Cyc_Core_Opt*fields;};struct Cyc_Absyn_Typedefdecl{struct
_tuple0*name;struct Cyc_Absyn_Tqual tq;struct Cyc_List_List*tvs;struct Cyc_Core_Opt*
kind;struct Cyc_Core_Opt*defn;struct Cyc_List_List*atts;};struct Cyc_Absyn_Var_d_struct{
int tag;struct Cyc_Absyn_Vardecl*f1;};struct Cyc_Absyn_Fn_d_struct{int tag;struct Cyc_Absyn_Fndecl*
f1;};struct Cyc_Absyn_Let_d_struct{int tag;struct Cyc_Absyn_Pat*f1;struct Cyc_Core_Opt*
f2;struct Cyc_Absyn_Exp*f3;};struct Cyc_Absyn_Letv_d_struct{int tag;struct Cyc_List_List*
f1;};struct Cyc_Absyn_Aggr_d_struct{int tag;struct Cyc_Absyn_Aggrdecl*f1;};struct
Cyc_Absyn_Tunion_d_struct{int tag;struct Cyc_Absyn_Tuniondecl*f1;};struct Cyc_Absyn_Enum_d_struct{
int tag;struct Cyc_Absyn_Enumdecl*f1;};struct Cyc_Absyn_Typedef_d_struct{int tag;
struct Cyc_Absyn_Typedefdecl*f1;};struct Cyc_Absyn_Namespace_d_struct{int tag;
struct _dyneither_ptr*f1;struct Cyc_List_List*f2;};struct Cyc_Absyn_Using_d_struct{
int tag;struct _tuple0*f1;struct Cyc_List_List*f2;};struct Cyc_Absyn_ExternC_d_struct{
int tag;struct Cyc_List_List*f1;};struct Cyc_Absyn_ExternCinclude_d_struct{int tag;
struct Cyc_List_List*f1;struct Cyc_List_List*f2;};struct Cyc_Absyn_Decl{void*r;
struct Cyc_Position_Segment*loc;};struct Cyc_Absyn_ArrayElement_struct{int tag;
struct Cyc_Absyn_Exp*f1;};struct Cyc_Absyn_FieldName_struct{int tag;struct
_dyneither_ptr*f1;};extern char Cyc_Absyn_EmptyAnnot[15];struct _tuple3{void*f1;
struct _tuple0*f2;};struct _tuple3 Cyc_Absyn_aggr_kinded_name(union Cyc_Absyn_AggrInfoU_union);
void Cyc_Absyn_print_decls(struct Cyc_List_List*);struct Cyc_PP_Ppstate;struct Cyc_PP_Out;
struct Cyc_PP_Doc;struct Cyc_Absynpp_Params{int expand_typedefs: 1;int qvar_to_Cids: 1;
int add_cyc_prefix: 1;int to_VC: 1;int decls_first: 1;int rewrite_temp_tvars: 1;int
print_all_tvars: 1;int print_all_kinds: 1;int print_all_effects: 1;int
print_using_stmts: 1;int print_externC_stmts: 1;int print_full_evars: 1;int
print_zeroterm: 1;int generate_line_directives: 1;int use_curr_namespace: 1;struct Cyc_List_List*
curr_namespace;};void Cyc_Absynpp_decllist2file(struct Cyc_List_List*tdl,struct Cyc___cycFILE*
f);struct Cyc_List_List*Cyc_Parse_parse_file(struct Cyc___cycFILE*f);struct Cyc_Declaration_spec;
struct Cyc_Declarator;struct Cyc_Abstractdeclarator;struct _tuple4{void*f1;int f2;};
struct Cyc_Int_tok_struct{int tag;struct _tuple4 f1;};struct Cyc_Char_tok_struct{int
tag;char f1;};struct Cyc_String_tok_struct{int tag;struct _dyneither_ptr f1;};struct
Cyc_Stringopt_tok_struct{int tag;struct Cyc_Core_Opt*f1;};struct Cyc_QualId_tok_struct{
int tag;struct _tuple0*f1;};struct _tuple5{struct Cyc_Position_Segment*f1;struct Cyc_Absyn_Conref*
f2;struct Cyc_Absyn_Conref*f3;};struct Cyc_YY1_struct{int tag;struct _tuple5*f1;};
struct Cyc_YY2_struct{int tag;struct Cyc_Absyn_Conref*f1;};struct Cyc_YY3_struct{int
tag;struct Cyc_Absyn_Exp*f1;};struct Cyc_YY4_struct{int tag;struct Cyc_Absyn_Exp*f1;
};struct Cyc_YY5_struct{int tag;struct Cyc_List_List*f1;};struct Cyc_YY6_struct{int
tag;struct Cyc_List_List*f1;};struct Cyc_YY7_struct{int tag;void*f1;};struct Cyc_YY8_struct{
int tag;struct Cyc_Core_Opt*f1;};struct Cyc_YY9_struct{int tag;struct Cyc_Absyn_Stmt*
f1;};struct Cyc_YY10_struct{int tag;struct Cyc_List_List*f1;};struct Cyc_YY11_struct{
int tag;struct Cyc_Absyn_Pat*f1;};struct _tuple6{struct Cyc_List_List*f1;int f2;};
struct Cyc_YY12_struct{int tag;struct _tuple6*f1;};struct Cyc_YY13_struct{int tag;
struct Cyc_List_List*f1;};struct _tuple7{struct Cyc_List_List*f1;struct Cyc_Absyn_Pat*
f2;};struct Cyc_YY14_struct{int tag;struct _tuple7*f1;};struct Cyc_YY15_struct{int
tag;struct Cyc_List_List*f1;};struct Cyc_YY16_struct{int tag;struct _tuple6*f1;};
struct Cyc_YY17_struct{int tag;struct Cyc_Absyn_Fndecl*f1;};struct Cyc_YY18_struct{
int tag;struct Cyc_List_List*f1;};struct Cyc_YY19_struct{int tag;struct Cyc_Declaration_spec*
f1;};struct _tuple8{struct Cyc_Declarator*f1;struct Cyc_Absyn_Exp*f2;};struct Cyc_YY20_struct{
int tag;struct _tuple8*f1;};struct Cyc_YY21_struct{int tag;struct Cyc_List_List*f1;};
struct Cyc_YY22_struct{int tag;void*f1;};struct Cyc_YY23_struct{int tag;void*f1;};
struct Cyc_YY24_struct{int tag;void*f1;};struct Cyc_YY25_struct{int tag;struct Cyc_Absyn_Tqual
f1;};struct Cyc_YY26_struct{int tag;struct Cyc_List_List*f1;};struct Cyc_YY27_struct{
int tag;struct Cyc_List_List*f1;};struct Cyc_YY28_struct{int tag;struct Cyc_List_List*
f1;};struct Cyc_YY29_struct{int tag;struct Cyc_Declarator*f1;};struct Cyc_YY30_struct{
int tag;struct Cyc_Abstractdeclarator*f1;};struct Cyc_YY31_struct{int tag;int f1;};
struct Cyc_YY32_struct{int tag;void*f1;};struct Cyc_YY33_struct{int tag;struct Cyc_Absyn_Tunionfield*
f1;};struct Cyc_YY34_struct{int tag;struct Cyc_List_List*f1;};struct _tuple9{struct
Cyc_Absyn_Tqual f1;struct Cyc_List_List*f2;struct Cyc_List_List*f3;};struct Cyc_YY35_struct{
int tag;struct _tuple9*f1;};struct Cyc_YY36_struct{int tag;struct Cyc_List_List*f1;};
struct Cyc_YY37_struct{int tag;struct _tuple1*f1;};struct Cyc_YY38_struct{int tag;
struct Cyc_List_List*f1;};struct _tuple10{struct Cyc_List_List*f1;int f2;struct Cyc_Absyn_VarargInfo*
f3;struct Cyc_Core_Opt*f4;struct Cyc_List_List*f5;};struct Cyc_YY39_struct{int tag;
struct _tuple10*f1;};struct Cyc_YY40_struct{int tag;struct Cyc_List_List*f1;};struct
Cyc_YY41_struct{int tag;struct Cyc_List_List*f1;};struct Cyc_YY42_struct{int tag;
void*f1;};struct Cyc_YY43_struct{int tag;void*f1;};struct Cyc_YY44_struct{int tag;
void*f1;};struct Cyc_YY45_struct{int tag;struct Cyc_List_List*f1;};struct Cyc_YY46_struct{
int tag;void*f1;};struct Cyc_YY47_struct{int tag;struct Cyc_Absyn_Enumfield*f1;};
struct Cyc_YY48_struct{int tag;struct Cyc_List_List*f1;};struct Cyc_YY49_struct{int
tag;struct Cyc_Core_Opt*f1;};struct Cyc_YY50_struct{int tag;struct Cyc_List_List*f1;
};struct Cyc_YY51_struct{int tag;struct Cyc_Absyn_Conref*f1;};struct Cyc_YY52_struct{
int tag;struct Cyc_List_List*f1;};struct Cyc_YYINITIALSVAL_struct{int tag;};union Cyc_YYSTYPE_union{
struct Cyc_Int_tok_struct Int_tok;struct Cyc_Char_tok_struct Char_tok;struct Cyc_String_tok_struct
String_tok;struct Cyc_Stringopt_tok_struct Stringopt_tok;struct Cyc_QualId_tok_struct
QualId_tok;struct Cyc_YY1_struct YY1;struct Cyc_YY2_struct YY2;struct Cyc_YY3_struct
YY3;struct Cyc_YY4_struct YY4;struct Cyc_YY5_struct YY5;struct Cyc_YY6_struct YY6;
struct Cyc_YY7_struct YY7;struct Cyc_YY8_struct YY8;struct Cyc_YY9_struct YY9;struct
Cyc_YY10_struct YY10;struct Cyc_YY11_struct YY11;struct Cyc_YY12_struct YY12;struct
Cyc_YY13_struct YY13;struct Cyc_YY14_struct YY14;struct Cyc_YY15_struct YY15;struct
Cyc_YY16_struct YY16;struct Cyc_YY17_struct YY17;struct Cyc_YY18_struct YY18;struct
Cyc_YY19_struct YY19;struct Cyc_YY20_struct YY20;struct Cyc_YY21_struct YY21;struct
Cyc_YY22_struct YY22;struct Cyc_YY23_struct YY23;struct Cyc_YY24_struct YY24;struct
Cyc_YY25_struct YY25;struct Cyc_YY26_struct YY26;struct Cyc_YY27_struct YY27;struct
Cyc_YY28_struct YY28;struct Cyc_YY29_struct YY29;struct Cyc_YY30_struct YY30;struct
Cyc_YY31_struct YY31;struct Cyc_YY32_struct YY32;struct Cyc_YY33_struct YY33;struct
Cyc_YY34_struct YY34;struct Cyc_YY35_struct YY35;struct Cyc_YY36_struct YY36;struct
Cyc_YY37_struct YY37;struct Cyc_YY38_struct YY38;struct Cyc_YY39_struct YY39;struct
Cyc_YY40_struct YY40;struct Cyc_YY41_struct YY41;struct Cyc_YY42_struct YY42;struct
Cyc_YY43_struct YY43;struct Cyc_YY44_struct YY44;struct Cyc_YY45_struct YY45;struct
Cyc_YY46_struct YY46;struct Cyc_YY47_struct YY47;struct Cyc_YY48_struct YY48;struct
Cyc_YY49_struct YY49;struct Cyc_YY50_struct YY50;struct Cyc_YY51_struct YY51;struct
Cyc_YY52_struct YY52;struct Cyc_YYINITIALSVAL_struct YYINITIALSVAL;};struct Cyc_Yyltype{
int timestamp;int first_line;int first_column;int last_line;int last_column;};struct
Cyc_Dict_T;struct Cyc_Dict_Dict{int(*rel)(void*,void*);struct _RegionHandle*r;
struct Cyc_Dict_T*t;};extern char Cyc_Dict_Present[12];extern char Cyc_Dict_Absent[11];
struct _tuple11{void*f1;void*f2;};struct _tuple11*Cyc_Dict_rchoose(struct
_RegionHandle*r,struct Cyc_Dict_Dict d);struct _tuple11*Cyc_Dict_rchoose(struct
_RegionHandle*,struct Cyc_Dict_Dict d);struct Cyc_RgnOrder_RgnPO;struct Cyc_RgnOrder_RgnPO*
Cyc_RgnOrder_initial_fn_po(struct _RegionHandle*,struct Cyc_List_List*tvs,struct
Cyc_List_List*po,void*effect,struct Cyc_Absyn_Tvar*fst_rgn,struct Cyc_Position_Segment*);
struct Cyc_RgnOrder_RgnPO*Cyc_RgnOrder_add_outlives_constraint(struct
_RegionHandle*,struct Cyc_RgnOrder_RgnPO*po,void*eff,void*rgn,struct Cyc_Position_Segment*
loc);struct Cyc_RgnOrder_RgnPO*Cyc_RgnOrder_add_youngest(struct _RegionHandle*,
struct Cyc_RgnOrder_RgnPO*po,struct Cyc_Absyn_Tvar*rgn,int resetable,int opened);int
Cyc_RgnOrder_is_region_resetable(struct Cyc_RgnOrder_RgnPO*po,struct Cyc_Absyn_Tvar*
r);int Cyc_RgnOrder_effect_outlives(struct Cyc_RgnOrder_RgnPO*po,void*eff,void*rgn);
int Cyc_RgnOrder_satisfies_constraints(struct Cyc_RgnOrder_RgnPO*po,struct Cyc_List_List*
constraints,void*default_bound,int do_pin);int Cyc_RgnOrder_eff_outlives_eff(
struct Cyc_RgnOrder_RgnPO*po,void*eff1,void*eff2);void Cyc_RgnOrder_print_region_po(
struct Cyc_RgnOrder_RgnPO*po);struct Cyc_Tcenv_CList{void*hd;struct Cyc_Tcenv_CList*
tl;};struct Cyc_Tcenv_VarRes_struct{int tag;void*f1;};struct Cyc_Tcenv_AggrRes_struct{
int tag;struct Cyc_Absyn_Aggrdecl*f1;};struct Cyc_Tcenv_TunionRes_struct{int tag;
struct Cyc_Absyn_Tuniondecl*f1;struct Cyc_Absyn_Tunionfield*f2;};struct Cyc_Tcenv_EnumRes_struct{
int tag;struct Cyc_Absyn_Enumdecl*f1;struct Cyc_Absyn_Enumfield*f2;};struct Cyc_Tcenv_AnonEnumRes_struct{
int tag;void*f1;struct Cyc_Absyn_Enumfield*f2;};struct Cyc_Tcenv_Genv{struct
_RegionHandle*grgn;struct Cyc_Set_Set*namespaces;struct Cyc_Dict_Dict aggrdecls;
struct Cyc_Dict_Dict tuniondecls;struct Cyc_Dict_Dict enumdecls;struct Cyc_Dict_Dict
typedefs;struct Cyc_Dict_Dict ordinaries;struct Cyc_List_List*availables;};struct
Cyc_Tcenv_Fenv;struct Cyc_Tcenv_Stmt_j_struct{int tag;struct Cyc_Absyn_Stmt*f1;};
struct Cyc_Tcenv_Tenv{struct Cyc_List_List*ns;struct Cyc_Dict_Dict ae;struct Cyc_Tcenv_Fenv*
le;int allow_valueof;};struct Cyc_Tcenv_Tenv*Cyc_Tcenv_tc_init(struct _RegionHandle*);
void Cyc_Tc_tc(struct _RegionHandle*,struct Cyc_Tcenv_Tenv*te,int var_default_init,
struct Cyc_List_List*ds);typedef struct{int quot;int rem;}Cyc_div_t;typedef struct{
long quot;long rem;}Cyc_ldiv_t;void*abort();void exit(int);int system(const char*);
struct Cyc_timespec{long tv_sec;int tv_nsec;};struct Cyc_stat{int st_dev;unsigned int
st_ino;unsigned short st_mode;unsigned short st_nlink;unsigned int st_uid;
unsigned int st_gid;int st_rdev;struct Cyc_timespec st_atimespec;struct Cyc_timespec
st_mtimespec;struct Cyc_timespec st_ctimespec;long long st_size;long long st_blocks;
unsigned int st_blksize;unsigned int st_flags;unsigned int st_gen;int st_lspare;
long long st_qspare[2];};int mkdir(const char*pathname,unsigned short mode);struct
Cyc_flock{long long l_start;long long l_len;int l_pid;short l_type;short l_whence;};
struct Cyc_Flock_struct{int tag;struct Cyc_flock*f1;};struct Cyc_Long_struct{int tag;
long f1;};int Cyc_open(const char*,int,struct _dyneither_ptr);struct Cyc_option{
struct _dyneither_ptr name;int has_arg;int*flag;int val;};int chdir(const char*);int
close(int);struct _dyneither_ptr Cyc_getcwd(struct _dyneither_ptr buf,unsigned long
size);int isspace(int);int toupper(int);void Cyc_Lex_lex_init(int
use_cyclone_keywords);extern char*Ccomp;static int Cyc_do_setjmp=0;struct Cyc___cycFILE*
Cyc_log_file=0;struct Cyc___cycFILE*Cyc_cstubs_file=0;struct Cyc___cycFILE*Cyc_cycstubs_file=
0;int Cyc_log(struct _dyneither_ptr fmt,struct _dyneither_ptr ap);int Cyc_log(struct
_dyneither_ptr fmt,struct _dyneither_ptr ap){if(Cyc_log_file == 0){{const char*
_tmp325;void*_tmp324;(_tmp324=0,Cyc_fprintf(Cyc_stderr,((_tmp325="Internal error: log file is NULL\n",
_tag_dyneither(_tmp325,sizeof(char),34))),_tag_dyneither(_tmp324,sizeof(void*),0)));}
exit(1);}{int _tmp2=Cyc_vfprintf((struct Cyc___cycFILE*)_check_null(Cyc_log_file),
fmt,ap);Cyc_fflush((struct Cyc___cycFILE*)((struct Cyc___cycFILE*)_check_null(Cyc_log_file)));
return _tmp2;}}static struct _dyneither_ptr*Cyc_current_source=0;static struct Cyc_List_List*
Cyc_current_args=0;static struct Cyc_Set_Set**Cyc_current_targets=0;static void Cyc_add_target(
struct _dyneither_ptr*sptr);static void Cyc_add_target(struct _dyneither_ptr*sptr){
struct Cyc_Set_Set**_tmp326;Cyc_current_targets=((_tmp326=_cycalloc(sizeof(*
_tmp326)),((_tmp326[0]=((struct Cyc_Set_Set*(*)(struct Cyc_Set_Set*s,struct
_dyneither_ptr*elt))Cyc_Set_insert)(*((struct Cyc_Set_Set**)_check_null(Cyc_current_targets)),
sptr),_tmp326))));}struct _tuple12{struct _dyneither_ptr*f1;struct Cyc_Set_Set*f2;}
;struct _tuple12*Cyc_line(struct Cyc_Lexing_lexbuf*);int Cyc_macroname(struct Cyc_Lexing_lexbuf*);
int Cyc_args(struct Cyc_Lexing_lexbuf*);int Cyc_token(struct Cyc_Lexing_lexbuf*);int
Cyc_string(struct Cyc_Lexing_lexbuf*);struct Cyc___cycFILE*Cyc_slurp_out=0;int Cyc_slurp(
struct Cyc_Lexing_lexbuf*);int Cyc_slurp_string(struct Cyc_Lexing_lexbuf*);int Cyc_asm(
struct Cyc_Lexing_lexbuf*);int Cyc_asm_string(struct Cyc_Lexing_lexbuf*);int Cyc_asm_comment(
struct Cyc_Lexing_lexbuf*);struct _tuple13{struct _dyneither_ptr f1;struct
_dyneither_ptr*f2;};struct _tuple13*Cyc_suck_line(struct Cyc_Lexing_lexbuf*);int
Cyc_suck_macroname(struct Cyc_Lexing_lexbuf*);int Cyc_suck_restofline(struct Cyc_Lexing_lexbuf*);
struct _dyneither_ptr Cyc_current_line=(struct _dyneither_ptr){(void*)0,(void*)0,(
void*)(0 + 0)};struct _tuple14{struct _dyneither_ptr f1;struct _dyneither_ptr f2;};
struct _tuple15{struct _dyneither_ptr f1;struct Cyc_List_List*f2;struct Cyc_List_List*
f3;struct Cyc_List_List*f4;struct Cyc_List_List*f5;struct Cyc_List_List*f6;};struct
_tuple15*Cyc_spec(struct Cyc_Lexing_lexbuf*);int Cyc_commands(struct Cyc_Lexing_lexbuf*);
int Cyc_snarfsymbols(struct Cyc_Lexing_lexbuf*);int Cyc_block(struct Cyc_Lexing_lexbuf*);
int Cyc_block_string(struct Cyc_Lexing_lexbuf*);int Cyc_block_comment(struct Cyc_Lexing_lexbuf*);
struct _dyneither_ptr Cyc_current_headerfile=(struct _dyneither_ptr){(void*)0,(void*)
0,(void*)(0 + 0)};struct Cyc_List_List*Cyc_snarfed_symbols=0;struct Cyc_List_List*
Cyc_current_symbols=0;struct Cyc_List_List*Cyc_current_cstubs=0;struct Cyc_List_List*
Cyc_current_cycstubs=0;struct Cyc_List_List*Cyc_current_hstubs=0;struct Cyc_List_List*
Cyc_current_omit_symbols=0;struct Cyc_List_List*Cyc_current_cpp=0;struct Cyc_Buffer_t*
Cyc_specbuf=0;int Cyc_braces_to_match=0;int Cyc_parens_to_match=0;int Cyc_numdef=0;
const int Cyc_lex_base[406]=(const int[406]){0,0,75,192,305,310,311,166,312,91,27,
384,28,523,637,715,283,325,92,- 3,0,- 1,- 2,- 8,- 3,1,- 2,323,- 4,2,166,- 5,605,797,312,
- 6,- 7,- 4,16,- 3,29,11,835,- 3,910,13,- 14,223,12,- 2,216,20,26,28,33,23,48,70,54,64,
74,100,91,107,94,370,386,111,103,95,122,122,375,414,111,111,153,377,1024,1139,
525,176,210,228,214,216,217,242,498,1253,1368,- 9,654,- 10,224,245,508,1482,1597,
684,- 8,720,- 11,432,510,515,1674,1751,1828,1909,434,465,550,1984,249,249,249,248,
244,254,0,13,4,2065,5,628,2073,2138,660,49,467,6,2099,7,705,2161,2199,820,- 21,
1051,1056,261,314,242,251,258,250,271,281,274,275,278,288,295,282,- 19,292,296,
300,313,321,326,309,325,319,315,326,365,400,417,418,404,400,400,427,431,- 16,422,
421,415,436,433,449,427,449,453,441,445,436,436,- 18,444,438,442,453,464,447,449,
482,489,490,1,4,6,491,492,505,504,516,516,524,554,2,21,556,557,654,19,20,21,592,
555,554,585,592,594,23,650,651,- 13,600,603,658,659,660,622,623,678,679,686,636,
637,693,697,698,645,648,703,704,705,- 12,654,655,1030,- 20,1165,654,664,661,671,
670,665,668,698,700,698,712,1144,730,731,730,743,1258,1170,747,758,748,749,747,
760,1370,752,753,765,778,1375,- 7,- 8,8,1263,2231,9,996,2255,2293,1341,1279,- 49,
1150,- 2,817,- 4,818,997,1028,819,993,1023,1448,820,2320,2363,824,866,868,907,2433,
870,991,- 36,- 42,- 37,2508,- 28,909,- 40,- 25,911,- 27,- 45,- 39,- 48,2583,2612,1467,889,
979,1563,2622,2652,1582,2281,2685,2716,2754,1001,1090,2824,2862,1082,1092,1084,
1127,1148,1204,- 6,- 34,927,2794,- 47,- 30,- 32,- 46,- 29,- 31,- 33,962,2902,963,964,2128,
965,1002,1003,1014,1018,1020,1024,1032,1043,2975,3059,- 23,- 17,- 15,- 22,2239,1077,
- 24,- 41,- 38,- 35,- 26,1282,3141,4,3224,1076,15,1051,1055,1056,1058,1054,1071,1141};
const int Cyc_lex_backtrk[406]=(const int[406]){- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,
0,- 1,- 1,- 1,- 1,- 1,- 1,- 1,2,- 1,- 1,- 1,- 1,2,- 1,8,- 1,3,5,- 1,- 1,6,5,- 1,- 1,- 1,6,- 1,5,1,0,
- 1,0,1,- 1,12,13,- 1,13,13,13,13,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,-
1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,
- 1,- 1,- 1,- 1,- 1,- 1,11,12,2,4,4,- 1,0,0,0,2,0,- 1,- 1,- 1,- 1,- 1,- 1,- 1,2,2,8,3,5,- 1,6,5,
6,5,2,8,3,5,- 1,6,5,- 1,20,20,20,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,-
1,- 1,14,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,16,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,
- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,
- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,
- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,
- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,6,1,9,2,4,- 1,5,
4,- 1,- 1,2,- 1,48,- 1,48,48,48,48,48,48,48,48,5,7,48,48,48,48,0,48,48,- 1,- 1,- 1,0,- 1,
43,- 1,- 1,42,- 1,- 1,- 1,- 1,9,7,- 1,7,7,- 1,8,9,- 1,- 1,9,5,6,5,5,- 1,4,4,4,6,6,5,5,- 1,- 1,
- 1,9,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,
- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,2,- 1,- 1,2,1,1,- 1,- 1,- 1,- 1,- 1,- 1,- 1};const int Cyc_lex_default[
406]=(const int[406]){- 1,- 1,- 1,297,286,138,23,36,23,19,- 1,- 1,12,31,46,31,36,23,19,
0,- 1,0,0,0,0,- 1,0,- 1,0,- 1,- 1,0,- 1,- 1,- 1,0,0,0,- 1,0,40,- 1,- 1,0,- 1,- 1,0,- 1,- 1,0,- 1,
- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,
- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,0,- 1,0,- 1,- 1,- 1,- 1,- 1,- 1,0,- 1,0,103,- 1,- 1,
- 1,- 1,- 1,110,110,110,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,130,-
1,- 1,- 1,- 1,- 1,- 1,- 1,0,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,0,- 1,- 1,- 1,
- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,0,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,
- 1,- 1,- 1,- 1,0,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,
- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,0,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,
- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,0,- 1,- 1,- 1,0,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,-
1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,0,0,- 1,- 1,- 1,- 1,- 1,- 1,- 1,
- 1,- 1,0,- 1,0,- 1,0,- 1,- 1,366,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,0,0,0,- 1,0,
- 1,0,0,- 1,0,0,0,0,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,
- 1,- 1,0,0,- 1,- 1,0,0,0,0,0,0,0,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,0,
0,0,0,- 1,- 1,0,0,0,0,0,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1};const int Cyc_lex_trans[
3481]=(const int[3481]){0,0,0,0,0,0,0,0,0,0,22,19,28,398,19,28,19,28,100,19,45,45,
45,45,45,22,45,0,0,0,0,0,21,202,213,399,21,22,- 1,- 1,22,- 1,- 1,45,203,45,204,22,
396,396,396,396,396,396,396,396,396,396,31,103,22,214,114,40,227,396,396,396,396,
396,396,396,396,396,396,396,396,396,396,396,396,396,396,396,396,396,396,396,396,
396,396,31,245,240,235,396,130,396,396,396,396,396,396,396,396,396,396,396,396,
396,396,396,396,396,396,396,396,396,396,396,396,396,396,394,394,394,394,394,394,
394,394,394,394,121,20,74,67,54,55,56,394,394,394,394,394,394,394,394,394,394,
394,394,394,394,394,394,394,394,394,394,394,394,394,394,394,394,57,58,59,60,394,
61,394,394,394,394,394,394,394,394,394,394,394,394,394,394,394,394,394,394,394,
394,394,394,394,394,394,394,62,63,37,298,299,298,298,299,39,22,64,65,68,69,70,
129,34,34,34,34,34,34,34,34,71,72,298,300,301,75,76,302,303,304,104,104,305,306,
104,307,308,309,310,311,311,311,311,311,311,311,311,311,312,77,313,314,315,104,
19,316,316,316,316,316,316,316,316,316,316,316,316,316,316,316,316,316,316,316,
316,316,316,316,316,316,316,19,- 1,- 1,317,316,101,316,316,316,316,316,316,316,316,
316,316,316,316,316,316,316,316,316,316,316,316,316,316,316,316,316,316,287,318,
37,288,139,139,24,24,139,131,122,94,84,81,85,38,82,86,28,87,24,29,83,25,289,88,
95,139,96,22,26,26,21,21,115,116,117,140,118,119,120,142,191,192,26,35,35,35,35,
35,35,35,35,177,171,162,30,30,30,30,30,30,30,30,66,66,156,149,66,73,73,78,78,73,
150,78,151,152,153,154,66,66,290,155,66,157,158,66,132,123,141,39,73,22,78,159,
143,144,145,160,146,161,27,66,147,31,46,21,73,73,163,164,73,148,165,166,167,113,
113,113,113,113,113,113,113,113,113,- 1,32,- 1,- 1,73,- 1,22,113,113,113,113,113,113,
113,113,113,113,113,113,113,113,113,113,113,113,113,113,113,113,113,113,113,113,
- 1,168,- 1,- 1,113,- 1,113,113,113,113,113,113,113,113,113,113,113,113,113,113,113,
113,113,113,113,113,113,113,113,113,113,113,89,89,28,169,89,170,100,172,173,174,
97,97,104,104,97,175,104,112,112,176,178,112,179,89,180,105,105,80,80,105,19,80,
21,97,181,104,182,183,184,185,112,186,187,188,189,190,251,193,105,194,80,195,112,
112,91,196,112,197,198,21,21,21,106,107,106,106,106,106,106,106,106,106,106,106,
21,112,199,200,201,205,206,106,106,106,106,106,106,106,106,106,106,106,106,106,
106,106,106,106,106,106,106,106,106,106,106,106,106,207,208,209,210,106,211,106,
106,106,106,106,106,106,106,106,106,106,106,106,106,106,106,106,106,106,106,106,
106,106,106,106,106,47,47,35,212,47,215,216,33,33,33,33,33,33,33,33,33,33,92,92,
230,222,92,223,47,33,33,33,33,33,33,128,128,128,128,128,128,128,128,48,224,92,
225,- 1,226,- 1,228,229,99,99,231,49,99,232,233,234,93,33,33,33,33,33,33,35,35,35,
35,35,35,35,35,99,236,237,238,239,- 1,217,- 1,41,41,218,102,41,101,101,241,242,101,
243,219,50,220,244,91,246,51,52,247,248,249,250,41,53,252,253,268,101,137,137,
137,137,137,137,137,137,263,259,42,42,42,42,42,42,42,42,42,42,221,260,261,262,93,
36,28,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,
42,264,100,265,266,42,267,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,
42,42,42,42,42,42,42,42,269,270,43,271,272,102,281,33,33,33,33,33,33,33,33,33,33,
276,277,278,279,280,282,283,33,33,33,33,33,33,35,35,35,35,35,35,35,35,284,285,
392,391,365,356,331,44,44,44,44,44,44,44,44,44,44,21,33,33,33,33,33,33,44,44,44,
44,44,44,44,44,44,44,44,44,44,44,44,44,44,44,44,44,44,44,44,44,44,44,327,328,329,
326,44,321,44,44,44,44,44,44,44,44,44,44,44,44,44,44,44,44,44,44,44,44,44,44,44,
44,44,44,44,44,44,44,44,44,44,44,44,44,323,324,325,28,330,359,100,44,44,44,44,44,
44,44,44,44,44,44,44,44,44,44,44,44,44,44,44,44,44,44,44,44,44,388,254,155,138,
44,100,44,44,44,44,44,44,44,44,44,44,44,44,44,44,44,44,44,44,44,44,44,44,44,44,
44,44,78,78,389,363,78,- 1,253,253,190,102,253,295,295,295,295,295,295,295,295,
319,250,364,100,78,229,390,46,273,273,253,384,273,255,255,- 1,360,255,254,176,79,
79,79,79,79,79,79,79,79,79,383,273,361,362,35,100,255,79,79,79,79,79,79,79,79,79,
79,79,79,79,79,79,79,79,79,79,79,79,79,79,79,79,79,320,385,21,35,79,367,79,79,79,
79,79,79,79,79,79,79,79,79,79,79,79,79,79,79,79,79,79,79,79,79,79,79,31,80,80,
274,400,80,267,267,256,401,267,402,393,275,393,393,403,404,257,35,31,31,36,258,
80,405,21,255,255,267,0,255,273,273,0,393,273,19,0,0,79,79,79,79,79,79,79,79,79,
79,255,35,31,31,36,273,36,79,79,79,79,79,79,79,79,79,79,79,79,79,79,79,79,79,79,
79,79,79,79,79,79,79,79,0,0,0,355,79,36,79,79,79,79,79,79,79,79,79,79,79,79,79,
79,79,79,79,79,79,79,79,79,79,79,79,79,89,89,256,355,89,272,272,274,0,272,296,
296,257,0,296,0,0,275,355,0,0,0,- 1,89,0,0,296,296,272,393,296,393,393,296,0,21,
31,0,0,90,90,90,90,90,90,90,90,90,90,296,355,21,393,0,0,0,90,90,90,90,90,90,90,
90,90,90,90,90,90,90,90,90,90,90,90,90,90,90,90,90,90,90,0,0,0,0,90,0,90,90,90,
90,90,90,90,90,90,90,90,90,90,90,90,90,90,90,90,90,90,90,90,90,90,90,91,92,92,
280,280,92,0,280,285,285,0,0,285,31,31,31,31,31,31,31,31,0,0,0,92,0,280,0,0,0,0,
285,0,0,28,0,0,0,0,35,90,90,90,90,90,90,90,90,90,90,0,0,0,0,0,0,0,90,90,90,90,90,
90,90,90,90,90,90,90,90,90,90,90,90,90,90,90,90,90,90,90,90,90,0,0,0,0,90,0,90,
90,90,90,90,90,90,90,90,90,90,90,90,90,90,90,90,90,90,90,90,90,90,90,90,90,97,97,
0,357,97,358,358,358,358,358,358,358,358,358,358,0,0,0,0,337,0,337,0,97,338,338,
338,338,338,338,338,338,338,338,0,0,0,0,0,98,98,98,98,98,98,98,98,98,98,0,0,0,0,
0,0,0,98,98,98,98,98,98,98,98,98,98,98,98,98,98,98,98,98,98,98,98,98,98,98,98,98,
98,0,0,0,0,98,0,98,98,98,98,98,98,98,98,98,98,98,98,98,98,98,98,98,98,98,98,98,
98,98,98,98,98,36,99,99,0,0,99,338,338,338,338,338,338,338,338,338,338,0,0,0,0,
341,0,341,0,99,342,342,342,342,342,342,342,342,342,342,0,0,0,0,0,98,98,98,98,98,
98,98,98,98,98,0,0,0,0,0,0,0,98,98,98,98,98,98,98,98,98,98,98,98,98,98,98,98,98,
98,98,98,98,98,98,98,98,98,0,0,0,0,98,0,98,98,98,98,98,98,98,98,98,98,98,98,98,
98,98,98,98,98,98,98,98,98,98,98,98,98,108,108,108,108,108,108,108,108,108,108,
108,108,22,0,0,0,0,0,0,108,108,108,108,108,108,108,108,108,108,108,108,108,108,
108,108,108,108,108,108,108,108,108,108,108,108,0,0,0,0,108,0,108,108,108,108,
108,108,108,108,108,108,108,108,108,108,108,108,108,108,108,108,108,108,108,108,
108,108,108,109,108,108,108,108,108,108,108,108,108,108,22,0,0,0,0,0,0,108,108,
108,108,108,108,108,108,108,108,108,108,108,108,108,108,108,108,108,108,108,108,
108,108,108,108,0,0,0,0,108,0,108,108,108,108,108,108,108,108,108,108,108,108,
108,108,108,108,108,108,108,108,108,108,108,108,108,108,108,108,108,108,108,108,
108,108,108,108,108,108,22,0,0,0,0,0,0,108,108,108,108,108,108,108,108,108,108,
108,108,108,108,108,108,108,108,108,108,108,108,108,108,108,108,- 1,0,0,- 1,108,0,
108,108,108,108,108,108,108,108,108,108,108,108,108,108,108,108,108,108,108,108,
108,108,108,108,108,108,0,0,0,0,109,109,109,109,109,109,109,109,109,109,109,109,
111,0,0,0,0,0,0,109,109,109,109,109,109,109,109,109,109,109,109,109,109,109,109,
109,109,109,109,109,109,109,109,109,109,0,0,0,0,109,0,109,109,109,109,109,109,
109,109,109,109,109,109,109,109,109,109,109,109,109,109,109,109,109,109,109,109,
113,113,113,113,113,113,113,113,113,113,0,0,0,0,0,0,0,113,113,113,113,113,113,
113,113,113,113,113,113,113,113,113,113,113,113,113,113,113,113,113,113,113,113,
28,0,0,124,113,0,113,113,113,113,113,113,113,113,113,113,113,113,113,113,113,113,
113,113,113,113,113,113,113,113,113,113,0,0,28,0,0,133,125,125,125,125,125,125,
125,125,127,127,127,127,127,127,127,127,127,127,0,0,0,0,0,0,0,127,127,127,127,
127,127,0,0,0,134,134,134,134,134,134,134,134,0,0,0,0,0,0,0,31,0,0,- 1,0,385,0,0,
127,127,127,127,127,127,386,386,386,386,386,386,386,386,0,126,127,127,127,127,
127,127,127,127,127,127,31,0,0,0,0,0,0,127,127,127,127,127,127,136,136,136,136,
136,136,136,136,136,136,135,0,0,0,0,0,0,136,136,136,136,136,136,0,0,0,127,127,
127,127,127,127,19,0,0,291,0,0,136,136,136,136,136,136,136,136,136,136,0,136,136,
136,136,136,136,136,136,136,136,136,136,0,0,0,0,0,0,0,0,385,292,292,292,292,292,
292,292,292,387,387,387,387,387,387,387,387,0,136,136,136,136,136,136,0,294,294,
294,294,294,294,294,294,294,294,0,0,0,0,0,0,0,294,294,294,294,294,294,0,0,28,342,
342,342,342,342,342,342,342,342,342,0,0,294,294,294,294,294,294,294,294,294,294,
293,294,294,294,294,294,294,294,294,294,294,294,294,0,0,332,0,343,343,343,343,
343,343,343,343,344,344,0,0,0,0,0,0,0,0,0,0,0,334,294,294,294,294,294,294,345,0,
0,0,0,0,0,0,0,346,0,0,347,332,0,333,333,333,333,333,333,333,333,333,333,334,0,0,
0,0,0,0,345,0,0,0,334,0,0,0,0,346,0,335,347,0,0,0,0,0,0,0,336,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,334,0,0,0,0,0,0,335,0,0,0,0,0,0,0,0,336,322,322,322,322,322,322,322,
322,322,322,0,0,0,0,0,0,0,322,322,322,322,322,322,322,322,322,322,322,322,322,
322,322,322,322,322,322,322,322,322,322,322,322,322,0,0,0,0,322,0,322,322,322,
322,322,322,322,322,322,322,322,322,322,322,322,322,322,322,322,322,322,322,322,
322,322,322,322,322,322,322,322,322,322,322,322,322,0,0,0,0,0,0,0,322,322,322,
322,322,322,322,322,322,322,322,322,322,322,322,322,322,322,322,322,322,322,322,
322,322,322,0,0,0,0,322,0,322,322,322,322,322,322,322,322,322,322,322,322,322,
322,322,322,322,322,322,322,322,322,322,322,322,322,339,339,339,339,339,339,339,
339,339,339,0,0,0,0,0,0,0,0,0,0,0,340,93,0,0,0,0,332,93,333,333,333,333,333,333,
333,333,333,333,338,338,338,338,338,338,338,338,338,338,0,334,0,0,340,93,0,0,335,
0,0,93,91,0,0,0,0,336,91,0,339,339,339,339,339,339,339,339,339,339,0,0,0,334,0,0,
0,0,0,0,335,340,93,0,91,0,0,0,93,336,91,0,0,342,342,342,342,342,342,342,342,342,
342,0,0,0,0,0,0,0,0,0,0,340,93,93,0,0,0,0,93,93,332,0,343,343,343,343,343,343,
343,343,344,344,0,0,0,0,0,0,0,0,0,0,0,334,0,93,0,0,0,0,353,93,0,0,0,0,0,0,332,
354,344,344,344,344,344,344,344,344,344,344,0,0,0,0,0,334,0,0,0,0,0,334,353,0,0,
0,0,0,351,0,0,354,0,0,0,0,0,352,0,0,358,358,358,358,358,358,358,358,358,358,0,0,
0,334,0,0,0,0,0,0,351,340,93,0,0,0,0,0,93,352,348,348,348,348,348,348,348,348,
348,348,0,0,0,0,0,0,0,348,348,348,348,348,348,340,93,0,0,0,0,0,93,0,0,0,0,0,0,0,
348,348,348,348,348,348,348,348,348,348,0,348,348,348,348,348,348,348,348,348,
348,348,348,0,0,0,368,0,349,0,0,369,0,0,0,0,0,350,0,0,370,370,370,370,370,370,
370,370,0,348,348,348,348,348,348,371,0,0,0,0,349,0,0,0,0,0,0,0,0,350,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,372,0,0,0,0,373,374,0,0,0,375,0,0,0,0,0,0,0,376,0,0,0,377,0,
378,0,379,0,380,381,381,381,381,381,381,381,381,381,381,0,0,0,0,0,0,0,381,381,
381,381,381,381,381,381,381,381,381,381,381,381,381,381,381,381,381,381,381,381,
381,381,381,381,0,0,0,0,0,0,381,381,381,381,381,381,381,381,381,381,381,381,381,
381,381,381,381,381,381,381,381,381,381,381,381,381,382,0,0,0,0,0,0,0,0,381,381,
381,381,381,381,381,381,381,381,0,0,0,0,0,0,0,381,381,381,381,381,381,381,381,
381,381,381,381,381,381,381,381,381,381,381,381,381,381,381,381,381,381,0,0,0,0,
0,0,381,381,381,381,381,381,381,381,381,381,381,381,381,381,381,381,381,381,381,
381,381,381,381,381,381,381,22,0,0,395,0,0,0,394,394,394,394,394,394,394,394,394,
394,0,0,0,0,0,0,0,394,394,394,394,394,394,394,394,394,394,394,394,394,394,394,
394,394,394,394,394,394,394,394,394,394,394,0,0,0,0,394,0,394,394,394,394,394,
394,394,394,394,394,394,394,394,394,394,394,394,394,394,394,394,394,394,394,394,
394,397,0,0,0,0,0,0,0,396,396,396,396,396,396,396,396,396,396,0,0,0,0,0,0,0,396,
396,396,396,396,396,396,396,396,396,396,396,396,396,396,396,396,396,396,396,396,
396,396,396,396,396,0,0,0,0,396,0,396,396,396,396,396,396,396,396,396,396,396,
396,396,396,396,396,396,396,396,396,396,396,396,396,396,396,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0};const int Cyc_lex_check[3481]=(const int[3481]){- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,
- 1,0,25,29,0,122,124,131,133,288,291,41,41,45,45,41,398,45,- 1,- 1,- 1,- 1,- 1,120,
201,212,0,395,10,12,40,10,12,40,41,202,45,203,20,1,1,1,1,1,1,1,1,1,1,38,48,121,
213,10,38,226,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,129,217,218,
219,1,129,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,2,2,2,2,2,2,2,2,2,
2,9,18,51,52,53,54,55,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,56,57,
58,59,2,60,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,61,62,7,3,3,3,3,3,
7,7,63,64,67,68,69,7,30,30,30,30,30,30,30,30,70,71,3,3,3,74,75,3,3,3,47,47,3,3,
47,3,3,3,3,3,3,3,3,3,3,3,3,3,3,76,3,3,3,47,0,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,
3,3,3,3,3,3,3,3,10,12,40,3,3,81,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,
3,3,4,3,16,4,5,5,6,8,5,6,8,82,83,50,84,16,50,85,27,86,17,27,50,17,4,87,94,5,95,5,
6,8,9,18,114,115,116,5,117,118,119,141,143,143,17,34,34,34,34,34,34,34,34,144,
145,146,27,27,27,27,27,27,27,27,65,65,147,148,65,72,72,77,77,72,149,77,150,151,
152,153,66,66,4,154,66,156,157,65,6,8,5,16,72,16,77,158,142,142,142,159,142,160,
17,66,142,27,161,7,73,73,162,163,73,142,164,165,166,11,11,11,11,11,11,11,11,11,
11,103,27,110,103,73,110,3,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,
11,11,11,11,11,11,11,11,111,167,130,111,11,130,11,11,11,11,11,11,11,11,11,11,11,
11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,88,88,66,168,88,169,170,171,172,173,
96,96,104,104,96,174,104,105,105,175,177,105,178,88,179,13,13,80,80,13,73,80,16,
96,180,104,181,182,183,184,105,185,186,187,188,189,191,192,13,193,80,194,112,112,
4,195,112,196,197,5,6,8,13,13,13,13,13,13,13,13,13,13,13,13,17,112,198,199,200,
204,205,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,
13,13,206,207,208,209,13,210,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,
13,13,13,13,13,13,13,13,13,14,14,80,211,14,214,215,32,32,32,32,32,32,32,32,32,32,
92,92,220,221,92,222,14,32,32,32,32,32,32,125,125,125,125,125,125,125,125,14,223,
92,224,103,225,110,227,228,99,99,230,14,99,231,232,233,234,32,32,32,32,32,32,128,
128,128,128,128,128,128,128,99,235,236,237,238,111,216,130,15,15,216,239,15,101,
101,240,241,101,242,216,14,216,243,244,245,14,14,246,247,248,249,15,14,251,252,
256,101,134,134,134,134,134,134,134,134,257,258,15,15,15,15,15,15,15,15,15,15,
216,259,260,261,92,262,13,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,
15,15,15,15,15,15,15,15,263,99,264,265,15,266,15,15,15,15,15,15,15,15,15,15,15,
15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,268,269,15,270,271,101,274,33,33,33,
33,33,33,33,33,33,33,275,276,277,278,279,281,282,33,33,33,33,33,33,137,137,137,
137,137,137,137,137,283,284,300,302,305,309,312,42,42,42,42,42,42,42,42,42,42,14,
33,33,33,33,33,33,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,
42,42,42,42,42,313,313,313,314,42,317,42,42,42,42,42,42,42,42,42,42,42,42,42,42,
42,42,42,42,42,42,42,42,42,42,42,42,44,44,44,44,44,44,44,44,44,44,315,315,324,15,
327,357,335,44,44,44,44,44,44,44,44,44,44,44,44,44,44,44,44,44,44,44,44,44,44,44,
44,44,44,366,368,369,371,44,335,44,44,44,44,44,44,44,44,44,44,44,44,44,44,44,44,
44,44,44,44,44,44,44,44,44,44,78,78,303,306,78,304,253,253,372,373,253,292,292,
292,292,292,292,292,292,318,374,306,336,78,375,303,376,139,139,253,377,139,140,
140,304,307,140,253,378,78,78,78,78,78,78,78,78,78,78,379,139,307,307,345,336,
140,78,78,78,78,78,78,78,78,78,78,78,78,78,78,78,78,78,78,78,78,78,78,78,78,78,
78,318,387,397,345,78,304,78,78,78,78,78,78,78,78,78,78,78,78,78,78,78,78,78,78,
78,78,78,78,78,78,78,78,78,79,79,139,399,79,267,267,140,400,267,401,298,139,298,
298,402,403,140,346,349,350,351,140,79,404,405,255,255,267,- 1,255,273,273,- 1,298,
273,267,- 1,- 1,79,79,79,79,79,79,79,79,79,79,255,346,349,350,351,273,352,79,79,79,
79,79,79,79,79,79,79,79,79,79,79,79,79,79,79,79,79,79,79,79,79,79,79,- 1,- 1,- 1,
353,79,352,79,79,79,79,79,79,79,79,79,79,79,79,79,79,79,79,79,79,79,79,79,79,79,
79,79,79,89,89,255,353,89,272,272,273,- 1,272,289,289,255,- 1,289,- 1,- 1,273,354,- 1,
- 1,- 1,304,89,- 1,- 1,296,296,272,393,296,393,393,289,- 1,289,272,- 1,- 1,89,89,89,89,
89,89,89,89,89,89,296,354,296,393,- 1,- 1,- 1,89,89,89,89,89,89,89,89,89,89,89,89,
89,89,89,89,89,89,89,89,89,89,89,89,89,89,- 1,- 1,- 1,- 1,89,- 1,89,89,89,89,89,89,89,
89,89,89,89,89,89,89,89,89,89,89,89,89,89,89,89,89,89,89,89,90,90,280,280,90,- 1,
280,285,285,- 1,- 1,285,295,295,295,295,295,295,295,295,- 1,- 1,- 1,90,- 1,280,- 1,- 1,-
1,- 1,285,- 1,- 1,280,- 1,- 1,- 1,- 1,285,90,90,90,90,90,90,90,90,90,90,- 1,- 1,- 1,- 1,- 1,
- 1,- 1,90,90,90,90,90,90,90,90,90,90,90,90,90,90,90,90,90,90,90,90,90,90,90,90,90,
90,- 1,- 1,- 1,- 1,90,- 1,90,90,90,90,90,90,90,90,90,90,90,90,90,90,90,90,90,90,90,90,
90,90,90,90,90,90,97,97,- 1,308,97,308,308,308,308,308,308,308,308,308,308,- 1,- 1,
- 1,- 1,334,- 1,334,- 1,97,334,334,334,334,334,334,334,334,334,334,- 1,- 1,- 1,- 1,- 1,97,
97,97,97,97,97,97,97,97,97,- 1,- 1,- 1,- 1,- 1,- 1,- 1,97,97,97,97,97,97,97,97,97,97,97,
97,97,97,97,97,97,97,97,97,97,97,97,97,97,97,- 1,- 1,- 1,- 1,97,- 1,97,97,97,97,97,97,
97,97,97,97,97,97,97,97,97,97,97,97,97,97,97,97,97,97,97,97,97,98,98,- 1,- 1,98,
337,337,337,337,337,337,337,337,337,337,- 1,- 1,- 1,- 1,340,- 1,340,- 1,98,340,340,340,
340,340,340,340,340,340,340,- 1,- 1,- 1,- 1,- 1,98,98,98,98,98,98,98,98,98,98,- 1,- 1,-
1,- 1,- 1,- 1,- 1,98,98,98,98,98,98,98,98,98,98,98,98,98,98,98,98,98,98,98,98,98,98,
98,98,98,98,- 1,- 1,- 1,- 1,98,- 1,98,98,98,98,98,98,98,98,98,98,98,98,98,98,98,98,98,
98,98,98,98,98,98,98,98,98,106,106,106,106,106,106,106,106,106,106,106,106,106,-
1,- 1,- 1,- 1,- 1,- 1,106,106,106,106,106,106,106,106,106,106,106,106,106,106,106,106,
106,106,106,106,106,106,106,106,106,106,- 1,- 1,- 1,- 1,106,- 1,106,106,106,106,106,
106,106,106,106,106,106,106,106,106,106,106,106,106,106,106,106,106,106,106,106,
106,107,107,107,107,107,107,107,107,107,107,107,107,107,- 1,- 1,- 1,- 1,- 1,- 1,107,
107,107,107,107,107,107,107,107,107,107,107,107,107,107,107,107,107,107,107,107,
107,107,107,107,107,- 1,- 1,- 1,- 1,107,- 1,107,107,107,107,107,107,107,107,107,107,
107,107,107,107,107,107,107,107,107,107,107,107,107,107,107,107,108,108,108,108,
108,108,108,108,108,108,108,108,108,- 1,- 1,- 1,- 1,- 1,- 1,108,108,108,108,108,108,
108,108,108,108,108,108,108,108,108,108,108,108,108,108,108,108,108,108,108,108,
109,- 1,- 1,109,108,- 1,108,108,108,108,108,108,108,108,108,108,108,108,108,108,108,
108,108,108,108,108,108,108,108,108,108,108,- 1,- 1,- 1,- 1,109,109,109,109,109,109,
109,109,109,109,109,109,109,- 1,- 1,- 1,- 1,- 1,- 1,109,109,109,109,109,109,109,109,
109,109,109,109,109,109,109,109,109,109,109,109,109,109,109,109,109,109,- 1,- 1,- 1,
- 1,109,- 1,109,109,109,109,109,109,109,109,109,109,109,109,109,109,109,109,109,
109,109,109,109,109,109,109,109,109,113,113,113,113,113,113,113,113,113,113,- 1,-
1,- 1,- 1,- 1,- 1,- 1,113,113,113,113,113,113,113,113,113,113,113,113,113,113,113,113,
113,113,113,113,113,113,113,113,113,113,123,- 1,- 1,123,113,- 1,113,113,113,113,113,
113,113,113,113,113,113,113,113,113,113,113,113,113,113,113,113,113,113,113,113,
113,- 1,- 1,132,- 1,- 1,132,123,123,123,123,123,123,123,123,126,126,126,126,126,126,
126,126,126,126,- 1,- 1,- 1,- 1,- 1,- 1,- 1,126,126,126,126,126,126,- 1,- 1,- 1,132,132,
132,132,132,132,132,132,- 1,- 1,- 1,- 1,- 1,- 1,- 1,123,- 1,- 1,109,- 1,370,- 1,- 1,126,126,
126,126,126,126,370,370,370,370,370,370,370,370,- 1,123,127,127,127,127,127,127,
127,127,127,127,132,- 1,- 1,- 1,- 1,- 1,- 1,127,127,127,127,127,127,135,135,135,135,
135,135,135,135,135,135,132,- 1,- 1,- 1,- 1,- 1,- 1,135,135,135,135,135,135,- 1,- 1,- 1,
127,127,127,127,127,127,290,- 1,- 1,290,- 1,- 1,136,136,136,136,136,136,136,136,136,
136,- 1,135,135,135,135,135,135,136,136,136,136,136,136,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,
386,290,290,290,290,290,290,290,290,386,386,386,386,386,386,386,386,- 1,136,136,
136,136,136,136,- 1,293,293,293,293,293,293,293,293,293,293,- 1,- 1,- 1,- 1,- 1,- 1,- 1,
293,293,293,293,293,293,- 1,- 1,290,341,341,341,341,341,341,341,341,341,341,- 1,- 1,
294,294,294,294,294,294,294,294,294,294,290,293,293,293,293,293,293,294,294,294,
294,294,294,- 1,- 1,310,- 1,310,310,310,310,310,310,310,310,310,310,- 1,- 1,- 1,- 1,- 1,
- 1,- 1,- 1,- 1,- 1,- 1,310,294,294,294,294,294,294,310,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,310,- 1,
- 1,310,311,- 1,311,311,311,311,311,311,311,311,311,311,310,- 1,- 1,- 1,- 1,- 1,- 1,310,
- 1,- 1,- 1,311,- 1,- 1,- 1,- 1,310,- 1,311,310,- 1,- 1,- 1,- 1,- 1,- 1,- 1,311,- 1,- 1,- 1,- 1,- 1,
- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,311,- 1,- 1,- 1,- 1,- 1,- 1,311,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,
311,316,316,316,316,316,316,316,316,316,316,- 1,- 1,- 1,- 1,- 1,- 1,- 1,316,316,316,316,
316,316,316,316,316,316,316,316,316,316,316,316,316,316,316,316,316,316,316,316,
316,316,- 1,- 1,- 1,- 1,316,- 1,316,316,316,316,316,316,316,316,316,316,316,316,316,
316,316,316,316,316,316,316,316,316,316,316,316,316,322,322,322,322,322,322,322,
322,322,322,- 1,- 1,- 1,- 1,- 1,- 1,- 1,322,322,322,322,322,322,322,322,322,322,322,322,
322,322,322,322,322,322,322,322,322,322,322,322,322,322,- 1,- 1,- 1,- 1,322,- 1,322,
322,322,322,322,322,322,322,322,322,322,322,322,322,322,322,322,322,322,322,322,
322,322,322,322,322,332,332,332,332,332,332,332,332,332,332,- 1,- 1,- 1,- 1,- 1,- 1,- 1,
- 1,- 1,- 1,- 1,332,332,- 1,- 1,- 1,- 1,333,332,333,333,333,333,333,333,333,333,333,333,
338,338,338,338,338,338,338,338,338,338,- 1,333,- 1,- 1,332,332,- 1,- 1,333,- 1,- 1,332,
338,- 1,- 1,- 1,- 1,333,338,- 1,339,339,339,339,339,339,339,339,339,339,- 1,- 1,- 1,333,
- 1,- 1,- 1,- 1,- 1,- 1,333,339,339,- 1,338,- 1,- 1,- 1,339,333,338,- 1,- 1,342,342,342,342,
342,342,342,342,342,342,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,339,339,342,- 1,- 1,- 1,- 1,
339,342,343,- 1,343,343,343,343,343,343,343,343,343,343,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,-
1,- 1,- 1,343,- 1,342,- 1,- 1,- 1,- 1,343,342,- 1,- 1,- 1,- 1,- 1,- 1,344,343,344,344,344,344,
344,344,344,344,344,344,- 1,- 1,- 1,- 1,- 1,343,- 1,- 1,- 1,- 1,- 1,344,343,- 1,- 1,- 1,- 1,- 1,
344,- 1,- 1,343,- 1,- 1,- 1,- 1,- 1,344,- 1,- 1,358,358,358,358,358,358,358,358,358,358,-
1,- 1,- 1,344,- 1,- 1,- 1,- 1,- 1,- 1,344,358,358,- 1,- 1,- 1,- 1,- 1,358,344,347,347,347,347,
347,347,347,347,347,347,- 1,- 1,- 1,- 1,- 1,- 1,- 1,347,347,347,347,347,347,358,358,- 1,
- 1,- 1,- 1,- 1,358,- 1,- 1,- 1,- 1,- 1,- 1,- 1,348,348,348,348,348,348,348,348,348,348,- 1,
347,347,347,347,347,347,348,348,348,348,348,348,- 1,- 1,- 1,367,- 1,348,- 1,- 1,367,- 1,
- 1,- 1,- 1,- 1,348,- 1,- 1,367,367,367,367,367,367,367,367,- 1,348,348,348,348,348,348,
367,- 1,- 1,- 1,- 1,348,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,348,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,
- 1,- 1,- 1,367,- 1,- 1,- 1,- 1,367,367,- 1,- 1,- 1,367,- 1,- 1,- 1,- 1,- 1,- 1,- 1,367,- 1,- 1,- 1,
367,- 1,367,- 1,367,- 1,367,380,380,380,380,380,380,380,380,380,380,- 1,- 1,- 1,- 1,- 1,
- 1,- 1,380,380,380,380,380,380,380,380,380,380,380,380,380,380,380,380,380,380,
380,380,380,380,380,380,380,380,- 1,- 1,- 1,- 1,- 1,- 1,380,380,380,380,380,380,380,
380,380,380,380,380,380,380,380,380,380,380,380,380,380,380,380,380,380,380,381,
- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,381,381,381,381,381,381,381,381,381,381,- 1,- 1,- 1,- 1,- 1,-
1,- 1,381,381,381,381,381,381,381,381,381,381,381,381,381,381,381,381,381,381,381,
381,381,381,381,381,381,381,- 1,- 1,- 1,- 1,- 1,- 1,381,381,381,381,381,381,381,381,
381,381,381,381,381,381,381,381,381,381,381,381,381,381,381,381,381,381,394,- 1,-
1,394,- 1,- 1,- 1,394,394,394,394,394,394,394,394,394,394,- 1,- 1,- 1,- 1,- 1,- 1,- 1,394,
394,394,394,394,394,394,394,394,394,394,394,394,394,394,394,394,394,394,394,394,
394,394,394,394,394,- 1,- 1,- 1,- 1,394,- 1,394,394,394,394,394,394,394,394,394,394,
394,394,394,394,394,394,394,394,394,394,394,394,394,394,394,394,396,- 1,- 1,- 1,- 1,
- 1,- 1,- 1,396,396,396,396,396,396,396,396,396,396,- 1,- 1,- 1,- 1,- 1,- 1,- 1,396,396,
396,396,396,396,396,396,396,396,396,396,396,396,396,396,396,396,396,396,396,396,
396,396,396,396,- 1,- 1,- 1,- 1,396,- 1,396,396,396,396,396,396,396,396,396,396,396,
396,396,396,396,396,396,396,396,396,396,396,396,396,396,396,- 1,- 1,- 1,- 1,- 1,- 1,- 1,
- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,
- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,
- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,
- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,
- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1};int Cyc_lex_engine(int
start_state,struct Cyc_Lexing_lexbuf*lbuf);int Cyc_lex_engine(int start_state,
struct Cyc_Lexing_lexbuf*lbuf){int state;int base;int backtrk;int c;state=start_state;
if(state >= 0){lbuf->lex_last_pos=(lbuf->lex_start_pos=lbuf->lex_curr_pos);lbuf->lex_last_action=
- 1;}else{state=(- state)- 1;}while(1){base=Cyc_lex_base[
_check_known_subscript_notnull(406,state)];if(base < 0)return(- base)- 1;backtrk=
Cyc_lex_backtrk[_check_known_subscript_notnull(406,state)];if(backtrk >= 0){lbuf->lex_last_pos=
lbuf->lex_curr_pos;lbuf->lex_last_action=backtrk;}if(lbuf->lex_curr_pos >= lbuf->lex_buffer_len){
if(!lbuf->lex_eof_reached)return(- state)- 1;else{c=256;}}else{c=(int)*((char*)
_check_dyneither_subscript(lbuf->lex_buffer,sizeof(char),lbuf->lex_curr_pos ++));
if(c == - 1)c=256;}if(Cyc_lex_check[_check_known_subscript_notnull(3481,base + c)]
== state)state=Cyc_lex_trans[_check_known_subscript_notnull(3481,base + c)];else{
state=Cyc_lex_default[_check_known_subscript_notnull(406,state)];}if(state < 0){
lbuf->lex_curr_pos=lbuf->lex_last_pos;if(lbuf->lex_last_action == - 1){struct Cyc_Lexing_Error_struct
_tmp32C;const char*_tmp32B;struct Cyc_Lexing_Error_struct*_tmp32A;(int)_throw((
void*)((_tmp32A=_cycalloc(sizeof(*_tmp32A)),((_tmp32A[0]=((_tmp32C.tag=Cyc_Lexing_Error,((
_tmp32C.f1=((_tmp32B="empty token",_tag_dyneither(_tmp32B,sizeof(char),12))),
_tmp32C)))),_tmp32A)))));}else{return lbuf->lex_last_action;}}else{if(c == 256)
lbuf->lex_eof_reached=0;}}}struct _tuple12*Cyc_line_rec(struct Cyc_Lexing_lexbuf*
lexbuf,int lexstate);struct _tuple12*Cyc_line_rec(struct Cyc_Lexing_lexbuf*lexbuf,
int lexstate){lexstate=Cyc_lex_engine(lexstate,lexbuf);switch(lexstate){case 0:
_LL0: Cyc_macroname(lexbuf);for(0;Cyc_current_args != 0;Cyc_current_args=((struct
Cyc_List_List*)_check_null(Cyc_current_args))->tl){struct Cyc_Set_Set**_tmp32D;
Cyc_current_targets=((_tmp32D=_cycalloc(sizeof(*_tmp32D)),((_tmp32D[0]=((struct
Cyc_Set_Set*(*)(struct Cyc_Set_Set*s,struct _dyneither_ptr*elt))Cyc_Set_delete)(*((
struct Cyc_Set_Set**)_check_null(Cyc_current_targets)),(struct _dyneither_ptr*)((
struct Cyc_List_List*)_check_null(Cyc_current_args))->hd),_tmp32D))));}{struct
_tuple12*_tmp32E;return(_tmp32E=_cycalloc(sizeof(*_tmp32E)),((_tmp32E->f1=(
struct _dyneither_ptr*)_check_null(Cyc_current_source),((_tmp32E->f2=*((struct Cyc_Set_Set**)
_check_null(Cyc_current_targets)),_tmp32E)))));}case 1: _LL1: return Cyc_line(lexbuf);
case 2: _LL2: return 0;default: _LL3:(lexbuf->refill_buff)(lexbuf);return Cyc_line_rec(
lexbuf,lexstate);}{struct Cyc_Lexing_Error_struct _tmp334;const char*_tmp333;struct
Cyc_Lexing_Error_struct*_tmp332;(int)_throw((void*)((_tmp332=_cycalloc(sizeof(*
_tmp332)),((_tmp332[0]=((_tmp334.tag=Cyc_Lexing_Error,((_tmp334.f1=((_tmp333="some action didn't return!",
_tag_dyneither(_tmp333,sizeof(char),27))),_tmp334)))),_tmp332)))));}}struct
_tuple12*Cyc_line(struct Cyc_Lexing_lexbuf*lexbuf);struct _tuple12*Cyc_line(struct
Cyc_Lexing_lexbuf*lexbuf){return Cyc_line_rec(lexbuf,0);}int Cyc_macroname_rec(
struct Cyc_Lexing_lexbuf*lexbuf,int lexstate);int Cyc_macroname_rec(struct Cyc_Lexing_lexbuf*
lexbuf,int lexstate){lexstate=Cyc_lex_engine(lexstate,lexbuf);switch(lexstate){
case 0: _LL5:{struct _dyneither_ptr*_tmp335;Cyc_current_source=((_tmp335=_cycalloc(
sizeof(*_tmp335)),((_tmp335[0]=(struct _dyneither_ptr)Cyc_substring((struct
_dyneither_ptr)Cyc_Lexing_lexeme(lexbuf),0,(unsigned long)((Cyc_Lexing_lexeme_end(
lexbuf)- Cyc_Lexing_lexeme_start(lexbuf))- 2)),_tmp335))));}Cyc_current_args=0;{
struct Cyc_Set_Set**_tmp336;Cyc_current_targets=((_tmp336=_cycalloc(sizeof(*
_tmp336)),((_tmp336[0]=((struct Cyc_Set_Set*(*)(int(*cmp)(struct _dyneither_ptr*,
struct _dyneither_ptr*)))Cyc_Set_empty)(Cyc_strptrcmp),_tmp336))));}Cyc_token(
lexbuf);return 0;case 1: _LL6:{struct _dyneither_ptr*_tmp337;Cyc_current_source=((
_tmp337=_cycalloc(sizeof(*_tmp337)),((_tmp337[0]=(struct _dyneither_ptr)Cyc_substring((
struct _dyneither_ptr)Cyc_Lexing_lexeme(lexbuf),0,(unsigned long)((Cyc_Lexing_lexeme_end(
lexbuf)- Cyc_Lexing_lexeme_start(lexbuf))- 1)),_tmp337))));}Cyc_current_args=0;{
struct Cyc_Set_Set**_tmp338;Cyc_current_targets=((_tmp338=_cycalloc(sizeof(*
_tmp338)),((_tmp338[0]=((struct Cyc_Set_Set*(*)(int(*cmp)(struct _dyneither_ptr*,
struct _dyneither_ptr*)))Cyc_Set_empty)(Cyc_strptrcmp),_tmp338))));}Cyc_args(
lexbuf);return 0;case 2: _LL7:{struct _dyneither_ptr*_tmp339;Cyc_current_source=((
_tmp339=_cycalloc(sizeof(*_tmp339)),((_tmp339[0]=(struct _dyneither_ptr)Cyc_Lexing_lexeme(
lexbuf),_tmp339))));}Cyc_current_args=0;{struct Cyc_Set_Set**_tmp33A;Cyc_current_targets=((
_tmp33A=_cycalloc(sizeof(*_tmp33A)),((_tmp33A[0]=((struct Cyc_Set_Set*(*)(int(*
cmp)(struct _dyneither_ptr*,struct _dyneither_ptr*)))Cyc_Set_empty)(Cyc_strptrcmp),
_tmp33A))));}Cyc_token(lexbuf);return 0;default: _LL8:(lexbuf->refill_buff)(lexbuf);
return Cyc_macroname_rec(lexbuf,lexstate);}{struct Cyc_Lexing_Error_struct _tmp340;
const char*_tmp33F;struct Cyc_Lexing_Error_struct*_tmp33E;(int)_throw((void*)((
_tmp33E=_cycalloc(sizeof(*_tmp33E)),((_tmp33E[0]=((_tmp340.tag=Cyc_Lexing_Error,((
_tmp340.f1=((_tmp33F="some action didn't return!",_tag_dyneither(_tmp33F,sizeof(
char),27))),_tmp340)))),_tmp33E)))));}}int Cyc_macroname(struct Cyc_Lexing_lexbuf*
lexbuf);int Cyc_macroname(struct Cyc_Lexing_lexbuf*lexbuf){return Cyc_macroname_rec(
lexbuf,1);}int Cyc_args_rec(struct Cyc_Lexing_lexbuf*lexbuf,int lexstate);int Cyc_args_rec(
struct Cyc_Lexing_lexbuf*lexbuf,int lexstate){lexstate=Cyc_lex_engine(lexstate,
lexbuf);switch(lexstate){case 0: _LLA: {struct _dyneither_ptr*_tmp341;struct
_dyneither_ptr*_tmp15=(_tmp341=_cycalloc(sizeof(*_tmp341)),((_tmp341[0]=(struct
_dyneither_ptr)Cyc_substring((struct _dyneither_ptr)Cyc_Lexing_lexeme(lexbuf),0,(
unsigned long)((Cyc_Lexing_lexeme_end(lexbuf)- Cyc_Lexing_lexeme_start(lexbuf))- 
2)),_tmp341)));{struct Cyc_List_List*_tmp342;Cyc_current_args=((_tmp342=_cycalloc(
sizeof(*_tmp342)),((_tmp342->hd=_tmp15,((_tmp342->tl=Cyc_current_args,_tmp342))))));}
return Cyc_args(lexbuf);}case 1: _LLB: {struct _dyneither_ptr*_tmp343;struct
_dyneither_ptr*_tmp18=(_tmp343=_cycalloc(sizeof(*_tmp343)),((_tmp343[0]=(struct
_dyneither_ptr)Cyc_substring((struct _dyneither_ptr)Cyc_Lexing_lexeme(lexbuf),0,(
unsigned long)((Cyc_Lexing_lexeme_end(lexbuf)- Cyc_Lexing_lexeme_start(lexbuf))- 
1)),_tmp343)));{struct Cyc_List_List*_tmp344;Cyc_current_args=((_tmp344=_cycalloc(
sizeof(*_tmp344)),((_tmp344->hd=_tmp18,((_tmp344->tl=Cyc_current_args,_tmp344))))));}
return Cyc_token(lexbuf);}default: _LLC:(lexbuf->refill_buff)(lexbuf);return Cyc_args_rec(
lexbuf,lexstate);}{struct Cyc_Lexing_Error_struct _tmp34A;const char*_tmp349;struct
Cyc_Lexing_Error_struct*_tmp348;(int)_throw((void*)((_tmp348=_cycalloc(sizeof(*
_tmp348)),((_tmp348[0]=((_tmp34A.tag=Cyc_Lexing_Error,((_tmp34A.f1=((_tmp349="some action didn't return!",
_tag_dyneither(_tmp349,sizeof(char),27))),_tmp34A)))),_tmp348)))));}}int Cyc_args(
struct Cyc_Lexing_lexbuf*lexbuf);int Cyc_args(struct Cyc_Lexing_lexbuf*lexbuf){
return Cyc_args_rec(lexbuf,2);}int Cyc_token_rec(struct Cyc_Lexing_lexbuf*lexbuf,
int lexstate);int Cyc_token_rec(struct Cyc_Lexing_lexbuf*lexbuf,int lexstate){
lexstate=Cyc_lex_engine(lexstate,lexbuf);switch(lexstate){case 0: _LLE:{struct
_dyneither_ptr*_tmp34B;Cyc_add_target(((_tmp34B=_cycalloc(sizeof(*_tmp34B)),((
_tmp34B[0]=(struct _dyneither_ptr)Cyc_Lexing_lexeme(lexbuf),_tmp34B)))));}return
Cyc_token(lexbuf);case 1: _LLF: return 0;case 2: _LL10: return Cyc_token(lexbuf);case 3:
_LL11: Cyc_string(lexbuf);return Cyc_token(lexbuf);case 4: _LL12: return Cyc_token(
lexbuf);case 5: _LL13: return Cyc_token(lexbuf);case 6: _LL14: return Cyc_token(lexbuf);
case 7: _LL15: return Cyc_token(lexbuf);case 8: _LL16: return Cyc_token(lexbuf);case 9:
_LL17: return Cyc_token(lexbuf);case 10: _LL18: return Cyc_token(lexbuf);case 11: _LL19:
return Cyc_token(lexbuf);case 12: _LL1A: return Cyc_token(lexbuf);case 13: _LL1B: return
Cyc_token(lexbuf);case 14: _LL1C: return Cyc_token(lexbuf);case 15: _LL1D: return Cyc_token(
lexbuf);case 16: _LL1E: return Cyc_token(lexbuf);case 17: _LL1F: return Cyc_token(lexbuf);
case 18: _LL20: return Cyc_token(lexbuf);case 19: _LL21: return Cyc_token(lexbuf);case 20:
_LL22: return Cyc_token(lexbuf);case 21: _LL23: return Cyc_token(lexbuf);case 22: _LL24:
return Cyc_token(lexbuf);case 23: _LL25: return Cyc_token(lexbuf);case 24: _LL26: return
Cyc_token(lexbuf);case 25: _LL27: return Cyc_token(lexbuf);case 26: _LL28: return Cyc_token(
lexbuf);case 27: _LL29: return Cyc_token(lexbuf);case 28: _LL2A: return Cyc_token(lexbuf);
case 29: _LL2B: return Cyc_token(lexbuf);case 30: _LL2C: return Cyc_token(lexbuf);case 31:
_LL2D: return Cyc_token(lexbuf);case 32: _LL2E: return Cyc_token(lexbuf);case 33: _LL2F:
return Cyc_token(lexbuf);case 34: _LL30: return Cyc_token(lexbuf);case 35: _LL31: return
Cyc_token(lexbuf);case 36: _LL32: return Cyc_token(lexbuf);case 37: _LL33: return Cyc_token(
lexbuf);case 38: _LL34: return Cyc_token(lexbuf);case 39: _LL35: return Cyc_token(lexbuf);
case 40: _LL36: return Cyc_token(lexbuf);case 41: _LL37: return Cyc_token(lexbuf);case 42:
_LL38: return Cyc_token(lexbuf);case 43: _LL39: return Cyc_token(lexbuf);case 44: _LL3A:
return Cyc_token(lexbuf);case 45: _LL3B: return Cyc_token(lexbuf);case 46: _LL3C: return
Cyc_token(lexbuf);case 47: _LL3D: return Cyc_token(lexbuf);case 48: _LL3E: return Cyc_token(
lexbuf);default: _LL3F:(lexbuf->refill_buff)(lexbuf);return Cyc_token_rec(lexbuf,
lexstate);}{struct Cyc_Lexing_Error_struct _tmp351;const char*_tmp350;struct Cyc_Lexing_Error_struct*
_tmp34F;(int)_throw((void*)((_tmp34F=_cycalloc(sizeof(*_tmp34F)),((_tmp34F[0]=((
_tmp351.tag=Cyc_Lexing_Error,((_tmp351.f1=((_tmp350="some action didn't return!",
_tag_dyneither(_tmp350,sizeof(char),27))),_tmp351)))),_tmp34F)))));}}int Cyc_token(
struct Cyc_Lexing_lexbuf*lexbuf);int Cyc_token(struct Cyc_Lexing_lexbuf*lexbuf){
return Cyc_token_rec(lexbuf,3);}int Cyc_string_rec(struct Cyc_Lexing_lexbuf*lexbuf,
int lexstate);int Cyc_string_rec(struct Cyc_Lexing_lexbuf*lexbuf,int lexstate){
lexstate=Cyc_lex_engine(lexstate,lexbuf);switch(lexstate){case 0: _LL41: return Cyc_string(
lexbuf);case 1: _LL42: return 0;case 2: _LL43: return Cyc_string(lexbuf);case 3: _LL44:
return Cyc_string(lexbuf);case 4: _LL45: return Cyc_string(lexbuf);case 5: _LL46: return
Cyc_string(lexbuf);case 6: _LL47: return Cyc_string(lexbuf);case 7: _LL48: return 0;case
8: _LL49: return 0;case 9: _LL4A: return Cyc_string(lexbuf);default: _LL4B:(lexbuf->refill_buff)(
lexbuf);return Cyc_string_rec(lexbuf,lexstate);}{struct Cyc_Lexing_Error_struct
_tmp357;const char*_tmp356;struct Cyc_Lexing_Error_struct*_tmp355;(int)_throw((
void*)((_tmp355=_cycalloc(sizeof(*_tmp355)),((_tmp355[0]=((_tmp357.tag=Cyc_Lexing_Error,((
_tmp357.f1=((_tmp356="some action didn't return!",_tag_dyneither(_tmp356,sizeof(
char),27))),_tmp357)))),_tmp355)))));}}int Cyc_string(struct Cyc_Lexing_lexbuf*
lexbuf);int Cyc_string(struct Cyc_Lexing_lexbuf*lexbuf){return Cyc_string_rec(
lexbuf,4);}int Cyc_slurp_rec(struct Cyc_Lexing_lexbuf*lexbuf,int lexstate);int Cyc_slurp_rec(
struct Cyc_Lexing_lexbuf*lexbuf,int lexstate){lexstate=Cyc_lex_engine(lexstate,
lexbuf);switch(lexstate){case 0: _LL4D: return 0;case 1: _LL4E: Cyc_fputc((int)'"',(
struct Cyc___cycFILE*)_check_null(Cyc_slurp_out));while(Cyc_slurp_string(lexbuf)){;}
return 1;case 2: _LL4F: Cyc_fputs((const char*)"*__IGNORE_FOR_CYCLONE_MALLOC(",(
struct Cyc___cycFILE*)_check_null(Cyc_slurp_out));{const char*_tmp35A;void*_tmp359;(
_tmp359=0,Cyc_log(((_tmp35A="Warning: declaration of malloc sidestepped\n",
_tag_dyneither(_tmp35A,sizeof(char),44))),_tag_dyneither(_tmp359,sizeof(void*),0)));}
return 1;case 3: _LL50: Cyc_fputs((const char*)" __IGNORE_FOR_CYCLONE_MALLOC(",(
struct Cyc___cycFILE*)_check_null(Cyc_slurp_out));{const char*_tmp35D;void*_tmp35C;(
_tmp35C=0,Cyc_log(((_tmp35D="Warning: declaration of malloc sidestepped\n",
_tag_dyneither(_tmp35D,sizeof(char),44))),_tag_dyneither(_tmp35C,sizeof(void*),0)));}
return 1;case 4: _LL51: Cyc_fputs((const char*)"*__IGNORE_FOR_CYCLONE_CALLOC(",(
struct Cyc___cycFILE*)_check_null(Cyc_slurp_out));{const char*_tmp360;void*_tmp35F;(
_tmp35F=0,Cyc_log(((_tmp360="Warning: declaration of calloc sidestepped\n",
_tag_dyneither(_tmp360,sizeof(char),44))),_tag_dyneither(_tmp35F,sizeof(void*),0)));}
return 1;case 5: _LL52: Cyc_fputs((const char*)" __IGNORE_FOR_CYCLONE_CALLOC(",(
struct Cyc___cycFILE*)_check_null(Cyc_slurp_out));{const char*_tmp363;void*_tmp362;(
_tmp362=0,Cyc_log(((_tmp363="Warning: declaration of calloc sidestepped\n",
_tag_dyneither(_tmp363,sizeof(char),44))),_tag_dyneither(_tmp362,sizeof(void*),0)));}
return 1;case 6: _LL53: Cyc_fputs((const char*)"__region",(struct Cyc___cycFILE*)
_check_null(Cyc_slurp_out));{const char*_tmp366;void*_tmp365;(_tmp365=0,Cyc_log(((
_tmp366="Warning: use of region sidestepped\n",_tag_dyneither(_tmp366,sizeof(
char),36))),_tag_dyneither(_tmp365,sizeof(void*),0)));}return 1;case 7: _LL54:
return 1;case 8: _LL55: return 1;case 9: _LL56: return 1;case 10: _LL57: return 1;case 11:
_LL58: return 1;case 12: _LL59: return 1;case 13: _LL5A: Cyc_fputs((const char*)"inline",(
struct Cyc___cycFILE*)_check_null(Cyc_slurp_out));return 1;case 14: _LL5B: Cyc_fputs((
const char*)"inline",(struct Cyc___cycFILE*)_check_null(Cyc_slurp_out));return 1;
case 15: _LL5C: Cyc_fputs((const char*)"const",(struct Cyc___cycFILE*)_check_null(Cyc_slurp_out));
return 1;case 16: _LL5D: Cyc_fputs((const char*)"const",(struct Cyc___cycFILE*)
_check_null(Cyc_slurp_out));return 1;case 17: _LL5E: Cyc_fputs((const char*)"int",(
struct Cyc___cycFILE*)_check_null(Cyc_slurp_out));return 1;case 18: _LL5F: return 1;
case 19: _LL60: Cyc_parens_to_match=1;while(Cyc_asm(lexbuf)){;}Cyc_fputs((const char*)"0",(
struct Cyc___cycFILE*)_check_null(Cyc_slurp_out));{const char*_tmp369;void*_tmp368;(
_tmp368=0,Cyc_log(((_tmp369="Warning: replacing use of __asm__ with 0\n",
_tag_dyneither(_tmp369,sizeof(char),42))),_tag_dyneither(_tmp368,sizeof(void*),0)));}
return 1;case 20: _LL61: Cyc_fputc((int)Cyc_Lexing_lexeme_char(lexbuf,0),(struct Cyc___cycFILE*)
_check_null(Cyc_slurp_out));return 1;default: _LL62:(lexbuf->refill_buff)(lexbuf);
return Cyc_slurp_rec(lexbuf,lexstate);}{struct Cyc_Lexing_Error_struct _tmp36F;
const char*_tmp36E;struct Cyc_Lexing_Error_struct*_tmp36D;(int)_throw((void*)((
_tmp36D=_cycalloc(sizeof(*_tmp36D)),((_tmp36D[0]=((_tmp36F.tag=Cyc_Lexing_Error,((
_tmp36F.f1=((_tmp36E="some action didn't return!",_tag_dyneither(_tmp36E,sizeof(
char),27))),_tmp36F)))),_tmp36D)))));}}int Cyc_slurp(struct Cyc_Lexing_lexbuf*
lexbuf);int Cyc_slurp(struct Cyc_Lexing_lexbuf*lexbuf){return Cyc_slurp_rec(lexbuf,
5);}int Cyc_slurp_string_rec(struct Cyc_Lexing_lexbuf*lexbuf,int lexstate);int Cyc_slurp_string_rec(
struct Cyc_Lexing_lexbuf*lexbuf,int lexstate){lexstate=Cyc_lex_engine(lexstate,
lexbuf);switch(lexstate){case 0: _LL64: return 0;case 1: _LL65: Cyc_fputc((int)'"',(
struct Cyc___cycFILE*)_check_null(Cyc_slurp_out));return 0;case 2: _LL66:{const char*
_tmp372;void*_tmp371;(_tmp371=0,Cyc_log(((_tmp372="Warning: unclosed string\n",
_tag_dyneither(_tmp372,sizeof(char),26))),_tag_dyneither(_tmp371,sizeof(void*),0)));}{
const char*_tmp376;void*_tmp375[1];struct Cyc_String_pa_struct _tmp374;(_tmp374.tag=
0,((_tmp374.f1=(struct _dyneither_ptr)((struct _dyneither_ptr)Cyc_Lexing_lexeme(
lexbuf)),((_tmp375[0]=& _tmp374,Cyc_fprintf((struct Cyc___cycFILE*)_check_null(Cyc_slurp_out),((
_tmp376="%s",_tag_dyneither(_tmp376,sizeof(char),3))),_tag_dyneither(_tmp375,
sizeof(void*),1)))))));}return 1;case 3: _LL67:{const char*_tmp37A;void*_tmp379[1];
struct Cyc_String_pa_struct _tmp378;(_tmp378.tag=0,((_tmp378.f1=(struct
_dyneither_ptr)((struct _dyneither_ptr)Cyc_Lexing_lexeme(lexbuf)),((_tmp379[0]=&
_tmp378,Cyc_fprintf((struct Cyc___cycFILE*)_check_null(Cyc_slurp_out),((_tmp37A="%s",
_tag_dyneither(_tmp37A,sizeof(char),3))),_tag_dyneither(_tmp379,sizeof(void*),1)))))));}
return 1;case 4: _LL68:{const char*_tmp37E;void*_tmp37D[1];struct Cyc_String_pa_struct
_tmp37C;(_tmp37C.tag=0,((_tmp37C.f1=(struct _dyneither_ptr)((struct _dyneither_ptr)
Cyc_Lexing_lexeme(lexbuf)),((_tmp37D[0]=& _tmp37C,Cyc_fprintf((struct Cyc___cycFILE*)
_check_null(Cyc_slurp_out),((_tmp37E="%s",_tag_dyneither(_tmp37E,sizeof(char),3))),
_tag_dyneither(_tmp37D,sizeof(void*),1)))))));}return 1;case 5: _LL69:{const char*
_tmp382;void*_tmp381[1];struct Cyc_String_pa_struct _tmp380;(_tmp380.tag=0,((
_tmp380.f1=(struct _dyneither_ptr)((struct _dyneither_ptr)Cyc_Lexing_lexeme(lexbuf)),((
_tmp381[0]=& _tmp380,Cyc_fprintf((struct Cyc___cycFILE*)_check_null(Cyc_slurp_out),((
_tmp382="%s",_tag_dyneither(_tmp382,sizeof(char),3))),_tag_dyneither(_tmp381,
sizeof(void*),1)))))));}return 1;case 6: _LL6A:{const char*_tmp386;void*_tmp385[1];
struct Cyc_String_pa_struct _tmp384;(_tmp384.tag=0,((_tmp384.f1=(struct
_dyneither_ptr)((struct _dyneither_ptr)Cyc_Lexing_lexeme(lexbuf)),((_tmp385[0]=&
_tmp384,Cyc_fprintf((struct Cyc___cycFILE*)_check_null(Cyc_slurp_out),((_tmp386="%s",
_tag_dyneither(_tmp386,sizeof(char),3))),_tag_dyneither(_tmp385,sizeof(void*),1)))))));}
return 1;case 7: _LL6B:{const char*_tmp38A;void*_tmp389[1];struct Cyc_String_pa_struct
_tmp388;(_tmp388.tag=0,((_tmp388.f1=(struct _dyneither_ptr)((struct _dyneither_ptr)
Cyc_Lexing_lexeme(lexbuf)),((_tmp389[0]=& _tmp388,Cyc_fprintf((struct Cyc___cycFILE*)
_check_null(Cyc_slurp_out),((_tmp38A="%s",_tag_dyneither(_tmp38A,sizeof(char),3))),
_tag_dyneither(_tmp389,sizeof(void*),1)))))));}return 1;case 8: _LL6C:{const char*
_tmp38E;void*_tmp38D[1];struct Cyc_String_pa_struct _tmp38C;(_tmp38C.tag=0,((
_tmp38C.f1=(struct _dyneither_ptr)((struct _dyneither_ptr)Cyc_Lexing_lexeme(lexbuf)),((
_tmp38D[0]=& _tmp38C,Cyc_fprintf((struct Cyc___cycFILE*)_check_null(Cyc_slurp_out),((
_tmp38E="%s",_tag_dyneither(_tmp38E,sizeof(char),3))),_tag_dyneither(_tmp38D,
sizeof(void*),1)))))));}return 1;default: _LL6D:(lexbuf->refill_buff)(lexbuf);
return Cyc_slurp_string_rec(lexbuf,lexstate);}{struct Cyc_Lexing_Error_struct
_tmp394;const char*_tmp393;struct Cyc_Lexing_Error_struct*_tmp392;(int)_throw((
void*)((_tmp392=_cycalloc(sizeof(*_tmp392)),((_tmp392[0]=((_tmp394.tag=Cyc_Lexing_Error,((
_tmp394.f1=((_tmp393="some action didn't return!",_tag_dyneither(_tmp393,sizeof(
char),27))),_tmp394)))),_tmp392)))));}}int Cyc_slurp_string(struct Cyc_Lexing_lexbuf*
lexbuf);int Cyc_slurp_string(struct Cyc_Lexing_lexbuf*lexbuf){return Cyc_slurp_string_rec(
lexbuf,6);}int Cyc_asm_rec(struct Cyc_Lexing_lexbuf*lexbuf,int lexstate);int Cyc_asm_rec(
struct Cyc_Lexing_lexbuf*lexbuf,int lexstate){lexstate=Cyc_lex_engine(lexstate,
lexbuf);switch(lexstate){case 0: _LL6F: return 0;case 1: _LL70: if(Cyc_parens_to_match
== 1)return 0;-- Cyc_parens_to_match;return 1;case 2: _LL71: ++ Cyc_parens_to_match;
return 1;case 3: _LL72: while(Cyc_asm_string(lexbuf)){;}return 1;case 4: _LL73: while(
Cyc_asm_comment(lexbuf)){;}return 1;case 5: _LL74: return 1;case 6: _LL75: return 1;
default: _LL76:(lexbuf->refill_buff)(lexbuf);return Cyc_asm_rec(lexbuf,lexstate);}{
struct Cyc_Lexing_Error_struct _tmp39A;const char*_tmp399;struct Cyc_Lexing_Error_struct*
_tmp398;(int)_throw((void*)((_tmp398=_cycalloc(sizeof(*_tmp398)),((_tmp398[0]=((
_tmp39A.tag=Cyc_Lexing_Error,((_tmp39A.f1=((_tmp399="some action didn't return!",
_tag_dyneither(_tmp399,sizeof(char),27))),_tmp39A)))),_tmp398)))));}}int Cyc_asm(
struct Cyc_Lexing_lexbuf*lexbuf);int Cyc_asm(struct Cyc_Lexing_lexbuf*lexbuf){
return Cyc_asm_rec(lexbuf,7);}int Cyc_asm_string_rec(struct Cyc_Lexing_lexbuf*
lexbuf,int lexstate);int Cyc_asm_string_rec(struct Cyc_Lexing_lexbuf*lexbuf,int
lexstate){lexstate=Cyc_lex_engine(lexstate,lexbuf);switch(lexstate){case 0: _LL78:{
const char*_tmp39D;void*_tmp39C;(_tmp39C=0,Cyc_log(((_tmp39D="Warning: unclosed string\n",
_tag_dyneither(_tmp39D,sizeof(char),26))),_tag_dyneither(_tmp39C,sizeof(void*),0)));}
return 0;case 1: _LL79: return 0;case 2: _LL7A:{const char*_tmp3A0;void*_tmp39F;(_tmp39F=
0,Cyc_log(((_tmp3A0="Warning: unclosed string\n",_tag_dyneither(_tmp3A0,sizeof(
char),26))),_tag_dyneither(_tmp39F,sizeof(void*),0)));}return 1;case 3: _LL7B:
return 1;case 4: _LL7C: return 1;case 5: _LL7D: return 1;case 6: _LL7E: return 1;case 7: _LL7F:
return 1;case 8: _LL80: return 1;default: _LL81:(lexbuf->refill_buff)(lexbuf);return
Cyc_asm_string_rec(lexbuf,lexstate);}{struct Cyc_Lexing_Error_struct _tmp3A6;const
char*_tmp3A5;struct Cyc_Lexing_Error_struct*_tmp3A4;(int)_throw((void*)((_tmp3A4=
_cycalloc(sizeof(*_tmp3A4)),((_tmp3A4[0]=((_tmp3A6.tag=Cyc_Lexing_Error,((
_tmp3A6.f1=((_tmp3A5="some action didn't return!",_tag_dyneither(_tmp3A5,sizeof(
char),27))),_tmp3A6)))),_tmp3A4)))));}}int Cyc_asm_string(struct Cyc_Lexing_lexbuf*
lexbuf);int Cyc_asm_string(struct Cyc_Lexing_lexbuf*lexbuf){return Cyc_asm_string_rec(
lexbuf,8);}int Cyc_asm_comment_rec(struct Cyc_Lexing_lexbuf*lexbuf,int lexstate);
int Cyc_asm_comment_rec(struct Cyc_Lexing_lexbuf*lexbuf,int lexstate){lexstate=Cyc_lex_engine(
lexstate,lexbuf);switch(lexstate){case 0: _LL83:{const char*_tmp3A9;void*_tmp3A8;(
_tmp3A8=0,Cyc_log(((_tmp3A9="Warning: unclosed comment\n",_tag_dyneither(_tmp3A9,
sizeof(char),27))),_tag_dyneither(_tmp3A8,sizeof(void*),0)));}return 0;case 1:
_LL84: return 0;case 2: _LL85: return 1;default: _LL86:(lexbuf->refill_buff)(lexbuf);
return Cyc_asm_comment_rec(lexbuf,lexstate);}{struct Cyc_Lexing_Error_struct
_tmp3AF;const char*_tmp3AE;struct Cyc_Lexing_Error_struct*_tmp3AD;(int)_throw((
void*)((_tmp3AD=_cycalloc(sizeof(*_tmp3AD)),((_tmp3AD[0]=((_tmp3AF.tag=Cyc_Lexing_Error,((
_tmp3AF.f1=((_tmp3AE="some action didn't return!",_tag_dyneither(_tmp3AE,sizeof(
char),27))),_tmp3AF)))),_tmp3AD)))));}}int Cyc_asm_comment(struct Cyc_Lexing_lexbuf*
lexbuf);int Cyc_asm_comment(struct Cyc_Lexing_lexbuf*lexbuf){return Cyc_asm_comment_rec(
lexbuf,9);}struct _tuple13*Cyc_suck_line_rec(struct Cyc_Lexing_lexbuf*lexbuf,int
lexstate);struct _tuple13*Cyc_suck_line_rec(struct Cyc_Lexing_lexbuf*lexbuf,int
lexstate){lexstate=Cyc_lex_engine(lexstate,lexbuf);switch(lexstate){case 0: _LL88:{
const char*_tmp3B0;Cyc_current_line=((_tmp3B0="#define ",_tag_dyneither(_tmp3B0,
sizeof(char),9)));}Cyc_suck_macroname(lexbuf);{struct _tuple13*_tmp3B1;return(
_tmp3B1=_cycalloc(sizeof(*_tmp3B1)),((_tmp3B1->f1=Cyc_current_line,((_tmp3B1->f2=(
struct _dyneither_ptr*)_check_null(Cyc_current_source),_tmp3B1)))));}case 1: _LL89:
return Cyc_suck_line(lexbuf);case 2: _LL8A: return 0;default: _LL8B:(lexbuf->refill_buff)(
lexbuf);return Cyc_suck_line_rec(lexbuf,lexstate);}{struct Cyc_Lexing_Error_struct
_tmp3B7;const char*_tmp3B6;struct Cyc_Lexing_Error_struct*_tmp3B5;(int)_throw((
void*)((_tmp3B5=_cycalloc(sizeof(*_tmp3B5)),((_tmp3B5[0]=((_tmp3B7.tag=Cyc_Lexing_Error,((
_tmp3B7.f1=((_tmp3B6="some action didn't return!",_tag_dyneither(_tmp3B6,sizeof(
char),27))),_tmp3B7)))),_tmp3B5)))));}}struct _tuple13*Cyc_suck_line(struct Cyc_Lexing_lexbuf*
lexbuf);struct _tuple13*Cyc_suck_line(struct Cyc_Lexing_lexbuf*lexbuf){return Cyc_suck_line_rec(
lexbuf,10);}int Cyc_suck_macroname_rec(struct Cyc_Lexing_lexbuf*lexbuf,int lexstate);
int Cyc_suck_macroname_rec(struct Cyc_Lexing_lexbuf*lexbuf,int lexstate){lexstate=
Cyc_lex_engine(lexstate,lexbuf);switch(lexstate){case 0: _LL8D:{struct
_dyneither_ptr*_tmp3B8;Cyc_current_source=((_tmp3B8=_cycalloc(sizeof(*_tmp3B8)),((
_tmp3B8[0]=(struct _dyneither_ptr)Cyc_Lexing_lexeme(lexbuf),_tmp3B8))));}Cyc_current_line=(
struct _dyneither_ptr)Cyc_strconcat((struct _dyneither_ptr)Cyc_current_line,(
struct _dyneither_ptr)*((struct _dyneither_ptr*)_check_null(Cyc_current_source)));
return Cyc_suck_restofline(lexbuf);default: _LL8E:(lexbuf->refill_buff)(lexbuf);
return Cyc_suck_macroname_rec(lexbuf,lexstate);}{struct Cyc_Lexing_Error_struct
_tmp3BE;const char*_tmp3BD;struct Cyc_Lexing_Error_struct*_tmp3BC;(int)_throw((
void*)((_tmp3BC=_cycalloc(sizeof(*_tmp3BC)),((_tmp3BC[0]=((_tmp3BE.tag=Cyc_Lexing_Error,((
_tmp3BE.f1=((_tmp3BD="some action didn't return!",_tag_dyneither(_tmp3BD,sizeof(
char),27))),_tmp3BE)))),_tmp3BC)))));}}int Cyc_suck_macroname(struct Cyc_Lexing_lexbuf*
lexbuf);int Cyc_suck_macroname(struct Cyc_Lexing_lexbuf*lexbuf){return Cyc_suck_macroname_rec(
lexbuf,11);}int Cyc_suck_restofline_rec(struct Cyc_Lexing_lexbuf*lexbuf,int
lexstate);int Cyc_suck_restofline_rec(struct Cyc_Lexing_lexbuf*lexbuf,int lexstate){
lexstate=Cyc_lex_engine(lexstate,lexbuf);switch(lexstate){case 0: _LL90: Cyc_current_line=(
struct _dyneither_ptr)Cyc_strconcat((struct _dyneither_ptr)Cyc_current_line,(
struct _dyneither_ptr)Cyc_Lexing_lexeme(lexbuf));return 0;default: _LL91:(lexbuf->refill_buff)(
lexbuf);return Cyc_suck_restofline_rec(lexbuf,lexstate);}{struct Cyc_Lexing_Error_struct
_tmp3C4;const char*_tmp3C3;struct Cyc_Lexing_Error_struct*_tmp3C2;(int)_throw((
void*)((_tmp3C2=_cycalloc(sizeof(*_tmp3C2)),((_tmp3C2[0]=((_tmp3C4.tag=Cyc_Lexing_Error,((
_tmp3C4.f1=((_tmp3C3="some action didn't return!",_tag_dyneither(_tmp3C3,sizeof(
char),27))),_tmp3C4)))),_tmp3C2)))));}}int Cyc_suck_restofline(struct Cyc_Lexing_lexbuf*
lexbuf);int Cyc_suck_restofline(struct Cyc_Lexing_lexbuf*lexbuf){return Cyc_suck_restofline_rec(
lexbuf,12);}struct _tuple15*Cyc_spec_rec(struct Cyc_Lexing_lexbuf*lexbuf,int
lexstate);struct _tuple15*Cyc_spec_rec(struct Cyc_Lexing_lexbuf*lexbuf,int lexstate){
lexstate=Cyc_lex_engine(lexstate,lexbuf);switch(lexstate){case 0: _LL93: return Cyc_spec(
lexbuf);case 1: _LL94: Cyc_current_headerfile=(struct _dyneither_ptr)Cyc_substring((
struct _dyneither_ptr)Cyc_Lexing_lexeme(lexbuf),0,(unsigned long)((Cyc_Lexing_lexeme_end(
lexbuf)- Cyc_Lexing_lexeme_start(lexbuf))- 1));Cyc_current_symbols=0;Cyc_current_omit_symbols=
0;Cyc_current_cstubs=0;Cyc_current_cycstubs=0;Cyc_current_hstubs=0;while(Cyc_commands(
lexbuf)){;}Cyc_current_hstubs=((struct Cyc_List_List*(*)(struct Cyc_List_List*x))
Cyc_List_imp_rev)(Cyc_current_hstubs);Cyc_current_cstubs=((struct Cyc_List_List*(*)(
struct Cyc_List_List*x))Cyc_List_imp_rev)(Cyc_current_cstubs);Cyc_current_cycstubs=((
struct Cyc_List_List*(*)(struct Cyc_List_List*x))Cyc_List_imp_rev)(Cyc_current_cycstubs);{
struct _tuple15*_tmp3C5;return(_tmp3C5=_cycalloc(sizeof(*_tmp3C5)),((_tmp3C5->f1=
Cyc_current_headerfile,((_tmp3C5->f2=Cyc_current_symbols,((_tmp3C5->f3=Cyc_current_omit_symbols,((
_tmp3C5->f4=Cyc_current_hstubs,((_tmp3C5->f5=Cyc_current_cstubs,((_tmp3C5->f6=
Cyc_current_cycstubs,_tmp3C5)))))))))))));}case 2: _LL95: return Cyc_spec(lexbuf);
case 3: _LL96: return 0;case 4: _LL97:{const char*_tmp3C9;void*_tmp3C8[1];struct Cyc_Int_pa_struct
_tmp3C7;(_tmp3C7.tag=1,((_tmp3C7.f1=(unsigned long)((int)Cyc_Lexing_lexeme_char(
lexbuf,0)),((_tmp3C8[0]=& _tmp3C7,Cyc_fprintf(Cyc_stderr,((_tmp3C9="Error in .cys file: expected header file name, found '%c' instead\n",
_tag_dyneither(_tmp3C9,sizeof(char),67))),_tag_dyneither(_tmp3C8,sizeof(void*),1)))))));}
return 0;default: _LL98:(lexbuf->refill_buff)(lexbuf);return Cyc_spec_rec(lexbuf,
lexstate);}{struct Cyc_Lexing_Error_struct _tmp3CF;const char*_tmp3CE;struct Cyc_Lexing_Error_struct*
_tmp3CD;(int)_throw((void*)((_tmp3CD=_cycalloc(sizeof(*_tmp3CD)),((_tmp3CD[0]=((
_tmp3CF.tag=Cyc_Lexing_Error,((_tmp3CF.f1=((_tmp3CE="some action didn't return!",
_tag_dyneither(_tmp3CE,sizeof(char),27))),_tmp3CF)))),_tmp3CD)))));}}struct
_tuple15*Cyc_spec(struct Cyc_Lexing_lexbuf*lexbuf);struct _tuple15*Cyc_spec(struct
Cyc_Lexing_lexbuf*lexbuf){return Cyc_spec_rec(lexbuf,13);}int Cyc_commands_rec(
struct Cyc_Lexing_lexbuf*lexbuf,int lexstate);int Cyc_commands_rec(struct Cyc_Lexing_lexbuf*
lexbuf,int lexstate){lexstate=Cyc_lex_engine(lexstate,lexbuf);switch(lexstate){
case 0: _LL9A: return 0;case 1: _LL9B: return 0;case 2: _LL9C: Cyc_snarfed_symbols=0;while(
Cyc_snarfsymbols(lexbuf)){;}Cyc_current_symbols=((struct Cyc_List_List*(*)(struct
Cyc_List_List*x,struct Cyc_List_List*y))Cyc_List_append)(Cyc_snarfed_symbols,Cyc_current_symbols);
return 1;case 3: _LL9D: Cyc_snarfed_symbols=0;while(Cyc_snarfsymbols(lexbuf)){;}Cyc_current_omit_symbols=((
struct Cyc_List_List*(*)(struct Cyc_List_List*x,struct Cyc_List_List*y))Cyc_List_append)(
Cyc_snarfed_symbols,Cyc_current_omit_symbols);return 1;case 4: _LL9E: Cyc_braces_to_match=
1;Cyc_specbuf=(struct Cyc_Buffer_t*)Cyc_Buffer_create(255);while(Cyc_block(lexbuf)){;}{
struct _tuple14*_tmp3D0;struct _tuple14*x=(_tmp3D0=_cycalloc(sizeof(*_tmp3D0)),((
_tmp3D0->f1=(struct _dyneither_ptr)_tag_dyneither(0,0,0),((_tmp3D0->f2=(struct
_dyneither_ptr)Cyc_Buffer_contents((struct Cyc_Buffer_t*)_check_null(Cyc_specbuf)),
_tmp3D0)))));{struct Cyc_List_List*_tmp3D1;Cyc_current_hstubs=((_tmp3D1=_cycalloc(
sizeof(*_tmp3D1)),((_tmp3D1->hd=x,((_tmp3D1->tl=Cyc_current_hstubs,_tmp3D1))))));}
return 1;}case 5: _LL9F: {struct _dyneither_ptr _tmp72=Cyc_Lexing_lexeme(lexbuf);{
const char*_tmp3D2;_dyneither_ptr_inplace_plus(& _tmp72,sizeof(char),(int)Cyc_strlen(((
_tmp3D2="hstub",_tag_dyneither(_tmp3D2,sizeof(char),6)))));}while(isspace((int)*((
char*)_check_dyneither_subscript(_tmp72,sizeof(char),0)))){
_dyneither_ptr_inplace_plus(& _tmp72,sizeof(char),1);}{struct _dyneither_ptr t=
_tmp72;while(!isspace((int)*((char*)_check_dyneither_subscript(t,sizeof(char),0)))){
_dyneither_ptr_inplace_plus(& t,sizeof(char),1);}{struct _dyneither_ptr _tmp74=Cyc_substring((
struct _dyneither_ptr)_tmp72,0,(unsigned long)((t.curr - _tmp72.curr)/ sizeof(char)));
Cyc_braces_to_match=1;Cyc_specbuf=(struct Cyc_Buffer_t*)Cyc_Buffer_create(255);
while(Cyc_block(lexbuf)){;}{struct _tuple14*_tmp3D3;struct _tuple14*x=(_tmp3D3=
_cycalloc(sizeof(*_tmp3D3)),((_tmp3D3->f1=(struct _dyneither_ptr)_tmp74,((_tmp3D3->f2=(
struct _dyneither_ptr)Cyc_Buffer_contents((struct Cyc_Buffer_t*)_check_null(Cyc_specbuf)),
_tmp3D3)))));{struct Cyc_List_List*_tmp3D4;Cyc_current_hstubs=((_tmp3D4=_cycalloc(
sizeof(*_tmp3D4)),((_tmp3D4->hd=x,((_tmp3D4->tl=Cyc_current_hstubs,_tmp3D4))))));}
return 1;}}}}case 6: _LLA0: Cyc_braces_to_match=1;Cyc_specbuf=(struct Cyc_Buffer_t*)
Cyc_Buffer_create(255);while(Cyc_block(lexbuf)){;}{struct _tuple14*_tmp3D5;struct
_tuple14*x=(_tmp3D5=_cycalloc(sizeof(*_tmp3D5)),((_tmp3D5->f1=(struct
_dyneither_ptr)_tag_dyneither(0,0,0),((_tmp3D5->f2=(struct _dyneither_ptr)Cyc_Buffer_contents((
struct Cyc_Buffer_t*)_check_null(Cyc_specbuf)),_tmp3D5)))));{struct Cyc_List_List*
_tmp3D6;Cyc_current_cstubs=((_tmp3D6=_cycalloc(sizeof(*_tmp3D6)),((_tmp3D6->hd=x,((
_tmp3D6->tl=Cyc_current_cstubs,_tmp3D6))))));}return 1;}case 7: _LLA1: {struct
_dyneither_ptr _tmp79=Cyc_Lexing_lexeme(lexbuf);{const char*_tmp3D7;
_dyneither_ptr_inplace_plus(& _tmp79,sizeof(char),(int)Cyc_strlen(((_tmp3D7="cstub",
_tag_dyneither(_tmp3D7,sizeof(char),6)))));}while(isspace((int)*((char*)
_check_dyneither_subscript(_tmp79,sizeof(char),0)))){_dyneither_ptr_inplace_plus(&
_tmp79,sizeof(char),1);}{struct _dyneither_ptr t=_tmp79;while(!isspace((int)*((
char*)_check_dyneither_subscript(t,sizeof(char),0)))){
_dyneither_ptr_inplace_plus(& t,sizeof(char),1);}{struct _dyneither_ptr _tmp7B=Cyc_substring((
struct _dyneither_ptr)_tmp79,0,(unsigned long)((t.curr - _tmp79.curr)/ sizeof(char)));
Cyc_braces_to_match=1;Cyc_specbuf=(struct Cyc_Buffer_t*)Cyc_Buffer_create(255);
while(Cyc_block(lexbuf)){;}{struct _tuple14*_tmp3D8;struct _tuple14*x=(_tmp3D8=
_cycalloc(sizeof(*_tmp3D8)),((_tmp3D8->f1=(struct _dyneither_ptr)_tmp7B,((_tmp3D8->f2=(
struct _dyneither_ptr)Cyc_Buffer_contents((struct Cyc_Buffer_t*)_check_null(Cyc_specbuf)),
_tmp3D8)))));{struct Cyc_List_List*_tmp3D9;Cyc_current_cstubs=((_tmp3D9=_cycalloc(
sizeof(*_tmp3D9)),((_tmp3D9->hd=x,((_tmp3D9->tl=Cyc_current_cstubs,_tmp3D9))))));}
return 1;}}}}case 8: _LLA2: Cyc_braces_to_match=1;Cyc_specbuf=(struct Cyc_Buffer_t*)
Cyc_Buffer_create(255);while(Cyc_block(lexbuf)){;}{struct _tuple14*_tmp3DA;struct
_tuple14*x=(_tmp3DA=_cycalloc(sizeof(*_tmp3DA)),((_tmp3DA->f1=(struct
_dyneither_ptr)_tag_dyneither(0,0,0),((_tmp3DA->f2=(struct _dyneither_ptr)Cyc_Buffer_contents((
struct Cyc_Buffer_t*)_check_null(Cyc_specbuf)),_tmp3DA)))));{struct Cyc_List_List*
_tmp3DB;Cyc_current_cycstubs=((_tmp3DB=_cycalloc(sizeof(*_tmp3DB)),((_tmp3DB->hd=
x,((_tmp3DB->tl=Cyc_current_cycstubs,_tmp3DB))))));}return 1;}case 9: _LLA3: {
struct _dyneither_ptr _tmp80=Cyc_Lexing_lexeme(lexbuf);{const char*_tmp3DC;
_dyneither_ptr_inplace_plus(& _tmp80,sizeof(char),(int)Cyc_strlen(((_tmp3DC="cycstub",
_tag_dyneither(_tmp3DC,sizeof(char),8)))));}while(isspace((int)*((char*)
_check_dyneither_subscript(_tmp80,sizeof(char),0)))){_dyneither_ptr_inplace_plus(&
_tmp80,sizeof(char),1);}{struct _dyneither_ptr t=_tmp80;while(!isspace((int)*((
char*)_check_dyneither_subscript(t,sizeof(char),0)))){
_dyneither_ptr_inplace_plus(& t,sizeof(char),1);}{struct _dyneither_ptr _tmp82=Cyc_substring((
struct _dyneither_ptr)_tmp80,0,(unsigned long)((t.curr - _tmp80.curr)/ sizeof(char)));
Cyc_braces_to_match=1;Cyc_specbuf=(struct Cyc_Buffer_t*)Cyc_Buffer_create(255);
while(Cyc_block(lexbuf)){;}{struct _tuple14*_tmp3DD;struct _tuple14*x=(_tmp3DD=
_cycalloc(sizeof(*_tmp3DD)),((_tmp3DD->f1=(struct _dyneither_ptr)_tmp82,((_tmp3DD->f2=(
struct _dyneither_ptr)Cyc_Buffer_contents((struct Cyc_Buffer_t*)_check_null(Cyc_specbuf)),
_tmp3DD)))));{struct Cyc_List_List*_tmp3DE;Cyc_current_cycstubs=((_tmp3DE=
_cycalloc(sizeof(*_tmp3DE)),((_tmp3DE->hd=x,((_tmp3DE->tl=Cyc_current_cycstubs,
_tmp3DE))))));}return 1;}}}}case 10: _LLA4: Cyc_braces_to_match=1;Cyc_specbuf=(
struct Cyc_Buffer_t*)Cyc_Buffer_create(255);while(Cyc_block(lexbuf)){;}{struct
_dyneither_ptr*_tmp3DF;struct _dyneither_ptr*x=(_tmp3DF=_cycalloc(sizeof(*_tmp3DF)),((
_tmp3DF[0]=(struct _dyneither_ptr)Cyc_Buffer_contents((struct Cyc_Buffer_t*)
_check_null(Cyc_specbuf)),_tmp3DF)));{struct Cyc_List_List*_tmp3E0;Cyc_current_cpp=((
_tmp3E0=_cycalloc(sizeof(*_tmp3E0)),((_tmp3E0->hd=x,((_tmp3E0->tl=Cyc_current_cpp,
_tmp3E0))))));}return 1;}case 11: _LLA5: return 1;case 12: _LLA6: return 1;case 13: _LLA7:{
const char*_tmp3E4;void*_tmp3E3[1];struct Cyc_Int_pa_struct _tmp3E2;(_tmp3E2.tag=1,((
_tmp3E2.f1=(unsigned long)((int)Cyc_Lexing_lexeme_char(lexbuf,0)),((_tmp3E3[0]=&
_tmp3E2,Cyc_fprintf(Cyc_stderr,((_tmp3E4="Error in .cys file: expected command, found '%c' instead\n",
_tag_dyneither(_tmp3E4,sizeof(char),58))),_tag_dyneither(_tmp3E3,sizeof(void*),1)))))));}
return 0;default: _LLA8:(lexbuf->refill_buff)(lexbuf);return Cyc_commands_rec(
lexbuf,lexstate);}{struct Cyc_Lexing_Error_struct _tmp3EA;const char*_tmp3E9;struct
Cyc_Lexing_Error_struct*_tmp3E8;(int)_throw((void*)((_tmp3E8=_cycalloc(sizeof(*
_tmp3E8)),((_tmp3E8[0]=((_tmp3EA.tag=Cyc_Lexing_Error,((_tmp3EA.f1=((_tmp3E9="some action didn't return!",
_tag_dyneither(_tmp3E9,sizeof(char),27))),_tmp3EA)))),_tmp3E8)))));}}int Cyc_commands(
struct Cyc_Lexing_lexbuf*lexbuf);int Cyc_commands(struct Cyc_Lexing_lexbuf*lexbuf){
return Cyc_commands_rec(lexbuf,14);}int Cyc_snarfsymbols_rec(struct Cyc_Lexing_lexbuf*
lexbuf,int lexstate);int Cyc_snarfsymbols_rec(struct Cyc_Lexing_lexbuf*lexbuf,int
lexstate){lexstate=Cyc_lex_engine(lexstate,lexbuf);switch(lexstate){case 0: _LLAA:{
struct _dyneither_ptr*_tmp3ED;struct Cyc_List_List*_tmp3EC;Cyc_snarfed_symbols=((
_tmp3EC=_cycalloc(sizeof(*_tmp3EC)),((_tmp3EC->hd=((_tmp3ED=_cycalloc(sizeof(*
_tmp3ED)),((_tmp3ED[0]=(struct _dyneither_ptr)Cyc_Lexing_lexeme(lexbuf),_tmp3ED)))),((
_tmp3EC->tl=Cyc_snarfed_symbols,_tmp3EC))))));}return 1;case 1: _LLAB: return 1;case 2:
_LLAC: return 0;case 3: _LLAD:{const char*_tmp3F0;void*_tmp3EF;(_tmp3EF=0,Cyc_fprintf(
Cyc_stderr,((_tmp3F0="Error in .cys file: unexpected end-of-file\n",
_tag_dyneither(_tmp3F0,sizeof(char),44))),_tag_dyneither(_tmp3EF,sizeof(void*),0)));}
return 0;case 4: _LLAE:{const char*_tmp3F4;void*_tmp3F3[1];struct Cyc_Int_pa_struct
_tmp3F2;(_tmp3F2.tag=1,((_tmp3F2.f1=(unsigned long)((int)Cyc_Lexing_lexeme_char(
lexbuf,0)),((_tmp3F3[0]=& _tmp3F2,Cyc_fprintf(Cyc_stderr,((_tmp3F4="Error in .cys file: expected symbol, found '%c' instead\n",
_tag_dyneither(_tmp3F4,sizeof(char),57))),_tag_dyneither(_tmp3F3,sizeof(void*),1)))))));}
return 0;default: _LLAF:(lexbuf->refill_buff)(lexbuf);return Cyc_snarfsymbols_rec(
lexbuf,lexstate);}{struct Cyc_Lexing_Error_struct _tmp3FA;const char*_tmp3F9;struct
Cyc_Lexing_Error_struct*_tmp3F8;(int)_throw((void*)((_tmp3F8=_cycalloc(sizeof(*
_tmp3F8)),((_tmp3F8[0]=((_tmp3FA.tag=Cyc_Lexing_Error,((_tmp3FA.f1=((_tmp3F9="some action didn't return!",
_tag_dyneither(_tmp3F9,sizeof(char),27))),_tmp3FA)))),_tmp3F8)))));}}int Cyc_snarfsymbols(
struct Cyc_Lexing_lexbuf*lexbuf);int Cyc_snarfsymbols(struct Cyc_Lexing_lexbuf*
lexbuf){return Cyc_snarfsymbols_rec(lexbuf,15);}int Cyc_block_rec(struct Cyc_Lexing_lexbuf*
lexbuf,int lexstate);int Cyc_block_rec(struct Cyc_Lexing_lexbuf*lexbuf,int lexstate){
lexstate=Cyc_lex_engine(lexstate,lexbuf);switch(lexstate){case 0: _LLB1:{const char*
_tmp3FD;void*_tmp3FC;(_tmp3FC=0,Cyc_log(((_tmp3FD="Warning: unclosed brace\n",
_tag_dyneither(_tmp3FD,sizeof(char),25))),_tag_dyneither(_tmp3FC,sizeof(void*),0)));}
return 0;case 1: _LLB2: if(Cyc_braces_to_match == 1)return 0;-- Cyc_braces_to_match;Cyc_Buffer_add_char((
struct Cyc_Buffer_t*)_check_null(Cyc_specbuf),'}');return 1;case 2: _LLB3: ++ Cyc_braces_to_match;
Cyc_Buffer_add_char((struct Cyc_Buffer_t*)_check_null(Cyc_specbuf),'{');return 1;
case 3: _LLB4: Cyc_Buffer_add_char((struct Cyc_Buffer_t*)_check_null(Cyc_specbuf),'"');
while(Cyc_block_string(lexbuf)){;}return 1;case 4: _LLB5:{const char*_tmp3FE;Cyc_Buffer_add_string((
struct Cyc_Buffer_t*)_check_null(Cyc_specbuf),((_tmp3FE="/*",_tag_dyneither(
_tmp3FE,sizeof(char),3))));}while(Cyc_block_comment(lexbuf)){;}return 1;case 5:
_LLB6: Cyc_Buffer_add_string((struct Cyc_Buffer_t*)_check_null(Cyc_specbuf),(
struct _dyneither_ptr)Cyc_Lexing_lexeme(lexbuf));return 1;case 6: _LLB7: Cyc_Buffer_add_char((
struct Cyc_Buffer_t*)_check_null(Cyc_specbuf),Cyc_Lexing_lexeme_char(lexbuf,0));
return 1;default: _LLB8:(lexbuf->refill_buff)(lexbuf);return Cyc_block_rec(lexbuf,
lexstate);}{struct Cyc_Lexing_Error_struct _tmp404;const char*_tmp403;struct Cyc_Lexing_Error_struct*
_tmp402;(int)_throw((void*)((_tmp402=_cycalloc(sizeof(*_tmp402)),((_tmp402[0]=((
_tmp404.tag=Cyc_Lexing_Error,((_tmp404.f1=((_tmp403="some action didn't return!",
_tag_dyneither(_tmp403,sizeof(char),27))),_tmp404)))),_tmp402)))));}}int Cyc_block(
struct Cyc_Lexing_lexbuf*lexbuf);int Cyc_block(struct Cyc_Lexing_lexbuf*lexbuf){
return Cyc_block_rec(lexbuf,16);}int Cyc_block_string_rec(struct Cyc_Lexing_lexbuf*
lexbuf,int lexstate);int Cyc_block_string_rec(struct Cyc_Lexing_lexbuf*lexbuf,int
lexstate){lexstate=Cyc_lex_engine(lexstate,lexbuf);switch(lexstate){case 0: _LLBA:{
const char*_tmp407;void*_tmp406;(_tmp406=0,Cyc_log(((_tmp407="Warning: unclosed string\n",
_tag_dyneither(_tmp407,sizeof(char),26))),_tag_dyneither(_tmp406,sizeof(void*),0)));}
return 0;case 1: _LLBB: Cyc_Buffer_add_char((struct Cyc_Buffer_t*)_check_null(Cyc_specbuf),'"');
return 0;case 2: _LLBC:{const char*_tmp40A;void*_tmp409;(_tmp409=0,Cyc_log(((_tmp40A="Warning: unclosed string\n",
_tag_dyneither(_tmp40A,sizeof(char),26))),_tag_dyneither(_tmp409,sizeof(void*),0)));}
Cyc_Buffer_add_string((struct Cyc_Buffer_t*)_check_null(Cyc_specbuf),(struct
_dyneither_ptr)Cyc_Lexing_lexeme(lexbuf));return 1;case 3: _LLBD: Cyc_Buffer_add_string((
struct Cyc_Buffer_t*)_check_null(Cyc_specbuf),(struct _dyneither_ptr)Cyc_Lexing_lexeme(
lexbuf));return 1;case 4: _LLBE: Cyc_Buffer_add_string((struct Cyc_Buffer_t*)
_check_null(Cyc_specbuf),(struct _dyneither_ptr)Cyc_Lexing_lexeme(lexbuf));return
1;case 5: _LLBF: Cyc_Buffer_add_string((struct Cyc_Buffer_t*)_check_null(Cyc_specbuf),(
struct _dyneither_ptr)Cyc_Lexing_lexeme(lexbuf));return 1;case 6: _LLC0: Cyc_Buffer_add_string((
struct Cyc_Buffer_t*)_check_null(Cyc_specbuf),(struct _dyneither_ptr)Cyc_Lexing_lexeme(
lexbuf));return 1;case 7: _LLC1: Cyc_Buffer_add_string((struct Cyc_Buffer_t*)
_check_null(Cyc_specbuf),(struct _dyneither_ptr)Cyc_Lexing_lexeme(lexbuf));return
1;case 8: _LLC2: Cyc_Buffer_add_string((struct Cyc_Buffer_t*)_check_null(Cyc_specbuf),(
struct _dyneither_ptr)Cyc_Lexing_lexeme(lexbuf));return 1;default: _LLC3:(lexbuf->refill_buff)(
lexbuf);return Cyc_block_string_rec(lexbuf,lexstate);}{struct Cyc_Lexing_Error_struct
_tmp410;const char*_tmp40F;struct Cyc_Lexing_Error_struct*_tmp40E;(int)_throw((
void*)((_tmp40E=_cycalloc(sizeof(*_tmp40E)),((_tmp40E[0]=((_tmp410.tag=Cyc_Lexing_Error,((
_tmp410.f1=((_tmp40F="some action didn't return!",_tag_dyneither(_tmp40F,sizeof(
char),27))),_tmp410)))),_tmp40E)))));}}int Cyc_block_string(struct Cyc_Lexing_lexbuf*
lexbuf);int Cyc_block_string(struct Cyc_Lexing_lexbuf*lexbuf){return Cyc_block_string_rec(
lexbuf,17);}int Cyc_block_comment_rec(struct Cyc_Lexing_lexbuf*lexbuf,int lexstate);
int Cyc_block_comment_rec(struct Cyc_Lexing_lexbuf*lexbuf,int lexstate){lexstate=
Cyc_lex_engine(lexstate,lexbuf);switch(lexstate){case 0: _LLC5:{const char*_tmp413;
void*_tmp412;(_tmp412=0,Cyc_log(((_tmp413="Warning: unclosed comment\n",
_tag_dyneither(_tmp413,sizeof(char),27))),_tag_dyneither(_tmp412,sizeof(void*),0)));}
return 0;case 1: _LLC6:{const char*_tmp414;Cyc_Buffer_add_string((struct Cyc_Buffer_t*)
_check_null(Cyc_specbuf),((_tmp414="*/",_tag_dyneither(_tmp414,sizeof(char),3))));}
return 0;case 2: _LLC7: Cyc_Buffer_add_string((struct Cyc_Buffer_t*)_check_null(Cyc_specbuf),(
struct _dyneither_ptr)Cyc_Lexing_lexeme(lexbuf));return 1;default: _LLC8:(lexbuf->refill_buff)(
lexbuf);return Cyc_block_comment_rec(lexbuf,lexstate);}{struct Cyc_Lexing_Error_struct
_tmp41A;const char*_tmp419;struct Cyc_Lexing_Error_struct*_tmp418;(int)_throw((
void*)((_tmp418=_cycalloc(sizeof(*_tmp418)),((_tmp418[0]=((_tmp41A.tag=Cyc_Lexing_Error,((
_tmp41A.f1=((_tmp419="some action didn't return!",_tag_dyneither(_tmp419,sizeof(
char),27))),_tmp41A)))),_tmp418)))));}}int Cyc_block_comment(struct Cyc_Lexing_lexbuf*
lexbuf);int Cyc_block_comment(struct Cyc_Lexing_lexbuf*lexbuf){return Cyc_block_comment_rec(
lexbuf,18);}void Cyc_scan_type(void*t);struct _tuple16{struct Cyc_List_List*f1;
struct Cyc_Absyn_Exp*f2;};void Cyc_scan_exp(struct Cyc_Absyn_Exp*e);void Cyc_scan_exp(
struct Cyc_Absyn_Exp*e){void*_tmpAA=(void*)((struct Cyc_Absyn_Exp*)_check_null(e))->r;
struct _tuple0*_tmpAB;struct _tuple0 _tmpAC;struct _dyneither_ptr*_tmpAD;struct
_tuple0*_tmpAE;struct _tuple0 _tmpAF;struct _dyneither_ptr*_tmpB0;struct Cyc_List_List*
_tmpB1;struct Cyc_Absyn_Exp*_tmpB2;struct Cyc_Absyn_Exp*_tmpB3;struct Cyc_Absyn_Exp*
_tmpB4;struct Cyc_Absyn_Exp*_tmpB5;struct Cyc_Absyn_Exp*_tmpB6;struct Cyc_Absyn_Exp*
_tmpB7;struct Cyc_Absyn_Exp*_tmpB8;struct Cyc_Absyn_Exp*_tmpB9;struct Cyc_Absyn_Exp*
_tmpBA;struct Cyc_Absyn_Exp*_tmpBB;struct Cyc_Absyn_Exp*_tmpBC;struct Cyc_Absyn_Exp*
_tmpBD;struct Cyc_Absyn_Exp*_tmpBE;struct Cyc_Absyn_Exp*_tmpBF;struct Cyc_Absyn_Exp*
_tmpC0;struct Cyc_Absyn_Exp*_tmpC1;struct Cyc_Absyn_Exp*_tmpC2;struct Cyc_Absyn_Exp*
_tmpC3;struct Cyc_List_List*_tmpC4;struct Cyc_Absyn_Exp*_tmpC5;struct Cyc_List_List*
_tmpC6;void*_tmpC7;struct Cyc_Absyn_Exp*_tmpC8;struct Cyc_Absyn_MallocInfo _tmpC9;
int _tmpCA;struct Cyc_Absyn_Exp*_tmpCB;void**_tmpCC;struct Cyc_Absyn_Exp*_tmpCD;
void*_tmpCE;void*_tmpCF;struct Cyc_Absyn_Exp*_tmpD0;struct _dyneither_ptr*_tmpD1;
struct Cyc_Absyn_Exp*_tmpD2;struct _dyneither_ptr*_tmpD3;void*_tmpD4;void*_tmpD5;
struct Cyc_List_List*_tmpD6;_LLCB: if(*((int*)_tmpAA)!= 1)goto _LLCD;_tmpAB=((
struct Cyc_Absyn_Var_e_struct*)_tmpAA)->f1;_tmpAC=*_tmpAB;_tmpAD=_tmpAC.f2;_LLCC:
_tmpB0=_tmpAD;goto _LLCE;_LLCD: if(*((int*)_tmpAA)!= 2)goto _LLCF;_tmpAE=((struct
Cyc_Absyn_UnknownId_e_struct*)_tmpAA)->f1;_tmpAF=*_tmpAE;_tmpB0=_tmpAF.f2;_LLCE:
Cyc_add_target(_tmpB0);return;_LLCF: if(*((int*)_tmpAA)!= 3)goto _LLD1;_tmpB1=((
struct Cyc_Absyn_Primop_e_struct*)_tmpAA)->f2;_LLD0: for(0;_tmpB1 != 0;_tmpB1=
_tmpB1->tl){Cyc_scan_exp((struct Cyc_Absyn_Exp*)((struct Cyc_Absyn_Exp*)_tmpB1->hd));}
return;_LLD1: if(*((int*)_tmpAA)!= 25)goto _LLD3;_tmpB2=((struct Cyc_Absyn_Subscript_e_struct*)
_tmpAA)->f1;_tmpB3=((struct Cyc_Absyn_Subscript_e_struct*)_tmpAA)->f2;_LLD2:
_tmpB4=_tmpB2;_tmpB5=_tmpB3;goto _LLD4;_LLD3: if(*((int*)_tmpAA)!= 9)goto _LLD5;
_tmpB4=((struct Cyc_Absyn_SeqExp_e_struct*)_tmpAA)->f1;_tmpB5=((struct Cyc_Absyn_SeqExp_e_struct*)
_tmpAA)->f2;_LLD4: _tmpB6=_tmpB4;_tmpB7=_tmpB5;goto _LLD6;_LLD5: if(*((int*)_tmpAA)
!= 4)goto _LLD7;_tmpB6=((struct Cyc_Absyn_AssignOp_e_struct*)_tmpAA)->f1;_tmpB7=((
struct Cyc_Absyn_AssignOp_e_struct*)_tmpAA)->f3;_LLD6: Cyc_scan_exp((struct Cyc_Absyn_Exp*)
_tmpB6);Cyc_scan_exp((struct Cyc_Absyn_Exp*)_tmpB7);return;_LLD7: if(*((int*)
_tmpAA)!= 22)goto _LLD9;_tmpB8=((struct Cyc_Absyn_Deref_e_struct*)_tmpAA)->f1;
_LLD8: _tmpB9=_tmpB8;goto _LLDA;_LLD9: if(*((int*)_tmpAA)!= 19)goto _LLDB;_tmpB9=((
struct Cyc_Absyn_Sizeofexp_e_struct*)_tmpAA)->f1;_LLDA: _tmpBA=_tmpB9;goto _LLDC;
_LLDB: if(*((int*)_tmpAA)!= 16)goto _LLDD;_tmpBA=((struct Cyc_Absyn_Address_e_struct*)
_tmpAA)->f1;_LLDC: _tmpBB=_tmpBA;goto _LLDE;_LLDD: if(*((int*)_tmpAA)!= 5)goto _LLDF;
_tmpBB=((struct Cyc_Absyn_Increment_e_struct*)_tmpAA)->f1;_LLDE: Cyc_scan_exp((
struct Cyc_Absyn_Exp*)_tmpBB);return;_LLDF: if(*((int*)_tmpAA)!= 6)goto _LLE1;
_tmpBC=((struct Cyc_Absyn_Conditional_e_struct*)_tmpAA)->f1;_tmpBD=((struct Cyc_Absyn_Conditional_e_struct*)
_tmpAA)->f2;_tmpBE=((struct Cyc_Absyn_Conditional_e_struct*)_tmpAA)->f3;_LLE0: Cyc_scan_exp((
struct Cyc_Absyn_Exp*)_tmpBC);Cyc_scan_exp((struct Cyc_Absyn_Exp*)_tmpBD);Cyc_scan_exp((
struct Cyc_Absyn_Exp*)_tmpBE);return;_LLE1: if(*((int*)_tmpAA)!= 7)goto _LLE3;
_tmpBF=((struct Cyc_Absyn_And_e_struct*)_tmpAA)->f1;_tmpC0=((struct Cyc_Absyn_And_e_struct*)
_tmpAA)->f2;_LLE2: _tmpC1=_tmpBF;_tmpC2=_tmpC0;goto _LLE4;_LLE3: if(*((int*)_tmpAA)
!= 8)goto _LLE5;_tmpC1=((struct Cyc_Absyn_Or_e_struct*)_tmpAA)->f1;_tmpC2=((struct
Cyc_Absyn_Or_e_struct*)_tmpAA)->f2;_LLE4: Cyc_scan_exp((struct Cyc_Absyn_Exp*)
_tmpC1);Cyc_scan_exp((struct Cyc_Absyn_Exp*)_tmpC2);return;_LLE5: if(*((int*)
_tmpAA)!= 11)goto _LLE7;_tmpC3=((struct Cyc_Absyn_FnCall_e_struct*)_tmpAA)->f1;
_tmpC4=((struct Cyc_Absyn_FnCall_e_struct*)_tmpAA)->f2;_LLE6: _tmpC5=_tmpC3;_tmpC6=
_tmpC4;goto _LLE8;_LLE7: if(*((int*)_tmpAA)!= 10)goto _LLE9;_tmpC5=((struct Cyc_Absyn_UnknownCall_e_struct*)
_tmpAA)->f1;_tmpC6=((struct Cyc_Absyn_UnknownCall_e_struct*)_tmpAA)->f2;_LLE8: Cyc_scan_exp((
struct Cyc_Absyn_Exp*)_tmpC5);for(0;_tmpC6 != 0;_tmpC6=_tmpC6->tl){Cyc_scan_exp((
struct Cyc_Absyn_Exp*)((struct Cyc_Absyn_Exp*)_tmpC6->hd));}return;_LLE9: if(*((int*)
_tmpAA)!= 15)goto _LLEB;_tmpC7=(void*)((struct Cyc_Absyn_Cast_e_struct*)_tmpAA)->f1;
_tmpC8=((struct Cyc_Absyn_Cast_e_struct*)_tmpAA)->f2;_LLEA: Cyc_scan_type(_tmpC7);
Cyc_scan_exp((struct Cyc_Absyn_Exp*)_tmpC8);return;_LLEB: if(*((int*)_tmpAA)!= 35)
goto _LLED;_tmpC9=((struct Cyc_Absyn_Malloc_e_struct*)_tmpAA)->f1;_tmpCA=_tmpC9.is_calloc;
_tmpCB=_tmpC9.rgn;_tmpCC=_tmpC9.elt_type;_tmpCD=_tmpC9.num_elts;_LLEC: if(_tmpCB
!= 0)Cyc_scan_exp((struct Cyc_Absyn_Exp*)((struct Cyc_Absyn_Exp*)_tmpCB));if(
_tmpCC != 0)Cyc_scan_type(*_tmpCC);Cyc_scan_exp((struct Cyc_Absyn_Exp*)_tmpCD);
return;_LLED: if(*((int*)_tmpAA)!= 39)goto _LLEF;_tmpCE=(void*)((struct Cyc_Absyn_Valueof_e_struct*)
_tmpAA)->f1;_LLEE: _tmpCF=_tmpCE;goto _LLF0;_LLEF: if(*((int*)_tmpAA)!= 18)goto
_LLF1;_tmpCF=(void*)((struct Cyc_Absyn_Sizeoftyp_e_struct*)_tmpAA)->f1;_LLF0: Cyc_scan_type(
_tmpCF);return;_LLF1: if(*((int*)_tmpAA)!= 23)goto _LLF3;_tmpD0=((struct Cyc_Absyn_AggrMember_e_struct*)
_tmpAA)->f1;_tmpD1=((struct Cyc_Absyn_AggrMember_e_struct*)_tmpAA)->f2;_LLF2:
_tmpD2=_tmpD0;_tmpD3=_tmpD1;goto _LLF4;_LLF3: if(*((int*)_tmpAA)!= 24)goto _LLF5;
_tmpD2=((struct Cyc_Absyn_AggrArrow_e_struct*)_tmpAA)->f1;_tmpD3=((struct Cyc_Absyn_AggrArrow_e_struct*)
_tmpAA)->f2;_LLF4: Cyc_scan_exp((struct Cyc_Absyn_Exp*)_tmpD2);Cyc_add_target(
_tmpD3);return;_LLF5: if(*((int*)_tmpAA)!= 20)goto _LLF7;_tmpD4=(void*)((struct Cyc_Absyn_Offsetof_e_struct*)
_tmpAA)->f1;_tmpD5=(void*)((struct Cyc_Absyn_Offsetof_e_struct*)_tmpAA)->f2;_LLF6:
Cyc_scan_type(_tmpD4);{void*_tmpD7=_tmpD5;struct _dyneither_ptr*_tmpD8;_LL11C: if(*((
int*)_tmpD7)!= 0)goto _LL11E;_tmpD8=((struct Cyc_Absyn_StructField_struct*)_tmpD7)->f1;
_LL11D: Cyc_add_target(_tmpD8);goto _LL11B;_LL11E: if(*((int*)_tmpD7)!= 1)goto
_LL11B;_LL11F: goto _LL11B;_LL11B:;}return;_LLF7: if(*((int*)_tmpAA)!= 0)goto _LLF9;
_LLF8: return;_LLF9: if(*((int*)_tmpAA)!= 37)goto _LLFB;_tmpD6=((struct Cyc_Absyn_UnresolvedMem_e_struct*)
_tmpAA)->f2;_LLFA: for(0;_tmpD6 != 0;_tmpD6=_tmpD6->tl){struct _tuple16 _tmpDA;
struct Cyc_Absyn_Exp*_tmpDB;struct _tuple16*_tmpD9=(struct _tuple16*)_tmpD6->hd;
_tmpDA=*_tmpD9;_tmpDB=_tmpDA.f2;Cyc_scan_exp((struct Cyc_Absyn_Exp*)_tmpDB);}
return;_LLFB: if(*((int*)_tmpAA)!= 36)goto _LLFD;_LLFC:{const char*_tmp41D;void*
_tmp41C;(_tmp41C=0,Cyc_fprintf(Cyc_stderr,((_tmp41D="Error: unexpected Swap_e\n",
_tag_dyneither(_tmp41D,sizeof(char),26))),_tag_dyneither(_tmp41C,sizeof(void*),0)));}
exit(1);return;_LLFD: if(*((int*)_tmpAA)!= 38)goto _LLFF;_LLFE:{const char*_tmp420;
void*_tmp41F;(_tmp41F=0,Cyc_fprintf(Cyc_stderr,((_tmp420="Error: unexpected Stmt_e\n",
_tag_dyneither(_tmp420,sizeof(char),26))),_tag_dyneither(_tmp41F,sizeof(void*),0)));}
exit(1);return;_LLFF: if(*((int*)_tmpAA)!= 12)goto _LL101;_LL100:{const char*
_tmp423;void*_tmp422;(_tmp422=0,Cyc_fprintf(Cyc_stderr,((_tmp423="Error: unexpected Throw_e\n",
_tag_dyneither(_tmp423,sizeof(char),27))),_tag_dyneither(_tmp422,sizeof(void*),0)));}
exit(1);return;_LL101: if(*((int*)_tmpAA)!= 13)goto _LL103;_LL102:{const char*
_tmp426;void*_tmp425;(_tmp425=0,Cyc_fprintf(Cyc_stderr,((_tmp426="Error: unexpected NoInstantiate_e\n",
_tag_dyneither(_tmp426,sizeof(char),35))),_tag_dyneither(_tmp425,sizeof(void*),0)));}
exit(1);return;_LL103: if(*((int*)_tmpAA)!= 14)goto _LL105;_LL104:{const char*
_tmp429;void*_tmp428;(_tmp428=0,Cyc_fprintf(Cyc_stderr,((_tmp429="Error: unexpected Instantiate_e\n",
_tag_dyneither(_tmp429,sizeof(char),33))),_tag_dyneither(_tmp428,sizeof(void*),0)));}
exit(1);return;_LL105: if(*((int*)_tmpAA)!= 17)goto _LL107;_LL106:{const char*
_tmp42C;void*_tmp42B;(_tmp42B=0,Cyc_fprintf(Cyc_stderr,((_tmp42C="Error: unexpected New_e\n",
_tag_dyneither(_tmp42C,sizeof(char),25))),_tag_dyneither(_tmp42B,sizeof(void*),0)));}
exit(1);return;_LL107: if(*((int*)_tmpAA)!= 21)goto _LL109;_LL108:{const char*
_tmp42F;void*_tmp42E;(_tmp42E=0,Cyc_fprintf(Cyc_stderr,((_tmp42F="Error: unexpected Gentyp_e\n",
_tag_dyneither(_tmp42F,sizeof(char),28))),_tag_dyneither(_tmp42E,sizeof(void*),0)));}
exit(1);return;_LL109: if(*((int*)_tmpAA)!= 26)goto _LL10B;_LL10A:{const char*
_tmp432;void*_tmp431;(_tmp431=0,Cyc_fprintf(Cyc_stderr,((_tmp432="Error: unexpected Tuple_e\n",
_tag_dyneither(_tmp432,sizeof(char),27))),_tag_dyneither(_tmp431,sizeof(void*),0)));}
exit(1);return;_LL10B: if(*((int*)_tmpAA)!= 27)goto _LL10D;_LL10C:{const char*
_tmp435;void*_tmp434;(_tmp434=0,Cyc_fprintf(Cyc_stderr,((_tmp435="Error: unexpected CompoundLit_e\n",
_tag_dyneither(_tmp435,sizeof(char),33))),_tag_dyneither(_tmp434,sizeof(void*),0)));}
exit(1);return;_LL10D: if(*((int*)_tmpAA)!= 28)goto _LL10F;_LL10E:{const char*
_tmp438;void*_tmp437;(_tmp437=0,Cyc_fprintf(Cyc_stderr,((_tmp438="Error: unexpected Array_e\n",
_tag_dyneither(_tmp438,sizeof(char),27))),_tag_dyneither(_tmp437,sizeof(void*),0)));}
exit(1);return;_LL10F: if(*((int*)_tmpAA)!= 29)goto _LL111;_LL110:{const char*
_tmp43B;void*_tmp43A;(_tmp43A=0,Cyc_fprintf(Cyc_stderr,((_tmp43B="Error: unexpected Comprehension_e\n",
_tag_dyneither(_tmp43B,sizeof(char),35))),_tag_dyneither(_tmp43A,sizeof(void*),0)));}
exit(1);return;_LL111: if(*((int*)_tmpAA)!= 30)goto _LL113;_LL112:{const char*
_tmp43E;void*_tmp43D;(_tmp43D=0,Cyc_fprintf(Cyc_stderr,((_tmp43E="Error: unexpected Struct_e\n",
_tag_dyneither(_tmp43E,sizeof(char),28))),_tag_dyneither(_tmp43D,sizeof(void*),0)));}
exit(1);return;_LL113: if(*((int*)_tmpAA)!= 31)goto _LL115;_LL114:{const char*
_tmp441;void*_tmp440;(_tmp440=0,Cyc_fprintf(Cyc_stderr,((_tmp441="Error: unexpected AnonStruct_e\n",
_tag_dyneither(_tmp441,sizeof(char),32))),_tag_dyneither(_tmp440,sizeof(void*),0)));}
exit(1);return;_LL115: if(*((int*)_tmpAA)!= 32)goto _LL117;_LL116:{const char*
_tmp444;void*_tmp443;(_tmp443=0,Cyc_fprintf(Cyc_stderr,((_tmp444="Error: unexpected Tunion_e\n",
_tag_dyneither(_tmp444,sizeof(char),28))),_tag_dyneither(_tmp443,sizeof(void*),0)));}
exit(1);return;_LL117: if(*((int*)_tmpAA)!= 33)goto _LL119;_LL118:{const char*
_tmp447;void*_tmp446;(_tmp446=0,Cyc_fprintf(Cyc_stderr,((_tmp447="Error: unexpected Enum_e\n",
_tag_dyneither(_tmp447,sizeof(char),26))),_tag_dyneither(_tmp446,sizeof(void*),0)));}
exit(1);return;_LL119: if(*((int*)_tmpAA)!= 34)goto _LLCA;_LL11A:{const char*
_tmp44A;void*_tmp449;(_tmp449=0,Cyc_fprintf(Cyc_stderr,((_tmp44A="Error: unexpected AnonEnum_e\n",
_tag_dyneither(_tmp44A,sizeof(char),30))),_tag_dyneither(_tmp449,sizeof(void*),0)));}
exit(1);return;_LLCA:;}void Cyc_scan_exp_opt(struct Cyc_Absyn_Exp*eo);void Cyc_scan_exp_opt(
struct Cyc_Absyn_Exp*eo){if((unsigned int)eo)Cyc_scan_exp(eo);return;}void Cyc_scan_type(
void*t);void Cyc_scan_type(void*t){void*_tmpFC=t;struct Cyc_Absyn_PtrInfo _tmpFD;
struct Cyc_Absyn_ArrayInfo _tmpFE;void*_tmpFF;struct Cyc_Absyn_Exp*_tmp100;struct
Cyc_Absyn_Conref*_tmp101;struct Cyc_Absyn_FnInfo _tmp102;struct Cyc_List_List*
_tmp103;struct Cyc_Absyn_AggrInfo _tmp104;union Cyc_Absyn_AggrInfoU_union _tmp105;
struct _tuple0*_tmp106;struct _tuple0 _tmp107;struct _dyneither_ptr*_tmp108;struct
_tuple0*_tmp109;struct _tuple0 _tmp10A;struct _dyneither_ptr*_tmp10B;_LL121: if((int)
_tmpFC != 0)goto _LL123;_LL122: goto _LL124;_LL123: if(_tmpFC <= (void*)4)goto _LL125;
if(*((int*)_tmpFC)!= 5)goto _LL125;_LL124: goto _LL126;_LL125: if((int)_tmpFC != 1)
goto _LL127;_LL126: goto _LL128;_LL127: if(_tmpFC <= (void*)4)goto _LL147;if(*((int*)
_tmpFC)!= 6)goto _LL129;_LL128: return;_LL129: if(*((int*)_tmpFC)!= 4)goto _LL12B;
_tmpFD=((struct Cyc_Absyn_PointerType_struct*)_tmpFC)->f1;_LL12A: Cyc_scan_type((
void*)_tmpFD.elt_typ);return;_LL12B: if(*((int*)_tmpFC)!= 7)goto _LL12D;_tmpFE=((
struct Cyc_Absyn_ArrayType_struct*)_tmpFC)->f1;_tmpFF=(void*)_tmpFE.elt_type;
_tmp100=_tmpFE.num_elts;_tmp101=_tmpFE.zero_term;_LL12C: Cyc_scan_type(_tmpFF);
Cyc_scan_exp_opt(_tmp100);return;_LL12D: if(*((int*)_tmpFC)!= 8)goto _LL12F;
_tmp102=((struct Cyc_Absyn_FnType_struct*)_tmpFC)->f1;_LL12E: Cyc_scan_type((void*)
_tmp102.ret_typ);{struct Cyc_List_List*_tmp10C=_tmp102.args;for(0;_tmp10C != 0;
_tmp10C=_tmp10C->tl){struct _tuple1 _tmp10E;void*_tmp10F;struct _tuple1*_tmp10D=(
struct _tuple1*)_tmp10C->hd;_tmp10E=*_tmp10D;_tmp10F=_tmp10E.f3;Cyc_scan_type(
_tmp10F);}}if(_tmp102.cyc_varargs != 0)Cyc_scan_type((void*)(_tmp102.cyc_varargs)->type);
return;_LL12F: if(*((int*)_tmpFC)!= 11)goto _LL131;_tmp103=((struct Cyc_Absyn_AnonAggrType_struct*)
_tmpFC)->f2;_LL130: for(0;_tmp103 != 0;_tmp103=_tmp103->tl){Cyc_scan_type((void*)((
struct Cyc_Absyn_Aggrfield*)_tmp103->hd)->type);Cyc_scan_exp_opt(((struct Cyc_Absyn_Aggrfield*)
_tmp103->hd)->width);}return;_LL131: if(*((int*)_tmpFC)!= 13)goto _LL133;_LL132:
return;_LL133: if(*((int*)_tmpFC)!= 10)goto _LL135;_tmp104=((struct Cyc_Absyn_AggrType_struct*)
_tmpFC)->f1;_tmp105=_tmp104.aggr_info;_LL134: {struct _tuple0*_tmp111;struct
_tuple0 _tmp112;struct _dyneither_ptr*_tmp113;struct _tuple3 _tmp110=Cyc_Absyn_aggr_kinded_name(
_tmp105);_tmp111=_tmp110.f2;_tmp112=*_tmp111;_tmp113=_tmp112.f2;_tmp108=_tmp113;
goto _LL136;}_LL135: if(*((int*)_tmpFC)!= 12)goto _LL137;_tmp106=((struct Cyc_Absyn_EnumType_struct*)
_tmpFC)->f1;_tmp107=*_tmp106;_tmp108=_tmp107.f2;_LL136: _tmp10B=_tmp108;goto
_LL138;_LL137: if(*((int*)_tmpFC)!= 16)goto _LL139;_tmp109=((struct Cyc_Absyn_TypedefType_struct*)
_tmpFC)->f1;_tmp10A=*_tmp109;_tmp10B=_tmp10A.f2;_LL138: Cyc_add_target(_tmp10B);
return;_LL139: if(*((int*)_tmpFC)!= 0)goto _LL13B;_LL13A:{const char*_tmp44D;void*
_tmp44C;(_tmp44C=0,Cyc_fprintf(Cyc_stderr,((_tmp44D="Error: unexpected Evar\n",
_tag_dyneither(_tmp44D,sizeof(char),24))),_tag_dyneither(_tmp44C,sizeof(void*),0)));}
exit(1);return;_LL13B: if(*((int*)_tmpFC)!= 1)goto _LL13D;_LL13C:{const char*
_tmp450;void*_tmp44F;(_tmp44F=0,Cyc_fprintf(Cyc_stderr,((_tmp450="Error: unexpected VarType\n",
_tag_dyneither(_tmp450,sizeof(char),27))),_tag_dyneither(_tmp44F,sizeof(void*),0)));}
exit(1);return;_LL13D: if(*((int*)_tmpFC)!= 2)goto _LL13F;_LL13E:{const char*
_tmp453;void*_tmp452;(_tmp452=0,Cyc_fprintf(Cyc_stderr,((_tmp453="Error: unexpected TunionType\n",
_tag_dyneither(_tmp453,sizeof(char),30))),_tag_dyneither(_tmp452,sizeof(void*),0)));}
exit(1);return;_LL13F: if(*((int*)_tmpFC)!= 3)goto _LL141;_LL140:{const char*
_tmp456;void*_tmp455;(_tmp455=0,Cyc_fprintf(Cyc_stderr,((_tmp456="Error: unexpected TunionFieldType\n",
_tag_dyneither(_tmp456,sizeof(char),35))),_tag_dyneither(_tmp455,sizeof(void*),0)));}
exit(1);return;_LL141: if(*((int*)_tmpFC)!= 9)goto _LL143;_LL142:{const char*
_tmp459;void*_tmp458;(_tmp458=0,Cyc_fprintf(Cyc_stderr,((_tmp459="Error: unexpected TupleType\n",
_tag_dyneither(_tmp459,sizeof(char),29))),_tag_dyneither(_tmp458,sizeof(void*),0)));}
exit(1);return;_LL143: if(*((int*)_tmpFC)!= 14)goto _LL145;_LL144:{const char*
_tmp45C;void*_tmp45B;(_tmp45B=0,Cyc_fprintf(Cyc_stderr,((_tmp45C="Error: unexpected RgnHandleType\n",
_tag_dyneither(_tmp45C,sizeof(char),33))),_tag_dyneither(_tmp45B,sizeof(void*),0)));}
exit(1);return;_LL145: if(*((int*)_tmpFC)!= 15)goto _LL147;_LL146:{const char*
_tmp45F;void*_tmp45E;(_tmp45E=0,Cyc_fprintf(Cyc_stderr,((_tmp45F="Error: unexpected DynRgnType\n",
_tag_dyneither(_tmp45F,sizeof(char),30))),_tag_dyneither(_tmp45E,sizeof(void*),0)));}
exit(1);return;_LL147: if((int)_tmpFC != 2)goto _LL149;_LL148:{const char*_tmp462;
void*_tmp461;(_tmp461=0,Cyc_fprintf(Cyc_stderr,((_tmp462="Error: unexpected HeapRgn\n",
_tag_dyneither(_tmp462,sizeof(char),27))),_tag_dyneither(_tmp461,sizeof(void*),0)));}
exit(1);return;_LL149: if((int)_tmpFC != 3)goto _LL14B;_LL14A:{const char*_tmp465;
void*_tmp464;(_tmp464=0,Cyc_fprintf(Cyc_stderr,((_tmp465="Error: unexpected UniqueRgn\n",
_tag_dyneither(_tmp465,sizeof(char),29))),_tag_dyneither(_tmp464,sizeof(void*),0)));}
exit(1);return;_LL14B: if(_tmpFC <= (void*)4)goto _LL14D;if(*((int*)_tmpFC)!= 19)
goto _LL14D;_LL14C:{const char*_tmp468;void*_tmp467;(_tmp467=0,Cyc_fprintf(Cyc_stderr,((
_tmp468="Error: unexpected AccessEff\n",_tag_dyneither(_tmp468,sizeof(char),29))),
_tag_dyneither(_tmp467,sizeof(void*),0)));}exit(1);return;_LL14D: if(_tmpFC <= (
void*)4)goto _LL14F;if(*((int*)_tmpFC)!= 20)goto _LL14F;_LL14E:{const char*_tmp46B;
void*_tmp46A;(_tmp46A=0,Cyc_fprintf(Cyc_stderr,((_tmp46B="Error: unexpected JoinEff\n",
_tag_dyneither(_tmp46B,sizeof(char),27))),_tag_dyneither(_tmp46A,sizeof(void*),0)));}
exit(1);return;_LL14F: if(_tmpFC <= (void*)4)goto _LL151;if(*((int*)_tmpFC)!= 21)
goto _LL151;_LL150:{const char*_tmp46E;void*_tmp46D;(_tmp46D=0,Cyc_fprintf(Cyc_stderr,((
_tmp46E="Error: unexpected RgnsEff\n",_tag_dyneither(_tmp46E,sizeof(char),27))),
_tag_dyneither(_tmp46D,sizeof(void*),0)));}exit(1);return;_LL151: if(_tmpFC <= (
void*)4)goto _LL153;if(*((int*)_tmpFC)!= 18)goto _LL153;_LL152:{const char*_tmp471;
void*_tmp470;(_tmp470=0,Cyc_fprintf(Cyc_stderr,((_tmp471="Error: unexpected tag_t\n",
_tag_dyneither(_tmp471,sizeof(char),25))),_tag_dyneither(_tmp470,sizeof(void*),0)));}
exit(1);return;_LL153: if(_tmpFC <= (void*)4)goto _LL120;if(*((int*)_tmpFC)!= 17)
goto _LL120;_LL154:{const char*_tmp474;void*_tmp473;(_tmp473=0,Cyc_fprintf(Cyc_stderr,((
_tmp474="Error: unexpected valueof_t\n",_tag_dyneither(_tmp474,sizeof(char),29))),
_tag_dyneither(_tmp473,sizeof(void*),0)));}exit(1);return;_LL120:;}struct
_tuple17{struct _dyneither_ptr*f1;struct Cyc_Absyn_Tqual f2;void*f3;};struct
_tuple12*Cyc_scan_decl(struct Cyc_Absyn_Decl*d);struct _tuple12*Cyc_scan_decl(
struct Cyc_Absyn_Decl*d){{struct Cyc_Set_Set**_tmp475;Cyc_current_targets=((
_tmp475=_cycalloc(sizeof(*_tmp475)),((_tmp475[0]=((struct Cyc_Set_Set*(*)(int(*
cmp)(struct _dyneither_ptr*,struct _dyneither_ptr*)))Cyc_Set_empty)(Cyc_strptrcmp),
_tmp475))));}{void*_tmp131=(void*)d->r;struct Cyc_Absyn_Vardecl*_tmp132;struct Cyc_Absyn_Fndecl*
_tmp133;struct Cyc_Absyn_Aggrdecl*_tmp134;struct Cyc_Absyn_Enumdecl*_tmp135;struct
Cyc_Absyn_Typedefdecl*_tmp136;_LL156: if(_tmp131 <= (void*)2)goto _LL160;if(*((int*)
_tmp131)!= 0)goto _LL158;_tmp132=((struct Cyc_Absyn_Var_d_struct*)_tmp131)->f1;
_LL157: {struct _tuple0 _tmp138;struct _dyneither_ptr*_tmp139;struct _tuple0*_tmp137=
_tmp132->name;_tmp138=*_tmp137;_tmp139=_tmp138.f2;Cyc_current_source=(struct
_dyneither_ptr*)_tmp139;Cyc_scan_type((void*)_tmp132->type);Cyc_scan_exp_opt(
_tmp132->initializer);goto _LL155;}_LL158: if(*((int*)_tmp131)!= 1)goto _LL15A;
_tmp133=((struct Cyc_Absyn_Fn_d_struct*)_tmp131)->f1;_LL159: {struct _tuple0
_tmp13B;struct _dyneither_ptr*_tmp13C;struct _tuple0*_tmp13A=_tmp133->name;_tmp13B=*
_tmp13A;_tmp13C=_tmp13B.f2;Cyc_current_source=(struct _dyneither_ptr*)_tmp13C;Cyc_scan_type((
void*)_tmp133->ret_type);{struct Cyc_List_List*_tmp13D=_tmp133->args;for(0;
_tmp13D != 0;_tmp13D=_tmp13D->tl){struct _tuple17 _tmp13F;void*_tmp140;struct
_tuple17*_tmp13E=(struct _tuple17*)_tmp13D->hd;_tmp13F=*_tmp13E;_tmp140=_tmp13F.f3;
Cyc_scan_type(_tmp140);}}if(_tmp133->cyc_varargs != 0)Cyc_scan_type((void*)((
struct Cyc_Absyn_VarargInfo*)_check_null(_tmp133->cyc_varargs))->type);if(_tmp133->is_inline){
const char*_tmp478;void*_tmp477;(_tmp477=0,Cyc_fprintf(Cyc_stderr,((_tmp478="Warning: ignoring inline function\n",
_tag_dyneither(_tmp478,sizeof(char),35))),_tag_dyneither(_tmp477,sizeof(void*),0)));}
goto _LL155;}_LL15A: if(*((int*)_tmp131)!= 4)goto _LL15C;_tmp134=((struct Cyc_Absyn_Aggr_d_struct*)
_tmp131)->f1;_LL15B: {struct _tuple0 _tmp144;struct _dyneither_ptr*_tmp145;struct
_tuple0*_tmp143=_tmp134->name;_tmp144=*_tmp143;_tmp145=_tmp144.f2;Cyc_current_source=(
struct _dyneither_ptr*)_tmp145;if((unsigned int)_tmp134->impl){{struct Cyc_List_List*
_tmp146=((struct Cyc_Absyn_AggrdeclImpl*)_check_null(_tmp134->impl))->fields;for(
0;_tmp146 != 0;_tmp146=_tmp146->tl){struct Cyc_Absyn_Aggrfield*_tmp147=(struct Cyc_Absyn_Aggrfield*)
_tmp146->hd;Cyc_scan_type((void*)_tmp147->type);Cyc_scan_exp_opt(_tmp147->width);}}{
struct Cyc_List_List*_tmp148=((struct Cyc_Absyn_AggrdeclImpl*)_check_null(_tmp134->impl))->fields;
for(0;_tmp148 != 0;_tmp148=_tmp148->tl){;}}}goto _LL155;}_LL15C: if(*((int*)_tmp131)
!= 6)goto _LL15E;_tmp135=((struct Cyc_Absyn_Enum_d_struct*)_tmp131)->f1;_LL15D: {
struct _tuple0 _tmp14A;struct _dyneither_ptr*_tmp14B;struct _tuple0*_tmp149=_tmp135->name;
_tmp14A=*_tmp149;_tmp14B=_tmp14A.f2;Cyc_current_source=(struct _dyneither_ptr*)
_tmp14B;if((unsigned int)_tmp135->fields){{struct Cyc_List_List*_tmp14C=(struct
Cyc_List_List*)((struct Cyc_Core_Opt*)_check_null(_tmp135->fields))->v;for(0;
_tmp14C != 0;_tmp14C=_tmp14C->tl){struct Cyc_Absyn_Enumfield*_tmp14D=(struct Cyc_Absyn_Enumfield*)
_tmp14C->hd;Cyc_scan_exp_opt(_tmp14D->tag);}}{struct Cyc_List_List*_tmp14E=(
struct Cyc_List_List*)((struct Cyc_Core_Opt*)_check_null(_tmp135->fields))->v;for(
0;_tmp14E != 0;_tmp14E=_tmp14E->tl){;}}}goto _LL155;}_LL15E: if(*((int*)_tmp131)!= 
7)goto _LL160;_tmp136=((struct Cyc_Absyn_Typedef_d_struct*)_tmp131)->f1;_LL15F: {
struct _tuple0 _tmp150;struct _dyneither_ptr*_tmp151;struct _tuple0*_tmp14F=_tmp136->name;
_tmp150=*_tmp14F;_tmp151=_tmp150.f2;Cyc_current_source=(struct _dyneither_ptr*)
_tmp151;if((unsigned int)_tmp136->defn)Cyc_scan_type((void*)((struct Cyc_Core_Opt*)
_check_null(_tmp136->defn))->v);goto _LL155;}_LL160: if((int)_tmp131 != 0)goto
_LL162;_LL161:{const char*_tmp47B;void*_tmp47A;(_tmp47A=0,Cyc_fprintf(Cyc_stderr,((
_tmp47B="Error: unexpected __cyclone_port_on__",_tag_dyneither(_tmp47B,sizeof(
char),38))),_tag_dyneither(_tmp47A,sizeof(void*),0)));}exit(1);return 0;_LL162:
if((int)_tmp131 != 1)goto _LL164;_LL163:{const char*_tmp47E;void*_tmp47D;(_tmp47D=0,
Cyc_fprintf(Cyc_stderr,((_tmp47E="Error: unexpected __cyclone_port_off__",
_tag_dyneither(_tmp47E,sizeof(char),39))),_tag_dyneither(_tmp47D,sizeof(void*),0)));}
exit(1);return 0;_LL164: if(_tmp131 <= (void*)2)goto _LL166;if(*((int*)_tmp131)!= 2)
goto _LL166;_LL165:{const char*_tmp481;void*_tmp480;(_tmp480=0,Cyc_fprintf(Cyc_stderr,((
_tmp481="Error: unexpected let declaration\n",_tag_dyneither(_tmp481,sizeof(char),
35))),_tag_dyneither(_tmp480,sizeof(void*),0)));}exit(1);return 0;_LL166: if(
_tmp131 <= (void*)2)goto _LL168;if(*((int*)_tmp131)!= 5)goto _LL168;_LL167:{const
char*_tmp484;void*_tmp483;(_tmp483=0,Cyc_fprintf(Cyc_stderr,((_tmp484="Error: unexpected tunion declaration\n",
_tag_dyneither(_tmp484,sizeof(char),38))),_tag_dyneither(_tmp483,sizeof(void*),0)));}
exit(1);return 0;_LL168: if(_tmp131 <= (void*)2)goto _LL16A;if(*((int*)_tmp131)!= 3)
goto _LL16A;_LL169:{const char*_tmp487;void*_tmp486;(_tmp486=0,Cyc_fprintf(Cyc_stderr,((
_tmp487="Error: unexpected let declaration\n",_tag_dyneither(_tmp487,sizeof(char),
35))),_tag_dyneither(_tmp486,sizeof(void*),0)));}exit(1);return 0;_LL16A: if(
_tmp131 <= (void*)2)goto _LL16C;if(*((int*)_tmp131)!= 8)goto _LL16C;_LL16B:{const
char*_tmp48A;void*_tmp489;(_tmp489=0,Cyc_fprintf(Cyc_stderr,((_tmp48A="Error: unexpected namespace declaration\n",
_tag_dyneither(_tmp48A,sizeof(char),41))),_tag_dyneither(_tmp489,sizeof(void*),0)));}
exit(1);return 0;_LL16C: if(_tmp131 <= (void*)2)goto _LL16E;if(*((int*)_tmp131)!= 9)
goto _LL16E;_LL16D:{const char*_tmp48D;void*_tmp48C;(_tmp48C=0,Cyc_fprintf(Cyc_stderr,((
_tmp48D="Error: unexpected using declaration\n",_tag_dyneither(_tmp48D,sizeof(
char),37))),_tag_dyneither(_tmp48C,sizeof(void*),0)));}exit(1);return 0;_LL16E:
if(_tmp131 <= (void*)2)goto _LL170;if(*((int*)_tmp131)!= 10)goto _LL170;_LL16F:{
const char*_tmp490;void*_tmp48F;(_tmp48F=0,Cyc_fprintf(Cyc_stderr,((_tmp490="Error: unexpected extern \"C\" declaration\n",
_tag_dyneither(_tmp490,sizeof(char),42))),_tag_dyneither(_tmp48F,sizeof(void*),0)));}
exit(1);return 0;_LL170: if(_tmp131 <= (void*)2)goto _LL155;if(*((int*)_tmp131)!= 11)
goto _LL155;_LL171:{const char*_tmp493;void*_tmp492;(_tmp492=0,Cyc_fprintf(Cyc_stderr,((
_tmp493="Error: unexpected extern \"C include\" declaration\n",_tag_dyneither(
_tmp493,sizeof(char),50))),_tag_dyneither(_tmp492,sizeof(void*),0)));}exit(1);
return 0;_LL155:;}{struct _tuple12*_tmp494;return(_tmp494=_cycalloc(sizeof(*
_tmp494)),((_tmp494->f1=(struct _dyneither_ptr*)_check_null(Cyc_current_source),((
_tmp494->f2=*((struct Cyc_Set_Set**)_check_null(Cyc_current_targets)),_tmp494)))));}}
struct Cyc_Hashtable_Table*Cyc_new_deps();struct Cyc_Hashtable_Table*Cyc_new_deps(){
return((struct Cyc_Hashtable_Table*(*)(int sz,int(*cmp)(struct _dyneither_ptr*,
struct _dyneither_ptr*),int(*hash)(struct _dyneither_ptr*)))Cyc_Hashtable_create)(
107,Cyc_strptrcmp,Cyc_Hashtable_hash_stringptr);}struct Cyc_Set_Set*Cyc_find(
struct Cyc_Hashtable_Table*t,struct _dyneither_ptr*x);struct Cyc_Set_Set*Cyc_find(
struct Cyc_Hashtable_Table*t,struct _dyneither_ptr*x){struct _handler_cons _tmp165;
_push_handler(& _tmp165);{int _tmp167=0;if(setjmp(_tmp165.handler))_tmp167=1;if(!
_tmp167){{struct Cyc_Set_Set*_tmp168=((struct Cyc_Set_Set*(*)(struct Cyc_Hashtable_Table*
t,struct _dyneither_ptr*key))Cyc_Hashtable_lookup)(t,x);_npop_handler(0);return
_tmp168;};_pop_handler();}else{void*_tmp166=(void*)_exn_thrown;void*_tmp16A=
_tmp166;_LL173: if(_tmp16A != Cyc_Core_Not_found)goto _LL175;_LL174: return((struct
Cyc_Set_Set*(*)(int(*cmp)(struct _dyneither_ptr*,struct _dyneither_ptr*)))Cyc_Set_empty)(
Cyc_strptrcmp);_LL175:;_LL176:(void)_throw(_tmp16A);_LL172:;}}}struct Cyc_Set_Set*
Cyc_reachable(struct Cyc_List_List*init,struct Cyc_Hashtable_Table*t);struct Cyc_Set_Set*
Cyc_reachable(struct Cyc_List_List*init,struct Cyc_Hashtable_Table*t){struct Cyc_Set_Set*
emptyset=((struct Cyc_Set_Set*(*)(int(*cmp)(struct _dyneither_ptr*,struct
_dyneither_ptr*)))Cyc_Set_empty)(Cyc_strptrcmp);struct Cyc_Set_Set*curr;for(curr=
emptyset;init != 0;init=init->tl){curr=((struct Cyc_Set_Set*(*)(struct Cyc_Set_Set*
s,struct _dyneither_ptr*elt))Cyc_Set_insert)(curr,(struct _dyneither_ptr*)init->hd);}{
struct Cyc_Set_Set*_tmp16B=curr;struct _dyneither_ptr*_tmp16C=_init_dyneither_ptr(
_cycalloc(sizeof(struct _dyneither_ptr)),"",sizeof(char),1);while(((int(*)(struct
Cyc_Set_Set*s))Cyc_Set_cardinality)(_tmp16B)> 0){struct Cyc_Set_Set*_tmp16D=
emptyset;struct Cyc_Iter_Iter _tmp16E=((struct Cyc_Iter_Iter(*)(struct _RegionHandle*
rgn,struct Cyc_Set_Set*s))Cyc_Set_make_iter)(Cyc_Core_heap_region,_tmp16B);while(((
int(*)(struct Cyc_Iter_Iter,struct _dyneither_ptr**))Cyc_Iter_next)(_tmp16E,&
_tmp16C)){_tmp16D=((struct Cyc_Set_Set*(*)(struct Cyc_Set_Set*s1,struct Cyc_Set_Set*
s2))Cyc_Set_union_two)(_tmp16D,Cyc_find(t,_tmp16C));}_tmp16B=((struct Cyc_Set_Set*(*)(
struct Cyc_Set_Set*s1,struct Cyc_Set_Set*s2))Cyc_Set_diff)(_tmp16D,curr);curr=((
struct Cyc_Set_Set*(*)(struct Cyc_Set_Set*s1,struct Cyc_Set_Set*s2))Cyc_Set_union_two)(
curr,_tmp16B);}return curr;}}static void*Cyc_mode=(void*)0;static int Cyc_gathering();
static int Cyc_gathering(){return Cyc_mode == (void*)1  || Cyc_mode == (void*)2;}
static struct Cyc___cycFILE*Cyc_script_file=0;int Cyc_prscript(struct _dyneither_ptr
fmt,struct _dyneither_ptr ap);int Cyc_prscript(struct _dyneither_ptr fmt,struct
_dyneither_ptr ap){if(Cyc_script_file == 0){{const char*_tmp497;void*_tmp496;(
_tmp496=0,Cyc_fprintf(Cyc_stderr,((_tmp497="Internal error: script file is NULL\n",
_tag_dyneither(_tmp497,sizeof(char),37))),_tag_dyneither(_tmp496,sizeof(void*),0)));}
exit(1);}return Cyc_vfprintf((struct Cyc___cycFILE*)_check_null(Cyc_script_file),
fmt,ap);}int Cyc_force_directory(struct _dyneither_ptr d);int Cyc_force_directory(
struct _dyneither_ptr d){if(Cyc_mode == (void*)2){const char*_tmp49C;void*_tmp49B[2];
struct Cyc_String_pa_struct _tmp49A;struct Cyc_String_pa_struct _tmp499;(_tmp499.tag=
0,((_tmp499.f1=(struct _dyneither_ptr)((struct _dyneither_ptr)d),((_tmp49A.tag=0,((
_tmp49A.f1=(struct _dyneither_ptr)((struct _dyneither_ptr)d),((_tmp49B[0]=& _tmp49A,((
_tmp49B[1]=& _tmp499,Cyc_prscript(((_tmp49C="if ! test -e %s; then mkdir %s; fi\n",
_tag_dyneither(_tmp49C,sizeof(char),36))),_tag_dyneither(_tmp49B,sizeof(void*),2)))))))))))));}
else{unsigned short _tmp49D[0];int _tmp176=Cyc_open((const char*)_check_null(
_untag_dyneither_ptr(d,sizeof(char),1)),0,_tag_dyneither(_tmp49D,sizeof(
unsigned short),0));if(_tmp176 == - 1){if(mkdir((const char*)_untag_dyneither_ptr(d,
sizeof(char),1),448)== - 1){{const char*_tmp4A1;void*_tmp4A0[1];struct Cyc_String_pa_struct
_tmp49F;(_tmp49F.tag=0,((_tmp49F.f1=(struct _dyneither_ptr)((struct _dyneither_ptr)
d),((_tmp4A0[0]=& _tmp49F,Cyc_fprintf(Cyc_stderr,((_tmp4A1="Error: could not create directory %s\n",
_tag_dyneither(_tmp4A1,sizeof(char),38))),_tag_dyneither(_tmp4A0,sizeof(void*),1)))))));}
return 1;}}else{close(_tmp176);}}return 0;}int Cyc_force_directory_prefixes(struct
_dyneither_ptr file);int Cyc_force_directory_prefixes(struct _dyneither_ptr file){
struct _dyneither_ptr _tmp17B=Cyc_strdup((struct _dyneither_ptr)file);struct Cyc_List_List*
_tmp17C=0;while(1){_tmp17B=Cyc_Filename_dirname((struct _dyneither_ptr)_tmp17B);
if(Cyc_strlen((struct _dyneither_ptr)_tmp17B)== 0)break;{struct _dyneither_ptr*
_tmp4A4;struct Cyc_List_List*_tmp4A3;_tmp17C=((_tmp4A3=_cycalloc(sizeof(*_tmp4A3)),((
_tmp4A3->hd=((_tmp4A4=_cycalloc(sizeof(*_tmp4A4)),((_tmp4A4[0]=(struct
_dyneither_ptr)_tmp17B,_tmp4A4)))),((_tmp4A3->tl=_tmp17C,_tmp4A3))))));}}for(0;
_tmp17C != 0;_tmp17C=_tmp17C->tl){if(Cyc_force_directory(*((struct _dyneither_ptr*)
_tmp17C->hd)))return 1;}return 0;}char Cyc_NO_SUPPORT[15]="\000\000\000\000NO_SUPPORT\000";
struct Cyc_NO_SUPPORT_struct{char*tag;struct _dyneither_ptr f1;};struct _tuple18{
struct Cyc_List_List*f1;struct Cyc_List_List*f2;};int Cyc_process_file(const char*
filename,struct Cyc_List_List*start_symbols,struct Cyc_List_List*omit_symbols,
struct Cyc_List_List*hstubs,struct Cyc_List_List*cstubs,struct Cyc_List_List*
cycstubs);int Cyc_process_file(const char*filename,struct Cyc_List_List*
start_symbols,struct Cyc_List_List*omit_symbols,struct Cyc_List_List*hstubs,struct
Cyc_List_List*cstubs,struct Cyc_List_List*cycstubs){struct Cyc___cycFILE*maybe;
struct Cyc___cycFILE*in_file;struct Cyc___cycFILE*out_file;int errorcode=0;{const
char*_tmp4AA;const char*_tmp4A9;void*_tmp4A8[1];struct Cyc_String_pa_struct _tmp4A7;(
_tmp4A7.tag=0,((_tmp4A7.f1=(struct _dyneither_ptr)((_tmp4AA=filename,
_tag_dyneither(_tmp4AA,sizeof(char),_get_zero_arr_size_char(_tmp4AA,1)))),((
_tmp4A8[0]=& _tmp4A7,Cyc_fprintf(Cyc_stderr,((_tmp4A9="********************************* %s...\n",
_tag_dyneither(_tmp4A9,sizeof(char),41))),_tag_dyneither(_tmp4A8,sizeof(void*),1)))))));}
if(!Cyc_gathering()){const char*_tmp4B0;const char*_tmp4AF;void*_tmp4AE[1];struct
Cyc_String_pa_struct _tmp4AD;(_tmp4AD.tag=0,((_tmp4AD.f1=(struct _dyneither_ptr)((
_tmp4B0=filename,_tag_dyneither(_tmp4B0,sizeof(char),_get_zero_arr_size_char(
_tmp4B0,1)))),((_tmp4AE[0]=& _tmp4AD,Cyc_log(((_tmp4AF="\n%s:\n",_tag_dyneither(
_tmp4AF,sizeof(char),6))),_tag_dyneither(_tmp4AE,sizeof(void*),1)))))));}{const
char*_tmp4B1;struct _dyneither_ptr _tmp187=Cyc_Filename_basename(((_tmp4B1=
filename,_tag_dyneither(_tmp4B1,sizeof(char),_get_zero_arr_size_char(_tmp4B1,1)))));
const char*_tmp4B2;struct _dyneither_ptr _tmp188=Cyc_Filename_dirname(((_tmp4B2=
filename,_tag_dyneither(_tmp4B2,sizeof(char),_get_zero_arr_size_char(_tmp4B2,1)))));
struct _dyneither_ptr _tmp189=Cyc_Filename_chop_extension((struct _dyneither_ptr)
_tmp187);const char*_tmp4B3;const char*_tmp18A=(const char*)_untag_dyneither_ptr(
Cyc_strconcat((struct _dyneither_ptr)_tmp189,((_tmp4B3=".iA",_tag_dyneither(
_tmp4B3,sizeof(char),4)))),sizeof(char),1);const char*_tmp4BB;void*_tmp4BA[1];
struct Cyc_String_pa_struct _tmp4B9;const char*_tmp4B7;void*_tmp4B6[1];struct Cyc_String_pa_struct
_tmp4B5;const char*_tmp18B=(const char*)_untag_dyneither_ptr(_get_dyneither_size(
_tmp188,sizeof(char))== 0?(_tmp4B9.tag=0,((_tmp4B9.f1=(struct _dyneither_ptr)((
struct _dyneither_ptr)_tmp189),((_tmp4BA[0]=& _tmp4B9,Cyc_aprintf(((_tmp4BB="%s.iB",
_tag_dyneither(_tmp4BB,sizeof(char),6))),_tag_dyneither(_tmp4BA,sizeof(void*),1))))))):
Cyc_Filename_concat((struct _dyneither_ptr)_tmp188,(struct _dyneither_ptr)((
_tmp4B5.tag=0,((_tmp4B5.f1=(struct _dyneither_ptr)((struct _dyneither_ptr)_tmp189),((
_tmp4B6[0]=& _tmp4B5,Cyc_aprintf(((_tmp4B7="%s.iB",_tag_dyneither(_tmp4B7,sizeof(
char),6))),_tag_dyneither(_tmp4B6,sizeof(void*),1))))))))),sizeof(char),1);const
char*_tmp4C3;void*_tmp4C2[1];struct Cyc_String_pa_struct _tmp4C1;const char*_tmp4BF;
void*_tmp4BE[1];struct Cyc_String_pa_struct _tmp4BD;const char*_tmp18C=(const char*)
_untag_dyneither_ptr(_get_dyneither_size(_tmp188,sizeof(char))== 0?(_tmp4C1.tag=
0,((_tmp4C1.f1=(struct _dyneither_ptr)((struct _dyneither_ptr)_tmp189),((_tmp4C2[0]=&
_tmp4C1,Cyc_aprintf(((_tmp4C3="%s.iC",_tag_dyneither(_tmp4C3,sizeof(char),6))),
_tag_dyneither(_tmp4C2,sizeof(void*),1))))))): Cyc_Filename_concat((struct
_dyneither_ptr)_tmp188,(struct _dyneither_ptr)((_tmp4BD.tag=0,((_tmp4BD.f1=(
struct _dyneither_ptr)((struct _dyneither_ptr)_tmp189),((_tmp4BE[0]=& _tmp4BD,Cyc_aprintf(((
_tmp4BF="%s.iC",_tag_dyneither(_tmp4BF,sizeof(char),6))),_tag_dyneither(_tmp4BE,
sizeof(void*),1))))))))),sizeof(char),1);const char*_tmp4CB;void*_tmp4CA[1];
struct Cyc_String_pa_struct _tmp4C9;const char*_tmp4C7;void*_tmp4C6[1];struct Cyc_String_pa_struct
_tmp4C5;const char*_tmp18D=(const char*)_untag_dyneither_ptr(_get_dyneither_size(
_tmp188,sizeof(char))== 0?(_tmp4C9.tag=0,((_tmp4C9.f1=(struct _dyneither_ptr)((
struct _dyneither_ptr)_tmp189),((_tmp4CA[0]=& _tmp4C9,Cyc_aprintf(((_tmp4CB="%s.iD",
_tag_dyneither(_tmp4CB,sizeof(char),6))),_tag_dyneither(_tmp4CA,sizeof(void*),1))))))):
Cyc_Filename_concat((struct _dyneither_ptr)_tmp188,(struct _dyneither_ptr)((
_tmp4C5.tag=0,((_tmp4C5.f1=(struct _dyneither_ptr)((struct _dyneither_ptr)_tmp189),((
_tmp4C6[0]=& _tmp4C5,Cyc_aprintf(((_tmp4C7="%s.iD",_tag_dyneither(_tmp4C7,sizeof(
char),6))),_tag_dyneither(_tmp4C6,sizeof(void*),1))))))))),sizeof(char),1);
struct _handler_cons _tmp18E;_push_handler(& _tmp18E);{int _tmp190=0;if(setjmp(
_tmp18E.handler))_tmp190=1;if(!_tmp190){{const char*_tmp4CC;if(Cyc_force_directory_prefixes(((
_tmp4CC=filename,_tag_dyneither(_tmp4CC,sizeof(char),_get_zero_arr_size_char(
_tmp4CC,1)))))){int _tmp192=1;_npop_handler(0);return _tmp192;}}if(Cyc_mode != (
void*)3){Cyc_current_cpp=((struct Cyc_List_List*(*)(struct Cyc_List_List*x))Cyc_List_imp_rev)(
Cyc_current_cpp);if(Cyc_mode == (void*)2){{const char*_tmp4D2;const char*_tmp4D1;
void*_tmp4D0[1];struct Cyc_String_pa_struct _tmp4CF;(_tmp4CF.tag=0,((_tmp4CF.f1=(
struct _dyneither_ptr)((_tmp4D2=_tmp18A,_tag_dyneither(_tmp4D2,sizeof(char),
_get_zero_arr_size_char(_tmp4D2,1)))),((_tmp4D0[0]=& _tmp4CF,Cyc_prscript(((
_tmp4D1="cat >%s <<XXX\n",_tag_dyneither(_tmp4D1,sizeof(char),15))),
_tag_dyneither(_tmp4D0,sizeof(void*),1)))))));}{struct Cyc_List_List*_tmp197=Cyc_current_cpp;
for(0;_tmp197 != 0;_tmp197=_tmp197->tl){const char*_tmp4D6;void*_tmp4D5[1];struct
Cyc_String_pa_struct _tmp4D4;(_tmp4D4.tag=0,((_tmp4D4.f1=(struct _dyneither_ptr)((
struct _dyneither_ptr)*((struct _dyneither_ptr*)_tmp197->hd)),((_tmp4D5[0]=&
_tmp4D4,Cyc_prscript(((_tmp4D6="%s",_tag_dyneither(_tmp4D6,sizeof(char),3))),
_tag_dyneither(_tmp4D5,sizeof(void*),1)))))));}}{const char*_tmp4DC;const char*
_tmp4DB;void*_tmp4DA[1];struct Cyc_String_pa_struct _tmp4D9;(_tmp4D9.tag=0,((
_tmp4D9.f1=(struct _dyneither_ptr)((_tmp4DC=filename,_tag_dyneither(_tmp4DC,
sizeof(char),_get_zero_arr_size_char(_tmp4DC,1)))),((_tmp4DA[0]=& _tmp4D9,Cyc_prscript(((
_tmp4DB="#include <%s>\n",_tag_dyneither(_tmp4DB,sizeof(char),15))),
_tag_dyneither(_tmp4DA,sizeof(void*),1)))))));}{const char*_tmp4DF;void*_tmp4DE;(
_tmp4DE=0,Cyc_prscript(((_tmp4DF="XXX\n",_tag_dyneither(_tmp4DF,sizeof(char),5))),
_tag_dyneither(_tmp4DE,sizeof(void*),0)));}{const char*_tmp4E8;const char*_tmp4E7;
const char*_tmp4E6;void*_tmp4E5[2];struct Cyc_String_pa_struct _tmp4E4;struct Cyc_String_pa_struct
_tmp4E3;(_tmp4E3.tag=0,((_tmp4E3.f1=(struct _dyneither_ptr)((_tmp4E8=_tmp18A,
_tag_dyneither(_tmp4E8,sizeof(char),_get_zero_arr_size_char(_tmp4E8,1)))),((
_tmp4E4.tag=0,((_tmp4E4.f1=(struct _dyneither_ptr)((_tmp4E7=_tmp18B,
_tag_dyneither(_tmp4E7,sizeof(char),_get_zero_arr_size_char(_tmp4E7,1)))),((
_tmp4E5[0]=& _tmp4E4,((_tmp4E5[1]=& _tmp4E3,Cyc_prscript(((_tmp4E6="$GCC -E -dM -o %s -x c %s && \\\n",
_tag_dyneither(_tmp4E6,sizeof(char),32))),_tag_dyneither(_tmp4E5,sizeof(void*),2)))))))))))));}{
const char*_tmp4F1;const char*_tmp4F0;const char*_tmp4EF;void*_tmp4EE[2];struct Cyc_String_pa_struct
_tmp4ED;struct Cyc_String_pa_struct _tmp4EC;(_tmp4EC.tag=0,((_tmp4EC.f1=(struct
_dyneither_ptr)((_tmp4F1=_tmp18A,_tag_dyneither(_tmp4F1,sizeof(char),
_get_zero_arr_size_char(_tmp4F1,1)))),((_tmp4ED.tag=0,((_tmp4ED.f1=(struct
_dyneither_ptr)((_tmp4F0=_tmp18C,_tag_dyneither(_tmp4F0,sizeof(char),
_get_zero_arr_size_char(_tmp4F0,1)))),((_tmp4EE[0]=& _tmp4ED,((_tmp4EE[1]=&
_tmp4EC,Cyc_prscript(((_tmp4EF="$GCC -E     -o %s -x c %s;\n",_tag_dyneither(
_tmp4EF,sizeof(char),28))),_tag_dyneither(_tmp4EE,sizeof(void*),2)))))))))))));}{
const char*_tmp4F7;const char*_tmp4F6;void*_tmp4F5[1];struct Cyc_String_pa_struct
_tmp4F4;(_tmp4F4.tag=0,((_tmp4F4.f1=(struct _dyneither_ptr)((_tmp4F7=_tmp18A,
_tag_dyneither(_tmp4F7,sizeof(char),_get_zero_arr_size_char(_tmp4F7,1)))),((
_tmp4F5[0]=& _tmp4F4,Cyc_prscript(((_tmp4F6="rm %s\n",_tag_dyneither(_tmp4F6,
sizeof(char),7))),_tag_dyneither(_tmp4F5,sizeof(void*),1)))))));}}else{maybe=Cyc_fopen(
_tmp18A,(const char*)"w");if(!((unsigned int)maybe)){{const char*_tmp4FD;const char*
_tmp4FC;void*_tmp4FB[1];struct Cyc_String_pa_struct _tmp4FA;(_tmp4FA.tag=0,((
_tmp4FA.f1=(struct _dyneither_ptr)((_tmp4FD=_tmp18A,_tag_dyneither(_tmp4FD,
sizeof(char),_get_zero_arr_size_char(_tmp4FD,1)))),((_tmp4FB[0]=& _tmp4FA,Cyc_fprintf(
Cyc_stderr,((_tmp4FC="Error: could not create file %s\n",_tag_dyneither(_tmp4FC,
sizeof(char),33))),_tag_dyneither(_tmp4FB,sizeof(void*),1)))))));}{int _tmp1B5=1;
_npop_handler(0);return _tmp1B5;}}out_file=(struct Cyc___cycFILE*)maybe;{struct Cyc_List_List*
_tmp1B6=Cyc_current_cpp;for(0;_tmp1B6 != 0;_tmp1B6=_tmp1B6->tl){Cyc_fputs((const
char*)_untag_dyneither_ptr(*((struct _dyneither_ptr*)_tmp1B6->hd),sizeof(char),1),
out_file);}}{const char*_tmp503;const char*_tmp502;void*_tmp501[1];struct Cyc_String_pa_struct
_tmp500;(_tmp500.tag=0,((_tmp500.f1=(struct _dyneither_ptr)((_tmp503=filename,
_tag_dyneither(_tmp503,sizeof(char),_get_zero_arr_size_char(_tmp503,1)))),((
_tmp501[0]=& _tmp500,Cyc_fprintf(out_file,((_tmp502="#include <%s>\n",
_tag_dyneither(_tmp502,sizeof(char),15))),_tag_dyneither(_tmp501,sizeof(void*),1)))))));}
Cyc_fclose(out_file);{struct _dyneither_ptr _tmp1BB=Cstring_to_string(Ccomp);const
char*_tmp50D;const char*_tmp50C;const char*_tmp50B;void*_tmp50A[3];struct Cyc_String_pa_struct
_tmp509;struct Cyc_String_pa_struct _tmp508;struct Cyc_String_pa_struct _tmp507;char*
cmd=(char*)_untag_dyneither_ptr(((_tmp507.tag=0,((_tmp507.f1=(struct
_dyneither_ptr)((_tmp50D=_tmp18A,_tag_dyneither(_tmp50D,sizeof(char),
_get_zero_arr_size_char(_tmp50D,1)))),((_tmp508.tag=0,((_tmp508.f1=(struct
_dyneither_ptr)((_tmp50C=_tmp18B,_tag_dyneither(_tmp50C,sizeof(char),
_get_zero_arr_size_char(_tmp50C,1)))),((_tmp509.tag=0,((_tmp509.f1=(struct
_dyneither_ptr)((struct _dyneither_ptr)_tmp1BB),((_tmp50A[0]=& _tmp509,((_tmp50A[1]=&
_tmp508,((_tmp50A[2]=& _tmp507,Cyc_aprintf(((_tmp50B="%s -E -dM -o %s -x c %s",
_tag_dyneither(_tmp50B,sizeof(char),24))),_tag_dyneither(_tmp50A,sizeof(void*),3)))))))))))))))))))),
sizeof(char),1);if(!system((const char*)cmd)){{const char*_tmp517;const char*
_tmp516;const char*_tmp515;void*_tmp514[3];struct Cyc_String_pa_struct _tmp513;
struct Cyc_String_pa_struct _tmp512;struct Cyc_String_pa_struct _tmp511;cmd=(char*)
_untag_dyneither_ptr(((_tmp511.tag=0,((_tmp511.f1=(struct _dyneither_ptr)((
_tmp517=_tmp18A,_tag_dyneither(_tmp517,sizeof(char),_get_zero_arr_size_char(
_tmp517,1)))),((_tmp512.tag=0,((_tmp512.f1=(struct _dyneither_ptr)((_tmp516=
_tmp18C,_tag_dyneither(_tmp516,sizeof(char),_get_zero_arr_size_char(_tmp516,1)))),((
_tmp513.tag=0,((_tmp513.f1=(struct _dyneither_ptr)((struct _dyneither_ptr)_tmp1BB),((
_tmp514[0]=& _tmp513,((_tmp514[1]=& _tmp512,((_tmp514[2]=& _tmp511,Cyc_aprintf(((
_tmp515="%s -E -o %s -x c %s",_tag_dyneither(_tmp515,sizeof(char),20))),
_tag_dyneither(_tmp514,sizeof(void*),3)))))))))))))))))))),sizeof(char),1);}
system((const char*)cmd);}remove(_tmp18A);}}}if(Cyc_gathering()){int _tmp1CA=0;
_npop_handler(0);return _tmp1CA;}{struct Cyc_Hashtable_Table*t=Cyc_new_deps();
maybe=Cyc_fopen(_tmp18B,(const char*)"r");if(!((unsigned int)maybe)){struct Cyc_NO_SUPPORT_struct
_tmp528;const char*_tmp527;const char*_tmp526;void*_tmp525[1];struct Cyc_String_pa_struct
_tmp524;struct Cyc_NO_SUPPORT_struct*_tmp523;(int)_throw((void*)((_tmp523=
_cycalloc(sizeof(*_tmp523)),((_tmp523[0]=((_tmp528.tag=Cyc_NO_SUPPORT,((_tmp528.f1=(
struct _dyneither_ptr)((_tmp524.tag=0,((_tmp524.f1=(struct _dyneither_ptr)((
_tmp527=_tmp18B,_tag_dyneither(_tmp527,sizeof(char),_get_zero_arr_size_char(
_tmp527,1)))),((_tmp525[0]=& _tmp524,Cyc_aprintf(((_tmp526="can't open macrosfile %s",
_tag_dyneither(_tmp526,sizeof(char),25))),_tag_dyneither(_tmp525,sizeof(void*),1)))))))),
_tmp528)))),_tmp523)))));}in_file=(struct Cyc___cycFILE*)maybe;{struct Cyc_Lexing_lexbuf*
_tmp1D1=Cyc_Lexing_from_file(in_file);struct _tuple12*entry;while((entry=((struct
_tuple12*(*)(struct Cyc_Lexing_lexbuf*lexbuf))Cyc_line)(_tmp1D1))!= 0){struct
_tuple12 _tmp1D3;struct _dyneither_ptr*_tmp1D4;struct Cyc_Set_Set*_tmp1D5;struct
_tuple12*_tmp1D2=(struct _tuple12*)_check_null(entry);_tmp1D3=*_tmp1D2;_tmp1D4=
_tmp1D3.f1;_tmp1D5=_tmp1D3.f2;((void(*)(struct Cyc_Hashtable_Table*t,struct
_dyneither_ptr*key,struct Cyc_Set_Set*val))Cyc_Hashtable_insert)(t,_tmp1D4,
_tmp1D5);}Cyc_fclose(in_file);maybe=Cyc_fopen(_tmp18C,(const char*)"r");if(!((
unsigned int)maybe)){struct Cyc_NO_SUPPORT_struct _tmp539;const char*_tmp538;const
char*_tmp537;void*_tmp536[1];struct Cyc_String_pa_struct _tmp535;struct Cyc_NO_SUPPORT_struct*
_tmp534;(int)_throw((void*)((_tmp534=_cycalloc(sizeof(*_tmp534)),((_tmp534[0]=((
_tmp539.tag=Cyc_NO_SUPPORT,((_tmp539.f1=(struct _dyneither_ptr)((_tmp535.tag=0,((
_tmp535.f1=(struct _dyneither_ptr)((_tmp538=_tmp18C,_tag_dyneither(_tmp538,
sizeof(char),_get_zero_arr_size_char(_tmp538,1)))),((_tmp536[0]=& _tmp535,Cyc_aprintf(((
_tmp537="can't open declsfile %s",_tag_dyneither(_tmp537,sizeof(char),24))),
_tag_dyneither(_tmp536,sizeof(void*),1)))))))),_tmp539)))),_tmp534)))));}in_file=(
struct Cyc___cycFILE*)maybe;_tmp1D1=Cyc_Lexing_from_file(in_file);Cyc_slurp_out=
Cyc_fopen(_tmp18D,(const char*)"w");if(!((unsigned int)Cyc_slurp_out)){int _tmp1DC=
1;_npop_handler(0);return _tmp1DC;}while(((int(*)(struct Cyc_Lexing_lexbuf*lexbuf))
Cyc_slurp)(_tmp1D1)){;}Cyc_fclose(in_file);Cyc_fclose((struct Cyc___cycFILE*)
_check_null(Cyc_slurp_out));if(Cyc_mode != (void*)3)remove(_tmp18C);maybe=Cyc_fopen(
_tmp18D,(const char*)"r");if(!((unsigned int)maybe)){int _tmp1DD=1;_npop_handler(0);
return _tmp1DD;}in_file=(struct Cyc___cycFILE*)maybe;{const char*_tmp53A;Cyc_Position_reset_position(((
_tmp53A=_tmp18D,_tag_dyneither(_tmp53A,sizeof(char),_get_zero_arr_size_char(
_tmp53A,1)))));}Cyc_Lex_lex_init(0);{struct Cyc_List_List*_tmp1DF=Cyc_Parse_parse_file(
in_file);Cyc_Lex_lex_init(0);Cyc_fclose(in_file);remove(_tmp18D);{struct Cyc_List_List*
_tmp1E0=_tmp1DF;for(0;_tmp1E0 != 0;_tmp1E0=_tmp1E0->tl){struct _tuple12*_tmp1E1=
Cyc_scan_decl((struct Cyc_Absyn_Decl*)_tmp1E0->hd);if(_tmp1E1 == 0)continue;{
struct _tuple12 _tmp1E3;struct _dyneither_ptr*_tmp1E4;struct Cyc_Set_Set*_tmp1E5;
struct _tuple12*_tmp1E2=(struct _tuple12*)_tmp1E1;_tmp1E3=*_tmp1E2;_tmp1E4=_tmp1E3.f1;
_tmp1E5=_tmp1E3.f2;{struct Cyc_Set_Set*old;{struct _handler_cons _tmp1E6;
_push_handler(& _tmp1E6);{int _tmp1E8=0;if(setjmp(_tmp1E6.handler))_tmp1E8=1;if(!
_tmp1E8){old=((struct Cyc_Set_Set*(*)(struct Cyc_Hashtable_Table*t,struct
_dyneither_ptr*key))Cyc_Hashtable_lookup)(t,_tmp1E4);;_pop_handler();}else{void*
_tmp1E7=(void*)_exn_thrown;void*_tmp1EA=_tmp1E7;_LL178: if(_tmp1EA != Cyc_Core_Not_found)
goto _LL17A;_LL179: old=((struct Cyc_Set_Set*(*)(int(*cmp)(struct _dyneither_ptr*,
struct _dyneither_ptr*)))Cyc_Set_empty)(Cyc_strptrcmp);goto _LL177;_LL17A:;_LL17B:(
void)_throw(_tmp1EA);_LL177:;}}}((void(*)(struct Cyc_Hashtable_Table*t,struct
_dyneither_ptr*key,struct Cyc_Set_Set*val))Cyc_Hashtable_insert)(t,_tmp1E4,((
struct Cyc_Set_Set*(*)(struct Cyc_Set_Set*s1,struct Cyc_Set_Set*s2))Cyc_Set_union_two)(
_tmp1E5,old));}}}}{struct Cyc_Set_Set*_tmp1EB=Cyc_reachable(start_symbols,t);
struct Cyc_List_List*_tmp1EC=0;struct Cyc_Set_Set*defined_symbols=((struct Cyc_Set_Set*(*)(
int(*cmp)(struct _dyneither_ptr*,struct _dyneither_ptr*)))Cyc_Set_empty)(Cyc_strptrcmp);{
struct Cyc_List_List*_tmp1ED=_tmp1DF;for(0;_tmp1ED != 0;_tmp1ED=_tmp1ED->tl){
struct Cyc_Absyn_Decl*_tmp1EE=(struct Cyc_Absyn_Decl*)_tmp1ED->hd;struct
_dyneither_ptr*name;{void*_tmp1EF=(void*)_tmp1EE->r;struct Cyc_Absyn_Vardecl*
_tmp1F0;struct Cyc_Absyn_Fndecl*_tmp1F1;struct Cyc_Absyn_Aggrdecl*_tmp1F2;struct
Cyc_Absyn_Enumdecl*_tmp1F3;struct Cyc_Absyn_Typedefdecl*_tmp1F4;_LL17D: if(_tmp1EF
<= (void*)2)goto _LL187;if(*((int*)_tmp1EF)!= 0)goto _LL17F;_tmp1F0=((struct Cyc_Absyn_Var_d_struct*)
_tmp1EF)->f1;_LL17E: {struct _tuple0 _tmp1F6;struct _dyneither_ptr*_tmp1F7;struct
_tuple0*_tmp1F5=_tmp1F0->name;_tmp1F6=*_tmp1F5;_tmp1F7=_tmp1F6.f2;
defined_symbols=((struct Cyc_Set_Set*(*)(struct Cyc_Set_Set*s,struct _dyneither_ptr*
elt))Cyc_Set_insert)(defined_symbols,_tmp1F7);if(((int(*)(int(*compare)(struct
_dyneither_ptr*,struct _dyneither_ptr*),struct Cyc_List_List*l,struct
_dyneither_ptr*x))Cyc_List_mem)(Cyc_strptrcmp,omit_symbols,_tmp1F7))name=0;else{
name=(struct _dyneither_ptr*)_tmp1F7;}goto _LL17C;}_LL17F: if(*((int*)_tmp1EF)!= 1)
goto _LL181;_tmp1F1=((struct Cyc_Absyn_Fn_d_struct*)_tmp1EF)->f1;_LL180: {struct
_tuple0 _tmp1F9;struct _dyneither_ptr*_tmp1FA;struct _tuple0*_tmp1F8=_tmp1F1->name;
_tmp1F9=*_tmp1F8;_tmp1FA=_tmp1F9.f2;defined_symbols=((struct Cyc_Set_Set*(*)(
struct Cyc_Set_Set*s,struct _dyneither_ptr*elt))Cyc_Set_insert)(defined_symbols,
_tmp1FA);if(((int(*)(int(*compare)(struct _dyneither_ptr*,struct _dyneither_ptr*),
struct Cyc_List_List*l,struct _dyneither_ptr*x))Cyc_List_mem)(Cyc_strptrcmp,
omit_symbols,_tmp1FA))name=0;else{name=(struct _dyneither_ptr*)_tmp1FA;}goto
_LL17C;}_LL181: if(*((int*)_tmp1EF)!= 4)goto _LL183;_tmp1F2=((struct Cyc_Absyn_Aggr_d_struct*)
_tmp1EF)->f1;_LL182: {struct _tuple0 _tmp1FC;struct _dyneither_ptr*_tmp1FD;struct
_tuple0*_tmp1FB=_tmp1F2->name;_tmp1FC=*_tmp1FB;_tmp1FD=_tmp1FC.f2;name=(struct
_dyneither_ptr*)_tmp1FD;goto _LL17C;}_LL183: if(*((int*)_tmp1EF)!= 6)goto _LL185;
_tmp1F3=((struct Cyc_Absyn_Enum_d_struct*)_tmp1EF)->f1;_LL184: {struct _tuple0
_tmp1FF;struct _dyneither_ptr*_tmp200;struct _tuple0*_tmp1FE=_tmp1F3->name;_tmp1FF=*
_tmp1FE;_tmp200=_tmp1FF.f2;name=(struct _dyneither_ptr*)_tmp200;if(name != 0  && ((
int(*)(struct Cyc_Set_Set*s,struct _dyneither_ptr*elt))Cyc_Set_member)(_tmp1EB,(
struct _dyneither_ptr*)name)){struct Cyc_List_List*_tmp53B;_tmp1EC=((_tmp53B=
_cycalloc(sizeof(*_tmp53B)),((_tmp53B->hd=_tmp1EE,((_tmp53B->tl=_tmp1EC,_tmp53B))))));}
else{if((unsigned int)_tmp1F3->fields){struct Cyc_List_List*_tmp202=(struct Cyc_List_List*)((
struct Cyc_Core_Opt*)_check_null(_tmp1F3->fields))->v;for(0;_tmp202 != 0;_tmp202=
_tmp202->tl){struct Cyc_Absyn_Enumfield*_tmp203=(struct Cyc_Absyn_Enumfield*)
_tmp202->hd;struct _tuple0 _tmp205;struct _dyneither_ptr*_tmp206;struct _tuple0*
_tmp204=_tmp203->name;_tmp205=*_tmp204;_tmp206=_tmp205.f2;if(((int(*)(struct Cyc_Set_Set*
s,struct _dyneither_ptr*elt))Cyc_Set_member)(_tmp1EB,_tmp206)){{struct Cyc_List_List*
_tmp53C;_tmp1EC=((_tmp53C=_cycalloc(sizeof(*_tmp53C)),((_tmp53C->hd=_tmp1EE,((
_tmp53C->tl=_tmp1EC,_tmp53C))))));}break;}}}}name=0;goto _LL17C;}_LL185: if(*((int*)
_tmp1EF)!= 7)goto _LL187;_tmp1F4=((struct Cyc_Absyn_Typedef_d_struct*)_tmp1EF)->f1;
_LL186: {struct _tuple0 _tmp209;struct _dyneither_ptr*_tmp20A;struct _tuple0*_tmp208=
_tmp1F4->name;_tmp209=*_tmp208;_tmp20A=_tmp209.f2;name=(struct _dyneither_ptr*)
_tmp20A;goto _LL17C;}_LL187: if((int)_tmp1EF != 0)goto _LL189;_LL188: goto _LL18A;
_LL189: if((int)_tmp1EF != 1)goto _LL18B;_LL18A: goto _LL18C;_LL18B: if(_tmp1EF <= (
void*)2)goto _LL18D;if(*((int*)_tmp1EF)!= 2)goto _LL18D;_LL18C: goto _LL18E;_LL18D:
if(_tmp1EF <= (void*)2)goto _LL18F;if(*((int*)_tmp1EF)!= 5)goto _LL18F;_LL18E: goto
_LL190;_LL18F: if(_tmp1EF <= (void*)2)goto _LL191;if(*((int*)_tmp1EF)!= 3)goto
_LL191;_LL190: goto _LL192;_LL191: if(_tmp1EF <= (void*)2)goto _LL193;if(*((int*)
_tmp1EF)!= 8)goto _LL193;_LL192: goto _LL194;_LL193: if(_tmp1EF <= (void*)2)goto
_LL195;if(*((int*)_tmp1EF)!= 9)goto _LL195;_LL194: goto _LL196;_LL195: if(_tmp1EF <= (
void*)2)goto _LL197;if(*((int*)_tmp1EF)!= 10)goto _LL197;_LL196: goto _LL198;_LL197:
if(_tmp1EF <= (void*)2)goto _LL17C;if(*((int*)_tmp1EF)!= 11)goto _LL17C;_LL198: name=
0;goto _LL17C;_LL17C:;}if(name != 0  && ((int(*)(struct Cyc_Set_Set*s,struct
_dyneither_ptr*elt))Cyc_Set_member)(_tmp1EB,(struct _dyneither_ptr*)name)){struct
Cyc_List_List*_tmp53D;_tmp1EC=((_tmp53D=_cycalloc(sizeof(*_tmp53D)),((_tmp53D->hd=
_tmp1EE,((_tmp53D->tl=_tmp1EC,_tmp53D))))));}}}if(!Cyc_do_setjmp){maybe=Cyc_fopen(
filename,(const char*)"w");if(!((unsigned int)maybe)){int _tmp20C=1;_npop_handler(
0);return _tmp20C;}out_file=(struct Cyc___cycFILE*)maybe;}else{out_file=Cyc_stdout;}{
const char*_tmp543;const char*_tmp542;void*_tmp541[1];struct Cyc_String_pa_struct
_tmp540;struct _dyneither_ptr ifdefmacro=(_tmp540.tag=0,((_tmp540.f1=(struct
_dyneither_ptr)((_tmp543=filename,_tag_dyneither(_tmp543,sizeof(char),
_get_zero_arr_size_char(_tmp543,1)))),((_tmp541[0]=& _tmp540,Cyc_aprintf(((
_tmp542="_%s_",_tag_dyneither(_tmp542,sizeof(char),5))),_tag_dyneither(_tmp541,
sizeof(void*),1)))))));{int _tmp20D=0;for(0;_tmp20D < _get_dyneither_size(
ifdefmacro,sizeof(char));++ _tmp20D){if(*((char*)_check_dyneither_subscript(
ifdefmacro,sizeof(char),_tmp20D))== '.'  || *((char*)_check_dyneither_subscript(
ifdefmacro,sizeof(char),_tmp20D))== '/'){char _tmp546;char _tmp545;struct
_dyneither_ptr _tmp544;(_tmp544=_dyneither_ptr_plus(ifdefmacro,sizeof(char),
_tmp20D),((_tmp545=*((char*)_check_dyneither_subscript(_tmp544,sizeof(char),0)),((
_tmp546='_',((_get_dyneither_size(_tmp544,sizeof(char))== 1  && (_tmp545 == '\000'
 && _tmp546 != '\000')?_throw_arraybounds(): 1,*((char*)_tmp544.curr)=_tmp546)))))));}
else{if(*((char*)_check_dyneither_subscript(ifdefmacro,sizeof(char),_tmp20D))!= '_'
 && *((char*)_check_dyneither_subscript(ifdefmacro,sizeof(char),_tmp20D))!= '/'){
char _tmp549;char _tmp548;struct _dyneither_ptr _tmp547;(_tmp547=_dyneither_ptr_plus(
ifdefmacro,sizeof(char),_tmp20D),((_tmp548=*((char*)_check_dyneither_subscript(
_tmp547,sizeof(char),0)),((_tmp549=(char)toupper((int)*((char*)
_check_dyneither_subscript(ifdefmacro,sizeof(char),_tmp20D))),((
_get_dyneither_size(_tmp547,sizeof(char))== 1  && (_tmp548 == '\000'  && _tmp549 != '\000')?
_throw_arraybounds(): 1,*((char*)_tmp547.curr)=_tmp549)))))));}}}}{const char*
_tmp54E;void*_tmp54D[2];struct Cyc_String_pa_struct _tmp54C;struct Cyc_String_pa_struct
_tmp54B;(_tmp54B.tag=0,((_tmp54B.f1=(struct _dyneither_ptr)((struct _dyneither_ptr)
ifdefmacro),((_tmp54C.tag=0,((_tmp54C.f1=(struct _dyneither_ptr)((struct
_dyneither_ptr)ifdefmacro),((_tmp54D[0]=& _tmp54C,((_tmp54D[1]=& _tmp54B,Cyc_fprintf(
out_file,((_tmp54E="#ifndef %s\n#define %s\n",_tag_dyneither(_tmp54E,sizeof(char),
23))),_tag_dyneither(_tmp54D,sizeof(void*),2)))))))))))));}{struct Cyc_List_List*
_tmp218=0;struct Cyc_List_List*_tmp219=0;{struct Cyc_List_List*_tmp21A=_tmp1EC;
for(0;_tmp21A != 0;_tmp21A=_tmp21A->tl){struct Cyc_Absyn_Decl*_tmp21B=(struct Cyc_Absyn_Decl*)
_tmp21A->hd;int _tmp21C=0;struct _dyneither_ptr*name;{void*_tmp21D=(void*)_tmp21B->r;
struct Cyc_Absyn_Vardecl*_tmp21E;struct Cyc_Absyn_Fndecl*_tmp21F;struct Cyc_Absyn_Aggrdecl*
_tmp220;struct Cyc_Absyn_Enumdecl*_tmp221;struct Cyc_Absyn_Typedefdecl*_tmp222;
_LL19A: if(_tmp21D <= (void*)2)goto _LL1A4;if(*((int*)_tmp21D)!= 0)goto _LL19C;
_tmp21E=((struct Cyc_Absyn_Var_d_struct*)_tmp21D)->f1;_LL19B: {struct _tuple0
_tmp224;struct _dyneither_ptr*_tmp225;struct _tuple0*_tmp223=_tmp21E->name;_tmp224=*
_tmp223;_tmp225=_tmp224.f2;name=(struct _dyneither_ptr*)_tmp225;goto _LL199;}
_LL19C: if(*((int*)_tmp21D)!= 1)goto _LL19E;_tmp21F=((struct Cyc_Absyn_Fn_d_struct*)
_tmp21D)->f1;_LL19D: {struct _tuple0 _tmp227;struct _dyneither_ptr*_tmp228;struct
_tuple0*_tmp226=_tmp21F->name;_tmp227=*_tmp226;_tmp228=_tmp227.f2;name=(struct
_dyneither_ptr*)_tmp228;goto _LL199;}_LL19E: if(*((int*)_tmp21D)!= 4)goto _LL1A0;
_tmp220=((struct Cyc_Absyn_Aggr_d_struct*)_tmp21D)->f1;_LL19F: {struct _tuple0
_tmp22A;struct _dyneither_ptr*_tmp22B;struct _tuple0*_tmp229=_tmp220->name;_tmp22A=*
_tmp229;_tmp22B=_tmp22A.f2;name=(struct _dyneither_ptr*)_tmp22B;goto _LL199;}
_LL1A0: if(*((int*)_tmp21D)!= 6)goto _LL1A2;_tmp221=((struct Cyc_Absyn_Enum_d_struct*)
_tmp21D)->f1;_LL1A1: {struct _tuple0 _tmp22D;struct _dyneither_ptr*_tmp22E;struct
_tuple0*_tmp22C=_tmp221->name;_tmp22D=*_tmp22C;_tmp22E=_tmp22D.f2;name=(struct
_dyneither_ptr*)_tmp22E;goto _LL199;}_LL1A2: if(*((int*)_tmp21D)!= 7)goto _LL1A4;
_tmp222=((struct Cyc_Absyn_Typedef_d_struct*)_tmp21D)->f1;_LL1A3: {struct _tuple0
_tmp230;struct _dyneither_ptr*_tmp231;struct _tuple0*_tmp22F=_tmp222->name;_tmp230=*
_tmp22F;_tmp231=_tmp230.f2;name=(struct _dyneither_ptr*)_tmp231;goto _LL199;}
_LL1A4: if((int)_tmp21D != 0)goto _LL1A6;_LL1A5: goto _LL1A7;_LL1A6: if((int)_tmp21D != 
1)goto _LL1A8;_LL1A7: goto _LL1A9;_LL1A8: if(_tmp21D <= (void*)2)goto _LL1AA;if(*((int*)
_tmp21D)!= 2)goto _LL1AA;_LL1A9: goto _LL1AB;_LL1AA: if(_tmp21D <= (void*)2)goto
_LL1AC;if(*((int*)_tmp21D)!= 5)goto _LL1AC;_LL1AB: goto _LL1AD;_LL1AC: if(_tmp21D <= (
void*)2)goto _LL1AE;if(*((int*)_tmp21D)!= 3)goto _LL1AE;_LL1AD: goto _LL1AF;_LL1AE:
if(_tmp21D <= (void*)2)goto _LL1B0;if(*((int*)_tmp21D)!= 8)goto _LL1B0;_LL1AF: goto
_LL1B1;_LL1B0: if(_tmp21D <= (void*)2)goto _LL1B2;if(*((int*)_tmp21D)!= 9)goto
_LL1B2;_LL1B1: goto _LL1B3;_LL1B2: if(_tmp21D <= (void*)2)goto _LL1B4;if(*((int*)
_tmp21D)!= 10)goto _LL1B4;_LL1B3: goto _LL1B5;_LL1B4: if(_tmp21D <= (void*)2)goto
_LL199;if(*((int*)_tmp21D)!= 11)goto _LL199;_LL1B5: name=0;goto _LL199;_LL199:;}if(
!((unsigned int)name) && !_tmp21C)continue;{struct Cyc_List_List*_tmp54F;_tmp218=((
_tmp54F=_cycalloc(sizeof(*_tmp54F)),((_tmp54F->hd=_tmp21B,((_tmp54F->tl=_tmp218,
_tmp54F))))));}{struct Cyc_List_List*_tmp550;_tmp219=((_tmp550=_cycalloc(sizeof(*
_tmp550)),((_tmp550->hd=name,((_tmp550->tl=_tmp219,_tmp550))))));}}}{struct
_RegionHandle _tmp234=_new_region("tc_rgn");struct _RegionHandle*tc_rgn=& _tmp234;
_push_region(tc_rgn);{struct Cyc_Tcenv_Tenv*_tmp235=Cyc_Tcenv_tc_init(tc_rgn);Cyc_Tc_tc(
tc_rgn,_tmp235,1,_tmp218);};_pop_region(tc_rgn);}{struct Cyc_List_List*_tmp238;
struct Cyc_List_List*_tmp239;struct _tuple18 _tmp551;struct _tuple18 _tmp237=(_tmp551.f1=
_tmp218,((_tmp551.f2=_tmp219,_tmp551)));_tmp238=_tmp237.f1;_tmp239=_tmp237.f2;
for(0;_tmp238 != 0  && _tmp239 != 0;(_tmp238=_tmp238->tl,_tmp239=_tmp239->tl)){
struct Cyc_Absyn_Decl*_tmp23A=(struct Cyc_Absyn_Decl*)_tmp238->hd;struct
_dyneither_ptr*_tmp23B=(struct _dyneither_ptr*)_tmp239->hd;int _tmp23C=0;if(!((
unsigned int)_tmp23B))_tmp23C=1;if((unsigned int)_tmp23B){{const char*_tmp555;
void*_tmp554[1];struct Cyc_String_pa_struct _tmp553;ifdefmacro=((_tmp553.tag=0,((
_tmp553.f1=(struct _dyneither_ptr)((struct _dyneither_ptr)*_tmp23B),((_tmp554[0]=&
_tmp553,Cyc_aprintf(((_tmp555="_%s_def_",_tag_dyneither(_tmp555,sizeof(char),9))),
_tag_dyneither(_tmp554,sizeof(void*),1))))))));}{const char*_tmp559;void*_tmp558[
1];struct Cyc_String_pa_struct _tmp557;(_tmp557.tag=0,((_tmp557.f1=(struct
_dyneither_ptr)((struct _dyneither_ptr)ifdefmacro),((_tmp558[0]=& _tmp557,Cyc_fprintf(
out_file,((_tmp559="#ifndef %s\n",_tag_dyneither(_tmp559,sizeof(char),12))),
_tag_dyneither(_tmp558,sizeof(void*),1)))))));}{const char*_tmp55D;void*_tmp55C[1];
struct Cyc_String_pa_struct _tmp55B;(_tmp55B.tag=0,((_tmp55B.f1=(struct
_dyneither_ptr)((struct _dyneither_ptr)ifdefmacro),((_tmp55C[0]=& _tmp55B,Cyc_fprintf(
out_file,((_tmp55D="#define %s\n",_tag_dyneither(_tmp55D,sizeof(char),12))),
_tag_dyneither(_tmp55C,sizeof(void*),1)))))));}{struct Cyc_Absyn_Decl*_tmp55E[1];
Cyc_Absynpp_decllist2file(((_tmp55E[0]=_tmp23A,((struct Cyc_List_List*(*)(struct
_dyneither_ptr))Cyc_List_list)(_tag_dyneither(_tmp55E,sizeof(struct Cyc_Absyn_Decl*),
1)))),out_file);}{const char*_tmp561;void*_tmp560;(_tmp560=0,Cyc_fprintf(out_file,((
_tmp561="#endif\n",_tag_dyneither(_tmp561,sizeof(char),8))),_tag_dyneither(
_tmp560,sizeof(void*),0)));}}else{struct Cyc_Absyn_Decl*_tmp562[1];Cyc_Absynpp_decllist2file(((
_tmp562[0]=_tmp23A,((struct Cyc_List_List*(*)(struct _dyneither_ptr))Cyc_List_list)(
_tag_dyneither(_tmp562,sizeof(struct Cyc_Absyn_Decl*),1)))),out_file);}}}maybe=
Cyc_fopen(_tmp18B,(const char*)"r");if(!((unsigned int)maybe)){struct Cyc_NO_SUPPORT_struct
_tmp573;const char*_tmp572;const char*_tmp571;void*_tmp570[1];struct Cyc_String_pa_struct
_tmp56F;struct Cyc_NO_SUPPORT_struct*_tmp56E;(int)_throw((void*)((_tmp56E=
_cycalloc(sizeof(*_tmp56E)),((_tmp56E[0]=((_tmp573.tag=Cyc_NO_SUPPORT,((_tmp573.f1=(
struct _dyneither_ptr)((_tmp56F.tag=0,((_tmp56F.f1=(struct _dyneither_ptr)((
_tmp572=_tmp18B,_tag_dyneither(_tmp572,sizeof(char),_get_zero_arr_size_char(
_tmp572,1)))),((_tmp570[0]=& _tmp56F,Cyc_aprintf(((_tmp571="can't open macrosfile %s",
_tag_dyneither(_tmp571,sizeof(char),25))),_tag_dyneither(_tmp570,sizeof(void*),1)))))))),
_tmp573)))),_tmp56E)))));}in_file=(struct Cyc___cycFILE*)maybe;_tmp1D1=Cyc_Lexing_from_file(
in_file);{struct _tuple13*entry2;while((entry2=((struct _tuple13*(*)(struct Cyc_Lexing_lexbuf*
lexbuf))Cyc_suck_line)(_tmp1D1))!= 0){struct _tuple13 _tmp251;struct _dyneither_ptr
_tmp252;struct _dyneither_ptr*_tmp253;struct _tuple13*_tmp250=(struct _tuple13*)
_check_null(entry2);_tmp251=*_tmp250;_tmp252=_tmp251.f1;_tmp253=_tmp251.f2;if(((
int(*)(struct Cyc_Set_Set*s,struct _dyneither_ptr*elt))Cyc_Set_member)(_tmp1EB,
_tmp253)){{const char*_tmp577;void*_tmp576[1];struct Cyc_String_pa_struct _tmp575;(
_tmp575.tag=0,((_tmp575.f1=(struct _dyneither_ptr)((struct _dyneither_ptr)*_tmp253),((
_tmp576[0]=& _tmp575,Cyc_fprintf(out_file,((_tmp577="#ifndef %s\n",_tag_dyneither(
_tmp577,sizeof(char),12))),_tag_dyneither(_tmp576,sizeof(void*),1)))))));}{const
char*_tmp57B;void*_tmp57A[1];struct Cyc_String_pa_struct _tmp579;(_tmp579.tag=0,((
_tmp579.f1=(struct _dyneither_ptr)((struct _dyneither_ptr)_tmp252),((_tmp57A[0]=&
_tmp579,Cyc_fprintf(out_file,((_tmp57B="%s\n",_tag_dyneither(_tmp57B,sizeof(char),
4))),_tag_dyneither(_tmp57A,sizeof(void*),1)))))));}{const char*_tmp57E;void*
_tmp57D;(_tmp57D=0,Cyc_fprintf(out_file,((_tmp57E="#endif\n",_tag_dyneither(
_tmp57E,sizeof(char),8))),_tag_dyneither(_tmp57D,sizeof(void*),0)));}}}Cyc_fclose(
in_file);if(Cyc_mode != (void*)3)remove(_tmp18B);if(hstubs != 0){struct Cyc_List_List*
_tmp25C=hstubs;for(0;_tmp25C != 0;_tmp25C=_tmp25C->tl){struct _tuple14 _tmp25E;
struct _dyneither_ptr _tmp25F;struct _dyneither_ptr _tmp260;struct _tuple14*_tmp25D=(
struct _tuple14*)_tmp25C->hd;_tmp25E=*_tmp25D;_tmp25F=_tmp25E.f1;_tmp260=_tmp25E.f2;{
struct _dyneither_ptr*_tmp57F;if(_tmp260.curr != ((struct _dyneither_ptr)
_tag_dyneither(0,0,0)).curr  && (_tmp25F.curr == ((struct _dyneither_ptr)
_tag_dyneither(0,0,0)).curr  || ((int(*)(struct Cyc_Set_Set*s,struct _dyneither_ptr*
elt))Cyc_Set_member)(defined_symbols,((_tmp57F=_cycalloc(sizeof(*_tmp57F)),((
_tmp57F[0]=_tmp25F,_tmp57F)))))))Cyc_fputs((const char*)_untag_dyneither_ptr(
_tmp260,sizeof(char),1),out_file);else{const char*_tmp583;void*_tmp582[1];struct
Cyc_String_pa_struct _tmp581;(_tmp581.tag=0,((_tmp581.f1=(struct _dyneither_ptr)((
struct _dyneither_ptr)_tmp25F),((_tmp582[0]=& _tmp581,Cyc_log(((_tmp583="%s is not supported on this platform\n",
_tag_dyneither(_tmp583,sizeof(char),38))),_tag_dyneither(_tmp582,sizeof(void*),1)))))));}}}}{
const char*_tmp586;void*_tmp585;(_tmp585=0,Cyc_fprintf(out_file,((_tmp586="#endif\n",
_tag_dyneither(_tmp586,sizeof(char),8))),_tag_dyneither(_tmp585,sizeof(void*),0)));}
if(Cyc_do_setjmp){int _tmp267=0;_npop_handler(0);return _tmp267;}else{Cyc_fclose(
out_file);}if(cstubs != 0){out_file=(struct Cyc___cycFILE*)_check_null(Cyc_cstubs_file);{
struct Cyc_List_List*_tmp268=cstubs;for(0;_tmp268 != 0;_tmp268=_tmp268->tl){struct
_tuple14 _tmp26A;struct _dyneither_ptr _tmp26B;struct _dyneither_ptr _tmp26C;struct
_tuple14*_tmp269=(struct _tuple14*)_tmp268->hd;_tmp26A=*_tmp269;_tmp26B=_tmp26A.f1;
_tmp26C=_tmp26A.f2;{struct _dyneither_ptr*_tmp587;if(_tmp26C.curr != ((struct
_dyneither_ptr)_tag_dyneither(0,0,0)).curr  && (_tmp26B.curr == ((struct
_dyneither_ptr)_tag_dyneither(0,0,0)).curr  || ((int(*)(struct Cyc_Set_Set*s,
struct _dyneither_ptr*elt))Cyc_Set_member)(defined_symbols,((_tmp587=_cycalloc(
sizeof(*_tmp587)),((_tmp587[0]=_tmp26B,_tmp587)))))))Cyc_fputs((const char*)
_untag_dyneither_ptr(_tmp26C,sizeof(char),1),out_file);}}}}out_file=(struct Cyc___cycFILE*)
_check_null(Cyc_cycstubs_file);{const char*_tmp58D;const char*_tmp58C;void*_tmp58B[
1];struct Cyc_String_pa_struct _tmp58A;(_tmp58A.tag=0,((_tmp58A.f1=(struct
_dyneither_ptr)((_tmp58D=filename,_tag_dyneither(_tmp58D,sizeof(char),
_get_zero_arr_size_char(_tmp58D,1)))),((_tmp58B[0]=& _tmp58A,Cyc_fprintf(out_file,((
_tmp58C="#include <%s>\n\n",_tag_dyneither(_tmp58C,sizeof(char),16))),
_tag_dyneither(_tmp58B,sizeof(void*),1)))))));}if(cycstubs != 0){out_file=(struct
Cyc___cycFILE*)_check_null(Cyc_cycstubs_file);{struct Cyc_List_List*_tmp272=
cycstubs;for(0;_tmp272 != 0;_tmp272=_tmp272->tl){struct _tuple14 _tmp274;struct
_dyneither_ptr _tmp275;struct _dyneither_ptr _tmp276;struct _tuple14*_tmp273=(struct
_tuple14*)_tmp272->hd;_tmp274=*_tmp273;_tmp275=_tmp274.f1;_tmp276=_tmp274.f2;{
struct _dyneither_ptr*_tmp58E;if(_tmp276.curr != ((struct _dyneither_ptr)
_tag_dyneither(0,0,0)).curr  && (_tmp275.curr == ((struct _dyneither_ptr)
_tag_dyneither(0,0,0)).curr  || ((int(*)(struct Cyc_Set_Set*s,struct _dyneither_ptr*
elt))Cyc_Set_member)(defined_symbols,((_tmp58E=_cycalloc(sizeof(*_tmp58E)),((
_tmp58E[0]=_tmp275,_tmp58E)))))))Cyc_fputs((const char*)_untag_dyneither_ptr(
_tmp276,sizeof(char),1),out_file);}}}{const char*_tmp591;void*_tmp590;(_tmp590=0,
Cyc_fprintf(out_file,((_tmp591="\n",_tag_dyneither(_tmp591,sizeof(char),2))),
_tag_dyneither(_tmp590,sizeof(void*),0)));}}{int _tmp27A=0;_npop_handler(0);
return _tmp27A;}}}}}}}};_pop_handler();}else{void*_tmp18F=(void*)_exn_thrown;void*
_tmp280=_tmp18F;struct _dyneither_ptr _tmp281;_LL1B7: if(*((void**)_tmp280)!= Cyc_NO_SUPPORT)
goto _LL1B9;_tmp281=((struct Cyc_NO_SUPPORT_struct*)_tmp280)->f1;_LL1B8:{const char*
_tmp595;void*_tmp594[1];struct Cyc_String_pa_struct _tmp593;(_tmp593.tag=0,((
_tmp593.f1=(struct _dyneither_ptr)((struct _dyneither_ptr)_tmp281),((_tmp594[0]=&
_tmp593,Cyc_fprintf(Cyc_stderr,((_tmp595="No support because %s\n",
_tag_dyneither(_tmp595,sizeof(char),23))),_tag_dyneither(_tmp594,sizeof(void*),1)))))));}
goto _LL1BA;_LL1B9:;_LL1BA: maybe=Cyc_fopen(filename,(const char*)"w");if(!((
unsigned int)maybe)){{const char*_tmp59B;const char*_tmp59A;void*_tmp599[1];struct
Cyc_String_pa_struct _tmp598;(_tmp598.tag=0,((_tmp598.f1=(struct _dyneither_ptr)((
_tmp59B=filename,_tag_dyneither(_tmp59B,sizeof(char),_get_zero_arr_size_char(
_tmp59B,1)))),((_tmp599[0]=& _tmp598,Cyc_fprintf(Cyc_stderr,((_tmp59A="Error: could not create file %s\n",
_tag_dyneither(_tmp59A,sizeof(char),33))),_tag_dyneither(_tmp599,sizeof(void*),1)))))));}
return 1;}out_file=(struct Cyc___cycFILE*)maybe;{const char*_tmp5A1;const char*
_tmp5A0;void*_tmp59F[1];struct Cyc_String_pa_struct _tmp59E;(_tmp59E.tag=0,((
_tmp59E.f1=(struct _dyneither_ptr)((_tmp5A1=filename,_tag_dyneither(_tmp5A1,
sizeof(char),_get_zero_arr_size_char(_tmp5A1,1)))),((_tmp59F[0]=& _tmp59E,Cyc_fprintf(
out_file,((_tmp5A0="#error -- %s is not supported on this platform\n",
_tag_dyneither(_tmp5A0,sizeof(char),48))),_tag_dyneither(_tmp59F,sizeof(void*),1)))))));}
Cyc_fclose(out_file);{const char*_tmp5A7;const char*_tmp5A6;void*_tmp5A5[1];struct
Cyc_String_pa_struct _tmp5A4;(_tmp5A4.tag=0,((_tmp5A4.f1=(struct _dyneither_ptr)((
_tmp5A7=filename,_tag_dyneither(_tmp5A7,sizeof(char),_get_zero_arr_size_char(
_tmp5A7,1)))),((_tmp5A5[0]=& _tmp5A4,Cyc_fprintf(Cyc_stderr,((_tmp5A6="Warning: %s will not be supported on this platform\n",
_tag_dyneither(_tmp5A6,sizeof(char),52))),_tag_dyneither(_tmp5A5,sizeof(void*),1)))))));}{
const char*_tmp5AA;void*_tmp5A9;(_tmp5A9=0,Cyc_log(((_tmp5AA="Not supported on this platform\n",
_tag_dyneither(_tmp5AA,sizeof(char),32))),_tag_dyneither(_tmp5A9,sizeof(void*),0)));}
remove(_tmp18B);remove(_tmp18C);remove(_tmp18D);return 0;_LL1BB:;_LL1BC:(void)
_throw(_tmp280);_LL1B6:;}}}}int Cyc_process_specfile(const char*file,const char*dir);
static void _tmp5B5(unsigned int*_tmp5B4,unsigned int*_tmp5B3,char**_tmp5B2){for(*
_tmp5B4=0;*_tmp5B4 < *_tmp5B3;(*_tmp5B4)++){(*_tmp5B2)[*_tmp5B4]='\000';}}int Cyc_process_specfile(
const char*file,const char*dir){struct Cyc___cycFILE*_tmp2A8=Cyc_fopen(file,(const
char*)"r");if(!((unsigned int)_tmp2A8)){{const char*_tmp5B0;const char*_tmp5AF;
void*_tmp5AE[1];struct Cyc_String_pa_struct _tmp5AD;(_tmp5AD.tag=0,((_tmp5AD.f1=(
struct _dyneither_ptr)((_tmp5B0=file,_tag_dyneither(_tmp5B0,sizeof(char),
_get_zero_arr_size_char(_tmp5B0,1)))),((_tmp5AE[0]=& _tmp5AD,Cyc_fprintf(Cyc_stderr,((
_tmp5AF="Error: could not open %s\n",_tag_dyneither(_tmp5AF,sizeof(char),26))),
_tag_dyneither(_tmp5AE,sizeof(void*),1)))))));}return 1;}{struct Cyc___cycFILE*
_tmp2AD=(struct Cyc___cycFILE*)_tmp2A8;char*_tmp5BA;unsigned int _tmp5B9;char*
_tmp5B8;unsigned int _tmp5B7;unsigned int _tmp5B6;struct _dyneither_ptr buf=(_tmp5BA=((
_tmp5B9=(unsigned int)1024,((_tmp5B8=(char*)_cycalloc_atomic(_check_times(
sizeof(char),_tmp5B9 + 1)),((((_tmp5B7=_tmp5B9,((_tmp5B5(& _tmp5B6,& _tmp5B7,&
_tmp5B8),_tmp5B8[_tmp5B7]=(char)0)))),_tmp5B8)))))),_tag_dyneither(_tmp5BA,
sizeof(char),_get_zero_arr_size_char(_tmp5BA,(unsigned int)1024 + 1)));struct
_dyneither_ptr _tmp2AE=Cyc_getcwd(buf,_get_dyneither_size(buf,sizeof(char)));if(
Cyc_mode != (void*)2){if(chdir(dir)){{const char*_tmp5C0;const char*_tmp5BF;void*
_tmp5BE[1];struct Cyc_String_pa_struct _tmp5BD;(_tmp5BD.tag=0,((_tmp5BD.f1=(struct
_dyneither_ptr)((_tmp5C0=dir,_tag_dyneither(_tmp5C0,sizeof(char),
_get_zero_arr_size_char(_tmp5C0,1)))),((_tmp5BE[0]=& _tmp5BD,Cyc_fprintf(Cyc_stderr,((
_tmp5BF="Error: can't change directory to %s\n",_tag_dyneither(_tmp5BF,sizeof(
char),37))),_tag_dyneither(_tmp5BE,sizeof(void*),1)))))));}return 1;}}if(Cyc_mode
== (void*)1){struct _dyneither_ptr _tmp2B3=Cstring_to_string(Ccomp);const char*
_tmp5C4;void*_tmp5C3[1];struct Cyc_String_pa_struct _tmp5C2;system((const char*)
_untag_dyneither_ptr(((_tmp5C2.tag=0,((_tmp5C2.f1=(struct _dyneither_ptr)((struct
_dyneither_ptr)_tmp2B3),((_tmp5C3[0]=& _tmp5C2,Cyc_aprintf(((_tmp5C4="echo | %s -E -dM - -o INITMACROS.h\n",
_tag_dyneither(_tmp5C4,sizeof(char),36))),_tag_dyneither(_tmp5C3,sizeof(void*),1)))))))),
sizeof(char),1));}{struct Cyc_Lexing_lexbuf*_tmp2B7=Cyc_Lexing_from_file(_tmp2AD);
struct _tuple15*entry;while((entry=((struct _tuple15*(*)(struct Cyc_Lexing_lexbuf*
lexbuf))Cyc_spec)(_tmp2B7))!= 0){struct _tuple15 _tmp2B9;struct _dyneither_ptr
_tmp2BA;struct Cyc_List_List*_tmp2BB;struct Cyc_List_List*_tmp2BC;struct Cyc_List_List*
_tmp2BD;struct Cyc_List_List*_tmp2BE;struct Cyc_List_List*_tmp2BF;struct _tuple15*
_tmp2B8=(struct _tuple15*)_check_null(entry);_tmp2B9=*_tmp2B8;_tmp2BA=_tmp2B9.f1;
_tmp2BB=_tmp2B9.f2;_tmp2BC=_tmp2B9.f3;_tmp2BD=_tmp2B9.f4;_tmp2BE=_tmp2B9.f5;
_tmp2BF=_tmp2B9.f6;if(Cyc_process_file((const char*)_untag_dyneither_ptr(_tmp2BA,
sizeof(char),1),_tmp2BB,_tmp2BC,_tmp2BD,_tmp2BE,_tmp2BF))return 1;}Cyc_fclose(
_tmp2AD);if(Cyc_mode != (void*)2){if(chdir((const char*)((char*)
_untag_dyneither_ptr(_tmp2AE,sizeof(char),1)))){{const char*_tmp5C8;void*_tmp5C7[
1];struct Cyc_String_pa_struct _tmp5C6;(_tmp5C6.tag=0,((_tmp5C6.f1=(struct
_dyneither_ptr)((struct _dyneither_ptr)_tmp2AE),((_tmp5C7[0]=& _tmp5C6,Cyc_fprintf(
Cyc_stderr,((_tmp5C8="Error: could not change directory to %s\n",_tag_dyneither(
_tmp5C8,sizeof(char),41))),_tag_dyneither(_tmp5C7,sizeof(void*),1)))))));}return
1;}}return 0;}}}int Cyc_process_setjmp(const char*dir);static void _tmp5CD(
unsigned int*_tmp5CC,unsigned int*_tmp5CB,char**_tmp5CA){for(*_tmp5CC=0;*_tmp5CC
< *_tmp5CB;(*_tmp5CC)++){(*_tmp5CA)[*_tmp5CC]='\000';}}int Cyc_process_setjmp(
const char*dir){char*_tmp5D2;unsigned int _tmp5D1;char*_tmp5D0;unsigned int _tmp5CF;
unsigned int _tmp5CE;struct _dyneither_ptr buf=(_tmp5D2=((_tmp5D1=(unsigned int)
1024,((_tmp5D0=(char*)_cycalloc_atomic(_check_times(sizeof(char),_tmp5D1 + 1)),((((
_tmp5CF=_tmp5D1,((_tmp5CD(& _tmp5CE,& _tmp5CF,& _tmp5D0),_tmp5D0[_tmp5CF]=(char)0)))),
_tmp5D0)))))),_tag_dyneither(_tmp5D2,sizeof(char),_get_zero_arr_size_char(
_tmp5D2,(unsigned int)1024 + 1)));struct _dyneither_ptr _tmp2C7=Cyc_getcwd(buf,
_get_dyneither_size(buf,sizeof(char)));if(chdir(dir)){{const char*_tmp5D8;const
char*_tmp5D7;void*_tmp5D6[1];struct Cyc_String_pa_struct _tmp5D5;(_tmp5D5.tag=0,((
_tmp5D5.f1=(struct _dyneither_ptr)((_tmp5D8=dir,_tag_dyneither(_tmp5D8,sizeof(
char),_get_zero_arr_size_char(_tmp5D8,1)))),((_tmp5D6[0]=& _tmp5D5,Cyc_fprintf(
Cyc_stderr,((_tmp5D7="Error: can't change directory to %s\n",_tag_dyneither(
_tmp5D7,sizeof(char),37))),_tag_dyneither(_tmp5D6,sizeof(void*),1)))))));}return
1;}{struct _tuple14*_tmp5E2;const char*_tmp5E1;const char*_tmp5E0;struct _tuple14*
_tmp5DF[1];struct _dyneither_ptr*_tmp5D9[1];if(Cyc_process_file((const char*)"setjmp.h",((
_tmp5D9[0]=_init_dyneither_ptr(_cycalloc(sizeof(struct _dyneither_ptr)),"jmp_buf",
sizeof(char),8),((struct Cyc_List_List*(*)(struct _dyneither_ptr))Cyc_List_list)(
_tag_dyneither(_tmp5D9,sizeof(struct _dyneither_ptr*),1)))),0,((_tmp5DF[0]=((
_tmp5E2=_cycalloc(sizeof(*_tmp5E2)),((_tmp5E2->f1=((_tmp5E0="setjmp",
_tag_dyneither(_tmp5E0,sizeof(char),7))),((_tmp5E2->f2=((_tmp5E1="extern int setjmp(jmp_buf);\n",
_tag_dyneither(_tmp5E1,sizeof(char),29))),_tmp5E2)))))),((struct Cyc_List_List*(*)(
struct _dyneither_ptr))Cyc_List_list)(_tag_dyneither(_tmp5DF,sizeof(struct
_tuple14*),1)))),0,0))return 1;}if(chdir((const char*)((char*)_untag_dyneither_ptr(
_tmp2C7,sizeof(char),1)))){{const char*_tmp5E6;void*_tmp5E5[1];struct Cyc_String_pa_struct
_tmp5E4;(_tmp5E4.tag=0,((_tmp5E4.f1=(struct _dyneither_ptr)((struct _dyneither_ptr)
_tmp2C7),((_tmp5E5[0]=& _tmp5E4,Cyc_fprintf(Cyc_stderr,((_tmp5E6="Error: could not change directory to %s\n",
_tag_dyneither(_tmp5E6,sizeof(char),41))),_tag_dyneither(_tmp5E5,sizeof(void*),1)))))));}
return 1;}return 0;}static char _tmp2D9[13]="BUILDLIB.OUT";static struct
_dyneither_ptr Cyc_output_dir={_tmp2D9,_tmp2D9,_tmp2D9 + 13};static void Cyc_set_output_dir(
struct _dyneither_ptr s);static void Cyc_set_output_dir(struct _dyneither_ptr s){Cyc_output_dir=
s;}static struct Cyc_List_List*Cyc_spec_files=0;static void Cyc_add_spec_file(struct
_dyneither_ptr s);static void Cyc_add_spec_file(struct _dyneither_ptr s){struct Cyc_List_List*
_tmp5E7;Cyc_spec_files=((_tmp5E7=_cycalloc(sizeof(*_tmp5E7)),((_tmp5E7->hd=(
const char*)_untag_dyneither_ptr(s,sizeof(char),1),((_tmp5E7->tl=Cyc_spec_files,
_tmp5E7))))));}static void Cyc_set_GATHER();static void Cyc_set_GATHER(){Cyc_mode=(
void*)1;}static void Cyc_set_GATHERSCRIPT();static void Cyc_set_GATHERSCRIPT(){Cyc_mode=(
void*)2;}static void Cyc_set_FINISH();static void Cyc_set_FINISH(){Cyc_mode=(void*)3;}
static int Cyc_badparse=0;static void Cyc_unsupported_option(struct _dyneither_ptr s);
static void Cyc_unsupported_option(struct _dyneither_ptr s){{const char*_tmp5EB;void*
_tmp5EA[1];struct Cyc_String_pa_struct _tmp5E9;(_tmp5E9.tag=0,((_tmp5E9.f1=(struct
_dyneither_ptr)((struct _dyneither_ptr)s),((_tmp5EA[0]=& _tmp5E9,Cyc_fprintf(Cyc_stderr,((
_tmp5EB="Unsupported option %s\n",_tag_dyneither(_tmp5EB,sizeof(char),23))),
_tag_dyneither(_tmp5EA,sizeof(void*),1)))))));}Cyc_badparse=1;}void
GC_blacklist_warn_clear();struct _tuple19{struct _dyneither_ptr f1;int f2;struct
_dyneither_ptr f3;void*f4;struct _dyneither_ptr f5;};int Cyc_main(int argc,struct
_dyneither_ptr argv);int Cyc_main(int argc,struct _dyneither_ptr argv){
GC_blacklist_warn_clear();{struct _tuple19*_tmp658;const char*_tmp657;struct Cyc_Arg_Flag_spec_struct
_tmp656;struct Cyc_Arg_Flag_spec_struct*_tmp655;const char*_tmp654;const char*
_tmp653;struct _tuple19*_tmp652;const char*_tmp651;struct Cyc_Arg_Set_spec_struct
_tmp650;struct Cyc_Arg_Set_spec_struct*_tmp64F;const char*_tmp64E;const char*
_tmp64D;struct _tuple19*_tmp64C;const char*_tmp64B;struct Cyc_Arg_Unit_spec_struct
_tmp64A;struct Cyc_Arg_Unit_spec_struct*_tmp649;const char*_tmp648;const char*
_tmp647;struct _tuple19*_tmp646;const char*_tmp645;struct Cyc_Arg_Unit_spec_struct
_tmp644;struct Cyc_Arg_Unit_spec_struct*_tmp643;const char*_tmp642;const char*
_tmp641;struct _tuple19*_tmp640;const char*_tmp63F;struct Cyc_Arg_Unit_spec_struct
_tmp63E;struct Cyc_Arg_Unit_spec_struct*_tmp63D;const char*_tmp63C;const char*
_tmp63B;struct _tuple19*_tmp63A;const char*_tmp639;struct Cyc_Arg_String_spec_struct
_tmp638;struct Cyc_Arg_String_spec_struct*_tmp637;const char*_tmp636;const char*
_tmp635;struct _tuple19*_tmp634[6];struct Cyc_List_List*options=(_tmp634[5]=((
_tmp658=_cycalloc(sizeof(*_tmp658)),((_tmp658->f1=((_tmp653="-",_tag_dyneither(
_tmp653,sizeof(char),2))),((_tmp658->f2=1,((_tmp658->f3=((_tmp654="",
_tag_dyneither(_tmp654,sizeof(char),1))),((_tmp658->f4=(void*)((_tmp655=
_cycalloc(sizeof(*_tmp655)),((_tmp655[0]=((_tmp656.tag=1,((_tmp656.f1=Cyc_unsupported_option,
_tmp656)))),_tmp655)))),((_tmp658->f5=((_tmp657="",_tag_dyneither(_tmp657,
sizeof(char),1))),_tmp658)))))))))))),((_tmp634[4]=((_tmp652=_cycalloc(sizeof(*
_tmp652)),((_tmp652->f1=((_tmp64D="-setjmp",_tag_dyneither(_tmp64D,sizeof(char),
8))),((_tmp652->f2=0,((_tmp652->f3=((_tmp64E="",_tag_dyneither(_tmp64E,sizeof(
char),1))),((_tmp652->f4=(void*)((_tmp64F=_cycalloc(sizeof(*_tmp64F)),((_tmp64F[
0]=((_tmp650.tag=3,((_tmp650.f1=& Cyc_do_setjmp,_tmp650)))),_tmp64F)))),((_tmp652->f5=((
_tmp651="Produce the jmp_buf and setjmp declarations on the standard output, for use by the Cyclone compiler special file cyc_setjmp.h.  Cannot be used with -gather, -gatherscript, or specfiles.",
_tag_dyneither(_tmp651,sizeof(char),186))),_tmp652)))))))))))),((_tmp634[3]=((
_tmp64C=_cycalloc(sizeof(*_tmp64C)),((_tmp64C->f1=((_tmp647="-finish",
_tag_dyneither(_tmp647,sizeof(char),8))),((_tmp64C->f2=0,((_tmp64C->f3=((_tmp648="",
_tag_dyneither(_tmp648,sizeof(char),1))),((_tmp64C->f4=(void*)((_tmp649=
_cycalloc(sizeof(*_tmp649)),((_tmp649[0]=((_tmp64A.tag=0,((_tmp64A.f1=Cyc_set_FINISH,
_tmp64A)))),_tmp649)))),((_tmp64C->f5=((_tmp64B="Produce Cyclone headers from pre-gathered C library info",
_tag_dyneither(_tmp64B,sizeof(char),57))),_tmp64C)))))))))))),((_tmp634[2]=((
_tmp646=_cycalloc(sizeof(*_tmp646)),((_tmp646->f1=((_tmp641="-gatherscript",
_tag_dyneither(_tmp641,sizeof(char),14))),((_tmp646->f2=0,((_tmp646->f3=((
_tmp642="",_tag_dyneither(_tmp642,sizeof(char),1))),((_tmp646->f4=(void*)((
_tmp643=_cycalloc(sizeof(*_tmp643)),((_tmp643[0]=((_tmp644.tag=0,((_tmp644.f1=
Cyc_set_GATHERSCRIPT,_tmp644)))),_tmp643)))),((_tmp646->f5=((_tmp645="Produce a script to gather C library info",
_tag_dyneither(_tmp645,sizeof(char),42))),_tmp646)))))))))))),((_tmp634[1]=((
_tmp640=_cycalloc(sizeof(*_tmp640)),((_tmp640->f1=((_tmp63B="-gather",
_tag_dyneither(_tmp63B,sizeof(char),8))),((_tmp640->f2=0,((_tmp640->f3=((_tmp63C="",
_tag_dyneither(_tmp63C,sizeof(char),1))),((_tmp640->f4=(void*)((_tmp63D=
_cycalloc(sizeof(*_tmp63D)),((_tmp63D[0]=((_tmp63E.tag=0,((_tmp63E.f1=Cyc_set_GATHER,
_tmp63E)))),_tmp63D)))),((_tmp640->f5=((_tmp63F="Gather C library info but don't produce Cyclone headers",
_tag_dyneither(_tmp63F,sizeof(char),56))),_tmp640)))))))))))),((_tmp634[0]=((
_tmp63A=_cycalloc(sizeof(*_tmp63A)),((_tmp63A->f1=((_tmp635="-d",_tag_dyneither(
_tmp635,sizeof(char),3))),((_tmp63A->f2=0,((_tmp63A->f3=((_tmp636=" <file>",
_tag_dyneither(_tmp636,sizeof(char),8))),((_tmp63A->f4=(void*)((_tmp637=
_cycalloc(sizeof(*_tmp637)),((_tmp637[0]=((_tmp638.tag=5,((_tmp638.f1=Cyc_set_output_dir,
_tmp638)))),_tmp637)))),((_tmp63A->f5=((_tmp639="Set the output directory to <file>",
_tag_dyneither(_tmp639,sizeof(char),35))),_tmp63A)))))))))))),((struct Cyc_List_List*(*)(
struct _dyneither_ptr))Cyc_List_list)(_tag_dyneither(_tmp634,sizeof(struct
_tuple19*),6)))))))))))));{const char*_tmp659;Cyc_Arg_parse(options,Cyc_add_spec_file,((
_tmp659="Options:",_tag_dyneither(_tmp659,sizeof(char),9))),argv);}if((((Cyc_badparse
 || !Cyc_do_setjmp  && Cyc_spec_files == 0) || Cyc_do_setjmp  && Cyc_spec_files != 0)
 || Cyc_do_setjmp  && Cyc_mode == (void*)1) || Cyc_do_setjmp  && Cyc_mode == (void*)
2){{const char*_tmp65A;Cyc_Arg_usage(options,((_tmp65A="Usage: buildlib [options] specfile1 specfile2 ...\nOptions:",
_tag_dyneither(_tmp65A,sizeof(char),59))));}return 1;}if(Cyc_mode == (void*)2){Cyc_script_file=
Cyc_fopen((const char*)"BUILDLIB.sh",(const char*)"w");if(!((unsigned int)Cyc_script_file)){{
const char*_tmp65D;void*_tmp65C;(_tmp65C=0,Cyc_fprintf(Cyc_stderr,((_tmp65D="Could not create file BUILDLIB.sh\n",
_tag_dyneither(_tmp65D,sizeof(char),35))),_tag_dyneither(_tmp65C,sizeof(void*),0)));}
exit(1);}{const char*_tmp660;void*_tmp65F;(_tmp65F=0,Cyc_prscript(((_tmp660="#!/bin/sh\n",
_tag_dyneither(_tmp660,sizeof(char),11))),_tag_dyneither(_tmp65F,sizeof(void*),0)));}{
const char*_tmp663;void*_tmp662;(_tmp662=0,Cyc_prscript(((_tmp663="GCC=\"gcc\"\n",
_tag_dyneither(_tmp663,sizeof(char),11))),_tag_dyneither(_tmp662,sizeof(void*),0)));}}
if(Cyc_force_directory_prefixes(Cyc_output_dir) || Cyc_force_directory(Cyc_output_dir)){{
const char*_tmp667;void*_tmp666[1];struct Cyc_String_pa_struct _tmp665;(_tmp665.tag=
0,((_tmp665.f1=(struct _dyneither_ptr)((struct _dyneither_ptr)Cyc_output_dir),((
_tmp666[0]=& _tmp665,Cyc_fprintf(Cyc_stderr,((_tmp667="Error: could not create directory %s\n",
_tag_dyneither(_tmp667,sizeof(char),38))),_tag_dyneither(_tmp666,sizeof(void*),1)))))));}
return 1;}if(Cyc_mode == (void*)2){{const char*_tmp66B;void*_tmp66A[1];struct Cyc_String_pa_struct
_tmp669;(_tmp669.tag=0,((_tmp669.f1=(struct _dyneither_ptr)((struct _dyneither_ptr)
Cyc_output_dir),((_tmp66A[0]=& _tmp669,Cyc_prscript(((_tmp66B="cd %s\n",
_tag_dyneither(_tmp66B,sizeof(char),7))),_tag_dyneither(_tmp66A,sizeof(void*),1)))))));}{
const char*_tmp66E;void*_tmp66D;(_tmp66D=0,Cyc_prscript(((_tmp66E="echo | $GCC -E -dM - -o INITMACROS.h\n",
_tag_dyneither(_tmp66E,sizeof(char),38))),_tag_dyneither(_tmp66D,sizeof(void*),0)));}}
if(!Cyc_gathering()){{const char*_tmp66F;Cyc_log_file=Cyc_fopen((const char*)
_untag_dyneither_ptr(Cyc_Filename_concat(Cyc_output_dir,((_tmp66F="BUILDLIB.LOG",
_tag_dyneither(_tmp66F,sizeof(char),13)))),sizeof(char),1),(const char*)"w");}if(
!((unsigned int)Cyc_log_file)){{const char*_tmp673;void*_tmp672[1];struct Cyc_String_pa_struct
_tmp671;(_tmp671.tag=0,((_tmp671.f1=(struct _dyneither_ptr)((struct _dyneither_ptr)
Cyc_output_dir),((_tmp672[0]=& _tmp671,Cyc_fprintf(Cyc_stderr,((_tmp673="Error: could not create log file in directory %s\n",
_tag_dyneither(_tmp673,sizeof(char),50))),_tag_dyneither(_tmp672,sizeof(void*),1)))))));}
return 1;}if(!Cyc_do_setjmp){{const char*_tmp674;Cyc_cstubs_file=Cyc_fopen((const
char*)_untag_dyneither_ptr(Cyc_Filename_concat(Cyc_output_dir,((_tmp674="cstubs.c",
_tag_dyneither(_tmp674,sizeof(char),9)))),sizeof(char),1),(const char*)"w");}if(!((
unsigned int)Cyc_cstubs_file)){{const char*_tmp678;void*_tmp677[1];struct Cyc_String_pa_struct
_tmp676;(_tmp676.tag=0,((_tmp676.f1=(struct _dyneither_ptr)((struct _dyneither_ptr)
Cyc_output_dir),((_tmp677[0]=& _tmp676,Cyc_fprintf(Cyc_stderr,((_tmp678="Error: could not create cstubs.c in directory %s\n",
_tag_dyneither(_tmp678,sizeof(char),50))),_tag_dyneither(_tmp677,sizeof(void*),1)))))));}
return 1;}{const char*_tmp679;Cyc_cycstubs_file=Cyc_fopen((const char*)
_untag_dyneither_ptr(Cyc_Filename_concat(Cyc_output_dir,((_tmp679="cycstubs.cyc",
_tag_dyneither(_tmp679,sizeof(char),13)))),sizeof(char),1),(const char*)"w");}if(
!((unsigned int)Cyc_cycstubs_file)){{const char*_tmp67D;void*_tmp67C[1];struct Cyc_String_pa_struct
_tmp67B;(_tmp67B.tag=0,((_tmp67B.f1=(struct _dyneither_ptr)((struct _dyneither_ptr)
Cyc_output_dir),((_tmp67C[0]=& _tmp67B,Cyc_fprintf(Cyc_stderr,((_tmp67D="Error: could not create cycstubs.c in directory %s\n",
_tag_dyneither(_tmp67D,sizeof(char),52))),_tag_dyneither(_tmp67C,sizeof(void*),1)))))));}
return 1;}{const char*_tmp680;void*_tmp67F;(_tmp67F=0,Cyc_fprintf((struct Cyc___cycFILE*)
_check_null(Cyc_cycstubs_file),((_tmp680="#include <core.h>\nusing Core;\n\n",
_tag_dyneither(_tmp680,sizeof(char),32))),_tag_dyneither(_tmp67F,sizeof(void*),0)));}}}{
const char*outdir=(const char*)_untag_dyneither_ptr(Cyc_output_dir,sizeof(char),1);
if(Cyc_do_setjmp  && Cyc_process_setjmp(outdir))return 1;else{for(0;Cyc_spec_files
!= 0;Cyc_spec_files=((struct Cyc_List_List*)_check_null(Cyc_spec_files))->tl){if(
Cyc_process_specfile((const char*)((struct Cyc_List_List*)_check_null(Cyc_spec_files))->hd,
outdir)){{const char*_tmp683;void*_tmp682;(_tmp682=0,Cyc_fprintf(Cyc_stderr,((
_tmp683="FATAL ERROR -- QUIT!\n",_tag_dyneither(_tmp683,sizeof(char),22))),
_tag_dyneither(_tmp682,sizeof(void*),0)));}exit(1);}}}if(Cyc_mode == (void*)2)Cyc_fclose((
struct Cyc___cycFILE*)_check_null(Cyc_script_file));else{if(!Cyc_gathering()){Cyc_fclose((
struct Cyc___cycFILE*)_check_null(Cyc_log_file));if(!Cyc_do_setjmp){Cyc_fclose((
struct Cyc___cycFILE*)_check_null(Cyc_cstubs_file));Cyc_fclose((struct Cyc___cycFILE*)
_check_null(Cyc_cycstubs_file));}}}return 0;}}}