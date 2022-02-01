#include <setjmp.h>
/* This is a C header used by the output of the Cyclone to
   C translator.  Corresponding definitions are in file lib/runtime_*.c */
#ifndef _CYC_INCLUDE_H_
#define _CYC_INCLUDE_H_

/* Need one of these per thread (see runtime_stack.c). The runtime maintains 
   a stack that contains either _handler_cons structs or _RegionHandle structs.
   The tag is 0 for a handler_cons and 1 for a region handle.  */
struct _RuntimeStack {
  int tag; 
  struct _RuntimeStack *next;
  void (*cleanup)(struct _RuntimeStack *frame);
};

#ifndef offsetof
/* should be size_t but int is fine */
#define offsetof(t,n) ((int)(&(((t*)0)->n)))
#endif

/* Fat pointers */
struct _fat_ptr {
  unsigned char *curr; 
  unsigned char *base; 
  unsigned char *last_plus_one; 
};  

/* Regions */
struct _RegionPage
{ 
#ifdef CYC_REGION_PROFILE
  unsigned total_bytes;
  unsigned free_bytes;
#endif
  struct _RegionPage *next;
  char data[1];
};

struct _pool;
struct bget_region_key;
struct _RegionAllocFunctions;

struct _RegionHandle {
  struct _RuntimeStack s;
  struct _RegionPage *curr;
#if(defined(__linux__) && defined(__KERNEL__))
  struct _RegionPage *vpage;
#endif 
  struct _RegionAllocFunctions *fcns;
  char               *offset;
  char               *last_plus_one;
  struct _pool *released_ptrs;
  struct bget_region_key *key;
#ifdef CYC_REGION_PROFILE
  const char *name;
#endif
  unsigned used_bytes;
  unsigned wasted_bytes;
};


// A dynamic region is just a region handle.  The wrapper struct is for type
// abstraction.
struct Cyc_Core_DynamicRegion {
  struct _RegionHandle h;
};

/* Alias qualifier stuff */
typedef unsigned int _AliasQualHandle_t; // must match aqualt_type() in toc.cyc

struct _RegionHandle _new_region(unsigned int, const char*);
void* _region_malloc(struct _RegionHandle*, _AliasQualHandle_t, unsigned);
void* _region_calloc(struct _RegionHandle*, _AliasQualHandle_t, unsigned t, unsigned n);
void* _region_vmalloc(struct _RegionHandle*, unsigned);
void * _aqual_malloc(_AliasQualHandle_t aq, unsigned int s);
void * _aqual_calloc(_AliasQualHandle_t aq, unsigned int n, unsigned int t);
void _free_region(struct _RegionHandle*);

/* Exceptions */
struct _handler_cons {
  struct _RuntimeStack s;
  jmp_buf handler;
};
void _push_handler(struct _handler_cons*);
void _push_region(struct _RegionHandle*);
void _npop_handler(int);
void _pop_handler();
void _pop_region();


#ifndef _throw
void* _throw_null_fn(const char*,unsigned);
void* _throw_arraybounds_fn(const char*,unsigned);
void* _throw_badalloc_fn(const char*,unsigned);
void* _throw_match_fn(const char*,unsigned);
void* _throw_assert_fn(const char *,unsigned);
void* _throw_fn(void*,const char*,unsigned);
void* _rethrow(void*);
#define _throw_null() (_throw_null_fn(__FILE__,__LINE__))
#define _throw_arraybounds() (_throw_arraybounds_fn(__FILE__,__LINE__))
#define _throw_badalloc() (_throw_badalloc_fn(__FILE__,__LINE__))
#define _throw_match() (_throw_match_fn(__FILE__,__LINE__))
#define _throw_assert() (_throw_assert_fn(__FILE__,__LINE__))
#define _throw(e) (_throw_fn((e),__FILE__,__LINE__))
#endif

void* Cyc_Core_get_exn_thrown();
/* Built-in Exceptions */
struct Cyc_Null_Exception_exn_struct { char *tag; };
struct Cyc_Array_bounds_exn_struct { char *tag; };
struct Cyc_Match_Exception_exn_struct { char *tag; };
struct Cyc_Bad_alloc_exn_struct { char *tag; };
struct Cyc_Assert_exn_struct { char *tag; };
extern char Cyc_Null_Exception[];
extern char Cyc_Array_bounds[];
extern char Cyc_Match_Exception[];
extern char Cyc_Bad_alloc[];
extern char Cyc_Assert[];

/* Built-in Run-time Checks and company */
#ifdef NO_CYC_NULL_CHECKS
#define _check_null(ptr) (ptr)
#else
#define _check_null(ptr) \
  ({ typeof(ptr) _cks_null = (ptr); \
     if (!_cks_null) _throw_null(); \
     _cks_null; })
#endif

#ifdef NO_CYC_BOUNDS_CHECKS
#define _check_known_subscript_notnull(ptr,bound,elt_sz,index)\
   (((char*)ptr) + (elt_sz)*(index))
#ifdef NO_CYC_NULL_CHECKS
#define _check_known_subscript_null _check_known_subscript_notnull
#else
#define _check_known_subscript_null(ptr,bound,elt_sz,index) ({ \
  char*_cks_ptr = (char*)(ptr);\
  int _index = (index);\
  if (!_cks_ptr) _throw_null(); \
  _cks_ptr + (elt_sz)*_index; })
#endif
#define _zero_arr_plus_char_fn(orig_x,orig_sz,orig_i,f,l) ((orig_x)+(orig_i))
#define _zero_arr_plus_other_fn(t_sz,orig_x,orig_sz,orig_i,f,l)((orig_x)+(orig_i))
#else
#define _check_known_subscript_null(ptr,bound,elt_sz,index) ({ \
  char*_cks_ptr = (char*)(ptr); \
  unsigned _cks_index = (index); \
  if (!_cks_ptr) _throw_null(); \
  if (_cks_index >= (bound)) _throw_arraybounds(); \
  _cks_ptr + (elt_sz)*_cks_index; })
#define _check_known_subscript_notnull(ptr,bound,elt_sz,index) ({ \
  char*_cks_ptr = (char*)(ptr); \
  unsigned _cks_index = (index); \
  if (_cks_index >= (bound)) _throw_arraybounds(); \
  _cks_ptr + (elt_sz)*_cks_index; })

/* _zero_arr_plus_*_fn(x,sz,i,filename,lineno) adds i to zero-terminated ptr
   x that has at least sz elements */
char* _zero_arr_plus_char_fn(char*,unsigned,int,const char*,unsigned);
void* _zero_arr_plus_other_fn(unsigned,void*,unsigned,int,const char*,unsigned);
#endif

/* _get_zero_arr_size_*(x,sz) returns the number of elements in a
   zero-terminated array that is NULL or has at least sz elements */
unsigned _get_zero_arr_size_char(const char*,unsigned);
unsigned _get_zero_arr_size_other(unsigned,const void*,unsigned);

/* _zero_arr_inplace_plus_*_fn(x,i,filename,lineno) sets
   zero-terminated pointer *x to *x + i */
char* _zero_arr_inplace_plus_char_fn(char**,int,const char*,unsigned);
char* _zero_arr_inplace_plus_post_char_fn(char**,int,const char*,unsigned);
// note: must cast result in toc.cyc
void* _zero_arr_inplace_plus_other_fn(unsigned,void**,int,const char*,unsigned);
void* _zero_arr_inplace_plus_post_other_fn(unsigned,void**,int,const char*,unsigned);
#define _zero_arr_plus_char(x,s,i) \
  (_zero_arr_plus_char_fn(x,s,i,__FILE__,__LINE__))
#define _zero_arr_inplace_plus_char(x,i) \
  _zero_arr_inplace_plus_char_fn((char**)(x),i,__FILE__,__LINE__)
#define _zero_arr_inplace_plus_post_char(x,i) \
  _zero_arr_inplace_plus_post_char_fn((char**)(x),(i),__FILE__,__LINE__)
#define _zero_arr_plus_other(t,x,s,i) \
  (_zero_arr_plus_other_fn(t,x,s,i,__FILE__,__LINE__))
#define _zero_arr_inplace_plus_other(t,x,i) \
  _zero_arr_inplace_plus_other_fn(t,(void**)(x),i,__FILE__,__LINE__)
#define _zero_arr_inplace_plus_post_other(t,x,i) \
  _zero_arr_inplace_plus_post_other_fn(t,(void**)(x),(i),__FILE__,__LINE__)

#ifdef NO_CYC_BOUNDS_CHECKS
#define _check_fat_subscript(arr,elt_sz,index) ((arr).curr + (elt_sz) * (index))
#define _untag_fat_ptr(arr,elt_sz,num_elts) ((arr).curr)
#define _untag_fat_ptr_check_bound(arr,elt_sz,num_elts) ((arr).curr)
#define _check_fat_at_base(arr) (arr)
#else
#define _check_fat_subscript(arr,elt_sz,index) ({ \
  struct _fat_ptr _cus_arr = (arr); \
  unsigned char *_cus_ans = _cus_arr.curr + (elt_sz) * (index); \
  /* JGM: not needed! if (!_cus_arr.base) _throw_null();*/ \
  if (_cus_ans < _cus_arr.base || _cus_ans >= _cus_arr.last_plus_one) \
    _throw_arraybounds(); \
  _cus_ans; })
#define _untag_fat_ptr(arr,elt_sz,num_elts) ((arr).curr)
#define _untag_fat_ptr_check_bound(arr,elt_sz,num_elts) ({ \
  struct _fat_ptr _arr = (arr); \
  unsigned char *_curr = _arr.curr; \
  if ((_curr < _arr.base || _curr + (elt_sz) * (num_elts) > _arr.last_plus_one) &&\
      _curr != (unsigned char*)0) \
    _throw_arraybounds(); \
  _curr; })
#define _check_fat_at_base(arr) ({ \
  struct _fat_ptr _arr = (arr); \
  if (_arr.base != _arr.curr) _throw_arraybounds(); \
  _arr; })
#endif

#define _tag_fat(tcurr,elt_sz,num_elts) ({ \
  struct _fat_ptr _ans; \
  unsigned _num_elts = (num_elts);\
  _ans.base = _ans.curr = (void*)(tcurr); \
  /* JGM: if we're tagging NULL, ignore num_elts */ \
  _ans.last_plus_one = _ans.base ? (_ans.base + (elt_sz) * _num_elts) : 0; \
  _ans; })

#define _get_fat_size(arr,elt_sz) \
  ({struct _fat_ptr _arr = (arr); \
    unsigned char *_arr_curr=_arr.curr; \
    unsigned char *_arr_last=_arr.last_plus_one; \
    (_arr_curr < _arr.base || _arr_curr >= _arr_last) ? 0 : \
    ((_arr_last - _arr_curr) / (elt_sz));})

#define _fat_ptr_plus(arr,elt_sz,change) ({ \
  struct _fat_ptr _ans = (arr); \
  int _change = (change);\
  _ans.curr += (elt_sz) * _change;\
  _ans; })
#define _fat_ptr_inplace_plus(arr_ptr,elt_sz,change) ({ \
  struct _fat_ptr * _arr_ptr = (arr_ptr); \
  _arr_ptr->curr += (elt_sz) * (change);\
  *_arr_ptr; })
#define _fat_ptr_inplace_plus_post(arr_ptr,elt_sz,change) ({ \
  struct _fat_ptr * _arr_ptr = (arr_ptr); \
  struct _fat_ptr _ans = *_arr_ptr; \
  _arr_ptr->curr += (elt_sz) * (change);\
  _ans; })

//Not a macro since initialization order matters. Defined in runtime_zeroterm.c.
struct _fat_ptr _fat_ptr_decrease_size(struct _fat_ptr,unsigned sz,unsigned numelts);

#ifdef CYC_GC_PTHREAD_REDIRECTS
# define pthread_create GC_pthread_create
# define pthread_sigmask GC_pthread_sigmask
# define pthread_join GC_pthread_join
# define pthread_detach GC_pthread_detach
# define dlopen GC_dlopen
#endif
/* Allocation */
void* GC_malloc(int);
void* GC_malloc_atomic(int);
void* GC_calloc(unsigned,unsigned);
void* GC_calloc_atomic(unsigned,unsigned);

#if(defined(__linux__) && defined(__KERNEL__))
void *cyc_vmalloc(unsigned);
void cyc_vfree(void*);
#endif
// bound the allocation size to be < MAX_ALLOC_SIZE. See macros below for usage.
#define MAX_MALLOC_SIZE (1 << 28)
void* _bounded_GC_malloc(int,const char*,int);
void* _bounded_GC_malloc_atomic(int,const char*,int);
void* _bounded_GC_calloc(unsigned,unsigned,const char*,int);
void* _bounded_GC_calloc_atomic(unsigned,unsigned,const char*,int);
/* these macros are overridden below ifdef CYC_REGION_PROFILE */
#ifndef CYC_REGION_PROFILE
#define _cycalloc(n) _bounded_GC_malloc(n,__FILE__,__LINE__)
#define _cycalloc_atomic(n) _bounded_GC_malloc_atomic(n,__FILE__,__LINE__)
#define _cyccalloc(n,s) _bounded_GC_calloc(n,s,__FILE__,__LINE__)
#define _cyccalloc_atomic(n,s) _bounded_GC_calloc_atomic(n,s,__FILE__,__LINE__)
#endif

static inline unsigned int _check_times(unsigned x, unsigned y) {
  unsigned long long whole_ans = 
    ((unsigned long long) x)*((unsigned long long)y);
  unsigned word_ans = (unsigned)whole_ans;
  if(word_ans < whole_ans || word_ans > MAX_MALLOC_SIZE)
    _throw_badalloc();
  return word_ans;
}

#define _CYC_MAX_REGION_CONST 0
#define _CYC_MIN_ALIGNMENT (sizeof(double))

#ifdef CYC_REGION_PROFILE
extern int rgn_total_bytes;
#endif

static inline void*_fast_region_malloc(struct _RegionHandle*r, _AliasQualHandle_t aq, unsigned orig_s) {  
  if (r > (struct _RegionHandle*)_CYC_MAX_REGION_CONST && r->curr != 0) { 
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
  return _region_malloc(r,aq,orig_s); 
}

//doesn't make sense to fast malloc with reaps
#ifndef DISABLE_REAPS
#define _fast_region_malloc _region_malloc
#endif

#ifdef CYC_REGION_PROFILE
/* see macros below for usage. defined in runtime_memory.c */
void* _profile_GC_malloc(int,const char*,const char*,int);
void* _profile_GC_malloc_atomic(int,const char*,const char*,int);
void* _profile_GC_calloc(unsigned,unsigned,const char*,const char*,int);
void* _profile_GC_calloc_atomic(unsigned,unsigned,const char*,const char*,int);
void* _profile_region_malloc(struct _RegionHandle*,_AliasQualHandle_t,unsigned,const char*,const char*,int);
void* _profile_region_calloc(struct _RegionHandle*,_AliasQualHandle_t,unsigned,unsigned,const char *,const char*,int);
void * _profile_aqual_malloc(_AliasQualHandle_t aq, unsigned int s,const char *file, const char *func, int lineno);
void * _profile_aqual_calloc(_AliasQualHandle_t aq, unsigned int t1,unsigned int t2,const char *file, const char *func, int lineno);
struct _RegionHandle _profile_new_region(unsigned int i, const char*,const char*,const char*,int);
void _profile_free_region(struct _RegionHandle*,const char*,const char*,int);
#ifndef RUNTIME_CYC
#define _new_region(i,n) _profile_new_region(i,n,__FILE__,__FUNCTION__,__LINE__)
#define _free_region(r) _profile_free_region(r,__FILE__,__FUNCTION__,__LINE__)
#define _region_malloc(rh,aq,n) _profile_region_malloc(rh,aq,n,__FILE__,__FUNCTION__,__LINE__)
#define _region_calloc(rh,aq,n,t) _profile_region_calloc(rh,aq,n,t,__FILE__,__FUNCTION__,__LINE__)
#define _aqual_malloc(aq,n) _profile_aqual_malloc(aq,n,__FILE__,__FUNCTION__,__LINE__)
#define _aqual_calloc(aq,n,t) _profile_aqual_calloc(aq,n,t,__FILE__,__FUNCTION__,__LINE__)
#endif
#define _cycalloc(n) _profile_GC_malloc(n,__FILE__,__FUNCTION__,__LINE__)
#define _cycalloc_atomic(n) _profile_GC_malloc_atomic(n,__FILE__,__FUNCTION__,__LINE__)
#define _cyccalloc(n,s) _profile_GC_calloc(n,s,__FILE__,__FUNCTION__,__LINE__)
#define _cyccalloc_atomic(n,s) _profile_GC_calloc_atomic(n,s,__FILE__,__FUNCTION__,__LINE__)
#endif //CYC_REGION_PROFILE
#endif //_CYC_INCLUDE_H
 struct Cyc_Core_Opt{void*v;};extern char Cyc_Core_Failure[8U];struct Cyc_Core_Failure_exn_struct{char*tag;struct _fat_ptr f1;};extern char Cyc_Core_Not_found[10U];struct Cyc_Core_Not_found_exn_struct{char*tag;};struct Cyc___cycFILE;
# 53 "cycboot.h"
extern struct Cyc___cycFILE*Cyc_stderr;struct Cyc_String_pa_PrintArg_struct{int tag;struct _fat_ptr f1;};struct Cyc_Int_pa_PrintArg_struct{int tag;unsigned long f1;};
# 73
extern struct _fat_ptr Cyc_aprintf(struct _fat_ptr,struct _fat_ptr);
# 100
extern int Cyc_fprintf(struct Cyc___cycFILE*,struct _fat_ptr,struct _fat_ptr);struct Cyc_Lexing_lexbuf{void(*refill_buff)(struct Cyc_Lexing_lexbuf*);void*refill_state;struct _fat_ptr lex_buffer;int lex_buffer_len;int lex_abs_pos;int lex_start_pos;int lex_curr_pos;int lex_last_pos;int lex_last_action;int lex_eof_reached;};
# 78 "lexing.h"
extern struct Cyc_Lexing_lexbuf*Cyc_Lexing_from_file(struct Cyc___cycFILE*);struct Cyc_List_List{void*hd;struct Cyc_List_List*tl;};
# 54 "list.h"
extern struct Cyc_List_List*Cyc_List_list(struct _fat_ptr);
# 61
extern int Cyc_List_length(struct Cyc_List_List*);
# 76
extern struct Cyc_List_List*Cyc_List_map(void*(*)(void*),struct Cyc_List_List*);
# 83
extern struct Cyc_List_List*Cyc_List_map_c(void*(*)(void*,void*),void*,struct Cyc_List_List*);
# 135
extern void Cyc_List_iter_c(void(*)(void*,void*),void*,struct Cyc_List_List*);
# 153
extern void*Cyc_List_fold_right(void*(*)(void*,void*),struct Cyc_List_List*,void*);
# 172
extern struct Cyc_List_List*Cyc_List_rev(struct Cyc_List_List*);
# 178
extern struct Cyc_List_List*Cyc_List_imp_rev(struct Cyc_List_List*);
# 184
extern struct Cyc_List_List*Cyc_List_append(struct Cyc_List_List*,struct Cyc_List_List*);
# 195
extern struct Cyc_List_List*Cyc_List_imp_append(struct Cyc_List_List*,struct Cyc_List_List*);
# 276
extern struct Cyc_List_List*Cyc_List_rzip(struct _RegionHandle*,struct _RegionHandle*,struct Cyc_List_List*,struct Cyc_List_List*);
# 38 "string.h"
extern unsigned long Cyc_strlen(struct _fat_ptr);
# 49 "string.h"
extern int Cyc_strcmp(struct _fat_ptr,struct _fat_ptr);
extern int Cyc_strptrcmp(struct _fat_ptr*,struct _fat_ptr*);
extern int Cyc_strncmp(struct _fat_ptr,struct _fat_ptr,unsigned long);
extern int Cyc_zstrcmp(struct _fat_ptr,struct _fat_ptr);
# 54
extern int Cyc_zstrptrcmp(struct _fat_ptr*,struct _fat_ptr*);
# 60
extern struct _fat_ptr Cyc_strcat(struct _fat_ptr,struct _fat_ptr);
# 62
extern struct _fat_ptr Cyc_strconcat(struct _fat_ptr,struct _fat_ptr);
# 71
extern struct _fat_ptr Cyc_strcpy(struct _fat_ptr,struct _fat_ptr);
# 121 "string.h"
extern struct _fat_ptr Cyc_strchr(struct _fat_ptr,char);struct Cyc_Hashtable_Table;
# 39 "hashtable.h"
extern struct Cyc_Hashtable_Table*Cyc_Hashtable_create(int,int(*)(void*,void*),int(*)(void*));
# 50
extern void Cyc_Hashtable_insert(struct Cyc_Hashtable_Table*,void*,void*);
# 52
extern void*Cyc_Hashtable_lookup(struct Cyc_Hashtable_Table*,void*);
# 86
extern int Cyc_Hashtable_hash_stringptr(struct _fat_ptr*);
# 33 "position.h"
extern unsigned Cyc_Position_loc_to_seg(unsigned);
# 37
extern struct _fat_ptr Cyc_Position_string_of_segment(unsigned);struct Cyc_AssnDef_ExistAssnFn;struct _union_Nmspace_Abs_n{int tag;struct Cyc_List_List*val;};struct _union_Nmspace_Rel_n{int tag;struct Cyc_List_List*val;};struct _union_Nmspace_C_n{int tag;struct Cyc_List_List*val;};struct _union_Nmspace_Loc_n{int tag;int val;};union Cyc_Absyn_Nmspace{struct _union_Nmspace_Abs_n Abs_n;struct _union_Nmspace_Rel_n Rel_n;struct _union_Nmspace_C_n C_n;struct _union_Nmspace_Loc_n Loc_n;};struct _tuple0{union Cyc_Absyn_Nmspace f0;struct _fat_ptr*f1;};
# 140 "absyn.h"
enum Cyc_Absyn_Scope{Cyc_Absyn_Static =0U,Cyc_Absyn_Abstract =1U,Cyc_Absyn_Public =2U,Cyc_Absyn_Extern =3U,Cyc_Absyn_ExternC =4U,Cyc_Absyn_Register =5U};struct Cyc_Absyn_Tqual{int print_const: 1;int q_volatile: 1;int q_restrict: 1;int real_const: 1;unsigned loc;};
# 161
enum Cyc_Absyn_Size_of{Cyc_Absyn_Char_sz =0U,Cyc_Absyn_Short_sz =1U,Cyc_Absyn_Int_sz =2U,Cyc_Absyn_Long_sz =3U,Cyc_Absyn_LongLong_sz =4U};
enum Cyc_Absyn_Sign{Cyc_Absyn_Signed =0U,Cyc_Absyn_Unsigned =1U,Cyc_Absyn_None =2U};
enum Cyc_Absyn_AggrKind{Cyc_Absyn_StructA =0U,Cyc_Absyn_UnionA =1U};
# 181 "absyn.h"
enum Cyc_Absyn_AliasHint{Cyc_Absyn_UniqueHint =0U,Cyc_Absyn_RefcntHint =1U,Cyc_Absyn_RestrictedHint =2U,Cyc_Absyn_NoHint =3U};
# 187
enum Cyc_Absyn_KindQual{Cyc_Absyn_AnyKind =0U,Cyc_Absyn_MemKind =1U,Cyc_Absyn_BoxKind =2U,Cyc_Absyn_EffKind =3U,Cyc_Absyn_IntKind =4U,Cyc_Absyn_BoolKind =5U,Cyc_Absyn_PtrBndKind =6U,Cyc_Absyn_AqualKind =7U};struct Cyc_Absyn_Kind{enum Cyc_Absyn_KindQual kind;enum Cyc_Absyn_AliasHint aliashint;};struct Cyc_Absyn_Eq_kb_Absyn_KindBound_struct{int tag;struct Cyc_Absyn_Kind*f1;};struct Cyc_Absyn_Unknown_kb_Absyn_KindBound_struct{int tag;struct Cyc_Core_Opt*f1;};struct Cyc_Absyn_Less_kb_Absyn_KindBound_struct{int tag;struct Cyc_Core_Opt*f1;struct Cyc_Absyn_Kind*f2;};struct Cyc_Absyn_Tvar{struct _fat_ptr*name;int identity;void*kind;void*aquals_bound;};struct Cyc_Absyn_PtrLoc{unsigned ptr_loc;unsigned rgn_loc;unsigned zt_loc;};struct Cyc_Absyn_PtrAtts{void*eff;void*nullable;void*bounds;void*zero_term;struct Cyc_Absyn_PtrLoc*ptrloc;void*autoreleased;void*aqual;};struct Cyc_Absyn_PtrInfo{void*elt_type;struct Cyc_Absyn_Tqual elt_tq;struct Cyc_Absyn_PtrAtts ptr_atts;};struct Cyc_Absyn_VarargInfo{struct _fat_ptr*name;struct Cyc_Absyn_Tqual tq;void*type;int inject;};struct Cyc_Absyn_FnInfo{struct Cyc_List_List*tvars;void*effect;struct Cyc_Absyn_Tqual ret_tqual;void*ret_type;struct Cyc_List_List*args;int c_varargs;struct Cyc_Absyn_VarargInfo*cyc_varargs;struct Cyc_List_List*qual_bnd;struct Cyc_List_List*attributes;struct Cyc_Absyn_Exp*checks_clause;struct Cyc_AssnDef_ExistAssnFn*checks_assn;struct Cyc_Absyn_Exp*requires_clause;struct Cyc_AssnDef_ExistAssnFn*requires_assn;struct Cyc_Absyn_Exp*ensures_clause;struct Cyc_AssnDef_ExistAssnFn*ensures_assn;struct Cyc_Absyn_Exp*throws_clause;struct Cyc_AssnDef_ExistAssnFn*throws_assn;struct Cyc_Absyn_Vardecl*return_value;struct Cyc_List_List*arg_vardecls;struct Cyc_List_List*effconstr;};struct Cyc_Absyn_UnknownDatatypeInfo{struct _tuple0*name;int is_extensible;};struct _union_DatatypeInfo_UnknownDatatype{int tag;struct Cyc_Absyn_UnknownDatatypeInfo val;};struct _union_DatatypeInfo_KnownDatatype{int tag;struct Cyc_Absyn_Datatypedecl**val;};union Cyc_Absyn_DatatypeInfo{struct _union_DatatypeInfo_UnknownDatatype UnknownDatatype;struct _union_DatatypeInfo_KnownDatatype KnownDatatype;};
# 312
union Cyc_Absyn_DatatypeInfo Cyc_Absyn_UnknownDatatype(struct Cyc_Absyn_UnknownDatatypeInfo);struct Cyc_Absyn_UnknownDatatypeFieldInfo{struct _tuple0*datatype_name;struct _tuple0*field_name;int is_extensible;};struct _union_DatatypeFieldInfo_UnknownDatatypefield{int tag;struct Cyc_Absyn_UnknownDatatypeFieldInfo val;};struct _tuple1{struct Cyc_Absyn_Datatypedecl*f0;struct Cyc_Absyn_Datatypefield*f1;};struct _union_DatatypeFieldInfo_KnownDatatypefield{int tag;struct _tuple1 val;};union Cyc_Absyn_DatatypeFieldInfo{struct _union_DatatypeFieldInfo_UnknownDatatypefield UnknownDatatypefield;struct _union_DatatypeFieldInfo_KnownDatatypefield KnownDatatypefield;};
# 325
union Cyc_Absyn_DatatypeFieldInfo Cyc_Absyn_UnknownDatatypefield(struct Cyc_Absyn_UnknownDatatypeFieldInfo);struct _tuple2{enum Cyc_Absyn_AggrKind f0;struct _tuple0*f1;struct Cyc_Core_Opt*f2;};struct _union_AggrInfo_UnknownAggr{int tag;struct _tuple2 val;};struct _union_AggrInfo_KnownAggr{int tag;struct Cyc_Absyn_Aggrdecl**val;};union Cyc_Absyn_AggrInfo{struct _union_AggrInfo_UnknownAggr UnknownAggr;struct _union_AggrInfo_KnownAggr KnownAggr;};
# 332
union Cyc_Absyn_AggrInfo Cyc_Absyn_UnknownAggr(enum Cyc_Absyn_AggrKind,struct _tuple0*,struct Cyc_Core_Opt*);struct Cyc_Absyn_ArrayInfo{void*elt_type;struct Cyc_Absyn_Tqual tq;struct Cyc_Absyn_Exp*num_elts;void*zero_term;unsigned zt_loc;};struct Cyc_Absyn_Aggr_td_Absyn_Raw_typedecl_struct{int tag;struct Cyc_Absyn_Aggrdecl*f1;};struct Cyc_Absyn_Enum_td_Absyn_Raw_typedecl_struct{int tag;struct Cyc_Absyn_Enumdecl*f1;};struct Cyc_Absyn_Datatype_td_Absyn_Raw_typedecl_struct{int tag;struct Cyc_Absyn_Datatypedecl*f1;};struct Cyc_Absyn_TypeDecl{void*r;unsigned loc;};struct Cyc_Absyn_IntCon_Absyn_TyCon_struct{int tag;enum Cyc_Absyn_Sign f1;enum Cyc_Absyn_Size_of f2;};struct Cyc_Absyn_EnumCon_Absyn_TyCon_struct{int tag;struct _tuple0*f1;struct Cyc_Absyn_Enumdecl*f2;};struct Cyc_Absyn_AnonEnumCon_Absyn_TyCon_struct{int tag;struct Cyc_List_List*f1;};struct Cyc_Absyn_DatatypeCon_Absyn_TyCon_struct{int tag;union Cyc_Absyn_DatatypeInfo f1;};struct Cyc_Absyn_AggrCon_Absyn_TyCon_struct{int tag;union Cyc_Absyn_AggrInfo f1;};struct Cyc_Absyn_SingleConstraint_Absyn_EffConstraint_struct{int tag;void*f1;};struct Cyc_Absyn_DisjointConstraint_Absyn_EffConstraint_struct{int tag;void*f1;void*f2;};struct Cyc_Absyn_SubsetConstraint_Absyn_EffConstraint_struct{int tag;void*f1;void*f2;};struct Cyc_Absyn_AppType_Absyn_Type_struct{int tag;void*f1;struct Cyc_List_List*f2;};struct Cyc_Absyn_Evar_Absyn_Type_struct{int tag;struct Cyc_Core_Opt*f1;void*f2;int f3;struct Cyc_Core_Opt*f4;};struct Cyc_Absyn_VarType_Absyn_Type_struct{int tag;struct Cyc_Absyn_Tvar*f1;};struct Cyc_Absyn_Cvar_Absyn_Type_struct{int tag;struct Cyc_Core_Opt*f1;void*f2;int f3;void*f4;const char*f5;const char*f6;int f7;};struct Cyc_Absyn_PointerType_Absyn_Type_struct{int tag;struct Cyc_Absyn_PtrInfo f1;};struct Cyc_Absyn_ArrayType_Absyn_Type_struct{int tag;struct Cyc_Absyn_ArrayInfo f1;};struct Cyc_Absyn_FnType_Absyn_Type_struct{int tag;struct Cyc_Absyn_FnInfo f1;};struct Cyc_Absyn_AnonAggrType_Absyn_Type_struct{int tag;enum Cyc_Absyn_AggrKind f1;int f2;struct Cyc_List_List*f3;};struct Cyc_Absyn_ValueofType_Absyn_Type_struct{int tag;struct Cyc_Absyn_Exp*f1;};struct Cyc_Absyn_TypeDeclType_Absyn_Type_struct{int tag;struct Cyc_Absyn_TypeDecl*f1;void**f2;};struct Cyc_Absyn_SubsetType_Absyn_Type_struct{int tag;struct Cyc_Absyn_Vardecl*f1;struct Cyc_Absyn_Exp*f2;struct Cyc_AssnDef_ExistAssnFn*f3;};struct Cyc_Absyn_Const_att_Absyn_Attribute_struct{int tag;};struct Cyc_Absyn_NoTypes_Absyn_Funcparams_struct{int tag;struct Cyc_List_List*f1;unsigned f2;};struct Cyc_Absyn_WithTypes_Absyn_Funcparams_struct{int tag;struct Cyc_List_List*f1;int f2;struct Cyc_Absyn_VarargInfo*f3;void*f4;struct Cyc_List_List*f5;struct Cyc_List_List*f6;struct Cyc_Absyn_Exp*f7;struct Cyc_Absyn_Exp*f8;struct Cyc_Absyn_Exp*f9;struct Cyc_Absyn_Exp*f10;};struct Cyc_Absyn_Carray_mod_Absyn_Type_modifier_struct{int tag;void*f1;unsigned f2;};struct Cyc_Absyn_ConstArray_mod_Absyn_Type_modifier_struct{int tag;struct Cyc_Absyn_Exp*f1;void*f2;unsigned f3;};struct Cyc_Absyn_Pointer_mod_Absyn_Type_modifier_struct{int tag;struct Cyc_Absyn_PtrAtts f1;struct Cyc_Absyn_Tqual f2;};struct Cyc_Absyn_Function_mod_Absyn_Type_modifier_struct{int tag;void*f1;};struct Cyc_Absyn_TypeParams_mod_Absyn_Type_modifier_struct{int tag;struct Cyc_List_List*f1;unsigned f2;int f3;};struct Cyc_Absyn_Attributes_mod_Absyn_Type_modifier_struct{int tag;unsigned f1;struct Cyc_List_List*f2;};struct _union_Cnst_Null_c{int tag;int val;};struct _tuple3{enum Cyc_Absyn_Sign f0;char f1;};struct _union_Cnst_Char_c{int tag;struct _tuple3 val;};struct _union_Cnst_Wchar_c{int tag;struct _fat_ptr val;};struct _tuple4{enum Cyc_Absyn_Sign f0;short f1;};struct _union_Cnst_Short_c{int tag;struct _tuple4 val;};struct _tuple5{enum Cyc_Absyn_Sign f0;int f1;};struct _union_Cnst_Int_c{int tag;struct _tuple5 val;};struct _tuple6{enum Cyc_Absyn_Sign f0;long long f1;};struct _union_Cnst_LongLong_c{int tag;struct _tuple6 val;};struct _tuple7{struct _fat_ptr f0;int f1;};struct _union_Cnst_Float_c{int tag;struct _tuple7 val;};struct _union_Cnst_String_c{int tag;struct _fat_ptr val;};struct _union_Cnst_Wstring_c{int tag;struct _fat_ptr val;};union Cyc_Absyn_Cnst{struct _union_Cnst_Null_c Null_c;struct _union_Cnst_Char_c Char_c;struct _union_Cnst_Wchar_c Wchar_c;struct _union_Cnst_Short_c Short_c;struct _union_Cnst_Int_c Int_c;struct _union_Cnst_LongLong_c LongLong_c;struct _union_Cnst_Float_c Float_c;struct _union_Cnst_String_c String_c;struct _union_Cnst_Wstring_c Wstring_c;};
# 526 "absyn.h"
enum Cyc_Absyn_Primop{Cyc_Absyn_Plus =0U,Cyc_Absyn_Times =1U,Cyc_Absyn_Minus =2U,Cyc_Absyn_Div =3U,Cyc_Absyn_Mod =4U,Cyc_Absyn_Eq =5U,Cyc_Absyn_Neq =6U,Cyc_Absyn_Gt =7U,Cyc_Absyn_Lt =8U,Cyc_Absyn_Gte =9U,Cyc_Absyn_Lte =10U,Cyc_Absyn_Not =11U,Cyc_Absyn_Bitnot =12U,Cyc_Absyn_Bitand =13U,Cyc_Absyn_Bitor =14U,Cyc_Absyn_Bitxor =15U,Cyc_Absyn_Bitlshift =16U,Cyc_Absyn_Bitlrshift =17U,Cyc_Absyn_Numelts =18U,Cyc_Absyn_Tagof =19U,Cyc_Absyn_UDiv =20U,Cyc_Absyn_UMod =21U,Cyc_Absyn_UGt =22U,Cyc_Absyn_ULt =23U,Cyc_Absyn_UGte =24U,Cyc_Absyn_ULte =25U};
# 533
enum Cyc_Absyn_Incrementor{Cyc_Absyn_PreInc =0U,Cyc_Absyn_PostInc =1U,Cyc_Absyn_PreDec =2U,Cyc_Absyn_PostDec =3U};struct Cyc_Absyn_StructField_Absyn_OffsetofField_struct{int tag;struct _fat_ptr*f1;};
# 551
enum Cyc_Absyn_Coercion{Cyc_Absyn_Unknown_coercion =0U,Cyc_Absyn_No_coercion =1U,Cyc_Absyn_Null_to_NonNull =2U,Cyc_Absyn_Subset_coercion =3U,Cyc_Absyn_Other_coercion =4U};struct Cyc_Absyn_ArrayElement_Absyn_Designator_struct{int tag;struct Cyc_Absyn_Exp*f1;};struct Cyc_Absyn_FieldName_Absyn_Designator_struct{int tag;struct _fat_ptr*f1;};
# 566
enum Cyc_Absyn_MallocKind{Cyc_Absyn_Malloc =0U,Cyc_Absyn_Calloc =1U,Cyc_Absyn_Vmalloc =2U};struct Cyc_Absyn_MallocInfo{enum Cyc_Absyn_MallocKind mknd;struct Cyc_Absyn_Exp*rgn;struct Cyc_Absyn_Exp*aqual;void**elt_type;struct Cyc_Absyn_Exp*num_elts;int fat_result;int inline_call;};struct Cyc_Absyn_Const_e_Absyn_Raw_exp_struct{int tag;union Cyc_Absyn_Cnst f1;};struct Cyc_Absyn_Var_e_Absyn_Raw_exp_struct{int tag;void*f1;};struct Cyc_Absyn_SeqExp_e_Absyn_Raw_exp_struct{int tag;struct Cyc_Absyn_Exp*f1;struct Cyc_Absyn_Exp*f2;};struct _tuple8{struct _fat_ptr*f0;struct Cyc_Absyn_Tqual f1;void*f2;};struct Cyc_Absyn_CompoundLit_e_Absyn_Raw_exp_struct{int tag;struct _tuple8*f1;struct Cyc_List_List*f2;};struct Cyc_Absyn_Comprehension_e_Absyn_Raw_exp_struct{int tag;struct Cyc_Absyn_Vardecl*f1;struct Cyc_Absyn_Exp*f2;struct Cyc_Absyn_Exp*f3;int f4;};struct Cyc_Absyn_ComprehensionNoinit_e_Absyn_Raw_exp_struct{int tag;struct Cyc_Absyn_Exp*f1;void*f2;int f3;};struct Cyc_Absyn_Aggregate_e_Absyn_Raw_exp_struct{int tag;struct _tuple0*f1;struct Cyc_List_List*f2;struct Cyc_List_List*f3;struct Cyc_Absyn_Aggrdecl*f4;};struct Cyc_Absyn_Malloc_e_Absyn_Raw_exp_struct{int tag;struct Cyc_Absyn_MallocInfo f1;};struct Cyc_Absyn_UnresolvedMem_e_Absyn_Raw_exp_struct{int tag;struct Cyc_Core_Opt*f1;struct Cyc_List_List*f2;};struct Cyc_Absyn_Tagcheck_e_Absyn_Raw_exp_struct{int tag;struct Cyc_Absyn_Exp*f1;struct _fat_ptr*f2;};struct Cyc_Absyn_Valueof_e_Absyn_Raw_exp_struct{int tag;void*f1;};struct Cyc_Absyn_Asm_e_Absyn_Raw_exp_struct{int tag;int f1;struct _fat_ptr f2;struct Cyc_List_List*f3;struct Cyc_List_List*f4;struct Cyc_List_List*f5;};struct Cyc_Absyn_Exp{void*topt;void*r;unsigned loc;void*annot;};struct Cyc_Absyn_Decl_s_Absyn_Raw_stmt_struct{int tag;struct Cyc_Absyn_Decl*f1;struct Cyc_Absyn_Stmt*f2;};struct Cyc_Absyn_Label_s_Absyn_Raw_stmt_struct{int tag;struct _fat_ptr*f1;struct Cyc_Absyn_Stmt*f2;};struct Cyc_Absyn_Stmt{void*r;unsigned loc;void*annot;};struct Cyc_Absyn_Wild_p_Absyn_Raw_pat_struct{int tag;};struct Cyc_Absyn_Var_p_Absyn_Raw_pat_struct{int tag;struct Cyc_Absyn_Vardecl*f1;struct Cyc_Absyn_Pat*f2;};struct Cyc_Absyn_AliasVar_p_Absyn_Raw_pat_struct{int tag;struct Cyc_Absyn_Tvar*f1;struct Cyc_Absyn_Vardecl*f2;};struct Cyc_Absyn_Reference_p_Absyn_Raw_pat_struct{int tag;struct Cyc_Absyn_Vardecl*f1;struct Cyc_Absyn_Pat*f2;};struct Cyc_Absyn_TagInt_p_Absyn_Raw_pat_struct{int tag;struct Cyc_Absyn_Tvar*f1;struct Cyc_Absyn_Vardecl*f2;};struct Cyc_Absyn_Pointer_p_Absyn_Raw_pat_struct{int tag;struct Cyc_Absyn_Pat*f1;};struct Cyc_Absyn_Aggr_p_Absyn_Raw_pat_struct{int tag;void*f1;int f2;struct Cyc_List_List*f3;struct Cyc_List_List*f4;int f5;};struct Cyc_Absyn_Null_p_Absyn_Raw_pat_struct{int tag;};struct Cyc_Absyn_Int_p_Absyn_Raw_pat_struct{int tag;enum Cyc_Absyn_Sign f1;int f2;};struct Cyc_Absyn_Char_p_Absyn_Raw_pat_struct{int tag;char f1;};struct Cyc_Absyn_Float_p_Absyn_Raw_pat_struct{int tag;struct _fat_ptr f1;int f2;};struct Cyc_Absyn_UnknownId_p_Absyn_Raw_pat_struct{int tag;struct _tuple0*f1;};struct Cyc_Absyn_UnknownCall_p_Absyn_Raw_pat_struct{int tag;struct _tuple0*f1;struct Cyc_List_List*f2;int f3;};struct Cyc_Absyn_Exp_p_Absyn_Raw_pat_struct{int tag;struct Cyc_Absyn_Exp*f1;};
# 734 "absyn.h"
extern struct Cyc_Absyn_Wild_p_Absyn_Raw_pat_struct Cyc_Absyn_Wild_p_val;
extern struct Cyc_Absyn_Null_p_Absyn_Raw_pat_struct Cyc_Absyn_Null_p_val;struct Cyc_Absyn_Pat{void*r;void*topt;unsigned loc;};struct Cyc_Absyn_Switch_clause{struct Cyc_Absyn_Pat*pattern;struct Cyc_Core_Opt*pat_vars;struct Cyc_Absyn_Exp*where_clause;struct Cyc_Absyn_Stmt*body;unsigned loc;};struct Cyc_Absyn_Unresolved_b_Absyn_Binding_struct{int tag;struct _tuple0*f1;};struct Cyc_Absyn_Vardecl{enum Cyc_Absyn_Scope sc;struct _tuple0*name;unsigned varloc;struct Cyc_Absyn_Tqual tq;void*type;struct Cyc_Absyn_Exp*initializer;void*rgn;struct Cyc_List_List*attributes;int escapes;int is_proto;struct Cyc_Absyn_Exp*rename;};struct Cyc_Absyn_Fndecl{enum Cyc_Absyn_Scope sc;int is_inline;struct _tuple0*name;struct Cyc_Absyn_Stmt*body;struct Cyc_Absyn_FnInfo i;void*cached_type;struct Cyc_Core_Opt*param_vardecls;struct Cyc_Absyn_Vardecl*fn_vardecl;enum Cyc_Absyn_Scope orig_scope;int escapes;};struct Cyc_Absyn_Aggrfield{struct _fat_ptr*name;struct Cyc_Absyn_Tqual tq;void*type;struct Cyc_Absyn_Exp*width;struct Cyc_List_List*attributes;struct Cyc_Absyn_Exp*requires_clause;};struct Cyc_Absyn_AggrdeclImpl{struct Cyc_List_List*exist_vars;struct Cyc_List_List*qual_bnd;struct Cyc_List_List*fields;int tagged;struct Cyc_List_List*effconstr;};struct Cyc_Absyn_Aggrdecl{enum Cyc_Absyn_AggrKind kind;enum Cyc_Absyn_Scope sc;struct _tuple0*name;struct Cyc_List_List*tvs;struct Cyc_Absyn_AggrdeclImpl*impl;struct Cyc_List_List*attributes;int expected_mem_kind;};struct Cyc_Absyn_Datatypefield{struct _tuple0*name;struct Cyc_List_List*typs;unsigned loc;enum Cyc_Absyn_Scope sc;};struct Cyc_Absyn_Datatypedecl{enum Cyc_Absyn_Scope sc;struct _tuple0*name;struct Cyc_List_List*tvs;struct Cyc_Core_Opt*fields;int is_extensible;};struct Cyc_Absyn_Enumfield{struct _tuple0*name;struct Cyc_Absyn_Exp*tag;unsigned loc;};struct Cyc_Absyn_Enumdecl{enum Cyc_Absyn_Scope sc;struct _tuple0*name;struct Cyc_Core_Opt*fields;};struct Cyc_Absyn_Typedefdecl{struct _tuple0*name;struct Cyc_Absyn_Tqual tq;struct Cyc_List_List*tvs;struct Cyc_Core_Opt*kind;void*defn;struct Cyc_List_List*atts;int extern_c;};struct Cyc_Absyn_Var_d_Absyn_Raw_decl_struct{int tag;struct Cyc_Absyn_Vardecl*f1;};struct Cyc_Absyn_Fn_d_Absyn_Raw_decl_struct{int tag;struct Cyc_Absyn_Fndecl*f1;};struct Cyc_Absyn_Aggr_d_Absyn_Raw_decl_struct{int tag;struct Cyc_Absyn_Aggrdecl*f1;};struct Cyc_Absyn_Datatype_d_Absyn_Raw_decl_struct{int tag;struct Cyc_Absyn_Datatypedecl*f1;};struct Cyc_Absyn_Enum_d_Absyn_Raw_decl_struct{int tag;struct Cyc_Absyn_Enumdecl*f1;};struct Cyc_Absyn_Typedef_d_Absyn_Raw_decl_struct{int tag;struct Cyc_Absyn_Typedefdecl*f1;};struct Cyc_Absyn_Namespace_d_Absyn_Raw_decl_struct{int tag;struct _fat_ptr*f1;struct Cyc_List_List*f2;};struct Cyc_Absyn_Using_d_Absyn_Raw_decl_struct{int tag;struct _tuple0*f1;struct Cyc_List_List*f2;};struct Cyc_Absyn_ExternC_d_Absyn_Raw_decl_struct{int tag;struct Cyc_List_List*f1;};struct _tuple10{unsigned f0;struct Cyc_List_List*f1;};struct Cyc_Absyn_ExternCinclude_d_Absyn_Raw_decl_struct{int tag;struct Cyc_List_List*f1;struct Cyc_List_List*f2;struct Cyc_List_List*f3;struct _tuple10*f4;};struct Cyc_Absyn_Porton_d_Absyn_Raw_decl_struct{int tag;};struct Cyc_Absyn_Portoff_d_Absyn_Raw_decl_struct{int tag;};struct Cyc_Absyn_Tempeston_d_Absyn_Raw_decl_struct{int tag;};struct Cyc_Absyn_Tempestoff_d_Absyn_Raw_decl_struct{int tag;};
# 902
extern struct Cyc_Absyn_Porton_d_Absyn_Raw_decl_struct Cyc_Absyn_Porton_d_val;
extern struct Cyc_Absyn_Portoff_d_Absyn_Raw_decl_struct Cyc_Absyn_Portoff_d_val;
extern struct Cyc_Absyn_Tempeston_d_Absyn_Raw_decl_struct Cyc_Absyn_Tempeston_d_val;
extern struct Cyc_Absyn_Tempestoff_d_Absyn_Raw_decl_struct Cyc_Absyn_Tempestoff_d_val;struct Cyc_Absyn_Decl{void*r;unsigned loc;};
# 928
int Cyc_Absyn_is_qvar_qualified(struct _tuple0*);
union Cyc_Absyn_Nmspace Cyc_Absyn_Loc_n (void);
union Cyc_Absyn_Nmspace Cyc_Absyn_Rel_n(struct Cyc_List_List*);
# 936
struct Cyc_Absyn_Tqual Cyc_Absyn_empty_tqual(unsigned);
struct Cyc_Absyn_Tqual Cyc_Absyn_combine_tqual(struct Cyc_Absyn_Tqual,struct Cyc_Absyn_Tqual);
# 942
void*Cyc_Absyn_compress(void*);
# 955
void*Cyc_Absyn_new_evar(struct Cyc_Core_Opt*,struct Cyc_Core_Opt*);
# 957
void*Cyc_Absyn_wildtyp(struct Cyc_Core_Opt*);
void*Cyc_Absyn_int_type(enum Cyc_Absyn_Sign,enum Cyc_Absyn_Size_of);
# 960
extern void*Cyc_Absyn_char_type;extern void*Cyc_Absyn_uint_type;
# 962
extern void*Cyc_Absyn_sint_type;
# 964
extern void*Cyc_Absyn_float_type;extern void*Cyc_Absyn_double_type;extern void*Cyc_Absyn_long_double_type;extern void*Cyc_Absyn_float128_type;
# 967
extern void*Cyc_Absyn_complex_type(void*);
# 969
extern void*Cyc_Absyn_heap_rgn_type;extern void*Cyc_Absyn_unique_rgn_shorthand_type;extern void*Cyc_Absyn_refcnt_rgn_shorthand_type;
# 971
extern void*Cyc_Absyn_al_qual_type;extern void*Cyc_Absyn_un_qual_type;extern void*Cyc_Absyn_rc_qual_type;extern void*Cyc_Absyn_rtd_qual_type;
# 975
extern void*Cyc_Absyn_true_type;extern void*Cyc_Absyn_false_type;
# 977
extern void*Cyc_Absyn_void_type;extern void*Cyc_Absyn_var_type(struct Cyc_Absyn_Tvar*);extern void*Cyc_Absyn_tag_type(void*);extern void*Cyc_Absyn_rgn_handle_type(void*);extern void*Cyc_Absyn_aqual_handle_type(void*);extern void*Cyc_Absyn_aqual_var_type(void*,void*);extern void*Cyc_Absyn_valueof_type(struct Cyc_Absyn_Exp*);extern void*Cyc_Absyn_typeof_type(struct Cyc_Absyn_Exp*);extern void*Cyc_Absyn_join_eff(struct Cyc_List_List*);extern void*Cyc_Absyn_regionsof_eff(void*);extern void*Cyc_Absyn_enum_type(struct _tuple0*,struct Cyc_Absyn_Enumdecl*);extern void*Cyc_Absyn_anon_enum_type(struct Cyc_List_List*);extern void*Cyc_Absyn_builtin_type(struct _fat_ptr,struct Cyc_Absyn_Kind*);extern void*Cyc_Absyn_tuple_type(struct Cyc_List_List*);extern void*Cyc_Absyn_typedef_type(struct _tuple0*,struct Cyc_List_List*,struct Cyc_Absyn_Typedefdecl*,void*);
# 996
struct Cyc_Absyn_FieldName_Absyn_Designator_struct*Cyc_Absyn_tuple_field_designator(int);
# 1008
extern void*Cyc_Absyn_fat_bound_type;
void*Cyc_Absyn_thin_bounds_type(void*);
void*Cyc_Absyn_thin_bounds_exp(struct Cyc_Absyn_Exp*);
# 1012
void*Cyc_Absyn_bounds_one (void);
void*Cyc_Absyn_cvar_type(struct Cyc_Core_Opt*);
void*Cyc_Absyn_cvar_type_name(struct Cyc_Core_Opt*,struct _fat_ptr);
# 1018
void*Cyc_Absyn_pointer_type(struct Cyc_Absyn_PtrInfo);
# 1038
void*Cyc_Absyn_array_type(void*,struct Cyc_Absyn_Tqual,struct Cyc_Absyn_Exp*,void*,unsigned);
# 1041
void*Cyc_Absyn_datatype_type(union Cyc_Absyn_DatatypeInfo,struct Cyc_List_List*);
void*Cyc_Absyn_datatype_field_type(union Cyc_Absyn_DatatypeFieldInfo,struct Cyc_List_List*);
void*Cyc_Absyn_aggr_type(union Cyc_Absyn_AggrInfo,struct Cyc_List_List*);
# 1045
void*Cyc_Absyn_aqualsof_type(void*);
# 1058
struct Cyc_Absyn_Exp*Cyc_Absyn_new_exp(void*,unsigned);
struct Cyc_Absyn_Exp*Cyc_Absyn_New_exp(struct Cyc_Absyn_Exp*,struct Cyc_Absyn_Exp*,struct Cyc_Absyn_Exp*,unsigned);
struct Cyc_Absyn_Exp*Cyc_Absyn_copy_exp(struct Cyc_Absyn_Exp*);
struct Cyc_Absyn_Exp*Cyc_Absyn_const_exp(union Cyc_Absyn_Cnst,unsigned);
struct Cyc_Absyn_Exp*Cyc_Absyn_null_exp(unsigned);
# 1064
struct Cyc_Absyn_Exp*Cyc_Absyn_true_exp(unsigned);
struct Cyc_Absyn_Exp*Cyc_Absyn_false_exp(unsigned);
struct Cyc_Absyn_Exp*Cyc_Absyn_int_exp(enum Cyc_Absyn_Sign,int,unsigned);
# 1068
struct Cyc_Absyn_Exp*Cyc_Absyn_uint_exp(unsigned,unsigned);
struct Cyc_Absyn_Exp*Cyc_Absyn_char_exp(char,unsigned);
struct Cyc_Absyn_Exp*Cyc_Absyn_wchar_exp(struct _fat_ptr,unsigned);
struct Cyc_Absyn_Exp*Cyc_Absyn_float_exp(struct _fat_ptr,int,unsigned);
struct Cyc_Absyn_Exp*Cyc_Absyn_string_exp(struct _fat_ptr,unsigned);
struct Cyc_Absyn_Exp*Cyc_Absyn_wstring_exp(struct _fat_ptr,unsigned);
struct Cyc_Absyn_Exp*Cyc_Absyn_var_exp(struct _tuple0*,unsigned);
# 1076
struct Cyc_Absyn_Exp*Cyc_Absyn_unknownid_exp(struct _tuple0*,unsigned);
struct Cyc_Absyn_Exp*Cyc_Absyn_tagof_exp(struct Cyc_Absyn_Exp*,unsigned);
struct Cyc_Absyn_Exp*Cyc_Absyn_pragma_exp(struct _fat_ptr,unsigned);
struct Cyc_Absyn_Exp*Cyc_Absyn_primop_exp(enum Cyc_Absyn_Primop,struct Cyc_List_List*,unsigned);
struct Cyc_Absyn_Exp*Cyc_Absyn_prim1_exp(enum Cyc_Absyn_Primop,struct Cyc_Absyn_Exp*,unsigned);
struct Cyc_Absyn_Exp*Cyc_Absyn_prim2_exp(enum Cyc_Absyn_Primop,struct Cyc_Absyn_Exp*,struct Cyc_Absyn_Exp*,unsigned);
struct Cyc_Absyn_Exp*Cyc_Absyn_swap_exp(struct Cyc_Absyn_Exp*,struct Cyc_Absyn_Exp*,unsigned);
# 1087
struct Cyc_Absyn_Exp*Cyc_Absyn_eq_exp(struct Cyc_Absyn_Exp*,struct Cyc_Absyn_Exp*,unsigned);
struct Cyc_Absyn_Exp*Cyc_Absyn_neq_exp(struct Cyc_Absyn_Exp*,struct Cyc_Absyn_Exp*,unsigned);
struct Cyc_Absyn_Exp*Cyc_Absyn_gt_exp(struct Cyc_Absyn_Exp*,struct Cyc_Absyn_Exp*,unsigned);
struct Cyc_Absyn_Exp*Cyc_Absyn_lt_exp(struct Cyc_Absyn_Exp*,struct Cyc_Absyn_Exp*,unsigned);
struct Cyc_Absyn_Exp*Cyc_Absyn_gte_exp(struct Cyc_Absyn_Exp*,struct Cyc_Absyn_Exp*,unsigned);
struct Cyc_Absyn_Exp*Cyc_Absyn_lte_exp(struct Cyc_Absyn_Exp*,struct Cyc_Absyn_Exp*,unsigned);
# 1097
struct Cyc_Absyn_Exp*Cyc_Absyn_assignop_exp(struct Cyc_Absyn_Exp*,struct Cyc_Core_Opt*,struct Cyc_Absyn_Exp*,unsigned);
# 1099
struct Cyc_Absyn_Exp*Cyc_Absyn_increment_exp(struct Cyc_Absyn_Exp*,enum Cyc_Absyn_Incrementor,unsigned);
struct Cyc_Absyn_Exp*Cyc_Absyn_conditional_exp(struct Cyc_Absyn_Exp*,struct Cyc_Absyn_Exp*,struct Cyc_Absyn_Exp*,unsigned);
struct Cyc_Absyn_Exp*Cyc_Absyn_and_exp(struct Cyc_Absyn_Exp*,struct Cyc_Absyn_Exp*,unsigned);
struct Cyc_Absyn_Exp*Cyc_Absyn_or_exp(struct Cyc_Absyn_Exp*,struct Cyc_Absyn_Exp*,unsigned);
struct Cyc_Absyn_Exp*Cyc_Absyn_seq_exp(struct Cyc_Absyn_Exp*,struct Cyc_Absyn_Exp*,unsigned);
struct Cyc_Absyn_Exp*Cyc_Absyn_unknowncall_exp(struct Cyc_Absyn_Exp*,struct Cyc_List_List*,unsigned);
# 1106
struct Cyc_Absyn_Exp*Cyc_Absyn_throw_exp(struct Cyc_Absyn_Exp*,unsigned);
# 1108
struct Cyc_Absyn_Exp*Cyc_Absyn_noinstantiate_exp(struct Cyc_Absyn_Exp*,unsigned);
struct Cyc_Absyn_Exp*Cyc_Absyn_instantiate_exp(struct Cyc_Absyn_Exp*,struct Cyc_List_List*,unsigned);
struct Cyc_Absyn_Exp*Cyc_Absyn_cast_exp(void*,struct Cyc_Absyn_Exp*,int,enum Cyc_Absyn_Coercion,unsigned);
struct Cyc_Absyn_Exp*Cyc_Absyn_address_exp(struct Cyc_Absyn_Exp*,unsigned);
struct Cyc_Absyn_Exp*Cyc_Absyn_sizeoftype_exp(void*,unsigned);
struct Cyc_Absyn_Exp*Cyc_Absyn_sizeofexp_exp(struct Cyc_Absyn_Exp*,unsigned);
struct Cyc_Absyn_Exp*Cyc_Absyn_offsetof_exp(void*,struct Cyc_List_List*,unsigned);
struct Cyc_Absyn_Exp*Cyc_Absyn_deref_exp(struct Cyc_Absyn_Exp*,unsigned);
struct Cyc_Absyn_Exp*Cyc_Absyn_aggrmember_exp(struct Cyc_Absyn_Exp*,struct _fat_ptr*,unsigned);
struct Cyc_Absyn_Exp*Cyc_Absyn_aggrarrow_exp(struct Cyc_Absyn_Exp*,struct _fat_ptr*,unsigned);
struct Cyc_Absyn_Exp*Cyc_Absyn_subscript_exp(struct Cyc_Absyn_Exp*,struct Cyc_Absyn_Exp*,unsigned);
struct Cyc_Absyn_Exp*Cyc_Absyn_tuple_exp(struct Cyc_List_List*,unsigned);
struct Cyc_Absyn_Exp*Cyc_Absyn_stmt_exp(struct Cyc_Absyn_Stmt*,unsigned);
# 1122
struct Cyc_Absyn_Exp*Cyc_Absyn_valueof_exp(void*,unsigned);
# 1126
struct Cyc_Absyn_Exp*Cyc_Absyn_extension_exp(struct Cyc_Absyn_Exp*,unsigned);
struct Cyc_Absyn_Exp*Cyc_Absyn_assert_exp(struct Cyc_Absyn_Exp*,int,unsigned);
struct Cyc_Absyn_Exp*Cyc_Absyn_assert_false_exp(struct Cyc_Absyn_Exp*,unsigned);
# 1140
struct Cyc_Absyn_Stmt*Cyc_Absyn_new_stmt(void*,unsigned);
struct Cyc_Absyn_Stmt*Cyc_Absyn_skip_stmt(unsigned);
struct Cyc_Absyn_Stmt*Cyc_Absyn_exp_stmt(struct Cyc_Absyn_Exp*,unsigned);
struct Cyc_Absyn_Stmt*Cyc_Absyn_seq_stmt(struct Cyc_Absyn_Stmt*,struct Cyc_Absyn_Stmt*,unsigned);
# 1145
struct Cyc_Absyn_Stmt*Cyc_Absyn_return_stmt(struct Cyc_Absyn_Exp*,unsigned);
struct Cyc_Absyn_Stmt*Cyc_Absyn_ifthenelse_stmt(struct Cyc_Absyn_Exp*,struct Cyc_Absyn_Stmt*,struct Cyc_Absyn_Stmt*,unsigned);
struct Cyc_Absyn_Stmt*Cyc_Absyn_while_stmt(struct Cyc_Absyn_Exp*,struct Cyc_Absyn_Stmt*,unsigned);
struct Cyc_Absyn_Stmt*Cyc_Absyn_break_stmt(unsigned);
struct Cyc_Absyn_Stmt*Cyc_Absyn_continue_stmt(unsigned);
struct Cyc_Absyn_Stmt*Cyc_Absyn_for_stmt(struct Cyc_Absyn_Exp*,struct Cyc_Absyn_Exp*,struct Cyc_Absyn_Exp*,struct Cyc_Absyn_Stmt*,unsigned);
struct Cyc_Absyn_Stmt*Cyc_Absyn_switch_stmt(struct Cyc_Absyn_Exp*,struct Cyc_List_List*,unsigned);
struct Cyc_Absyn_Stmt*Cyc_Absyn_fallthru_stmt(struct Cyc_List_List*,unsigned);
# 1156
struct Cyc_Absyn_Stmt*Cyc_Absyn_do_stmt(struct Cyc_Absyn_Stmt*,struct Cyc_Absyn_Exp*,unsigned);
struct Cyc_Absyn_Stmt*Cyc_Absyn_goto_stmt(struct _fat_ptr*,unsigned);
# 1159
struct Cyc_Absyn_Stmt*Cyc_Absyn_trycatch_stmt(struct Cyc_Absyn_Stmt*,struct Cyc_List_List*,unsigned);
# 1162
struct Cyc_Absyn_Pat*Cyc_Absyn_new_pat(void*,unsigned);
struct Cyc_Absyn_Pat*Cyc_Absyn_exp_pat(struct Cyc_Absyn_Exp*);
# 1166
struct Cyc_Absyn_Decl*Cyc_Absyn_new_decl(void*,unsigned);
struct Cyc_Absyn_Decl*Cyc_Absyn_let_decl(struct Cyc_Absyn_Pat*,struct Cyc_Absyn_Exp*,unsigned);
struct Cyc_Absyn_Decl*Cyc_Absyn_letv_decl(struct Cyc_List_List*,unsigned);
struct Cyc_Absyn_Decl*Cyc_Absyn_region_decl(struct Cyc_Absyn_Tvar*,struct Cyc_Absyn_Vardecl*,struct Cyc_Absyn_Exp*,unsigned);
# 1171
struct Cyc_Absyn_Vardecl*Cyc_Absyn_new_vardecl(unsigned,struct _tuple0*,void*,struct Cyc_Absyn_Exp*,struct Cyc_Absyn_Exp*);
# 1173
struct Cyc_Absyn_AggrdeclImpl*Cyc_Absyn_aggrdecl_impl(struct Cyc_List_List*,struct Cyc_List_List*,struct Cyc_List_List*,struct Cyc_List_List*,int);
# 1182
struct Cyc_Absyn_TypeDecl*Cyc_Absyn_aggr_tdecl(enum Cyc_Absyn_AggrKind,enum Cyc_Absyn_Scope,struct _tuple0*,struct Cyc_List_List*,struct Cyc_Absyn_AggrdeclImpl*,struct Cyc_List_List*,unsigned);
# 1189
struct Cyc_Absyn_Decl*Cyc_Absyn_datatype_decl(enum Cyc_Absyn_Scope,struct _tuple0*,struct Cyc_List_List*,struct Cyc_Core_Opt*,int,unsigned);
# 1192
struct Cyc_Absyn_TypeDecl*Cyc_Absyn_datatype_tdecl(enum Cyc_Absyn_Scope,struct _tuple0*,struct Cyc_List_List*,struct Cyc_Core_Opt*,int,unsigned);
# 1197
void*Cyc_Absyn_function_type(struct Cyc_List_List*,void*,struct Cyc_Absyn_Tqual,void*,struct Cyc_List_List*,int,struct Cyc_Absyn_VarargInfo*,struct Cyc_List_List*,struct Cyc_List_List*,struct Cyc_List_List*,struct Cyc_Absyn_Exp*,struct Cyc_Absyn_Exp*,struct Cyc_Absyn_Exp*,struct Cyc_Absyn_Exp*);
# 37 "warn.h"
void Cyc_Warn_warn(unsigned,struct _fat_ptr,struct _fat_ptr);
# 43
void Cyc_Warn_err(unsigned,struct _fat_ptr,struct _fat_ptr);
# 48
void*Cyc_Warn_impos(struct _fat_ptr,struct _fat_ptr);struct Cyc_Warn_String_Warn_Warg_struct{int tag;struct _fat_ptr f1;};
# 75
void Cyc_Warn_err2(unsigned,struct _fat_ptr);
# 77
void Cyc_Warn_warn2(unsigned,struct _fat_ptr);
# 30 "flags.h"
extern int Cyc_Flags_porting_c_code;
# 39
extern int Cyc_Flags_override_fat;
extern int Cyc_Flags_interproc;
extern int Cyc_Flags_resolve;
# 46
extern int Cyc_Flags_no_register;
# 30 "kinds.h"
extern struct Cyc_Absyn_Kind Cyc_Kinds_bk;
# 32
extern struct Cyc_Absyn_Kind Cyc_Kinds_ek;
extern struct Cyc_Absyn_Kind Cyc_Kinds_ik;
# 36
extern struct Cyc_Absyn_Kind Cyc_Kinds_aqk;
# 54 "kinds.h"
extern struct Cyc_Core_Opt Cyc_Kinds_bko;
# 56
extern struct Cyc_Core_Opt Cyc_Kinds_iko;
extern struct Cyc_Core_Opt Cyc_Kinds_eko;
# 59
extern struct Cyc_Core_Opt Cyc_Kinds_ptrbko;
extern struct Cyc_Core_Opt Cyc_Kinds_aqko;
# 76 "kinds.h"
struct Cyc_Core_Opt*Cyc_Kinds_kind_to_opt(struct Cyc_Absyn_Kind*);
void*Cyc_Kinds_kind_to_bound(struct Cyc_Absyn_Kind*);
# 80
struct Cyc_Absyn_Kind*Cyc_Kinds_id_to_kind(struct _fat_ptr,unsigned);
# 89
void*Cyc_Kinds_compress_kb(void*);
# 101
void*Cyc_Kinds_consistent_aliashint(unsigned,void*,void*);
# 42 "tcutil.h"
int Cyc_Tcutil_is_array_type(void*);
# 92
void*Cyc_Tcutil_copy_type(void*);
# 215 "tcutil.h"
struct Cyc_Absyn_Tvar*Cyc_Tcutil_new_tvar(void*);
# 246
void*Cyc_Tcutil_promote_array(void*,void*,void*,int);
# 256
void*Cyc_Tcutil_any_bool(struct Cyc_List_List*);
# 278
void*Cyc_Tcutil_ptrbnd_cvar_equivalent(void*);
# 29 "attributes.h"
extern struct Cyc_Absyn_Const_att_Absyn_Attribute_struct Cyc_Atts_Const_att_val;
# 46
void*Cyc_Atts_parse_nullary_att(unsigned,struct _fat_ptr);
void*Cyc_Atts_parse_unary_att(unsigned,struct _fat_ptr,unsigned,struct Cyc_Absyn_Exp*);
void*Cyc_Atts_parse_format_att(unsigned,unsigned,struct _fat_ptr,struct _fat_ptr,unsigned,unsigned);
# 68
int Cyc_Atts_fntype_att(void*);
# 29 "currgn.h"
extern struct _fat_ptr Cyc_CurRgn_curr_rgn_name;
# 31
void*Cyc_CurRgn_curr_rgn_type (void);
# 68 "absynpp.h"
struct _fat_ptr Cyc_Absynpp_cnst2string(union Cyc_Absyn_Cnst);
struct _fat_ptr Cyc_Absynpp_exp2string(struct Cyc_Absyn_Exp*);
struct _fat_ptr Cyc_Absynpp_stmt2string(struct Cyc_Absyn_Stmt*);
struct _fat_ptr Cyc_Absynpp_qvar2string(struct _tuple0*);
# 17 "bansheeif.h"
void*Cyc_BansheeIf_equality_constraint(void*,void*);
void*Cyc_BansheeIf_cond_equality_constraint(void*,void*);
void*Cyc_BansheeIf_inclusion_constraint(void*,void*);
void*Cyc_BansheeIf_implication_constraint(void*,void*);
void*Cyc_BansheeIf_and_constraint(void*,void*);
void*Cyc_BansheeIf_or_constraint(void*,void*);
void*Cyc_BansheeIf_not_constraint(void*);
void*Cyc_BansheeIf_cmpeq_constraint(void*,void*);
void*Cyc_BansheeIf_check_constraint(void*);
# 27
void*Cyc_BansheeIf_add_location(struct _fat_ptr,void*);extern char Cyc_Parse_Exit[5U];struct Cyc_Parse_Exit_exn_struct{char*tag;};struct Cyc_Parse_FlatList{struct Cyc_Parse_FlatList*tl;void*hd[0U] __attribute__((aligned )) ;};struct Cyc_Parse_Type_specifier{int Signed_spec: 1;int Unsigned_spec: 1;int Short_spec: 1;int Long_spec: 1;int Long_Long_spec: 1;int Complex_spec: 1;int Valid_type_spec: 1;void*Type_spec;unsigned loc;};struct Cyc_Parse_Declarator{struct _tuple0*id;unsigned varloc;struct Cyc_List_List*tms;};struct _tuple12{struct Cyc_Parse_Declarator f0;struct Cyc_Absyn_Exp*f1;struct Cyc_Absyn_Exp*f2;};struct _tuple11{struct _tuple11*tl;struct _tuple12 hd  __attribute__((aligned )) ;};struct _tuple13{struct _tuple13*tl;struct Cyc_Parse_Declarator hd  __attribute__((aligned )) ;};
# 57 "parse.h"
enum Cyc_Parse_Storage_class{Cyc_Parse_Typedef_sc =0U,Cyc_Parse_Extern_sc =1U,Cyc_Parse_ExternC_sc =2U,Cyc_Parse_Static_sc =3U,Cyc_Parse_Auto_sc =4U,Cyc_Parse_Register_sc =5U,Cyc_Parse_Abstract_sc =6U,Cyc_Parse_None_sc =7U};
# 62
enum Cyc_Parse_ConstraintOps{Cyc_Parse_C_AND_OP =0U,Cyc_Parse_C_OR_OP =1U,Cyc_Parse_C_NOT_OP =2U,Cyc_Parse_C_EQ_OP =3U,Cyc_Parse_C_INCL_OP =4U};struct Cyc_Parse_Declaration_spec{enum Cyc_Parse_Storage_class sc;struct Cyc_Absyn_Tqual tq;struct Cyc_Parse_Type_specifier type_specs;int is_inline;struct Cyc_List_List*attributes;};struct Cyc_Parse_Abstractdeclarator{struct Cyc_List_List*tms;};
# 80 "parse.y"
void Cyc_Lex_register_typedef(struct _tuple0*);
void Cyc_Lex_enter_namespace(struct _fat_ptr*);
void Cyc_Lex_leave_namespace (void);
void Cyc_Lex_enter_using(struct _tuple0*);
void Cyc_Lex_leave_using (void);
void Cyc_Lex_enter_extern_c (void);
void Cyc_Lex_leave_extern_c (void);
extern struct _tuple0*Cyc_Lex_token_qvar;
extern struct _fat_ptr Cyc_Lex_token_string;
# 104 "parse.y"
int Cyc_Parse_parsing_tempest=0;
# 110
struct Cyc_Parse_FlatList*Cyc_Parse_flat_imp_rev(struct Cyc_Parse_FlatList*x){struct Cyc_Parse_FlatList*_T0;struct Cyc_Parse_FlatList*_T1;struct Cyc_Parse_FlatList*_T2;struct Cyc_Parse_FlatList*_T3;struct Cyc_Parse_FlatList*_T4;struct Cyc_Parse_FlatList*_T5;
if(x!=0)goto _TL0;_T0=x;return _T0;_TL0: {
struct Cyc_Parse_FlatList*first=x;_T1=x;{
struct Cyc_Parse_FlatList*second=_T1->tl;_T2=x;
_T2->tl=0;
_TL2: if(second!=0)goto _TL3;else{goto _TL4;}
_TL3: _T3=second;{struct Cyc_Parse_FlatList*temp=_T3->tl;_T4=second;
_T4->tl=first;
first=second;
second=temp;}goto _TL2;_TL4: _T5=first;
# 121
return _T5;}}}char Cyc_Parse_Exit[5U]="Exit";struct Cyc_Parse_Numelts_ptrqual_Parse_Pointer_qual_struct{int tag;struct Cyc_Absyn_Exp*f1;};struct Cyc_Parse_Region_ptrqual_Parse_Pointer_qual_struct{int tag;void*f1;};struct Cyc_Parse_Effect_ptrqual_Parse_Pointer_qual_struct{int tag;struct Cyc_List_List*f1;};struct Cyc_Parse_Thin_ptrqual_Parse_Pointer_qual_struct{int tag;};struct Cyc_Parse_Fat_ptrqual_Parse_Pointer_qual_struct{int tag;};struct Cyc_Parse_Zeroterm_ptrqual_Parse_Pointer_qual_struct{int tag;};struct Cyc_Parse_Nozeroterm_ptrqual_Parse_Pointer_qual_struct{int tag;};struct Cyc_Parse_Autoreleased_ptrqual_Parse_Pointer_qual_struct{int tag;};struct Cyc_Parse_Notnull_ptrqual_Parse_Pointer_qual_struct{int tag;};struct Cyc_Parse_Nullable_ptrqual_Parse_Pointer_qual_struct{int tag;};struct Cyc_Parse_Alias_ptrqual_Parse_Pointer_qual_struct{int tag;void*f1;};
# 177
static void*Cyc_Parse_collapse_type_specifiers(struct Cyc_Parse_Type_specifier,unsigned);struct _tuple14{struct Cyc_Absyn_Tqual f0;void*f1;struct Cyc_List_List*f2;struct Cyc_List_List*f3;};
static struct _tuple14 Cyc_Parse_apply_tms(struct Cyc_Absyn_Tqual,void*,struct Cyc_List_List*,struct Cyc_List_List*);
# 182
static struct Cyc_List_List*Cyc_Parse_parse_result=0;
static struct Cyc_List_List*Cyc_Parse_constraint_graph=0;
static int Cyc_Parse_inside_function_definition=0;
static int Cyc_Parse_inside_noinference_block=0;
static void*Cyc_Parse_parse_abort(unsigned loc,struct _fat_ptr msg){struct Cyc_Warn_String_Warn_Warg_struct _T0;unsigned _T1;struct _fat_ptr _T2;struct Cyc_Parse_Exit_exn_struct*_T3;void*_T4;{struct Cyc_Warn_String_Warn_Warg_struct _T5;_T5.tag=0;
_T5.f1=msg;_T0=_T5;}{struct Cyc_Warn_String_Warn_Warg_struct _T5=_T0;void*_T6[1];_T6[0]=& _T5;_T1=loc;_T2=_tag_fat(_T6,sizeof(void*),1);Cyc_Warn_err2(_T1,_T2);}{struct Cyc_Parse_Exit_exn_struct*_T5=_cycalloc(sizeof(struct Cyc_Parse_Exit_exn_struct));
_T5->tag=Cyc_Parse_Exit;_T3=(struct Cyc_Parse_Exit_exn_struct*)_T5;}_T4=(void*)_T3;_throw(_T4);}
# 191
static void*Cyc_Parse_type_name_to_type(struct _tuple8*tqt,unsigned loc){struct _tuple8*_T0;struct Cyc_Absyn_Tqual _T1;int _T2;struct Cyc_Absyn_Tqual _T3;int _T4;struct Cyc_Absyn_Tqual _T5;int _T6;struct Cyc_Absyn_Tqual _T7;unsigned _T8;struct Cyc_Absyn_Tqual _T9;unsigned _TA;struct _fat_ptr _TB;struct _fat_ptr _TC;void*_TD;void*_TE;struct Cyc_Absyn_Tqual _TF;_T0=tqt;{struct _tuple8 _T10=*_T0;_TF=_T10.f1;_TE=_T10.f2;}{struct Cyc_Absyn_Tqual tq=_TF;void*t=_TE;_T1=tq;_T2=_T1.print_const;
# 194
if(_T2)goto _TL7;else{goto _TL9;}_TL9: _T3=tq;_T4=_T3.q_volatile;if(_T4)goto _TL7;else{goto _TL8;}_TL8: _T5=tq;_T6=_T5.q_restrict;if(_T6)goto _TL7;else{goto _TL5;}
_TL7: _T7=tq;_T8=_T7.loc;if(_T8==0U)goto _TLA;_T9=tq;loc=_T9.loc;goto _TLB;_TLA: _TLB: _TA=loc;_TB=
_tag_fat("qualifier on type is ignored",sizeof(char),29U);_TC=_tag_fat(0U,sizeof(void*),0);Cyc_Warn_warn(_TA,_TB,_TC);goto _TL6;_TL5: _TL6: _TD=t;
# 198
return _TD;}}
# 201
static void*Cyc_Parse_make_pointer_mod(struct _RegionHandle*r,struct Cyc_Absyn_PtrLoc*loc,void*nullable,void*bound,void*eff,struct Cyc_List_List*pqs,struct Cyc_Absyn_Tqual tqs){struct Cyc_List_List*_T0;int*_T1;unsigned _T2;void*_T3;void*_T4;int(*_T5)(struct _fat_ptr,struct _fat_ptr);void*(*_T6)(struct _fat_ptr,struct _fat_ptr);struct _fat_ptr _T7;struct _fat_ptr _T8;struct Cyc_List_List*_T9;struct Cyc_Absyn_Pointer_mod_Absyn_Type_modifier_struct*_TA;struct _RegionHandle*_TB;struct Cyc_Core_Opt*_TC;struct Cyc_Core_Opt*_TD;void*_TE;void*_TF;void*_T10;
# 206
void*zeroterm=Cyc_Tcutil_any_bool(0);
void*autoreleased=Cyc_Tcutil_any_bool(0);
void*aqual=0;
_TLF: if(pqs!=0)goto _TLD;else{goto _TLE;}
_TLD: _T0=pqs;{void*_T11=_T0->hd;void*_T12;_T1=(int*)_T11;_T2=*_T1;switch(_T2){case 5:
 zeroterm=Cyc_Absyn_true_type;goto _LL0;case 6:
 zeroterm=Cyc_Absyn_false_type;goto _LL0;case 7:
 autoreleased=Cyc_Absyn_true_type;goto _LL0;case 9:
 nullable=Cyc_Absyn_true_type;goto _LL0;case 8:
 nullable=Cyc_Absyn_false_type;goto _LL0;case 4:
 bound=Cyc_Absyn_fat_bound_type;goto _LL0;case 3:
 bound=Cyc_Absyn_bounds_one();goto _LL0;case 0:{struct Cyc_Parse_Numelts_ptrqual_Parse_Pointer_qual_struct*_T13=(struct Cyc_Parse_Numelts_ptrqual_Parse_Pointer_qual_struct*)_T11;_T12=_T13->f1;}{struct Cyc_Absyn_Exp*e=_T12;
bound=Cyc_Absyn_thin_bounds_exp(e);goto _LL0;}case 10:{struct Cyc_Parse_Alias_ptrqual_Parse_Pointer_qual_struct*_T13=(struct Cyc_Parse_Alias_ptrqual_Parse_Pointer_qual_struct*)_T11;_T3=_T13->f1;_T12=(void*)_T3;}{void*aq=_T12;
aqual=aq;goto _LL0;}case 2:{struct Cyc_Parse_Effect_ptrqual_Parse_Pointer_qual_struct*_T13=(struct Cyc_Parse_Effect_ptrqual_Parse_Pointer_qual_struct*)_T11;_T12=_T13->f1;}{struct Cyc_List_List*ts=_T12;
eff=Cyc_Absyn_join_eff(ts);goto _LL0;}default:{struct Cyc_Parse_Region_ptrqual_Parse_Pointer_qual_struct*_T13=(struct Cyc_Parse_Region_ptrqual_Parse_Pointer_qual_struct*)_T11;_T4=_T13->f1;_T12=(void*)_T4;}{void*t=_T12;_T6=Cyc_Warn_impos;{
int(*_T13)(struct _fat_ptr,struct _fat_ptr)=(int(*)(struct _fat_ptr,struct _fat_ptr))_T6;_T5=_T13;}_T7=_tag_fat("Found region pointer qual",sizeof(char),26U);_T8=_tag_fat(0U,sizeof(void*),0);_T5(_T7,_T8);goto _LL0;}}_LL0:;}_T9=pqs;
# 209
pqs=_T9->tl;goto _TLF;_TLE: _TB=r;{struct Cyc_Absyn_Pointer_mod_Absyn_Type_modifier_struct*_T11=_region_malloc(_TB,0U,sizeof(struct Cyc_Absyn_Pointer_mod_Absyn_Type_modifier_struct));_T11->tag=2;
# 223
_T11->f1.eff=eff;_T11->f1.nullable=nullable;_T11->f1.bounds=bound;_T11->f1.zero_term=zeroterm;
_T11->f1.ptrloc=loc;_T11->f1.autoreleased=autoreleased;
if(aqual==0)goto _TL11;_T11->f1.aqual=aqual;goto _TL12;_TL11: _TC=& Cyc_Kinds_aqko;_TD=(struct Cyc_Core_Opt*)_TC;_TE=Cyc_Absyn_new_evar(_TD,0);_TF=Cyc_Absyn_rtd_qual_type;_T11->f1.aqual=Cyc_Absyn_aqual_var_type(_TE,_TF);_TL12:
 _T11->f2=tqs;_TA=(struct Cyc_Absyn_Pointer_mod_Absyn_Type_modifier_struct*)_T11;}_T10=(void*)_TA;
# 223
return _T10;}
# 230
static void Cyc_Parse_check_single_constraint(unsigned loc,struct _fat_ptr id){struct _fat_ptr _T0;struct _fat_ptr _T1;int _T2;struct Cyc_String_pa_PrintArg_struct _T3;unsigned _T4;struct _fat_ptr _T5;struct _fat_ptr _T6;_T0=id;_T1=
_tag_fat("single",sizeof(char),7U);_T2=Cyc_zstrcmp(_T0,_T1);if(!_T2)goto _TL13;{struct Cyc_String_pa_PrintArg_struct _T7;_T7.tag=0;
_T7.f1=_tag_fat(" is not a valid effect constraint",sizeof(char),34U);_T3=_T7;}{struct Cyc_String_pa_PrintArg_struct _T7=_T3;void*_T8[1];_T8[0]=& _T7;_T4=loc;_T5=id;_T6=_tag_fat(_T8,sizeof(void*),1);Cyc_Warn_err(_T4,_T5,_T6);}goto _TL14;_TL13: _TL14:;}
# 234
static void*Cyc_Parse_effect_from_atomic(struct Cyc_List_List*effs){int _T0;struct Cyc_List_List*_T1;void*_T2;void*_T3;_T0=
Cyc_List_length(effs);if(_T0!=1)goto _TL15;_T1=
_check_null(effs);_T2=_T1->hd;return _T2;
# 238
_TL15: _T3=Cyc_Absyn_join_eff(effs);return _T3;}
# 241
static struct _tuple0*Cyc_Parse_gensym_enum (void){struct _tuple0*_T0;struct _fat_ptr*_T1;struct _fat_ptr _T2;struct Cyc_Int_pa_PrintArg_struct _T3;int _T4;int _T5;struct _fat_ptr _T6;struct _fat_ptr _T7;
static int enum_counter=0;{struct _tuple0*_T8=_cycalloc(sizeof(struct _tuple0));
_T8->f0=Cyc_Absyn_Rel_n(0);{struct _fat_ptr*_T9=_cycalloc(sizeof(struct _fat_ptr));{struct Cyc_Int_pa_PrintArg_struct _TA;_TA.tag=1;_T4=enum_counter;
enum_counter=_T4 + 1;_T5=_T4;_TA.f1=(unsigned long)_T5;_T3=_TA;}{struct Cyc_Int_pa_PrintArg_struct _TA=_T3;void*_TB[1];_TB[0]=& _TA;_T6=_tag_fat("__anonymous_enum_%d__",sizeof(char),22U);_T7=_tag_fat(_TB,sizeof(void*),1);_T2=Cyc_aprintf(_T6,_T7);}*_T9=_T2;_T1=(struct _fat_ptr*)_T9;}_T8->f1=_T1;_T0=(struct _tuple0*)_T8;}
# 243
return _T0;}struct _tuple15{unsigned f0;struct _tuple0*f1;struct Cyc_Absyn_Tqual f2;void*f3;struct Cyc_List_List*f4;struct Cyc_List_List*f5;};struct _tuple16{struct Cyc_Absyn_Exp*f0;struct Cyc_Absyn_Exp*f1;};struct _tuple17{struct _tuple15*f0;struct _tuple16*f1;};
# 247
static struct Cyc_Absyn_Aggrfield*Cyc_Parse_make_aggr_field(unsigned loc,struct _tuple17*field_info){struct _tuple17*_T0;struct _tuple15*_T1;struct _tuple16*_T2;unsigned _T3;struct _fat_ptr _T4;struct _fat_ptr _T5;int _T6;unsigned _T7;struct _fat_ptr _T8;struct _fat_ptr _T9;struct Cyc_Absyn_Aggrfield*_TA;struct _tuple0*_TB;struct _tuple0 _TC;struct Cyc_Absyn_Exp*_TD;struct Cyc_Absyn_Exp*_TE;struct Cyc_List_List*_TF;struct Cyc_List_List*_T10;void*_T11;struct Cyc_Absyn_Tqual _T12;struct _tuple0*_T13;unsigned _T14;_T0=field_info;{struct _tuple17 _T15=*_T0;_T1=_T15.f0;{struct _tuple15 _T16=*_T1;_T14=_T16.f0;_T13=_T16.f1;_T12=_T16.f2;_T11=_T16.f3;_T10=_T16.f4;_TF=_T16.f5;}_T2=_T15.f1;{struct _tuple16 _T16=*_T2;_TE=_T16.f0;_TD=_T16.f1;}}{unsigned varloc=_T14;struct _tuple0*qid=_T13;struct Cyc_Absyn_Tqual tq=_T12;void*t=_T11;struct Cyc_List_List*tvs=_T10;struct Cyc_List_List*atts=_TF;struct Cyc_Absyn_Exp*widthopt=_TE;struct Cyc_Absyn_Exp*reqopt=_TD;
# 253
if(tvs==0)goto _TL17;_T3=loc;_T4=
_tag_fat("bad type params in struct field",sizeof(char),32U);_T5=_tag_fat(0U,sizeof(void*),0);Cyc_Warn_err(_T3,_T4,_T5);goto _TL18;_TL17: _TL18: _T6=
Cyc_Absyn_is_qvar_qualified(qid);if(!_T6)goto _TL19;_T7=loc;_T8=
_tag_fat("struct or union field cannot be qualified with a namespace",sizeof(char),59U);_T9=_tag_fat(0U,sizeof(void*),0);Cyc_Warn_err(_T7,_T8,_T9);goto _TL1A;_TL19: _TL1A:{struct Cyc_Absyn_Aggrfield*_T15=_cycalloc(sizeof(struct Cyc_Absyn_Aggrfield));_TB=qid;_TC=*_TB;
_T15->name=_TC.f1;_T15->tq=tq;_T15->type=t;
_T15->width=widthopt;_T15->attributes=atts;
_T15->requires_clause=reqopt;_TA=(struct Cyc_Absyn_Aggrfield*)_T15;}
# 257
return _TA;}}
# 262
static struct Cyc_Parse_Type_specifier Cyc_Parse_empty_spec(unsigned loc){struct Cyc_Parse_Type_specifier _T0;{struct Cyc_Parse_Type_specifier _T1;
_T1.Signed_spec=0;
_T1.Unsigned_spec=0;
_T1.Short_spec=0;
_T1.Long_spec=0;
_T1.Long_Long_spec=0;
_T1.Complex_spec=0;
_T1.Valid_type_spec=0;
_T1.Type_spec=Cyc_Absyn_sint_type;
_T1.loc=loc;_T0=_T1;}
# 263
return _T0;}
# 274
static struct Cyc_Parse_Type_specifier Cyc_Parse_type_spec(void*t,unsigned loc){struct Cyc_Parse_Type_specifier _T0;
struct Cyc_Parse_Type_specifier s=Cyc_Parse_empty_spec(loc);
s.Type_spec=t;
s.Valid_type_spec=1;_T0=s;
return _T0;}
# 280
static struct Cyc_Parse_Type_specifier Cyc_Parse_signed_spec(unsigned loc){struct Cyc_Parse_Type_specifier _T0;
struct Cyc_Parse_Type_specifier s=Cyc_Parse_empty_spec(loc);
s.Signed_spec=1;_T0=s;
return _T0;}
# 285
static struct Cyc_Parse_Type_specifier Cyc_Parse_unsigned_spec(unsigned loc){struct Cyc_Parse_Type_specifier _T0;
struct Cyc_Parse_Type_specifier s=Cyc_Parse_empty_spec(loc);
s.Unsigned_spec=1;_T0=s;
return _T0;}
# 290
static struct Cyc_Parse_Type_specifier Cyc_Parse_short_spec(unsigned loc){struct Cyc_Parse_Type_specifier _T0;
struct Cyc_Parse_Type_specifier s=Cyc_Parse_empty_spec(loc);
s.Short_spec=1;_T0=s;
return _T0;}
# 295
static struct Cyc_Parse_Type_specifier Cyc_Parse_long_spec(unsigned loc){struct Cyc_Parse_Type_specifier _T0;
struct Cyc_Parse_Type_specifier s=Cyc_Parse_empty_spec(loc);
s.Long_spec=1;_T0=s;
return _T0;}
# 300
static struct Cyc_Parse_Type_specifier Cyc_Parse_complex_spec(unsigned loc){struct Cyc_Parse_Type_specifier _T0;
struct Cyc_Parse_Type_specifier s=Cyc_Parse_empty_spec(loc);
s.Complex_spec=1;_T0=s;
return _T0;}
# 307
static void*Cyc_Parse_array2ptr(void*t,int argposn){void*_T0;int _T1;void*_T2;void*_T3;int _T4;struct Cyc_Core_Opt*_T5;struct Cyc_Core_Opt*_T6;void*_T7;_T1=
# 309
Cyc_Tcutil_is_array_type(t);if(!_T1)goto _TL1B;_T2=t;_T4=argposn;
if(!_T4)goto _TL1D;_T5=& Cyc_Kinds_eko;_T6=(struct Cyc_Core_Opt*)_T5;_T3=Cyc_Absyn_new_evar(_T6,0);goto _TL1E;_TL1D: _T3=Cyc_Absyn_heap_rgn_type;_TL1E: _T7=Cyc_Absyn_al_qual_type;_T0=Cyc_Tcutil_promote_array(_T2,_T3,_T7,0);goto _TL1C;_TL1B: _T0=t;_TL1C:
# 309
 return _T0;}struct _tuple18{struct _fat_ptr*f0;void*f1;};
# 322 "parse.y"
static struct Cyc_List_List*Cyc_Parse_get_arg_tags(struct Cyc_List_List*x){struct Cyc_List_List*_T0;void*_T1;struct _tuple8*_T2;void*_T3;int*_T4;int _T5;struct _tuple8*_T6;void*_T7;struct Cyc_Absyn_AppType_Absyn_Type_struct*_T8;void*_T9;int*_TA;int _TB;struct _tuple8*_TC;void*_TD;struct Cyc_Absyn_AppType_Absyn_Type_struct*_TE;struct Cyc_List_List*_TF;struct _tuple8*_T10;void*_T11;struct Cyc_Absyn_AppType_Absyn_Type_struct*_T12;struct Cyc_List_List*_T13;struct Cyc_List_List*_T14;struct Cyc_List_List*_T15;void*_T16;struct Cyc_List_List*_T17;void*_T18;struct _fat_ptr*_T19;void*_T1A;int*_T1B;int _T1C;void*_T1D;void*_T1E;struct Cyc_Absyn_Evar_Absyn_Type_struct*_T1F;void**_T20;struct _fat_ptr*_T21;struct _fat_ptr _T22;struct Cyc_String_pa_PrintArg_struct _T23;struct _fat_ptr*_T24;struct _fat_ptr _T25;struct _fat_ptr _T26;void**_T27;struct Cyc_Absyn_Tvar*_T28;struct Cyc_Absyn_Eq_kb_Absyn_KindBound_struct*_T29;struct Cyc_Absyn_Kind*_T2A;struct Cyc_List_List*_T2B;struct _tuple18*_T2C;struct _tuple8*_T2D;struct _fat_ptr*_T2E;struct _tuple8*_T2F;struct _fat_ptr*_T30;struct _tuple8*_T31;struct _fat_ptr*_T32;struct _tuple8*_T33;struct _fat_ptr*_T34;struct _tuple8*_T35;void*_T36;struct Cyc_Absyn_AppType_Absyn_Type_struct*_T37;void*_T38;int*_T39;int _T3A;struct _tuple8*_T3B;void*_T3C;struct Cyc_Absyn_AppType_Absyn_Type_struct*_T3D;struct Cyc_List_List*_T3E;struct _tuple8*_T3F;void*_T40;struct Cyc_Absyn_AppType_Absyn_Type_struct*_T41;struct Cyc_List_List*_T42;struct Cyc_List_List*_T43;void*_T44;int*_T45;int _T46;struct _tuple8*_T47;void*_T48;struct Cyc_Absyn_AppType_Absyn_Type_struct*_T49;struct Cyc_List_List*_T4A;struct Cyc_List_List*_T4B;struct Cyc_List_List*_T4C;struct _fat_ptr*_T4D;void*_T4E;struct Cyc_List_List*_T4F;void*_T50;void*_T51;struct Cyc_Absyn_AppType_Absyn_Type_struct*_T52;struct Cyc_List_List*_T53;void*_T54;struct Cyc_Absyn_Evar_Absyn_Type_struct*_T55;void**_T56;struct _fat_ptr*_T57;struct _fat_ptr _T58;struct Cyc_String_pa_PrintArg_struct _T59;struct _fat_ptr _T5A;struct _fat_ptr _T5B;void**_T5C;struct Cyc_Absyn_Tvar*_T5D;struct Cyc_Absyn_Eq_kb_Absyn_KindBound_struct*_T5E;struct Cyc_Absyn_Kind*_T5F;struct _tuple8*_T60;struct _fat_ptr*_T61;struct Cyc_List_List*_T62;struct Cyc_List_List*_T63;
struct Cyc_List_List*res=0;
_TL22: if(x!=0)goto _TL20;else{goto _TL21;}
_TL20: _T0=x;_T1=_T0->hd;{struct _tuple8*_T64=(struct _tuple8*)_T1;struct _fat_ptr _T65;void*_T66;void*_T67;_T2=(struct _tuple8*)_T64;_T3=_T2->f2;_T4=(int*)_T3;_T5=*_T4;if(_T5!=0)goto _TL23;_T6=(struct _tuple8*)_T64;_T7=_T6->f2;_T8=(struct Cyc_Absyn_AppType_Absyn_Type_struct*)_T7;_T9=_T8->f1;_TA=(int*)_T9;_TB=*_TA;if(_TB!=5)goto _TL25;_TC=(struct _tuple8*)_T64;_TD=_TC->f2;_TE=(struct Cyc_Absyn_AppType_Absyn_Type_struct*)_TD;_TF=_TE->f2;if(_TF==0)goto _TL27;_T10=(struct _tuple8*)_T64;_T11=_T10->f2;_T12=(struct Cyc_Absyn_AppType_Absyn_Type_struct*)_T11;_T13=_T12->f2;_T14=(struct Cyc_List_List*)_T13;_T15=_T14->tl;if(_T15!=0)goto _TL29;{struct _tuple8 _T68=*_T64;_T67=_T68.f0;_T16=_T68.f2;{struct Cyc_Absyn_AppType_Absyn_Type_struct*_T69=(struct Cyc_Absyn_AppType_Absyn_Type_struct*)_T16;_T17=_T69->f2;{struct Cyc_List_List _T6A=*_T17;_T18=_T6A.hd;_T66=(void*)_T18;}}}_T19=(struct _fat_ptr*)_T67;if(_T19==0)goto _TL2B;{struct _fat_ptr*v=_T67;void*i=_T66;{void*_T68;_T1A=i;_T1B=(int*)_T1A;_T1C=*_T1B;if(_T1C!=1)goto _TL2D;_T1D=i;{struct Cyc_Absyn_Evar_Absyn_Type_struct*_T69=(struct Cyc_Absyn_Evar_Absyn_Type_struct*)_T1D;_T1E=i;_T1F=(struct Cyc_Absyn_Evar_Absyn_Type_struct*)_T1E;_T20=& _T1F->f2;_T68=(void**)_T20;}{void**z=(void**)_T68;
# 331
struct _fat_ptr*nm;nm=_cycalloc(sizeof(struct _fat_ptr));_T21=nm;{struct Cyc_String_pa_PrintArg_struct _T69;_T69.tag=0;_T24=v;_T69.f1=*_T24;_T23=_T69;}{struct Cyc_String_pa_PrintArg_struct _T69=_T23;void*_T6A[1];_T6A[0]=& _T69;_T25=_tag_fat("`%s",sizeof(char),4U);_T26=_tag_fat(_T6A,sizeof(void*),1);_T22=Cyc_aprintf(_T25,_T26);}*_T21=_T22;_T27=z;{struct Cyc_Absyn_Tvar*_T69=_cycalloc(sizeof(struct Cyc_Absyn_Tvar));
_T69->name=nm;_T69->identity=- 1;{struct Cyc_Absyn_Eq_kb_Absyn_KindBound_struct*_T6A=_cycalloc(sizeof(struct Cyc_Absyn_Eq_kb_Absyn_KindBound_struct));_T6A->tag=0;_T2A=& Cyc_Kinds_ik;_T6A->f1=(struct Cyc_Absyn_Kind*)_T2A;_T29=(struct Cyc_Absyn_Eq_kb_Absyn_KindBound_struct*)_T6A;}_T69->kind=(void*)_T29;_T69->aquals_bound=0;_T28=(struct Cyc_Absyn_Tvar*)_T69;}*_T27=Cyc_Absyn_var_type(_T28);goto _LL7;}_TL2D: goto _LL7;_LL7:;}{struct Cyc_List_List*_T68=_cycalloc(sizeof(struct Cyc_List_List));{struct _tuple18*_T69=_cycalloc(sizeof(struct _tuple18));
# 336
_T69->f0=v;_T69->f1=i;_T2C=(struct _tuple18*)_T69;}_T68->hd=_T2C;_T68->tl=res;_T2B=(struct Cyc_List_List*)_T68;}res=_T2B;goto _LL0;}_TL2B: _T2D=(struct _tuple8*)_T64;_T2E=_T2D->f0;if(_T2E==0)goto _TL2F;goto _LL5;_TL2F: goto _LL5;_TL29: _T2F=(struct _tuple8*)_T64;_T30=_T2F->f0;if(_T30==0)goto _TL31;goto _LL5;_TL31: goto _LL5;_TL27: _T31=(struct _tuple8*)_T64;_T32=_T31->f0;if(_T32==0)goto _TL33;goto _LL5;_TL33: goto _LL5;_TL25: _T33=(struct _tuple8*)_T64;_T34=_T33->f0;if(_T34==0)goto _TL35;_T35=(struct _tuple8*)_T64;_T36=_T35->f2;_T37=(struct Cyc_Absyn_AppType_Absyn_Type_struct*)_T36;_T38=_T37->f1;_T39=(int*)_T38;_T3A=*_T39;if(_T3A!=4)goto _TL37;_T3B=(struct _tuple8*)_T64;_T3C=_T3B->f2;_T3D=(struct Cyc_Absyn_AppType_Absyn_Type_struct*)_T3C;_T3E=_T3D->f2;if(_T3E==0)goto _TL39;_T3F=(struct _tuple8*)_T64;_T40=_T3F->f2;_T41=(struct Cyc_Absyn_AppType_Absyn_Type_struct*)_T40;_T42=_T41->f2;_T43=(struct Cyc_List_List*)_T42;_T44=_T43->hd;_T45=(int*)_T44;_T46=*_T45;if(_T46!=1)goto _TL3B;_T47=(struct _tuple8*)_T64;_T48=_T47->f2;_T49=(struct Cyc_Absyn_AppType_Absyn_Type_struct*)_T48;_T4A=_T49->f2;_T4B=(struct Cyc_List_List*)_T4A;_T4C=_T4B->tl;if(_T4C!=0)goto _TL3D;{struct _tuple8 _T68=*_T64;_T4D=_T68.f0;{struct _fat_ptr _T69=*_T4D;_T65=_T69;}_T4E=_T68.f2;{struct Cyc_Absyn_AppType_Absyn_Type_struct*_T69=(struct Cyc_Absyn_AppType_Absyn_Type_struct*)_T4E;_T4F=_T69->f2;{struct Cyc_List_List _T6A=*_T4F;_T50=_T6A.hd;{struct Cyc_Absyn_Evar_Absyn_Type_struct*_T6B=(struct Cyc_Absyn_Evar_Absyn_Type_struct*)_T50;_T51=_T64->f2;_T52=(struct Cyc_Absyn_AppType_Absyn_Type_struct*)_T51;_T53=_T52->f2;_T54=_T53->hd;_T55=(struct Cyc_Absyn_Evar_Absyn_Type_struct*)_T54;_T56=& _T55->f2;_T67=(void**)_T56;}}}}{struct _fat_ptr v=_T65;void**z=(void**)_T67;
# 340
struct _fat_ptr*nm;nm=_cycalloc(sizeof(struct _fat_ptr));_T57=nm;{struct Cyc_String_pa_PrintArg_struct _T68;_T68.tag=0;_T68.f1=v;_T59=_T68;}{struct Cyc_String_pa_PrintArg_struct _T68=_T59;void*_T69[1];_T69[0]=& _T68;_T5A=_tag_fat("`%s",sizeof(char),4U);_T5B=_tag_fat(_T69,sizeof(void*),1);_T58=Cyc_aprintf(_T5A,_T5B);}*_T57=_T58;_T5C=z;{struct Cyc_Absyn_Tvar*_T68=_cycalloc(sizeof(struct Cyc_Absyn_Tvar));
_T68->name=nm;_T68->identity=- 1;{struct Cyc_Absyn_Eq_kb_Absyn_KindBound_struct*_T69=_cycalloc(sizeof(struct Cyc_Absyn_Eq_kb_Absyn_KindBound_struct));_T69->tag=0;_T5F=& Cyc_Kinds_ek;_T69->f1=(struct Cyc_Absyn_Kind*)_T5F;_T5E=(struct Cyc_Absyn_Eq_kb_Absyn_KindBound_struct*)_T69;}_T68->kind=(void*)_T5E;_T68->aquals_bound=0;_T5D=(struct Cyc_Absyn_Tvar*)_T68;}*_T5C=Cyc_Absyn_var_type(_T5D);goto _LL0;}_TL3D: goto _LL5;_TL3B: goto _LL5;_TL39: goto _LL5;_TL37: goto _LL5;_TL35: goto _LL5;_TL23: _T60=(struct _tuple8*)_T64;_T61=_T60->f0;if(_T61==0)goto _TL3F;goto _LL5;_TL3F: _LL5: goto _LL0;_LL0:;}_T62=x;
# 324
x=_T62->tl;goto _TL22;_TL21: _T63=res;
# 345
return _T63;}
# 349
static struct Cyc_List_List*Cyc_Parse_get_aggrfield_tags(struct Cyc_List_List*x){struct Cyc_List_List*_T0;void*_T1;struct Cyc_Absyn_Aggrfield*_T2;int*_T3;int _T4;struct Cyc_Absyn_AppType_Absyn_Type_struct*_T5;void*_T6;int*_T7;int _T8;struct Cyc_Absyn_AppType_Absyn_Type_struct*_T9;struct Cyc_List_List*_TA;struct Cyc_Absyn_AppType_Absyn_Type_struct*_TB;struct Cyc_List_List*_TC;struct Cyc_List_List*_TD;struct Cyc_List_List*_TE;struct Cyc_List_List*_TF;void*_T10;struct Cyc_List_List*_T11;struct _tuple18*_T12;struct Cyc_List_List*_T13;void*_T14;struct Cyc_Absyn_Aggrfield*_T15;struct Cyc_List_List*_T16;struct Cyc_List_List*_T17;
struct Cyc_List_List*res=0;
_TL44: if(x!=0)goto _TL42;else{goto _TL43;}
_TL42: _T0=x;_T1=_T0->hd;_T2=(struct Cyc_Absyn_Aggrfield*)_T1;{void*_T18=_T2->type;void*_T19;_T3=(int*)_T18;_T4=*_T3;if(_T4!=0)goto _TL45;_T5=(struct Cyc_Absyn_AppType_Absyn_Type_struct*)_T18;_T6=_T5->f1;_T7=(int*)_T6;_T8=*_T7;if(_T8!=5)goto _TL47;_T9=(struct Cyc_Absyn_AppType_Absyn_Type_struct*)_T18;_TA=_T9->f2;if(_TA==0)goto _TL49;_TB=(struct Cyc_Absyn_AppType_Absyn_Type_struct*)_T18;_TC=_TB->f2;_TD=(struct Cyc_List_List*)_TC;_TE=_TD->tl;if(_TE!=0)goto _TL4B;{struct Cyc_Absyn_AppType_Absyn_Type_struct*_T1A=(struct Cyc_Absyn_AppType_Absyn_Type_struct*)_T18;_TF=_T1A->f2;{struct Cyc_List_List _T1B=*_TF;_T10=_T1B.hd;_T19=(void*)_T10;}}{void*i=_T19;{struct Cyc_List_List*_T1A=_cycalloc(sizeof(struct Cyc_List_List));{struct _tuple18*_T1B=_cycalloc(sizeof(struct _tuple18));_T13=x;_T14=_T13->hd;_T15=(struct Cyc_Absyn_Aggrfield*)_T14;
# 354
_T1B->f0=_T15->name;_T1B->f1=i;_T12=(struct _tuple18*)_T1B;}_T1A->hd=_T12;_T1A->tl=res;_T11=(struct Cyc_List_List*)_T1A;}res=_T11;goto _LL0;}_TL4B: goto _LL3;_TL49: goto _LL3;_TL47: goto _LL3;_TL45: _LL3: goto _LL0;_LL0:;}_T16=x;
# 351
x=_T16->tl;goto _TL44;_TL43: _T17=res;
# 357
return _T17;}
# 361
static struct Cyc_Absyn_Exp*Cyc_Parse_substitute_tags_exp(struct Cyc_List_List*tags,struct Cyc_Absyn_Exp*e){struct Cyc_Absyn_Exp*_T0;int*_T1;int _T2;struct Cyc_Absyn_Var_e_Absyn_Raw_exp_struct*_T3;void*_T4;int*_T5;int _T6;struct Cyc_Absyn_Var_e_Absyn_Raw_exp_struct*_T7;void*_T8;struct Cyc_Absyn_Unresolved_b_Absyn_Binding_struct*_T9;struct _tuple0*_TA;struct _tuple0*_TB;union Cyc_Absyn_Nmspace _TC;struct _union_Nmspace_Rel_n _TD;unsigned _TE;struct Cyc_Absyn_Var_e_Absyn_Raw_exp_struct*_TF;void*_T10;struct Cyc_Absyn_Unresolved_b_Absyn_Binding_struct*_T11;struct _tuple0*_T12;struct _tuple0*_T13;union Cyc_Absyn_Nmspace _T14;struct _union_Nmspace_Rel_n _T15;struct Cyc_List_List*_T16;void*_T17;struct _tuple0*_T18;struct Cyc_List_List*_T19;void*_T1A;int _T1B;struct Cyc_Absyn_Valueof_e_Absyn_Raw_exp_struct*_T1C;void*_T1D;struct Cyc_Absyn_Exp*_T1E;unsigned _T1F;struct Cyc_Absyn_Exp*_T20;struct Cyc_List_List*_T21;struct Cyc_Absyn_Exp*_T22;_T0=e;{
void*_T23=_T0->r;struct _fat_ptr*_T24;_T1=(int*)_T23;_T2=*_T1;if(_T2!=1)goto _TL4D;_T3=(struct Cyc_Absyn_Var_e_Absyn_Raw_exp_struct*)_T23;_T4=_T3->f1;_T5=(int*)_T4;_T6=*_T5;if(_T6!=0)goto _TL4F;_T7=(struct Cyc_Absyn_Var_e_Absyn_Raw_exp_struct*)_T23;_T8=_T7->f1;_T9=(struct Cyc_Absyn_Unresolved_b_Absyn_Binding_struct*)_T8;_TA=_T9->f1;_TB=(struct _tuple0*)_TA;_TC=_TB->f0;_TD=_TC.Rel_n;_TE=_TD.tag;if(_TE!=2)goto _TL51;_TF=(struct Cyc_Absyn_Var_e_Absyn_Raw_exp_struct*)_T23;_T10=_TF->f1;_T11=(struct Cyc_Absyn_Unresolved_b_Absyn_Binding_struct*)_T10;_T12=_T11->f1;_T13=(struct _tuple0*)_T12;_T14=_T13->f0;_T15=_T14.Rel_n;_T16=_T15.val;if(_T16!=0)goto _TL53;{struct Cyc_Absyn_Var_e_Absyn_Raw_exp_struct*_T25=(struct Cyc_Absyn_Var_e_Absyn_Raw_exp_struct*)_T23;_T17=_T25->f1;{struct Cyc_Absyn_Unresolved_b_Absyn_Binding_struct*_T26=(struct Cyc_Absyn_Unresolved_b_Absyn_Binding_struct*)_T17;_T18=_T26->f1;{struct _tuple0 _T27=*_T18;_T24=_T27.f1;}}}{struct _fat_ptr*y=_T24;{
# 364
struct Cyc_List_List*ts=tags;_TL58: if(ts!=0)goto _TL56;else{goto _TL57;}
_TL56: _T19=ts;_T1A=_T19->hd;{struct _tuple18*_T25=(struct _tuple18*)_T1A;void*_T26;struct _fat_ptr*_T27;{struct _tuple18 _T28=*_T25;_T27=_T28.f0;_T26=_T28.f1;}{struct _fat_ptr*x=_T27;void*i=_T26;_T1B=
Cyc_strptrcmp(x,y);if(_T1B!=0)goto _TL59;{struct Cyc_Absyn_Valueof_e_Absyn_Raw_exp_struct*_T28=_cycalloc(sizeof(struct Cyc_Absyn_Valueof_e_Absyn_Raw_exp_struct));_T28->tag=38;
_T28->f1=Cyc_Tcutil_copy_type(i);_T1C=(struct Cyc_Absyn_Valueof_e_Absyn_Raw_exp_struct*)_T28;}_T1D=(void*)_T1C;_T1E=e;_T1F=_T1E->loc;_T20=Cyc_Absyn_new_exp(_T1D,_T1F);return _T20;_TL59:;}}_T21=ts;
# 364
ts=_T21->tl;goto _TL58;_TL57:;}goto _LL0;}_TL53: goto _LL3;_TL51: goto _LL3;_TL4F: goto _LL3;_TL4D: _LL3: goto _LL0;_LL0:;}_T22=e;
# 372
return _T22;}
# 377
static void*Cyc_Parse_substitute_tags(struct Cyc_List_List*tags,void*t){void*_T0;int*_T1;unsigned _T2;void*_T3;struct Cyc_Absyn_ArrayInfo _T4;struct Cyc_Absyn_ArrayInfo _T5;struct Cyc_Absyn_ArrayInfo _T6;struct Cyc_Absyn_ArrayInfo _T7;struct Cyc_Absyn_ArrayInfo _T8;void*_T9;void*_TA;struct Cyc_Absyn_PtrInfo _TB;struct Cyc_Absyn_PtrInfo _TC;struct Cyc_Absyn_PtrInfo _TD;struct Cyc_Absyn_PtrAtts _TE;struct Cyc_Absyn_PtrInfo _TF;struct Cyc_Absyn_PtrAtts _T10;struct Cyc_Absyn_PtrInfo _T11;struct Cyc_Absyn_PtrAtts _T12;struct Cyc_Absyn_PtrInfo _T13;struct Cyc_Absyn_PtrAtts _T14;struct Cyc_Absyn_PtrInfo _T15;struct Cyc_Absyn_PtrAtts _T16;struct Cyc_Absyn_PtrInfo _T17;struct Cyc_Absyn_PtrAtts _T18;struct Cyc_Absyn_PtrInfo _T19;struct Cyc_Absyn_PtrAtts _T1A;struct Cyc_Absyn_PtrInfo _T1B;void*_T1C;void*_T1D;struct Cyc_Absyn_AppType_Absyn_Type_struct*_T1E;void*_T1F;int*_T20;int _T21;void*_T22;struct Cyc_Absyn_AppType_Absyn_Type_struct*_T23;struct Cyc_List_List*_T24;void*_T25;struct Cyc_Absyn_AppType_Absyn_Type_struct*_T26;struct Cyc_List_List*_T27;struct Cyc_List_List*_T28;struct Cyc_List_List*_T29;void*_T2A;struct Cyc_List_List*_T2B;void*_T2C;void*_T2D;void*_T2E;void*_T2F;void*_T30;{struct Cyc_Absyn_Exp*_T31;void*_T32;void*_T33;struct Cyc_Absyn_PtrLoc*_T34;void*_T35;void*_T36;unsigned _T37;void*_T38;void*_T39;struct Cyc_Absyn_Tqual _T3A;void*_T3B;_T0=t;_T1=(int*)_T0;_T2=*_T1;switch(_T2){case 5: _T3=t;{struct Cyc_Absyn_ArrayType_Absyn_Type_struct*_T3C=(struct Cyc_Absyn_ArrayType_Absyn_Type_struct*)_T3;_T4=_T3C->f1;_T3B=_T4.elt_type;_T5=_T3C->f1;_T3A=_T5.tq;_T6=_T3C->f1;_T39=_T6.num_elts;_T7=_T3C->f1;_T38=_T7.zero_term;_T8=_T3C->f1;_T37=_T8.zt_loc;}{void*et=_T3B;struct Cyc_Absyn_Tqual tq=_T3A;struct Cyc_Absyn_Exp*nelts=_T39;void*zt=_T38;unsigned ztloc=_T37;
# 380
struct Cyc_Absyn_Exp*nelts2=nelts;
if(nelts==0)goto _TL5C;
nelts2=Cyc_Parse_substitute_tags_exp(tags,nelts);goto _TL5D;_TL5C: _TL5D: {
void*et2=Cyc_Parse_substitute_tags(tags,et);
if(nelts!=nelts2)goto _TL60;else{goto _TL61;}_TL61: if(et!=et2)goto _TL60;else{goto _TL5E;}
_TL60: _T9=Cyc_Absyn_array_type(et2,tq,nelts2,zt,ztloc);return _T9;_TL5E: goto _LL0;}}case 4: _TA=t;{struct Cyc_Absyn_PointerType_Absyn_Type_struct*_T3C=(struct Cyc_Absyn_PointerType_Absyn_Type_struct*)_TA;_TB=_T3C->f1;_T3B=_TB.elt_type;_TC=_T3C->f1;_T3A=_TC.elt_tq;_TD=_T3C->f1;_TE=_TD.ptr_atts;_T39=_TE.eff;_TF=_T3C->f1;_T10=_TF.ptr_atts;_T38=_T10.nullable;_T11=_T3C->f1;_T12=_T11.ptr_atts;_T36=_T12.bounds;_T13=_T3C->f1;_T14=_T13.ptr_atts;_T35=_T14.zero_term;_T15=_T3C->f1;_T16=_T15.ptr_atts;_T34=_T16.ptrloc;_T17=_T3C->f1;_T18=_T17.ptr_atts;_T33=_T18.autoreleased;_T19=_T3C->f1;_T1A=_T19.ptr_atts;_T32=_T1A.aqual;}{void*et=_T3B;struct Cyc_Absyn_Tqual tq=_T3A;void*r=_T39;void*n=_T38;void*b=_T36;void*zt=_T35;struct Cyc_Absyn_PtrLoc*pl=_T34;void*rel=_T33;void*aq=_T32;
# 388
void*et2=Cyc_Parse_substitute_tags(tags,et);
void*b2=Cyc_Parse_substitute_tags(tags,b);
if(et2!=et)goto _TL64;else{goto _TL65;}_TL65: if(b2!=b)goto _TL64;else{goto _TL62;}
_TL64:{struct Cyc_Absyn_PtrInfo _T3C;_T3C.elt_type=et2;_T3C.elt_tq=tq;_T3C.ptr_atts.eff=r;_T3C.ptr_atts.nullable=n;_T3C.ptr_atts.bounds=b2;_T3C.ptr_atts.zero_term=zt;_T3C.ptr_atts.ptrloc=pl;_T3C.ptr_atts.autoreleased=rel;_T3C.ptr_atts.aqual=aq;_T1B=_T3C;}_T1C=Cyc_Absyn_pointer_type(_T1B);return _T1C;_TL62: goto _LL0;}case 0: _T1D=t;_T1E=(struct Cyc_Absyn_AppType_Absyn_Type_struct*)_T1D;_T1F=_T1E->f1;_T20=(int*)_T1F;_T21=*_T20;if(_T21!=13)goto _TL66;_T22=t;_T23=(struct Cyc_Absyn_AppType_Absyn_Type_struct*)_T22;_T24=_T23->f2;if(_T24==0)goto _TL68;_T25=t;_T26=(struct Cyc_Absyn_AppType_Absyn_Type_struct*)_T25;_T27=_T26->f2;_T28=(struct Cyc_List_List*)_T27;_T29=_T28->tl;if(_T29!=0)goto _TL6A;_T2A=t;{struct Cyc_Absyn_AppType_Absyn_Type_struct*_T3C=(struct Cyc_Absyn_AppType_Absyn_Type_struct*)_T2A;_T2B=_T3C->f2;{struct Cyc_List_List _T3D=*_T2B;_T2C=_T3D.hd;_T3B=(void*)_T2C;}}{void*t=_T3B;
# 394
void*t2=Cyc_Parse_substitute_tags(tags,t);
if(t==t2)goto _TL6C;_T2D=Cyc_Absyn_thin_bounds_type(t2);return _T2D;_TL6C: goto _LL0;}_TL6A: goto _LL9;_TL68: goto _LL9;_TL66: goto _LL9;case 9: _T2E=t;{struct Cyc_Absyn_ValueofType_Absyn_Type_struct*_T3C=(struct Cyc_Absyn_ValueofType_Absyn_Type_struct*)_T2E;_T31=_T3C->f1;}{struct Cyc_Absyn_Exp*e=_T31;
# 398
struct Cyc_Absyn_Exp*e2=Cyc_Parse_substitute_tags_exp(tags,e);
if(e2==e)goto _TL6E;_T2F=Cyc_Absyn_valueof_type(e2);return _T2F;_TL6E: goto _LL0;}default: _LL9: goto _LL0;}_LL0:;}_T30=t;
# 403
return _T30;}
# 408
static void Cyc_Parse_substitute_aggrfield_tags(struct Cyc_List_List*tags,struct Cyc_Absyn_Aggrfield*x){struct Cyc_Absyn_Aggrfield*_T0;struct Cyc_List_List*_T1;struct Cyc_Absyn_Aggrfield*_T2;void*_T3;_T0=x;_T1=tags;_T2=x;_T3=_T2->type;
_T0->type=Cyc_Parse_substitute_tags(_T1,_T3);}struct _tuple19{struct Cyc_Absyn_Tqual f0;void*f1;};struct _tuple20{void*f0;struct Cyc_Absyn_Tqual f1;void*f2;};
# 415
static struct _tuple19*Cyc_Parse_get_tqual_typ(unsigned loc,struct _tuple20*t){struct _tuple19*_T0;struct _tuple20*_T1;struct _tuple20 _T2;struct _tuple20*_T3;struct _tuple20 _T4;{struct _tuple19*_T5=_cycalloc(sizeof(struct _tuple19));_T1=t;_T2=*_T1;
_T5->f0=_T2.f1;_T3=t;_T4=*_T3;_T5->f1=_T4.f2;_T0=(struct _tuple19*)_T5;}return _T0;}
# 419
static int Cyc_Parse_is_typeparam(void*tm){void*_T0;int*_T1;int _T2;_T0=tm;_T1=(int*)_T0;_T2=*_T1;if(_T2!=4)goto _TL70;
# 421
return 1;_TL70:
 return 0;;}
# 428
static void*Cyc_Parse_id2type(struct _fat_ptr s,void*k,void*aliashint,unsigned loc){struct _fat_ptr _T0;struct _fat_ptr _T1;int _T2;void*_T3;struct _fat_ptr _T4;struct _fat_ptr _T5;int _T6;void*_T7;struct _fat_ptr _T8;struct _fat_ptr _T9;int _TA;void*_TB;int _TC;void*_TD;struct Cyc_Absyn_Tvar*_TE;struct _fat_ptr*_TF;void*_T10;_T0=s;_T1=
_tag_fat("`H",sizeof(char),3U);_T2=Cyc_zstrcmp(_T0,_T1);if(_T2!=0)goto _TL72;_T3=Cyc_Absyn_heap_rgn_type;
return _T3;_TL72: _T4=s;_T5=
# 432
_tag_fat("`U",sizeof(char),3U);_T6=Cyc_zstrcmp(_T4,_T5);if(_T6!=0)goto _TL74;_T7=Cyc_Absyn_unique_rgn_shorthand_type;
return _T7;_TL74: _T8=s;_T9=
_tag_fat("`RC",sizeof(char),4U);_TA=Cyc_zstrcmp(_T8,_T9);if(_TA!=0)goto _TL76;_TB=Cyc_Absyn_refcnt_rgn_shorthand_type;
return _TB;_TL76: _TC=
Cyc_zstrcmp(s,Cyc_CurRgn_curr_rgn_name);if(_TC!=0)goto _TL78;_TD=
Cyc_CurRgn_curr_rgn_type();return _TD;_TL78:
 aliashint=Cyc_Kinds_consistent_aliashint(loc,k,aliashint);{struct Cyc_Absyn_Tvar*_T11=_cycalloc(sizeof(struct Cyc_Absyn_Tvar));{struct _fat_ptr*_T12=_cycalloc(sizeof(struct _fat_ptr));
*_T12=s;_TF=(struct _fat_ptr*)_T12;}_T11->name=_TF;_T11->identity=- 1;_T11->kind=k;_T11->aquals_bound=aliashint;_TE=(struct Cyc_Absyn_Tvar*)_T11;}_T10=Cyc_Absyn_var_type(_TE);return _T10;}
# 442
static void*Cyc_Parse_id2aqual(unsigned loc,struct _fat_ptr s){unsigned long _T0;struct _fat_ptr _T1;unsigned char*_T2;const char*_T3;const char*_T4;int _T5;void*_T6;void*_T7;void*_T8;unsigned long _T9;struct _fat_ptr _TA;unsigned char*_TB;const char*_TC;const char*_TD;char _TE;int _TF;struct _fat_ptr _T10;unsigned char*_T11;const char*_T12;char _T13;int _T14;void*_T15;struct Cyc_Warn_String_Warn_Warg_struct _T16;struct Cyc_Warn_String_Warn_Warg_struct _T17;unsigned _T18;struct _fat_ptr _T19;void*_T1A;_T0=
Cyc_strlen(s);if(_T0!=2U)goto _TL7A;_T1=s;_T2=_T1.curr;_T3=(const char*)_T2;_T4=
_check_null(_T3);{char _T1B=_T4[1];_T5=(int)_T1B;switch(_T5){case 65: _T6=Cyc_Absyn_al_qual_type;
return _T6;case 85: _T7=Cyc_Absyn_un_qual_type;
return _T7;case 84: _T8=Cyc_Absyn_rtd_qual_type;
return _T8;default: goto _LL0;}_LL0:;}goto _TL7B;
# 451
_TL7A: _T9=Cyc_strlen(s);if(_T9!=3U)goto _TL7D;_TA=s;_TB=_TA.curr;_TC=(const char*)_TB;_TD=
_check_null(_TC);_TE=_TD[1];_TF=(int)_TE;if(_TF!=82)goto _TL7F;_T10=s;_T11=_T10.curr;_T12=(const char*)_T11;_T13=_T12[2];_T14=(int)_T13;if(_T14!=67)goto _TL7F;_T15=Cyc_Absyn_rc_qual_type;
return _T15;_TL7F: goto _TL7E;_TL7D: _TL7E: _TL7B:{struct Cyc_Warn_String_Warn_Warg_struct _T1B;_T1B.tag=0;
# 455
_T1B.f1=_tag_fat("bad aqual bound ",sizeof(char),17U);_T16=_T1B;}{struct Cyc_Warn_String_Warn_Warg_struct _T1B=_T16;{struct Cyc_Warn_String_Warn_Warg_struct _T1C;_T1C.tag=0;_T1C.f1=s;_T17=_T1C;}{struct Cyc_Warn_String_Warn_Warg_struct _T1C=_T17;void*_T1D[2];_T1D[0]=& _T1B;_T1D[1]=& _T1C;_T18=loc;_T19=_tag_fat(_T1D,sizeof(void*),2);Cyc_Warn_err2(_T18,_T19);}}_T1A=Cyc_Absyn_al_qual_type;
return _T1A;}
# 459
static struct Cyc_List_List*Cyc_Parse_insert_aqual(struct _RegionHandle*yy,struct Cyc_List_List*qlist,void*aq,unsigned loc){struct Cyc_List_List*_T0;int*_T1;int _T2;struct Cyc_Warn_String_Warn_Warg_struct _T3;unsigned _T4;struct _fat_ptr _T5;struct Cyc_List_List*_T6;struct Cyc_List_List*_T7;struct Cyc_List_List*_T8;struct _RegionHandle*_T9;struct Cyc_Parse_Alias_ptrqual_Parse_Pointer_qual_struct*_TA;struct _RegionHandle*_TB;{
struct Cyc_List_List*l=qlist;_TL84: if(l!=0)goto _TL82;else{goto _TL83;}
_TL82: _T0=l;{void*_TC=_T0->hd;_T1=(int*)_TC;_T2=*_T1;if(_T2!=10)goto _TL85;{struct Cyc_Warn_String_Warn_Warg_struct _TD;_TD.tag=0;
# 463
_TD.f1=_tag_fat("Multiple alias qualifiers",sizeof(char),26U);_T3=_TD;}{struct Cyc_Warn_String_Warn_Warg_struct _TD=_T3;void*_TE[1];_TE[0]=& _TD;_T4=loc;_T5=_tag_fat(_TE,sizeof(void*),1);Cyc_Warn_err2(_T4,_T5);}_T6=qlist;
return _T6;_TL85: goto _LL0;_LL0:;}_T7=l;
# 460
l=_T7->tl;goto _TL84;_TL83:;}_T9=yy;{struct Cyc_List_List*_TC=_region_malloc(_T9,0U,sizeof(struct Cyc_List_List));_TB=yy;{struct Cyc_Parse_Alias_ptrqual_Parse_Pointer_qual_struct*_TD=_region_malloc(_TB,0U,sizeof(struct Cyc_Parse_Alias_ptrqual_Parse_Pointer_qual_struct));_TD->tag=10;
# 469
_TD->f1=aq;_TA=(struct Cyc_Parse_Alias_ptrqual_Parse_Pointer_qual_struct*)_TD;}_TC->hd=(void*)_TA;_TC->tl=qlist;_T8=(struct Cyc_List_List*)_TC;}return _T8;}
# 472
static void Cyc_Parse_tvar_ok(struct _fat_ptr s,unsigned loc){struct _fat_ptr _T0;struct _fat_ptr _T1;int _T2;unsigned _T3;struct _fat_ptr _T4;struct _fat_ptr _T5;struct _fat_ptr _T6;struct _fat_ptr _T7;int _T8;unsigned _T9;struct _fat_ptr _TA;struct _fat_ptr _TB;struct _fat_ptr _TC;struct _fat_ptr _TD;int _TE;unsigned _TF;struct _fat_ptr _T10;struct _fat_ptr _T11;int _T12;unsigned _T13;struct _fat_ptr _T14;struct _fat_ptr _T15;_T0=s;_T1=
_tag_fat("`H",sizeof(char),3U);_T2=Cyc_zstrcmp(_T0,_T1);if(_T2!=0)goto _TL87;_T3=loc;_T4=
_tag_fat("bad occurrence of heap region",sizeof(char),30U);_T5=_tag_fat(0U,sizeof(void*),0);Cyc_Warn_err(_T3,_T4,_T5);goto _TL88;_TL87: _TL88: _T6=s;_T7=
_tag_fat("`U",sizeof(char),3U);_T8=Cyc_zstrcmp(_T6,_T7);if(_T8!=0)goto _TL89;_T9=loc;_TA=
_tag_fat("bad occurrence of unique region",sizeof(char),32U);_TB=_tag_fat(0U,sizeof(void*),0);Cyc_Warn_err(_T9,_TA,_TB);goto _TL8A;_TL89: _TL8A: _TC=s;_TD=
_tag_fat("`RC",sizeof(char),4U);_TE=Cyc_zstrcmp(_TC,_TD);if(_TE!=0)goto _TL8B;_TF=loc;_T10=
_tag_fat("bad occurrence of refcounted region",sizeof(char),36U);_T11=_tag_fat(0U,sizeof(void*),0);Cyc_Warn_err(_TF,_T10,_T11);goto _TL8C;_TL8B: _TL8C: _T12=
Cyc_zstrcmp(s,Cyc_CurRgn_curr_rgn_name);if(_T12!=0)goto _TL8D;_T13=loc;_T14=
_tag_fat("bad occurrence of \"current\" region",sizeof(char),35U);_T15=_tag_fat(0U,sizeof(void*),0);Cyc_Warn_err(_T13,_T14,_T15);goto _TL8E;_TL8D: _TL8E:;}
# 487
static struct Cyc_Absyn_Tvar*Cyc_Parse_typ2tvar(unsigned loc,void*t){void*_T0;int*_T1;int _T2;void*_T3;struct Cyc_Absyn_Tvar*_T4;int(*_T5)(unsigned,struct _fat_ptr);unsigned _T6;struct _fat_ptr _T7;struct Cyc_Absyn_Tvar*_T8;_T0=t;_T1=(int*)_T0;_T2=*_T1;if(_T2!=2)goto _TL8F;_T3=t;{struct Cyc_Absyn_VarType_Absyn_Type_struct*_T9=(struct Cyc_Absyn_VarType_Absyn_Type_struct*)_T3;_T8=_T9->f1;}{struct Cyc_Absyn_Tvar*pr=_T8;_T4=pr;
# 489
return _T4;}_TL8F:{
int(*_T9)(unsigned,struct _fat_ptr)=(int(*)(unsigned,struct _fat_ptr))Cyc_Parse_parse_abort;_T5=_T9;}_T6=loc;_T7=_tag_fat("expecting a list of type variables, not types",sizeof(char),46U);_T5(_T6,_T7);;}
# 495
static void Cyc_Parse_set_vartyp_kind(void*t,struct Cyc_Absyn_Kind*k,int leq){int*_T0;int _T1;struct Cyc_Absyn_Tvar*_T2;struct Cyc_Absyn_VarType_Absyn_Type_struct*_T3;struct Cyc_Absyn_Tvar*_T4;void**_T5;void**_T6;void*_T7;int*_T8;int _T9;void**_TA;void*_TB;int _TC;struct Cyc_Absyn_Less_kb_Absyn_KindBound_struct*_TD;
void*_TE=Cyc_Absyn_compress(t);void*_TF;_T0=(int*)_TE;_T1=*_T0;if(_T1!=2)goto _TL91;{struct Cyc_Absyn_VarType_Absyn_Type_struct*_T10=(struct Cyc_Absyn_VarType_Absyn_Type_struct*)_TE;_T2=_T10->f1;{struct Cyc_Absyn_Tvar _T11=*_T2;_T3=(struct Cyc_Absyn_VarType_Absyn_Type_struct*)_TE;_T4=_T3->f1;_T5=& _T4->kind;_TF=(void**)_T5;}}{void**cptr=(void**)_TF;_T6=cptr;_T7=*_T6;{
# 498
void*_T10=Cyc_Kinds_compress_kb(_T7);_T8=(int*)_T10;_T9=*_T8;if(_T9!=1)goto _TL93;_TA=cptr;_TC=leq;
# 500
if(!_TC)goto _TL95;{struct Cyc_Absyn_Less_kb_Absyn_KindBound_struct*_T11=_cycalloc(sizeof(struct Cyc_Absyn_Less_kb_Absyn_KindBound_struct));_T11->tag=2;_T11->f1=0;_T11->f2=k;_TD=(struct Cyc_Absyn_Less_kb_Absyn_KindBound_struct*)_T11;}_TB=(void*)_TD;goto _TL96;_TL95: _TB=Cyc_Kinds_kind_to_bound(k);_TL96:*_TA=_TB;return;_TL93:
 return;;}}goto _TL92;_TL91:
# 503
 return;_TL92:;}
# 508
static struct Cyc_List_List*Cyc_Parse_oldstyle2newstyle(struct _RegionHandle*yy,struct Cyc_List_List*tms,struct Cyc_List_List*tds,unsigned loc){struct Cyc_List_List*_T0;int*_T1;int _T2;void*_T3;struct Cyc_List_List*_T4;struct Cyc_List_List*_T5;struct Cyc_List_List*_T6;struct Cyc_List_List*_T7;void*_T8;int _T9;struct Cyc_List_List*_TA;struct Cyc_List_List*_TB;struct Cyc_List_List*_TC;struct Cyc_List_List*_TD;void*_TE;int*_TF;int _T10;unsigned _T11;struct _fat_ptr _T12;struct _fat_ptr _T13;struct Cyc_List_List*_T14;void*_T15;int _T16;int _T17;int(*_T18)(unsigned,struct _fat_ptr);unsigned _T19;struct _fat_ptr _T1A;struct Cyc_List_List*_T1B;void*_T1C;struct Cyc_Absyn_Decl*_T1D;int*_T1E;int _T1F;struct Cyc_Absyn_Vardecl*_T20;struct _tuple0*_T21;struct _tuple0 _T22;struct _fat_ptr*_T23;struct Cyc_List_List*_T24;void*_T25;struct _fat_ptr*_T26;int _T27;struct Cyc_Absyn_Vardecl*_T28;struct Cyc_Absyn_Exp*_T29;int(*_T2A)(unsigned,struct _fat_ptr);struct Cyc_Absyn_Decl*_T2B;unsigned _T2C;struct _fat_ptr _T2D;struct Cyc_Absyn_Vardecl*_T2E;struct _tuple0*_T2F;int _T30;int(*_T31)(unsigned,struct _fat_ptr);struct Cyc_Absyn_Decl*_T32;unsigned _T33;struct _fat_ptr _T34;struct Cyc_List_List*_T35;struct _tuple8*_T36;struct Cyc_Absyn_Vardecl*_T37;struct _tuple0*_T38;struct _tuple0 _T39;struct Cyc_Absyn_Vardecl*_T3A;struct Cyc_Absyn_Vardecl*_T3B;int(*_T3C)(unsigned,struct _fat_ptr);struct Cyc_Absyn_Decl*_T3D;unsigned _T3E;struct _fat_ptr _T3F;struct Cyc_List_List*_T40;int(*_T41)(unsigned,struct _fat_ptr);unsigned _T42;struct Cyc_List_List*_T43;void*_T44;struct _fat_ptr*_T45;struct _fat_ptr _T46;struct _fat_ptr _T47;struct _fat_ptr _T48;struct Cyc_List_List*_T49;struct Cyc_List_List*_T4A;struct _RegionHandle*_T4B;struct Cyc_Absyn_Function_mod_Absyn_Type_modifier_struct*_T4C;struct _RegionHandle*_T4D;struct Cyc_Absyn_WithTypes_Absyn_Funcparams_struct*_T4E;struct _RegionHandle*_T4F;struct Cyc_List_List*_T50;struct _RegionHandle*_T51;struct Cyc_List_List*_T52;struct _RegionHandle*_T53;struct Cyc_List_List*_T54;struct Cyc_List_List*_T55;struct Cyc_List_List*_T56;unsigned _T57;
# 516
if(tms!=0)goto _TL97;return 0;_TL97: _T0=tms;{
# 518
void*_T58=_T0->hd;void*_T59;_T1=(int*)_T58;_T2=*_T1;if(_T2!=3)goto _TL99;{struct Cyc_Absyn_Function_mod_Absyn_Type_modifier_struct*_T5A=(struct Cyc_Absyn_Function_mod_Absyn_Type_modifier_struct*)_T58;_T3=_T5A->f1;_T59=(void*)_T3;}{void*args=_T59;_T4=tms;_T5=_T4->tl;
# 521
if(_T5==0)goto _TL9D;else{goto _TL9E;}_TL9E: _T6=tms;_T7=_T6->tl;_T8=_T7->hd;_T9=
Cyc_Parse_is_typeparam(_T8);
# 521
if(_T9)goto _TL9F;else{goto _TL9B;}_TL9F: _TA=tms;_TB=_TA->tl;_TC=
_check_null(_TB);_TD=_TC->tl;
# 521
if(_TD==0)goto _TL9D;else{goto _TL9B;}
# 524
_TL9D:{struct Cyc_List_List*_T5A;_TE=args;_TF=(int*)_TE;_T10=*_TF;if(_T10!=1)goto _TLA0;_T11=loc;_T12=
# 527
_tag_fat("function declaration with both new- and old-style parameter declarations; ignoring old-style",sizeof(char),93U);_T13=_tag_fat(0U,sizeof(void*),0);
# 526
Cyc_Warn_warn(_T11,_T12,_T13);_T14=tms;
# 528
return _T14;_TLA0: _T15=args;{struct Cyc_Absyn_NoTypes_Absyn_Funcparams_struct*_T5B=(struct Cyc_Absyn_NoTypes_Absyn_Funcparams_struct*)_T15;_T5A=_T5B->f1;}{struct Cyc_List_List*ids=_T5A;_T16=
# 530
Cyc_List_length(ids);_T17=Cyc_List_length(tds);if(_T16==_T17)goto _TLA2;{
int(*_T5B)(unsigned,struct _fat_ptr)=(int(*)(unsigned,struct _fat_ptr))Cyc_Parse_parse_abort;_T18=_T5B;}_T19=loc;_T1A=
_tag_fat("wrong number of parameter declarations in old-style function declaration",sizeof(char),73U);
# 531
_T18(_T19,_T1A);goto _TLA3;_TLA2: _TLA3: {
# 534
struct Cyc_List_List*rev_new_params=0;
_TLA7: if(ids!=0)goto _TLA5;else{goto _TLA6;}
_TLA5:{struct Cyc_List_List*tds2=tds;
_TLAB: if(tds2!=0)goto _TLA9;else{goto _TLAA;}
_TLA9: _T1B=tds2;_T1C=_T1B->hd;{struct Cyc_Absyn_Decl*x=(struct Cyc_Absyn_Decl*)_T1C;_T1D=x;{
void*_T5B=_T1D->r;struct Cyc_Absyn_Vardecl*_T5C;_T1E=(int*)_T5B;_T1F=*_T1E;if(_T1F!=0)goto _TLAC;{struct Cyc_Absyn_Var_d_Absyn_Raw_decl_struct*_T5D=(struct Cyc_Absyn_Var_d_Absyn_Raw_decl_struct*)_T5B;_T5C=_T5D->f1;}{struct Cyc_Absyn_Vardecl*vd=_T5C;_T20=vd;_T21=_T20->name;_T22=*_T21;_T23=_T22.f1;_T24=ids;_T25=_T24->hd;_T26=(struct _fat_ptr*)_T25;_T27=
# 541
Cyc_zstrptrcmp(_T23,_T26);if(_T27==0)goto _TLAE;goto _TLA8;_TLAE: _T28=vd;_T29=_T28->initializer;
# 543
if(_T29==0)goto _TLB0;{
int(*_T5D)(unsigned,struct _fat_ptr)=(int(*)(unsigned,struct _fat_ptr))Cyc_Parse_parse_abort;_T2A=_T5D;}_T2B=x;_T2C=_T2B->loc;_T2D=_tag_fat("initializer found in parameter declaration",sizeof(char),43U);_T2A(_T2C,_T2D);goto _TLB1;_TLB0: _TLB1: _T2E=vd;_T2F=_T2E->name;_T30=
Cyc_Absyn_is_qvar_qualified(_T2F);if(!_T30)goto _TLB2;{
int(*_T5D)(unsigned,struct _fat_ptr)=(int(*)(unsigned,struct _fat_ptr))Cyc_Parse_parse_abort;_T31=_T5D;}_T32=x;_T33=_T32->loc;_T34=_tag_fat("namespaces forbidden in parameter declarations",sizeof(char),47U);_T31(_T33,_T34);goto _TLB3;_TLB2: _TLB3:{struct Cyc_List_List*_T5D=_cycalloc(sizeof(struct Cyc_List_List));{struct _tuple8*_T5E=_cycalloc(sizeof(struct _tuple8));_T37=vd;_T38=_T37->name;_T39=*_T38;
_T5E->f0=_T39.f1;_T3A=vd;_T5E->f1=_T3A->tq;_T3B=vd;_T5E->f2=_T3B->type;_T36=(struct _tuple8*)_T5E;}_T5D->hd=_T36;
_T5D->tl=rev_new_params;_T35=(struct Cyc_List_List*)_T5D;}
# 547
rev_new_params=_T35;goto L;}_TLAC:{
# 550
int(*_T5D)(unsigned,struct _fat_ptr)=(int(*)(unsigned,struct _fat_ptr))Cyc_Parse_parse_abort;_T3C=_T5D;}_T3D=x;_T3E=_T3D->loc;_T3F=_tag_fat("nonvariable declaration in parameter type",sizeof(char),42U);_T3C(_T3E,_T3F);;}}_TLA8: _T40=tds2;
# 537
tds2=_T40->tl;goto _TLAB;_TLAA:
# 553
 L: if(tds2!=0)goto _TLB4;{
int(*_T5B)(unsigned,struct _fat_ptr)=(int(*)(unsigned,struct _fat_ptr))Cyc_Parse_parse_abort;_T41=_T5B;}_T42=loc;_T43=ids;_T44=_T43->hd;_T45=(struct _fat_ptr*)_T44;_T46=*_T45;_T47=_tag_fat(" is not given a type",sizeof(char),21U);_T48=Cyc_strconcat(_T46,_T47);_T41(_T42,_T48);goto _TLB5;_TLB4: _TLB5:;}_T49=ids;
# 535
ids=_T49->tl;goto _TLA7;_TLA6: _T4B=yy;{struct Cyc_List_List*_T5B=_region_malloc(_T4B,0U,sizeof(struct Cyc_List_List));_T4D=yy;{struct Cyc_Absyn_Function_mod_Absyn_Type_modifier_struct*_T5C=_region_malloc(_T4D,0U,sizeof(struct Cyc_Absyn_Function_mod_Absyn_Type_modifier_struct));_T5C->tag=3;_T4F=yy;{struct Cyc_Absyn_WithTypes_Absyn_Funcparams_struct*_T5D=_region_malloc(_T4F,0U,sizeof(struct Cyc_Absyn_WithTypes_Absyn_Funcparams_struct));_T5D->tag=1;
# 557
_T5D->f1=Cyc_List_imp_rev(rev_new_params);
_T5D->f2=0;_T5D->f3=0;_T5D->f4=0;_T5D->f5=0;_T5D->f6=0;_T5D->f7=0;_T5D->f8=0;_T5D->f9=0;_T5D->f10=0;_T4E=(struct Cyc_Absyn_WithTypes_Absyn_Funcparams_struct*)_T5D;}
# 557
_T5C->f1=(void*)_T4E;_T4C=(struct Cyc_Absyn_Function_mod_Absyn_Type_modifier_struct*)_T5C;}_T5B->hd=(void*)_T4C;
# 559
_T5B->tl=0;_T4A=(struct Cyc_List_List*)_T5B;}
# 556
return _T4A;}};}goto _TL9C;_TL9B: _TL9C: goto _LL4;}_TL99: _LL4: _T51=yy;{struct Cyc_List_List*_T5A=_region_malloc(_T51,0U,sizeof(struct Cyc_List_List));_T52=tms;
# 564
_T5A->hd=_T52->hd;_T53=yy;_T54=tms;_T55=_T54->tl;_T56=tds;_T57=loc;_T5A->tl=Cyc_Parse_oldstyle2newstyle(_T53,_T55,_T56,_T57);_T50=(struct Cyc_List_List*)_T5A;}return _T50;;}}
# 570
static struct Cyc_Absyn_Fndecl*Cyc_Parse_make_function(struct _RegionHandle*yy,struct Cyc_Parse_Declaration_spec*dso,struct Cyc_Parse_Declarator d,struct Cyc_List_List*tds,struct Cyc_Absyn_Stmt*body,unsigned loc){struct Cyc_Parse_Declarator _T0;struct Cyc_Parse_Declarator _T1;struct Cyc_Parse_Declarator _T2;struct _RegionHandle*_T3;struct Cyc_Parse_Declarator _T4;struct Cyc_List_List*_T5;struct Cyc_List_List*_T6;unsigned _T7;struct Cyc_Parse_Declaration_spec*_T8;struct Cyc_Parse_Declaration_spec*_T9;struct Cyc_Parse_Declaration_spec*_TA;struct Cyc_Parse_Declaration_spec*_TB;struct Cyc_Parse_Declaration_spec*_TC;int _TD;unsigned _TE;struct _fat_ptr _TF;struct _fat_ptr _T10;struct Cyc_Absyn_Tqual _T11;void*_T12;struct Cyc_List_List*_T13;struct Cyc_Parse_Declarator _T14;struct Cyc_List_List*_T15;unsigned _T16;struct _fat_ptr _T17;struct _fat_ptr _T18;void*_T19;int*_T1A;int _T1B;void*_T1C;struct Cyc_Absyn_FnInfo _T1D;struct Cyc_List_List*_T1E;void*_T1F;struct _tuple8*_T20;struct _tuple8 _T21;struct _fat_ptr*_T22;unsigned _T23;struct _fat_ptr _T24;struct _fat_ptr _T25;struct Cyc_List_List*_T26;void*_T27;struct _tuple8*_T28;struct _fat_ptr*_T29;struct Cyc_List_List*_T2A;struct Cyc_Absyn_FnInfo _T2B;struct Cyc_List_List*_T2C;struct Cyc_List_List*_T2D;struct Cyc_Absyn_Fndecl*_T2E;struct Cyc_Parse_Declarator _T2F;int(*_T30)(unsigned,struct _fat_ptr);unsigned _T31;struct _fat_ptr _T32;
# 574
if(tds==0)goto _TLB6;{struct Cyc_Parse_Declarator _T33;_T1=d;
_T33.id=_T1.id;_T2=d;_T33.varloc=_T2.varloc;_T3=yy;_T4=d;_T5=_T4.tms;_T6=tds;_T7=loc;_T33.tms=Cyc_Parse_oldstyle2newstyle(_T3,_T5,_T6,_T7);_T0=_T33;}d=_T0;goto _TLB7;_TLB6: _TLB7: {
enum Cyc_Absyn_Scope sc=2U;
struct Cyc_Parse_Type_specifier tss=Cyc_Parse_empty_spec(loc);
struct Cyc_Absyn_Tqual tq=Cyc_Absyn_empty_tqual(0U);
int is_inline=0;
struct Cyc_List_List*atts=0;
# 582
if(dso==0)goto _TLB8;_T8=dso;
tss=_T8->type_specs;_T9=dso;
tq=_T9->tq;_TA=dso;
is_inline=_TA->is_inline;_TB=dso;
atts=_TB->attributes;_TC=dso;{
# 588
enum Cyc_Parse_Storage_class _T33=_TC->sc;_TD=(int)_T33;switch(_TD){case Cyc_Parse_None_sc: goto _LL0;case Cyc_Parse_Extern_sc:
# 590
 sc=3U;goto _LL0;case Cyc_Parse_Static_sc:
 sc=0U;goto _LL0;default: _TE=loc;_TF=
_tag_fat("bad storage class on function",sizeof(char),30U);_T10=_tag_fat(0U,sizeof(void*),0);Cyc_Warn_err(_TE,_TF,_T10);goto _LL0;}_LL0:;}goto _TLB9;_TLB8: _TLB9: {
# 595
void*t=Cyc_Parse_collapse_type_specifiers(tss,loc);_T11=tq;_T12=t;_T13=atts;_T14=d;_T15=_T14.tms;{
struct _tuple14 _T33=Cyc_Parse_apply_tms(_T11,_T12,_T13,_T15);struct Cyc_List_List*_T34;struct Cyc_List_List*_T35;void*_T36;struct Cyc_Absyn_Tqual _T37;_T37=_T33.f0;_T36=_T33.f1;_T35=_T33.f2;_T34=_T33.f3;{struct Cyc_Absyn_Tqual fn_tqual=_T37;void*fn_type=_T36;struct Cyc_List_List*x=_T35;struct Cyc_List_List*out_atts=_T34;
# 600
if(x==0)goto _TLBB;_T16=loc;_T17=
_tag_fat("bad type params, ignoring",sizeof(char),26U);_T18=_tag_fat(0U,sizeof(void*),0);Cyc_Warn_warn(_T16,_T17,_T18);goto _TLBC;_TLBB: _TLBC: {struct Cyc_Absyn_FnInfo _T38;_T19=fn_type;_T1A=(int*)_T19;_T1B=*_T1A;if(_T1B!=6)goto _TLBD;_T1C=fn_type;{struct Cyc_Absyn_FnType_Absyn_Type_struct*_T39=(struct Cyc_Absyn_FnType_Absyn_Type_struct*)_T1C;_T38=_T39->f1;}{struct Cyc_Absyn_FnInfo i=_T38;_T1D=i;{
# 605
struct Cyc_List_List*args2=_T1D.args;_TLC2: if(args2!=0)goto _TLC0;else{goto _TLC1;}
_TLC0: _T1E=args2;_T1F=_T1E->hd;_T20=(struct _tuple8*)_T1F;_T21=*_T20;_T22=_T21.f0;if(_T22!=0)goto _TLC3;_T23=loc;_T24=
_tag_fat("missing argument variable in function prototype",sizeof(char),48U);_T25=_tag_fat(0U,sizeof(void*),0);Cyc_Warn_err(_T23,_T24,_T25);_T26=args2;_T27=_T26->hd;_T28=(struct _tuple8*)_T27;{struct _fat_ptr*_T39=_cycalloc(sizeof(struct _fat_ptr));
*_T39=_tag_fat("?",sizeof(char),2U);_T29=(struct _fat_ptr*)_T39;}(*_T28).f0=_T29;goto _TLC4;_TLC3: _TLC4: _T2A=args2;
# 605
args2=_T2A->tl;goto _TLC2;_TLC1:;}_T2B=i;_T2C=_T2B.attributes;_T2D=out_atts;
# 616
i.attributes=Cyc_List_append(_T2C,_T2D);{struct Cyc_Absyn_Fndecl*_T39=_cycalloc(sizeof(struct Cyc_Absyn_Fndecl));
_T39->sc=sc;_T39->is_inline=is_inline;_T2F=d;_T39->name=_T2F.id;_T39->body=body;_T39->i=i;
_T39->cached_type=0;_T39->param_vardecls=0;_T39->fn_vardecl=0;
_T39->orig_scope=sc;_T39->escapes=0;_T2E=(struct Cyc_Absyn_Fndecl*)_T39;}
# 617
return _T2E;}_TLBD:{
# 620
int(*_T39)(unsigned,struct _fat_ptr)=(int(*)(unsigned,struct _fat_ptr))Cyc_Parse_parse_abort;_T30=_T39;}_T31=loc;_T32=_tag_fat("declarator is not a function prototype",sizeof(char),39U);_T30(_T31,_T32);;}}}}}}static char _TmpG0[76U]="at most one type may appear within a type specifier \n\t(missing ';' or ','?)";
# 624
static struct _fat_ptr Cyc_Parse_msg1={(unsigned char*)_TmpG0,(unsigned char*)_TmpG0,(unsigned char*)_TmpG0 + 76U};static char _TmpG1[84U]="sign specifier may appear only once within a type specifier \n\t(missing ';' or ','?)";
# 626
static struct _fat_ptr Cyc_Parse_msg2={(unsigned char*)_TmpG1,(unsigned char*)_TmpG1,(unsigned char*)_TmpG1 + 84U};
# 633
static struct Cyc_Parse_Type_specifier Cyc_Parse_combine_specifiers(unsigned loc,struct Cyc_Parse_Type_specifier s1,struct Cyc_Parse_Type_specifier s2){struct Cyc_Parse_Type_specifier _T0;int _T1;struct Cyc_Parse_Type_specifier _T2;int _T3;unsigned _T4;struct _fat_ptr _T5;struct _fat_ptr _T6;struct Cyc_Parse_Type_specifier _T7;int _T8;struct Cyc_Parse_Type_specifier _T9;int _TA;struct Cyc_Parse_Type_specifier _TB;int _TC;unsigned _TD;struct _fat_ptr _TE;struct _fat_ptr _TF;struct Cyc_Parse_Type_specifier _T10;int _T11;struct Cyc_Parse_Type_specifier _T12;int _T13;struct Cyc_Parse_Type_specifier _T14;int _T15;unsigned _T16;struct _fat_ptr _T17;struct _fat_ptr _T18;struct Cyc_Parse_Type_specifier _T19;int _T1A;struct Cyc_Parse_Type_specifier _T1B;int _T1C;struct Cyc_Parse_Type_specifier _T1D;int _T1E;unsigned _T1F;struct _fat_ptr _T20;struct _fat_ptr _T21;struct Cyc_Parse_Type_specifier _T22;int _T23;struct Cyc_Parse_Type_specifier _T24;int _T25;struct Cyc_Parse_Type_specifier _T26;int _T27;struct Cyc_Parse_Type_specifier _T28;int _T29;struct Cyc_Parse_Type_specifier _T2A;int _T2B;struct Cyc_Parse_Type_specifier _T2C;int _T2D;struct Cyc_Parse_Type_specifier _T2E;int _T2F;unsigned _T30;struct _fat_ptr _T31;struct _fat_ptr _T32;int _T33;struct Cyc_Parse_Type_specifier _T34;int _T35;struct Cyc_Parse_Type_specifier _T36;int _T37;int _T38;struct Cyc_Parse_Type_specifier _T39;int _T3A;struct Cyc_Parse_Type_specifier _T3B;int _T3C;struct Cyc_Parse_Type_specifier _T3D;int _T3E;int _T3F;struct Cyc_Parse_Type_specifier _T40;int _T41;struct Cyc_Parse_Type_specifier _T42;struct Cyc_Parse_Type_specifier _T43;int _T44;struct Cyc_Parse_Type_specifier _T45;int _T46;unsigned _T47;struct _fat_ptr _T48;struct _fat_ptr _T49;struct Cyc_Parse_Type_specifier _T4A;int _T4B;struct Cyc_Parse_Type_specifier _T4C;struct Cyc_Parse_Type_specifier _T4D;_T0=s1;_T1=_T0.Signed_spec;
# 636
if(!_T1)goto _TLC5;_T2=s2;_T3=_T2.Signed_spec;if(!_T3)goto _TLC5;_T4=loc;_T5=Cyc_Parse_msg2;_T6=_tag_fat(0U,sizeof(void*),0);
Cyc_Warn_warn(_T4,_T5,_T6);goto _TLC6;_TLC5: _TLC6: _T7=s2;_T8=_T7.Signed_spec;
s1.Signed_spec=s1.Signed_spec | _T8;_T9=s1;_TA=_T9.Unsigned_spec;
if(!_TA)goto _TLC7;_TB=s2;_TC=_TB.Unsigned_spec;if(!_TC)goto _TLC7;_TD=loc;_TE=Cyc_Parse_msg2;_TF=_tag_fat(0U,sizeof(void*),0);
Cyc_Warn_warn(_TD,_TE,_TF);goto _TLC8;_TLC7: _TLC8: _T10=s2;_T11=_T10.Unsigned_spec;
s1.Unsigned_spec=s1.Unsigned_spec | _T11;_T12=s1;_T13=_T12.Short_spec;
if(!_T13)goto _TLC9;_T14=s2;_T15=_T14.Short_spec;if(!_T15)goto _TLC9;_T16=loc;_T17=
_tag_fat("too many occurrences of short in specifiers",sizeof(char),44U);_T18=_tag_fat(0U,sizeof(void*),0);Cyc_Warn_warn(_T16,_T17,_T18);goto _TLCA;_TLC9: _TLCA: _T19=s2;_T1A=_T19.Short_spec;
s1.Short_spec=s1.Short_spec | _T1A;_T1B=s1;_T1C=_T1B.Complex_spec;
if(!_T1C)goto _TLCB;_T1D=s2;_T1E=_T1D.Complex_spec;if(!_T1E)goto _TLCB;_T1F=loc;_T20=
_tag_fat("too many occurrences of _Complex or __complex__ in specifiers",sizeof(char),62U);_T21=_tag_fat(0U,sizeof(void*),0);Cyc_Warn_warn(_T1F,_T20,_T21);goto _TLCC;_TLCB: _TLCC: _T22=s2;_T23=_T22.Complex_spec;
s1.Complex_spec=s1.Complex_spec | _T23;_T24=s1;_T25=_T24.Long_Long_spec;
if(_T25)goto _TLD2;else{goto _TLD1;}_TLD2: _T26=s2;_T27=_T26.Long_Long_spec;if(_T27)goto _TLCF;else{goto _TLD1;}_TLD1: _T28=s1;_T29=_T28.Long_Long_spec;if(_T29)goto _TLD3;else{goto _TLD0;}_TLD3: _T2A=s2;_T2B=_T2A.Long_spec;if(_T2B)goto _TLCF;else{goto _TLD0;}_TLD0: _T2C=s2;_T2D=_T2C.Long_Long_spec;if(_T2D)goto _TLD4;else{goto _TLCD;}_TLD4: _T2E=s1;_T2F=_T2E.Long_spec;if(_T2F)goto _TLCF;else{goto _TLCD;}
# 651
_TLCF: _T30=loc;_T31=_tag_fat("too many occurrences of long in specifiers",sizeof(char),43U);_T32=_tag_fat(0U,sizeof(void*),0);Cyc_Warn_warn(_T30,_T31,_T32);goto _TLCE;_TLCD: _TLCE: _T34=s1;_T35=_T34.Long_Long_spec;
# 653
if(_T35)goto _TLD7;else{goto _TLD8;}_TLD8: _T36=s2;_T37=_T36.Long_Long_spec;if(_T37)goto _TLD7;else{goto _TLD5;}_TLD7: _T33=1;goto _TLD6;_TLD5: _T39=s1;_T3A=_T39.Long_spec;if(!_T3A)goto _TLD9;_T3B=s2;_T38=_T3B.Long_spec;goto _TLDA;_TLD9: _T38=0;_TLDA: _T33=_T38;_TLD6:
# 652
 s1.Long_Long_spec=_T33;_T3D=s1;_T3E=_T3D.Long_Long_spec;
# 654
if(_T3E)goto _TLDB;else{goto _TLDD;}_TLDD: _T40=s1;_T41=_T40.Long_spec;if(!_T41)goto _TLDE;_T3F=1;goto _TLDF;_TLDE: _T42=s2;_T3F=_T42.Long_spec;_TLDF: _T3C=_T3F;goto _TLDC;_TLDB: _T3C=0;_TLDC: s1.Long_spec=_T3C;_T43=s1;_T44=_T43.Valid_type_spec;
if(!_T44)goto _TLE0;_T45=s2;_T46=_T45.Valid_type_spec;if(!_T46)goto _TLE0;_T47=loc;_T48=Cyc_Parse_msg1;_T49=_tag_fat(0U,sizeof(void*),0);
Cyc_Warn_err(_T47,_T48,_T49);goto _TLE1;
_TLE0: _T4A=s2;_T4B=_T4A.Valid_type_spec;if(!_T4B)goto _TLE2;_T4C=s2;
s1.Type_spec=_T4C.Type_spec;
s1.Valid_type_spec=1;goto _TLE3;_TLE2: _TLE3: _TLE1: _T4D=s1;
# 661
return _T4D;}
# 667
static void*Cyc_Parse_collapse_type_specifiers(struct Cyc_Parse_Type_specifier ts,unsigned loc){struct Cyc_Parse_Type_specifier _T0;int _T1;struct Cyc_Parse_Type_specifier _T2;int _T3;struct Cyc_Parse_Type_specifier _T4;int _T5;struct Cyc_Parse_Type_specifier _T6;int _T7;struct Cyc_Parse_Type_specifier _T8;int _T9;struct Cyc_Parse_Type_specifier _TA;void*_TB;int _TC;struct Cyc_Parse_Type_specifier _TD;int _TE;int _TF;struct Cyc_Parse_Type_specifier _T10;int _T11;struct Cyc_Parse_Type_specifier _T12;int _T13;unsigned _T14;struct _fat_ptr _T15;struct _fat_ptr _T16;struct Cyc_Parse_Type_specifier _T17;int _T18;struct Cyc_Parse_Type_specifier _T19;int _T1A;struct Cyc_Parse_Type_specifier _T1B;int _T1C;struct Cyc_Parse_Type_specifier _T1D;int _T1E;struct Cyc_Parse_Type_specifier _T1F;int _T20;struct Cyc_Parse_Type_specifier _T21;int _T22;unsigned _T23;struct _fat_ptr _T24;struct _fat_ptr _T25;struct Cyc_Parse_Type_specifier _T26;int _T27;struct Cyc_Parse_Type_specifier _T28;int _T29;struct Cyc_Parse_Type_specifier _T2A;int _T2B;int _T2C;int _T2D;int _T2E;struct Cyc_Parse_Type_specifier _T2F;int _T30;void*_T31;unsigned _T32;struct _fat_ptr _T33;struct _fat_ptr _T34;struct Cyc_Parse_Type_specifier _T35;int _T36;void*_T37;void*_T38;void*_T39;void*_T3A;int*_T3B;int _T3C;void*_T3D;struct Cyc_Absyn_AppType_Absyn_Type_struct*_T3E;void*_T3F;int*_T40;unsigned _T41;void*_T42;void*_T43;int _T44;enum Cyc_Absyn_Sign _T45;int _T46;enum Cyc_Absyn_Sign _T47;int _T48;int _T49;enum Cyc_Absyn_Size_of _T4A;int _T4B;enum Cyc_Absyn_Size_of _T4C;int _T4D;struct Cyc_Parse_Type_specifier _T4E;int _T4F;int _T50;struct Cyc_Parse_Type_specifier _T51;int _T52;int _T53;unsigned _T54;struct _fat_ptr _T55;struct _fat_ptr _T56;int _T57;unsigned _T58;struct _fat_ptr _T59;struct _fat_ptr _T5A;void*_T5B;_T0=ts;{
int seen_type=_T0.Valid_type_spec;_T2=ts;_T3=_T2.Signed_spec;
if(!_T3)goto _TLE4;_T1=1;goto _TLE5;_TLE4: _T4=ts;_T1=_T4.Unsigned_spec;_TLE5: {int seen_sign=_T1;_T6=ts;_T7=_T6.Short_spec;
if(_T7)goto _TLE8;else{goto _TLE9;}_TLE9: _T8=ts;_T9=_T8.Long_spec;if(_T9)goto _TLE8;else{goto _TLE6;}_TLE8: _T5=1;goto _TLE7;_TLE6: _TA=ts;_T5=_TA.Long_Long_spec;_TLE7: {int seen_size=_T5;_TC=seen_type;
if(!_TC)goto _TLEA;_TD=ts;_TB=_TD.Type_spec;goto _TLEB;_TLEA: _TB=Cyc_Absyn_void_type;_TLEB: {void*t=_TB;
enum Cyc_Absyn_Size_of sz=2U;
enum Cyc_Absyn_Sign sgn=0U;_TE=seen_size;
# 675
if(_TE)goto _TLEE;else{goto _TLEF;}_TLEF: _TF=seen_sign;if(_TF)goto _TLEE;else{goto _TLEC;}
_TLEE: _T10=ts;_T11=_T10.Signed_spec;if(!_T11)goto _TLF0;_T12=ts;_T13=_T12.Unsigned_spec;if(!_T13)goto _TLF0;_T14=loc;_T15=Cyc_Parse_msg2;_T16=_tag_fat(0U,sizeof(void*),0);
Cyc_Warn_err(_T14,_T15,_T16);goto _TLF1;_TLF0: _TLF1: _T17=ts;_T18=_T17.Unsigned_spec;
if(!_T18)goto _TLF2;sgn=1U;goto _TLF3;_TLF2: _TLF3: _T19=ts;_T1A=_T19.Short_spec;
if(_T1A)goto _TLF8;else{goto _TLF7;}_TLF8: _T1B=ts;_T1C=_T1B.Long_spec;if(_T1C)goto _TLF6;else{goto _TLF9;}_TLF9: _T1D=ts;_T1E=_T1D.Long_Long_spec;if(_T1E)goto _TLF6;else{goto _TLF7;}_TLF7: _T1F=ts;_T20=_T1F.Long_spec;if(_T20)goto _TLFA;else{goto _TLF4;}_TLFA: _T21=ts;_T22=_T21.Long_Long_spec;if(_T22)goto _TLF6;else{goto _TLF4;}
# 681
_TLF6: _T23=loc;_T24=Cyc_Parse_msg2;_T25=_tag_fat(0U,sizeof(void*),0);Cyc_Warn_err(_T23,_T24,_T25);goto _TLF5;_TLF4: _TLF5: _T26=ts;_T27=_T26.Short_spec;
if(!_T27)goto _TLFB;sz=1U;goto _TLFC;_TLFB: _TLFC: _T28=ts;_T29=_T28.Long_spec;
if(!_T29)goto _TLFD;sz=3U;goto _TLFE;_TLFD: _TLFE: _T2A=ts;_T2B=_T2A.Long_Long_spec;
if(!_T2B)goto _TLFF;sz=4U;goto _TL100;_TLFF: _TL100: goto _TLED;_TLEC: _TLED: _T2C=seen_type;
# 688
if(_T2C)goto _TL101;else{goto _TL103;}
_TL103: _T2D=seen_sign;if(_T2D)goto _TL104;else{goto _TL106;}_TL106: _T2E=seen_size;if(_T2E)goto _TL104;else{goto _TL107;}
_TL107: _T2F=ts;_T30=_T2F.Complex_spec;if(!_T30)goto _TL108;_T31=
Cyc_Absyn_complex_type(Cyc_Absyn_double_type);return _T31;_TL108: _T32=loc;_T33=
_tag_fat("missing type within specifier",sizeof(char),30U);_T34=_tag_fat(0U,sizeof(void*),0);Cyc_Warn_warn(_T32,_T33,_T34);goto _TL105;_TL104: _TL105: _T35=ts;_T36=_T35.Complex_spec;
# 694
if(!_T36)goto _TL10A;_T37=
Cyc_Absyn_int_type(sgn,sz);_T38=Cyc_Absyn_complex_type(_T37);return _T38;
_TL10A: _T39=Cyc_Absyn_int_type(sgn,sz);return _T39;_TL101:{enum Cyc_Absyn_Size_of _T5C;enum Cyc_Absyn_Sign _T5D;_T3A=t;_T3B=(int*)_T3A;_T3C=*_T3B;if(_T3C!=0)goto _TL10C;_T3D=t;_T3E=(struct Cyc_Absyn_AppType_Absyn_Type_struct*)_T3D;_T3F=_T3E->f1;_T40=(int*)_T3F;_T41=*_T40;switch(_T41){case 1: _T42=t;{struct Cyc_Absyn_AppType_Absyn_Type_struct*_T5E=(struct Cyc_Absyn_AppType_Absyn_Type_struct*)_T42;_T43=_T5E->f1;{struct Cyc_Absyn_IntCon_Absyn_TyCon_struct*_T5F=(struct Cyc_Absyn_IntCon_Absyn_TyCon_struct*)_T43;_T5D=_T5F->f1;_T5C=_T5F->f2;}}{enum Cyc_Absyn_Sign sgn2=_T5D;enum Cyc_Absyn_Size_of sz2=_T5C;_T44=seen_sign;
# 700
if(!_T44)goto _TL10F;_T45=sgn2;_T46=(int)_T45;_T47=sgn;_T48=(int)_T47;if(_T46==_T48)goto _TL10F;
sgn2=sgn;
t=Cyc_Absyn_int_type(sgn,sz2);goto _TL110;_TL10F: _TL110: _T49=seen_size;
# 704
if(!_T49)goto _TL111;_T4A=sz2;_T4B=(int)_T4A;_T4C=sz;_T4D=(int)_T4C;if(_T4B==_T4D)goto _TL111;
t=Cyc_Absyn_int_type(sgn2,sz);goto _TL112;_TL111: _TL112: _T4E=ts;_T4F=_T4E.Complex_spec;
if(!_T4F)goto _TL113;
t=Cyc_Absyn_complex_type(t);goto _TL114;_TL113: _TL114: goto _LL0;}case 2: _T50=seen_size;
# 710
if(!_T50)goto _TL115;
t=Cyc_Absyn_long_double_type;goto _TL116;_TL115: _TL116: _T51=ts;_T52=_T51.Complex_spec;
if(!_T52)goto _TL117;
t=Cyc_Absyn_complex_type(t);goto _TL118;_TL117: _TL118: goto _LL0;default: goto _LL5;}goto _TL10D;_TL10C: _LL5: _T53=seen_sign;
# 716
if(!_T53)goto _TL119;_T54=loc;_T55=
_tag_fat("sign specification on non-integral type",sizeof(char),40U);_T56=_tag_fat(0U,sizeof(void*),0);Cyc_Warn_err(_T54,_T55,_T56);goto _TL11A;_TL119: _TL11A: _T57=seen_size;
if(!_T57)goto _TL11B;_T58=loc;_T59=
_tag_fat("size qualifier on non-integral type",sizeof(char),36U);_T5A=_tag_fat(0U,sizeof(void*),0);Cyc_Warn_err(_T58,_T59,_T5A);goto _TL11C;_TL11B: _TL11C: goto _LL0;_TL10D: _LL0:;}_T5B=t;
# 722
return _T5B;}}}}}
# 725
static struct Cyc_List_List*Cyc_Parse_apply_tmss(struct _RegionHandle*r,struct Cyc_Absyn_Tqual tq,void*t,struct _tuple13*ds,struct Cyc_List_List*shared_atts){struct _tuple13*_T0;struct Cyc_Parse_Declarator _T1;struct Cyc_Parse_Declarator _T2;struct Cyc_Absyn_Tqual _T3;void*_T4;struct Cyc_List_List*_T5;struct Cyc_Parse_Declarator _T6;struct Cyc_List_List*_T7;struct Cyc_List_List*_T8;struct _tuple13*_T9;struct _tuple13*_TA;struct _RegionHandle*_TB;struct Cyc_Absyn_Tqual _TC;void*_TD;struct _tuple13*_TE;struct _tuple13*_TF;struct Cyc_List_List*_T10;struct Cyc_List_List*_T11;struct _RegionHandle*_T12;struct _tuple15*_T13;struct _RegionHandle*_T14;
# 729
if(ds!=0)goto _TL11D;return 0;_TL11D: _T0=ds;{
struct Cyc_Parse_Declarator d=_T0->hd;_T1=d;{
struct _tuple0*q=_T1.id;_T2=d;{
unsigned varloc=_T2.varloc;_T3=tq;_T4=t;_T5=shared_atts;_T6=d;_T7=_T6.tms;{
struct _tuple14 _T15=Cyc_Parse_apply_tms(_T3,_T4,_T5,_T7);struct Cyc_List_List*_T16;struct Cyc_List_List*_T17;void*_T18;struct Cyc_Absyn_Tqual _T19;_T19=_T15.f0;_T18=_T15.f1;_T17=_T15.f2;_T16=_T15.f3;{struct Cyc_Absyn_Tqual tq2=_T19;void*new_typ=_T18;struct Cyc_List_List*tvs=_T17;struct Cyc_List_List*atts=_T16;_T9=ds;_TA=_T9->tl;
# 736
if(_TA!=0)goto _TL11F;_T8=0;goto _TL120;_TL11F: _TB=r;_TC=tq;_TD=Cyc_Tcutil_copy_type(t);_TE=ds;_TF=_TE->tl;_T10=shared_atts;_T8=Cyc_Parse_apply_tmss(_TB,_TC,_TD,_TF,_T10);_TL120: {struct Cyc_List_List*tl=_T8;_T12=r;{struct Cyc_List_List*_T1A=_region_malloc(_T12,0U,sizeof(struct Cyc_List_List));_T14=r;{struct _tuple15*_T1B=_region_malloc(_T14,0U,sizeof(struct _tuple15));
_T1B->f0=varloc;_T1B->f1=q;_T1B->f2=tq2;_T1B->f3=new_typ;_T1B->f4=tvs;_T1B->f5=atts;_T13=(struct _tuple15*)_T1B;}_T1A->hd=_T13;_T1A->tl=tl;_T11=(struct Cyc_List_List*)_T1A;}return _T11;}}}}}}}
# 740
static struct _tuple14 Cyc_Parse_apply_tms(struct Cyc_Absyn_Tqual tq,void*t,struct Cyc_List_List*atts,struct Cyc_List_List*tms){struct _tuple14 _T0;struct Cyc_List_List*_T1;int*_T2;unsigned _T3;void*_T4;struct Cyc_Absyn_Tqual _T5;void*_T6;struct Cyc_List_List*_T7;struct Cyc_List_List*_T8;struct Cyc_List_List*_T9;struct _tuple14 _TA;void*_TB;struct Cyc_Absyn_Tqual _TC;void*_TD;struct Cyc_List_List*_TE;struct Cyc_List_List*_TF;struct Cyc_List_List*_T10;struct _tuple14 _T11;void*_T12;void*_T13;int*_T14;int _T15;void*_T16;void*_T17;struct Cyc_List_List*_T18;void*_T19;int _T1A;struct Cyc_List_List*_T1B;struct Cyc_List_List*_T1C;struct Cyc_List_List*_T1D;struct Cyc_List_List*_T1E;struct Cyc_List_List*_T1F;struct Cyc_List_List*_T20;struct Cyc_List_List*_T21;struct Cyc_List_List*_T22;struct Cyc_List_List*_T23;int*_T24;int _T25;struct Cyc_List_List*_T26;int _T27;struct Cyc_List_List*_T28;struct Cyc_List_List*_T29;struct Cyc_List_List*_T2A;void*_T2B;struct _tuple8*_T2C;struct _tuple8 _T2D;struct _fat_ptr*_T2E;struct Cyc_List_List*_T2F;void*_T30;struct _tuple8*_T31;struct _tuple8 _T32;void*_T33;void*_T34;struct Cyc_List_List*_T35;void*_T36;void**_T37;void**_T38;int*_T39;int _T3A;struct Cyc_Absyn_ArrayInfo _T3B;struct Cyc_Absyn_ArrayInfo _T3C;struct Cyc_Absyn_ArrayInfo _T3D;struct Cyc_Absyn_ArrayInfo _T3E;struct Cyc_Absyn_ArrayInfo _T3F;struct _tuple0*_T40;struct _tuple0*_T41;struct _tuple0*_T42;struct Cyc_List_List*_T43;struct _fat_ptr _T44;struct Cyc_List_List*_T45;struct Cyc_List_List*_T46;struct Cyc_List_List*_T47;void*_T48;struct Cyc_List_List*_T49;struct Cyc_Absyn_Exp*_T4A;struct Cyc_List_List*_T4B;void*_T4C;struct Cyc_Absyn_Exp*_T4D;struct Cyc_List_List*_T4E;struct Cyc_List_List*_T4F;void*_T50;void**_T51;void**_T52;struct Cyc_List_List*_T53;void**_T54;void*_T55;void**_T56;void**_T57;void*_T58;struct Cyc_List_List*_T59;struct Cyc_Absyn_Tqual _T5A;unsigned _T5B;struct Cyc_Absyn_Tqual _T5C;void*_T5D;struct Cyc_List_List*_T5E;struct Cyc_List_List*_T5F;struct Cyc_List_List*_T60;struct _tuple14 _T61;void*_T62;int(*_T63)(unsigned,struct _fat_ptr);unsigned _T64;struct _fat_ptr _T65;struct Cyc_List_List*_T66;struct Cyc_List_List*_T67;struct _tuple14 _T68;int(*_T69)(unsigned,struct _fat_ptr);unsigned _T6A;struct _fat_ptr _T6B;struct Cyc_Absyn_Tqual _T6C;struct Cyc_Absyn_PtrInfo _T6D;void*_T6E;struct Cyc_List_List*_T6F;struct Cyc_List_List*_T70;struct Cyc_List_List*_T71;struct _tuple14 _T72;struct Cyc_Absyn_Tqual _T73;void*_T74;struct Cyc_List_List*_T75;struct Cyc_List_List*_T76;struct Cyc_List_List*_T77;struct _tuple14 _T78;
# 743
if(tms!=0)goto _TL121;{struct _tuple14 _T79;_T79.f0=tq;_T79.f1=t;_T79.f2=0;_T79.f3=atts;_T0=_T79;}return _T0;_TL121: _T1=tms;{
void*_T79=_T1->hd;struct Cyc_Absyn_Tqual _T7A;struct Cyc_Absyn_PtrAtts _T7B;struct Cyc_List_List*_T7C;void*_T7D;unsigned _T7E;void*_T7F;_T2=(int*)_T79;_T3=*_T2;switch(_T3){case 0:{struct Cyc_Absyn_Carray_mod_Absyn_Type_modifier_struct*_T80=(struct Cyc_Absyn_Carray_mod_Absyn_Type_modifier_struct*)_T79;_T4=_T80->f1;_T7F=(void*)_T4;_T7E=_T80->f2;}{void*zeroterm=_T7F;unsigned ztloc=_T7E;_T5=
# 746
Cyc_Absyn_empty_tqual(0U);_T6=
Cyc_Absyn_array_type(t,tq,0,zeroterm,ztloc);_T7=atts;_T8=tms;_T9=_T8->tl;_TA=
# 746
Cyc_Parse_apply_tms(_T5,_T6,_T7,_T9);return _TA;}case 1:{struct Cyc_Absyn_ConstArray_mod_Absyn_Type_modifier_struct*_T80=(struct Cyc_Absyn_ConstArray_mod_Absyn_Type_modifier_struct*)_T79;_T7F=_T80->f1;_TB=_T80->f2;_T7D=(void*)_TB;_T7E=_T80->f3;}{struct Cyc_Absyn_Exp*e=_T7F;void*zeroterm=_T7D;unsigned ztloc=_T7E;_TC=
# 749
Cyc_Absyn_empty_tqual(0U);_TD=
Cyc_Absyn_array_type(t,tq,e,zeroterm,ztloc);_TE=atts;_TF=tms;_T10=_TF->tl;_T11=
# 749
Cyc_Parse_apply_tms(_TC,_TD,_TE,_T10);return _T11;}case 3:{struct Cyc_Absyn_Function_mod_Absyn_Type_modifier_struct*_T80=(struct Cyc_Absyn_Function_mod_Absyn_Type_modifier_struct*)_T79;_T12=_T80->f1;_T7F=(void*)_T12;}{void*args=_T7F;unsigned _T80;struct Cyc_Absyn_Exp*_T81;struct Cyc_Absyn_Exp*_T82;struct Cyc_Absyn_Exp*_T83;struct Cyc_Absyn_Exp*_T84;struct Cyc_List_List*_T85;struct Cyc_List_List*_T86;void*_T87;struct Cyc_Absyn_VarargInfo*_T88;int _T89;struct Cyc_List_List*_T8A;_T13=args;_T14=(int*)_T13;_T15=*_T14;if(_T15!=1)goto _TL124;_T16=args;{struct Cyc_Absyn_WithTypes_Absyn_Funcparams_struct*_T8B=(struct Cyc_Absyn_WithTypes_Absyn_Funcparams_struct*)_T16;_T8A=_T8B->f1;_T89=_T8B->f2;_T88=_T8B->f3;_T17=_T8B->f4;_T87=(void*)_T17;_T86=_T8B->f5;_T85=_T8B->f6;_T84=_T8B->f7;_T83=_T8B->f8;_T82=_T8B->f9;_T81=_T8B->f10;}{struct Cyc_List_List*args2=_T8A;int c_vararg=_T89;struct Cyc_Absyn_VarargInfo*cyc_vararg=_T88;void*eff=_T87;struct Cyc_List_List*effc=_T86;struct Cyc_List_List*qb=_T85;struct Cyc_Absyn_Exp*chks=_T84;struct Cyc_Absyn_Exp*req=_T83;struct Cyc_Absyn_Exp*ens=_T82;struct Cyc_Absyn_Exp*thrw=_T81;
# 754
struct Cyc_List_List*typvars=0;
# 756
struct Cyc_List_List*fn_atts=0;struct Cyc_List_List*new_atts=0;{
struct Cyc_List_List*as=atts;_TL129: if(as!=0)goto _TL127;else{goto _TL128;}
_TL127: _T18=as;_T19=_T18->hd;_T1A=Cyc_Atts_fntype_att(_T19);if(!_T1A)goto _TL12A;{struct Cyc_List_List*_T8B=_cycalloc(sizeof(struct Cyc_List_List));_T1C=as;
_T8B->hd=_T1C->hd;_T8B->tl=fn_atts;_T1B=(struct Cyc_List_List*)_T8B;}fn_atts=_T1B;goto _TL12B;
# 761
_TL12A:{struct Cyc_List_List*_T8B=_cycalloc(sizeof(struct Cyc_List_List));_T1E=as;_T8B->hd=_T1E->hd;_T8B->tl=new_atts;_T1D=(struct Cyc_List_List*)_T8B;}new_atts=_T1D;_TL12B: _T1F=as;
# 757
as=_T1F->tl;goto _TL129;_TL128:;}_T20=tms;_T21=_T20->tl;
# 763
if(_T21==0)goto _TL12C;_T22=tms;_T23=_T22->tl;{
void*_T8B=_T23->hd;struct Cyc_List_List*_T8C;_T24=(int*)_T8B;_T25=*_T24;if(_T25!=4)goto _TL12E;{struct Cyc_Absyn_TypeParams_mod_Absyn_Type_modifier_struct*_T8D=(struct Cyc_Absyn_TypeParams_mod_Absyn_Type_modifier_struct*)_T8B;_T8C=_T8D->f1;}{struct Cyc_List_List*ts=_T8C;
# 766
typvars=ts;_T26=tms;
tms=_T26->tl;goto _LL12;}_TL12E: goto _LL12;_LL12:;}goto _TL12D;_TL12C: _TL12D: _T27=c_vararg;
# 772
if(_T27)goto _TL130;else{goto _TL132;}_TL132: if(cyc_vararg!=0)goto _TL130;if(args2==0)goto _TL130;_T28=args2;_T29=_T28->tl;if(_T29!=0)goto _TL130;_T2A=args2;_T2B=_T2A->hd;_T2C=(struct _tuple8*)_T2B;_T2D=*_T2C;_T2E=_T2D.f0;if(_T2E!=0)goto _TL130;_T2F=args2;_T30=_T2F->hd;_T31=(struct _tuple8*)_T30;_T32=*_T31;_T33=_T32.f2;_T34=Cyc_Absyn_void_type;if(_T33!=_T34)goto _TL130;
# 777
args2=0;goto _TL131;_TL130: _TL131: {
# 782
struct Cyc_List_List*new_requires=0;{
struct Cyc_List_List*a=args2;_TL136: if(a!=0)goto _TL134;else{goto _TL135;}
_TL134: _T35=a;_T36=_T35->hd;{struct _tuple8*_T8B=(struct _tuple8*)_T36;void*_T8C;struct Cyc_Absyn_Tqual _T8D;struct _fat_ptr*_T8E;{struct _tuple8 _T8F=*_T8B;_T8E=_T8F.f0;_T8D=_T8F.f1;_T37=& _T8B->f2;_T8C=(void**)_T37;}{struct _fat_ptr*vopt=_T8E;struct Cyc_Absyn_Tqual tq=_T8D;void**t=(void**)_T8C;_T38=t;{
void*_T8F=*_T38;unsigned _T90;void*_T91;struct Cyc_Absyn_Exp*_T92;struct Cyc_Absyn_Tqual _T93;void*_T94;_T39=(int*)_T8F;_T3A=*_T39;if(_T3A!=5)goto _TL137;{struct Cyc_Absyn_ArrayType_Absyn_Type_struct*_T95=(struct Cyc_Absyn_ArrayType_Absyn_Type_struct*)_T8F;_T3B=_T95->f1;_T94=_T3B.elt_type;_T3C=_T95->f1;_T93=_T3C.tq;_T3D=_T95->f1;_T92=_T3D.num_elts;_T3E=_T95->f1;_T91=_T3E.zero_term;_T3F=_T95->f1;_T90=_T3F.zt_loc;}{void*et=_T94;struct Cyc_Absyn_Tqual tq=_T93;struct Cyc_Absyn_Exp*neltsopt=_T92;void*zt=_T91;unsigned ztloc=_T90;
# 787
if(neltsopt==0)goto _TL139;if(vopt==0)goto _TL139;{
struct _tuple0*v;v=_cycalloc(sizeof(struct _tuple0));_T40=v;_T40->f0.Loc_n.tag=4U;_T41=v;_T41->f0.Loc_n.val=0;_T42=v;_T42->f1=vopt;{
struct Cyc_Absyn_Exp*nelts=Cyc_Absyn_copy_exp(neltsopt);{struct Cyc_Absyn_Exp*_T95[1];_T95[0]=
Cyc_Absyn_var_exp(v,0U);_T44=_tag_fat(_T95,sizeof(struct Cyc_Absyn_Exp*),1);_T43=Cyc_List_list(_T44);}{struct Cyc_Absyn_Exp*e2=Cyc_Absyn_primop_exp(18U,_T43,0U);
struct Cyc_Absyn_Exp*new_req=Cyc_Absyn_lte_exp(nelts,e2,0U);{struct Cyc_List_List*_T95=_cycalloc(sizeof(struct Cyc_List_List));
_T95->hd=new_req;_T95->tl=new_requires;_T45=(struct Cyc_List_List*)_T95;}new_requires=_T45;}}}goto _TL13A;_TL139: _TL13A: goto _LL1A;}_TL137: goto _LL1A;_LL1A:;}}}_T46=a;
# 783
a=_T46->tl;goto _TL136;_TL135:;}
# 798
if(new_requires==0)goto _TL13B;{
struct Cyc_Absyn_Exp*r;
if(req==0)goto _TL13D;
r=req;goto _TL13E;
# 803
_TL13D: _T47=new_requires;_T48=_T47->hd;r=(struct Cyc_Absyn_Exp*)_T48;_T49=new_requires;
new_requires=_T49->tl;_TL13E:
# 806
 _TL142: if(new_requires!=0)goto _TL140;else{goto _TL141;}
_TL140: _T4A=r;_T4B=new_requires;_T4C=_T4B->hd;_T4D=(struct Cyc_Absyn_Exp*)_T4C;r=Cyc_Absyn_and_exp(_T4A,_T4D,0U);_T4E=new_requires;
# 806
new_requires=_T4E->tl;goto _TL142;_TL141:
# 808
 req=r;}goto _TL13C;_TL13B: _TL13C: {
# 812
struct Cyc_List_List*tags=Cyc_Parse_get_arg_tags(args2);
# 814
if(tags==0)goto _TL143;
t=Cyc_Parse_substitute_tags(tags,t);goto _TL144;_TL143: _TL144:
 t=Cyc_Parse_array2ptr(t,0);{
# 819
struct Cyc_List_List*a=args2;_TL148: if(a!=0)goto _TL146;else{goto _TL147;}
_TL146: _T4F=a;_T50=_T4F->hd;{struct _tuple8*_T8B=(struct _tuple8*)_T50;void*_T8C;struct Cyc_Absyn_Tqual _T8D;struct _fat_ptr*_T8E;{struct _tuple8 _T8F=*_T8B;_T8E=_T8F.f0;_T8D=_T8F.f1;_T51=& _T8B->f2;_T8C=(void**)_T51;}{struct _fat_ptr*vopt=_T8E;struct Cyc_Absyn_Tqual tq=_T8D;void**t=(void**)_T8C;
if(tags==0)goto _TL149;_T52=t;_T53=tags;_T54=t;_T55=*_T54;
*_T52=Cyc_Parse_substitute_tags(_T53,_T55);goto _TL14A;_TL149: _TL14A: _T56=t;_T57=t;_T58=*_T57;
*_T56=Cyc_Parse_array2ptr(_T58,1);}}_T59=a;
# 819
a=_T59->tl;goto _TL148;_TL147:;}_T5A=tq;_T5B=_T5A.loc;_T5C=
# 831
Cyc_Absyn_empty_tqual(_T5B);_T5D=
Cyc_Absyn_function_type(typvars,eff,tq,t,args2,c_vararg,cyc_vararg,effc,qb,fn_atts,chks,req,ens,thrw);_T5E=new_atts;_T5F=
# 836
_check_null(tms);_T60=_T5F->tl;_T61=
# 831
Cyc_Parse_apply_tms(_T5C,_T5D,_T5E,_T60);return _T61;}}}_TL124: _T62=args;{struct Cyc_Absyn_NoTypes_Absyn_Funcparams_struct*_T8B=(struct Cyc_Absyn_NoTypes_Absyn_Funcparams_struct*)_T62;_T80=_T8B->f2;}{unsigned loc=_T80;{
# 838
int(*_T8B)(unsigned,struct _fat_ptr)=(int(*)(unsigned,struct _fat_ptr))Cyc_Parse_parse_abort;_T63=_T8B;}_T64=loc;_T65=_tag_fat("function declaration without parameter types",sizeof(char),45U);_T63(_T64,_T65);};}case 4:{struct Cyc_Absyn_TypeParams_mod_Absyn_Type_modifier_struct*_T80=(struct Cyc_Absyn_TypeParams_mod_Absyn_Type_modifier_struct*)_T79;_T7C=_T80->f1;_T7E=_T80->f2;}{struct Cyc_List_List*ts=_T7C;unsigned loc=_T7E;_T66=tms;_T67=_T66->tl;
# 845
if(_T67!=0)goto _TL14B;{struct _tuple14 _T80;
_T80.f0=tq;_T80.f1=t;_T80.f2=ts;_T80.f3=atts;_T68=_T80;}return _T68;_TL14B:{
# 850
int(*_T80)(unsigned,struct _fat_ptr)=(int(*)(unsigned,struct _fat_ptr))Cyc_Parse_parse_abort;_T69=_T80;}_T6A=loc;_T6B=
_tag_fat("type parameters must appear before function arguments in declarator",sizeof(char),68U);
# 850
_T69(_T6A,_T6B);}case 2:{struct Cyc_Absyn_Pointer_mod_Absyn_Type_modifier_struct*_T80=(struct Cyc_Absyn_Pointer_mod_Absyn_Type_modifier_struct*)_T79;_T7B=_T80->f1;_T7A=_T80->f2;}{struct Cyc_Absyn_PtrAtts ptratts=_T7B;struct Cyc_Absyn_Tqual tq2=_T7A;_T6C=tq2;{struct Cyc_Absyn_PtrInfo _T80;
# 853
_T80.elt_type=t;_T80.elt_tq=tq;_T80.ptr_atts=ptratts;_T6D=_T80;}_T6E=Cyc_Absyn_pointer_type(_T6D);_T6F=atts;_T70=tms;_T71=_T70->tl;_T72=Cyc_Parse_apply_tms(_T6C,_T6E,_T6F,_T71);return _T72;}default:{struct Cyc_Absyn_Attributes_mod_Absyn_Type_modifier_struct*_T80=(struct Cyc_Absyn_Attributes_mod_Absyn_Type_modifier_struct*)_T79;_T7E=_T80->f1;_T7C=_T80->f2;}{unsigned loc=_T7E;struct Cyc_List_List*atts2=_T7C;_T73=tq;_T74=t;_T75=
# 858
Cyc_List_append(atts,atts2);_T76=tms;_T77=_T76->tl;_T78=Cyc_Parse_apply_tms(_T73,_T74,_T75,_T77);return _T78;}};}}
# 864
void*Cyc_Parse_speclist2typ(struct Cyc_Parse_Type_specifier tss,unsigned loc){void*_T0;_T0=
Cyc_Parse_collapse_type_specifiers(tss,loc);return _T0;}
# 873
static struct Cyc_Absyn_Decl*Cyc_Parse_v_typ_to_typedef(unsigned loc,struct _tuple15*t){struct _tuple15*_T0;void*_T1;int*_T2;int _T3;void*_T4;struct Cyc_Core_Opt*_T5;struct Cyc_Core_Opt*_T6;struct Cyc_Absyn_Typedef_d_Absyn_Raw_decl_struct*_T7;struct Cyc_Absyn_Typedefdecl*_T8;void*_T9;unsigned _TA;struct Cyc_Absyn_Decl*_TB;struct Cyc_List_List*_TC;struct Cyc_List_List*_TD;void*_TE;struct Cyc_Absyn_Tqual _TF;struct _tuple0*_T10;unsigned _T11;_T0=t;{struct _tuple15 _T12=*_T0;_T11=_T12.f0;_T10=_T12.f1;_TF=_T12.f2;_TE=_T12.f3;_TD=_T12.f4;_TC=_T12.f5;}{unsigned varloc=_T11;struct _tuple0*x=_T10;struct Cyc_Absyn_Tqual tq=_TF;void*typ=_TE;struct Cyc_List_List*tvs=_TD;struct Cyc_List_List*atts=_TC;
# 876
Cyc_Lex_register_typedef(x);{
# 878
struct Cyc_Core_Opt*kind;
void*type;{struct Cyc_Core_Opt*_T12;_T1=typ;_T2=(int*)_T1;_T3=*_T2;if(_T3!=1)goto _TL14D;_T4=typ;{struct Cyc_Absyn_Evar_Absyn_Type_struct*_T13=(struct Cyc_Absyn_Evar_Absyn_Type_struct*)_T4;_T12=_T13->f1;}{struct Cyc_Core_Opt*kopt=_T12;
# 882
type=0;
if(kopt!=0)goto _TL14F;_T6=& Cyc_Kinds_bko;_T5=(struct Cyc_Core_Opt*)_T6;goto _TL150;_TL14F: _T5=kopt;_TL150: kind=_T5;goto _LL3;}_TL14D:
# 885
 kind=0;type=typ;goto _LL3;_LL3:;}{struct Cyc_Absyn_Typedef_d_Absyn_Raw_decl_struct*_T12=_cycalloc(sizeof(struct Cyc_Absyn_Typedef_d_Absyn_Raw_decl_struct));_T12->tag=8;{struct Cyc_Absyn_Typedefdecl*_T13=_cycalloc(sizeof(struct Cyc_Absyn_Typedefdecl));
# 887
_T13->name=x;_T13->tvs=tvs;_T13->kind=kind;
_T13->defn=type;_T13->atts=atts;
_T13->tq=tq;_T13->extern_c=0;_T8=(struct Cyc_Absyn_Typedefdecl*)_T13;}
# 887
_T12->f1=_T8;_T7=(struct Cyc_Absyn_Typedef_d_Absyn_Raw_decl_struct*)_T12;}_T9=(void*)_T7;_TA=loc;_TB=Cyc_Absyn_new_decl(_T9,_TA);return _TB;}}}
# 894
static struct Cyc_Absyn_Stmt*Cyc_Parse_flatten_decl(struct Cyc_Absyn_Decl*d,struct Cyc_Absyn_Stmt*s){struct Cyc_Absyn_Decl_s_Absyn_Raw_stmt_struct*_T0;void*_T1;struct Cyc_Absyn_Decl*_T2;unsigned _T3;struct Cyc_Absyn_Stmt*_T4;{struct Cyc_Absyn_Decl_s_Absyn_Raw_stmt_struct*_T5=_cycalloc(sizeof(struct Cyc_Absyn_Decl_s_Absyn_Raw_stmt_struct));_T5->tag=12;
_T5->f1=d;_T5->f2=s;_T0=(struct Cyc_Absyn_Decl_s_Absyn_Raw_stmt_struct*)_T5;}_T1=(void*)_T0;_T2=d;_T3=_T2->loc;_T4=Cyc_Absyn_new_stmt(_T1,_T3);return _T4;}
# 898
static struct Cyc_Absyn_Stmt*Cyc_Parse_flatten_declarations(struct Cyc_List_List*ds,struct Cyc_Absyn_Stmt*s){struct Cyc_Absyn_Stmt*(*_T0)(struct Cyc_Absyn_Stmt*(*)(struct Cyc_Absyn_Decl*,struct Cyc_Absyn_Stmt*),struct Cyc_List_List*,struct Cyc_Absyn_Stmt*);void*(*_T1)(void*(*)(void*,void*),struct Cyc_List_List*,void*);struct Cyc_List_List*_T2;struct Cyc_Absyn_Stmt*_T3;struct Cyc_Absyn_Stmt*_T4;_T1=Cyc_List_fold_right;{
struct Cyc_Absyn_Stmt*(*_T5)(struct Cyc_Absyn_Stmt*(*)(struct Cyc_Absyn_Decl*,struct Cyc_Absyn_Stmt*),struct Cyc_List_List*,struct Cyc_Absyn_Stmt*)=(struct Cyc_Absyn_Stmt*(*)(struct Cyc_Absyn_Stmt*(*)(struct Cyc_Absyn_Decl*,struct Cyc_Absyn_Stmt*),struct Cyc_List_List*,struct Cyc_Absyn_Stmt*))_T1;_T0=_T5;}_T2=ds;_T3=s;_T4=_T0(Cyc_Parse_flatten_decl,_T2,_T3);return _T4;}
# 902
static void Cyc_Parse_decl_split(struct _RegionHandle*r,struct _tuple11*ds,struct _tuple13**decls,struct Cyc_List_List**es,struct Cyc_List_List**rs){struct _tuple11*_T0;struct _tuple13*_T1;struct _RegionHandle*_T2;struct Cyc_List_List*_T3;struct _RegionHandle*_T4;struct Cyc_List_List*_T5;struct _RegionHandle*_T6;struct _tuple11*_T7;struct Cyc_List_List**_T8;struct Cyc_List_List**_T9;struct _tuple13**_TA;struct _tuple13*(*_TB)(struct _tuple13*);
# 906
struct _tuple13*declarators=0;
struct Cyc_List_List*exprs=0;
struct Cyc_List_List*renames=0;
_TL154: if(ds!=0)goto _TL152;else{goto _TL153;}
_TL152: _T0=ds;{struct _tuple12 _TC=_T0->hd;struct Cyc_Absyn_Exp*_TD;struct Cyc_Absyn_Exp*_TE;struct Cyc_Parse_Declarator _TF;_TF=_TC.f0;_TE=_TC.f1;_TD=_TC.f2;{struct Cyc_Parse_Declarator d=_TF;struct Cyc_Absyn_Exp*e=_TE;struct Cyc_Absyn_Exp*rename=_TD;_T2=r;{struct _tuple13*_T10=_region_malloc(_T2,0U,sizeof(struct _tuple13));
_T10->tl=declarators;_T10->hd=d;_T1=(struct _tuple13*)_T10;}declarators=_T1;_T4=r;{struct Cyc_List_List*_T10=_region_malloc(_T4,0U,sizeof(struct Cyc_List_List));
_T10->hd=e;_T10->tl=exprs;_T3=(struct Cyc_List_List*)_T10;}exprs=_T3;_T6=r;{struct Cyc_List_List*_T10=_region_malloc(_T6,0U,sizeof(struct Cyc_List_List));
_T10->hd=rename;_T10->tl=renames;_T5=(struct Cyc_List_List*)_T10;}renames=_T5;}}_T7=ds;
# 909
ds=_T7->tl;goto _TL154;_TL153: _T8=es;
# 915
*_T8=Cyc_List_imp_rev(exprs);_T9=rs;
*_T9=Cyc_List_imp_rev(renames);_TA=decls;{
struct _tuple13*(*_TC)(struct _tuple13*)=(struct _tuple13*(*)(struct _tuple13*))Cyc_Parse_flat_imp_rev;_TB=_TC;}*_TA=_TB(declarators);}
# 925
static struct Cyc_List_List*Cyc_Parse_make_declarations(struct Cyc_Parse_Declaration_spec ds,struct _tuple11*ids,unsigned tqual_loc,unsigned loc){struct Cyc_Parse_Declaration_spec _T0;struct Cyc_Parse_Declaration_spec _T1;struct Cyc_Parse_Declaration_spec _T2;struct Cyc_Absyn_Tqual _T3;unsigned _T4;struct Cyc_Parse_Declaration_spec _T5;int _T6;unsigned _T7;struct _fat_ptr _T8;struct _fat_ptr _T9;struct Cyc_Parse_Declaration_spec _TA;int _TB;enum Cyc_Absyn_Scope _TC;int _TD;struct _RegionHandle*_TE;struct _tuple11*_TF;struct _tuple13**_T10;struct Cyc_List_List**_T11;struct Cyc_List_List**_T12;struct Cyc_List_List*_T13;void*_T14;struct Cyc_Absyn_Exp*_T15;struct Cyc_List_List*_T16;void*_T17;int*_T18;unsigned _T19;void*_T1A;struct Cyc_Absyn_TypeDeclType_Absyn_Type_struct*_T1B;struct Cyc_Absyn_TypeDecl*_T1C;struct Cyc_Absyn_TypeDecl*_T1D;void*_T1E;int*_T1F;unsigned _T20;void*_T21;struct Cyc_Absyn_TypeDecl*_T22;void*_T23;struct Cyc_Absyn_Aggrdecl*_T24;struct Cyc_Absyn_Aggrdecl*_T25;struct Cyc_List_List*_T26;struct Cyc_List_List*_T27;struct Cyc_Absyn_Aggrdecl*_T28;struct Cyc_Absyn_Aggr_d_Absyn_Raw_decl_struct*_T29;void*_T2A;unsigned _T2B;void*_T2C;struct Cyc_Absyn_TypeDecl*_T2D;void*_T2E;unsigned _T2F;struct _fat_ptr _T30;struct _fat_ptr _T31;struct Cyc_Absyn_Enumdecl*_T32;struct Cyc_Absyn_Enum_d_Absyn_Raw_decl_struct*_T33;void*_T34;unsigned _T35;void*_T36;struct Cyc_Absyn_TypeDecl*_T37;void*_T38;unsigned _T39;struct _fat_ptr _T3A;struct _fat_ptr _T3B;struct Cyc_Absyn_Datatypedecl*_T3C;struct Cyc_Absyn_Datatype_d_Absyn_Raw_decl_struct*_T3D;void*_T3E;unsigned _T3F;void*_T40;struct Cyc_Absyn_AppType_Absyn_Type_struct*_T41;void*_T42;int*_T43;unsigned _T44;void*_T45;struct Cyc_Absyn_AppType_Absyn_Type_struct*_T46;void*_T47;struct Cyc_Absyn_AggrCon_Absyn_TyCon_struct*_T48;union Cyc_Absyn_AggrInfo _T49;struct _union_AggrInfo_UnknownAggr _T4A;unsigned _T4B;void*_T4C;void*_T4D;union Cyc_Absyn_AggrInfo _T4E;struct _union_AggrInfo_UnknownAggr _T4F;struct _tuple2 _T50;union Cyc_Absyn_AggrInfo _T51;struct _union_AggrInfo_UnknownAggr _T52;struct _tuple2 _T53;struct Cyc_List_List*(*_T54)(struct Cyc_Absyn_Tvar*(*)(unsigned,void*),unsigned,struct Cyc_List_List*);struct Cyc_List_List*(*_T55)(void*(*)(void*,void*),void*,struct Cyc_List_List*);unsigned _T56;struct Cyc_List_List*_T57;struct Cyc_Absyn_Aggrdecl*_T58;struct Cyc_Absyn_Aggrdecl*_T59;struct Cyc_Absyn_Aggrdecl*_T5A;struct Cyc_Absyn_Aggrdecl*_T5B;struct Cyc_Absyn_Aggrdecl*_T5C;struct Cyc_Absyn_Aggrdecl*_T5D;struct Cyc_Absyn_Aggrdecl*_T5E;unsigned _T5F;struct _fat_ptr _T60;struct _fat_ptr _T61;struct Cyc_Absyn_Aggr_d_Absyn_Raw_decl_struct*_T62;void*_T63;unsigned _T64;void*_T65;struct Cyc_Absyn_AppType_Absyn_Type_struct*_T66;void*_T67;struct Cyc_Absyn_DatatypeCon_Absyn_TyCon_struct*_T68;union Cyc_Absyn_DatatypeInfo _T69;struct _union_DatatypeInfo_KnownDatatype _T6A;unsigned _T6B;void*_T6C;void*_T6D;union Cyc_Absyn_DatatypeInfo _T6E;struct _union_DatatypeInfo_KnownDatatype _T6F;unsigned _T70;struct _fat_ptr _T71;struct _fat_ptr _T72;struct Cyc_Absyn_Datatype_d_Absyn_Raw_decl_struct*_T73;struct Cyc_Absyn_Datatypedecl**_T74;void*_T75;unsigned _T76;void*_T77;void*_T78;union Cyc_Absyn_DatatypeInfo _T79;struct _union_DatatypeInfo_UnknownDatatype _T7A;struct Cyc_Absyn_UnknownDatatypeInfo _T7B;union Cyc_Absyn_DatatypeInfo _T7C;struct _union_DatatypeInfo_UnknownDatatype _T7D;struct Cyc_Absyn_UnknownDatatypeInfo _T7E;struct Cyc_List_List*(*_T7F)(struct Cyc_Absyn_Tvar*(*)(unsigned,void*),unsigned,struct Cyc_List_List*);struct Cyc_List_List*(*_T80)(void*(*)(void*,void*),void*,struct Cyc_List_List*);unsigned _T81;struct Cyc_List_List*_T82;unsigned _T83;struct _fat_ptr _T84;struct _fat_ptr _T85;void*_T86;void*_T87;struct Cyc_Absyn_Enumdecl*_T88;struct Cyc_Absyn_Enumdecl*_T89;struct Cyc_Absyn_Enumdecl*_T8A;unsigned _T8B;struct _fat_ptr _T8C;struct _fat_ptr _T8D;struct Cyc_Absyn_Decl*_T8E;struct Cyc_Absyn_Enum_d_Absyn_Raw_decl_struct*_T8F;void*_T90;void*_T91;struct Cyc_Absyn_Enumdecl*_T92;struct Cyc_Absyn_Enumdecl*_T93;struct Cyc_Absyn_Enumdecl*_T94;struct Cyc_Core_Opt*_T95;unsigned _T96;struct _fat_ptr _T97;struct _fat_ptr _T98;struct Cyc_Absyn_Decl*_T99;struct Cyc_Absyn_Enum_d_Absyn_Raw_decl_struct*_T9A;unsigned _T9B;struct _fat_ptr _T9C;struct _fat_ptr _T9D;int _T9E;int _T9F;unsigned _TA0;struct _fat_ptr _TA1;struct _fat_ptr _TA2;struct Cyc_List_List*(*_TA3)(struct Cyc_Absyn_Decl*(*)(unsigned,struct _tuple15*),unsigned,struct Cyc_List_List*);struct Cyc_List_List*(*_TA4)(void*(*)(void*,void*),void*,struct Cyc_List_List*);unsigned _TA5;struct Cyc_List_List*_TA6;struct Cyc_List_List*_TA7;void*_TA8;unsigned _TA9;struct _fat_ptr _TAA;struct _fat_ptr _TAB;int(*_TAC)(unsigned,struct _fat_ptr);unsigned _TAD;struct _fat_ptr _TAE;int(*_TAF)(unsigned,struct _fat_ptr);unsigned _TB0;struct _fat_ptr _TB1;unsigned _TB2;struct _tuple0*_TB3;void*_TB4;struct Cyc_List_List*_TB5;void*_TB6;struct Cyc_Absyn_Exp*_TB7;struct Cyc_List_List*_TB8;void*_TB9;struct Cyc_Absyn_Exp*_TBA;struct Cyc_Absyn_Vardecl*_TBB;struct Cyc_Absyn_Vardecl*_TBC;struct Cyc_Absyn_Vardecl*_TBD;struct Cyc_List_List*_TBE;struct Cyc_Absyn_Decl*_TBF;struct Cyc_Absyn_Var_d_Absyn_Raw_decl_struct*_TC0;struct Cyc_List_List*_TC1;struct Cyc_List_List*_TC2;struct Cyc_List_List*_TC3;struct _RegionHandle _TC4=_new_region(0U,"mkrgn");struct _RegionHandle*mkrgn=& _TC4;_push_region(mkrgn);{struct Cyc_List_List*_TC5;struct Cyc_Parse_Type_specifier _TC6;struct Cyc_Absyn_Tqual _TC7;_T0=ds;_TC7=_T0.tq;_T1=ds;_TC6=_T1.type_specs;_T2=ds;_TC5=_T2.attributes;{struct Cyc_Absyn_Tqual tq=_TC7;struct Cyc_Parse_Type_specifier tss=_TC6;struct Cyc_List_List*atts=_TC5;_T3=tq;_T4=_T3.loc;
# 930
if(_T4!=0U)goto _TL155;tq.loc=tqual_loc;goto _TL156;_TL155: _TL156: _T5=ds;_T6=_T5.is_inline;
if(!_T6)goto _TL157;_T7=loc;_T8=
_tag_fat("inline qualifier on non-function definition",sizeof(char),44U);_T9=_tag_fat(0U,sizeof(void*),0);Cyc_Warn_warn(_T7,_T8,_T9);goto _TL158;_TL157: _TL158: {
# 934
enum Cyc_Absyn_Scope s=2U;
int istypedef=0;_TA=ds;{
enum Cyc_Parse_Storage_class _TC8=_TA.sc;_TB=(int)_TC8;switch(_TB){case Cyc_Parse_Typedef_sc:
 istypedef=1;goto _LL3;case Cyc_Parse_Extern_sc:
 s=3U;goto _LL3;case Cyc_Parse_ExternC_sc:
 s=4U;goto _LL3;case Cyc_Parse_Static_sc:
 s=0U;goto _LL3;case Cyc_Parse_Auto_sc:
 s=2U;goto _LL3;case Cyc_Parse_Register_sc: _TD=Cyc_Flags_no_register;
if(!_TD)goto _TL15A;_TC=2U;goto _TL15B;_TL15A: _TC=5U;_TL15B: s=_TC;goto _LL3;case Cyc_Parse_Abstract_sc:
 s=1U;goto _LL3;default: goto _LL3;}_LL3:;}{
# 950
struct _tuple13*declarators=0;
struct Cyc_List_List*exprs=0;
struct Cyc_List_List*renames=0;_TE=mkrgn;_TF=ids;_T10=& declarators;_T11=& exprs;_T12=& renames;
Cyc_Parse_decl_split(_TE,_TF,_T10,_T11,_T12);{
# 955
int exps_empty=1;{
struct Cyc_List_List*es=exprs;_TL15F: if(es!=0)goto _TL15D;else{goto _TL15E;}
_TL15D: _T13=es;_T14=_T13->hd;_T15=(struct Cyc_Absyn_Exp*)_T14;if(_T15==0)goto _TL160;
exps_empty=0;goto _TL15E;_TL160: _T16=es;
# 956
es=_T16->tl;goto _TL15F;_TL15E:;}{
# 963
void*base_type=Cyc_Parse_collapse_type_specifiers(tss,loc);
if(declarators!=0)goto _TL162;{int _TC8;struct Cyc_Absyn_Datatypedecl**_TC9;struct Cyc_List_List*_TCA;struct _tuple0*_TCB;enum Cyc_Absyn_AggrKind _TCC;struct Cyc_Absyn_Datatypedecl*_TCD;struct Cyc_Absyn_Enumdecl*_TCE;struct Cyc_Absyn_Aggrdecl*_TCF;_T17=base_type;_T18=(int*)_T17;_T19=*_T18;switch(_T19){case 10: _T1A=base_type;_T1B=(struct Cyc_Absyn_TypeDeclType_Absyn_Type_struct*)_T1A;_T1C=_T1B->f1;_T1D=(struct Cyc_Absyn_TypeDecl*)_T1C;_T1E=_T1D->r;_T1F=(int*)_T1E;_T20=*_T1F;switch(_T20){case 0: _T21=base_type;{struct Cyc_Absyn_TypeDeclType_Absyn_Type_struct*_TD0=(struct Cyc_Absyn_TypeDeclType_Absyn_Type_struct*)_T21;_T22=_TD0->f1;{struct Cyc_Absyn_TypeDecl _TD1=*_T22;_T23=_TD1.r;{struct Cyc_Absyn_Aggr_td_Absyn_Raw_typedecl_struct*_TD2=(struct Cyc_Absyn_Aggr_td_Absyn_Raw_typedecl_struct*)_T23;_TCF=_TD2->f1;}}}{struct Cyc_Absyn_Aggrdecl*ad=_TCF;_T24=ad;_T25=ad;_T26=_T25->attributes;_T27=atts;
# 969
_T24->attributes=Cyc_List_append(_T26,_T27);_T28=ad;
_T28->sc=s;{struct Cyc_List_List*_TD0;_TD0=_cycalloc(sizeof(struct Cyc_List_List));{struct Cyc_Absyn_Aggr_d_Absyn_Raw_decl_struct*_TD1=_cycalloc(sizeof(struct Cyc_Absyn_Aggr_d_Absyn_Raw_decl_struct));_TD1->tag=5;
_TD1->f1=ad;_T29=(struct Cyc_Absyn_Aggr_d_Absyn_Raw_decl_struct*)_TD1;}_T2A=(void*)_T29;_T2B=loc;_TD0->hd=Cyc_Absyn_new_decl(_T2A,_T2B);_TD0->tl=0;_npop_handler(0);return _TD0;}}case 1: _T2C=base_type;{struct Cyc_Absyn_TypeDeclType_Absyn_Type_struct*_TD0=(struct Cyc_Absyn_TypeDeclType_Absyn_Type_struct*)_T2C;_T2D=_TD0->f1;{struct Cyc_Absyn_TypeDecl _TD1=*_T2D;_T2E=_TD1.r;{struct Cyc_Absyn_Enum_td_Absyn_Raw_typedecl_struct*_TD2=(struct Cyc_Absyn_Enum_td_Absyn_Raw_typedecl_struct*)_T2E;_TCE=_TD2->f1;}}}{struct Cyc_Absyn_Enumdecl*ed=_TCE;
# 973
if(atts==0)goto _TL166;_T2F=loc;_T30=_tag_fat("attributes on enum not supported",sizeof(char),33U);_T31=_tag_fat(0U,sizeof(void*),0);Cyc_Warn_err(_T2F,_T30,_T31);goto _TL167;_TL166: _TL167: _T32=ed;
_T32->sc=s;{struct Cyc_List_List*_TD0;_TD0=_cycalloc(sizeof(struct Cyc_List_List));{struct Cyc_Absyn_Enum_d_Absyn_Raw_decl_struct*_TD1=_cycalloc(sizeof(struct Cyc_Absyn_Enum_d_Absyn_Raw_decl_struct));_TD1->tag=7;
_TD1->f1=ed;_T33=(struct Cyc_Absyn_Enum_d_Absyn_Raw_decl_struct*)_TD1;}_T34=(void*)_T33;_T35=loc;_TD0->hd=Cyc_Absyn_new_decl(_T34,_T35);_TD0->tl=0;_npop_handler(0);return _TD0;}}default: _T36=base_type;{struct Cyc_Absyn_TypeDeclType_Absyn_Type_struct*_TD0=(struct Cyc_Absyn_TypeDeclType_Absyn_Type_struct*)_T36;_T37=_TD0->f1;{struct Cyc_Absyn_TypeDecl _TD1=*_T37;_T38=_TD1.r;{struct Cyc_Absyn_Datatype_td_Absyn_Raw_typedecl_struct*_TD2=(struct Cyc_Absyn_Datatype_td_Absyn_Raw_typedecl_struct*)_T38;_TCD=_TD2->f1;}}}{struct Cyc_Absyn_Datatypedecl*dd=_TCD;
# 977
if(atts==0)goto _TL168;_T39=loc;_T3A=_tag_fat("attributes on datatypes not supported",sizeof(char),38U);_T3B=_tag_fat(0U,sizeof(void*),0);Cyc_Warn_err(_T39,_T3A,_T3B);goto _TL169;_TL168: _TL169: _T3C=dd;
_T3C->sc=s;{struct Cyc_List_List*_TD0;_TD0=_cycalloc(sizeof(struct Cyc_List_List));{struct Cyc_Absyn_Datatype_d_Absyn_Raw_decl_struct*_TD1=_cycalloc(sizeof(struct Cyc_Absyn_Datatype_d_Absyn_Raw_decl_struct));_TD1->tag=6;
_TD1->f1=dd;_T3D=(struct Cyc_Absyn_Datatype_d_Absyn_Raw_decl_struct*)_TD1;}_T3E=(void*)_T3D;_T3F=loc;_TD0->hd=Cyc_Absyn_new_decl(_T3E,_T3F);_TD0->tl=0;_npop_handler(0);return _TD0;}}};case 0: _T40=base_type;_T41=(struct Cyc_Absyn_AppType_Absyn_Type_struct*)_T40;_T42=_T41->f1;_T43=(int*)_T42;_T44=*_T43;switch(_T44){case 24: _T45=base_type;_T46=(struct Cyc_Absyn_AppType_Absyn_Type_struct*)_T45;_T47=_T46->f1;_T48=(struct Cyc_Absyn_AggrCon_Absyn_TyCon_struct*)_T47;_T49=_T48->f1;_T4A=_T49.UnknownAggr;_T4B=_T4A.tag;if(_T4B!=1)goto _TL16B;_T4C=base_type;{struct Cyc_Absyn_AppType_Absyn_Type_struct*_TD0=(struct Cyc_Absyn_AppType_Absyn_Type_struct*)_T4C;_T4D=_TD0->f1;{struct Cyc_Absyn_AggrCon_Absyn_TyCon_struct*_TD1=(struct Cyc_Absyn_AggrCon_Absyn_TyCon_struct*)_T4D;_T4E=_TD1->f1;_T4F=_T4E.UnknownAggr;_T50=_T4F.val;_TCC=_T50.f0;_T51=_TD1->f1;_T52=_T51.UnknownAggr;_T53=_T52.val;_TCB=_T53.f1;}_TCA=_TD0->f2;}{enum Cyc_Absyn_AggrKind k=_TCC;struct _tuple0*n=_TCB;struct Cyc_List_List*ts=_TCA;_T55=Cyc_List_map_c;{
# 981
struct Cyc_List_List*(*_TD0)(struct Cyc_Absyn_Tvar*(*)(unsigned,void*),unsigned,struct Cyc_List_List*)=(struct Cyc_List_List*(*)(struct Cyc_Absyn_Tvar*(*)(unsigned,void*),unsigned,struct Cyc_List_List*))_T55;_T54=_TD0;}_T56=loc;_T57=ts;{struct Cyc_List_List*ts2=_T54(Cyc_Parse_typ2tvar,_T56,_T57);
struct Cyc_Absyn_Aggrdecl*ad;ad=_cycalloc(sizeof(struct Cyc_Absyn_Aggrdecl));_T58=ad;_T58->kind=k;_T59=ad;_T59->sc=s;_T5A=ad;_T5A->name=n;_T5B=ad;_T5B->tvs=ts2;_T5C=ad;_T5C->impl=0;_T5D=ad;_T5D->attributes=0;_T5E=ad;_T5E->expected_mem_kind=0;
if(atts==0)goto _TL16D;_T5F=loc;_T60=_tag_fat("bad attributes on type declaration",sizeof(char),35U);_T61=_tag_fat(0U,sizeof(void*),0);Cyc_Warn_err(_T5F,_T60,_T61);goto _TL16E;_TL16D: _TL16E: {struct Cyc_List_List*_TD0;_TD0=_cycalloc(sizeof(struct Cyc_List_List));{struct Cyc_Absyn_Aggr_d_Absyn_Raw_decl_struct*_TD1=_cycalloc(sizeof(struct Cyc_Absyn_Aggr_d_Absyn_Raw_decl_struct));_TD1->tag=5;
_TD1->f1=ad;_T62=(struct Cyc_Absyn_Aggr_d_Absyn_Raw_decl_struct*)_TD1;}_T63=(void*)_T62;_T64=loc;_TD0->hd=Cyc_Absyn_new_decl(_T63,_T64);_TD0->tl=0;_npop_handler(0);return _TD0;}}}_TL16B: goto _LL25;case 22: _T65=base_type;_T66=(struct Cyc_Absyn_AppType_Absyn_Type_struct*)_T65;_T67=_T66->f1;_T68=(struct Cyc_Absyn_DatatypeCon_Absyn_TyCon_struct*)_T67;_T69=_T68->f1;_T6A=_T69.KnownDatatype;_T6B=_T6A.tag;if(_T6B!=2)goto _TL16F;_T6C=base_type;{struct Cyc_Absyn_AppType_Absyn_Type_struct*_TD0=(struct Cyc_Absyn_AppType_Absyn_Type_struct*)_T6C;_T6D=_TD0->f1;{struct Cyc_Absyn_DatatypeCon_Absyn_TyCon_struct*_TD1=(struct Cyc_Absyn_DatatypeCon_Absyn_TyCon_struct*)_T6D;_T6E=_TD1->f1;_T6F=_T6E.KnownDatatype;_TC9=_T6F.val;}}{struct Cyc_Absyn_Datatypedecl**tudp=_TC9;
# 986
if(atts==0)goto _TL171;_T70=loc;_T71=_tag_fat("bad attributes on datatype",sizeof(char),27U);_T72=_tag_fat(0U,sizeof(void*),0);Cyc_Warn_err(_T70,_T71,_T72);goto _TL172;_TL171: _TL172: {struct Cyc_List_List*_TD0;_TD0=_cycalloc(sizeof(struct Cyc_List_List));{struct Cyc_Absyn_Datatype_d_Absyn_Raw_decl_struct*_TD1=_cycalloc(sizeof(struct Cyc_Absyn_Datatype_d_Absyn_Raw_decl_struct));_TD1->tag=6;_T74=tudp;
_TD1->f1=*_T74;_T73=(struct Cyc_Absyn_Datatype_d_Absyn_Raw_decl_struct*)_TD1;}_T75=(void*)_T73;_T76=loc;_TD0->hd=Cyc_Absyn_new_decl(_T75,_T76);_TD0->tl=0;_npop_handler(0);return _TD0;}}_TL16F: _T77=base_type;{struct Cyc_Absyn_AppType_Absyn_Type_struct*_TD0=(struct Cyc_Absyn_AppType_Absyn_Type_struct*)_T77;_T78=_TD0->f1;{struct Cyc_Absyn_DatatypeCon_Absyn_TyCon_struct*_TD1=(struct Cyc_Absyn_DatatypeCon_Absyn_TyCon_struct*)_T78;_T79=_TD1->f1;_T7A=_T79.UnknownDatatype;_T7B=_T7A.val;_TCB=_T7B.name;_T7C=_TD1->f1;_T7D=_T7C.UnknownDatatype;_T7E=_T7D.val;_TC8=_T7E.is_extensible;}_TCA=_TD0->f2;}{struct _tuple0*n=_TCB;int isx=_TC8;struct Cyc_List_List*ts=_TCA;_T80=Cyc_List_map_c;{
# 989
struct Cyc_List_List*(*_TD0)(struct Cyc_Absyn_Tvar*(*)(unsigned,void*),unsigned,struct Cyc_List_List*)=(struct Cyc_List_List*(*)(struct Cyc_Absyn_Tvar*(*)(unsigned,void*),unsigned,struct Cyc_List_List*))_T80;_T7F=_TD0;}_T81=loc;_T82=ts;{struct Cyc_List_List*ts2=_T7F(Cyc_Parse_typ2tvar,_T81,_T82);
struct Cyc_Absyn_Decl*tud=Cyc_Absyn_datatype_decl(s,n,ts2,0,isx,loc);
if(atts==0)goto _TL173;_T83=loc;_T84=_tag_fat("bad attributes on datatype",sizeof(char),27U);_T85=_tag_fat(0U,sizeof(void*),0);Cyc_Warn_err(_T83,_T84,_T85);goto _TL174;_TL173: _TL174: {struct Cyc_List_List*_TD0;_TD0=_cycalloc(sizeof(struct Cyc_List_List));
_TD0->hd=tud;_TD0->tl=0;_npop_handler(0);return _TD0;}}}case 19: _T86=base_type;{struct Cyc_Absyn_AppType_Absyn_Type_struct*_TD0=(struct Cyc_Absyn_AppType_Absyn_Type_struct*)_T86;_T87=_TD0->f1;{struct Cyc_Absyn_EnumCon_Absyn_TyCon_struct*_TD1=(struct Cyc_Absyn_EnumCon_Absyn_TyCon_struct*)_T87;_TCB=_TD1->f1;}}{struct _tuple0*n=_TCB;
# 994
struct Cyc_Absyn_Enumdecl*ed;ed=_cycalloc(sizeof(struct Cyc_Absyn_Enumdecl));_T88=ed;_T88->sc=s;_T89=ed;_T89->name=n;_T8A=ed;_T8A->fields=0;
if(atts==0)goto _TL175;_T8B=loc;_T8C=_tag_fat("bad attributes on enum",sizeof(char),23U);_T8D=_tag_fat(0U,sizeof(void*),0);Cyc_Warn_err(_T8B,_T8C,_T8D);goto _TL176;_TL175: _TL176: {struct Cyc_List_List*_TD0;_TD0=_cycalloc(sizeof(struct Cyc_List_List));{struct Cyc_Absyn_Decl*_TD1=_cycalloc(sizeof(struct Cyc_Absyn_Decl));{struct Cyc_Absyn_Enum_d_Absyn_Raw_decl_struct*_TD2=_cycalloc(sizeof(struct Cyc_Absyn_Enum_d_Absyn_Raw_decl_struct));_TD2->tag=7;
_TD2->f1=ed;_T8F=(struct Cyc_Absyn_Enum_d_Absyn_Raw_decl_struct*)_TD2;}_TD1->r=(void*)_T8F;_TD1->loc=loc;_T8E=(struct Cyc_Absyn_Decl*)_TD1;}_TD0->hd=_T8E;_TD0->tl=0;_npop_handler(0);return _TD0;}}case 20: _T90=base_type;{struct Cyc_Absyn_AppType_Absyn_Type_struct*_TD0=(struct Cyc_Absyn_AppType_Absyn_Type_struct*)_T90;_T91=_TD0->f1;{struct Cyc_Absyn_AnonEnumCon_Absyn_TyCon_struct*_TD1=(struct Cyc_Absyn_AnonEnumCon_Absyn_TyCon_struct*)_T91;_TCA=_TD1->f1;}}{struct Cyc_List_List*fs=_TCA;
# 1000
struct Cyc_Absyn_Enumdecl*ed;ed=_cycalloc(sizeof(struct Cyc_Absyn_Enumdecl));_T92=ed;_T92->sc=s;_T93=ed;_T93->name=Cyc_Parse_gensym_enum();_T94=ed;{struct Cyc_Core_Opt*_TD0=_cycalloc(sizeof(struct Cyc_Core_Opt));_TD0->v=fs;_T95=(struct Cyc_Core_Opt*)_TD0;}_T94->fields=_T95;
if(atts==0)goto _TL177;_T96=loc;_T97=_tag_fat("bad attributes on enum",sizeof(char),23U);_T98=_tag_fat(0U,sizeof(void*),0);Cyc_Warn_err(_T96,_T97,_T98);goto _TL178;_TL177: _TL178: {struct Cyc_List_List*_TD0;_TD0=_cycalloc(sizeof(struct Cyc_List_List));{struct Cyc_Absyn_Decl*_TD1=_cycalloc(sizeof(struct Cyc_Absyn_Decl));{struct Cyc_Absyn_Enum_d_Absyn_Raw_decl_struct*_TD2=_cycalloc(sizeof(struct Cyc_Absyn_Enum_d_Absyn_Raw_decl_struct));_TD2->tag=7;
_TD2->f1=ed;_T9A=(struct Cyc_Absyn_Enum_d_Absyn_Raw_decl_struct*)_TD2;}_TD1->r=(void*)_T9A;_TD1->loc=loc;_T99=(struct Cyc_Absyn_Decl*)_TD1;}_TD0->hd=_T99;_TD0->tl=0;_npop_handler(0);return _TD0;}}default: goto _LL25;};default: _LL25: _T9B=loc;_T9C=
_tag_fat("missing declarator",sizeof(char),19U);_T9D=_tag_fat(0U,sizeof(void*),0);Cyc_Warn_err(_T9B,_T9C,_T9D);{struct Cyc_List_List*_TD0=0;_npop_handler(0);return _TD0;}};}goto _TL163;_TL162: _TL163: {
# 1007
struct Cyc_List_List*fields=Cyc_Parse_apply_tmss(mkrgn,tq,base_type,declarators,atts);_T9E=istypedef;
if(!_T9E)goto _TL179;_T9F=exps_empty;
# 1012
if(_T9F)goto _TL17B;else{goto _TL17D;}
_TL17D: _TA0=loc;_TA1=_tag_fat("initializer in typedef declaration",sizeof(char),35U);_TA2=_tag_fat(0U,sizeof(void*),0);Cyc_Warn_err(_TA0,_TA1,_TA2);goto _TL17C;_TL17B: _TL17C: _TA4=Cyc_List_map_c;{
struct Cyc_List_List*(*_TC8)(struct Cyc_Absyn_Decl*(*)(unsigned,struct _tuple15*),unsigned,struct Cyc_List_List*)=(struct Cyc_List_List*(*)(struct Cyc_Absyn_Decl*(*)(unsigned,struct _tuple15*),unsigned,struct Cyc_List_List*))_TA4;_TA3=_TC8;}_TA5=loc;_TA6=fields;{struct Cyc_List_List*decls=_TA3(Cyc_Parse_v_typ_to_typedef,_TA5,_TA6);struct Cyc_List_List*_TC8=decls;_npop_handler(0);return _TC8;}_TL179: {
# 1018
struct Cyc_List_List*decls=0;{
struct Cyc_List_List*ds=fields;
_TL181:
# 1019
 if(ds!=0)goto _TL17F;else{goto _TL180;}
# 1022
_TL17F: _TA7=ds;_TA8=_TA7->hd;{struct _tuple15*_TC8=(struct _tuple15*)_TA8;struct Cyc_List_List*_TC9;struct Cyc_List_List*_TCA;void*_TCB;struct Cyc_Absyn_Tqual _TCC;struct _tuple0*_TCD;unsigned _TCE;{struct _tuple15 _TCF=*_TC8;_TCE=_TCF.f0;_TCD=_TCF.f1;_TCC=_TCF.f2;_TCB=_TCF.f3;_TCA=_TCF.f4;_TC9=_TCF.f5;}{unsigned varloc=_TCE;struct _tuple0*x=_TCD;struct Cyc_Absyn_Tqual tq2=_TCC;void*t2=_TCB;struct Cyc_List_List*tvs2=_TCA;struct Cyc_List_List*atts2=_TC9;
if(tvs2==0)goto _TL182;_TA9=loc;_TAA=
_tag_fat("bad type params, ignoring",sizeof(char),26U);_TAB=_tag_fat(0U,sizeof(void*),0);Cyc_Warn_warn(_TA9,_TAA,_TAB);goto _TL183;_TL182: _TL183:
 if(exprs!=0)goto _TL184;{
int(*_TCF)(unsigned,struct _fat_ptr)=(int(*)(unsigned,struct _fat_ptr))Cyc_Parse_parse_abort;_TAC=_TCF;}_TAD=loc;_TAE=_tag_fat("unexpected NULL in parse!",sizeof(char),26U);_TAC(_TAD,_TAE);goto _TL185;_TL184: _TL185:
 if(renames!=0)goto _TL186;{
int(*_TCF)(unsigned,struct _fat_ptr)=(int(*)(unsigned,struct _fat_ptr))Cyc_Parse_parse_abort;_TAF=_TCF;}_TB0=loc;_TB1=_tag_fat("unexpected NULL in parse!",sizeof(char),26U);_TAF(_TB0,_TB1);goto _TL187;_TL186: _TL187: _TB2=varloc;_TB3=x;_TB4=t2;_TB5=exprs;_TB6=_TB5->hd;_TB7=(struct Cyc_Absyn_Exp*)_TB6;_TB8=renames;_TB9=_TB8->hd;_TBA=(struct Cyc_Absyn_Exp*)_TB9;{
struct Cyc_Absyn_Vardecl*vd=Cyc_Absyn_new_vardecl(_TB2,_TB3,_TB4,_TB7,_TBA);_TBB=vd;
_TBB->tq=tq2;_TBC=vd;
_TBC->sc=s;_TBD=vd;
_TBD->attributes=atts2;{struct Cyc_List_List*_TCF=_cycalloc(sizeof(struct Cyc_List_List));{struct Cyc_Absyn_Decl*_TD0=_cycalloc(sizeof(struct Cyc_Absyn_Decl));{struct Cyc_Absyn_Var_d_Absyn_Raw_decl_struct*_TD1=_cycalloc(sizeof(struct Cyc_Absyn_Var_d_Absyn_Raw_decl_struct));_TD1->tag=0;
_TD1->f1=vd;_TC0=(struct Cyc_Absyn_Var_d_Absyn_Raw_decl_struct*)_TD1;}_TD0->r=(void*)_TC0;_TD0->loc=loc;_TBF=(struct Cyc_Absyn_Decl*)_TD0;}_TCF->hd=_TBF;_TCF->tl=decls;_TBE=(struct Cyc_List_List*)_TCF;}decls=_TBE;}}}_TC1=ds;
# 1021
ds=_TC1->tl;_TC2=_check_null(exprs);exprs=_TC2->tl;_TC3=_check_null(renames);renames=_TC3->tl;goto _TL181;_TL180:;}{struct Cyc_List_List*_TC8=
# 1035
Cyc_List_imp_rev(decls);_npop_handler(0);return _TC8;}}}}}}}}}_pop_region();}
# 1039
static unsigned Cyc_Parse_cnst2uint(unsigned loc,union Cyc_Absyn_Cnst x){union Cyc_Absyn_Cnst _T0;struct _union_Cnst_LongLong_c _T1;unsigned _T2;union Cyc_Absyn_Cnst _T3;struct _union_Cnst_Int_c _T4;struct _tuple5 _T5;int _T6;unsigned _T7;union Cyc_Absyn_Cnst _T8;struct _union_Cnst_Char_c _T9;struct _tuple3 _TA;char _TB;unsigned _TC;union Cyc_Absyn_Cnst _TD;struct _union_Cnst_LongLong_c _TE;struct _tuple6 _TF;long long _T10;unsigned _T11;struct _fat_ptr _T12;struct _fat_ptr _T13;long long _T14;unsigned _T15;struct Cyc_String_pa_PrintArg_struct _T16;unsigned _T17;struct _fat_ptr _T18;struct _fat_ptr _T19;long long _T1A;char _T1B;int _T1C;_T0=x;_T1=_T0.LongLong_c;_T2=_T1.tag;switch(_T2){case 5: _T3=x;_T4=_T3.Int_c;_T5=_T4.val;_T1C=_T5.f1;{int i=_T1C;_T6=i;_T7=(unsigned)_T6;
# 1041
return _T7;}case 2: _T8=x;_T9=_T8.Char_c;_TA=_T9.val;_T1B=_TA.f1;{char c=_T1B;_TB=c;_TC=(unsigned)_TB;
return _TC;}case 6: _TD=x;_TE=_TD.LongLong_c;_TF=_TE.val;_T1A=_TF.f1;{long long x=_T1A;_T10=x;{
# 1044
unsigned long long y=(unsigned long long)_T10;
if(y <= 4294967295U)goto _TL189;_T11=loc;_T12=
_tag_fat("integer constant too large",sizeof(char),27U);_T13=_tag_fat(0U,sizeof(void*),0);Cyc_Warn_err(_T11,_T12,_T13);goto _TL18A;_TL189: _TL18A: _T14=x;_T15=(unsigned)_T14;
return _T15;}}default:{struct Cyc_String_pa_PrintArg_struct _T1D;_T1D.tag=0;
# 1049
_T1D.f1=Cyc_Absynpp_cnst2string(x);_T16=_T1D;}{struct Cyc_String_pa_PrintArg_struct _T1D=_T16;void*_T1E[1];_T1E[0]=& _T1D;_T17=loc;_T18=_tag_fat("expected integer constant but found %s",sizeof(char),39U);_T19=_tag_fat(_T1E,sizeof(void*),1);Cyc_Warn_err(_T17,_T18,_T19);}
return 0U;};}
# 1055
static struct Cyc_Absyn_Exp*Cyc_Parse_pat2exp(struct Cyc_Absyn_Pat*p){struct Cyc_Absyn_Pat*_T0;int*_T1;unsigned _T2;struct _tuple0*_T3;struct Cyc_Absyn_Pat*_T4;unsigned _T5;struct Cyc_Absyn_Exp*_T6;struct Cyc_Absyn_Reference_p_Absyn_Raw_pat_struct*_T7;struct Cyc_Absyn_Pat*_T8;struct Cyc_Absyn_Pat*_T9;void*_TA;int*_TB;int _TC;struct Cyc_Absyn_Vardecl*_TD;struct _tuple0*_TE;struct Cyc_Absyn_Pat*_TF;unsigned _T10;struct Cyc_Absyn_Exp*_T11;struct Cyc_Absyn_Pat*_T12;unsigned _T13;struct Cyc_Absyn_Exp*_T14;struct Cyc_Absyn_Exp*_T15;struct Cyc_Absyn_Pat*_T16;unsigned _T17;struct Cyc_Absyn_Exp*_T18;struct Cyc_Absyn_Pat*_T19;unsigned _T1A;struct Cyc_Absyn_Exp*_T1B;enum Cyc_Absyn_Sign _T1C;int _T1D;struct Cyc_Absyn_Pat*_T1E;unsigned _T1F;struct Cyc_Absyn_Exp*_T20;char _T21;struct Cyc_Absyn_Pat*_T22;unsigned _T23;struct Cyc_Absyn_Exp*_T24;struct _fat_ptr _T25;int _T26;struct Cyc_Absyn_Pat*_T27;unsigned _T28;struct Cyc_Absyn_Exp*_T29;struct Cyc_Absyn_UnknownCall_p_Absyn_Raw_pat_struct*_T2A;int _T2B;struct _tuple0*_T2C;struct Cyc_Absyn_Pat*_T2D;unsigned _T2E;struct Cyc_List_List*(*_T2F)(struct Cyc_Absyn_Exp*(*)(struct Cyc_Absyn_Pat*),struct Cyc_List_List*);struct Cyc_List_List*(*_T30)(void*(*)(void*),struct Cyc_List_List*);struct Cyc_List_List*_T31;struct Cyc_Absyn_Exp*_T32;struct Cyc_List_List*_T33;struct Cyc_Absyn_Pat*_T34;unsigned _T35;struct Cyc_Absyn_Exp*_T36;struct Cyc_Absyn_Exp*_T37;struct Cyc_Absyn_Pat*_T38;unsigned _T39;struct _fat_ptr _T3A;struct _fat_ptr _T3B;struct Cyc_Absyn_Pat*_T3C;unsigned _T3D;struct Cyc_Absyn_Exp*_T3E;_T0=p;{
void*_T3F=_T0->r;struct Cyc_Absyn_Exp*_T40;struct Cyc_List_List*_T41;struct _fat_ptr _T42;char _T43;int _T44;enum Cyc_Absyn_Sign _T45;struct Cyc_Absyn_Pat*_T46;struct Cyc_Absyn_Vardecl*_T47;struct _tuple0*_T48;_T1=(int*)_T3F;_T2=*_T1;switch(_T2){case 14:{struct Cyc_Absyn_UnknownId_p_Absyn_Raw_pat_struct*_T49=(struct Cyc_Absyn_UnknownId_p_Absyn_Raw_pat_struct*)_T3F;_T48=_T49->f1;}{struct _tuple0*x=_T48;_T3=x;_T4=p;_T5=_T4->loc;_T6=
Cyc_Absyn_unknownid_exp(_T3,_T5);return _T6;}case 3: _T7=(struct Cyc_Absyn_Reference_p_Absyn_Raw_pat_struct*)_T3F;_T8=_T7->f2;_T9=(struct Cyc_Absyn_Pat*)_T8;_TA=_T9->r;_TB=(int*)_TA;_TC=*_TB;if(_TC!=0)goto _TL18C;{struct Cyc_Absyn_Reference_p_Absyn_Raw_pat_struct*_T49=(struct Cyc_Absyn_Reference_p_Absyn_Raw_pat_struct*)_T3F;_T47=_T49->f1;}{struct Cyc_Absyn_Vardecl*vd=_T47;_TD=vd;_TE=_TD->name;_TF=p;_T10=_TF->loc;_T11=
# 1059
Cyc_Absyn_unknownid_exp(_TE,_T10);_T12=p;_T13=_T12->loc;_T14=Cyc_Absyn_deref_exp(_T11,_T13);return _T14;}_TL18C: goto _LL13;case 5:{struct Cyc_Absyn_Pointer_p_Absyn_Raw_pat_struct*_T49=(struct Cyc_Absyn_Pointer_p_Absyn_Raw_pat_struct*)_T3F;_T46=_T49->f1;}{struct Cyc_Absyn_Pat*p2=_T46;_T15=
Cyc_Parse_pat2exp(p2);_T16=p;_T17=_T16->loc;_T18=Cyc_Absyn_address_exp(_T15,_T17);return _T18;}case 8: _T19=p;_T1A=_T19->loc;_T1B=
Cyc_Absyn_null_exp(_T1A);return _T1B;case 9:{struct Cyc_Absyn_Int_p_Absyn_Raw_pat_struct*_T49=(struct Cyc_Absyn_Int_p_Absyn_Raw_pat_struct*)_T3F;_T45=_T49->f1;_T44=_T49->f2;}{enum Cyc_Absyn_Sign s=_T45;int i=_T44;_T1C=s;_T1D=i;_T1E=p;_T1F=_T1E->loc;_T20=
Cyc_Absyn_int_exp(_T1C,_T1D,_T1F);return _T20;}case 10:{struct Cyc_Absyn_Char_p_Absyn_Raw_pat_struct*_T49=(struct Cyc_Absyn_Char_p_Absyn_Raw_pat_struct*)_T3F;_T43=_T49->f1;}{char c=_T43;_T21=c;_T22=p;_T23=_T22->loc;_T24=
Cyc_Absyn_char_exp(_T21,_T23);return _T24;}case 11:{struct Cyc_Absyn_Float_p_Absyn_Raw_pat_struct*_T49=(struct Cyc_Absyn_Float_p_Absyn_Raw_pat_struct*)_T3F;_T42=_T49->f1;_T44=_T49->f2;}{struct _fat_ptr s=_T42;int i=_T44;_T25=s;_T26=i;_T27=p;_T28=_T27->loc;_T29=
Cyc_Absyn_float_exp(_T25,_T26,_T28);return _T29;}case 15: _T2A=(struct Cyc_Absyn_UnknownCall_p_Absyn_Raw_pat_struct*)_T3F;_T2B=_T2A->f3;if(_T2B!=0)goto _TL18E;{struct Cyc_Absyn_UnknownCall_p_Absyn_Raw_pat_struct*_T49=(struct Cyc_Absyn_UnknownCall_p_Absyn_Raw_pat_struct*)_T3F;_T48=_T49->f1;_T41=_T49->f2;}{struct _tuple0*x=_T48;struct Cyc_List_List*ps=_T41;_T2C=x;_T2D=p;_T2E=_T2D->loc;{
# 1066
struct Cyc_Absyn_Exp*e1=Cyc_Absyn_unknownid_exp(_T2C,_T2E);_T30=Cyc_List_map;{
struct Cyc_List_List*(*_T49)(struct Cyc_Absyn_Exp*(*)(struct Cyc_Absyn_Pat*),struct Cyc_List_List*)=(struct Cyc_List_List*(*)(struct Cyc_Absyn_Exp*(*)(struct Cyc_Absyn_Pat*),struct Cyc_List_List*))_T30;_T2F=_T49;}_T31=ps;{struct Cyc_List_List*es=_T2F(Cyc_Parse_pat2exp,_T31);_T32=e1;_T33=es;_T34=p;_T35=_T34->loc;_T36=
Cyc_Absyn_unknowncall_exp(_T32,_T33,_T35);return _T36;}}}_TL18E: goto _LL13;case 16:{struct Cyc_Absyn_Exp_p_Absyn_Raw_pat_struct*_T49=(struct Cyc_Absyn_Exp_p_Absyn_Raw_pat_struct*)_T3F;_T40=_T49->f1;}{struct Cyc_Absyn_Exp*e=_T40;_T37=e;
return _T37;}default: _LL13: _T38=p;_T39=_T38->loc;_T3A=
# 1071
_tag_fat("cannot mix patterns and expressions in case",sizeof(char),44U);_T3B=_tag_fat(0U,sizeof(void*),0);Cyc_Warn_err(_T39,_T3A,_T3B);_T3C=p;_T3D=_T3C->loc;_T3E=
Cyc_Absyn_null_exp(_T3D);return _T3E;};}}
# 1075
static struct _tuple16 Cyc_Parse_split_seq(struct Cyc_Absyn_Exp*maybe_seq){struct Cyc_Absyn_Exp*_T0;int*_T1;int _T2;struct _tuple16 _T3;struct _tuple16 _T4;_T0=maybe_seq;{
void*_T5=_T0->r;struct Cyc_Absyn_Exp*_T6;struct Cyc_Absyn_Exp*_T7;_T1=(int*)_T5;_T2=*_T1;if(_T2!=9)goto _TL190;{struct Cyc_Absyn_SeqExp_e_Absyn_Raw_exp_struct*_T8=(struct Cyc_Absyn_SeqExp_e_Absyn_Raw_exp_struct*)_T5;_T7=_T8->f1;_T6=_T8->f2;}{struct Cyc_Absyn_Exp*e1=_T7;struct Cyc_Absyn_Exp*e2=_T6;{struct _tuple16 _T8;
# 1078
_T8.f0=e1;_T8.f1=e2;_T3=_T8;}return _T3;}_TL190:{struct _tuple16 _T8;
# 1080
_T8.f0=maybe_seq;_T8.f1=0;_T4=_T8;}return _T4;;}}
# 1083
static struct Cyc_Absyn_Exp*Cyc_Parse_join_assn(struct Cyc_Absyn_Exp*e1,struct Cyc_Absyn_Exp*e2){struct Cyc_Absyn_Exp*_T0;struct Cyc_Absyn_Exp*_T1;struct Cyc_Absyn_Exp*_T2;
if(e1==0)goto _TL192;if(e2==0)goto _TL192;_T0=Cyc_Absyn_and_exp(e1,e2,0U);return _T0;
_TL192: if(e1==0)goto _TL194;_T1=e1;return _T1;
_TL194: _T2=e2;return _T2;}struct _tuple21{struct Cyc_Absyn_Exp*f0;struct Cyc_Absyn_Exp*f1;struct Cyc_Absyn_Exp*f2;struct Cyc_Absyn_Exp*f3;};
# 1088
static struct _tuple21 Cyc_Parse_join_assns(struct _tuple21 a1,struct _tuple21 a2){struct _tuple21 _T0;struct _tuple21 _T1;struct _tuple21 _T2;struct _tuple21 _T3;struct _tuple21 _T4;struct _tuple21 _T5;struct _tuple21 _T6;struct _tuple21 _T7;struct _tuple21 _T8;struct Cyc_Absyn_Exp*_T9;struct Cyc_Absyn_Exp*_TA;struct Cyc_Absyn_Exp*_TB;struct Cyc_Absyn_Exp*_TC;_T0=a1;_TC=_T0.f0;_T1=a1;_TB=_T1.f1;_T2=a1;_TA=_T2.f2;_T3=a1;_T9=_T3.f3;{struct Cyc_Absyn_Exp*c1=_TC;struct Cyc_Absyn_Exp*r1=_TB;struct Cyc_Absyn_Exp*e1=_TA;struct Cyc_Absyn_Exp*t1=_T9;struct Cyc_Absyn_Exp*_TD;struct Cyc_Absyn_Exp*_TE;struct Cyc_Absyn_Exp*_TF;struct Cyc_Absyn_Exp*_T10;_T4=a2;_T10=_T4.f0;_T5=a2;_TF=_T5.f1;_T6=a2;_TE=_T6.f2;_T7=a2;_TD=_T7.f3;{struct Cyc_Absyn_Exp*c2=_T10;struct Cyc_Absyn_Exp*r2=_TF;struct Cyc_Absyn_Exp*e2=_TE;struct Cyc_Absyn_Exp*t2=_TD;
# 1091
struct Cyc_Absyn_Exp*c=Cyc_Parse_join_assn(c1,c2);
struct Cyc_Absyn_Exp*r=Cyc_Parse_join_assn(r1,r2);
struct Cyc_Absyn_Exp*e=Cyc_Parse_join_assn(e1,e2);
struct Cyc_Absyn_Exp*t=Cyc_Parse_join_assn(t1,t2);{struct _tuple21 _T11;
_T11.f0=c;_T11.f1=r;_T11.f2=e;_T11.f3=t;_T8=_T11;}return _T8;}}}
# 1098
static void*Cyc_Parse_assign_cvar_pos(struct _fat_ptr posstr,int ovfat,void*cv){void*_T0;int*_T1;int _T2;void*_T3;void*_T4;struct Cyc_Absyn_Cvar_Absyn_Type_struct*_T5;const char**_T6;void*_T7;struct Cyc_Absyn_Cvar_Absyn_Type_struct*_T8;int*_T9;const char**_TA;struct _fat_ptr _TB;unsigned char*_TC;int*_TD;void*_TE;void*_TF;void*_T10;void*_T11;_T0=cv;_T1=(int*)_T0;_T2=*_T1;if(_T2!=3)goto _TL196;_T3=cv;{struct Cyc_Absyn_Cvar_Absyn_Type_struct*_T12=(struct Cyc_Absyn_Cvar_Absyn_Type_struct*)_T3;_T4=cv;_T5=(struct Cyc_Absyn_Cvar_Absyn_Type_struct*)_T4;_T6=& _T5->f6;_T11=(const char**)_T6;_T7=cv;_T8=(struct Cyc_Absyn_Cvar_Absyn_Type_struct*)_T7;_T9=& _T8->f7;_T10=(int*)_T9;}{const char**pos=(const char**)_T11;int*ov=(int*)_T10;_TA=pos;_TB=posstr;_TC=_untag_fat_ptr_check_bound(_TB,sizeof(char),1U);
# 1101
*_TA=(const char*)_TC;_TD=ov;
*_TD=ovfat;_TE=cv;
return _TE;}_TL196: _TF=cv;
# 1105
return _TF;;}
# 1109
static void*Cyc_Parse_typevar2cvar(struct _fat_ptr s){struct Cyc_Hashtable_Table*_T0;unsigned _T1;struct Cyc_Hashtable_Table*(*_T2)(int,int(*)(struct _fat_ptr*,struct _fat_ptr*),int(*)(struct _fat_ptr*));struct Cyc_Hashtable_Table*(*_T3)(int,int(*)(void*,void*),int(*)(void*));int(*_T4)(struct _fat_ptr*,struct _fat_ptr*);int(*_T5)(struct _fat_ptr*);struct _handler_cons*_T6;int _T7;void*(*_T8)(struct Cyc_Hashtable_Table*,struct _fat_ptr*);void*(*_T9)(struct Cyc_Hashtable_Table*,void*);struct Cyc_Hashtable_Table*_TA;struct _fat_ptr*_TB;void*_TC;struct Cyc_Core_Not_found_exn_struct*_TD;char*_TE;char*_TF;struct _fat_ptr _T10;struct _fat_ptr _T11;struct _fat_ptr*_T12;struct _fat_ptr _T13;struct _fat_ptr _T14;int _T15;struct Cyc_Core_Opt*_T16;struct Cyc_Core_Opt*_T17;struct _fat_ptr _T18;void(*_T19)(struct Cyc_Hashtable_Table*,struct _fat_ptr*,void*);void(*_T1A)(struct Cyc_Hashtable_Table*,void*,void*);struct Cyc_Hashtable_Table*_T1B;struct _fat_ptr*_T1C;void*_T1D;void*_T1E;int(*_T1F)(unsigned,struct _fat_ptr);struct _fat_ptr _T20;
# 1112
static struct Cyc_Hashtable_Table*cvmap=0;_T0=cvmap;_T1=(unsigned)_T0;
if(_T1)goto _TL198;else{goto _TL19A;}
_TL19A: _T3=Cyc_Hashtable_create;{struct Cyc_Hashtable_Table*(*_T21)(int,int(*)(struct _fat_ptr*,struct _fat_ptr*),int(*)(struct _fat_ptr*))=(struct Cyc_Hashtable_Table*(*)(int,int(*)(struct _fat_ptr*,struct _fat_ptr*),int(*)(struct _fat_ptr*)))_T3;_T2=_T21;}_T4=Cyc_strptrcmp;_T5=Cyc_Hashtable_hash_stringptr;cvmap=_T2(101,_T4,_T5);goto _TL199;_TL198: _TL199: {struct _handler_cons _T21;_T6=& _T21;_push_handler(_T6);{int _T22=0;_T7=setjmp(_T21.handler);if(!_T7)goto _TL19B;_T22=1;goto _TL19C;_TL19B: _TL19C: if(_T22)goto _TL19D;else{goto _TL19F;}_TL19F: _T9=Cyc_Hashtable_lookup;{
# 1116
void*(*_T23)(struct Cyc_Hashtable_Table*,struct _fat_ptr*)=(void*(*)(struct Cyc_Hashtable_Table*,struct _fat_ptr*))_T9;_T8=_T23;}_TA=cvmap;{struct _fat_ptr*_T23=_cycalloc(sizeof(struct _fat_ptr));*_T23=s;_TB=(struct _fat_ptr*)_T23;}{void*_T23=_T8(_TA,_TB);_npop_handler(0);return _T23;}_pop_handler();goto _TL19E;_TL19D: _TC=Cyc_Core_get_exn_thrown();{void*_T23=(void*)_TC;void*_T24;_TD=(struct Cyc_Core_Not_found_exn_struct*)_T23;_TE=_TD->tag;_TF=Cyc_Core_Not_found;if(_TE!=_TF)goto _TL1A0;{
# 1119
struct _fat_ptr kind=Cyc_strchr(s,'_');_T10=kind;_T11=
_fat_ptr_plus(_T10,sizeof(char),1);{struct _fat_ptr name=Cyc_strchr(_T11,'_');_T12=& name;
_fat_ptr_inplace_plus(_T12,sizeof(char),1);_T13=kind;_T14=
_tag_fat("_PTRBND",sizeof(char),8U);_T15=Cyc_strncmp(_T13,_T14,7U);if(_T15)goto _TL1A2;else{goto _TL1A4;}
_TL1A4: _T16=& Cyc_Kinds_ptrbko;_T17=(struct Cyc_Core_Opt*)_T16;_T18=name;{void*t=Cyc_Absyn_cvar_type_name(_T17,_T18);_T1A=Cyc_Hashtable_insert;{
void(*_T25)(struct Cyc_Hashtable_Table*,struct _fat_ptr*,void*)=(void(*)(struct Cyc_Hashtable_Table*,struct _fat_ptr*,void*))_T1A;_T19=_T25;}_T1B=_check_null(cvmap);{struct _fat_ptr*_T25=_cycalloc(sizeof(struct _fat_ptr));*_T25=s;_T1C=(struct _fat_ptr*)_T25;}_T1D=t;_T19(_T1B,_T1C,_T1D);_T1E=t;
return _T1E;}
# 1128
_TL1A2:{int(*_T25)(unsigned,struct _fat_ptr)=(int(*)(unsigned,struct _fat_ptr))Cyc_Parse_parse_abort;_T1F=_T25;}_T20=_tag_fat("Constraint variable unknown kind",sizeof(char),33U);_T1F(0U,_T20);;}}goto _TL1A1;_TL1A0: _T24=_T23;{void*exn=_T24;_rethrow(exn);}_TL1A1:;}_TL19E:;}}}
# 1134
static void*Cyc_Parse_str2type(unsigned loc,struct _fat_ptr s){struct _fat_ptr _T0;struct _fat_ptr _T1;int _T2;void*_T3;void*_T4;struct _fat_ptr _T5;struct _fat_ptr _T6;int _T7;void*_T8;void*_T9;void*_TA;int(*_TB)(unsigned,struct _fat_ptr);unsigned _TC;struct _fat_ptr _TD;struct Cyc_String_pa_PrintArg_struct _TE;struct _fat_ptr _TF;struct _fat_ptr _T10;_T0=s;_T1=
_tag_fat("@fat",sizeof(char),5U);_T2=Cyc_strcmp(_T0,_T1);if(_T2)goto _TL1A5;else{goto _TL1A7;}
_TL1A7: _T3=Cyc_Tcutil_ptrbnd_cvar_equivalent(Cyc_Absyn_fat_bound_type);_T4=_check_null(_T3);return _T4;
# 1138
_TL1A5: _T5=s;_T6=_tag_fat("@thin @numelts{valueof_t(1U)}",sizeof(char),30U);_T7=Cyc_strcmp(_T5,_T6);if(_T7)goto _TL1A8;else{goto _TL1AA;}
_TL1AA: _T8=Cyc_Absyn_bounds_one();_T9=Cyc_Tcutil_ptrbnd_cvar_equivalent(_T8);_TA=_check_null(_T9);return _TA;_TL1A8:{
# 1141
int(*_T11)(unsigned,struct _fat_ptr)=(int(*)(unsigned,struct _fat_ptr))Cyc_Parse_parse_abort;_TB=_T11;}_TC=loc;{struct Cyc_String_pa_PrintArg_struct _T11;_T11.tag=0;_T11.f1=s;_TE=_T11;}{struct Cyc_String_pa_PrintArg_struct _T11=_TE;void*_T12[1];_T12[0]=& _T11;_TF=_tag_fat("Unknown type constant:: %s",sizeof(char),27U);_T10=_tag_fat(_T12,sizeof(void*),1);_TD=Cyc_aprintf(_TF,_T10);}_TB(_TC,_TD);}
# 1144
static void*Cyc_Parse_composite_constraint(enum Cyc_Parse_ConstraintOps op,void*t1,void*t2){enum Cyc_Parse_ConstraintOps _T0;int _T1;void*_T2;void*_T3;void*_T4;void*_T5;void*_T6;void*_T7;void*_T8;int(*_T9)(unsigned,struct _fat_ptr);struct _fat_ptr _TA;_T0=op;_T1=(int)_T0;switch(_T1){case Cyc_Parse_C_AND_OP: _T2=t1;_T3=
# 1146
_check_null(t2);_T4=Cyc_BansheeIf_and_constraint(_T2,_T3);return _T4;case Cyc_Parse_C_OR_OP: _T5=t1;_T6=
_check_null(t2);_T7=Cyc_BansheeIf_or_constraint(_T5,_T6);return _T7;case Cyc_Parse_C_NOT_OP: _T8=
Cyc_BansheeIf_not_constraint(t1);return _T8;default:{
# 1150
int(*_TB)(unsigned,struct _fat_ptr)=(int(*)(unsigned,struct _fat_ptr))Cyc_Parse_parse_abort;_T9=_TB;}_TA=_tag_fat("Unexpected operator for composite constraint",sizeof(char),45U);_T9(0U,_TA);};}
# 1154
static void*Cyc_Parse_comparison_constraint(enum Cyc_Parse_ConstraintOps op,void*t1,void*t2){enum Cyc_Parse_ConstraintOps _T0;int _T1;void*_T2;void*_T3;int(*_T4)(unsigned,struct _fat_ptr);struct _fat_ptr _T5;_T0=op;_T1=(int)_T0;switch(_T1){case Cyc_Parse_C_EQ_OP: _T2=
# 1156
Cyc_BansheeIf_cmpeq_constraint(t1,t2);return _T2;case Cyc_Parse_C_INCL_OP: _T3=
Cyc_BansheeIf_inclusion_constraint(t1,t2);return _T3;default:{
# 1159
int(*_T6)(unsigned,struct _fat_ptr)=(int(*)(unsigned,struct _fat_ptr))Cyc_Parse_parse_abort;_T4=_T6;}_T5=_tag_fat("Unexpected operator for composite constraint",sizeof(char),45U);_T4(0U,_T5);};}struct _union_YYSTYPE_YYINITIALSVAL{int tag;int val;};struct _union_YYSTYPE_Int_tok{int tag;union Cyc_Absyn_Cnst val;};struct _union_YYSTYPE_Char_tok{int tag;char val;};struct _union_YYSTYPE_String_tok{int tag;struct _fat_ptr val;};struct _union_YYSTYPE_QualId_tok{int tag;struct _tuple0*val;};struct _union_YYSTYPE_Exp_tok{int tag;struct Cyc_Absyn_Exp*val;};struct _union_YYSTYPE_Stmt_tok{int tag;struct Cyc_Absyn_Stmt*val;};struct _tuple22{unsigned f0;void*f1;void*f2;};struct _union_YYSTYPE_YY1{int tag;struct _tuple22*val;};struct _union_YYSTYPE_YY2{int tag;void*val;};struct _union_YYSTYPE_YY3{int tag;struct Cyc_List_List*val;};struct _union_YYSTYPE_YY4{int tag;struct Cyc_List_List*val;};struct _union_YYSTYPE_YY5{int tag;struct Cyc_List_List*val;};struct _union_YYSTYPE_YY6{int tag;enum Cyc_Absyn_Primop val;};struct _union_YYSTYPE_YY7{int tag;struct Cyc_Core_Opt*val;};struct _union_YYSTYPE_YY8{int tag;struct Cyc_List_List*val;};struct _union_YYSTYPE_YY9{int tag;struct Cyc_Absyn_Pat*val;};struct _tuple23{struct Cyc_List_List*f0;int f1;};struct _union_YYSTYPE_YY10{int tag;struct _tuple23*val;};struct _union_YYSTYPE_YY11{int tag;struct Cyc_List_List*val;};struct _tuple24{struct Cyc_List_List*f0;struct Cyc_Absyn_Pat*f1;};struct _union_YYSTYPE_YY12{int tag;struct _tuple24*val;};struct _union_YYSTYPE_YY13{int tag;struct Cyc_List_List*val;};struct _union_YYSTYPE_YY14{int tag;struct _tuple23*val;};struct _union_YYSTYPE_YY15{int tag;struct Cyc_Absyn_Fndecl*val;};struct _union_YYSTYPE_YY16{int tag;struct Cyc_List_List*val;};struct _union_YYSTYPE_YY17{int tag;struct Cyc_Parse_Declaration_spec val;};struct _union_YYSTYPE_YY18{int tag;struct _tuple12 val;};struct _union_YYSTYPE_YY19{int tag;struct _tuple11*val;};struct _union_YYSTYPE_YY20{int tag;enum Cyc_Parse_Storage_class val;};struct _union_YYSTYPE_YY21{int tag;struct Cyc_Parse_Type_specifier val;};struct _union_YYSTYPE_YY22{int tag;enum Cyc_Absyn_AggrKind val;};struct _tuple25{int f0;enum Cyc_Absyn_AggrKind f1;};struct _union_YYSTYPE_YY23{int tag;struct _tuple25 val;};struct _union_YYSTYPE_YY24{int tag;struct Cyc_Absyn_Tqual val;};struct _union_YYSTYPE_YY25{int tag;struct Cyc_List_List*val;};struct _union_YYSTYPE_YY26{int tag;struct Cyc_List_List*val;};struct _union_YYSTYPE_YY27{int tag;struct Cyc_List_List*val;};struct _union_YYSTYPE_YY28{int tag;struct Cyc_Parse_Declarator val;};struct _union_YYSTYPE_YY29{int tag;struct _tuple12*val;};struct _union_YYSTYPE_YY30{int tag;struct Cyc_List_List*val;};struct _union_YYSTYPE_YY31{int tag;struct Cyc_Parse_Abstractdeclarator val;};struct _union_YYSTYPE_YY32{int tag;int val;};struct _union_YYSTYPE_YY33{int tag;enum Cyc_Absyn_Scope val;};struct _union_YYSTYPE_YY34{int tag;struct Cyc_Absyn_Datatypefield*val;};struct _union_YYSTYPE_YY35{int tag;struct Cyc_List_List*val;};struct _tuple26{struct Cyc_Absyn_Tqual f0;struct Cyc_Parse_Type_specifier f1;struct Cyc_List_List*f2;};struct _union_YYSTYPE_YY36{int tag;struct _tuple26 val;};struct _union_YYSTYPE_YY37{int tag;struct Cyc_List_List*val;};struct _union_YYSTYPE_YY38{int tag;struct _tuple8*val;};struct _union_YYSTYPE_YY39{int tag;struct Cyc_List_List*val;};struct _tuple27{struct Cyc_List_List*f0;int f1;struct Cyc_Absyn_VarargInfo*f2;void*f3;struct Cyc_List_List*f4;struct Cyc_List_List*f5;};struct _union_YYSTYPE_YY40{int tag;struct _tuple27*val;};struct _union_YYSTYPE_YY41{int tag;struct Cyc_List_List*val;};struct _union_YYSTYPE_YY42{int tag;struct Cyc_List_List*val;};struct _union_YYSTYPE_YY43{int tag;void*val;};struct _union_YYSTYPE_YY44{int tag;struct Cyc_Absyn_Kind*val;};struct _union_YYSTYPE_YY45{int tag;void*val;};struct _union_YYSTYPE_YY46{int tag;struct Cyc_List_List*val;};struct _union_YYSTYPE_YY47{int tag;void*val;};struct _union_YYSTYPE_YY48{int tag;struct Cyc_Absyn_Enumfield*val;};struct _union_YYSTYPE_YY49{int tag;struct Cyc_List_List*val;};struct _union_YYSTYPE_YY50{int tag;void*val;};struct _tuple28{struct Cyc_List_List*f0;struct Cyc_List_List*f1;};struct _union_YYSTYPE_YY51{int tag;struct _tuple28*val;};struct _union_YYSTYPE_YY52{int tag;void*val;};struct _tuple29{void*f0;void*f1;};struct _union_YYSTYPE_YY53{int tag;struct _tuple29*val;};struct _union_YYSTYPE_YY54{int tag;void*val;};struct _union_YYSTYPE_YY55{int tag;struct Cyc_List_List*val;};struct _tuple30{struct Cyc_List_List*f0;unsigned f1;};struct _union_YYSTYPE_YY56{int tag;struct _tuple30*val;};struct _union_YYSTYPE_YY57{int tag;struct Cyc_List_List*val;};struct _union_YYSTYPE_YY58{int tag;void*val;};struct _union_YYSTYPE_YY59{int tag;void*val;};struct _union_YYSTYPE_YY60{int tag;struct Cyc_List_List*val;};struct _union_YYSTYPE_YY61{int tag;struct Cyc_Absyn_Exp*val;};struct _union_YYSTYPE_YY62{int tag;struct _tuple21 val;};struct _union_YYSTYPE_YY63{int tag;void*val;};struct _tuple31{struct Cyc_List_List*f0;struct Cyc_List_List*f1;struct Cyc_List_List*f2;};struct _union_YYSTYPE_YY64{int tag;struct _tuple31*val;};struct _union_YYSTYPE_YY65{int tag;struct _tuple28*val;};struct _union_YYSTYPE_YY66{int tag;struct Cyc_List_List*val;};struct _union_YYSTYPE_YY67{int tag;struct Cyc_List_List*val;};struct _tuple32{struct _fat_ptr f0;struct Cyc_Absyn_Exp*f1;};struct _union_YYSTYPE_YY68{int tag;struct _tuple32*val;};struct _union_YYSTYPE_YY69{int tag;struct Cyc_Absyn_Exp*(*val)(struct Cyc_Absyn_Exp*,struct Cyc_Absyn_Exp*,unsigned);};struct _union_YYSTYPE_YY70{int tag;enum Cyc_Parse_ConstraintOps val;};struct _union_YYSTYPE_YY71{int tag;struct Cyc_List_List*val;};struct _union_YYSTYPE_YY72{int tag;struct Cyc_List_List*val;};struct _union_YYSTYPE_YY73{int tag;void*val;};struct _union_YYSTYPE_YY74{int tag;int val;};union Cyc_YYSTYPE{struct _union_YYSTYPE_YYINITIALSVAL YYINITIALSVAL;struct _union_YYSTYPE_Int_tok Int_tok;struct _union_YYSTYPE_Char_tok Char_tok;struct _union_YYSTYPE_String_tok String_tok;struct _union_YYSTYPE_QualId_tok QualId_tok;struct _union_YYSTYPE_Exp_tok Exp_tok;struct _union_YYSTYPE_Stmt_tok Stmt_tok;struct _union_YYSTYPE_YY1 YY1;struct _union_YYSTYPE_YY2 YY2;struct _union_YYSTYPE_YY3 YY3;struct _union_YYSTYPE_YY4 YY4;struct _union_YYSTYPE_YY5 YY5;struct _union_YYSTYPE_YY6 YY6;struct _union_YYSTYPE_YY7 YY7;struct _union_YYSTYPE_YY8 YY8;struct _union_YYSTYPE_YY9 YY9;struct _union_YYSTYPE_YY10 YY10;struct _union_YYSTYPE_YY11 YY11;struct _union_YYSTYPE_YY12 YY12;struct _union_YYSTYPE_YY13 YY13;struct _union_YYSTYPE_YY14 YY14;struct _union_YYSTYPE_YY15 YY15;struct _union_YYSTYPE_YY16 YY16;struct _union_YYSTYPE_YY17 YY17;struct _union_YYSTYPE_YY18 YY18;struct _union_YYSTYPE_YY19 YY19;struct _union_YYSTYPE_YY20 YY20;struct _union_YYSTYPE_YY21 YY21;struct _union_YYSTYPE_YY22 YY22;struct _union_YYSTYPE_YY23 YY23;struct _union_YYSTYPE_YY24 YY24;struct _union_YYSTYPE_YY25 YY25;struct _union_YYSTYPE_YY26 YY26;struct _union_YYSTYPE_YY27 YY27;struct _union_YYSTYPE_YY28 YY28;struct _union_YYSTYPE_YY29 YY29;struct _union_YYSTYPE_YY30 YY30;struct _union_YYSTYPE_YY31 YY31;struct _union_YYSTYPE_YY32 YY32;struct _union_YYSTYPE_YY33 YY33;struct _union_YYSTYPE_YY34 YY34;struct _union_YYSTYPE_YY35 YY35;struct _union_YYSTYPE_YY36 YY36;struct _union_YYSTYPE_YY37 YY37;struct _union_YYSTYPE_YY38 YY38;struct _union_YYSTYPE_YY39 YY39;struct _union_YYSTYPE_YY40 YY40;struct _union_YYSTYPE_YY41 YY41;struct _union_YYSTYPE_YY42 YY42;struct _union_YYSTYPE_YY43 YY43;struct _union_YYSTYPE_YY44 YY44;struct _union_YYSTYPE_YY45 YY45;struct _union_YYSTYPE_YY46 YY46;struct _union_YYSTYPE_YY47 YY47;struct _union_YYSTYPE_YY48 YY48;struct _union_YYSTYPE_YY49 YY49;struct _union_YYSTYPE_YY50 YY50;struct _union_YYSTYPE_YY51 YY51;struct _union_YYSTYPE_YY52 YY52;struct _union_YYSTYPE_YY53 YY53;struct _union_YYSTYPE_YY54 YY54;struct _union_YYSTYPE_YY55 YY55;struct _union_YYSTYPE_YY56 YY56;struct _union_YYSTYPE_YY57 YY57;struct _union_YYSTYPE_YY58 YY58;struct _union_YYSTYPE_YY59 YY59;struct _union_YYSTYPE_YY60 YY60;struct _union_YYSTYPE_YY61 YY61;struct _union_YYSTYPE_YY62 YY62;struct _union_YYSTYPE_YY63 YY63;struct _union_YYSTYPE_YY64 YY64;struct _union_YYSTYPE_YY65 YY65;struct _union_YYSTYPE_YY66 YY66;struct _union_YYSTYPE_YY67 YY67;struct _union_YYSTYPE_YY68 YY68;struct _union_YYSTYPE_YY69 YY69;struct _union_YYSTYPE_YY70 YY70;struct _union_YYSTYPE_YY71 YY71;struct _union_YYSTYPE_YY72 YY72;struct _union_YYSTYPE_YY73 YY73;struct _union_YYSTYPE_YY74 YY74;};
# 1252
static void Cyc_yythrowfail(struct _fat_ptr s){struct Cyc_Core_Failure_exn_struct*_T0;void*_T1;{struct Cyc_Core_Failure_exn_struct*_T2=_cycalloc(sizeof(struct Cyc_Core_Failure_exn_struct));_T2->tag=Cyc_Core_Failure;
_T2->f1=s;_T0=(struct Cyc_Core_Failure_exn_struct*)_T2;}_T1=(void*)_T0;_throw(_T1);}static char _TmpG2[7U]="cnst_t";
# 1215 "parse.y"
static union Cyc_Absyn_Cnst Cyc_yyget_Int_tok(union Cyc_YYSTYPE*yy1){union Cyc_YYSTYPE*_T0;union Cyc_YYSTYPE*_T1;struct _union_YYSTYPE_Int_tok _T2;unsigned _T3;union Cyc_YYSTYPE*_T4;struct _union_YYSTYPE_Int_tok _T5;union Cyc_Absyn_Cnst _T6;
static struct _fat_ptr s={(unsigned char*)_TmpG2,(unsigned char*)_TmpG2,(unsigned char*)_TmpG2 + 7U};union Cyc_Absyn_Cnst _T7;_T0=yy1;_T1=(union Cyc_YYSTYPE*)_T0;_T2=_T1->Int_tok;_T3=_T2.tag;if(_T3!=2)goto _TL1AD;_T4=yy1;{union Cyc_YYSTYPE _T8=*_T4;_T5=_T8.Int_tok;_T7=_T5.val;}{union Cyc_Absyn_Cnst yy=_T7;_T6=yy;
# 1218
return _T6;}_TL1AD:
 Cyc_yythrowfail(s);;}
# 1222
static union Cyc_YYSTYPE Cyc_Int_tok(union Cyc_Absyn_Cnst yy1){union Cyc_YYSTYPE _T0;{union Cyc_YYSTYPE _T1;_T1.Int_tok.tag=2U;_T1.Int_tok.val=yy1;_T0=_T1;}return _T0;}static char _TmpG3[5U]="char";
# 1216 "parse.y"
static char Cyc_yyget_Char_tok(union Cyc_YYSTYPE*yy1){union Cyc_YYSTYPE*_T0;union Cyc_YYSTYPE*_T1;struct _union_YYSTYPE_Char_tok _T2;unsigned _T3;union Cyc_YYSTYPE*_T4;struct _union_YYSTYPE_Char_tok _T5;char _T6;
static struct _fat_ptr s={(unsigned char*)_TmpG3,(unsigned char*)_TmpG3,(unsigned char*)_TmpG3 + 5U};char _T7;_T0=yy1;_T1=(union Cyc_YYSTYPE*)_T0;_T2=_T1->Char_tok;_T3=_T2.tag;if(_T3!=3)goto _TL1AF;_T4=yy1;{union Cyc_YYSTYPE _T8=*_T4;_T5=_T8.Char_tok;_T7=_T5.val;}{char yy=_T7;_T6=yy;
# 1219
return _T6;}_TL1AF:
 Cyc_yythrowfail(s);;}
# 1223
static union Cyc_YYSTYPE Cyc_Char_tok(char yy1){union Cyc_YYSTYPE _T0;{union Cyc_YYSTYPE _T1;_T1.Char_tok.tag=3U;_T1.Char_tok.val=yy1;_T0=_T1;}return _T0;}static char _TmpG4[13U]="string_t<`H>";
# 1217 "parse.y"
static struct _fat_ptr Cyc_yyget_String_tok(union Cyc_YYSTYPE*yy1){union Cyc_YYSTYPE*_T0;union Cyc_YYSTYPE*_T1;struct _union_YYSTYPE_String_tok _T2;unsigned _T3;union Cyc_YYSTYPE*_T4;struct _union_YYSTYPE_String_tok _T5;struct _fat_ptr _T6;
static struct _fat_ptr s={(unsigned char*)_TmpG4,(unsigned char*)_TmpG4,(unsigned char*)_TmpG4 + 13U};struct _fat_ptr _T7;_T0=yy1;_T1=(union Cyc_YYSTYPE*)_T0;_T2=_T1->String_tok;_T3=_T2.tag;if(_T3!=4)goto _TL1B1;_T4=yy1;{union Cyc_YYSTYPE _T8=*_T4;_T5=_T8.String_tok;_T7=_T5.val;}{struct _fat_ptr yy=_T7;_T6=yy;
# 1220
return _T6;}_TL1B1:
 Cyc_yythrowfail(s);;}
# 1224
static union Cyc_YYSTYPE Cyc_String_tok(struct _fat_ptr yy1){union Cyc_YYSTYPE _T0;{union Cyc_YYSTYPE _T1;_T1.String_tok.tag=4U;_T1.String_tok.val=yy1;_T0=_T1;}return _T0;}static char _TmpG5[35U]="$(seg_t,booltype_t, ptrbound_t)@`H";
# 1220 "parse.y"
static struct _tuple22*Cyc_yyget_YY1(union Cyc_YYSTYPE*yy1){union Cyc_YYSTYPE*_T0;union Cyc_YYSTYPE*_T1;struct _union_YYSTYPE_YY1 _T2;unsigned _T3;union Cyc_YYSTYPE*_T4;struct _union_YYSTYPE_YY1 _T5;struct _tuple22*_T6;
static struct _fat_ptr s={(unsigned char*)_TmpG5,(unsigned char*)_TmpG5,(unsigned char*)_TmpG5 + 35U};struct _tuple22*_T7;_T0=yy1;_T1=(union Cyc_YYSTYPE*)_T0;_T2=_T1->YY1;_T3=_T2.tag;if(_T3!=8)goto _TL1B3;_T4=yy1;{union Cyc_YYSTYPE _T8=*_T4;_T5=_T8.YY1;_T7=_T5.val;}{struct _tuple22*yy=_T7;_T6=yy;
# 1223
return _T6;}_TL1B3:
 Cyc_yythrowfail(s);;}
# 1227
static union Cyc_YYSTYPE Cyc_YY1(struct _tuple22*yy1){union Cyc_YYSTYPE _T0;{union Cyc_YYSTYPE _T1;_T1.YY1.tag=8U;_T1.YY1.val=yy1;_T0=_T1;}return _T0;}static char _TmpG6[11U]="ptrbound_t";
# 1221 "parse.y"
static void*Cyc_yyget_YY2(union Cyc_YYSTYPE*yy1){union Cyc_YYSTYPE*_T0;union Cyc_YYSTYPE*_T1;struct _union_YYSTYPE_YY2 _T2;unsigned _T3;union Cyc_YYSTYPE*_T4;struct _union_YYSTYPE_YY2 _T5;void*_T6;
static struct _fat_ptr s={(unsigned char*)_TmpG6,(unsigned char*)_TmpG6,(unsigned char*)_TmpG6 + 11U};void*_T7;_T0=yy1;_T1=(union Cyc_YYSTYPE*)_T0;_T2=_T1->YY2;_T3=_T2.tag;if(_T3!=9)goto _TL1B5;_T4=yy1;{union Cyc_YYSTYPE _T8=*_T4;_T5=_T8.YY2;_T7=_T5.val;}{void*yy=_T7;_T6=yy;
# 1224
return _T6;}_TL1B5:
 Cyc_yythrowfail(s);;}
# 1228
static union Cyc_YYSTYPE Cyc_YY2(void*yy1){union Cyc_YYSTYPE _T0;{union Cyc_YYSTYPE _T1;_T1.YY2.tag=9U;_T1.YY2.val=yy1;_T0=_T1;}return _T0;}static char _TmpG7[28U]="list_t<offsetof_field_t,`H>";
# 1222 "parse.y"
static struct Cyc_List_List*Cyc_yyget_YY3(union Cyc_YYSTYPE*yy1){union Cyc_YYSTYPE*_T0;union Cyc_YYSTYPE*_T1;struct _union_YYSTYPE_YY3 _T2;unsigned _T3;union Cyc_YYSTYPE*_T4;struct _union_YYSTYPE_YY3 _T5;struct Cyc_List_List*_T6;
static struct _fat_ptr s={(unsigned char*)_TmpG7,(unsigned char*)_TmpG7,(unsigned char*)_TmpG7 + 28U};struct Cyc_List_List*_T7;_T0=yy1;_T1=(union Cyc_YYSTYPE*)_T0;_T2=_T1->YY3;_T3=_T2.tag;if(_T3!=10)goto _TL1B7;_T4=yy1;{union Cyc_YYSTYPE _T8=*_T4;_T5=_T8.YY3;_T7=_T5.val;}{struct Cyc_List_List*yy=_T7;_T6=yy;
# 1225
return _T6;}_TL1B7:
 Cyc_yythrowfail(s);;}
# 1229
static union Cyc_YYSTYPE Cyc_YY3(struct Cyc_List_List*yy1){union Cyc_YYSTYPE _T0;{union Cyc_YYSTYPE _T1;_T1.YY3.tag=10U;_T1.YY3.val=yy1;_T0=_T1;}return _T0;}static char _TmpG8[6U]="exp_t";
# 1223 "parse.y"
static struct Cyc_Absyn_Exp*Cyc_yyget_Exp_tok(union Cyc_YYSTYPE*yy1){union Cyc_YYSTYPE*_T0;union Cyc_YYSTYPE*_T1;struct _union_YYSTYPE_Exp_tok _T2;unsigned _T3;union Cyc_YYSTYPE*_T4;struct _union_YYSTYPE_Exp_tok _T5;struct Cyc_Absyn_Exp*_T6;
static struct _fat_ptr s={(unsigned char*)_TmpG8,(unsigned char*)_TmpG8,(unsigned char*)_TmpG8 + 6U};struct Cyc_Absyn_Exp*_T7;_T0=yy1;_T1=(union Cyc_YYSTYPE*)_T0;_T2=_T1->Exp_tok;_T3=_T2.tag;if(_T3!=6)goto _TL1B9;_T4=yy1;{union Cyc_YYSTYPE _T8=*_T4;_T5=_T8.Exp_tok;_T7=_T5.val;}{struct Cyc_Absyn_Exp*yy=_T7;_T6=yy;
# 1226
return _T6;}_TL1B9:
 Cyc_yythrowfail(s);;}
# 1230
static union Cyc_YYSTYPE Cyc_Exp_tok(struct Cyc_Absyn_Exp*yy1){union Cyc_YYSTYPE _T0;{union Cyc_YYSTYPE _T1;_T1.Exp_tok.tag=6U;_T1.Exp_tok.val=yy1;_T0=_T1;}return _T0;}static char _TmpG9[17U]="list_t<exp_t,`H>";
static struct Cyc_List_List*Cyc_yyget_YY4(union Cyc_YYSTYPE*yy1){union Cyc_YYSTYPE*_T0;union Cyc_YYSTYPE*_T1;struct _union_YYSTYPE_YY4 _T2;unsigned _T3;union Cyc_YYSTYPE*_T4;struct _union_YYSTYPE_YY4 _T5;struct Cyc_List_List*_T6;
static struct _fat_ptr s={(unsigned char*)_TmpG9,(unsigned char*)_TmpG9,(unsigned char*)_TmpG9 + 17U};struct Cyc_List_List*_T7;_T0=yy1;_T1=(union Cyc_YYSTYPE*)_T0;_T2=_T1->YY4;_T3=_T2.tag;if(_T3!=11)goto _TL1BB;_T4=yy1;{union Cyc_YYSTYPE _T8=*_T4;_T5=_T8.YY4;_T7=_T5.val;}{struct Cyc_List_List*yy=_T7;_T6=yy;
# 1234
return _T6;}_TL1BB:
 Cyc_yythrowfail(s);;}
# 1238
static union Cyc_YYSTYPE Cyc_YY4(struct Cyc_List_List*yy1){union Cyc_YYSTYPE _T0;{union Cyc_YYSTYPE _T1;_T1.YY4.tag=11U;_T1.YY4.val=yy1;_T0=_T1;}return _T0;}static char _TmpGA[47U]="list_t<$(list_t<designator_t,`H>,exp_t)@`H,`H>";
# 1232 "parse.y"
static struct Cyc_List_List*Cyc_yyget_YY5(union Cyc_YYSTYPE*yy1){union Cyc_YYSTYPE*_T0;union Cyc_YYSTYPE*_T1;struct _union_YYSTYPE_YY5 _T2;unsigned _T3;union Cyc_YYSTYPE*_T4;struct _union_YYSTYPE_YY5 _T5;struct Cyc_List_List*_T6;
static struct _fat_ptr s={(unsigned char*)_TmpGA,(unsigned char*)_TmpGA,(unsigned char*)_TmpGA + 47U};struct Cyc_List_List*_T7;_T0=yy1;_T1=(union Cyc_YYSTYPE*)_T0;_T2=_T1->YY5;_T3=_T2.tag;if(_T3!=12)goto _TL1BD;_T4=yy1;{union Cyc_YYSTYPE _T8=*_T4;_T5=_T8.YY5;_T7=_T5.val;}{struct Cyc_List_List*yy=_T7;_T6=yy;
# 1235
return _T6;}_TL1BD:
 Cyc_yythrowfail(s);;}
# 1239
static union Cyc_YYSTYPE Cyc_YY5(struct Cyc_List_List*yy1){union Cyc_YYSTYPE _T0;{union Cyc_YYSTYPE _T1;_T1.YY5.tag=12U;_T1.YY5.val=yy1;_T0=_T1;}return _T0;}static char _TmpGB[9U]="primop_t";
# 1233 "parse.y"
static enum Cyc_Absyn_Primop Cyc_yyget_YY6(union Cyc_YYSTYPE*yy1){union Cyc_YYSTYPE*_T0;union Cyc_YYSTYPE*_T1;struct _union_YYSTYPE_YY6 _T2;unsigned _T3;union Cyc_YYSTYPE*_T4;struct _union_YYSTYPE_YY6 _T5;enum Cyc_Absyn_Primop _T6;
static struct _fat_ptr s={(unsigned char*)_TmpGB,(unsigned char*)_TmpGB,(unsigned char*)_TmpGB + 9U};enum Cyc_Absyn_Primop _T7;_T0=yy1;_T1=(union Cyc_YYSTYPE*)_T0;_T2=_T1->YY6;_T3=_T2.tag;if(_T3!=13)goto _TL1BF;_T4=yy1;{union Cyc_YYSTYPE _T8=*_T4;_T5=_T8.YY6;_T7=_T5.val;}{enum Cyc_Absyn_Primop yy=_T7;_T6=yy;
# 1236
return _T6;}_TL1BF:
 Cyc_yythrowfail(s);;}
# 1240
static union Cyc_YYSTYPE Cyc_YY6(enum Cyc_Absyn_Primop yy1){union Cyc_YYSTYPE _T0;{union Cyc_YYSTYPE _T1;_T1.YY6.tag=13U;_T1.YY6.val=yy1;_T0=_T1;}return _T0;}static char _TmpGC[19U]="opt_t<primop_t,`H>";
# 1234 "parse.y"
static struct Cyc_Core_Opt*Cyc_yyget_YY7(union Cyc_YYSTYPE*yy1){union Cyc_YYSTYPE*_T0;union Cyc_YYSTYPE*_T1;struct _union_YYSTYPE_YY7 _T2;unsigned _T3;union Cyc_YYSTYPE*_T4;struct _union_YYSTYPE_YY7 _T5;struct Cyc_Core_Opt*_T6;
static struct _fat_ptr s={(unsigned char*)_TmpGC,(unsigned char*)_TmpGC,(unsigned char*)_TmpGC + 19U};struct Cyc_Core_Opt*_T7;_T0=yy1;_T1=(union Cyc_YYSTYPE*)_T0;_T2=_T1->YY7;_T3=_T2.tag;if(_T3!=14)goto _TL1C1;_T4=yy1;{union Cyc_YYSTYPE _T8=*_T4;_T5=_T8.YY7;_T7=_T5.val;}{struct Cyc_Core_Opt*yy=_T7;_T6=yy;
# 1237
return _T6;}_TL1C1:
 Cyc_yythrowfail(s);;}
# 1241
static union Cyc_YYSTYPE Cyc_YY7(struct Cyc_Core_Opt*yy1){union Cyc_YYSTYPE _T0;{union Cyc_YYSTYPE _T1;_T1.YY7.tag=14U;_T1.YY7.val=yy1;_T0=_T1;}return _T0;}static char _TmpGD[7U]="qvar_t";
# 1235 "parse.y"
static struct _tuple0*Cyc_yyget_QualId_tok(union Cyc_YYSTYPE*yy1){union Cyc_YYSTYPE*_T0;union Cyc_YYSTYPE*_T1;struct _union_YYSTYPE_QualId_tok _T2;unsigned _T3;union Cyc_YYSTYPE*_T4;struct _union_YYSTYPE_QualId_tok _T5;struct _tuple0*_T6;
static struct _fat_ptr s={(unsigned char*)_TmpGD,(unsigned char*)_TmpGD,(unsigned char*)_TmpGD + 7U};struct _tuple0*_T7;_T0=yy1;_T1=(union Cyc_YYSTYPE*)_T0;_T2=_T1->QualId_tok;_T3=_T2.tag;if(_T3!=5)goto _TL1C3;_T4=yy1;{union Cyc_YYSTYPE _T8=*_T4;_T5=_T8.QualId_tok;_T7=_T5.val;}{struct _tuple0*yy=_T7;_T6=yy;
# 1238
return _T6;}_TL1C3:
 Cyc_yythrowfail(s);;}
# 1242
static union Cyc_YYSTYPE Cyc_QualId_tok(struct _tuple0*yy1){union Cyc_YYSTYPE _T0;{union Cyc_YYSTYPE _T1;_T1.QualId_tok.tag=5U;_T1.QualId_tok.val=yy1;_T0=_T1;}return _T0;}static char _TmpGE[7U]="stmt_t";
# 1238 "parse.y"
static struct Cyc_Absyn_Stmt*Cyc_yyget_Stmt_tok(union Cyc_YYSTYPE*yy1){union Cyc_YYSTYPE*_T0;union Cyc_YYSTYPE*_T1;struct _union_YYSTYPE_Stmt_tok _T2;unsigned _T3;union Cyc_YYSTYPE*_T4;struct _union_YYSTYPE_Stmt_tok _T5;struct Cyc_Absyn_Stmt*_T6;
static struct _fat_ptr s={(unsigned char*)_TmpGE,(unsigned char*)_TmpGE,(unsigned char*)_TmpGE + 7U};struct Cyc_Absyn_Stmt*_T7;_T0=yy1;_T1=(union Cyc_YYSTYPE*)_T0;_T2=_T1->Stmt_tok;_T3=_T2.tag;if(_T3!=7)goto _TL1C5;_T4=yy1;{union Cyc_YYSTYPE _T8=*_T4;_T5=_T8.Stmt_tok;_T7=_T5.val;}{struct Cyc_Absyn_Stmt*yy=_T7;_T6=yy;
# 1241
return _T6;}_TL1C5:
 Cyc_yythrowfail(s);;}
# 1245
static union Cyc_YYSTYPE Cyc_Stmt_tok(struct Cyc_Absyn_Stmt*yy1){union Cyc_YYSTYPE _T0;{union Cyc_YYSTYPE _T1;_T1.Stmt_tok.tag=7U;_T1.Stmt_tok.val=yy1;_T0=_T1;}return _T0;}static char _TmpGF[27U]="list_t<switch_clause_t,`H>";
# 1241 "parse.y"
static struct Cyc_List_List*Cyc_yyget_YY8(union Cyc_YYSTYPE*yy1){union Cyc_YYSTYPE*_T0;union Cyc_YYSTYPE*_T1;struct _union_YYSTYPE_YY8 _T2;unsigned _T3;union Cyc_YYSTYPE*_T4;struct _union_YYSTYPE_YY8 _T5;struct Cyc_List_List*_T6;
static struct _fat_ptr s={(unsigned char*)_TmpGF,(unsigned char*)_TmpGF,(unsigned char*)_TmpGF + 27U};struct Cyc_List_List*_T7;_T0=yy1;_T1=(union Cyc_YYSTYPE*)_T0;_T2=_T1->YY8;_T3=_T2.tag;if(_T3!=15)goto _TL1C7;_T4=yy1;{union Cyc_YYSTYPE _T8=*_T4;_T5=_T8.YY8;_T7=_T5.val;}{struct Cyc_List_List*yy=_T7;_T6=yy;
# 1244
return _T6;}_TL1C7:
 Cyc_yythrowfail(s);;}
# 1248
static union Cyc_YYSTYPE Cyc_YY8(struct Cyc_List_List*yy1){union Cyc_YYSTYPE _T0;{union Cyc_YYSTYPE _T1;_T1.YY8.tag=15U;_T1.YY8.val=yy1;_T0=_T1;}return _T0;}static char _TmpG10[6U]="pat_t";
# 1242 "parse.y"
static struct Cyc_Absyn_Pat*Cyc_yyget_YY9(union Cyc_YYSTYPE*yy1){union Cyc_YYSTYPE*_T0;union Cyc_YYSTYPE*_T1;struct _union_YYSTYPE_YY9 _T2;unsigned _T3;union Cyc_YYSTYPE*_T4;struct _union_YYSTYPE_YY9 _T5;struct Cyc_Absyn_Pat*_T6;
static struct _fat_ptr s={(unsigned char*)_TmpG10,(unsigned char*)_TmpG10,(unsigned char*)_TmpG10 + 6U};struct Cyc_Absyn_Pat*_T7;_T0=yy1;_T1=(union Cyc_YYSTYPE*)_T0;_T2=_T1->YY9;_T3=_T2.tag;if(_T3!=16)goto _TL1C9;_T4=yy1;{union Cyc_YYSTYPE _T8=*_T4;_T5=_T8.YY9;_T7=_T5.val;}{struct Cyc_Absyn_Pat*yy=_T7;_T6=yy;
# 1245
return _T6;}_TL1C9:
 Cyc_yythrowfail(s);;}
# 1249
static union Cyc_YYSTYPE Cyc_YY9(struct Cyc_Absyn_Pat*yy1){union Cyc_YYSTYPE _T0;{union Cyc_YYSTYPE _T1;_T1.YY9.tag=16U;_T1.YY9.val=yy1;_T0=_T1;}return _T0;}static char _TmpG11[28U]="$(list_t<pat_t,`H>,bool)@`H";
# 1247 "parse.y"
static struct _tuple23*Cyc_yyget_YY10(union Cyc_YYSTYPE*yy1){union Cyc_YYSTYPE*_T0;union Cyc_YYSTYPE*_T1;struct _union_YYSTYPE_YY10 _T2;unsigned _T3;union Cyc_YYSTYPE*_T4;struct _union_YYSTYPE_YY10 _T5;struct _tuple23*_T6;
static struct _fat_ptr s={(unsigned char*)_TmpG11,(unsigned char*)_TmpG11,(unsigned char*)_TmpG11 + 28U};struct _tuple23*_T7;_T0=yy1;_T1=(union Cyc_YYSTYPE*)_T0;_T2=_T1->YY10;_T3=_T2.tag;if(_T3!=17)goto _TL1CB;_T4=yy1;{union Cyc_YYSTYPE _T8=*_T4;_T5=_T8.YY10;_T7=_T5.val;}{struct _tuple23*yy=_T7;_T6=yy;
# 1250
return _T6;}_TL1CB:
 Cyc_yythrowfail(s);;}
# 1254
static union Cyc_YYSTYPE Cyc_YY10(struct _tuple23*yy1){union Cyc_YYSTYPE _T0;{union Cyc_YYSTYPE _T1;_T1.YY10.tag=17U;_T1.YY10.val=yy1;_T0=_T1;}return _T0;}static char _TmpG12[17U]="list_t<pat_t,`H>";
# 1248 "parse.y"
static struct Cyc_List_List*Cyc_yyget_YY11(union Cyc_YYSTYPE*yy1){union Cyc_YYSTYPE*_T0;union Cyc_YYSTYPE*_T1;struct _union_YYSTYPE_YY11 _T2;unsigned _T3;union Cyc_YYSTYPE*_T4;struct _union_YYSTYPE_YY11 _T5;struct Cyc_List_List*_T6;
static struct _fat_ptr s={(unsigned char*)_TmpG12,(unsigned char*)_TmpG12,(unsigned char*)_TmpG12 + 17U};struct Cyc_List_List*_T7;_T0=yy1;_T1=(union Cyc_YYSTYPE*)_T0;_T2=_T1->YY11;_T3=_T2.tag;if(_T3!=18)goto _TL1CD;_T4=yy1;{union Cyc_YYSTYPE _T8=*_T4;_T5=_T8.YY11;_T7=_T5.val;}{struct Cyc_List_List*yy=_T7;_T6=yy;
# 1251
return _T6;}_TL1CD:
 Cyc_yythrowfail(s);;}
# 1255
static union Cyc_YYSTYPE Cyc_YY11(struct Cyc_List_List*yy1){union Cyc_YYSTYPE _T0;{union Cyc_YYSTYPE _T1;_T1.YY11.tag=18U;_T1.YY11.val=yy1;_T0=_T1;}return _T0;}static char _TmpG13[36U]="$(list_t<designator_t,`H>,pat_t)@`H";
# 1249 "parse.y"
static struct _tuple24*Cyc_yyget_YY12(union Cyc_YYSTYPE*yy1){union Cyc_YYSTYPE*_T0;union Cyc_YYSTYPE*_T1;struct _union_YYSTYPE_YY12 _T2;unsigned _T3;union Cyc_YYSTYPE*_T4;struct _union_YYSTYPE_YY12 _T5;struct _tuple24*_T6;
static struct _fat_ptr s={(unsigned char*)_TmpG13,(unsigned char*)_TmpG13,(unsigned char*)_TmpG13 + 36U};struct _tuple24*_T7;_T0=yy1;_T1=(union Cyc_YYSTYPE*)_T0;_T2=_T1->YY12;_T3=_T2.tag;if(_T3!=19)goto _TL1CF;_T4=yy1;{union Cyc_YYSTYPE _T8=*_T4;_T5=_T8.YY12;_T7=_T5.val;}{struct _tuple24*yy=_T7;_T6=yy;
# 1252
return _T6;}_TL1CF:
 Cyc_yythrowfail(s);;}
# 1256
static union Cyc_YYSTYPE Cyc_YY12(struct _tuple24*yy1){union Cyc_YYSTYPE _T0;{union Cyc_YYSTYPE _T1;_T1.YY12.tag=19U;_T1.YY12.val=yy1;_T0=_T1;}return _T0;}static char _TmpG14[47U]="list_t<$(list_t<designator_t,`H>,pat_t)@`H,`H>";
# 1250 "parse.y"
static struct Cyc_List_List*Cyc_yyget_YY13(union Cyc_YYSTYPE*yy1){union Cyc_YYSTYPE*_T0;union Cyc_YYSTYPE*_T1;struct _union_YYSTYPE_YY13 _T2;unsigned _T3;union Cyc_YYSTYPE*_T4;struct _union_YYSTYPE_YY13 _T5;struct Cyc_List_List*_T6;
static struct _fat_ptr s={(unsigned char*)_TmpG14,(unsigned char*)_TmpG14,(unsigned char*)_TmpG14 + 47U};struct Cyc_List_List*_T7;_T0=yy1;_T1=(union Cyc_YYSTYPE*)_T0;_T2=_T1->YY13;_T3=_T2.tag;if(_T3!=20)goto _TL1D1;_T4=yy1;{union Cyc_YYSTYPE _T8=*_T4;_T5=_T8.YY13;_T7=_T5.val;}{struct Cyc_List_List*yy=_T7;_T6=yy;
# 1253
return _T6;}_TL1D1:
 Cyc_yythrowfail(s);;}
# 1257
static union Cyc_YYSTYPE Cyc_YY13(struct Cyc_List_List*yy1){union Cyc_YYSTYPE _T0;{union Cyc_YYSTYPE _T1;_T1.YY13.tag=20U;_T1.YY13.val=yy1;_T0=_T1;}return _T0;}static char _TmpG15[58U]="$(list_t<$(list_t<designator_t,`H>,pat_t)@`H,`H>,bool)@`H";
# 1251 "parse.y"
static struct _tuple23*Cyc_yyget_YY14(union Cyc_YYSTYPE*yy1){union Cyc_YYSTYPE*_T0;union Cyc_YYSTYPE*_T1;struct _union_YYSTYPE_YY14 _T2;unsigned _T3;union Cyc_YYSTYPE*_T4;struct _union_YYSTYPE_YY14 _T5;struct _tuple23*_T6;
static struct _fat_ptr s={(unsigned char*)_TmpG15,(unsigned char*)_TmpG15,(unsigned char*)_TmpG15 + 58U};struct _tuple23*_T7;_T0=yy1;_T1=(union Cyc_YYSTYPE*)_T0;_T2=_T1->YY14;_T3=_T2.tag;if(_T3!=21)goto _TL1D3;_T4=yy1;{union Cyc_YYSTYPE _T8=*_T4;_T5=_T8.YY14;_T7=_T5.val;}{struct _tuple23*yy=_T7;_T6=yy;
# 1254
return _T6;}_TL1D3:
 Cyc_yythrowfail(s);;}
# 1258
static union Cyc_YYSTYPE Cyc_YY14(struct _tuple23*yy1){union Cyc_YYSTYPE _T0;{union Cyc_YYSTYPE _T1;_T1.YY14.tag=21U;_T1.YY14.val=yy1;_T0=_T1;}return _T0;}static char _TmpG16[9U]="fndecl_t";
# 1252 "parse.y"
static struct Cyc_Absyn_Fndecl*Cyc_yyget_YY15(union Cyc_YYSTYPE*yy1){union Cyc_YYSTYPE*_T0;union Cyc_YYSTYPE*_T1;struct _union_YYSTYPE_YY15 _T2;unsigned _T3;union Cyc_YYSTYPE*_T4;struct _union_YYSTYPE_YY15 _T5;struct Cyc_Absyn_Fndecl*_T6;
static struct _fat_ptr s={(unsigned char*)_TmpG16,(unsigned char*)_TmpG16,(unsigned char*)_TmpG16 + 9U};struct Cyc_Absyn_Fndecl*_T7;_T0=yy1;_T1=(union Cyc_YYSTYPE*)_T0;_T2=_T1->YY15;_T3=_T2.tag;if(_T3!=22)goto _TL1D5;_T4=yy1;{union Cyc_YYSTYPE _T8=*_T4;_T5=_T8.YY15;_T7=_T5.val;}{struct Cyc_Absyn_Fndecl*yy=_T7;_T6=yy;
# 1255
return _T6;}_TL1D5:
 Cyc_yythrowfail(s);;}
# 1259
static union Cyc_YYSTYPE Cyc_YY15(struct Cyc_Absyn_Fndecl*yy1){union Cyc_YYSTYPE _T0;{union Cyc_YYSTYPE _T1;_T1.YY15.tag=22U;_T1.YY15.val=yy1;_T0=_T1;}return _T0;}static char _TmpG17[18U]="list_t<decl_t,`H>";
# 1253 "parse.y"
static struct Cyc_List_List*Cyc_yyget_YY16(union Cyc_YYSTYPE*yy1){union Cyc_YYSTYPE*_T0;union Cyc_YYSTYPE*_T1;struct _union_YYSTYPE_YY16 _T2;unsigned _T3;union Cyc_YYSTYPE*_T4;struct _union_YYSTYPE_YY16 _T5;struct Cyc_List_List*_T6;
static struct _fat_ptr s={(unsigned char*)_TmpG17,(unsigned char*)_TmpG17,(unsigned char*)_TmpG17 + 18U};struct Cyc_List_List*_T7;_T0=yy1;_T1=(union Cyc_YYSTYPE*)_T0;_T2=_T1->YY16;_T3=_T2.tag;if(_T3!=23)goto _TL1D7;_T4=yy1;{union Cyc_YYSTYPE _T8=*_T4;_T5=_T8.YY16;_T7=_T5.val;}{struct Cyc_List_List*yy=_T7;_T6=yy;
# 1256
return _T6;}_TL1D7:
 Cyc_yythrowfail(s);;}
# 1260
static union Cyc_YYSTYPE Cyc_YY16(struct Cyc_List_List*yy1){union Cyc_YYSTYPE _T0;{union Cyc_YYSTYPE _T1;_T1.YY16.tag=23U;_T1.YY16.val=yy1;_T0=_T1;}return _T0;}static char _TmpG18[12U]="decl_spec_t";
# 1256 "parse.y"
static struct Cyc_Parse_Declaration_spec Cyc_yyget_YY17(union Cyc_YYSTYPE*yy1){union Cyc_YYSTYPE*_T0;union Cyc_YYSTYPE*_T1;struct _union_YYSTYPE_YY17 _T2;unsigned _T3;union Cyc_YYSTYPE*_T4;struct _union_YYSTYPE_YY17 _T5;struct Cyc_Parse_Declaration_spec _T6;
static struct _fat_ptr s={(unsigned char*)_TmpG18,(unsigned char*)_TmpG18,(unsigned char*)_TmpG18 + 12U};struct Cyc_Parse_Declaration_spec _T7;_T0=yy1;_T1=(union Cyc_YYSTYPE*)_T0;_T2=_T1->YY17;_T3=_T2.tag;if(_T3!=24)goto _TL1D9;_T4=yy1;{union Cyc_YYSTYPE _T8=*_T4;_T5=_T8.YY17;_T7=_T5.val;}{struct Cyc_Parse_Declaration_spec yy=_T7;_T6=yy;
# 1259
return _T6;}_TL1D9:
 Cyc_yythrowfail(s);;}
# 1263
static union Cyc_YYSTYPE Cyc_YY17(struct Cyc_Parse_Declaration_spec yy1){union Cyc_YYSTYPE _T0;{union Cyc_YYSTYPE _T1;_T1.YY17.tag=24U;_T1.YY17.val=yy1;_T0=_T1;}return _T0;}static char _TmpG19[41U]="$(declarator_t<`yy>,exp_opt_t,exp_opt_t)";
# 1257 "parse.y"
static struct _tuple12 Cyc_yyget_YY18(union Cyc_YYSTYPE*yy1){union Cyc_YYSTYPE*_T0;union Cyc_YYSTYPE*_T1;struct _union_YYSTYPE_YY18 _T2;unsigned _T3;union Cyc_YYSTYPE*_T4;struct _union_YYSTYPE_YY18 _T5;struct _tuple12 _T6;
static struct _fat_ptr s={(unsigned char*)_TmpG19,(unsigned char*)_TmpG19,(unsigned char*)_TmpG19 + 41U};struct _tuple12 _T7;_T0=yy1;_T1=(union Cyc_YYSTYPE*)_T0;_T2=_T1->YY18;_T3=_T2.tag;if(_T3!=25)goto _TL1DB;_T4=yy1;{union Cyc_YYSTYPE _T8=*_T4;_T5=_T8.YY18;_T7=_T5.val;}{struct _tuple12 yy=_T7;_T6=yy;
# 1260
return _T6;}_TL1DB:
 Cyc_yythrowfail(s);;}
# 1264
static union Cyc_YYSTYPE Cyc_YY18(struct _tuple12 yy1){union Cyc_YYSTYPE _T0;{union Cyc_YYSTYPE _T1;_T1.YY18.tag=25U;_T1.YY18.val=yy1;_T0=_T1;}return _T0;}static char _TmpG1A[23U]="declarator_list_t<`yy>";
# 1258 "parse.y"
static struct _tuple11*Cyc_yyget_YY19(union Cyc_YYSTYPE*yy1){union Cyc_YYSTYPE*_T0;union Cyc_YYSTYPE*_T1;struct _union_YYSTYPE_YY19 _T2;unsigned _T3;union Cyc_YYSTYPE*_T4;struct _union_YYSTYPE_YY19 _T5;struct _tuple11*_T6;
static struct _fat_ptr s={(unsigned char*)_TmpG1A,(unsigned char*)_TmpG1A,(unsigned char*)_TmpG1A + 23U};struct _tuple11*_T7;_T0=yy1;_T1=(union Cyc_YYSTYPE*)_T0;_T2=_T1->YY19;_T3=_T2.tag;if(_T3!=26)goto _TL1DD;_T4=yy1;{union Cyc_YYSTYPE _T8=*_T4;_T5=_T8.YY19;_T7=_T5.val;}{struct _tuple11*yy=_T7;_T6=yy;
# 1261
return _T6;}_TL1DD:
 Cyc_yythrowfail(s);;}
# 1265
static union Cyc_YYSTYPE Cyc_YY19(struct _tuple11*yy1){union Cyc_YYSTYPE _T0;{union Cyc_YYSTYPE _T1;_T1.YY19.tag=26U;_T1.YY19.val=yy1;_T0=_T1;}return _T0;}static char _TmpG1B[16U]="storage_class_t";
# 1259 "parse.y"
static enum Cyc_Parse_Storage_class Cyc_yyget_YY20(union Cyc_YYSTYPE*yy1){union Cyc_YYSTYPE*_T0;union Cyc_YYSTYPE*_T1;struct _union_YYSTYPE_YY20 _T2;unsigned _T3;union Cyc_YYSTYPE*_T4;struct _union_YYSTYPE_YY20 _T5;enum Cyc_Parse_Storage_class _T6;
static struct _fat_ptr s={(unsigned char*)_TmpG1B,(unsigned char*)_TmpG1B,(unsigned char*)_TmpG1B + 16U};enum Cyc_Parse_Storage_class _T7;_T0=yy1;_T1=(union Cyc_YYSTYPE*)_T0;_T2=_T1->YY20;_T3=_T2.tag;if(_T3!=27)goto _TL1DF;_T4=yy1;{union Cyc_YYSTYPE _T8=*_T4;_T5=_T8.YY20;_T7=_T5.val;}{enum Cyc_Parse_Storage_class yy=_T7;_T6=yy;
# 1262
return _T6;}_TL1DF:
 Cyc_yythrowfail(s);;}
# 1266
static union Cyc_YYSTYPE Cyc_YY20(enum Cyc_Parse_Storage_class yy1){union Cyc_YYSTYPE _T0;{union Cyc_YYSTYPE _T1;_T1.YY20.tag=27U;_T1.YY20.val=yy1;_T0=_T1;}return _T0;}static char _TmpG1C[17U]="type_specifier_t";
# 1260 "parse.y"
static struct Cyc_Parse_Type_specifier Cyc_yyget_YY21(union Cyc_YYSTYPE*yy1){union Cyc_YYSTYPE*_T0;union Cyc_YYSTYPE*_T1;struct _union_YYSTYPE_YY21 _T2;unsigned _T3;union Cyc_YYSTYPE*_T4;struct _union_YYSTYPE_YY21 _T5;struct Cyc_Parse_Type_specifier _T6;
static struct _fat_ptr s={(unsigned char*)_TmpG1C,(unsigned char*)_TmpG1C,(unsigned char*)_TmpG1C + 17U};struct Cyc_Parse_Type_specifier _T7;_T0=yy1;_T1=(union Cyc_YYSTYPE*)_T0;_T2=_T1->YY21;_T3=_T2.tag;if(_T3!=28)goto _TL1E1;_T4=yy1;{union Cyc_YYSTYPE _T8=*_T4;_T5=_T8.YY21;_T7=_T5.val;}{struct Cyc_Parse_Type_specifier yy=_T7;_T6=yy;
# 1263
return _T6;}_TL1E1:
 Cyc_yythrowfail(s);;}
# 1267
static union Cyc_YYSTYPE Cyc_YY21(struct Cyc_Parse_Type_specifier yy1){union Cyc_YYSTYPE _T0;{union Cyc_YYSTYPE _T1;_T1.YY21.tag=28U;_T1.YY21.val=yy1;_T0=_T1;}return _T0;}static char _TmpG1D[12U]="aggr_kind_t";
# 1262 "parse.y"
static enum Cyc_Absyn_AggrKind Cyc_yyget_YY22(union Cyc_YYSTYPE*yy1){union Cyc_YYSTYPE*_T0;union Cyc_YYSTYPE*_T1;struct _union_YYSTYPE_YY22 _T2;unsigned _T3;union Cyc_YYSTYPE*_T4;struct _union_YYSTYPE_YY22 _T5;enum Cyc_Absyn_AggrKind _T6;
static struct _fat_ptr s={(unsigned char*)_TmpG1D,(unsigned char*)_TmpG1D,(unsigned char*)_TmpG1D + 12U};enum Cyc_Absyn_AggrKind _T7;_T0=yy1;_T1=(union Cyc_YYSTYPE*)_T0;_T2=_T1->YY22;_T3=_T2.tag;if(_T3!=29)goto _TL1E3;_T4=yy1;{union Cyc_YYSTYPE _T8=*_T4;_T5=_T8.YY22;_T7=_T5.val;}{enum Cyc_Absyn_AggrKind yy=_T7;_T6=yy;
# 1265
return _T6;}_TL1E3:
 Cyc_yythrowfail(s);;}
# 1269
static union Cyc_YYSTYPE Cyc_YY22(enum Cyc_Absyn_AggrKind yy1){union Cyc_YYSTYPE _T0;{union Cyc_YYSTYPE _T1;_T1.YY22.tag=29U;_T1.YY22.val=yy1;_T0=_T1;}return _T0;}static char _TmpG1E[20U]="$(bool,aggr_kind_t)";
# 1263 "parse.y"
static struct _tuple25 Cyc_yyget_YY23(union Cyc_YYSTYPE*yy1){union Cyc_YYSTYPE*_T0;union Cyc_YYSTYPE*_T1;struct _union_YYSTYPE_YY23 _T2;unsigned _T3;union Cyc_YYSTYPE*_T4;struct _union_YYSTYPE_YY23 _T5;struct _tuple25 _T6;
static struct _fat_ptr s={(unsigned char*)_TmpG1E,(unsigned char*)_TmpG1E,(unsigned char*)_TmpG1E + 20U};struct _tuple25 _T7;_T0=yy1;_T1=(union Cyc_YYSTYPE*)_T0;_T2=_T1->YY23;_T3=_T2.tag;if(_T3!=30)goto _TL1E5;_T4=yy1;{union Cyc_YYSTYPE _T8=*_T4;_T5=_T8.YY23;_T7=_T5.val;}{struct _tuple25 yy=_T7;_T6=yy;
# 1266
return _T6;}_TL1E5:
 Cyc_yythrowfail(s);;}
# 1270
static union Cyc_YYSTYPE Cyc_YY23(struct _tuple25 yy1){union Cyc_YYSTYPE _T0;{union Cyc_YYSTYPE _T1;_T1.YY23.tag=30U;_T1.YY23.val=yy1;_T0=_T1;}return _T0;}static char _TmpG1F[8U]="tqual_t";
# 1264 "parse.y"
static struct Cyc_Absyn_Tqual Cyc_yyget_YY24(union Cyc_YYSTYPE*yy1){union Cyc_YYSTYPE*_T0;union Cyc_YYSTYPE*_T1;struct _union_YYSTYPE_YY24 _T2;unsigned _T3;union Cyc_YYSTYPE*_T4;struct _union_YYSTYPE_YY24 _T5;struct Cyc_Absyn_Tqual _T6;
static struct _fat_ptr s={(unsigned char*)_TmpG1F,(unsigned char*)_TmpG1F,(unsigned char*)_TmpG1F + 8U};struct Cyc_Absyn_Tqual _T7;_T0=yy1;_T1=(union Cyc_YYSTYPE*)_T0;_T2=_T1->YY24;_T3=_T2.tag;if(_T3!=31)goto _TL1E7;_T4=yy1;{union Cyc_YYSTYPE _T8=*_T4;_T5=_T8.YY24;_T7=_T5.val;}{struct Cyc_Absyn_Tqual yy=_T7;_T6=yy;
# 1267
return _T6;}_TL1E7:
 Cyc_yythrowfail(s);;}
# 1271
static union Cyc_YYSTYPE Cyc_YY24(struct Cyc_Absyn_Tqual yy1){union Cyc_YYSTYPE _T0;{union Cyc_YYSTYPE _T1;_T1.YY24.tag=31U;_T1.YY24.val=yy1;_T0=_T1;}return _T0;}static char _TmpG20[23U]="list_t<aggrfield_t,`H>";
# 1265 "parse.y"
static struct Cyc_List_List*Cyc_yyget_YY25(union Cyc_YYSTYPE*yy1){union Cyc_YYSTYPE*_T0;union Cyc_YYSTYPE*_T1;struct _union_YYSTYPE_YY25 _T2;unsigned _T3;union Cyc_YYSTYPE*_T4;struct _union_YYSTYPE_YY25 _T5;struct Cyc_List_List*_T6;
static struct _fat_ptr s={(unsigned char*)_TmpG20,(unsigned char*)_TmpG20,(unsigned char*)_TmpG20 + 23U};struct Cyc_List_List*_T7;_T0=yy1;_T1=(union Cyc_YYSTYPE*)_T0;_T2=_T1->YY25;_T3=_T2.tag;if(_T3!=32)goto _TL1E9;_T4=yy1;{union Cyc_YYSTYPE _T8=*_T4;_T5=_T8.YY25;_T7=_T5.val;}{struct Cyc_List_List*yy=_T7;_T6=yy;
# 1268
return _T6;}_TL1E9:
 Cyc_yythrowfail(s);;}
# 1272
static union Cyc_YYSTYPE Cyc_YY25(struct Cyc_List_List*yy1){union Cyc_YYSTYPE _T0;{union Cyc_YYSTYPE _T1;_T1.YY25.tag=32U;_T1.YY25.val=yy1;_T0=_T1;}return _T0;}static char _TmpG21[34U]="list_t<list_t<aggrfield_t,`H>,`H>";
# 1266 "parse.y"
static struct Cyc_List_List*Cyc_yyget_YY26(union Cyc_YYSTYPE*yy1){union Cyc_YYSTYPE*_T0;union Cyc_YYSTYPE*_T1;struct _union_YYSTYPE_YY26 _T2;unsigned _T3;union Cyc_YYSTYPE*_T4;struct _union_YYSTYPE_YY26 _T5;struct Cyc_List_List*_T6;
static struct _fat_ptr s={(unsigned char*)_TmpG21,(unsigned char*)_TmpG21,(unsigned char*)_TmpG21 + 34U};struct Cyc_List_List*_T7;_T0=yy1;_T1=(union Cyc_YYSTYPE*)_T0;_T2=_T1->YY26;_T3=_T2.tag;if(_T3!=33)goto _TL1EB;_T4=yy1;{union Cyc_YYSTYPE _T8=*_T4;_T5=_T8.YY26;_T7=_T5.val;}{struct Cyc_List_List*yy=_T7;_T6=yy;
# 1269
return _T6;}_TL1EB:
 Cyc_yythrowfail(s);;}
# 1273
static union Cyc_YYSTYPE Cyc_YY26(struct Cyc_List_List*yy1){union Cyc_YYSTYPE _T0;{union Cyc_YYSTYPE _T1;_T1.YY26.tag=33U;_T1.YY26.val=yy1;_T0=_T1;}return _T0;}static char _TmpG22[33U]="list_t<type_modifier_t<`yy>,`yy>";
# 1267 "parse.y"
static struct Cyc_List_List*Cyc_yyget_YY27(union Cyc_YYSTYPE*yy1){union Cyc_YYSTYPE*_T0;union Cyc_YYSTYPE*_T1;struct _union_YYSTYPE_YY27 _T2;unsigned _T3;union Cyc_YYSTYPE*_T4;struct _union_YYSTYPE_YY27 _T5;struct Cyc_List_List*_T6;
static struct _fat_ptr s={(unsigned char*)_TmpG22,(unsigned char*)_TmpG22,(unsigned char*)_TmpG22 + 33U};struct Cyc_List_List*_T7;_T0=yy1;_T1=(union Cyc_YYSTYPE*)_T0;_T2=_T1->YY27;_T3=_T2.tag;if(_T3!=34)goto _TL1ED;_T4=yy1;{union Cyc_YYSTYPE _T8=*_T4;_T5=_T8.YY27;_T7=_T5.val;}{struct Cyc_List_List*yy=_T7;_T6=yy;
# 1270
return _T6;}_TL1ED:
 Cyc_yythrowfail(s);;}
# 1274
static union Cyc_YYSTYPE Cyc_YY27(struct Cyc_List_List*yy1){union Cyc_YYSTYPE _T0;{union Cyc_YYSTYPE _T1;_T1.YY27.tag=34U;_T1.YY27.val=yy1;_T0=_T1;}return _T0;}static char _TmpG23[18U]="declarator_t<`yy>";
# 1268 "parse.y"
static struct Cyc_Parse_Declarator Cyc_yyget_YY28(union Cyc_YYSTYPE*yy1){union Cyc_YYSTYPE*_T0;union Cyc_YYSTYPE*_T1;struct _union_YYSTYPE_YY28 _T2;unsigned _T3;union Cyc_YYSTYPE*_T4;struct _union_YYSTYPE_YY28 _T5;struct Cyc_Parse_Declarator _T6;
static struct _fat_ptr s={(unsigned char*)_TmpG23,(unsigned char*)_TmpG23,(unsigned char*)_TmpG23 + 18U};struct Cyc_Parse_Declarator _T7;_T0=yy1;_T1=(union Cyc_YYSTYPE*)_T0;_T2=_T1->YY28;_T3=_T2.tag;if(_T3!=35)goto _TL1EF;_T4=yy1;{union Cyc_YYSTYPE _T8=*_T4;_T5=_T8.YY28;_T7=_T5.val;}{struct Cyc_Parse_Declarator yy=_T7;_T6=yy;
# 1271
return _T6;}_TL1EF:
 Cyc_yythrowfail(s);;}
# 1275
static union Cyc_YYSTYPE Cyc_YY28(struct Cyc_Parse_Declarator yy1){union Cyc_YYSTYPE _T0;{union Cyc_YYSTYPE _T1;_T1.YY28.tag=35U;_T1.YY28.val=yy1;_T0=_T1;}return _T0;}static char _TmpG24[45U]="$(declarator_t<`yy>,exp_opt_t,exp_opt_t)@`yy";
# 1269 "parse.y"
static struct _tuple12*Cyc_yyget_YY29(union Cyc_YYSTYPE*yy1){union Cyc_YYSTYPE*_T0;union Cyc_YYSTYPE*_T1;struct _union_YYSTYPE_YY29 _T2;unsigned _T3;union Cyc_YYSTYPE*_T4;struct _union_YYSTYPE_YY29 _T5;struct _tuple12*_T6;
static struct _fat_ptr s={(unsigned char*)_TmpG24,(unsigned char*)_TmpG24,(unsigned char*)_TmpG24 + 45U};struct _tuple12*_T7;_T0=yy1;_T1=(union Cyc_YYSTYPE*)_T0;_T2=_T1->YY29;_T3=_T2.tag;if(_T3!=36)goto _TL1F1;_T4=yy1;{union Cyc_YYSTYPE _T8=*_T4;_T5=_T8.YY29;_T7=_T5.val;}{struct _tuple12*yy=_T7;_T6=yy;
# 1272
return _T6;}_TL1F1:
 Cyc_yythrowfail(s);;}
# 1276
static union Cyc_YYSTYPE Cyc_YY29(struct _tuple12*yy1){union Cyc_YYSTYPE _T0;{union Cyc_YYSTYPE _T1;_T1.YY29.tag=36U;_T1.YY29.val=yy1;_T0=_T1;}return _T0;}static char _TmpG25[57U]="list_t<$(declarator_t<`yy>,exp_opt_t,exp_opt_t)@`yy,`yy>";
# 1270 "parse.y"
static struct Cyc_List_List*Cyc_yyget_YY30(union Cyc_YYSTYPE*yy1){union Cyc_YYSTYPE*_T0;union Cyc_YYSTYPE*_T1;struct _union_YYSTYPE_YY30 _T2;unsigned _T3;union Cyc_YYSTYPE*_T4;struct _union_YYSTYPE_YY30 _T5;struct Cyc_List_List*_T6;
static struct _fat_ptr s={(unsigned char*)_TmpG25,(unsigned char*)_TmpG25,(unsigned char*)_TmpG25 + 57U};struct Cyc_List_List*_T7;_T0=yy1;_T1=(union Cyc_YYSTYPE*)_T0;_T2=_T1->YY30;_T3=_T2.tag;if(_T3!=37)goto _TL1F3;_T4=yy1;{union Cyc_YYSTYPE _T8=*_T4;_T5=_T8.YY30;_T7=_T5.val;}{struct Cyc_List_List*yy=_T7;_T6=yy;
# 1273
return _T6;}_TL1F3:
 Cyc_yythrowfail(s);;}
# 1277
static union Cyc_YYSTYPE Cyc_YY30(struct Cyc_List_List*yy1){union Cyc_YYSTYPE _T0;{union Cyc_YYSTYPE _T1;_T1.YY30.tag=37U;_T1.YY30.val=yy1;_T0=_T1;}return _T0;}static char _TmpG26[26U]="abstractdeclarator_t<`yy>";
# 1271 "parse.y"
static struct Cyc_Parse_Abstractdeclarator Cyc_yyget_YY31(union Cyc_YYSTYPE*yy1){union Cyc_YYSTYPE*_T0;union Cyc_YYSTYPE*_T1;struct _union_YYSTYPE_YY31 _T2;unsigned _T3;union Cyc_YYSTYPE*_T4;struct _union_YYSTYPE_YY31 _T5;struct Cyc_Parse_Abstractdeclarator _T6;
static struct _fat_ptr s={(unsigned char*)_TmpG26,(unsigned char*)_TmpG26,(unsigned char*)_TmpG26 + 26U};struct Cyc_Parse_Abstractdeclarator _T7;_T0=yy1;_T1=(union Cyc_YYSTYPE*)_T0;_T2=_T1->YY31;_T3=_T2.tag;if(_T3!=38)goto _TL1F5;_T4=yy1;{union Cyc_YYSTYPE _T8=*_T4;_T5=_T8.YY31;_T7=_T5.val;}{struct Cyc_Parse_Abstractdeclarator yy=_T7;_T6=yy;
# 1274
return _T6;}_TL1F5:
 Cyc_yythrowfail(s);;}
# 1278
static union Cyc_YYSTYPE Cyc_YY31(struct Cyc_Parse_Abstractdeclarator yy1){union Cyc_YYSTYPE _T0;{union Cyc_YYSTYPE _T1;_T1.YY31.tag=38U;_T1.YY31.val=yy1;_T0=_T1;}return _T0;}static char _TmpG27[5U]="bool";
# 1272 "parse.y"
static int Cyc_yyget_YY32(union Cyc_YYSTYPE*yy1){union Cyc_YYSTYPE*_T0;union Cyc_YYSTYPE*_T1;struct _union_YYSTYPE_YY32 _T2;unsigned _T3;union Cyc_YYSTYPE*_T4;struct _union_YYSTYPE_YY32 _T5;int _T6;
static struct _fat_ptr s={(unsigned char*)_TmpG27,(unsigned char*)_TmpG27,(unsigned char*)_TmpG27 + 5U};int _T7;_T0=yy1;_T1=(union Cyc_YYSTYPE*)_T0;_T2=_T1->YY32;_T3=_T2.tag;if(_T3!=39)goto _TL1F7;_T4=yy1;{union Cyc_YYSTYPE _T8=*_T4;_T5=_T8.YY32;_T7=_T5.val;}{int yy=_T7;_T6=yy;
# 1275
return _T6;}_TL1F7:
 Cyc_yythrowfail(s);;}
# 1279
static union Cyc_YYSTYPE Cyc_YY32(int yy1){union Cyc_YYSTYPE _T0;{union Cyc_YYSTYPE _T1;_T1.YY32.tag=39U;_T1.YY32.val=yy1;_T0=_T1;}return _T0;}static char _TmpG28[8U]="scope_t";
# 1273 "parse.y"
static enum Cyc_Absyn_Scope Cyc_yyget_YY33(union Cyc_YYSTYPE*yy1){union Cyc_YYSTYPE*_T0;union Cyc_YYSTYPE*_T1;struct _union_YYSTYPE_YY33 _T2;unsigned _T3;union Cyc_YYSTYPE*_T4;struct _union_YYSTYPE_YY33 _T5;enum Cyc_Absyn_Scope _T6;
static struct _fat_ptr s={(unsigned char*)_TmpG28,(unsigned char*)_TmpG28,(unsigned char*)_TmpG28 + 8U};enum Cyc_Absyn_Scope _T7;_T0=yy1;_T1=(union Cyc_YYSTYPE*)_T0;_T2=_T1->YY33;_T3=_T2.tag;if(_T3!=40)goto _TL1F9;_T4=yy1;{union Cyc_YYSTYPE _T8=*_T4;_T5=_T8.YY33;_T7=_T5.val;}{enum Cyc_Absyn_Scope yy=_T7;_T6=yy;
# 1276
return _T6;}_TL1F9:
 Cyc_yythrowfail(s);;}
# 1280
static union Cyc_YYSTYPE Cyc_YY33(enum Cyc_Absyn_Scope yy1){union Cyc_YYSTYPE _T0;{union Cyc_YYSTYPE _T1;_T1.YY33.tag=40U;_T1.YY33.val=yy1;_T0=_T1;}return _T0;}static char _TmpG29[16U]="datatypefield_t";
# 1274 "parse.y"
static struct Cyc_Absyn_Datatypefield*Cyc_yyget_YY34(union Cyc_YYSTYPE*yy1){union Cyc_YYSTYPE*_T0;union Cyc_YYSTYPE*_T1;struct _union_YYSTYPE_YY34 _T2;unsigned _T3;union Cyc_YYSTYPE*_T4;struct _union_YYSTYPE_YY34 _T5;struct Cyc_Absyn_Datatypefield*_T6;
static struct _fat_ptr s={(unsigned char*)_TmpG29,(unsigned char*)_TmpG29,(unsigned char*)_TmpG29 + 16U};struct Cyc_Absyn_Datatypefield*_T7;_T0=yy1;_T1=(union Cyc_YYSTYPE*)_T0;_T2=_T1->YY34;_T3=_T2.tag;if(_T3!=41)goto _TL1FB;_T4=yy1;{union Cyc_YYSTYPE _T8=*_T4;_T5=_T8.YY34;_T7=_T5.val;}{struct Cyc_Absyn_Datatypefield*yy=_T7;_T6=yy;
# 1277
return _T6;}_TL1FB:
 Cyc_yythrowfail(s);;}
# 1281
static union Cyc_YYSTYPE Cyc_YY34(struct Cyc_Absyn_Datatypefield*yy1){union Cyc_YYSTYPE _T0;{union Cyc_YYSTYPE _T1;_T1.YY34.tag=41U;_T1.YY34.val=yy1;_T0=_T1;}return _T0;}static char _TmpG2A[27U]="list_t<datatypefield_t,`H>";
# 1275 "parse.y"
static struct Cyc_List_List*Cyc_yyget_YY35(union Cyc_YYSTYPE*yy1){union Cyc_YYSTYPE*_T0;union Cyc_YYSTYPE*_T1;struct _union_YYSTYPE_YY35 _T2;unsigned _T3;union Cyc_YYSTYPE*_T4;struct _union_YYSTYPE_YY35 _T5;struct Cyc_List_List*_T6;
static struct _fat_ptr s={(unsigned char*)_TmpG2A,(unsigned char*)_TmpG2A,(unsigned char*)_TmpG2A + 27U};struct Cyc_List_List*_T7;_T0=yy1;_T1=(union Cyc_YYSTYPE*)_T0;_T2=_T1->YY35;_T3=_T2.tag;if(_T3!=42)goto _TL1FD;_T4=yy1;{union Cyc_YYSTYPE _T8=*_T4;_T5=_T8.YY35;_T7=_T5.val;}{struct Cyc_List_List*yy=_T7;_T6=yy;
# 1278
return _T6;}_TL1FD:
 Cyc_yythrowfail(s);;}
# 1282
static union Cyc_YYSTYPE Cyc_YY35(struct Cyc_List_List*yy1){union Cyc_YYSTYPE _T0;{union Cyc_YYSTYPE _T1;_T1.YY35.tag=42U;_T1.YY35.val=yy1;_T0=_T1;}return _T0;}static char _TmpG2B[41U]="$(tqual_t,type_specifier_t,attributes_t)";
# 1276 "parse.y"
static struct _tuple26 Cyc_yyget_YY36(union Cyc_YYSTYPE*yy1){union Cyc_YYSTYPE*_T0;union Cyc_YYSTYPE*_T1;struct _union_YYSTYPE_YY36 _T2;unsigned _T3;union Cyc_YYSTYPE*_T4;struct _union_YYSTYPE_YY36 _T5;struct _tuple26 _T6;
static struct _fat_ptr s={(unsigned char*)_TmpG2B,(unsigned char*)_TmpG2B,(unsigned char*)_TmpG2B + 41U};struct _tuple26 _T7;_T0=yy1;_T1=(union Cyc_YYSTYPE*)_T0;_T2=_T1->YY36;_T3=_T2.tag;if(_T3!=43)goto _TL1FF;_T4=yy1;{union Cyc_YYSTYPE _T8=*_T4;_T5=_T8.YY36;_T7=_T5.val;}{struct _tuple26 yy=_T7;_T6=yy;
# 1279
return _T6;}_TL1FF:
 Cyc_yythrowfail(s);;}
# 1283
static union Cyc_YYSTYPE Cyc_YY36(struct _tuple26 yy1){union Cyc_YYSTYPE _T0;{union Cyc_YYSTYPE _T1;_T1.YY36.tag=43U;_T1.YY36.val=yy1;_T0=_T1;}return _T0;}static char _TmpG2C[17U]="list_t<var_t,`H>";
# 1277 "parse.y"
static struct Cyc_List_List*Cyc_yyget_YY37(union Cyc_YYSTYPE*yy1){union Cyc_YYSTYPE*_T0;union Cyc_YYSTYPE*_T1;struct _union_YYSTYPE_YY37 _T2;unsigned _T3;union Cyc_YYSTYPE*_T4;struct _union_YYSTYPE_YY37 _T5;struct Cyc_List_List*_T6;
static struct _fat_ptr s={(unsigned char*)_TmpG2C,(unsigned char*)_TmpG2C,(unsigned char*)_TmpG2C + 17U};struct Cyc_List_List*_T7;_T0=yy1;_T1=(union Cyc_YYSTYPE*)_T0;_T2=_T1->YY37;_T3=_T2.tag;if(_T3!=44)goto _TL201;_T4=yy1;{union Cyc_YYSTYPE _T8=*_T4;_T5=_T8.YY37;_T7=_T5.val;}{struct Cyc_List_List*yy=_T7;_T6=yy;
# 1280
return _T6;}_TL201:
 Cyc_yythrowfail(s);;}
# 1284
static union Cyc_YYSTYPE Cyc_YY37(struct Cyc_List_List*yy1){union Cyc_YYSTYPE _T0;{union Cyc_YYSTYPE _T1;_T1.YY37.tag=44U;_T1.YY37.val=yy1;_T0=_T1;}return _T0;}static char _TmpG2D[31U]="$(var_opt_t,tqual_t,type_t)@`H";
# 1278 "parse.y"
static struct _tuple8*Cyc_yyget_YY38(union Cyc_YYSTYPE*yy1){union Cyc_YYSTYPE*_T0;union Cyc_YYSTYPE*_T1;struct _union_YYSTYPE_YY38 _T2;unsigned _T3;union Cyc_YYSTYPE*_T4;struct _union_YYSTYPE_YY38 _T5;struct _tuple8*_T6;
static struct _fat_ptr s={(unsigned char*)_TmpG2D,(unsigned char*)_TmpG2D,(unsigned char*)_TmpG2D + 31U};struct _tuple8*_T7;_T0=yy1;_T1=(union Cyc_YYSTYPE*)_T0;_T2=_T1->YY38;_T3=_T2.tag;if(_T3!=45)goto _TL203;_T4=yy1;{union Cyc_YYSTYPE _T8=*_T4;_T5=_T8.YY38;_T7=_T5.val;}{struct _tuple8*yy=_T7;_T6=yy;
# 1281
return _T6;}_TL203:
 Cyc_yythrowfail(s);;}
# 1285
static union Cyc_YYSTYPE Cyc_YY38(struct _tuple8*yy1){union Cyc_YYSTYPE _T0;{union Cyc_YYSTYPE _T1;_T1.YY38.tag=45U;_T1.YY38.val=yy1;_T0=_T1;}return _T0;}static char _TmpG2E[42U]="list_t<$(var_opt_t,tqual_t,type_t)@`H,`H>";
# 1279 "parse.y"
static struct Cyc_List_List*Cyc_yyget_YY39(union Cyc_YYSTYPE*yy1){union Cyc_YYSTYPE*_T0;union Cyc_YYSTYPE*_T1;struct _union_YYSTYPE_YY39 _T2;unsigned _T3;union Cyc_YYSTYPE*_T4;struct _union_YYSTYPE_YY39 _T5;struct Cyc_List_List*_T6;
static struct _fat_ptr s={(unsigned char*)_TmpG2E,(unsigned char*)_TmpG2E,(unsigned char*)_TmpG2E + 42U};struct Cyc_List_List*_T7;_T0=yy1;_T1=(union Cyc_YYSTYPE*)_T0;_T2=_T1->YY39;_T3=_T2.tag;if(_T3!=46)goto _TL205;_T4=yy1;{union Cyc_YYSTYPE _T8=*_T4;_T5=_T8.YY39;_T7=_T5.val;}{struct Cyc_List_List*yy=_T7;_T6=yy;
# 1282
return _T6;}_TL205:
 Cyc_yythrowfail(s);;}
# 1286
static union Cyc_YYSTYPE Cyc_YY39(struct Cyc_List_List*yy1){union Cyc_YYSTYPE _T0;{union Cyc_YYSTYPE _T1;_T1.YY39.tag=46U;_T1.YY39.val=yy1;_T0=_T1;}return _T0;}static char _TmpG2F[139U]="$(list_t<$(var_opt_t,tqual_t,type_t)@`H,`H>, bool,vararg_info_t *`H,type_opt_t, list_t<effconstr_t,`H>, list_t<$(type_t,type_t)@`H,`H>)@`H";
# 1280 "parse.y"
static struct _tuple27*Cyc_yyget_YY40(union Cyc_YYSTYPE*yy1){union Cyc_YYSTYPE*_T0;union Cyc_YYSTYPE*_T1;struct _union_YYSTYPE_YY40 _T2;unsigned _T3;union Cyc_YYSTYPE*_T4;struct _union_YYSTYPE_YY40 _T5;struct _tuple27*_T6;
static struct _fat_ptr s={(unsigned char*)_TmpG2F,(unsigned char*)_TmpG2F,(unsigned char*)_TmpG2F + 139U};struct _tuple27*_T7;_T0=yy1;_T1=(union Cyc_YYSTYPE*)_T0;_T2=_T1->YY40;_T3=_T2.tag;if(_T3!=47)goto _TL207;_T4=yy1;{union Cyc_YYSTYPE _T8=*_T4;_T5=_T8.YY40;_T7=_T5.val;}{struct _tuple27*yy=_T7;_T6=yy;
# 1283
return _T6;}_TL207:
 Cyc_yythrowfail(s);;}
# 1287
static union Cyc_YYSTYPE Cyc_YY40(struct _tuple27*yy1){union Cyc_YYSTYPE _T0;{union Cyc_YYSTYPE _T1;_T1.YY40.tag=47U;_T1.YY40.val=yy1;_T0=_T1;}return _T0;}static char _TmpG30[8U]="types_t";
# 1281 "parse.y"
static struct Cyc_List_List*Cyc_yyget_YY41(union Cyc_YYSTYPE*yy1){union Cyc_YYSTYPE*_T0;union Cyc_YYSTYPE*_T1;struct _union_YYSTYPE_YY41 _T2;unsigned _T3;union Cyc_YYSTYPE*_T4;struct _union_YYSTYPE_YY41 _T5;struct Cyc_List_List*_T6;
static struct _fat_ptr s={(unsigned char*)_TmpG30,(unsigned char*)_TmpG30,(unsigned char*)_TmpG30 + 8U};struct Cyc_List_List*_T7;_T0=yy1;_T1=(union Cyc_YYSTYPE*)_T0;_T2=_T1->YY41;_T3=_T2.tag;if(_T3!=48)goto _TL209;_T4=yy1;{union Cyc_YYSTYPE _T8=*_T4;_T5=_T8.YY41;_T7=_T5.val;}{struct Cyc_List_List*yy=_T7;_T6=yy;
# 1284
return _T6;}_TL209:
 Cyc_yythrowfail(s);;}
# 1288
static union Cyc_YYSTYPE Cyc_YY41(struct Cyc_List_List*yy1){union Cyc_YYSTYPE _T0;{union Cyc_YYSTYPE _T1;_T1.YY41.tag=48U;_T1.YY41.val=yy1;_T0=_T1;}return _T0;}static char _TmpG31[24U]="list_t<designator_t,`H>";
# 1283 "parse.y"
static struct Cyc_List_List*Cyc_yyget_YY42(union Cyc_YYSTYPE*yy1){union Cyc_YYSTYPE*_T0;union Cyc_YYSTYPE*_T1;struct _union_YYSTYPE_YY42 _T2;unsigned _T3;union Cyc_YYSTYPE*_T4;struct _union_YYSTYPE_YY42 _T5;struct Cyc_List_List*_T6;
static struct _fat_ptr s={(unsigned char*)_TmpG31,(unsigned char*)_TmpG31,(unsigned char*)_TmpG31 + 24U};struct Cyc_List_List*_T7;_T0=yy1;_T1=(union Cyc_YYSTYPE*)_T0;_T2=_T1->YY42;_T3=_T2.tag;if(_T3!=49)goto _TL20B;_T4=yy1;{union Cyc_YYSTYPE _T8=*_T4;_T5=_T8.YY42;_T7=_T5.val;}{struct Cyc_List_List*yy=_T7;_T6=yy;
# 1286
return _T6;}_TL20B:
 Cyc_yythrowfail(s);;}
# 1290
static union Cyc_YYSTYPE Cyc_YY42(struct Cyc_List_List*yy1){union Cyc_YYSTYPE _T0;{union Cyc_YYSTYPE _T1;_T1.YY42.tag=49U;_T1.YY42.val=yy1;_T0=_T1;}return _T0;}static char _TmpG32[13U]="designator_t";
# 1284 "parse.y"
static void*Cyc_yyget_YY43(union Cyc_YYSTYPE*yy1){union Cyc_YYSTYPE*_T0;union Cyc_YYSTYPE*_T1;struct _union_YYSTYPE_YY43 _T2;unsigned _T3;union Cyc_YYSTYPE*_T4;struct _union_YYSTYPE_YY43 _T5;void*_T6;
static struct _fat_ptr s={(unsigned char*)_TmpG32,(unsigned char*)_TmpG32,(unsigned char*)_TmpG32 + 13U};void*_T7;_T0=yy1;_T1=(union Cyc_YYSTYPE*)_T0;_T2=_T1->YY43;_T3=_T2.tag;if(_T3!=50)goto _TL20D;_T4=yy1;{union Cyc_YYSTYPE _T8=*_T4;_T5=_T8.YY43;_T7=_T5.val;}{void*yy=_T7;_T6=yy;
# 1287
return _T6;}_TL20D:
 Cyc_yythrowfail(s);;}
# 1291
static union Cyc_YYSTYPE Cyc_YY43(void*yy1){union Cyc_YYSTYPE _T0;{union Cyc_YYSTYPE _T1;_T1.YY43.tag=50U;_T1.YY43.val=yy1;_T0=_T1;}return _T0;}static char _TmpG33[7U]="kind_t";
# 1285 "parse.y"
static struct Cyc_Absyn_Kind*Cyc_yyget_YY44(union Cyc_YYSTYPE*yy1){union Cyc_YYSTYPE*_T0;union Cyc_YYSTYPE*_T1;struct _union_YYSTYPE_YY44 _T2;unsigned _T3;union Cyc_YYSTYPE*_T4;struct _union_YYSTYPE_YY44 _T5;struct Cyc_Absyn_Kind*_T6;
static struct _fat_ptr s={(unsigned char*)_TmpG33,(unsigned char*)_TmpG33,(unsigned char*)_TmpG33 + 7U};struct Cyc_Absyn_Kind*_T7;_T0=yy1;_T1=(union Cyc_YYSTYPE*)_T0;_T2=_T1->YY44;_T3=_T2.tag;if(_T3!=51)goto _TL20F;_T4=yy1;{union Cyc_YYSTYPE _T8=*_T4;_T5=_T8.YY44;_T7=_T5.val;}{struct Cyc_Absyn_Kind*yy=_T7;_T6=yy;
# 1288
return _T6;}_TL20F:
 Cyc_yythrowfail(s);;}
# 1292
static union Cyc_YYSTYPE Cyc_YY44(struct Cyc_Absyn_Kind*yy1){union Cyc_YYSTYPE _T0;{union Cyc_YYSTYPE _T1;_T1.YY44.tag=51U;_T1.YY44.val=yy1;_T0=_T1;}return _T0;}static char _TmpG34[7U]="type_t";
# 1286 "parse.y"
static void*Cyc_yyget_YY45(union Cyc_YYSTYPE*yy1){union Cyc_YYSTYPE*_T0;union Cyc_YYSTYPE*_T1;struct _union_YYSTYPE_YY45 _T2;unsigned _T3;union Cyc_YYSTYPE*_T4;struct _union_YYSTYPE_YY45 _T5;void*_T6;
static struct _fat_ptr s={(unsigned char*)_TmpG34,(unsigned char*)_TmpG34,(unsigned char*)_TmpG34 + 7U};void*_T7;_T0=yy1;_T1=(union Cyc_YYSTYPE*)_T0;_T2=_T1->YY45;_T3=_T2.tag;if(_T3!=52)goto _TL211;_T4=yy1;{union Cyc_YYSTYPE _T8=*_T4;_T5=_T8.YY45;_T7=_T5.val;}{void*yy=_T7;_T6=yy;
# 1289
return _T6;}_TL211:
 Cyc_yythrowfail(s);;}
# 1293
static union Cyc_YYSTYPE Cyc_YY45(void*yy1){union Cyc_YYSTYPE _T0;{union Cyc_YYSTYPE _T1;_T1.YY45.tag=52U;_T1.YY45.val=yy1;_T0=_T1;}return _T0;}static char _TmpG35[23U]="list_t<attribute_t,`H>";
# 1287 "parse.y"
static struct Cyc_List_List*Cyc_yyget_YY46(union Cyc_YYSTYPE*yy1){union Cyc_YYSTYPE*_T0;union Cyc_YYSTYPE*_T1;struct _union_YYSTYPE_YY46 _T2;unsigned _T3;union Cyc_YYSTYPE*_T4;struct _union_YYSTYPE_YY46 _T5;struct Cyc_List_List*_T6;
static struct _fat_ptr s={(unsigned char*)_TmpG35,(unsigned char*)_TmpG35,(unsigned char*)_TmpG35 + 23U};struct Cyc_List_List*_T7;_T0=yy1;_T1=(union Cyc_YYSTYPE*)_T0;_T2=_T1->YY46;_T3=_T2.tag;if(_T3!=53)goto _TL213;_T4=yy1;{union Cyc_YYSTYPE _T8=*_T4;_T5=_T8.YY46;_T7=_T5.val;}{struct Cyc_List_List*yy=_T7;_T6=yy;
# 1290
return _T6;}_TL213:
 Cyc_yythrowfail(s);;}
# 1294
static union Cyc_YYSTYPE Cyc_YY46(struct Cyc_List_List*yy1){union Cyc_YYSTYPE _T0;{union Cyc_YYSTYPE _T1;_T1.YY46.tag=53U;_T1.YY46.val=yy1;_T0=_T1;}return _T0;}static char _TmpG36[12U]="attribute_t";
# 1288 "parse.y"
static void*Cyc_yyget_YY47(union Cyc_YYSTYPE*yy1){union Cyc_YYSTYPE*_T0;union Cyc_YYSTYPE*_T1;struct _union_YYSTYPE_YY47 _T2;unsigned _T3;union Cyc_YYSTYPE*_T4;struct _union_YYSTYPE_YY47 _T5;void*_T6;
static struct _fat_ptr s={(unsigned char*)_TmpG36,(unsigned char*)_TmpG36,(unsigned char*)_TmpG36 + 12U};void*_T7;_T0=yy1;_T1=(union Cyc_YYSTYPE*)_T0;_T2=_T1->YY47;_T3=_T2.tag;if(_T3!=54)goto _TL215;_T4=yy1;{union Cyc_YYSTYPE _T8=*_T4;_T5=_T8.YY47;_T7=_T5.val;}{void*yy=_T7;_T6=yy;
# 1291
return _T6;}_TL215:
 Cyc_yythrowfail(s);;}
# 1295
static union Cyc_YYSTYPE Cyc_YY47(void*yy1){union Cyc_YYSTYPE _T0;{union Cyc_YYSTYPE _T1;_T1.YY47.tag=54U;_T1.YY47.val=yy1;_T0=_T1;}return _T0;}static char _TmpG37[12U]="enumfield_t";
# 1289 "parse.y"
static struct Cyc_Absyn_Enumfield*Cyc_yyget_YY48(union Cyc_YYSTYPE*yy1){union Cyc_YYSTYPE*_T0;union Cyc_YYSTYPE*_T1;struct _union_YYSTYPE_YY48 _T2;unsigned _T3;union Cyc_YYSTYPE*_T4;struct _union_YYSTYPE_YY48 _T5;struct Cyc_Absyn_Enumfield*_T6;
static struct _fat_ptr s={(unsigned char*)_TmpG37,(unsigned char*)_TmpG37,(unsigned char*)_TmpG37 + 12U};struct Cyc_Absyn_Enumfield*_T7;_T0=yy1;_T1=(union Cyc_YYSTYPE*)_T0;_T2=_T1->YY48;_T3=_T2.tag;if(_T3!=55)goto _TL217;_T4=yy1;{union Cyc_YYSTYPE _T8=*_T4;_T5=_T8.YY48;_T7=_T5.val;}{struct Cyc_Absyn_Enumfield*yy=_T7;_T6=yy;
# 1292
return _T6;}_TL217:
 Cyc_yythrowfail(s);;}
# 1296
static union Cyc_YYSTYPE Cyc_YY48(struct Cyc_Absyn_Enumfield*yy1){union Cyc_YYSTYPE _T0;{union Cyc_YYSTYPE _T1;_T1.YY48.tag=55U;_T1.YY48.val=yy1;_T0=_T1;}return _T0;}static char _TmpG38[23U]="list_t<enumfield_t,`H>";
# 1290 "parse.y"
static struct Cyc_List_List*Cyc_yyget_YY49(union Cyc_YYSTYPE*yy1){union Cyc_YYSTYPE*_T0;union Cyc_YYSTYPE*_T1;struct _union_YYSTYPE_YY49 _T2;unsigned _T3;union Cyc_YYSTYPE*_T4;struct _union_YYSTYPE_YY49 _T5;struct Cyc_List_List*_T6;
static struct _fat_ptr s={(unsigned char*)_TmpG38,(unsigned char*)_TmpG38,(unsigned char*)_TmpG38 + 23U};struct Cyc_List_List*_T7;_T0=yy1;_T1=(union Cyc_YYSTYPE*)_T0;_T2=_T1->YY49;_T3=_T2.tag;if(_T3!=56)goto _TL219;_T4=yy1;{union Cyc_YYSTYPE _T8=*_T4;_T5=_T8.YY49;_T7=_T5.val;}{struct Cyc_List_List*yy=_T7;_T6=yy;
# 1293
return _T6;}_TL219:
 Cyc_yythrowfail(s);;}
# 1297
static union Cyc_YYSTYPE Cyc_YY49(struct Cyc_List_List*yy1){union Cyc_YYSTYPE _T0;{union Cyc_YYSTYPE _T1;_T1.YY49.tag=56U;_T1.YY49.val=yy1;_T0=_T1;}return _T0;}static char _TmpG39[11U]="type_opt_t";
# 1291 "parse.y"
static void*Cyc_yyget_YY50(union Cyc_YYSTYPE*yy1){union Cyc_YYSTYPE*_T0;union Cyc_YYSTYPE*_T1;struct _union_YYSTYPE_YY50 _T2;unsigned _T3;union Cyc_YYSTYPE*_T4;struct _union_YYSTYPE_YY50 _T5;void*_T6;
static struct _fat_ptr s={(unsigned char*)_TmpG39,(unsigned char*)_TmpG39,(unsigned char*)_TmpG39 + 11U};void*_T7;_T0=yy1;_T1=(union Cyc_YYSTYPE*)_T0;_T2=_T1->YY50;_T3=_T2.tag;if(_T3!=57)goto _TL21B;_T4=yy1;{union Cyc_YYSTYPE _T8=*_T4;_T5=_T8.YY50;_T7=_T5.val;}{void*yy=_T7;_T6=yy;
# 1294
return _T6;}_TL21B:
 Cyc_yythrowfail(s);;}
# 1298
static union Cyc_YYSTYPE Cyc_YY50(void*yy1){union Cyc_YYSTYPE _T0;{union Cyc_YYSTYPE _T1;_T1.YY50.tag=57U;_T1.YY50.val=yy1;_T0=_T1;}return _T0;}static char _TmpG3A[61U]="$(list_t<effconstr_t,`H>, list_t<$(type_t,type_t)@`H,`H>)*`H";
# 1293 "parse.y"
static struct _tuple28*Cyc_yyget_YY51(union Cyc_YYSTYPE*yy1){union Cyc_YYSTYPE*_T0;union Cyc_YYSTYPE*_T1;struct _union_YYSTYPE_YY51 _T2;unsigned _T3;union Cyc_YYSTYPE*_T4;struct _union_YYSTYPE_YY51 _T5;struct _tuple28*_T6;
static struct _fat_ptr s={(unsigned char*)_TmpG3A,(unsigned char*)_TmpG3A,(unsigned char*)_TmpG3A + 61U};struct _tuple28*_T7;_T0=yy1;_T1=(union Cyc_YYSTYPE*)_T0;_T2=_T1->YY51;_T3=_T2.tag;if(_T3!=58)goto _TL21D;_T4=yy1;{union Cyc_YYSTYPE _T8=*_T4;_T5=_T8.YY51;_T7=_T5.val;}{struct _tuple28*yy=_T7;_T6=yy;
# 1296
return _T6;}_TL21D:
 Cyc_yythrowfail(s);;}
# 1300
static union Cyc_YYSTYPE Cyc_YY51(struct _tuple28*yy1){union Cyc_YYSTYPE _T0;{union Cyc_YYSTYPE _T1;_T1.YY51.tag=58U;_T1.YY51.val=yy1;_T0=_T1;}return _T0;}static char _TmpG3B[12U]="effconstr_t";
# 1294 "parse.y"
static void*Cyc_yyget_YY52(union Cyc_YYSTYPE*yy1){union Cyc_YYSTYPE*_T0;union Cyc_YYSTYPE*_T1;struct _union_YYSTYPE_YY52 _T2;unsigned _T3;union Cyc_YYSTYPE*_T4;struct _union_YYSTYPE_YY52 _T5;void*_T6;
static struct _fat_ptr s={(unsigned char*)_TmpG3B,(unsigned char*)_TmpG3B,(unsigned char*)_TmpG3B + 12U};void*_T7;_T0=yy1;_T1=(union Cyc_YYSTYPE*)_T0;_T2=_T1->YY52;_T3=_T2.tag;if(_T3!=59)goto _TL21F;_T4=yy1;{union Cyc_YYSTYPE _T8=*_T4;_T5=_T8.YY52;_T7=_T5.val;}{void*yy=_T7;_T6=yy;
# 1297
return _T6;}_TL21F:
 Cyc_yythrowfail(s);;}
# 1301
static union Cyc_YYSTYPE Cyc_YY52(void*yy1){union Cyc_YYSTYPE _T0;{union Cyc_YYSTYPE _T1;_T1.YY52.tag=59U;_T1.YY52.val=yy1;_T0=_T1;}return _T0;}static char _TmpG3C[21U]="$(type_t, type_t)@`H";
# 1295 "parse.y"
static struct _tuple29*Cyc_yyget_YY53(union Cyc_YYSTYPE*yy1){union Cyc_YYSTYPE*_T0;union Cyc_YYSTYPE*_T1;struct _union_YYSTYPE_YY53 _T2;unsigned _T3;union Cyc_YYSTYPE*_T4;struct _union_YYSTYPE_YY53 _T5;struct _tuple29*_T6;
static struct _fat_ptr s={(unsigned char*)_TmpG3C,(unsigned char*)_TmpG3C,(unsigned char*)_TmpG3C + 21U};struct _tuple29*_T7;_T0=yy1;_T1=(union Cyc_YYSTYPE*)_T0;_T2=_T1->YY53;_T3=_T2.tag;if(_T3!=60)goto _TL221;_T4=yy1;{union Cyc_YYSTYPE _T8=*_T4;_T5=_T8.YY53;_T7=_T5.val;}{struct _tuple29*yy=_T7;_T6=yy;
# 1298
return _T6;}_TL221:
 Cyc_yythrowfail(s);;}
# 1302
static union Cyc_YYSTYPE Cyc_YY53(struct _tuple29*yy1){union Cyc_YYSTYPE _T0;{union Cyc_YYSTYPE _T1;_T1.YY53.tag=60U;_T1.YY53.val=yy1;_T0=_T1;}return _T0;}static char _TmpG3D[11U]="booltype_t";
# 1296 "parse.y"
static void*Cyc_yyget_YY54(union Cyc_YYSTYPE*yy1){union Cyc_YYSTYPE*_T0;union Cyc_YYSTYPE*_T1;struct _union_YYSTYPE_YY54 _T2;unsigned _T3;union Cyc_YYSTYPE*_T4;struct _union_YYSTYPE_YY54 _T5;void*_T6;
static struct _fat_ptr s={(unsigned char*)_TmpG3D,(unsigned char*)_TmpG3D,(unsigned char*)_TmpG3D + 11U};void*_T7;_T0=yy1;_T1=(union Cyc_YYSTYPE*)_T0;_T2=_T1->YY54;_T3=_T2.tag;if(_T3!=61)goto _TL223;_T4=yy1;{union Cyc_YYSTYPE _T8=*_T4;_T5=_T8.YY54;_T7=_T5.val;}{void*yy=_T7;_T6=yy;
# 1299
return _T6;}_TL223:
 Cyc_yythrowfail(s);;}
# 1303
static union Cyc_YYSTYPE Cyc_YY54(void*yy1){union Cyc_YYSTYPE _T0;{union Cyc_YYSTYPE _T1;_T1.YY54.tag=61U;_T1.YY54.val=yy1;_T0=_T1;}return _T0;}static char _TmpG3E[35U]="list_t<$(seg_t,qvar_t,bool)@`H,`H>";
# 1297 "parse.y"
static struct Cyc_List_List*Cyc_yyget_YY55(union Cyc_YYSTYPE*yy1){union Cyc_YYSTYPE*_T0;union Cyc_YYSTYPE*_T1;struct _union_YYSTYPE_YY55 _T2;unsigned _T3;union Cyc_YYSTYPE*_T4;struct _union_YYSTYPE_YY55 _T5;struct Cyc_List_List*_T6;
static struct _fat_ptr s={(unsigned char*)_TmpG3E,(unsigned char*)_TmpG3E,(unsigned char*)_TmpG3E + 35U};struct Cyc_List_List*_T7;_T0=yy1;_T1=(union Cyc_YYSTYPE*)_T0;_T2=_T1->YY55;_T3=_T2.tag;if(_T3!=62)goto _TL225;_T4=yy1;{union Cyc_YYSTYPE _T8=*_T4;_T5=_T8.YY55;_T7=_T5.val;}{struct Cyc_List_List*yy=_T7;_T6=yy;
# 1300
return _T6;}_TL225:
 Cyc_yythrowfail(s);;}
# 1304
static union Cyc_YYSTYPE Cyc_YY55(struct Cyc_List_List*yy1){union Cyc_YYSTYPE _T0;{union Cyc_YYSTYPE _T1;_T1.YY55.tag=62U;_T1.YY55.val=yy1;_T0=_T1;}return _T0;}static char _TmpG3F[48U]="$(list_t<$(seg_t,qvar_t,bool)@`H,`H>, seg_t)@`H";
# 1298 "parse.y"
static struct _tuple30*Cyc_yyget_YY56(union Cyc_YYSTYPE*yy1){union Cyc_YYSTYPE*_T0;union Cyc_YYSTYPE*_T1;struct _union_YYSTYPE_YY56 _T2;unsigned _T3;union Cyc_YYSTYPE*_T4;struct _union_YYSTYPE_YY56 _T5;struct _tuple30*_T6;
static struct _fat_ptr s={(unsigned char*)_TmpG3F,(unsigned char*)_TmpG3F,(unsigned char*)_TmpG3F + 48U};struct _tuple30*_T7;_T0=yy1;_T1=(union Cyc_YYSTYPE*)_T0;_T2=_T1->YY56;_T3=_T2.tag;if(_T3!=63)goto _TL227;_T4=yy1;{union Cyc_YYSTYPE _T8=*_T4;_T5=_T8.YY56;_T7=_T5.val;}{struct _tuple30*yy=_T7;_T6=yy;
# 1301
return _T6;}_TL227:
 Cyc_yythrowfail(s);;}
# 1305
static union Cyc_YYSTYPE Cyc_YY56(struct _tuple30*yy1){union Cyc_YYSTYPE _T0;{union Cyc_YYSTYPE _T1;_T1.YY56.tag=63U;_T1.YY56.val=yy1;_T0=_T1;}return _T0;}static char _TmpG40[18U]="list_t<qvar_t,`H>";
# 1299 "parse.y"
static struct Cyc_List_List*Cyc_yyget_YY57(union Cyc_YYSTYPE*yy1){union Cyc_YYSTYPE*_T0;union Cyc_YYSTYPE*_T1;struct _union_YYSTYPE_YY57 _T2;unsigned _T3;union Cyc_YYSTYPE*_T4;struct _union_YYSTYPE_YY57 _T5;struct Cyc_List_List*_T6;
static struct _fat_ptr s={(unsigned char*)_TmpG40,(unsigned char*)_TmpG40,(unsigned char*)_TmpG40 + 18U};struct Cyc_List_List*_T7;_T0=yy1;_T1=(union Cyc_YYSTYPE*)_T0;_T2=_T1->YY57;_T3=_T2.tag;if(_T3!=64)goto _TL229;_T4=yy1;{union Cyc_YYSTYPE _T8=*_T4;_T5=_T8.YY57;_T7=_T5.val;}{struct Cyc_List_List*yy=_T7;_T6=yy;
# 1302
return _T6;}_TL229:
 Cyc_yythrowfail(s);;}
# 1306
static union Cyc_YYSTYPE Cyc_YY57(struct Cyc_List_List*yy1){union Cyc_YYSTYPE _T0;{union Cyc_YYSTYPE _T1;_T1.YY57.tag=64U;_T1.YY57.val=yy1;_T0=_T1;}return _T0;}static char _TmpG41[12U]="aqualtype_t";
# 1300 "parse.y"
static void*Cyc_yyget_YY58(union Cyc_YYSTYPE*yy1){union Cyc_YYSTYPE*_T0;union Cyc_YYSTYPE*_T1;struct _union_YYSTYPE_YY58 _T2;unsigned _T3;union Cyc_YYSTYPE*_T4;struct _union_YYSTYPE_YY58 _T5;void*_T6;
static struct _fat_ptr s={(unsigned char*)_TmpG41,(unsigned char*)_TmpG41,(unsigned char*)_TmpG41 + 12U};void*_T7;_T0=yy1;_T1=(union Cyc_YYSTYPE*)_T0;_T2=_T1->YY58;_T3=_T2.tag;if(_T3!=65)goto _TL22B;_T4=yy1;{union Cyc_YYSTYPE _T8=*_T4;_T5=_T8.YY58;_T7=_T5.val;}{void*yy=_T7;_T6=yy;
# 1303
return _T6;}_TL22B:
 Cyc_yythrowfail(s);;}
# 1307
static union Cyc_YYSTYPE Cyc_YY58(void*yy1){union Cyc_YYSTYPE _T0;{union Cyc_YYSTYPE _T1;_T1.YY58.tag=65U;_T1.YY58.val=yy1;_T0=_T1;}return _T0;}static char _TmpG42[20U]="pointer_qual_t<`yy>";
# 1301 "parse.y"
static void*Cyc_yyget_YY59(union Cyc_YYSTYPE*yy1){union Cyc_YYSTYPE*_T0;union Cyc_YYSTYPE*_T1;struct _union_YYSTYPE_YY59 _T2;unsigned _T3;union Cyc_YYSTYPE*_T4;struct _union_YYSTYPE_YY59 _T5;void*_T6;
static struct _fat_ptr s={(unsigned char*)_TmpG42,(unsigned char*)_TmpG42,(unsigned char*)_TmpG42 + 20U};void*_T7;_T0=yy1;_T1=(union Cyc_YYSTYPE*)_T0;_T2=_T1->YY59;_T3=_T2.tag;if(_T3!=66)goto _TL22D;_T4=yy1;{union Cyc_YYSTYPE _T8=*_T4;_T5=_T8.YY59;_T7=_T5.val;}{void*yy=_T7;_T6=yy;
# 1304
return _T6;}_TL22D:
 Cyc_yythrowfail(s);;}
# 1308
static union Cyc_YYSTYPE Cyc_YY59(void*yy1){union Cyc_YYSTYPE _T0;{union Cyc_YYSTYPE _T1;_T1.YY59.tag=66U;_T1.YY59.val=yy1;_T0=_T1;}return _T0;}static char _TmpG43[21U]="pointer_quals_t<`yy>";
# 1302 "parse.y"
static struct Cyc_List_List*Cyc_yyget_YY60(union Cyc_YYSTYPE*yy1){union Cyc_YYSTYPE*_T0;union Cyc_YYSTYPE*_T1;struct _union_YYSTYPE_YY60 _T2;unsigned _T3;union Cyc_YYSTYPE*_T4;struct _union_YYSTYPE_YY60 _T5;struct Cyc_List_List*_T6;
static struct _fat_ptr s={(unsigned char*)_TmpG43,(unsigned char*)_TmpG43,(unsigned char*)_TmpG43 + 21U};struct Cyc_List_List*_T7;_T0=yy1;_T1=(union Cyc_YYSTYPE*)_T0;_T2=_T1->YY60;_T3=_T2.tag;if(_T3!=67)goto _TL22F;_T4=yy1;{union Cyc_YYSTYPE _T8=*_T4;_T5=_T8.YY60;_T7=_T5.val;}{struct Cyc_List_List*yy=_T7;_T6=yy;
# 1305
return _T6;}_TL22F:
 Cyc_yythrowfail(s);;}
# 1309
static union Cyc_YYSTYPE Cyc_YY60(struct Cyc_List_List*yy1){union Cyc_YYSTYPE _T0;{union Cyc_YYSTYPE _T1;_T1.YY60.tag=67U;_T1.YY60.val=yy1;_T0=_T1;}return _T0;}static char _TmpG44[10U]="exp_opt_t";
# 1303 "parse.y"
static struct Cyc_Absyn_Exp*Cyc_yyget_YY61(union Cyc_YYSTYPE*yy1){union Cyc_YYSTYPE*_T0;union Cyc_YYSTYPE*_T1;struct _union_YYSTYPE_YY61 _T2;unsigned _T3;union Cyc_YYSTYPE*_T4;struct _union_YYSTYPE_YY61 _T5;struct Cyc_Absyn_Exp*_T6;
static struct _fat_ptr s={(unsigned char*)_TmpG44,(unsigned char*)_TmpG44,(unsigned char*)_TmpG44 + 10U};struct Cyc_Absyn_Exp*_T7;_T0=yy1;_T1=(union Cyc_YYSTYPE*)_T0;_T2=_T1->YY61;_T3=_T2.tag;if(_T3!=68)goto _TL231;_T4=yy1;{union Cyc_YYSTYPE _T8=*_T4;_T5=_T8.YY61;_T7=_T5.val;}{struct Cyc_Absyn_Exp*yy=_T7;_T6=yy;
# 1306
return _T6;}_TL231:
 Cyc_yythrowfail(s);;}
# 1310
static union Cyc_YYSTYPE Cyc_YY61(struct Cyc_Absyn_Exp*yy1){union Cyc_YYSTYPE _T0;{union Cyc_YYSTYPE _T1;_T1.YY61.tag=68U;_T1.YY61.val=yy1;_T0=_T1;}return _T0;}static char _TmpG45[43U]="$(exp_opt_t,exp_opt_t,exp_opt_t,exp_opt_t)";
# 1304 "parse.y"
static struct _tuple21 Cyc_yyget_YY62(union Cyc_YYSTYPE*yy1){union Cyc_YYSTYPE*_T0;union Cyc_YYSTYPE*_T1;struct _union_YYSTYPE_YY62 _T2;unsigned _T3;union Cyc_YYSTYPE*_T4;struct _union_YYSTYPE_YY62 _T5;struct _tuple21 _T6;
static struct _fat_ptr s={(unsigned char*)_TmpG45,(unsigned char*)_TmpG45,(unsigned char*)_TmpG45 + 43U};struct _tuple21 _T7;_T0=yy1;_T1=(union Cyc_YYSTYPE*)_T0;_T2=_T1->YY62;_T3=_T2.tag;if(_T3!=69)goto _TL233;_T4=yy1;{union Cyc_YYSTYPE _T8=*_T4;_T5=_T8.YY62;_T7=_T5.val;}{struct _tuple21 yy=_T7;_T6=yy;
# 1307
return _T6;}_TL233:
 Cyc_yythrowfail(s);;}
# 1311
static union Cyc_YYSTYPE Cyc_YY62(struct _tuple21 yy1){union Cyc_YYSTYPE _T0;{union Cyc_YYSTYPE _T1;_T1.YY62.tag=69U;_T1.YY62.val=yy1;_T0=_T1;}return _T0;}static char _TmpG46[10U]="raw_exp_t";
# 1306 "parse.y"
static void*Cyc_yyget_YY63(union Cyc_YYSTYPE*yy1){union Cyc_YYSTYPE*_T0;union Cyc_YYSTYPE*_T1;struct _union_YYSTYPE_YY63 _T2;unsigned _T3;union Cyc_YYSTYPE*_T4;struct _union_YYSTYPE_YY63 _T5;void*_T6;
static struct _fat_ptr s={(unsigned char*)_TmpG46,(unsigned char*)_TmpG46,(unsigned char*)_TmpG46 + 10U};void*_T7;_T0=yy1;_T1=(union Cyc_YYSTYPE*)_T0;_T2=_T1->YY63;_T3=_T2.tag;if(_T3!=70)goto _TL235;_T4=yy1;{union Cyc_YYSTYPE _T8=*_T4;_T5=_T8.YY63;_T7=_T5.val;}{void*yy=_T7;_T6=yy;
# 1309
return _T6;}_TL235:
 Cyc_yythrowfail(s);;}
# 1313
static union Cyc_YYSTYPE Cyc_YY63(void*yy1){union Cyc_YYSTYPE _T0;{union Cyc_YYSTYPE _T1;_T1.YY63.tag=70U;_T1.YY63.val=yy1;_T0=_T1;}return _T0;}static char _TmpG47[112U]="$(list_t<$(string_t<`H>, exp_t)@`H, `H>, list_t<$(string_t<`H>, exp_t)@`H, `H>, list_t<string_t<`H>@`H, `H>)@`H";
# 1307 "parse.y"
static struct _tuple31*Cyc_yyget_YY64(union Cyc_YYSTYPE*yy1){union Cyc_YYSTYPE*_T0;union Cyc_YYSTYPE*_T1;struct _union_YYSTYPE_YY64 _T2;unsigned _T3;union Cyc_YYSTYPE*_T4;struct _union_YYSTYPE_YY64 _T5;struct _tuple31*_T6;
static struct _fat_ptr s={(unsigned char*)_TmpG47,(unsigned char*)_TmpG47,(unsigned char*)_TmpG47 + 112U};struct _tuple31*_T7;_T0=yy1;_T1=(union Cyc_YYSTYPE*)_T0;_T2=_T1->YY64;_T3=_T2.tag;if(_T3!=71)goto _TL237;_T4=yy1;{union Cyc_YYSTYPE _T8=*_T4;_T5=_T8.YY64;_T7=_T5.val;}{struct _tuple31*yy=_T7;_T6=yy;
# 1310
return _T6;}_TL237:
 Cyc_yythrowfail(s);;}
# 1314
static union Cyc_YYSTYPE Cyc_YY64(struct _tuple31*yy1){union Cyc_YYSTYPE _T0;{union Cyc_YYSTYPE _T1;_T1.YY64.tag=71U;_T1.YY64.val=yy1;_T0=_T1;}return _T0;}static char _TmpG48[73U]="$(list_t<$(string_t<`H>, exp_t)@`H, `H>, list_t<string_t<`H>@`H, `H>)@`H";
# 1308 "parse.y"
static struct _tuple28*Cyc_yyget_YY65(union Cyc_YYSTYPE*yy1){union Cyc_YYSTYPE*_T0;union Cyc_YYSTYPE*_T1;struct _union_YYSTYPE_YY65 _T2;unsigned _T3;union Cyc_YYSTYPE*_T4;struct _union_YYSTYPE_YY65 _T5;struct _tuple28*_T6;
static struct _fat_ptr s={(unsigned char*)_TmpG48,(unsigned char*)_TmpG48,(unsigned char*)_TmpG48 + 73U};struct _tuple28*_T7;_T0=yy1;_T1=(union Cyc_YYSTYPE*)_T0;_T2=_T1->YY65;_T3=_T2.tag;if(_T3!=72)goto _TL239;_T4=yy1;{union Cyc_YYSTYPE _T8=*_T4;_T5=_T8.YY65;_T7=_T5.val;}{struct _tuple28*yy=_T7;_T6=yy;
# 1311
return _T6;}_TL239:
 Cyc_yythrowfail(s);;}
# 1315
static union Cyc_YYSTYPE Cyc_YY65(struct _tuple28*yy1){union Cyc_YYSTYPE _T0;{union Cyc_YYSTYPE _T1;_T1.YY65.tag=72U;_T1.YY65.val=yy1;_T0=_T1;}return _T0;}static char _TmpG49[28U]="list_t<string_t<`H>@`H, `H>";
# 1309 "parse.y"
static struct Cyc_List_List*Cyc_yyget_YY66(union Cyc_YYSTYPE*yy1){union Cyc_YYSTYPE*_T0;union Cyc_YYSTYPE*_T1;struct _union_YYSTYPE_YY66 _T2;unsigned _T3;union Cyc_YYSTYPE*_T4;struct _union_YYSTYPE_YY66 _T5;struct Cyc_List_List*_T6;
static struct _fat_ptr s={(unsigned char*)_TmpG49,(unsigned char*)_TmpG49,(unsigned char*)_TmpG49 + 28U};struct Cyc_List_List*_T7;_T0=yy1;_T1=(union Cyc_YYSTYPE*)_T0;_T2=_T1->YY66;_T3=_T2.tag;if(_T3!=73)goto _TL23B;_T4=yy1;{union Cyc_YYSTYPE _T8=*_T4;_T5=_T8.YY66;_T7=_T5.val;}{struct Cyc_List_List*yy=_T7;_T6=yy;
# 1312
return _T6;}_TL23B:
 Cyc_yythrowfail(s);;}
# 1316
static union Cyc_YYSTYPE Cyc_YY66(struct Cyc_List_List*yy1){union Cyc_YYSTYPE _T0;{union Cyc_YYSTYPE _T1;_T1.YY66.tag=73U;_T1.YY66.val=yy1;_T0=_T1;}return _T0;}static char _TmpG4A[38U]="list_t<$(string_t<`H>, exp_t)@`H, `H>";
# 1310 "parse.y"
static struct Cyc_List_List*Cyc_yyget_YY67(union Cyc_YYSTYPE*yy1){union Cyc_YYSTYPE*_T0;union Cyc_YYSTYPE*_T1;struct _union_YYSTYPE_YY67 _T2;unsigned _T3;union Cyc_YYSTYPE*_T4;struct _union_YYSTYPE_YY67 _T5;struct Cyc_List_List*_T6;
static struct _fat_ptr s={(unsigned char*)_TmpG4A,(unsigned char*)_TmpG4A,(unsigned char*)_TmpG4A + 38U};struct Cyc_List_List*_T7;_T0=yy1;_T1=(union Cyc_YYSTYPE*)_T0;_T2=_T1->YY67;_T3=_T2.tag;if(_T3!=74)goto _TL23D;_T4=yy1;{union Cyc_YYSTYPE _T8=*_T4;_T5=_T8.YY67;_T7=_T5.val;}{struct Cyc_List_List*yy=_T7;_T6=yy;
# 1313
return _T6;}_TL23D:
 Cyc_yythrowfail(s);;}
# 1317
static union Cyc_YYSTYPE Cyc_YY67(struct Cyc_List_List*yy1){union Cyc_YYSTYPE _T0;{union Cyc_YYSTYPE _T1;_T1.YY67.tag=74U;_T1.YY67.val=yy1;_T0=_T1;}return _T0;}static char _TmpG4B[26U]="$(string_t<`H>, exp_t)@`H";
# 1311 "parse.y"
static struct _tuple32*Cyc_yyget_YY68(union Cyc_YYSTYPE*yy1){union Cyc_YYSTYPE*_T0;union Cyc_YYSTYPE*_T1;struct _union_YYSTYPE_YY68 _T2;unsigned _T3;union Cyc_YYSTYPE*_T4;struct _union_YYSTYPE_YY68 _T5;struct _tuple32*_T6;
static struct _fat_ptr s={(unsigned char*)_TmpG4B,(unsigned char*)_TmpG4B,(unsigned char*)_TmpG4B + 26U};struct _tuple32*_T7;_T0=yy1;_T1=(union Cyc_YYSTYPE*)_T0;_T2=_T1->YY68;_T3=_T2.tag;if(_T3!=75)goto _TL23F;_T4=yy1;{union Cyc_YYSTYPE _T8=*_T4;_T5=_T8.YY68;_T7=_T5.val;}{struct _tuple32*yy=_T7;_T6=yy;
# 1314
return _T6;}_TL23F:
 Cyc_yythrowfail(s);;}
# 1318
static union Cyc_YYSTYPE Cyc_YY68(struct _tuple32*yy1){union Cyc_YYSTYPE _T0;{union Cyc_YYSTYPE _T1;_T1.YY68.tag=75U;_T1.YY68.val=yy1;_T0=_T1;}return _T0;}static char _TmpG4C[16U]="exp_maker_fun_t";
# 1312 "parse.y"
static struct Cyc_Absyn_Exp*(*Cyc_yyget_YY69(union Cyc_YYSTYPE*yy1))(struct Cyc_Absyn_Exp*,struct Cyc_Absyn_Exp*,unsigned){union Cyc_YYSTYPE*_T0;union Cyc_YYSTYPE*_T1;struct _union_YYSTYPE_YY69 _T2;unsigned _T3;union Cyc_YYSTYPE*_T4;struct _union_YYSTYPE_YY69 _T5;struct Cyc_Absyn_Exp*(*_T6)(struct Cyc_Absyn_Exp*,struct Cyc_Absyn_Exp*,unsigned);
static struct _fat_ptr s={(unsigned char*)_TmpG4C,(unsigned char*)_TmpG4C,(unsigned char*)_TmpG4C + 16U};struct Cyc_Absyn_Exp*(*_T7)(struct Cyc_Absyn_Exp*,struct Cyc_Absyn_Exp*,unsigned);_T0=yy1;_T1=(union Cyc_YYSTYPE*)_T0;_T2=_T1->YY69;_T3=_T2.tag;if(_T3!=76)goto _TL241;_T4=yy1;{union Cyc_YYSTYPE _T8=*_T4;_T5=_T8.YY69;_T7=_T5.val;}{struct Cyc_Absyn_Exp*(*yy)(struct Cyc_Absyn_Exp*,struct Cyc_Absyn_Exp*,unsigned)=_T7;_T6=yy;
# 1315
return _T6;}_TL241:
 Cyc_yythrowfail(s);;}
# 1319
static union Cyc_YYSTYPE Cyc_YY69(struct Cyc_Absyn_Exp*(*yy1)(struct Cyc_Absyn_Exp*,struct Cyc_Absyn_Exp*,unsigned)){union Cyc_YYSTYPE _T0;{union Cyc_YYSTYPE _T1;_T1.YY69.tag=76U;_T1.YY69.val=yy1;_T0=_T1;}return _T0;}static char _TmpG4D[19U]="enum ConstraintOps";
# 1314 "parse.y"
static enum Cyc_Parse_ConstraintOps Cyc_yyget_YY70(union Cyc_YYSTYPE*yy1){union Cyc_YYSTYPE*_T0;union Cyc_YYSTYPE*_T1;struct _union_YYSTYPE_YY70 _T2;unsigned _T3;union Cyc_YYSTYPE*_T4;struct _union_YYSTYPE_YY70 _T5;enum Cyc_Parse_ConstraintOps _T6;
static struct _fat_ptr s={(unsigned char*)_TmpG4D,(unsigned char*)_TmpG4D,(unsigned char*)_TmpG4D + 19U};enum Cyc_Parse_ConstraintOps _T7;_T0=yy1;_T1=(union Cyc_YYSTYPE*)_T0;_T2=_T1->YY70;_T3=_T2.tag;if(_T3!=77)goto _TL243;_T4=yy1;{union Cyc_YYSTYPE _T8=*_T4;_T5=_T8.YY70;_T7=_T5.val;}{enum Cyc_Parse_ConstraintOps yy=_T7;_T6=yy;
# 1317
return _T6;}_TL243:
 Cyc_yythrowfail(s);;}
# 1321
static union Cyc_YYSTYPE Cyc_YY70(enum Cyc_Parse_ConstraintOps yy1){union Cyc_YYSTYPE _T0;{union Cyc_YYSTYPE _T1;_T1.YY70.tag=77U;_T1.YY70.val=yy1;_T0=_T1;}return _T0;}static char _TmpG4E[60U]="list_t<$(type_t, list_t<BansheeIf::constraint_t,`H>)@`H,`H>";
# 1315 "parse.y"
static struct Cyc_List_List*Cyc_yyget_YY71(union Cyc_YYSTYPE*yy1){union Cyc_YYSTYPE*_T0;union Cyc_YYSTYPE*_T1;struct _union_YYSTYPE_YY71 _T2;unsigned _T3;union Cyc_YYSTYPE*_T4;struct _union_YYSTYPE_YY71 _T5;struct Cyc_List_List*_T6;
static struct _fat_ptr s={(unsigned char*)_TmpG4E,(unsigned char*)_TmpG4E,(unsigned char*)_TmpG4E + 60U};struct Cyc_List_List*_T7;_T0=yy1;_T1=(union Cyc_YYSTYPE*)_T0;_T2=_T1->YY71;_T3=_T2.tag;if(_T3!=78)goto _TL245;_T4=yy1;{union Cyc_YYSTYPE _T8=*_T4;_T5=_T8.YY71;_T7=_T5.val;}{struct Cyc_List_List*yy=_T7;_T6=yy;
# 1318
return _T6;}_TL245:
 Cyc_yythrowfail(s);;}
# 1322
static union Cyc_YYSTYPE Cyc_YY71(struct Cyc_List_List*yy1){union Cyc_YYSTYPE _T0;{union Cyc_YYSTYPE _T1;_T1.YY71.tag=78U;_T1.YY71.val=yy1;_T0=_T1;}return _T0;}static char _TmpG4F[35U]="list_t<BansheeIf::constraint_t,`H>";
# 1316 "parse.y"
static struct Cyc_List_List*Cyc_yyget_YY72(union Cyc_YYSTYPE*yy1){union Cyc_YYSTYPE*_T0;union Cyc_YYSTYPE*_T1;struct _union_YYSTYPE_YY72 _T2;unsigned _T3;union Cyc_YYSTYPE*_T4;struct _union_YYSTYPE_YY72 _T5;struct Cyc_List_List*_T6;
static struct _fat_ptr s={(unsigned char*)_TmpG4F,(unsigned char*)_TmpG4F,(unsigned char*)_TmpG4F + 35U};struct Cyc_List_List*_T7;_T0=yy1;_T1=(union Cyc_YYSTYPE*)_T0;_T2=_T1->YY72;_T3=_T2.tag;if(_T3!=79)goto _TL247;_T4=yy1;{union Cyc_YYSTYPE _T8=*_T4;_T5=_T8.YY72;_T7=_T5.val;}{struct Cyc_List_List*yy=_T7;_T6=yy;
# 1319
return _T6;}_TL247:
 Cyc_yythrowfail(s);;}
# 1323
static union Cyc_YYSTYPE Cyc_YY72(struct Cyc_List_List*yy1){union Cyc_YYSTYPE _T0;{union Cyc_YYSTYPE _T1;_T1.YY72.tag=79U;_T1.YY72.val=yy1;_T0=_T1;}return _T0;}static char _TmpG50[24U]="BansheeIf::constraint_t";
# 1317 "parse.y"
static void*Cyc_yyget_YY73(union Cyc_YYSTYPE*yy1){union Cyc_YYSTYPE*_T0;union Cyc_YYSTYPE*_T1;struct _union_YYSTYPE_YY73 _T2;unsigned _T3;union Cyc_YYSTYPE*_T4;struct _union_YYSTYPE_YY73 _T5;void*_T6;
static struct _fat_ptr s={(unsigned char*)_TmpG50,(unsigned char*)_TmpG50,(unsigned char*)_TmpG50 + 24U};void*_T7;_T0=yy1;_T1=(union Cyc_YYSTYPE*)_T0;_T2=_T1->YY73;_T3=_T2.tag;if(_T3!=80)goto _TL249;_T4=yy1;{union Cyc_YYSTYPE _T8=*_T4;_T5=_T8.YY73;_T7=_T5.val;}{void*yy=_T7;_T6=yy;
# 1320
return _T6;}_TL249:
 Cyc_yythrowfail(s);;}
# 1324
static union Cyc_YYSTYPE Cyc_YY73(void*yy1){union Cyc_YYSTYPE _T0;{union Cyc_YYSTYPE _T1;_T1.YY73.tag=80U;_T1.YY73.val=yy1;_T0=_T1;}return _T0;}static char _TmpG51[4U]="int";
# 1319 "parse.y"
static int Cyc_yyget_YY74(union Cyc_YYSTYPE*yy1){union Cyc_YYSTYPE*_T0;union Cyc_YYSTYPE*_T1;struct _union_YYSTYPE_YY74 _T2;unsigned _T3;union Cyc_YYSTYPE*_T4;struct _union_YYSTYPE_YY74 _T5;int _T6;
static struct _fat_ptr s={(unsigned char*)_TmpG51,(unsigned char*)_TmpG51,(unsigned char*)_TmpG51 + 4U};int _T7;_T0=yy1;_T1=(union Cyc_YYSTYPE*)_T0;_T2=_T1->YY74;_T3=_T2.tag;if(_T3!=81)goto _TL24B;_T4=yy1;{union Cyc_YYSTYPE _T8=*_T4;_T5=_T8.YY74;_T7=_T5.val;}{int yy=_T7;_T6=yy;
# 1322
return _T6;}_TL24B:
 Cyc_yythrowfail(s);;}
# 1326
static union Cyc_YYSTYPE Cyc_YY74(int yy1){union Cyc_YYSTYPE _T0;{union Cyc_YYSTYPE _T1;_T1.YY74.tag=81U;_T1.YY74.val=yy1;_T0=_T1;}return _T0;}struct Cyc_Yyltype{int timestamp;unsigned first_line;unsigned first_column;unsigned last_line;unsigned last_column;};
# 1342
struct Cyc_Yyltype Cyc_yynewloc (void){struct Cyc_Yyltype _T0;{struct Cyc_Yyltype _T1;
_T1.timestamp=0;_T1.first_line=0U;_T1.first_column=0U;_T1.last_line=0U;_T1.last_column=0U;_T0=_T1;}return _T0;}
# 1345
struct Cyc_Yyltype Cyc_yylloc={0,0U,0U,0U,0U};
# 1356 "parse.y"
static short Cyc_yytranslate[403U]={0,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,174,2,2,159,172,169,2,156,157,152,167,151,170,162,171,2,2,2,2,2,2,2,2,2,2,161,148,154,153,155,166,165,175,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,176,2,2,2,2,163,2,164,168,158,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,149,160,150,173,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,52,53,54,55,56,57,58,59,60,61,62,63,64,65,66,67,68,69,70,71,72,73,74,75,76,77,78,79,80,81,82,83,84,85,86,87,88,89,90,91,92,93,94,95,96,97,98,99,100,101,102,103,104,105,106,107,108,109,110,111,112,113,114,115,116,117,118,119,120,121,122,123,124,125,126,127,128,129,130,131,132,133,134,135,136,137,138,139,140,141,142,143,144,145,146,147};static char _TmpG52[2U]="$";static char _TmpG53[6U]="error";static char _TmpG54[12U]="$undefined.";static char _TmpG55[5U]="AUTO";static char _TmpG56[9U]="REGISTER";static char _TmpG57[7U]="STATIC";static char _TmpG58[7U]="EXTERN";static char _TmpG59[8U]="TYPEDEF";static char _TmpG5A[5U]="VOID";static char _TmpG5B[5U]="CHAR";static char _TmpG5C[6U]="SHORT";static char _TmpG5D[4U]="INT";static char _TmpG5E[5U]="LONG";static char _TmpG5F[6U]="FLOAT";static char _TmpG60[9U]="FLOAT128";static char _TmpG61[7U]="DOUBLE";static char _TmpG62[7U]="SIGNED";static char _TmpG63[9U]="UNSIGNED";static char _TmpG64[6U]="CONST";static char _TmpG65[9U]="VOLATILE";static char _TmpG66[9U]="RESTRICT";static char _TmpG67[7U]="STRUCT";static char _TmpG68[6U]="UNION";static char _TmpG69[5U]="CASE";static char _TmpG6A[8U]="DEFAULT";static char _TmpG6B[7U]="INLINE";static char _TmpG6C[7U]="SIZEOF";static char _TmpG6D[9U]="OFFSETOF";static char _TmpG6E[3U]="IF";static char _TmpG6F[5U]="ELSE";static char _TmpG70[7U]="SWITCH";static char _TmpG71[6U]="WHILE";static char _TmpG72[3U]="DO";static char _TmpG73[4U]="FOR";static char _TmpG74[5U]="GOTO";static char _TmpG75[9U]="CONTINUE";static char _TmpG76[6U]="BREAK";static char _TmpG77[7U]="RETURN";static char _TmpG78[5U]="ENUM";static char _TmpG79[7U]="TYPEOF";static char _TmpG7A[16U]="BUILTIN_VA_LIST";static char _TmpG7B[10U]="EXTENSION";static char _TmpG7C[8U]="COMPLEX";static char _TmpG7D[8U]="NULL_kw";static char _TmpG7E[4U]="LET";static char _TmpG7F[6U]="THROW";static char _TmpG80[4U]="TRY";static char _TmpG81[6U]="CATCH";static char _TmpG82[7U]="EXPORT";static char _TmpG83[9U]="OVERRIDE";static char _TmpG84[5U]="HIDE";static char _TmpG85[4U]="NEW";static char _TmpG86[5U]="QNEW";static char _TmpG87[9U]="ABSTRACT";static char _TmpG88[9U]="FALLTHRU";static char _TmpG89[6U]="USING";static char _TmpG8A[10U]="NAMESPACE";static char _TmpG8B[12U]="NOINFERENCE";static char _TmpG8C[9U]="DATATYPE";static char _TmpG8D[7U]="MALLOC";static char _TmpG8E[8U]="RMALLOC";static char _TmpG8F[9U]="RVMALLOC";static char _TmpG90[15U]="RMALLOC_INLINE";static char _TmpG91[8U]="QMALLOC";static char _TmpG92[7U]="CALLOC";static char _TmpG93[8U]="QCALLOC";static char _TmpG94[8U]="RCALLOC";static char _TmpG95[5U]="SWAP";static char _TmpG96[7U]="ASSERT";static char _TmpG97[9U]="REGION_T";static char _TmpG98[6U]="TAG_T";static char _TmpG99[7U]="REGION";static char _TmpG9A[5U]="RNEW";static char _TmpG9B[8U]="REGIONS";static char _TmpG9C[7U]="PORTON";static char _TmpG9D[8U]="PORTOFF";static char _TmpG9E[7U]="PRAGMA";static char _TmpG9F[10U]="TEMPESTON";static char _TmpGA0[11U]="TEMPESTOFF";static char _TmpGA1[13U]="AQ_ALIASABLE";static char _TmpGA2[10U]="AQ_REFCNT";static char _TmpGA3[14U]="AQ_RESTRICTED";static char _TmpGA4[10U]="AQ_UNIQUE";static char _TmpGA5[8U]="AQUAL_T";static char _TmpGA6[8U]="NUMELTS";static char _TmpGA7[6U]="TAGOF";static char _TmpGA8[8U]="VALUEOF";static char _TmpGA9[10U]="VALUEOF_T";static char _TmpGAA[9U]="TAGCHECK";static char _TmpGAB[13U]="NUMELTS_QUAL";static char _TmpGAC[10U]="THIN_QUAL";static char _TmpGAD[9U]="FAT_QUAL";static char _TmpGAE[13U]="NOTNULL_QUAL";static char _TmpGAF[14U]="NULLABLE_QUAL";static char _TmpGB0[14U]="REQUIRES_QUAL";static char _TmpGB1[13U]="ENSURES_QUAL";static char _TmpGB2[12U]="EFFECT_QUAL";static char _TmpGB3[12U]="THROWS_QUAL";static char _TmpGB4[12U]="SUBSET_QUAL";static char _TmpGB5[12U]="REGION_QUAL";static char _TmpGB6[16U]="NOZEROTERM_QUAL";static char _TmpGB7[14U]="ZEROTERM_QUAL";static char _TmpGB8[12U]="TAGGED_QUAL";static char _TmpGB9[12U]="ASSERT_QUAL";static char _TmpGBA[18U]="ASSERT_FALSE_QUAL";static char _TmpGBB[11U]="ALIAS_QUAL";static char _TmpGBC[7U]="AQUALS";static char _TmpGBD[12U]="CHECKS_QUAL";static char _TmpGBE[16U]="EXTENSIBLE_QUAL";static char _TmpGBF[18U]="AUTORELEASED_QUAL";static char _TmpGC0[7U]="PTR_OP";static char _TmpGC1[7U]="INC_OP";static char _TmpGC2[7U]="DEC_OP";static char _TmpGC3[8U]="LEFT_OP";static char _TmpGC4[9U]="RIGHT_OP";static char _TmpGC5[6U]="LE_OP";static char _TmpGC6[6U]="GE_OP";static char _TmpGC7[6U]="EQ_OP";static char _TmpGC8[6U]="NE_OP";static char _TmpGC9[7U]="AND_OP";static char _TmpGCA[6U]="OR_OP";static char _TmpGCB[11U]="MUL_ASSIGN";static char _TmpGCC[11U]="DIV_ASSIGN";static char _TmpGCD[11U]="MOD_ASSIGN";static char _TmpGCE[11U]="ADD_ASSIGN";static char _TmpGCF[11U]="SUB_ASSIGN";static char _TmpGD0[12U]="LEFT_ASSIGN";static char _TmpGD1[13U]="RIGHT_ASSIGN";static char _TmpGD2[11U]="AND_ASSIGN";static char _TmpGD3[11U]="XOR_ASSIGN";static char _TmpGD4[10U]="OR_ASSIGN";static char _TmpGD5[9U]="ELLIPSIS";static char _TmpGD6[11U]="LEFT_RIGHT";static char _TmpGD7[12U]="COLON_COLON";static char _TmpGD8[11U]="IDENTIFIER";static char _TmpGD9[17U]="INTEGER_CONSTANT";static char _TmpGDA[7U]="STRING";static char _TmpGDB[8U]="WSTRING";static char _TmpGDC[19U]="CHARACTER_CONSTANT";static char _TmpGDD[20U]="WCHARACTER_CONSTANT";static char _TmpGDE[18U]="FLOATING_CONSTANT";static char _TmpGDF[9U]="TYPE_VAR";static char _TmpGE0[13U]="TYPEDEF_NAME";static char _TmpGE1[16U]="QUAL_IDENTIFIER";static char _TmpGE2[18U]="QUAL_TYPEDEF_NAME";static char _TmpGE3[18U]="AQUAL_SHORT_CONST";static char _TmpGE4[10U]="ATTRIBUTE";static char _TmpGE5[8U]="ASM_TOK";static char _TmpGE6[4U]="';'";static char _TmpGE7[4U]="'{'";static char _TmpGE8[4U]="'}'";static char _TmpGE9[4U]="','";static char _TmpGEA[4U]="'*'";static char _TmpGEB[4U]="'='";static char _TmpGEC[4U]="'<'";static char _TmpGED[4U]="'>'";static char _TmpGEE[4U]="'('";static char _TmpGEF[4U]="')'";static char _TmpGF0[4U]="'_'";static char _TmpGF1[4U]="'$'";static char _TmpGF2[4U]="'|'";static char _TmpGF3[4U]="':'";static char _TmpGF4[4U]="'.'";static char _TmpGF5[4U]="'['";static char _TmpGF6[4U]="']'";static char _TmpGF7[4U]="'@'";static char _TmpGF8[4U]="'?'";static char _TmpGF9[4U]="'+'";static char _TmpGFA[4U]="'^'";static char _TmpGFB[4U]="'&'";static char _TmpGFC[4U]="'-'";static char _TmpGFD[4U]="'/'";static char _TmpGFE[4U]="'%'";static char _TmpGFF[4U]="'~'";static char _TmpG100[4U]="'!'";static char _TmpG101[4U]="'A'";static char _TmpG102[4U]="'V'";static char _TmpG103[20U]="prog_or_constraints";static char _TmpG104[5U]="prog";static char _TmpG105[17U]="translation_unit";static char _TmpG106[18U]="tempest_on_action";static char _TmpG107[19U]="tempest_off_action";static char _TmpG108[16U]="extern_c_action";static char _TmpG109[13U]="end_extern_c";static char _TmpG10A[14U]="hide_list_opt";static char _TmpG10B[17U]="hide_list_values";static char _TmpG10C[16U]="export_list_opt";static char _TmpG10D[12U]="export_list";static char _TmpG10E[19U]="export_list_values";static char _TmpG10F[13U]="override_opt";static char _TmpG110[21U]="external_declaration";static char _TmpG111[14U]="optional_semi";static char _TmpG112[20U]="function_definition";static char _TmpG113[21U]="function_definition2";static char _TmpG114[13U]="using_action";static char _TmpG115[15U]="unusing_action";static char _TmpG116[17U]="namespace_action";static char _TmpG117[19U]="unnamespace_action";static char _TmpG118[19U]="noinference_action";static char _TmpG119[21U]="unnoinference_action";static char _TmpG11A[12U]="declaration";static char _TmpG11B[9U]="open_opt";static char _TmpG11C[17U]="declaration_list";static char _TmpG11D[23U]="declaration_specifiers";static char _TmpG11E[24U]="storage_class_specifier";static char _TmpG11F[15U]="attributes_opt";static char _TmpG120[11U]="attributes";static char _TmpG121[15U]="attribute_list";static char _TmpG122[10U]="attribute";static char _TmpG123[15U]="type_specifier";static char _TmpG124[25U]="type_specifier_notypedef";static char _TmpG125[5U]="kind";static char _TmpG126[15U]="type_qualifier";static char _TmpG127[15U]="enum_specifier";static char _TmpG128[11U]="enum_field";static char _TmpG129[22U]="enum_declaration_list";static char _TmpG12A[26U]="struct_or_union_specifier";static char _TmpG12B[26U]="maybe_tagged_struct_union";static char _TmpG12C[16U]="struct_or_union";static char _TmpG12D[16U]="type_params_opt";static char _TmpG12E[24U]="struct_declaration_list";static char _TmpG12F[25U]="struct_declaration_list0";static char _TmpG130[25U]="init_declarator_list_rev";static char _TmpG131[16U]="init_declarator";static char _TmpG132[19U]="struct_declaration";static char _TmpG133[25U]="specifier_qualifier_list";static char _TmpG134[35U]="notypedef_specifier_qualifier_list";static char _TmpG135[27U]="struct_declarator_list_rev";static char _TmpG136[18U]="struct_declarator";static char _TmpG137[20U]="requires_clause_opt";static char _TmpG138[19U]="datatype_specifier";static char _TmpG139[14U]="qual_datatype";static char _TmpG13A[19U]="datatypefield_list";static char _TmpG13B[20U]="datatypefield_scope";static char _TmpG13C[14U]="datatypefield";static char _TmpG13D[11U]="declarator";static char _TmpG13E[23U]="declarator_withtypedef";static char _TmpG13F[18U]="direct_declarator";static char _TmpG140[30U]="direct_declarator_withtypedef";static char _TmpG141[8U]="pointer";static char _TmpG142[12U]="one_pointer";static char _TmpG143[14U]="pointer_quals";static char _TmpG144[13U]="pointer_qual";static char _TmpG145[16U]="aqual_specifier";static char _TmpG146[23U]="pointer_null_and_bound";static char _TmpG147[14U]="pointer_bound";static char _TmpG148[18U]="zeroterm_qual_opt";static char _TmpG149[8U]="eff_set";static char _TmpG14A[8U]="eff_opt";static char _TmpG14B[11U]="tqual_list";static char _TmpG14C[20U]="parameter_type_list";static char _TmpG14D[14U]="opt_aqual_bnd";static char _TmpG14E[9U]="type_var";static char _TmpG14F[16U]="optional_effect";static char _TmpG150[27U]="optional_effconstr_qualbnd";static char _TmpG151[18U]="effconstr_qualbnd";static char _TmpG152[14U]="effconstr_elt";static char _TmpG153[13U]="qual_bnd_elt";static char _TmpG154[12U]="aqual_const";static char _TmpG155[15U]="qual_bnd_const";static char _TmpG156[14U]="qual_bnd_term";static char _TmpG157[16U]="optional_inject";static char _TmpG158[11U]="effect_set";static char _TmpG159[14U]="atomic_effect";static char _TmpG15A[11U]="region_set";static char _TmpG15B[15U]="parameter_list";static char _TmpG15C[22U]="parameter_declaration";static char _TmpG15D[16U]="identifier_list";static char _TmpG15E[17U]="identifier_list0";static char _TmpG15F[12U]="initializer";static char _TmpG160[18U]="array_initializer";static char _TmpG161[17U]="initializer_list";static char _TmpG162[12U]="designation";static char _TmpG163[16U]="designator_list";static char _TmpG164[11U]="designator";static char _TmpG165[10U]="type_name";static char _TmpG166[14U]="any_type_name";static char _TmpG167[15U]="type_name_list";static char _TmpG168[20U]="abstract_declarator";static char _TmpG169[27U]="direct_abstract_declarator";static char _TmpG16A[16U]="chk_req_ens_thr";static char _TmpG16B[20U]="chk_req_ens_thr_opt";static char _TmpG16C[10U]="statement";static char _TmpG16D[18U]="labeled_statement";static char _TmpG16E[21U]="expression_statement";static char _TmpG16F[18U]="start_fndef_block";static char _TmpG170[16U]="end_fndef_block";static char _TmpG171[25U]="fndef_compound_statement";static char _TmpG172[19U]="compound_statement";static char _TmpG173[16U]="block_item_list";static char _TmpG174[20U]="selection_statement";static char _TmpG175[15U]="switch_clauses";static char _TmpG176[20U]="iteration_statement";static char _TmpG177[12U]="for_exp_opt";static char _TmpG178[15U]="jump_statement";static char _TmpG179[12U]="exp_pattern";static char _TmpG17A[20U]="conditional_pattern";static char _TmpG17B[19U]="logical_or_pattern";static char _TmpG17C[20U]="logical_and_pattern";static char _TmpG17D[21U]="inclusive_or_pattern";static char _TmpG17E[21U]="exclusive_or_pattern";static char _TmpG17F[12U]="and_pattern";static char _TmpG180[17U]="equality_pattern";static char _TmpG181[19U]="relational_pattern";static char _TmpG182[14U]="shift_pattern";static char _TmpG183[17U]="additive_pattern";static char _TmpG184[23U]="multiplicative_pattern";static char _TmpG185[13U]="cast_pattern";static char _TmpG186[14U]="unary_pattern";static char _TmpG187[16U]="postfix_pattern";static char _TmpG188[16U]="primary_pattern";static char _TmpG189[8U]="pattern";static char _TmpG18A[19U]="tuple_pattern_list";static char _TmpG18B[20U]="tuple_pattern_list0";static char _TmpG18C[14U]="field_pattern";static char _TmpG18D[19U]="field_pattern_list";static char _TmpG18E[20U]="field_pattern_list0";static char _TmpG18F[11U]="expression";static char _TmpG190[22U]="assignment_expression";static char _TmpG191[20U]="assignment_operator";static char _TmpG192[23U]="conditional_expression";static char _TmpG193[20U]="constant_expression";static char _TmpG194[22U]="logical_or_expression";static char _TmpG195[23U]="logical_and_expression";static char _TmpG196[24U]="inclusive_or_expression";static char _TmpG197[24U]="exclusive_or_expression";static char _TmpG198[15U]="and_expression";static char _TmpG199[20U]="equality_expression";static char _TmpG19A[22U]="relational_expression";static char _TmpG19B[17U]="shift_expression";static char _TmpG19C[20U]="additive_expression";static char _TmpG19D[26U]="multiplicative_expression";static char _TmpG19E[12U]="equality_op";static char _TmpG19F[14U]="relational_op";static char _TmpG1A0[12U]="additive_op";static char _TmpG1A1[18U]="multiplicative_op";static char _TmpG1A2[16U]="cast_expression";static char _TmpG1A3[17U]="unary_expression";static char _TmpG1A4[15U]="unary_operator";static char _TmpG1A5[9U]="asm_expr";static char _TmpG1A6[13U]="volatile_opt";static char _TmpG1A7[12U]="asm_out_opt";static char _TmpG1A8[12U]="asm_outlist";static char _TmpG1A9[11U]="asm_in_opt";static char _TmpG1AA[11U]="asm_inlist";static char _TmpG1AB[11U]="asm_io_elt";static char _TmpG1AC[16U]="asm_clobber_opt";static char _TmpG1AD[17U]="asm_clobber_list";static char _TmpG1AE[19U]="postfix_expression";static char _TmpG1AF[17U]="field_expression";static char _TmpG1B0[19U]="primary_expression";static char _TmpG1B1[25U]="argument_expression_list";static char _TmpG1B2[26U]="argument_expression_list0";static char _TmpG1B3[9U]="constant";static char _TmpG1B4[20U]="qual_opt_identifier";static char _TmpG1B5[17U]="qual_opt_typedef";static char _TmpG1B6[18U]="struct_union_name";static char _TmpG1B7[11U]="field_name";static char _TmpG1B8[12U]="right_angle";static char _TmpG1B9[16U]="all_constraints";static char _TmpG1BA[20U]="constraint_list_opt";static char _TmpG1BB[16U]="constraint_list";static char _TmpG1BC[15U]="tvar_or_string";static char _TmpG1BD[11U]="constraint";static char _TmpG1BE[5U]="c_op";
# 1759 "parse.y"
static struct _fat_ptr Cyc_yytname[365U]={{(unsigned char*)_TmpG52,(unsigned char*)_TmpG52,(unsigned char*)_TmpG52 + 2U},{(unsigned char*)_TmpG53,(unsigned char*)_TmpG53,(unsigned char*)_TmpG53 + 6U},{(unsigned char*)_TmpG54,(unsigned char*)_TmpG54,(unsigned char*)_TmpG54 + 12U},{(unsigned char*)_TmpG55,(unsigned char*)_TmpG55,(unsigned char*)_TmpG55 + 5U},{(unsigned char*)_TmpG56,(unsigned char*)_TmpG56,(unsigned char*)_TmpG56 + 9U},{(unsigned char*)_TmpG57,(unsigned char*)_TmpG57,(unsigned char*)_TmpG57 + 7U},{(unsigned char*)_TmpG58,(unsigned char*)_TmpG58,(unsigned char*)_TmpG58 + 7U},{(unsigned char*)_TmpG59,(unsigned char*)_TmpG59,(unsigned char*)_TmpG59 + 8U},{(unsigned char*)_TmpG5A,(unsigned char*)_TmpG5A,(unsigned char*)_TmpG5A + 5U},{(unsigned char*)_TmpG5B,(unsigned char*)_TmpG5B,(unsigned char*)_TmpG5B + 5U},{(unsigned char*)_TmpG5C,(unsigned char*)_TmpG5C,(unsigned char*)_TmpG5C + 6U},{(unsigned char*)_TmpG5D,(unsigned char*)_TmpG5D,(unsigned char*)_TmpG5D + 4U},{(unsigned char*)_TmpG5E,(unsigned char*)_TmpG5E,(unsigned char*)_TmpG5E + 5U},{(unsigned char*)_TmpG5F,(unsigned char*)_TmpG5F,(unsigned char*)_TmpG5F + 6U},{(unsigned char*)_TmpG60,(unsigned char*)_TmpG60,(unsigned char*)_TmpG60 + 9U},{(unsigned char*)_TmpG61,(unsigned char*)_TmpG61,(unsigned char*)_TmpG61 + 7U},{(unsigned char*)_TmpG62,(unsigned char*)_TmpG62,(unsigned char*)_TmpG62 + 7U},{(unsigned char*)_TmpG63,(unsigned char*)_TmpG63,(unsigned char*)_TmpG63 + 9U},{(unsigned char*)_TmpG64,(unsigned char*)_TmpG64,(unsigned char*)_TmpG64 + 6U},{(unsigned char*)_TmpG65,(unsigned char*)_TmpG65,(unsigned char*)_TmpG65 + 9U},{(unsigned char*)_TmpG66,(unsigned char*)_TmpG66,(unsigned char*)_TmpG66 + 9U},{(unsigned char*)_TmpG67,(unsigned char*)_TmpG67,(unsigned char*)_TmpG67 + 7U},{(unsigned char*)_TmpG68,(unsigned char*)_TmpG68,(unsigned char*)_TmpG68 + 6U},{(unsigned char*)_TmpG69,(unsigned char*)_TmpG69,(unsigned char*)_TmpG69 + 5U},{(unsigned char*)_TmpG6A,(unsigned char*)_TmpG6A,(unsigned char*)_TmpG6A + 8U},{(unsigned char*)_TmpG6B,(unsigned char*)_TmpG6B,(unsigned char*)_TmpG6B + 7U},{(unsigned char*)_TmpG6C,(unsigned char*)_TmpG6C,(unsigned char*)_TmpG6C + 7U},{(unsigned char*)_TmpG6D,(unsigned char*)_TmpG6D,(unsigned char*)_TmpG6D + 9U},{(unsigned char*)_TmpG6E,(unsigned char*)_TmpG6E,(unsigned char*)_TmpG6E + 3U},{(unsigned char*)_TmpG6F,(unsigned char*)_TmpG6F,(unsigned char*)_TmpG6F + 5U},{(unsigned char*)_TmpG70,(unsigned char*)_TmpG70,(unsigned char*)_TmpG70 + 7U},{(unsigned char*)_TmpG71,(unsigned char*)_TmpG71,(unsigned char*)_TmpG71 + 6U},{(unsigned char*)_TmpG72,(unsigned char*)_TmpG72,(unsigned char*)_TmpG72 + 3U},{(unsigned char*)_TmpG73,(unsigned char*)_TmpG73,(unsigned char*)_TmpG73 + 4U},{(unsigned char*)_TmpG74,(unsigned char*)_TmpG74,(unsigned char*)_TmpG74 + 5U},{(unsigned char*)_TmpG75,(unsigned char*)_TmpG75,(unsigned char*)_TmpG75 + 9U},{(unsigned char*)_TmpG76,(unsigned char*)_TmpG76,(unsigned char*)_TmpG76 + 6U},{(unsigned char*)_TmpG77,(unsigned char*)_TmpG77,(unsigned char*)_TmpG77 + 7U},{(unsigned char*)_TmpG78,(unsigned char*)_TmpG78,(unsigned char*)_TmpG78 + 5U},{(unsigned char*)_TmpG79,(unsigned char*)_TmpG79,(unsigned char*)_TmpG79 + 7U},{(unsigned char*)_TmpG7A,(unsigned char*)_TmpG7A,(unsigned char*)_TmpG7A + 16U},{(unsigned char*)_TmpG7B,(unsigned char*)_TmpG7B,(unsigned char*)_TmpG7B + 10U},{(unsigned char*)_TmpG7C,(unsigned char*)_TmpG7C,(unsigned char*)_TmpG7C + 8U},{(unsigned char*)_TmpG7D,(unsigned char*)_TmpG7D,(unsigned char*)_TmpG7D + 8U},{(unsigned char*)_TmpG7E,(unsigned char*)_TmpG7E,(unsigned char*)_TmpG7E + 4U},{(unsigned char*)_TmpG7F,(unsigned char*)_TmpG7F,(unsigned char*)_TmpG7F + 6U},{(unsigned char*)_TmpG80,(unsigned char*)_TmpG80,(unsigned char*)_TmpG80 + 4U},{(unsigned char*)_TmpG81,(unsigned char*)_TmpG81,(unsigned char*)_TmpG81 + 6U},{(unsigned char*)_TmpG82,(unsigned char*)_TmpG82,(unsigned char*)_TmpG82 + 7U},{(unsigned char*)_TmpG83,(unsigned char*)_TmpG83,(unsigned char*)_TmpG83 + 9U},{(unsigned char*)_TmpG84,(unsigned char*)_TmpG84,(unsigned char*)_TmpG84 + 5U},{(unsigned char*)_TmpG85,(unsigned char*)_TmpG85,(unsigned char*)_TmpG85 + 4U},{(unsigned char*)_TmpG86,(unsigned char*)_TmpG86,(unsigned char*)_TmpG86 + 5U},{(unsigned char*)_TmpG87,(unsigned char*)_TmpG87,(unsigned char*)_TmpG87 + 9U},{(unsigned char*)_TmpG88,(unsigned char*)_TmpG88,(unsigned char*)_TmpG88 + 9U},{(unsigned char*)_TmpG89,(unsigned char*)_TmpG89,(unsigned char*)_TmpG89 + 6U},{(unsigned char*)_TmpG8A,(unsigned char*)_TmpG8A,(unsigned char*)_TmpG8A + 10U},{(unsigned char*)_TmpG8B,(unsigned char*)_TmpG8B,(unsigned char*)_TmpG8B + 12U},{(unsigned char*)_TmpG8C,(unsigned char*)_TmpG8C,(unsigned char*)_TmpG8C + 9U},{(unsigned char*)_TmpG8D,(unsigned char*)_TmpG8D,(unsigned char*)_TmpG8D + 7U},{(unsigned char*)_TmpG8E,(unsigned char*)_TmpG8E,(unsigned char*)_TmpG8E + 8U},{(unsigned char*)_TmpG8F,(unsigned char*)_TmpG8F,(unsigned char*)_TmpG8F + 9U},{(unsigned char*)_TmpG90,(unsigned char*)_TmpG90,(unsigned char*)_TmpG90 + 15U},{(unsigned char*)_TmpG91,(unsigned char*)_TmpG91,(unsigned char*)_TmpG91 + 8U},{(unsigned char*)_TmpG92,(unsigned char*)_TmpG92,(unsigned char*)_TmpG92 + 7U},{(unsigned char*)_TmpG93,(unsigned char*)_TmpG93,(unsigned char*)_TmpG93 + 8U},{(unsigned char*)_TmpG94,(unsigned char*)_TmpG94,(unsigned char*)_TmpG94 + 8U},{(unsigned char*)_TmpG95,(unsigned char*)_TmpG95,(unsigned char*)_TmpG95 + 5U},{(unsigned char*)_TmpG96,(unsigned char*)_TmpG96,(unsigned char*)_TmpG96 + 7U},{(unsigned char*)_TmpG97,(unsigned char*)_TmpG97,(unsigned char*)_TmpG97 + 9U},{(unsigned char*)_TmpG98,(unsigned char*)_TmpG98,(unsigned char*)_TmpG98 + 6U},{(unsigned char*)_TmpG99,(unsigned char*)_TmpG99,(unsigned char*)_TmpG99 + 7U},{(unsigned char*)_TmpG9A,(unsigned char*)_TmpG9A,(unsigned char*)_TmpG9A + 5U},{(unsigned char*)_TmpG9B,(unsigned char*)_TmpG9B,(unsigned char*)_TmpG9B + 8U},{(unsigned char*)_TmpG9C,(unsigned char*)_TmpG9C,(unsigned char*)_TmpG9C + 7U},{(unsigned char*)_TmpG9D,(unsigned char*)_TmpG9D,(unsigned char*)_TmpG9D + 8U},{(unsigned char*)_TmpG9E,(unsigned char*)_TmpG9E,(unsigned char*)_TmpG9E + 7U},{(unsigned char*)_TmpG9F,(unsigned char*)_TmpG9F,(unsigned char*)_TmpG9F + 10U},{(unsigned char*)_TmpGA0,(unsigned char*)_TmpGA0,(unsigned char*)_TmpGA0 + 11U},{(unsigned char*)_TmpGA1,(unsigned char*)_TmpGA1,(unsigned char*)_TmpGA1 + 13U},{(unsigned char*)_TmpGA2,(unsigned char*)_TmpGA2,(unsigned char*)_TmpGA2 + 10U},{(unsigned char*)_TmpGA3,(unsigned char*)_TmpGA3,(unsigned char*)_TmpGA3 + 14U},{(unsigned char*)_TmpGA4,(unsigned char*)_TmpGA4,(unsigned char*)_TmpGA4 + 10U},{(unsigned char*)_TmpGA5,(unsigned char*)_TmpGA5,(unsigned char*)_TmpGA5 + 8U},{(unsigned char*)_TmpGA6,(unsigned char*)_TmpGA6,(unsigned char*)_TmpGA6 + 8U},{(unsigned char*)_TmpGA7,(unsigned char*)_TmpGA7,(unsigned char*)_TmpGA7 + 6U},{(unsigned char*)_TmpGA8,(unsigned char*)_TmpGA8,(unsigned char*)_TmpGA8 + 8U},{(unsigned char*)_TmpGA9,(unsigned char*)_TmpGA9,(unsigned char*)_TmpGA9 + 10U},{(unsigned char*)_TmpGAA,(unsigned char*)_TmpGAA,(unsigned char*)_TmpGAA + 9U},{(unsigned char*)_TmpGAB,(unsigned char*)_TmpGAB,(unsigned char*)_TmpGAB + 13U},{(unsigned char*)_TmpGAC,(unsigned char*)_TmpGAC,(unsigned char*)_TmpGAC + 10U},{(unsigned char*)_TmpGAD,(unsigned char*)_TmpGAD,(unsigned char*)_TmpGAD + 9U},{(unsigned char*)_TmpGAE,(unsigned char*)_TmpGAE,(unsigned char*)_TmpGAE + 13U},{(unsigned char*)_TmpGAF,(unsigned char*)_TmpGAF,(unsigned char*)_TmpGAF + 14U},{(unsigned char*)_TmpGB0,(unsigned char*)_TmpGB0,(unsigned char*)_TmpGB0 + 14U},{(unsigned char*)_TmpGB1,(unsigned char*)_TmpGB1,(unsigned char*)_TmpGB1 + 13U},{(unsigned char*)_TmpGB2,(unsigned char*)_TmpGB2,(unsigned char*)_TmpGB2 + 12U},{(unsigned char*)_TmpGB3,(unsigned char*)_TmpGB3,(unsigned char*)_TmpGB3 + 12U},{(unsigned char*)_TmpGB4,(unsigned char*)_TmpGB4,(unsigned char*)_TmpGB4 + 12U},{(unsigned char*)_TmpGB5,(unsigned char*)_TmpGB5,(unsigned char*)_TmpGB5 + 12U},{(unsigned char*)_TmpGB6,(unsigned char*)_TmpGB6,(unsigned char*)_TmpGB6 + 16U},{(unsigned char*)_TmpGB7,(unsigned char*)_TmpGB7,(unsigned char*)_TmpGB7 + 14U},{(unsigned char*)_TmpGB8,(unsigned char*)_TmpGB8,(unsigned char*)_TmpGB8 + 12U},{(unsigned char*)_TmpGB9,(unsigned char*)_TmpGB9,(unsigned char*)_TmpGB9 + 12U},{(unsigned char*)_TmpGBA,(unsigned char*)_TmpGBA,(unsigned char*)_TmpGBA + 18U},{(unsigned char*)_TmpGBB,(unsigned char*)_TmpGBB,(unsigned char*)_TmpGBB + 11U},{(unsigned char*)_TmpGBC,(unsigned char*)_TmpGBC,(unsigned char*)_TmpGBC + 7U},{(unsigned char*)_TmpGBD,(unsigned char*)_TmpGBD,(unsigned char*)_TmpGBD + 12U},{(unsigned char*)_TmpGBE,(unsigned char*)_TmpGBE,(unsigned char*)_TmpGBE + 16U},{(unsigned char*)_TmpGBF,(unsigned char*)_TmpGBF,(unsigned char*)_TmpGBF + 18U},{(unsigned char*)_TmpGC0,(unsigned char*)_TmpGC0,(unsigned char*)_TmpGC0 + 7U},{(unsigned char*)_TmpGC1,(unsigned char*)_TmpGC1,(unsigned char*)_TmpGC1 + 7U},{(unsigned char*)_TmpGC2,(unsigned char*)_TmpGC2,(unsigned char*)_TmpGC2 + 7U},{(unsigned char*)_TmpGC3,(unsigned char*)_TmpGC3,(unsigned char*)_TmpGC3 + 8U},{(unsigned char*)_TmpGC4,(unsigned char*)_TmpGC4,(unsigned char*)_TmpGC4 + 9U},{(unsigned char*)_TmpGC5,(unsigned char*)_TmpGC5,(unsigned char*)_TmpGC5 + 6U},{(unsigned char*)_TmpGC6,(unsigned char*)_TmpGC6,(unsigned char*)_TmpGC6 + 6U},{(unsigned char*)_TmpGC7,(unsigned char*)_TmpGC7,(unsigned char*)_TmpGC7 + 6U},{(unsigned char*)_TmpGC8,(unsigned char*)_TmpGC8,(unsigned char*)_TmpGC8 + 6U},{(unsigned char*)_TmpGC9,(unsigned char*)_TmpGC9,(unsigned char*)_TmpGC9 + 7U},{(unsigned char*)_TmpGCA,(unsigned char*)_TmpGCA,(unsigned char*)_TmpGCA + 6U},{(unsigned char*)_TmpGCB,(unsigned char*)_TmpGCB,(unsigned char*)_TmpGCB + 11U},{(unsigned char*)_TmpGCC,(unsigned char*)_TmpGCC,(unsigned char*)_TmpGCC + 11U},{(unsigned char*)_TmpGCD,(unsigned char*)_TmpGCD,(unsigned char*)_TmpGCD + 11U},{(unsigned char*)_TmpGCE,(unsigned char*)_TmpGCE,(unsigned char*)_TmpGCE + 11U},{(unsigned char*)_TmpGCF,(unsigned char*)_TmpGCF,(unsigned char*)_TmpGCF + 11U},{(unsigned char*)_TmpGD0,(unsigned char*)_TmpGD0,(unsigned char*)_TmpGD0 + 12U},{(unsigned char*)_TmpGD1,(unsigned char*)_TmpGD1,(unsigned char*)_TmpGD1 + 13U},{(unsigned char*)_TmpGD2,(unsigned char*)_TmpGD2,(unsigned char*)_TmpGD2 + 11U},{(unsigned char*)_TmpGD3,(unsigned char*)_TmpGD3,(unsigned char*)_TmpGD3 + 11U},{(unsigned char*)_TmpGD4,(unsigned char*)_TmpGD4,(unsigned char*)_TmpGD4 + 10U},{(unsigned char*)_TmpGD5,(unsigned char*)_TmpGD5,(unsigned char*)_TmpGD5 + 9U},{(unsigned char*)_TmpGD6,(unsigned char*)_TmpGD6,(unsigned char*)_TmpGD6 + 11U},{(unsigned char*)_TmpGD7,(unsigned char*)_TmpGD7,(unsigned char*)_TmpGD7 + 12U},{(unsigned char*)_TmpGD8,(unsigned char*)_TmpGD8,(unsigned char*)_TmpGD8 + 11U},{(unsigned char*)_TmpGD9,(unsigned char*)_TmpGD9,(unsigned char*)_TmpGD9 + 17U},{(unsigned char*)_TmpGDA,(unsigned char*)_TmpGDA,(unsigned char*)_TmpGDA + 7U},{(unsigned char*)_TmpGDB,(unsigned char*)_TmpGDB,(unsigned char*)_TmpGDB + 8U},{(unsigned char*)_TmpGDC,(unsigned char*)_TmpGDC,(unsigned char*)_TmpGDC + 19U},{(unsigned char*)_TmpGDD,(unsigned char*)_TmpGDD,(unsigned char*)_TmpGDD + 20U},{(unsigned char*)_TmpGDE,(unsigned char*)_TmpGDE,(unsigned char*)_TmpGDE + 18U},{(unsigned char*)_TmpGDF,(unsigned char*)_TmpGDF,(unsigned char*)_TmpGDF + 9U},{(unsigned char*)_TmpGE0,(unsigned char*)_TmpGE0,(unsigned char*)_TmpGE0 + 13U},{(unsigned char*)_TmpGE1,(unsigned char*)_TmpGE1,(unsigned char*)_TmpGE1 + 16U},{(unsigned char*)_TmpGE2,(unsigned char*)_TmpGE2,(unsigned char*)_TmpGE2 + 18U},{(unsigned char*)_TmpGE3,(unsigned char*)_TmpGE3,(unsigned char*)_TmpGE3 + 18U},{(unsigned char*)_TmpGE4,(unsigned char*)_TmpGE4,(unsigned char*)_TmpGE4 + 10U},{(unsigned char*)_TmpGE5,(unsigned char*)_TmpGE5,(unsigned char*)_TmpGE5 + 8U},{(unsigned char*)_TmpGE6,(unsigned char*)_TmpGE6,(unsigned char*)_TmpGE6 + 4U},{(unsigned char*)_TmpGE7,(unsigned char*)_TmpGE7,(unsigned char*)_TmpGE7 + 4U},{(unsigned char*)_TmpGE8,(unsigned char*)_TmpGE8,(unsigned char*)_TmpGE8 + 4U},{(unsigned char*)_TmpGE9,(unsigned char*)_TmpGE9,(unsigned char*)_TmpGE9 + 4U},{(unsigned char*)_TmpGEA,(unsigned char*)_TmpGEA,(unsigned char*)_TmpGEA + 4U},{(unsigned char*)_TmpGEB,(unsigned char*)_TmpGEB,(unsigned char*)_TmpGEB + 4U},{(unsigned char*)_TmpGEC,(unsigned char*)_TmpGEC,(unsigned char*)_TmpGEC + 4U},{(unsigned char*)_TmpGED,(unsigned char*)_TmpGED,(unsigned char*)_TmpGED + 4U},{(unsigned char*)_TmpGEE,(unsigned char*)_TmpGEE,(unsigned char*)_TmpGEE + 4U},{(unsigned char*)_TmpGEF,(unsigned char*)_TmpGEF,(unsigned char*)_TmpGEF + 4U},{(unsigned char*)_TmpGF0,(unsigned char*)_TmpGF0,(unsigned char*)_TmpGF0 + 4U},{(unsigned char*)_TmpGF1,(unsigned char*)_TmpGF1,(unsigned char*)_TmpGF1 + 4U},{(unsigned char*)_TmpGF2,(unsigned char*)_TmpGF2,(unsigned char*)_TmpGF2 + 4U},{(unsigned char*)_TmpGF3,(unsigned char*)_TmpGF3,(unsigned char*)_TmpGF3 + 4U},{(unsigned char*)_TmpGF4,(unsigned char*)_TmpGF4,(unsigned char*)_TmpGF4 + 4U},{(unsigned char*)_TmpGF5,(unsigned char*)_TmpGF5,(unsigned char*)_TmpGF5 + 4U},{(unsigned char*)_TmpGF6,(unsigned char*)_TmpGF6,(unsigned char*)_TmpGF6 + 4U},{(unsigned char*)_TmpGF7,(unsigned char*)_TmpGF7,(unsigned char*)_TmpGF7 + 4U},{(unsigned char*)_TmpGF8,(unsigned char*)_TmpGF8,(unsigned char*)_TmpGF8 + 4U},{(unsigned char*)_TmpGF9,(unsigned char*)_TmpGF9,(unsigned char*)_TmpGF9 + 4U},{(unsigned char*)_TmpGFA,(unsigned char*)_TmpGFA,(unsigned char*)_TmpGFA + 4U},{(unsigned char*)_TmpGFB,(unsigned char*)_TmpGFB,(unsigned char*)_TmpGFB + 4U},{(unsigned char*)_TmpGFC,(unsigned char*)_TmpGFC,(unsigned char*)_TmpGFC + 4U},{(unsigned char*)_TmpGFD,(unsigned char*)_TmpGFD,(unsigned char*)_TmpGFD + 4U},{(unsigned char*)_TmpGFE,(unsigned char*)_TmpGFE,(unsigned char*)_TmpGFE + 4U},{(unsigned char*)_TmpGFF,(unsigned char*)_TmpGFF,(unsigned char*)_TmpGFF + 4U},{(unsigned char*)_TmpG100,(unsigned char*)_TmpG100,(unsigned char*)_TmpG100 + 4U},{(unsigned char*)_TmpG101,(unsigned char*)_TmpG101,(unsigned char*)_TmpG101 + 4U},{(unsigned char*)_TmpG102,(unsigned char*)_TmpG102,(unsigned char*)_TmpG102 + 4U},{(unsigned char*)_TmpG103,(unsigned char*)_TmpG103,(unsigned char*)_TmpG103 + 20U},{(unsigned char*)_TmpG104,(unsigned char*)_TmpG104,(unsigned char*)_TmpG104 + 5U},{(unsigned char*)_TmpG105,(unsigned char*)_TmpG105,(unsigned char*)_TmpG105 + 17U},{(unsigned char*)_TmpG106,(unsigned char*)_TmpG106,(unsigned char*)_TmpG106 + 18U},{(unsigned char*)_TmpG107,(unsigned char*)_TmpG107,(unsigned char*)_TmpG107 + 19U},{(unsigned char*)_TmpG108,(unsigned char*)_TmpG108,(unsigned char*)_TmpG108 + 16U},{(unsigned char*)_TmpG109,(unsigned char*)_TmpG109,(unsigned char*)_TmpG109 + 13U},{(unsigned char*)_TmpG10A,(unsigned char*)_TmpG10A,(unsigned char*)_TmpG10A + 14U},{(unsigned char*)_TmpG10B,(unsigned char*)_TmpG10B,(unsigned char*)_TmpG10B + 17U},{(unsigned char*)_TmpG10C,(unsigned char*)_TmpG10C,(unsigned char*)_TmpG10C + 16U},{(unsigned char*)_TmpG10D,(unsigned char*)_TmpG10D,(unsigned char*)_TmpG10D + 12U},{(unsigned char*)_TmpG10E,(unsigned char*)_TmpG10E,(unsigned char*)_TmpG10E + 19U},{(unsigned char*)_TmpG10F,(unsigned char*)_TmpG10F,(unsigned char*)_TmpG10F + 13U},{(unsigned char*)_TmpG110,(unsigned char*)_TmpG110,(unsigned char*)_TmpG110 + 21U},{(unsigned char*)_TmpG111,(unsigned char*)_TmpG111,(unsigned char*)_TmpG111 + 14U},{(unsigned char*)_TmpG112,(unsigned char*)_TmpG112,(unsigned char*)_TmpG112 + 20U},{(unsigned char*)_TmpG113,(unsigned char*)_TmpG113,(unsigned char*)_TmpG113 + 21U},{(unsigned char*)_TmpG114,(unsigned char*)_TmpG114,(unsigned char*)_TmpG114 + 13U},{(unsigned char*)_TmpG115,(unsigned char*)_TmpG115,(unsigned char*)_TmpG115 + 15U},{(unsigned char*)_TmpG116,(unsigned char*)_TmpG116,(unsigned char*)_TmpG116 + 17U},{(unsigned char*)_TmpG117,(unsigned char*)_TmpG117,(unsigned char*)_TmpG117 + 19U},{(unsigned char*)_TmpG118,(unsigned char*)_TmpG118,(unsigned char*)_TmpG118 + 19U},{(unsigned char*)_TmpG119,(unsigned char*)_TmpG119,(unsigned char*)_TmpG119 + 21U},{(unsigned char*)_TmpG11A,(unsigned char*)_TmpG11A,(unsigned char*)_TmpG11A + 12U},{(unsigned char*)_TmpG11B,(unsigned char*)_TmpG11B,(unsigned char*)_TmpG11B + 9U},{(unsigned char*)_TmpG11C,(unsigned char*)_TmpG11C,(unsigned char*)_TmpG11C + 17U},{(unsigned char*)_TmpG11D,(unsigned char*)_TmpG11D,(unsigned char*)_TmpG11D + 23U},{(unsigned char*)_TmpG11E,(unsigned char*)_TmpG11E,(unsigned char*)_TmpG11E + 24U},{(unsigned char*)_TmpG11F,(unsigned char*)_TmpG11F,(unsigned char*)_TmpG11F + 15U},{(unsigned char*)_TmpG120,(unsigned char*)_TmpG120,(unsigned char*)_TmpG120 + 11U},{(unsigned char*)_TmpG121,(unsigned char*)_TmpG121,(unsigned char*)_TmpG121 + 15U},{(unsigned char*)_TmpG122,(unsigned char*)_TmpG122,(unsigned char*)_TmpG122 + 10U},{(unsigned char*)_TmpG123,(unsigned char*)_TmpG123,(unsigned char*)_TmpG123 + 15U},{(unsigned char*)_TmpG124,(unsigned char*)_TmpG124,(unsigned char*)_TmpG124 + 25U},{(unsigned char*)_TmpG125,(unsigned char*)_TmpG125,(unsigned char*)_TmpG125 + 5U},{(unsigned char*)_TmpG126,(unsigned char*)_TmpG126,(unsigned char*)_TmpG126 + 15U},{(unsigned char*)_TmpG127,(unsigned char*)_TmpG127,(unsigned char*)_TmpG127 + 15U},{(unsigned char*)_TmpG128,(unsigned char*)_TmpG128,(unsigned char*)_TmpG128 + 11U},{(unsigned char*)_TmpG129,(unsigned char*)_TmpG129,(unsigned char*)_TmpG129 + 22U},{(unsigned char*)_TmpG12A,(unsigned char*)_TmpG12A,(unsigned char*)_TmpG12A + 26U},{(unsigned char*)_TmpG12B,(unsigned char*)_TmpG12B,(unsigned char*)_TmpG12B + 26U},{(unsigned char*)_TmpG12C,(unsigned char*)_TmpG12C,(unsigned char*)_TmpG12C + 16U},{(unsigned char*)_TmpG12D,(unsigned char*)_TmpG12D,(unsigned char*)_TmpG12D + 16U},{(unsigned char*)_TmpG12E,(unsigned char*)_TmpG12E,(unsigned char*)_TmpG12E + 24U},{(unsigned char*)_TmpG12F,(unsigned char*)_TmpG12F,(unsigned char*)_TmpG12F + 25U},{(unsigned char*)_TmpG130,(unsigned char*)_TmpG130,(unsigned char*)_TmpG130 + 25U},{(unsigned char*)_TmpG131,(unsigned char*)_TmpG131,(unsigned char*)_TmpG131 + 16U},{(unsigned char*)_TmpG132,(unsigned char*)_TmpG132,(unsigned char*)_TmpG132 + 19U},{(unsigned char*)_TmpG133,(unsigned char*)_TmpG133,(unsigned char*)_TmpG133 + 25U},{(unsigned char*)_TmpG134,(unsigned char*)_TmpG134,(unsigned char*)_TmpG134 + 35U},{(unsigned char*)_TmpG135,(unsigned char*)_TmpG135,(unsigned char*)_TmpG135 + 27U},{(unsigned char*)_TmpG136,(unsigned char*)_TmpG136,(unsigned char*)_TmpG136 + 18U},{(unsigned char*)_TmpG137,(unsigned char*)_TmpG137,(unsigned char*)_TmpG137 + 20U},{(unsigned char*)_TmpG138,(unsigned char*)_TmpG138,(unsigned char*)_TmpG138 + 19U},{(unsigned char*)_TmpG139,(unsigned char*)_TmpG139,(unsigned char*)_TmpG139 + 14U},{(unsigned char*)_TmpG13A,(unsigned char*)_TmpG13A,(unsigned char*)_TmpG13A + 19U},{(unsigned char*)_TmpG13B,(unsigned char*)_TmpG13B,(unsigned char*)_TmpG13B + 20U},{(unsigned char*)_TmpG13C,(unsigned char*)_TmpG13C,(unsigned char*)_TmpG13C + 14U},{(unsigned char*)_TmpG13D,(unsigned char*)_TmpG13D,(unsigned char*)_TmpG13D + 11U},{(unsigned char*)_TmpG13E,(unsigned char*)_TmpG13E,(unsigned char*)_TmpG13E + 23U},{(unsigned char*)_TmpG13F,(unsigned char*)_TmpG13F,(unsigned char*)_TmpG13F + 18U},{(unsigned char*)_TmpG140,(unsigned char*)_TmpG140,(unsigned char*)_TmpG140 + 30U},{(unsigned char*)_TmpG141,(unsigned char*)_TmpG141,(unsigned char*)_TmpG141 + 8U},{(unsigned char*)_TmpG142,(unsigned char*)_TmpG142,(unsigned char*)_TmpG142 + 12U},{(unsigned char*)_TmpG143,(unsigned char*)_TmpG143,(unsigned char*)_TmpG143 + 14U},{(unsigned char*)_TmpG144,(unsigned char*)_TmpG144,(unsigned char*)_TmpG144 + 13U},{(unsigned char*)_TmpG145,(unsigned char*)_TmpG145,(unsigned char*)_TmpG145 + 16U},{(unsigned char*)_TmpG146,(unsigned char*)_TmpG146,(unsigned char*)_TmpG146 + 23U},{(unsigned char*)_TmpG147,(unsigned char*)_TmpG147,(unsigned char*)_TmpG147 + 14U},{(unsigned char*)_TmpG148,(unsigned char*)_TmpG148,(unsigned char*)_TmpG148 + 18U},{(unsigned char*)_TmpG149,(unsigned char*)_TmpG149,(unsigned char*)_TmpG149 + 8U},{(unsigned char*)_TmpG14A,(unsigned char*)_TmpG14A,(unsigned char*)_TmpG14A + 8U},{(unsigned char*)_TmpG14B,(unsigned char*)_TmpG14B,(unsigned char*)_TmpG14B + 11U},{(unsigned char*)_TmpG14C,(unsigned char*)_TmpG14C,(unsigned char*)_TmpG14C + 20U},{(unsigned char*)_TmpG14D,(unsigned char*)_TmpG14D,(unsigned char*)_TmpG14D + 14U},{(unsigned char*)_TmpG14E,(unsigned char*)_TmpG14E,(unsigned char*)_TmpG14E + 9U},{(unsigned char*)_TmpG14F,(unsigned char*)_TmpG14F,(unsigned char*)_TmpG14F + 16U},{(unsigned char*)_TmpG150,(unsigned char*)_TmpG150,(unsigned char*)_TmpG150 + 27U},{(unsigned char*)_TmpG151,(unsigned char*)_TmpG151,(unsigned char*)_TmpG151 + 18U},{(unsigned char*)_TmpG152,(unsigned char*)_TmpG152,(unsigned char*)_TmpG152 + 14U},{(unsigned char*)_TmpG153,(unsigned char*)_TmpG153,(unsigned char*)_TmpG153 + 13U},{(unsigned char*)_TmpG154,(unsigned char*)_TmpG154,(unsigned char*)_TmpG154 + 12U},{(unsigned char*)_TmpG155,(unsigned char*)_TmpG155,(unsigned char*)_TmpG155 + 15U},{(unsigned char*)_TmpG156,(unsigned char*)_TmpG156,(unsigned char*)_TmpG156 + 14U},{(unsigned char*)_TmpG157,(unsigned char*)_TmpG157,(unsigned char*)_TmpG157 + 16U},{(unsigned char*)_TmpG158,(unsigned char*)_TmpG158,(unsigned char*)_TmpG158 + 11U},{(unsigned char*)_TmpG159,(unsigned char*)_TmpG159,(unsigned char*)_TmpG159 + 14U},{(unsigned char*)_TmpG15A,(unsigned char*)_TmpG15A,(unsigned char*)_TmpG15A + 11U},{(unsigned char*)_TmpG15B,(unsigned char*)_TmpG15B,(unsigned char*)_TmpG15B + 15U},{(unsigned char*)_TmpG15C,(unsigned char*)_TmpG15C,(unsigned char*)_TmpG15C + 22U},{(unsigned char*)_TmpG15D,(unsigned char*)_TmpG15D,(unsigned char*)_TmpG15D + 16U},{(unsigned char*)_TmpG15E,(unsigned char*)_TmpG15E,(unsigned char*)_TmpG15E + 17U},{(unsigned char*)_TmpG15F,(unsigned char*)_TmpG15F,(unsigned char*)_TmpG15F + 12U},{(unsigned char*)_TmpG160,(unsigned char*)_TmpG160,(unsigned char*)_TmpG160 + 18U},{(unsigned char*)_TmpG161,(unsigned char*)_TmpG161,(unsigned char*)_TmpG161 + 17U},{(unsigned char*)_TmpG162,(unsigned char*)_TmpG162,(unsigned char*)_TmpG162 + 12U},{(unsigned char*)_TmpG163,(unsigned char*)_TmpG163,(unsigned char*)_TmpG163 + 16U},{(unsigned char*)_TmpG164,(unsigned char*)_TmpG164,(unsigned char*)_TmpG164 + 11U},{(unsigned char*)_TmpG165,(unsigned char*)_TmpG165,(unsigned char*)_TmpG165 + 10U},{(unsigned char*)_TmpG166,(unsigned char*)_TmpG166,(unsigned char*)_TmpG166 + 14U},{(unsigned char*)_TmpG167,(unsigned char*)_TmpG167,(unsigned char*)_TmpG167 + 15U},{(unsigned char*)_TmpG168,(unsigned char*)_TmpG168,(unsigned char*)_TmpG168 + 20U},{(unsigned char*)_TmpG169,(unsigned char*)_TmpG169,(unsigned char*)_TmpG169 + 27U},{(unsigned char*)_TmpG16A,(unsigned char*)_TmpG16A,(unsigned char*)_TmpG16A + 16U},{(unsigned char*)_TmpG16B,(unsigned char*)_TmpG16B,(unsigned char*)_TmpG16B + 20U},{(unsigned char*)_TmpG16C,(unsigned char*)_TmpG16C,(unsigned char*)_TmpG16C + 10U},{(unsigned char*)_TmpG16D,(unsigned char*)_TmpG16D,(unsigned char*)_TmpG16D + 18U},{(unsigned char*)_TmpG16E,(unsigned char*)_TmpG16E,(unsigned char*)_TmpG16E + 21U},{(unsigned char*)_TmpG16F,(unsigned char*)_TmpG16F,(unsigned char*)_TmpG16F + 18U},{(unsigned char*)_TmpG170,(unsigned char*)_TmpG170,(unsigned char*)_TmpG170 + 16U},{(unsigned char*)_TmpG171,(unsigned char*)_TmpG171,(unsigned char*)_TmpG171 + 25U},{(unsigned char*)_TmpG172,(unsigned char*)_TmpG172,(unsigned char*)_TmpG172 + 19U},{(unsigned char*)_TmpG173,(unsigned char*)_TmpG173,(unsigned char*)_TmpG173 + 16U},{(unsigned char*)_TmpG174,(unsigned char*)_TmpG174,(unsigned char*)_TmpG174 + 20U},{(unsigned char*)_TmpG175,(unsigned char*)_TmpG175,(unsigned char*)_TmpG175 + 15U},{(unsigned char*)_TmpG176,(unsigned char*)_TmpG176,(unsigned char*)_TmpG176 + 20U},{(unsigned char*)_TmpG177,(unsigned char*)_TmpG177,(unsigned char*)_TmpG177 + 12U},{(unsigned char*)_TmpG178,(unsigned char*)_TmpG178,(unsigned char*)_TmpG178 + 15U},{(unsigned char*)_TmpG179,(unsigned char*)_TmpG179,(unsigned char*)_TmpG179 + 12U},{(unsigned char*)_TmpG17A,(unsigned char*)_TmpG17A,(unsigned char*)_TmpG17A + 20U},{(unsigned char*)_TmpG17B,(unsigned char*)_TmpG17B,(unsigned char*)_TmpG17B + 19U},{(unsigned char*)_TmpG17C,(unsigned char*)_TmpG17C,(unsigned char*)_TmpG17C + 20U},{(unsigned char*)_TmpG17D,(unsigned char*)_TmpG17D,(unsigned char*)_TmpG17D + 21U},{(unsigned char*)_TmpG17E,(unsigned char*)_TmpG17E,(unsigned char*)_TmpG17E + 21U},{(unsigned char*)_TmpG17F,(unsigned char*)_TmpG17F,(unsigned char*)_TmpG17F + 12U},{(unsigned char*)_TmpG180,(unsigned char*)_TmpG180,(unsigned char*)_TmpG180 + 17U},{(unsigned char*)_TmpG181,(unsigned char*)_TmpG181,(unsigned char*)_TmpG181 + 19U},{(unsigned char*)_TmpG182,(unsigned char*)_TmpG182,(unsigned char*)_TmpG182 + 14U},{(unsigned char*)_TmpG183,(unsigned char*)_TmpG183,(unsigned char*)_TmpG183 + 17U},{(unsigned char*)_TmpG184,(unsigned char*)_TmpG184,(unsigned char*)_TmpG184 + 23U},{(unsigned char*)_TmpG185,(unsigned char*)_TmpG185,(unsigned char*)_TmpG185 + 13U},{(unsigned char*)_TmpG186,(unsigned char*)_TmpG186,(unsigned char*)_TmpG186 + 14U},{(unsigned char*)_TmpG187,(unsigned char*)_TmpG187,(unsigned char*)_TmpG187 + 16U},{(unsigned char*)_TmpG188,(unsigned char*)_TmpG188,(unsigned char*)_TmpG188 + 16U},{(unsigned char*)_TmpG189,(unsigned char*)_TmpG189,(unsigned char*)_TmpG189 + 8U},{(unsigned char*)_TmpG18A,(unsigned char*)_TmpG18A,(unsigned char*)_TmpG18A + 19U},{(unsigned char*)_TmpG18B,(unsigned char*)_TmpG18B,(unsigned char*)_TmpG18B + 20U},{(unsigned char*)_TmpG18C,(unsigned char*)_TmpG18C,(unsigned char*)_TmpG18C + 14U},{(unsigned char*)_TmpG18D,(unsigned char*)_TmpG18D,(unsigned char*)_TmpG18D + 19U},{(unsigned char*)_TmpG18E,(unsigned char*)_TmpG18E,(unsigned char*)_TmpG18E + 20U},{(unsigned char*)_TmpG18F,(unsigned char*)_TmpG18F,(unsigned char*)_TmpG18F + 11U},{(unsigned char*)_TmpG190,(unsigned char*)_TmpG190,(unsigned char*)_TmpG190 + 22U},{(unsigned char*)_TmpG191,(unsigned char*)_TmpG191,(unsigned char*)_TmpG191 + 20U},{(unsigned char*)_TmpG192,(unsigned char*)_TmpG192,(unsigned char*)_TmpG192 + 23U},{(unsigned char*)_TmpG193,(unsigned char*)_TmpG193,(unsigned char*)_TmpG193 + 20U},{(unsigned char*)_TmpG194,(unsigned char*)_TmpG194,(unsigned char*)_TmpG194 + 22U},{(unsigned char*)_TmpG195,(unsigned char*)_TmpG195,(unsigned char*)_TmpG195 + 23U},{(unsigned char*)_TmpG196,(unsigned char*)_TmpG196,(unsigned char*)_TmpG196 + 24U},{(unsigned char*)_TmpG197,(unsigned char*)_TmpG197,(unsigned char*)_TmpG197 + 24U},{(unsigned char*)_TmpG198,(unsigned char*)_TmpG198,(unsigned char*)_TmpG198 + 15U},{(unsigned char*)_TmpG199,(unsigned char*)_TmpG199,(unsigned char*)_TmpG199 + 20U},{(unsigned char*)_TmpG19A,(unsigned char*)_TmpG19A,(unsigned char*)_TmpG19A + 22U},{(unsigned char*)_TmpG19B,(unsigned char*)_TmpG19B,(unsigned char*)_TmpG19B + 17U},{(unsigned char*)_TmpG19C,(unsigned char*)_TmpG19C,(unsigned char*)_TmpG19C + 20U},{(unsigned char*)_TmpG19D,(unsigned char*)_TmpG19D,(unsigned char*)_TmpG19D + 26U},{(unsigned char*)_TmpG19E,(unsigned char*)_TmpG19E,(unsigned char*)_TmpG19E + 12U},{(unsigned char*)_TmpG19F,(unsigned char*)_TmpG19F,(unsigned char*)_TmpG19F + 14U},{(unsigned char*)_TmpG1A0,(unsigned char*)_TmpG1A0,(unsigned char*)_TmpG1A0 + 12U},{(unsigned char*)_TmpG1A1,(unsigned char*)_TmpG1A1,(unsigned char*)_TmpG1A1 + 18U},{(unsigned char*)_TmpG1A2,(unsigned char*)_TmpG1A2,(unsigned char*)_TmpG1A2 + 16U},{(unsigned char*)_TmpG1A3,(unsigned char*)_TmpG1A3,(unsigned char*)_TmpG1A3 + 17U},{(unsigned char*)_TmpG1A4,(unsigned char*)_TmpG1A4,(unsigned char*)_TmpG1A4 + 15U},{(unsigned char*)_TmpG1A5,(unsigned char*)_TmpG1A5,(unsigned char*)_TmpG1A5 + 9U},{(unsigned char*)_TmpG1A6,(unsigned char*)_TmpG1A6,(unsigned char*)_TmpG1A6 + 13U},{(unsigned char*)_TmpG1A7,(unsigned char*)_TmpG1A7,(unsigned char*)_TmpG1A7 + 12U},{(unsigned char*)_TmpG1A8,(unsigned char*)_TmpG1A8,(unsigned char*)_TmpG1A8 + 12U},{(unsigned char*)_TmpG1A9,(unsigned char*)_TmpG1A9,(unsigned char*)_TmpG1A9 + 11U},{(unsigned char*)_TmpG1AA,(unsigned char*)_TmpG1AA,(unsigned char*)_TmpG1AA + 11U},{(unsigned char*)_TmpG1AB,(unsigned char*)_TmpG1AB,(unsigned char*)_TmpG1AB + 11U},{(unsigned char*)_TmpG1AC,(unsigned char*)_TmpG1AC,(unsigned char*)_TmpG1AC + 16U},{(unsigned char*)_TmpG1AD,(unsigned char*)_TmpG1AD,(unsigned char*)_TmpG1AD + 17U},{(unsigned char*)_TmpG1AE,(unsigned char*)_TmpG1AE,(unsigned char*)_TmpG1AE + 19U},{(unsigned char*)_TmpG1AF,(unsigned char*)_TmpG1AF,(unsigned char*)_TmpG1AF + 17U},{(unsigned char*)_TmpG1B0,(unsigned char*)_TmpG1B0,(unsigned char*)_TmpG1B0 + 19U},{(unsigned char*)_TmpG1B1,(unsigned char*)_TmpG1B1,(unsigned char*)_TmpG1B1 + 25U},{(unsigned char*)_TmpG1B2,(unsigned char*)_TmpG1B2,(unsigned char*)_TmpG1B2 + 26U},{(unsigned char*)_TmpG1B3,(unsigned char*)_TmpG1B3,(unsigned char*)_TmpG1B3 + 9U},{(unsigned char*)_TmpG1B4,(unsigned char*)_TmpG1B4,(unsigned char*)_TmpG1B4 + 20U},{(unsigned char*)_TmpG1B5,(unsigned char*)_TmpG1B5,(unsigned char*)_TmpG1B5 + 17U},{(unsigned char*)_TmpG1B6,(unsigned char*)_TmpG1B6,(unsigned char*)_TmpG1B6 + 18U},{(unsigned char*)_TmpG1B7,(unsigned char*)_TmpG1B7,(unsigned char*)_TmpG1B7 + 11U},{(unsigned char*)_TmpG1B8,(unsigned char*)_TmpG1B8,(unsigned char*)_TmpG1B8 + 12U},{(unsigned char*)_TmpG1B9,(unsigned char*)_TmpG1B9,(unsigned char*)_TmpG1B9 + 16U},{(unsigned char*)_TmpG1BA,(unsigned char*)_TmpG1BA,(unsigned char*)_TmpG1BA + 20U},{(unsigned char*)_TmpG1BB,(unsigned char*)_TmpG1BB,(unsigned char*)_TmpG1BB + 16U},{(unsigned char*)_TmpG1BC,(unsigned char*)_TmpG1BC,(unsigned char*)_TmpG1BC + 15U},{(unsigned char*)_TmpG1BD,(unsigned char*)_TmpG1BD,(unsigned char*)_TmpG1BD + 11U},{(unsigned char*)_TmpG1BE,(unsigned char*)_TmpG1BE,(unsigned char*)_TmpG1BE + 5U}};
# 1826
static short Cyc_yyr1[626U]={0,177,177,178,179,179,179,179,179,179,179,179,179,179,179,179,180,181,182,183,184,184,185,185,185,186,186,187,187,187,188,188,189,189,190,190,190,191,191,192,192,192,192,193,193,194,195,196,197,198,199,200,200,200,200,200,200,201,201,202,202,203,203,203,203,203,203,203,203,203,203,203,204,204,204,204,204,204,204,205,205,206,207,207,208,208,208,208,209,209,210,210,210,210,210,210,210,210,210,210,210,210,210,210,210,210,210,210,210,210,210,210,210,210,210,210,210,211,212,212,212,213,213,213,214,214,215,215,215,216,216,216,217,217,218,218,219,219,220,220,221,221,222,222,223,223,223,223,224,225,225,225,225,225,225,226,226,226,226,226,226,227,227,228,228,228,228,229,229,230,230,230,231,231,232,232,232,232,233,233,233,234,234,235,235,236,236,237,237,237,237,237,237,237,237,237,238,238,238,238,238,238,238,238,238,238,239,239,240,241,241,242,242,242,242,242,242,242,242,242,242,242,242,243,243,243,- 1,- 1,244,244,244,245,245,245,246,246,246,247,247,248,248,248,249,249,250,250,250,250,250,251,251,252,252,253,253,254,254,255,255,255,255,256,256,256,257,258,258,258,258,258,259,259,260,260,261,261,262,262,263,263,263,263,264,264,265,265,266,266,266,267,268,268,269,269,270,270,270,270,270,271,271,271,271,272,272,273,273,274,274,275,275,276,276,276,276,276,276,277,277,278,278,278,279,279,279,279,279,279,279,279,279,280,280,280,280,280,280,280,280,281,281,282,282,282,282,282,282,283,284,284,285,286,287,287,288,288,289,289,289,289,289,289,289,289,290,290,290,290,290,290,291,291,291,291,291,291,292,292,292,292,293,293,294,294,294,294,294,294,294,294,295,296,296,297,297,298,298,299,299,300,300,301,301,302,302,303,303,304,304,304,305,305,306,306,307,307,308,308,308,308,308,309,310,311,311,311,311,311,311,311,311,311,311,311,311,311,311,311,311,311,312,312,312,313,313,314,314,315,315,315,316,316,317,317,318,318,318,319,319,319,319,319,319,319,319,319,319,319,320,320,320,320,320,320,320,320,320,321,322,322,323,323,324,324,325,325,326,326,327,327,328,328,329,329,329,330,330,331,331,332,332,333,333,333,333,334,334,335,335,335,336,336,337,337,337,337,337,337,337,337,337,337,337,337,337,337,337,337,337,337,337,337,337,337,337,337,337,337,337,338,338,338,338,339,340,340,341,341,341,342,342,343,343,343,344,344,345,346,346,346,347,347,348,348,348,348,348,348,348,348,348,348,348,349,349,349,349,350,350,350,350,350,350,350,350,350,350,350,351,352,352,353,353,353,353,353,354,354,355,355,356,356,357,357,358,358,359,359,360,360,361,361,362,362,363,363,363,363,363,363,363,364,364,364,364,364};
# 1892
static short Cyc_yyr2[626U]={0,1,1,1,2,3,5,3,5,5,8,3,3,3,3,0,1,1,2,1,0,4,1,2,3,0,1,4,3,4,2,3,0,4,1,1,1,1,0,3,4,4,5,3,4,2,1,2,1,1,1,2,3,5,3,6,4,0,5,1,2,1,2,2,1,2,1,2,1,2,1,2,1,1,1,1,2,1,1,0,1,6,1,3,1,1,4,8,1,2,1,1,1,1,1,1,1,1,1,1,1,1,1,4,1,1,1,1,3,4,4,1,4,4,1,4,7,1,1,1,1,5,2,4,1,3,1,2,3,4,8,3,2,1,1,1,0,3,0,1,1,2,1,3,1,3,3,5,3,1,2,1,2,1,2,1,2,1,2,1,2,1,3,2,2,0,3,4,0,6,3,5,1,2,1,2,3,3,0,1,1,2,5,1,2,1,2,1,3,4,4,5,5,4,4,2,1,1,3,4,4,5,5,4,4,2,1,2,5,0,2,4,4,4,1,1,1,1,1,1,1,4,1,1,1,4,0,1,2,2,1,0,3,3,0,1,1,1,3,0,1,1,0,2,2,3,5,5,7,0,1,2,4,0,2,0,2,1,1,3,3,3,3,4,3,1,1,1,1,1,1,4,1,4,0,1,1,3,2,3,4,1,1,3,1,3,2,1,2,1,1,3,1,1,2,3,4,8,8,1,2,3,4,2,2,1,2,3,2,1,2,1,2,3,4,3,1,1,3,1,1,2,3,3,4,4,5,4,5,4,2,4,4,4,4,5,5,5,5,0,1,1,1,1,1,1,1,3,1,2,1,1,2,3,2,3,1,2,3,4,1,2,1,2,5,7,7,5,8,6,0,4,4,5,6,7,5,7,9,8,0,1,3,2,2,2,3,2,4,5,1,1,5,1,3,1,3,1,3,1,3,1,3,1,3,1,3,1,3,3,1,3,1,3,1,4,1,2,4,2,6,1,1,1,3,1,2,1,3,6,6,4,4,5,4,2,2,4,4,4,1,3,1,1,3,1,2,1,3,1,1,3,1,3,1,3,3,1,1,1,1,1,1,1,1,1,1,1,1,5,2,2,2,5,5,5,5,1,1,3,1,3,1,3,1,3,1,3,1,3,1,3,1,3,3,1,3,1,3,1,1,1,1,1,1,1,1,1,1,1,1,4,1,2,2,2,2,2,4,2,6,4,8,6,6,6,9,13,11,4,4,6,6,4,2,2,4,4,4,1,1,1,1,5,0,1,0,2,3,1,3,0,2,3,1,3,4,0,1,2,1,3,1,4,3,4,3,3,2,2,5,6,7,1,1,3,3,1,4,1,1,1,3,2,5,5,4,5,1,1,3,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,8,0,1,1,3,1,1,5,9,7,7,7,7,9,1,1,1,1,1};
# 1958
static short Cyc_yydefact[1270U]={0,36,72,73,74,75,77,90,91,92,93,94,95,96,97,98,99,118,119,120,134,135,68,0,0,104,0,100,0,78,0,0,49,172,111,114,0,0,0,16,17,0,0,0,0,0,596,249,598,597,599,0,231,0,107,0,231,230,1,3,0,0,0,0,34,0,0,0,35,0,61,70,64,88,66,101,102,0,133,105,0,0,183,0,206,209,106,187,136,2,76,75,249,69,0,122,0,63,594,0,596,591,592,593,595,0,136,0,0,423,0,0,0,289,0,427,425,45,47,0,0,57,0,0,0,0,0,0,132,173,0,0,250,251,0,0,228,0,0,0,0,229,0,0,0,4,0,0,0,0,0,51,0,142,144,62,71,65,67,600,601,136,138,136,350,59,0,0,0,38,0,253,0,195,184,207,0,214,215,219,220,0,0,218,217,0,216,222,239,209,0,89,76,126,0,124,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,580,581,544,0,0,0,542,0,541,539,540,0,452,454,468,478,480,482,484,486,488,490,492,495,497,510,0,512,562,579,577,596,435,0,0,0,0,436,0,0,426,54,0,0,136,0,0,265,267,268,266,0,269,0,153,149,151,309,270,316,311,0,0,0,0,0,11,12,0,0,224,223,0,0,602,603,249,117,0,0,0,0,0,188,108,287,0,284,13,14,0,5,0,7,0,0,52,0,544,0,0,38,131,0,139,140,165,0,170,60,38,144,0,0,0,0,0,0,0,0,0,0,0,0,596,348,0,351,0,362,356,0,360,341,342,352,343,0,344,345,346,0,37,39,317,0,274,290,0,0,255,253,0,234,0,0,0,0,0,241,240,79,237,210,0,127,123,0,0,0,519,0,0,535,470,510,0,471,472,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,513,514,545,534,0,516,0,0,0,0,515,0,103,0,0,0,0,0,0,499,500,0,503,504,501,502,0,0,0,505,506,0,507,508,509,0,0,458,459,460,461,462,463,464,465,466,467,457,0,517,0,568,569,0,0,0,583,0,136,428,0,0,0,449,596,603,0,0,0,0,305,445,450,0,447,0,0,424,0,291,0,0,442,443,0,440,0,0,312,0,282,154,159,155,157,150,152,253,0,319,310,320,605,604,0,110,113,0,56,0,0,112,115,0,0,185,0,196,197,252,0,85,84,0,82,233,232,189,253,286,319,288,0,109,19,32,46,0,48,0,50,0,143,145,146,293,292,38,40,136,129,141,0,0,161,168,136,178,41,0,0,0,0,0,596,0,380,0,383,384,385,0,0,387,0,0,354,0,0,363,357,144,361,353,349,0,194,275,0,0,0,281,254,276,339,0,244,0,255,193,236,235,190,234,0,0,0,0,242,80,0,137,128,477,125,121,0,0,0,0,596,294,299,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,582,589,0,588,453,479,0,481,483,485,487,489,491,493,494,496,498,456,455,567,564,0,566,0,0,0,438,439,0,308,0,446,303,306,434,0,304,437,431,53,0,432,0,0,0,313,0,160,156,158,0,0,234,0,321,0,253,0,330,315,0,0,0,0,0,0,0,253,0,205,186,608,0,0,0,153,136,285,0,25,6,8,9,0,42,255,164,148,165,0,0,163,171,180,179,0,0,174,0,0,0,370,0,0,0,380,0,381,382,386,0,0,0,358,347,355,0,43,318,253,0,278,0,0,0,0,0,0,340,192,0,256,257,258,0,0,274,245,191,211,213,212,221,242,208,238,518,0,0,0,295,0,300,0,521,0,0,0,0,0,0,0,536,0,578,529,530,533,0,0,537,538,546,0,0,511,586,0,0,565,563,0,0,0,0,307,448,451,433,441,444,314,271,283,339,322,323,234,0,0,234,0,0,55,225,0,198,0,0,0,0,234,0,0,0,609,610,596,0,81,83,0,0,20,26,147,138,162,0,166,169,181,178,178,0,0,0,0,0,0,0,0,0,380,370,388,0,359,44,255,0,279,277,0,0,0,0,0,0,0,0,0,0,255,0,243,574,0,573,0,296,301,0,473,474,453,453,453,0,0,0,0,475,476,567,566,551,0,587,570,0,590,469,584,585,0,429,430,327,325,329,339,324,234,58,199,116,204,339,203,200,234,0,0,0,0,0,0,0,606,0,0,86,0,0,0,0,0,0,0,177,176,364,370,0,0,0,0,0,390,391,393,395,397,399,401,403,405,407,410,412,414,416,421,422,0,0,367,376,0,380,0,0,389,247,280,0,0,0,0,0,259,260,0,272,264,261,262,246,253,520,0,0,302,523,524,525,0,0,0,0,532,531,0,557,551,547,549,543,571,0,328,326,202,201,0,0,0,0,0,0,0,0,607,611,0,33,28,0,0,38,0,10,130,167,0,0,0,370,0,419,0,0,370,0,0,0,0,0,0,0,0,0,0,0,0,0,417,370,0,0,380,369,332,333,334,331,263,0,255,576,575,0,0,0,0,0,0,558,557,554,552,0,548,572,613,612,0,0,0,0,624,625,623,621,622,0,0,0,0,29,27,0,30,0,22,182,365,366,0,0,0,0,370,372,394,0,396,398,400,402,404,406,408,409,411,413,0,371,377,0,0,336,337,338,335,0,248,0,0,522,0,0,0,0,560,559,0,553,550,0,0,0,0,0,0,614,0,31,21,23,0,368,418,0,415,373,0,370,379,0,273,298,297,526,0,0,556,0,555,0,0,0,0,0,0,87,24,0,392,370,374,378,0,0,561,617,618,619,616,0,0,420,375,0,528,0,0,0,620,615,527,0,0,0};
# 2088
static short Cyc_yydefgoto[188U]={1267,58,59,60,61,62,554,991,1159,888,889,1084,744,63,365,64,351,65,556,66,558,67,560,68,286,166,69,70,627,276,542,543,277,73,298,278,75,193,194,76,77,78,191,325,326,152,153,327,279,514,572,573,756,79,80,760,761,762,81,574,82,534,83,84,188,189,291,85,136,621,382,383,809,715,133,86,372,615,796,797,798,280,281,1040,607,611,800,508,373,309,112,113,641,564,642,487,488,489,282,366,367,716,520,793,794,354,355,356,168,357,169,358,359,360,904,361,771,362,1003,1004,1005,1006,1007,1008,1009,1010,1011,1012,1013,1014,1015,1016,1017,1018,490,503,504,491,492,493,363,234,467,235,633,236,237,238,239,240,241,242,243,244,245,440,445,450,454,246,247,248,422,423,952,1060,1061,1134,1062,1136,1201,249,932,250,667,668,251,252,88,1085,494,524,89,879,880,1142,881,1151};
# 2110
static short Cyc_yypact[1270U]={3585,- -32768,- -32768,- -32768,- -32768,- 28,- -32768,- -32768,- -32768,- -32768,- -32768,- -32768,- -32768,- -32768,- -32768,- -32768,- -32768,- -32768,- -32768,- -32768,- -32768,- -32768,4721,302,9,- -32768,4721,- -32768,3539,- -32768,268,- 4,- -32768,- -32768,67,89,- 25,139,155,- -32768,- -32768,223,178,288,739,321,- -32768,475,- -32768,- -32768,- -32768,303,293,898,335,333,293,- -32768,- -32768,- -32768,352,354,369,3418,- -32768,625,661,392,- -32768,1040,4721,4721,4721,- -32768,4721,- -32768,- -32768,395,400,- -32768,268,4497,278,206,325,663,- -32768,- -32768,398,- -32768,408,434,- 18,- -32768,268,430,7504,- -32768,- -32768,3814,275,- -32768,- -32768,- -32768,- -32768,427,398,449,7504,- -32768,440,3814,457,482,472,- -32768,83,- -32768,- -32768,1555,1555,498,529,3418,3418,721,7504,5356,- -32768,- -32768,221,507,- -32768,- -32768,541,6298,- -32768,176,542,221,5356,- -32768,3418,3418,3751,- -32768,3418,3751,3418,3751,3751,- -32768,170,- -32768,4183,- -32768,- -32768,- -32768,- -32768,- -32768,- -32768,398,5356,220,- -32768,- -32768,4497,1040,2049,564,1555,4830,6432,- -32768,278,- -32768,545,- -32768,- -32768,- -32768,- -32768,558,576,- -32768,- -32768,578,- -32768,- -32768,79,663,1555,- -32768,- -32768,593,626,627,268,7876,632,7995,7504,1690,637,639,641,649,651,659,676,691,695,705,708,711,722,732,735,745,752,754,7995,7995,- -32768,- -32768,864,8114,3076,759,- -32768,8114,- -32768,- -32768,- -32768,336,- -32768,- -32768,- 53,821,785,776,778,696,169,727,273,73,- -32768,798,8114,309,- 7,- -32768,800,62,- -32768,3814,118,809,1095,822,366,1095,- -32768,- -32768,823,7504,398,1337,805,- -32768,- -32768,- -32768,- -32768,806,- -32768,5017,5356,5391,5356,633,- -32768,- -32768,- -32768,- 50,- 50,829,816,810,- -32768,- -32768,815,- 15,- -32768,- -32768,379,999,- -32768,- -32768,827,- -32768,839,60,828,831,820,- -32768,- -32768,824,417,- -32768,- -32768,- -32768,832,- -32768,833,- -32768,834,836,- -32768,176,864,6566,4497,564,844,846,5356,- -32768,915,268,845,- -32768,564,474,847,135,850,5493,852,866,853,861,6700,2909,5493,283,855,- -32768,2221,- -32768,856,2393,2393,1040,2393,- -32768,- -32768,- -32768,- -32768,863,- -32768,- -32768,- -32768,337,- -32768,- -32768,857,- 10,883,- -32768,130,862,859,353,869,749,865,7504,882,1555,721,- -32768,- -32768,884,868,- -32768,- 10,268,- -32768,7504,878,3076,- -32768,5356,3076,- -32768,- -32768,- -32768,5628,- -32768,911,7504,7504,7504,7504,7504,7504,7504,7504,7504,7504,7504,899,7504,7504,5356,699,7504,7504,- -32768,- -32768,- -32768,- -32768,880,- -32768,2393,888,477,7504,- -32768,7504,- -32768,8114,7504,8114,8114,8114,8114,- -32768,- -32768,8114,- -32768,- -32768,- -32768,- -32768,8114,8114,8114,- -32768,- -32768,8114,- -32768,- -32768,- -32768,8114,7504,- -32768,- -32768,- -32768,- -32768,- -32768,- -32768,- -32768,- -32768,- -32768,- -32768,- -32768,7504,- -32768,221,- -32768,- -32768,6834,221,7504,- -32768,893,398,- -32768,905,907,910,- -32768,81,427,221,7504,3814,401,- -32768,- -32768,- -32768,887,919,912,3814,- -32768,909,- -32768,371,1095,- -32768,- -32768,917,921,1555,1555,- -32768,925,928,- -32768,5391,5391,5391,- -32768,- -32768,3860,6968,119,- -32768,399,- -32768,- -32768,130,- -32768,- -32768,926,- -32768,949,1555,- -32768,- -32768,751,924,432,262,- -32768,- -32768,- -32768,929,- -32768,931,932,937,- -32768,- -32768,- -32768,1383,- -32768,365,- -32768,5356,- -32768,- -32768,1042,- -32768,3418,- -32768,3418,- -32768,3418,- -32768,939,- -32768,- -32768,- -32768,564,- -32768,398,- -32768,- -32768,7504,481,- -32768,24,398,848,- -32768,7504,7504,943,946,7504,941,1074,2737,959,- -32768,- -32768,- -32768,495,1062,- -32768,7102,2565,- -32768,962,3243,- -32768,- -32768,4340,- -32768,- -32768,- -32768,1555,- -32768,- -32768,5356,957,5169,- -32768,- -32768,951,291,873,- -32768,5204,859,- -32768,- -32768,- -32768,- -32768,749,963,966,32,968,668,- -32768,882,- -32768,- -32768,- -32768,- -32768,- -32768,969,977,973,995,970,- -32768,- -32768,719,6566,480,975,983,984,985,986,988,1002,1003,1004,496,1006,1010,1011,1018,655,1020,1021,1044,1031,7757,- -32768,- -32768,1025,1033,- -32768,821,66,785,776,778,696,169,727,273,273,73,- -32768,- -32768,- -32768,- -32768,- -32768,1030,- -32768,22,1555,6164,5356,- -32768,5356,- -32768,1035,- -32768,- -32768,- -32768,- -32768,1219,- -32768,- -32768,- -32768,- -32768,1045,- -32768,3372,138,180,- -32768,5356,- -32768,- -32768,- -32768,1054,1055,749,1037,399,1555,4982,7236,- -32768,- -32768,7504,1049,209,999,1058,7504,1555,4830,7370,- -32768,432,1066,7638,1059,60,4019,398,- -32768,1068,1160,- -32768,- -32768,- -32768,6566,- -32768,859,- -32768,- -32768,915,1063,7504,- -32768,- -32768,- -32768,- -32768,1072,268,585,497,499,7504,851,514,5493,1071,7504,1080,1081,- -32768,- -32768,1082,1092,1084,2393,- -32768,- -32768,4609,- -32768,857,1094,1555,- -32768,1093,130,1089,1090,1096,1099,- -32768,- -32768,1100,- -32768,1097,1098,1134,- 8,211,- -32768,- -32768,- -32768,- -32768,- -32768,- -32768,668,- -32768,- -32768,1110,- 23,1110,1106,- -32768,5762,- -32768,1690,- -32768,7504,7504,7504,7504,1237,7504,7504,- -32768,1690,- -32768,- -32768,- -32768,- -32768,221,221,- -32768,- -32768,1104,1109,5896,- -32768,- -32768,7504,7504,- -32768,- -32768,- 10,726,1133,1135,- -32768,- -32768,- -32768,- -32768,- -32768,- -32768,- -32768,- -32768,- -32768,291,- -32768,- -32768,749,- 10,1113,749,1107,517,- -32768,- -32768,1115,- -32768,1116,- 10,1118,1119,749,1114,616,1120,- -32768,1131,1129,1124,- -32768,- -32768,3751,1137,1232,- -32768,- -32768,5356,- -32768,7504,- -32768,- -32768,1128,63,848,5493,1138,1139,1051,1127,1141,5493,7504,1144,7504,851,- -32768,1146,- -32768,- -32768,859,243,- -32768,- -32768,7504,7504,7504,7504,130,873,873,47,1156,130,859,5356,- -32768,- -32768,246,- -32768,7504,- -32768,- -32768,6566,- -32768,911,1142,1143,1145,1147,1148,1152,1154,- -32768,911,1149,1150,- 26,1151,- -32768,- -32768,731,- -32768,- -32768,- -32768,- -32768,6164,- -32768,- -32768,- -32768,- -32768,- -32768,291,- -32768,749,- -32768,- -32768,- -32768,- -32768,291,- -32768,- -32768,749,1153,1155,1157,1159,1161,1162,1165,1169,1066,1177,- -32768,1173,972,1181,3418,1183,1180,5356,- -32768,- -32768,1305,851,1190,8233,1184,3243,1182,- -32768,- 40,- -32768,1222,1185,703,696,169,792,273,73,- -32768,- -32768,- -32768,- -32768,1223,8114,2393,- -32768,- -32768,524,7504,1196,1197,- -32768,- -32768,- -32768,1192,1194,1195,1198,1199,- -32768,- -32768,1204,- -32768,- -32768,- -32768,- -32768,- -32768,1094,- -32768,328,175,- -32768,- -32768,- -32768,- -32768,7504,5356,7504,1339,- -32768,- -32768,1207,70,286,- -32768,- -32768,- -32768,- -32768,6030,- -32768,- -32768,- -32768,- -32768,197,197,1066,197,299,299,1066,1230,- -32768,- -32768,1216,- -32768,- -32768,1220,1224,595,395,- -32768,- -32768,- -32768,565,5493,1229,851,3076,- -32768,5356,1212,1877,8114,7504,8114,8114,8114,8114,8114,8114,8114,8114,8114,8114,7504,- -32768,851,1235,1215,7504,- -32768,291,291,291,291,- -32768,1555,859,- -32768,- -32768,3243,1227,1249,1236,1233,7504,1250,355,- -32768,- -32768,1271,- -32768,- -32768,- -32768,- -32768,1257,1258,1259,1260,- -32768,- -32768,- -32768,- -32768,- -32768,1261,1262,1263,1279,- -32768,- -32768,395,- -32768,1266,600,- -32768,- -32768,- -32768,1267,1269,1268,8114,851,- -32768,821,426,785,776,776,696,169,727,273,273,73,- -32768,441,- -32768,- -32768,5493,1270,- -32768,- -32768,- -32768,- -32768,455,- -32768,1274,784,- -32768,1272,1392,5356,580,- -32768,1277,1271,- -32768,- -32768,197,197,1066,197,1066,197,- -32768,1273,- -32768,- -32768,- -32768,395,- -32768,1110,- 23,- -32768,- -32768,7504,1877,- -32768,5493,- -32768,- -32768,- -32768,- -32768,1275,1276,- -32768,1296,- -32768,1280,1281,1282,1283,1285,1292,- -32768,- -32768,447,- -32768,851,- -32768,- -32768,5356,1288,- -32768,- -32768,- -32768,- -32768,- -32768,1066,197,- -32768,- -32768,1289,- -32768,1291,1293,1294,- -32768,- -32768,- -32768,1434,1449,- -32768};
# 2240
static short Cyc_yypgoto[188U]={- -32768,- -32768,165,- -32768,- -32768,- -32768,- -32768,- -32768,238,- -32768,- -32768,298,- -32768,- -32768,- 298,- -32768,- -32768,- -32768,- -32768,- -32768,- -32768,- -32768,- -32768,- 52,- -32768,- 121,192,- -32768,- -32768,0,718,- -32768,106,- 182,1319,13,- -32768,- -32768,- 140,- -32768,- -32768,1415,- 65,569,- -32768,- -32768,1164,1136,553,313,- -32768,716,- -32768,- -32768,- -32768,41,- -32768,- -32768,- 3,- 125,1378,- 284,120,- -32768,1284,- -32768,1085,- -32768,1408,- 579,- 331,- -32768,666,- 156,1186,116,- 368,- 527,19,- -32768,- -32768,- 87,- 553,- -32768,677,700,- 321,- 506,- 131,- 515,- 152,- -32768,- 318,- 183,- 628,- 370,- -32768,1009,- 196,- 112,- 138,- 186,- 427,- 275,- 773,- 333,- -32768,- -32768,- -32768,1140,- 89,- 555,- 294,- -32768,- 718,- -32768,- 724,- -32768,- -32768,- -32768,- -32768,- -32768,- -32768,- -32768,- -32768,- -32768,- -32768,- -32768,- -32768,- -32768,- -32768,- -32768,- -32768,- -32768,72,- -32768,- -32768,787,- 219,- -32768,594,918,- -32768,- 180,- 438,- 169,- 420,- 418,- 425,1056,- 410,- 416,- 414,- 358,- 448,484,479,485,487,- 208,712,601,1187,- -32768,- -32768,- -32768,444,- -32768,- 962,374,- -32768,1102,290,- -32768,- 421,- -32768,123,561,- 71,- 76,- 116,- 247,521,- -32768,525,- 998,- 1017,437};
# 2266
static short Cyc_yytable[8408U]={71,161,680,563,584,617,160,283,284,308,673,591,670,74,299,371,672,424,399,374,396,429,71,299,676,567,71,675,643,165,426,677,400,322,577,74,742,525,293,74,468,258,497,803,530,782,907,624,695,612,138,686,386,137,596,1144,390,598,599,1153,601,799,847,71,521,323,154,432,758,759,71,71,71,1143,71,1145,74,332,540,509,1099,71,173,74,74,74,963,74,678,679,802,719,784,519,74,512,324,1135,330,521,114,742,165,787,521,522,72,926,90,121,1058,296,931,433,331,130,352,523,754,297,605,550,719,71,71,475,1100,132,72,122,118,663,72,751,304,1059,74,74,861,630,522,604,71,71,71,522,71,71,71,71,71,115,927,1038,71,74,74,74,476,74,74,74,74,74,333,96,71,74,71,72,533,254,777,430,173,1204,72,72,72,74,72,74,548,262,1026,755,845,72,1039,806,1237,1027,1239,1066,541,635,255,636,637,523,1068,500,724,608,175,858,1058,1235,1236,632,1238,955,1240,- 175,93,255,256,430,97,658,92,119,115,891,537,451,913,843,145,72,72,1133,266,566,115,256,537,381,1261,267,1234,292,- 602,120,452,453,681,631,72,72,72,735,72,72,72,72,72,537,1262,479,72,779,155,156,157,735,158,625,749,46,331,92,72,167,72,516,480,511,49,609,1092,705,517,964,441,442,967,123,288,289,513,579,872,293,580,856,352,975,810,352,352,1115,352,124,384,523,632,310,311,312,46,313,314,315,316,317,333,894,318,49,643,319,71,443,444,817,430,478,52,512,512,512,53,1140,126,74,1127,857,1141,502,46,56,57,71,901,606,167,523,71,49,600,71,71,684,71,296,74,687,167,370,353,74,53,297,74,74,869,74,- 253,694,799,799,- 253,352,190,1164,523,125,115,129,1169,115,329,628,115,789,790,1029,791,1067,115,632,1186,708,709,1183,46,1069,792,518,1030,1043,46,1045,48,49,50,729,1046,255,523,49,690,509,1044,535,914,727,532,469,470,471,729,- 290,51,71,- 290,549,72,256,890,592,170,928,171,779,46,1137,74,593,448,172,135,449,127,49,937,1059,535,72,1221,94,1146,1147,72,993,840,72,72,134,72,537,296,1125,537,472,308,612,139,643,297,473,474,1148,1149,1150,741,52,537,781,1031,1032,1033,1034,912,603,610,430,430,140,56,57,783,431,384,848,292,849,936,46,142,370,143,750,616,1246,1202,48,49,50,757,511,511,511,167,509,1133,430,144,704,723,547,430,496,513,513,513,1258,517,46,430,72,728,770,734,97,531,48,49,50,353,150,778,353,353,51,353,740,165,162,632,846,190,720,697,721,71,- 18,71,696,71,87,722,485,486,864,997,702,551,74,192,74,1023,74,552,632,874,430,51,196,875,257,863,259,95,71,731,1222,732,116,937,117,430,873,71,733,261,1192,74,958,71,1035,1223,870,1257,263,1042,74,130,1046,115,131,1226,74,87,870,965,353,115,1048,132,320,523,115,87,265,972,321,430,752,87,430,753,264,665,938,518,818,159,610,808,163,936,774,87,947,430,430,430,939,430,285,535,828,899,195,900,537,300,948,116,1180,72,957,72,430,72,549,430,741,287,905,116,915,969,430,1124,1173,1174,1170,295,1114,537,1172,87,87,17,18,19,1176,233,72,1177,307,1175,937,933,301,87,305,72,377,260,87,87,87,72,87,87,87,87,87,364,632,378,328,551,949,950,723,294,745,1161,746,307,747,352,1113,87,331,610,430,379,897,380,734,898,1232,632,632,632,632,98,364,387,384,1157,936,1215,1178,1179,1216,176,177,178,179,180,195,1162,181,20,21,182,183,184,833,470,471,185,977,978,979,186,146,147,213,388,167,71,855,389,71,980,981,982,52,167,1158,393,516,983,74,167,401,74,402,517,403,56,57,269,270,271,272,1168,404,1097,405,187,148,149,472,1112,438,439,406,116,834,474,116,427,808,116,115,712,713,714,290,116,510,115,515,407,46,101,222,223,102,103,104,446,447,49,1187,1188,1189,1190,408,535,619,620,409,1224,758,759,394,536,1129,227,499,535,410,92,1090,411,455,274,412,536,815,816,1103,1104,535,902,903,959,960,413,328,87,1064,1065,421,72,46,71,72,414,536,575,415,1247,48,49,50,581,51,1165,74,1166,416,1181,52,610,1107,1108,532,417,392,418,395,397,397,87,428,56,57,160,456,457,458,459,460,461,462,463,464,465,1245,1126,1193,419,420,1228,430,590,397,995,996,434,397,1036,1037,436,435,608,437,195,477,481,466,269,270,271,272,495,498,46,1220,397,505,506,526,527,528,48,49,50,352,353,529,132,167,1019,539,52,546,544,273,547,545,553,555,557,427,559,517,427,56,57,71,72,568,576,644,569,646,647,648,586,587,1231,578,74,654,582,795,585,588,1160,1191,597,349,92,160,594,606,274,613,614,71,609,92,523,115,618,671,634,622,51,432,46,655,74,629,662,699,610,610,610,49,1244,610,51,664,352,689,116,46,52,988,1259,303,53,395,116,48,49,50,691,116,692,56,57,693,703,52,688,307,700,532,707,701,706,710,571,1000,1001,711,56,57,725,726,730,736,160,737,739,738,376,743,748,536,98,766,536,72,71,765,307,397,768,933,307,769,46,773,536,775,536,74,780,785,48,49,50,87,788,87,804,87,1082,805,1083,807,811,72,812,814,813,- 602,819,46,820,821,822,823,98,824,1160,48,49,50,397,160,397,397,397,397,307,52,397,825,826,532,1087,397,397,397,307,827,397,829,56,57,397,830,831,307,99,352,763,764,46,832,767,835,836,772,837,838,841,49,842,253,101,844,151,102,103,104,52,105,49,853,53,868,397,850,106,862,878,107,72,56,57,1002,887,109,110,859,860,353,99,871,884,886,228,893,111,230,895,71,231,232,482,906,908,483,101,909,430,102,103,104,74,484,49,565,910,911,370,916,106,918,919,107,923,924,925,108,920,109,110,921,922,485,486,839,934,116,98,944,111,951,953,961,116,962,966,968,970,971,307,973,974,984,976,985,986,987,990,397,994,307,989,998,1021,536,353,1022,1025,510,1028,623,999,1041,1052,1049,1050,536,1051,1054,1053,1055,1056,1057,1063,1070,1077,1071,1080,1072,536,1073,565,1074,1075,867,645,1076,896,1081,649,650,651,652,653,72,1086,656,657,1088,1091,660,661,1089,99,1093,1096,1101,1111,1098,1116,1102,666,1117,669,1118,851,1119,1120,483,101,1121,1122,102,103,104,1123,484,49,1132,772,1131,131,1154,106,1167,1155,107,1185,682,1156,108,397,109,110,1163,98,485,486,1184,1195,683,1200,1197,111,1198,666,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,1196,1058,1205,1206,1207,1208,1209,1210,1212,353,1214,1217,1230,1219,1211,23,24,25,1227,27,1218,1225,1233,1229,1241,1248,1250,1249,1268,718,1255,1251,1252,1253,1254,33,397,1256,328,1260,1263,87,1264,1269,1265,1266,34,35,1242,1213,99,885,306,128,992,174,570,116,141,626,41,397,501,892,42,253,101,385,930,102,103,104,929,105,49,43,307,561,538,44,106,852,917,107,1106,45,674,108,1105,109,110,698,1109,602,1024,1110,772,1020,1138,1078,111,562,1203,1243,1079,666,1152,0,368,666,0,46,659,0,0,0,0,0,92,48,49,50,1047,51,397,370,0,0,0,52,0,0,0,547,397,54,55,0,0,0,517,307,56,57,159,0,87,0,0,397,0,0,0,0,0,565,0,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,23,24,25,260,27,0,0,0,0,0,0,0,397,0,0,565,0,0,0,0,33,0,0,0,0,0,772,0,0,0,0,34,35,0,0,268,0,397,397,397,397,269,270,271,272,41,0,866,0,42,0,0,0,0,159,0,0,0,877,0,43,0,883,0,44,0,0,0,273,0,45,0,0,565,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,666,0,0,0,0,427,0,0,0,0,0,1171,0,92,48,0,50,274,51,0,0,275,1182,0,0,0,0,772,0,1095,54,55,0,197,198,159,0,0,1194,0,0,0,0,1199,0,0,0,0,199,397,98,565,0,0,0,940,941,942,943,0,945,946,0,0,0,0,203,204,205,206,207,208,209,210,565,211,0,956,0,0,0,0,0,213,0,0,0,0,0,0,0,214,215,216,159,217,0,0,0,0,0,0,0,0,0,0,0,0,0,0,218,219,0,0,0,0,0,0,220,221,0,0,0,0,0,0,0,0,397,0,397,397,397,397,397,397,397,397,397,397,0,46,101,222,223,102,103,104,0,0,49,0,0,0,224,0,398,0,0,225,0,0,0,226,0,0,227,0,0,0,0,0,565,0,228,0,229,230,0,0,231,232,0,0,0,0,0,0,0,0,0,0,0,0,0,565,397,2,3,4,91,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,902,903,22,197,198,334,0,335,336,337,338,339,340,341,342,23,24,25,343,27,98,28,200,344,0,0,0,0,201,202,29,345,0,0,397,33,203,204,205,206,207,208,209,210,0,211,34,35,36,212,0,0,0,213,0,0,0,0,0,0,41,214,215,216,42,217,0,0,0,0,1128,0,1130,0,0,43,0,0,0,44,218,219,0,565,0,45,0,0,220,221,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,346,101,222,223,102,103,104,92,48,49,50,0,51,224,347,348,0,0,225,0,0,0,226,0,54,350,0,0,0,0,0,0,0,228,0,229,230,0,0,231,232,2,3,4,91,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,0,0,22,197,198,334,0,335,336,337,338,339,340,341,342,23,24,25,343,27,98,28,200,344,0,0,0,0,201,202,29,345,0,0,0,33,203,204,205,206,207,208,209,210,0,211,34,35,36,212,0,0,0,213,0,0,0,0,0,0,41,214,215,216,42,217,0,0,0,0,0,0,0,0,0,43,0,0,0,44,218,219,0,0,0,45,0,0,220,221,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,346,101,222,223,102,103,104,92,48,49,50,0,51,224,347,348,349,0,225,0,0,0,226,0,54,350,0,0,0,0,0,0,0,228,0,229,230,0,0,231,232,2,3,4,91,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,0,0,22,197,198,334,0,335,336,337,338,339,340,341,342,23,24,25,343,27,98,28,200,344,0,0,0,0,201,202,29,345,0,0,0,33,203,204,205,206,207,208,209,210,0,211,34,35,36,212,0,0,0,213,0,0,0,0,0,0,41,214,215,216,42,217,0,0,0,0,0,0,0,0,0,43,0,0,0,44,218,219,0,0,0,45,0,0,220,221,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,346,101,222,223,102,103,104,92,48,49,50,0,51,224,347,348,595,0,225,0,0,0,226,0,54,350,0,0,0,0,0,0,0,228,0,229,230,0,0,231,232,2,3,4,91,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,0,0,22,197,198,334,0,335,336,337,338,339,340,341,342,23,24,25,343,27,98,28,200,344,0,0,0,0,201,202,29,345,0,0,0,33,203,204,205,206,207,208,209,210,0,211,34,35,36,212,0,0,0,213,0,0,0,0,0,0,41,214,215,216,42,217,0,0,0,0,0,0,0,0,0,43,0,0,0,44,218,219,0,0,0,45,0,0,220,221,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,346,101,222,223,102,103,104,92,48,49,50,0,51,224,347,348,0,0,225,0,0,0,226,0,54,350,0,0,0,0,0,0,0,228,0,229,230,0,0,231,232,2,3,4,91,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,0,0,22,197,198,334,0,335,336,337,338,339,340,341,342,23,24,25,343,27,98,28,200,344,0,0,0,0,201,202,29,345,0,0,0,33,203,204,205,206,207,208,209,210,0,211,34,35,36,212,0,0,0,213,0,0,0,0,0,0,41,214,215,216,42,217,0,0,0,0,0,0,0,0,0,43,0,0,0,44,218,219,0,0,0,45,0,0,220,221,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,583,101,222,223,102,103,104,92,48,49,50,0,51,224,347,348,0,0,225,0,0,0,226,0,54,350,0,0,0,0,0,0,0,228,0,229,230,0,0,231,232,2,3,4,91,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,0,0,22,197,198,0,0,0,0,0,0,0,0,0,0,23,24,25,343,27,98,28,200,0,0,0,0,0,201,202,29,0,0,0,0,33,203,204,205,206,207,208,209,210,0,211,34,35,36,212,0,0,0,213,0,0,0,0,0,0,41,214,215,216,42,217,0,0,0,0,0,0,0,0,0,43,0,0,0,44,218,219,0,0,0,45,0,0,220,221,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,46,101,222,223,102,103,104,92,48,49,50,0,51,224,0,0,0,0,225,0,0,0,226,0,54,350,0,0,0,0,0,0,0,228,0,229,230,0,0,231,232,2,3,4,91,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,0,0,22,197,198,0,0,0,0,0,0,0,0,0,0,23,24,25,343,27,98,0,0,0,0,0,0,0,0,0,29,0,0,0,0,33,203,204,205,206,207,208,209,210,0,211,34,35,0,0,0,0,0,213,0,0,0,0,0,0,41,214,215,216,42,217,0,0,0,0,0,0,0,0,0,43,0,0,0,44,218,219,0,0,0,45,0,0,220,221,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,46,101,222,223,102,103,104,92,48,49,50,0,51,224,0,0,0,0,225,0,0,0,394,0,54,350,0,0,0,0,0,0,0,228,0,229,230,0,0,231,232,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,0,0,0,197,198,0,0,0,0,0,0,0,0,0,0,23,24,25,199,27,98,0,200,0,0,0,0,0,201,202,0,0,0,0,0,33,203,204,205,206,207,208,209,210,0,211,34,35,0,212,0,0,0,213,0,0,0,0,0,0,41,214,215,216,42,217,0,0,0,0,0,0,0,0,0,43,0,0,0,44,218,219,0,0,0,45,0,0,220,221,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,46,101,222,223,102,103,104,92,48,49,50,0,51,224,0,425,0,0,225,0,0,0,226,0,54,350,0,0,0,0,0,0,0,228,0,229,230,0,0,231,232,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,0,0,0,197,198,0,0,0,0,0,0,0,0,0,0,23,24,25,199,27,98,0,200,0,0,0,0,0,201,202,0,0,0,0,0,33,203,204,205,206,207,208,209,210,0,211,34,35,0,212,0,0,0,213,0,0,0,0,0,0,41,214,215,216,42,217,0,0,0,0,0,0,0,0,0,43,0,0,0,44,218,219,0,0,0,45,0,0,220,221,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,46,101,222,223,102,103,104,92,48,49,50,0,51,224,0,0,0,0,225,0,0,0,226,0,54,350,0,0,0,0,0,0,0,228,0,229,230,0,98,231,232,- 15,1,0,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,0,0,22,0,0,0,0,0,0,0,0,0,0,0,0,23,24,25,26,27,0,28,0,0,0,0,0,0,0,0,29,0,30,31,32,33,0,0,0,0,0,0,0,0,0,0,34,35,36,0,99,37,38,0,39,40,0,0,0,0,41,0,854,0,42,253,101,0,0,102,103,104,0,105,49,43,0,0,0,44,106,0,0,107,0,45,0,108,0,109,110,0,0,0,0,0,0,0,0,0,111,0,0,0,0,0,0,0,0,0,0,46,0,0,0,0,0,0,92,48,49,50,0,51,0,0,0,- 15,0,52,0,0,0,53,0,54,55,0,0,0,0,98,56,57,- 15,1,0,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,0,0,22,0,0,0,0,0,0,0,0,0,0,0,0,23,24,25,26,27,0,28,0,0,0,0,0,0,0,0,29,0,30,31,32,33,0,0,0,0,0,0,0,0,0,0,34,35,36,0,99,37,38,0,39,40,0,0,0,0,41,0,0,0,42,100,101,0,0,102,103,104,0,105,49,43,0,0,0,44,106,0,0,107,0,45,0,108,0,109,110,0,0,0,0,0,0,0,0,0,111,0,0,0,0,0,0,0,0,0,0,46,0,0,0,0,0,0,47,48,49,50,0,51,0,0,0,0,0,52,0,0,0,53,0,54,55,0,0,0,0,0,56,57,1,0,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,0,0,22,0,0,0,0,0,0,0,0,0,0,0,0,23,24,25,26,27,0,28,0,0,0,0,0,0,0,0,29,0,30,31,32,33,0,0,0,0,0,0,0,0,0,0,34,35,36,0,0,37,38,0,39,40,0,0,0,0,41,0,0,0,42,0,0,0,0,0,0,0,0,0,0,43,0,0,0,44,0,0,0,98,0,45,0,0,0,0,0,0,0,0,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,0,0,46,0,0,0,0,0,0,92,48,49,50,0,51,23,24,25,- 15,27,52,0,0,0,53,0,54,55,0,0,0,0,0,56,57,33,0,0,0,0,0,0,0,0,0,0,34,35,0,0,99,0,0,0,0,0,0,0,0,0,41,0,0,0,42,253,101,0,0,102,103,104,0,105,49,43,0,0,0,44,106,0,0,107,0,45,0,108,0,109,110,0,0,0,0,0,0,0,0,0,111,0,0,0,0,0,0,0,368,0,0,0,0,0,0,0,0,0,92,48,0,50,0,51,0,370,0,0,0,52,0,0,0,516,0,54,55,0,0,0,517,0,56,57,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,23,24,25,0,27,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,33,0,0,0,0,0,0,0,0,0,0,34,35,0,0,0,0,0,0,0,0,0,0,0,0,41,0,0,0,42,0,0,0,0,0,0,0,0,0,0,43,0,0,0,44,0,0,0,0,0,45,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,46,0,0,0,0,0,0,92,48,49,50,0,51,0,0,0,0,0,52,0,0,0,532,0,54,55,0,0,0,0,0,56,57,2,3,4,91,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,0,0,22,0,0,0,0,0,0,0,0,0,0,0,0,23,24,25,26,27,0,28,0,0,0,0,0,0,0,0,29,0,0,0,0,33,0,0,0,0,0,0,0,0,0,0,34,35,36,0,0,0,0,0,0,0,0,0,0,0,41,0,0,0,42,0,0,0,0,0,0,0,0,0,0,43,0,0,0,44,0,0,0,0,0,45,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,92,48,0,50,0,51,320,0,164,0,0,0,321,0,0,0,0,54,55,2,3,4,91,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,0,0,22,0,0,0,0,0,0,0,0,0,0,0,0,23,24,25,26,27,0,28,0,0,0,0,0,0,0,0,29,0,0,0,0,33,0,0,0,0,0,0,0,0,0,0,34,35,36,0,0,0,0,0,0,0,0,0,0,0,41,0,0,0,42,0,0,0,0,0,0,0,0,0,0,43,0,0,0,44,0,0,0,0,0,45,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,92,48,0,50,0,51,320,0,348,0,0,0,321,0,0,0,0,54,55,2,3,4,91,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,0,0,22,0,0,0,0,0,0,0,0,0,0,0,0,23,24,25,26,27,0,28,0,0,0,0,0,0,0,0,29,0,0,0,0,33,0,0,0,0,0,0,0,0,0,0,34,35,36,0,0,0,0,0,0,0,0,0,0,0,41,0,0,0,42,0,0,0,0,0,0,0,0,0,0,43,0,0,0,44,0,0,0,0,0,45,0,0,0,0,0,0,2,3,4,91,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,0,0,22,0,0,0,92,48,0,50,0,51,0,0,164,23,24,25,26,27,0,28,0,54,55,0,0,0,0,0,29,0,0,0,0,33,0,0,0,0,0,0,0,0,0,0,34,35,36,0,0,0,0,0,0,0,0,0,0,0,41,0,0,0,42,0,0,0,0,0,0,0,0,0,0,43,0,0,0,44,0,0,0,0,0,45,0,0,0,0,0,0,2,3,4,91,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,0,0,22,0,0,0,92,48,0,50,0,51,0,0,348,23,24,25,26,27,0,0,0,54,55,0,0,0,0,0,29,0,0,0,0,33,0,0,0,0,0,0,0,0,0,0,34,35,0,0,0,0,0,0,0,0,0,0,0,0,41,0,0,0,42,0,0,0,0,0,0,0,0,0,0,43,0,0,0,44,0,0,0,0,0,45,0,0,0,0,0,0,0,0,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,0,0,0,0,0,0,0,0,0,92,48,0,50,0,51,23,24,25,0,27,0,0,0,0,0,0,54,55,0,0,0,0,0,0,0,33,0,0,0,0,0,0,0,0,0,0,34,35,0,0,0,0,0,0,0,0,0,0,0,0,41,0,0,0,42,0,0,0,0,0,0,0,0,0,0,43,0,0,0,44,0,0,0,0,0,45,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,368,0,0,369,0,0,0,0,0,0,92,48,0,50,0,51,0,370,0,0,0,0,0,0,0,0,0,54,55,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,23,24,25,0,27,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,33,0,0,0,0,0,0,0,0,0,0,34,35,0,0,23,24,25,0,27,0,0,0,0,0,41,0,0,0,42,0,0,0,0,0,33,0,0,0,0,43,0,0,0,44,0,34,35,0,0,45,0,0,0,0,0,0,0,0,0,41,0,0,0,42,0,0,0,0,0,0,0,0,368,0,43,0,0,0,44,0,0,0,92,48,45,50,0,51,0,370,0,0,0,0,0,0,0,0,0,54,55,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,92,48,0,50,0,51,0,0,0,507,0,0,0,0,0,0,0,54,55,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,23,24,25,0,27,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,33,0,0,0,0,0,0,0,0,0,0,34,35,0,0,23,24,25,0,27,0,0,0,0,0,41,0,0,0,42,0,0,0,0,0,33,0,0,0,0,43,0,0,0,44,0,34,35,0,0,45,0,0,0,0,0,0,0,0,0,41,0,0,0,42,0,0,0,0,0,0,0,0,0,0,43,0,0,0,44,0,0,0,92,48,45,50,0,51,0,0,0,786,0,0,0,0,0,0,0,54,55,0,0,0,0,0,0,801,0,0,0,0,0,0,0,0,0,92,48,0,50,0,51,0,0,0,0,0,0,0,0,0,0,0,54,55,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,23,24,25,0,27,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,33,0,0,0,0,0,0,0,0,0,0,34,35,0,0,23,24,25,0,27,0,0,0,0,0,41,0,0,0,42,0,0,0,0,0,33,0,0,0,0,43,0,0,0,44,0,34,35,0,0,45,0,0,0,0,0,0,0,0,0,41,0,0,0,42,0,0,0,0,0,0,0,0,0,0,43,0,0,0,44,0,0,0,92,48,45,50,0,51,0,0,0,0,0,0,0,0,0,0,0,54,55,0,0,0,197,198,334,0,335,336,337,338,339,340,341,342,0,92,0,199,0,98,51,200,344,0,0,0,0,201,202,0,345,0,54,55,0,203,204,205,206,207,208,209,210,0,211,0,0,0,212,0,0,0,213,0,0,0,0,0,0,0,214,215,216,0,217,0,0,0,0,0,0,0,0,0,0,0,0,0,0,218,219,0,0,0,0,0,0,220,221,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,583,101,222,223,102,103,104,0,0,49,0,0,0,224,347,348,0,0,225,0,0,0,226,0,0,227,0,197,198,0,0,0,0,228,638,229,230,0,0,231,232,0,199,0,98,0,200,0,0,0,0,0,201,202,0,0,0,0,0,0,203,204,205,206,207,208,209,210,0,211,0,0,0,212,0,0,0,213,0,0,0,0,0,0,0,214,215,216,0,217,0,0,0,0,0,0,0,0,0,0,0,0,0,0,218,219,0,0,0,0,0,0,220,221,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,639,101,222,223,102,103,104,0,297,49,0,0,0,224,0,398,640,0,225,0,0,0,226,0,0,227,197,198,485,486,0,0,0,228,0,229,230,0,0,231,232,199,0,98,0,200,0,0,0,0,0,201,202,0,0,0,0,0,0,203,204,205,206,207,208,209,210,0,211,0,0,0,212,0,0,0,213,0,0,0,0,0,0,0,214,215,216,0,217,0,0,0,0,0,0,0,0,0,0,0,0,0,0,218,219,0,0,0,0,0,0,220,221,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,639,101,222,223,102,103,104,0,297,49,0,0,0,224,0,398,935,0,225,0,0,0,226,0,0,227,197,198,485,486,0,0,0,228,0,229,230,0,0,231,232,199,0,98,0,200,0,0,0,0,0,201,202,0,0,0,0,0,0,203,204,205,206,207,208,209,210,0,211,0,0,0,212,0,0,0,213,0,0,0,0,0,0,0,214,215,216,0,217,0,0,0,0,0,0,0,0,0,0,0,0,0,0,218,219,0,0,0,0,0,0,220,221,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,639,101,222,223,102,103,104,0,297,49,0,0,0,224,0,398,954,0,225,0,0,0,226,0,0,227,197,198,485,486,0,0,0,228,0,229,230,0,0,231,232,199,0,98,0,200,0,0,0,0,0,201,202,0,0,0,0,0,0,203,204,205,206,207,208,209,210,0,211,0,0,0,212,0,0,0,213,0,0,0,0,0,0,0,214,215,216,0,217,0,0,0,0,0,0,0,0,0,0,0,0,0,0,218,219,0,0,0,0,0,0,220,221,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,639,101,222,223,102,103,104,0,297,49,0,0,0,224,0,398,1139,0,225,0,0,0,226,0,0,227,197,198,485,486,0,0,0,228,0,229,230,0,0,231,232,199,0,98,0,200,0,0,0,0,0,201,202,0,0,0,0,0,0,203,204,205,206,207,208,209,210,0,211,0,0,0,212,0,0,0,213,0,0,0,0,0,0,0,214,215,216,0,217,0,0,0,0,0,0,0,0,0,0,0,0,0,0,218,219,0,0,0,0,0,0,220,221,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,639,101,222,223,102,103,104,0,297,49,0,0,0,224,0,398,0,0,225,0,0,0,226,0,0,227,197,198,485,486,0,0,0,228,0,229,230,0,0,231,232,199,0,98,0,200,0,0,0,0,0,201,202,0,0,0,0,0,0,203,204,205,206,207,208,209,210,0,211,0,0,0,212,0,0,0,213,0,0,0,0,0,0,0,214,215,216,0,217,0,0,0,0,0,0,0,0,0,0,0,0,0,0,218,219,0,0,0,0,0,0,220,221,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,46,101,222,223,102,103,104,302,0,49,0,0,0,224,0,0,0,0,225,0,0,0,226,0,0,227,197,198,0,0,0,0,0,228,0,229,230,0,0,231,232,199,0,98,0,200,0,0,0,0,0,201,202,0,0,0,0,0,0,203,204,205,206,207,208,209,210,0,211,0,0,0,212,0,0,0,213,0,0,0,0,0,0,0,214,215,216,0,217,0,0,0,0,0,0,0,0,0,0,0,0,0,0,218,219,0,0,0,0,0,0,220,221,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,46,101,222,223,102,103,104,0,0,49,0,0,0,224,0,0,0,0,225,0,0,0,226,0,0,227,197,198,0,0,375,0,0,228,0,229,230,0,0,231,232,199,0,98,0,200,0,0,0,0,0,201,202,0,0,0,0,0,0,203,204,205,206,207,208,209,210,0,211,0,0,0,212,0,0,0,213,0,0,0,0,0,0,0,214,215,216,0,217,0,0,0,0,0,0,0,0,0,0,0,0,0,0,218,219,0,0,0,0,0,0,220,221,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,46,101,222,223,102,103,104,0,0,49,0,0,0,224,0,398,0,0,225,0,0,0,226,0,0,227,197,198,0,0,0,0,0,228,0,229,230,0,0,231,232,199,0,98,0,200,0,0,0,0,0,201,202,0,0,0,0,0,0,203,204,205,206,207,208,209,210,0,211,0,0,0,212,0,0,0,213,0,0,0,0,0,0,0,214,215,216,0,217,0,0,0,0,0,0,0,0,0,0,0,0,0,0,218,219,0,0,0,0,0,0,220,221,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,46,101,222,223,102,103,104,0,0,49,0,0,0,224,589,0,0,0,225,0,0,0,226,0,0,227,197,198,0,0,0,0,0,228,0,229,230,0,0,231,232,199,0,98,0,200,0,0,0,0,0,201,202,0,0,0,0,0,0,203,204,205,206,207,208,209,210,0,211,0,0,0,212,0,0,0,213,0,0,0,0,0,0,0,214,215,216,0,217,0,0,0,0,0,0,0,0,0,0,0,0,0,0,218,219,0,0,0,0,0,0,220,221,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,46,101,222,223,102,103,104,0,0,49,0,0,0,224,0,0,0,0,225,0,0,0,226,685,0,227,197,198,0,0,0,0,0,228,0,229,230,0,0,231,232,199,0,98,0,200,0,0,0,0,0,201,202,0,0,0,0,0,0,203,204,205,206,207,208,209,210,0,211,0,0,0,212,0,0,0,213,0,0,0,0,0,0,0,214,215,216,0,217,0,0,0,0,0,0,0,0,0,0,0,0,0,0,218,219,0,0,0,0,0,0,220,221,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,46,101,222,223,102,103,104,0,0,49,0,0,0,224,0,0,0,0,225,0,0,0,226,0,0,227,197,198,0,0,717,0,0,228,0,229,230,0,0,231,232,199,0,98,0,200,0,0,0,0,0,201,202,0,0,0,0,0,0,203,204,205,206,207,208,209,210,0,211,0,0,0,212,0,0,0,213,0,0,0,0,0,0,0,214,215,216,0,217,0,0,0,0,0,0,0,0,0,0,0,0,0,0,218,219,0,0,0,0,0,0,220,221,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,46,101,222,223,102,103,104,0,0,49,0,0,0,224,0,0,0,0,225,0,0,0,226,776,0,227,197,198,0,0,0,0,0,228,0,229,230,0,0,231,232,199,0,98,0,200,0,0,0,0,0,201,202,0,0,0,0,0,0,203,204,205,206,207,208,209,210,0,211,0,0,0,212,0,0,0,213,0,0,0,0,0,0,0,214,215,216,0,217,0,0,0,0,0,0,0,0,0,0,0,0,0,0,218,219,0,0,0,0,0,0,220,221,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,46,101,222,223,102,103,104,0,0,49,0,0,0,224,0,0,0,0,225,0,0,0,226,0,0,227,197,198,0,0,865,0,0,228,0,229,230,0,0,231,232,199,0,98,0,200,0,0,0,0,0,201,202,0,0,0,0,0,0,203,204,205,206,207,208,209,210,0,211,0,0,0,212,0,0,0,213,0,0,0,0,0,0,0,214,215,216,0,217,0,0,0,0,0,0,0,0,0,0,0,0,0,0,218,219,0,0,0,0,0,0,220,221,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,46,101,222,223,102,103,104,0,0,49,0,0,0,224,0,0,0,0,225,0,0,0,226,0,0,227,197,198,0,0,876,0,0,228,0,229,230,0,0,231,232,199,0,98,0,200,0,0,0,0,0,201,202,0,0,0,0,0,0,203,204,205,206,207,208,209,210,0,211,0,0,0,212,0,0,0,213,0,0,0,0,0,0,0,214,215,216,0,217,0,0,0,0,0,0,0,0,0,0,0,0,0,0,218,219,0,0,0,0,0,0,220,221,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,46,101,222,223,102,103,104,0,0,49,0,0,0,224,0,0,0,0,225,0,0,0,226,0,0,227,197,198,0,0,0,0,0,228,0,229,230,0,0,231,232,199,0,98,0,200,0,0,0,0,0,201,202,0,0,0,0,0,0,203,204,205,206,207,208,209,210,0,211,0,0,0,212,0,0,0,213,0,0,0,0,0,0,0,214,215,216,0,217,0,0,0,0,0,0,0,0,0,0,0,0,0,0,218,219,0,0,0,0,0,0,220,221,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,882,101,222,223,102,103,104,0,0,49,0,197,198,224,0,0,0,0,225,0,0,0,226,0,0,227,199,0,98,0,0,0,0,228,0,229,230,0,0,231,232,0,0,0,203,204,205,206,207,208,209,210,0,211,0,0,0,0,0,0,0,213,0,0,0,0,0,0,0,214,215,216,0,217,0,0,0,0,0,0,0,0,0,0,0,0,0,0,218,219,0,0,0,0,0,0,220,221,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,46,101,222,223,102,103,104,0,0,49,0,197,198,224,0,839,0,0,225,0,0,0,226,0,0,227,199,0,98,0,0,0,0,228,0,229,230,0,0,231,232,0,0,0,203,204,205,206,207,208,209,210,0,211,0,0,0,0,0,0,0,213,0,0,0,0,0,0,0,214,215,216,0,217,0,0,0,0,0,0,0,0,0,0,0,0,0,0,218,219,0,0,0,0,0,0,220,221,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,46,101,222,223,102,103,104,0,0,49,0,197,198,224,0,0,0,0,225,0,0,0,391,0,0,227,199,0,98,0,0,0,0,228,0,229,230,0,0,231,232,0,0,0,203,204,205,206,207,208,209,210,0,211,0,0,0,0,0,0,0,213,0,0,0,0,0,0,0,214,215,216,0,217,0,0,0,0,0,0,0,0,0,0,0,0,0,0,218,219,0,0,0,0,0,0,220,221,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,46,101,222,223,102,103,104,0,0,49,0,197,198,224,0,0,0,0,225,0,0,0,394,0,0,227,199,0,98,0,0,0,0,228,0,229,230,0,0,231,232,0,0,0,203,204,205,206,207,208,209,210,0,211,0,0,0,0,0,0,0,213,0,0,0,0,0,0,0,214,215,216,0,217,0,0,0,0,0,0,0,0,0,0,0,0,0,0,218,219,0,0,0,0,0,0,220,221,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,46,101,222,223,102,103,104,0,0,49,0,197,198,224,0,0,0,0,225,0,0,0,226,0,0,227,199,0,98,0,0,0,0,228,0,229,230,0,0,231,232,0,0,0,203,204,205,206,207,208,209,210,0,211,0,0,0,0,0,0,0,213,0,0,0,0,0,0,0,214,215,216,0,217,0,0,0,0,0,0,0,0,0,0,0,0,0,0,218,219,0,0,0,0,0,0,220,221,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,46,101,222,223,102,103,104,0,0,49,0,0,0,224,0,0,0,0,225,0,0,0,1094,0,0,227,0,0,0,0,0,0,0,228,0,229,230,0,0,231,232};
# 3110
static short Cyc_yycheck[8408U]={0,77,450,321,337,373,77,119,120,140,435,344,432,0,130,171,434,225,201,171,200,229,22,139,440,323,26,437,398,81,226,445,201,154,332,22,551,284,125,26,248,106,261,622,291,600,770,378,486,370,53,472,190,53,348,1072,196,351,352,1076,354,614,690,63,114,154,69,120,5,6,70,71,72,1071,74,1073,63,166,18,275,120,81,82,70,71,72,859,74,446,447,617,518,607,279,81,277,161,1059,163,114,28,616,154,609,114,155,0,115,136,134,136,134,135,166,166,133,168,167,94,142,367,307,549,123,124,132,166,145,22,154,134,425,26,571,137,161,123,124,717,386,155,151,142,143,144,155,146,147,148,149,150,28,160,106,154,142,143,144,165,146,147,148,149,150,167,156,166,154,168,63,295,99,593,151,174,1137,70,71,72,166,74,168,307,111,908,161,164,81,141,157,1207,909,1209,966,134,391,134,393,394,167,973,266,523,73,84,711,136,1205,1206,389,1208,839,1210,150,22,134,154,151,26,415,141,154,99,750,295,152,781,161,63,123,124,161,149,322,111,154,307,158,1255,156,1202,125,161,154,171,172,454,387,142,143,144,535,146,147,148,149,150,328,1256,141,154,594,70,71,72,549,74,379,566,134,322,141,166,81,168,156,158,277,143,149,998,500,163,862,115,116,865,148,123,124,277,156,730,380,159,157,348,876,629,351,352,1025,354,148,188,167,486,142,143,144,134,146,147,148,149,150,319,755,148,143,690,151,322,154,155,643,151,255,152,511,512,513,156,136,156,322,161,157,141,267,134,165,166,343,765,134,154,167,348,143,353,351,352,469,354,134,343,473,166,148,168,348,156,142,351,352,157,354,157,485,923,924,161,425,154,1093,167,154,255,58,1098,258,162,383,261,94,95,914,97,968,267,571,1116,505,506,1113,134,976,107,279,157,928,134,157,142,143,144,532,162,134,167,143,477,609,929,295,784,529,156,110,111,112,547,148,146,425,151,307,322,154,748,148,154,801,156,768,134,151,425,156,167,163,149,170,156,143,816,161,328,343,1168,149,153,154,348,893,664,351,352,156,354,532,134,135,535,156,597,788,133,839,142,162,163,174,175,176,547,152,549,600,918,919,920,921,778,148,370,151,151,156,165,166,604,157,378,691,380,693,816,134,148,148,148,568,151,1223,151,142,143,144,575,511,512,513,322,711,161,151,149,148,520,156,151,157,511,512,513,1245,163,134,151,425,532,585,534,343,157,142,143,144,348,149,594,351,352,146,354,547,600,149,730,689,154,154,153,156,556,149,558,487,560,0,163,162,163,721,899,495,151,556,136,558,905,560,157,755,732,151,146,149,732,154,720,134,23,585,154,161,156,28,960,30,151,731,594,163,156,1124,585,846,600,922,161,728,157,148,927,594,133,162,487,136,157,600,53,740,863,425,495,937,145,147,167,500,63,153,873,153,151,148,69,151,151,151,157,818,516,157,77,523,627,80,960,148,83,828,151,151,151,818,151,153,532,157,157,94,157,728,151,828,99,1109,556,843,558,151,560,547,151,740,141,157,111,785,157,151,1044,1102,1103,1099,127,157,753,1101,123,124,18,19,20,1105,96,585,1106,140,1104,1065,812,156,137,157,594,156,108,142,143,144,600,146,147,148,149,150,148,893,156,162,151,833,834,719,126,556,157,558,171,560,778,1021,167,781,614,151,156,148,156,735,151,157,918,919,920,921,43,148,151,629,151,1065,148,1107,1108,151,89,90,91,92,93,196,1091,96,21,22,99,100,101,110,111,112,105,153,154,155,109,148,149,76,150,585,778,707,153,781,166,167,168,152,594,1085,156,156,174,778,600,156,781,156,163,156,165,166,79,80,81,82,1098,156,1002,156,145,148,149,156,1020,117,118,156,255,162,163,258,226,808,261,700,511,512,513,106,267,276,707,278,156,134,135,136,137,138,139,140,113,114,143,1118,1119,1120,1121,156,728,100,101,156,1185,5,6,156,295,1053,159,265,740,156,141,994,156,67,145,156,307,150,151,168,169,753,23,24,150,151,156,326,319,150,151,19,778,134,886,781,156,328,329,156,1225,142,143,144,335,146,1094,886,1096,156,1110,152,788,113,114,156,156,197,156,199,200,201,353,156,165,166,989,121,122,123,124,125,126,127,128,129,130,1223,1046,1127,220,221,150,151,342,225,897,898,119,229,923,924,168,160,73,169,387,149,141,153,79,80,81,82,134,134,134,1167,248,156,156,134,148,155,142,143,144,1021,778,156,145,781,902,136,152,157,150,106,156,150,150,150,150,391,150,163,394,165,166,991,886,149,149,401,150,403,404,405,134,148,1198,156,991,411,156,134,156,148,1086,1123,156,150,141,1086,161,134,145,157,161,1021,149,141,167,902,157,433,150,164,146,120,134,134,1021,167,156,150,922,923,924,143,1222,927,146,157,1098,154,487,134,152,886,1248,135,156,343,495,142,143,144,155,500,155,165,166,155,157,152,474,516,151,156,151,161,157,150,161,26,27,151,165,166,156,134,160,156,1157,156,151,157,172,49,153,532,43,149,535,991,1098,156,547,389,161,1219,551,31,134,148,547,47,549,1098,150,156,142,143,144,556,167,558,157,560,150,157,152,157,157,1021,151,134,157,161,157,134,151,151,151,151,43,151,1216,142,143,144,432,1216,434,435,436,437,597,152,440,151,151,156,991,445,446,447,607,157,450,157,165,166,454,157,157,616,119,1223,578,579,134,157,582,157,157,585,136,150,157,143,151,134,135,157,148,138,139,140,152,142,143,150,156,148,486,164,149,164,136,152,1098,165,166,156,48,158,159,157,157,1021,119,157,157,149,167,156,169,170,150,1223,173,174,131,156,148,134,135,149,151,138,139,140,1223,142,143,321,148,157,148,150,149,156,156,152,151,151,116,156,156,158,159,156,156,162,163,149,154,700,43,26,169,161,157,134,707,134,157,164,157,157,721,157,157,157,164,148,151,157,50,571,156,732,149,149,161,728,1098,150,148,740,148,377,157,141,151,157,157,740,157,151,156,151,157,157,157,156,141,156,135,156,753,156,398,156,156,725,402,156,761,150,406,407,408,409,410,1223,149,413,414,150,29,417,418,157,119,149,156,119,119,161,148,160,428,150,430,157,131,157,157,134,135,157,157,138,139,140,156,142,143,156,770,26,136,151,149,157,150,152,157,455,150,156,664,158,159,150,43,162,163,148,157,467,136,151,169,156,472,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,157,136,151,151,151,151,151,151,135,1223,150,150,26,151,157,38,39,40,150,42,157,157,151,157,157,156,136,157,0,517,151,157,157,157,157,58,730,151,891,157,157,886,157,0,157,157,69,70,1216,1157,119,739,139,44,891,83,326,902,56,380,83,755,131,753,87,134,135,189,808,138,139,140,801,142,143,98,929,319,298,102,149,700,788,152,1011,108,436,156,1010,158,159,488,1013,359,906,1014,908,902,1060,984,169,320,1134,1219,985,593,1075,- 1,131,597,- 1,134,416,- 1,- 1,- 1,- 1,- 1,141,142,143,144,934,146,818,148,- 1,- 1,- 1,152,- 1,- 1,- 1,156,828,158,159,- 1,- 1,- 1,163,994,165,166,989,- 1,991,- 1,- 1,843,- 1,- 1,- 1,- 1,- 1,643,- 1,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,38,39,40,1002,42,- 1,- 1,- 1,- 1,- 1,- 1,- 1,893,- 1,- 1,690,- 1,- 1,- 1,- 1,58,- 1,- 1,- 1,- 1,- 1,1025,- 1,- 1,- 1,- 1,69,70,- 1,- 1,73,- 1,918,919,920,921,79,80,81,82,83,- 1,722,- 1,87,- 1,- 1,- 1,- 1,1086,- 1,- 1,- 1,733,- 1,98,- 1,737,- 1,102,- 1,- 1,- 1,106,- 1,108,- 1,- 1,748,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,765,- 1,- 1,- 1,- 1,1094,- 1,- 1,- 1,- 1,- 1,1100,- 1,141,142,- 1,144,145,146,- 1,- 1,149,1111,- 1,- 1,- 1,- 1,1116,- 1,1000,158,159,- 1,26,27,1157,- 1,- 1,1127,- 1,- 1,- 1,- 1,1132,- 1,- 1,- 1,- 1,41,1020,43,816,- 1,- 1,- 1,820,821,822,823,- 1,825,826,- 1,- 1,- 1,- 1,59,60,61,62,63,64,65,66,839,68,- 1,842,- 1,- 1,- 1,- 1,- 1,76,- 1,- 1,- 1,- 1,- 1,- 1,- 1,84,85,86,1216,88,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,103,104,- 1,- 1,- 1,- 1,- 1,- 1,111,112,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,1099,- 1,1101,1102,1103,1104,1105,1106,1107,1108,1109,1110,- 1,134,135,136,137,138,139,140,- 1,- 1,143,- 1,- 1,- 1,147,- 1,149,- 1,- 1,152,- 1,- 1,- 1,156,- 1,- 1,159,- 1,- 1,- 1,- 1,- 1,937,- 1,167,- 1,169,170,- 1,- 1,173,174,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,960,1167,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,- 1,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,- 1,- 1,- 1,- 1,51,52,53,54,- 1,- 1,1222,58,59,60,61,62,63,64,65,66,- 1,68,69,70,71,72,- 1,- 1,- 1,76,- 1,- 1,- 1,- 1,- 1,- 1,83,84,85,86,87,88,- 1,- 1,- 1,- 1,1052,- 1,1054,- 1,- 1,98,- 1,- 1,- 1,102,103,104,- 1,1065,- 1,108,- 1,- 1,111,112,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,134,135,136,137,138,139,140,141,142,143,144,- 1,146,147,148,149,- 1,- 1,152,- 1,- 1,- 1,156,- 1,158,159,- 1,- 1,- 1,- 1,- 1,- 1,- 1,167,- 1,169,170,- 1,- 1,173,174,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,- 1,- 1,25,26,27,28,- 1,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,- 1,- 1,- 1,- 1,51,52,53,54,- 1,- 1,- 1,58,59,60,61,62,63,64,65,66,- 1,68,69,70,71,72,- 1,- 1,- 1,76,- 1,- 1,- 1,- 1,- 1,- 1,83,84,85,86,87,88,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,98,- 1,- 1,- 1,102,103,104,- 1,- 1,- 1,108,- 1,- 1,111,112,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,134,135,136,137,138,139,140,141,142,143,144,- 1,146,147,148,149,150,- 1,152,- 1,- 1,- 1,156,- 1,158,159,- 1,- 1,- 1,- 1,- 1,- 1,- 1,167,- 1,169,170,- 1,- 1,173,174,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,- 1,- 1,25,26,27,28,- 1,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,- 1,- 1,- 1,- 1,51,52,53,54,- 1,- 1,- 1,58,59,60,61,62,63,64,65,66,- 1,68,69,70,71,72,- 1,- 1,- 1,76,- 1,- 1,- 1,- 1,- 1,- 1,83,84,85,86,87,88,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,98,- 1,- 1,- 1,102,103,104,- 1,- 1,- 1,108,- 1,- 1,111,112,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,134,135,136,137,138,139,140,141,142,143,144,- 1,146,147,148,149,150,- 1,152,- 1,- 1,- 1,156,- 1,158,159,- 1,- 1,- 1,- 1,- 1,- 1,- 1,167,- 1,169,170,- 1,- 1,173,174,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,- 1,- 1,25,26,27,28,- 1,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,- 1,- 1,- 1,- 1,51,52,53,54,- 1,- 1,- 1,58,59,60,61,62,63,64,65,66,- 1,68,69,70,71,72,- 1,- 1,- 1,76,- 1,- 1,- 1,- 1,- 1,- 1,83,84,85,86,87,88,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,98,- 1,- 1,- 1,102,103,104,- 1,- 1,- 1,108,- 1,- 1,111,112,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,134,135,136,137,138,139,140,141,142,143,144,- 1,146,147,148,149,- 1,- 1,152,- 1,- 1,- 1,156,- 1,158,159,- 1,- 1,- 1,- 1,- 1,- 1,- 1,167,- 1,169,170,- 1,- 1,173,174,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,- 1,- 1,25,26,27,28,- 1,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,- 1,- 1,- 1,- 1,51,52,53,54,- 1,- 1,- 1,58,59,60,61,62,63,64,65,66,- 1,68,69,70,71,72,- 1,- 1,- 1,76,- 1,- 1,- 1,- 1,- 1,- 1,83,84,85,86,87,88,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,98,- 1,- 1,- 1,102,103,104,- 1,- 1,- 1,108,- 1,- 1,111,112,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,134,135,136,137,138,139,140,141,142,143,144,- 1,146,147,148,149,- 1,- 1,152,- 1,- 1,- 1,156,- 1,158,159,- 1,- 1,- 1,- 1,- 1,- 1,- 1,167,- 1,169,170,- 1,- 1,173,174,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,- 1,- 1,25,26,27,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,38,39,40,41,42,43,44,45,- 1,- 1,- 1,- 1,- 1,51,52,53,- 1,- 1,- 1,- 1,58,59,60,61,62,63,64,65,66,- 1,68,69,70,71,72,- 1,- 1,- 1,76,- 1,- 1,- 1,- 1,- 1,- 1,83,84,85,86,87,88,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,98,- 1,- 1,- 1,102,103,104,- 1,- 1,- 1,108,- 1,- 1,111,112,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,134,135,136,137,138,139,140,141,142,143,144,- 1,146,147,- 1,- 1,- 1,- 1,152,- 1,- 1,- 1,156,- 1,158,159,- 1,- 1,- 1,- 1,- 1,- 1,- 1,167,- 1,169,170,- 1,- 1,173,174,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,- 1,- 1,25,26,27,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,38,39,40,41,42,43,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,53,- 1,- 1,- 1,- 1,58,59,60,61,62,63,64,65,66,- 1,68,69,70,- 1,- 1,- 1,- 1,- 1,76,- 1,- 1,- 1,- 1,- 1,- 1,83,84,85,86,87,88,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,98,- 1,- 1,- 1,102,103,104,- 1,- 1,- 1,108,- 1,- 1,111,112,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,134,135,136,137,138,139,140,141,142,143,144,- 1,146,147,- 1,- 1,- 1,- 1,152,- 1,- 1,- 1,156,- 1,158,159,- 1,- 1,- 1,- 1,- 1,- 1,- 1,167,- 1,169,170,- 1,- 1,173,174,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,- 1,- 1,- 1,26,27,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,38,39,40,41,42,43,- 1,45,- 1,- 1,- 1,- 1,- 1,51,52,- 1,- 1,- 1,- 1,- 1,58,59,60,61,62,63,64,65,66,- 1,68,69,70,- 1,72,- 1,- 1,- 1,76,- 1,- 1,- 1,- 1,- 1,- 1,83,84,85,86,87,88,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,98,- 1,- 1,- 1,102,103,104,- 1,- 1,- 1,108,- 1,- 1,111,112,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,134,135,136,137,138,139,140,141,142,143,144,- 1,146,147,- 1,149,- 1,- 1,152,- 1,- 1,- 1,156,- 1,158,159,- 1,- 1,- 1,- 1,- 1,- 1,- 1,167,- 1,169,170,- 1,- 1,173,174,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,- 1,- 1,- 1,26,27,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,38,39,40,41,42,43,- 1,45,- 1,- 1,- 1,- 1,- 1,51,52,- 1,- 1,- 1,- 1,- 1,58,59,60,61,62,63,64,65,66,- 1,68,69,70,- 1,72,- 1,- 1,- 1,76,- 1,- 1,- 1,- 1,- 1,- 1,83,84,85,86,87,88,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,98,- 1,- 1,- 1,102,103,104,- 1,- 1,- 1,108,- 1,- 1,111,112,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,134,135,136,137,138,139,140,141,142,143,144,- 1,146,147,- 1,- 1,- 1,- 1,152,- 1,- 1,- 1,156,- 1,158,159,- 1,- 1,- 1,- 1,- 1,- 1,- 1,167,- 1,169,170,- 1,43,173,174,0,1,- 1,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,- 1,- 1,25,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,38,39,40,41,42,- 1,44,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,53,- 1,55,56,57,58,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,69,70,71,- 1,119,74,75,- 1,77,78,- 1,- 1,- 1,- 1,83,- 1,131,- 1,87,134,135,- 1,- 1,138,139,140,- 1,142,143,98,- 1,- 1,- 1,102,149,- 1,- 1,152,- 1,108,- 1,156,- 1,158,159,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,169,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,134,- 1,- 1,- 1,- 1,- 1,- 1,141,142,143,144,- 1,146,- 1,- 1,- 1,150,- 1,152,- 1,- 1,- 1,156,- 1,158,159,- 1,- 1,- 1,- 1,43,165,166,0,1,- 1,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,- 1,- 1,25,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,38,39,40,41,42,- 1,44,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,53,- 1,55,56,57,58,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,69,70,71,- 1,119,74,75,- 1,77,78,- 1,- 1,- 1,- 1,83,- 1,- 1,- 1,87,134,135,- 1,- 1,138,139,140,- 1,142,143,98,- 1,- 1,- 1,102,149,- 1,- 1,152,- 1,108,- 1,156,- 1,158,159,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,169,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,134,- 1,- 1,- 1,- 1,- 1,- 1,141,142,143,144,- 1,146,- 1,- 1,- 1,- 1,- 1,152,- 1,- 1,- 1,156,- 1,158,159,- 1,- 1,- 1,- 1,- 1,165,166,1,- 1,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,- 1,- 1,25,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,38,39,40,41,42,- 1,44,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,53,- 1,55,56,57,58,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,69,70,71,- 1,- 1,74,75,- 1,77,78,- 1,- 1,- 1,- 1,83,- 1,- 1,- 1,87,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,98,- 1,- 1,- 1,102,- 1,- 1,- 1,43,- 1,108,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,- 1,- 1,134,- 1,- 1,- 1,- 1,- 1,- 1,141,142,143,144,- 1,146,38,39,40,150,42,152,- 1,- 1,- 1,156,- 1,158,159,- 1,- 1,- 1,- 1,- 1,165,166,58,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,69,70,- 1,- 1,119,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,83,- 1,- 1,- 1,87,134,135,- 1,- 1,138,139,140,- 1,142,143,98,- 1,- 1,- 1,102,149,- 1,- 1,152,- 1,108,- 1,156,- 1,158,159,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,169,- 1,- 1,- 1,- 1,- 1,- 1,- 1,131,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,141,142,- 1,144,- 1,146,- 1,148,- 1,- 1,- 1,152,- 1,- 1,- 1,156,- 1,158,159,- 1,- 1,- 1,163,- 1,165,166,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,38,39,40,- 1,42,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,58,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,69,70,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,83,- 1,- 1,- 1,87,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,98,- 1,- 1,- 1,102,- 1,- 1,- 1,- 1,- 1,108,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,134,- 1,- 1,- 1,- 1,- 1,- 1,141,142,143,144,- 1,146,- 1,- 1,- 1,- 1,- 1,152,- 1,- 1,- 1,156,- 1,158,159,- 1,- 1,- 1,- 1,- 1,165,166,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,- 1,- 1,25,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,38,39,40,41,42,- 1,44,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,53,- 1,- 1,- 1,- 1,58,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,69,70,71,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,83,- 1,- 1,- 1,87,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,98,- 1,- 1,- 1,102,- 1,- 1,- 1,- 1,- 1,108,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,141,142,- 1,144,- 1,146,147,- 1,149,- 1,- 1,- 1,153,- 1,- 1,- 1,- 1,158,159,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,- 1,- 1,25,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,38,39,40,41,42,- 1,44,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,53,- 1,- 1,- 1,- 1,58,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,69,70,71,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,83,- 1,- 1,- 1,87,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,98,- 1,- 1,- 1,102,- 1,- 1,- 1,- 1,- 1,108,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,141,142,- 1,144,- 1,146,147,- 1,149,- 1,- 1,- 1,153,- 1,- 1,- 1,- 1,158,159,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,- 1,- 1,25,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,38,39,40,41,42,- 1,44,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,53,- 1,- 1,- 1,- 1,58,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,69,70,71,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,83,- 1,- 1,- 1,87,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,98,- 1,- 1,- 1,102,- 1,- 1,- 1,- 1,- 1,108,- 1,- 1,- 1,- 1,- 1,- 1,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,- 1,- 1,25,- 1,- 1,- 1,141,142,- 1,144,- 1,146,- 1,- 1,149,38,39,40,41,42,- 1,44,- 1,158,159,- 1,- 1,- 1,- 1,- 1,53,- 1,- 1,- 1,- 1,58,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,69,70,71,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,83,- 1,- 1,- 1,87,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,98,- 1,- 1,- 1,102,- 1,- 1,- 1,- 1,- 1,108,- 1,- 1,- 1,- 1,- 1,- 1,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,- 1,- 1,25,- 1,- 1,- 1,141,142,- 1,144,- 1,146,- 1,- 1,149,38,39,40,41,42,- 1,- 1,- 1,158,159,- 1,- 1,- 1,- 1,- 1,53,- 1,- 1,- 1,- 1,58,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,69,70,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,83,- 1,- 1,- 1,87,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,98,- 1,- 1,- 1,102,- 1,- 1,- 1,- 1,- 1,108,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,141,142,- 1,144,- 1,146,38,39,40,- 1,42,- 1,- 1,- 1,- 1,- 1,- 1,158,159,- 1,- 1,- 1,- 1,- 1,- 1,- 1,58,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,69,70,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,83,- 1,- 1,- 1,87,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,98,- 1,- 1,- 1,102,- 1,- 1,- 1,- 1,- 1,108,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,131,- 1,- 1,134,- 1,- 1,- 1,- 1,- 1,- 1,141,142,- 1,144,- 1,146,- 1,148,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,158,159,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,38,39,40,- 1,42,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,58,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,69,70,- 1,- 1,38,39,40,- 1,42,- 1,- 1,- 1,- 1,- 1,83,- 1,- 1,- 1,87,- 1,- 1,- 1,- 1,- 1,58,- 1,- 1,- 1,- 1,98,- 1,- 1,- 1,102,- 1,69,70,- 1,- 1,108,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,83,- 1,- 1,- 1,87,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,131,- 1,98,- 1,- 1,- 1,102,- 1,- 1,- 1,141,142,108,144,- 1,146,- 1,148,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,158,159,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,141,142,- 1,144,- 1,146,- 1,- 1,- 1,150,- 1,- 1,- 1,- 1,- 1,- 1,- 1,158,159,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,38,39,40,- 1,42,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,58,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,69,70,- 1,- 1,38,39,40,- 1,42,- 1,- 1,- 1,- 1,- 1,83,- 1,- 1,- 1,87,- 1,- 1,- 1,- 1,- 1,58,- 1,- 1,- 1,- 1,98,- 1,- 1,- 1,102,- 1,69,70,- 1,- 1,108,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,83,- 1,- 1,- 1,87,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,98,- 1,- 1,- 1,102,- 1,- 1,- 1,141,142,108,144,- 1,146,- 1,- 1,- 1,150,- 1,- 1,- 1,- 1,- 1,- 1,- 1,158,159,- 1,- 1,- 1,- 1,- 1,- 1,131,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,141,142,- 1,144,- 1,146,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,158,159,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,38,39,40,- 1,42,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,58,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,69,70,- 1,- 1,38,39,40,- 1,42,- 1,- 1,- 1,- 1,- 1,83,- 1,- 1,- 1,87,- 1,- 1,- 1,- 1,- 1,58,- 1,- 1,- 1,- 1,98,- 1,- 1,- 1,102,- 1,69,70,- 1,- 1,108,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,83,- 1,- 1,- 1,87,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,98,- 1,- 1,- 1,102,- 1,- 1,- 1,141,142,108,144,- 1,146,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,158,159,- 1,- 1,- 1,26,27,28,- 1,30,31,32,33,34,35,36,37,- 1,141,- 1,41,- 1,43,146,45,46,- 1,- 1,- 1,- 1,51,52,- 1,54,- 1,158,159,- 1,59,60,61,62,63,64,65,66,- 1,68,- 1,- 1,- 1,72,- 1,- 1,- 1,76,- 1,- 1,- 1,- 1,- 1,- 1,- 1,84,85,86,- 1,88,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,103,104,- 1,- 1,- 1,- 1,- 1,- 1,111,112,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,134,135,136,137,138,139,140,- 1,- 1,143,- 1,- 1,- 1,147,148,149,- 1,- 1,152,- 1,- 1,- 1,156,- 1,- 1,159,- 1,26,27,- 1,- 1,- 1,- 1,167,33,169,170,- 1,- 1,173,174,- 1,41,- 1,43,- 1,45,- 1,- 1,- 1,- 1,- 1,51,52,- 1,- 1,- 1,- 1,- 1,- 1,59,60,61,62,63,64,65,66,- 1,68,- 1,- 1,- 1,72,- 1,- 1,- 1,76,- 1,- 1,- 1,- 1,- 1,- 1,- 1,84,85,86,- 1,88,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,103,104,- 1,- 1,- 1,- 1,- 1,- 1,111,112,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,134,135,136,137,138,139,140,- 1,142,143,- 1,- 1,- 1,147,- 1,149,150,- 1,152,- 1,- 1,- 1,156,- 1,- 1,159,26,27,162,163,- 1,- 1,- 1,167,- 1,169,170,- 1,- 1,173,174,41,- 1,43,- 1,45,- 1,- 1,- 1,- 1,- 1,51,52,- 1,- 1,- 1,- 1,- 1,- 1,59,60,61,62,63,64,65,66,- 1,68,- 1,- 1,- 1,72,- 1,- 1,- 1,76,- 1,- 1,- 1,- 1,- 1,- 1,- 1,84,85,86,- 1,88,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,103,104,- 1,- 1,- 1,- 1,- 1,- 1,111,112,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,134,135,136,137,138,139,140,- 1,142,143,- 1,- 1,- 1,147,- 1,149,150,- 1,152,- 1,- 1,- 1,156,- 1,- 1,159,26,27,162,163,- 1,- 1,- 1,167,- 1,169,170,- 1,- 1,173,174,41,- 1,43,- 1,45,- 1,- 1,- 1,- 1,- 1,51,52,- 1,- 1,- 1,- 1,- 1,- 1,59,60,61,62,63,64,65,66,- 1,68,- 1,- 1,- 1,72,- 1,- 1,- 1,76,- 1,- 1,- 1,- 1,- 1,- 1,- 1,84,85,86,- 1,88,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,103,104,- 1,- 1,- 1,- 1,- 1,- 1,111,112,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,134,135,136,137,138,139,140,- 1,142,143,- 1,- 1,- 1,147,- 1,149,150,- 1,152,- 1,- 1,- 1,156,- 1,- 1,159,26,27,162,163,- 1,- 1,- 1,167,- 1,169,170,- 1,- 1,173,174,41,- 1,43,- 1,45,- 1,- 1,- 1,- 1,- 1,51,52,- 1,- 1,- 1,- 1,- 1,- 1,59,60,61,62,63,64,65,66,- 1,68,- 1,- 1,- 1,72,- 1,- 1,- 1,76,- 1,- 1,- 1,- 1,- 1,- 1,- 1,84,85,86,- 1,88,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,103,104,- 1,- 1,- 1,- 1,- 1,- 1,111,112,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,134,135,136,137,138,139,140,- 1,142,143,- 1,- 1,- 1,147,- 1,149,150,- 1,152,- 1,- 1,- 1,156,- 1,- 1,159,26,27,162,163,- 1,- 1,- 1,167,- 1,169,170,- 1,- 1,173,174,41,- 1,43,- 1,45,- 1,- 1,- 1,- 1,- 1,51,52,- 1,- 1,- 1,- 1,- 1,- 1,59,60,61,62,63,64,65,66,- 1,68,- 1,- 1,- 1,72,- 1,- 1,- 1,76,- 1,- 1,- 1,- 1,- 1,- 1,- 1,84,85,86,- 1,88,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,103,104,- 1,- 1,- 1,- 1,- 1,- 1,111,112,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,134,135,136,137,138,139,140,- 1,142,143,- 1,- 1,- 1,147,- 1,149,- 1,- 1,152,- 1,- 1,- 1,156,- 1,- 1,159,26,27,162,163,- 1,- 1,- 1,167,- 1,169,170,- 1,- 1,173,174,41,- 1,43,- 1,45,- 1,- 1,- 1,- 1,- 1,51,52,- 1,- 1,- 1,- 1,- 1,- 1,59,60,61,62,63,64,65,66,- 1,68,- 1,- 1,- 1,72,- 1,- 1,- 1,76,- 1,- 1,- 1,- 1,- 1,- 1,- 1,84,85,86,- 1,88,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,103,104,- 1,- 1,- 1,- 1,- 1,- 1,111,112,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,134,135,136,137,138,139,140,141,- 1,143,- 1,- 1,- 1,147,- 1,- 1,- 1,- 1,152,- 1,- 1,- 1,156,- 1,- 1,159,26,27,- 1,- 1,- 1,- 1,- 1,167,- 1,169,170,- 1,- 1,173,174,41,- 1,43,- 1,45,- 1,- 1,- 1,- 1,- 1,51,52,- 1,- 1,- 1,- 1,- 1,- 1,59,60,61,62,63,64,65,66,- 1,68,- 1,- 1,- 1,72,- 1,- 1,- 1,76,- 1,- 1,- 1,- 1,- 1,- 1,- 1,84,85,86,- 1,88,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,103,104,- 1,- 1,- 1,- 1,- 1,- 1,111,112,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,134,135,136,137,138,139,140,- 1,- 1,143,- 1,- 1,- 1,147,- 1,- 1,- 1,- 1,152,- 1,- 1,- 1,156,- 1,- 1,159,26,27,- 1,- 1,164,- 1,- 1,167,- 1,169,170,- 1,- 1,173,174,41,- 1,43,- 1,45,- 1,- 1,- 1,- 1,- 1,51,52,- 1,- 1,- 1,- 1,- 1,- 1,59,60,61,62,63,64,65,66,- 1,68,- 1,- 1,- 1,72,- 1,- 1,- 1,76,- 1,- 1,- 1,- 1,- 1,- 1,- 1,84,85,86,- 1,88,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,103,104,- 1,- 1,- 1,- 1,- 1,- 1,111,112,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,134,135,136,137,138,139,140,- 1,- 1,143,- 1,- 1,- 1,147,- 1,149,- 1,- 1,152,- 1,- 1,- 1,156,- 1,- 1,159,26,27,- 1,- 1,- 1,- 1,- 1,167,- 1,169,170,- 1,- 1,173,174,41,- 1,43,- 1,45,- 1,- 1,- 1,- 1,- 1,51,52,- 1,- 1,- 1,- 1,- 1,- 1,59,60,61,62,63,64,65,66,- 1,68,- 1,- 1,- 1,72,- 1,- 1,- 1,76,- 1,- 1,- 1,- 1,- 1,- 1,- 1,84,85,86,- 1,88,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,103,104,- 1,- 1,- 1,- 1,- 1,- 1,111,112,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,134,135,136,137,138,139,140,- 1,- 1,143,- 1,- 1,- 1,147,148,- 1,- 1,- 1,152,- 1,- 1,- 1,156,- 1,- 1,159,26,27,- 1,- 1,- 1,- 1,- 1,167,- 1,169,170,- 1,- 1,173,174,41,- 1,43,- 1,45,- 1,- 1,- 1,- 1,- 1,51,52,- 1,- 1,- 1,- 1,- 1,- 1,59,60,61,62,63,64,65,66,- 1,68,- 1,- 1,- 1,72,- 1,- 1,- 1,76,- 1,- 1,- 1,- 1,- 1,- 1,- 1,84,85,86,- 1,88,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,103,104,- 1,- 1,- 1,- 1,- 1,- 1,111,112,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,134,135,136,137,138,139,140,- 1,- 1,143,- 1,- 1,- 1,147,- 1,- 1,- 1,- 1,152,- 1,- 1,- 1,156,157,- 1,159,26,27,- 1,- 1,- 1,- 1,- 1,167,- 1,169,170,- 1,- 1,173,174,41,- 1,43,- 1,45,- 1,- 1,- 1,- 1,- 1,51,52,- 1,- 1,- 1,- 1,- 1,- 1,59,60,61,62,63,64,65,66,- 1,68,- 1,- 1,- 1,72,- 1,- 1,- 1,76,- 1,- 1,- 1,- 1,- 1,- 1,- 1,84,85,86,- 1,88,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,103,104,- 1,- 1,- 1,- 1,- 1,- 1,111,112,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,134,135,136,137,138,139,140,- 1,- 1,143,- 1,- 1,- 1,147,- 1,- 1,- 1,- 1,152,- 1,- 1,- 1,156,- 1,- 1,159,26,27,- 1,- 1,164,- 1,- 1,167,- 1,169,170,- 1,- 1,173,174,41,- 1,43,- 1,45,- 1,- 1,- 1,- 1,- 1,51,52,- 1,- 1,- 1,- 1,- 1,- 1,59,60,61,62,63,64,65,66,- 1,68,- 1,- 1,- 1,72,- 1,- 1,- 1,76,- 1,- 1,- 1,- 1,- 1,- 1,- 1,84,85,86,- 1,88,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,103,104,- 1,- 1,- 1,- 1,- 1,- 1,111,112,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,134,135,136,137,138,139,140,- 1,- 1,143,- 1,- 1,- 1,147,- 1,- 1,- 1,- 1,152,- 1,- 1,- 1,156,157,- 1,159,26,27,- 1,- 1,- 1,- 1,- 1,167,- 1,169,170,- 1,- 1,173,174,41,- 1,43,- 1,45,- 1,- 1,- 1,- 1,- 1,51,52,- 1,- 1,- 1,- 1,- 1,- 1,59,60,61,62,63,64,65,66,- 1,68,- 1,- 1,- 1,72,- 1,- 1,- 1,76,- 1,- 1,- 1,- 1,- 1,- 1,- 1,84,85,86,- 1,88,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,103,104,- 1,- 1,- 1,- 1,- 1,- 1,111,112,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,134,135,136,137,138,139,140,- 1,- 1,143,- 1,- 1,- 1,147,- 1,- 1,- 1,- 1,152,- 1,- 1,- 1,156,- 1,- 1,159,26,27,- 1,- 1,164,- 1,- 1,167,- 1,169,170,- 1,- 1,173,174,41,- 1,43,- 1,45,- 1,- 1,- 1,- 1,- 1,51,52,- 1,- 1,- 1,- 1,- 1,- 1,59,60,61,62,63,64,65,66,- 1,68,- 1,- 1,- 1,72,- 1,- 1,- 1,76,- 1,- 1,- 1,- 1,- 1,- 1,- 1,84,85,86,- 1,88,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,103,104,- 1,- 1,- 1,- 1,- 1,- 1,111,112,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,134,135,136,137,138,139,140,- 1,- 1,143,- 1,- 1,- 1,147,- 1,- 1,- 1,- 1,152,- 1,- 1,- 1,156,- 1,- 1,159,26,27,- 1,- 1,164,- 1,- 1,167,- 1,169,170,- 1,- 1,173,174,41,- 1,43,- 1,45,- 1,- 1,- 1,- 1,- 1,51,52,- 1,- 1,- 1,- 1,- 1,- 1,59,60,61,62,63,64,65,66,- 1,68,- 1,- 1,- 1,72,- 1,- 1,- 1,76,- 1,- 1,- 1,- 1,- 1,- 1,- 1,84,85,86,- 1,88,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,103,104,- 1,- 1,- 1,- 1,- 1,- 1,111,112,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,134,135,136,137,138,139,140,- 1,- 1,143,- 1,- 1,- 1,147,- 1,- 1,- 1,- 1,152,- 1,- 1,- 1,156,- 1,- 1,159,26,27,- 1,- 1,- 1,- 1,- 1,167,- 1,169,170,- 1,- 1,173,174,41,- 1,43,- 1,45,- 1,- 1,- 1,- 1,- 1,51,52,- 1,- 1,- 1,- 1,- 1,- 1,59,60,61,62,63,64,65,66,- 1,68,- 1,- 1,- 1,72,- 1,- 1,- 1,76,- 1,- 1,- 1,- 1,- 1,- 1,- 1,84,85,86,- 1,88,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,103,104,- 1,- 1,- 1,- 1,- 1,- 1,111,112,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,134,135,136,137,138,139,140,- 1,- 1,143,- 1,26,27,147,- 1,- 1,- 1,- 1,152,- 1,- 1,- 1,156,- 1,- 1,159,41,- 1,43,- 1,- 1,- 1,- 1,167,- 1,169,170,- 1,- 1,173,174,- 1,- 1,- 1,59,60,61,62,63,64,65,66,- 1,68,- 1,- 1,- 1,- 1,- 1,- 1,- 1,76,- 1,- 1,- 1,- 1,- 1,- 1,- 1,84,85,86,- 1,88,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,103,104,- 1,- 1,- 1,- 1,- 1,- 1,111,112,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,134,135,136,137,138,139,140,- 1,- 1,143,- 1,26,27,147,- 1,149,- 1,- 1,152,- 1,- 1,- 1,156,- 1,- 1,159,41,- 1,43,- 1,- 1,- 1,- 1,167,- 1,169,170,- 1,- 1,173,174,- 1,- 1,- 1,59,60,61,62,63,64,65,66,- 1,68,- 1,- 1,- 1,- 1,- 1,- 1,- 1,76,- 1,- 1,- 1,- 1,- 1,- 1,- 1,84,85,86,- 1,88,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,103,104,- 1,- 1,- 1,- 1,- 1,- 1,111,112,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,134,135,136,137,138,139,140,- 1,- 1,143,- 1,26,27,147,- 1,- 1,- 1,- 1,152,- 1,- 1,- 1,156,- 1,- 1,159,41,- 1,43,- 1,- 1,- 1,- 1,167,- 1,169,170,- 1,- 1,173,174,- 1,- 1,- 1,59,60,61,62,63,64,65,66,- 1,68,- 1,- 1,- 1,- 1,- 1,- 1,- 1,76,- 1,- 1,- 1,- 1,- 1,- 1,- 1,84,85,86,- 1,88,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,103,104,- 1,- 1,- 1,- 1,- 1,- 1,111,112,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,134,135,136,137,138,139,140,- 1,- 1,143,- 1,26,27,147,- 1,- 1,- 1,- 1,152,- 1,- 1,- 1,156,- 1,- 1,159,41,- 1,43,- 1,- 1,- 1,- 1,167,- 1,169,170,- 1,- 1,173,174,- 1,- 1,- 1,59,60,61,62,63,64,65,66,- 1,68,- 1,- 1,- 1,- 1,- 1,- 1,- 1,76,- 1,- 1,- 1,- 1,- 1,- 1,- 1,84,85,86,- 1,88,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,103,104,- 1,- 1,- 1,- 1,- 1,- 1,111,112,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,134,135,136,137,138,139,140,- 1,- 1,143,- 1,26,27,147,- 1,- 1,- 1,- 1,152,- 1,- 1,- 1,156,- 1,- 1,159,41,- 1,43,- 1,- 1,- 1,- 1,167,- 1,169,170,- 1,- 1,173,174,- 1,- 1,- 1,59,60,61,62,63,64,65,66,- 1,68,- 1,- 1,- 1,- 1,- 1,- 1,- 1,76,- 1,- 1,- 1,- 1,- 1,- 1,- 1,84,85,86,- 1,88,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,103,104,- 1,- 1,- 1,- 1,- 1,- 1,111,112,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,- 1,134,135,136,137,138,139,140,- 1,- 1,143,- 1,- 1,- 1,147,- 1,- 1,- 1,- 1,152,- 1,- 1,- 1,156,- 1,- 1,159,- 1,- 1,- 1,- 1,- 1,- 1,- 1,167,- 1,169,170,- 1,- 1,173,174};char Cyc_Yystack_overflow[17U]="Yystack_overflow";struct Cyc_Yystack_overflow_exn_struct{char*tag;int f1;};
# 45 "cycbison.simple"
struct Cyc_Yystack_overflow_exn_struct Cyc_Yystack_overflow_val={Cyc_Yystack_overflow,0};
# 72 "cycbison.simple"
extern void Cyc_yyerror(struct _fat_ptr,int,int);
# 82 "cycbison.simple"
extern int Cyc_yylex(struct Cyc_Lexing_lexbuf*,union Cyc_YYSTYPE*,struct Cyc_Yyltype*);struct Cyc_Yystacktype{union Cyc_YYSTYPE v;struct Cyc_Yyltype l;};struct _tuple33{unsigned f0;struct _tuple0*f1;int f2;};struct _tuple34{struct Cyc_List_List*f0;struct Cyc_Absyn_Exp*f1;};struct _tuple35{void*f0;struct Cyc_List_List*f1;};
# 145 "cycbison.simple"
int Cyc_yyparse(struct _RegionHandle*yyr,struct Cyc_Lexing_lexbuf*yylex_buf){union Cyc_YYSTYPE _T0;struct _fat_ptr _T1;struct _RegionHandle*_T2;void*_T3;struct Cyc_Yystacktype*_T4;struct Cyc_Yystacktype*_T5;struct _RegionHandle*_T6;unsigned _T7;unsigned _T8;unsigned _T9;struct _fat_ptr _TA;int _TB;unsigned char*_TC;short*_TD;int _TE;int _TF;int _T10;int _T11;struct _fat_ptr _T12;int _T13;int _T14;struct Cyc_Yystack_overflow_exn_struct*_T15;struct Cyc_Yystack_overflow_exn_struct*_T16;struct _fat_ptr _T17;int _T18;short*_T19;struct _RegionHandle*_T1A;unsigned _T1B;unsigned _T1C;int _T1D;unsigned _T1E;unsigned _T1F;struct _fat_ptr _T20;unsigned char*_T21;short*_T22;unsigned _T23;int _T24;unsigned _T25;struct _fat_ptr _T26;int _T27;struct Cyc_Yystacktype*_T28;struct _RegionHandle*_T29;unsigned _T2A;unsigned _T2B;int _T2C;unsigned _T2D;unsigned _T2E;struct _fat_ptr _T2F;unsigned _T30;int _T31;unsigned char*_T32;struct Cyc_Yystacktype*_T33;unsigned _T34;struct _fat_ptr _T35;unsigned char*_T36;struct Cyc_Yystacktype*_T37;short*_T38;int _T39;char*_T3A;short*_T3B;short _T3C;int _T3D;int _T3E;int _T3F;int _T40;struct Cyc_Lexing_lexbuf*_T41;union Cyc_YYSTYPE*_T42;union Cyc_YYSTYPE*_T43;struct Cyc_Yyltype*_T44;struct Cyc_Yyltype*_T45;int _T46;short*_T47;int _T48;short _T49;int _T4A;short*_T4B;int _T4C;short _T4D;int _T4E;int _T4F;short*_T50;int _T51;short _T52;int _T53;int _T54;struct _fat_ptr _T55;int _T56;unsigned char*_T57;struct Cyc_Yystacktype*_T58;struct Cyc_Yystacktype _T59;short*_T5A;int _T5B;char*_T5C;short*_T5D;short _T5E;short*_T5F;int _T60;char*_T61;short*_T62;short _T63;struct _fat_ptr _T64;int _T65;int _T66;int _T67;struct _fat_ptr _T68;unsigned char*_T69;unsigned char*_T6A;struct Cyc_Yystacktype*_T6B;struct Cyc_Yystacktype _T6C;int _T6D;int _T6E;struct Cyc_Yystacktype*_T6F;union Cyc_YYSTYPE*_T70;union Cyc_YYSTYPE*_T71;struct Cyc_Yystacktype*_T72;struct Cyc_Yystacktype _T73;struct Cyc_Yystacktype*_T74;union Cyc_YYSTYPE*_T75;union Cyc_YYSTYPE*_T76;struct Cyc_Yystacktype*_T77;union Cyc_YYSTYPE*_T78;union Cyc_YYSTYPE*_T79;struct Cyc_List_List*_T7A;struct Cyc_Yystacktype*_T7B;union Cyc_YYSTYPE*_T7C;union Cyc_YYSTYPE*_T7D;struct Cyc_List_List*_T7E;struct Cyc_List_List*_T7F;struct Cyc_List_List*_T80;struct Cyc_Absyn_Decl*_T81;struct Cyc_Absyn_Using_d_Absyn_Raw_decl_struct*_T82;struct Cyc_Yystacktype*_T83;union Cyc_YYSTYPE*_T84;union Cyc_YYSTYPE*_T85;struct Cyc_Yystacktype*_T86;union Cyc_YYSTYPE*_T87;union Cyc_YYSTYPE*_T88;struct Cyc_Yystacktype*_T89;struct Cyc_Yystacktype _T8A;struct Cyc_Yyltype _T8B;unsigned _T8C;struct Cyc_List_List*_T8D;struct Cyc_Absyn_Decl*_T8E;struct Cyc_Absyn_Using_d_Absyn_Raw_decl_struct*_T8F;struct Cyc_Yystacktype*_T90;union Cyc_YYSTYPE*_T91;union Cyc_YYSTYPE*_T92;struct Cyc_Yystacktype*_T93;union Cyc_YYSTYPE*_T94;union Cyc_YYSTYPE*_T95;struct Cyc_Yystacktype*_T96;struct Cyc_Yystacktype _T97;struct Cyc_Yyltype _T98;unsigned _T99;struct Cyc_Yystacktype*_T9A;union Cyc_YYSTYPE*_T9B;union Cyc_YYSTYPE*_T9C;struct Cyc_List_List*_T9D;struct Cyc_Absyn_Decl*_T9E;struct Cyc_Absyn_Namespace_d_Absyn_Raw_decl_struct*_T9F;struct _fat_ptr*_TA0;struct Cyc_Yystacktype*_TA1;union Cyc_YYSTYPE*_TA2;union Cyc_YYSTYPE*_TA3;struct Cyc_Yystacktype*_TA4;union Cyc_YYSTYPE*_TA5;union Cyc_YYSTYPE*_TA6;struct Cyc_Yystacktype*_TA7;struct Cyc_Yystacktype _TA8;struct Cyc_Yyltype _TA9;unsigned _TAA;struct Cyc_List_List*_TAB;struct Cyc_Absyn_Decl*_TAC;struct Cyc_Absyn_Namespace_d_Absyn_Raw_decl_struct*_TAD;struct _fat_ptr*_TAE;struct Cyc_Yystacktype*_TAF;union Cyc_YYSTYPE*_TB0;union Cyc_YYSTYPE*_TB1;struct Cyc_Yystacktype*_TB2;union Cyc_YYSTYPE*_TB3;union Cyc_YYSTYPE*_TB4;struct Cyc_Yystacktype*_TB5;struct Cyc_Yystacktype _TB6;struct Cyc_Yyltype _TB7;unsigned _TB8;struct Cyc_Yystacktype*_TB9;union Cyc_YYSTYPE*_TBA;union Cyc_YYSTYPE*_TBB;struct Cyc_Yystacktype*_TBC;union Cyc_YYSTYPE*_TBD;union Cyc_YYSTYPE*_TBE;struct Cyc_List_List*_TBF;struct Cyc_Yystacktype*_TC0;union Cyc_YYSTYPE*_TC1;union Cyc_YYSTYPE*_TC2;struct Cyc_List_List*_TC3;struct Cyc_List_List*_TC4;struct Cyc_Yystacktype*_TC5;union Cyc_YYSTYPE*_TC6;union Cyc_YYSTYPE*_TC7;struct Cyc_Yystacktype*_TC8;union Cyc_YYSTYPE*_TC9;union Cyc_YYSTYPE*_TCA;struct Cyc_Yystacktype*_TCB;union Cyc_YYSTYPE*_TCC;union Cyc_YYSTYPE*_TCD;struct Cyc_Yystacktype*_TCE;union Cyc_YYSTYPE*_TCF;union Cyc_YYSTYPE*_TD0;struct Cyc_Yystacktype*_TD1;struct Cyc_Yystacktype _TD2;struct Cyc_Yyltype _TD3;unsigned _TD4;unsigned _TD5;struct _fat_ptr _TD6;struct _fat_ptr _TD7;struct Cyc_List_List*_TD8;unsigned _TD9;unsigned _TDA;int _TDB;struct Cyc_Yystacktype*_TDC;struct Cyc_Yystacktype _TDD;struct Cyc_Yyltype _TDE;unsigned _TDF;int _TE0;struct Cyc_Yystacktype*_TE1;struct Cyc_Yystacktype _TE2;struct Cyc_Yyltype _TE3;unsigned _TE4;unsigned _TE5;struct _fat_ptr _TE6;struct _fat_ptr _TE7;struct Cyc_List_List*_TE8;struct Cyc_Absyn_Decl*_TE9;struct Cyc_Absyn_ExternCinclude_d_Absyn_Raw_decl_struct*_TEA;struct Cyc_Yystacktype*_TEB;union Cyc_YYSTYPE*_TEC;union Cyc_YYSTYPE*_TED;struct _tuple10*_TEE;struct Cyc_Yystacktype*_TEF;struct Cyc_Yystacktype _TF0;struct Cyc_Yyltype _TF1;unsigned _TF2;struct Cyc_Yystacktype*_TF3;union Cyc_YYSTYPE*_TF4;union Cyc_YYSTYPE*_TF5;struct Cyc_List_List*_TF6;struct Cyc_Absyn_Decl*_TF7;struct Cyc_Absyn_ExternC_d_Absyn_Raw_decl_struct*_TF8;struct Cyc_Yystacktype*_TF9;union Cyc_YYSTYPE*_TFA;union Cyc_YYSTYPE*_TFB;struct Cyc_Yystacktype*_TFC;struct Cyc_Yystacktype _TFD;struct Cyc_Yyltype _TFE;unsigned _TFF;struct Cyc_Yystacktype*_T100;union Cyc_YYSTYPE*_T101;union Cyc_YYSTYPE*_T102;struct Cyc_List_List*_T103;struct Cyc_Absyn_Decl*_T104;struct Cyc_Absyn_ExternCinclude_d_Absyn_Raw_decl_struct*_T105;struct Cyc_Yystacktype*_T106;union Cyc_YYSTYPE*_T107;union Cyc_YYSTYPE*_T108;struct _tuple10*_T109;struct Cyc_Yystacktype*_T10A;struct Cyc_Yystacktype _T10B;struct Cyc_Yyltype _T10C;unsigned _T10D;struct Cyc_Yystacktype*_T10E;union Cyc_YYSTYPE*_T10F;union Cyc_YYSTYPE*_T110;struct Cyc_List_List*_T111;struct Cyc_Absyn_Decl*_T112;struct Cyc_Absyn_Porton_d_Absyn_Raw_decl_struct*_T113;struct Cyc_Absyn_Porton_d_Absyn_Raw_decl_struct*_T114;struct Cyc_Yystacktype*_T115;struct Cyc_Yystacktype _T116;struct Cyc_Yyltype _T117;unsigned _T118;struct Cyc_Yystacktype*_T119;union Cyc_YYSTYPE*_T11A;union Cyc_YYSTYPE*_T11B;struct Cyc_List_List*_T11C;struct Cyc_Absyn_Decl*_T11D;struct Cyc_Absyn_Portoff_d_Absyn_Raw_decl_struct*_T11E;struct Cyc_Absyn_Portoff_d_Absyn_Raw_decl_struct*_T11F;struct Cyc_Yystacktype*_T120;struct Cyc_Yystacktype _T121;struct Cyc_Yyltype _T122;unsigned _T123;struct Cyc_Yystacktype*_T124;union Cyc_YYSTYPE*_T125;union Cyc_YYSTYPE*_T126;struct Cyc_List_List*_T127;struct Cyc_Absyn_Decl*_T128;struct Cyc_Absyn_Tempeston_d_Absyn_Raw_decl_struct*_T129;struct Cyc_Absyn_Tempeston_d_Absyn_Raw_decl_struct*_T12A;struct Cyc_Yystacktype*_T12B;struct Cyc_Yystacktype _T12C;struct Cyc_Yyltype _T12D;unsigned _T12E;struct Cyc_Yystacktype*_T12F;union Cyc_YYSTYPE*_T130;union Cyc_YYSTYPE*_T131;struct Cyc_List_List*_T132;struct Cyc_Absyn_Decl*_T133;struct Cyc_Absyn_Tempestoff_d_Absyn_Raw_decl_struct*_T134;struct Cyc_Absyn_Tempestoff_d_Absyn_Raw_decl_struct*_T135;struct Cyc_Yystacktype*_T136;struct Cyc_Yystacktype _T137;struct Cyc_Yyltype _T138;unsigned _T139;struct Cyc_Yystacktype*_T13A;union Cyc_YYSTYPE*_T13B;union Cyc_YYSTYPE*_T13C;struct Cyc_Yystacktype*_T13D;union Cyc_YYSTYPE*_T13E;union Cyc_YYSTYPE*_T13F;struct _fat_ptr _T140;struct _fat_ptr _T141;int _T142;struct Cyc_Yystacktype*_T143;union Cyc_YYSTYPE*_T144;union Cyc_YYSTYPE*_T145;struct _fat_ptr _T146;struct _fat_ptr _T147;int _T148;struct Cyc_Yystacktype*_T149;struct Cyc_Yystacktype _T14A;struct Cyc_Yyltype _T14B;unsigned _T14C;unsigned _T14D;struct _fat_ptr _T14E;struct _fat_ptr _T14F;struct Cyc_Yystacktype*_T150;struct Cyc_Yystacktype _T151;struct Cyc_List_List*_T152;struct Cyc_Yystacktype*_T153;union Cyc_YYSTYPE*_T154;union Cyc_YYSTYPE*_T155;struct Cyc_List_List*_T156;struct Cyc_Yystacktype*_T157;union Cyc_YYSTYPE*_T158;union Cyc_YYSTYPE*_T159;struct Cyc_List_List*_T15A;struct Cyc_Yystacktype*_T15B;union Cyc_YYSTYPE*_T15C;union Cyc_YYSTYPE*_T15D;struct Cyc_Yystacktype*_T15E;union Cyc_YYSTYPE*_T15F;union Cyc_YYSTYPE*_T160;struct _tuple30*_T161;struct Cyc_Yystacktype*_T162;struct Cyc_Yystacktype _T163;struct _tuple30*_T164;struct Cyc_Yystacktype*_T165;union Cyc_YYSTYPE*_T166;union Cyc_YYSTYPE*_T167;struct _tuple30*_T168;struct _tuple30*_T169;struct Cyc_Yystacktype*_T16A;struct Cyc_Yystacktype _T16B;struct Cyc_Yyltype _T16C;unsigned _T16D;struct Cyc_List_List*_T16E;struct _tuple33*_T16F;struct Cyc_Yystacktype*_T170;struct Cyc_Yystacktype _T171;struct Cyc_Yyltype _T172;unsigned _T173;struct Cyc_Yystacktype*_T174;union Cyc_YYSTYPE*_T175;union Cyc_YYSTYPE*_T176;struct Cyc_List_List*_T177;struct _tuple33*_T178;struct Cyc_Yystacktype*_T179;struct Cyc_Yystacktype _T17A;struct Cyc_Yyltype _T17B;unsigned _T17C;struct Cyc_Yystacktype*_T17D;union Cyc_YYSTYPE*_T17E;union Cyc_YYSTYPE*_T17F;struct Cyc_Yystacktype*_T180;union Cyc_YYSTYPE*_T181;union Cyc_YYSTYPE*_T182;struct Cyc_Yystacktype*_T183;struct Cyc_Yystacktype _T184;struct Cyc_List_List*_T185;struct Cyc_Absyn_Fn_d_Absyn_Raw_decl_struct*_T186;struct Cyc_Yystacktype*_T187;union Cyc_YYSTYPE*_T188;union Cyc_YYSTYPE*_T189;void*_T18A;struct Cyc_Yystacktype*_T18B;struct Cyc_Yystacktype _T18C;struct Cyc_Yyltype _T18D;unsigned _T18E;unsigned _T18F;struct Cyc_Yystacktype*_T190;struct Cyc_Yystacktype _T191;struct _RegionHandle*_T192;struct Cyc_Yystacktype*_T193;union Cyc_YYSTYPE*_T194;union Cyc_YYSTYPE*_T195;struct Cyc_Parse_Declarator _T196;struct Cyc_Yystacktype*_T197;union Cyc_YYSTYPE*_T198;union Cyc_YYSTYPE*_T199;struct Cyc_Absyn_Stmt*_T19A;struct Cyc_Yystacktype*_T19B;struct Cyc_Yystacktype _T19C;struct Cyc_Yyltype _T19D;unsigned _T19E;unsigned _T19F;struct Cyc_Absyn_Fndecl*_T1A0;struct Cyc_Yystacktype*_T1A1;union Cyc_YYSTYPE*_T1A2;union Cyc_YYSTYPE*_T1A3;struct _RegionHandle*_T1A4;struct Cyc_Parse_Declaration_spec*_T1A5;struct Cyc_Parse_Declaration_spec*_T1A6;struct Cyc_Yystacktype*_T1A7;union Cyc_YYSTYPE*_T1A8;union Cyc_YYSTYPE*_T1A9;struct Cyc_Parse_Declarator _T1AA;struct Cyc_Yystacktype*_T1AB;union Cyc_YYSTYPE*_T1AC;union Cyc_YYSTYPE*_T1AD;struct Cyc_Absyn_Stmt*_T1AE;struct Cyc_Yystacktype*_T1AF;struct Cyc_Yystacktype _T1B0;struct Cyc_Yyltype _T1B1;unsigned _T1B2;unsigned _T1B3;struct Cyc_Absyn_Fndecl*_T1B4;struct _RegionHandle*_T1B5;struct Cyc_Yystacktype*_T1B6;union Cyc_YYSTYPE*_T1B7;union Cyc_YYSTYPE*_T1B8;struct Cyc_Parse_Declarator _T1B9;struct Cyc_Yystacktype*_T1BA;union Cyc_YYSTYPE*_T1BB;union Cyc_YYSTYPE*_T1BC;struct Cyc_List_List*_T1BD;struct Cyc_Yystacktype*_T1BE;union Cyc_YYSTYPE*_T1BF;union Cyc_YYSTYPE*_T1C0;struct Cyc_Absyn_Stmt*_T1C1;struct Cyc_Yystacktype*_T1C2;struct Cyc_Yystacktype _T1C3;struct Cyc_Yyltype _T1C4;unsigned _T1C5;unsigned _T1C6;struct Cyc_Absyn_Fndecl*_T1C7;struct Cyc_Yystacktype*_T1C8;union Cyc_YYSTYPE*_T1C9;union Cyc_YYSTYPE*_T1CA;struct _RegionHandle*_T1CB;struct Cyc_Parse_Declaration_spec*_T1CC;struct Cyc_Parse_Declaration_spec*_T1CD;struct Cyc_Yystacktype*_T1CE;union Cyc_YYSTYPE*_T1CF;union Cyc_YYSTYPE*_T1D0;struct Cyc_Parse_Declarator _T1D1;struct Cyc_Yystacktype*_T1D2;union Cyc_YYSTYPE*_T1D3;union Cyc_YYSTYPE*_T1D4;struct Cyc_List_List*_T1D5;struct Cyc_Yystacktype*_T1D6;union Cyc_YYSTYPE*_T1D7;union Cyc_YYSTYPE*_T1D8;struct Cyc_Absyn_Stmt*_T1D9;struct Cyc_Yystacktype*_T1DA;struct Cyc_Yystacktype _T1DB;struct Cyc_Yyltype _T1DC;unsigned _T1DD;unsigned _T1DE;struct Cyc_Absyn_Fndecl*_T1DF;struct Cyc_Yystacktype*_T1E0;union Cyc_YYSTYPE*_T1E1;union Cyc_YYSTYPE*_T1E2;struct _RegionHandle*_T1E3;struct Cyc_Parse_Declaration_spec*_T1E4;struct Cyc_Parse_Declaration_spec*_T1E5;struct Cyc_Yystacktype*_T1E6;union Cyc_YYSTYPE*_T1E7;union Cyc_YYSTYPE*_T1E8;struct Cyc_Parse_Declarator _T1E9;struct Cyc_Yystacktype*_T1EA;union Cyc_YYSTYPE*_T1EB;union Cyc_YYSTYPE*_T1EC;struct Cyc_Absyn_Stmt*_T1ED;struct Cyc_Yystacktype*_T1EE;struct Cyc_Yystacktype _T1EF;struct Cyc_Yyltype _T1F0;unsigned _T1F1;unsigned _T1F2;struct Cyc_Absyn_Fndecl*_T1F3;struct Cyc_Yystacktype*_T1F4;union Cyc_YYSTYPE*_T1F5;union Cyc_YYSTYPE*_T1F6;struct _RegionHandle*_T1F7;struct Cyc_Parse_Declaration_spec*_T1F8;struct Cyc_Parse_Declaration_spec*_T1F9;struct Cyc_Yystacktype*_T1FA;union Cyc_YYSTYPE*_T1FB;union Cyc_YYSTYPE*_T1FC;struct Cyc_Parse_Declarator _T1FD;struct Cyc_Yystacktype*_T1FE;union Cyc_YYSTYPE*_T1FF;union Cyc_YYSTYPE*_T200;struct Cyc_List_List*_T201;struct Cyc_Yystacktype*_T202;union Cyc_YYSTYPE*_T203;union Cyc_YYSTYPE*_T204;struct Cyc_Absyn_Stmt*_T205;struct Cyc_Yystacktype*_T206;struct Cyc_Yystacktype _T207;struct Cyc_Yyltype _T208;unsigned _T209;unsigned _T20A;struct Cyc_Absyn_Fndecl*_T20B;struct Cyc_Yystacktype*_T20C;union Cyc_YYSTYPE*_T20D;union Cyc_YYSTYPE*_T20E;struct _tuple0*_T20F;struct Cyc_Yystacktype*_T210;struct Cyc_Yystacktype _T211;struct _fat_ptr*_T212;struct Cyc_Yystacktype*_T213;union Cyc_YYSTYPE*_T214;union Cyc_YYSTYPE*_T215;struct Cyc_Yystacktype*_T216;struct Cyc_Yystacktype _T217;struct Cyc_Yystacktype*_T218;union Cyc_YYSTYPE*_T219;union Cyc_YYSTYPE*_T21A;struct Cyc_Parse_Declaration_spec _T21B;struct Cyc_Yystacktype*_T21C;struct Cyc_Yystacktype _T21D;struct Cyc_Yyltype _T21E;unsigned _T21F;unsigned _T220;struct Cyc_Yystacktype*_T221;struct Cyc_Yystacktype _T222;struct Cyc_Yyltype _T223;unsigned _T224;unsigned _T225;struct Cyc_List_List*_T226;struct Cyc_Yystacktype*_T227;union Cyc_YYSTYPE*_T228;union Cyc_YYSTYPE*_T229;struct Cyc_Parse_Declaration_spec _T22A;struct _tuple11*(*_T22B)(struct _tuple11*);struct Cyc_Yystacktype*_T22C;union Cyc_YYSTYPE*_T22D;union Cyc_YYSTYPE*_T22E;struct _tuple11*_T22F;struct _tuple11*_T230;struct Cyc_Yystacktype*_T231;struct Cyc_Yystacktype _T232;struct Cyc_Yyltype _T233;unsigned _T234;unsigned _T235;struct Cyc_Yystacktype*_T236;struct Cyc_Yystacktype _T237;struct Cyc_Yyltype _T238;unsigned _T239;unsigned _T23A;struct Cyc_List_List*_T23B;struct Cyc_List_List*_T23C;struct Cyc_Yystacktype*_T23D;union Cyc_YYSTYPE*_T23E;union Cyc_YYSTYPE*_T23F;struct Cyc_Absyn_Pat*_T240;struct Cyc_Yystacktype*_T241;union Cyc_YYSTYPE*_T242;union Cyc_YYSTYPE*_T243;struct Cyc_Absyn_Exp*_T244;struct Cyc_Yystacktype*_T245;struct Cyc_Yystacktype _T246;struct Cyc_Yyltype _T247;unsigned _T248;unsigned _T249;struct Cyc_Yystacktype*_T24A;union Cyc_YYSTYPE*_T24B;union Cyc_YYSTYPE*_T24C;struct _tuple0*_T24D;struct _tuple0*_T24E;struct Cyc_List_List*_T24F;void*_T250;struct Cyc_List_List*_T251;struct _tuple0*_T252;void*_T253;struct Cyc_List_List*_T254;struct Cyc_List_List*_T255;struct Cyc_List_List*_T256;struct Cyc_Yystacktype*_T257;struct Cyc_Yystacktype _T258;struct Cyc_Yyltype _T259;unsigned _T25A;unsigned _T25B;struct Cyc_Yystacktype*_T25C;union Cyc_YYSTYPE*_T25D;union Cyc_YYSTYPE*_T25E;struct _fat_ptr _T25F;struct Cyc_Yystacktype*_T260;struct Cyc_Yystacktype _T261;struct Cyc_Yyltype _T262;unsigned _T263;unsigned _T264;struct Cyc_Absyn_Tvar*_T265;struct _fat_ptr*_T266;struct Cyc_Yystacktype*_T267;union Cyc_YYSTYPE*_T268;union Cyc_YYSTYPE*_T269;struct Cyc_Absyn_Tvar*_T26A;struct Cyc_Absyn_Tvar*_T26B;struct Cyc_Absyn_Kind*_T26C;struct Cyc_Absyn_Kind*_T26D;struct Cyc_Absyn_Tvar*_T26E;struct Cyc_Yystacktype*_T26F;struct Cyc_Yystacktype _T270;struct Cyc_Yyltype _T271;unsigned _T272;unsigned _T273;struct _tuple0*_T274;struct _fat_ptr*_T275;struct Cyc_Yystacktype*_T276;union Cyc_YYSTYPE*_T277;union Cyc_YYSTYPE*_T278;void*_T279;struct Cyc_List_List*_T27A;struct Cyc_Absyn_Tvar*_T27B;struct Cyc_Absyn_Vardecl*_T27C;struct Cyc_Yystacktype*_T27D;struct Cyc_Yystacktype _T27E;struct Cyc_Yyltype _T27F;unsigned _T280;unsigned _T281;struct _fat_ptr _T282;struct Cyc_String_pa_PrintArg_struct _T283;struct Cyc_Yystacktype*_T284;union Cyc_YYSTYPE*_T285;union Cyc_YYSTYPE*_T286;struct _fat_ptr _T287;struct _fat_ptr _T288;struct Cyc_Yystacktype*_T289;union Cyc_YYSTYPE*_T28A;union Cyc_YYSTYPE*_T28B;struct _fat_ptr _T28C;struct Cyc_Yystacktype*_T28D;struct Cyc_Yystacktype _T28E;struct Cyc_Yyltype _T28F;unsigned _T290;unsigned _T291;struct Cyc_Absyn_Tvar*_T292;struct _fat_ptr*_T293;struct Cyc_Absyn_Tvar*_T294;struct Cyc_Absyn_Tvar*_T295;struct Cyc_Absyn_Kind*_T296;struct Cyc_Absyn_Kind*_T297;struct Cyc_Absyn_Tvar*_T298;struct Cyc_Yystacktype*_T299;struct Cyc_Yystacktype _T29A;struct Cyc_Yyltype _T29B;unsigned _T29C;unsigned _T29D;struct _tuple0*_T29E;struct _fat_ptr*_T29F;struct Cyc_Yystacktype*_T2A0;union Cyc_YYSTYPE*_T2A1;union Cyc_YYSTYPE*_T2A2;void*_T2A3;struct Cyc_List_List*_T2A4;struct Cyc_Absyn_Tvar*_T2A5;struct Cyc_Absyn_Vardecl*_T2A6;struct Cyc_Yystacktype*_T2A7;union Cyc_YYSTYPE*_T2A8;union Cyc_YYSTYPE*_T2A9;struct Cyc_Absyn_Exp*_T2AA;struct Cyc_Yystacktype*_T2AB;struct Cyc_Yystacktype _T2AC;struct Cyc_Yyltype _T2AD;unsigned _T2AE;unsigned _T2AF;struct Cyc_Yystacktype*_T2B0;union Cyc_YYSTYPE*_T2B1;union Cyc_YYSTYPE*_T2B2;struct _fat_ptr _T2B3;struct _fat_ptr _T2B4;int _T2B5;struct Cyc_Yystacktype*_T2B6;struct Cyc_Yystacktype _T2B7;struct Cyc_Yyltype _T2B8;unsigned _T2B9;unsigned _T2BA;struct _fat_ptr _T2BB;struct _fat_ptr _T2BC;struct Cyc_Yystacktype*_T2BD;union Cyc_YYSTYPE*_T2BE;union Cyc_YYSTYPE*_T2BF;struct Cyc_Absyn_Exp*_T2C0;struct Cyc_Yystacktype*_T2C1;struct Cyc_Yystacktype _T2C2;struct Cyc_Yystacktype*_T2C3;union Cyc_YYSTYPE*_T2C4;union Cyc_YYSTYPE*_T2C5;struct Cyc_List_List*_T2C6;struct Cyc_Yystacktype*_T2C7;union Cyc_YYSTYPE*_T2C8;union Cyc_YYSTYPE*_T2C9;struct Cyc_List_List*_T2CA;struct Cyc_List_List*_T2CB;struct Cyc_Parse_Declaration_spec _T2CC;struct Cyc_Yystacktype*_T2CD;union Cyc_YYSTYPE*_T2CE;union Cyc_YYSTYPE*_T2CF;struct Cyc_Yystacktype*_T2D0;struct Cyc_Yystacktype _T2D1;struct Cyc_Yyltype _T2D2;unsigned _T2D3;unsigned _T2D4;struct Cyc_Yystacktype*_T2D5;union Cyc_YYSTYPE*_T2D6;union Cyc_YYSTYPE*_T2D7;struct Cyc_Parse_Declaration_spec _T2D8;enum Cyc_Parse_Storage_class _T2D9;int _T2DA;struct Cyc_Yystacktype*_T2DB;struct Cyc_Yystacktype _T2DC;struct Cyc_Yyltype _T2DD;unsigned _T2DE;unsigned _T2DF;struct _fat_ptr _T2E0;struct _fat_ptr _T2E1;struct Cyc_Parse_Declaration_spec _T2E2;struct Cyc_Yystacktype*_T2E3;union Cyc_YYSTYPE*_T2E4;union Cyc_YYSTYPE*_T2E5;struct Cyc_Parse_Declaration_spec _T2E6;struct Cyc_Parse_Declaration_spec _T2E7;struct Cyc_Parse_Declaration_spec _T2E8;struct Cyc_Parse_Declaration_spec _T2E9;struct Cyc_Yystacktype*_T2EA;struct Cyc_Yystacktype _T2EB;struct Cyc_Yyltype _T2EC;unsigned _T2ED;unsigned _T2EE;struct _fat_ptr _T2EF;struct _fat_ptr _T2F0;struct Cyc_Yystacktype*_T2F1;struct Cyc_Yystacktype _T2F2;struct Cyc_Parse_Declaration_spec _T2F3;struct Cyc_Yystacktype*_T2F4;struct Cyc_Yystacktype _T2F5;struct Cyc_Yyltype _T2F6;unsigned _T2F7;unsigned _T2F8;struct Cyc_Yystacktype*_T2F9;union Cyc_YYSTYPE*_T2FA;union Cyc_YYSTYPE*_T2FB;struct Cyc_Yystacktype*_T2FC;union Cyc_YYSTYPE*_T2FD;union Cyc_YYSTYPE*_T2FE;struct Cyc_Parse_Declaration_spec _T2FF;struct Cyc_Parse_Declaration_spec _T300;struct Cyc_Parse_Declaration_spec _T301;struct Cyc_Yystacktype*_T302;struct Cyc_Yystacktype _T303;struct Cyc_Yyltype _T304;unsigned _T305;unsigned _T306;struct Cyc_Parse_Declaration_spec _T307;struct Cyc_Parse_Type_specifier _T308;struct Cyc_Yystacktype*_T309;union Cyc_YYSTYPE*_T30A;union Cyc_YYSTYPE*_T30B;struct Cyc_Parse_Type_specifier _T30C;struct Cyc_Parse_Declaration_spec _T30D;struct Cyc_Parse_Declaration_spec _T30E;struct Cyc_Parse_Declaration_spec _T30F;struct Cyc_Yystacktype*_T310;union Cyc_YYSTYPE*_T311;union Cyc_YYSTYPE*_T312;struct Cyc_Yystacktype*_T313;union Cyc_YYSTYPE*_T314;union Cyc_YYSTYPE*_T315;struct Cyc_Parse_Declaration_spec _T316;struct Cyc_Parse_Declaration_spec _T317;struct Cyc_Yystacktype*_T318;union Cyc_YYSTYPE*_T319;union Cyc_YYSTYPE*_T31A;struct Cyc_Absyn_Tqual _T31B;struct Cyc_Parse_Declaration_spec _T31C;struct Cyc_Absyn_Tqual _T31D;struct Cyc_Parse_Declaration_spec _T31E;struct Cyc_Parse_Declaration_spec _T31F;struct Cyc_Parse_Declaration_spec _T320;struct Cyc_Parse_Declaration_spec _T321;struct Cyc_Yystacktype*_T322;struct Cyc_Yystacktype _T323;struct Cyc_Yyltype _T324;unsigned _T325;unsigned _T326;struct Cyc_Yystacktype*_T327;union Cyc_YYSTYPE*_T328;union Cyc_YYSTYPE*_T329;struct Cyc_Parse_Declaration_spec _T32A;struct Cyc_Parse_Declaration_spec _T32B;struct Cyc_Parse_Declaration_spec _T32C;struct Cyc_Parse_Declaration_spec _T32D;struct Cyc_Parse_Declaration_spec _T32E;struct Cyc_Parse_Declaration_spec _T32F;struct Cyc_Yystacktype*_T330;struct Cyc_Yystacktype _T331;struct Cyc_Yyltype _T332;unsigned _T333;unsigned _T334;struct Cyc_Yystacktype*_T335;union Cyc_YYSTYPE*_T336;union Cyc_YYSTYPE*_T337;struct Cyc_Yystacktype*_T338;union Cyc_YYSTYPE*_T339;union Cyc_YYSTYPE*_T33A;struct Cyc_Parse_Declaration_spec _T33B;struct Cyc_Parse_Declaration_spec _T33C;struct Cyc_Parse_Declaration_spec _T33D;struct Cyc_Parse_Declaration_spec _T33E;struct Cyc_Parse_Declaration_spec _T33F;struct Cyc_Yystacktype*_T340;union Cyc_YYSTYPE*_T341;union Cyc_YYSTYPE*_T342;struct Cyc_List_List*_T343;struct Cyc_Parse_Declaration_spec _T344;struct Cyc_List_List*_T345;struct Cyc_Yystacktype*_T346;union Cyc_YYSTYPE*_T347;union Cyc_YYSTYPE*_T348;struct _fat_ptr _T349;struct _fat_ptr _T34A;int _T34B;struct Cyc_Yystacktype*_T34C;struct Cyc_Yystacktype _T34D;struct Cyc_Yyltype _T34E;unsigned _T34F;unsigned _T350;struct _fat_ptr _T351;struct _fat_ptr _T352;struct Cyc_Yystacktype*_T353;struct Cyc_Yystacktype _T354;struct Cyc_Yystacktype*_T355;struct Cyc_Yystacktype _T356;struct Cyc_List_List*_T357;struct Cyc_Yystacktype*_T358;union Cyc_YYSTYPE*_T359;union Cyc_YYSTYPE*_T35A;struct Cyc_List_List*_T35B;struct Cyc_Yystacktype*_T35C;union Cyc_YYSTYPE*_T35D;union Cyc_YYSTYPE*_T35E;struct Cyc_Yystacktype*_T35F;union Cyc_YYSTYPE*_T360;union Cyc_YYSTYPE*_T361;struct Cyc_Yystacktype*_T362;struct Cyc_Yystacktype _T363;struct Cyc_Yyltype _T364;unsigned _T365;unsigned _T366;struct Cyc_Yystacktype*_T367;union Cyc_YYSTYPE*_T368;union Cyc_YYSTYPE*_T369;struct _fat_ptr _T36A;void*_T36B;struct Cyc_Absyn_Const_att_Absyn_Attribute_struct*_T36C;struct Cyc_Absyn_Const_att_Absyn_Attribute_struct*_T36D;void*_T36E;struct Cyc_Yystacktype*_T36F;struct Cyc_Yystacktype _T370;struct Cyc_Yyltype _T371;unsigned _T372;unsigned _T373;struct Cyc_Yystacktype*_T374;union Cyc_YYSTYPE*_T375;union Cyc_YYSTYPE*_T376;struct _fat_ptr _T377;struct Cyc_Yystacktype*_T378;struct Cyc_Yystacktype _T379;struct Cyc_Yyltype _T37A;unsigned _T37B;unsigned _T37C;struct Cyc_Yystacktype*_T37D;union Cyc_YYSTYPE*_T37E;union Cyc_YYSTYPE*_T37F;struct Cyc_Absyn_Exp*_T380;void*_T381;struct Cyc_Yystacktype*_T382;struct Cyc_Yystacktype _T383;struct Cyc_Yyltype _T384;unsigned _T385;unsigned _T386;struct Cyc_Yystacktype*_T387;struct Cyc_Yystacktype _T388;struct Cyc_Yyltype _T389;unsigned _T38A;unsigned _T38B;struct Cyc_Yystacktype*_T38C;union Cyc_YYSTYPE*_T38D;union Cyc_YYSTYPE*_T38E;struct _fat_ptr _T38F;struct Cyc_Yystacktype*_T390;union Cyc_YYSTYPE*_T391;union Cyc_YYSTYPE*_T392;struct _fat_ptr _T393;struct Cyc_Yystacktype*_T394;struct Cyc_Yystacktype _T395;struct Cyc_Yyltype _T396;unsigned _T397;unsigned _T398;struct Cyc_Yystacktype*_T399;union Cyc_YYSTYPE*_T39A;union Cyc_YYSTYPE*_T39B;union Cyc_Absyn_Cnst _T39C;unsigned _T39D;struct Cyc_Yystacktype*_T39E;struct Cyc_Yystacktype _T39F;struct Cyc_Yyltype _T3A0;unsigned _T3A1;unsigned _T3A2;struct Cyc_Yystacktype*_T3A3;union Cyc_YYSTYPE*_T3A4;union Cyc_YYSTYPE*_T3A5;union Cyc_Absyn_Cnst _T3A6;unsigned _T3A7;void*_T3A8;struct Cyc_Yystacktype*_T3A9;struct Cyc_Yystacktype _T3AA;struct Cyc_Yystacktype*_T3AB;union Cyc_YYSTYPE*_T3AC;union Cyc_YYSTYPE*_T3AD;struct _tuple0*_T3AE;struct Cyc_Yystacktype*_T3AF;union Cyc_YYSTYPE*_T3B0;union Cyc_YYSTYPE*_T3B1;struct Cyc_List_List*_T3B2;void*_T3B3;struct Cyc_Yystacktype*_T3B4;struct Cyc_Yystacktype _T3B5;struct Cyc_Yyltype _T3B6;unsigned _T3B7;unsigned _T3B8;struct Cyc_Parse_Type_specifier _T3B9;void*_T3BA;struct Cyc_Yystacktype*_T3BB;struct Cyc_Yystacktype _T3BC;struct Cyc_Yyltype _T3BD;unsigned _T3BE;unsigned _T3BF;struct Cyc_Parse_Type_specifier _T3C0;void*_T3C1;struct Cyc_Yystacktype*_T3C2;struct Cyc_Yystacktype _T3C3;struct Cyc_Yyltype _T3C4;unsigned _T3C5;unsigned _T3C6;struct Cyc_Parse_Type_specifier _T3C7;struct Cyc_Yystacktype*_T3C8;struct Cyc_Yystacktype _T3C9;struct Cyc_Yyltype _T3CA;unsigned _T3CB;unsigned _T3CC;struct Cyc_Parse_Type_specifier _T3CD;void*_T3CE;struct Cyc_Yystacktype*_T3CF;struct Cyc_Yystacktype _T3D0;struct Cyc_Yyltype _T3D1;unsigned _T3D2;unsigned _T3D3;struct Cyc_Parse_Type_specifier _T3D4;struct Cyc_Yystacktype*_T3D5;struct Cyc_Yystacktype _T3D6;struct Cyc_Yyltype _T3D7;unsigned _T3D8;unsigned _T3D9;struct Cyc_Parse_Type_specifier _T3DA;void*_T3DB;struct Cyc_Yystacktype*_T3DC;struct Cyc_Yystacktype _T3DD;struct Cyc_Yyltype _T3DE;unsigned _T3DF;unsigned _T3E0;struct Cyc_Parse_Type_specifier _T3E1;void*_T3E2;struct Cyc_Yystacktype*_T3E3;struct Cyc_Yystacktype _T3E4;struct Cyc_Yyltype _T3E5;unsigned _T3E6;unsigned _T3E7;struct Cyc_Parse_Type_specifier _T3E8;void*_T3E9;struct Cyc_Yystacktype*_T3EA;struct Cyc_Yystacktype _T3EB;struct Cyc_Yyltype _T3EC;unsigned _T3ED;unsigned _T3EE;struct Cyc_Parse_Type_specifier _T3EF;struct Cyc_Yystacktype*_T3F0;struct Cyc_Yystacktype _T3F1;struct Cyc_Yyltype _T3F2;unsigned _T3F3;unsigned _T3F4;struct Cyc_Parse_Type_specifier _T3F5;struct Cyc_Yystacktype*_T3F6;struct Cyc_Yystacktype _T3F7;struct Cyc_Yyltype _T3F8;unsigned _T3F9;unsigned _T3FA;struct Cyc_Parse_Type_specifier _T3FB;struct Cyc_Yystacktype*_T3FC;struct Cyc_Yystacktype _T3FD;struct Cyc_Yyltype _T3FE;unsigned _T3FF;unsigned _T400;struct Cyc_Parse_Type_specifier _T401;struct Cyc_Yystacktype*_T402;struct Cyc_Yystacktype _T403;struct Cyc_Yystacktype*_T404;struct Cyc_Yystacktype _T405;struct Cyc_Yystacktype*_T406;union Cyc_YYSTYPE*_T407;union Cyc_YYSTYPE*_T408;struct Cyc_Absyn_Exp*_T409;void*_T40A;struct Cyc_Yystacktype*_T40B;struct Cyc_Yystacktype _T40C;struct Cyc_Yyltype _T40D;unsigned _T40E;unsigned _T40F;struct Cyc_Parse_Type_specifier _T410;struct _fat_ptr _T411;struct Cyc_Absyn_Kind*_T412;struct Cyc_Absyn_Kind*_T413;void*_T414;struct Cyc_Yystacktype*_T415;struct Cyc_Yystacktype _T416;struct Cyc_Yyltype _T417;unsigned _T418;unsigned _T419;struct Cyc_Parse_Type_specifier _T41A;struct Cyc_Yystacktype*_T41B;struct Cyc_Yystacktype _T41C;struct Cyc_Yystacktype*_T41D;union Cyc_YYSTYPE*_T41E;union Cyc_YYSTYPE*_T41F;void*_T420;struct Cyc_Yystacktype*_T421;struct Cyc_Yystacktype _T422;struct Cyc_Yyltype _T423;unsigned _T424;unsigned _T425;struct Cyc_Parse_Type_specifier _T426;void*_T427;struct Cyc_Yystacktype*_T428;struct Cyc_Yystacktype _T429;struct Cyc_Yyltype _T42A;unsigned _T42B;unsigned _T42C;struct Cyc_Parse_Type_specifier _T42D;struct Cyc_Yystacktype*_T42E;union Cyc_YYSTYPE*_T42F;union Cyc_YYSTYPE*_T430;struct Cyc_Absyn_Kind*_T431;struct Cyc_Core_Opt*_T432;void*_T433;struct Cyc_Yystacktype*_T434;struct Cyc_Yystacktype _T435;struct Cyc_Yyltype _T436;unsigned _T437;unsigned _T438;struct Cyc_Parse_Type_specifier _T439;struct Cyc_List_List*(*_T43A)(struct _tuple19*(*)(unsigned,struct _tuple8*),unsigned,struct Cyc_List_List*);struct Cyc_List_List*(*_T43B)(void*(*)(void*,void*),void*,struct Cyc_List_List*);struct _tuple19*(*_T43C)(unsigned,struct _tuple8*);struct Cyc_Yystacktype*_T43D;struct Cyc_Yystacktype _T43E;struct Cyc_Yyltype _T43F;unsigned _T440;unsigned _T441;struct Cyc_Yystacktype*_T442;union Cyc_YYSTYPE*_T443;union Cyc_YYSTYPE*_T444;struct Cyc_List_List*_T445;struct Cyc_List_List*_T446;struct Cyc_List_List*_T447;void*_T448;struct Cyc_Yystacktype*_T449;struct Cyc_Yystacktype _T44A;struct Cyc_Yyltype _T44B;unsigned _T44C;unsigned _T44D;struct Cyc_Parse_Type_specifier _T44E;struct Cyc_Yystacktype*_T44F;union Cyc_YYSTYPE*_T450;union Cyc_YYSTYPE*_T451;void*_T452;void*_T453;struct Cyc_Yystacktype*_T454;struct Cyc_Yystacktype _T455;struct Cyc_Yyltype _T456;unsigned _T457;unsigned _T458;struct Cyc_Parse_Type_specifier _T459;struct Cyc_Core_Opt*_T45A;struct Cyc_Core_Opt*_T45B;void*_T45C;void*_T45D;struct Cyc_Yystacktype*_T45E;struct Cyc_Yystacktype _T45F;struct Cyc_Yyltype _T460;unsigned _T461;unsigned _T462;struct Cyc_Parse_Type_specifier _T463;struct Cyc_Yystacktype*_T464;union Cyc_YYSTYPE*_T465;union Cyc_YYSTYPE*_T466;void*_T467;void*_T468;struct Cyc_Yystacktype*_T469;struct Cyc_Yystacktype _T46A;struct Cyc_Yyltype _T46B;unsigned _T46C;unsigned _T46D;struct Cyc_Parse_Type_specifier _T46E;struct Cyc_Yystacktype*_T46F;union Cyc_YYSTYPE*_T470;union Cyc_YYSTYPE*_T471;void*_T472;void*_T473;struct Cyc_Yystacktype*_T474;struct Cyc_Yystacktype _T475;struct Cyc_Yyltype _T476;unsigned _T477;unsigned _T478;struct Cyc_Parse_Type_specifier _T479;struct Cyc_Core_Opt*_T47A;struct Cyc_Core_Opt*_T47B;void*_T47C;void*_T47D;struct Cyc_Yystacktype*_T47E;struct Cyc_Yystacktype _T47F;struct Cyc_Yyltype _T480;unsigned _T481;unsigned _T482;struct Cyc_Parse_Type_specifier _T483;struct Cyc_Yystacktype*_T484;union Cyc_YYSTYPE*_T485;union Cyc_YYSTYPE*_T486;struct Cyc_Absyn_Exp*_T487;void*_T488;struct Cyc_Yystacktype*_T489;struct Cyc_Yystacktype _T48A;struct Cyc_Yyltype _T48B;unsigned _T48C;unsigned _T48D;struct Cyc_Parse_Type_specifier _T48E;struct Cyc_Yystacktype*_T48F;union Cyc_YYSTYPE*_T490;union Cyc_YYSTYPE*_T491;struct Cyc_Absyn_Tqual _T492;unsigned _T493;struct Cyc_Yystacktype*_T494;struct Cyc_Yystacktype _T495;struct Cyc_Yyltype _T496;unsigned _T497;struct Cyc_Yystacktype*_T498;union Cyc_YYSTYPE*_T499;union Cyc_YYSTYPE*_T49A;struct Cyc_Parse_Type_specifier _T49B;struct Cyc_Yystacktype*_T49C;struct Cyc_Yystacktype _T49D;struct Cyc_Yyltype _T49E;unsigned _T49F;unsigned _T4A0;struct Cyc_Warn_String_Warn_Warg_struct _T4A1;struct Cyc_Yystacktype*_T4A2;struct Cyc_Yystacktype _T4A3;struct Cyc_Yyltype _T4A4;unsigned _T4A5;unsigned _T4A6;struct _fat_ptr _T4A7;int _T4A8;struct Cyc_Warn_String_Warn_Warg_struct _T4A9;struct Cyc_Yystacktype*_T4AA;struct Cyc_Yystacktype _T4AB;struct Cyc_Yyltype _T4AC;unsigned _T4AD;unsigned _T4AE;struct _fat_ptr _T4AF;struct Cyc_Warn_String_Warn_Warg_struct _T4B0;struct Cyc_Yystacktype*_T4B1;struct Cyc_Yystacktype _T4B2;struct Cyc_Yyltype _T4B3;unsigned _T4B4;unsigned _T4B5;struct _fat_ptr _T4B6;struct Cyc_Absyn_SubsetType_Absyn_Type_struct*_T4B7;struct Cyc_Yystacktype*_T4B8;union Cyc_YYSTYPE*_T4B9;union Cyc_YYSTYPE*_T4BA;void*_T4BB;struct Cyc_Yystacktype*_T4BC;struct Cyc_Yystacktype _T4BD;struct Cyc_Yyltype _T4BE;unsigned _T4BF;unsigned _T4C0;struct Cyc_Parse_Type_specifier _T4C1;struct Cyc_Yystacktype*_T4C2;union Cyc_YYSTYPE*_T4C3;union Cyc_YYSTYPE*_T4C4;struct _fat_ptr _T4C5;struct Cyc_Yystacktype*_T4C6;struct Cyc_Yystacktype _T4C7;struct Cyc_Yyltype _T4C8;unsigned _T4C9;unsigned _T4CA;struct Cyc_Absyn_Kind*_T4CB;unsigned _T4CC;int _T4CD;struct Cyc_Yystacktype*_T4CE;struct Cyc_Yystacktype _T4CF;struct Cyc_Yyltype _T4D0;unsigned _T4D1;struct Cyc_Absyn_Tqual _T4D2;struct Cyc_Absyn_Tqual _T4D3;struct Cyc_Absyn_Tqual _T4D4;struct Cyc_Absyn_TypeDecl*_T4D5;struct Cyc_Absyn_Enum_td_Absyn_Raw_typedecl_struct*_T4D6;struct Cyc_Absyn_Enumdecl*_T4D7;struct Cyc_Yystacktype*_T4D8;union Cyc_YYSTYPE*_T4D9;union Cyc_YYSTYPE*_T4DA;struct Cyc_Core_Opt*_T4DB;struct Cyc_Yystacktype*_T4DC;union Cyc_YYSTYPE*_T4DD;union Cyc_YYSTYPE*_T4DE;struct Cyc_Absyn_TypeDecl*_T4DF;struct Cyc_Yystacktype*_T4E0;struct Cyc_Yystacktype _T4E1;struct Cyc_Yyltype _T4E2;unsigned _T4E3;struct Cyc_Absyn_TypeDeclType_Absyn_Type_struct*_T4E4;void*_T4E5;struct Cyc_Yystacktype*_T4E6;struct Cyc_Yystacktype _T4E7;struct Cyc_Yyltype _T4E8;unsigned _T4E9;unsigned _T4EA;struct Cyc_Parse_Type_specifier _T4EB;struct Cyc_Yystacktype*_T4EC;union Cyc_YYSTYPE*_T4ED;union Cyc_YYSTYPE*_T4EE;struct _tuple0*_T4EF;void*_T4F0;struct Cyc_Yystacktype*_T4F1;struct Cyc_Yystacktype _T4F2;struct Cyc_Yyltype _T4F3;unsigned _T4F4;unsigned _T4F5;struct Cyc_Parse_Type_specifier _T4F6;struct Cyc_Yystacktype*_T4F7;union Cyc_YYSTYPE*_T4F8;union Cyc_YYSTYPE*_T4F9;struct Cyc_List_List*_T4FA;void*_T4FB;struct Cyc_Yystacktype*_T4FC;struct Cyc_Yystacktype _T4FD;struct Cyc_Yyltype _T4FE;unsigned _T4FF;unsigned _T500;struct Cyc_Parse_Type_specifier _T501;struct Cyc_Absyn_Enumfield*_T502;struct Cyc_Yystacktype*_T503;union Cyc_YYSTYPE*_T504;union Cyc_YYSTYPE*_T505;struct Cyc_Yystacktype*_T506;struct Cyc_Yystacktype _T507;struct Cyc_Yyltype _T508;unsigned _T509;struct Cyc_Absyn_Enumfield*_T50A;struct Cyc_Yystacktype*_T50B;union Cyc_YYSTYPE*_T50C;union Cyc_YYSTYPE*_T50D;struct Cyc_Yystacktype*_T50E;union Cyc_YYSTYPE*_T50F;union Cyc_YYSTYPE*_T510;struct Cyc_Yystacktype*_T511;struct Cyc_Yystacktype _T512;struct Cyc_Yyltype _T513;unsigned _T514;struct Cyc_List_List*_T515;struct Cyc_Yystacktype*_T516;union Cyc_YYSTYPE*_T517;union Cyc_YYSTYPE*_T518;struct Cyc_List_List*_T519;struct Cyc_Yystacktype*_T51A;union Cyc_YYSTYPE*_T51B;union Cyc_YYSTYPE*_T51C;struct Cyc_List_List*_T51D;struct Cyc_Yystacktype*_T51E;union Cyc_YYSTYPE*_T51F;union Cyc_YYSTYPE*_T520;struct Cyc_Yystacktype*_T521;union Cyc_YYSTYPE*_T522;union Cyc_YYSTYPE*_T523;struct Cyc_Absyn_AnonAggrType_Absyn_Type_struct*_T524;struct Cyc_Yystacktype*_T525;union Cyc_YYSTYPE*_T526;union Cyc_YYSTYPE*_T527;struct Cyc_Yystacktype*_T528;union Cyc_YYSTYPE*_T529;union Cyc_YYSTYPE*_T52A;void*_T52B;struct Cyc_Yystacktype*_T52C;struct Cyc_Yystacktype _T52D;struct Cyc_Yyltype _T52E;unsigned _T52F;unsigned _T530;struct Cyc_Parse_Type_specifier _T531;struct Cyc_List_List*(*_T532)(struct Cyc_Absyn_Tvar*(*)(unsigned,void*),unsigned,struct Cyc_List_List*);struct Cyc_List_List*(*_T533)(void*(*)(void*,void*),void*,struct Cyc_List_List*);struct Cyc_Yystacktype*_T534;struct Cyc_Yystacktype _T535;struct Cyc_Yyltype _T536;unsigned _T537;unsigned _T538;struct Cyc_Yystacktype*_T539;union Cyc_YYSTYPE*_T53A;union Cyc_YYSTYPE*_T53B;struct Cyc_List_List*_T53C;struct Cyc_List_List*(*_T53D)(struct Cyc_Absyn_Tvar*(*)(unsigned,void*),unsigned,struct Cyc_List_List*);struct Cyc_List_List*(*_T53E)(void*(*)(void*,void*),void*,struct Cyc_List_List*);struct Cyc_Yystacktype*_T53F;struct Cyc_Yystacktype _T540;struct Cyc_Yyltype _T541;unsigned _T542;unsigned _T543;struct Cyc_Yystacktype*_T544;union Cyc_YYSTYPE*_T545;union Cyc_YYSTYPE*_T546;struct Cyc_List_List*_T547;struct Cyc_Yystacktype*_T548;union Cyc_YYSTYPE*_T549;union Cyc_YYSTYPE*_T54A;struct _tuple28 _T54B;struct _tuple28*_T54C;unsigned _T54D;struct _tuple28*_T54E;struct _tuple28 _T54F;struct Cyc_Yystacktype*_T550;union Cyc_YYSTYPE*_T551;union Cyc_YYSTYPE*_T552;struct _tuple25 _T553;enum Cyc_Absyn_AggrKind _T554;struct Cyc_Yystacktype*_T555;union Cyc_YYSTYPE*_T556;union Cyc_YYSTYPE*_T557;struct _tuple0*_T558;struct Cyc_List_List*_T559;struct Cyc_List_List*_T55A;struct Cyc_List_List*_T55B;struct Cyc_List_List*_T55C;struct Cyc_Yystacktype*_T55D;union Cyc_YYSTYPE*_T55E;union Cyc_YYSTYPE*_T55F;struct Cyc_List_List*_T560;struct Cyc_Yystacktype*_T561;union Cyc_YYSTYPE*_T562;union Cyc_YYSTYPE*_T563;struct _tuple25 _T564;int _T565;struct Cyc_Absyn_AggrdeclImpl*_T566;struct Cyc_Yystacktype*_T567;struct Cyc_Yystacktype _T568;struct Cyc_Yyltype _T569;unsigned _T56A;unsigned _T56B;struct Cyc_Absyn_TypeDeclType_Absyn_Type_struct*_T56C;void*_T56D;struct Cyc_Yystacktype*_T56E;struct Cyc_Yystacktype _T56F;struct Cyc_Yyltype _T570;unsigned _T571;unsigned _T572;struct Cyc_Parse_Type_specifier _T573;struct Cyc_Yystacktype*_T574;union Cyc_YYSTYPE*_T575;union Cyc_YYSTYPE*_T576;struct _tuple25 _T577;enum Cyc_Absyn_AggrKind _T578;struct Cyc_Yystacktype*_T579;union Cyc_YYSTYPE*_T57A;union Cyc_YYSTYPE*_T57B;struct _tuple0*_T57C;struct Cyc_Core_Opt*_T57D;struct Cyc_Yystacktype*_T57E;union Cyc_YYSTYPE*_T57F;union Cyc_YYSTYPE*_T580;struct _tuple25 _T581;int _T582;struct Cyc_Core_Opt*_T583;union Cyc_Absyn_AggrInfo _T584;struct Cyc_Yystacktype*_T585;union Cyc_YYSTYPE*_T586;union Cyc_YYSTYPE*_T587;struct Cyc_List_List*_T588;void*_T589;struct Cyc_Yystacktype*_T58A;struct Cyc_Yystacktype _T58B;struct Cyc_Yyltype _T58C;unsigned _T58D;unsigned _T58E;struct Cyc_Parse_Type_specifier _T58F;struct _tuple25 _T590;struct Cyc_Yystacktype*_T591;union Cyc_YYSTYPE*_T592;union Cyc_YYSTYPE*_T593;struct _tuple25 _T594;struct Cyc_Yystacktype*_T595;union Cyc_YYSTYPE*_T596;union Cyc_YYSTYPE*_T597;struct Cyc_Yystacktype*_T598;union Cyc_YYSTYPE*_T599;union Cyc_YYSTYPE*_T59A;struct Cyc_List_List*_T59B;struct Cyc_List_List*_T59C;struct Cyc_Yystacktype*_T59D;union Cyc_YYSTYPE*_T59E;union Cyc_YYSTYPE*_T59F;struct Cyc_List_List*_T5A0;void*_T5A1;struct Cyc_List_List*_T5A2;struct Cyc_List_List*_T5A3;struct Cyc_List_List*_T5A4;void(*_T5A5)(void(*)(struct Cyc_List_List*,struct Cyc_Absyn_Aggrfield*),struct Cyc_List_List*,struct Cyc_List_List*);void(*_T5A6)(void(*)(void*,void*),void*,struct Cyc_List_List*);struct Cyc_List_List*_T5A7;struct Cyc_List_List*_T5A8;struct Cyc_List_List*_T5A9;struct Cyc_Yystacktype*_T5AA;union Cyc_YYSTYPE*_T5AB;union Cyc_YYSTYPE*_T5AC;struct Cyc_List_List*_T5AD;struct Cyc_Yystacktype*_T5AE;union Cyc_YYSTYPE*_T5AF;union Cyc_YYSTYPE*_T5B0;struct Cyc_Yystacktype*_T5B1;union Cyc_YYSTYPE*_T5B2;union Cyc_YYSTYPE*_T5B3;struct _tuple11*_T5B4;struct _RegionHandle*_T5B5;struct Cyc_Yystacktype*_T5B6;union Cyc_YYSTYPE*_T5B7;union Cyc_YYSTYPE*_T5B8;struct _tuple11*_T5B9;struct _RegionHandle*_T5BA;struct Cyc_Yystacktype*_T5BB;union Cyc_YYSTYPE*_T5BC;union Cyc_YYSTYPE*_T5BD;struct Cyc_Yystacktype*_T5BE;union Cyc_YYSTYPE*_T5BF;union Cyc_YYSTYPE*_T5C0;struct _tuple12 _T5C1;struct Cyc_Yystacktype*_T5C2;union Cyc_YYSTYPE*_T5C3;union Cyc_YYSTYPE*_T5C4;struct _tuple12 _T5C5;struct Cyc_Yystacktype*_T5C6;union Cyc_YYSTYPE*_T5C7;union Cyc_YYSTYPE*_T5C8;struct Cyc_Yystacktype*_T5C9;union Cyc_YYSTYPE*_T5CA;union Cyc_YYSTYPE*_T5CB;void*_T5CC;struct Cyc_Yystacktype*_T5CD;struct Cyc_Yystacktype _T5CE;struct Cyc_Yyltype _T5CF;unsigned _T5D0;unsigned _T5D1;struct _tuple12 _T5D2;struct Cyc_Yystacktype*_T5D3;union Cyc_YYSTYPE*_T5D4;union Cyc_YYSTYPE*_T5D5;struct Cyc_Yystacktype*_T5D6;union Cyc_YYSTYPE*_T5D7;union Cyc_YYSTYPE*_T5D8;struct _tuple12 _T5D9;struct Cyc_Yystacktype*_T5DA;union Cyc_YYSTYPE*_T5DB;union Cyc_YYSTYPE*_T5DC;struct Cyc_Yystacktype*_T5DD;union Cyc_YYSTYPE*_T5DE;union Cyc_YYSTYPE*_T5DF;struct Cyc_Yystacktype*_T5E0;union Cyc_YYSTYPE*_T5E1;union Cyc_YYSTYPE*_T5E2;void*_T5E3;struct Cyc_Yystacktype*_T5E4;struct Cyc_Yystacktype _T5E5;struct Cyc_Yyltype _T5E6;unsigned _T5E7;unsigned _T5E8;struct Cyc_Yystacktype*_T5E9;union Cyc_YYSTYPE*_T5EA;union Cyc_YYSTYPE*_T5EB;struct Cyc_Absyn_Tqual _T5EC;unsigned _T5ED;struct Cyc_Yystacktype*_T5EE;struct Cyc_Yystacktype _T5EF;struct Cyc_Yyltype _T5F0;unsigned _T5F1;struct Cyc_Yystacktype*_T5F2;union Cyc_YYSTYPE*_T5F3;union Cyc_YYSTYPE*_T5F4;struct Cyc_List_List*_T5F5;void*_T5F6;struct _tuple13*_T5F7;struct _RegionHandle*_T5F8;struct Cyc_List_List*_T5F9;struct _RegionHandle*_T5FA;struct _tuple16*_T5FB;struct _RegionHandle*_T5FC;struct Cyc_List_List*_T5FD;struct Cyc_Parse_Type_specifier _T5FE;struct Cyc_Yystacktype*_T5FF;struct Cyc_Yystacktype _T600;struct Cyc_Yyltype _T601;unsigned _T602;unsigned _T603;struct _RegionHandle*_T604;struct _RegionHandle*_T605;struct Cyc_List_List*_T606;struct Cyc_List_List*_T607;struct Cyc_List_List*(*_T608)(struct Cyc_Absyn_Aggrfield*(*)(unsigned,struct _tuple17*),unsigned,struct Cyc_List_List*);struct Cyc_List_List*(*_T609)(void*(*)(void*,void*),void*,struct Cyc_List_List*);struct Cyc_Yystacktype*_T60A;struct Cyc_Yystacktype _T60B;struct Cyc_Yyltype _T60C;unsigned _T60D;unsigned _T60E;struct Cyc_List_List*_T60F;struct Cyc_List_List*_T610;struct _tuple26 _T611;struct Cyc_Yystacktype*_T612;struct Cyc_Yystacktype _T613;struct Cyc_Yyltype _T614;unsigned _T615;unsigned _T616;struct Cyc_Yystacktype*_T617;union Cyc_YYSTYPE*_T618;union Cyc_YYSTYPE*_T619;struct Cyc_Yystacktype*_T61A;union Cyc_YYSTYPE*_T61B;union Cyc_YYSTYPE*_T61C;struct _tuple26 _T61D;struct _tuple26 _T61E;struct Cyc_Yystacktype*_T61F;struct Cyc_Yystacktype _T620;struct Cyc_Yyltype _T621;unsigned _T622;unsigned _T623;struct Cyc_Yystacktype*_T624;union Cyc_YYSTYPE*_T625;union Cyc_YYSTYPE*_T626;struct Cyc_Parse_Type_specifier _T627;struct _tuple26 _T628;struct Cyc_Parse_Type_specifier _T629;struct _tuple26 _T62A;struct _tuple26 _T62B;struct Cyc_Yystacktype*_T62C;union Cyc_YYSTYPE*_T62D;union Cyc_YYSTYPE*_T62E;struct Cyc_Yystacktype*_T62F;union Cyc_YYSTYPE*_T630;union Cyc_YYSTYPE*_T631;struct _tuple26 _T632;struct Cyc_Yystacktype*_T633;union Cyc_YYSTYPE*_T634;union Cyc_YYSTYPE*_T635;struct Cyc_Absyn_Tqual _T636;struct _tuple26 _T637;struct Cyc_Absyn_Tqual _T638;struct _tuple26 _T639;struct _tuple26 _T63A;struct _tuple26 _T63B;struct Cyc_Yystacktype*_T63C;struct Cyc_Yystacktype _T63D;struct Cyc_Yyltype _T63E;unsigned _T63F;unsigned _T640;struct Cyc_Yystacktype*_T641;union Cyc_YYSTYPE*_T642;union Cyc_YYSTYPE*_T643;struct Cyc_Yystacktype*_T644;union Cyc_YYSTYPE*_T645;union Cyc_YYSTYPE*_T646;struct _tuple26 _T647;struct _tuple26 _T648;struct _tuple26 _T649;struct Cyc_Yystacktype*_T64A;union Cyc_YYSTYPE*_T64B;union Cyc_YYSTYPE*_T64C;struct Cyc_List_List*_T64D;struct _tuple26 _T64E;struct Cyc_List_List*_T64F;struct _tuple26 _T650;struct Cyc_Yystacktype*_T651;struct Cyc_Yystacktype _T652;struct Cyc_Yyltype _T653;unsigned _T654;unsigned _T655;struct Cyc_Yystacktype*_T656;union Cyc_YYSTYPE*_T657;union Cyc_YYSTYPE*_T658;struct Cyc_Yystacktype*_T659;union Cyc_YYSTYPE*_T65A;union Cyc_YYSTYPE*_T65B;struct _tuple26 _T65C;struct _tuple26 _T65D;struct Cyc_Yystacktype*_T65E;struct Cyc_Yystacktype _T65F;struct Cyc_Yyltype _T660;unsigned _T661;unsigned _T662;struct Cyc_Yystacktype*_T663;union Cyc_YYSTYPE*_T664;union Cyc_YYSTYPE*_T665;struct Cyc_Parse_Type_specifier _T666;struct _tuple26 _T667;struct Cyc_Parse_Type_specifier _T668;struct _tuple26 _T669;struct _tuple26 _T66A;struct Cyc_Yystacktype*_T66B;union Cyc_YYSTYPE*_T66C;union Cyc_YYSTYPE*_T66D;struct Cyc_Yystacktype*_T66E;union Cyc_YYSTYPE*_T66F;union Cyc_YYSTYPE*_T670;struct _tuple26 _T671;struct Cyc_Yystacktype*_T672;union Cyc_YYSTYPE*_T673;union Cyc_YYSTYPE*_T674;struct Cyc_Absyn_Tqual _T675;struct _tuple26 _T676;struct Cyc_Absyn_Tqual _T677;struct _tuple26 _T678;struct _tuple26 _T679;struct _tuple26 _T67A;struct Cyc_Yystacktype*_T67B;struct Cyc_Yystacktype _T67C;struct Cyc_Yyltype _T67D;unsigned _T67E;unsigned _T67F;struct Cyc_Yystacktype*_T680;union Cyc_YYSTYPE*_T681;union Cyc_YYSTYPE*_T682;struct Cyc_Yystacktype*_T683;union Cyc_YYSTYPE*_T684;union Cyc_YYSTYPE*_T685;struct _tuple26 _T686;struct _tuple26 _T687;struct _tuple26 _T688;struct Cyc_Yystacktype*_T689;union Cyc_YYSTYPE*_T68A;union Cyc_YYSTYPE*_T68B;struct Cyc_List_List*_T68C;struct _tuple26 _T68D;struct Cyc_List_List*_T68E;struct Cyc_List_List*_T68F;struct _RegionHandle*_T690;struct Cyc_Yystacktype*_T691;union Cyc_YYSTYPE*_T692;union Cyc_YYSTYPE*_T693;struct Cyc_List_List*_T694;struct _RegionHandle*_T695;struct Cyc_Yystacktype*_T696;union Cyc_YYSTYPE*_T697;union Cyc_YYSTYPE*_T698;struct Cyc_Yystacktype*_T699;union Cyc_YYSTYPE*_T69A;union Cyc_YYSTYPE*_T69B;struct _tuple12*_T69C;struct _RegionHandle*_T69D;struct Cyc_Yystacktype*_T69E;union Cyc_YYSTYPE*_T69F;union Cyc_YYSTYPE*_T6A0;struct Cyc_Yystacktype*_T6A1;union Cyc_YYSTYPE*_T6A2;union Cyc_YYSTYPE*_T6A3;struct _tuple12*_T6A4;struct _RegionHandle*_T6A5;struct _tuple0*_T6A6;struct _fat_ptr*_T6A7;struct Cyc_Yystacktype*_T6A8;union Cyc_YYSTYPE*_T6A9;union Cyc_YYSTYPE*_T6AA;struct _tuple12*_T6AB;struct _RegionHandle*_T6AC;struct _tuple0*_T6AD;struct _fat_ptr*_T6AE;struct _tuple12*_T6AF;struct _RegionHandle*_T6B0;struct Cyc_Yystacktype*_T6B1;union Cyc_YYSTYPE*_T6B2;union Cyc_YYSTYPE*_T6B3;struct Cyc_Yystacktype*_T6B4;union Cyc_YYSTYPE*_T6B5;union Cyc_YYSTYPE*_T6B6;struct Cyc_Yystacktype*_T6B7;union Cyc_YYSTYPE*_T6B8;union Cyc_YYSTYPE*_T6B9;struct Cyc_Absyn_Exp*_T6BA;struct Cyc_Yystacktype*_T6BB;union Cyc_YYSTYPE*_T6BC;union Cyc_YYSTYPE*_T6BD;struct Cyc_List_List*(*_T6BE)(struct Cyc_Absyn_Tvar*(*)(unsigned,void*),unsigned,struct Cyc_List_List*);struct Cyc_List_List*(*_T6BF)(void*(*)(void*,void*),void*,struct Cyc_List_List*);struct Cyc_Yystacktype*_T6C0;struct Cyc_Yystacktype _T6C1;struct Cyc_Yyltype _T6C2;unsigned _T6C3;unsigned _T6C4;struct Cyc_Yystacktype*_T6C5;union Cyc_YYSTYPE*_T6C6;union Cyc_YYSTYPE*_T6C7;struct Cyc_List_List*_T6C8;struct Cyc_Yystacktype*_T6C9;union Cyc_YYSTYPE*_T6CA;union Cyc_YYSTYPE*_T6CB;struct _tuple0*_T6CC;struct Cyc_List_List*_T6CD;struct Cyc_Core_Opt*_T6CE;struct Cyc_Yystacktype*_T6CF;union Cyc_YYSTYPE*_T6D0;union Cyc_YYSTYPE*_T6D1;int _T6D2;struct Cyc_Yystacktype*_T6D3;struct Cyc_Yystacktype _T6D4;struct Cyc_Yyltype _T6D5;unsigned _T6D6;unsigned _T6D7;struct Cyc_Absyn_TypeDeclType_Absyn_Type_struct*_T6D8;void*_T6D9;struct Cyc_Yystacktype*_T6DA;struct Cyc_Yystacktype _T6DB;struct Cyc_Yyltype _T6DC;unsigned _T6DD;unsigned _T6DE;struct Cyc_Parse_Type_specifier _T6DF;struct Cyc_Yystacktype*_T6E0;union Cyc_YYSTYPE*_T6E1;union Cyc_YYSTYPE*_T6E2;struct Cyc_Absyn_UnknownDatatypeInfo _T6E3;struct Cyc_Yystacktype*_T6E4;union Cyc_YYSTYPE*_T6E5;union Cyc_YYSTYPE*_T6E6;union Cyc_Absyn_DatatypeInfo _T6E7;struct Cyc_Yystacktype*_T6E8;union Cyc_YYSTYPE*_T6E9;union Cyc_YYSTYPE*_T6EA;struct Cyc_List_List*_T6EB;void*_T6EC;struct Cyc_Yystacktype*_T6ED;struct Cyc_Yystacktype _T6EE;struct Cyc_Yyltype _T6EF;unsigned _T6F0;unsigned _T6F1;struct Cyc_Parse_Type_specifier _T6F2;struct Cyc_Yystacktype*_T6F3;union Cyc_YYSTYPE*_T6F4;union Cyc_YYSTYPE*_T6F5;struct Cyc_Absyn_UnknownDatatypeFieldInfo _T6F6;struct Cyc_Yystacktype*_T6F7;union Cyc_YYSTYPE*_T6F8;union Cyc_YYSTYPE*_T6F9;struct Cyc_Yystacktype*_T6FA;union Cyc_YYSTYPE*_T6FB;union Cyc_YYSTYPE*_T6FC;union Cyc_Absyn_DatatypeFieldInfo _T6FD;struct Cyc_Yystacktype*_T6FE;union Cyc_YYSTYPE*_T6FF;union Cyc_YYSTYPE*_T700;struct Cyc_List_List*_T701;void*_T702;struct Cyc_Yystacktype*_T703;struct Cyc_Yystacktype _T704;struct Cyc_Yyltype _T705;unsigned _T706;unsigned _T707;struct Cyc_Parse_Type_specifier _T708;struct Cyc_List_List*_T709;struct Cyc_Yystacktype*_T70A;union Cyc_YYSTYPE*_T70B;union Cyc_YYSTYPE*_T70C;struct Cyc_List_List*_T70D;struct Cyc_Yystacktype*_T70E;union Cyc_YYSTYPE*_T70F;union Cyc_YYSTYPE*_T710;struct Cyc_List_List*_T711;struct Cyc_Yystacktype*_T712;union Cyc_YYSTYPE*_T713;union Cyc_YYSTYPE*_T714;struct Cyc_Yystacktype*_T715;union Cyc_YYSTYPE*_T716;union Cyc_YYSTYPE*_T717;struct Cyc_List_List*_T718;struct Cyc_Yystacktype*_T719;union Cyc_YYSTYPE*_T71A;union Cyc_YYSTYPE*_T71B;struct Cyc_Yystacktype*_T71C;union Cyc_YYSTYPE*_T71D;union Cyc_YYSTYPE*_T71E;struct Cyc_Absyn_Datatypefield*_T71F;struct Cyc_Yystacktype*_T720;union Cyc_YYSTYPE*_T721;union Cyc_YYSTYPE*_T722;struct Cyc_Yystacktype*_T723;struct Cyc_Yystacktype _T724;struct Cyc_Yyltype _T725;unsigned _T726;struct Cyc_Yystacktype*_T727;union Cyc_YYSTYPE*_T728;union Cyc_YYSTYPE*_T729;struct Cyc_List_List*(*_T72A)(struct _tuple19*(*)(unsigned,struct _tuple8*),unsigned,struct Cyc_List_List*);struct Cyc_List_List*(*_T72B)(void*(*)(void*,void*),void*,struct Cyc_List_List*);struct _tuple19*(*_T72C)(unsigned,struct _tuple8*);struct Cyc_Yystacktype*_T72D;struct Cyc_Yystacktype _T72E;struct Cyc_Yyltype _T72F;unsigned _T730;unsigned _T731;struct Cyc_Yystacktype*_T732;union Cyc_YYSTYPE*_T733;union Cyc_YYSTYPE*_T734;struct Cyc_List_List*_T735;struct Cyc_List_List*_T736;struct Cyc_Absyn_Datatypefield*_T737;struct Cyc_Yystacktype*_T738;union Cyc_YYSTYPE*_T739;union Cyc_YYSTYPE*_T73A;struct Cyc_Yystacktype*_T73B;struct Cyc_Yystacktype _T73C;struct Cyc_Yyltype _T73D;unsigned _T73E;struct Cyc_Yystacktype*_T73F;union Cyc_YYSTYPE*_T740;union Cyc_YYSTYPE*_T741;struct Cyc_Yystacktype*_T742;struct Cyc_Yystacktype _T743;struct Cyc_Yystacktype*_T744;union Cyc_YYSTYPE*_T745;union Cyc_YYSTYPE*_T746;struct Cyc_Parse_Declarator _T747;struct Cyc_Parse_Declarator _T748;struct Cyc_Parse_Declarator _T749;struct Cyc_Yystacktype*_T74A;union Cyc_YYSTYPE*_T74B;union Cyc_YYSTYPE*_T74C;struct Cyc_List_List*_T74D;struct Cyc_Parse_Declarator _T74E;struct Cyc_List_List*_T74F;struct Cyc_Yystacktype*_T750;struct Cyc_Yystacktype _T751;struct Cyc_Yystacktype*_T752;union Cyc_YYSTYPE*_T753;union Cyc_YYSTYPE*_T754;struct Cyc_Parse_Declarator _T755;struct Cyc_Parse_Declarator _T756;struct Cyc_Parse_Declarator _T757;struct Cyc_Yystacktype*_T758;union Cyc_YYSTYPE*_T759;union Cyc_YYSTYPE*_T75A;struct Cyc_List_List*_T75B;struct Cyc_Parse_Declarator _T75C;struct Cyc_List_List*_T75D;struct Cyc_Parse_Declarator _T75E;struct Cyc_Yystacktype*_T75F;union Cyc_YYSTYPE*_T760;union Cyc_YYSTYPE*_T761;struct Cyc_Yystacktype*_T762;struct Cyc_Yystacktype _T763;struct Cyc_Yyltype _T764;unsigned _T765;struct Cyc_Yystacktype*_T766;struct Cyc_Yystacktype _T767;struct Cyc_Yystacktype*_T768;union Cyc_YYSTYPE*_T769;union Cyc_YYSTYPE*_T76A;struct Cyc_List_List*_T76B;struct _RegionHandle*_T76C;struct Cyc_Absyn_Attributes_mod_Absyn_Type_modifier_struct*_T76D;struct _RegionHandle*_T76E;struct Cyc_Yystacktype*_T76F;struct Cyc_Yystacktype _T770;struct Cyc_Yyltype _T771;unsigned _T772;struct Cyc_Yystacktype*_T773;union Cyc_YYSTYPE*_T774;union Cyc_YYSTYPE*_T775;struct Cyc_Parse_Declarator _T776;struct Cyc_Yystacktype*_T777;struct Cyc_Yystacktype _T778;struct Cyc_Parse_Declarator _T779;struct Cyc_Yystacktype*_T77A;union Cyc_YYSTYPE*_T77B;union Cyc_YYSTYPE*_T77C;struct Cyc_Parse_Declarator _T77D;struct Cyc_Yystacktype*_T77E;union Cyc_YYSTYPE*_T77F;union Cyc_YYSTYPE*_T780;struct Cyc_Parse_Declarator _T781;struct Cyc_List_List*_T782;struct _RegionHandle*_T783;struct Cyc_Absyn_Carray_mod_Absyn_Type_modifier_struct*_T784;struct _RegionHandle*_T785;struct Cyc_Yystacktype*_T786;union Cyc_YYSTYPE*_T787;union Cyc_YYSTYPE*_T788;struct Cyc_Yystacktype*_T789;struct Cyc_Yystacktype _T78A;struct Cyc_Yyltype _T78B;unsigned _T78C;struct Cyc_Yystacktype*_T78D;union Cyc_YYSTYPE*_T78E;union Cyc_YYSTYPE*_T78F;struct Cyc_Parse_Declarator _T790;struct Cyc_Parse_Declarator _T791;struct Cyc_Yystacktype*_T792;union Cyc_YYSTYPE*_T793;union Cyc_YYSTYPE*_T794;struct Cyc_Parse_Declarator _T795;struct Cyc_Yystacktype*_T796;union Cyc_YYSTYPE*_T797;union Cyc_YYSTYPE*_T798;struct Cyc_Parse_Declarator _T799;struct Cyc_List_List*_T79A;struct _RegionHandle*_T79B;struct Cyc_Absyn_ConstArray_mod_Absyn_Type_modifier_struct*_T79C;struct _RegionHandle*_T79D;struct Cyc_Yystacktype*_T79E;union Cyc_YYSTYPE*_T79F;union Cyc_YYSTYPE*_T7A0;struct Cyc_Yystacktype*_T7A1;union Cyc_YYSTYPE*_T7A2;union Cyc_YYSTYPE*_T7A3;struct Cyc_Yystacktype*_T7A4;struct Cyc_Yystacktype _T7A5;struct Cyc_Yyltype _T7A6;unsigned _T7A7;struct Cyc_Yystacktype*_T7A8;union Cyc_YYSTYPE*_T7A9;union Cyc_YYSTYPE*_T7AA;struct Cyc_Parse_Declarator _T7AB;struct Cyc_Yystacktype*_T7AC;union Cyc_YYSTYPE*_T7AD;union Cyc_YYSTYPE*_T7AE;struct Cyc_Yystacktype*_T7AF;union Cyc_YYSTYPE*_T7B0;union Cyc_YYSTYPE*_T7B1;struct Cyc_Parse_Declarator _T7B2;struct Cyc_Yystacktype*_T7B3;union Cyc_YYSTYPE*_T7B4;union Cyc_YYSTYPE*_T7B5;struct Cyc_Parse_Declarator _T7B6;struct Cyc_Yystacktype*_T7B7;union Cyc_YYSTYPE*_T7B8;union Cyc_YYSTYPE*_T7B9;struct Cyc_Parse_Declarator _T7BA;struct Cyc_List_List*_T7BB;struct _RegionHandle*_T7BC;struct Cyc_Absyn_Function_mod_Absyn_Type_modifier_struct*_T7BD;struct _RegionHandle*_T7BE;struct Cyc_Absyn_WithTypes_Absyn_Funcparams_struct*_T7BF;struct _RegionHandle*_T7C0;struct Cyc_Yystacktype*_T7C1;union Cyc_YYSTYPE*_T7C2;union Cyc_YYSTYPE*_T7C3;struct Cyc_Parse_Declarator _T7C4;struct Cyc_Parse_Declarator _T7C5;struct Cyc_Yystacktype*_T7C6;union Cyc_YYSTYPE*_T7C7;union Cyc_YYSTYPE*_T7C8;struct Cyc_Parse_Declarator _T7C9;struct Cyc_Yystacktype*_T7CA;union Cyc_YYSTYPE*_T7CB;union Cyc_YYSTYPE*_T7CC;struct Cyc_Parse_Declarator _T7CD;struct Cyc_List_List*_T7CE;struct _RegionHandle*_T7CF;struct Cyc_Absyn_Function_mod_Absyn_Type_modifier_struct*_T7D0;struct _RegionHandle*_T7D1;struct Cyc_Absyn_NoTypes_Absyn_Funcparams_struct*_T7D2;struct _RegionHandle*_T7D3;struct Cyc_Yystacktype*_T7D4;union Cyc_YYSTYPE*_T7D5;union Cyc_YYSTYPE*_T7D6;struct Cyc_Yystacktype*_T7D7;struct Cyc_Yystacktype _T7D8;struct Cyc_Yyltype _T7D9;unsigned _T7DA;struct Cyc_Yystacktype*_T7DB;union Cyc_YYSTYPE*_T7DC;union Cyc_YYSTYPE*_T7DD;struct Cyc_Parse_Declarator _T7DE;struct Cyc_List_List*(*_T7DF)(struct Cyc_Absyn_Tvar*(*)(unsigned,void*),unsigned,struct Cyc_List_List*);struct Cyc_List_List*(*_T7E0)(void*(*)(void*,void*),void*,struct Cyc_List_List*);struct Cyc_Yystacktype*_T7E1;struct Cyc_Yystacktype _T7E2;struct Cyc_Yyltype _T7E3;unsigned _T7E4;unsigned _T7E5;struct Cyc_Yystacktype*_T7E6;union Cyc_YYSTYPE*_T7E7;union Cyc_YYSTYPE*_T7E8;struct Cyc_List_List*_T7E9;struct Cyc_List_List*_T7EA;struct Cyc_Parse_Declarator _T7EB;struct Cyc_Yystacktype*_T7EC;union Cyc_YYSTYPE*_T7ED;union Cyc_YYSTYPE*_T7EE;struct Cyc_Parse_Declarator _T7EF;struct Cyc_Yystacktype*_T7F0;union Cyc_YYSTYPE*_T7F1;union Cyc_YYSTYPE*_T7F2;struct Cyc_Parse_Declarator _T7F3;struct Cyc_List_List*_T7F4;struct _RegionHandle*_T7F5;struct Cyc_Absyn_TypeParams_mod_Absyn_Type_modifier_struct*_T7F6;struct _RegionHandle*_T7F7;struct Cyc_Yystacktype*_T7F8;struct Cyc_Yystacktype _T7F9;struct Cyc_Yyltype _T7FA;unsigned _T7FB;struct Cyc_Yystacktype*_T7FC;union Cyc_YYSTYPE*_T7FD;union Cyc_YYSTYPE*_T7FE;struct Cyc_Parse_Declarator _T7FF;struct Cyc_Parse_Declarator _T800;struct Cyc_Yystacktype*_T801;union Cyc_YYSTYPE*_T802;union Cyc_YYSTYPE*_T803;struct Cyc_Parse_Declarator _T804;struct Cyc_Yystacktype*_T805;union Cyc_YYSTYPE*_T806;union Cyc_YYSTYPE*_T807;struct Cyc_Parse_Declarator _T808;struct Cyc_List_List*_T809;struct _RegionHandle*_T80A;struct Cyc_Absyn_Attributes_mod_Absyn_Type_modifier_struct*_T80B;struct _RegionHandle*_T80C;struct Cyc_Yystacktype*_T80D;struct Cyc_Yystacktype _T80E;struct Cyc_Yyltype _T80F;unsigned _T810;struct Cyc_Yystacktype*_T811;union Cyc_YYSTYPE*_T812;union Cyc_YYSTYPE*_T813;struct Cyc_Yystacktype*_T814;union Cyc_YYSTYPE*_T815;union Cyc_YYSTYPE*_T816;struct Cyc_Parse_Declarator _T817;struct Cyc_Parse_Declarator _T818;struct Cyc_Yystacktype*_T819;union Cyc_YYSTYPE*_T81A;union Cyc_YYSTYPE*_T81B;struct Cyc_Yystacktype*_T81C;struct Cyc_Yystacktype _T81D;struct Cyc_Yyltype _T81E;unsigned _T81F;struct Cyc_Parse_Declarator _T820;struct Cyc_Yystacktype*_T821;union Cyc_YYSTYPE*_T822;union Cyc_YYSTYPE*_T823;struct Cyc_Yystacktype*_T824;struct Cyc_Yystacktype _T825;struct Cyc_Yyltype _T826;unsigned _T827;struct Cyc_Yystacktype*_T828;struct Cyc_Yystacktype _T829;struct Cyc_Yystacktype*_T82A;union Cyc_YYSTYPE*_T82B;union Cyc_YYSTYPE*_T82C;struct Cyc_List_List*_T82D;struct _RegionHandle*_T82E;struct Cyc_Absyn_Attributes_mod_Absyn_Type_modifier_struct*_T82F;struct _RegionHandle*_T830;struct Cyc_Yystacktype*_T831;struct Cyc_Yystacktype _T832;struct Cyc_Yyltype _T833;unsigned _T834;struct Cyc_Yystacktype*_T835;union Cyc_YYSTYPE*_T836;union Cyc_YYSTYPE*_T837;struct Cyc_Parse_Declarator _T838;struct Cyc_Yystacktype*_T839;struct Cyc_Yystacktype _T83A;struct Cyc_Yystacktype*_T83B;union Cyc_YYSTYPE*_T83C;union Cyc_YYSTYPE*_T83D;struct Cyc_Parse_Declarator _T83E;struct Cyc_Parse_Declarator _T83F;struct Cyc_Parse_Declarator _T840;struct Cyc_List_List*_T841;struct _RegionHandle*_T842;struct Cyc_Absyn_Carray_mod_Absyn_Type_modifier_struct*_T843;struct _RegionHandle*_T844;struct Cyc_Yystacktype*_T845;union Cyc_YYSTYPE*_T846;union Cyc_YYSTYPE*_T847;struct Cyc_Yystacktype*_T848;struct Cyc_Yystacktype _T849;struct Cyc_Yyltype _T84A;unsigned _T84B;struct Cyc_Parse_Declarator _T84C;struct Cyc_Yystacktype*_T84D;union Cyc_YYSTYPE*_T84E;union Cyc_YYSTYPE*_T84F;struct Cyc_Parse_Declarator _T850;struct Cyc_Parse_Declarator _T851;struct Cyc_Parse_Declarator _T852;struct Cyc_List_List*_T853;struct _RegionHandle*_T854;struct Cyc_Absyn_ConstArray_mod_Absyn_Type_modifier_struct*_T855;struct _RegionHandle*_T856;struct Cyc_Yystacktype*_T857;union Cyc_YYSTYPE*_T858;union Cyc_YYSTYPE*_T859;struct Cyc_Yystacktype*_T85A;union Cyc_YYSTYPE*_T85B;union Cyc_YYSTYPE*_T85C;struct Cyc_Yystacktype*_T85D;struct Cyc_Yystacktype _T85E;struct Cyc_Yyltype _T85F;unsigned _T860;struct Cyc_Parse_Declarator _T861;struct Cyc_Yystacktype*_T862;union Cyc_YYSTYPE*_T863;union Cyc_YYSTYPE*_T864;struct Cyc_Yystacktype*_T865;union Cyc_YYSTYPE*_T866;union Cyc_YYSTYPE*_T867;struct Cyc_Yystacktype*_T868;union Cyc_YYSTYPE*_T869;union Cyc_YYSTYPE*_T86A;struct Cyc_Parse_Declarator _T86B;struct Cyc_Parse_Declarator _T86C;struct Cyc_Parse_Declarator _T86D;struct Cyc_List_List*_T86E;struct _RegionHandle*_T86F;struct Cyc_Absyn_Function_mod_Absyn_Type_modifier_struct*_T870;struct _RegionHandle*_T871;struct Cyc_Absyn_WithTypes_Absyn_Funcparams_struct*_T872;struct _RegionHandle*_T873;struct Cyc_Parse_Declarator _T874;struct Cyc_Yystacktype*_T875;union Cyc_YYSTYPE*_T876;union Cyc_YYSTYPE*_T877;struct Cyc_Parse_Declarator _T878;struct Cyc_Parse_Declarator _T879;struct Cyc_Parse_Declarator _T87A;struct Cyc_List_List*_T87B;struct _RegionHandle*_T87C;struct Cyc_Absyn_Function_mod_Absyn_Type_modifier_struct*_T87D;struct _RegionHandle*_T87E;struct Cyc_Absyn_NoTypes_Absyn_Funcparams_struct*_T87F;struct _RegionHandle*_T880;struct Cyc_Yystacktype*_T881;union Cyc_YYSTYPE*_T882;union Cyc_YYSTYPE*_T883;struct Cyc_Yystacktype*_T884;struct Cyc_Yystacktype _T885;struct Cyc_Yyltype _T886;unsigned _T887;struct Cyc_Parse_Declarator _T888;struct Cyc_List_List*(*_T889)(struct Cyc_Absyn_Tvar*(*)(unsigned,void*),unsigned,struct Cyc_List_List*);struct Cyc_List_List*(*_T88A)(void*(*)(void*,void*),void*,struct Cyc_List_List*);struct Cyc_Yystacktype*_T88B;struct Cyc_Yystacktype _T88C;struct Cyc_Yyltype _T88D;unsigned _T88E;unsigned _T88F;struct Cyc_Yystacktype*_T890;union Cyc_YYSTYPE*_T891;union Cyc_YYSTYPE*_T892;struct Cyc_List_List*_T893;struct Cyc_List_List*_T894;struct Cyc_Yystacktype*_T895;union Cyc_YYSTYPE*_T896;union Cyc_YYSTYPE*_T897;struct Cyc_Parse_Declarator _T898;struct Cyc_Parse_Declarator _T899;struct Cyc_Parse_Declarator _T89A;struct Cyc_List_List*_T89B;struct _RegionHandle*_T89C;struct Cyc_Absyn_TypeParams_mod_Absyn_Type_modifier_struct*_T89D;struct _RegionHandle*_T89E;struct Cyc_Yystacktype*_T89F;struct Cyc_Yystacktype _T8A0;struct Cyc_Yyltype _T8A1;unsigned _T8A2;struct Cyc_Parse_Declarator _T8A3;struct Cyc_Yystacktype*_T8A4;union Cyc_YYSTYPE*_T8A5;union Cyc_YYSTYPE*_T8A6;struct Cyc_Parse_Declarator _T8A7;struct Cyc_Parse_Declarator _T8A8;struct Cyc_Parse_Declarator _T8A9;struct Cyc_List_List*_T8AA;struct _RegionHandle*_T8AB;struct Cyc_Absyn_Attributes_mod_Absyn_Type_modifier_struct*_T8AC;struct _RegionHandle*_T8AD;struct Cyc_Yystacktype*_T8AE;struct Cyc_Yystacktype _T8AF;struct Cyc_Yyltype _T8B0;unsigned _T8B1;struct Cyc_Yystacktype*_T8B2;union Cyc_YYSTYPE*_T8B3;union Cyc_YYSTYPE*_T8B4;struct Cyc_Parse_Declarator _T8B5;struct Cyc_Yystacktype*_T8B6;struct Cyc_Yystacktype _T8B7;struct Cyc_Yystacktype*_T8B8;union Cyc_YYSTYPE*_T8B9;union Cyc_YYSTYPE*_T8BA;struct Cyc_List_List*_T8BB;struct Cyc_Yystacktype*_T8BC;union Cyc_YYSTYPE*_T8BD;union Cyc_YYSTYPE*_T8BE;struct Cyc_List_List*_T8BF;struct Cyc_List_List*_T8C0;struct Cyc_Yystacktype*_T8C1;union Cyc_YYSTYPE*_T8C2;union Cyc_YYSTYPE*_T8C3;struct Cyc_List_List*_T8C4;struct Cyc_List_List*_T8C5;struct _RegionHandle*_T8C6;struct Cyc_Absyn_Attributes_mod_Absyn_Type_modifier_struct*_T8C7;struct _RegionHandle*_T8C8;struct Cyc_Yystacktype*_T8C9;struct Cyc_Yystacktype _T8CA;struct Cyc_Yyltype _T8CB;unsigned _T8CC;struct Cyc_Yystacktype*_T8CD;union Cyc_YYSTYPE*_T8CE;union Cyc_YYSTYPE*_T8CF;struct Cyc_Yystacktype*_T8D0;union Cyc_YYSTYPE*_T8D1;union Cyc_YYSTYPE*_T8D2;struct _tuple22*_T8D3;int _T8D4;struct Cyc_Absyn_PtrLoc*_T8D5;struct Cyc_Yystacktype*_T8D6;struct Cyc_Yystacktype _T8D7;struct Cyc_Yyltype _T8D8;unsigned _T8D9;struct Cyc_Yystacktype*_T8DA;struct Cyc_Yystacktype _T8DB;struct Cyc_Yyltype _T8DC;unsigned _T8DD;struct _RegionHandle*_T8DE;struct Cyc_Absyn_PtrLoc*_T8DF;void*_T8E0;void*_T8E1;struct Cyc_Yystacktype*_T8E2;union Cyc_YYSTYPE*_T8E3;union Cyc_YYSTYPE*_T8E4;void*_T8E5;struct Cyc_Yystacktype*_T8E6;union Cyc_YYSTYPE*_T8E7;union Cyc_YYSTYPE*_T8E8;struct Cyc_List_List*_T8E9;struct Cyc_Yystacktype*_T8EA;union Cyc_YYSTYPE*_T8EB;union Cyc_YYSTYPE*_T8EC;struct Cyc_Absyn_Tqual _T8ED;struct Cyc_List_List*_T8EE;struct _RegionHandle*_T8EF;struct Cyc_List_List*_T8F0;struct _RegionHandle*_T8F1;struct Cyc_Yystacktype*_T8F2;union Cyc_YYSTYPE*_T8F3;union Cyc_YYSTYPE*_T8F4;struct Cyc_Yystacktype*_T8F5;union Cyc_YYSTYPE*_T8F6;union Cyc_YYSTYPE*_T8F7;struct Cyc_Parse_Numelts_ptrqual_Parse_Pointer_qual_struct*_T8F8;struct _RegionHandle*_T8F9;struct Cyc_Yystacktype*_T8FA;union Cyc_YYSTYPE*_T8FB;union Cyc_YYSTYPE*_T8FC;void*_T8FD;struct Cyc_Yystacktype*_T8FE;struct Cyc_Yystacktype _T8FF;struct Cyc_Yyltype _T900;unsigned _T901;unsigned _T902;struct _fat_ptr _T903;struct _fat_ptr _T904;struct Cyc_Parse_Effect_ptrqual_Parse_Pointer_qual_struct*_T905;struct _RegionHandle*_T906;struct Cyc_List_List*_T907;struct Cyc_Yystacktype*_T908;union Cyc_YYSTYPE*_T909;union Cyc_YYSTYPE*_T90A;void*_T90B;struct Cyc_Parse_Effect_ptrqual_Parse_Pointer_qual_struct*_T90C;struct _RegionHandle*_T90D;struct Cyc_Yystacktype*_T90E;union Cyc_YYSTYPE*_T90F;union Cyc_YYSTYPE*_T910;void*_T911;struct Cyc_Parse_Thin_ptrqual_Parse_Pointer_qual_struct*_T912;struct _RegionHandle*_T913;void*_T914;struct Cyc_Parse_Fat_ptrqual_Parse_Pointer_qual_struct*_T915;struct _RegionHandle*_T916;void*_T917;struct Cyc_Parse_Autoreleased_ptrqual_Parse_Pointer_qual_struct*_T918;struct _RegionHandle*_T919;void*_T91A;struct Cyc_Parse_Zeroterm_ptrqual_Parse_Pointer_qual_struct*_T91B;struct _RegionHandle*_T91C;void*_T91D;struct Cyc_Parse_Nozeroterm_ptrqual_Parse_Pointer_qual_struct*_T91E;struct _RegionHandle*_T91F;void*_T920;struct Cyc_Parse_Notnull_ptrqual_Parse_Pointer_qual_struct*_T921;struct _RegionHandle*_T922;void*_T923;struct Cyc_Parse_Nullable_ptrqual_Parse_Pointer_qual_struct*_T924;struct _RegionHandle*_T925;void*_T926;struct Cyc_Parse_Alias_ptrqual_Parse_Pointer_qual_struct*_T927;struct _RegionHandle*_T928;struct Cyc_Yystacktype*_T929;union Cyc_YYSTYPE*_T92A;union Cyc_YYSTYPE*_T92B;void*_T92C;struct Cyc_Yystacktype*_T92D;struct Cyc_Yystacktype _T92E;struct Cyc_Yyltype _T92F;unsigned _T930;unsigned _T931;struct Cyc_Yystacktype*_T932;union Cyc_YYSTYPE*_T933;union Cyc_YYSTYPE*_T934;struct _fat_ptr _T935;struct Cyc_Parse_Alias_ptrqual_Parse_Pointer_qual_struct*_T936;struct _RegionHandle*_T937;void*_T938;struct Cyc_Yystacktype*_T939;union Cyc_YYSTYPE*_T93A;union Cyc_YYSTYPE*_T93B;void*_T93C;struct Cyc_Yystacktype*_T93D;union Cyc_YYSTYPE*_T93E;union Cyc_YYSTYPE*_T93F;void*_T940;void*_T941;void*_T942;struct Cyc_Yystacktype*_T943;union Cyc_YYSTYPE*_T944;union Cyc_YYSTYPE*_T945;void*_T946;void*_T947;void*_T948;void*_T949;struct Cyc_Yystacktype*_T94A;struct Cyc_Yystacktype _T94B;struct Cyc_Yyltype _T94C;unsigned _T94D;unsigned _T94E;struct Cyc_Yystacktype*_T94F;union Cyc_YYSTYPE*_T950;union Cyc_YYSTYPE*_T951;struct _fat_ptr _T952;void*_T953;struct _tuple22*_T954;struct Cyc_Yystacktype*_T955;struct Cyc_Yystacktype _T956;struct Cyc_Yyltype _T957;unsigned _T958;int _T959;struct Cyc_Yystacktype*_T95A;struct Cyc_Yystacktype _T95B;struct Cyc_Yyltype _T95C;unsigned _T95D;unsigned _T95E;struct _fat_ptr _T95F;struct Cyc_Yystacktype*_T960;union Cyc_YYSTYPE*_T961;union Cyc_YYSTYPE*_T962;void*_T963;struct _tuple22*_T964;struct Cyc_Yystacktype*_T965;struct Cyc_Yystacktype _T966;struct Cyc_Yyltype _T967;unsigned _T968;struct Cyc_Yystacktype*_T969;struct Cyc_Yystacktype _T96A;struct Cyc_Yyltype _T96B;unsigned _T96C;unsigned _T96D;struct _fat_ptr _T96E;struct Cyc_Yystacktype*_T96F;union Cyc_YYSTYPE*_T970;union Cyc_YYSTYPE*_T971;void*_T972;struct _tuple22*_T973;int _T974;int _T975;struct _tuple22*_T976;struct Cyc_Yystacktype*_T977;struct Cyc_Yystacktype _T978;struct Cyc_Yyltype _T979;unsigned _T97A;struct Cyc_Yystacktype*_T97B;struct Cyc_Yystacktype _T97C;struct Cyc_Yyltype _T97D;unsigned _T97E;unsigned _T97F;struct _fat_ptr _T980;struct Cyc_Core_Opt*_T981;struct Cyc_Core_Opt*_T982;void*_T983;struct _tuple22*_T984;struct Cyc_Yystacktype*_T985;struct Cyc_Yystacktype _T986;struct Cyc_Yyltype _T987;unsigned _T988;void*_T989;int _T98A;struct Cyc_Core_Opt*_T98B;struct Cyc_Core_Opt*_T98C;struct Cyc_Yystacktype*_T98D;union Cyc_YYSTYPE*_T98E;union Cyc_YYSTYPE*_T98F;struct Cyc_Absyn_Exp*_T990;void*_T991;int _T992;int(*_T993)(unsigned,struct _fat_ptr);struct _fat_ptr _T994;struct Cyc_Yystacktype*_T995;union Cyc_YYSTYPE*_T996;union Cyc_YYSTYPE*_T997;struct _fat_ptr _T998;void*_T999;void*_T99A;struct Cyc_List_List*_T99B;struct Cyc_Yystacktype*_T99C;union Cyc_YYSTYPE*_T99D;union Cyc_YYSTYPE*_T99E;struct Cyc_List_List*_T99F;struct Cyc_Yystacktype*_T9A0;union Cyc_YYSTYPE*_T9A1;union Cyc_YYSTYPE*_T9A2;struct Cyc_Yystacktype*_T9A3;union Cyc_YYSTYPE*_T9A4;union Cyc_YYSTYPE*_T9A5;struct Cyc_Core_Opt*_T9A6;struct Cyc_Core_Opt*_T9A7;void*_T9A8;struct Cyc_Yystacktype*_T9A9;union Cyc_YYSTYPE*_T9AA;union Cyc_YYSTYPE*_T9AB;int _T9AC;struct Cyc_List_List*_T9AD;void*_T9AE;struct Cyc_Yystacktype*_T9AF;union Cyc_YYSTYPE*_T9B0;union Cyc_YYSTYPE*_T9B1;struct Cyc_List_List*_T9B2;void*_T9B3;struct Cyc_Core_Opt*_T9B4;struct Cyc_Core_Opt*_T9B5;void*_T9B6;struct _fat_ptr _T9B7;int _T9B8;unsigned char*_T9B9;struct Cyc_Yystacktype*_T9BA;struct Cyc_Yystacktype _T9BB;struct Cyc_Yyltype _T9BC;unsigned _T9BD;unsigned _T9BE;struct Cyc_Absyn_Tqual _T9BF;struct Cyc_Yystacktype*_T9C0;union Cyc_YYSTYPE*_T9C1;union Cyc_YYSTYPE*_T9C2;struct Cyc_Absyn_Tqual _T9C3;struct Cyc_Yystacktype*_T9C4;union Cyc_YYSTYPE*_T9C5;union Cyc_YYSTYPE*_T9C6;struct Cyc_Absyn_Tqual _T9C7;struct Cyc_Absyn_Tqual _T9C8;struct Cyc_Yystacktype*_T9C9;union Cyc_YYSTYPE*_T9CA;union Cyc_YYSTYPE*_T9CB;struct _tuple28 _T9CC;struct _tuple28*_T9CD;unsigned _T9CE;struct _tuple28*_T9CF;struct _tuple28 _T9D0;struct _tuple27*_T9D1;struct Cyc_Yystacktype*_T9D2;union Cyc_YYSTYPE*_T9D3;union Cyc_YYSTYPE*_T9D4;struct Cyc_Yystacktype*_T9D5;union Cyc_YYSTYPE*_T9D6;union Cyc_YYSTYPE*_T9D7;struct _tuple28 _T9D8;struct _tuple28*_T9D9;unsigned _T9DA;struct _tuple28*_T9DB;struct _tuple28 _T9DC;struct _tuple27*_T9DD;struct Cyc_Yystacktype*_T9DE;union Cyc_YYSTYPE*_T9DF;union Cyc_YYSTYPE*_T9E0;struct Cyc_List_List*_T9E1;struct Cyc_Yystacktype*_T9E2;union Cyc_YYSTYPE*_T9E3;union Cyc_YYSTYPE*_T9E4;struct Cyc_Yystacktype*_T9E5;union Cyc_YYSTYPE*_T9E6;union Cyc_YYSTYPE*_T9E7;struct _tuple28 _T9E8;struct _tuple28*_T9E9;unsigned _T9EA;struct _tuple28*_T9EB;struct _tuple28 _T9EC;struct _tuple27*_T9ED;struct Cyc_Yystacktype*_T9EE;union Cyc_YYSTYPE*_T9EF;union Cyc_YYSTYPE*_T9F0;struct Cyc_List_List*_T9F1;struct Cyc_Yystacktype*_T9F2;union Cyc_YYSTYPE*_T9F3;union Cyc_YYSTYPE*_T9F4;struct Cyc_Yystacktype*_T9F5;union Cyc_YYSTYPE*_T9F6;union Cyc_YYSTYPE*_T9F7;struct Cyc_Absyn_VarargInfo*_T9F8;struct Cyc_Absyn_VarargInfo*_T9F9;struct Cyc_Absyn_VarargInfo*_T9FA;struct Cyc_Absyn_VarargInfo*_T9FB;struct Cyc_Yystacktype*_T9FC;union Cyc_YYSTYPE*_T9FD;union Cyc_YYSTYPE*_T9FE;struct Cyc_Yystacktype*_T9FF;union Cyc_YYSTYPE*_TA00;union Cyc_YYSTYPE*_TA01;struct _tuple28 _TA02;struct _tuple28*_TA03;unsigned _TA04;struct _tuple28*_TA05;struct _tuple28 _TA06;struct _tuple27*_TA07;struct Cyc_Yystacktype*_TA08;union Cyc_YYSTYPE*_TA09;union Cyc_YYSTYPE*_TA0A;struct Cyc_Yystacktype*_TA0B;union Cyc_YYSTYPE*_TA0C;union Cyc_YYSTYPE*_TA0D;struct Cyc_Absyn_VarargInfo*_TA0E;struct Cyc_Absyn_VarargInfo*_TA0F;struct Cyc_Absyn_VarargInfo*_TA10;struct Cyc_Absyn_VarargInfo*_TA11;struct Cyc_Yystacktype*_TA12;union Cyc_YYSTYPE*_TA13;union Cyc_YYSTYPE*_TA14;struct Cyc_Yystacktype*_TA15;union Cyc_YYSTYPE*_TA16;union Cyc_YYSTYPE*_TA17;struct _tuple28 _TA18;struct _tuple28*_TA19;unsigned _TA1A;struct _tuple28*_TA1B;struct _tuple28 _TA1C;struct _tuple27*_TA1D;struct Cyc_Yystacktype*_TA1E;union Cyc_YYSTYPE*_TA1F;union Cyc_YYSTYPE*_TA20;struct Cyc_List_List*_TA21;struct Cyc_Yystacktype*_TA22;union Cyc_YYSTYPE*_TA23;union Cyc_YYSTYPE*_TA24;struct Cyc_Yystacktype*_TA25;struct Cyc_Yystacktype _TA26;struct Cyc_Yyltype _TA27;unsigned _TA28;unsigned _TA29;struct Cyc_Yystacktype*_TA2A;union Cyc_YYSTYPE*_TA2B;union Cyc_YYSTYPE*_TA2C;struct _fat_ptr _TA2D;void*_TA2E;struct Cyc_Yystacktype*_TA2F;union Cyc_YYSTYPE*_TA30;union Cyc_YYSTYPE*_TA31;struct _fat_ptr _TA32;struct Cyc_Absyn_Unknown_kb_Absyn_KindBound_struct*_TA33;void*_TA34;struct Cyc_Yystacktype*_TA35;union Cyc_YYSTYPE*_TA36;union Cyc_YYSTYPE*_TA37;void*_TA38;struct Cyc_Yystacktype*_TA39;struct Cyc_Yystacktype _TA3A;struct Cyc_Yyltype _TA3B;unsigned _TA3C;unsigned _TA3D;void*_TA3E;struct Cyc_Yystacktype*_TA3F;union Cyc_YYSTYPE*_TA40;union Cyc_YYSTYPE*_TA41;struct _fat_ptr _TA42;struct Cyc_Yystacktype*_TA43;union Cyc_YYSTYPE*_TA44;union Cyc_YYSTYPE*_TA45;struct Cyc_Absyn_Kind*_TA46;void*_TA47;struct Cyc_Yystacktype*_TA48;union Cyc_YYSTYPE*_TA49;union Cyc_YYSTYPE*_TA4A;void*_TA4B;struct Cyc_Yystacktype*_TA4C;struct Cyc_Yystacktype _TA4D;struct Cyc_Yyltype _TA4E;unsigned _TA4F;unsigned _TA50;void*_TA51;struct Cyc_Yystacktype*_TA52;union Cyc_YYSTYPE*_TA53;union Cyc_YYSTYPE*_TA54;struct Cyc_List_List*_TA55;void*_TA56;struct Cyc_Yystacktype*_TA57;struct Cyc_Yystacktype _TA58;struct _tuple28*_TA59;struct Cyc_List_List*_TA5A;struct Cyc_Yystacktype*_TA5B;union Cyc_YYSTYPE*_TA5C;union Cyc_YYSTYPE*_TA5D;struct _fat_ptr _TA5E;struct _tuple28*_TA5F;struct Cyc_List_List*_TA60;struct Cyc_Yystacktype*_TA61;union Cyc_YYSTYPE*_TA62;union Cyc_YYSTYPE*_TA63;struct _fat_ptr _TA64;struct Cyc_Yystacktype*_TA65;union Cyc_YYSTYPE*_TA66;union Cyc_YYSTYPE*_TA67;struct _tuple28*_TA68;struct _tuple28*_TA69;struct Cyc_List_List*_TA6A;struct Cyc_Yystacktype*_TA6B;union Cyc_YYSTYPE*_TA6C;union Cyc_YYSTYPE*_TA6D;struct Cyc_Yystacktype*_TA6E;union Cyc_YYSTYPE*_TA6F;union Cyc_YYSTYPE*_TA70;struct _tuple28*_TA71;struct _tuple28*_TA72;struct Cyc_List_List*_TA73;struct Cyc_Yystacktype*_TA74;union Cyc_YYSTYPE*_TA75;union Cyc_YYSTYPE*_TA76;struct Cyc_Absyn_Less_kb_Absyn_KindBound_struct*_TA77;struct Cyc_Absyn_Less_kb_Absyn_KindBound_struct*_TA78;struct Cyc_Absyn_Less_kb_Absyn_KindBound_struct*_TA79;struct Cyc_Absyn_Kind*_TA7A;struct Cyc_Yystacktype*_TA7B;union Cyc_YYSTYPE*_TA7C;union Cyc_YYSTYPE*_TA7D;struct _fat_ptr _TA7E;struct Cyc_Absyn_Less_kb_Absyn_KindBound_struct*_TA7F;void*_TA80;struct Cyc_Yystacktype*_TA81;struct Cyc_Yystacktype _TA82;struct Cyc_Yyltype _TA83;unsigned _TA84;unsigned _TA85;struct Cyc_Absyn_SubsetConstraint_Absyn_EffConstraint_struct*_TA86;struct Cyc_Yystacktype*_TA87;union Cyc_YYSTYPE*_TA88;union Cyc_YYSTYPE*_TA89;struct Cyc_List_List*_TA8A;void*_TA8B;struct Cyc_Absyn_DisjointConstraint_Absyn_EffConstraint_struct*_TA8C;struct Cyc_Yystacktype*_TA8D;union Cyc_YYSTYPE*_TA8E;union Cyc_YYSTYPE*_TA8F;struct Cyc_List_List*_TA90;struct Cyc_Yystacktype*_TA91;union Cyc_YYSTYPE*_TA92;union Cyc_YYSTYPE*_TA93;struct Cyc_List_List*_TA94;void*_TA95;struct Cyc_Yystacktype*_TA96;struct Cyc_Yystacktype _TA97;struct Cyc_Yyltype _TA98;unsigned _TA99;unsigned _TA9A;struct Cyc_Yystacktype*_TA9B;union Cyc_YYSTYPE*_TA9C;union Cyc_YYSTYPE*_TA9D;struct _fat_ptr _TA9E;struct Cyc_Absyn_SingleConstraint_Absyn_EffConstraint_struct*_TA9F;struct Cyc_Yystacktype*_TAA0;union Cyc_YYSTYPE*_TAA1;union Cyc_YYSTYPE*_TAA2;struct Cyc_List_List*_TAA3;void*_TAA4;struct _tuple29*_TAA5;struct Cyc_Yystacktype*_TAA6;union Cyc_YYSTYPE*_TAA7;union Cyc_YYSTYPE*_TAA8;struct Cyc_Yystacktype*_TAA9;union Cyc_YYSTYPE*_TAAA;union Cyc_YYSTYPE*_TAAB;struct Cyc_Yystacktype*_TAAC;struct Cyc_Yystacktype _TAAD;struct Cyc_Yyltype _TAAE;unsigned _TAAF;unsigned _TAB0;struct Cyc_Yystacktype*_TAB1;union Cyc_YYSTYPE*_TAB2;union Cyc_YYSTYPE*_TAB3;struct _fat_ptr _TAB4;void*_TAB5;struct Cyc_Yystacktype*_TAB6;union Cyc_YYSTYPE*_TAB7;union Cyc_YYSTYPE*_TAB8;void*_TAB9;struct Cyc_Yystacktype*_TABA;union Cyc_YYSTYPE*_TABB;union Cyc_YYSTYPE*_TABC;void*_TABD;void*_TABE;void*_TABF;void*_TAC0;struct Cyc_Absyn_Eq_kb_Absyn_KindBound_struct*_TAC1;struct Cyc_Absyn_Eq_kb_Absyn_KindBound_struct*_TAC2;struct Cyc_Absyn_Kind*_TAC3;struct Cyc_Yystacktype*_TAC4;union Cyc_YYSTYPE*_TAC5;union Cyc_YYSTYPE*_TAC6;struct _fat_ptr _TAC7;struct Cyc_Absyn_Eq_kb_Absyn_KindBound_struct*_TAC8;void*_TAC9;struct Cyc_Yystacktype*_TACA;struct Cyc_Yystacktype _TACB;struct Cyc_Yyltype _TACC;unsigned _TACD;unsigned _TACE;void*_TACF;struct Cyc_Yystacktype*_TAD0;union Cyc_YYSTYPE*_TAD1;union Cyc_YYSTYPE*_TAD2;void*_TAD3;void*_TAD4;struct Cyc_Yystacktype*_TAD5;union Cyc_YYSTYPE*_TAD6;union Cyc_YYSTYPE*_TAD7;struct _fat_ptr _TAD8;struct _fat_ptr _TAD9;int _TADA;struct Cyc_Warn_String_Warn_Warg_struct _TADB;struct Cyc_Yystacktype*_TADC;struct Cyc_Yystacktype _TADD;struct Cyc_Yyltype _TADE;unsigned _TADF;unsigned _TAE0;struct _fat_ptr _TAE1;struct Cyc_Yystacktype*_TAE2;struct Cyc_Yystacktype _TAE3;struct Cyc_Yystacktype*_TAE4;union Cyc_YYSTYPE*_TAE5;union Cyc_YYSTYPE*_TAE6;struct Cyc_List_List*_TAE7;struct Cyc_Yystacktype*_TAE8;union Cyc_YYSTYPE*_TAE9;union Cyc_YYSTYPE*_TAEA;struct Cyc_List_List*_TAEB;struct Cyc_List_List*_TAEC;struct Cyc_Yystacktype*_TAED;struct Cyc_Yystacktype _TAEE;struct Cyc_List_List*_TAEF;struct Cyc_Yystacktype*_TAF0;union Cyc_YYSTYPE*_TAF1;union Cyc_YYSTYPE*_TAF2;void*_TAF3;struct Cyc_Yystacktype*_TAF4;union Cyc_YYSTYPE*_TAF5;union Cyc_YYSTYPE*_TAF6;void*_TAF7;struct Cyc_Absyn_Kind*_TAF8;struct Cyc_Absyn_Kind*_TAF9;struct Cyc_List_List*_TAFA;struct Cyc_Yystacktype*_TAFB;union Cyc_YYSTYPE*_TAFC;union Cyc_YYSTYPE*_TAFD;struct Cyc_List_List*_TAFE;struct Cyc_Yystacktype*_TAFF;union Cyc_YYSTYPE*_TB00;union Cyc_YYSTYPE*_TB01;struct _tuple8*_TB02;struct Cyc_Yystacktype*_TB03;struct Cyc_Yystacktype _TB04;struct Cyc_Yyltype _TB05;unsigned _TB06;unsigned _TB07;struct Cyc_List_List*_TB08;struct Cyc_Yystacktype*_TB09;union Cyc_YYSTYPE*_TB0A;union Cyc_YYSTYPE*_TB0B;struct _tuple8*_TB0C;struct Cyc_Yystacktype*_TB0D;struct Cyc_Yystacktype _TB0E;struct Cyc_Yyltype _TB0F;unsigned _TB10;unsigned _TB11;struct Cyc_Yystacktype*_TB12;union Cyc_YYSTYPE*_TB13;union Cyc_YYSTYPE*_TB14;struct Cyc_List_List*_TB15;struct Cyc_Yystacktype*_TB16;union Cyc_YYSTYPE*_TB17;union Cyc_YYSTYPE*_TB18;struct Cyc_List_List*_TB19;struct Cyc_Yystacktype*_TB1A;union Cyc_YYSTYPE*_TB1B;union Cyc_YYSTYPE*_TB1C;struct Cyc_Yystacktype*_TB1D;union Cyc_YYSTYPE*_TB1E;union Cyc_YYSTYPE*_TB1F;struct Cyc_Yystacktype*_TB20;union Cyc_YYSTYPE*_TB21;union Cyc_YYSTYPE*_TB22;struct Cyc_Absyn_Tqual _TB23;unsigned _TB24;struct Cyc_Yystacktype*_TB25;struct Cyc_Yystacktype _TB26;struct Cyc_Yyltype _TB27;unsigned _TB28;struct Cyc_Yystacktype*_TB29;union Cyc_YYSTYPE*_TB2A;union Cyc_YYSTYPE*_TB2B;struct Cyc_Parse_Type_specifier _TB2C;struct Cyc_Yystacktype*_TB2D;struct Cyc_Yystacktype _TB2E;struct Cyc_Yyltype _TB2F;unsigned _TB30;unsigned _TB31;struct Cyc_Warn_String_Warn_Warg_struct _TB32;struct Cyc_Yystacktype*_TB33;struct Cyc_Yystacktype _TB34;struct Cyc_Yyltype _TB35;unsigned _TB36;unsigned _TB37;struct _fat_ptr _TB38;int _TB39;struct Cyc_Warn_String_Warn_Warg_struct _TB3A;struct Cyc_Yystacktype*_TB3B;struct Cyc_Yystacktype _TB3C;struct Cyc_Yyltype _TB3D;unsigned _TB3E;unsigned _TB3F;struct _fat_ptr _TB40;struct _tuple0*_TB41;struct _tuple0 _TB42;struct Cyc_Warn_String_Warn_Warg_struct _TB43;struct Cyc_Yystacktype*_TB44;struct Cyc_Yystacktype _TB45;struct Cyc_Yyltype _TB46;unsigned _TB47;unsigned _TB48;struct _fat_ptr _TB49;struct _tuple8*_TB4A;struct Cyc_Yystacktype*_TB4B;union Cyc_YYSTYPE*_TB4C;union Cyc_YYSTYPE*_TB4D;struct Cyc_Absyn_Tqual _TB4E;unsigned _TB4F;struct Cyc_Yystacktype*_TB50;struct Cyc_Yystacktype _TB51;struct Cyc_Yyltype _TB52;unsigned _TB53;struct Cyc_Parse_Type_specifier _TB54;struct Cyc_Yystacktype*_TB55;struct Cyc_Yystacktype _TB56;struct Cyc_Yyltype _TB57;unsigned _TB58;unsigned _TB59;struct Cyc_Warn_String_Warn_Warg_struct _TB5A;struct Cyc_Yystacktype*_TB5B;struct Cyc_Yystacktype _TB5C;struct Cyc_Yyltype _TB5D;unsigned _TB5E;unsigned _TB5F;struct _fat_ptr _TB60;struct _tuple8*_TB61;struct Cyc_Yystacktype*_TB62;union Cyc_YYSTYPE*_TB63;union Cyc_YYSTYPE*_TB64;struct Cyc_Absyn_Tqual _TB65;unsigned _TB66;struct Cyc_Yystacktype*_TB67;struct Cyc_Yystacktype _TB68;struct Cyc_Yyltype _TB69;unsigned _TB6A;struct Cyc_Parse_Type_specifier _TB6B;struct Cyc_Yystacktype*_TB6C;struct Cyc_Yystacktype _TB6D;struct Cyc_Yyltype _TB6E;unsigned _TB6F;unsigned _TB70;struct Cyc_Yystacktype*_TB71;union Cyc_YYSTYPE*_TB72;union Cyc_YYSTYPE*_TB73;struct Cyc_Parse_Abstractdeclarator _TB74;struct Cyc_Warn_String_Warn_Warg_struct _TB75;struct Cyc_Yystacktype*_TB76;struct Cyc_Yystacktype _TB77;struct Cyc_Yyltype _TB78;unsigned _TB79;unsigned _TB7A;struct _fat_ptr _TB7B;struct Cyc_Warn_String_Warn_Warg_struct _TB7C;struct Cyc_Yystacktype*_TB7D;struct Cyc_Yystacktype _TB7E;struct Cyc_Yyltype _TB7F;unsigned _TB80;unsigned _TB81;struct _fat_ptr _TB82;struct _tuple8*_TB83;struct Cyc_Yystacktype*_TB84;union Cyc_YYSTYPE*_TB85;union Cyc_YYSTYPE*_TB86;struct Cyc_List_List*_TB87;struct Cyc_List_List*_TB88;struct Cyc_List_List*_TB89;struct _fat_ptr*_TB8A;struct Cyc_Yystacktype*_TB8B;union Cyc_YYSTYPE*_TB8C;union Cyc_YYSTYPE*_TB8D;struct Cyc_List_List*_TB8E;struct _fat_ptr*_TB8F;struct Cyc_Yystacktype*_TB90;union Cyc_YYSTYPE*_TB91;union Cyc_YYSTYPE*_TB92;struct Cyc_Yystacktype*_TB93;union Cyc_YYSTYPE*_TB94;union Cyc_YYSTYPE*_TB95;struct Cyc_Yystacktype*_TB96;struct Cyc_Yystacktype _TB97;struct Cyc_Yystacktype*_TB98;struct Cyc_Yystacktype _TB99;struct Cyc_Absyn_UnresolvedMem_e_Absyn_Raw_exp_struct*_TB9A;void*_TB9B;struct Cyc_Yystacktype*_TB9C;struct Cyc_Yystacktype _TB9D;struct Cyc_Yyltype _TB9E;unsigned _TB9F;unsigned _TBA0;struct Cyc_Absyn_Exp*_TBA1;struct Cyc_Absyn_UnresolvedMem_e_Absyn_Raw_exp_struct*_TBA2;struct Cyc_Yystacktype*_TBA3;union Cyc_YYSTYPE*_TBA4;union Cyc_YYSTYPE*_TBA5;struct Cyc_List_List*_TBA6;void*_TBA7;struct Cyc_Yystacktype*_TBA8;struct Cyc_Yystacktype _TBA9;struct Cyc_Yyltype _TBAA;unsigned _TBAB;unsigned _TBAC;struct Cyc_Absyn_Exp*_TBAD;struct Cyc_Absyn_UnresolvedMem_e_Absyn_Raw_exp_struct*_TBAE;struct Cyc_Yystacktype*_TBAF;union Cyc_YYSTYPE*_TBB0;union Cyc_YYSTYPE*_TBB1;struct Cyc_List_List*_TBB2;void*_TBB3;struct Cyc_Yystacktype*_TBB4;struct Cyc_Yystacktype _TBB5;struct Cyc_Yyltype _TBB6;unsigned _TBB7;unsigned _TBB8;struct Cyc_Absyn_Exp*_TBB9;struct Cyc_Yystacktype*_TBBA;struct Cyc_Yystacktype _TBBB;struct Cyc_Yyltype _TBBC;unsigned _TBBD;unsigned _TBBE;struct _tuple0*_TBBF;struct _fat_ptr*_TBC0;struct Cyc_Yystacktype*_TBC1;union Cyc_YYSTYPE*_TBC2;union Cyc_YYSTYPE*_TBC3;void*_TBC4;struct Cyc_Yystacktype*_TBC5;struct Cyc_Yystacktype _TBC6;struct Cyc_Yyltype _TBC7;unsigned _TBC8;unsigned _TBC9;struct Cyc_Absyn_Exp*_TBCA;struct Cyc_Absyn_Vardecl*_TBCB;struct Cyc_Absyn_Comprehension_e_Absyn_Raw_exp_struct*_TBCC;struct Cyc_Yystacktype*_TBCD;union Cyc_YYSTYPE*_TBCE;union Cyc_YYSTYPE*_TBCF;struct Cyc_Yystacktype*_TBD0;union Cyc_YYSTYPE*_TBD1;union Cyc_YYSTYPE*_TBD2;void*_TBD3;struct Cyc_Yystacktype*_TBD4;struct Cyc_Yystacktype _TBD5;struct Cyc_Yyltype _TBD6;unsigned _TBD7;unsigned _TBD8;struct Cyc_Absyn_Exp*_TBD9;struct Cyc_Yystacktype*_TBDA;union Cyc_YYSTYPE*_TBDB;union Cyc_YYSTYPE*_TBDC;struct _tuple8*_TBDD;struct Cyc_Yystacktype*_TBDE;struct Cyc_Yystacktype _TBDF;struct Cyc_Yyltype _TBE0;unsigned _TBE1;unsigned _TBE2;struct Cyc_Absyn_ComprehensionNoinit_e_Absyn_Raw_exp_struct*_TBE3;struct Cyc_Yystacktype*_TBE4;union Cyc_YYSTYPE*_TBE5;union Cyc_YYSTYPE*_TBE6;void*_TBE7;struct Cyc_Yystacktype*_TBE8;struct Cyc_Yystacktype _TBE9;struct Cyc_Yyltype _TBEA;unsigned _TBEB;unsigned _TBEC;struct Cyc_Absyn_Exp*_TBED;struct Cyc_List_List*_TBEE;struct _tuple34*_TBEF;struct Cyc_Yystacktype*_TBF0;union Cyc_YYSTYPE*_TBF1;union Cyc_YYSTYPE*_TBF2;struct Cyc_List_List*_TBF3;struct _tuple34*_TBF4;struct Cyc_Yystacktype*_TBF5;union Cyc_YYSTYPE*_TBF6;union Cyc_YYSTYPE*_TBF7;struct Cyc_Yystacktype*_TBF8;union Cyc_YYSTYPE*_TBF9;union Cyc_YYSTYPE*_TBFA;struct Cyc_List_List*_TBFB;struct _tuple34*_TBFC;struct Cyc_Yystacktype*_TBFD;union Cyc_YYSTYPE*_TBFE;union Cyc_YYSTYPE*_TBFF;struct Cyc_Yystacktype*_TC00;union Cyc_YYSTYPE*_TC01;union Cyc_YYSTYPE*_TC02;struct Cyc_List_List*_TC03;struct _tuple34*_TC04;struct Cyc_Yystacktype*_TC05;union Cyc_YYSTYPE*_TC06;union Cyc_YYSTYPE*_TC07;struct Cyc_Yystacktype*_TC08;union Cyc_YYSTYPE*_TC09;union Cyc_YYSTYPE*_TC0A;struct Cyc_Yystacktype*_TC0B;union Cyc_YYSTYPE*_TC0C;union Cyc_YYSTYPE*_TC0D;struct Cyc_Yystacktype*_TC0E;union Cyc_YYSTYPE*_TC0F;union Cyc_YYSTYPE*_TC10;struct Cyc_List_List*_TC11;struct Cyc_List_List*_TC12;struct Cyc_List_List*_TC13;struct Cyc_Absyn_FieldName_Absyn_Designator_struct*_TC14;struct _fat_ptr*_TC15;struct Cyc_Yystacktype*_TC16;union Cyc_YYSTYPE*_TC17;union Cyc_YYSTYPE*_TC18;struct Cyc_List_List*_TC19;struct Cyc_Yystacktype*_TC1A;union Cyc_YYSTYPE*_TC1B;union Cyc_YYSTYPE*_TC1C;struct Cyc_List_List*_TC1D;struct Cyc_Yystacktype*_TC1E;union Cyc_YYSTYPE*_TC1F;union Cyc_YYSTYPE*_TC20;struct Cyc_Yystacktype*_TC21;union Cyc_YYSTYPE*_TC22;union Cyc_YYSTYPE*_TC23;struct Cyc_Absyn_ArrayElement_Absyn_Designator_struct*_TC24;struct Cyc_Yystacktype*_TC25;union Cyc_YYSTYPE*_TC26;union Cyc_YYSTYPE*_TC27;void*_TC28;struct Cyc_Absyn_FieldName_Absyn_Designator_struct*_TC29;struct _fat_ptr*_TC2A;struct Cyc_Yystacktype*_TC2B;union Cyc_YYSTYPE*_TC2C;union Cyc_YYSTYPE*_TC2D;void*_TC2E;struct Cyc_Yystacktype*_TC2F;union Cyc_YYSTYPE*_TC30;union Cyc_YYSTYPE*_TC31;struct Cyc_Parse_Type_specifier _TC32;struct Cyc_Yystacktype*_TC33;struct Cyc_Yystacktype _TC34;struct Cyc_Yyltype _TC35;unsigned _TC36;unsigned _TC37;struct Cyc_Yystacktype*_TC38;struct Cyc_Yystacktype _TC39;struct Cyc_Yyltype _TC3A;unsigned _TC3B;unsigned _TC3C;struct _fat_ptr _TC3D;struct _fat_ptr _TC3E;struct _tuple8*_TC3F;struct Cyc_Yystacktype*_TC40;union Cyc_YYSTYPE*_TC41;union Cyc_YYSTYPE*_TC42;struct Cyc_Parse_Type_specifier _TC43;struct Cyc_Yystacktype*_TC44;struct Cyc_Yystacktype _TC45;struct Cyc_Yyltype _TC46;unsigned _TC47;unsigned _TC48;struct Cyc_Yystacktype*_TC49;union Cyc_YYSTYPE*_TC4A;union Cyc_YYSTYPE*_TC4B;struct Cyc_Parse_Abstractdeclarator _TC4C;struct _tuple14 _TC4D;struct Cyc_List_List*_TC4E;struct Cyc_Yystacktype*_TC4F;struct Cyc_Yystacktype _TC50;struct Cyc_Yyltype _TC51;unsigned _TC52;unsigned _TC53;struct _fat_ptr _TC54;struct _fat_ptr _TC55;struct _tuple14 _TC56;struct Cyc_List_List*_TC57;struct Cyc_Yystacktype*_TC58;struct Cyc_Yystacktype _TC59;struct Cyc_Yyltype _TC5A;unsigned _TC5B;unsigned _TC5C;struct _fat_ptr _TC5D;struct _fat_ptr _TC5E;struct _tuple8*_TC5F;struct _tuple14 _TC60;struct _tuple14 _TC61;struct Cyc_Yystacktype*_TC62;union Cyc_YYSTYPE*_TC63;union Cyc_YYSTYPE*_TC64;struct _tuple8*_TC65;struct Cyc_Yystacktype*_TC66;struct Cyc_Yystacktype _TC67;struct Cyc_Yyltype _TC68;unsigned _TC69;unsigned _TC6A;void*_TC6B;void*_TC6C;struct Cyc_Yystacktype*_TC6D;union Cyc_YYSTYPE*_TC6E;union Cyc_YYSTYPE*_TC6F;struct Cyc_List_List*_TC70;void*_TC71;struct Cyc_Yystacktype*_TC72;union Cyc_YYSTYPE*_TC73;union Cyc_YYSTYPE*_TC74;void*_TC75;void*_TC76;struct Cyc_List_List*_TC77;struct Cyc_Yystacktype*_TC78;union Cyc_YYSTYPE*_TC79;union Cyc_YYSTYPE*_TC7A;struct Cyc_Yystacktype*_TC7B;union Cyc_YYSTYPE*_TC7C;union Cyc_YYSTYPE*_TC7D;void*_TC7E;struct Cyc_Yystacktype*_TC7F;union Cyc_YYSTYPE*_TC80;union Cyc_YYSTYPE*_TC81;void*_TC82;struct Cyc_List_List*_TC83;struct Cyc_Yystacktype*_TC84;union Cyc_YYSTYPE*_TC85;union Cyc_YYSTYPE*_TC86;struct Cyc_List_List*_TC87;struct Cyc_Yystacktype*_TC88;union Cyc_YYSTYPE*_TC89;union Cyc_YYSTYPE*_TC8A;struct Cyc_Yystacktype*_TC8B;union Cyc_YYSTYPE*_TC8C;union Cyc_YYSTYPE*_TC8D;struct Cyc_Parse_Abstractdeclarator _TC8E;struct Cyc_Yystacktype*_TC8F;union Cyc_YYSTYPE*_TC90;union Cyc_YYSTYPE*_TC91;struct Cyc_Yystacktype*_TC92;struct Cyc_Yystacktype _TC93;struct Cyc_Parse_Abstractdeclarator _TC94;struct Cyc_Yystacktype*_TC95;union Cyc_YYSTYPE*_TC96;union Cyc_YYSTYPE*_TC97;struct Cyc_List_List*_TC98;struct Cyc_Yystacktype*_TC99;union Cyc_YYSTYPE*_TC9A;union Cyc_YYSTYPE*_TC9B;struct Cyc_Parse_Abstractdeclarator _TC9C;struct Cyc_List_List*_TC9D;struct Cyc_Yystacktype*_TC9E;struct Cyc_Yystacktype _TC9F;struct Cyc_Parse_Abstractdeclarator _TCA0;struct Cyc_List_List*_TCA1;struct _RegionHandle*_TCA2;struct Cyc_Absyn_Carray_mod_Absyn_Type_modifier_struct*_TCA3;struct _RegionHandle*_TCA4;struct Cyc_Yystacktype*_TCA5;union Cyc_YYSTYPE*_TCA6;union Cyc_YYSTYPE*_TCA7;struct Cyc_Yystacktype*_TCA8;struct Cyc_Yystacktype _TCA9;struct Cyc_Yyltype _TCAA;unsigned _TCAB;struct Cyc_Parse_Abstractdeclarator _TCAC;struct Cyc_List_List*_TCAD;struct _RegionHandle*_TCAE;struct Cyc_Absyn_Carray_mod_Absyn_Type_modifier_struct*_TCAF;struct _RegionHandle*_TCB0;struct Cyc_Yystacktype*_TCB1;union Cyc_YYSTYPE*_TCB2;union Cyc_YYSTYPE*_TCB3;struct Cyc_Yystacktype*_TCB4;struct Cyc_Yystacktype _TCB5;struct Cyc_Yyltype _TCB6;unsigned _TCB7;struct Cyc_Yystacktype*_TCB8;union Cyc_YYSTYPE*_TCB9;union Cyc_YYSTYPE*_TCBA;struct Cyc_Parse_Abstractdeclarator _TCBB;struct Cyc_Parse_Abstractdeclarator _TCBC;struct Cyc_List_List*_TCBD;struct _RegionHandle*_TCBE;struct Cyc_Absyn_ConstArray_mod_Absyn_Type_modifier_struct*_TCBF;struct _RegionHandle*_TCC0;struct Cyc_Yystacktype*_TCC1;union Cyc_YYSTYPE*_TCC2;union Cyc_YYSTYPE*_TCC3;struct Cyc_Yystacktype*_TCC4;union Cyc_YYSTYPE*_TCC5;union Cyc_YYSTYPE*_TCC6;struct Cyc_Yystacktype*_TCC7;struct Cyc_Yystacktype _TCC8;struct Cyc_Yyltype _TCC9;unsigned _TCCA;struct Cyc_Parse_Abstractdeclarator _TCCB;struct Cyc_List_List*_TCCC;struct _RegionHandle*_TCCD;struct Cyc_Absyn_ConstArray_mod_Absyn_Type_modifier_struct*_TCCE;struct _RegionHandle*_TCCF;struct Cyc_Yystacktype*_TCD0;union Cyc_YYSTYPE*_TCD1;union Cyc_YYSTYPE*_TCD2;struct Cyc_Yystacktype*_TCD3;union Cyc_YYSTYPE*_TCD4;union Cyc_YYSTYPE*_TCD5;struct Cyc_Yystacktype*_TCD6;struct Cyc_Yystacktype _TCD7;struct Cyc_Yyltype _TCD8;unsigned _TCD9;struct Cyc_Yystacktype*_TCDA;union Cyc_YYSTYPE*_TCDB;union Cyc_YYSTYPE*_TCDC;struct Cyc_Parse_Abstractdeclarator _TCDD;struct Cyc_Yystacktype*_TCDE;union Cyc_YYSTYPE*_TCDF;union Cyc_YYSTYPE*_TCE0;struct Cyc_Yystacktype*_TCE1;union Cyc_YYSTYPE*_TCE2;union Cyc_YYSTYPE*_TCE3;struct Cyc_Parse_Abstractdeclarator _TCE4;struct Cyc_List_List*_TCE5;struct _RegionHandle*_TCE6;struct Cyc_Absyn_Function_mod_Absyn_Type_modifier_struct*_TCE7;struct _RegionHandle*_TCE8;struct Cyc_Absyn_WithTypes_Absyn_Funcparams_struct*_TCE9;struct _RegionHandle*_TCEA;struct Cyc_Yystacktype*_TCEB;union Cyc_YYSTYPE*_TCEC;union Cyc_YYSTYPE*_TCED;struct Cyc_Yystacktype*_TCEE;union Cyc_YYSTYPE*_TCEF;union Cyc_YYSTYPE*_TCF0;struct Cyc_Parse_Abstractdeclarator _TCF1;struct Cyc_List_List*_TCF2;struct _RegionHandle*_TCF3;struct Cyc_Absyn_Function_mod_Absyn_Type_modifier_struct*_TCF4;struct _RegionHandle*_TCF5;struct Cyc_Absyn_WithTypes_Absyn_Funcparams_struct*_TCF6;struct _RegionHandle*_TCF7;struct Cyc_Yystacktype*_TCF8;union Cyc_YYSTYPE*_TCF9;union Cyc_YYSTYPE*_TCFA;struct Cyc_Parse_Abstractdeclarator _TCFB;struct Cyc_List_List*(*_TCFC)(struct Cyc_Absyn_Tvar*(*)(unsigned,void*),unsigned,struct Cyc_List_List*);struct Cyc_List_List*(*_TCFD)(void*(*)(void*,void*),void*,struct Cyc_List_List*);struct Cyc_Yystacktype*_TCFE;struct Cyc_Yystacktype _TCFF;struct Cyc_Yyltype _TD00;unsigned _TD01;unsigned _TD02;struct Cyc_Yystacktype*_TD03;union Cyc_YYSTYPE*_TD04;union Cyc_YYSTYPE*_TD05;struct Cyc_List_List*_TD06;struct Cyc_List_List*_TD07;struct Cyc_Parse_Abstractdeclarator _TD08;struct Cyc_List_List*_TD09;struct _RegionHandle*_TD0A;struct Cyc_Absyn_TypeParams_mod_Absyn_Type_modifier_struct*_TD0B;struct _RegionHandle*_TD0C;struct Cyc_Yystacktype*_TD0D;struct Cyc_Yystacktype _TD0E;struct Cyc_Yyltype _TD0F;unsigned _TD10;struct Cyc_Yystacktype*_TD11;union Cyc_YYSTYPE*_TD12;union Cyc_YYSTYPE*_TD13;struct Cyc_Parse_Abstractdeclarator _TD14;struct Cyc_Parse_Abstractdeclarator _TD15;struct Cyc_List_List*_TD16;struct _RegionHandle*_TD17;struct Cyc_Absyn_Attributes_mod_Absyn_Type_modifier_struct*_TD18;struct _RegionHandle*_TD19;struct Cyc_Yystacktype*_TD1A;struct Cyc_Yystacktype _TD1B;struct Cyc_Yyltype _TD1C;unsigned _TD1D;struct Cyc_Yystacktype*_TD1E;union Cyc_YYSTYPE*_TD1F;union Cyc_YYSTYPE*_TD20;struct Cyc_Yystacktype*_TD21;union Cyc_YYSTYPE*_TD22;union Cyc_YYSTYPE*_TD23;struct Cyc_Parse_Abstractdeclarator _TD24;struct _tuple21 _TD25;struct Cyc_Yystacktype*_TD26;union Cyc_YYSTYPE*_TD27;union Cyc_YYSTYPE*_TD28;struct _tuple21 _TD29;struct Cyc_Yystacktype*_TD2A;union Cyc_YYSTYPE*_TD2B;union Cyc_YYSTYPE*_TD2C;struct _tuple21 _TD2D;struct Cyc_Yystacktype*_TD2E;union Cyc_YYSTYPE*_TD2F;union Cyc_YYSTYPE*_TD30;struct _tuple21 _TD31;struct Cyc_Yystacktype*_TD32;union Cyc_YYSTYPE*_TD33;union Cyc_YYSTYPE*_TD34;struct _tuple21 _TD35;struct Cyc_Yystacktype*_TD36;union Cyc_YYSTYPE*_TD37;union Cyc_YYSTYPE*_TD38;struct Cyc_Yystacktype*_TD39;union Cyc_YYSTYPE*_TD3A;union Cyc_YYSTYPE*_TD3B;struct _tuple21 _TD3C;struct _tuple21 _TD3D;struct _tuple21 _TD3E;struct Cyc_Yystacktype*_TD3F;union Cyc_YYSTYPE*_TD40;union Cyc_YYSTYPE*_TD41;struct Cyc_Yystacktype*_TD42;union Cyc_YYSTYPE*_TD43;union Cyc_YYSTYPE*_TD44;struct _tuple21 _TD45;struct _tuple21 _TD46;struct _tuple21 _TD47;struct Cyc_Yystacktype*_TD48;union Cyc_YYSTYPE*_TD49;union Cyc_YYSTYPE*_TD4A;struct Cyc_Yystacktype*_TD4B;union Cyc_YYSTYPE*_TD4C;union Cyc_YYSTYPE*_TD4D;struct _tuple21 _TD4E;struct _tuple21 _TD4F;struct _tuple21 _TD50;struct Cyc_Yystacktype*_TD51;union Cyc_YYSTYPE*_TD52;union Cyc_YYSTYPE*_TD53;struct Cyc_Yystacktype*_TD54;union Cyc_YYSTYPE*_TD55;union Cyc_YYSTYPE*_TD56;struct _tuple21 _TD57;struct _tuple21 _TD58;struct _tuple21 _TD59;struct Cyc_Yystacktype*_TD5A;struct Cyc_Yystacktype _TD5B;struct Cyc_Yystacktype*_TD5C;struct Cyc_Yystacktype _TD5D;struct Cyc_Yystacktype*_TD5E;struct Cyc_Yystacktype _TD5F;struct Cyc_Yystacktype*_TD60;struct Cyc_Yystacktype _TD61;struct Cyc_Yystacktype*_TD62;struct Cyc_Yystacktype _TD63;struct Cyc_Yystacktype*_TD64;struct Cyc_Yystacktype _TD65;struct Cyc_Yystacktype*_TD66;struct Cyc_Yystacktype _TD67;struct Cyc_Absyn_Label_s_Absyn_Raw_stmt_struct*_TD68;struct _fat_ptr*_TD69;struct Cyc_Yystacktype*_TD6A;union Cyc_YYSTYPE*_TD6B;union Cyc_YYSTYPE*_TD6C;struct Cyc_Yystacktype*_TD6D;union Cyc_YYSTYPE*_TD6E;union Cyc_YYSTYPE*_TD6F;void*_TD70;struct Cyc_Yystacktype*_TD71;struct Cyc_Yystacktype _TD72;struct Cyc_Yyltype _TD73;unsigned _TD74;unsigned _TD75;struct Cyc_Absyn_Stmt*_TD76;struct Cyc_Yystacktype*_TD77;struct Cyc_Yystacktype _TD78;struct Cyc_Yyltype _TD79;unsigned _TD7A;unsigned _TD7B;struct Cyc_Absyn_Stmt*_TD7C;struct Cyc_Yystacktype*_TD7D;union Cyc_YYSTYPE*_TD7E;union Cyc_YYSTYPE*_TD7F;struct Cyc_Absyn_Exp*_TD80;struct Cyc_Yystacktype*_TD81;struct Cyc_Yystacktype _TD82;struct Cyc_Yyltype _TD83;unsigned _TD84;unsigned _TD85;struct Cyc_Absyn_Stmt*_TD86;struct Cyc_Yystacktype*_TD87;struct Cyc_Yystacktype _TD88;struct Cyc_Yyltype _TD89;unsigned _TD8A;unsigned _TD8B;struct Cyc_Absyn_Stmt*_TD8C;struct Cyc_Yystacktype*_TD8D;struct Cyc_Yystacktype _TD8E;struct Cyc_Yystacktype*_TD8F;struct Cyc_Yystacktype _TD90;struct Cyc_Yyltype _TD91;unsigned _TD92;unsigned _TD93;struct Cyc_Absyn_Stmt*_TD94;struct Cyc_Yystacktype*_TD95;struct Cyc_Yystacktype _TD96;struct Cyc_Yystacktype*_TD97;union Cyc_YYSTYPE*_TD98;union Cyc_YYSTYPE*_TD99;struct Cyc_List_List*_TD9A;struct Cyc_Yystacktype*_TD9B;struct Cyc_Yystacktype _TD9C;struct Cyc_Yyltype _TD9D;unsigned _TD9E;unsigned _TD9F;struct Cyc_Absyn_Stmt*_TDA0;struct Cyc_Absyn_Stmt*_TDA1;struct Cyc_Yystacktype*_TDA2;union Cyc_YYSTYPE*_TDA3;union Cyc_YYSTYPE*_TDA4;struct Cyc_List_List*_TDA5;struct Cyc_Yystacktype*_TDA6;union Cyc_YYSTYPE*_TDA7;union Cyc_YYSTYPE*_TDA8;struct Cyc_Absyn_Stmt*_TDA9;struct Cyc_Absyn_Stmt*_TDAA;struct Cyc_Absyn_Label_s_Absyn_Raw_stmt_struct*_TDAB;struct _fat_ptr*_TDAC;struct Cyc_Yystacktype*_TDAD;union Cyc_YYSTYPE*_TDAE;union Cyc_YYSTYPE*_TDAF;struct Cyc_Yystacktype*_TDB0;union Cyc_YYSTYPE*_TDB1;union Cyc_YYSTYPE*_TDB2;struct Cyc_List_List*_TDB3;struct Cyc_Absyn_Stmt*_TDB4;void*_TDB5;struct Cyc_Yystacktype*_TDB6;struct Cyc_Yystacktype _TDB7;struct Cyc_Yyltype _TDB8;unsigned _TDB9;unsigned _TDBA;struct Cyc_Absyn_Stmt*_TDBB;struct Cyc_Absyn_Label_s_Absyn_Raw_stmt_struct*_TDBC;struct _fat_ptr*_TDBD;struct Cyc_Yystacktype*_TDBE;union Cyc_YYSTYPE*_TDBF;union Cyc_YYSTYPE*_TDC0;struct Cyc_Yystacktype*_TDC1;union Cyc_YYSTYPE*_TDC2;union Cyc_YYSTYPE*_TDC3;struct Cyc_List_List*_TDC4;struct Cyc_Yystacktype*_TDC5;union Cyc_YYSTYPE*_TDC6;union Cyc_YYSTYPE*_TDC7;struct Cyc_Absyn_Stmt*_TDC8;void*_TDC9;struct Cyc_Yystacktype*_TDCA;struct Cyc_Yystacktype _TDCB;struct Cyc_Yyltype _TDCC;unsigned _TDCD;unsigned _TDCE;struct Cyc_Absyn_Stmt*_TDCF;struct Cyc_Yystacktype*_TDD0;struct Cyc_Yystacktype _TDD1;struct Cyc_Yystacktype*_TDD2;union Cyc_YYSTYPE*_TDD3;union Cyc_YYSTYPE*_TDD4;struct Cyc_Absyn_Stmt*_TDD5;struct Cyc_Yystacktype*_TDD6;union Cyc_YYSTYPE*_TDD7;union Cyc_YYSTYPE*_TDD8;struct Cyc_Absyn_Stmt*_TDD9;struct Cyc_Yystacktype*_TDDA;struct Cyc_Yystacktype _TDDB;struct Cyc_Yyltype _TDDC;unsigned _TDDD;unsigned _TDDE;struct Cyc_Absyn_Stmt*_TDDF;struct Cyc_Absyn_Fn_d_Absyn_Raw_decl_struct*_TDE0;struct Cyc_Yystacktype*_TDE1;union Cyc_YYSTYPE*_TDE2;union Cyc_YYSTYPE*_TDE3;void*_TDE4;struct Cyc_Yystacktype*_TDE5;struct Cyc_Yystacktype _TDE6;struct Cyc_Yyltype _TDE7;unsigned _TDE8;unsigned _TDE9;struct Cyc_Absyn_Decl*_TDEA;struct Cyc_Absyn_Stmt*_TDEB;struct Cyc_Absyn_Stmt*_TDEC;struct Cyc_Absyn_Fn_d_Absyn_Raw_decl_struct*_TDED;struct Cyc_Yystacktype*_TDEE;union Cyc_YYSTYPE*_TDEF;union Cyc_YYSTYPE*_TDF0;void*_TDF1;struct Cyc_Yystacktype*_TDF2;struct Cyc_Yystacktype _TDF3;struct Cyc_Yyltype _TDF4;unsigned _TDF5;unsigned _TDF6;struct Cyc_Absyn_Decl*_TDF7;struct Cyc_Yystacktype*_TDF8;union Cyc_YYSTYPE*_TDF9;union Cyc_YYSTYPE*_TDFA;struct Cyc_Absyn_Stmt*_TDFB;struct Cyc_Absyn_Stmt*_TDFC;struct Cyc_Yystacktype*_TDFD;union Cyc_YYSTYPE*_TDFE;union Cyc_YYSTYPE*_TDFF;struct Cyc_Absyn_Exp*_TE00;struct Cyc_Yystacktype*_TE01;union Cyc_YYSTYPE*_TE02;union Cyc_YYSTYPE*_TE03;struct Cyc_Absyn_Stmt*_TE04;struct Cyc_Absyn_Stmt*_TE05;struct Cyc_Yystacktype*_TE06;struct Cyc_Yystacktype _TE07;struct Cyc_Yyltype _TE08;unsigned _TE09;unsigned _TE0A;struct Cyc_Absyn_Stmt*_TE0B;struct Cyc_Yystacktype*_TE0C;union Cyc_YYSTYPE*_TE0D;union Cyc_YYSTYPE*_TE0E;struct Cyc_Absyn_Exp*_TE0F;struct Cyc_Yystacktype*_TE10;union Cyc_YYSTYPE*_TE11;union Cyc_YYSTYPE*_TE12;struct Cyc_Absyn_Stmt*_TE13;struct Cyc_Yystacktype*_TE14;union Cyc_YYSTYPE*_TE15;union Cyc_YYSTYPE*_TE16;struct Cyc_Absyn_Stmt*_TE17;struct Cyc_Yystacktype*_TE18;struct Cyc_Yystacktype _TE19;struct Cyc_Yyltype _TE1A;unsigned _TE1B;unsigned _TE1C;struct Cyc_Absyn_Stmt*_TE1D;struct Cyc_Yystacktype*_TE1E;union Cyc_YYSTYPE*_TE1F;union Cyc_YYSTYPE*_TE20;struct Cyc_Absyn_Exp*_TE21;struct Cyc_Yystacktype*_TE22;union Cyc_YYSTYPE*_TE23;union Cyc_YYSTYPE*_TE24;struct Cyc_List_List*_TE25;struct Cyc_Yystacktype*_TE26;struct Cyc_Yystacktype _TE27;struct Cyc_Yyltype _TE28;unsigned _TE29;unsigned _TE2A;struct Cyc_Absyn_Stmt*_TE2B;struct Cyc_Yystacktype*_TE2C;union Cyc_YYSTYPE*_TE2D;union Cyc_YYSTYPE*_TE2E;struct _tuple0*_TE2F;struct Cyc_Yystacktype*_TE30;struct Cyc_Yystacktype _TE31;struct Cyc_Yyltype _TE32;unsigned _TE33;unsigned _TE34;struct Cyc_Absyn_Exp*_TE35;struct Cyc_Yystacktype*_TE36;union Cyc_YYSTYPE*_TE37;union Cyc_YYSTYPE*_TE38;struct Cyc_List_List*_TE39;struct Cyc_Yystacktype*_TE3A;struct Cyc_Yystacktype _TE3B;struct Cyc_Yyltype _TE3C;unsigned _TE3D;unsigned _TE3E;struct Cyc_Absyn_Stmt*_TE3F;struct Cyc_Yystacktype*_TE40;union Cyc_YYSTYPE*_TE41;union Cyc_YYSTYPE*_TE42;struct Cyc_List_List*_TE43;struct Cyc_Yystacktype*_TE44;struct Cyc_Yystacktype _TE45;struct Cyc_Yyltype _TE46;unsigned _TE47;unsigned _TE48;struct Cyc_Absyn_Exp*_TE49;struct Cyc_Yystacktype*_TE4A;union Cyc_YYSTYPE*_TE4B;union Cyc_YYSTYPE*_TE4C;struct Cyc_List_List*_TE4D;struct Cyc_Yystacktype*_TE4E;struct Cyc_Yystacktype _TE4F;struct Cyc_Yyltype _TE50;unsigned _TE51;unsigned _TE52;struct Cyc_Absyn_Stmt*_TE53;struct Cyc_Yystacktype*_TE54;union Cyc_YYSTYPE*_TE55;union Cyc_YYSTYPE*_TE56;struct Cyc_Absyn_Stmt*_TE57;struct Cyc_Yystacktype*_TE58;union Cyc_YYSTYPE*_TE59;union Cyc_YYSTYPE*_TE5A;struct Cyc_List_List*_TE5B;struct Cyc_Yystacktype*_TE5C;struct Cyc_Yystacktype _TE5D;struct Cyc_Yyltype _TE5E;unsigned _TE5F;unsigned _TE60;struct Cyc_Absyn_Stmt*_TE61;struct Cyc_List_List*_TE62;struct Cyc_Absyn_Switch_clause*_TE63;struct Cyc_Absyn_Wild_p_Absyn_Raw_pat_struct*_TE64;struct Cyc_Absyn_Wild_p_Absyn_Raw_pat_struct*_TE65;void*_TE66;struct Cyc_Yystacktype*_TE67;struct Cyc_Yystacktype _TE68;struct Cyc_Yyltype _TE69;unsigned _TE6A;unsigned _TE6B;struct Cyc_Yystacktype*_TE6C;union Cyc_YYSTYPE*_TE6D;union Cyc_YYSTYPE*_TE6E;struct Cyc_Yystacktype*_TE6F;struct Cyc_Yystacktype _TE70;struct Cyc_Yyltype _TE71;unsigned _TE72;struct Cyc_Yystacktype*_TE73;union Cyc_YYSTYPE*_TE74;union Cyc_YYSTYPE*_TE75;struct Cyc_List_List*_TE76;struct Cyc_Absyn_Switch_clause*_TE77;struct Cyc_Yystacktype*_TE78;union Cyc_YYSTYPE*_TE79;union Cyc_YYSTYPE*_TE7A;struct Cyc_Yystacktype*_TE7B;struct Cyc_Yystacktype _TE7C;struct Cyc_Yyltype _TE7D;unsigned _TE7E;unsigned _TE7F;struct Cyc_Yystacktype*_TE80;struct Cyc_Yystacktype _TE81;struct Cyc_Yyltype _TE82;unsigned _TE83;struct Cyc_Yystacktype*_TE84;union Cyc_YYSTYPE*_TE85;union Cyc_YYSTYPE*_TE86;struct Cyc_List_List*_TE87;struct Cyc_Absyn_Switch_clause*_TE88;struct Cyc_Yystacktype*_TE89;union Cyc_YYSTYPE*_TE8A;union Cyc_YYSTYPE*_TE8B;struct Cyc_Yystacktype*_TE8C;union Cyc_YYSTYPE*_TE8D;union Cyc_YYSTYPE*_TE8E;struct Cyc_Yystacktype*_TE8F;struct Cyc_Yystacktype _TE90;struct Cyc_Yyltype _TE91;unsigned _TE92;struct Cyc_Yystacktype*_TE93;union Cyc_YYSTYPE*_TE94;union Cyc_YYSTYPE*_TE95;struct Cyc_List_List*_TE96;struct Cyc_Absyn_Switch_clause*_TE97;struct Cyc_Yystacktype*_TE98;union Cyc_YYSTYPE*_TE99;union Cyc_YYSTYPE*_TE9A;struct Cyc_Yystacktype*_TE9B;union Cyc_YYSTYPE*_TE9C;union Cyc_YYSTYPE*_TE9D;struct Cyc_Yystacktype*_TE9E;struct Cyc_Yystacktype _TE9F;struct Cyc_Yyltype _TEA0;unsigned _TEA1;unsigned _TEA2;struct Cyc_Yystacktype*_TEA3;struct Cyc_Yystacktype _TEA4;struct Cyc_Yyltype _TEA5;unsigned _TEA6;struct Cyc_Yystacktype*_TEA7;union Cyc_YYSTYPE*_TEA8;union Cyc_YYSTYPE*_TEA9;struct Cyc_List_List*_TEAA;struct Cyc_Absyn_Switch_clause*_TEAB;struct Cyc_Yystacktype*_TEAC;union Cyc_YYSTYPE*_TEAD;union Cyc_YYSTYPE*_TEAE;struct Cyc_Yystacktype*_TEAF;union Cyc_YYSTYPE*_TEB0;union Cyc_YYSTYPE*_TEB1;struct Cyc_Yystacktype*_TEB2;union Cyc_YYSTYPE*_TEB3;union Cyc_YYSTYPE*_TEB4;struct Cyc_Yystacktype*_TEB5;struct Cyc_Yystacktype _TEB6;struct Cyc_Yyltype _TEB7;unsigned _TEB8;struct Cyc_Yystacktype*_TEB9;union Cyc_YYSTYPE*_TEBA;union Cyc_YYSTYPE*_TEBB;struct Cyc_Yystacktype*_TEBC;union Cyc_YYSTYPE*_TEBD;union Cyc_YYSTYPE*_TEBE;struct Cyc_Absyn_Exp*_TEBF;struct Cyc_Yystacktype*_TEC0;union Cyc_YYSTYPE*_TEC1;union Cyc_YYSTYPE*_TEC2;struct Cyc_Absyn_Stmt*_TEC3;struct Cyc_Yystacktype*_TEC4;struct Cyc_Yystacktype _TEC5;struct Cyc_Yyltype _TEC6;unsigned _TEC7;unsigned _TEC8;struct Cyc_Absyn_Stmt*_TEC9;struct Cyc_Yystacktype*_TECA;union Cyc_YYSTYPE*_TECB;union Cyc_YYSTYPE*_TECC;struct Cyc_Absyn_Stmt*_TECD;struct Cyc_Yystacktype*_TECE;union Cyc_YYSTYPE*_TECF;union Cyc_YYSTYPE*_TED0;struct Cyc_Absyn_Exp*_TED1;struct Cyc_Yystacktype*_TED2;struct Cyc_Yystacktype _TED3;struct Cyc_Yyltype _TED4;unsigned _TED5;unsigned _TED6;struct Cyc_Absyn_Stmt*_TED7;struct Cyc_Yystacktype*_TED8;union Cyc_YYSTYPE*_TED9;union Cyc_YYSTYPE*_TEDA;struct Cyc_Absyn_Exp*_TEDB;struct Cyc_Yystacktype*_TEDC;union Cyc_YYSTYPE*_TEDD;union Cyc_YYSTYPE*_TEDE;struct Cyc_Absyn_Exp*_TEDF;struct Cyc_Yystacktype*_TEE0;union Cyc_YYSTYPE*_TEE1;union Cyc_YYSTYPE*_TEE2;struct Cyc_Absyn_Exp*_TEE3;struct Cyc_Yystacktype*_TEE4;union Cyc_YYSTYPE*_TEE5;union Cyc_YYSTYPE*_TEE6;struct Cyc_Absyn_Stmt*_TEE7;struct Cyc_Yystacktype*_TEE8;struct Cyc_Yystacktype _TEE9;struct Cyc_Yyltype _TEEA;unsigned _TEEB;unsigned _TEEC;struct Cyc_Absyn_Stmt*_TEED;struct Cyc_Absyn_Exp*_TEEE;struct Cyc_Yystacktype*_TEEF;union Cyc_YYSTYPE*_TEF0;union Cyc_YYSTYPE*_TEF1;struct Cyc_Absyn_Exp*_TEF2;struct Cyc_Yystacktype*_TEF3;union Cyc_YYSTYPE*_TEF4;union Cyc_YYSTYPE*_TEF5;struct Cyc_Absyn_Exp*_TEF6;struct Cyc_Yystacktype*_TEF7;union Cyc_YYSTYPE*_TEF8;union Cyc_YYSTYPE*_TEF9;struct Cyc_Absyn_Stmt*_TEFA;struct Cyc_Yystacktype*_TEFB;struct Cyc_Yystacktype _TEFC;struct Cyc_Yyltype _TEFD;unsigned _TEFE;unsigned _TEFF;struct Cyc_Yystacktype*_TF00;union Cyc_YYSTYPE*_TF01;union Cyc_YYSTYPE*_TF02;struct Cyc_List_List*_TF03;struct Cyc_Absyn_Stmt*_TF04;struct Cyc_Absyn_Stmt*_TF05;struct Cyc_Absyn_Exp*_TF06;struct Cyc_Yystacktype*_TF07;struct Cyc_Yystacktype _TF08;struct _fat_ptr*_TF09;struct Cyc_Yystacktype*_TF0A;union Cyc_YYSTYPE*_TF0B;union Cyc_YYSTYPE*_TF0C;struct Cyc_Yystacktype*_TF0D;struct Cyc_Yystacktype _TF0E;struct Cyc_Yyltype _TF0F;unsigned _TF10;unsigned _TF11;struct Cyc_Absyn_Stmt*_TF12;struct Cyc_Yystacktype*_TF13;struct Cyc_Yystacktype _TF14;struct Cyc_Yyltype _TF15;unsigned _TF16;unsigned _TF17;struct Cyc_Absyn_Stmt*_TF18;struct Cyc_Yystacktype*_TF19;struct Cyc_Yystacktype _TF1A;struct Cyc_Yyltype _TF1B;unsigned _TF1C;unsigned _TF1D;struct Cyc_Absyn_Stmt*_TF1E;struct Cyc_Yystacktype*_TF1F;struct Cyc_Yystacktype _TF20;struct Cyc_Yyltype _TF21;unsigned _TF22;unsigned _TF23;struct Cyc_Absyn_Stmt*_TF24;struct Cyc_Yystacktype*_TF25;union Cyc_YYSTYPE*_TF26;union Cyc_YYSTYPE*_TF27;struct Cyc_Absyn_Exp*_TF28;struct Cyc_Yystacktype*_TF29;struct Cyc_Yystacktype _TF2A;struct Cyc_Yyltype _TF2B;unsigned _TF2C;unsigned _TF2D;struct Cyc_Absyn_Stmt*_TF2E;struct Cyc_Yystacktype*_TF2F;struct Cyc_Yystacktype _TF30;struct Cyc_Yyltype _TF31;unsigned _TF32;unsigned _TF33;struct Cyc_Absyn_Stmt*_TF34;struct Cyc_Yystacktype*_TF35;struct Cyc_Yystacktype _TF36;struct Cyc_Yyltype _TF37;unsigned _TF38;unsigned _TF39;struct Cyc_Absyn_Stmt*_TF3A;struct Cyc_Yystacktype*_TF3B;union Cyc_YYSTYPE*_TF3C;union Cyc_YYSTYPE*_TF3D;struct Cyc_List_List*_TF3E;struct Cyc_Yystacktype*_TF3F;struct Cyc_Yystacktype _TF40;struct Cyc_Yyltype _TF41;unsigned _TF42;unsigned _TF43;struct Cyc_Absyn_Stmt*_TF44;struct Cyc_Yystacktype*_TF45;struct Cyc_Yystacktype _TF46;struct Cyc_Yystacktype*_TF47;struct Cyc_Yystacktype _TF48;struct Cyc_Yystacktype*_TF49;union Cyc_YYSTYPE*_TF4A;union Cyc_YYSTYPE*_TF4B;struct Cyc_Absyn_Pat*_TF4C;struct Cyc_Absyn_Exp*_TF4D;struct Cyc_Yystacktype*_TF4E;union Cyc_YYSTYPE*_TF4F;union Cyc_YYSTYPE*_TF50;struct Cyc_Absyn_Exp*_TF51;struct Cyc_Yystacktype*_TF52;union Cyc_YYSTYPE*_TF53;union Cyc_YYSTYPE*_TF54;struct Cyc_Absyn_Exp*_TF55;struct Cyc_Yystacktype*_TF56;struct Cyc_Yystacktype _TF57;struct Cyc_Yyltype _TF58;unsigned _TF59;unsigned _TF5A;struct Cyc_Absyn_Exp*_TF5B;struct Cyc_Absyn_Pat*_TF5C;struct Cyc_Yystacktype*_TF5D;struct Cyc_Yystacktype _TF5E;struct Cyc_Yystacktype*_TF5F;union Cyc_YYSTYPE*_TF60;union Cyc_YYSTYPE*_TF61;struct Cyc_Absyn_Pat*_TF62;struct Cyc_Absyn_Exp*_TF63;struct Cyc_Yystacktype*_TF64;union Cyc_YYSTYPE*_TF65;union Cyc_YYSTYPE*_TF66;struct Cyc_Absyn_Exp*_TF67;struct Cyc_Yystacktype*_TF68;struct Cyc_Yystacktype _TF69;struct Cyc_Yyltype _TF6A;unsigned _TF6B;unsigned _TF6C;struct Cyc_Absyn_Exp*_TF6D;struct Cyc_Absyn_Pat*_TF6E;struct Cyc_Yystacktype*_TF6F;struct Cyc_Yystacktype _TF70;struct Cyc_Yystacktype*_TF71;union Cyc_YYSTYPE*_TF72;union Cyc_YYSTYPE*_TF73;struct Cyc_Absyn_Pat*_TF74;struct Cyc_Absyn_Exp*_TF75;struct Cyc_Yystacktype*_TF76;union Cyc_YYSTYPE*_TF77;union Cyc_YYSTYPE*_TF78;struct Cyc_Absyn_Exp*_TF79;struct Cyc_Yystacktype*_TF7A;struct Cyc_Yystacktype _TF7B;struct Cyc_Yyltype _TF7C;unsigned _TF7D;unsigned _TF7E;struct Cyc_Absyn_Exp*_TF7F;struct Cyc_Absyn_Pat*_TF80;struct Cyc_Yystacktype*_TF81;struct Cyc_Yystacktype _TF82;struct Cyc_Yystacktype*_TF83;union Cyc_YYSTYPE*_TF84;union Cyc_YYSTYPE*_TF85;struct Cyc_Absyn_Pat*_TF86;struct Cyc_Absyn_Exp*_TF87;struct Cyc_Yystacktype*_TF88;union Cyc_YYSTYPE*_TF89;union Cyc_YYSTYPE*_TF8A;struct Cyc_Absyn_Exp*_TF8B;struct Cyc_Yystacktype*_TF8C;struct Cyc_Yystacktype _TF8D;struct Cyc_Yyltype _TF8E;unsigned _TF8F;unsigned _TF90;struct Cyc_Absyn_Exp*_TF91;struct Cyc_Absyn_Pat*_TF92;struct Cyc_Yystacktype*_TF93;struct Cyc_Yystacktype _TF94;struct Cyc_Yystacktype*_TF95;union Cyc_YYSTYPE*_TF96;union Cyc_YYSTYPE*_TF97;struct Cyc_Absyn_Pat*_TF98;struct Cyc_Absyn_Exp*_TF99;struct Cyc_Yystacktype*_TF9A;union Cyc_YYSTYPE*_TF9B;union Cyc_YYSTYPE*_TF9C;struct Cyc_Absyn_Exp*_TF9D;struct Cyc_Yystacktype*_TF9E;struct Cyc_Yystacktype _TF9F;struct Cyc_Yyltype _TFA0;unsigned _TFA1;unsigned _TFA2;struct Cyc_Absyn_Exp*_TFA3;struct Cyc_Absyn_Pat*_TFA4;struct Cyc_Yystacktype*_TFA5;struct Cyc_Yystacktype _TFA6;struct Cyc_Yystacktype*_TFA7;union Cyc_YYSTYPE*_TFA8;union Cyc_YYSTYPE*_TFA9;struct Cyc_Absyn_Pat*_TFAA;struct Cyc_Absyn_Exp*_TFAB;struct Cyc_Yystacktype*_TFAC;union Cyc_YYSTYPE*_TFAD;union Cyc_YYSTYPE*_TFAE;struct Cyc_Absyn_Exp*_TFAF;struct Cyc_Yystacktype*_TFB0;struct Cyc_Yystacktype _TFB1;struct Cyc_Yyltype _TFB2;unsigned _TFB3;unsigned _TFB4;struct Cyc_Absyn_Exp*_TFB5;struct Cyc_Absyn_Pat*_TFB6;struct Cyc_Yystacktype*_TFB7;struct Cyc_Yystacktype _TFB8;struct Cyc_Yystacktype*_TFB9;union Cyc_YYSTYPE*_TFBA;union Cyc_YYSTYPE*_TFBB;struct Cyc_Absyn_Exp*(*_TFBC)(struct Cyc_Absyn_Exp*,struct Cyc_Absyn_Exp*,unsigned);struct Cyc_Yystacktype*_TFBD;union Cyc_YYSTYPE*_TFBE;union Cyc_YYSTYPE*_TFBF;struct Cyc_Absyn_Pat*_TFC0;struct Cyc_Absyn_Exp*_TFC1;struct Cyc_Yystacktype*_TFC2;union Cyc_YYSTYPE*_TFC3;union Cyc_YYSTYPE*_TFC4;struct Cyc_Absyn_Exp*_TFC5;struct Cyc_Yystacktype*_TFC6;struct Cyc_Yystacktype _TFC7;struct Cyc_Yyltype _TFC8;unsigned _TFC9;unsigned _TFCA;struct Cyc_Absyn_Exp*_TFCB;struct Cyc_Absyn_Pat*_TFCC;struct Cyc_Yystacktype*_TFCD;struct Cyc_Yystacktype _TFCE;struct Cyc_Yystacktype*_TFCF;union Cyc_YYSTYPE*_TFD0;union Cyc_YYSTYPE*_TFD1;struct Cyc_Absyn_Exp*(*_TFD2)(struct Cyc_Absyn_Exp*,struct Cyc_Absyn_Exp*,unsigned);struct Cyc_Yystacktype*_TFD3;union Cyc_YYSTYPE*_TFD4;union Cyc_YYSTYPE*_TFD5;struct Cyc_Absyn_Pat*_TFD6;struct Cyc_Absyn_Exp*_TFD7;struct Cyc_Yystacktype*_TFD8;union Cyc_YYSTYPE*_TFD9;union Cyc_YYSTYPE*_TFDA;struct Cyc_Absyn_Exp*_TFDB;struct Cyc_Yystacktype*_TFDC;struct Cyc_Yystacktype _TFDD;struct Cyc_Yyltype _TFDE;unsigned _TFDF;unsigned _TFE0;struct Cyc_Absyn_Exp*_TFE1;struct Cyc_Absyn_Pat*_TFE2;struct Cyc_Yystacktype*_TFE3;struct Cyc_Yystacktype _TFE4;struct Cyc_Yystacktype*_TFE5;union Cyc_YYSTYPE*_TFE6;union Cyc_YYSTYPE*_TFE7;struct Cyc_Absyn_Pat*_TFE8;struct Cyc_Absyn_Exp*_TFE9;struct Cyc_Yystacktype*_TFEA;union Cyc_YYSTYPE*_TFEB;union Cyc_YYSTYPE*_TFEC;struct Cyc_Absyn_Exp*_TFED;struct Cyc_Yystacktype*_TFEE;struct Cyc_Yystacktype _TFEF;struct Cyc_Yyltype _TFF0;unsigned _TFF1;unsigned _TFF2;struct Cyc_Absyn_Exp*_TFF3;struct Cyc_Absyn_Pat*_TFF4;struct Cyc_Yystacktype*_TFF5;union Cyc_YYSTYPE*_TFF6;union Cyc_YYSTYPE*_TFF7;struct Cyc_Absyn_Pat*_TFF8;struct Cyc_Absyn_Exp*_TFF9;struct Cyc_Yystacktype*_TFFA;union Cyc_YYSTYPE*_TFFB;union Cyc_YYSTYPE*_TFFC;struct Cyc_Absyn_Exp*_TFFD;struct Cyc_Yystacktype*_TFFE;struct Cyc_Yystacktype _TFFF;struct Cyc_Yyltype _T1000;unsigned _T1001;unsigned _T1002;struct Cyc_Absyn_Exp*_T1003;struct Cyc_Absyn_Pat*_T1004;struct Cyc_Yystacktype*_T1005;struct Cyc_Yystacktype _T1006;struct Cyc_Yystacktype*_T1007;union Cyc_YYSTYPE*_T1008;union Cyc_YYSTYPE*_T1009;enum Cyc_Absyn_Primop _T100A;struct Cyc_Yystacktype*_T100B;union Cyc_YYSTYPE*_T100C;union Cyc_YYSTYPE*_T100D;struct Cyc_Absyn_Pat*_T100E;struct Cyc_Absyn_Exp*_T100F;struct Cyc_Yystacktype*_T1010;union Cyc_YYSTYPE*_T1011;union Cyc_YYSTYPE*_T1012;struct Cyc_Absyn_Exp*_T1013;struct Cyc_Yystacktype*_T1014;struct Cyc_Yystacktype _T1015;struct Cyc_Yyltype _T1016;unsigned _T1017;unsigned _T1018;struct Cyc_Absyn_Exp*_T1019;struct Cyc_Absyn_Pat*_T101A;struct Cyc_Yystacktype*_T101B;struct Cyc_Yystacktype _T101C;struct Cyc_Yystacktype*_T101D;union Cyc_YYSTYPE*_T101E;union Cyc_YYSTYPE*_T101F;enum Cyc_Absyn_Primop _T1020;struct Cyc_Yystacktype*_T1021;union Cyc_YYSTYPE*_T1022;union Cyc_YYSTYPE*_T1023;struct Cyc_Absyn_Pat*_T1024;struct Cyc_Absyn_Exp*_T1025;struct Cyc_Yystacktype*_T1026;union Cyc_YYSTYPE*_T1027;union Cyc_YYSTYPE*_T1028;struct Cyc_Absyn_Exp*_T1029;struct Cyc_Yystacktype*_T102A;struct Cyc_Yystacktype _T102B;struct Cyc_Yyltype _T102C;unsigned _T102D;unsigned _T102E;struct Cyc_Absyn_Exp*_T102F;struct Cyc_Absyn_Pat*_T1030;struct Cyc_Yystacktype*_T1031;struct Cyc_Yystacktype _T1032;struct Cyc_Yystacktype*_T1033;union Cyc_YYSTYPE*_T1034;union Cyc_YYSTYPE*_T1035;struct _tuple8*_T1036;struct Cyc_Yystacktype*_T1037;struct Cyc_Yystacktype _T1038;struct Cyc_Yyltype _T1039;unsigned _T103A;unsigned _T103B;void*_T103C;struct Cyc_Yystacktype*_T103D;union Cyc_YYSTYPE*_T103E;union Cyc_YYSTYPE*_T103F;struct Cyc_Absyn_Exp*_T1040;struct Cyc_Yystacktype*_T1041;struct Cyc_Yystacktype _T1042;struct Cyc_Yyltype _T1043;unsigned _T1044;unsigned _T1045;struct Cyc_Absyn_Exp*_T1046;struct Cyc_Absyn_Pat*_T1047;struct Cyc_Yystacktype*_T1048;struct Cyc_Yystacktype _T1049;struct Cyc_Yystacktype*_T104A;union Cyc_YYSTYPE*_T104B;union Cyc_YYSTYPE*_T104C;enum Cyc_Absyn_Primop _T104D;struct Cyc_Yystacktype*_T104E;union Cyc_YYSTYPE*_T104F;union Cyc_YYSTYPE*_T1050;struct Cyc_Absyn_Exp*_T1051;struct Cyc_Yystacktype*_T1052;struct Cyc_Yystacktype _T1053;struct Cyc_Yyltype _T1054;unsigned _T1055;unsigned _T1056;struct Cyc_Absyn_Exp*_T1057;struct Cyc_Absyn_Pat*_T1058;struct Cyc_Yystacktype*_T1059;union Cyc_YYSTYPE*_T105A;union Cyc_YYSTYPE*_T105B;struct _tuple8*_T105C;struct Cyc_Yystacktype*_T105D;struct Cyc_Yystacktype _T105E;struct Cyc_Yyltype _T105F;unsigned _T1060;unsigned _T1061;void*_T1062;struct Cyc_Yystacktype*_T1063;struct Cyc_Yystacktype _T1064;struct Cyc_Yyltype _T1065;unsigned _T1066;unsigned _T1067;struct Cyc_Absyn_Exp*_T1068;struct Cyc_Absyn_Pat*_T1069;struct Cyc_Yystacktype*_T106A;union Cyc_YYSTYPE*_T106B;union Cyc_YYSTYPE*_T106C;struct Cyc_Absyn_Exp*_T106D;struct Cyc_Yystacktype*_T106E;struct Cyc_Yystacktype _T106F;struct Cyc_Yyltype _T1070;unsigned _T1071;unsigned _T1072;struct Cyc_Absyn_Exp*_T1073;struct Cyc_Absyn_Pat*_T1074;struct Cyc_Yystacktype*_T1075;union Cyc_YYSTYPE*_T1076;union Cyc_YYSTYPE*_T1077;struct _tuple8*_T1078;struct _tuple8 _T1079;void*_T107A;struct Cyc_Yystacktype*_T107B;union Cyc_YYSTYPE*_T107C;union Cyc_YYSTYPE*_T107D;struct Cyc_List_List*_T107E;struct Cyc_List_List*_T107F;struct Cyc_Yystacktype*_T1080;struct Cyc_Yystacktype _T1081;struct Cyc_Yyltype _T1082;unsigned _T1083;unsigned _T1084;struct Cyc_Absyn_Exp*_T1085;struct Cyc_Absyn_Pat*_T1086;struct Cyc_Yystacktype*_T1087;struct Cyc_Yystacktype _T1088;struct Cyc_Yystacktype*_T1089;struct Cyc_Yystacktype _T108A;struct Cyc_Absyn_Wild_p_Absyn_Raw_pat_struct*_T108B;struct Cyc_Absyn_Wild_p_Absyn_Raw_pat_struct*_T108C;void*_T108D;struct Cyc_Yystacktype*_T108E;struct Cyc_Yystacktype _T108F;struct Cyc_Yyltype _T1090;unsigned _T1091;unsigned _T1092;struct Cyc_Absyn_Pat*_T1093;struct Cyc_Yystacktype*_T1094;union Cyc_YYSTYPE*_T1095;union Cyc_YYSTYPE*_T1096;struct Cyc_Absyn_Exp*_T1097;struct Cyc_Absyn_Pat*_T1098;struct Cyc_Absyn_UnknownId_p_Absyn_Raw_pat_struct*_T1099;struct Cyc_Yystacktype*_T109A;union Cyc_YYSTYPE*_T109B;union Cyc_YYSTYPE*_T109C;void*_T109D;struct Cyc_Yystacktype*_T109E;struct Cyc_Yystacktype _T109F;struct Cyc_Yyltype _T10A0;unsigned _T10A1;unsigned _T10A2;struct Cyc_Absyn_Pat*_T10A3;struct Cyc_Absyn_Pointer_p_Absyn_Raw_pat_struct*_T10A4;struct Cyc_Yystacktype*_T10A5;union Cyc_YYSTYPE*_T10A6;union Cyc_YYSTYPE*_T10A7;void*_T10A8;struct Cyc_Yystacktype*_T10A9;struct Cyc_Yystacktype _T10AA;struct Cyc_Yyltype _T10AB;unsigned _T10AC;unsigned _T10AD;struct Cyc_Absyn_Pat*_T10AE;struct Cyc_Yystacktype*_T10AF;union Cyc_YYSTYPE*_T10B0;union Cyc_YYSTYPE*_T10B1;struct Cyc_Absyn_Exp*_T10B2;int*_T10B3;int _T10B4;struct Cyc_Absyn_Const_e_Absyn_Raw_exp_struct*_T10B5;union Cyc_Absyn_Cnst _T10B6;struct _union_Cnst_LongLong_c _T10B7;unsigned _T10B8;union Cyc_Absyn_Cnst _T10B9;struct _union_Cnst_Char_c _T10BA;struct _tuple3 _T10BB;union Cyc_Absyn_Cnst _T10BC;struct _union_Cnst_Char_c _T10BD;struct _tuple3 _T10BE;struct Cyc_Absyn_Char_p_Absyn_Raw_pat_struct*_T10BF;void*_T10C0;struct Cyc_Absyn_Exp*_T10C1;unsigned _T10C2;struct Cyc_Absyn_Pat*_T10C3;union Cyc_Absyn_Cnst _T10C4;struct _union_Cnst_Short_c _T10C5;struct _tuple4 _T10C6;union Cyc_Absyn_Cnst _T10C7;struct _union_Cnst_Short_c _T10C8;struct _tuple4 _T10C9;struct Cyc_Absyn_Int_p_Absyn_Raw_pat_struct*_T10CA;short _T10CB;void*_T10CC;struct Cyc_Absyn_Exp*_T10CD;unsigned _T10CE;struct Cyc_Absyn_Pat*_T10CF;union Cyc_Absyn_Cnst _T10D0;struct _union_Cnst_Int_c _T10D1;struct _tuple5 _T10D2;union Cyc_Absyn_Cnst _T10D3;struct _union_Cnst_Int_c _T10D4;struct _tuple5 _T10D5;struct Cyc_Absyn_Int_p_Absyn_Raw_pat_struct*_T10D6;void*_T10D7;struct Cyc_Absyn_Exp*_T10D8;unsigned _T10D9;struct Cyc_Absyn_Pat*_T10DA;union Cyc_Absyn_Cnst _T10DB;struct _union_Cnst_Float_c _T10DC;struct _tuple7 _T10DD;union Cyc_Absyn_Cnst _T10DE;struct _union_Cnst_Float_c _T10DF;struct _tuple7 _T10E0;struct Cyc_Absyn_Float_p_Absyn_Raw_pat_struct*_T10E1;void*_T10E2;struct Cyc_Absyn_Exp*_T10E3;unsigned _T10E4;struct Cyc_Absyn_Pat*_T10E5;struct Cyc_Absyn_Null_p_Absyn_Raw_pat_struct*_T10E6;struct Cyc_Absyn_Null_p_Absyn_Raw_pat_struct*_T10E7;void*_T10E8;struct Cyc_Absyn_Exp*_T10E9;unsigned _T10EA;struct Cyc_Absyn_Pat*_T10EB;struct Cyc_Yystacktype*_T10EC;struct Cyc_Yystacktype _T10ED;struct Cyc_Yyltype _T10EE;unsigned _T10EF;unsigned _T10F0;struct _fat_ptr _T10F1;struct _fat_ptr _T10F2;struct Cyc_Yystacktype*_T10F3;struct Cyc_Yystacktype _T10F4;struct Cyc_Yyltype _T10F5;unsigned _T10F6;unsigned _T10F7;struct _fat_ptr _T10F8;struct _fat_ptr _T10F9;struct Cyc_Yystacktype*_T10FA;struct Cyc_Yystacktype _T10FB;struct Cyc_Yyltype _T10FC;unsigned _T10FD;unsigned _T10FE;struct _fat_ptr _T10FF;struct _fat_ptr _T1100;struct Cyc_Yystacktype*_T1101;union Cyc_YYSTYPE*_T1102;union Cyc_YYSTYPE*_T1103;struct _fat_ptr _T1104;struct _fat_ptr _T1105;int _T1106;struct Cyc_Yystacktype*_T1107;struct Cyc_Yystacktype _T1108;struct Cyc_Yyltype _T1109;unsigned _T110A;unsigned _T110B;struct _fat_ptr _T110C;struct _fat_ptr _T110D;struct Cyc_Absyn_Var_p_Absyn_Raw_pat_struct*_T110E;struct Cyc_Yystacktype*_T110F;struct Cyc_Yystacktype _T1110;struct Cyc_Yyltype _T1111;unsigned _T1112;unsigned _T1113;struct _tuple0*_T1114;struct _fat_ptr*_T1115;struct Cyc_Yystacktype*_T1116;union Cyc_YYSTYPE*_T1117;union Cyc_YYSTYPE*_T1118;void*_T1119;struct Cyc_Yystacktype*_T111A;union Cyc_YYSTYPE*_T111B;union Cyc_YYSTYPE*_T111C;void*_T111D;struct Cyc_Yystacktype*_T111E;struct Cyc_Yystacktype _T111F;struct Cyc_Yyltype _T1120;unsigned _T1121;unsigned _T1122;struct Cyc_Absyn_Pat*_T1123;struct Cyc_Yystacktype*_T1124;union Cyc_YYSTYPE*_T1125;union Cyc_YYSTYPE*_T1126;struct _fat_ptr _T1127;struct _fat_ptr _T1128;int _T1129;struct Cyc_Yystacktype*_T112A;struct Cyc_Yystacktype _T112B;struct Cyc_Yyltype _T112C;unsigned _T112D;unsigned _T112E;struct _fat_ptr _T112F;struct _fat_ptr _T1130;struct Cyc_Yystacktype*_T1131;struct Cyc_Yystacktype _T1132;struct Cyc_Yyltype _T1133;unsigned _T1134;struct Cyc_Yystacktype*_T1135;union Cyc_YYSTYPE*_T1136;union Cyc_YYSTYPE*_T1137;struct _fat_ptr _T1138;unsigned _T1139;struct Cyc_Absyn_Tvar*_T113A;struct _fat_ptr*_T113B;struct Cyc_Yystacktype*_T113C;union Cyc_YYSTYPE*_T113D;union Cyc_YYSTYPE*_T113E;struct Cyc_Absyn_Tvar*_T113F;struct Cyc_Absyn_Tvar*_T1140;struct Cyc_Absyn_Eq_kb_Absyn_KindBound_struct*_T1141;struct Cyc_Absyn_Kind*_T1142;struct Cyc_Absyn_Tvar*_T1143;struct Cyc_Yystacktype*_T1144;struct Cyc_Yystacktype _T1145;struct Cyc_Yyltype _T1146;unsigned _T1147;unsigned _T1148;struct _tuple0*_T1149;struct _fat_ptr*_T114A;struct Cyc_Yystacktype*_T114B;union Cyc_YYSTYPE*_T114C;union Cyc_YYSTYPE*_T114D;struct Cyc_Yystacktype*_T114E;union Cyc_YYSTYPE*_T114F;union Cyc_YYSTYPE*_T1150;struct _tuple8*_T1151;struct Cyc_Yystacktype*_T1152;struct Cyc_Yystacktype _T1153;struct Cyc_Yyltype _T1154;unsigned _T1155;unsigned _T1156;void*_T1157;struct Cyc_Absyn_AliasVar_p_Absyn_Raw_pat_struct*_T1158;void*_T1159;unsigned _T115A;struct Cyc_Absyn_Pat*_T115B;struct Cyc_Yystacktype*_T115C;union Cyc_YYSTYPE*_T115D;union Cyc_YYSTYPE*_T115E;struct _fat_ptr _T115F;struct _fat_ptr _T1160;int _T1161;struct Cyc_Yystacktype*_T1162;struct Cyc_Yystacktype _T1163;struct Cyc_Yyltype _T1164;unsigned _T1165;unsigned _T1166;struct _fat_ptr _T1167;struct _fat_ptr _T1168;struct Cyc_Yystacktype*_T1169;struct Cyc_Yystacktype _T116A;struct Cyc_Yyltype _T116B;unsigned _T116C;struct Cyc_Yystacktype*_T116D;union Cyc_YYSTYPE*_T116E;union Cyc_YYSTYPE*_T116F;struct _fat_ptr _T1170;unsigned _T1171;struct Cyc_Absyn_Tvar*_T1172;struct _fat_ptr*_T1173;struct Cyc_Yystacktype*_T1174;union Cyc_YYSTYPE*_T1175;union Cyc_YYSTYPE*_T1176;struct Cyc_Absyn_Tvar*_T1177;struct Cyc_Absyn_Tvar*_T1178;struct Cyc_Absyn_Eq_kb_Absyn_KindBound_struct*_T1179;struct Cyc_Absyn_Kind*_T117A;struct Cyc_Absyn_Tvar*_T117B;struct Cyc_Yystacktype*_T117C;struct Cyc_Yystacktype _T117D;struct Cyc_Yyltype _T117E;unsigned _T117F;unsigned _T1180;struct _tuple0*_T1181;struct _fat_ptr*_T1182;struct Cyc_Yystacktype*_T1183;union Cyc_YYSTYPE*_T1184;union Cyc_YYSTYPE*_T1185;struct Cyc_Yystacktype*_T1186;union Cyc_YYSTYPE*_T1187;union Cyc_YYSTYPE*_T1188;struct _tuple8*_T1189;struct Cyc_Yystacktype*_T118A;struct Cyc_Yystacktype _T118B;struct Cyc_Yyltype _T118C;unsigned _T118D;unsigned _T118E;void*_T118F;struct Cyc_Absyn_AliasVar_p_Absyn_Raw_pat_struct*_T1190;void*_T1191;unsigned _T1192;struct Cyc_Absyn_Pat*_T1193;struct Cyc_Yystacktype*_T1194;union Cyc_YYSTYPE*_T1195;union Cyc_YYSTYPE*_T1196;struct _tuple23*_T1197;struct Cyc_Absyn_Aggr_p_Absyn_Raw_pat_struct*_T1198;void*_T1199;struct Cyc_Yystacktype*_T119A;struct Cyc_Yystacktype _T119B;struct Cyc_Yyltype _T119C;unsigned _T119D;unsigned _T119E;struct Cyc_Absyn_Pat*_T119F;struct Cyc_Yystacktype*_T11A0;union Cyc_YYSTYPE*_T11A1;union Cyc_YYSTYPE*_T11A2;struct _tuple23*_T11A3;struct Cyc_Absyn_UnknownCall_p_Absyn_Raw_pat_struct*_T11A4;struct Cyc_Yystacktype*_T11A5;union Cyc_YYSTYPE*_T11A6;union Cyc_YYSTYPE*_T11A7;void*_T11A8;struct Cyc_Yystacktype*_T11A9;struct Cyc_Yystacktype _T11AA;struct Cyc_Yyltype _T11AB;unsigned _T11AC;unsigned _T11AD;struct Cyc_Absyn_Pat*_T11AE;struct Cyc_Yystacktype*_T11AF;union Cyc_YYSTYPE*_T11B0;union Cyc_YYSTYPE*_T11B1;struct _tuple23*_T11B2;struct Cyc_List_List*(*_T11B3)(struct Cyc_Absyn_Tvar*(*)(unsigned,void*),unsigned,struct Cyc_List_List*);struct Cyc_List_List*(*_T11B4)(void*(*)(void*,void*),void*,struct Cyc_List_List*);struct Cyc_Yystacktype*_T11B5;struct Cyc_Yystacktype _T11B6;struct Cyc_Yyltype _T11B7;unsigned _T11B8;unsigned _T11B9;struct Cyc_Yystacktype*_T11BA;union Cyc_YYSTYPE*_T11BB;union Cyc_YYSTYPE*_T11BC;struct Cyc_List_List*_T11BD;struct Cyc_Yystacktype*_T11BE;union Cyc_YYSTYPE*_T11BF;union Cyc_YYSTYPE*_T11C0;struct _tuple0*_T11C1;struct Cyc_Absyn_AppType_Absyn_Type_struct*_T11C2;struct Cyc_Absyn_AppType_Absyn_Type_struct*_T11C3;struct Cyc_Absyn_AggrCon_Absyn_TyCon_struct*_T11C4;struct Cyc_Absyn_AppType_Absyn_Type_struct*_T11C5;struct Cyc_Absyn_Aggr_p_Absyn_Raw_pat_struct*_T11C6;struct Cyc_Absyn_AppType_Absyn_Type_struct*_T11C7;void*_T11C8;struct Cyc_Yystacktype*_T11C9;struct Cyc_Yystacktype _T11CA;struct Cyc_Yyltype _T11CB;unsigned _T11CC;unsigned _T11CD;struct Cyc_Absyn_Pat*_T11CE;struct Cyc_Yystacktype*_T11CF;union Cyc_YYSTYPE*_T11D0;union Cyc_YYSTYPE*_T11D1;struct _tuple23*_T11D2;struct Cyc_List_List*(*_T11D3)(struct Cyc_Absyn_Tvar*(*)(unsigned,void*),unsigned,struct Cyc_List_List*);struct Cyc_List_List*(*_T11D4)(void*(*)(void*,void*),void*,struct Cyc_List_List*);struct Cyc_Yystacktype*_T11D5;struct Cyc_Yystacktype _T11D6;struct Cyc_Yyltype _T11D7;unsigned _T11D8;unsigned _T11D9;struct Cyc_Yystacktype*_T11DA;union Cyc_YYSTYPE*_T11DB;union Cyc_YYSTYPE*_T11DC;struct Cyc_List_List*_T11DD;struct Cyc_Absyn_Aggr_p_Absyn_Raw_pat_struct*_T11DE;void*_T11DF;struct Cyc_Yystacktype*_T11E0;struct Cyc_Yystacktype _T11E1;struct Cyc_Yyltype _T11E2;unsigned _T11E3;unsigned _T11E4;struct Cyc_Absyn_Pat*_T11E5;struct Cyc_Absyn_Pointer_p_Absyn_Raw_pat_struct*_T11E6;struct Cyc_Absyn_Pointer_p_Absyn_Raw_pat_struct*_T11E7;struct Cyc_Yystacktype*_T11E8;union Cyc_YYSTYPE*_T11E9;union Cyc_YYSTYPE*_T11EA;void*_T11EB;struct Cyc_Yystacktype*_T11EC;struct Cyc_Yystacktype _T11ED;struct Cyc_Yyltype _T11EE;unsigned _T11EF;unsigned _T11F0;void*_T11F1;struct Cyc_Yystacktype*_T11F2;struct Cyc_Yystacktype _T11F3;struct Cyc_Yyltype _T11F4;unsigned _T11F5;unsigned _T11F6;struct Cyc_Absyn_Pat*_T11F7;struct Cyc_Absyn_Reference_p_Absyn_Raw_pat_struct*_T11F8;struct Cyc_Yystacktype*_T11F9;struct Cyc_Yystacktype _T11FA;struct Cyc_Yyltype _T11FB;unsigned _T11FC;unsigned _T11FD;struct _tuple0*_T11FE;struct _fat_ptr*_T11FF;struct Cyc_Yystacktype*_T1200;union Cyc_YYSTYPE*_T1201;union Cyc_YYSTYPE*_T1202;void*_T1203;struct Cyc_Absyn_Wild_p_Absyn_Raw_pat_struct*_T1204;struct Cyc_Absyn_Wild_p_Absyn_Raw_pat_struct*_T1205;void*_T1206;struct Cyc_Yystacktype*_T1207;struct Cyc_Yystacktype _T1208;struct Cyc_Yyltype _T1209;unsigned _T120A;unsigned _T120B;void*_T120C;struct Cyc_Yystacktype*_T120D;struct Cyc_Yystacktype _T120E;struct Cyc_Yyltype _T120F;unsigned _T1210;unsigned _T1211;struct Cyc_Absyn_Pat*_T1212;struct Cyc_Yystacktype*_T1213;union Cyc_YYSTYPE*_T1214;union Cyc_YYSTYPE*_T1215;struct _fat_ptr _T1216;struct _fat_ptr _T1217;int _T1218;struct Cyc_Yystacktype*_T1219;struct Cyc_Yystacktype _T121A;struct Cyc_Yyltype _T121B;unsigned _T121C;unsigned _T121D;struct _fat_ptr _T121E;struct _fat_ptr _T121F;struct Cyc_Absyn_Reference_p_Absyn_Raw_pat_struct*_T1220;struct Cyc_Yystacktype*_T1221;struct Cyc_Yystacktype _T1222;struct Cyc_Yyltype _T1223;unsigned _T1224;unsigned _T1225;struct _tuple0*_T1226;struct _fat_ptr*_T1227;struct Cyc_Yystacktype*_T1228;union Cyc_YYSTYPE*_T1229;union Cyc_YYSTYPE*_T122A;void*_T122B;struct Cyc_Yystacktype*_T122C;union Cyc_YYSTYPE*_T122D;union Cyc_YYSTYPE*_T122E;void*_T122F;struct Cyc_Yystacktype*_T1230;struct Cyc_Yystacktype _T1231;struct Cyc_Yyltype _T1232;unsigned _T1233;unsigned _T1234;struct Cyc_Absyn_Pat*_T1235;struct Cyc_Yystacktype*_T1236;union Cyc_YYSTYPE*_T1237;union Cyc_YYSTYPE*_T1238;struct _fat_ptr _T1239;struct Cyc_Absyn_Kind*_T123A;struct Cyc_Absyn_Kind*_T123B;void*_T123C;struct Cyc_Yystacktype*_T123D;struct Cyc_Yystacktype _T123E;struct Cyc_Yyltype _T123F;unsigned _T1240;unsigned _T1241;struct Cyc_Absyn_TagInt_p_Absyn_Raw_pat_struct*_T1242;struct Cyc_Yystacktype*_T1243;struct Cyc_Yystacktype _T1244;struct Cyc_Yyltype _T1245;unsigned _T1246;unsigned _T1247;void*_T1248;struct Cyc_Yystacktype*_T1249;struct Cyc_Yystacktype _T124A;struct Cyc_Yyltype _T124B;unsigned _T124C;unsigned _T124D;struct _tuple0*_T124E;struct _fat_ptr*_T124F;struct Cyc_Yystacktype*_T1250;union Cyc_YYSTYPE*_T1251;union Cyc_YYSTYPE*_T1252;void*_T1253;void*_T1254;struct Cyc_Yystacktype*_T1255;struct Cyc_Yystacktype _T1256;struct Cyc_Yyltype _T1257;unsigned _T1258;unsigned _T1259;struct Cyc_Absyn_Pat*_T125A;struct Cyc_Absyn_Kind*_T125B;struct Cyc_Absyn_Kind*_T125C;void*_T125D;struct Cyc_Absyn_TagInt_p_Absyn_Raw_pat_struct*_T125E;struct Cyc_Yystacktype*_T125F;struct Cyc_Yystacktype _T1260;struct Cyc_Yyltype _T1261;unsigned _T1262;unsigned _T1263;struct _tuple0*_T1264;struct _fat_ptr*_T1265;struct Cyc_Yystacktype*_T1266;union Cyc_YYSTYPE*_T1267;union Cyc_YYSTYPE*_T1268;void*_T1269;void*_T126A;void*_T126B;struct Cyc_Yystacktype*_T126C;struct Cyc_Yystacktype _T126D;struct Cyc_Yyltype _T126E;unsigned _T126F;unsigned _T1270;struct Cyc_Absyn_Pat*_T1271;struct _tuple23*_T1272;struct Cyc_Yystacktype*_T1273;union Cyc_YYSTYPE*_T1274;union Cyc_YYSTYPE*_T1275;struct Cyc_List_List*_T1276;struct _tuple23*_T1277;struct Cyc_Yystacktype*_T1278;union Cyc_YYSTYPE*_T1279;union Cyc_YYSTYPE*_T127A;struct Cyc_List_List*_T127B;struct _tuple23*_T127C;struct Cyc_List_List*_T127D;struct Cyc_Yystacktype*_T127E;union Cyc_YYSTYPE*_T127F;union Cyc_YYSTYPE*_T1280;struct Cyc_List_List*_T1281;struct Cyc_Yystacktype*_T1282;union Cyc_YYSTYPE*_T1283;union Cyc_YYSTYPE*_T1284;struct Cyc_Yystacktype*_T1285;union Cyc_YYSTYPE*_T1286;union Cyc_YYSTYPE*_T1287;struct _tuple24*_T1288;struct Cyc_Yystacktype*_T1289;union Cyc_YYSTYPE*_T128A;union Cyc_YYSTYPE*_T128B;struct _tuple24*_T128C;struct Cyc_Yystacktype*_T128D;union Cyc_YYSTYPE*_T128E;union Cyc_YYSTYPE*_T128F;struct Cyc_Yystacktype*_T1290;union Cyc_YYSTYPE*_T1291;union Cyc_YYSTYPE*_T1292;struct _tuple23*_T1293;struct Cyc_Yystacktype*_T1294;union Cyc_YYSTYPE*_T1295;union Cyc_YYSTYPE*_T1296;struct Cyc_List_List*_T1297;struct _tuple23*_T1298;struct Cyc_Yystacktype*_T1299;union Cyc_YYSTYPE*_T129A;union Cyc_YYSTYPE*_T129B;struct Cyc_List_List*_T129C;struct _tuple23*_T129D;struct Cyc_List_List*_T129E;struct Cyc_Yystacktype*_T129F;union Cyc_YYSTYPE*_T12A0;union Cyc_YYSTYPE*_T12A1;struct Cyc_List_List*_T12A2;struct Cyc_Yystacktype*_T12A3;union Cyc_YYSTYPE*_T12A4;union Cyc_YYSTYPE*_T12A5;struct Cyc_Yystacktype*_T12A6;union Cyc_YYSTYPE*_T12A7;union Cyc_YYSTYPE*_T12A8;struct Cyc_Yystacktype*_T12A9;struct Cyc_Yystacktype _T12AA;struct Cyc_Yystacktype*_T12AB;union Cyc_YYSTYPE*_T12AC;union Cyc_YYSTYPE*_T12AD;struct Cyc_Absyn_Exp*_T12AE;struct Cyc_Yystacktype*_T12AF;union Cyc_YYSTYPE*_T12B0;union Cyc_YYSTYPE*_T12B1;struct Cyc_Absyn_Exp*_T12B2;struct Cyc_Yystacktype*_T12B3;struct Cyc_Yystacktype _T12B4;struct Cyc_Yyltype _T12B5;unsigned _T12B6;unsigned _T12B7;struct Cyc_Absyn_Exp*_T12B8;struct Cyc_Yystacktype*_T12B9;struct Cyc_Yystacktype _T12BA;struct Cyc_Yystacktype*_T12BB;union Cyc_YYSTYPE*_T12BC;union Cyc_YYSTYPE*_T12BD;struct Cyc_Absyn_Exp*_T12BE;struct Cyc_Yystacktype*_T12BF;union Cyc_YYSTYPE*_T12C0;union Cyc_YYSTYPE*_T12C1;struct Cyc_Core_Opt*_T12C2;struct Cyc_Yystacktype*_T12C3;union Cyc_YYSTYPE*_T12C4;union Cyc_YYSTYPE*_T12C5;struct Cyc_Absyn_Exp*_T12C6;struct Cyc_Yystacktype*_T12C7;struct Cyc_Yystacktype _T12C8;struct Cyc_Yyltype _T12C9;unsigned _T12CA;unsigned _T12CB;struct Cyc_Absyn_Exp*_T12CC;struct Cyc_Yystacktype*_T12CD;union Cyc_YYSTYPE*_T12CE;union Cyc_YYSTYPE*_T12CF;struct Cyc_Absyn_Exp*_T12D0;struct Cyc_Yystacktype*_T12D1;union Cyc_YYSTYPE*_T12D2;union Cyc_YYSTYPE*_T12D3;struct Cyc_Absyn_Exp*_T12D4;struct Cyc_Yystacktype*_T12D5;struct Cyc_Yystacktype _T12D6;struct Cyc_Yyltype _T12D7;unsigned _T12D8;unsigned _T12D9;struct Cyc_Absyn_Exp*_T12DA;struct Cyc_Core_Opt*_T12DB;struct Cyc_Core_Opt*_T12DC;struct Cyc_Core_Opt*_T12DD;struct Cyc_Core_Opt*_T12DE;struct Cyc_Core_Opt*_T12DF;struct Cyc_Core_Opt*_T12E0;struct Cyc_Core_Opt*_T12E1;struct Cyc_Core_Opt*_T12E2;struct Cyc_Core_Opt*_T12E3;struct Cyc_Core_Opt*_T12E4;struct Cyc_Yystacktype*_T12E5;struct Cyc_Yystacktype _T12E6;struct Cyc_Yystacktype*_T12E7;union Cyc_YYSTYPE*_T12E8;union Cyc_YYSTYPE*_T12E9;struct Cyc_Absyn_Exp*_T12EA;struct Cyc_Yystacktype*_T12EB;union Cyc_YYSTYPE*_T12EC;union Cyc_YYSTYPE*_T12ED;struct Cyc_Absyn_Exp*_T12EE;struct Cyc_Yystacktype*_T12EF;union Cyc_YYSTYPE*_T12F0;union Cyc_YYSTYPE*_T12F1;struct Cyc_Absyn_Exp*_T12F2;struct Cyc_Yystacktype*_T12F3;struct Cyc_Yystacktype _T12F4;struct Cyc_Yyltype _T12F5;unsigned _T12F6;unsigned _T12F7;struct Cyc_Absyn_Exp*_T12F8;struct Cyc_Yystacktype*_T12F9;union Cyc_YYSTYPE*_T12FA;union Cyc_YYSTYPE*_T12FB;struct Cyc_Absyn_Exp*_T12FC;struct Cyc_Yystacktype*_T12FD;struct Cyc_Yystacktype _T12FE;struct Cyc_Yyltype _T12FF;unsigned _T1300;unsigned _T1301;struct Cyc_Absyn_Exp*_T1302;struct Cyc_Yystacktype*_T1303;union Cyc_YYSTYPE*_T1304;union Cyc_YYSTYPE*_T1305;struct Cyc_Absyn_Exp*_T1306;struct Cyc_Yystacktype*_T1307;struct Cyc_Yystacktype _T1308;struct Cyc_Yyltype _T1309;unsigned _T130A;unsigned _T130B;struct Cyc_Absyn_Exp*_T130C;struct Cyc_Yystacktype*_T130D;union Cyc_YYSTYPE*_T130E;union Cyc_YYSTYPE*_T130F;struct Cyc_Absyn_Exp*_T1310;struct Cyc_Yystacktype*_T1311;struct Cyc_Yystacktype _T1312;struct Cyc_Yyltype _T1313;unsigned _T1314;unsigned _T1315;struct Cyc_Absyn_Exp*_T1316;struct Cyc_Yystacktype*_T1317;union Cyc_YYSTYPE*_T1318;union Cyc_YYSTYPE*_T1319;struct Cyc_Absyn_Exp*_T131A;struct Cyc_Yystacktype*_T131B;union Cyc_YYSTYPE*_T131C;union Cyc_YYSTYPE*_T131D;struct Cyc_Absyn_Exp*_T131E;struct Cyc_Yystacktype*_T131F;struct Cyc_Yystacktype _T1320;struct Cyc_Yyltype _T1321;unsigned _T1322;unsigned _T1323;struct Cyc_Absyn_Exp*_T1324;struct Cyc_Yystacktype*_T1325;union Cyc_YYSTYPE*_T1326;union Cyc_YYSTYPE*_T1327;struct Cyc_Absyn_Exp*_T1328;struct Cyc_Yystacktype*_T1329;union Cyc_YYSTYPE*_T132A;union Cyc_YYSTYPE*_T132B;struct Cyc_Absyn_Exp*_T132C;struct Cyc_Yystacktype*_T132D;struct Cyc_Yystacktype _T132E;struct Cyc_Yyltype _T132F;unsigned _T1330;unsigned _T1331;struct Cyc_Absyn_Exp*_T1332;struct Cyc_Yystacktype*_T1333;union Cyc_YYSTYPE*_T1334;union Cyc_YYSTYPE*_T1335;struct Cyc_Absyn_Exp*_T1336;struct Cyc_Absyn_Exp*_T1337;struct Cyc_Yystacktype*_T1338;union Cyc_YYSTYPE*_T1339;union Cyc_YYSTYPE*_T133A;struct Cyc_Absyn_Exp*_T133B;struct Cyc_Absyn_Exp*_T133C;struct Cyc_Yystacktype*_T133D;struct Cyc_Yystacktype _T133E;struct Cyc_Yyltype _T133F;unsigned _T1340;unsigned _T1341;struct Cyc_Absyn_Exp*_T1342;struct Cyc_Yystacktype*_T1343;union Cyc_YYSTYPE*_T1344;union Cyc_YYSTYPE*_T1345;struct Cyc_Absyn_Exp*_T1346;struct Cyc_Absyn_Exp*_T1347;struct Cyc_Yystacktype*_T1348;union Cyc_YYSTYPE*_T1349;union Cyc_YYSTYPE*_T134A;struct Cyc_Absyn_Exp*_T134B;struct Cyc_Absyn_Exp*_T134C;struct Cyc_Yystacktype*_T134D;struct Cyc_Yystacktype _T134E;struct Cyc_Yyltype _T134F;unsigned _T1350;unsigned _T1351;struct Cyc_Absyn_Exp*_T1352;struct Cyc_Yystacktype*_T1353;struct Cyc_Yystacktype _T1354;struct Cyc_Yystacktype*_T1355;struct Cyc_Yystacktype _T1356;struct Cyc_Yystacktype*_T1357;union Cyc_YYSTYPE*_T1358;union Cyc_YYSTYPE*_T1359;struct Cyc_Absyn_Exp*_T135A;struct Cyc_Yystacktype*_T135B;union Cyc_YYSTYPE*_T135C;union Cyc_YYSTYPE*_T135D;struct Cyc_Absyn_Exp*_T135E;struct Cyc_Yystacktype*_T135F;struct Cyc_Yystacktype _T1360;struct Cyc_Yyltype _T1361;unsigned _T1362;unsigned _T1363;struct Cyc_Absyn_Exp*_T1364;struct Cyc_Yystacktype*_T1365;struct Cyc_Yystacktype _T1366;struct Cyc_Yystacktype*_T1367;union Cyc_YYSTYPE*_T1368;union Cyc_YYSTYPE*_T1369;struct Cyc_Absyn_Exp*_T136A;struct Cyc_Yystacktype*_T136B;union Cyc_YYSTYPE*_T136C;union Cyc_YYSTYPE*_T136D;struct Cyc_Absyn_Exp*_T136E;struct Cyc_Yystacktype*_T136F;struct Cyc_Yystacktype _T1370;struct Cyc_Yyltype _T1371;unsigned _T1372;unsigned _T1373;struct Cyc_Absyn_Exp*_T1374;struct Cyc_Yystacktype*_T1375;struct Cyc_Yystacktype _T1376;struct Cyc_Yystacktype*_T1377;union Cyc_YYSTYPE*_T1378;union Cyc_YYSTYPE*_T1379;struct Cyc_Absyn_Exp*_T137A;struct Cyc_Yystacktype*_T137B;union Cyc_YYSTYPE*_T137C;union Cyc_YYSTYPE*_T137D;struct Cyc_Absyn_Exp*_T137E;struct Cyc_Yystacktype*_T137F;struct Cyc_Yystacktype _T1380;struct Cyc_Yyltype _T1381;unsigned _T1382;unsigned _T1383;struct Cyc_Absyn_Exp*_T1384;struct Cyc_Yystacktype*_T1385;struct Cyc_Yystacktype _T1386;struct Cyc_Yystacktype*_T1387;union Cyc_YYSTYPE*_T1388;union Cyc_YYSTYPE*_T1389;struct Cyc_Absyn_Exp*_T138A;struct Cyc_Yystacktype*_T138B;union Cyc_YYSTYPE*_T138C;union Cyc_YYSTYPE*_T138D;struct Cyc_Absyn_Exp*_T138E;struct Cyc_Yystacktype*_T138F;struct Cyc_Yystacktype _T1390;struct Cyc_Yyltype _T1391;unsigned _T1392;unsigned _T1393;struct Cyc_Absyn_Exp*_T1394;struct Cyc_Yystacktype*_T1395;struct Cyc_Yystacktype _T1396;struct Cyc_Yystacktype*_T1397;union Cyc_YYSTYPE*_T1398;union Cyc_YYSTYPE*_T1399;struct Cyc_Absyn_Exp*_T139A;struct Cyc_Yystacktype*_T139B;union Cyc_YYSTYPE*_T139C;union Cyc_YYSTYPE*_T139D;struct Cyc_Absyn_Exp*_T139E;struct Cyc_Yystacktype*_T139F;struct Cyc_Yystacktype _T13A0;struct Cyc_Yyltype _T13A1;unsigned _T13A2;unsigned _T13A3;struct Cyc_Absyn_Exp*_T13A4;struct Cyc_Yystacktype*_T13A5;struct Cyc_Yystacktype _T13A6;struct Cyc_Yystacktype*_T13A7;union Cyc_YYSTYPE*_T13A8;union Cyc_YYSTYPE*_T13A9;struct Cyc_Absyn_Exp*(*_T13AA)(struct Cyc_Absyn_Exp*,struct Cyc_Absyn_Exp*,unsigned);struct Cyc_Yystacktype*_T13AB;union Cyc_YYSTYPE*_T13AC;union Cyc_YYSTYPE*_T13AD;struct Cyc_Absyn_Exp*_T13AE;struct Cyc_Yystacktype*_T13AF;union Cyc_YYSTYPE*_T13B0;union Cyc_YYSTYPE*_T13B1;struct Cyc_Absyn_Exp*_T13B2;struct Cyc_Yystacktype*_T13B3;struct Cyc_Yystacktype _T13B4;struct Cyc_Yyltype _T13B5;unsigned _T13B6;unsigned _T13B7;struct Cyc_Absyn_Exp*_T13B8;struct Cyc_Yystacktype*_T13B9;struct Cyc_Yystacktype _T13BA;struct Cyc_Yystacktype*_T13BB;union Cyc_YYSTYPE*_T13BC;union Cyc_YYSTYPE*_T13BD;struct Cyc_Absyn_Exp*(*_T13BE)(struct Cyc_Absyn_Exp*,struct Cyc_Absyn_Exp*,unsigned);struct Cyc_Yystacktype*_T13BF;union Cyc_YYSTYPE*_T13C0;union Cyc_YYSTYPE*_T13C1;struct Cyc_Absyn_Exp*_T13C2;struct Cyc_Yystacktype*_T13C3;union Cyc_YYSTYPE*_T13C4;union Cyc_YYSTYPE*_T13C5;struct Cyc_Absyn_Exp*_T13C6;struct Cyc_Yystacktype*_T13C7;struct Cyc_Yystacktype _T13C8;struct Cyc_Yyltype _T13C9;unsigned _T13CA;unsigned _T13CB;struct Cyc_Absyn_Exp*_T13CC;struct Cyc_Yystacktype*_T13CD;struct Cyc_Yystacktype _T13CE;struct Cyc_Yystacktype*_T13CF;union Cyc_YYSTYPE*_T13D0;union Cyc_YYSTYPE*_T13D1;struct Cyc_Absyn_Exp*_T13D2;struct Cyc_Yystacktype*_T13D3;union Cyc_YYSTYPE*_T13D4;union Cyc_YYSTYPE*_T13D5;struct Cyc_Absyn_Exp*_T13D6;struct Cyc_Yystacktype*_T13D7;struct Cyc_Yystacktype _T13D8;struct Cyc_Yyltype _T13D9;unsigned _T13DA;unsigned _T13DB;struct Cyc_Absyn_Exp*_T13DC;struct Cyc_Yystacktype*_T13DD;union Cyc_YYSTYPE*_T13DE;union Cyc_YYSTYPE*_T13DF;struct Cyc_Absyn_Exp*_T13E0;struct Cyc_Yystacktype*_T13E1;union Cyc_YYSTYPE*_T13E2;union Cyc_YYSTYPE*_T13E3;struct Cyc_Absyn_Exp*_T13E4;struct Cyc_Yystacktype*_T13E5;struct Cyc_Yystacktype _T13E6;struct Cyc_Yyltype _T13E7;unsigned _T13E8;unsigned _T13E9;struct Cyc_Absyn_Exp*_T13EA;struct Cyc_Yystacktype*_T13EB;struct Cyc_Yystacktype _T13EC;struct Cyc_Yystacktype*_T13ED;union Cyc_YYSTYPE*_T13EE;union Cyc_YYSTYPE*_T13EF;enum Cyc_Absyn_Primop _T13F0;struct Cyc_Yystacktype*_T13F1;union Cyc_YYSTYPE*_T13F2;union Cyc_YYSTYPE*_T13F3;struct Cyc_Absyn_Exp*_T13F4;struct Cyc_Yystacktype*_T13F5;union Cyc_YYSTYPE*_T13F6;union Cyc_YYSTYPE*_T13F7;struct Cyc_Absyn_Exp*_T13F8;struct Cyc_Yystacktype*_T13F9;struct Cyc_Yystacktype _T13FA;struct Cyc_Yyltype _T13FB;unsigned _T13FC;unsigned _T13FD;struct Cyc_Absyn_Exp*_T13FE;struct Cyc_Yystacktype*_T13FF;struct Cyc_Yystacktype _T1400;struct Cyc_Yystacktype*_T1401;union Cyc_YYSTYPE*_T1402;union Cyc_YYSTYPE*_T1403;enum Cyc_Absyn_Primop _T1404;struct Cyc_Yystacktype*_T1405;union Cyc_YYSTYPE*_T1406;union Cyc_YYSTYPE*_T1407;struct Cyc_Absyn_Exp*_T1408;struct Cyc_Yystacktype*_T1409;union Cyc_YYSTYPE*_T140A;union Cyc_YYSTYPE*_T140B;struct Cyc_Absyn_Exp*_T140C;struct Cyc_Yystacktype*_T140D;struct Cyc_Yystacktype _T140E;struct Cyc_Yyltype _T140F;unsigned _T1410;unsigned _T1411;struct Cyc_Absyn_Exp*_T1412;struct Cyc_Absyn_Exp*(*_T1413)(struct Cyc_Absyn_Exp*,struct Cyc_Absyn_Exp*,unsigned);struct Cyc_Absyn_Exp*(*_T1414)(struct Cyc_Absyn_Exp*,struct Cyc_Absyn_Exp*,unsigned);struct Cyc_Absyn_Exp*(*_T1415)(struct Cyc_Absyn_Exp*,struct Cyc_Absyn_Exp*,unsigned);struct Cyc_Absyn_Exp*(*_T1416)(struct Cyc_Absyn_Exp*,struct Cyc_Absyn_Exp*,unsigned);struct Cyc_Absyn_Exp*(*_T1417)(struct Cyc_Absyn_Exp*,struct Cyc_Absyn_Exp*,unsigned);struct Cyc_Absyn_Exp*(*_T1418)(struct Cyc_Absyn_Exp*,struct Cyc_Absyn_Exp*,unsigned);struct Cyc_Yystacktype*_T1419;struct Cyc_Yystacktype _T141A;struct Cyc_Yystacktype*_T141B;union Cyc_YYSTYPE*_T141C;union Cyc_YYSTYPE*_T141D;struct _tuple8*_T141E;struct Cyc_Yystacktype*_T141F;struct Cyc_Yystacktype _T1420;struct Cyc_Yyltype _T1421;unsigned _T1422;unsigned _T1423;void*_T1424;struct Cyc_Yystacktype*_T1425;union Cyc_YYSTYPE*_T1426;union Cyc_YYSTYPE*_T1427;struct Cyc_Absyn_Exp*_T1428;struct Cyc_Yystacktype*_T1429;struct Cyc_Yystacktype _T142A;struct Cyc_Yyltype _T142B;unsigned _T142C;unsigned _T142D;struct Cyc_Absyn_Exp*_T142E;struct Cyc_Yystacktype*_T142F;struct Cyc_Yystacktype _T1430;struct Cyc_Yystacktype*_T1431;union Cyc_YYSTYPE*_T1432;union Cyc_YYSTYPE*_T1433;struct Cyc_Absyn_Exp*_T1434;struct Cyc_Yystacktype*_T1435;struct Cyc_Yystacktype _T1436;struct Cyc_Yyltype _T1437;unsigned _T1438;unsigned _T1439;struct Cyc_Absyn_Exp*_T143A;struct Cyc_Yystacktype*_T143B;union Cyc_YYSTYPE*_T143C;union Cyc_YYSTYPE*_T143D;struct Cyc_Absyn_Exp*_T143E;struct Cyc_Yystacktype*_T143F;struct Cyc_Yystacktype _T1440;struct Cyc_Yyltype _T1441;unsigned _T1442;unsigned _T1443;struct Cyc_Absyn_Exp*_T1444;struct Cyc_Yystacktype*_T1445;union Cyc_YYSTYPE*_T1446;union Cyc_YYSTYPE*_T1447;struct Cyc_Absyn_Exp*_T1448;struct Cyc_Yystacktype*_T1449;struct Cyc_Yystacktype _T144A;struct Cyc_Yyltype _T144B;unsigned _T144C;unsigned _T144D;struct Cyc_Absyn_Exp*_T144E;struct Cyc_Yystacktype*_T144F;union Cyc_YYSTYPE*_T1450;union Cyc_YYSTYPE*_T1451;struct Cyc_Absyn_Exp*_T1452;struct Cyc_Yystacktype*_T1453;struct Cyc_Yystacktype _T1454;struct Cyc_Yyltype _T1455;unsigned _T1456;unsigned _T1457;struct Cyc_Absyn_Exp*_T1458;struct Cyc_Yystacktype*_T1459;union Cyc_YYSTYPE*_T145A;union Cyc_YYSTYPE*_T145B;enum Cyc_Absyn_Primop _T145C;struct Cyc_Yystacktype*_T145D;union Cyc_YYSTYPE*_T145E;union Cyc_YYSTYPE*_T145F;struct Cyc_Absyn_Exp*_T1460;struct Cyc_Yystacktype*_T1461;struct Cyc_Yystacktype _T1462;struct Cyc_Yyltype _T1463;unsigned _T1464;unsigned _T1465;struct Cyc_Absyn_Exp*_T1466;struct Cyc_Yystacktype*_T1467;union Cyc_YYSTYPE*_T1468;union Cyc_YYSTYPE*_T1469;struct _tuple8*_T146A;struct Cyc_Yystacktype*_T146B;struct Cyc_Yystacktype _T146C;struct Cyc_Yyltype _T146D;unsigned _T146E;unsigned _T146F;void*_T1470;struct Cyc_Yystacktype*_T1471;struct Cyc_Yystacktype _T1472;struct Cyc_Yyltype _T1473;unsigned _T1474;unsigned _T1475;struct Cyc_Absyn_Exp*_T1476;struct Cyc_Yystacktype*_T1477;union Cyc_YYSTYPE*_T1478;union Cyc_YYSTYPE*_T1479;struct Cyc_Absyn_Exp*_T147A;struct Cyc_Yystacktype*_T147B;struct Cyc_Yystacktype _T147C;struct Cyc_Yyltype _T147D;unsigned _T147E;unsigned _T147F;struct Cyc_Absyn_Exp*_T1480;struct Cyc_Yystacktype*_T1481;union Cyc_YYSTYPE*_T1482;union Cyc_YYSTYPE*_T1483;struct _tuple8*_T1484;struct Cyc_Yystacktype*_T1485;struct Cyc_Yystacktype _T1486;struct Cyc_Yyltype _T1487;unsigned _T1488;unsigned _T1489;void*_T148A;struct Cyc_Yystacktype*_T148B;union Cyc_YYSTYPE*_T148C;union Cyc_YYSTYPE*_T148D;struct Cyc_List_List*_T148E;struct Cyc_List_List*_T148F;struct Cyc_Yystacktype*_T1490;struct Cyc_Yystacktype _T1491;struct Cyc_Yyltype _T1492;unsigned _T1493;unsigned _T1494;struct Cyc_Absyn_Exp*_T1495;struct Cyc_Absyn_Malloc_e_Absyn_Raw_exp_struct*_T1496;struct Cyc_Yystacktype*_T1497;union Cyc_YYSTYPE*_T1498;union Cyc_YYSTYPE*_T1499;void*_T149A;struct Cyc_Yystacktype*_T149B;struct Cyc_Yystacktype _T149C;struct Cyc_Yyltype _T149D;unsigned _T149E;unsigned _T149F;struct Cyc_Absyn_Exp*_T14A0;struct Cyc_Absyn_Malloc_e_Absyn_Raw_exp_struct*_T14A1;struct Cyc_Yystacktype*_T14A2;union Cyc_YYSTYPE*_T14A3;union Cyc_YYSTYPE*_T14A4;struct Cyc_Yystacktype*_T14A5;union Cyc_YYSTYPE*_T14A6;union Cyc_YYSTYPE*_T14A7;struct Cyc_Yystacktype*_T14A8;union Cyc_YYSTYPE*_T14A9;union Cyc_YYSTYPE*_T14AA;void*_T14AB;struct Cyc_Yystacktype*_T14AC;struct Cyc_Yystacktype _T14AD;struct Cyc_Yyltype _T14AE;unsigned _T14AF;unsigned _T14B0;struct Cyc_Absyn_Exp*_T14B1;struct Cyc_Absyn_Malloc_e_Absyn_Raw_exp_struct*_T14B2;struct Cyc_Yystacktype*_T14B3;union Cyc_YYSTYPE*_T14B4;union Cyc_YYSTYPE*_T14B5;struct Cyc_Yystacktype*_T14B6;union Cyc_YYSTYPE*_T14B7;union Cyc_YYSTYPE*_T14B8;void*_T14B9;struct Cyc_Yystacktype*_T14BA;struct Cyc_Yystacktype _T14BB;struct Cyc_Yyltype _T14BC;unsigned _T14BD;unsigned _T14BE;struct Cyc_Absyn_Exp*_T14BF;struct Cyc_Yystacktype*_T14C0;union Cyc_YYSTYPE*_T14C1;union Cyc_YYSTYPE*_T14C2;struct Cyc_Absyn_Exp*_T14C3;struct Cyc_Absyn_Malloc_e_Absyn_Raw_exp_struct*_T14C4;struct Cyc_Yystacktype*_T14C5;union Cyc_YYSTYPE*_T14C6;union Cyc_YYSTYPE*_T14C7;void*_T14C8;struct Cyc_Yystacktype*_T14C9;struct Cyc_Yystacktype _T14CA;struct Cyc_Yyltype _T14CB;unsigned _T14CC;unsigned _T14CD;struct Cyc_Absyn_Exp*_T14CE;struct Cyc_Yystacktype*_T14CF;union Cyc_YYSTYPE*_T14D0;union Cyc_YYSTYPE*_T14D1;struct Cyc_Absyn_Exp*_T14D2;struct Cyc_Absyn_Malloc_e_Absyn_Raw_exp_struct*_T14D3;struct Cyc_Yystacktype*_T14D4;union Cyc_YYSTYPE*_T14D5;union Cyc_YYSTYPE*_T14D6;void*_T14D7;struct Cyc_Yystacktype*_T14D8;struct Cyc_Yystacktype _T14D9;struct Cyc_Yyltype _T14DA;unsigned _T14DB;unsigned _T14DC;struct Cyc_Absyn_Exp*_T14DD;struct Cyc_Yystacktype*_T14DE;union Cyc_YYSTYPE*_T14DF;union Cyc_YYSTYPE*_T14E0;struct _tuple8*_T14E1;struct Cyc_Yystacktype*_T14E2;struct Cyc_Yystacktype _T14E3;struct Cyc_Yyltype _T14E4;unsigned _T14E5;unsigned _T14E6;struct Cyc_Absyn_Malloc_e_Absyn_Raw_exp_struct*_T14E7;void**_T14E8;struct Cyc_Yystacktype*_T14E9;union Cyc_YYSTYPE*_T14EA;union Cyc_YYSTYPE*_T14EB;void*_T14EC;struct Cyc_Yystacktype*_T14ED;struct Cyc_Yystacktype _T14EE;struct Cyc_Yyltype _T14EF;unsigned _T14F0;unsigned _T14F1;struct Cyc_Absyn_Exp*_T14F2;struct Cyc_Yystacktype*_T14F3;union Cyc_YYSTYPE*_T14F4;union Cyc_YYSTYPE*_T14F5;struct _tuple8*_T14F6;struct Cyc_Yystacktype*_T14F7;struct Cyc_Yystacktype _T14F8;struct Cyc_Yyltype _T14F9;unsigned _T14FA;unsigned _T14FB;struct Cyc_Absyn_Malloc_e_Absyn_Raw_exp_struct*_T14FC;struct Cyc_Yystacktype*_T14FD;union Cyc_YYSTYPE*_T14FE;union Cyc_YYSTYPE*_T14FF;struct Cyc_Yystacktype*_T1500;union Cyc_YYSTYPE*_T1501;union Cyc_YYSTYPE*_T1502;void**_T1503;struct Cyc_Yystacktype*_T1504;union Cyc_YYSTYPE*_T1505;union Cyc_YYSTYPE*_T1506;void*_T1507;struct Cyc_Yystacktype*_T1508;struct Cyc_Yystacktype _T1509;struct Cyc_Yyltype _T150A;unsigned _T150B;unsigned _T150C;struct Cyc_Absyn_Exp*_T150D;struct Cyc_Yystacktype*_T150E;union Cyc_YYSTYPE*_T150F;union Cyc_YYSTYPE*_T1510;struct _tuple8*_T1511;struct Cyc_Yystacktype*_T1512;struct Cyc_Yystacktype _T1513;struct Cyc_Yyltype _T1514;unsigned _T1515;unsigned _T1516;struct Cyc_Absyn_Malloc_e_Absyn_Raw_exp_struct*_T1517;struct Cyc_Yystacktype*_T1518;union Cyc_YYSTYPE*_T1519;union Cyc_YYSTYPE*_T151A;void**_T151B;struct Cyc_Yystacktype*_T151C;union Cyc_YYSTYPE*_T151D;union Cyc_YYSTYPE*_T151E;void*_T151F;struct Cyc_Yystacktype*_T1520;struct Cyc_Yystacktype _T1521;struct Cyc_Yyltype _T1522;unsigned _T1523;unsigned _T1524;struct Cyc_Absyn_Exp*_T1525;struct Cyc_List_List*_T1526;struct Cyc_Yystacktype*_T1527;union Cyc_YYSTYPE*_T1528;union Cyc_YYSTYPE*_T1529;struct _fat_ptr _T152A;struct Cyc_Yystacktype*_T152B;struct Cyc_Yystacktype _T152C;struct Cyc_Yyltype _T152D;unsigned _T152E;unsigned _T152F;struct Cyc_Absyn_Exp*_T1530;struct Cyc_Yystacktype*_T1531;union Cyc_YYSTYPE*_T1532;union Cyc_YYSTYPE*_T1533;struct Cyc_Absyn_Exp*_T1534;struct Cyc_Yystacktype*_T1535;struct Cyc_Yystacktype _T1536;struct Cyc_Yyltype _T1537;unsigned _T1538;unsigned _T1539;struct Cyc_Absyn_Exp*_T153A;struct Cyc_Absyn_Tagcheck_e_Absyn_Raw_exp_struct*_T153B;struct Cyc_Yystacktype*_T153C;union Cyc_YYSTYPE*_T153D;union Cyc_YYSTYPE*_T153E;struct _fat_ptr*_T153F;struct Cyc_Yystacktype*_T1540;union Cyc_YYSTYPE*_T1541;union Cyc_YYSTYPE*_T1542;void*_T1543;struct Cyc_Yystacktype*_T1544;struct Cyc_Yystacktype _T1545;struct Cyc_Yyltype _T1546;unsigned _T1547;unsigned _T1548;struct Cyc_Absyn_Exp*_T1549;struct Cyc_Absyn_Tagcheck_e_Absyn_Raw_exp_struct*_T154A;struct Cyc_Yystacktype*_T154B;union Cyc_YYSTYPE*_T154C;union Cyc_YYSTYPE*_T154D;struct Cyc_Absyn_Exp*_T154E;struct Cyc_Yystacktype*_T154F;struct Cyc_Yystacktype _T1550;struct Cyc_Yyltype _T1551;unsigned _T1552;unsigned _T1553;struct _fat_ptr*_T1554;struct Cyc_Yystacktype*_T1555;union Cyc_YYSTYPE*_T1556;union Cyc_YYSTYPE*_T1557;void*_T1558;struct Cyc_Yystacktype*_T1559;struct Cyc_Yystacktype _T155A;struct Cyc_Yyltype _T155B;unsigned _T155C;unsigned _T155D;struct Cyc_Absyn_Exp*_T155E;struct Cyc_Yystacktype*_T155F;union Cyc_YYSTYPE*_T1560;union Cyc_YYSTYPE*_T1561;struct _tuple8*_T1562;struct Cyc_Yystacktype*_T1563;struct Cyc_Yystacktype _T1564;struct Cyc_Yyltype _T1565;unsigned _T1566;unsigned _T1567;void*_T1568;struct Cyc_Yystacktype*_T1569;struct Cyc_Yystacktype _T156A;struct Cyc_Yyltype _T156B;unsigned _T156C;unsigned _T156D;struct Cyc_Absyn_Exp*_T156E;struct Cyc_Yystacktype*_T156F;union Cyc_YYSTYPE*_T1570;union Cyc_YYSTYPE*_T1571;void*_T1572;struct Cyc_Yystacktype*_T1573;struct Cyc_Yystacktype _T1574;struct Cyc_Yyltype _T1575;unsigned _T1576;unsigned _T1577;struct Cyc_Absyn_Exp*_T1578;struct Cyc_Yystacktype*_T1579;union Cyc_YYSTYPE*_T157A;union Cyc_YYSTYPE*_T157B;struct Cyc_Absyn_Exp*_T157C;struct Cyc_Yystacktype*_T157D;struct Cyc_Yystacktype _T157E;struct Cyc_Yyltype _T157F;unsigned _T1580;unsigned _T1581;struct Cyc_Absyn_Exp*_T1582;struct Cyc_Yystacktype*_T1583;union Cyc_YYSTYPE*_T1584;union Cyc_YYSTYPE*_T1585;struct Cyc_Absyn_Exp*_T1586;struct Cyc_Yystacktype*_T1587;struct Cyc_Yystacktype _T1588;struct Cyc_Yyltype _T1589;unsigned _T158A;unsigned _T158B;struct Cyc_Absyn_Exp*_T158C;struct Cyc_Yystacktype*_T158D;union Cyc_YYSTYPE*_T158E;union Cyc_YYSTYPE*_T158F;struct Cyc_Absyn_Exp*_T1590;struct Cyc_Yystacktype*_T1591;struct Cyc_Yystacktype _T1592;struct Cyc_Yyltype _T1593;unsigned _T1594;unsigned _T1595;struct Cyc_Absyn_Exp*_T1596;struct Cyc_Yystacktype*_T1597;union Cyc_YYSTYPE*_T1598;union Cyc_YYSTYPE*_T1599;struct Cyc_Absyn_Exp*_T159A;struct Cyc_Yystacktype*_T159B;struct Cyc_Yystacktype _T159C;struct Cyc_Yyltype _T159D;unsigned _T159E;unsigned _T159F;struct Cyc_Absyn_Exp*_T15A0;struct Cyc_Yystacktype*_T15A1;union Cyc_YYSTYPE*_T15A2;union Cyc_YYSTYPE*_T15A3;struct Cyc_Absyn_Asm_e_Absyn_Raw_exp_struct*_T15A4;struct Cyc_Yystacktype*_T15A5;union Cyc_YYSTYPE*_T15A6;union Cyc_YYSTYPE*_T15A7;struct Cyc_Yystacktype*_T15A8;union Cyc_YYSTYPE*_T15A9;union Cyc_YYSTYPE*_T15AA;void*_T15AB;struct _tuple31*_T15AC;struct Cyc_Yystacktype*_T15AD;union Cyc_YYSTYPE*_T15AE;union Cyc_YYSTYPE*_T15AF;struct _tuple31*_T15B0;struct Cyc_Yystacktype*_T15B1;union Cyc_YYSTYPE*_T15B2;union Cyc_YYSTYPE*_T15B3;struct _tuple31*_T15B4;struct Cyc_Yystacktype*_T15B5;union Cyc_YYSTYPE*_T15B6;union Cyc_YYSTYPE*_T15B7;struct Cyc_List_List*_T15B8;struct Cyc_List_List*_T15B9;struct Cyc_Yystacktype*_T15BA;union Cyc_YYSTYPE*_T15BB;union Cyc_YYSTYPE*_T15BC;struct Cyc_List_List*_T15BD;struct Cyc_Yystacktype*_T15BE;union Cyc_YYSTYPE*_T15BF;union Cyc_YYSTYPE*_T15C0;struct Cyc_Yystacktype*_T15C1;union Cyc_YYSTYPE*_T15C2;union Cyc_YYSTYPE*_T15C3;struct _tuple28*_T15C4;struct _tuple28*_T15C5;struct Cyc_Yystacktype*_T15C6;union Cyc_YYSTYPE*_T15C7;union Cyc_YYSTYPE*_T15C8;struct _tuple28*_T15C9;struct Cyc_Yystacktype*_T15CA;union Cyc_YYSTYPE*_T15CB;union Cyc_YYSTYPE*_T15CC;struct Cyc_List_List*_T15CD;struct Cyc_Yystacktype*_T15CE;union Cyc_YYSTYPE*_T15CF;union Cyc_YYSTYPE*_T15D0;struct Cyc_List_List*_T15D1;struct Cyc_Yystacktype*_T15D2;union Cyc_YYSTYPE*_T15D3;union Cyc_YYSTYPE*_T15D4;struct Cyc_List_List*_T15D5;struct Cyc_Yystacktype*_T15D6;union Cyc_YYSTYPE*_T15D7;union Cyc_YYSTYPE*_T15D8;struct Cyc_Yystacktype*_T15D9;union Cyc_YYSTYPE*_T15DA;union Cyc_YYSTYPE*_T15DB;struct _tuple32*_T15DC;struct Cyc_Yystacktype*_T15DD;union Cyc_YYSTYPE*_T15DE;union Cyc_YYSTYPE*_T15DF;struct Cyc_Yystacktype*_T15E0;union Cyc_YYSTYPE*_T15E1;union Cyc_YYSTYPE*_T15E2;struct Cyc_Yystacktype*_T15E3;union Cyc_YYSTYPE*_T15E4;union Cyc_YYSTYPE*_T15E5;struct Cyc_List_List*_T15E6;struct Cyc_List_List*_T15E7;struct Cyc_List_List*_T15E8;struct _fat_ptr*_T15E9;struct Cyc_Yystacktype*_T15EA;union Cyc_YYSTYPE*_T15EB;union Cyc_YYSTYPE*_T15EC;struct Cyc_List_List*_T15ED;struct _fat_ptr*_T15EE;struct Cyc_Yystacktype*_T15EF;union Cyc_YYSTYPE*_T15F0;union Cyc_YYSTYPE*_T15F1;struct Cyc_Yystacktype*_T15F2;union Cyc_YYSTYPE*_T15F3;union Cyc_YYSTYPE*_T15F4;struct Cyc_Yystacktype*_T15F5;struct Cyc_Yystacktype _T15F6;struct Cyc_Yystacktype*_T15F7;union Cyc_YYSTYPE*_T15F8;union Cyc_YYSTYPE*_T15F9;struct Cyc_Absyn_Exp*_T15FA;struct Cyc_Yystacktype*_T15FB;union Cyc_YYSTYPE*_T15FC;union Cyc_YYSTYPE*_T15FD;struct Cyc_Absyn_Exp*_T15FE;struct Cyc_Yystacktype*_T15FF;struct Cyc_Yystacktype _T1600;struct Cyc_Yyltype _T1601;unsigned _T1602;unsigned _T1603;struct Cyc_Absyn_Exp*_T1604;struct Cyc_Yystacktype*_T1605;union Cyc_YYSTYPE*_T1606;union Cyc_YYSTYPE*_T1607;struct Cyc_Absyn_Exp*_T1608;struct Cyc_Yystacktype*_T1609;struct Cyc_Yystacktype _T160A;struct Cyc_Yyltype _T160B;unsigned _T160C;unsigned _T160D;struct Cyc_Absyn_Exp*_T160E;struct Cyc_Yystacktype*_T160F;union Cyc_YYSTYPE*_T1610;union Cyc_YYSTYPE*_T1611;struct Cyc_Absyn_Exp*_T1612;struct Cyc_Yystacktype*_T1613;union Cyc_YYSTYPE*_T1614;union Cyc_YYSTYPE*_T1615;struct Cyc_List_List*_T1616;struct Cyc_Yystacktype*_T1617;struct Cyc_Yystacktype _T1618;struct Cyc_Yyltype _T1619;unsigned _T161A;unsigned _T161B;struct Cyc_Absyn_Exp*_T161C;struct Cyc_Yystacktype*_T161D;union Cyc_YYSTYPE*_T161E;union Cyc_YYSTYPE*_T161F;struct Cyc_Absyn_Exp*_T1620;struct _fat_ptr*_T1621;struct Cyc_Yystacktype*_T1622;union Cyc_YYSTYPE*_T1623;union Cyc_YYSTYPE*_T1624;struct Cyc_Yystacktype*_T1625;struct Cyc_Yystacktype _T1626;struct Cyc_Yyltype _T1627;unsigned _T1628;unsigned _T1629;struct Cyc_Absyn_Exp*_T162A;struct Cyc_Yystacktype*_T162B;union Cyc_YYSTYPE*_T162C;union Cyc_YYSTYPE*_T162D;struct Cyc_Absyn_Exp*_T162E;struct _fat_ptr*_T162F;struct Cyc_Yystacktype*_T1630;union Cyc_YYSTYPE*_T1631;union Cyc_YYSTYPE*_T1632;struct Cyc_Yystacktype*_T1633;struct Cyc_Yystacktype _T1634;struct Cyc_Yyltype _T1635;unsigned _T1636;unsigned _T1637;struct Cyc_Absyn_Exp*_T1638;struct Cyc_Yystacktype*_T1639;union Cyc_YYSTYPE*_T163A;union Cyc_YYSTYPE*_T163B;struct Cyc_Absyn_Exp*_T163C;struct Cyc_Yystacktype*_T163D;struct Cyc_Yystacktype _T163E;struct Cyc_Yyltype _T163F;unsigned _T1640;unsigned _T1641;struct Cyc_Absyn_Exp*_T1642;struct Cyc_Yystacktype*_T1643;union Cyc_YYSTYPE*_T1644;union Cyc_YYSTYPE*_T1645;struct Cyc_Absyn_Exp*_T1646;struct Cyc_Yystacktype*_T1647;struct Cyc_Yystacktype _T1648;struct Cyc_Yyltype _T1649;unsigned _T164A;unsigned _T164B;struct Cyc_Absyn_Exp*_T164C;struct Cyc_Absyn_CompoundLit_e_Absyn_Raw_exp_struct*_T164D;struct Cyc_Yystacktype*_T164E;union Cyc_YYSTYPE*_T164F;union Cyc_YYSTYPE*_T1650;void*_T1651;struct Cyc_Yystacktype*_T1652;struct Cyc_Yystacktype _T1653;struct Cyc_Yyltype _T1654;unsigned _T1655;unsigned _T1656;struct Cyc_Absyn_Exp*_T1657;struct Cyc_Absyn_CompoundLit_e_Absyn_Raw_exp_struct*_T1658;struct Cyc_Yystacktype*_T1659;union Cyc_YYSTYPE*_T165A;union Cyc_YYSTYPE*_T165B;struct Cyc_Yystacktype*_T165C;union Cyc_YYSTYPE*_T165D;union Cyc_YYSTYPE*_T165E;struct Cyc_List_List*_T165F;void*_T1660;struct Cyc_Yystacktype*_T1661;struct Cyc_Yystacktype _T1662;struct Cyc_Yyltype _T1663;unsigned _T1664;unsigned _T1665;struct Cyc_Absyn_Exp*_T1666;struct Cyc_Absyn_CompoundLit_e_Absyn_Raw_exp_struct*_T1667;struct Cyc_Yystacktype*_T1668;union Cyc_YYSTYPE*_T1669;union Cyc_YYSTYPE*_T166A;struct Cyc_Yystacktype*_T166B;union Cyc_YYSTYPE*_T166C;union Cyc_YYSTYPE*_T166D;struct Cyc_List_List*_T166E;void*_T166F;struct Cyc_Yystacktype*_T1670;struct Cyc_Yystacktype _T1671;struct Cyc_Yyltype _T1672;unsigned _T1673;unsigned _T1674;struct Cyc_Absyn_Exp*_T1675;struct Cyc_List_List*_T1676;struct Cyc_Absyn_StructField_Absyn_OffsetofField_struct*_T1677;struct _fat_ptr*_T1678;struct Cyc_Yystacktype*_T1679;union Cyc_YYSTYPE*_T167A;union Cyc_YYSTYPE*_T167B;struct Cyc_Yystacktype*_T167C;struct Cyc_Yystacktype _T167D;struct Cyc_Yyltype _T167E;unsigned _T167F;unsigned _T1680;struct Cyc_Yystacktype*_T1681;union Cyc_YYSTYPE*_T1682;union Cyc_YYSTYPE*_T1683;union Cyc_Absyn_Cnst _T1684;unsigned _T1685;int _T1686;struct Cyc_List_List*_T1687;struct Cyc_Absyn_StructField_Absyn_OffsetofField_struct*_T1688;struct Cyc_List_List*_T1689;struct Cyc_Absyn_StructField_Absyn_OffsetofField_struct*_T168A;struct _fat_ptr*_T168B;struct Cyc_Yystacktype*_T168C;union Cyc_YYSTYPE*_T168D;union Cyc_YYSTYPE*_T168E;struct Cyc_Yystacktype*_T168F;union Cyc_YYSTYPE*_T1690;union Cyc_YYSTYPE*_T1691;struct Cyc_Yystacktype*_T1692;struct Cyc_Yystacktype _T1693;struct Cyc_Yyltype _T1694;unsigned _T1695;unsigned _T1696;struct Cyc_Yystacktype*_T1697;union Cyc_YYSTYPE*_T1698;union Cyc_YYSTYPE*_T1699;union Cyc_Absyn_Cnst _T169A;unsigned _T169B;int _T169C;struct Cyc_List_List*_T169D;struct Cyc_Absyn_StructField_Absyn_OffsetofField_struct*_T169E;struct Cyc_Yystacktype*_T169F;union Cyc_YYSTYPE*_T16A0;union Cyc_YYSTYPE*_T16A1;struct Cyc_Yystacktype*_T16A2;union Cyc_YYSTYPE*_T16A3;union Cyc_YYSTYPE*_T16A4;struct _tuple0*_T16A5;struct Cyc_Yystacktype*_T16A6;struct Cyc_Yystacktype _T16A7;struct Cyc_Yyltype _T16A8;unsigned _T16A9;unsigned _T16AA;struct Cyc_Absyn_Exp*_T16AB;struct Cyc_Yystacktype*_T16AC;union Cyc_YYSTYPE*_T16AD;union Cyc_YYSTYPE*_T16AE;struct _fat_ptr _T16AF;struct Cyc_Yystacktype*_T16B0;struct Cyc_Yystacktype _T16B1;struct Cyc_Yyltype _T16B2;unsigned _T16B3;unsigned _T16B4;struct Cyc_Absyn_Exp*_T16B5;struct Cyc_Yystacktype*_T16B6;struct Cyc_Yystacktype _T16B7;struct Cyc_Yystacktype*_T16B8;union Cyc_YYSTYPE*_T16B9;union Cyc_YYSTYPE*_T16BA;struct _fat_ptr _T16BB;struct Cyc_Yystacktype*_T16BC;struct Cyc_Yystacktype _T16BD;struct Cyc_Yyltype _T16BE;unsigned _T16BF;unsigned _T16C0;struct Cyc_Absyn_Exp*_T16C1;struct Cyc_Yystacktype*_T16C2;union Cyc_YYSTYPE*_T16C3;union Cyc_YYSTYPE*_T16C4;struct _fat_ptr _T16C5;struct Cyc_Yystacktype*_T16C6;struct Cyc_Yystacktype _T16C7;struct Cyc_Yyltype _T16C8;unsigned _T16C9;unsigned _T16CA;struct Cyc_Absyn_Exp*_T16CB;struct Cyc_Yystacktype*_T16CC;struct Cyc_Yystacktype _T16CD;struct Cyc_Yystacktype*_T16CE;union Cyc_YYSTYPE*_T16CF;union Cyc_YYSTYPE*_T16D0;struct Cyc_Absyn_Exp*_T16D1;struct Cyc_Yystacktype*_T16D2;struct Cyc_Yystacktype _T16D3;struct Cyc_Yyltype _T16D4;unsigned _T16D5;unsigned _T16D6;struct Cyc_Absyn_Exp*_T16D7;struct Cyc_Yystacktype*_T16D8;union Cyc_YYSTYPE*_T16D9;union Cyc_YYSTYPE*_T16DA;struct Cyc_Absyn_Exp*_T16DB;struct Cyc_Yystacktype*_T16DC;union Cyc_YYSTYPE*_T16DD;union Cyc_YYSTYPE*_T16DE;struct Cyc_List_List*_T16DF;struct Cyc_List_List*_T16E0;struct Cyc_Yystacktype*_T16E1;struct Cyc_Yystacktype _T16E2;struct Cyc_Yyltype _T16E3;unsigned _T16E4;unsigned _T16E5;struct Cyc_Absyn_Exp*_T16E6;struct Cyc_Absyn_Aggregate_e_Absyn_Raw_exp_struct*_T16E7;struct Cyc_Yystacktype*_T16E8;union Cyc_YYSTYPE*_T16E9;union Cyc_YYSTYPE*_T16EA;struct Cyc_Yystacktype*_T16EB;union Cyc_YYSTYPE*_T16EC;union Cyc_YYSTYPE*_T16ED;struct Cyc_Yystacktype*_T16EE;union Cyc_YYSTYPE*_T16EF;union Cyc_YYSTYPE*_T16F0;struct Cyc_List_List*_T16F1;void*_T16F2;struct Cyc_Yystacktype*_T16F3;struct Cyc_Yystacktype _T16F4;struct Cyc_Yyltype _T16F5;unsigned _T16F6;unsigned _T16F7;struct Cyc_Absyn_Exp*_T16F8;struct Cyc_Yystacktype*_T16F9;union Cyc_YYSTYPE*_T16FA;union Cyc_YYSTYPE*_T16FB;struct Cyc_List_List*_T16FC;struct Cyc_Yystacktype*_T16FD;struct Cyc_Yystacktype _T16FE;struct Cyc_Yyltype _T16FF;unsigned _T1700;unsigned _T1701;struct Cyc_Absyn_Exp*_T1702;struct Cyc_Yystacktype*_T1703;union Cyc_YYSTYPE*_T1704;union Cyc_YYSTYPE*_T1705;struct Cyc_Absyn_Stmt*_T1706;struct Cyc_Yystacktype*_T1707;struct Cyc_Yystacktype _T1708;struct Cyc_Yyltype _T1709;unsigned _T170A;unsigned _T170B;struct Cyc_Absyn_Exp*_T170C;struct Cyc_Yystacktype*_T170D;union Cyc_YYSTYPE*_T170E;union Cyc_YYSTYPE*_T170F;struct Cyc_List_List*_T1710;struct Cyc_List_List*_T1711;struct Cyc_List_List*_T1712;struct Cyc_Yystacktype*_T1713;union Cyc_YYSTYPE*_T1714;union Cyc_YYSTYPE*_T1715;struct Cyc_List_List*_T1716;struct Cyc_Yystacktype*_T1717;union Cyc_YYSTYPE*_T1718;union Cyc_YYSTYPE*_T1719;struct Cyc_Yystacktype*_T171A;union Cyc_YYSTYPE*_T171B;union Cyc_YYSTYPE*_T171C;struct Cyc_Yystacktype*_T171D;union Cyc_YYSTYPE*_T171E;union Cyc_YYSTYPE*_T171F;union Cyc_Absyn_Cnst _T1720;struct Cyc_Yystacktype*_T1721;struct Cyc_Yystacktype _T1722;struct Cyc_Yyltype _T1723;unsigned _T1724;unsigned _T1725;struct Cyc_Absyn_Exp*_T1726;struct Cyc_Yystacktype*_T1727;union Cyc_YYSTYPE*_T1728;union Cyc_YYSTYPE*_T1729;char _T172A;struct Cyc_Yystacktype*_T172B;struct Cyc_Yystacktype _T172C;struct Cyc_Yyltype _T172D;unsigned _T172E;unsigned _T172F;struct Cyc_Absyn_Exp*_T1730;struct Cyc_Yystacktype*_T1731;union Cyc_YYSTYPE*_T1732;union Cyc_YYSTYPE*_T1733;struct _fat_ptr _T1734;struct Cyc_Yystacktype*_T1735;struct Cyc_Yystacktype _T1736;struct Cyc_Yyltype _T1737;unsigned _T1738;unsigned _T1739;struct Cyc_Absyn_Exp*_T173A;struct Cyc_Yystacktype*_T173B;struct Cyc_Yystacktype _T173C;struct Cyc_Yyltype _T173D;unsigned _T173E;unsigned _T173F;struct Cyc_Absyn_Exp*_T1740;struct Cyc_Yystacktype*_T1741;union Cyc_YYSTYPE*_T1742;union Cyc_YYSTYPE*_T1743;unsigned long _T1744;struct _fat_ptr _T1745;unsigned char*_T1746;const char*_T1747;const char*_T1748;int _T1749;char _T174A;int _T174B;char _T174C;int _T174D;char _T174E;int _T174F;char _T1750;int _T1751;struct _fat_ptr _T1752;int _T1753;struct Cyc_Yystacktype*_T1754;struct Cyc_Yystacktype _T1755;struct Cyc_Yyltype _T1756;unsigned _T1757;unsigned _T1758;struct Cyc_Absyn_Exp*_T1759;struct _tuple0*_T175A;struct _fat_ptr*_T175B;struct Cyc_Yystacktype*_T175C;union Cyc_YYSTYPE*_T175D;union Cyc_YYSTYPE*_T175E;struct Cyc_Yystacktype*_T175F;struct Cyc_Yystacktype _T1760;struct _tuple0*_T1761;struct _fat_ptr*_T1762;struct Cyc_Yystacktype*_T1763;union Cyc_YYSTYPE*_T1764;union Cyc_YYSTYPE*_T1765;struct Cyc_Yystacktype*_T1766;struct Cyc_Yystacktype _T1767;struct Cyc_Yystacktype*_T1768;struct Cyc_Yystacktype _T1769;struct Cyc_Yystacktype*_T176A;struct Cyc_Yystacktype _T176B;struct Cyc_Yystacktype*_T176C;struct Cyc_Yystacktype _T176D;struct Cyc_Yystacktype*_T176E;struct Cyc_Yystacktype _T176F;struct Cyc_Lexing_lexbuf*_T1770;struct Cyc_List_List*_T1771;struct _tuple35*_T1772;struct Cyc_Yystacktype*_T1773;union Cyc_YYSTYPE*_T1774;union Cyc_YYSTYPE*_T1775;struct _fat_ptr _T1776;struct Cyc_Yystacktype*_T1777;union Cyc_YYSTYPE*_T1778;union Cyc_YYSTYPE*_T1779;struct _fat_ptr _T177A;struct _fat_ptr _T177B;int _T177C;int _T177D;struct Cyc_Yystacktype*_T177E;union Cyc_YYSTYPE*_T177F;union Cyc_YYSTYPE*_T1780;struct _fat_ptr _T1781;void*_T1782;struct Cyc_Yystacktype*_T1783;union Cyc_YYSTYPE*_T1784;union Cyc_YYSTYPE*_T1785;struct Cyc_Yystacktype*_T1786;union Cyc_YYSTYPE*_T1787;union Cyc_YYSTYPE*_T1788;struct Cyc_Yystacktype*_T1789;union Cyc_YYSTYPE*_T178A;union Cyc_YYSTYPE*_T178B;struct Cyc_List_List*_T178C;struct Cyc_List_List*_T178D;struct Cyc_Yystacktype*_T178E;union Cyc_YYSTYPE*_T178F;union Cyc_YYSTYPE*_T1790;struct Cyc_List_List*_T1791;struct Cyc_Yystacktype*_T1792;union Cyc_YYSTYPE*_T1793;union Cyc_YYSTYPE*_T1794;struct Cyc_Yystacktype*_T1795;union Cyc_YYSTYPE*_T1796;union Cyc_YYSTYPE*_T1797;struct Cyc_Yystacktype*_T1798;union Cyc_YYSTYPE*_T1799;union Cyc_YYSTYPE*_T179A;struct _fat_ptr _T179B;void*_T179C;struct Cyc_Yystacktype*_T179D;struct Cyc_Yystacktype _T179E;struct Cyc_Yyltype _T179F;unsigned _T17A0;unsigned _T17A1;struct Cyc_Yystacktype*_T17A2;union Cyc_YYSTYPE*_T17A3;union Cyc_YYSTYPE*_T17A4;struct _fat_ptr _T17A5;void*_T17A6;struct Cyc_Yystacktype*_T17A7;union Cyc_YYSTYPE*_T17A8;union Cyc_YYSTYPE*_T17A9;struct _fat_ptr _T17AA;struct Cyc_Yystacktype*_T17AB;union Cyc_YYSTYPE*_T17AC;union Cyc_YYSTYPE*_T17AD;void*_T17AE;void*_T17AF;void*_T17B0;struct Cyc_Yystacktype*_T17B1;union Cyc_YYSTYPE*_T17B2;union Cyc_YYSTYPE*_T17B3;struct _fat_ptr _T17B4;struct Cyc_Yystacktype*_T17B5;union Cyc_YYSTYPE*_T17B6;union Cyc_YYSTYPE*_T17B7;enum Cyc_Parse_ConstraintOps _T17B8;struct Cyc_Yystacktype*_T17B9;union Cyc_YYSTYPE*_T17BA;union Cyc_YYSTYPE*_T17BB;void*_T17BC;struct Cyc_Yystacktype*_T17BD;union Cyc_YYSTYPE*_T17BE;union Cyc_YYSTYPE*_T17BF;void*_T17C0;void*_T17C1;void*_T17C2;struct Cyc_Yystacktype*_T17C3;union Cyc_YYSTYPE*_T17C4;union Cyc_YYSTYPE*_T17C5;struct _fat_ptr _T17C6;struct Cyc_Yystacktype*_T17C7;union Cyc_YYSTYPE*_T17C8;union Cyc_YYSTYPE*_T17C9;void*_T17CA;struct Cyc_Yystacktype*_T17CB;union Cyc_YYSTYPE*_T17CC;union Cyc_YYSTYPE*_T17CD;void*_T17CE;void*_T17CF;void*_T17D0;struct Cyc_Yystacktype*_T17D1;union Cyc_YYSTYPE*_T17D2;union Cyc_YYSTYPE*_T17D3;struct _fat_ptr _T17D4;struct Cyc_Yystacktype*_T17D5;union Cyc_YYSTYPE*_T17D6;union Cyc_YYSTYPE*_T17D7;void*_T17D8;struct Cyc_Yystacktype*_T17D9;union Cyc_YYSTYPE*_T17DA;union Cyc_YYSTYPE*_T17DB;void*_T17DC;void*_T17DD;void*_T17DE;struct Cyc_Yystacktype*_T17DF;union Cyc_YYSTYPE*_T17E0;union Cyc_YYSTYPE*_T17E1;struct _fat_ptr _T17E2;struct Cyc_Yystacktype*_T17E3;union Cyc_YYSTYPE*_T17E4;union Cyc_YYSTYPE*_T17E5;void*_T17E6;struct Cyc_Yystacktype*_T17E7;union Cyc_YYSTYPE*_T17E8;union Cyc_YYSTYPE*_T17E9;void*_T17EA;void*_T17EB;void*_T17EC;struct Cyc_Yystacktype*_T17ED;union Cyc_YYSTYPE*_T17EE;union Cyc_YYSTYPE*_T17EF;struct _fat_ptr _T17F0;struct Cyc_Yystacktype*_T17F1;union Cyc_YYSTYPE*_T17F2;union Cyc_YYSTYPE*_T17F3;void*_T17F4;struct Cyc_Yystacktype*_T17F5;union Cyc_YYSTYPE*_T17F6;union Cyc_YYSTYPE*_T17F7;void*_T17F8;void*_T17F9;void*_T17FA;struct Cyc_Yystacktype*_T17FB;union Cyc_YYSTYPE*_T17FC;union Cyc_YYSTYPE*_T17FD;struct _fat_ptr _T17FE;struct Cyc_Yystacktype*_T17FF;union Cyc_YYSTYPE*_T1800;union Cyc_YYSTYPE*_T1801;enum Cyc_Parse_ConstraintOps _T1802;struct Cyc_Yystacktype*_T1803;union Cyc_YYSTYPE*_T1804;union Cyc_YYSTYPE*_T1805;void*_T1806;struct Cyc_Yystacktype*_T1807;union Cyc_YYSTYPE*_T1808;union Cyc_YYSTYPE*_T1809;void*_T180A;void*_T180B;void*_T180C;int _T180D;int _T180E;struct _fat_ptr _T180F;int _T1810;unsigned char*_T1811;struct Cyc_Yystacktype*_T1812;struct _fat_ptr _T1813;int _T1814;struct _fat_ptr _T1815;unsigned char*_T1816;unsigned char*_T1817;struct Cyc_Yystacktype*_T1818;struct Cyc_Yyltype _T1819;struct Cyc_Yystacktype*_T181A;struct Cyc_Yyltype _T181B;struct Cyc_Yystacktype*_T181C;struct Cyc_Yystacktype*_T181D;struct Cyc_Yystacktype _T181E;struct Cyc_Yyltype _T181F;struct Cyc_Yystacktype*_T1820;struct Cyc_Yystacktype*_T1821;struct Cyc_Yystacktype _T1822;struct Cyc_Yyltype _T1823;struct _fat_ptr _T1824;unsigned char*_T1825;struct Cyc_Yystacktype*_T1826;int _T1827;struct _fat_ptr _T1828;int _T1829;int _T182A;unsigned char*_T182B;struct Cyc_Yystacktype*_T182C;struct Cyc_Yystacktype _T182D;struct Cyc_Yyltype _T182E;struct _fat_ptr _T182F;unsigned char*_T1830;struct Cyc_Yystacktype*_T1831;int _T1832;struct _fat_ptr _T1833;unsigned char*_T1834;struct Cyc_Yystacktype*_T1835;int _T1836;int _T1837;struct Cyc_Yystacktype _T1838;struct Cyc_Yyltype _T1839;short*_T183A;int _T183B;char*_T183C;short*_T183D;short _T183E;short*_T183F;int _T1840;char*_T1841;short*_T1842;short _T1843;int _T1844;struct _fat_ptr _T1845;int _T1846;unsigned char*_T1847;short*_T1848;short _T1849;int _T184A;short*_T184B;int _T184C;short _T184D;int _T184E;struct _fat_ptr _T184F;unsigned char*_T1850;short*_T1851;int _T1852;short _T1853;int _T1854;short*_T1855;int _T1856;short _T1857;short*_T1858;int _T1859;short _T185A;short*_T185B;int _T185C;char*_T185D;short*_T185E;short _T185F;int _T1860;int _T1861;int _T1862;int _T1863;unsigned _T1864;unsigned _T1865;short*_T1866;int _T1867;char*_T1868;short*_T1869;short _T186A;int _T186B;int _T186C;struct _fat_ptr*_T186D;int _T186E;char*_T186F;struct _fat_ptr*_T1870;struct _fat_ptr _T1871;unsigned long _T1872;unsigned long _T1873;struct _fat_ptr _T1874;int _T1875;unsigned _T1876;char*_T1877;struct _RegionHandle*_T1878;unsigned _T1879;unsigned _T187A;struct _fat_ptr _T187B;struct _fat_ptr _T187C;int _T187D;int _T187E;unsigned _T187F;unsigned _T1880;short*_T1881;int _T1882;char*_T1883;short*_T1884;short _T1885;int _T1886;int _T1887;struct _fat_ptr _T1888;struct _fat_ptr _T1889;struct _fat_ptr _T188A;struct _fat_ptr _T188B;struct _fat_ptr _T188C;struct _fat_ptr*_T188D;int _T188E;char*_T188F;struct _fat_ptr*_T1890;struct _fat_ptr _T1891;struct _fat_ptr _T1892;struct _fat_ptr _T1893;struct _fat_ptr _T1894;int _T1895;int _T1896;struct _fat_ptr _T1897;int _T1898;unsigned char*_T1899;short*_T189A;short _T189B;short*_T189C;int _T189D;char*_T189E;short*_T189F;short _T18A0;int _T18A1;int _T18A2;short*_T18A3;int _T18A4;short _T18A5;int _T18A6;short*_T18A7;int _T18A8;short _T18A9;int _T18AA;int _T18AB;struct _fat_ptr _T18AC;int _T18AD;unsigned char*_T18AE;struct Cyc_Yystacktype*_T18AF;struct Cyc_Yystacktype _T18B0;struct _RegionHandle _T18B1=_new_region(0U,"yyregion");struct _RegionHandle*yyregion=& _T18B1;_push_region(yyregion);{
# 149
int yystate;
int yyn=0;
int yyerrstatus;
int yychar1=0;
# 154
int yychar;{union Cyc_YYSTYPE _T18B2;_T18B2.YYINITIALSVAL.tag=1U;
_T18B2.YYINITIALSVAL.val=0;_T0=_T18B2;}{union Cyc_YYSTYPE yylval=_T0;
int yynerrs;
# 158
struct Cyc_Yyltype yylloc;
# 162
int yyssp_offset;{unsigned _T18B2=200U;_T2=yyregion;_T3=_region_calloc(_T2,0U,sizeof(short),_T18B2);_T1=_tag_fat(_T3,sizeof(short),_T18B2);}{
# 164
struct _fat_ptr yyss=_T1;
# 166
int yyvsp_offset;{unsigned _T18B2=200U;_T6=yyregion;_T7=_check_times(_T18B2,sizeof(struct Cyc_Yystacktype));{struct Cyc_Yystacktype*_T18B3=_region_malloc(_T6,0U,_T7);{unsigned _T18B4=_T18B2;unsigned i;i=0;_TL250: if(i < _T18B4)goto _TL24E;else{goto _TL24F;}_TL24E: _T8=i;
# 169
_T18B3[_T8].v=yylval;_T9=i;_T18B3[_T9].l=Cyc_yynewloc();i=i + 1;goto _TL250;_TL24F:;}_T5=(struct Cyc_Yystacktype*)_T18B3;}_T4=_T5;}{
# 168
struct _fat_ptr yyvs=
_tag_fat(_T4,sizeof(struct Cyc_Yystacktype),200U);
# 174
struct Cyc_Yystacktype*yyyvsp;
# 177
int yystacksize=200;
# 179
union Cyc_YYSTYPE yyval=yylval;
# 183
int yylen;
# 190
yystate=0;
yyerrstatus=0;
yynerrs=0;
yychar=- 2;
# 200
yyssp_offset=- 1;
yyvsp_offset=0;
# 206
yynewstate: _TA=yyss;
# 208
yyssp_offset=yyssp_offset + 1;_TB=yyssp_offset;_TC=_check_fat_subscript(_TA,sizeof(short),_TB);_TD=(short*)_TC;_TE=yystate;*_TD=(short)_TE;_TF=yyssp_offset;_T10=yystacksize - 1;_T11=_T10 - 14;
# 210
if(_TF < _T11)goto _TL251;
# 212
if(yystacksize < 30000)goto _TL253;_T12=
_tag_fat("parser stack overflow",sizeof(char),22U);_T13=yystate;_T14=yychar;Cyc_yyerror(_T12,_T13,_T14);_T15=& Cyc_Yystack_overflow_val;_T16=(struct Cyc_Yystack_overflow_exn_struct*)_T15;_throw(_T16);goto _TL254;_TL253: _TL254:
# 216
 yystacksize=yystacksize * 2;
if(yystacksize <= 30000)goto _TL255;
yystacksize=30000;goto _TL256;_TL255: _TL256: _T18=yystacksize;{unsigned _T18B2=(unsigned)_T18;_T1A=yyregion;_T1B=_check_times(_T18B2,sizeof(short));{short*_T18B3=_region_malloc(_T1A,0U,_T1B);{unsigned _T18B4=_T18B2;unsigned i;i=0;_TL25A: if(i < _T18B4)goto _TL258;else{goto _TL259;}_TL258: _T1C=i;_T1D=yyssp_offset;_T1E=(unsigned)_T1D;
# 220
if(_T1C > _T1E)goto _TL25B;_T1F=i;_T20=yyss;_T21=_T20.curr;_T22=(short*)_T21;_T23=i;_T24=(int)_T23;_T18B3[_T1F]=_T22[_T24];goto _TL25C;_TL25B: _T25=i;_T18B3[_T25]=0;_TL25C: i=i + 1;goto _TL25A;_TL259:;}_T19=(short*)_T18B3;}_T17=
# 219
_tag_fat(_T19,sizeof(short),_T18B2);}{struct _fat_ptr yyss1=_T17;_T27=yystacksize;{unsigned _T18B2=(unsigned)_T27;_T29=yyregion;_T2A=_check_times(_T18B2,sizeof(struct Cyc_Yystacktype));{struct Cyc_Yystacktype*_T18B3=_region_malloc(_T29,0U,_T2A);{unsigned _T18B4=_T18B2;unsigned i;i=0;_TL260: if(i < _T18B4)goto _TL25E;else{goto _TL25F;}_TL25E: _T2B=i;_T2C=yyssp_offset;_T2D=(unsigned)_T2C;
# 224
if(_T2B > _T2D)goto _TL261;_T2E=i;_T2F=yyvs;_T30=i;_T31=(int)_T30;_T32=_check_fat_subscript(_T2F,sizeof(struct Cyc_Yystacktype),_T31);_T33=(struct Cyc_Yystacktype*)_T32;_T18B3[_T2E]=*_T33;goto _TL262;_TL261: _T34=i;_T35=yyvs;_T36=_check_fat_subscript(_T35,sizeof(struct Cyc_Yystacktype),0);_T37=(struct Cyc_Yystacktype*)_T36;_T18B3[_T34]=*_T37;_TL262: i=i + 1;goto _TL260;_TL25F:;}_T28=(struct Cyc_Yystacktype*)_T18B3;}_T26=
# 223
_tag_fat(_T28,sizeof(struct Cyc_Yystacktype),_T18B2);}{
# 222
struct _fat_ptr yyvs1=_T26;
# 230
yyss=yyss1;
yyvs=yyvs1;}}goto _TL252;_TL251: _TL252: goto yybackup;
# 242
yybackup: _T38=Cyc_yypact;_T39=yystate;_T3A=_check_known_subscript_notnull(_T38,1270U,sizeof(short),_T39);_T3B=(short*)_T3A;_T3C=*_T3B;
# 254 "cycbison.simple"
yyn=(int)_T3C;_T3D=yyn;_T3E=- 32768;
if(_T3D!=_T3E)goto _TL263;goto yydefault;_TL263: _T3F=yychar;_T40=- 2;
# 261
if(_T3F!=_T40)goto _TL265;_T41=yylex_buf;_T42=& yylval;_T43=(union Cyc_YYSTYPE*)_T42;_T44=& yylloc;_T45=(struct Cyc_Yyltype*)_T44;
# 267
yychar=Cyc_yylex(_T41,_T43,_T45);goto _TL266;_TL265: _TL266:
# 271
 if(yychar > 0)goto _TL267;
# 273
yychar1=0;
yychar=0;goto _TL268;
# 282
_TL267: if(yychar <= 0)goto _TL269;if(yychar > 402)goto _TL269;_T47=Cyc_yytranslate;_T48=yychar;_T49=_T47[_T48];_T46=(int)_T49;goto _TL26A;_TL269: _T46=365;_TL26A: yychar1=_T46;_TL268: _T4A=yychar1;
# 299 "cycbison.simple"
yyn=yyn + _T4A;
if(yyn < 0)goto _TL26D;else{goto _TL26F;}_TL26F: if(yyn > 8407)goto _TL26D;else{goto _TL26E;}_TL26E: _T4B=Cyc_yycheck;_T4C=yyn;_T4D=_T4B[_T4C];_T4E=(int)_T4D;_T4F=yychar1;if(_T4E!=_T4F)goto _TL26D;else{goto _TL26B;}_TL26D: goto yydefault;_TL26B: _T50=Cyc_yytable;_T51=yyn;_T52=_T50[_T51];
# 302
yyn=(int)_T52;
# 309
if(yyn >= 0)goto _TL270;_T53=yyn;_T54=- 32768;
# 311
if(_T53!=_T54)goto _TL272;goto yyerrlab;_TL272:
 yyn=- yyn;goto yyreduce;
# 315
_TL270: if(yyn!=0)goto _TL274;goto yyerrlab;_TL274:
# 317
 if(yyn!=1269)goto _TL276;{int _T18B2=0;_npop_handler(0);return _T18B2;}_TL276:
# 328 "cycbison.simple"
 if(yychar==0)goto _TL278;
yychar=- 2;goto _TL279;_TL278: _TL279: _T55=yyvs;
# 332
yyvsp_offset=yyvsp_offset + 1;_T56=yyvsp_offset;_T57=_check_fat_subscript(_T55,sizeof(struct Cyc_Yystacktype),_T56);_T58=(struct Cyc_Yystacktype*)_T57;{struct Cyc_Yystacktype _T18B2;_T18B2.v=yylval;_T18B2.l=yylloc;_T59=_T18B2;}*_T58=_T59;
# 338
if(yyerrstatus==0)goto _TL27A;yyerrstatus=yyerrstatus + -1;goto _TL27B;_TL27A: _TL27B:
# 340
 yystate=yyn;goto yynewstate;
# 344
yydefault: _T5A=Cyc_yydefact;_T5B=yystate;_T5C=_check_known_subscript_notnull(_T5A,1270U,sizeof(short),_T5B);_T5D=(short*)_T5C;_T5E=*_T5D;
# 346
yyn=(int)_T5E;
if(yyn!=0)goto _TL27C;goto yyerrlab;_TL27C:
# 351
 yyreduce: _T5F=Cyc_yyr2;_T60=yyn;_T61=_check_known_subscript_notnull(_T5F,626U,sizeof(short),_T60);_T62=(short*)_T61;_T63=*_T62;
# 353
yylen=(int)_T63;_T64=yyvs;_T65=yyvsp_offset + 1;_T66=yylen;_T67=_T65 - _T66;_T68=
_fat_ptr_plus(_T64,sizeof(struct Cyc_Yystacktype),_T67);_T69=_untag_fat_ptr_check_bound(_T68,sizeof(struct Cyc_Yystacktype),14U);_T6A=_check_null(_T69);yyyvsp=(struct Cyc_Yystacktype*)_T6A;
if(yylen <= 0)goto _TL27E;_T6B=yyyvsp;_T6C=_T6B[0];
yyval=_T6C.v;goto _TL27F;_TL27E: _TL27F: _T6D=yyn;_T6E=(int)_T6D;switch(_T6E){case 1:
# 1324 "parse.y"
 yyval=Cyc_YY74(0);goto _LL0;case 2:
# 1327 "parse.y"
 yyval=Cyc_YY74(1);_T6F=yyyvsp;_T70=& _T6F[0].v;_T71=(union Cyc_YYSTYPE*)_T70;
Cyc_Parse_constraint_graph=Cyc_yyget_YY71(_T71);goto _LL0;case 3: _T72=yyyvsp;_T73=_T72[0];
# 1334 "parse.y"
yyval=_T73.v;_T74=yyyvsp;_T75=& _T74[0].v;_T76=(union Cyc_YYSTYPE*)_T75;
Cyc_Parse_parse_result=Cyc_yyget_YY16(_T76);goto _LL0;case 4: _T77=yyyvsp;_T78=& _T77[0].v;_T79=(union Cyc_YYSTYPE*)_T78;_T7A=
# 1341 "parse.y"
Cyc_yyget_YY16(_T79);_T7B=yyyvsp;_T7C=& _T7B[1].v;_T7D=(union Cyc_YYSTYPE*)_T7C;_T7E=Cyc_yyget_YY16(_T7D);_T7F=Cyc_List_imp_append(_T7A,_T7E);yyval=Cyc_YY16(_T7F);goto _LL0;case 5:{struct Cyc_List_List*_T18B2=_cycalloc(sizeof(struct Cyc_List_List));{struct Cyc_Absyn_Decl*_T18B3=_cycalloc(sizeof(struct Cyc_Absyn_Decl));{struct Cyc_Absyn_Using_d_Absyn_Raw_decl_struct*_T18B4=_cycalloc(sizeof(struct Cyc_Absyn_Using_d_Absyn_Raw_decl_struct));_T18B4->tag=10;_T83=yyyvsp;_T84=& _T83[0].v;_T85=(union Cyc_YYSTYPE*)_T84;
# 1345 "parse.y"
_T18B4->f1=Cyc_yyget_QualId_tok(_T85);_T86=yyyvsp;_T87=& _T86[2].v;_T88=(union Cyc_YYSTYPE*)_T87;_T18B4->f2=Cyc_yyget_YY16(_T88);_T82=(struct Cyc_Absyn_Using_d_Absyn_Raw_decl_struct*)_T18B4;}_T18B3->r=(void*)_T82;_T89=yyyvsp;_T8A=_T89[0];_T8B=_T8A.l;_T8C=_T8B.first_line;_T18B3->loc=Cyc_Position_loc_to_seg(_T8C);_T81=(struct Cyc_Absyn_Decl*)_T18B3;}_T18B2->hd=_T81;_T18B2->tl=0;_T80=(struct Cyc_List_List*)_T18B2;}yyval=Cyc_YY16(_T80);
Cyc_Lex_leave_using();goto _LL0;case 6:{struct Cyc_List_List*_T18B2=_cycalloc(sizeof(struct Cyc_List_List));{struct Cyc_Absyn_Decl*_T18B3=_cycalloc(sizeof(struct Cyc_Absyn_Decl));{struct Cyc_Absyn_Using_d_Absyn_Raw_decl_struct*_T18B4=_cycalloc(sizeof(struct Cyc_Absyn_Using_d_Absyn_Raw_decl_struct));_T18B4->tag=10;_T90=yyyvsp;_T91=& _T90[0].v;_T92=(union Cyc_YYSTYPE*)_T91;
# 1349 "parse.y"
_T18B4->f1=Cyc_yyget_QualId_tok(_T92);_T93=yyyvsp;_T94=& _T93[2].v;_T95=(union Cyc_YYSTYPE*)_T94;_T18B4->f2=Cyc_yyget_YY16(_T95);_T8F=(struct Cyc_Absyn_Using_d_Absyn_Raw_decl_struct*)_T18B4;}_T18B3->r=(void*)_T8F;_T96=yyyvsp;_T97=_T96[0];_T98=_T97.l;_T99=_T98.first_line;_T18B3->loc=Cyc_Position_loc_to_seg(_T99);_T8E=(struct Cyc_Absyn_Decl*)_T18B3;}_T18B2->hd=_T8E;_T9A=yyyvsp;_T9B=& _T9A[4].v;_T9C=(union Cyc_YYSTYPE*)_T9B;_T18B2->tl=Cyc_yyget_YY16(_T9C);_T8D=(struct Cyc_List_List*)_T18B2;}yyval=Cyc_YY16(_T8D);goto _LL0;case 7:{struct Cyc_List_List*_T18B2=_cycalloc(sizeof(struct Cyc_List_List));{struct Cyc_Absyn_Decl*_T18B3=_cycalloc(sizeof(struct Cyc_Absyn_Decl));{struct Cyc_Absyn_Namespace_d_Absyn_Raw_decl_struct*_T18B4=_cycalloc(sizeof(struct Cyc_Absyn_Namespace_d_Absyn_Raw_decl_struct));_T18B4->tag=9;{struct _fat_ptr*_T18B5=_cycalloc(sizeof(struct _fat_ptr));_TA1=yyyvsp;_TA2=& _TA1[0].v;_TA3=(union Cyc_YYSTYPE*)_TA2;
# 1352
*_T18B5=Cyc_yyget_String_tok(_TA3);_TA0=(struct _fat_ptr*)_T18B5;}_T18B4->f1=_TA0;_TA4=yyyvsp;_TA5=& _TA4[2].v;_TA6=(union Cyc_YYSTYPE*)_TA5;_T18B4->f2=Cyc_yyget_YY16(_TA6);_T9F=(struct Cyc_Absyn_Namespace_d_Absyn_Raw_decl_struct*)_T18B4;}_T18B3->r=(void*)_T9F;_TA7=yyyvsp;_TA8=_TA7[0];_TA9=_TA8.l;_TAA=_TA9.first_line;_T18B3->loc=Cyc_Position_loc_to_seg(_TAA);_T9E=(struct Cyc_Absyn_Decl*)_T18B3;}_T18B2->hd=_T9E;_T18B2->tl=0;_T9D=(struct Cyc_List_List*)_T18B2;}yyval=Cyc_YY16(_T9D);
Cyc_Lex_leave_namespace();goto _LL0;case 8:{struct Cyc_List_List*_T18B2=_cycalloc(sizeof(struct Cyc_List_List));{struct Cyc_Absyn_Decl*_T18B3=_cycalloc(sizeof(struct Cyc_Absyn_Decl));{struct Cyc_Absyn_Namespace_d_Absyn_Raw_decl_struct*_T18B4=_cycalloc(sizeof(struct Cyc_Absyn_Namespace_d_Absyn_Raw_decl_struct));_T18B4->tag=9;{struct _fat_ptr*_T18B5=_cycalloc(sizeof(struct _fat_ptr));_TAF=yyyvsp;_TB0=& _TAF[0].v;_TB1=(union Cyc_YYSTYPE*)_TB0;
# 1356 "parse.y"
*_T18B5=Cyc_yyget_String_tok(_TB1);_TAE=(struct _fat_ptr*)_T18B5;}_T18B4->f1=_TAE;_TB2=yyyvsp;_TB3=& _TB2[2].v;_TB4=(union Cyc_YYSTYPE*)_TB3;_T18B4->f2=Cyc_yyget_YY16(_TB4);_TAD=(struct Cyc_Absyn_Namespace_d_Absyn_Raw_decl_struct*)_T18B4;}_T18B3->r=(void*)_TAD;_TB5=yyyvsp;_TB6=_TB5[0];_TB7=_TB6.l;_TB8=_TB7.first_line;_T18B3->loc=Cyc_Position_loc_to_seg(_TB8);_TAC=(struct Cyc_Absyn_Decl*)_T18B3;}_T18B2->hd=_TAC;_TB9=yyyvsp;_TBA=& _TB9[4].v;_TBB=(union Cyc_YYSTYPE*)_TBA;_T18B2->tl=Cyc_yyget_YY16(_TBB);_TAB=(struct Cyc_List_List*)_T18B2;}yyval=Cyc_YY16(_TAB);goto _LL0;case 9: _TBC=yyyvsp;_TBD=& _TBC[2].v;_TBE=(union Cyc_YYSTYPE*)_TBD;_TBF=
# 1358 "parse.y"
Cyc_yyget_YY16(_TBE);_TC0=yyyvsp;_TC1=& _TC0[4].v;_TC2=(union Cyc_YYSTYPE*)_TC1;_TC3=Cyc_yyget_YY16(_TC2);_TC4=Cyc_List_append(_TBF,_TC3);yyval=Cyc_YY16(_TC4);goto _LL0;case 10: _TC5=yyyvsp;_TC6=& _TC5[0].v;_TC7=(union Cyc_YYSTYPE*)_TC6;{
# 1360 "parse.y"
int is_c_include=Cyc_yyget_YY32(_TC7);_TC8=yyyvsp;_TC9=& _TC8[4].v;_TCA=(union Cyc_YYSTYPE*)_TC9;{
struct Cyc_List_List*cycdecls=Cyc_yyget_YY16(_TCA);_TCB=yyyvsp;_TCC=& _TCB[5].v;_TCD=(union Cyc_YYSTYPE*)_TCC;{
struct _tuple30*_T18B2=Cyc_yyget_YY56(_TCD);unsigned _T18B3;struct Cyc_List_List*_T18B4;{struct _tuple30 _T18B5=*_T18B2;_T18B4=_T18B5.f0;_T18B3=_T18B5.f1;}{struct Cyc_List_List*exs=_T18B4;unsigned wc=_T18B3;_TCE=yyyvsp;_TCF=& _TCE[6].v;_TD0=(union Cyc_YYSTYPE*)_TCF;{
struct Cyc_List_List*hides=Cyc_yyget_YY57(_TD0);
if(exs==0)goto _TL281;if(hides==0)goto _TL281;_TD1=yyyvsp;_TD2=_TD1[0];_TD3=_TD2.l;_TD4=_TD3.first_line;_TD5=
Cyc_Position_loc_to_seg(_TD4);_TD6=_tag_fat("hide list can only be used with export { * }, or no export block",sizeof(char),65U);_TD7=_tag_fat(0U,sizeof(void*),0);Cyc_Warn_err(_TD5,_TD6,_TD7);goto _TL282;_TL281: _TL282: _TD8=hides;_TD9=(unsigned)_TD8;
if(!_TD9)goto _TL283;_TDA=wc;_TDB=(int)_TDA;if(_TDB)goto _TL283;else{goto _TL285;}
_TL285: _TDC=yyyvsp;_TDD=_TDC[6];_TDE=_TDD.l;_TDF=_TDE.first_line;wc=Cyc_Position_loc_to_seg(_TDF);goto _TL284;_TL283: _TL284: _TE0=is_c_include;
if(_TE0)goto _TL286;else{goto _TL288;}
_TL288: if(exs!=0)goto _TL28B;else{goto _TL28C;}_TL28C: if(cycdecls!=0)goto _TL28B;else{goto _TL289;}
_TL28B: _TE1=yyyvsp;_TE2=_TE1[0];_TE3=_TE2.l;_TE4=_TE3.first_line;_TE5=Cyc_Position_loc_to_seg(_TE4);_TE6=_tag_fat("expecting \"C include\"",sizeof(char),22U);_TE7=_tag_fat(0U,sizeof(void*),0);Cyc_Warn_err(_TE5,_TE6,_TE7);{struct Cyc_List_List*_T18B5=_cycalloc(sizeof(struct Cyc_List_List));{struct Cyc_Absyn_Decl*_T18B6=_cycalloc(sizeof(struct Cyc_Absyn_Decl));{struct Cyc_Absyn_ExternCinclude_d_Absyn_Raw_decl_struct*_T18B7=_cycalloc(sizeof(struct Cyc_Absyn_ExternCinclude_d_Absyn_Raw_decl_struct));_T18B7->tag=12;_TEB=yyyvsp;_TEC=& _TEB[2].v;_TED=(union Cyc_YYSTYPE*)_TEC;
_T18B7->f1=Cyc_yyget_YY16(_TED);_T18B7->f2=cycdecls;_T18B7->f3=exs;{struct _tuple10*_T18B8=_cycalloc(sizeof(struct _tuple10));_T18B8->f0=wc;_T18B8->f1=hides;_TEE=(struct _tuple10*)_T18B8;}_T18B7->f4=_TEE;_TEA=(struct Cyc_Absyn_ExternCinclude_d_Absyn_Raw_decl_struct*)_T18B7;}_T18B6->r=(void*)_TEA;_TEF=yyyvsp;_TF0=_TEF[0];_TF1=_TF0.l;_TF2=_TF1.first_line;_T18B6->loc=Cyc_Position_loc_to_seg(_TF2);_TE9=(struct Cyc_Absyn_Decl*)_T18B6;}_T18B5->hd=_TE9;_TF3=yyyvsp;_TF4=& _TF3[7].v;_TF5=(union Cyc_YYSTYPE*)_TF4;_T18B5->tl=Cyc_yyget_YY16(_TF5);_TE8=(struct Cyc_List_List*)_T18B5;}yyval=Cyc_YY16(_TE8);goto _TL28A;
# 1374
_TL289:{struct Cyc_List_List*_T18B5=_cycalloc(sizeof(struct Cyc_List_List));{struct Cyc_Absyn_Decl*_T18B6=_cycalloc(sizeof(struct Cyc_Absyn_Decl));{struct Cyc_Absyn_ExternC_d_Absyn_Raw_decl_struct*_T18B7=_cycalloc(sizeof(struct Cyc_Absyn_ExternC_d_Absyn_Raw_decl_struct));_T18B7->tag=11;_TF9=yyyvsp;_TFA=& _TF9[2].v;_TFB=(union Cyc_YYSTYPE*)_TFA;_T18B7->f1=Cyc_yyget_YY16(_TFB);_TF8=(struct Cyc_Absyn_ExternC_d_Absyn_Raw_decl_struct*)_T18B7;}_T18B6->r=(void*)_TF8;_TFC=yyyvsp;_TFD=_TFC[0];_TFE=_TFD.l;_TFF=_TFE.first_line;_T18B6->loc=Cyc_Position_loc_to_seg(_TFF);_TF7=(struct Cyc_Absyn_Decl*)_T18B6;}_T18B5->hd=_TF7;_T100=yyyvsp;_T101=& _T100[7].v;_T102=(union Cyc_YYSTYPE*)_T101;_T18B5->tl=Cyc_yyget_YY16(_T102);_TF6=(struct Cyc_List_List*)_T18B5;}yyval=Cyc_YY16(_TF6);_TL28A: goto _TL287;
# 1377
_TL286:{struct Cyc_List_List*_T18B5=_cycalloc(sizeof(struct Cyc_List_List));{struct Cyc_Absyn_Decl*_T18B6=_cycalloc(sizeof(struct Cyc_Absyn_Decl));{struct Cyc_Absyn_ExternCinclude_d_Absyn_Raw_decl_struct*_T18B7=_cycalloc(sizeof(struct Cyc_Absyn_ExternCinclude_d_Absyn_Raw_decl_struct));_T18B7->tag=12;_T106=yyyvsp;_T107=& _T106[2].v;_T108=(union Cyc_YYSTYPE*)_T107;_T18B7->f1=Cyc_yyget_YY16(_T108);_T18B7->f2=cycdecls;_T18B7->f3=exs;{struct _tuple10*_T18B8=_cycalloc(sizeof(struct _tuple10));_T18B8->f0=wc;_T18B8->f1=hides;_T109=(struct _tuple10*)_T18B8;}_T18B7->f4=_T109;_T105=(struct Cyc_Absyn_ExternCinclude_d_Absyn_Raw_decl_struct*)_T18B7;}_T18B6->r=(void*)_T105;_T10A=yyyvsp;_T10B=_T10A[0];_T10C=_T10B.l;_T10D=_T10C.first_line;_T18B6->loc=Cyc_Position_loc_to_seg(_T10D);_T104=(struct Cyc_Absyn_Decl*)_T18B6;}_T18B5->hd=_T104;_T10E=yyyvsp;_T10F=& _T10E[7].v;_T110=(union Cyc_YYSTYPE*)_T10F;_T18B5->tl=Cyc_yyget_YY16(_T110);_T103=(struct Cyc_List_List*)_T18B5;}yyval=Cyc_YY16(_T103);_TL287: goto _LL0;}}}}}case 11:{struct Cyc_List_List*_T18B2=_cycalloc(sizeof(struct Cyc_List_List));{struct Cyc_Absyn_Decl*_T18B3=_cycalloc(sizeof(struct Cyc_Absyn_Decl));_T113=& Cyc_Absyn_Porton_d_val;_T114=(struct Cyc_Absyn_Porton_d_Absyn_Raw_decl_struct*)_T113;
# 1380 "parse.y"
_T18B3->r=(void*)_T114;_T115=yyyvsp;_T116=_T115[0];_T117=_T116.l;_T118=_T117.first_line;_T18B3->loc=Cyc_Position_loc_to_seg(_T118);_T112=(struct Cyc_Absyn_Decl*)_T18B3;}_T18B2->hd=_T112;_T119=yyyvsp;_T11A=& _T119[2].v;_T11B=(union Cyc_YYSTYPE*)_T11A;_T18B2->tl=Cyc_yyget_YY16(_T11B);_T111=(struct Cyc_List_List*)_T18B2;}yyval=Cyc_YY16(_T111);goto _LL0;case 12:{struct Cyc_List_List*_T18B2=_cycalloc(sizeof(struct Cyc_List_List));{struct Cyc_Absyn_Decl*_T18B3=_cycalloc(sizeof(struct Cyc_Absyn_Decl));_T11E=& Cyc_Absyn_Portoff_d_val;_T11F=(struct Cyc_Absyn_Portoff_d_Absyn_Raw_decl_struct*)_T11E;
# 1382 "parse.y"
_T18B3->r=(void*)_T11F;_T120=yyyvsp;_T121=_T120[0];_T122=_T121.l;_T123=_T122.first_line;_T18B3->loc=Cyc_Position_loc_to_seg(_T123);_T11D=(struct Cyc_Absyn_Decl*)_T18B3;}_T18B2->hd=_T11D;_T124=yyyvsp;_T125=& _T124[2].v;_T126=(union Cyc_YYSTYPE*)_T125;_T18B2->tl=Cyc_yyget_YY16(_T126);_T11C=(struct Cyc_List_List*)_T18B2;}yyval=Cyc_YY16(_T11C);goto _LL0;case 13:{struct Cyc_List_List*_T18B2=_cycalloc(sizeof(struct Cyc_List_List));{struct Cyc_Absyn_Decl*_T18B3=_cycalloc(sizeof(struct Cyc_Absyn_Decl));_T129=& Cyc_Absyn_Tempeston_d_val;_T12A=(struct Cyc_Absyn_Tempeston_d_Absyn_Raw_decl_struct*)_T129;
# 1384 "parse.y"
_T18B3->r=(void*)_T12A;_T12B=yyyvsp;_T12C=_T12B[0];_T12D=_T12C.l;_T12E=_T12D.first_line;_T18B3->loc=Cyc_Position_loc_to_seg(_T12E);_T128=(struct Cyc_Absyn_Decl*)_T18B3;}_T18B2->hd=_T128;_T12F=yyyvsp;_T130=& _T12F[2].v;_T131=(union Cyc_YYSTYPE*)_T130;_T18B2->tl=Cyc_yyget_YY16(_T131);_T127=(struct Cyc_List_List*)_T18B2;}yyval=Cyc_YY16(_T127);goto _LL0;case 14:{struct Cyc_List_List*_T18B2=_cycalloc(sizeof(struct Cyc_List_List));{struct Cyc_Absyn_Decl*_T18B3=_cycalloc(sizeof(struct Cyc_Absyn_Decl));_T134=& Cyc_Absyn_Tempestoff_d_val;_T135=(struct Cyc_Absyn_Tempestoff_d_Absyn_Raw_decl_struct*)_T134;
# 1386 "parse.y"
_T18B3->r=(void*)_T135;_T136=yyyvsp;_T137=_T136[0];_T138=_T137.l;_T139=_T138.first_line;_T18B3->loc=Cyc_Position_loc_to_seg(_T139);_T133=(struct Cyc_Absyn_Decl*)_T18B3;}_T18B2->hd=_T133;_T13A=yyyvsp;_T13B=& _T13A[2].v;_T13C=(union Cyc_YYSTYPE*)_T13B;_T18B2->tl=Cyc_yyget_YY16(_T13C);_T132=(struct Cyc_List_List*)_T18B2;}yyval=Cyc_YY16(_T132);goto _LL0;case 15:
# 1387 "parse.y"
 yyval=Cyc_YY16(0);goto _LL0;case 16:
# 1391 "parse.y"
 Cyc_Parse_parsing_tempest=1;goto _LL0;case 17:
# 1394
 Cyc_Parse_parsing_tempest=0;goto _LL0;case 18:
# 1399 "parse.y"
 Cyc_Lex_enter_extern_c();_T13D=yyyvsp;_T13E=& _T13D[1].v;_T13F=(union Cyc_YYSTYPE*)_T13E;_T140=
Cyc_yyget_String_tok(_T13F);_T141=_tag_fat("C",sizeof(char),2U);_T142=Cyc_strcmp(_T140,_T141);if(_T142!=0)goto _TL28D;
yyval=Cyc_YY32(0);goto _TL28E;
_TL28D: _T143=yyyvsp;_T144=& _T143[1].v;_T145=(union Cyc_YYSTYPE*)_T144;_T146=Cyc_yyget_String_tok(_T145);_T147=_tag_fat("C include",sizeof(char),10U);_T148=Cyc_strcmp(_T146,_T147);if(_T148!=0)goto _TL28F;
yyval=Cyc_YY32(1);goto _TL290;
# 1405
_TL28F: _T149=yyyvsp;_T14A=_T149[0];_T14B=_T14A.l;_T14C=_T14B.first_line;_T14D=Cyc_Position_loc_to_seg(_T14C);_T14E=_tag_fat("expecting \"C\" or \"C include\"",sizeof(char),29U);_T14F=_tag_fat(0U,sizeof(void*),0);Cyc_Warn_err(_T14D,_T14E,_T14F);
yyval=Cyc_YY32(1);_TL290: _TL28E: goto _LL0;case 19:
# 1412 "parse.y"
 Cyc_Lex_leave_extern_c();goto _LL0;case 20:
# 1416 "parse.y"
 yyval=Cyc_YY57(0);goto _LL0;case 21: _T150=yyyvsp;_T151=_T150[2];
# 1417 "parse.y"
yyval=_T151.v;goto _LL0;case 22:{struct Cyc_List_List*_T18B2=_cycalloc(sizeof(struct Cyc_List_List));_T153=yyyvsp;_T154=& _T153[0].v;_T155=(union Cyc_YYSTYPE*)_T154;
# 1421 "parse.y"
_T18B2->hd=Cyc_yyget_QualId_tok(_T155);_T18B2->tl=0;_T152=(struct Cyc_List_List*)_T18B2;}yyval=Cyc_YY57(_T152);goto _LL0;case 23:{struct Cyc_List_List*_T18B2=_cycalloc(sizeof(struct Cyc_List_List));_T157=yyyvsp;_T158=& _T157[0].v;_T159=(union Cyc_YYSTYPE*)_T158;
# 1422 "parse.y"
_T18B2->hd=Cyc_yyget_QualId_tok(_T159);_T18B2->tl=0;_T156=(struct Cyc_List_List*)_T18B2;}yyval=Cyc_YY57(_T156);goto _LL0;case 24:{struct Cyc_List_List*_T18B2=_cycalloc(sizeof(struct Cyc_List_List));_T15B=yyyvsp;_T15C=& _T15B[0].v;_T15D=(union Cyc_YYSTYPE*)_T15C;
# 1423 "parse.y"
_T18B2->hd=Cyc_yyget_QualId_tok(_T15D);_T15E=yyyvsp;_T15F=& _T15E[2].v;_T160=(union Cyc_YYSTYPE*)_T15F;_T18B2->tl=Cyc_yyget_YY57(_T160);_T15A=(struct Cyc_List_List*)_T18B2;}yyval=Cyc_YY57(_T15A);goto _LL0;case 25:{struct _tuple30*_T18B2=_cycalloc(sizeof(struct _tuple30));
# 1427 "parse.y"
_T18B2->f0=0;_T18B2->f1=0U;_T161=(struct _tuple30*)_T18B2;}yyval=Cyc_YY56(_T161);goto _LL0;case 26: _T162=yyyvsp;_T163=_T162[0];
# 1428 "parse.y"
yyval=_T163.v;goto _LL0;case 27:{struct _tuple30*_T18B2=_cycalloc(sizeof(struct _tuple30));_T165=yyyvsp;_T166=& _T165[2].v;_T167=(union Cyc_YYSTYPE*)_T166;
# 1432 "parse.y"
_T18B2->f0=Cyc_yyget_YY55(_T167);_T18B2->f1=0U;_T164=(struct _tuple30*)_T18B2;}yyval=Cyc_YY56(_T164);goto _LL0;case 28:{struct _tuple30*_T18B2=_cycalloc(sizeof(struct _tuple30));
# 1433 "parse.y"
_T18B2->f0=0;_T18B2->f1=0U;_T168=(struct _tuple30*)_T18B2;}yyval=Cyc_YY56(_T168);goto _LL0;case 29:{struct _tuple30*_T18B2=_cycalloc(sizeof(struct _tuple30));
# 1434 "parse.y"
_T18B2->f0=0;_T16A=yyyvsp;_T16B=_T16A[0];_T16C=_T16B.l;_T16D=_T16C.first_line;_T18B2->f1=Cyc_Position_loc_to_seg(_T16D);_T169=(struct _tuple30*)_T18B2;}yyval=Cyc_YY56(_T169);goto _LL0;case 30:{struct Cyc_List_List*_T18B2=_cycalloc(sizeof(struct Cyc_List_List));{struct _tuple33*_T18B3=_cycalloc(sizeof(struct _tuple33));_T170=yyyvsp;_T171=_T170[0];_T172=_T171.l;_T173=_T172.first_line;
# 1439 "parse.y"
_T18B3->f0=Cyc_Position_loc_to_seg(_T173);_T174=yyyvsp;_T175=& _T174[0].v;_T176=(union Cyc_YYSTYPE*)_T175;_T18B3->f1=Cyc_yyget_QualId_tok(_T176);_T18B3->f2=0;_T16F=(struct _tuple33*)_T18B3;}_T18B2->hd=_T16F;_T18B2->tl=0;_T16E=(struct Cyc_List_List*)_T18B2;}yyval=Cyc_YY55(_T16E);goto _LL0;case 31:{struct Cyc_List_List*_T18B2=_cycalloc(sizeof(struct Cyc_List_List));{struct _tuple33*_T18B3=_cycalloc(sizeof(struct _tuple33));_T179=yyyvsp;_T17A=_T179[0];_T17B=_T17A.l;_T17C=_T17B.first_line;
# 1441 "parse.y"
_T18B3->f0=Cyc_Position_loc_to_seg(_T17C);_T17D=yyyvsp;_T17E=& _T17D[0].v;_T17F=(union Cyc_YYSTYPE*)_T17E;_T18B3->f1=Cyc_yyget_QualId_tok(_T17F);_T18B3->f2=0;_T178=(struct _tuple33*)_T18B3;}_T18B2->hd=_T178;_T180=yyyvsp;_T181=& _T180[2].v;_T182=(union Cyc_YYSTYPE*)_T181;_T18B2->tl=Cyc_yyget_YY55(_T182);_T177=(struct Cyc_List_List*)_T18B2;}yyval=Cyc_YY55(_T177);goto _LL0;case 32:
# 1445 "parse.y"
 yyval=Cyc_YY16(0);goto _LL0;case 33: _T183=yyyvsp;_T184=_T183[2];
# 1446 "parse.y"
yyval=_T184.v;goto _LL0;case 34:{struct Cyc_List_List*_T18B2=_cycalloc(sizeof(struct Cyc_List_List));{struct Cyc_Absyn_Fn_d_Absyn_Raw_decl_struct*_T18B3=_cycalloc(sizeof(struct Cyc_Absyn_Fn_d_Absyn_Raw_decl_struct));_T18B3->tag=1;_T187=yyyvsp;_T188=& _T187[0].v;_T189=(union Cyc_YYSTYPE*)_T188;
# 1450 "parse.y"
_T18B3->f1=Cyc_yyget_YY15(_T189);_T186=(struct Cyc_Absyn_Fn_d_Absyn_Raw_decl_struct*)_T18B3;}_T18A=(void*)_T186;_T18B=yyyvsp;_T18C=_T18B[0];_T18D=_T18C.l;_T18E=_T18D.first_line;_T18F=Cyc_Position_loc_to_seg(_T18E);_T18B2->hd=Cyc_Absyn_new_decl(_T18A,_T18F);_T18B2->tl=0;_T185=(struct Cyc_List_List*)_T18B2;}yyval=Cyc_YY16(_T185);goto _LL0;case 35: _T190=yyyvsp;_T191=_T190[0];
# 1451 "parse.y"
yyval=_T191.v;goto _LL0;case 36:
# 1452 "parse.y"
 yyval=Cyc_YY16(0);goto _LL0;case 39: _T192=yyr;_T193=yyyvsp;_T194=& _T193[0].v;_T195=(union Cyc_YYSTYPE*)_T194;_T196=
# 1486 "parse.y"
Cyc_yyget_YY28(_T195);_T197=yyyvsp;_T198=& _T197[1].v;_T199=(union Cyc_YYSTYPE*)_T198;_T19A=Cyc_yyget_Stmt_tok(_T199);_T19B=yyyvsp;_T19C=_T19B[0];_T19D=_T19C.l;_T19E=_T19D.first_line;_T19F=Cyc_Position_loc_to_seg(_T19E);_T1A0=Cyc_Parse_make_function(_T192,0,_T196,0,_T19A,_T19F);yyval=Cyc_YY15(_T1A0);goto _LL0;case 40: _T1A1=yyyvsp;_T1A2=& _T1A1[0].v;_T1A3=(union Cyc_YYSTYPE*)_T1A2;{
# 1488 "parse.y"
struct Cyc_Parse_Declaration_spec d=Cyc_yyget_YY17(_T1A3);_T1A4=yyr;_T1A5=& d;_T1A6=(struct Cyc_Parse_Declaration_spec*)_T1A5;_T1A7=yyyvsp;_T1A8=& _T1A7[1].v;_T1A9=(union Cyc_YYSTYPE*)_T1A8;_T1AA=
Cyc_yyget_YY28(_T1A9);_T1AB=yyyvsp;_T1AC=& _T1AB[2].v;_T1AD=(union Cyc_YYSTYPE*)_T1AC;_T1AE=Cyc_yyget_Stmt_tok(_T1AD);_T1AF=yyyvsp;_T1B0=_T1AF[0];_T1B1=_T1B0.l;_T1B2=_T1B1.first_line;_T1B3=Cyc_Position_loc_to_seg(_T1B2);_T1B4=Cyc_Parse_make_function(_T1A4,_T1A6,_T1AA,0,_T1AE,_T1B3);yyval=Cyc_YY15(_T1B4);goto _LL0;}case 41: _T1B5=yyr;_T1B6=yyyvsp;_T1B7=& _T1B6[0].v;_T1B8=(union Cyc_YYSTYPE*)_T1B7;_T1B9=
# 1500 "parse.y"
Cyc_yyget_YY28(_T1B8);_T1BA=yyyvsp;_T1BB=& _T1BA[1].v;_T1BC=(union Cyc_YYSTYPE*)_T1BB;_T1BD=Cyc_yyget_YY16(_T1BC);_T1BE=yyyvsp;_T1BF=& _T1BE[2].v;_T1C0=(union Cyc_YYSTYPE*)_T1BF;_T1C1=Cyc_yyget_Stmt_tok(_T1C0);_T1C2=yyyvsp;_T1C3=_T1C2[0];_T1C4=_T1C3.l;_T1C5=_T1C4.first_line;_T1C6=Cyc_Position_loc_to_seg(_T1C5);_T1C7=Cyc_Parse_make_function(_T1B5,0,_T1B9,_T1BD,_T1C1,_T1C6);yyval=Cyc_YY15(_T1C7);goto _LL0;case 42: _T1C8=yyyvsp;_T1C9=& _T1C8[0].v;_T1CA=(union Cyc_YYSTYPE*)_T1C9;{
# 1502 "parse.y"
struct Cyc_Parse_Declaration_spec d=Cyc_yyget_YY17(_T1CA);_T1CB=yyr;_T1CC=& d;_T1CD=(struct Cyc_Parse_Declaration_spec*)_T1CC;_T1CE=yyyvsp;_T1CF=& _T1CE[1].v;_T1D0=(union Cyc_YYSTYPE*)_T1CF;_T1D1=
Cyc_yyget_YY28(_T1D0);_T1D2=yyyvsp;_T1D3=& _T1D2[2].v;_T1D4=(union Cyc_YYSTYPE*)_T1D3;_T1D5=Cyc_yyget_YY16(_T1D4);_T1D6=yyyvsp;_T1D7=& _T1D6[3].v;_T1D8=(union Cyc_YYSTYPE*)_T1D7;_T1D9=Cyc_yyget_Stmt_tok(_T1D8);_T1DA=yyyvsp;_T1DB=_T1DA[0];_T1DC=_T1DB.l;_T1DD=_T1DC.first_line;_T1DE=Cyc_Position_loc_to_seg(_T1DD);_T1DF=Cyc_Parse_make_function(_T1CB,_T1CD,_T1D1,_T1D5,_T1D9,_T1DE);yyval=Cyc_YY15(_T1DF);goto _LL0;}case 43: _T1E0=yyyvsp;_T1E1=& _T1E0[0].v;_T1E2=(union Cyc_YYSTYPE*)_T1E1;{
# 1510 "parse.y"
struct Cyc_Parse_Declaration_spec d=Cyc_yyget_YY17(_T1E2);_T1E3=yyr;_T1E4=& d;_T1E5=(struct Cyc_Parse_Declaration_spec*)_T1E4;_T1E6=yyyvsp;_T1E7=& _T1E6[1].v;_T1E8=(union Cyc_YYSTYPE*)_T1E7;_T1E9=
Cyc_yyget_YY28(_T1E8);_T1EA=yyyvsp;_T1EB=& _T1EA[2].v;_T1EC=(union Cyc_YYSTYPE*)_T1EB;_T1ED=Cyc_yyget_Stmt_tok(_T1EC);_T1EE=yyyvsp;_T1EF=_T1EE[0];_T1F0=_T1EF.l;_T1F1=_T1F0.first_line;_T1F2=Cyc_Position_loc_to_seg(_T1F1);_T1F3=Cyc_Parse_make_function(_T1E3,_T1E5,_T1E9,0,_T1ED,_T1F2);yyval=Cyc_YY15(_T1F3);goto _LL0;}case 44: _T1F4=yyyvsp;_T1F5=& _T1F4[0].v;_T1F6=(union Cyc_YYSTYPE*)_T1F5;{
# 1513 "parse.y"
struct Cyc_Parse_Declaration_spec d=Cyc_yyget_YY17(_T1F6);_T1F7=yyr;_T1F8=& d;_T1F9=(struct Cyc_Parse_Declaration_spec*)_T1F8;_T1FA=yyyvsp;_T1FB=& _T1FA[1].v;_T1FC=(union Cyc_YYSTYPE*)_T1FB;_T1FD=
Cyc_yyget_YY28(_T1FC);_T1FE=yyyvsp;_T1FF=& _T1FE[2].v;_T200=(union Cyc_YYSTYPE*)_T1FF;_T201=Cyc_yyget_YY16(_T200);_T202=yyyvsp;_T203=& _T202[3].v;_T204=(union Cyc_YYSTYPE*)_T203;_T205=Cyc_yyget_Stmt_tok(_T204);_T206=yyyvsp;_T207=_T206[0];_T208=_T207.l;_T209=_T208.first_line;_T20A=Cyc_Position_loc_to_seg(_T209);_T20B=Cyc_Parse_make_function(_T1F7,_T1F9,_T1FD,_T201,_T205,_T20A);yyval=Cyc_YY15(_T20B);goto _LL0;}case 45: _T20C=yyyvsp;_T20D=& _T20C[1].v;_T20E=(union Cyc_YYSTYPE*)_T20D;_T20F=
# 1518 "parse.y"
Cyc_yyget_QualId_tok(_T20E);Cyc_Lex_enter_using(_T20F);_T210=yyyvsp;_T211=_T210[1];yyval=_T211.v;goto _LL0;case 46:
# 1521
 Cyc_Lex_leave_using();goto _LL0;case 47:{struct _fat_ptr*_T18B2=_cycalloc(sizeof(struct _fat_ptr));_T213=yyyvsp;_T214=& _T213[1].v;_T215=(union Cyc_YYSTYPE*)_T214;
# 1524
*_T18B2=Cyc_yyget_String_tok(_T215);_T212=(struct _fat_ptr*)_T18B2;}Cyc_Lex_enter_namespace(_T212);_T216=yyyvsp;_T217=_T216[1];yyval=_T217.v;goto _LL0;case 48:
# 1527
 Cyc_Lex_leave_namespace();goto _LL0;case 49:
# 1530
 Cyc_Parse_inside_noinference_block=Cyc_Parse_inside_noinference_block + 1;goto _LL0;case 50:
# 1533
 Cyc_Parse_inside_noinference_block=Cyc_Parse_inside_noinference_block + -1;goto _LL0;case 51: _T218=yyyvsp;_T219=& _T218[0].v;_T21A=(union Cyc_YYSTYPE*)_T219;_T21B=
# 1538 "parse.y"
Cyc_yyget_YY17(_T21A);_T21C=yyyvsp;_T21D=_T21C[0];_T21E=_T21D.l;_T21F=_T21E.first_line;_T220=Cyc_Position_loc_to_seg(_T21F);_T221=yyyvsp;_T222=_T221[0];_T223=_T222.l;_T224=_T223.first_line;_T225=Cyc_Position_loc_to_seg(_T224);_T226=Cyc_Parse_make_declarations(_T21B,0,_T220,_T225);yyval=Cyc_YY16(_T226);goto _LL0;case 52: _T227=yyyvsp;_T228=& _T227[0].v;_T229=(union Cyc_YYSTYPE*)_T228;_T22A=
# 1540 "parse.y"
Cyc_yyget_YY17(_T229);{struct _tuple11*(*_T18B2)(struct _tuple11*)=(struct _tuple11*(*)(struct _tuple11*))Cyc_Parse_flat_imp_rev;_T22B=_T18B2;}_T22C=yyyvsp;_T22D=& _T22C[1].v;_T22E=(union Cyc_YYSTYPE*)_T22D;_T22F=Cyc_yyget_YY19(_T22E);_T230=_T22B(_T22F);_T231=yyyvsp;_T232=_T231[0];_T233=_T232.l;_T234=_T233.first_line;_T235=Cyc_Position_loc_to_seg(_T234);_T236=yyyvsp;_T237=_T236[0];_T238=_T237.l;_T239=_T238.first_line;_T23A=Cyc_Position_loc_to_seg(_T239);_T23B=Cyc_Parse_make_declarations(_T22A,_T230,_T235,_T23A);yyval=Cyc_YY16(_T23B);goto _LL0;case 53:{struct Cyc_List_List*_T18B2=_cycalloc(sizeof(struct Cyc_List_List));_T23D=yyyvsp;_T23E=& _T23D[1].v;_T23F=(union Cyc_YYSTYPE*)_T23E;_T240=
# 1543
Cyc_yyget_YY9(_T23F);_T241=yyyvsp;_T242=& _T241[3].v;_T243=(union Cyc_YYSTYPE*)_T242;_T244=Cyc_yyget_Exp_tok(_T243);_T245=yyyvsp;_T246=_T245[0];_T247=_T246.l;_T248=_T247.first_line;_T249=Cyc_Position_loc_to_seg(_T248);_T18B2->hd=Cyc_Absyn_let_decl(_T240,_T244,_T249);_T18B2->tl=0;_T23C=(struct Cyc_List_List*)_T18B2;}yyval=Cyc_YY16(_T23C);goto _LL0;case 54:  {
# 1545 "parse.y"
struct Cyc_List_List*vds=0;_T24A=yyyvsp;_T24B=& _T24A[1].v;_T24C=(union Cyc_YYSTYPE*)_T24B;{
struct Cyc_List_List*ids=Cyc_yyget_YY37(_T24C);_TL294: if(ids!=0)goto _TL292;else{goto _TL293;}
_TL292:{struct _tuple0*qv;qv=_cycalloc(sizeof(struct _tuple0));_T24D=qv;_T24D->f0=Cyc_Absyn_Rel_n(0);_T24E=qv;_T24F=ids;_T250=_T24F->hd;_T24E->f1=(struct _fat_ptr*)_T250;{struct Cyc_List_List*_T18B2=_cycalloc(sizeof(struct Cyc_List_List));_T252=qv;_T253=
Cyc_Absyn_wildtyp(0);_T18B2->hd=Cyc_Absyn_new_vardecl(0U,_T252,_T253,0,0);_T18B2->tl=vds;_T251=(struct Cyc_List_List*)_T18B2;}vds=_T251;}_T254=ids;
# 1546
ids=_T254->tl;goto _TL294;_TL293:;}{struct Cyc_List_List*_T18B2=_cycalloc(sizeof(struct Cyc_List_List));_T256=
# 1550
Cyc_List_imp_rev(vds);_T257=yyyvsp;_T258=_T257[0];_T259=_T258.l;_T25A=_T259.first_line;_T25B=Cyc_Position_loc_to_seg(_T25A);_T18B2->hd=Cyc_Absyn_letv_decl(_T256,_T25B);_T18B2->tl=0;_T255=(struct Cyc_List_List*)_T18B2;}yyval=Cyc_YY16(_T255);goto _LL0;}case 55: _T25C=yyyvsp;_T25D=& _T25C[2].v;_T25E=(union Cyc_YYSTYPE*)_T25D;_T25F=
# 1555 "parse.y"
Cyc_yyget_String_tok(_T25E);_T260=yyyvsp;_T261=_T260[2];_T262=_T261.l;_T263=_T262.first_line;_T264=Cyc_Position_loc_to_seg(_T263);Cyc_Parse_tvar_ok(_T25F,_T264);{
struct Cyc_Absyn_Tvar*tv;tv=_cycalloc(sizeof(struct Cyc_Absyn_Tvar));_T265=tv;{struct _fat_ptr*_T18B2=_cycalloc(sizeof(struct _fat_ptr));_T267=yyyvsp;_T268=& _T267[2].v;_T269=(union Cyc_YYSTYPE*)_T268;*_T18B2=Cyc_yyget_String_tok(_T269);_T266=(struct _fat_ptr*)_T18B2;}_T265->name=_T266;_T26A=tv;_T26A->identity=- 1;_T26B=tv;_T26C=& Cyc_Kinds_ek;_T26D=(struct Cyc_Absyn_Kind*)_T26C;_T26B->kind=Cyc_Kinds_kind_to_bound(_T26D);_T26E=tv;_T26E->aquals_bound=0;{
void*t=Cyc_Absyn_var_type(tv);_T26F=yyyvsp;_T270=_T26F[4];_T271=_T270.l;_T272=_T271.first_line;_T273=
Cyc_Position_loc_to_seg(_T272);{struct _tuple0*_T18B2=_cycalloc(sizeof(struct _tuple0));_T18B2->f0=Cyc_Absyn_Loc_n();{struct _fat_ptr*_T18B3=_cycalloc(sizeof(struct _fat_ptr));_T276=yyyvsp;_T277=& _T276[4].v;_T278=(union Cyc_YYSTYPE*)_T277;*_T18B3=Cyc_yyget_String_tok(_T278);_T275=(struct _fat_ptr*)_T18B3;}_T18B2->f1=_T275;_T274=(struct _tuple0*)_T18B2;}_T279=Cyc_Absyn_rgn_handle_type(t);{struct Cyc_Absyn_Vardecl*vd=Cyc_Absyn_new_vardecl(_T273,_T274,_T279,0,0);{struct Cyc_List_List*_T18B2=_cycalloc(sizeof(struct Cyc_List_List));_T27B=tv;_T27C=vd;_T27D=yyyvsp;_T27E=_T27D[0];_T27F=_T27E.l;_T280=_T27F.first_line;_T281=
Cyc_Position_loc_to_seg(_T280);_T18B2->hd=Cyc_Absyn_region_decl(_T27B,_T27C,0,_T281);_T18B2->tl=0;_T27A=(struct Cyc_List_List*)_T18B2;}yyval=Cyc_YY16(_T27A);goto _LL0;}}}case 56:{struct Cyc_String_pa_PrintArg_struct _T18B2;_T18B2.tag=0;_T284=yyyvsp;_T285=& _T284[1].v;_T286=(union Cyc_YYSTYPE*)_T285;
# 1563
_T18B2.f1=Cyc_yyget_String_tok(_T286);_T283=_T18B2;}{struct Cyc_String_pa_PrintArg_struct _T18B2=_T283;void*_T18B3[1];_T18B3[0]=& _T18B2;_T287=_tag_fat("`%s",sizeof(char),4U);_T288=_tag_fat(_T18B3,sizeof(void*),1);_T282=Cyc_aprintf(_T287,_T288);}{struct _fat_ptr tvstring=_T282;_T289=yyyvsp;_T28A=& _T289[1].v;_T28B=(union Cyc_YYSTYPE*)_T28A;_T28C=
Cyc_yyget_String_tok(_T28B);_T28D=yyyvsp;_T28E=_T28D[1];_T28F=_T28E.l;_T290=_T28F.first_line;_T291=Cyc_Position_loc_to_seg(_T290);Cyc_Parse_tvar_ok(_T28C,_T291);{
struct Cyc_Absyn_Tvar*tv;tv=_cycalloc(sizeof(struct Cyc_Absyn_Tvar));_T292=tv;{struct _fat_ptr*_T18B2=_cycalloc(sizeof(struct _fat_ptr));*_T18B2=tvstring;_T293=(struct _fat_ptr*)_T18B2;}_T292->name=_T293;_T294=tv;_T294->identity=- 1;_T295=tv;_T296=& Cyc_Kinds_ek;_T297=(struct Cyc_Absyn_Kind*)_T296;_T295->kind=Cyc_Kinds_kind_to_bound(_T297);_T298=tv;_T298->aquals_bound=0;{
void*t=Cyc_Absyn_var_type(tv);_T299=yyyvsp;_T29A=_T299[1];_T29B=_T29A.l;_T29C=_T29B.first_line;_T29D=
Cyc_Position_loc_to_seg(_T29C);{struct _tuple0*_T18B2=_cycalloc(sizeof(struct _tuple0));_T18B2->f0=Cyc_Absyn_Loc_n();{struct _fat_ptr*_T18B3=_cycalloc(sizeof(struct _fat_ptr));_T2A0=yyyvsp;_T2A1=& _T2A0[1].v;_T2A2=(union Cyc_YYSTYPE*)_T2A1;*_T18B3=Cyc_yyget_String_tok(_T2A2);_T29F=(struct _fat_ptr*)_T18B3;}_T18B2->f1=_T29F;_T29E=(struct _tuple0*)_T18B2;}_T2A3=Cyc_Absyn_rgn_handle_type(t);{struct Cyc_Absyn_Vardecl*vd=Cyc_Absyn_new_vardecl(_T29D,_T29E,_T2A3,0,0);{struct Cyc_List_List*_T18B2=_cycalloc(sizeof(struct Cyc_List_List));_T2A5=tv;_T2A6=vd;_T2A7=yyyvsp;_T2A8=& _T2A7[2].v;_T2A9=(union Cyc_YYSTYPE*)_T2A8;_T2AA=
Cyc_yyget_YY61(_T2A9);_T2AB=yyyvsp;_T2AC=_T2AB[0];_T2AD=_T2AC.l;_T2AE=_T2AD.first_line;_T2AF=Cyc_Position_loc_to_seg(_T2AE);_T18B2->hd=Cyc_Absyn_region_decl(_T2A5,_T2A6,_T2AA,_T2AF);_T18B2->tl=0;_T2A4=(struct Cyc_List_List*)_T18B2;}yyval=Cyc_YY16(_T2A4);goto _LL0;}}}}case 57:
# 1573 "parse.y"
 yyval=Cyc_YY61(0);goto _LL0;case 58: _T2B0=yyyvsp;_T2B1=& _T2B0[1].v;_T2B2=(union Cyc_YYSTYPE*)_T2B1;_T2B3=
# 1575 "parse.y"
Cyc_yyget_String_tok(_T2B2);_T2B4=_tag_fat("open",sizeof(char),5U);_T2B5=Cyc_strcmp(_T2B3,_T2B4);if(_T2B5==0)goto _TL295;_T2B6=yyyvsp;_T2B7=_T2B6[3];_T2B8=_T2B7.l;_T2B9=_T2B8.first_line;_T2BA=Cyc_Position_loc_to_seg(_T2B9);_T2BB=_tag_fat("expecting `open'",sizeof(char),17U);_T2BC=_tag_fat(0U,sizeof(void*),0);Cyc_Warn_err(_T2BA,_T2BB,_T2BC);goto _TL296;_TL295: _TL296: _T2BD=yyyvsp;_T2BE=& _T2BD[3].v;_T2BF=(union Cyc_YYSTYPE*)_T2BE;_T2C0=
Cyc_yyget_Exp_tok(_T2BF);yyval=Cyc_YY61(_T2C0);goto _LL0;case 59: _T2C1=yyyvsp;_T2C2=_T2C1[0];
# 1579
yyval=_T2C2.v;goto _LL0;case 60: _T2C3=yyyvsp;_T2C4=& _T2C3[0].v;_T2C5=(union Cyc_YYSTYPE*)_T2C4;_T2C6=
# 1580 "parse.y"
Cyc_yyget_YY16(_T2C5);_T2C7=yyyvsp;_T2C8=& _T2C7[1].v;_T2C9=(union Cyc_YYSTYPE*)_T2C8;_T2CA=Cyc_yyget_YY16(_T2C9);_T2CB=Cyc_List_imp_append(_T2C6,_T2CA);yyval=Cyc_YY16(_T2CB);goto _LL0;case 61:{struct Cyc_Parse_Declaration_spec _T18B2;_T2CD=yyyvsp;_T2CE=& _T2CD[0].v;_T2CF=(union Cyc_YYSTYPE*)_T2CE;
# 1586 "parse.y"
_T18B2.sc=Cyc_yyget_YY20(_T2CF);_T2D0=yyyvsp;_T2D1=_T2D0[0];_T2D2=_T2D1.l;_T2D3=_T2D2.first_line;_T2D4=Cyc_Position_loc_to_seg(_T2D3);_T18B2.tq=Cyc_Absyn_empty_tqual(_T2D4);
_T18B2.type_specs=Cyc_Parse_empty_spec(0U);_T18B2.is_inline=0;_T18B2.attributes=0;_T2CC=_T18B2;}
# 1586
yyval=Cyc_YY17(_T2CC);goto _LL0;case 62: _T2D5=yyyvsp;_T2D6=& _T2D5[1].v;_T2D7=(union Cyc_YYSTYPE*)_T2D6;{
# 1589 "parse.y"
struct Cyc_Parse_Declaration_spec two=Cyc_yyget_YY17(_T2D7);_T2D8=two;_T2D9=_T2D8.sc;_T2DA=(int)_T2D9;
if(_T2DA==7)goto _TL297;_T2DB=yyyvsp;_T2DC=_T2DB[0];_T2DD=_T2DC.l;_T2DE=_T2DD.first_line;_T2DF=
Cyc_Position_loc_to_seg(_T2DE);_T2E0=
_tag_fat("Only one storage class is allowed in a declaration (missing ';' or ','?)",sizeof(char),73U);_T2E1=_tag_fat(0U,sizeof(void*),0);
# 1591
Cyc_Warn_warn(_T2DF,_T2E0,_T2E1);goto _TL298;_TL297: _TL298:{struct Cyc_Parse_Declaration_spec _T18B2;_T2E3=yyyvsp;_T2E4=& _T2E3[0].v;_T2E5=(union Cyc_YYSTYPE*)_T2E4;
# 1593
_T18B2.sc=Cyc_yyget_YY20(_T2E5);_T2E6=two;_T18B2.tq=_T2E6.tq;_T2E7=two;_T18B2.type_specs=_T2E7.type_specs;_T2E8=two;
_T18B2.is_inline=_T2E8.is_inline;_T2E9=two;_T18B2.attributes=_T2E9.attributes;_T2E2=_T18B2;}
# 1593
yyval=Cyc_YY17(_T2E2);goto _LL0;}case 63: _T2EA=yyyvsp;_T2EB=_T2EA[0];_T2EC=_T2EB.l;_T2ED=_T2EC.first_line;_T2EE=
# 1597 "parse.y"
Cyc_Position_loc_to_seg(_T2ED);_T2EF=_tag_fat("__extension__ keyword ignored in declaration",sizeof(char),45U);_T2F0=_tag_fat(0U,sizeof(void*),0);Cyc_Warn_warn(_T2EE,_T2EF,_T2F0);_T2F1=yyyvsp;_T2F2=_T2F1[1];
yyval=_T2F2.v;goto _LL0;case 64:{struct Cyc_Parse_Declaration_spec _T18B2;
# 1601 "parse.y"
_T18B2.sc=7U;_T2F4=yyyvsp;_T2F5=_T2F4[0];_T2F6=_T2F5.l;_T2F7=_T2F6.first_line;_T2F8=Cyc_Position_loc_to_seg(_T2F7);_T18B2.tq=Cyc_Absyn_empty_tqual(_T2F8);_T2F9=yyyvsp;_T2FA=& _T2F9[0].v;_T2FB=(union Cyc_YYSTYPE*)_T2FA;
_T18B2.type_specs=Cyc_yyget_YY21(_T2FB);_T18B2.is_inline=0;_T18B2.attributes=0;_T2F3=_T18B2;}
# 1601
yyval=Cyc_YY17(_T2F3);goto _LL0;case 65: _T2FC=yyyvsp;_T2FD=& _T2FC[1].v;_T2FE=(union Cyc_YYSTYPE*)_T2FD;{
# 1604 "parse.y"
struct Cyc_Parse_Declaration_spec two=Cyc_yyget_YY17(_T2FE);{struct Cyc_Parse_Declaration_spec _T18B2;_T300=two;
_T18B2.sc=_T300.sc;_T301=two;_T18B2.tq=_T301.tq;_T302=yyyvsp;_T303=_T302[0];_T304=_T303.l;_T305=_T304.first_line;_T306=
Cyc_Position_loc_to_seg(_T305);_T307=two;_T308=_T307.type_specs;_T309=yyyvsp;_T30A=& _T309[0].v;_T30B=(union Cyc_YYSTYPE*)_T30A;_T30C=Cyc_yyget_YY21(_T30B);_T18B2.type_specs=Cyc_Parse_combine_specifiers(_T306,_T308,_T30C);_T30D=two;
_T18B2.is_inline=_T30D.is_inline;_T30E=two;_T18B2.attributes=_T30E.attributes;_T2FF=_T18B2;}
# 1605
yyval=Cyc_YY17(_T2FF);goto _LL0;}case 66:{struct Cyc_Parse_Declaration_spec _T18B2;
# 1610 "parse.y"
_T18B2.sc=7U;_T310=yyyvsp;_T311=& _T310[0].v;_T312=(union Cyc_YYSTYPE*)_T311;_T18B2.tq=Cyc_yyget_YY24(_T312);_T18B2.type_specs=Cyc_Parse_empty_spec(0U);_T18B2.is_inline=0;_T18B2.attributes=0;_T30F=_T18B2;}yyval=Cyc_YY17(_T30F);goto _LL0;case 67: _T313=yyyvsp;_T314=& _T313[1].v;_T315=(union Cyc_YYSTYPE*)_T314;{
# 1612 "parse.y"
struct Cyc_Parse_Declaration_spec two=Cyc_yyget_YY17(_T315);{struct Cyc_Parse_Declaration_spec _T18B2;_T317=two;
_T18B2.sc=_T317.sc;_T318=yyyvsp;_T319=& _T318[0].v;_T31A=(union Cyc_YYSTYPE*)_T319;_T31B=Cyc_yyget_YY24(_T31A);_T31C=two;_T31D=_T31C.tq;_T18B2.tq=Cyc_Absyn_combine_tqual(_T31B,_T31D);_T31E=two;
_T18B2.type_specs=_T31E.type_specs;_T31F=two;_T18B2.is_inline=_T31F.is_inline;_T320=two;_T18B2.attributes=_T320.attributes;_T316=_T18B2;}
# 1613
yyval=Cyc_YY17(_T316);goto _LL0;}case 68:{struct Cyc_Parse_Declaration_spec _T18B2;
# 1617 "parse.y"
_T18B2.sc=7U;_T322=yyyvsp;_T323=_T322[0];_T324=_T323.l;_T325=_T324.first_line;_T326=Cyc_Position_loc_to_seg(_T325);_T18B2.tq=Cyc_Absyn_empty_tqual(_T326);
_T18B2.type_specs=Cyc_Parse_empty_spec(0U);_T18B2.is_inline=1;_T18B2.attributes=0;_T321=_T18B2;}
# 1617
yyval=Cyc_YY17(_T321);goto _LL0;case 69: _T327=yyyvsp;_T328=& _T327[1].v;_T329=(union Cyc_YYSTYPE*)_T328;{
# 1620 "parse.y"
struct Cyc_Parse_Declaration_spec two=Cyc_yyget_YY17(_T329);{struct Cyc_Parse_Declaration_spec _T18B2;_T32B=two;
_T18B2.sc=_T32B.sc;_T32C=two;_T18B2.tq=_T32C.tq;_T32D=two;_T18B2.type_specs=_T32D.type_specs;_T18B2.is_inline=1;_T32E=two;_T18B2.attributes=_T32E.attributes;_T32A=_T18B2;}yyval=Cyc_YY17(_T32A);goto _LL0;}case 70:{struct Cyc_Parse_Declaration_spec _T18B2;
# 1624 "parse.y"
_T18B2.sc=7U;_T330=yyyvsp;_T331=_T330[0];_T332=_T331.l;_T333=_T332.first_line;_T334=Cyc_Position_loc_to_seg(_T333);_T18B2.tq=Cyc_Absyn_empty_tqual(_T334);
_T18B2.type_specs=Cyc_Parse_empty_spec(0U);_T18B2.is_inline=0;_T335=yyyvsp;_T336=& _T335[0].v;_T337=(union Cyc_YYSTYPE*)_T336;_T18B2.attributes=Cyc_yyget_YY46(_T337);_T32F=_T18B2;}
# 1624
yyval=Cyc_YY17(_T32F);goto _LL0;case 71: _T338=yyyvsp;_T339=& _T338[1].v;_T33A=(union Cyc_YYSTYPE*)_T339;{
# 1627 "parse.y"
struct Cyc_Parse_Declaration_spec two=Cyc_yyget_YY17(_T33A);{struct Cyc_Parse_Declaration_spec _T18B2;_T33C=two;
_T18B2.sc=_T33C.sc;_T33D=two;_T18B2.tq=_T33D.tq;_T33E=two;
_T18B2.type_specs=_T33E.type_specs;_T33F=two;_T18B2.is_inline=_T33F.is_inline;_T340=yyyvsp;_T341=& _T340[0].v;_T342=(union Cyc_YYSTYPE*)_T341;_T343=
Cyc_yyget_YY46(_T342);_T344=two;_T345=_T344.attributes;_T18B2.attributes=Cyc_List_imp_append(_T343,_T345);_T33B=_T18B2;}
# 1628
yyval=Cyc_YY17(_T33B);goto _LL0;}case 72:
# 1634 "parse.y"
 yyval=Cyc_YY20(4U);goto _LL0;case 73:
# 1635 "parse.y"
 yyval=Cyc_YY20(5U);goto _LL0;case 74:
# 1636 "parse.y"
 yyval=Cyc_YY20(3U);goto _LL0;case 75:
# 1637 "parse.y"
 yyval=Cyc_YY20(1U);goto _LL0;case 76: _T346=yyyvsp;_T347=& _T346[1].v;_T348=(union Cyc_YYSTYPE*)_T347;_T349=
# 1639 "parse.y"
Cyc_yyget_String_tok(_T348);_T34A=_tag_fat("C",sizeof(char),2U);_T34B=Cyc_strcmp(_T349,_T34A);if(_T34B==0)goto _TL299;_T34C=yyyvsp;_T34D=_T34C[0];_T34E=_T34D.l;_T34F=_T34E.first_line;_T350=
Cyc_Position_loc_to_seg(_T34F);_T351=_tag_fat("only extern or extern \"C\" is allowed",sizeof(char),37U);_T352=_tag_fat(0U,sizeof(void*),0);Cyc_Warn_err(_T350,_T351,_T352);goto _TL29A;_TL299: _TL29A:
 yyval=Cyc_YY20(2U);goto _LL0;case 77:
# 1643 "parse.y"
 yyval=Cyc_YY20(0U);goto _LL0;case 78:
# 1645 "parse.y"
 yyval=Cyc_YY20(6U);goto _LL0;case 79:
# 1650 "parse.y"
 yyval=Cyc_YY46(0);goto _LL0;case 80: _T353=yyyvsp;_T354=_T353[0];
# 1651 "parse.y"
yyval=_T354.v;goto _LL0;case 81: _T355=yyyvsp;_T356=_T355[3];
# 1655 "parse.y"
yyval=_T356.v;goto _LL0;case 82:{struct Cyc_List_List*_T18B2=_cycalloc(sizeof(struct Cyc_List_List));_T358=yyyvsp;_T359=& _T358[0].v;_T35A=(union Cyc_YYSTYPE*)_T359;
# 1658
_T18B2->hd=Cyc_yyget_YY47(_T35A);_T18B2->tl=0;_T357=(struct Cyc_List_List*)_T18B2;}yyval=Cyc_YY46(_T357);goto _LL0;case 83:{struct Cyc_List_List*_T18B2=_cycalloc(sizeof(struct Cyc_List_List));_T35C=yyyvsp;_T35D=& _T35C[0].v;_T35E=(union Cyc_YYSTYPE*)_T35D;
# 1659 "parse.y"
_T18B2->hd=Cyc_yyget_YY47(_T35E);_T35F=yyyvsp;_T360=& _T35F[2].v;_T361=(union Cyc_YYSTYPE*)_T360;_T18B2->tl=Cyc_yyget_YY46(_T361);_T35B=(struct Cyc_List_List*)_T18B2;}yyval=Cyc_YY46(_T35B);goto _LL0;case 84: _T362=yyyvsp;_T363=_T362[0];_T364=_T363.l;_T365=_T364.first_line;_T366=
# 1662
Cyc_Position_loc_to_seg(_T365);_T367=yyyvsp;_T368=& _T367[0].v;_T369=(union Cyc_YYSTYPE*)_T368;_T36A=Cyc_yyget_String_tok(_T369);_T36B=Cyc_Atts_parse_nullary_att(_T366,_T36A);yyval=Cyc_YY47(_T36B);goto _LL0;case 85: _T36C=& Cyc_Atts_Const_att_val;_T36D=(struct Cyc_Absyn_Const_att_Absyn_Attribute_struct*)_T36C;_T36E=(void*)_T36D;
# 1663 "parse.y"
yyval=Cyc_YY47(_T36E);goto _LL0;case 86: _T36F=yyyvsp;_T370=_T36F[0];_T371=_T370.l;_T372=_T371.first_line;_T373=
# 1665 "parse.y"
Cyc_Position_loc_to_seg(_T372);_T374=yyyvsp;_T375=& _T374[0].v;_T376=(union Cyc_YYSTYPE*)_T375;_T377=Cyc_yyget_String_tok(_T376);_T378=yyyvsp;_T379=_T378[2];_T37A=_T379.l;_T37B=_T37A.first_line;_T37C=Cyc_Position_loc_to_seg(_T37B);_T37D=yyyvsp;_T37E=& _T37D[2].v;_T37F=(union Cyc_YYSTYPE*)_T37E;_T380=Cyc_yyget_Exp_tok(_T37F);_T381=Cyc_Atts_parse_unary_att(_T373,_T377,_T37C,_T380);yyval=Cyc_YY47(_T381);goto _LL0;case 87: _T382=yyyvsp;_T383=_T382[0];_T384=_T383.l;_T385=_T384.first_line;_T386=
# 1667 "parse.y"
Cyc_Position_loc_to_seg(_T385);_T387=yyyvsp;_T388=_T387[2];_T389=_T388.l;_T38A=_T389.first_line;_T38B=Cyc_Position_loc_to_seg(_T38A);_T38C=yyyvsp;_T38D=& _T38C[0].v;_T38E=(union Cyc_YYSTYPE*)_T38D;_T38F=Cyc_yyget_String_tok(_T38E);_T390=yyyvsp;_T391=& _T390[2].v;_T392=(union Cyc_YYSTYPE*)_T391;_T393=Cyc_yyget_String_tok(_T392);_T394=yyyvsp;_T395=_T394[4];_T396=_T395.l;_T397=_T396.first_line;_T398=
Cyc_Position_loc_to_seg(_T397);_T399=yyyvsp;_T39A=& _T399[4].v;_T39B=(union Cyc_YYSTYPE*)_T39A;_T39C=Cyc_yyget_Int_tok(_T39B);_T39D=Cyc_Parse_cnst2uint(_T398,_T39C);_T39E=yyyvsp;_T39F=_T39E[6];_T3A0=_T39F.l;_T3A1=_T3A0.first_line;_T3A2=
Cyc_Position_loc_to_seg(_T3A1);_T3A3=yyyvsp;_T3A4=& _T3A3[6].v;_T3A5=(union Cyc_YYSTYPE*)_T3A4;_T3A6=Cyc_yyget_Int_tok(_T3A5);_T3A7=Cyc_Parse_cnst2uint(_T3A2,_T3A6);_T3A8=
# 1667
Cyc_Atts_parse_format_att(_T386,_T38B,_T38F,_T393,_T39D,_T3A7);yyval=Cyc_YY47(_T3A8);goto _LL0;case 88: _T3A9=yyyvsp;_T3AA=_T3A9[0];
# 1679 "parse.y"
yyval=_T3AA.v;goto _LL0;case 89: _T3AB=yyyvsp;_T3AC=& _T3AB[0].v;_T3AD=(union Cyc_YYSTYPE*)_T3AC;_T3AE=
# 1681 "parse.y"
Cyc_yyget_QualId_tok(_T3AD);_T3AF=yyyvsp;_T3B0=& _T3AF[1].v;_T3B1=(union Cyc_YYSTYPE*)_T3B0;_T3B2=Cyc_yyget_YY41(_T3B1);_T3B3=Cyc_Absyn_typedef_type(_T3AE,_T3B2,0,0);_T3B4=yyyvsp;_T3B5=_T3B4[0];_T3B6=_T3B5.l;_T3B7=_T3B6.first_line;_T3B8=Cyc_Position_loc_to_seg(_T3B7);_T3B9=Cyc_Parse_type_spec(_T3B3,_T3B8);yyval=Cyc_YY21(_T3B9);goto _LL0;case 90: _T3BA=Cyc_Absyn_void_type;_T3BB=yyyvsp;_T3BC=_T3BB[0];_T3BD=_T3BC.l;_T3BE=_T3BD.first_line;_T3BF=
# 1685 "parse.y"
Cyc_Position_loc_to_seg(_T3BE);_T3C0=Cyc_Parse_type_spec(_T3BA,_T3BF);yyval=Cyc_YY21(_T3C0);goto _LL0;case 91: _T3C1=Cyc_Absyn_char_type;_T3C2=yyyvsp;_T3C3=_T3C2[0];_T3C4=_T3C3.l;_T3C5=_T3C4.first_line;_T3C6=
# 1686 "parse.y"
Cyc_Position_loc_to_seg(_T3C5);_T3C7=Cyc_Parse_type_spec(_T3C1,_T3C6);yyval=Cyc_YY21(_T3C7);goto _LL0;case 92: _T3C8=yyyvsp;_T3C9=_T3C8[0];_T3CA=_T3C9.l;_T3CB=_T3CA.first_line;_T3CC=
# 1687 "parse.y"
Cyc_Position_loc_to_seg(_T3CB);_T3CD=Cyc_Parse_short_spec(_T3CC);yyval=Cyc_YY21(_T3CD);goto _LL0;case 93: _T3CE=Cyc_Absyn_sint_type;_T3CF=yyyvsp;_T3D0=_T3CF[0];_T3D1=_T3D0.l;_T3D2=_T3D1.first_line;_T3D3=
# 1688 "parse.y"
Cyc_Position_loc_to_seg(_T3D2);_T3D4=Cyc_Parse_type_spec(_T3CE,_T3D3);yyval=Cyc_YY21(_T3D4);goto _LL0;case 94: _T3D5=yyyvsp;_T3D6=_T3D5[0];_T3D7=_T3D6.l;_T3D8=_T3D7.first_line;_T3D9=
# 1689 "parse.y"
Cyc_Position_loc_to_seg(_T3D8);_T3DA=Cyc_Parse_long_spec(_T3D9);yyval=Cyc_YY21(_T3DA);goto _LL0;case 95: _T3DB=Cyc_Absyn_float_type;_T3DC=yyyvsp;_T3DD=_T3DC[0];_T3DE=_T3DD.l;_T3DF=_T3DE.first_line;_T3E0=
# 1690 "parse.y"
Cyc_Position_loc_to_seg(_T3DF);_T3E1=Cyc_Parse_type_spec(_T3DB,_T3E0);yyval=Cyc_YY21(_T3E1);goto _LL0;case 96: _T3E2=Cyc_Absyn_float128_type;_T3E3=yyyvsp;_T3E4=_T3E3[0];_T3E5=_T3E4.l;_T3E6=_T3E5.first_line;_T3E7=
# 1691 "parse.y"
Cyc_Position_loc_to_seg(_T3E6);_T3E8=Cyc_Parse_type_spec(_T3E2,_T3E7);yyval=Cyc_YY21(_T3E8);goto _LL0;case 97: _T3E9=Cyc_Absyn_double_type;_T3EA=yyyvsp;_T3EB=_T3EA[0];_T3EC=_T3EB.l;_T3ED=_T3EC.first_line;_T3EE=
# 1692 "parse.y"
Cyc_Position_loc_to_seg(_T3ED);_T3EF=Cyc_Parse_type_spec(_T3E9,_T3EE);yyval=Cyc_YY21(_T3EF);goto _LL0;case 98: _T3F0=yyyvsp;_T3F1=_T3F0[0];_T3F2=_T3F1.l;_T3F3=_T3F2.first_line;_T3F4=
# 1693 "parse.y"
Cyc_Position_loc_to_seg(_T3F3);_T3F5=Cyc_Parse_signed_spec(_T3F4);yyval=Cyc_YY21(_T3F5);goto _LL0;case 99: _T3F6=yyyvsp;_T3F7=_T3F6[0];_T3F8=_T3F7.l;_T3F9=_T3F8.first_line;_T3FA=
# 1694 "parse.y"
Cyc_Position_loc_to_seg(_T3F9);_T3FB=Cyc_Parse_unsigned_spec(_T3FA);yyval=Cyc_YY21(_T3FB);goto _LL0;case 100: _T3FC=yyyvsp;_T3FD=_T3FC[0];_T3FE=_T3FD.l;_T3FF=_T3FE.first_line;_T400=
# 1695 "parse.y"
Cyc_Position_loc_to_seg(_T3FF);_T401=Cyc_Parse_complex_spec(_T400);yyval=Cyc_YY21(_T401);goto _LL0;case 101: _T402=yyyvsp;_T403=_T402[0];
# 1696 "parse.y"
yyval=_T403.v;goto _LL0;case 102: _T404=yyyvsp;_T405=_T404[0];
# 1697 "parse.y"
yyval=_T405.v;goto _LL0;case 103: _T406=yyyvsp;_T407=& _T406[2].v;_T408=(union Cyc_YYSTYPE*)_T407;_T409=
# 1699 "parse.y"
Cyc_yyget_Exp_tok(_T408);_T40A=Cyc_Absyn_typeof_type(_T409);_T40B=yyyvsp;_T40C=_T40B[0];_T40D=_T40C.l;_T40E=_T40D.first_line;_T40F=Cyc_Position_loc_to_seg(_T40E);_T410=Cyc_Parse_type_spec(_T40A,_T40F);yyval=Cyc_YY21(_T410);goto _LL0;case 104: _T411=
# 1701 "parse.y"
_tag_fat("__builtin_va_list",sizeof(char),18U);_T412=& Cyc_Kinds_bk;_T413=(struct Cyc_Absyn_Kind*)_T412;_T414=Cyc_Absyn_builtin_type(_T411,_T413);_T415=yyyvsp;_T416=_T415[0];_T417=_T416.l;_T418=_T417.first_line;_T419=Cyc_Position_loc_to_seg(_T418);_T41A=Cyc_Parse_type_spec(_T414,_T419);yyval=Cyc_YY21(_T41A);goto _LL0;case 105: _T41B=yyyvsp;_T41C=_T41B[0];
# 1703 "parse.y"
yyval=_T41C.v;goto _LL0;case 106: _T41D=yyyvsp;_T41E=& _T41D[0].v;_T41F=(union Cyc_YYSTYPE*)_T41E;_T420=
# 1705 "parse.y"
Cyc_yyget_YY45(_T41F);_T421=yyyvsp;_T422=_T421[0];_T423=_T422.l;_T424=_T423.first_line;_T425=Cyc_Position_loc_to_seg(_T424);_T426=Cyc_Parse_type_spec(_T420,_T425);yyval=Cyc_YY21(_T426);goto _LL0;case 107: _T427=
# 1707 "parse.y"
Cyc_Absyn_new_evar(0,0);_T428=yyyvsp;_T429=_T428[0];_T42A=_T429.l;_T42B=_T42A.first_line;_T42C=Cyc_Position_loc_to_seg(_T42B);_T42D=Cyc_Parse_type_spec(_T427,_T42C);yyval=Cyc_YY21(_T42D);goto _LL0;case 108: _T42E=yyyvsp;_T42F=& _T42E[2].v;_T430=(union Cyc_YYSTYPE*)_T42F;_T431=
# 1709 "parse.y"
Cyc_yyget_YY44(_T430);_T432=Cyc_Kinds_kind_to_opt(_T431);_T433=Cyc_Absyn_new_evar(_T432,0);_T434=yyyvsp;_T435=_T434[0];_T436=_T435.l;_T437=_T436.first_line;_T438=Cyc_Position_loc_to_seg(_T437);_T439=Cyc_Parse_type_spec(_T433,_T438);yyval=Cyc_YY21(_T439);goto _LL0;case 109: _T43B=Cyc_List_map_c;{
# 1711 "parse.y"
struct Cyc_List_List*(*_T18B2)(struct _tuple19*(*)(unsigned,struct _tuple8*),unsigned,struct Cyc_List_List*)=(struct Cyc_List_List*(*)(struct _tuple19*(*)(unsigned,struct _tuple8*),unsigned,struct Cyc_List_List*))_T43B;_T43A=_T18B2;}{struct _tuple19*(*_T18B2)(unsigned,struct _tuple8*)=(struct _tuple19*(*)(unsigned,struct _tuple8*))Cyc_Parse_get_tqual_typ;_T43C=_T18B2;}_T43D=yyyvsp;_T43E=_T43D[2];_T43F=_T43E.l;_T440=_T43F.first_line;_T441=Cyc_Position_loc_to_seg(_T440);_T442=yyyvsp;_T443=& _T442[2].v;_T444=(union Cyc_YYSTYPE*)_T443;_T445=Cyc_yyget_YY39(_T444);_T446=Cyc_List_imp_rev(_T445);_T447=_T43A(_T43C,_T441,_T446);_T448=Cyc_Absyn_tuple_type(_T447);_T449=yyyvsp;_T44A=_T449[0];_T44B=_T44A.l;_T44C=_T44B.first_line;_T44D=
Cyc_Position_loc_to_seg(_T44C);_T44E=
# 1711
Cyc_Parse_type_spec(_T448,_T44D);yyval=Cyc_YY21(_T44E);goto _LL0;case 110: _T44F=yyyvsp;_T450=& _T44F[2].v;_T451=(union Cyc_YYSTYPE*)_T450;_T452=
# 1714 "parse.y"
Cyc_yyget_YY45(_T451);_T453=Cyc_Absyn_rgn_handle_type(_T452);_T454=yyyvsp;_T455=_T454[0];_T456=_T455.l;_T457=_T456.first_line;_T458=Cyc_Position_loc_to_seg(_T457);_T459=Cyc_Parse_type_spec(_T453,_T458);yyval=Cyc_YY21(_T459);goto _LL0;case 111: _T45A=& Cyc_Kinds_eko;_T45B=(struct Cyc_Core_Opt*)_T45A;_T45C=
# 1716 "parse.y"
Cyc_Absyn_new_evar(_T45B,0);_T45D=Cyc_Absyn_rgn_handle_type(_T45C);_T45E=yyyvsp;_T45F=_T45E[0];_T460=_T45F.l;_T461=_T460.first_line;_T462=Cyc_Position_loc_to_seg(_T461);_T463=Cyc_Parse_type_spec(_T45D,_T462);yyval=Cyc_YY21(_T463);goto _LL0;case 112: _T464=yyyvsp;_T465=& _T464[2].v;_T466=(union Cyc_YYSTYPE*)_T465;_T467=
# 1718 "parse.y"
Cyc_yyget_YY58(_T466);_T468=Cyc_Absyn_aqual_handle_type(_T467);_T469=yyyvsp;_T46A=_T469[0];_T46B=_T46A.l;_T46C=_T46B.first_line;_T46D=Cyc_Position_loc_to_seg(_T46C);_T46E=Cyc_Parse_type_spec(_T468,_T46D);yyval=Cyc_YY21(_T46E);goto _LL0;case 113: _T46F=yyyvsp;_T470=& _T46F[2].v;_T471=(union Cyc_YYSTYPE*)_T470;_T472=
# 1720 "parse.y"
Cyc_yyget_YY45(_T471);_T473=Cyc_Absyn_tag_type(_T472);_T474=yyyvsp;_T475=_T474[0];_T476=_T475.l;_T477=_T476.first_line;_T478=Cyc_Position_loc_to_seg(_T477);_T479=Cyc_Parse_type_spec(_T473,_T478);yyval=Cyc_YY21(_T479);goto _LL0;case 114: _T47A=& Cyc_Kinds_iko;_T47B=(struct Cyc_Core_Opt*)_T47A;_T47C=
# 1722 "parse.y"
Cyc_Absyn_new_evar(_T47B,0);_T47D=Cyc_Absyn_tag_type(_T47C);_T47E=yyyvsp;_T47F=_T47E[0];_T480=_T47F.l;_T481=_T480.first_line;_T482=Cyc_Position_loc_to_seg(_T481);_T483=Cyc_Parse_type_spec(_T47D,_T482);yyval=Cyc_YY21(_T483);goto _LL0;case 115: _T484=yyyvsp;_T485=& _T484[2].v;_T486=(union Cyc_YYSTYPE*)_T485;_T487=
# 1724 "parse.y"
Cyc_yyget_Exp_tok(_T486);_T488=Cyc_Absyn_valueof_type(_T487);_T489=yyyvsp;_T48A=_T489[0];_T48B=_T48A.l;_T48C=_T48B.first_line;_T48D=Cyc_Position_loc_to_seg(_T48C);_T48E=Cyc_Parse_type_spec(_T488,_T48D);yyval=Cyc_YY21(_T48E);goto _LL0;case 116: _T48F=yyyvsp;_T490=& _T48F[2].v;_T491=(union Cyc_YYSTYPE*)_T490;{
# 1727
struct _tuple26 _T18B2=Cyc_yyget_YY36(_T491);struct Cyc_List_List*_T18B3;struct Cyc_Parse_Type_specifier _T18B4;struct Cyc_Absyn_Tqual _T18B5;_T18B5=_T18B2.f0;_T18B4=_T18B2.f1;_T18B3=_T18B2.f2;{struct Cyc_Absyn_Tqual tq=_T18B5;struct Cyc_Parse_Type_specifier tspecs=_T18B4;struct Cyc_List_List*atts=_T18B3;_T492=tq;_T493=_T492.loc;
if(_T493!=0U)goto _TL29B;_T494=yyyvsp;_T495=_T494[2];_T496=_T495.l;_T497=_T496.first_line;tq.loc=Cyc_Position_loc_to_seg(_T497);goto _TL29C;_TL29B: _TL29C: _T498=yyyvsp;_T499=& _T498[3].v;_T49A=(union Cyc_YYSTYPE*)_T499;{
struct Cyc_Parse_Declarator _T18B6=Cyc_yyget_YY28(_T49A);struct Cyc_List_List*_T18B7;unsigned _T18B8;struct _tuple0*_T18B9;_T18B9=_T18B6.id;_T18B8=_T18B6.varloc;_T18B7=_T18B6.tms;{struct _tuple0*qv=_T18B9;unsigned varloc=_T18B8;struct Cyc_List_List*tms=_T18B7;_T49B=tspecs;_T49C=yyyvsp;_T49D=_T49C[2];_T49E=_T49D.l;_T49F=_T49E.first_line;_T4A0=
Cyc_Position_loc_to_seg(_T49F);{void*t=Cyc_Parse_speclist2typ(_T49B,_T4A0);
struct _tuple14 _T18BA=Cyc_Parse_apply_tms(tq,t,atts,tms);struct Cyc_List_List*_T18BB;struct Cyc_List_List*_T18BC;void*_T18BD;struct Cyc_Absyn_Tqual _T18BE;_T18BE=_T18BA.f0;_T18BD=_T18BA.f1;_T18BC=_T18BA.f2;_T18BB=_T18BA.f3;{struct Cyc_Absyn_Tqual tq2=_T18BE;void*t2=_T18BD;struct Cyc_List_List*tvs=_T18BC;struct Cyc_List_List*atts2=_T18BB;
if(tvs==0)goto _TL29D;{struct Cyc_Warn_String_Warn_Warg_struct _T18BF;_T18BF.tag=0;_T18BF.f1=_tag_fat("parameter with bad type params",sizeof(char),31U);_T4A1=_T18BF;}{struct Cyc_Warn_String_Warn_Warg_struct _T18BF=_T4A1;void*_T18C0[1];_T18C0[0]=& _T18BF;_T4A2=yyyvsp;_T4A3=_T4A2[3];_T4A4=_T4A3.l;_T4A5=_T4A4.first_line;_T4A6=Cyc_Position_loc_to_seg(_T4A5);_T4A7=_tag_fat(_T18C0,sizeof(void*),1);Cyc_Warn_err2(_T4A6,_T4A7);}goto _TL29E;_TL29D: _TL29E: _T4A8=
Cyc_Absyn_is_qvar_qualified(qv);if(!_T4A8)goto _TL29F;{struct Cyc_Warn_String_Warn_Warg_struct _T18BF;_T18BF.tag=0;
_T18BF.f1=_tag_fat("parameter cannot be qualified with a namespace",sizeof(char),47U);_T4A9=_T18BF;}{struct Cyc_Warn_String_Warn_Warg_struct _T18BF=_T4A9;void*_T18C0[1];_T18C0[0]=& _T18BF;_T4AA=yyyvsp;_T4AB=_T4AA[0];_T4AC=_T4AB.l;_T4AD=_T4AC.first_line;_T4AE=Cyc_Position_loc_to_seg(_T4AD);_T4AF=_tag_fat(_T18C0,sizeof(void*),1);Cyc_Warn_err2(_T4AE,_T4AF);}goto _TL2A0;_TL29F: _TL2A0:
 if(atts2==0)goto _TL2A1;{struct Cyc_Warn_String_Warn_Warg_struct _T18BF;_T18BF.tag=0;_T18BF.f1=_tag_fat("extra attributes on parameter, ignoring",sizeof(char),40U);_T4B0=_T18BF;}{struct Cyc_Warn_String_Warn_Warg_struct _T18BF=_T4B0;void*_T18C0[1];_T18C0[0]=& _T18BF;_T4B1=yyyvsp;_T4B2=_T4B1[0];_T4B3=_T4B2.l;_T4B4=_T4B3.first_line;_T4B5=Cyc_Position_loc_to_seg(_T4B4);_T4B6=_tag_fat(_T18C0,sizeof(void*),1);Cyc_Warn_warn2(_T4B5,_T4B6);}goto _TL2A2;_TL2A1: _TL2A2: {
struct Cyc_Absyn_Vardecl*vd=Cyc_Absyn_new_vardecl(varloc,qv,t2,0,0);{struct Cyc_Absyn_SubsetType_Absyn_Type_struct*_T18BF=_cycalloc(sizeof(struct Cyc_Absyn_SubsetType_Absyn_Type_struct));_T18BF->tag=12;
_T18BF->f1=vd;_T4B8=yyyvsp;_T4B9=& _T4B8[5].v;_T4BA=(union Cyc_YYSTYPE*)_T4B9;_T18BF->f2=Cyc_yyget_Exp_tok(_T4BA);_T18BF->f3=0;_T4B7=(struct Cyc_Absyn_SubsetType_Absyn_Type_struct*)_T18BF;}_T4BB=(void*)_T4B7;_T4BC=yyyvsp;_T4BD=_T4BC[0];_T4BE=_T4BD.l;_T4BF=_T4BE.first_line;_T4C0=Cyc_Position_loc_to_seg(_T4BF);_T4C1=Cyc_Parse_type_spec(_T4BB,_T4C0);yyval=Cyc_YY21(_T4C1);goto _LL0;}}}}}}}case 117: _T4C2=yyyvsp;_T4C3=& _T4C2[0].v;_T4C4=(union Cyc_YYSTYPE*)_T4C3;_T4C5=
# 1743 "parse.y"
Cyc_yyget_String_tok(_T4C4);_T4C6=yyyvsp;_T4C7=_T4C6[0];_T4C8=_T4C7.l;_T4C9=_T4C8.first_line;_T4CA=Cyc_Position_loc_to_seg(_T4C9);_T4CB=Cyc_Kinds_id_to_kind(_T4C5,_T4CA);yyval=Cyc_YY44(_T4CB);goto _LL0;case 118: _T4CD=Cyc_Flags_porting_c_code;
# 1747 "parse.y"
if(!_T4CD)goto _TL2A3;_T4CE=yyyvsp;_T4CF=_T4CE[0];_T4D0=_T4CF.l;_T4D1=_T4D0.first_line;_T4CC=Cyc_Position_loc_to_seg(_T4D1);goto _TL2A4;_TL2A3: _T4CC=0U;_TL2A4: {unsigned loc=_T4CC;{struct Cyc_Absyn_Tqual _T18B2;
_T18B2.print_const=1;_T18B2.q_volatile=0;_T18B2.q_restrict=0;_T18B2.real_const=1;_T18B2.loc=loc;_T4D2=_T18B2;}yyval=Cyc_YY24(_T4D2);goto _LL0;}case 119:{struct Cyc_Absyn_Tqual _T18B2;
# 1749 "parse.y"
_T18B2.print_const=0;_T18B2.q_volatile=1;_T18B2.q_restrict=0;_T18B2.real_const=0;_T18B2.loc=0U;_T4D3=_T18B2;}yyval=Cyc_YY24(_T4D3);goto _LL0;case 120:{struct Cyc_Absyn_Tqual _T18B2;
# 1750 "parse.y"
_T18B2.print_const=0;_T18B2.q_volatile=0;_T18B2.q_restrict=1;_T18B2.real_const=0;_T18B2.loc=0U;_T4D4=_T18B2;}yyval=Cyc_YY24(_T4D4);goto _LL0;case 121:  {
# 1756 "parse.y"
struct Cyc_Absyn_TypeDecl*ed;ed=_cycalloc(sizeof(struct Cyc_Absyn_TypeDecl));_T4D5=ed;{struct Cyc_Absyn_Enum_td_Absyn_Raw_typedecl_struct*_T18B2=_cycalloc(sizeof(struct Cyc_Absyn_Enum_td_Absyn_Raw_typedecl_struct));_T18B2->tag=1;{struct Cyc_Absyn_Enumdecl*_T18B3=_cycalloc(sizeof(struct Cyc_Absyn_Enumdecl));_T18B3->sc=2U;_T4D8=yyyvsp;_T4D9=& _T4D8[1].v;_T4DA=(union Cyc_YYSTYPE*)_T4D9;_T18B3->name=Cyc_yyget_QualId_tok(_T4DA);{struct Cyc_Core_Opt*_T18B4=_cycalloc(sizeof(struct Cyc_Core_Opt));_T4DC=yyyvsp;_T4DD=& _T4DC[3].v;_T4DE=(union Cyc_YYSTYPE*)_T4DD;_T18B4->v=Cyc_yyget_YY49(_T4DE);_T4DB=(struct Cyc_Core_Opt*)_T18B4;}_T18B3->fields=_T4DB;_T4D7=(struct Cyc_Absyn_Enumdecl*)_T18B3;}_T18B2->f1=_T4D7;_T4D6=(struct Cyc_Absyn_Enum_td_Absyn_Raw_typedecl_struct*)_T18B2;}_T4D5->r=(void*)_T4D6;_T4DF=ed;_T4E0=yyyvsp;_T4E1=_T4E0[0];_T4E2=_T4E1.l;_T4E3=_T4E2.first_line;
_T4DF->loc=Cyc_Position_loc_to_seg(_T4E3);{struct Cyc_Absyn_TypeDeclType_Absyn_Type_struct*_T18B2=_cycalloc(sizeof(struct Cyc_Absyn_TypeDeclType_Absyn_Type_struct));_T18B2->tag=10;
_T18B2->f1=ed;_T18B2->f2=0;_T4E4=(struct Cyc_Absyn_TypeDeclType_Absyn_Type_struct*)_T18B2;}_T4E5=(void*)_T4E4;_T4E6=yyyvsp;_T4E7=_T4E6[0];_T4E8=_T4E7.l;_T4E9=_T4E8.first_line;_T4EA=Cyc_Position_loc_to_seg(_T4E9);_T4EB=Cyc_Parse_type_spec(_T4E5,_T4EA);yyval=Cyc_YY21(_T4EB);goto _LL0;}case 122: _T4EC=yyyvsp;_T4ED=& _T4EC[1].v;_T4EE=(union Cyc_YYSTYPE*)_T4ED;_T4EF=
# 1761 "parse.y"
Cyc_yyget_QualId_tok(_T4EE);_T4F0=Cyc_Absyn_enum_type(_T4EF,0);_T4F1=yyyvsp;_T4F2=_T4F1[0];_T4F3=_T4F2.l;_T4F4=_T4F3.first_line;_T4F5=Cyc_Position_loc_to_seg(_T4F4);_T4F6=Cyc_Parse_type_spec(_T4F0,_T4F5);yyval=Cyc_YY21(_T4F6);goto _LL0;case 123: _T4F7=yyyvsp;_T4F8=& _T4F7[2].v;_T4F9=(union Cyc_YYSTYPE*)_T4F8;_T4FA=
# 1763 "parse.y"
Cyc_yyget_YY49(_T4F9);_T4FB=Cyc_Absyn_anon_enum_type(_T4FA);_T4FC=yyyvsp;_T4FD=_T4FC[0];_T4FE=_T4FD.l;_T4FF=_T4FE.first_line;_T500=Cyc_Position_loc_to_seg(_T4FF);_T501=Cyc_Parse_type_spec(_T4FB,_T500);yyval=Cyc_YY21(_T501);goto _LL0;case 124:{struct Cyc_Absyn_Enumfield*_T18B2=_cycalloc(sizeof(struct Cyc_Absyn_Enumfield));_T503=yyyvsp;_T504=& _T503[0].v;_T505=(union Cyc_YYSTYPE*)_T504;
# 1769 "parse.y"
_T18B2->name=Cyc_yyget_QualId_tok(_T505);_T18B2->tag=0;_T506=yyyvsp;_T507=_T506[0];_T508=_T507.l;_T509=_T508.first_line;_T18B2->loc=Cyc_Position_loc_to_seg(_T509);_T502=(struct Cyc_Absyn_Enumfield*)_T18B2;}yyval=Cyc_YY48(_T502);goto _LL0;case 125:{struct Cyc_Absyn_Enumfield*_T18B2=_cycalloc(sizeof(struct Cyc_Absyn_Enumfield));_T50B=yyyvsp;_T50C=& _T50B[0].v;_T50D=(union Cyc_YYSTYPE*)_T50C;
# 1771 "parse.y"
_T18B2->name=Cyc_yyget_QualId_tok(_T50D);_T50E=yyyvsp;_T50F=& _T50E[2].v;_T510=(union Cyc_YYSTYPE*)_T50F;_T18B2->tag=Cyc_yyget_Exp_tok(_T510);_T511=yyyvsp;_T512=_T511[0];_T513=_T512.l;_T514=_T513.first_line;_T18B2->loc=Cyc_Position_loc_to_seg(_T514);_T50A=(struct Cyc_Absyn_Enumfield*)_T18B2;}yyval=Cyc_YY48(_T50A);goto _LL0;case 126:{struct Cyc_List_List*_T18B2=_cycalloc(sizeof(struct Cyc_List_List));_T516=yyyvsp;_T517=& _T516[0].v;_T518=(union Cyc_YYSTYPE*)_T517;
# 1775 "parse.y"
_T18B2->hd=Cyc_yyget_YY48(_T518);_T18B2->tl=0;_T515=(struct Cyc_List_List*)_T18B2;}yyval=Cyc_YY49(_T515);goto _LL0;case 127:{struct Cyc_List_List*_T18B2=_cycalloc(sizeof(struct Cyc_List_List));_T51A=yyyvsp;_T51B=& _T51A[0].v;_T51C=(union Cyc_YYSTYPE*)_T51B;
# 1776 "parse.y"
_T18B2->hd=Cyc_yyget_YY48(_T51C);_T18B2->tl=0;_T519=(struct Cyc_List_List*)_T18B2;}yyval=Cyc_YY49(_T519);goto _LL0;case 128:{struct Cyc_List_List*_T18B2=_cycalloc(sizeof(struct Cyc_List_List));_T51E=yyyvsp;_T51F=& _T51E[0].v;_T520=(union Cyc_YYSTYPE*)_T51F;
# 1777 "parse.y"
_T18B2->hd=Cyc_yyget_YY48(_T520);_T521=yyyvsp;_T522=& _T521[2].v;_T523=(union Cyc_YYSTYPE*)_T522;_T18B2->tl=Cyc_yyget_YY49(_T523);_T51D=(struct Cyc_List_List*)_T18B2;}yyval=Cyc_YY49(_T51D);goto _LL0;case 129:{struct Cyc_Absyn_AnonAggrType_Absyn_Type_struct*_T18B2=_cycalloc(sizeof(struct Cyc_Absyn_AnonAggrType_Absyn_Type_struct));_T18B2->tag=7;_T525=yyyvsp;_T526=& _T525[0].v;_T527=(union Cyc_YYSTYPE*)_T526;
# 1783 "parse.y"
_T18B2->f1=Cyc_yyget_YY22(_T527);_T18B2->f2=0;_T528=yyyvsp;_T529=& _T528[2].v;_T52A=(union Cyc_YYSTYPE*)_T529;_T18B2->f3=Cyc_yyget_YY25(_T52A);_T524=(struct Cyc_Absyn_AnonAggrType_Absyn_Type_struct*)_T18B2;}_T52B=(void*)_T524;_T52C=yyyvsp;_T52D=_T52C[0];_T52E=_T52D.l;_T52F=_T52E.first_line;_T530=Cyc_Position_loc_to_seg(_T52F);_T531=Cyc_Parse_type_spec(_T52B,_T530);yyval=Cyc_YY21(_T531);goto _LL0;case 130: _T533=Cyc_List_map_c;{
# 1787 "parse.y"
struct Cyc_List_List*(*_T18B2)(struct Cyc_Absyn_Tvar*(*)(unsigned,void*),unsigned,struct Cyc_List_List*)=(struct Cyc_List_List*(*)(struct Cyc_Absyn_Tvar*(*)(unsigned,void*),unsigned,struct Cyc_List_List*))_T533;_T532=_T18B2;}_T534=yyyvsp;_T535=_T534[2];_T536=_T535.l;_T537=_T536.first_line;_T538=Cyc_Position_loc_to_seg(_T537);_T539=yyyvsp;_T53A=& _T539[2].v;_T53B=(union Cyc_YYSTYPE*)_T53A;_T53C=Cyc_yyget_YY41(_T53B);{struct Cyc_List_List*ts=_T532(Cyc_Parse_typ2tvar,_T538,_T53C);_T53E=Cyc_List_map_c;{
struct Cyc_List_List*(*_T18B2)(struct Cyc_Absyn_Tvar*(*)(unsigned,void*),unsigned,struct Cyc_List_List*)=(struct Cyc_List_List*(*)(struct Cyc_Absyn_Tvar*(*)(unsigned,void*),unsigned,struct Cyc_List_List*))_T53E;_T53D=_T18B2;}_T53F=yyyvsp;_T540=_T53F[4];_T541=_T540.l;_T542=_T541.first_line;_T543=Cyc_Position_loc_to_seg(_T542);_T544=yyyvsp;_T545=& _T544[4].v;_T546=(union Cyc_YYSTYPE*)_T545;_T547=Cyc_yyget_YY41(_T546);{struct Cyc_List_List*exist_ts=_T53D(Cyc_Parse_typ2tvar,_T543,_T547);_T548=yyyvsp;_T549=& _T548[5].v;_T54A=(union Cyc_YYSTYPE*)_T549;{
struct _tuple28*ec_qb=Cyc_yyget_YY51(_T54A);_T54C=ec_qb;_T54D=(unsigned)_T54C;
if(!_T54D)goto _TL2A5;_T54E=ec_qb;_T54B=*_T54E;goto _TL2A6;_TL2A5:{struct _tuple28 _T18B2;_T18B2.f0=0;_T18B2.f1=0;_T54F=_T18B2;}_T54B=_T54F;_TL2A6: {struct _tuple28 _T18B2=_T54B;struct Cyc_List_List*_T18B3;struct Cyc_List_List*_T18B4;_T18B4=_T18B2.f0;_T18B3=_T18B2.f1;{struct Cyc_List_List*ec=_T18B4;struct Cyc_List_List*qb=_T18B3;_T550=yyyvsp;_T551=& _T550[0].v;_T552=(union Cyc_YYSTYPE*)_T551;_T553=
Cyc_yyget_YY23(_T552);_T554=_T553.f1;_T555=yyyvsp;_T556=& _T555[1].v;_T557=(union Cyc_YYSTYPE*)_T556;_T558=Cyc_yyget_QualId_tok(_T557);_T559=ts;_T55A=exist_ts;_T55B=ec;_T55C=qb;_T55D=yyyvsp;_T55E=& _T55D[6].v;_T55F=(union Cyc_YYSTYPE*)_T55E;_T560=
Cyc_yyget_YY25(_T55F);_T561=yyyvsp;_T562=& _T561[0].v;_T563=(union Cyc_YYSTYPE*)_T562;_T564=Cyc_yyget_YY23(_T563);_T565=_T564.f0;_T566=Cyc_Absyn_aggrdecl_impl(_T55A,_T55B,_T55C,_T560,_T565);_T567=yyyvsp;_T568=_T567[0];_T569=_T568.l;_T56A=_T569.first_line;_T56B=
Cyc_Position_loc_to_seg(_T56A);{
# 1791
struct Cyc_Absyn_TypeDecl*td=Cyc_Absyn_aggr_tdecl(_T554,2U,_T558,_T559,_T566,0,_T56B);{struct Cyc_Absyn_TypeDeclType_Absyn_Type_struct*_T18B5=_cycalloc(sizeof(struct Cyc_Absyn_TypeDeclType_Absyn_Type_struct));_T18B5->tag=10;
# 1794
_T18B5->f1=td;_T18B5->f2=0;_T56C=(struct Cyc_Absyn_TypeDeclType_Absyn_Type_struct*)_T18B5;}_T56D=(void*)_T56C;_T56E=yyyvsp;_T56F=_T56E[0];_T570=_T56F.l;_T571=_T570.first_line;_T572=Cyc_Position_loc_to_seg(_T571);_T573=Cyc_Parse_type_spec(_T56D,_T572);yyval=Cyc_YY21(_T573);goto _LL0;}}}}}}case 131: _T574=yyyvsp;_T575=& _T574[0].v;_T576=(union Cyc_YYSTYPE*)_T575;_T577=
# 1797 "parse.y"
Cyc_yyget_YY23(_T576);_T578=_T577.f1;_T579=yyyvsp;_T57A=& _T579[1].v;_T57B=(union Cyc_YYSTYPE*)_T57A;_T57C=Cyc_yyget_QualId_tok(_T57B);_T57E=yyyvsp;_T57F=& _T57E[0].v;_T580=(union Cyc_YYSTYPE*)_T57F;_T581=Cyc_yyget_YY23(_T580);_T582=_T581.f0;if(!_T582)goto _TL2A7;{struct Cyc_Core_Opt*_T18B2=_cycalloc(sizeof(struct Cyc_Core_Opt));_T18B2->v=(void*)1;_T583=(struct Cyc_Core_Opt*)_T18B2;}_T57D=_T583;goto _TL2A8;_TL2A7: _T57D=0;_TL2A8: _T584=Cyc_Absyn_UnknownAggr(_T578,_T57C,_T57D);_T585=yyyvsp;_T586=& _T585[2].v;_T587=(union Cyc_YYSTYPE*)_T586;_T588=
Cyc_yyget_YY41(_T587);_T589=
# 1797
Cyc_Absyn_aggr_type(_T584,_T588);_T58A=yyyvsp;_T58B=_T58A[0];_T58C=_T58B.l;_T58D=_T58C.first_line;_T58E=
Cyc_Position_loc_to_seg(_T58D);_T58F=
# 1797
Cyc_Parse_type_spec(_T589,_T58E);yyval=Cyc_YY21(_T58F);goto _LL0;case 132:{struct _tuple25 _T18B2;
# 1802 "parse.y"
_T18B2.f0=1;_T591=yyyvsp;_T592=& _T591[1].v;_T593=(union Cyc_YYSTYPE*)_T592;_T18B2.f1=Cyc_yyget_YY22(_T593);_T590=_T18B2;}yyval=Cyc_YY23(_T590);goto _LL0;case 133:{struct _tuple25 _T18B2;
# 1803 "parse.y"
_T18B2.f0=0;_T595=yyyvsp;_T596=& _T595[0].v;_T597=(union Cyc_YYSTYPE*)_T596;_T18B2.f1=Cyc_yyget_YY22(_T597);_T594=_T18B2;}yyval=Cyc_YY23(_T594);goto _LL0;case 134:
# 1806
 yyval=Cyc_YY22(0U);goto _LL0;case 135:
# 1807 "parse.y"
 yyval=Cyc_YY22(1U);goto _LL0;case 136:
# 1810
 yyval=Cyc_YY41(0);goto _LL0;case 137: _T598=yyyvsp;_T599=& _T598[1].v;_T59A=(union Cyc_YYSTYPE*)_T599;_T59B=
# 1811 "parse.y"
Cyc_yyget_YY41(_T59A);_T59C=Cyc_List_imp_rev(_T59B);yyval=Cyc_YY41(_T59C);goto _LL0;case 138:
# 1816 "parse.y"
 yyval=Cyc_YY25(0);goto _LL0;case 139:  {
# 1818 "parse.y"
struct Cyc_List_List*decls=0;_T59D=yyyvsp;_T59E=& _T59D[0].v;_T59F=(union Cyc_YYSTYPE*)_T59E;{
struct Cyc_List_List*x=Cyc_yyget_YY26(_T59F);_TL2AC: if(x!=0)goto _TL2AA;else{goto _TL2AB;}
_TL2AA: _T5A0=x;_T5A1=_T5A0->hd;_T5A2=(struct Cyc_List_List*)_T5A1;_T5A3=decls;decls=Cyc_List_imp_append(_T5A2,_T5A3);_T5A4=x;
# 1819
x=_T5A4->tl;goto _TL2AC;_TL2AB:;}{
# 1822
struct Cyc_List_List*tags=Cyc_Parse_get_aggrfield_tags(decls);
if(tags==0)goto _TL2AD;_T5A6=Cyc_List_iter_c;{
void(*_T18B2)(void(*)(struct Cyc_List_List*,struct Cyc_Absyn_Aggrfield*),struct Cyc_List_List*,struct Cyc_List_List*)=(void(*)(void(*)(struct Cyc_List_List*,struct Cyc_Absyn_Aggrfield*),struct Cyc_List_List*,struct Cyc_List_List*))_T5A6;_T5A5=_T18B2;}_T5A7=tags;_T5A8=decls;_T5A5(Cyc_Parse_substitute_aggrfield_tags,_T5A7,_T5A8);goto _TL2AE;_TL2AD: _TL2AE:
 yyval=Cyc_YY25(decls);goto _LL0;}}case 140:{struct Cyc_List_List*_T18B2=_cycalloc(sizeof(struct Cyc_List_List));_T5AA=yyyvsp;_T5AB=& _T5AA[0].v;_T5AC=(union Cyc_YYSTYPE*)_T5AB;
# 1831 "parse.y"
_T18B2->hd=Cyc_yyget_YY25(_T5AC);_T18B2->tl=0;_T5A9=(struct Cyc_List_List*)_T18B2;}yyval=Cyc_YY26(_T5A9);goto _LL0;case 141:{struct Cyc_List_List*_T18B2=_cycalloc(sizeof(struct Cyc_List_List));_T5AE=yyyvsp;_T5AF=& _T5AE[1].v;_T5B0=(union Cyc_YYSTYPE*)_T5AF;
# 1832 "parse.y"
_T18B2->hd=Cyc_yyget_YY25(_T5B0);_T5B1=yyyvsp;_T5B2=& _T5B1[0].v;_T5B3=(union Cyc_YYSTYPE*)_T5B2;_T18B2->tl=Cyc_yyget_YY26(_T5B3);_T5AD=(struct Cyc_List_List*)_T18B2;}yyval=Cyc_YY26(_T5AD);goto _LL0;case 142: _T5B5=yyr;{struct _tuple11*_T18B2=_region_malloc(_T5B5,0U,sizeof(struct _tuple11));
# 1838 "parse.y"
_T18B2->tl=0;_T5B6=yyyvsp;_T5B7=& _T5B6[0].v;_T5B8=(union Cyc_YYSTYPE*)_T5B7;_T18B2->hd=Cyc_yyget_YY18(_T5B8);_T5B4=(struct _tuple11*)_T18B2;}yyval=Cyc_YY19(_T5B4);goto _LL0;case 143: _T5BA=yyr;{struct _tuple11*_T18B2=_region_malloc(_T5BA,0U,sizeof(struct _tuple11));_T5BB=yyyvsp;_T5BC=& _T5BB[0].v;_T5BD=(union Cyc_YYSTYPE*)_T5BC;
# 1840 "parse.y"
_T18B2->tl=Cyc_yyget_YY19(_T5BD);_T5BE=yyyvsp;_T5BF=& _T5BE[2].v;_T5C0=(union Cyc_YYSTYPE*)_T5BF;_T18B2->hd=Cyc_yyget_YY18(_T5C0);_T5B9=(struct _tuple11*)_T18B2;}yyval=Cyc_YY19(_T5B9);goto _LL0;case 144:{struct _tuple12 _T18B2;_T5C2=yyyvsp;_T5C3=& _T5C2[0].v;_T5C4=(union Cyc_YYSTYPE*)_T5C3;
# 1844 "parse.y"
_T18B2.f0=Cyc_yyget_YY28(_T5C4);_T18B2.f1=0;_T18B2.f2=0;_T5C1=_T18B2;}yyval=Cyc_YY18(_T5C1);goto _LL0;case 145:{struct _tuple12 _T18B2;_T5C6=yyyvsp;_T5C7=& _T5C6[0].v;_T5C8=(union Cyc_YYSTYPE*)_T5C7;
# 1845 "parse.y"
_T18B2.f0=Cyc_yyget_YY28(_T5C8);_T18B2.f1=0;_T5C9=yyyvsp;_T5CA=& _T5C9[2].v;_T5CB=(union Cyc_YYSTYPE*)_T5CA;_T5CC=Cyc_yyget_YY63(_T5CB);_T5CD=yyyvsp;_T5CE=_T5CD[1];_T5CF=_T5CE.l;_T5D0=_T5CF.first_line;_T5D1=Cyc_Position_loc_to_seg(_T5D0);_T18B2.f2=Cyc_Absyn_new_exp(_T5CC,_T5D1);_T5C5=_T18B2;}yyval=Cyc_YY18(_T5C5);goto _LL0;case 146:{struct _tuple12 _T18B2;_T5D3=yyyvsp;_T5D4=& _T5D3[0].v;_T5D5=(union Cyc_YYSTYPE*)_T5D4;
# 1846 "parse.y"
_T18B2.f0=Cyc_yyget_YY28(_T5D5);_T5D6=yyyvsp;_T5D7=& _T5D6[2].v;_T5D8=(union Cyc_YYSTYPE*)_T5D7;_T18B2.f1=Cyc_yyget_Exp_tok(_T5D8);_T18B2.f2=0;_T5D2=_T18B2;}yyval=Cyc_YY18(_T5D2);goto _LL0;case 147:{struct _tuple12 _T18B2;_T5DA=yyyvsp;_T5DB=& _T5DA[0].v;_T5DC=(union Cyc_YYSTYPE*)_T5DB;
# 1847 "parse.y"
_T18B2.f0=Cyc_yyget_YY28(_T5DC);_T5DD=yyyvsp;_T5DE=& _T5DD[4].v;_T5DF=(union Cyc_YYSTYPE*)_T5DE;_T18B2.f1=Cyc_yyget_Exp_tok(_T5DF);_T5E0=yyyvsp;_T5E1=& _T5E0[2].v;_T5E2=(union Cyc_YYSTYPE*)_T5E1;_T5E3=Cyc_yyget_YY63(_T5E2);_T5E4=yyyvsp;_T5E5=_T5E4[1];_T5E6=_T5E5.l;_T5E7=_T5E6.first_line;_T5E8=Cyc_Position_loc_to_seg(_T5E7);_T18B2.f2=Cyc_Absyn_new_exp(_T5E3,_T5E8);_T5D9=_T18B2;}yyval=Cyc_YY18(_T5D9);goto _LL0;case 148:  {struct _RegionHandle _T18B2=_new_region(0U,"temp");struct _RegionHandle*temp=& _T18B2;_push_region(temp);_T5E9=yyyvsp;_T5EA=& _T5E9[0].v;_T5EB=(union Cyc_YYSTYPE*)_T5EA;{
# 1853 "parse.y"
struct _tuple26 _T18B3=Cyc_yyget_YY36(_T5EB);struct Cyc_List_List*_T18B4;struct Cyc_Parse_Type_specifier _T18B5;struct Cyc_Absyn_Tqual _T18B6;_T18B6=_T18B3.f0;_T18B5=_T18B3.f1;_T18B4=_T18B3.f2;{struct Cyc_Absyn_Tqual tq=_T18B6;struct Cyc_Parse_Type_specifier tspecs=_T18B5;struct Cyc_List_List*atts=_T18B4;_T5EC=tq;_T5ED=_T5EC.loc;
if(_T5ED!=0U)goto _TL2AF;_T5EE=yyyvsp;_T5EF=_T5EE[0];_T5F0=_T5EF.l;_T5F1=_T5F0.first_line;tq.loc=Cyc_Position_loc_to_seg(_T5F1);goto _TL2B0;_TL2AF: _TL2B0: {
struct _tuple13*decls=0;
struct Cyc_List_List*widths_and_reqs=0;_T5F2=yyyvsp;_T5F3=& _T5F2[1].v;_T5F4=(union Cyc_YYSTYPE*)_T5F3;{
struct Cyc_List_List*x=Cyc_yyget_YY30(_T5F4);_TL2B4: if(x!=0)goto _TL2B2;else{goto _TL2B3;}
_TL2B2: _T5F5=x;_T5F6=_T5F5->hd;{struct _tuple12*_T18B7=(struct _tuple12*)_T5F6;struct Cyc_Absyn_Exp*_T18B8;struct Cyc_Absyn_Exp*_T18B9;struct Cyc_Parse_Declarator _T18BA;{struct _tuple12 _T18BB=*_T18B7;_T18BA=_T18BB.f0;_T18B9=_T18BB.f1;_T18B8=_T18BB.f2;}{struct Cyc_Parse_Declarator d=_T18BA;struct Cyc_Absyn_Exp*wd=_T18B9;struct Cyc_Absyn_Exp*wh=_T18B8;_T5F8=temp;{struct _tuple13*_T18BB=_region_malloc(_T5F8,0U,sizeof(struct _tuple13));
_T18BB->tl=decls;_T18BB->hd=d;_T5F7=(struct _tuple13*)_T18BB;}decls=_T5F7;_T5FA=temp;{struct Cyc_List_List*_T18BB=_region_malloc(_T5FA,0U,sizeof(struct Cyc_List_List));_T5FC=temp;{struct _tuple16*_T18BC=_region_malloc(_T5FC,0U,sizeof(struct _tuple16));
_T18BC->f0=wd;_T18BC->f1=wh;_T5FB=(struct _tuple16*)_T18BC;}_T18BB->hd=_T5FB;_T18BB->tl=widths_and_reqs;_T5F9=(struct Cyc_List_List*)_T18BB;}widths_and_reqs=_T5F9;}}_T5FD=x;
# 1857
x=_T5FD->tl;goto _TL2B4;_TL2B3:;}_T5FE=tspecs;_T5FF=yyyvsp;_T600=_T5FF[0];_T601=_T600.l;_T602=_T601.first_line;_T603=
# 1862
Cyc_Position_loc_to_seg(_T602);{void*t=Cyc_Parse_speclist2typ(_T5FE,_T603);_T604=temp;_T605=temp;_T606=
# 1864
Cyc_Parse_apply_tmss(temp,tq,t,decls,atts);_T607=widths_and_reqs;{
# 1863
struct Cyc_List_List*info=Cyc_List_rzip(_T604,_T605,_T606,_T607);_T609=Cyc_List_map_c;{
# 1865
struct Cyc_List_List*(*_T18B7)(struct Cyc_Absyn_Aggrfield*(*)(unsigned,struct _tuple17*),unsigned,struct Cyc_List_List*)=(struct Cyc_List_List*(*)(struct Cyc_Absyn_Aggrfield*(*)(unsigned,struct _tuple17*),unsigned,struct Cyc_List_List*))_T609;_T608=_T18B7;}_T60A=yyyvsp;_T60B=_T60A[0];_T60C=_T60B.l;_T60D=_T60C.first_line;_T60E=Cyc_Position_loc_to_seg(_T60D);_T60F=info;_T610=_T608(Cyc_Parse_make_aggr_field,_T60E,_T60F);yyval=Cyc_YY25(_T610);_npop_handler(0);goto _LL0;}}}}}_pop_region();}case 149:{struct _tuple26 _T18B2;_T612=yyyvsp;_T613=_T612[0];_T614=_T613.l;_T615=_T614.first_line;_T616=
# 1874 "parse.y"
Cyc_Position_loc_to_seg(_T615);_T18B2.f0=Cyc_Absyn_empty_tqual(_T616);_T617=yyyvsp;_T618=& _T617[0].v;_T619=(union Cyc_YYSTYPE*)_T618;_T18B2.f1=Cyc_yyget_YY21(_T619);_T18B2.f2=0;_T611=_T18B2;}yyval=Cyc_YY36(_T611);goto _LL0;case 150: _T61A=yyyvsp;_T61B=& _T61A[1].v;_T61C=(union Cyc_YYSTYPE*)_T61B;{
# 1876 "parse.y"
struct _tuple26 two=Cyc_yyget_YY36(_T61C);{struct _tuple26 _T18B2;_T61E=two;_T18B2.f0=_T61E.f0;_T61F=yyyvsp;_T620=_T61F[0];_T621=_T620.l;_T622=_T621.first_line;_T623=Cyc_Position_loc_to_seg(_T622);_T624=yyyvsp;_T625=& _T624[0].v;_T626=(union Cyc_YYSTYPE*)_T625;_T627=Cyc_yyget_YY21(_T626);_T628=two;_T629=_T628.f1;_T18B2.f1=Cyc_Parse_combine_specifiers(_T623,_T627,_T629);_T62A=two;_T18B2.f2=_T62A.f2;_T61D=_T18B2;}yyval=Cyc_YY36(_T61D);goto _LL0;}case 151:{struct _tuple26 _T18B2;_T62C=yyyvsp;_T62D=& _T62C[0].v;_T62E=(union Cyc_YYSTYPE*)_T62D;
# 1878 "parse.y"
_T18B2.f0=Cyc_yyget_YY24(_T62E);_T18B2.f1=Cyc_Parse_empty_spec(0U);_T18B2.f2=0;_T62B=_T18B2;}yyval=Cyc_YY36(_T62B);goto _LL0;case 152: _T62F=yyyvsp;_T630=& _T62F[1].v;_T631=(union Cyc_YYSTYPE*)_T630;{
# 1880 "parse.y"
struct _tuple26 two=Cyc_yyget_YY36(_T631);{struct _tuple26 _T18B2;_T633=yyyvsp;_T634=& _T633[0].v;_T635=(union Cyc_YYSTYPE*)_T634;_T636=Cyc_yyget_YY24(_T635);_T637=two;_T638=_T637.f0;_T18B2.f0=Cyc_Absyn_combine_tqual(_T636,_T638);_T639=two;_T18B2.f1=_T639.f1;_T63A=two;_T18B2.f2=_T63A.f2;_T632=_T18B2;}yyval=Cyc_YY36(_T632);goto _LL0;}case 153:{struct _tuple26 _T18B2;_T63C=yyyvsp;_T63D=_T63C[0];_T63E=_T63D.l;_T63F=_T63E.first_line;_T640=
# 1882 "parse.y"
Cyc_Position_loc_to_seg(_T63F);_T18B2.f0=Cyc_Absyn_empty_tqual(_T640);_T18B2.f1=Cyc_Parse_empty_spec(0U);_T641=yyyvsp;_T642=& _T641[0].v;_T643=(union Cyc_YYSTYPE*)_T642;_T18B2.f2=Cyc_yyget_YY46(_T643);_T63B=_T18B2;}yyval=Cyc_YY36(_T63B);goto _LL0;case 154: _T644=yyyvsp;_T645=& _T644[1].v;_T646=(union Cyc_YYSTYPE*)_T645;{
# 1884 "parse.y"
struct _tuple26 two=Cyc_yyget_YY36(_T646);{struct _tuple26 _T18B2;_T648=two;_T18B2.f0=_T648.f0;_T649=two;_T18B2.f1=_T649.f1;_T64A=yyyvsp;_T64B=& _T64A[0].v;_T64C=(union Cyc_YYSTYPE*)_T64B;_T64D=Cyc_yyget_YY46(_T64C);_T64E=two;_T64F=_T64E.f2;_T18B2.f2=Cyc_List_append(_T64D,_T64F);_T647=_T18B2;}yyval=Cyc_YY36(_T647);goto _LL0;}case 155:{struct _tuple26 _T18B2;_T651=yyyvsp;_T652=_T651[0];_T653=_T652.l;_T654=_T653.first_line;_T655=
# 1890 "parse.y"
Cyc_Position_loc_to_seg(_T654);_T18B2.f0=Cyc_Absyn_empty_tqual(_T655);_T656=yyyvsp;_T657=& _T656[0].v;_T658=(union Cyc_YYSTYPE*)_T657;_T18B2.f1=Cyc_yyget_YY21(_T658);_T18B2.f2=0;_T650=_T18B2;}yyval=Cyc_YY36(_T650);goto _LL0;case 156: _T659=yyyvsp;_T65A=& _T659[1].v;_T65B=(union Cyc_YYSTYPE*)_T65A;{
# 1892 "parse.y"
struct _tuple26 two=Cyc_yyget_YY36(_T65B);{struct _tuple26 _T18B2;_T65D=two;_T18B2.f0=_T65D.f0;_T65E=yyyvsp;_T65F=_T65E[0];_T660=_T65F.l;_T661=_T660.first_line;_T662=Cyc_Position_loc_to_seg(_T661);_T663=yyyvsp;_T664=& _T663[0].v;_T665=(union Cyc_YYSTYPE*)_T664;_T666=Cyc_yyget_YY21(_T665);_T667=two;_T668=_T667.f1;_T18B2.f1=Cyc_Parse_combine_specifiers(_T662,_T666,_T668);_T669=two;_T18B2.f2=_T669.f2;_T65C=_T18B2;}yyval=Cyc_YY36(_T65C);goto _LL0;}case 157:{struct _tuple26 _T18B2;_T66B=yyyvsp;_T66C=& _T66B[0].v;_T66D=(union Cyc_YYSTYPE*)_T66C;
# 1894 "parse.y"
_T18B2.f0=Cyc_yyget_YY24(_T66D);_T18B2.f1=Cyc_Parse_empty_spec(0U);_T18B2.f2=0;_T66A=_T18B2;}yyval=Cyc_YY36(_T66A);goto _LL0;case 158: _T66E=yyyvsp;_T66F=& _T66E[1].v;_T670=(union Cyc_YYSTYPE*)_T66F;{
# 1896 "parse.y"
struct _tuple26 two=Cyc_yyget_YY36(_T670);{struct _tuple26 _T18B2;_T672=yyyvsp;_T673=& _T672[0].v;_T674=(union Cyc_YYSTYPE*)_T673;_T675=Cyc_yyget_YY24(_T674);_T676=two;_T677=_T676.f0;_T18B2.f0=Cyc_Absyn_combine_tqual(_T675,_T677);_T678=two;_T18B2.f1=_T678.f1;_T679=two;_T18B2.f2=_T679.f2;_T671=_T18B2;}yyval=Cyc_YY36(_T671);goto _LL0;}case 159:{struct _tuple26 _T18B2;_T67B=yyyvsp;_T67C=_T67B[0];_T67D=_T67C.l;_T67E=_T67D.first_line;_T67F=
# 1898 "parse.y"
Cyc_Position_loc_to_seg(_T67E);_T18B2.f0=Cyc_Absyn_empty_tqual(_T67F);_T18B2.f1=Cyc_Parse_empty_spec(0U);_T680=yyyvsp;_T681=& _T680[0].v;_T682=(union Cyc_YYSTYPE*)_T681;_T18B2.f2=Cyc_yyget_YY46(_T682);_T67A=_T18B2;}yyval=Cyc_YY36(_T67A);goto _LL0;case 160: _T683=yyyvsp;_T684=& _T683[1].v;_T685=(union Cyc_YYSTYPE*)_T684;{
# 1900 "parse.y"
struct _tuple26 two=Cyc_yyget_YY36(_T685);{struct _tuple26 _T18B2;_T687=two;_T18B2.f0=_T687.f0;_T688=two;_T18B2.f1=_T688.f1;_T689=yyyvsp;_T68A=& _T689[0].v;_T68B=(union Cyc_YYSTYPE*)_T68A;_T68C=Cyc_yyget_YY46(_T68B);_T68D=two;_T68E=_T68D.f2;_T18B2.f2=Cyc_List_append(_T68C,_T68E);_T686=_T18B2;}yyval=Cyc_YY36(_T686);goto _LL0;}case 161: _T690=yyr;{struct Cyc_List_List*_T18B2=_region_malloc(_T690,0U,sizeof(struct Cyc_List_List));_T691=yyyvsp;_T692=& _T691[0].v;_T693=(union Cyc_YYSTYPE*)_T692;
# 1906 "parse.y"
_T18B2->hd=Cyc_yyget_YY29(_T693);_T18B2->tl=0;_T68F=(struct Cyc_List_List*)_T18B2;}yyval=Cyc_YY30(_T68F);goto _LL0;case 162: _T695=yyr;{struct Cyc_List_List*_T18B2=_region_malloc(_T695,0U,sizeof(struct Cyc_List_List));_T696=yyyvsp;_T697=& _T696[2].v;_T698=(union Cyc_YYSTYPE*)_T697;
# 1908 "parse.y"
_T18B2->hd=Cyc_yyget_YY29(_T698);_T699=yyyvsp;_T69A=& _T699[0].v;_T69B=(union Cyc_YYSTYPE*)_T69A;_T18B2->tl=Cyc_yyget_YY30(_T69B);_T694=(struct Cyc_List_List*)_T18B2;}yyval=Cyc_YY30(_T694);goto _LL0;case 163: _T69D=yyr;{struct _tuple12*_T18B2=_region_malloc(_T69D,0U,sizeof(struct _tuple12));_T69E=yyyvsp;_T69F=& _T69E[0].v;_T6A0=(union Cyc_YYSTYPE*)_T69F;
# 1913 "parse.y"
_T18B2->f0=Cyc_yyget_YY28(_T6A0);_T18B2->f1=0;_T6A1=yyyvsp;_T6A2=& _T6A1[1].v;_T6A3=(union Cyc_YYSTYPE*)_T6A2;_T18B2->f2=Cyc_yyget_YY61(_T6A3);_T69C=(struct _tuple12*)_T18B2;}yyval=Cyc_YY29(_T69C);goto _LL0;case 164: _T6A5=yyr;{struct _tuple12*_T18B2=_region_malloc(_T6A5,0U,sizeof(struct _tuple12));{struct _tuple0*_T18B3=_cycalloc(sizeof(struct _tuple0));
# 1917 "parse.y"
_T18B3->f0=Cyc_Absyn_Rel_n(0);{struct _fat_ptr*_T18B4=_cycalloc(sizeof(struct _fat_ptr));*_T18B4=_tag_fat("",sizeof(char),1U);_T6A7=(struct _fat_ptr*)_T18B4;}_T18B3->f1=_T6A7;_T6A6=(struct _tuple0*)_T18B3;}_T18B2->f0.id=_T6A6;_T18B2->f0.varloc=0U;_T18B2->f0.tms=0;_T6A8=yyyvsp;_T6A9=& _T6A8[1].v;_T6AA=(union Cyc_YYSTYPE*)_T6A9;_T18B2->f1=Cyc_yyget_Exp_tok(_T6AA);_T18B2->f2=0;_T6A4=(struct _tuple12*)_T18B2;}yyval=Cyc_YY29(_T6A4);goto _LL0;case 165: _T6AC=yyr;{struct _tuple12*_T18B2=_region_malloc(_T6AC,0U,sizeof(struct _tuple12));{struct _tuple0*_T18B3=_cycalloc(sizeof(struct _tuple0));
# 1922 "parse.y"
_T18B3->f0=Cyc_Absyn_Rel_n(0);{struct _fat_ptr*_T18B4=_cycalloc(sizeof(struct _fat_ptr));*_T18B4=_tag_fat("",sizeof(char),1U);_T6AE=(struct _fat_ptr*)_T18B4;}_T18B3->f1=_T6AE;_T6AD=(struct _tuple0*)_T18B3;}_T18B2->f0.id=_T6AD;_T18B2->f0.varloc=0U;_T18B2->f0.tms=0;_T18B2->f1=0;_T18B2->f2=0;_T6AB=(struct _tuple12*)_T18B2;}yyval=Cyc_YY29(_T6AB);goto _LL0;case 166: _T6B0=yyr;{struct _tuple12*_T18B2=_region_malloc(_T6B0,0U,sizeof(struct _tuple12));_T6B1=yyyvsp;_T6B2=& _T6B1[0].v;_T6B3=(union Cyc_YYSTYPE*)_T6B2;
# 1925 "parse.y"
_T18B2->f0=Cyc_yyget_YY28(_T6B3);_T6B4=yyyvsp;_T6B5=& _T6B4[2].v;_T6B6=(union Cyc_YYSTYPE*)_T6B5;_T18B2->f1=Cyc_yyget_Exp_tok(_T6B6);_T18B2->f2=0;_T6AF=(struct _tuple12*)_T18B2;}yyval=Cyc_YY29(_T6AF);goto _LL0;case 167: _T6B7=yyyvsp;_T6B8=& _T6B7[2].v;_T6B9=(union Cyc_YYSTYPE*)_T6B8;_T6BA=
# 1929 "parse.y"
Cyc_yyget_Exp_tok(_T6B9);yyval=Cyc_YY61(_T6BA);goto _LL0;case 168:
# 1930 "parse.y"
 yyval=Cyc_YY61(0);goto _LL0;case 169: _T6BB=yyyvsp;_T6BC=& _T6BB[0].v;_T6BD=(union Cyc_YYSTYPE*)_T6BC;{
# 1936 "parse.y"
int is_extensible=Cyc_yyget_YY32(_T6BD);_T6BF=Cyc_List_map_c;{
struct Cyc_List_List*(*_T18B2)(struct Cyc_Absyn_Tvar*(*)(unsigned,void*),unsigned,struct Cyc_List_List*)=(struct Cyc_List_List*(*)(struct Cyc_Absyn_Tvar*(*)(unsigned,void*),unsigned,struct Cyc_List_List*))_T6BF;_T6BE=_T18B2;}_T6C0=yyyvsp;_T6C1=_T6C0[2];_T6C2=_T6C1.l;_T6C3=_T6C2.first_line;_T6C4=Cyc_Position_loc_to_seg(_T6C3);_T6C5=yyyvsp;_T6C6=& _T6C5[2].v;_T6C7=(union Cyc_YYSTYPE*)_T6C6;_T6C8=Cyc_yyget_YY41(_T6C7);{struct Cyc_List_List*ts=_T6BE(Cyc_Parse_typ2tvar,_T6C4,_T6C8);_T6C9=yyyvsp;_T6CA=& _T6C9[1].v;_T6CB=(union Cyc_YYSTYPE*)_T6CA;_T6CC=
Cyc_yyget_QualId_tok(_T6CB);_T6CD=ts;{struct Cyc_Core_Opt*_T18B2=_cycalloc(sizeof(struct Cyc_Core_Opt));_T6CF=yyyvsp;_T6D0=& _T6CF[4].v;_T6D1=(union Cyc_YYSTYPE*)_T6D0;_T18B2->v=Cyc_yyget_YY35(_T6D1);_T6CE=(struct Cyc_Core_Opt*)_T18B2;}_T6D2=is_extensible;_T6D3=yyyvsp;_T6D4=_T6D3[0];_T6D5=_T6D4.l;_T6D6=_T6D5.first_line;_T6D7=
Cyc_Position_loc_to_seg(_T6D6);{
# 1938
struct Cyc_Absyn_TypeDecl*dd=Cyc_Absyn_datatype_tdecl(2U,_T6CC,_T6CD,_T6CE,_T6D2,_T6D7);{struct Cyc_Absyn_TypeDeclType_Absyn_Type_struct*_T18B2=_cycalloc(sizeof(struct Cyc_Absyn_TypeDeclType_Absyn_Type_struct));_T18B2->tag=10;
# 1940
_T18B2->f1=dd;_T18B2->f2=0;_T6D8=(struct Cyc_Absyn_TypeDeclType_Absyn_Type_struct*)_T18B2;}_T6D9=(void*)_T6D8;_T6DA=yyyvsp;_T6DB=_T6DA[0];_T6DC=_T6DB.l;_T6DD=_T6DC.first_line;_T6DE=Cyc_Position_loc_to_seg(_T6DD);_T6DF=Cyc_Parse_type_spec(_T6D9,_T6DE);yyval=Cyc_YY21(_T6DF);goto _LL0;}}}case 170: _T6E0=yyyvsp;_T6E1=& _T6E0[0].v;_T6E2=(union Cyc_YYSTYPE*)_T6E1;{
# 1943 "parse.y"
int is_extensible=Cyc_yyget_YY32(_T6E2);{struct Cyc_Absyn_UnknownDatatypeInfo _T18B2;_T6E4=yyyvsp;_T6E5=& _T6E4[1].v;_T6E6=(union Cyc_YYSTYPE*)_T6E5;
_T18B2.name=Cyc_yyget_QualId_tok(_T6E6);_T18B2.is_extensible=is_extensible;_T6E3=_T18B2;}_T6E7=Cyc_Absyn_UnknownDatatype(_T6E3);_T6E8=yyyvsp;_T6E9=& _T6E8[2].v;_T6EA=(union Cyc_YYSTYPE*)_T6E9;_T6EB=Cyc_yyget_YY41(_T6EA);_T6EC=Cyc_Absyn_datatype_type(_T6E7,_T6EB);_T6ED=yyyvsp;_T6EE=_T6ED[0];_T6EF=_T6EE.l;_T6F0=_T6EF.first_line;_T6F1=Cyc_Position_loc_to_seg(_T6F0);_T6F2=Cyc_Parse_type_spec(_T6EC,_T6F1);yyval=Cyc_YY21(_T6F2);goto _LL0;}case 171: _T6F3=yyyvsp;_T6F4=& _T6F3[0].v;_T6F5=(union Cyc_YYSTYPE*)_T6F4;{
# 1947 "parse.y"
int is_extensible=Cyc_yyget_YY32(_T6F5);{struct Cyc_Absyn_UnknownDatatypeFieldInfo _T18B2;_T6F7=yyyvsp;_T6F8=& _T6F7[1].v;_T6F9=(union Cyc_YYSTYPE*)_T6F8;
_T18B2.datatype_name=Cyc_yyget_QualId_tok(_T6F9);_T6FA=yyyvsp;_T6FB=& _T6FA[3].v;_T6FC=(union Cyc_YYSTYPE*)_T6FB;_T18B2.field_name=Cyc_yyget_QualId_tok(_T6FC);_T18B2.is_extensible=is_extensible;_T6F6=_T18B2;}_T6FD=Cyc_Absyn_UnknownDatatypefield(_T6F6);_T6FE=yyyvsp;_T6FF=& _T6FE[4].v;_T700=(union Cyc_YYSTYPE*)_T6FF;_T701=Cyc_yyget_YY41(_T700);_T702=Cyc_Absyn_datatype_field_type(_T6FD,_T701);_T703=yyyvsp;_T704=_T703[0];_T705=_T704.l;_T706=_T705.first_line;_T707=Cyc_Position_loc_to_seg(_T706);_T708=Cyc_Parse_type_spec(_T702,_T707);yyval=Cyc_YY21(_T708);goto _LL0;}case 172:
# 1953 "parse.y"
 yyval=Cyc_YY32(0);goto _LL0;case 173:
# 1954 "parse.y"
 yyval=Cyc_YY32(1);goto _LL0;case 174:{struct Cyc_List_List*_T18B2=_cycalloc(sizeof(struct Cyc_List_List));_T70A=yyyvsp;_T70B=& _T70A[0].v;_T70C=(union Cyc_YYSTYPE*)_T70B;
# 1958 "parse.y"
_T18B2->hd=Cyc_yyget_YY34(_T70C);_T18B2->tl=0;_T709=(struct Cyc_List_List*)_T18B2;}yyval=Cyc_YY35(_T709);goto _LL0;case 175:{struct Cyc_List_List*_T18B2=_cycalloc(sizeof(struct Cyc_List_List));_T70E=yyyvsp;_T70F=& _T70E[0].v;_T710=(union Cyc_YYSTYPE*)_T70F;
# 1959 "parse.y"
_T18B2->hd=Cyc_yyget_YY34(_T710);_T18B2->tl=0;_T70D=(struct Cyc_List_List*)_T18B2;}yyval=Cyc_YY35(_T70D);goto _LL0;case 176:{struct Cyc_List_List*_T18B2=_cycalloc(sizeof(struct Cyc_List_List));_T712=yyyvsp;_T713=& _T712[0].v;_T714=(union Cyc_YYSTYPE*)_T713;
# 1960 "parse.y"
_T18B2->hd=Cyc_yyget_YY34(_T714);_T715=yyyvsp;_T716=& _T715[2].v;_T717=(union Cyc_YYSTYPE*)_T716;_T18B2->tl=Cyc_yyget_YY35(_T717);_T711=(struct Cyc_List_List*)_T18B2;}yyval=Cyc_YY35(_T711);goto _LL0;case 177:{struct Cyc_List_List*_T18B2=_cycalloc(sizeof(struct Cyc_List_List));_T719=yyyvsp;_T71A=& _T719[0].v;_T71B=(union Cyc_YYSTYPE*)_T71A;
# 1961 "parse.y"
_T18B2->hd=Cyc_yyget_YY34(_T71B);_T71C=yyyvsp;_T71D=& _T71C[2].v;_T71E=(union Cyc_YYSTYPE*)_T71D;_T18B2->tl=Cyc_yyget_YY35(_T71E);_T718=(struct Cyc_List_List*)_T18B2;}yyval=Cyc_YY35(_T718);goto _LL0;case 178:
# 1965 "parse.y"
 yyval=Cyc_YY33(2U);goto _LL0;case 179:
# 1966 "parse.y"
 yyval=Cyc_YY33(3U);goto _LL0;case 180:
# 1967 "parse.y"
 yyval=Cyc_YY33(0U);goto _LL0;case 181:{struct Cyc_Absyn_Datatypefield*_T18B2=_cycalloc(sizeof(struct Cyc_Absyn_Datatypefield));_T720=yyyvsp;_T721=& _T720[1].v;_T722=(union Cyc_YYSTYPE*)_T721;
# 1971 "parse.y"
_T18B2->name=Cyc_yyget_QualId_tok(_T722);_T18B2->typs=0;_T723=yyyvsp;_T724=_T723[0];_T725=_T724.l;_T726=_T725.first_line;_T18B2->loc=Cyc_Position_loc_to_seg(_T726);_T727=yyyvsp;_T728=& _T727[0].v;_T729=(union Cyc_YYSTYPE*)_T728;_T18B2->sc=Cyc_yyget_YY33(_T729);_T71F=(struct Cyc_Absyn_Datatypefield*)_T18B2;}yyval=Cyc_YY34(_T71F);goto _LL0;case 182: _T72B=Cyc_List_map_c;{
# 1973 "parse.y"
struct Cyc_List_List*(*_T18B2)(struct _tuple19*(*)(unsigned,struct _tuple8*),unsigned,struct Cyc_List_List*)=(struct Cyc_List_List*(*)(struct _tuple19*(*)(unsigned,struct _tuple8*),unsigned,struct Cyc_List_List*))_T72B;_T72A=_T18B2;}{struct _tuple19*(*_T18B2)(unsigned,struct _tuple8*)=(struct _tuple19*(*)(unsigned,struct _tuple8*))Cyc_Parse_get_tqual_typ;_T72C=_T18B2;}_T72D=yyyvsp;_T72E=_T72D[3];_T72F=_T72E.l;_T730=_T72F.first_line;_T731=Cyc_Position_loc_to_seg(_T730);_T732=yyyvsp;_T733=& _T732[3].v;_T734=(union Cyc_YYSTYPE*)_T733;_T735=Cyc_yyget_YY39(_T734);_T736=Cyc_List_imp_rev(_T735);{struct Cyc_List_List*typs=_T72A(_T72C,_T731,_T736);{struct Cyc_Absyn_Datatypefield*_T18B2=_cycalloc(sizeof(struct Cyc_Absyn_Datatypefield));_T738=yyyvsp;_T739=& _T738[1].v;_T73A=(union Cyc_YYSTYPE*)_T739;
_T18B2->name=Cyc_yyget_QualId_tok(_T73A);_T18B2->typs=typs;_T73B=yyyvsp;_T73C=_T73B[0];_T73D=_T73C.l;_T73E=_T73D.first_line;_T18B2->loc=Cyc_Position_loc_to_seg(_T73E);_T73F=yyyvsp;_T740=& _T73F[0].v;_T741=(union Cyc_YYSTYPE*)_T740;_T18B2->sc=Cyc_yyget_YY33(_T741);_T737=(struct Cyc_Absyn_Datatypefield*)_T18B2;}yyval=Cyc_YY34(_T737);goto _LL0;}case 183: _T742=yyyvsp;_T743=_T742[0];
# 1979 "parse.y"
yyval=_T743.v;goto _LL0;case 184: _T744=yyyvsp;_T745=& _T744[1].v;_T746=(union Cyc_YYSTYPE*)_T745;{
# 1981 "parse.y"
struct Cyc_Parse_Declarator two=Cyc_yyget_YY28(_T746);{struct Cyc_Parse_Declarator _T18B2;_T748=two;
_T18B2.id=_T748.id;_T749=two;_T18B2.varloc=_T749.varloc;_T74A=yyyvsp;_T74B=& _T74A[0].v;_T74C=(union Cyc_YYSTYPE*)_T74B;_T74D=Cyc_yyget_YY27(_T74C);_T74E=two;_T74F=_T74E.tms;_T18B2.tms=Cyc_List_imp_append(_T74D,_T74F);_T747=_T18B2;}yyval=Cyc_YY28(_T747);goto _LL0;}case 185: _T750=yyyvsp;_T751=_T750[0];
# 1988 "parse.y"
yyval=_T751.v;goto _LL0;case 186: _T752=yyyvsp;_T753=& _T752[1].v;_T754=(union Cyc_YYSTYPE*)_T753;{
# 1990 "parse.y"
struct Cyc_Parse_Declarator two=Cyc_yyget_YY28(_T754);{struct Cyc_Parse_Declarator _T18B2;_T756=two;
_T18B2.id=_T756.id;_T757=two;_T18B2.varloc=_T757.varloc;_T758=yyyvsp;_T759=& _T758[0].v;_T75A=(union Cyc_YYSTYPE*)_T759;_T75B=Cyc_yyget_YY27(_T75A);_T75C=two;_T75D=_T75C.tms;_T18B2.tms=Cyc_List_imp_append(_T75B,_T75D);_T755=_T18B2;}yyval=Cyc_YY28(_T755);goto _LL0;}case 187:{struct Cyc_Parse_Declarator _T18B2;_T75F=yyyvsp;_T760=& _T75F[0].v;_T761=(union Cyc_YYSTYPE*)_T760;
# 1995 "parse.y"
_T18B2.id=Cyc_yyget_QualId_tok(_T761);_T762=yyyvsp;_T763=_T762[0];_T764=_T763.l;_T765=_T764.first_line;_T18B2.varloc=Cyc_Position_loc_to_seg(_T765);_T18B2.tms=0;_T75E=_T18B2;}yyval=Cyc_YY28(_T75E);goto _LL0;case 188: _T766=yyyvsp;_T767=_T766[1];
# 1996 "parse.y"
yyval=_T767.v;goto _LL0;case 189: _T768=yyyvsp;_T769=& _T768[2].v;_T76A=(union Cyc_YYSTYPE*)_T769;{
# 2000 "parse.y"
struct Cyc_Parse_Declarator d=Cyc_yyget_YY28(_T76A);_T76C=yyr;{struct Cyc_List_List*_T18B2=_region_malloc(_T76C,0U,sizeof(struct Cyc_List_List));_T76E=yyr;{struct Cyc_Absyn_Attributes_mod_Absyn_Type_modifier_struct*_T18B3=_region_malloc(_T76E,0U,sizeof(struct Cyc_Absyn_Attributes_mod_Absyn_Type_modifier_struct));_T18B3->tag=5;_T76F=yyyvsp;_T770=_T76F[1];_T771=_T770.l;_T772=_T771.first_line;
_T18B3->f1=Cyc_Position_loc_to_seg(_T772);_T773=yyyvsp;_T774=& _T773[1].v;_T775=(union Cyc_YYSTYPE*)_T774;_T18B3->f2=Cyc_yyget_YY46(_T775);_T76D=(struct Cyc_Absyn_Attributes_mod_Absyn_Type_modifier_struct*)_T18B3;}_T18B2->hd=(void*)_T76D;_T776=d;_T18B2->tl=_T776.tms;_T76B=(struct Cyc_List_List*)_T18B2;}d.tms=_T76B;_T777=yyyvsp;_T778=_T777[2];
yyval=_T778.v;goto _LL0;}case 190:{struct Cyc_Parse_Declarator _T18B2;_T77A=yyyvsp;_T77B=& _T77A[0].v;_T77C=(union Cyc_YYSTYPE*)_T77B;_T77D=
# 2005 "parse.y"
Cyc_yyget_YY28(_T77C);_T18B2.id=_T77D.id;_T77E=yyyvsp;_T77F=& _T77E[0].v;_T780=(union Cyc_YYSTYPE*)_T77F;_T781=Cyc_yyget_YY28(_T780);_T18B2.varloc=_T781.varloc;_T783=yyr;{struct Cyc_List_List*_T18B3=_region_malloc(_T783,0U,sizeof(struct Cyc_List_List));_T785=yyr;{struct Cyc_Absyn_Carray_mod_Absyn_Type_modifier_struct*_T18B4=_region_malloc(_T785,0U,sizeof(struct Cyc_Absyn_Carray_mod_Absyn_Type_modifier_struct));_T18B4->tag=0;_T786=yyyvsp;_T787=& _T786[3].v;_T788=(union Cyc_YYSTYPE*)_T787;_T18B4->f1=Cyc_yyget_YY54(_T788);_T789=yyyvsp;_T78A=_T789[3];_T78B=_T78A.l;_T78C=_T78B.first_line;_T18B4->f2=Cyc_Position_loc_to_seg(_T78C);_T784=(struct Cyc_Absyn_Carray_mod_Absyn_Type_modifier_struct*)_T18B4;}_T18B3->hd=(void*)_T784;_T78D=yyyvsp;_T78E=& _T78D[0].v;_T78F=(union Cyc_YYSTYPE*)_T78E;_T790=Cyc_yyget_YY28(_T78F);_T18B3->tl=_T790.tms;_T782=(struct Cyc_List_List*)_T18B3;}_T18B2.tms=_T782;_T779=_T18B2;}yyval=Cyc_YY28(_T779);goto _LL0;case 191:{struct Cyc_Parse_Declarator _T18B2;_T792=yyyvsp;_T793=& _T792[0].v;_T794=(union Cyc_YYSTYPE*)_T793;_T795=
# 2007 "parse.y"
Cyc_yyget_YY28(_T794);_T18B2.id=_T795.id;_T796=yyyvsp;_T797=& _T796[0].v;_T798=(union Cyc_YYSTYPE*)_T797;_T799=Cyc_yyget_YY28(_T798);_T18B2.varloc=_T799.varloc;_T79B=yyr;{struct Cyc_List_List*_T18B3=_region_malloc(_T79B,0U,sizeof(struct Cyc_List_List));_T79D=yyr;{struct Cyc_Absyn_ConstArray_mod_Absyn_Type_modifier_struct*_T18B4=_region_malloc(_T79D,0U,sizeof(struct Cyc_Absyn_ConstArray_mod_Absyn_Type_modifier_struct));_T18B4->tag=1;_T79E=yyyvsp;_T79F=& _T79E[2].v;_T7A0=(union Cyc_YYSTYPE*)_T79F;
_T18B4->f1=Cyc_yyget_Exp_tok(_T7A0);_T7A1=yyyvsp;_T7A2=& _T7A1[4].v;_T7A3=(union Cyc_YYSTYPE*)_T7A2;_T18B4->f2=Cyc_yyget_YY54(_T7A3);_T7A4=yyyvsp;_T7A5=_T7A4[4];_T7A6=_T7A5.l;_T7A7=_T7A6.first_line;_T18B4->f3=Cyc_Position_loc_to_seg(_T7A7);_T79C=(struct Cyc_Absyn_ConstArray_mod_Absyn_Type_modifier_struct*)_T18B4;}_T18B3->hd=(void*)_T79C;_T7A8=yyyvsp;_T7A9=& _T7A8[0].v;_T7AA=(union Cyc_YYSTYPE*)_T7A9;_T7AB=Cyc_yyget_YY28(_T7AA);_T18B3->tl=_T7AB.tms;_T79A=(struct Cyc_List_List*)_T18B3;}_T18B2.tms=_T79A;_T791=_T18B2;}
# 2007
yyval=Cyc_YY28(_T791);goto _LL0;case 192: _T7AC=yyyvsp;_T7AD=& _T7AC[2].v;_T7AE=(union Cyc_YYSTYPE*)_T7AD;{
# 2010 "parse.y"
struct _tuple27*_T18B2=Cyc_yyget_YY40(_T7AE);struct Cyc_List_List*_T18B3;struct Cyc_List_List*_T18B4;void*_T18B5;struct Cyc_Absyn_VarargInfo*_T18B6;int _T18B7;struct Cyc_List_List*_T18B8;{struct _tuple27 _T18B9=*_T18B2;_T18B8=_T18B9.f0;_T18B7=_T18B9.f1;_T18B6=_T18B9.f2;_T18B5=_T18B9.f3;_T18B4=_T18B9.f4;_T18B3=_T18B9.f5;}{struct Cyc_List_List*lis=_T18B8;int b=_T18B7;struct Cyc_Absyn_VarargInfo*c=_T18B6;void*eff=_T18B5;struct Cyc_List_List*ec=_T18B4;struct Cyc_List_List*qb=_T18B3;_T7AF=yyyvsp;_T7B0=& _T7AF[4].v;_T7B1=(union Cyc_YYSTYPE*)_T7B0;{
struct _tuple21 _T18B9=Cyc_yyget_YY62(_T7B1);struct Cyc_Absyn_Exp*_T18BA;struct Cyc_Absyn_Exp*_T18BB;struct Cyc_Absyn_Exp*_T18BC;struct Cyc_Absyn_Exp*_T18BD;_T18BD=_T18B9.f0;_T18BC=_T18B9.f1;_T18BB=_T18B9.f2;_T18BA=_T18B9.f3;{struct Cyc_Absyn_Exp*chk=_T18BD;struct Cyc_Absyn_Exp*req=_T18BC;struct Cyc_Absyn_Exp*ens=_T18BB;struct Cyc_Absyn_Exp*thrws=_T18BA;{struct Cyc_Parse_Declarator _T18BE;_T7B3=yyyvsp;_T7B4=& _T7B3[0].v;_T7B5=(union Cyc_YYSTYPE*)_T7B4;_T7B6=
Cyc_yyget_YY28(_T7B5);_T18BE.id=_T7B6.id;_T7B7=yyyvsp;_T7B8=& _T7B7[0].v;_T7B9=(union Cyc_YYSTYPE*)_T7B8;_T7BA=Cyc_yyget_YY28(_T7B9);_T18BE.varloc=_T7BA.varloc;_T7BC=yyr;{struct Cyc_List_List*_T18BF=_region_malloc(_T7BC,0U,sizeof(struct Cyc_List_List));_T7BE=yyr;{struct Cyc_Absyn_Function_mod_Absyn_Type_modifier_struct*_T18C0=_region_malloc(_T7BE,0U,sizeof(struct Cyc_Absyn_Function_mod_Absyn_Type_modifier_struct));_T18C0->tag=3;_T7C0=yyr;{struct Cyc_Absyn_WithTypes_Absyn_Funcparams_struct*_T18C1=_region_malloc(_T7C0,0U,sizeof(struct Cyc_Absyn_WithTypes_Absyn_Funcparams_struct));_T18C1->tag=1;_T18C1->f1=lis;_T18C1->f2=b;_T18C1->f3=c;_T18C1->f4=eff;_T18C1->f5=ec;_T18C1->f6=qb;_T18C1->f7=chk;_T18C1->f8=req;_T18C1->f9=ens;_T18C1->f10=thrws;_T7BF=(struct Cyc_Absyn_WithTypes_Absyn_Funcparams_struct*)_T18C1;}_T18C0->f1=(void*)_T7BF;_T7BD=(struct Cyc_Absyn_Function_mod_Absyn_Type_modifier_struct*)_T18C0;}_T18BF->hd=(void*)_T7BD;_T7C1=yyyvsp;_T7C2=& _T7C1[0].v;_T7C3=(union Cyc_YYSTYPE*)_T7C2;_T7C4=Cyc_yyget_YY28(_T7C3);_T18BF->tl=_T7C4.tms;_T7BB=(struct Cyc_List_List*)_T18BF;}_T18BE.tms=_T7BB;_T7B2=_T18BE;}yyval=Cyc_YY28(_T7B2);goto _LL0;}}}}case 193:{struct Cyc_Parse_Declarator _T18B2;_T7C6=yyyvsp;_T7C7=& _T7C6[0].v;_T7C8=(union Cyc_YYSTYPE*)_T7C7;_T7C9=
# 2015 "parse.y"
Cyc_yyget_YY28(_T7C8);_T18B2.id=_T7C9.id;_T7CA=yyyvsp;_T7CB=& _T7CA[0].v;_T7CC=(union Cyc_YYSTYPE*)_T7CB;_T7CD=Cyc_yyget_YY28(_T7CC);_T18B2.varloc=_T7CD.varloc;_T7CF=yyr;{struct Cyc_List_List*_T18B3=_region_malloc(_T7CF,0U,sizeof(struct Cyc_List_List));_T7D1=yyr;{struct Cyc_Absyn_Function_mod_Absyn_Type_modifier_struct*_T18B4=_region_malloc(_T7D1,0U,sizeof(struct Cyc_Absyn_Function_mod_Absyn_Type_modifier_struct));_T18B4->tag=3;_T7D3=yyr;{struct Cyc_Absyn_NoTypes_Absyn_Funcparams_struct*_T18B5=_region_malloc(_T7D3,0U,sizeof(struct Cyc_Absyn_NoTypes_Absyn_Funcparams_struct));_T18B5->tag=0;_T7D4=yyyvsp;_T7D5=& _T7D4[2].v;_T7D6=(union Cyc_YYSTYPE*)_T7D5;_T18B5->f1=Cyc_yyget_YY37(_T7D6);_T7D7=yyyvsp;_T7D8=_T7D7[0];_T7D9=_T7D8.l;_T7DA=_T7D9.first_line;_T18B5->f2=Cyc_Position_loc_to_seg(_T7DA);_T7D2=(struct Cyc_Absyn_NoTypes_Absyn_Funcparams_struct*)_T18B5;}_T18B4->f1=(void*)_T7D2;_T7D0=(struct Cyc_Absyn_Function_mod_Absyn_Type_modifier_struct*)_T18B4;}_T18B3->hd=(void*)_T7D0;_T7DB=yyyvsp;_T7DC=& _T7DB[0].v;_T7DD=(union Cyc_YYSTYPE*)_T7DC;_T7DE=Cyc_yyget_YY28(_T7DD);_T18B3->tl=_T7DE.tms;_T7CE=(struct Cyc_List_List*)_T18B3;}_T18B2.tms=_T7CE;_T7C5=_T18B2;}yyval=Cyc_YY28(_T7C5);goto _LL0;case 194: _T7E0=Cyc_List_map_c;{
# 2018
struct Cyc_List_List*(*_T18B2)(struct Cyc_Absyn_Tvar*(*)(unsigned,void*),unsigned,struct Cyc_List_List*)=(struct Cyc_List_List*(*)(struct Cyc_Absyn_Tvar*(*)(unsigned,void*),unsigned,struct Cyc_List_List*))_T7E0;_T7DF=_T18B2;}_T7E1=yyyvsp;_T7E2=_T7E1[1];_T7E3=_T7E2.l;_T7E4=_T7E3.first_line;_T7E5=Cyc_Position_loc_to_seg(_T7E4);_T7E6=yyyvsp;_T7E7=& _T7E6[2].v;_T7E8=(union Cyc_YYSTYPE*)_T7E7;_T7E9=Cyc_yyget_YY41(_T7E8);_T7EA=Cyc_List_imp_rev(_T7E9);{struct Cyc_List_List*ts=_T7DF(Cyc_Parse_typ2tvar,_T7E5,_T7EA);{struct Cyc_Parse_Declarator _T18B2;_T7EC=yyyvsp;_T7ED=& _T7EC[0].v;_T7EE=(union Cyc_YYSTYPE*)_T7ED;_T7EF=
Cyc_yyget_YY28(_T7EE);_T18B2.id=_T7EF.id;_T7F0=yyyvsp;_T7F1=& _T7F0[0].v;_T7F2=(union Cyc_YYSTYPE*)_T7F1;_T7F3=Cyc_yyget_YY28(_T7F2);_T18B2.varloc=_T7F3.varloc;_T7F5=yyr;{struct Cyc_List_List*_T18B3=_region_malloc(_T7F5,0U,sizeof(struct Cyc_List_List));_T7F7=yyr;{struct Cyc_Absyn_TypeParams_mod_Absyn_Type_modifier_struct*_T18B4=_region_malloc(_T7F7,0U,sizeof(struct Cyc_Absyn_TypeParams_mod_Absyn_Type_modifier_struct));_T18B4->tag=4;_T18B4->f1=ts;_T7F8=yyyvsp;_T7F9=_T7F8[0];_T7FA=_T7F9.l;_T7FB=_T7FA.first_line;_T18B4->f2=Cyc_Position_loc_to_seg(_T7FB);_T18B4->f3=0;_T7F6=(struct Cyc_Absyn_TypeParams_mod_Absyn_Type_modifier_struct*)_T18B4;}_T18B3->hd=(void*)_T7F6;_T7FC=yyyvsp;_T7FD=& _T7FC[0].v;_T7FE=(union Cyc_YYSTYPE*)_T7FD;_T7FF=Cyc_yyget_YY28(_T7FE);_T18B3->tl=_T7FF.tms;_T7F4=(struct Cyc_List_List*)_T18B3;}_T18B2.tms=_T7F4;_T7EB=_T18B2;}yyval=Cyc_YY28(_T7EB);goto _LL0;}case 195:{struct Cyc_Parse_Declarator _T18B2;_T801=yyyvsp;_T802=& _T801[0].v;_T803=(union Cyc_YYSTYPE*)_T802;_T804=
# 2022 "parse.y"
Cyc_yyget_YY28(_T803);_T18B2.id=_T804.id;_T805=yyyvsp;_T806=& _T805[0].v;_T807=(union Cyc_YYSTYPE*)_T806;_T808=Cyc_yyget_YY28(_T807);_T18B2.varloc=_T808.varloc;_T80A=yyr;{struct Cyc_List_List*_T18B3=_region_malloc(_T80A,0U,sizeof(struct Cyc_List_List));_T80C=yyr;{struct Cyc_Absyn_Attributes_mod_Absyn_Type_modifier_struct*_T18B4=_region_malloc(_T80C,0U,sizeof(struct Cyc_Absyn_Attributes_mod_Absyn_Type_modifier_struct));_T18B4->tag=5;_T80D=yyyvsp;_T80E=_T80D[1];_T80F=_T80E.l;_T810=_T80F.first_line;_T18B4->f1=Cyc_Position_loc_to_seg(_T810);_T811=yyyvsp;_T812=& _T811[1].v;_T813=(union Cyc_YYSTYPE*)_T812;_T18B4->f2=Cyc_yyget_YY46(_T813);_T80B=(struct Cyc_Absyn_Attributes_mod_Absyn_Type_modifier_struct*)_T18B4;}_T18B3->hd=(void*)_T80B;_T814=yyyvsp;_T815=& _T814[0].v;_T816=(union Cyc_YYSTYPE*)_T815;_T817=
Cyc_yyget_YY28(_T816);_T18B3->tl=_T817.tms;_T809=(struct Cyc_List_List*)_T18B3;}
# 2022
_T18B2.tms=_T809;_T800=_T18B2;}yyval=Cyc_YY28(_T800);goto _LL0;case 196:{struct Cyc_Parse_Declarator _T18B2;_T819=yyyvsp;_T81A=& _T819[0].v;_T81B=(union Cyc_YYSTYPE*)_T81A;
# 2029 "parse.y"
_T18B2.id=Cyc_yyget_QualId_tok(_T81B);_T81C=yyyvsp;_T81D=_T81C[0];_T81E=_T81D.l;_T81F=_T81E.first_line;_T18B2.varloc=Cyc_Position_loc_to_seg(_T81F);_T18B2.tms=0;_T818=_T18B2;}yyval=Cyc_YY28(_T818);goto _LL0;case 197:{struct Cyc_Parse_Declarator _T18B2;_T821=yyyvsp;_T822=& _T821[0].v;_T823=(union Cyc_YYSTYPE*)_T822;
# 2030 "parse.y"
_T18B2.id=Cyc_yyget_QualId_tok(_T823);_T824=yyyvsp;_T825=_T824[0];_T826=_T825.l;_T827=_T826.first_line;_T18B2.varloc=Cyc_Position_loc_to_seg(_T827);_T18B2.tms=0;_T820=_T18B2;}yyval=Cyc_YY28(_T820);goto _LL0;case 198: _T828=yyyvsp;_T829=_T828[1];
# 2031 "parse.y"
yyval=_T829.v;goto _LL0;case 199: _T82A=yyyvsp;_T82B=& _T82A[2].v;_T82C=(union Cyc_YYSTYPE*)_T82B;{
# 2035 "parse.y"
struct Cyc_Parse_Declarator d=Cyc_yyget_YY28(_T82C);_T82E=yyr;{struct Cyc_List_List*_T18B2=_region_malloc(_T82E,0U,sizeof(struct Cyc_List_List));_T830=yyr;{struct Cyc_Absyn_Attributes_mod_Absyn_Type_modifier_struct*_T18B3=_region_malloc(_T830,0U,sizeof(struct Cyc_Absyn_Attributes_mod_Absyn_Type_modifier_struct));_T18B3->tag=5;_T831=yyyvsp;_T832=_T831[1];_T833=_T832.l;_T834=_T833.first_line;
_T18B3->f1=Cyc_Position_loc_to_seg(_T834);_T835=yyyvsp;_T836=& _T835[1].v;_T837=(union Cyc_YYSTYPE*)_T836;_T18B3->f2=Cyc_yyget_YY46(_T837);_T82F=(struct Cyc_Absyn_Attributes_mod_Absyn_Type_modifier_struct*)_T18B3;}_T18B2->hd=(void*)_T82F;_T838=d;_T18B2->tl=_T838.tms;_T82D=(struct Cyc_List_List*)_T18B2;}d.tms=_T82D;_T839=yyyvsp;_T83A=_T839[2];
yyval=_T83A.v;goto _LL0;}case 200: _T83B=yyyvsp;_T83C=& _T83B[0].v;_T83D=(union Cyc_YYSTYPE*)_T83C;{
# 2040 "parse.y"
struct Cyc_Parse_Declarator one=Cyc_yyget_YY28(_T83D);{struct Cyc_Parse_Declarator _T18B2;_T83F=one;
_T18B2.id=_T83F.id;_T840=one;_T18B2.varloc=_T840.varloc;_T842=yyr;{struct Cyc_List_List*_T18B3=_region_malloc(_T842,0U,sizeof(struct Cyc_List_List));_T844=yyr;{struct Cyc_Absyn_Carray_mod_Absyn_Type_modifier_struct*_T18B4=_region_malloc(_T844,0U,sizeof(struct Cyc_Absyn_Carray_mod_Absyn_Type_modifier_struct));_T18B4->tag=0;_T845=yyyvsp;_T846=& _T845[3].v;_T847=(union Cyc_YYSTYPE*)_T846;
_T18B4->f1=Cyc_yyget_YY54(_T847);_T848=yyyvsp;_T849=_T848[3];_T84A=_T849.l;_T84B=_T84A.first_line;_T18B4->f2=Cyc_Position_loc_to_seg(_T84B);_T843=(struct Cyc_Absyn_Carray_mod_Absyn_Type_modifier_struct*)_T18B4;}_T18B3->hd=(void*)_T843;_T84C=one;_T18B3->tl=_T84C.tms;_T841=(struct Cyc_List_List*)_T18B3;}_T18B2.tms=_T841;_T83E=_T18B2;}
# 2041
yyval=Cyc_YY28(_T83E);goto _LL0;}case 201: _T84D=yyyvsp;_T84E=& _T84D[0].v;_T84F=(union Cyc_YYSTYPE*)_T84E;{
# 2044 "parse.y"
struct Cyc_Parse_Declarator one=Cyc_yyget_YY28(_T84F);{struct Cyc_Parse_Declarator _T18B2;_T851=one;
_T18B2.id=_T851.id;_T852=one;_T18B2.varloc=_T852.varloc;_T854=yyr;{struct Cyc_List_List*_T18B3=_region_malloc(_T854,0U,sizeof(struct Cyc_List_List));_T856=yyr;{struct Cyc_Absyn_ConstArray_mod_Absyn_Type_modifier_struct*_T18B4=_region_malloc(_T856,0U,sizeof(struct Cyc_Absyn_ConstArray_mod_Absyn_Type_modifier_struct));_T18B4->tag=1;_T857=yyyvsp;_T858=& _T857[2].v;_T859=(union Cyc_YYSTYPE*)_T858;
_T18B4->f1=Cyc_yyget_Exp_tok(_T859);_T85A=yyyvsp;_T85B=& _T85A[4].v;_T85C=(union Cyc_YYSTYPE*)_T85B;_T18B4->f2=Cyc_yyget_YY54(_T85C);_T85D=yyyvsp;_T85E=_T85D[4];_T85F=_T85E.l;_T860=_T85F.first_line;_T18B4->f3=Cyc_Position_loc_to_seg(_T860);_T855=(struct Cyc_Absyn_ConstArray_mod_Absyn_Type_modifier_struct*)_T18B4;}_T18B3->hd=(void*)_T855;_T861=one;
_T18B3->tl=_T861.tms;_T853=(struct Cyc_List_List*)_T18B3;}
# 2046
_T18B2.tms=_T853;_T850=_T18B2;}
# 2045
yyval=Cyc_YY28(_T850);goto _LL0;}case 202: _T862=yyyvsp;_T863=& _T862[2].v;_T864=(union Cyc_YYSTYPE*)_T863;{
# 2049 "parse.y"
struct _tuple27*_T18B2=Cyc_yyget_YY40(_T864);struct Cyc_List_List*_T18B3;struct Cyc_List_List*_T18B4;void*_T18B5;struct Cyc_Absyn_VarargInfo*_T18B6;int _T18B7;struct Cyc_List_List*_T18B8;{struct _tuple27 _T18B9=*_T18B2;_T18B8=_T18B9.f0;_T18B7=_T18B9.f1;_T18B6=_T18B9.f2;_T18B5=_T18B9.f3;_T18B4=_T18B9.f4;_T18B3=_T18B9.f5;}{struct Cyc_List_List*lis=_T18B8;int b=_T18B7;struct Cyc_Absyn_VarargInfo*c=_T18B6;void*eff=_T18B5;struct Cyc_List_List*ec=_T18B4;struct Cyc_List_List*qb=_T18B3;_T865=yyyvsp;_T866=& _T865[4].v;_T867=(union Cyc_YYSTYPE*)_T866;{
struct _tuple21 _T18B9=Cyc_yyget_YY62(_T867);struct Cyc_Absyn_Exp*_T18BA;struct Cyc_Absyn_Exp*_T18BB;struct Cyc_Absyn_Exp*_T18BC;struct Cyc_Absyn_Exp*_T18BD;_T18BD=_T18B9.f0;_T18BC=_T18B9.f1;_T18BB=_T18B9.f2;_T18BA=_T18B9.f3;{struct Cyc_Absyn_Exp*chk=_T18BD;struct Cyc_Absyn_Exp*req=_T18BC;struct Cyc_Absyn_Exp*ens=_T18BB;struct Cyc_Absyn_Exp*thrws=_T18BA;_T868=yyyvsp;_T869=& _T868[0].v;_T86A=(union Cyc_YYSTYPE*)_T869;{
struct Cyc_Parse_Declarator one=Cyc_yyget_YY28(_T86A);{struct Cyc_Parse_Declarator _T18BE;_T86C=one;
_T18BE.id=_T86C.id;_T86D=one;_T18BE.varloc=_T86D.varloc;_T86F=yyr;{struct Cyc_List_List*_T18BF=_region_malloc(_T86F,0U,sizeof(struct Cyc_List_List));_T871=yyr;{struct Cyc_Absyn_Function_mod_Absyn_Type_modifier_struct*_T18C0=_region_malloc(_T871,0U,sizeof(struct Cyc_Absyn_Function_mod_Absyn_Type_modifier_struct));_T18C0->tag=3;_T873=yyr;{struct Cyc_Absyn_WithTypes_Absyn_Funcparams_struct*_T18C1=_region_malloc(_T873,0U,sizeof(struct Cyc_Absyn_WithTypes_Absyn_Funcparams_struct));_T18C1->tag=1;_T18C1->f1=lis;_T18C1->f2=b;_T18C1->f3=c;_T18C1->f4=eff;_T18C1->f5=ec;_T18C1->f6=qb;_T18C1->f7=chk;_T18C1->f8=req;_T18C1->f9=ens;_T18C1->f10=thrws;_T872=(struct Cyc_Absyn_WithTypes_Absyn_Funcparams_struct*)_T18C1;}_T18C0->f1=(void*)_T872;_T870=(struct Cyc_Absyn_Function_mod_Absyn_Type_modifier_struct*)_T18C0;}_T18BF->hd=(void*)_T870;_T874=one;_T18BF->tl=_T874.tms;_T86E=(struct Cyc_List_List*)_T18BF;}_T18BE.tms=_T86E;_T86B=_T18BE;}yyval=Cyc_YY28(_T86B);goto _LL0;}}}}}case 203: _T875=yyyvsp;_T876=& _T875[0].v;_T877=(union Cyc_YYSTYPE*)_T876;{
# 2055 "parse.y"
struct Cyc_Parse_Declarator one=Cyc_yyget_YY28(_T877);{struct Cyc_Parse_Declarator _T18B2;_T879=one;
_T18B2.id=_T879.id;_T87A=one;_T18B2.varloc=_T87A.varloc;_T87C=yyr;{struct Cyc_List_List*_T18B3=_region_malloc(_T87C,0U,sizeof(struct Cyc_List_List));_T87E=yyr;{struct Cyc_Absyn_Function_mod_Absyn_Type_modifier_struct*_T18B4=_region_malloc(_T87E,0U,sizeof(struct Cyc_Absyn_Function_mod_Absyn_Type_modifier_struct));_T18B4->tag=3;_T880=yyr;{struct Cyc_Absyn_NoTypes_Absyn_Funcparams_struct*_T18B5=_region_malloc(_T880,0U,sizeof(struct Cyc_Absyn_NoTypes_Absyn_Funcparams_struct));_T18B5->tag=0;_T881=yyyvsp;_T882=& _T881[2].v;_T883=(union Cyc_YYSTYPE*)_T882;_T18B5->f1=Cyc_yyget_YY37(_T883);_T884=yyyvsp;_T885=_T884[0];_T886=_T885.l;_T887=_T886.first_line;_T18B5->f2=Cyc_Position_loc_to_seg(_T887);_T87F=(struct Cyc_Absyn_NoTypes_Absyn_Funcparams_struct*)_T18B5;}_T18B4->f1=(void*)_T87F;_T87D=(struct Cyc_Absyn_Function_mod_Absyn_Type_modifier_struct*)_T18B4;}_T18B3->hd=(void*)_T87D;_T888=one;_T18B3->tl=_T888.tms;_T87B=(struct Cyc_List_List*)_T18B3;}_T18B2.tms=_T87B;_T878=_T18B2;}yyval=Cyc_YY28(_T878);goto _LL0;}case 204: _T88A=Cyc_List_map_c;{
# 2059
struct Cyc_List_List*(*_T18B2)(struct Cyc_Absyn_Tvar*(*)(unsigned,void*),unsigned,struct Cyc_List_List*)=(struct Cyc_List_List*(*)(struct Cyc_Absyn_Tvar*(*)(unsigned,void*),unsigned,struct Cyc_List_List*))_T88A;_T889=_T18B2;}_T88B=yyyvsp;_T88C=_T88B[1];_T88D=_T88C.l;_T88E=_T88D.first_line;_T88F=Cyc_Position_loc_to_seg(_T88E);_T890=yyyvsp;_T891=& _T890[2].v;_T892=(union Cyc_YYSTYPE*)_T891;_T893=Cyc_yyget_YY41(_T892);_T894=Cyc_List_imp_rev(_T893);{struct Cyc_List_List*ts=_T889(Cyc_Parse_typ2tvar,_T88F,_T894);_T895=yyyvsp;_T896=& _T895[0].v;_T897=(union Cyc_YYSTYPE*)_T896;{
struct Cyc_Parse_Declarator one=Cyc_yyget_YY28(_T897);{struct Cyc_Parse_Declarator _T18B2;_T899=one;
_T18B2.id=_T899.id;_T89A=one;_T18B2.varloc=_T89A.varloc;_T89C=yyr;{struct Cyc_List_List*_T18B3=_region_malloc(_T89C,0U,sizeof(struct Cyc_List_List));_T89E=yyr;{struct Cyc_Absyn_TypeParams_mod_Absyn_Type_modifier_struct*_T18B4=_region_malloc(_T89E,0U,sizeof(struct Cyc_Absyn_TypeParams_mod_Absyn_Type_modifier_struct));_T18B4->tag=4;_T18B4->f1=ts;_T89F=yyyvsp;_T8A0=_T89F[0];_T8A1=_T8A0.l;_T8A2=_T8A1.first_line;_T18B4->f2=Cyc_Position_loc_to_seg(_T8A2);_T18B4->f3=0;_T89D=(struct Cyc_Absyn_TypeParams_mod_Absyn_Type_modifier_struct*)_T18B4;}_T18B3->hd=(void*)_T89D;_T8A3=one;_T18B3->tl=_T8A3.tms;_T89B=(struct Cyc_List_List*)_T18B3;}_T18B2.tms=_T89B;_T898=_T18B2;}yyval=Cyc_YY28(_T898);goto _LL0;}}case 205: _T8A4=yyyvsp;_T8A5=& _T8A4[0].v;_T8A6=(union Cyc_YYSTYPE*)_T8A5;{
# 2063 "parse.y"
struct Cyc_Parse_Declarator one=Cyc_yyget_YY28(_T8A6);{struct Cyc_Parse_Declarator _T18B2;_T8A8=one;
_T18B2.id=_T8A8.id;_T8A9=one;_T18B2.varloc=_T8A9.varloc;_T8AB=yyr;{struct Cyc_List_List*_T18B3=_region_malloc(_T8AB,0U,sizeof(struct Cyc_List_List));_T8AD=yyr;{struct Cyc_Absyn_Attributes_mod_Absyn_Type_modifier_struct*_T18B4=_region_malloc(_T8AD,0U,sizeof(struct Cyc_Absyn_Attributes_mod_Absyn_Type_modifier_struct));_T18B4->tag=5;_T8AE=yyyvsp;_T8AF=_T8AE[1];_T8B0=_T8AF.l;_T8B1=_T8B0.first_line;_T18B4->f1=Cyc_Position_loc_to_seg(_T8B1);_T8B2=yyyvsp;_T8B3=& _T8B2[1].v;_T8B4=(union Cyc_YYSTYPE*)_T8B3;_T18B4->f2=Cyc_yyget_YY46(_T8B4);_T8AC=(struct Cyc_Absyn_Attributes_mod_Absyn_Type_modifier_struct*)_T18B4;}_T18B3->hd=(void*)_T8AC;_T8B5=one;_T18B3->tl=_T8B5.tms;_T8AA=(struct Cyc_List_List*)_T18B3;}_T18B2.tms=_T8AA;_T8A7=_T18B2;}yyval=Cyc_YY28(_T8A7);goto _LL0;}case 206: _T8B6=yyyvsp;_T8B7=_T8B6[0];
# 2069 "parse.y"
yyval=_T8B7.v;goto _LL0;case 207: _T8B8=yyyvsp;_T8B9=& _T8B8[0].v;_T8BA=(union Cyc_YYSTYPE*)_T8B9;_T8BB=
# 2070 "parse.y"
Cyc_yyget_YY27(_T8BA);_T8BC=yyyvsp;_T8BD=& _T8BC[1].v;_T8BE=(union Cyc_YYSTYPE*)_T8BD;_T8BF=Cyc_yyget_YY27(_T8BE);_T8C0=Cyc_List_imp_append(_T8BB,_T8BF);yyval=Cyc_YY27(_T8C0);goto _LL0;case 208:  {
# 2074 "parse.y"
struct Cyc_List_List*ans=0;_T8C1=yyyvsp;_T8C2=& _T8C1[3].v;_T8C3=(union Cyc_YYSTYPE*)_T8C2;_T8C4=
Cyc_yyget_YY46(_T8C3);if(_T8C4==0)goto _TL2B5;_T8C6=yyr;{struct Cyc_List_List*_T18B2=_region_malloc(_T8C6,0U,sizeof(struct Cyc_List_List));_T8C8=yyr;{struct Cyc_Absyn_Attributes_mod_Absyn_Type_modifier_struct*_T18B3=_region_malloc(_T8C8,0U,sizeof(struct Cyc_Absyn_Attributes_mod_Absyn_Type_modifier_struct));_T18B3->tag=5;_T8C9=yyyvsp;_T8CA=_T8C9[3];_T8CB=_T8CA.l;_T8CC=_T8CB.first_line;
_T18B3->f1=Cyc_Position_loc_to_seg(_T8CC);_T8CD=yyyvsp;_T8CE=& _T8CD[3].v;_T8CF=(union Cyc_YYSTYPE*)_T8CE;_T18B3->f2=Cyc_yyget_YY46(_T8CF);_T8C7=(struct Cyc_Absyn_Attributes_mod_Absyn_Type_modifier_struct*)_T18B3;}_T18B2->hd=(void*)_T8C7;_T18B2->tl=ans;_T8C5=(struct Cyc_List_List*)_T18B2;}ans=_T8C5;goto _TL2B6;_TL2B5: _TL2B6: {
# 2078
struct Cyc_Absyn_PtrLoc*ptrloc=0;_T8D0=yyyvsp;_T8D1=& _T8D0[0].v;_T8D2=(union Cyc_YYSTYPE*)_T8D1;_T8D3=
Cyc_yyget_YY1(_T8D2);{struct _tuple22 _T18B2=*_T8D3;void*_T18B3;void*_T18B4;unsigned _T18B5;_T18B5=_T18B2.f0;_T18B4=_T18B2.f1;_T18B3=_T18B2.f2;{unsigned ploc=_T18B5;void*nullable=_T18B4;void*bound=_T18B3;_T8D4=Cyc_Flags_porting_c_code;
if(!_T8D4)goto _TL2B7;{struct Cyc_Absyn_PtrLoc*_T18B6=_cycalloc(sizeof(struct Cyc_Absyn_PtrLoc));
_T18B6->ptr_loc=ploc;_T8D6=yyyvsp;_T8D7=_T8D6[2];_T8D8=_T8D7.l;_T8D9=_T8D8.first_line;_T18B6->rgn_loc=Cyc_Position_loc_to_seg(_T8D9);_T8DA=yyyvsp;_T8DB=_T8DA[1];_T8DC=_T8DB.l;_T8DD=_T8DC.first_line;_T18B6->zt_loc=Cyc_Position_loc_to_seg(_T8DD);_T8D5=(struct Cyc_Absyn_PtrLoc*)_T18B6;}ptrloc=_T8D5;goto _TL2B8;_TL2B7: _TL2B8: _T8DE=yyr;_T8DF=ptrloc;_T8E0=nullable;_T8E1=bound;_T8E2=yyyvsp;_T8E3=& _T8E2[2].v;_T8E4=(union Cyc_YYSTYPE*)_T8E3;_T8E5=
Cyc_yyget_YY45(_T8E4);_T8E6=yyyvsp;_T8E7=& _T8E6[1].v;_T8E8=(union Cyc_YYSTYPE*)_T8E7;_T8E9=Cyc_yyget_YY60(_T8E8);_T8EA=yyyvsp;_T8EB=& _T8EA[4].v;_T8EC=(union Cyc_YYSTYPE*)_T8EB;_T8ED=Cyc_yyget_YY24(_T8EC);{void*mod=Cyc_Parse_make_pointer_mod(_T8DE,_T8DF,_T8E0,_T8E1,_T8E5,_T8E9,_T8ED);_T8EF=yyr;{struct Cyc_List_List*_T18B6=_region_malloc(_T8EF,0U,sizeof(struct Cyc_List_List));
_T18B6->hd=mod;_T18B6->tl=ans;_T8EE=(struct Cyc_List_List*)_T18B6;}ans=_T8EE;
yyval=Cyc_YY27(ans);goto _LL0;}}}}}case 209:
# 2088
 yyval=Cyc_YY60(0);goto _LL0;case 210: _T8F1=yyr;{struct Cyc_List_List*_T18B2=_region_malloc(_T8F1,0U,sizeof(struct Cyc_List_List));_T8F2=yyyvsp;_T8F3=& _T8F2[0].v;_T8F4=(union Cyc_YYSTYPE*)_T8F3;
# 2089 "parse.y"
_T18B2->hd=Cyc_yyget_YY59(_T8F4);_T8F5=yyyvsp;_T8F6=& _T8F5[1].v;_T8F7=(union Cyc_YYSTYPE*)_T8F6;_T18B2->tl=Cyc_yyget_YY60(_T8F7);_T8F0=(struct Cyc_List_List*)_T18B2;}yyval=Cyc_YY60(_T8F0);goto _LL0;case 211: _T8F9=yyr;{struct Cyc_Parse_Numelts_ptrqual_Parse_Pointer_qual_struct*_T18B2=_region_malloc(_T8F9,0U,sizeof(struct Cyc_Parse_Numelts_ptrqual_Parse_Pointer_qual_struct));_T18B2->tag=0;_T8FA=yyyvsp;_T8FB=& _T8FA[2].v;_T8FC=(union Cyc_YYSTYPE*)_T8FB;
# 2094 "parse.y"
_T18B2->f1=Cyc_yyget_Exp_tok(_T8FC);_T8F8=(struct Cyc_Parse_Numelts_ptrqual_Parse_Pointer_qual_struct*)_T18B2;}_T8FD=(void*)_T8F8;yyval=Cyc_YY59(_T8FD);goto _LL0;case 212: _T8FE=yyyvsp;_T8FF=_T8FE[0];_T900=_T8FF.l;_T901=_T900.first_line;_T902=
# 2096 "parse.y"
Cyc_Position_loc_to_seg(_T901);_T903=_tag_fat("@region qualifiers are deprecated; use @effect instead",sizeof(char),55U);_T904=_tag_fat(0U,sizeof(void*),0);Cyc_Warn_warn(_T902,_T903,_T904);_T906=yyr;{struct Cyc_Parse_Effect_ptrqual_Parse_Pointer_qual_struct*_T18B2=_region_malloc(_T906,0U,sizeof(struct Cyc_Parse_Effect_ptrqual_Parse_Pointer_qual_struct));_T18B2->tag=2;{struct Cyc_List_List*_T18B3=_cycalloc(sizeof(struct Cyc_List_List));_T908=yyyvsp;_T909=& _T908[2].v;_T90A=(union Cyc_YYSTYPE*)_T909;
_T18B3->hd=Cyc_yyget_YY45(_T90A);_T18B3->tl=0;_T907=(struct Cyc_List_List*)_T18B3;}_T18B2->f1=_T907;_T905=(struct Cyc_Parse_Effect_ptrqual_Parse_Pointer_qual_struct*)_T18B2;}_T90B=(void*)_T905;yyval=Cyc_YY59(_T90B);goto _LL0;case 213: _T90D=yyr;{struct Cyc_Parse_Effect_ptrqual_Parse_Pointer_qual_struct*_T18B2=_region_malloc(_T90D,0U,sizeof(struct Cyc_Parse_Effect_ptrqual_Parse_Pointer_qual_struct));_T18B2->tag=2;_T90E=yyyvsp;_T90F=& _T90E[2].v;_T910=(union Cyc_YYSTYPE*)_T90F;
# 2098 "parse.y"
_T18B2->f1=Cyc_yyget_YY41(_T910);_T90C=(struct Cyc_Parse_Effect_ptrqual_Parse_Pointer_qual_struct*)_T18B2;}_T911=(void*)_T90C;yyval=Cyc_YY59(_T911);goto _LL0;case 214: _T913=yyr;{struct Cyc_Parse_Thin_ptrqual_Parse_Pointer_qual_struct*_T18B2=_region_malloc(_T913,0U,sizeof(struct Cyc_Parse_Thin_ptrqual_Parse_Pointer_qual_struct));
# 2099 "parse.y"
_T18B2->tag=3;_T912=(struct Cyc_Parse_Thin_ptrqual_Parse_Pointer_qual_struct*)_T18B2;}_T914=(void*)_T912;yyval=Cyc_YY59(_T914);goto _LL0;case 215: _T916=yyr;{struct Cyc_Parse_Fat_ptrqual_Parse_Pointer_qual_struct*_T18B2=_region_malloc(_T916,0U,sizeof(struct Cyc_Parse_Fat_ptrqual_Parse_Pointer_qual_struct));
# 2100 "parse.y"
_T18B2->tag=4;_T915=(struct Cyc_Parse_Fat_ptrqual_Parse_Pointer_qual_struct*)_T18B2;}_T917=(void*)_T915;yyval=Cyc_YY59(_T917);goto _LL0;case 216: _T919=yyr;{struct Cyc_Parse_Autoreleased_ptrqual_Parse_Pointer_qual_struct*_T18B2=_region_malloc(_T919,0U,sizeof(struct Cyc_Parse_Autoreleased_ptrqual_Parse_Pointer_qual_struct));
# 2101 "parse.y"
_T18B2->tag=7;_T918=(struct Cyc_Parse_Autoreleased_ptrqual_Parse_Pointer_qual_struct*)_T18B2;}_T91A=(void*)_T918;yyval=Cyc_YY59(_T91A);goto _LL0;case 217: _T91C=yyr;{struct Cyc_Parse_Zeroterm_ptrqual_Parse_Pointer_qual_struct*_T18B2=_region_malloc(_T91C,0U,sizeof(struct Cyc_Parse_Zeroterm_ptrqual_Parse_Pointer_qual_struct));
# 2102 "parse.y"
_T18B2->tag=5;_T91B=(struct Cyc_Parse_Zeroterm_ptrqual_Parse_Pointer_qual_struct*)_T18B2;}_T91D=(void*)_T91B;yyval=Cyc_YY59(_T91D);goto _LL0;case 218: _T91F=yyr;{struct Cyc_Parse_Nozeroterm_ptrqual_Parse_Pointer_qual_struct*_T18B2=_region_malloc(_T91F,0U,sizeof(struct Cyc_Parse_Nozeroterm_ptrqual_Parse_Pointer_qual_struct));
# 2103 "parse.y"
_T18B2->tag=6;_T91E=(struct Cyc_Parse_Nozeroterm_ptrqual_Parse_Pointer_qual_struct*)_T18B2;}_T920=(void*)_T91E;yyval=Cyc_YY59(_T920);goto _LL0;case 219: _T922=yyr;{struct Cyc_Parse_Notnull_ptrqual_Parse_Pointer_qual_struct*_T18B2=_region_malloc(_T922,0U,sizeof(struct Cyc_Parse_Notnull_ptrqual_Parse_Pointer_qual_struct));
# 2104 "parse.y"
_T18B2->tag=8;_T921=(struct Cyc_Parse_Notnull_ptrqual_Parse_Pointer_qual_struct*)_T18B2;}_T923=(void*)_T921;yyval=Cyc_YY59(_T923);goto _LL0;case 220: _T925=yyr;{struct Cyc_Parse_Nullable_ptrqual_Parse_Pointer_qual_struct*_T18B2=_region_malloc(_T925,0U,sizeof(struct Cyc_Parse_Nullable_ptrqual_Parse_Pointer_qual_struct));
# 2105 "parse.y"
_T18B2->tag=9;_T924=(struct Cyc_Parse_Nullable_ptrqual_Parse_Pointer_qual_struct*)_T18B2;}_T926=(void*)_T924;yyval=Cyc_YY59(_T926);goto _LL0;case 221: _T928=yyr;{struct Cyc_Parse_Alias_ptrqual_Parse_Pointer_qual_struct*_T18B2=_region_malloc(_T928,0U,sizeof(struct Cyc_Parse_Alias_ptrqual_Parse_Pointer_qual_struct));_T18B2->tag=10;_T929=yyyvsp;_T92A=& _T929[2].v;_T92B=(union Cyc_YYSTYPE*)_T92A;
# 2106 "parse.y"
_T18B2->f1=Cyc_yyget_YY58(_T92B);_T927=(struct Cyc_Parse_Alias_ptrqual_Parse_Pointer_qual_struct*)_T18B2;}_T92C=(void*)_T927;yyval=Cyc_YY59(_T92C);goto _LL0;case 222: _T92D=yyyvsp;_T92E=_T92D[0];_T92F=_T92E.l;_T930=_T92F.first_line;_T931=
# 2107 "parse.y"
Cyc_Position_loc_to_seg(_T930);_T932=yyyvsp;_T933=& _T932[0].v;_T934=(union Cyc_YYSTYPE*)_T933;_T935=Cyc_yyget_String_tok(_T934);{void*aq=Cyc_Parse_id2aqual(_T931,_T935);_T937=yyr;{struct Cyc_Parse_Alias_ptrqual_Parse_Pointer_qual_struct*_T18B2=_region_malloc(_T937,0U,sizeof(struct Cyc_Parse_Alias_ptrqual_Parse_Pointer_qual_struct));_T18B2->tag=10;_T18B2->f1=aq;_T936=(struct Cyc_Parse_Alias_ptrqual_Parse_Pointer_qual_struct*)_T18B2;}_T938=(void*)_T936;yyval=Cyc_YY59(_T938);goto _LL0;}case 223: _T939=yyyvsp;_T93A=& _T939[0].v;_T93B=(union Cyc_YYSTYPE*)_T93A;_T93C=
# 2111 "parse.y"
Cyc_yyget_YY58(_T93B);yyval=Cyc_YY58(_T93C);goto _LL0;case 224: _T93D=yyyvsp;_T93E=& _T93D[0].v;_T93F=(union Cyc_YYSTYPE*)_T93E;_T940=
# 2117 "parse.y"
Cyc_yyget_YY45(_T93F);_T941=Cyc_Absyn_al_qual_type;_T942=Cyc_Absyn_aqual_var_type(_T940,_T941);yyval=Cyc_YY58(_T942);goto _LL0;case 225: _T943=yyyvsp;_T944=& _T943[2].v;_T945=(union Cyc_YYSTYPE*)_T944;_T946=
# 2121 "parse.y"
Cyc_yyget_YY45(_T945);_T947=Cyc_Absyn_aqualsof_type(_T946);_T948=Cyc_Absyn_al_qual_type;_T949=Cyc_Absyn_aqual_var_type(_T947,_T948);yyval=Cyc_YY58(_T949);goto _LL0;case 226: goto _LL0;case 227: _T94A=yyyvsp;_T94B=_T94A[0];_T94C=_T94B.l;_T94D=_T94C.first_line;_T94E=
# 2127 "parse.y"
Cyc_Position_loc_to_seg(_T94D);_T94F=yyyvsp;_T950=& _T94F[0].v;_T951=(union Cyc_YYSTYPE*)_T950;_T952=Cyc_yyget_String_tok(_T951);_T953=Cyc_Parse_id2aqual(_T94E,_T952);yyval=Cyc_YY58(_T953);goto _LL0;case 228:{struct _tuple22*_T18B2=_cycalloc(sizeof(struct _tuple22));_T955=yyyvsp;_T956=_T955[0];_T957=_T956.l;_T958=_T957.first_line;
# 2133 "parse.y"
_T18B2->f0=Cyc_Position_loc_to_seg(_T958);_T18B2->f1=Cyc_Absyn_true_type;_T959=Cyc_Parse_parsing_tempest;if(!_T959)goto _TL2B9;_T18B2->f2=Cyc_Absyn_fat_bound_type;goto _TL2BA;_TL2B9: _T95A=yyyvsp;_T95B=_T95A[0];_T95C=_T95B.l;_T95D=_T95C.first_line;_T95E=Cyc_Position_loc_to_seg(_T95D);_T95F=Cyc_Position_string_of_segment(_T95E);_T960=yyyvsp;_T961=& _T960[1].v;_T962=(union Cyc_YYSTYPE*)_T961;_T963=Cyc_yyget_YY2(_T962);_T18B2->f2=Cyc_Parse_assign_cvar_pos(_T95F,0,_T963);_TL2BA: _T954=(struct _tuple22*)_T18B2;}yyval=Cyc_YY1(_T954);goto _LL0;case 229:{struct _tuple22*_T18B2=_cycalloc(sizeof(struct _tuple22));_T965=yyyvsp;_T966=_T965[0];_T967=_T966.l;_T968=_T967.first_line;
# 2134 "parse.y"
_T18B2->f0=Cyc_Position_loc_to_seg(_T968);_T18B2->f1=Cyc_Absyn_false_type;_T969=yyyvsp;_T96A=_T969[0];_T96B=_T96A.l;_T96C=_T96B.first_line;_T96D=Cyc_Position_loc_to_seg(_T96C);_T96E=Cyc_Position_string_of_segment(_T96D);_T96F=yyyvsp;_T970=& _T96F[1].v;_T971=(union Cyc_YYSTYPE*)_T970;_T972=Cyc_yyget_YY2(_T971);_T18B2->f2=Cyc_Parse_assign_cvar_pos(_T96E,0,_T972);_T964=(struct _tuple22*)_T18B2;}yyval=Cyc_YY1(_T964);goto _LL0;case 230: _T974=Cyc_Flags_override_fat;
# 2135 "parse.y"
if(!_T974)goto _TL2BB;if(Cyc_Parse_inside_noinference_block!=0)goto _TL2BB;_T975=Cyc_Flags_interproc;if(!_T975)goto _TL2BB;{struct _tuple22*_T18B2=_cycalloc(sizeof(struct _tuple22));_T977=yyyvsp;_T978=_T977[0];_T979=_T978.l;_T97A=_T979.first_line;
_T18B2->f0=Cyc_Position_loc_to_seg(_T97A);_T18B2->f1=Cyc_Absyn_true_type;_T97B=yyyvsp;_T97C=_T97B[0];_T97D=_T97C.l;_T97E=_T97D.first_line;_T97F=Cyc_Position_loc_to_seg(_T97E);_T980=Cyc_Position_string_of_segment(_T97F);_T981=& Cyc_Kinds_ptrbko;_T982=(struct Cyc_Core_Opt*)_T981;_T983=Cyc_Absyn_cvar_type(_T982);_T18B2->f2=Cyc_Parse_assign_cvar_pos(_T980,1,_T983);_T976=(struct _tuple22*)_T18B2;}_T973=_T976;goto _TL2BC;_TL2BB:{struct _tuple22*_T18B2=_cycalloc(sizeof(struct _tuple22));_T985=yyyvsp;_T986=_T985[0];_T987=_T986.l;_T988=_T987.first_line;
_T18B2->f0=Cyc_Position_loc_to_seg(_T988);_T18B2->f1=Cyc_Absyn_true_type;_T18B2->f2=Cyc_Absyn_fat_bound_type;_T984=(struct _tuple22*)_T18B2;}_T973=_T984;_TL2BC:
# 2135
 yyval=Cyc_YY1(_T973);goto _LL0;case 231: _T98A=Cyc_Flags_interproc;
# 2140
if(!_T98A)goto _TL2BD;if(Cyc_Parse_inside_noinference_block!=0)goto _TL2BD;_T98B=& Cyc_Kinds_ptrbko;_T98C=(struct Cyc_Core_Opt*)_T98B;_T989=Cyc_Absyn_cvar_type(_T98C);goto _TL2BE;_TL2BD: _T989=Cyc_Absyn_bounds_one();_TL2BE: yyval=Cyc_YY2(_T989);goto _LL0;case 232: _T98D=yyyvsp;_T98E=& _T98D[1].v;_T98F=(union Cyc_YYSTYPE*)_T98E;_T990=
# 2141 "parse.y"
Cyc_yyget_Exp_tok(_T98F);_T991=Cyc_Absyn_thin_bounds_exp(_T990);yyval=Cyc_YY2(_T991);goto _LL0;case 233: _T992=Cyc_Flags_resolve;
# 2144 "parse.y"
if(_T992)goto _TL2BF;else{goto _TL2C1;}
_TL2C1:{int(*_T18B2)(unsigned,struct _fat_ptr)=(int(*)(unsigned,struct _fat_ptr))Cyc_Parse_parse_abort;_T993=_T18B2;}_T994=_tag_fat("Type variable not permitted in pointer bound",sizeof(char),45U);_T993(0U,_T994);goto _TL2C0;_TL2BF: _TL2C0: _T995=yyyvsp;_T996=& _T995[1].v;_T997=(union Cyc_YYSTYPE*)_T996;_T998=
Cyc_yyget_String_tok(_T997);_T999=Cyc_Parse_typevar2cvar(_T998);yyval=Cyc_YY2(_T999);goto _LL0;case 234: _T99A=
# 2151 "parse.y"
Cyc_Tcutil_any_bool(0);yyval=Cyc_YY54(_T99A);goto _LL0;case 235:
# 2152 "parse.y"
 yyval=Cyc_YY54(Cyc_Absyn_true_type);goto _LL0;case 236:
# 2153 "parse.y"
 yyval=Cyc_YY54(Cyc_Absyn_false_type);goto _LL0;case 237:{struct Cyc_List_List*_T18B2=_cycalloc(sizeof(struct Cyc_List_List));_T99C=yyyvsp;_T99D=& _T99C[0].v;_T99E=(union Cyc_YYSTYPE*)_T99D;
# 2157 "parse.y"
_T18B2->hd=Cyc_yyget_YY45(_T99E);_T18B2->tl=0;_T99B=(struct Cyc_List_List*)_T18B2;}yyval=Cyc_YY41(_T99B);goto _LL0;case 238:{struct Cyc_List_List*_T18B2=_cycalloc(sizeof(struct Cyc_List_List));_T9A0=yyyvsp;_T9A1=& _T9A0[0].v;_T9A2=(union Cyc_YYSTYPE*)_T9A1;
# 2158 "parse.y"
_T18B2->hd=Cyc_yyget_YY45(_T9A2);_T9A3=yyyvsp;_T9A4=& _T9A3[2].v;_T9A5=(union Cyc_YYSTYPE*)_T9A4;_T18B2->tl=Cyc_yyget_YY41(_T9A5);_T99F=(struct Cyc_List_List*)_T18B2;}yyval=Cyc_YY41(_T99F);goto _LL0;case 239: _T9A6=& Cyc_Kinds_eko;_T9A7=(struct Cyc_Core_Opt*)_T9A6;_T9A8=
# 2162 "parse.y"
Cyc_Absyn_new_evar(_T9A7,0);yyval=Cyc_YY45(_T9A8);goto _LL0;case 240: _T9A9=yyyvsp;_T9AA=& _T9A9[0].v;_T9AB=(union Cyc_YYSTYPE*)_T9AA;{
# 2163 "parse.y"
struct Cyc_List_List*es=Cyc_yyget_YY41(_T9AB);_T9AC=Cyc_List_length(es);if(_T9AC!=1)goto _TL2C2;_T9AD=_check_null(es);_T9AE=_T9AD->hd;yyval=Cyc_YY45(_T9AE);goto _TL2C3;_TL2C2: _T9AF=yyyvsp;_T9B0=& _T9AF[0].v;_T9B1=(union Cyc_YYSTYPE*)_T9B0;_T9B2=Cyc_yyget_YY41(_T9B1);_T9B3=Cyc_Absyn_join_eff(_T9B2);yyval=Cyc_YY45(_T9B3);_TL2C3: goto _LL0;}case 241: _T9B4=& Cyc_Kinds_eko;_T9B5=(struct Cyc_Core_Opt*)_T9B4;_T9B6=
# 2164 "parse.y"
Cyc_Absyn_new_evar(_T9B5,0);yyval=Cyc_YY45(_T9B6);goto _LL0;case 242: _T9B7=yyvs;_T9B8=yyvsp_offset + 1;_T9B9=_check_fat_subscript(_T9B7,sizeof(struct Cyc_Yystacktype),_T9B8);_T9BA=(struct Cyc_Yystacktype*)_T9B9;_T9BB=*_T9BA;_T9BC=_T9BB.l;_T9BD=_T9BC.first_line;_T9BE=
# 2175 "parse.y"
Cyc_Position_loc_to_seg(_T9BD);_T9BF=Cyc_Absyn_empty_tqual(_T9BE);yyval=Cyc_YY24(_T9BF);goto _LL0;case 243: _T9C0=yyyvsp;_T9C1=& _T9C0[0].v;_T9C2=(union Cyc_YYSTYPE*)_T9C1;_T9C3=
# 2176 "parse.y"
Cyc_yyget_YY24(_T9C2);_T9C4=yyyvsp;_T9C5=& _T9C4[1].v;_T9C6=(union Cyc_YYSTYPE*)_T9C5;_T9C7=Cyc_yyget_YY24(_T9C6);_T9C8=Cyc_Absyn_combine_tqual(_T9C3,_T9C7);yyval=Cyc_YY24(_T9C8);goto _LL0;case 244: _T9C9=yyyvsp;_T9CA=& _T9C9[1].v;_T9CB=(union Cyc_YYSTYPE*)_T9CA;{
# 2181 "parse.y"
struct _tuple28*ec_qb=Cyc_yyget_YY51(_T9CB);_T9CD=ec_qb;_T9CE=(unsigned)_T9CD;
if(!_T9CE)goto _TL2C4;_T9CF=ec_qb;_T9CC=*_T9CF;goto _TL2C5;_TL2C4:{struct _tuple28 _T18B2;_T18B2.f0=0;_T18B2.f1=0;_T9D0=_T18B2;}_T9CC=_T9D0;_TL2C5: {struct _tuple28 _T18B2=_T9CC;struct Cyc_List_List*_T18B3;struct Cyc_List_List*_T18B4;_T18B4=_T18B2.f0;_T18B3=_T18B2.f1;{struct Cyc_List_List*ec=_T18B4;struct Cyc_List_List*qb=_T18B3;{struct _tuple27*_T18B5=_cycalloc(sizeof(struct _tuple27));
_T18B5->f0=0;_T18B5->f1=0;_T18B5->f2=0;_T9D2=yyyvsp;_T9D3=& _T9D2[0].v;_T9D4=(union Cyc_YYSTYPE*)_T9D3;_T18B5->f3=Cyc_yyget_YY50(_T9D4);_T18B5->f4=ec;_T18B5->f5=qb;_T9D1=(struct _tuple27*)_T18B5;}yyval=Cyc_YY40(_T9D1);goto _LL0;}}}case 245: _T9D5=yyyvsp;_T9D6=& _T9D5[2].v;_T9D7=(union Cyc_YYSTYPE*)_T9D6;{
# 2185 "parse.y"
struct _tuple28*ec_qb=Cyc_yyget_YY51(_T9D7);_T9D9=ec_qb;_T9DA=(unsigned)_T9D9;
if(!_T9DA)goto _TL2C6;_T9DB=ec_qb;_T9D8=*_T9DB;goto _TL2C7;_TL2C6:{struct _tuple28 _T18B2;_T18B2.f0=0;_T18B2.f1=0;_T9DC=_T18B2;}_T9D8=_T9DC;_TL2C7: {struct _tuple28 _T18B2=_T9D8;struct Cyc_List_List*_T18B3;struct Cyc_List_List*_T18B4;_T18B4=_T18B2.f0;_T18B3=_T18B2.f1;{struct Cyc_List_List*ec=_T18B4;struct Cyc_List_List*qb=_T18B3;{struct _tuple27*_T18B5=_cycalloc(sizeof(struct _tuple27));_T9DE=yyyvsp;_T9DF=& _T9DE[0].v;_T9E0=(union Cyc_YYSTYPE*)_T9DF;_T9E1=
Cyc_yyget_YY39(_T9E0);_T18B5->f0=Cyc_List_imp_rev(_T9E1);_T18B5->f1=0;_T18B5->f2=0;_T9E2=yyyvsp;_T9E3=& _T9E2[1].v;_T9E4=(union Cyc_YYSTYPE*)_T9E3;_T18B5->f3=Cyc_yyget_YY50(_T9E4);_T18B5->f4=ec;_T18B5->f5=qb;_T9DD=(struct _tuple27*)_T18B5;}yyval=Cyc_YY40(_T9DD);goto _LL0;}}}case 246: _T9E5=yyyvsp;_T9E6=& _T9E5[4].v;_T9E7=(union Cyc_YYSTYPE*)_T9E6;{
# 2189 "parse.y"
struct _tuple28*ec_qb=Cyc_yyget_YY51(_T9E7);_T9E9=ec_qb;_T9EA=(unsigned)_T9E9;
if(!_T9EA)goto _TL2C8;_T9EB=ec_qb;_T9E8=*_T9EB;goto _TL2C9;_TL2C8:{struct _tuple28 _T18B2;_T18B2.f0=0;_T18B2.f1=0;_T9EC=_T18B2;}_T9E8=_T9EC;_TL2C9: {struct _tuple28 _T18B2=_T9E8;struct Cyc_List_List*_T18B3;struct Cyc_List_List*_T18B4;_T18B4=_T18B2.f0;_T18B3=_T18B2.f1;{struct Cyc_List_List*ec=_T18B4;struct Cyc_List_List*qb=_T18B3;{struct _tuple27*_T18B5=_cycalloc(sizeof(struct _tuple27));_T9EE=yyyvsp;_T9EF=& _T9EE[0].v;_T9F0=(union Cyc_YYSTYPE*)_T9EF;_T9F1=
Cyc_yyget_YY39(_T9F0);_T18B5->f0=Cyc_List_imp_rev(_T9F1);_T18B5->f1=1;_T18B5->f2=0;_T9F2=yyyvsp;_T9F3=& _T9F2[3].v;_T9F4=(union Cyc_YYSTYPE*)_T9F3;_T18B5->f3=Cyc_yyget_YY50(_T9F4);_T18B5->f4=ec;_T18B5->f5=qb;_T9ED=(struct _tuple27*)_T18B5;}yyval=Cyc_YY40(_T9ED);goto _LL0;}}}case 247: _T9F5=yyyvsp;_T9F6=& _T9F5[2].v;_T9F7=(union Cyc_YYSTYPE*)_T9F6;{
# 2194
struct _tuple8*_T18B2=Cyc_yyget_YY38(_T9F7);void*_T18B3;struct Cyc_Absyn_Tqual _T18B4;struct _fat_ptr*_T18B5;{struct _tuple8 _T18B6=*_T18B2;_T18B5=_T18B6.f0;_T18B4=_T18B6.f1;_T18B3=_T18B6.f2;}{struct _fat_ptr*n=_T18B5;struct Cyc_Absyn_Tqual tq=_T18B4;void*t=_T18B3;
struct Cyc_Absyn_VarargInfo*v;v=_cycalloc(sizeof(struct Cyc_Absyn_VarargInfo));_T9F8=v;_T9F8->name=n;_T9F9=v;_T9F9->tq=tq;_T9FA=v;_T9FA->type=t;_T9FB=v;_T9FC=yyyvsp;_T9FD=& _T9FC[1].v;_T9FE=(union Cyc_YYSTYPE*)_T9FD;_T9FB->inject=Cyc_yyget_YY32(_T9FE);_T9FF=yyyvsp;_TA00=& _T9FF[4].v;_TA01=(union Cyc_YYSTYPE*)_TA00;{
struct _tuple28*ec_qb=Cyc_yyget_YY51(_TA01);_TA03=ec_qb;_TA04=(unsigned)_TA03;
if(!_TA04)goto _TL2CA;_TA05=ec_qb;_TA02=*_TA05;goto _TL2CB;_TL2CA:{struct _tuple28 _T18B6;_T18B6.f0=0;_T18B6.f1=0;_TA06=_T18B6;}_TA02=_TA06;_TL2CB: {struct _tuple28 _T18B6=_TA02;struct Cyc_List_List*_T18B7;struct Cyc_List_List*_T18B8;_T18B8=_T18B6.f0;_T18B7=_T18B6.f1;{struct Cyc_List_List*ec=_T18B8;struct Cyc_List_List*qb=_T18B7;{struct _tuple27*_T18B9=_cycalloc(sizeof(struct _tuple27));
_T18B9->f0=0;_T18B9->f1=0;_T18B9->f2=v;_TA08=yyyvsp;_TA09=& _TA08[3].v;_TA0A=(union Cyc_YYSTYPE*)_TA09;_T18B9->f3=Cyc_yyget_YY50(_TA0A);_T18B9->f4=ec;_T18B9->f5=qb;_TA07=(struct _tuple27*)_T18B9;}yyval=Cyc_YY40(_TA07);goto _LL0;}}}}}case 248: _TA0B=yyyvsp;_TA0C=& _TA0B[4].v;_TA0D=(union Cyc_YYSTYPE*)_TA0C;{
# 2202
struct _tuple8*_T18B2=Cyc_yyget_YY38(_TA0D);void*_T18B3;struct Cyc_Absyn_Tqual _T18B4;struct _fat_ptr*_T18B5;{struct _tuple8 _T18B6=*_T18B2;_T18B5=_T18B6.f0;_T18B4=_T18B6.f1;_T18B3=_T18B6.f2;}{struct _fat_ptr*n=_T18B5;struct Cyc_Absyn_Tqual tq=_T18B4;void*t=_T18B3;
struct Cyc_Absyn_VarargInfo*v;v=_cycalloc(sizeof(struct Cyc_Absyn_VarargInfo));_TA0E=v;_TA0E->name=n;_TA0F=v;_TA0F->tq=tq;_TA10=v;_TA10->type=t;_TA11=v;_TA12=yyyvsp;_TA13=& _TA12[3].v;_TA14=(union Cyc_YYSTYPE*)_TA13;_TA11->inject=Cyc_yyget_YY32(_TA14);_TA15=yyyvsp;_TA16=& _TA15[6].v;_TA17=(union Cyc_YYSTYPE*)_TA16;{
struct _tuple28*ec_qb=Cyc_yyget_YY51(_TA17);_TA19=ec_qb;_TA1A=(unsigned)_TA19;
if(!_TA1A)goto _TL2CC;_TA1B=ec_qb;_TA18=*_TA1B;goto _TL2CD;_TL2CC:{struct _tuple28 _T18B6;_T18B6.f0=0;_T18B6.f1=0;_TA1C=_T18B6;}_TA18=_TA1C;_TL2CD: {struct _tuple28 _T18B6=_TA18;struct Cyc_List_List*_T18B7;struct Cyc_List_List*_T18B8;_T18B8=_T18B6.f0;_T18B7=_T18B6.f1;{struct Cyc_List_List*ec=_T18B8;struct Cyc_List_List*qb=_T18B7;{struct _tuple27*_T18B9=_cycalloc(sizeof(struct _tuple27));_TA1E=yyyvsp;_TA1F=& _TA1E[0].v;_TA20=(union Cyc_YYSTYPE*)_TA1F;_TA21=
Cyc_yyget_YY39(_TA20);_T18B9->f0=Cyc_List_imp_rev(_TA21);_T18B9->f1=0;_T18B9->f2=v;_TA22=yyyvsp;_TA23=& _TA22[5].v;_TA24=(union Cyc_YYSTYPE*)_TA23;_T18B9->f3=Cyc_yyget_YY50(_TA24);_T18B9->f4=ec;_T18B9->f5=qb;_TA1D=(struct _tuple27*)_T18B9;}yyval=Cyc_YY40(_TA1D);goto _LL0;}}}}}case 249:
# 2212 "parse.y"
 yyval=Cyc_YY50(0);goto _LL0;case 250: _TA25=yyyvsp;_TA26=_TA25[0];_TA27=_TA26.l;_TA28=_TA27.first_line;_TA29=
# 2213 "parse.y"
Cyc_Position_loc_to_seg(_TA28);_TA2A=yyyvsp;_TA2B=& _TA2A[0].v;_TA2C=(union Cyc_YYSTYPE*)_TA2B;_TA2D=Cyc_yyget_String_tok(_TA2C);_TA2E=Cyc_Parse_id2aqual(_TA29,_TA2D);yyval=Cyc_YY50(_TA2E);goto _LL0;case 251: _TA2F=yyyvsp;_TA30=& _TA2F[0].v;_TA31=(union Cyc_YYSTYPE*)_TA30;_TA32=
# 2216
Cyc_yyget_String_tok(_TA31);{struct Cyc_Absyn_Unknown_kb_Absyn_KindBound_struct*_T18B2=_cycalloc(sizeof(struct Cyc_Absyn_Unknown_kb_Absyn_KindBound_struct));_T18B2->tag=1;_T18B2->f1=0;_TA33=(struct Cyc_Absyn_Unknown_kb_Absyn_KindBound_struct*)_T18B2;}_TA34=(void*)_TA33;_TA35=yyyvsp;_TA36=& _TA35[1].v;_TA37=(union Cyc_YYSTYPE*)_TA36;_TA38=Cyc_yyget_YY50(_TA37);_TA39=yyyvsp;_TA3A=_TA39[0];_TA3B=_TA3A.l;_TA3C=_TA3B.first_line;_TA3D=Cyc_Position_loc_to_seg(_TA3C);_TA3E=Cyc_Parse_id2type(_TA32,_TA34,_TA38,_TA3D);yyval=Cyc_YY45(_TA3E);goto _LL0;case 252: _TA3F=yyyvsp;_TA40=& _TA3F[0].v;_TA41=(union Cyc_YYSTYPE*)_TA40;_TA42=
# 2217 "parse.y"
Cyc_yyget_String_tok(_TA41);_TA43=yyyvsp;_TA44=& _TA43[2].v;_TA45=(union Cyc_YYSTYPE*)_TA44;_TA46=Cyc_yyget_YY44(_TA45);_TA47=Cyc_Kinds_kind_to_bound(_TA46);_TA48=yyyvsp;_TA49=& _TA48[3].v;_TA4A=(union Cyc_YYSTYPE*)_TA49;_TA4B=Cyc_yyget_YY50(_TA4A);_TA4C=yyyvsp;_TA4D=_TA4C[0];_TA4E=_TA4D.l;_TA4F=_TA4E.first_line;_TA50=Cyc_Position_loc_to_seg(_TA4F);_TA51=Cyc_Parse_id2type(_TA42,_TA47,_TA4B,_TA50);yyval=Cyc_YY45(_TA51);goto _LL0;case 253:
# 2220
 yyval=Cyc_YY50(0);goto _LL0;case 254: _TA52=yyyvsp;_TA53=& _TA52[1].v;_TA54=(union Cyc_YYSTYPE*)_TA53;_TA55=
# 2221 "parse.y"
Cyc_yyget_YY41(_TA54);_TA56=Cyc_Absyn_join_eff(_TA55);yyval=Cyc_YY50(_TA56);goto _LL0;case 255:
# 2225 "parse.y"
 yyval=Cyc_YY51(0);goto _LL0;case 256: _TA57=yyyvsp;_TA58=_TA57[1];
# 2226 "parse.y"
yyval=_TA58.v;goto _LL0;case 257:{struct _tuple28*_T18B2=_cycalloc(sizeof(struct _tuple28));{void*_T18B3[1];_TA5B=yyyvsp;_TA5C=& _TA5B[0].v;_TA5D=(union Cyc_YYSTYPE*)_TA5C;_T18B3[0]=
# 2231 "parse.y"
Cyc_yyget_YY52(_TA5D);_TA5E=_tag_fat(_T18B3,sizeof(void*),1);_TA5A=Cyc_List_list(_TA5E);}_T18B2->f0=_TA5A;_T18B2->f1=0;_TA59=(struct _tuple28*)_T18B2;}yyval=Cyc_YY51(_TA59);goto _LL0;case 258:{struct _tuple28*_T18B2=_cycalloc(sizeof(struct _tuple28));
# 2233 "parse.y"
_T18B2->f0=0;{struct _tuple29*_T18B3[1];_TA61=yyyvsp;_TA62=& _TA61[0].v;_TA63=(union Cyc_YYSTYPE*)_TA62;_T18B3[0]=Cyc_yyget_YY53(_TA63);_TA64=_tag_fat(_T18B3,sizeof(struct _tuple29*),1);_TA60=Cyc_List_list(_TA64);}_T18B2->f1=_TA60;_TA5F=(struct _tuple28*)_T18B2;}yyval=Cyc_YY51(_TA5F);goto _LL0;case 259: _TA65=yyyvsp;_TA66=& _TA65[2].v;_TA67=(union Cyc_YYSTYPE*)_TA66;{
# 2236 "parse.y"
struct _tuple28*rest=Cyc_yyget_YY51(_TA67);_TA68=
_check_null(rest);{struct _tuple28 _T18B2=*_TA68;struct Cyc_List_List*_T18B3;_T18B3=_T18B2.f0;{struct Cyc_List_List*rpo=_T18B3;_TA69=rest;{struct Cyc_List_List*_T18B4=_cycalloc(sizeof(struct Cyc_List_List));_TA6B=yyyvsp;_TA6C=& _TA6B[0].v;_TA6D=(union Cyc_YYSTYPE*)_TA6C;
_T18B4->hd=Cyc_yyget_YY52(_TA6D);_T18B4->tl=rpo;_TA6A=(struct Cyc_List_List*)_T18B4;}(*_TA69).f0=_TA6A;
yyval=Cyc_YY51(rest);goto _LL0;}}}case 260: _TA6E=yyyvsp;_TA6F=& _TA6E[2].v;_TA70=(union Cyc_YYSTYPE*)_TA6F;{
# 2243 "parse.y"
struct _tuple28*rest=Cyc_yyget_YY51(_TA70);_TA71=
_check_null(rest);{struct _tuple28 _T18B2=*_TA71;struct Cyc_List_List*_T18B3;_T18B3=_T18B2.f1;{struct Cyc_List_List*qb=_T18B3;_TA72=rest;{struct Cyc_List_List*_T18B4=_cycalloc(sizeof(struct Cyc_List_List));_TA74=yyyvsp;_TA75=& _TA74[0].v;_TA76=(union Cyc_YYSTYPE*)_TA75;
_T18B4->hd=Cyc_yyget_YY53(_TA76);_T18B4->tl=qb;_TA73=(struct Cyc_List_List*)_T18B4;}(*_TA72).f1=_TA73;
yyval=Cyc_YY51(rest);goto _LL0;}}}case 261:  {
# 2253 "parse.y"
struct Cyc_Absyn_Less_kb_Absyn_KindBound_struct*kb;kb=_cycalloc(sizeof(struct Cyc_Absyn_Less_kb_Absyn_KindBound_struct));_TA77=kb;_TA77->tag=2;_TA78=kb;_TA78->f1=0;_TA79=kb;_TA7A=& Cyc_Kinds_ek;_TA79->f2=(struct Cyc_Absyn_Kind*)_TA7A;_TA7B=yyyvsp;_TA7C=& _TA7B[2].v;_TA7D=(union Cyc_YYSTYPE*)_TA7C;_TA7E=
Cyc_yyget_String_tok(_TA7D);_TA7F=kb;_TA80=(void*)_TA7F;_TA81=yyyvsp;_TA82=_TA81[2];_TA83=_TA82.l;_TA84=_TA83.first_line;_TA85=Cyc_Position_loc_to_seg(_TA84);{void*t=Cyc_Parse_id2type(_TA7E,_TA80,0,_TA85);{struct Cyc_Absyn_SubsetConstraint_Absyn_EffConstraint_struct*_T18B2=_cycalloc(sizeof(struct Cyc_Absyn_SubsetConstraint_Absyn_EffConstraint_struct));_T18B2->tag=2;_TA87=yyyvsp;_TA88=& _TA87[0].v;_TA89=(union Cyc_YYSTYPE*)_TA88;_TA8A=
Cyc_yyget_YY41(_TA89);_T18B2->f1=Cyc_Parse_effect_from_atomic(_TA8A);_T18B2->f2=t;_TA86=(struct Cyc_Absyn_SubsetConstraint_Absyn_EffConstraint_struct*)_T18B2;}_TA8B=(void*)_TA86;yyval=Cyc_YY52(_TA8B);goto _LL0;}}case 262:{struct Cyc_Absyn_DisjointConstraint_Absyn_EffConstraint_struct*_T18B2=_cycalloc(sizeof(struct Cyc_Absyn_DisjointConstraint_Absyn_EffConstraint_struct));_T18B2->tag=1;_TA8D=yyyvsp;_TA8E=& _TA8D[0].v;_TA8F=(union Cyc_YYSTYPE*)_TA8E;_TA90=
# 2259 "parse.y"
Cyc_yyget_YY41(_TA8F);_T18B2->f1=Cyc_Parse_effect_from_atomic(_TA90);_TA91=yyyvsp;_TA92=& _TA91[2].v;_TA93=(union Cyc_YYSTYPE*)_TA92;_TA94=
Cyc_yyget_YY41(_TA93);_T18B2->f2=Cyc_Parse_effect_from_atomic(_TA94);_TA8C=(struct Cyc_Absyn_DisjointConstraint_Absyn_EffConstraint_struct*)_T18B2;}_TA95=(void*)_TA8C;
# 2259
yyval=Cyc_YY52(_TA95);goto _LL0;case 263: _TA96=yyyvsp;_TA97=_TA96[0];_TA98=_TA97.l;_TA99=_TA98.first_line;_TA9A=
# 2264 "parse.y"
Cyc_Position_loc_to_seg(_TA99);_TA9B=yyyvsp;_TA9C=& _TA9B[0].v;_TA9D=(union Cyc_YYSTYPE*)_TA9C;_TA9E=Cyc_yyget_String_tok(_TA9D);Cyc_Parse_check_single_constraint(_TA9A,_TA9E);{struct Cyc_Absyn_SingleConstraint_Absyn_EffConstraint_struct*_T18B2=_cycalloc(sizeof(struct Cyc_Absyn_SingleConstraint_Absyn_EffConstraint_struct));_T18B2->tag=0;_TAA0=yyyvsp;_TAA1=& _TAA0[2].v;_TAA2=(union Cyc_YYSTYPE*)_TAA1;_TAA3=
Cyc_yyget_YY41(_TAA2);_T18B2->f1=Cyc_Parse_effect_from_atomic(_TAA3);_TA9F=(struct Cyc_Absyn_SingleConstraint_Absyn_EffConstraint_struct*)_T18B2;}_TAA4=(void*)_TA9F;yyval=Cyc_YY52(_TAA4);goto _LL0;case 264:{struct _tuple29*_T18B2=_cycalloc(sizeof(struct _tuple29));_TAA6=yyyvsp;_TAA7=& _TAA6[2].v;_TAA8=(union Cyc_YYSTYPE*)_TAA7;
# 2272 "parse.y"
_T18B2->f0=Cyc_yyget_YY45(_TAA8);_TAA9=yyyvsp;_TAAA=& _TAA9[0].v;_TAAB=(union Cyc_YYSTYPE*)_TAAA;_T18B2->f1=Cyc_yyget_YY45(_TAAB);_TAA5=(struct _tuple29*)_T18B2;}yyval=Cyc_YY53(_TAA5);goto _LL0;case 265:
# 2277 "parse.y"
 yyval=Cyc_YY58(Cyc_Absyn_al_qual_type);goto _LL0;case 266:
# 2278 "parse.y"
 yyval=Cyc_YY58(Cyc_Absyn_un_qual_type);goto _LL0;case 267:
# 2279 "parse.y"
 yyval=Cyc_YY58(Cyc_Absyn_rc_qual_type);goto _LL0;case 268:
# 2280 "parse.y"
 yyval=Cyc_YY58(Cyc_Absyn_rtd_qual_type);goto _LL0;case 269: _TAAC=yyyvsp;_TAAD=_TAAC[0];_TAAE=_TAAD.l;_TAAF=_TAAE.first_line;_TAB0=
# 2281 "parse.y"
Cyc_Position_loc_to_seg(_TAAF);_TAB1=yyyvsp;_TAB2=& _TAB1[0].v;_TAB3=(union Cyc_YYSTYPE*)_TAB2;_TAB4=Cyc_yyget_String_tok(_TAB3);_TAB5=Cyc_Parse_id2aqual(_TAB0,_TAB4);yyval=Cyc_YY58(_TAB5);goto _LL0;case 270: _TAB6=yyyvsp;_TAB7=& _TAB6[0].v;_TAB8=(union Cyc_YYSTYPE*)_TAB7;_TAB9=
# 2286 "parse.y"
Cyc_yyget_YY58(_TAB8);yyval=Cyc_YY45(_TAB9);goto _LL0;case 271: _TABA=yyyvsp;_TABB=& _TABA[2].v;_TABC=(union Cyc_YYSTYPE*)_TABB;_TABD=
# 2289 "parse.y"
Cyc_yyget_YY45(_TABC);_TABE=Cyc_Absyn_aqualsof_type(_TABD);_TABF=Cyc_Absyn_al_qual_type;_TAC0=Cyc_Absyn_aqual_var_type(_TABE,_TABF);yyval=Cyc_YY45(_TAC0);goto _LL0;case 272:  {
# 2296 "parse.y"
struct Cyc_Absyn_Eq_kb_Absyn_KindBound_struct*kb;kb=_cycalloc(sizeof(struct Cyc_Absyn_Eq_kb_Absyn_KindBound_struct));_TAC1=kb;_TAC1->tag=0;_TAC2=kb;_TAC3=& Cyc_Kinds_aqk;_TAC2->f1=(struct Cyc_Absyn_Kind*)_TAC3;_TAC4=yyyvsp;_TAC5=& _TAC4[0].v;_TAC6=(union Cyc_YYSTYPE*)_TAC5;_TAC7=
Cyc_yyget_String_tok(_TAC6);_TAC8=kb;_TAC9=(void*)_TAC8;_TACA=yyyvsp;_TACB=_TACA[0];_TACC=_TACB.l;_TACD=_TACC.first_line;_TACE=Cyc_Position_loc_to_seg(_TACD);_TACF=Cyc_Parse_id2type(_TAC7,_TAC9,0,_TACE);yyval=Cyc_YY45(_TACF);goto _LL0;}case 273: _TAD0=yyyvsp;_TAD1=& _TAD0[2].v;_TAD2=(union Cyc_YYSTYPE*)_TAD1;_TAD3=
# 2301 "parse.y"
Cyc_yyget_YY45(_TAD2);_TAD4=Cyc_Absyn_aqualsof_type(_TAD3);yyval=Cyc_YY45(_TAD4);goto _LL0;case 274:
# 2330 "parse.y"
 yyval=Cyc_YY32(0);goto _LL0;case 275: _TAD5=yyyvsp;_TAD6=& _TAD5[0].v;_TAD7=(union Cyc_YYSTYPE*)_TAD6;_TAD8=
# 2332 "parse.y"
Cyc_yyget_String_tok(_TAD7);_TAD9=_tag_fat("inject",sizeof(char),7U);_TADA=Cyc_zstrcmp(_TAD8,_TAD9);if(_TADA==0)goto _TL2CE;{struct Cyc_Warn_String_Warn_Warg_struct _T18B2;_T18B2.tag=0;
_T18B2.f1=_tag_fat("missing type in function declaration",sizeof(char),37U);_TADB=_T18B2;}{struct Cyc_Warn_String_Warn_Warg_struct _T18B2=_TADB;void*_T18B3[1];_T18B3[0]=& _T18B2;_TADC=yyyvsp;_TADD=_TADC[0];_TADE=_TADD.l;_TADF=_TADE.first_line;_TAE0=Cyc_Position_loc_to_seg(_TADF);_TAE1=_tag_fat(_T18B3,sizeof(void*),1);Cyc_Warn_err2(_TAE0,_TAE1);}goto _TL2CF;_TL2CE: _TL2CF:
 yyval=Cyc_YY32(1);goto _LL0;case 276: _TAE2=yyyvsp;_TAE3=_TAE2[0];
# 2339 "parse.y"
yyval=_TAE3.v;goto _LL0;case 277: _TAE4=yyyvsp;_TAE5=& _TAE4[0].v;_TAE6=(union Cyc_YYSTYPE*)_TAE5;_TAE7=
# 2340 "parse.y"
Cyc_yyget_YY41(_TAE6);_TAE8=yyyvsp;_TAE9=& _TAE8[2].v;_TAEA=(union Cyc_YYSTYPE*)_TAE9;_TAEB=Cyc_yyget_YY41(_TAEA);_TAEC=Cyc_List_imp_append(_TAE7,_TAEB);yyval=Cyc_YY41(_TAEC);goto _LL0;case 278:
# 2344 "parse.y"
 yyval=Cyc_YY41(0);goto _LL0;case 279: _TAED=yyyvsp;_TAEE=_TAED[1];
# 2345 "parse.y"
yyval=_TAEE.v;goto _LL0;case 280:{struct Cyc_List_List*_T18B2=_cycalloc(sizeof(struct Cyc_List_List));_TAF0=yyyvsp;_TAF1=& _TAF0[2].v;_TAF2=(union Cyc_YYSTYPE*)_TAF1;_TAF3=
# 2346 "parse.y"
Cyc_yyget_YY45(_TAF2);_T18B2->hd=Cyc_Absyn_regionsof_eff(_TAF3);_T18B2->tl=0;_TAEF=(struct Cyc_List_List*)_T18B2;}yyval=Cyc_YY41(_TAEF);goto _LL0;case 281: _TAF4=yyyvsp;_TAF5=& _TAF4[0].v;_TAF6=(union Cyc_YYSTYPE*)_TAF5;_TAF7=
# 2348 "parse.y"
Cyc_yyget_YY45(_TAF6);_TAF8=& Cyc_Kinds_ek;_TAF9=(struct Cyc_Absyn_Kind*)_TAF8;Cyc_Parse_set_vartyp_kind(_TAF7,_TAF9,0);{struct Cyc_List_List*_T18B2=_cycalloc(sizeof(struct Cyc_List_List));_TAFB=yyyvsp;_TAFC=& _TAFB[0].v;_TAFD=(union Cyc_YYSTYPE*)_TAFC;
_T18B2->hd=Cyc_yyget_YY45(_TAFD);_T18B2->tl=0;_TAFA=(struct Cyc_List_List*)_T18B2;}yyval=Cyc_YY41(_TAFA);goto _LL0;case 282:{struct Cyc_List_List*_T18B2=_cycalloc(sizeof(struct Cyc_List_List));_TAFF=yyyvsp;_TB00=& _TAFF[0].v;_TB01=(union Cyc_YYSTYPE*)_TB00;_TB02=
# 2356 "parse.y"
Cyc_yyget_YY38(_TB01);_TB03=yyyvsp;_TB04=_TB03[0];_TB05=_TB04.l;_TB06=_TB05.first_line;_TB07=Cyc_Position_loc_to_seg(_TB06);_T18B2->hd=Cyc_Parse_type_name_to_type(_TB02,_TB07);_T18B2->tl=0;_TAFE=(struct Cyc_List_List*)_T18B2;}yyval=Cyc_YY41(_TAFE);goto _LL0;case 283:{struct Cyc_List_List*_T18B2=_cycalloc(sizeof(struct Cyc_List_List));_TB09=yyyvsp;_TB0A=& _TB09[0].v;_TB0B=(union Cyc_YYSTYPE*)_TB0A;_TB0C=
# 2358 "parse.y"
Cyc_yyget_YY38(_TB0B);_TB0D=yyyvsp;_TB0E=_TB0D[0];_TB0F=_TB0E.l;_TB10=_TB0F.first_line;_TB11=Cyc_Position_loc_to_seg(_TB10);_T18B2->hd=Cyc_Parse_type_name_to_type(_TB0C,_TB11);_TB12=yyyvsp;_TB13=& _TB12[2].v;_TB14=(union Cyc_YYSTYPE*)_TB13;_T18B2->tl=Cyc_yyget_YY41(_TB14);_TB08=(struct Cyc_List_List*)_T18B2;}yyval=Cyc_YY41(_TB08);goto _LL0;case 284:{struct Cyc_List_List*_T18B2=_cycalloc(sizeof(struct Cyc_List_List));_TB16=yyyvsp;_TB17=& _TB16[0].v;_TB18=(union Cyc_YYSTYPE*)_TB17;
# 2363 "parse.y"
_T18B2->hd=Cyc_yyget_YY38(_TB18);_T18B2->tl=0;_TB15=(struct Cyc_List_List*)_T18B2;}yyval=Cyc_YY39(_TB15);goto _LL0;case 285:{struct Cyc_List_List*_T18B2=_cycalloc(sizeof(struct Cyc_List_List));_TB1A=yyyvsp;_TB1B=& _TB1A[2].v;_TB1C=(union Cyc_YYSTYPE*)_TB1B;
# 2364 "parse.y"
_T18B2->hd=Cyc_yyget_YY38(_TB1C);_TB1D=yyyvsp;_TB1E=& _TB1D[0].v;_TB1F=(union Cyc_YYSTYPE*)_TB1E;_T18B2->tl=Cyc_yyget_YY39(_TB1F);_TB19=(struct Cyc_List_List*)_T18B2;}yyval=Cyc_YY39(_TB19);goto _LL0;case 286: _TB20=yyyvsp;_TB21=& _TB20[0].v;_TB22=(union Cyc_YYSTYPE*)_TB21;{
# 2370 "parse.y"
struct _tuple26 _T18B2=Cyc_yyget_YY36(_TB22);struct Cyc_List_List*_T18B3;struct Cyc_Parse_Type_specifier _T18B4;struct Cyc_Absyn_Tqual _T18B5;_T18B5=_T18B2.f0;_T18B4=_T18B2.f1;_T18B3=_T18B2.f2;{struct Cyc_Absyn_Tqual tq=_T18B5;struct Cyc_Parse_Type_specifier tspecs=_T18B4;struct Cyc_List_List*atts=_T18B3;_TB23=tq;_TB24=_TB23.loc;
if(_TB24!=0U)goto _TL2D0;_TB25=yyyvsp;_TB26=_TB25[0];_TB27=_TB26.l;_TB28=_TB27.first_line;tq.loc=Cyc_Position_loc_to_seg(_TB28);goto _TL2D1;_TL2D0: _TL2D1: _TB29=yyyvsp;_TB2A=& _TB29[1].v;_TB2B=(union Cyc_YYSTYPE*)_TB2A;{
struct Cyc_Parse_Declarator _T18B6=Cyc_yyget_YY28(_TB2B);struct Cyc_List_List*_T18B7;unsigned _T18B8;struct _tuple0*_T18B9;_T18B9=_T18B6.id;_T18B8=_T18B6.varloc;_T18B7=_T18B6.tms;{struct _tuple0*qv=_T18B9;unsigned varloc=_T18B8;struct Cyc_List_List*tms=_T18B7;_TB2C=tspecs;_TB2D=yyyvsp;_TB2E=_TB2D[0];_TB2F=_TB2E.l;_TB30=_TB2F.first_line;_TB31=
Cyc_Position_loc_to_seg(_TB30);{void*t=Cyc_Parse_speclist2typ(_TB2C,_TB31);
struct _tuple14 _T18BA=Cyc_Parse_apply_tms(tq,t,atts,tms);struct Cyc_List_List*_T18BB;struct Cyc_List_List*_T18BC;void*_T18BD;struct Cyc_Absyn_Tqual _T18BE;_T18BE=_T18BA.f0;_T18BD=_T18BA.f1;_T18BC=_T18BA.f2;_T18BB=_T18BA.f3;{struct Cyc_Absyn_Tqual tq2=_T18BE;void*t2=_T18BD;struct Cyc_List_List*tvs=_T18BC;struct Cyc_List_List*atts2=_T18BB;
if(tvs==0)goto _TL2D2;{struct Cyc_Warn_String_Warn_Warg_struct _T18BF;_T18BF.tag=0;
_T18BF.f1=_tag_fat("parameter with bad type params",sizeof(char),31U);_TB32=_T18BF;}{struct Cyc_Warn_String_Warn_Warg_struct _T18BF=_TB32;void*_T18C0[1];_T18C0[0]=& _T18BF;_TB33=yyyvsp;_TB34=_TB33[1];_TB35=_TB34.l;_TB36=_TB35.first_line;_TB37=Cyc_Position_loc_to_seg(_TB36);_TB38=_tag_fat(_T18C0,sizeof(void*),1);Cyc_Warn_err2(_TB37,_TB38);}goto _TL2D3;_TL2D2: _TL2D3: _TB39=
Cyc_Absyn_is_qvar_qualified(qv);if(!_TB39)goto _TL2D4;{struct Cyc_Warn_String_Warn_Warg_struct _T18BF;_T18BF.tag=0;
_T18BF.f1=_tag_fat("parameter cannot be qualified with a namespace",sizeof(char),47U);_TB3A=_T18BF;}{struct Cyc_Warn_String_Warn_Warg_struct _T18BF=_TB3A;void*_T18C0[1];_T18C0[0]=& _T18BF;_TB3B=yyyvsp;_TB3C=_TB3B[0];_TB3D=_TB3C.l;_TB3E=_TB3D.first_line;_TB3F=Cyc_Position_loc_to_seg(_TB3E);_TB40=_tag_fat(_T18C0,sizeof(void*),1);Cyc_Warn_err2(_TB3F,_TB40);}goto _TL2D5;_TL2D4: _TL2D5: _TB41=qv;_TB42=*_TB41;{
struct _fat_ptr*idopt=_TB42.f1;
if(atts2==0)goto _TL2D6;{struct Cyc_Warn_String_Warn_Warg_struct _T18BF;_T18BF.tag=0;
_T18BF.f1=_tag_fat("extra attributes on parameter, ignoring",sizeof(char),40U);_TB43=_T18BF;}{struct Cyc_Warn_String_Warn_Warg_struct _T18BF=_TB43;void*_T18C0[1];_T18C0[0]=& _T18BF;_TB44=yyyvsp;_TB45=_TB44[0];_TB46=_TB45.l;_TB47=_TB46.first_line;_TB48=Cyc_Position_loc_to_seg(_TB47);_TB49=_tag_fat(_T18C0,sizeof(void*),1);Cyc_Warn_warn2(_TB48,_TB49);}goto _TL2D7;_TL2D6: _TL2D7:{struct _tuple8*_T18BF=_cycalloc(sizeof(struct _tuple8));
_T18BF->f0=idopt;_T18BF->f1=tq2;_T18BF->f2=t2;_TB4A=(struct _tuple8*)_T18BF;}yyval=Cyc_YY38(_TB4A);goto _LL0;}}}}}}}case 287: _TB4B=yyyvsp;_TB4C=& _TB4B[0].v;_TB4D=(union Cyc_YYSTYPE*)_TB4C;{
# 2385 "parse.y"
struct _tuple26 _T18B2=Cyc_yyget_YY36(_TB4D);struct Cyc_List_List*_T18B3;struct Cyc_Parse_Type_specifier _T18B4;struct Cyc_Absyn_Tqual _T18B5;_T18B5=_T18B2.f0;_T18B4=_T18B2.f1;_T18B3=_T18B2.f2;{struct Cyc_Absyn_Tqual tq=_T18B5;struct Cyc_Parse_Type_specifier tspecs=_T18B4;struct Cyc_List_List*atts=_T18B3;_TB4E=tq;_TB4F=_TB4E.loc;
if(_TB4F!=0U)goto _TL2D8;_TB50=yyyvsp;_TB51=_TB50[0];_TB52=_TB51.l;_TB53=_TB52.first_line;tq.loc=Cyc_Position_loc_to_seg(_TB53);goto _TL2D9;_TL2D8: _TL2D9: _TB54=tspecs;_TB55=yyyvsp;_TB56=_TB55[0];_TB57=_TB56.l;_TB58=_TB57.first_line;_TB59=
Cyc_Position_loc_to_seg(_TB58);{void*t=Cyc_Parse_speclist2typ(_TB54,_TB59);
if(atts==0)goto _TL2DA;{struct Cyc_Warn_String_Warn_Warg_struct _T18B6;_T18B6.tag=0;
_T18B6.f1=_tag_fat("bad attributes on parameter, ignoring",sizeof(char),38U);_TB5A=_T18B6;}{struct Cyc_Warn_String_Warn_Warg_struct _T18B6=_TB5A;void*_T18B7[1];_T18B7[0]=& _T18B6;_TB5B=yyyvsp;_TB5C=_TB5B[0];_TB5D=_TB5C.l;_TB5E=_TB5D.first_line;_TB5F=Cyc_Position_loc_to_seg(_TB5E);_TB60=_tag_fat(_T18B7,sizeof(void*),1);Cyc_Warn_warn2(_TB5F,_TB60);}goto _TL2DB;_TL2DA: _TL2DB:{struct _tuple8*_T18B6=_cycalloc(sizeof(struct _tuple8));
_T18B6->f0=0;_T18B6->f1=tq;_T18B6->f2=t;_TB61=(struct _tuple8*)_T18B6;}yyval=Cyc_YY38(_TB61);goto _LL0;}}}case 288: _TB62=yyyvsp;_TB63=& _TB62[0].v;_TB64=(union Cyc_YYSTYPE*)_TB63;{
# 2393 "parse.y"
struct _tuple26 _T18B2=Cyc_yyget_YY36(_TB64);struct Cyc_List_List*_T18B3;struct Cyc_Parse_Type_specifier _T18B4;struct Cyc_Absyn_Tqual _T18B5;_T18B5=_T18B2.f0;_T18B4=_T18B2.f1;_T18B3=_T18B2.f2;{struct Cyc_Absyn_Tqual tq=_T18B5;struct Cyc_Parse_Type_specifier tspecs=_T18B4;struct Cyc_List_List*atts=_T18B3;_TB65=tq;_TB66=_TB65.loc;
if(_TB66!=0U)goto _TL2DC;_TB67=yyyvsp;_TB68=_TB67[0];_TB69=_TB68.l;_TB6A=_TB69.first_line;tq.loc=Cyc_Position_loc_to_seg(_TB6A);goto _TL2DD;_TL2DC: _TL2DD: _TB6B=tspecs;_TB6C=yyyvsp;_TB6D=_TB6C[0];_TB6E=_TB6D.l;_TB6F=_TB6E.first_line;_TB70=
Cyc_Position_loc_to_seg(_TB6F);{void*t=Cyc_Parse_speclist2typ(_TB6B,_TB70);_TB71=yyyvsp;_TB72=& _TB71[1].v;_TB73=(union Cyc_YYSTYPE*)_TB72;_TB74=
Cyc_yyget_YY31(_TB73);{struct Cyc_List_List*tms=_TB74.tms;
struct _tuple14 _T18B6=Cyc_Parse_apply_tms(tq,t,atts,tms);struct Cyc_List_List*_T18B7;struct Cyc_List_List*_T18B8;void*_T18B9;struct Cyc_Absyn_Tqual _T18BA;_T18BA=_T18B6.f0;_T18B9=_T18B6.f1;_T18B8=_T18B6.f2;_T18B7=_T18B6.f3;{struct Cyc_Absyn_Tqual tq2=_T18BA;void*t2=_T18B9;struct Cyc_List_List*tvs=_T18B8;struct Cyc_List_List*atts2=_T18B7;
if(tvs==0)goto _TL2DE;{struct Cyc_Warn_String_Warn_Warg_struct _T18BB;_T18BB.tag=0;
# 2400
_T18BB.f1=_tag_fat("bad type parameters on formal argument, ignoring",sizeof(char),49U);_TB75=_T18BB;}{struct Cyc_Warn_String_Warn_Warg_struct _T18BB=_TB75;void*_T18BC[1];_T18BC[0]=& _T18BB;_TB76=yyyvsp;_TB77=_TB76[0];_TB78=_TB77.l;_TB79=_TB78.first_line;_TB7A=
# 2399
Cyc_Position_loc_to_seg(_TB79);_TB7B=_tag_fat(_T18BC,sizeof(void*),1);Cyc_Warn_warn2(_TB7A,_TB7B);}goto _TL2DF;_TL2DE: _TL2DF:
# 2401
 if(atts2==0)goto _TL2E0;{struct Cyc_Warn_String_Warn_Warg_struct _T18BB;_T18BB.tag=0;
_T18BB.f1=_tag_fat("bad attributes on parameter, ignoring",sizeof(char),38U);_TB7C=_T18BB;}{struct Cyc_Warn_String_Warn_Warg_struct _T18BB=_TB7C;void*_T18BC[1];_T18BC[0]=& _T18BB;_TB7D=yyyvsp;_TB7E=_TB7D[0];_TB7F=_TB7E.l;_TB80=_TB7F.first_line;_TB81=Cyc_Position_loc_to_seg(_TB80);_TB82=_tag_fat(_T18BC,sizeof(void*),1);Cyc_Warn_warn2(_TB81,_TB82);}goto _TL2E1;_TL2E0: _TL2E1:{struct _tuple8*_T18BB=_cycalloc(sizeof(struct _tuple8));
_T18BB->f0=0;_T18BB->f1=tq2;_T18BB->f2=t2;_TB83=(struct _tuple8*)_T18BB;}yyval=Cyc_YY38(_TB83);goto _LL0;}}}}}case 289: _TB84=yyyvsp;_TB85=& _TB84[0].v;_TB86=(union Cyc_YYSTYPE*)_TB85;_TB87=
# 2408 "parse.y"
Cyc_yyget_YY37(_TB86);_TB88=Cyc_List_imp_rev(_TB87);yyval=Cyc_YY37(_TB88);goto _LL0;case 290:{struct Cyc_List_List*_T18B2=_cycalloc(sizeof(struct Cyc_List_List));{struct _fat_ptr*_T18B3=_cycalloc(sizeof(struct _fat_ptr));_TB8B=yyyvsp;_TB8C=& _TB8B[0].v;_TB8D=(union Cyc_YYSTYPE*)_TB8C;
# 2411
*_T18B3=Cyc_yyget_String_tok(_TB8D);_TB8A=(struct _fat_ptr*)_T18B3;}_T18B2->hd=_TB8A;_T18B2->tl=0;_TB89=(struct Cyc_List_List*)_T18B2;}yyval=Cyc_YY37(_TB89);goto _LL0;case 291:{struct Cyc_List_List*_T18B2=_cycalloc(sizeof(struct Cyc_List_List));{struct _fat_ptr*_T18B3=_cycalloc(sizeof(struct _fat_ptr));_TB90=yyyvsp;_TB91=& _TB90[2].v;_TB92=(union Cyc_YYSTYPE*)_TB91;
# 2412 "parse.y"
*_T18B3=Cyc_yyget_String_tok(_TB92);_TB8F=(struct _fat_ptr*)_T18B3;}_T18B2->hd=_TB8F;_TB93=yyyvsp;_TB94=& _TB93[0].v;_TB95=(union Cyc_YYSTYPE*)_TB94;_T18B2->tl=Cyc_yyget_YY37(_TB95);_TB8E=(struct Cyc_List_List*)_T18B2;}yyval=Cyc_YY37(_TB8E);goto _LL0;case 292: _TB96=yyyvsp;_TB97=_TB96[0];
# 2416 "parse.y"
yyval=_TB97.v;goto _LL0;case 293: _TB98=yyyvsp;_TB99=_TB98[0];
# 2417 "parse.y"
yyval=_TB99.v;goto _LL0;case 294:{struct Cyc_Absyn_UnresolvedMem_e_Absyn_Raw_exp_struct*_T18B2=_cycalloc(sizeof(struct Cyc_Absyn_UnresolvedMem_e_Absyn_Raw_exp_struct));_T18B2->tag=35;
# 2422 "parse.y"
_T18B2->f1=0;_T18B2->f2=0;_TB9A=(struct Cyc_Absyn_UnresolvedMem_e_Absyn_Raw_exp_struct*)_T18B2;}_TB9B=(void*)_TB9A;_TB9C=yyyvsp;_TB9D=_TB9C[0];_TB9E=_TB9D.l;_TB9F=_TB9E.first_line;_TBA0=Cyc_Position_loc_to_seg(_TB9F);_TBA1=Cyc_Absyn_new_exp(_TB9B,_TBA0);yyval=Cyc_Exp_tok(_TBA1);goto _LL0;case 295:{struct Cyc_Absyn_UnresolvedMem_e_Absyn_Raw_exp_struct*_T18B2=_cycalloc(sizeof(struct Cyc_Absyn_UnresolvedMem_e_Absyn_Raw_exp_struct));_T18B2->tag=35;
# 2424 "parse.y"
_T18B2->f1=0;_TBA3=yyyvsp;_TBA4=& _TBA3[1].v;_TBA5=(union Cyc_YYSTYPE*)_TBA4;_TBA6=Cyc_yyget_YY5(_TBA5);_T18B2->f2=Cyc_List_imp_rev(_TBA6);_TBA2=(struct Cyc_Absyn_UnresolvedMem_e_Absyn_Raw_exp_struct*)_T18B2;}_TBA7=(void*)_TBA2;_TBA8=yyyvsp;_TBA9=_TBA8[0];_TBAA=_TBA9.l;_TBAB=_TBAA.first_line;_TBAC=Cyc_Position_loc_to_seg(_TBAB);_TBAD=Cyc_Absyn_new_exp(_TBA7,_TBAC);yyval=Cyc_Exp_tok(_TBAD);goto _LL0;case 296:{struct Cyc_Absyn_UnresolvedMem_e_Absyn_Raw_exp_struct*_T18B2=_cycalloc(sizeof(struct Cyc_Absyn_UnresolvedMem_e_Absyn_Raw_exp_struct));_T18B2->tag=35;
# 2426 "parse.y"
_T18B2->f1=0;_TBAF=yyyvsp;_TBB0=& _TBAF[1].v;_TBB1=(union Cyc_YYSTYPE*)_TBB0;_TBB2=Cyc_yyget_YY5(_TBB1);_T18B2->f2=Cyc_List_imp_rev(_TBB2);_TBAE=(struct Cyc_Absyn_UnresolvedMem_e_Absyn_Raw_exp_struct*)_T18B2;}_TBB3=(void*)_TBAE;_TBB4=yyyvsp;_TBB5=_TBB4[0];_TBB6=_TBB5.l;_TBB7=_TBB6.first_line;_TBB8=Cyc_Position_loc_to_seg(_TBB7);_TBB9=Cyc_Absyn_new_exp(_TBB3,_TBB8);yyval=Cyc_Exp_tok(_TBB9);goto _LL0;case 297: _TBBA=yyyvsp;_TBBB=_TBBA[2];_TBBC=_TBBB.l;_TBBD=_TBBC.first_line;_TBBE=
# 2428 "parse.y"
Cyc_Position_loc_to_seg(_TBBD);{struct _tuple0*_T18B2=_cycalloc(sizeof(struct _tuple0));_T18B2->f0=Cyc_Absyn_Loc_n();{struct _fat_ptr*_T18B3=_cycalloc(sizeof(struct _fat_ptr));_TBC1=yyyvsp;_TBC2=& _TBC1[2].v;_TBC3=(union Cyc_YYSTYPE*)_TBC2;*_T18B3=Cyc_yyget_String_tok(_TBC3);_TBC0=(struct _fat_ptr*)_T18B3;}_T18B2->f1=_TBC0;_TBBF=(struct _tuple0*)_T18B2;}_TBC4=Cyc_Absyn_uint_type;_TBC5=yyyvsp;_TBC6=_TBC5[2];_TBC7=_TBC6.l;_TBC8=_TBC7.first_line;_TBC9=
Cyc_Position_loc_to_seg(_TBC8);_TBCA=Cyc_Absyn_uint_exp(0U,_TBC9);{
# 2428
struct Cyc_Absyn_Vardecl*vd=Cyc_Absyn_new_vardecl(_TBBE,_TBBF,_TBC4,_TBCA,0);_TBCB=vd;
# 2431
_TBCB->tq.real_const=1;{struct Cyc_Absyn_Comprehension_e_Absyn_Raw_exp_struct*_T18B2=_cycalloc(sizeof(struct Cyc_Absyn_Comprehension_e_Absyn_Raw_exp_struct));_T18B2->tag=26;
_T18B2->f1=vd;_TBCD=yyyvsp;_TBCE=& _TBCD[4].v;_TBCF=(union Cyc_YYSTYPE*)_TBCE;_T18B2->f2=Cyc_yyget_Exp_tok(_TBCF);_TBD0=yyyvsp;_TBD1=& _TBD0[6].v;_TBD2=(union Cyc_YYSTYPE*)_TBD1;_T18B2->f3=Cyc_yyget_Exp_tok(_TBD2);_T18B2->f4=0;_TBCC=(struct Cyc_Absyn_Comprehension_e_Absyn_Raw_exp_struct*)_T18B2;}_TBD3=(void*)_TBCC;_TBD4=yyyvsp;_TBD5=_TBD4[0];_TBD6=_TBD5.l;_TBD7=_TBD6.first_line;_TBD8=Cyc_Position_loc_to_seg(_TBD7);_TBD9=Cyc_Absyn_new_exp(_TBD3,_TBD8);yyval=Cyc_Exp_tok(_TBD9);goto _LL0;}case 298: _TBDA=yyyvsp;_TBDB=& _TBDA[6].v;_TBDC=(union Cyc_YYSTYPE*)_TBDB;_TBDD=
# 2436 "parse.y"
Cyc_yyget_YY38(_TBDC);_TBDE=yyyvsp;_TBDF=_TBDE[6];_TBE0=_TBDF.l;_TBE1=_TBE0.first_line;_TBE2=Cyc_Position_loc_to_seg(_TBE1);{void*t=Cyc_Parse_type_name_to_type(_TBDD,_TBE2);{struct Cyc_Absyn_ComprehensionNoinit_e_Absyn_Raw_exp_struct*_T18B2=_cycalloc(sizeof(struct Cyc_Absyn_ComprehensionNoinit_e_Absyn_Raw_exp_struct));_T18B2->tag=27;_TBE4=yyyvsp;_TBE5=& _TBE4[4].v;_TBE6=(union Cyc_YYSTYPE*)_TBE5;
_T18B2->f1=Cyc_yyget_Exp_tok(_TBE6);_T18B2->f2=t;_T18B2->f3=0;_TBE3=(struct Cyc_Absyn_ComprehensionNoinit_e_Absyn_Raw_exp_struct*)_T18B2;}_TBE7=(void*)_TBE3;_TBE8=yyyvsp;_TBE9=_TBE8[0];_TBEA=_TBE9.l;_TBEB=_TBEA.first_line;_TBEC=Cyc_Position_loc_to_seg(_TBEB);_TBED=Cyc_Absyn_new_exp(_TBE7,_TBEC);yyval=Cyc_Exp_tok(_TBED);goto _LL0;}case 299:{struct Cyc_List_List*_T18B2=_cycalloc(sizeof(struct Cyc_List_List));{struct _tuple34*_T18B3=_cycalloc(sizeof(struct _tuple34));
# 2444 "parse.y"
_T18B3->f0=0;_TBF0=yyyvsp;_TBF1=& _TBF0[0].v;_TBF2=(union Cyc_YYSTYPE*)_TBF1;_T18B3->f1=Cyc_yyget_Exp_tok(_TBF2);_TBEF=(struct _tuple34*)_T18B3;}_T18B2->hd=_TBEF;_T18B2->tl=0;_TBEE=(struct Cyc_List_List*)_T18B2;}yyval=Cyc_YY5(_TBEE);goto _LL0;case 300:{struct Cyc_List_List*_T18B2=_cycalloc(sizeof(struct Cyc_List_List));{struct _tuple34*_T18B3=_cycalloc(sizeof(struct _tuple34));_TBF5=yyyvsp;_TBF6=& _TBF5[0].v;_TBF7=(union Cyc_YYSTYPE*)_TBF6;
# 2446 "parse.y"
_T18B3->f0=Cyc_yyget_YY42(_TBF7);_TBF8=yyyvsp;_TBF9=& _TBF8[1].v;_TBFA=(union Cyc_YYSTYPE*)_TBF9;_T18B3->f1=Cyc_yyget_Exp_tok(_TBFA);_TBF4=(struct _tuple34*)_T18B3;}_T18B2->hd=_TBF4;_T18B2->tl=0;_TBF3=(struct Cyc_List_List*)_T18B2;}yyval=Cyc_YY5(_TBF3);goto _LL0;case 301:{struct Cyc_List_List*_T18B2=_cycalloc(sizeof(struct Cyc_List_List));{struct _tuple34*_T18B3=_cycalloc(sizeof(struct _tuple34));
# 2448 "parse.y"
_T18B3->f0=0;_TBFD=yyyvsp;_TBFE=& _TBFD[2].v;_TBFF=(union Cyc_YYSTYPE*)_TBFE;_T18B3->f1=Cyc_yyget_Exp_tok(_TBFF);_TBFC=(struct _tuple34*)_T18B3;}_T18B2->hd=_TBFC;_TC00=yyyvsp;_TC01=& _TC00[0].v;_TC02=(union Cyc_YYSTYPE*)_TC01;_T18B2->tl=Cyc_yyget_YY5(_TC02);_TBFB=(struct Cyc_List_List*)_T18B2;}yyval=Cyc_YY5(_TBFB);goto _LL0;case 302:{struct Cyc_List_List*_T18B2=_cycalloc(sizeof(struct Cyc_List_List));{struct _tuple34*_T18B3=_cycalloc(sizeof(struct _tuple34));_TC05=yyyvsp;_TC06=& _TC05[2].v;_TC07=(union Cyc_YYSTYPE*)_TC06;
# 2450 "parse.y"
_T18B3->f0=Cyc_yyget_YY42(_TC07);_TC08=yyyvsp;_TC09=& _TC08[3].v;_TC0A=(union Cyc_YYSTYPE*)_TC09;_T18B3->f1=Cyc_yyget_Exp_tok(_TC0A);_TC04=(struct _tuple34*)_T18B3;}_T18B2->hd=_TC04;_TC0B=yyyvsp;_TC0C=& _TC0B[0].v;_TC0D=(union Cyc_YYSTYPE*)_TC0C;_T18B2->tl=Cyc_yyget_YY5(_TC0D);_TC03=(struct Cyc_List_List*)_T18B2;}yyval=Cyc_YY5(_TC03);goto _LL0;case 303: _TC0E=yyyvsp;_TC0F=& _TC0E[0].v;_TC10=(union Cyc_YYSTYPE*)_TC0F;_TC11=
# 2454 "parse.y"
Cyc_yyget_YY42(_TC10);_TC12=Cyc_List_imp_rev(_TC11);yyval=Cyc_YY42(_TC12);goto _LL0;case 304:{struct Cyc_List_List*_T18B2=_cycalloc(sizeof(struct Cyc_List_List));{struct Cyc_Absyn_FieldName_Absyn_Designator_struct*_T18B3=_cycalloc(sizeof(struct Cyc_Absyn_FieldName_Absyn_Designator_struct));_T18B3->tag=1;{struct _fat_ptr*_T18B4=_cycalloc(sizeof(struct _fat_ptr));_TC16=yyyvsp;_TC17=& _TC16[0].v;_TC18=(union Cyc_YYSTYPE*)_TC17;
# 2455 "parse.y"
*_T18B4=Cyc_yyget_String_tok(_TC18);_TC15=(struct _fat_ptr*)_T18B4;}_T18B3->f1=_TC15;_TC14=(struct Cyc_Absyn_FieldName_Absyn_Designator_struct*)_T18B3;}_T18B2->hd=(void*)_TC14;_T18B2->tl=0;_TC13=(struct Cyc_List_List*)_T18B2;}yyval=Cyc_YY42(_TC13);goto _LL0;case 305:{struct Cyc_List_List*_T18B2=_cycalloc(sizeof(struct Cyc_List_List));_TC1A=yyyvsp;_TC1B=& _TC1A[0].v;_TC1C=(union Cyc_YYSTYPE*)_TC1B;
# 2460 "parse.y"
_T18B2->hd=Cyc_yyget_YY43(_TC1C);_T18B2->tl=0;_TC19=(struct Cyc_List_List*)_T18B2;}yyval=Cyc_YY42(_TC19);goto _LL0;case 306:{struct Cyc_List_List*_T18B2=_cycalloc(sizeof(struct Cyc_List_List));_TC1E=yyyvsp;_TC1F=& _TC1E[1].v;_TC20=(union Cyc_YYSTYPE*)_TC1F;
# 2461 "parse.y"
_T18B2->hd=Cyc_yyget_YY43(_TC20);_TC21=yyyvsp;_TC22=& _TC21[0].v;_TC23=(union Cyc_YYSTYPE*)_TC22;_T18B2->tl=Cyc_yyget_YY42(_TC23);_TC1D=(struct Cyc_List_List*)_T18B2;}yyval=Cyc_YY42(_TC1D);goto _LL0;case 307:{struct Cyc_Absyn_ArrayElement_Absyn_Designator_struct*_T18B2=_cycalloc(sizeof(struct Cyc_Absyn_ArrayElement_Absyn_Designator_struct));_T18B2->tag=0;_TC25=yyyvsp;_TC26=& _TC25[1].v;_TC27=(union Cyc_YYSTYPE*)_TC26;
# 2465 "parse.y"
_T18B2->f1=Cyc_yyget_Exp_tok(_TC27);_TC24=(struct Cyc_Absyn_ArrayElement_Absyn_Designator_struct*)_T18B2;}_TC28=(void*)_TC24;yyval=Cyc_YY43(_TC28);goto _LL0;case 308:{struct Cyc_Absyn_FieldName_Absyn_Designator_struct*_T18B2=_cycalloc(sizeof(struct Cyc_Absyn_FieldName_Absyn_Designator_struct));_T18B2->tag=1;{struct _fat_ptr*_T18B3=_cycalloc(sizeof(struct _fat_ptr));_TC2B=yyyvsp;_TC2C=& _TC2B[1].v;_TC2D=(union Cyc_YYSTYPE*)_TC2C;
# 2466 "parse.y"
*_T18B3=Cyc_yyget_String_tok(_TC2D);_TC2A=(struct _fat_ptr*)_T18B3;}_T18B2->f1=_TC2A;_TC29=(struct Cyc_Absyn_FieldName_Absyn_Designator_struct*)_T18B2;}_TC2E=(void*)_TC29;yyval=Cyc_YY43(_TC2E);goto _LL0;case 309: _TC2F=yyyvsp;_TC30=& _TC2F[0].v;_TC31=(union Cyc_YYSTYPE*)_TC30;{
# 2471 "parse.y"
struct _tuple26 _T18B2=Cyc_yyget_YY36(_TC31);struct Cyc_List_List*_T18B3;struct Cyc_Parse_Type_specifier _T18B4;struct Cyc_Absyn_Tqual _T18B5;_T18B5=_T18B2.f0;_T18B4=_T18B2.f1;_T18B3=_T18B2.f2;{struct Cyc_Absyn_Tqual tq=_T18B5;struct Cyc_Parse_Type_specifier tss=_T18B4;struct Cyc_List_List*atts=_T18B3;_TC32=tss;_TC33=yyyvsp;_TC34=_TC33[0];_TC35=_TC34.l;_TC36=_TC35.first_line;_TC37=
Cyc_Position_loc_to_seg(_TC36);{void*t=Cyc_Parse_speclist2typ(_TC32,_TC37);
if(atts==0)goto _TL2E2;_TC38=yyyvsp;_TC39=_TC38[0];_TC3A=_TC39.l;_TC3B=_TC3A.first_line;_TC3C=
Cyc_Position_loc_to_seg(_TC3B);_TC3D=_tag_fat("ignoring attributes in type",sizeof(char),28U);_TC3E=_tag_fat(0U,sizeof(void*),0);Cyc_Warn_warn(_TC3C,_TC3D,_TC3E);goto _TL2E3;_TL2E2: _TL2E3:{struct _tuple8*_T18B6=_cycalloc(sizeof(struct _tuple8));
_T18B6->f0=0;_T18B6->f1=tq;_T18B6->f2=t;_TC3F=(struct _tuple8*)_T18B6;}yyval=Cyc_YY38(_TC3F);goto _LL0;}}}case 310: _TC40=yyyvsp;_TC41=& _TC40[0].v;_TC42=(union Cyc_YYSTYPE*)_TC41;{
# 2478 "parse.y"
struct _tuple26 _T18B2=Cyc_yyget_YY36(_TC42);struct Cyc_List_List*_T18B3;struct Cyc_Parse_Type_specifier _T18B4;struct Cyc_Absyn_Tqual _T18B5;_T18B5=_T18B2.f0;_T18B4=_T18B2.f1;_T18B3=_T18B2.f2;{struct Cyc_Absyn_Tqual tq=_T18B5;struct Cyc_Parse_Type_specifier tss=_T18B4;struct Cyc_List_List*atts=_T18B3;_TC43=tss;_TC44=yyyvsp;_TC45=_TC44[0];_TC46=_TC45.l;_TC47=_TC46.first_line;_TC48=
Cyc_Position_loc_to_seg(_TC47);{void*t=Cyc_Parse_speclist2typ(_TC43,_TC48);_TC49=yyyvsp;_TC4A=& _TC49[1].v;_TC4B=(union Cyc_YYSTYPE*)_TC4A;_TC4C=
Cyc_yyget_YY31(_TC4B);{struct Cyc_List_List*tms=_TC4C.tms;
struct _tuple14 t_info=Cyc_Parse_apply_tms(tq,t,atts,tms);_TC4D=t_info;_TC4E=_TC4D.f2;
if(_TC4E==0)goto _TL2E4;_TC4F=yyyvsp;_TC50=_TC4F[1];_TC51=_TC50.l;_TC52=_TC51.first_line;_TC53=
Cyc_Position_loc_to_seg(_TC52);_TC54=_tag_fat("bad type params, ignoring",sizeof(char),26U);_TC55=_tag_fat(0U,sizeof(void*),0);Cyc_Warn_warn(_TC53,_TC54,_TC55);goto _TL2E5;_TL2E4: _TL2E5: _TC56=t_info;_TC57=_TC56.f3;
if(_TC57==0)goto _TL2E6;_TC58=yyyvsp;_TC59=_TC58[1];_TC5A=_TC59.l;_TC5B=_TC5A.first_line;_TC5C=
Cyc_Position_loc_to_seg(_TC5B);_TC5D=_tag_fat("bad specifiers, ignoring",sizeof(char),25U);_TC5E=_tag_fat(0U,sizeof(void*),0);Cyc_Warn_warn(_TC5C,_TC5D,_TC5E);goto _TL2E7;_TL2E6: _TL2E7:{struct _tuple8*_T18B6=_cycalloc(sizeof(struct _tuple8));
_T18B6->f0=0;_TC60=t_info;_T18B6->f1=_TC60.f0;_TC61=t_info;_T18B6->f2=_TC61.f1;_TC5F=(struct _tuple8*)_T18B6;}yyval=Cyc_YY38(_TC5F);goto _LL0;}}}}case 311: _TC62=yyyvsp;_TC63=& _TC62[0].v;_TC64=(union Cyc_YYSTYPE*)_TC63;_TC65=
# 2491 "parse.y"
Cyc_yyget_YY38(_TC64);_TC66=yyyvsp;_TC67=_TC66[0];_TC68=_TC67.l;_TC69=_TC68.first_line;_TC6A=Cyc_Position_loc_to_seg(_TC69);_TC6B=Cyc_Parse_type_name_to_type(_TC65,_TC6A);yyval=Cyc_YY45(_TC6B);goto _LL0;case 312: _TC6C=
# 2492 "parse.y"
Cyc_Absyn_join_eff(0);yyval=Cyc_YY45(_TC6C);goto _LL0;case 313: _TC6D=yyyvsp;_TC6E=& _TC6D[1].v;_TC6F=(union Cyc_YYSTYPE*)_TC6E;_TC70=
# 2493 "parse.y"
Cyc_yyget_YY41(_TC6F);_TC71=Cyc_Absyn_join_eff(_TC70);yyval=Cyc_YY45(_TC71);goto _LL0;case 314: _TC72=yyyvsp;_TC73=& _TC72[2].v;_TC74=(union Cyc_YYSTYPE*)_TC73;_TC75=
# 2494 "parse.y"
Cyc_yyget_YY45(_TC74);_TC76=Cyc_Absyn_regionsof_eff(_TC75);yyval=Cyc_YY45(_TC76);goto _LL0;case 315:{struct Cyc_List_List*_T18B2=_cycalloc(sizeof(struct Cyc_List_List));_TC78=yyyvsp;_TC79=& _TC78[0].v;_TC7A=(union Cyc_YYSTYPE*)_TC79;
# 2495 "parse.y"
_T18B2->hd=Cyc_yyget_YY45(_TC7A);_TC7B=yyyvsp;_TC7C=& _TC7B[2].v;_TC7D=(union Cyc_YYSTYPE*)_TC7C;_T18B2->tl=Cyc_yyget_YY41(_TC7D);_TC77=(struct Cyc_List_List*)_T18B2;}_TC7E=Cyc_Absyn_join_eff(_TC77);yyval=Cyc_YY45(_TC7E);goto _LL0;case 316: _TC7F=yyyvsp;_TC80=& _TC7F[0].v;_TC81=(union Cyc_YYSTYPE*)_TC80;_TC82=
# 2496 "parse.y"
Cyc_yyget_YY45(_TC81);yyval=Cyc_YY45(_TC82);goto _LL0;case 317:{struct Cyc_List_List*_T18B2=_cycalloc(sizeof(struct Cyc_List_List));_TC84=yyyvsp;_TC85=& _TC84[0].v;_TC86=(union Cyc_YYSTYPE*)_TC85;
# 2502 "parse.y"
_T18B2->hd=Cyc_yyget_YY45(_TC86);_T18B2->tl=0;_TC83=(struct Cyc_List_List*)_T18B2;}yyval=Cyc_YY41(_TC83);goto _LL0;case 318:{struct Cyc_List_List*_T18B2=_cycalloc(sizeof(struct Cyc_List_List));_TC88=yyyvsp;_TC89=& _TC88[2].v;_TC8A=(union Cyc_YYSTYPE*)_TC89;
# 2503 "parse.y"
_T18B2->hd=Cyc_yyget_YY45(_TC8A);_TC8B=yyyvsp;_TC8C=& _TC8B[0].v;_TC8D=(union Cyc_YYSTYPE*)_TC8C;_T18B2->tl=Cyc_yyget_YY41(_TC8D);_TC87=(struct Cyc_List_List*)_T18B2;}yyval=Cyc_YY41(_TC87);goto _LL0;case 319:{struct Cyc_Parse_Abstractdeclarator _T18B2;_TC8F=yyyvsp;_TC90=& _TC8F[0].v;_TC91=(union Cyc_YYSTYPE*)_TC90;
# 2507 "parse.y"
_T18B2.tms=Cyc_yyget_YY27(_TC91);_TC8E=_T18B2;}yyval=Cyc_YY31(_TC8E);goto _LL0;case 320: _TC92=yyyvsp;_TC93=_TC92[0];
# 2508 "parse.y"
yyval=_TC93.v;goto _LL0;case 321:{struct Cyc_Parse_Abstractdeclarator _T18B2;_TC95=yyyvsp;_TC96=& _TC95[0].v;_TC97=(union Cyc_YYSTYPE*)_TC96;_TC98=
# 2510 "parse.y"
Cyc_yyget_YY27(_TC97);_TC99=yyyvsp;_TC9A=& _TC99[1].v;_TC9B=(union Cyc_YYSTYPE*)_TC9A;_TC9C=Cyc_yyget_YY31(_TC9B);_TC9D=_TC9C.tms;_T18B2.tms=Cyc_List_imp_append(_TC98,_TC9D);_TC94=_T18B2;}yyval=Cyc_YY31(_TC94);goto _LL0;case 322: _TC9E=yyyvsp;_TC9F=_TC9E[1];
# 2515 "parse.y"
yyval=_TC9F.v;goto _LL0;case 323:{struct Cyc_Parse_Abstractdeclarator _T18B2;_TCA2=yyr;{struct Cyc_List_List*_T18B3=_region_malloc(_TCA2,0U,sizeof(struct Cyc_List_List));_TCA4=yyr;{struct Cyc_Absyn_Carray_mod_Absyn_Type_modifier_struct*_T18B4=_region_malloc(_TCA4,0U,sizeof(struct Cyc_Absyn_Carray_mod_Absyn_Type_modifier_struct));_T18B4->tag=0;_TCA5=yyyvsp;_TCA6=& _TCA5[2].v;_TCA7=(union Cyc_YYSTYPE*)_TCA6;
# 2517 "parse.y"
_T18B4->f1=Cyc_yyget_YY54(_TCA7);_TCA8=yyyvsp;_TCA9=_TCA8[2];_TCAA=_TCA9.l;_TCAB=_TCAA.first_line;_T18B4->f2=Cyc_Position_loc_to_seg(_TCAB);_TCA3=(struct Cyc_Absyn_Carray_mod_Absyn_Type_modifier_struct*)_T18B4;}_T18B3->hd=(void*)_TCA3;_T18B3->tl=0;_TCA1=(struct Cyc_List_List*)_T18B3;}_T18B2.tms=_TCA1;_TCA0=_T18B2;}yyval=Cyc_YY31(_TCA0);goto _LL0;case 324:{struct Cyc_Parse_Abstractdeclarator _T18B2;_TCAE=yyr;{struct Cyc_List_List*_T18B3=_region_malloc(_TCAE,0U,sizeof(struct Cyc_List_List));_TCB0=yyr;{struct Cyc_Absyn_Carray_mod_Absyn_Type_modifier_struct*_T18B4=_region_malloc(_TCB0,0U,sizeof(struct Cyc_Absyn_Carray_mod_Absyn_Type_modifier_struct));_T18B4->tag=0;_TCB1=yyyvsp;_TCB2=& _TCB1[3].v;_TCB3=(union Cyc_YYSTYPE*)_TCB2;
# 2519 "parse.y"
_T18B4->f1=Cyc_yyget_YY54(_TCB3);_TCB4=yyyvsp;_TCB5=_TCB4[3];_TCB6=_TCB5.l;_TCB7=_TCB6.first_line;_T18B4->f2=Cyc_Position_loc_to_seg(_TCB7);_TCAF=(struct Cyc_Absyn_Carray_mod_Absyn_Type_modifier_struct*)_T18B4;}_T18B3->hd=(void*)_TCAF;_TCB8=yyyvsp;_TCB9=& _TCB8[0].v;_TCBA=(union Cyc_YYSTYPE*)_TCB9;_TCBB=Cyc_yyget_YY31(_TCBA);_T18B3->tl=_TCBB.tms;_TCAD=(struct Cyc_List_List*)_T18B3;}_T18B2.tms=_TCAD;_TCAC=_T18B2;}yyval=Cyc_YY31(_TCAC);goto _LL0;case 325:{struct Cyc_Parse_Abstractdeclarator _T18B2;_TCBE=yyr;{struct Cyc_List_List*_T18B3=_region_malloc(_TCBE,0U,sizeof(struct Cyc_List_List));_TCC0=yyr;{struct Cyc_Absyn_ConstArray_mod_Absyn_Type_modifier_struct*_T18B4=_region_malloc(_TCC0,0U,sizeof(struct Cyc_Absyn_ConstArray_mod_Absyn_Type_modifier_struct));_T18B4->tag=1;_TCC1=yyyvsp;_TCC2=& _TCC1[1].v;_TCC3=(union Cyc_YYSTYPE*)_TCC2;
# 2521 "parse.y"
_T18B4->f1=Cyc_yyget_Exp_tok(_TCC3);_TCC4=yyyvsp;_TCC5=& _TCC4[3].v;_TCC6=(union Cyc_YYSTYPE*)_TCC5;_T18B4->f2=Cyc_yyget_YY54(_TCC6);_TCC7=yyyvsp;_TCC8=_TCC7[3];_TCC9=_TCC8.l;_TCCA=_TCC9.first_line;_T18B4->f3=Cyc_Position_loc_to_seg(_TCCA);_TCBF=(struct Cyc_Absyn_ConstArray_mod_Absyn_Type_modifier_struct*)_T18B4;}_T18B3->hd=(void*)_TCBF;_T18B3->tl=0;_TCBD=(struct Cyc_List_List*)_T18B3;}_T18B2.tms=_TCBD;_TCBC=_T18B2;}yyval=Cyc_YY31(_TCBC);goto _LL0;case 326:{struct Cyc_Parse_Abstractdeclarator _T18B2;_TCCD=yyr;{struct Cyc_List_List*_T18B3=_region_malloc(_TCCD,0U,sizeof(struct Cyc_List_List));_TCCF=yyr;{struct Cyc_Absyn_ConstArray_mod_Absyn_Type_modifier_struct*_T18B4=_region_malloc(_TCCF,0U,sizeof(struct Cyc_Absyn_ConstArray_mod_Absyn_Type_modifier_struct));_T18B4->tag=1;_TCD0=yyyvsp;_TCD1=& _TCD0[2].v;_TCD2=(union Cyc_YYSTYPE*)_TCD1;
# 2523 "parse.y"
_T18B4->f1=Cyc_yyget_Exp_tok(_TCD2);_TCD3=yyyvsp;_TCD4=& _TCD3[4].v;_TCD5=(union Cyc_YYSTYPE*)_TCD4;_T18B4->f2=Cyc_yyget_YY54(_TCD5);_TCD6=yyyvsp;_TCD7=_TCD6[4];_TCD8=_TCD7.l;_TCD9=_TCD8.first_line;_T18B4->f3=Cyc_Position_loc_to_seg(_TCD9);_TCCE=(struct Cyc_Absyn_ConstArray_mod_Absyn_Type_modifier_struct*)_T18B4;}_T18B3->hd=(void*)_TCCE;_TCDA=yyyvsp;_TCDB=& _TCDA[0].v;_TCDC=(union Cyc_YYSTYPE*)_TCDB;_TCDD=
Cyc_yyget_YY31(_TCDC);_T18B3->tl=_TCDD.tms;_TCCC=(struct Cyc_List_List*)_T18B3;}
# 2523
_T18B2.tms=_TCCC;_TCCB=_T18B2;}yyval=Cyc_YY31(_TCCB);goto _LL0;case 327: _TCDE=yyyvsp;_TCDF=& _TCDE[1].v;_TCE0=(union Cyc_YYSTYPE*)_TCDF;{
# 2527 "parse.y"
struct _tuple27*_T18B2=Cyc_yyget_YY40(_TCE0);struct Cyc_List_List*_T18B3;struct Cyc_List_List*_T18B4;void*_T18B5;struct Cyc_Absyn_VarargInfo*_T18B6;int _T18B7;struct Cyc_List_List*_T18B8;{struct _tuple27 _T18B9=*_T18B2;_T18B8=_T18B9.f0;_T18B7=_T18B9.f1;_T18B6=_T18B9.f2;_T18B5=_T18B9.f3;_T18B4=_T18B9.f4;_T18B3=_T18B9.f5;}{struct Cyc_List_List*lis=_T18B8;int b=_T18B7;struct Cyc_Absyn_VarargInfo*c=_T18B6;void*eff=_T18B5;struct Cyc_List_List*ec=_T18B4;struct Cyc_List_List*qb=_T18B3;_TCE1=yyyvsp;_TCE2=& _TCE1[3].v;_TCE3=(union Cyc_YYSTYPE*)_TCE2;{
struct _tuple21 _T18B9=Cyc_yyget_YY62(_TCE3);struct Cyc_Absyn_Exp*_T18BA;struct Cyc_Absyn_Exp*_T18BB;struct Cyc_Absyn_Exp*_T18BC;struct Cyc_Absyn_Exp*_T18BD;_T18BD=_T18B9.f0;_T18BC=_T18B9.f1;_T18BB=_T18B9.f2;_T18BA=_T18B9.f3;{struct Cyc_Absyn_Exp*chk=_T18BD;struct Cyc_Absyn_Exp*req=_T18BC;struct Cyc_Absyn_Exp*ens=_T18BB;struct Cyc_Absyn_Exp*thrws=_T18BA;{struct Cyc_Parse_Abstractdeclarator _T18BE;_TCE6=yyr;{struct Cyc_List_List*_T18BF=_region_malloc(_TCE6,0U,sizeof(struct Cyc_List_List));_TCE8=yyr;{struct Cyc_Absyn_Function_mod_Absyn_Type_modifier_struct*_T18C0=_region_malloc(_TCE8,0U,sizeof(struct Cyc_Absyn_Function_mod_Absyn_Type_modifier_struct));_T18C0->tag=3;_TCEA=yyr;{struct Cyc_Absyn_WithTypes_Absyn_Funcparams_struct*_T18C1=_region_malloc(_TCEA,0U,sizeof(struct Cyc_Absyn_WithTypes_Absyn_Funcparams_struct));_T18C1->tag=1;
_T18C1->f1=lis;_T18C1->f2=b;_T18C1->f3=c;_T18C1->f4=eff;_T18C1->f5=ec;_T18C1->f6=qb;_T18C1->f7=chk;_T18C1->f8=req;_T18C1->f9=ens;_T18C1->f10=thrws;_TCE9=(struct Cyc_Absyn_WithTypes_Absyn_Funcparams_struct*)_T18C1;}_T18C0->f1=(void*)_TCE9;_TCE7=(struct Cyc_Absyn_Function_mod_Absyn_Type_modifier_struct*)_T18C0;}_T18BF->hd=(void*)_TCE7;_T18BF->tl=0;_TCE5=(struct Cyc_List_List*)_T18BF;}_T18BE.tms=_TCE5;_TCE4=_T18BE;}yyval=Cyc_YY31(_TCE4);goto _LL0;}}}}case 328: _TCEB=yyyvsp;_TCEC=& _TCEB[2].v;_TCED=(union Cyc_YYSTYPE*)_TCEC;{
# 2532 "parse.y"
struct _tuple27*_T18B2=Cyc_yyget_YY40(_TCED);struct Cyc_List_List*_T18B3;struct Cyc_List_List*_T18B4;void*_T18B5;struct Cyc_Absyn_VarargInfo*_T18B6;int _T18B7;struct Cyc_List_List*_T18B8;{struct _tuple27 _T18B9=*_T18B2;_T18B8=_T18B9.f0;_T18B7=_T18B9.f1;_T18B6=_T18B9.f2;_T18B5=_T18B9.f3;_T18B4=_T18B9.f4;_T18B3=_T18B9.f5;}{struct Cyc_List_List*lis=_T18B8;int b=_T18B7;struct Cyc_Absyn_VarargInfo*c=_T18B6;void*eff=_T18B5;struct Cyc_List_List*ec=_T18B4;struct Cyc_List_List*qb=_T18B3;_TCEE=yyyvsp;_TCEF=& _TCEE[4].v;_TCF0=(union Cyc_YYSTYPE*)_TCEF;{
struct _tuple21 _T18B9=Cyc_yyget_YY62(_TCF0);struct Cyc_Absyn_Exp*_T18BA;struct Cyc_Absyn_Exp*_T18BB;struct Cyc_Absyn_Exp*_T18BC;struct Cyc_Absyn_Exp*_T18BD;_T18BD=_T18B9.f0;_T18BC=_T18B9.f1;_T18BB=_T18B9.f2;_T18BA=_T18B9.f3;{struct Cyc_Absyn_Exp*chk=_T18BD;struct Cyc_Absyn_Exp*req=_T18BC;struct Cyc_Absyn_Exp*ens=_T18BB;struct Cyc_Absyn_Exp*thrws=_T18BA;{struct Cyc_Parse_Abstractdeclarator _T18BE;_TCF3=yyr;{struct Cyc_List_List*_T18BF=_region_malloc(_TCF3,0U,sizeof(struct Cyc_List_List));_TCF5=yyr;{struct Cyc_Absyn_Function_mod_Absyn_Type_modifier_struct*_T18C0=_region_malloc(_TCF5,0U,sizeof(struct Cyc_Absyn_Function_mod_Absyn_Type_modifier_struct));_T18C0->tag=3;_TCF7=yyr;{struct Cyc_Absyn_WithTypes_Absyn_Funcparams_struct*_T18C1=_region_malloc(_TCF7,0U,sizeof(struct Cyc_Absyn_WithTypes_Absyn_Funcparams_struct));_T18C1->tag=1;
_T18C1->f1=lis;
_T18C1->f2=b;_T18C1->f3=c;_T18C1->f4=eff;_T18C1->f5=ec;_T18C1->f6=qb;_T18C1->f7=chk;_T18C1->f8=req;_T18C1->f9=ens;_T18C1->f10=thrws;_TCF6=(struct Cyc_Absyn_WithTypes_Absyn_Funcparams_struct*)_T18C1;}
# 2534
_T18C0->f1=(void*)_TCF6;_TCF4=(struct Cyc_Absyn_Function_mod_Absyn_Type_modifier_struct*)_T18C0;}_T18BF->hd=(void*)_TCF4;_TCF8=yyyvsp;_TCF9=& _TCF8[0].v;_TCFA=(union Cyc_YYSTYPE*)_TCF9;_TCFB=
Cyc_yyget_YY31(_TCFA);_T18BF->tl=_TCFB.tms;_TCF2=(struct Cyc_List_List*)_T18BF;}
# 2534
_T18BE.tms=_TCF2;_TCF1=_T18BE;}yyval=Cyc_YY31(_TCF1);goto _LL0;}}}}case 329: _TCFD=Cyc_List_map_c;{
# 2539
struct Cyc_List_List*(*_T18B2)(struct Cyc_Absyn_Tvar*(*)(unsigned,void*),unsigned,struct Cyc_List_List*)=(struct Cyc_List_List*(*)(struct Cyc_Absyn_Tvar*(*)(unsigned,void*),unsigned,struct Cyc_List_List*))_TCFD;_TCFC=_T18B2;}_TCFE=yyyvsp;_TCFF=_TCFE[1];_TD00=_TCFF.l;_TD01=_TD00.first_line;_TD02=Cyc_Position_loc_to_seg(_TD01);_TD03=yyyvsp;_TD04=& _TD03[2].v;_TD05=(union Cyc_YYSTYPE*)_TD04;_TD06=Cyc_yyget_YY41(_TD05);_TD07=Cyc_List_imp_rev(_TD06);{struct Cyc_List_List*ts=_TCFC(Cyc_Parse_typ2tvar,_TD02,_TD07);{struct Cyc_Parse_Abstractdeclarator _T18B2;_TD0A=yyr;{struct Cyc_List_List*_T18B3=_region_malloc(_TD0A,0U,sizeof(struct Cyc_List_List));_TD0C=yyr;{struct Cyc_Absyn_TypeParams_mod_Absyn_Type_modifier_struct*_T18B4=_region_malloc(_TD0C,0U,sizeof(struct Cyc_Absyn_TypeParams_mod_Absyn_Type_modifier_struct));_T18B4->tag=4;
_T18B4->f1=ts;_TD0D=yyyvsp;_TD0E=_TD0D[1];_TD0F=_TD0E.l;_TD10=_TD0F.first_line;_T18B4->f2=Cyc_Position_loc_to_seg(_TD10);_T18B4->f3=0;_TD0B=(struct Cyc_Absyn_TypeParams_mod_Absyn_Type_modifier_struct*)_T18B4;}_T18B3->hd=(void*)_TD0B;_TD11=yyyvsp;_TD12=& _TD11[0].v;_TD13=(union Cyc_YYSTYPE*)_TD12;_TD14=
Cyc_yyget_YY31(_TD13);_T18B3->tl=_TD14.tms;_TD09=(struct Cyc_List_List*)_T18B3;}
# 2540
_T18B2.tms=_TD09;_TD08=_T18B2;}yyval=Cyc_YY31(_TD08);goto _LL0;}case 330:{struct Cyc_Parse_Abstractdeclarator _T18B2;_TD17=yyr;{struct Cyc_List_List*_T18B3=_region_malloc(_TD17,0U,sizeof(struct Cyc_List_List));_TD19=yyr;{struct Cyc_Absyn_Attributes_mod_Absyn_Type_modifier_struct*_T18B4=_region_malloc(_TD19,0U,sizeof(struct Cyc_Absyn_Attributes_mod_Absyn_Type_modifier_struct));_T18B4->tag=5;_TD1A=yyyvsp;_TD1B=_TD1A[1];_TD1C=_TD1B.l;_TD1D=_TD1C.first_line;
# 2544 "parse.y"
_T18B4->f1=Cyc_Position_loc_to_seg(_TD1D);_TD1E=yyyvsp;_TD1F=& _TD1E[1].v;_TD20=(union Cyc_YYSTYPE*)_TD1F;_T18B4->f2=Cyc_yyget_YY46(_TD20);_TD18=(struct Cyc_Absyn_Attributes_mod_Absyn_Type_modifier_struct*)_T18B4;}_T18B3->hd=(void*)_TD18;_TD21=yyyvsp;_TD22=& _TD21[0].v;_TD23=(union Cyc_YYSTYPE*)_TD22;_TD24=Cyc_yyget_YY31(_TD23);_T18B3->tl=_TD24.tms;_TD16=(struct Cyc_List_List*)_T18B3;}_T18B2.tms=_TD16;_TD15=_T18B2;}yyval=Cyc_YY31(_TD15);goto _LL0;case 331:{struct _tuple21 _T18B2;_TD26=yyyvsp;_TD27=& _TD26[2].v;_TD28=(union Cyc_YYSTYPE*)_TD27;
# 2550 "parse.y"
_T18B2.f0=Cyc_yyget_Exp_tok(_TD28);_T18B2.f1=0;_T18B2.f2=0;_T18B2.f3=0;_TD25=_T18B2;}yyval=Cyc_YY62(_TD25);goto _LL0;case 332:{struct _tuple21 _T18B2;
# 2552 "parse.y"
_T18B2.f0=0;_TD2A=yyyvsp;_TD2B=& _TD2A[2].v;_TD2C=(union Cyc_YYSTYPE*)_TD2B;_T18B2.f1=Cyc_yyget_Exp_tok(_TD2C);_T18B2.f2=0;_T18B2.f3=0;_TD29=_T18B2;}yyval=Cyc_YY62(_TD29);goto _LL0;case 333:{struct _tuple21 _T18B2;
# 2554 "parse.y"
_T18B2.f0=0;_T18B2.f1=0;_TD2E=yyyvsp;_TD2F=& _TD2E[2].v;_TD30=(union Cyc_YYSTYPE*)_TD2F;_T18B2.f2=Cyc_yyget_Exp_tok(_TD30);_T18B2.f3=0;_TD2D=_T18B2;}yyval=Cyc_YY62(_TD2D);goto _LL0;case 334:{struct _tuple21 _T18B2;
# 2556 "parse.y"
_T18B2.f0=0;_T18B2.f1=0;_T18B2.f2=0;_TD32=yyyvsp;_TD33=& _TD32[2].v;_TD34=(union Cyc_YYSTYPE*)_TD33;_T18B2.f3=Cyc_yyget_Exp_tok(_TD34);_TD31=_T18B2;}yyval=Cyc_YY62(_TD31);goto _LL0;case 335:{struct _tuple21 _T18B2;_TD36=yyyvsp;_TD37=& _TD36[2].v;_TD38=(union Cyc_YYSTYPE*)_TD37;
# 2558 "parse.y"
_T18B2.f0=Cyc_yyget_Exp_tok(_TD38);_T18B2.f1=0;_T18B2.f2=0;_T18B2.f3=0;_TD35=_T18B2;}_TD39=yyyvsp;_TD3A=& _TD39[4].v;_TD3B=(union Cyc_YYSTYPE*)_TD3A;_TD3C=Cyc_yyget_YY62(_TD3B);_TD3D=Cyc_Parse_join_assns(_TD35,_TD3C);yyval=Cyc_YY62(_TD3D);goto _LL0;case 336:{struct _tuple21 _T18B2;
# 2560 "parse.y"
_T18B2.f0=0;_TD3F=yyyvsp;_TD40=& _TD3F[2].v;_TD41=(union Cyc_YYSTYPE*)_TD40;_T18B2.f1=Cyc_yyget_Exp_tok(_TD41);_T18B2.f2=0;_T18B2.f3=0;_TD3E=_T18B2;}_TD42=yyyvsp;_TD43=& _TD42[4].v;_TD44=(union Cyc_YYSTYPE*)_TD43;_TD45=Cyc_yyget_YY62(_TD44);_TD46=Cyc_Parse_join_assns(_TD3E,_TD45);yyval=Cyc_YY62(_TD46);goto _LL0;case 337:{struct _tuple21 _T18B2;
# 2562 "parse.y"
_T18B2.f0=0;_T18B2.f1=0;_TD48=yyyvsp;_TD49=& _TD48[2].v;_TD4A=(union Cyc_YYSTYPE*)_TD49;_T18B2.f2=Cyc_yyget_Exp_tok(_TD4A);_T18B2.f3=0;_TD47=_T18B2;}_TD4B=yyyvsp;_TD4C=& _TD4B[4].v;_TD4D=(union Cyc_YYSTYPE*)_TD4C;_TD4E=Cyc_yyget_YY62(_TD4D);_TD4F=Cyc_Parse_join_assns(_TD47,_TD4E);yyval=Cyc_YY62(_TD4F);goto _LL0;case 338:{struct _tuple21 _T18B2;
# 2564 "parse.y"
_T18B2.f0=0;_T18B2.f1=0;_T18B2.f2=0;_TD51=yyyvsp;_TD52=& _TD51[2].v;_TD53=(union Cyc_YYSTYPE*)_TD52;_T18B2.f3=Cyc_yyget_Exp_tok(_TD53);_TD50=_T18B2;}_TD54=yyyvsp;_TD55=& _TD54[4].v;_TD56=(union Cyc_YYSTYPE*)_TD55;_TD57=Cyc_yyget_YY62(_TD56);_TD58=Cyc_Parse_join_assns(_TD50,_TD57);yyval=Cyc_YY62(_TD58);goto _LL0;case 339:{struct _tuple21 _T18B2;
# 2568 "parse.y"
_T18B2.f0=0;_T18B2.f1=0;_T18B2.f2=0;_T18B2.f3=0;_TD59=_T18B2;}yyval=Cyc_YY62(_TD59);goto _LL0;case 340: _TD5A=yyyvsp;_TD5B=_TD5A[0];
# 2569 "parse.y"
yyval=_TD5B.v;goto _LL0;case 341: _TD5C=yyyvsp;_TD5D=_TD5C[0];
# 2574 "parse.y"
yyval=_TD5D.v;goto _LL0;case 342: _TD5E=yyyvsp;_TD5F=_TD5E[0];
# 2575 "parse.y"
yyval=_TD5F.v;goto _LL0;case 343: _TD60=yyyvsp;_TD61=_TD60[0];
# 2576 "parse.y"
yyval=_TD61.v;goto _LL0;case 344: _TD62=yyyvsp;_TD63=_TD62[0];
# 2577 "parse.y"
yyval=_TD63.v;goto _LL0;case 345: _TD64=yyyvsp;_TD65=_TD64[0];
# 2578 "parse.y"
yyval=_TD65.v;goto _LL0;case 346: _TD66=yyyvsp;_TD67=_TD66[0];
# 2579 "parse.y"
yyval=_TD67.v;goto _LL0;case 347:{struct Cyc_Absyn_Label_s_Absyn_Raw_stmt_struct*_T18B2=_cycalloc(sizeof(struct Cyc_Absyn_Label_s_Absyn_Raw_stmt_struct));_T18B2->tag=13;{struct _fat_ptr*_T18B3=_cycalloc(sizeof(struct _fat_ptr));_TD6A=yyyvsp;_TD6B=& _TD6A[0].v;_TD6C=(union Cyc_YYSTYPE*)_TD6B;
# 2585 "parse.y"
*_T18B3=Cyc_yyget_String_tok(_TD6C);_TD69=(struct _fat_ptr*)_T18B3;}_T18B2->f1=_TD69;_TD6D=yyyvsp;_TD6E=& _TD6D[2].v;_TD6F=(union Cyc_YYSTYPE*)_TD6E;_T18B2->f2=Cyc_yyget_Stmt_tok(_TD6F);_TD68=(struct Cyc_Absyn_Label_s_Absyn_Raw_stmt_struct*)_T18B2;}_TD70=(void*)_TD68;_TD71=yyyvsp;_TD72=_TD71[0];_TD73=_TD72.l;_TD74=_TD73.first_line;_TD75=Cyc_Position_loc_to_seg(_TD74);_TD76=Cyc_Absyn_new_stmt(_TD70,_TD75);yyval=Cyc_Stmt_tok(_TD76);goto _LL0;case 348: _TD77=yyyvsp;_TD78=_TD77[0];_TD79=_TD78.l;_TD7A=_TD79.first_line;_TD7B=
# 2589 "parse.y"
Cyc_Position_loc_to_seg(_TD7A);_TD7C=Cyc_Absyn_skip_stmt(_TD7B);yyval=Cyc_Stmt_tok(_TD7C);goto _LL0;case 349: _TD7D=yyyvsp;_TD7E=& _TD7D[0].v;_TD7F=(union Cyc_YYSTYPE*)_TD7E;_TD80=
# 2590 "parse.y"
Cyc_yyget_Exp_tok(_TD7F);_TD81=yyyvsp;_TD82=_TD81[0];_TD83=_TD82.l;_TD84=_TD83.first_line;_TD85=Cyc_Position_loc_to_seg(_TD84);_TD86=Cyc_Absyn_exp_stmt(_TD80,_TD85);yyval=Cyc_Stmt_tok(_TD86);goto _LL0;case 350:
# 2595 "parse.y"
 Cyc_Parse_inside_function_definition=1;goto _LL0;case 351:
# 2599 "parse.y"
 Cyc_Parse_inside_function_definition=0;goto _LL0;case 352: _TD87=yyyvsp;_TD88=_TD87[0];_TD89=_TD88.l;_TD8A=_TD89.first_line;_TD8B=
# 2603 "parse.y"
Cyc_Position_loc_to_seg(_TD8A);_TD8C=Cyc_Absyn_skip_stmt(_TD8B);yyval=Cyc_Stmt_tok(_TD8C);goto _LL0;case 353: _TD8D=yyyvsp;_TD8E=_TD8D[1];
# 2604 "parse.y"
yyval=_TD8E.v;goto _LL0;case 354: _TD8F=yyyvsp;_TD90=_TD8F[0];_TD91=_TD90.l;_TD92=_TD91.first_line;_TD93=
# 2609 "parse.y"
Cyc_Position_loc_to_seg(_TD92);_TD94=Cyc_Absyn_skip_stmt(_TD93);yyval=Cyc_Stmt_tok(_TD94);goto _LL0;case 355: _TD95=yyyvsp;_TD96=_TD95[1];
# 2610 "parse.y"
yyval=_TD96.v;goto _LL0;case 356: _TD97=yyyvsp;_TD98=& _TD97[0].v;_TD99=(union Cyc_YYSTYPE*)_TD98;_TD9A=
# 2615 "parse.y"
Cyc_yyget_YY16(_TD99);_TD9B=yyyvsp;_TD9C=_TD9B[0];_TD9D=_TD9C.l;_TD9E=_TD9D.first_line;_TD9F=Cyc_Position_loc_to_seg(_TD9E);_TDA0=Cyc_Absyn_skip_stmt(_TD9F);_TDA1=Cyc_Parse_flatten_declarations(_TD9A,_TDA0);yyval=Cyc_Stmt_tok(_TDA1);goto _LL0;case 357: _TDA2=yyyvsp;_TDA3=& _TDA2[0].v;_TDA4=(union Cyc_YYSTYPE*)_TDA3;_TDA5=
# 2616 "parse.y"
Cyc_yyget_YY16(_TDA4);_TDA6=yyyvsp;_TDA7=& _TDA6[1].v;_TDA8=(union Cyc_YYSTYPE*)_TDA7;_TDA9=Cyc_yyget_Stmt_tok(_TDA8);_TDAA=Cyc_Parse_flatten_declarations(_TDA5,_TDA9);yyval=Cyc_Stmt_tok(_TDAA);goto _LL0;case 358:{struct Cyc_Absyn_Label_s_Absyn_Raw_stmt_struct*_T18B2=_cycalloc(sizeof(struct Cyc_Absyn_Label_s_Absyn_Raw_stmt_struct));_T18B2->tag=13;{struct _fat_ptr*_T18B3=_cycalloc(sizeof(struct _fat_ptr));_TDAD=yyyvsp;_TDAE=& _TDAD[0].v;_TDAF=(union Cyc_YYSTYPE*)_TDAE;
# 2618 "parse.y"
*_T18B3=Cyc_yyget_String_tok(_TDAF);_TDAC=(struct _fat_ptr*)_T18B3;}_T18B2->f1=_TDAC;_TDB0=yyyvsp;_TDB1=& _TDB0[2].v;_TDB2=(union Cyc_YYSTYPE*)_TDB1;_TDB3=Cyc_yyget_YY16(_TDB2);_TDB4=Cyc_Absyn_skip_stmt(0U);_T18B2->f2=Cyc_Parse_flatten_declarations(_TDB3,_TDB4);_TDAB=(struct Cyc_Absyn_Label_s_Absyn_Raw_stmt_struct*)_T18B2;}_TDB5=(void*)_TDAB;_TDB6=yyyvsp;_TDB7=_TDB6[0];_TDB8=_TDB7.l;_TDB9=_TDB8.first_line;_TDBA=Cyc_Position_loc_to_seg(_TDB9);_TDBB=Cyc_Absyn_new_stmt(_TDB5,_TDBA);yyval=Cyc_Stmt_tok(_TDBB);goto _LL0;case 359:{struct Cyc_Absyn_Label_s_Absyn_Raw_stmt_struct*_T18B2=_cycalloc(sizeof(struct Cyc_Absyn_Label_s_Absyn_Raw_stmt_struct));_T18B2->tag=13;{struct _fat_ptr*_T18B3=_cycalloc(sizeof(struct _fat_ptr));_TDBE=yyyvsp;_TDBF=& _TDBE[0].v;_TDC0=(union Cyc_YYSTYPE*)_TDBF;
# 2620 "parse.y"
*_T18B3=Cyc_yyget_String_tok(_TDC0);_TDBD=(struct _fat_ptr*)_T18B3;}_T18B2->f1=_TDBD;_TDC1=yyyvsp;_TDC2=& _TDC1[2].v;_TDC3=(union Cyc_YYSTYPE*)_TDC2;_TDC4=Cyc_yyget_YY16(_TDC3);_TDC5=yyyvsp;_TDC6=& _TDC5[3].v;_TDC7=(union Cyc_YYSTYPE*)_TDC6;_TDC8=Cyc_yyget_Stmt_tok(_TDC7);_T18B2->f2=Cyc_Parse_flatten_declarations(_TDC4,_TDC8);_TDBC=(struct Cyc_Absyn_Label_s_Absyn_Raw_stmt_struct*)_T18B2;}_TDC9=(void*)_TDBC;_TDCA=yyyvsp;_TDCB=_TDCA[0];_TDCC=_TDCB.l;_TDCD=_TDCC.first_line;_TDCE=Cyc_Position_loc_to_seg(_TDCD);_TDCF=Cyc_Absyn_new_stmt(_TDC9,_TDCE);yyval=Cyc_Stmt_tok(_TDCF);goto _LL0;case 360: _TDD0=yyyvsp;_TDD1=_TDD0[0];
# 2621 "parse.y"
yyval=_TDD1.v;goto _LL0;case 361: _TDD2=yyyvsp;_TDD3=& _TDD2[0].v;_TDD4=(union Cyc_YYSTYPE*)_TDD3;_TDD5=
# 2622 "parse.y"
Cyc_yyget_Stmt_tok(_TDD4);_TDD6=yyyvsp;_TDD7=& _TDD6[1].v;_TDD8=(union Cyc_YYSTYPE*)_TDD7;_TDD9=Cyc_yyget_Stmt_tok(_TDD8);_TDDA=yyyvsp;_TDDB=_TDDA[0];_TDDC=_TDDB.l;_TDDD=_TDDC.first_line;_TDDE=Cyc_Position_loc_to_seg(_TDDD);_TDDF=Cyc_Absyn_seq_stmt(_TDD5,_TDD9,_TDDE);yyval=Cyc_Stmt_tok(_TDDF);goto _LL0;case 362:{struct Cyc_Absyn_Fn_d_Absyn_Raw_decl_struct*_T18B2=_cycalloc(sizeof(struct Cyc_Absyn_Fn_d_Absyn_Raw_decl_struct));_T18B2->tag=1;_TDE1=yyyvsp;_TDE2=& _TDE1[0].v;_TDE3=(union Cyc_YYSTYPE*)_TDE2;
# 2623 "parse.y"
_T18B2->f1=Cyc_yyget_YY15(_TDE3);_TDE0=(struct Cyc_Absyn_Fn_d_Absyn_Raw_decl_struct*)_T18B2;}_TDE4=(void*)_TDE0;_TDE5=yyyvsp;_TDE6=_TDE5[0];_TDE7=_TDE6.l;_TDE8=_TDE7.first_line;_TDE9=Cyc_Position_loc_to_seg(_TDE8);_TDEA=Cyc_Absyn_new_decl(_TDE4,_TDE9);_TDEB=
Cyc_Absyn_skip_stmt(0U);_TDEC=
# 2623
Cyc_Parse_flatten_decl(_TDEA,_TDEB);yyval=Cyc_Stmt_tok(_TDEC);goto _LL0;case 363:{struct Cyc_Absyn_Fn_d_Absyn_Raw_decl_struct*_T18B2=_cycalloc(sizeof(struct Cyc_Absyn_Fn_d_Absyn_Raw_decl_struct));_T18B2->tag=1;_TDEE=yyyvsp;_TDEF=& _TDEE[0].v;_TDF0=(union Cyc_YYSTYPE*)_TDEF;
# 2626 "parse.y"
_T18B2->f1=Cyc_yyget_YY15(_TDF0);_TDED=(struct Cyc_Absyn_Fn_d_Absyn_Raw_decl_struct*)_T18B2;}_TDF1=(void*)_TDED;_TDF2=yyyvsp;_TDF3=_TDF2[0];_TDF4=_TDF3.l;_TDF5=_TDF4.first_line;_TDF6=Cyc_Position_loc_to_seg(_TDF5);_TDF7=Cyc_Absyn_new_decl(_TDF1,_TDF6);_TDF8=yyyvsp;_TDF9=& _TDF8[1].v;_TDFA=(union Cyc_YYSTYPE*)_TDF9;_TDFB=Cyc_yyget_Stmt_tok(_TDFA);_TDFC=Cyc_Parse_flatten_decl(_TDF7,_TDFB);yyval=Cyc_Stmt_tok(_TDFC);goto _LL0;case 364: _TDFD=yyyvsp;_TDFE=& _TDFD[2].v;_TDFF=(union Cyc_YYSTYPE*)_TDFE;_TE00=
# 2631 "parse.y"
Cyc_yyget_Exp_tok(_TDFF);_TE01=yyyvsp;_TE02=& _TE01[4].v;_TE03=(union Cyc_YYSTYPE*)_TE02;_TE04=Cyc_yyget_Stmt_tok(_TE03);_TE05=Cyc_Absyn_skip_stmt(0U);_TE06=yyyvsp;_TE07=_TE06[0];_TE08=_TE07.l;_TE09=_TE08.first_line;_TE0A=Cyc_Position_loc_to_seg(_TE09);_TE0B=Cyc_Absyn_ifthenelse_stmt(_TE00,_TE04,_TE05,_TE0A);yyval=Cyc_Stmt_tok(_TE0B);goto _LL0;case 365: _TE0C=yyyvsp;_TE0D=& _TE0C[2].v;_TE0E=(union Cyc_YYSTYPE*)_TE0D;_TE0F=
# 2633 "parse.y"
Cyc_yyget_Exp_tok(_TE0E);_TE10=yyyvsp;_TE11=& _TE10[4].v;_TE12=(union Cyc_YYSTYPE*)_TE11;_TE13=Cyc_yyget_Stmt_tok(_TE12);_TE14=yyyvsp;_TE15=& _TE14[6].v;_TE16=(union Cyc_YYSTYPE*)_TE15;_TE17=Cyc_yyget_Stmt_tok(_TE16);_TE18=yyyvsp;_TE19=_TE18[0];_TE1A=_TE19.l;_TE1B=_TE1A.first_line;_TE1C=Cyc_Position_loc_to_seg(_TE1B);_TE1D=Cyc_Absyn_ifthenelse_stmt(_TE0F,_TE13,_TE17,_TE1C);yyval=Cyc_Stmt_tok(_TE1D);goto _LL0;case 366: _TE1E=yyyvsp;_TE1F=& _TE1E[2].v;_TE20=(union Cyc_YYSTYPE*)_TE1F;_TE21=
# 2639 "parse.y"
Cyc_yyget_Exp_tok(_TE20);_TE22=yyyvsp;_TE23=& _TE22[5].v;_TE24=(union Cyc_YYSTYPE*)_TE23;_TE25=Cyc_yyget_YY8(_TE24);_TE26=yyyvsp;_TE27=_TE26[0];_TE28=_TE27.l;_TE29=_TE28.first_line;_TE2A=Cyc_Position_loc_to_seg(_TE29);_TE2B=Cyc_Absyn_switch_stmt(_TE21,_TE25,_TE2A);yyval=Cyc_Stmt_tok(_TE2B);goto _LL0;case 367: _TE2C=yyyvsp;_TE2D=& _TE2C[1].v;_TE2E=(union Cyc_YYSTYPE*)_TE2D;_TE2F=
# 2642
Cyc_yyget_QualId_tok(_TE2E);_TE30=yyyvsp;_TE31=_TE30[1];_TE32=_TE31.l;_TE33=_TE32.first_line;_TE34=Cyc_Position_loc_to_seg(_TE33);_TE35=Cyc_Absyn_unknownid_exp(_TE2F,_TE34);_TE36=yyyvsp;_TE37=& _TE36[3].v;_TE38=(union Cyc_YYSTYPE*)_TE37;_TE39=Cyc_yyget_YY8(_TE38);_TE3A=yyyvsp;_TE3B=_TE3A[0];_TE3C=_TE3B.l;_TE3D=_TE3C.first_line;_TE3E=Cyc_Position_loc_to_seg(_TE3D);_TE3F=Cyc_Absyn_switch_stmt(_TE35,_TE39,_TE3E);yyval=Cyc_Stmt_tok(_TE3F);goto _LL0;case 368: _TE40=yyyvsp;_TE41=& _TE40[3].v;_TE42=(union Cyc_YYSTYPE*)_TE41;_TE43=
# 2645
Cyc_yyget_YY4(_TE42);_TE44=yyyvsp;_TE45=_TE44[1];_TE46=_TE45.l;_TE47=_TE46.first_line;_TE48=Cyc_Position_loc_to_seg(_TE47);_TE49=Cyc_Absyn_tuple_exp(_TE43,_TE48);_TE4A=yyyvsp;_TE4B=& _TE4A[6].v;_TE4C=(union Cyc_YYSTYPE*)_TE4B;_TE4D=Cyc_yyget_YY8(_TE4C);_TE4E=yyyvsp;_TE4F=_TE4E[0];_TE50=_TE4F.l;_TE51=_TE50.first_line;_TE52=Cyc_Position_loc_to_seg(_TE51);_TE53=Cyc_Absyn_switch_stmt(_TE49,_TE4D,_TE52);yyval=Cyc_Stmt_tok(_TE53);goto _LL0;case 369: _TE54=yyyvsp;_TE55=& _TE54[1].v;_TE56=(union Cyc_YYSTYPE*)_TE55;_TE57=
# 2649 "parse.y"
Cyc_yyget_Stmt_tok(_TE56);_TE58=yyyvsp;_TE59=& _TE58[4].v;_TE5A=(union Cyc_YYSTYPE*)_TE59;_TE5B=Cyc_yyget_YY8(_TE5A);_TE5C=yyyvsp;_TE5D=_TE5C[0];_TE5E=_TE5D.l;_TE5F=_TE5E.first_line;_TE60=Cyc_Position_loc_to_seg(_TE5F);_TE61=Cyc_Absyn_trycatch_stmt(_TE57,_TE5B,_TE60);yyval=Cyc_Stmt_tok(_TE61);goto _LL0;case 370:
# 2663 "parse.y"
 yyval=Cyc_YY8(0);goto _LL0;case 371:{struct Cyc_List_List*_T18B2=_cycalloc(sizeof(struct Cyc_List_List));{struct Cyc_Absyn_Switch_clause*_T18B3=_cycalloc(sizeof(struct Cyc_Absyn_Switch_clause));_TE64=& Cyc_Absyn_Wild_p_val;_TE65=(struct Cyc_Absyn_Wild_p_Absyn_Raw_pat_struct*)_TE64;_TE66=(void*)_TE65;_TE67=yyyvsp;_TE68=_TE67[0];_TE69=_TE68.l;_TE6A=_TE69.first_line;_TE6B=
# 2666 "parse.y"
Cyc_Position_loc_to_seg(_TE6A);_T18B3->pattern=Cyc_Absyn_new_pat(_TE66,_TE6B);_T18B3->pat_vars=0;
_T18B3->where_clause=0;_TE6C=yyyvsp;_TE6D=& _TE6C[2].v;_TE6E=(union Cyc_YYSTYPE*)_TE6D;_T18B3->body=Cyc_yyget_Stmt_tok(_TE6E);_TE6F=yyyvsp;_TE70=_TE6F[0];_TE71=_TE70.l;_TE72=_TE71.first_line;_T18B3->loc=Cyc_Position_loc_to_seg(_TE72);_TE63=(struct Cyc_Absyn_Switch_clause*)_T18B3;}
# 2666
_T18B2->hd=_TE63;_TE73=yyyvsp;_TE74=& _TE73[3].v;_TE75=(union Cyc_YYSTYPE*)_TE74;
_T18B2->tl=Cyc_yyget_YY8(_TE75);_TE62=(struct Cyc_List_List*)_T18B2;}
# 2666
yyval=Cyc_YY8(_TE62);goto _LL0;case 372:{struct Cyc_List_List*_T18B2=_cycalloc(sizeof(struct Cyc_List_List));{struct Cyc_Absyn_Switch_clause*_T18B3=_cycalloc(sizeof(struct Cyc_Absyn_Switch_clause));_TE78=yyyvsp;_TE79=& _TE78[1].v;_TE7A=(union Cyc_YYSTYPE*)_TE79;
# 2669 "parse.y"
_T18B3->pattern=Cyc_yyget_YY9(_TE7A);_T18B3->pat_vars=0;_T18B3->where_clause=0;_TE7B=yyyvsp;_TE7C=_TE7B[2];_TE7D=_TE7C.l;_TE7E=_TE7D.first_line;_TE7F=
Cyc_Position_loc_to_seg(_TE7E);_T18B3->body=Cyc_Absyn_fallthru_stmt(0,_TE7F);_TE80=yyyvsp;_TE81=_TE80[0];_TE82=_TE81.l;_TE83=_TE82.first_line;
_T18B3->loc=Cyc_Position_loc_to_seg(_TE83);_TE77=(struct Cyc_Absyn_Switch_clause*)_T18B3;}
# 2669
_T18B2->hd=_TE77;_TE84=yyyvsp;_TE85=& _TE84[3].v;_TE86=(union Cyc_YYSTYPE*)_TE85;
# 2671
_T18B2->tl=Cyc_yyget_YY8(_TE86);_TE76=(struct Cyc_List_List*)_T18B2;}
# 2669
yyval=Cyc_YY8(_TE76);goto _LL0;case 373:{struct Cyc_List_List*_T18B2=_cycalloc(sizeof(struct Cyc_List_List));{struct Cyc_Absyn_Switch_clause*_T18B3=_cycalloc(sizeof(struct Cyc_Absyn_Switch_clause));_TE89=yyyvsp;_TE8A=& _TE89[1].v;_TE8B=(union Cyc_YYSTYPE*)_TE8A;
# 2673 "parse.y"
_T18B3->pattern=Cyc_yyget_YY9(_TE8B);_T18B3->pat_vars=0;_T18B3->where_clause=0;_TE8C=yyyvsp;_TE8D=& _TE8C[3].v;_TE8E=(union Cyc_YYSTYPE*)_TE8D;_T18B3->body=Cyc_yyget_Stmt_tok(_TE8E);_TE8F=yyyvsp;_TE90=_TE8F[0];_TE91=_TE90.l;_TE92=_TE91.first_line;_T18B3->loc=Cyc_Position_loc_to_seg(_TE92);_TE88=(struct Cyc_Absyn_Switch_clause*)_T18B3;}_T18B2->hd=_TE88;_TE93=yyyvsp;_TE94=& _TE93[4].v;_TE95=(union Cyc_YYSTYPE*)_TE94;_T18B2->tl=Cyc_yyget_YY8(_TE95);_TE87=(struct Cyc_List_List*)_T18B2;}yyval=Cyc_YY8(_TE87);goto _LL0;case 374:{struct Cyc_List_List*_T18B2=_cycalloc(sizeof(struct Cyc_List_List));{struct Cyc_Absyn_Switch_clause*_T18B3=_cycalloc(sizeof(struct Cyc_Absyn_Switch_clause));_TE98=yyyvsp;_TE99=& _TE98[1].v;_TE9A=(union Cyc_YYSTYPE*)_TE99;
# 2675 "parse.y"
_T18B3->pattern=Cyc_yyget_YY9(_TE9A);_T18B3->pat_vars=0;_TE9B=yyyvsp;_TE9C=& _TE9B[3].v;_TE9D=(union Cyc_YYSTYPE*)_TE9C;_T18B3->where_clause=Cyc_yyget_Exp_tok(_TE9D);_TE9E=yyyvsp;_TE9F=_TE9E[4];_TEA0=_TE9F.l;_TEA1=_TEA0.first_line;_TEA2=
Cyc_Position_loc_to_seg(_TEA1);_T18B3->body=Cyc_Absyn_fallthru_stmt(0,_TEA2);_TEA3=yyyvsp;_TEA4=_TEA3[0];_TEA5=_TEA4.l;_TEA6=_TEA5.first_line;
_T18B3->loc=Cyc_Position_loc_to_seg(_TEA6);_TE97=(struct Cyc_Absyn_Switch_clause*)_T18B3;}
# 2675
_T18B2->hd=_TE97;_TEA7=yyyvsp;_TEA8=& _TEA7[5].v;_TEA9=(union Cyc_YYSTYPE*)_TEA8;
# 2677
_T18B2->tl=Cyc_yyget_YY8(_TEA9);_TE96=(struct Cyc_List_List*)_T18B2;}
# 2675
yyval=Cyc_YY8(_TE96);goto _LL0;case 375:{struct Cyc_List_List*_T18B2=_cycalloc(sizeof(struct Cyc_List_List));{struct Cyc_Absyn_Switch_clause*_T18B3=_cycalloc(sizeof(struct Cyc_Absyn_Switch_clause));_TEAC=yyyvsp;_TEAD=& _TEAC[1].v;_TEAE=(union Cyc_YYSTYPE*)_TEAD;
# 2679 "parse.y"
_T18B3->pattern=Cyc_yyget_YY9(_TEAE);_T18B3->pat_vars=0;_TEAF=yyyvsp;_TEB0=& _TEAF[3].v;_TEB1=(union Cyc_YYSTYPE*)_TEB0;_T18B3->where_clause=Cyc_yyget_Exp_tok(_TEB1);_TEB2=yyyvsp;_TEB3=& _TEB2[5].v;_TEB4=(union Cyc_YYSTYPE*)_TEB3;_T18B3->body=Cyc_yyget_Stmt_tok(_TEB4);_TEB5=yyyvsp;_TEB6=_TEB5[0];_TEB7=_TEB6.l;_TEB8=_TEB7.first_line;_T18B3->loc=Cyc_Position_loc_to_seg(_TEB8);_TEAB=(struct Cyc_Absyn_Switch_clause*)_T18B3;}_T18B2->hd=_TEAB;_TEB9=yyyvsp;_TEBA=& _TEB9[6].v;_TEBB=(union Cyc_YYSTYPE*)_TEBA;_T18B2->tl=Cyc_yyget_YY8(_TEBB);_TEAA=(struct Cyc_List_List*)_T18B2;}yyval=Cyc_YY8(_TEAA);goto _LL0;case 376: _TEBC=yyyvsp;_TEBD=& _TEBC[2].v;_TEBE=(union Cyc_YYSTYPE*)_TEBD;_TEBF=
# 2686 "parse.y"
Cyc_yyget_Exp_tok(_TEBE);_TEC0=yyyvsp;_TEC1=& _TEC0[4].v;_TEC2=(union Cyc_YYSTYPE*)_TEC1;_TEC3=Cyc_yyget_Stmt_tok(_TEC2);_TEC4=yyyvsp;_TEC5=_TEC4[0];_TEC6=_TEC5.l;_TEC7=_TEC6.first_line;_TEC8=Cyc_Position_loc_to_seg(_TEC7);_TEC9=Cyc_Absyn_while_stmt(_TEBF,_TEC3,_TEC8);yyval=Cyc_Stmt_tok(_TEC9);goto _LL0;case 377: _TECA=yyyvsp;_TECB=& _TECA[1].v;_TECC=(union Cyc_YYSTYPE*)_TECB;_TECD=
# 2688 "parse.y"
Cyc_yyget_Stmt_tok(_TECC);_TECE=yyyvsp;_TECF=& _TECE[4].v;_TED0=(union Cyc_YYSTYPE*)_TECF;_TED1=Cyc_yyget_Exp_tok(_TED0);_TED2=yyyvsp;_TED3=_TED2[0];_TED4=_TED3.l;_TED5=_TED4.first_line;_TED6=Cyc_Position_loc_to_seg(_TED5);_TED7=Cyc_Absyn_do_stmt(_TECD,_TED1,_TED6);yyval=Cyc_Stmt_tok(_TED7);goto _LL0;case 378: _TED8=yyyvsp;_TED9=& _TED8[2].v;_TEDA=(union Cyc_YYSTYPE*)_TED9;_TEDB=
# 2690 "parse.y"
Cyc_yyget_Exp_tok(_TEDA);_TEDC=yyyvsp;_TEDD=& _TEDC[4].v;_TEDE=(union Cyc_YYSTYPE*)_TEDD;_TEDF=Cyc_yyget_Exp_tok(_TEDE);_TEE0=yyyvsp;_TEE1=& _TEE0[6].v;_TEE2=(union Cyc_YYSTYPE*)_TEE1;_TEE3=Cyc_yyget_Exp_tok(_TEE2);_TEE4=yyyvsp;_TEE5=& _TEE4[8].v;_TEE6=(union Cyc_YYSTYPE*)_TEE5;_TEE7=Cyc_yyget_Stmt_tok(_TEE6);_TEE8=yyyvsp;_TEE9=_TEE8[0];_TEEA=_TEE9.l;_TEEB=_TEEA.first_line;_TEEC=Cyc_Position_loc_to_seg(_TEEB);_TEED=Cyc_Absyn_for_stmt(_TEDB,_TEDF,_TEE3,_TEE7,_TEEC);yyval=Cyc_Stmt_tok(_TEED);goto _LL0;case 379: _TEEE=
# 2692 "parse.y"
Cyc_Absyn_false_exp(0U);_TEEF=yyyvsp;_TEF0=& _TEEF[3].v;_TEF1=(union Cyc_YYSTYPE*)_TEF0;_TEF2=Cyc_yyget_Exp_tok(_TEF1);_TEF3=yyyvsp;_TEF4=& _TEF3[5].v;_TEF5=(union Cyc_YYSTYPE*)_TEF4;_TEF6=Cyc_yyget_Exp_tok(_TEF5);_TEF7=yyyvsp;_TEF8=& _TEF7[7].v;_TEF9=(union Cyc_YYSTYPE*)_TEF8;_TEFA=Cyc_yyget_Stmt_tok(_TEF9);_TEFB=yyyvsp;_TEFC=_TEFB[0];_TEFD=_TEFC.l;_TEFE=_TEFD.first_line;_TEFF=Cyc_Position_loc_to_seg(_TEFE);{struct Cyc_Absyn_Stmt*s=Cyc_Absyn_for_stmt(_TEEE,_TEF2,_TEF6,_TEFA,_TEFF);_TF00=yyyvsp;_TF01=& _TF00[2].v;_TF02=(union Cyc_YYSTYPE*)_TF01;_TF03=
Cyc_yyget_YY16(_TF02);_TF04=s;_TF05=Cyc_Parse_flatten_declarations(_TF03,_TF04);yyval=Cyc_Stmt_tok(_TF05);goto _LL0;}case 380: _TF06=
# 2696
Cyc_Absyn_true_exp(0U);yyval=Cyc_Exp_tok(_TF06);goto _LL0;case 381: _TF07=yyyvsp;_TF08=_TF07[0];
# 2697 "parse.y"
yyval=_TF08.v;goto _LL0;case 382:{struct _fat_ptr*_T18B2=_cycalloc(sizeof(struct _fat_ptr));_TF0A=yyyvsp;_TF0B=& _TF0A[1].v;_TF0C=(union Cyc_YYSTYPE*)_TF0B;
# 2700
*_T18B2=Cyc_yyget_String_tok(_TF0C);_TF09=(struct _fat_ptr*)_T18B2;}_TF0D=yyyvsp;_TF0E=_TF0D[0];_TF0F=_TF0E.l;_TF10=_TF0F.first_line;_TF11=Cyc_Position_loc_to_seg(_TF10);_TF12=Cyc_Absyn_goto_stmt(_TF09,_TF11);yyval=Cyc_Stmt_tok(_TF12);goto _LL0;case 383: _TF13=yyyvsp;_TF14=_TF13[0];_TF15=_TF14.l;_TF16=_TF15.first_line;_TF17=
# 2701 "parse.y"
Cyc_Position_loc_to_seg(_TF16);_TF18=Cyc_Absyn_continue_stmt(_TF17);yyval=Cyc_Stmt_tok(_TF18);goto _LL0;case 384: _TF19=yyyvsp;_TF1A=_TF19[0];_TF1B=_TF1A.l;_TF1C=_TF1B.first_line;_TF1D=
# 2702 "parse.y"
Cyc_Position_loc_to_seg(_TF1C);_TF1E=Cyc_Absyn_break_stmt(_TF1D);yyval=Cyc_Stmt_tok(_TF1E);goto _LL0;case 385: _TF1F=yyyvsp;_TF20=_TF1F[0];_TF21=_TF20.l;_TF22=_TF21.first_line;_TF23=
# 2703 "parse.y"
Cyc_Position_loc_to_seg(_TF22);_TF24=Cyc_Absyn_return_stmt(0,_TF23);yyval=Cyc_Stmt_tok(_TF24);goto _LL0;case 386: _TF25=yyyvsp;_TF26=& _TF25[1].v;_TF27=(union Cyc_YYSTYPE*)_TF26;_TF28=
# 2704 "parse.y"
Cyc_yyget_Exp_tok(_TF27);_TF29=yyyvsp;_TF2A=_TF29[0];_TF2B=_TF2A.l;_TF2C=_TF2B.first_line;_TF2D=Cyc_Position_loc_to_seg(_TF2C);_TF2E=Cyc_Absyn_return_stmt(_TF28,_TF2D);yyval=Cyc_Stmt_tok(_TF2E);goto _LL0;case 387: _TF2F=yyyvsp;_TF30=_TF2F[0];_TF31=_TF30.l;_TF32=_TF31.first_line;_TF33=
# 2706 "parse.y"
Cyc_Position_loc_to_seg(_TF32);_TF34=Cyc_Absyn_fallthru_stmt(0,_TF33);yyval=Cyc_Stmt_tok(_TF34);goto _LL0;case 388: _TF35=yyyvsp;_TF36=_TF35[0];_TF37=_TF36.l;_TF38=_TF37.first_line;_TF39=
# 2707 "parse.y"
Cyc_Position_loc_to_seg(_TF38);_TF3A=Cyc_Absyn_fallthru_stmt(0,_TF39);yyval=Cyc_Stmt_tok(_TF3A);goto _LL0;case 389: _TF3B=yyyvsp;_TF3C=& _TF3B[2].v;_TF3D=(union Cyc_YYSTYPE*)_TF3C;_TF3E=
# 2709 "parse.y"
Cyc_yyget_YY4(_TF3D);_TF3F=yyyvsp;_TF40=_TF3F[0];_TF41=_TF40.l;_TF42=_TF41.first_line;_TF43=Cyc_Position_loc_to_seg(_TF42);_TF44=Cyc_Absyn_fallthru_stmt(_TF3E,_TF43);yyval=Cyc_Stmt_tok(_TF44);goto _LL0;case 390: _TF45=yyyvsp;_TF46=_TF45[0];
# 2718 "parse.y"
yyval=_TF46.v;goto _LL0;case 391: _TF47=yyyvsp;_TF48=_TF47[0];
# 2721
yyval=_TF48.v;goto _LL0;case 392: _TF49=yyyvsp;_TF4A=& _TF49[0].v;_TF4B=(union Cyc_YYSTYPE*)_TF4A;_TF4C=
# 2723 "parse.y"
Cyc_yyget_YY9(_TF4B);_TF4D=Cyc_Parse_pat2exp(_TF4C);_TF4E=yyyvsp;_TF4F=& _TF4E[2].v;_TF50=(union Cyc_YYSTYPE*)_TF4F;_TF51=Cyc_yyget_Exp_tok(_TF50);_TF52=yyyvsp;_TF53=& _TF52[4].v;_TF54=(union Cyc_YYSTYPE*)_TF53;_TF55=Cyc_yyget_Exp_tok(_TF54);_TF56=yyyvsp;_TF57=_TF56[0];_TF58=_TF57.l;_TF59=_TF58.first_line;_TF5A=Cyc_Position_loc_to_seg(_TF59);_TF5B=Cyc_Absyn_conditional_exp(_TF4D,_TF51,_TF55,_TF5A);_TF5C=Cyc_Absyn_exp_pat(_TF5B);yyval=Cyc_YY9(_TF5C);goto _LL0;case 393: _TF5D=yyyvsp;_TF5E=_TF5D[0];
# 2726
yyval=_TF5E.v;goto _LL0;case 394: _TF5F=yyyvsp;_TF60=& _TF5F[0].v;_TF61=(union Cyc_YYSTYPE*)_TF60;_TF62=
# 2728 "parse.y"
Cyc_yyget_YY9(_TF61);_TF63=Cyc_Parse_pat2exp(_TF62);_TF64=yyyvsp;_TF65=& _TF64[2].v;_TF66=(union Cyc_YYSTYPE*)_TF65;_TF67=Cyc_yyget_Exp_tok(_TF66);_TF68=yyyvsp;_TF69=_TF68[0];_TF6A=_TF69.l;_TF6B=_TF6A.first_line;_TF6C=Cyc_Position_loc_to_seg(_TF6B);_TF6D=Cyc_Absyn_or_exp(_TF63,_TF67,_TF6C);_TF6E=Cyc_Absyn_exp_pat(_TF6D);yyval=Cyc_YY9(_TF6E);goto _LL0;case 395: _TF6F=yyyvsp;_TF70=_TF6F[0];
# 2731
yyval=_TF70.v;goto _LL0;case 396: _TF71=yyyvsp;_TF72=& _TF71[0].v;_TF73=(union Cyc_YYSTYPE*)_TF72;_TF74=
# 2733 "parse.y"
Cyc_yyget_YY9(_TF73);_TF75=Cyc_Parse_pat2exp(_TF74);_TF76=yyyvsp;_TF77=& _TF76[2].v;_TF78=(union Cyc_YYSTYPE*)_TF77;_TF79=Cyc_yyget_Exp_tok(_TF78);_TF7A=yyyvsp;_TF7B=_TF7A[0];_TF7C=_TF7B.l;_TF7D=_TF7C.first_line;_TF7E=Cyc_Position_loc_to_seg(_TF7D);_TF7F=Cyc_Absyn_and_exp(_TF75,_TF79,_TF7E);_TF80=Cyc_Absyn_exp_pat(_TF7F);yyval=Cyc_YY9(_TF80);goto _LL0;case 397: _TF81=yyyvsp;_TF82=_TF81[0];
# 2736
yyval=_TF82.v;goto _LL0;case 398: _TF83=yyyvsp;_TF84=& _TF83[0].v;_TF85=(union Cyc_YYSTYPE*)_TF84;_TF86=
# 2738 "parse.y"
Cyc_yyget_YY9(_TF85);_TF87=Cyc_Parse_pat2exp(_TF86);_TF88=yyyvsp;_TF89=& _TF88[2].v;_TF8A=(union Cyc_YYSTYPE*)_TF89;_TF8B=Cyc_yyget_Exp_tok(_TF8A);_TF8C=yyyvsp;_TF8D=_TF8C[0];_TF8E=_TF8D.l;_TF8F=_TF8E.first_line;_TF90=Cyc_Position_loc_to_seg(_TF8F);_TF91=Cyc_Absyn_prim2_exp(14U,_TF87,_TF8B,_TF90);_TF92=Cyc_Absyn_exp_pat(_TF91);yyval=Cyc_YY9(_TF92);goto _LL0;case 399: _TF93=yyyvsp;_TF94=_TF93[0];
# 2741
yyval=_TF94.v;goto _LL0;case 400: _TF95=yyyvsp;_TF96=& _TF95[0].v;_TF97=(union Cyc_YYSTYPE*)_TF96;_TF98=
# 2743 "parse.y"
Cyc_yyget_YY9(_TF97);_TF99=Cyc_Parse_pat2exp(_TF98);_TF9A=yyyvsp;_TF9B=& _TF9A[2].v;_TF9C=(union Cyc_YYSTYPE*)_TF9B;_TF9D=Cyc_yyget_Exp_tok(_TF9C);_TF9E=yyyvsp;_TF9F=_TF9E[0];_TFA0=_TF9F.l;_TFA1=_TFA0.first_line;_TFA2=Cyc_Position_loc_to_seg(_TFA1);_TFA3=Cyc_Absyn_prim2_exp(15U,_TF99,_TF9D,_TFA2);_TFA4=Cyc_Absyn_exp_pat(_TFA3);yyval=Cyc_YY9(_TFA4);goto _LL0;case 401: _TFA5=yyyvsp;_TFA6=_TFA5[0];
# 2746
yyval=_TFA6.v;goto _LL0;case 402: _TFA7=yyyvsp;_TFA8=& _TFA7[0].v;_TFA9=(union Cyc_YYSTYPE*)_TFA8;_TFAA=
# 2748 "parse.y"
Cyc_yyget_YY9(_TFA9);_TFAB=Cyc_Parse_pat2exp(_TFAA);_TFAC=yyyvsp;_TFAD=& _TFAC[2].v;_TFAE=(union Cyc_YYSTYPE*)_TFAD;_TFAF=Cyc_yyget_Exp_tok(_TFAE);_TFB0=yyyvsp;_TFB1=_TFB0[0];_TFB2=_TFB1.l;_TFB3=_TFB2.first_line;_TFB4=Cyc_Position_loc_to_seg(_TFB3);_TFB5=Cyc_Absyn_prim2_exp(13U,_TFAB,_TFAF,_TFB4);_TFB6=Cyc_Absyn_exp_pat(_TFB5);yyval=Cyc_YY9(_TFB6);goto _LL0;case 403: _TFB7=yyyvsp;_TFB8=_TFB7[0];
# 2751
yyval=_TFB8.v;goto _LL0;case 404: _TFB9=yyyvsp;_TFBA=& _TFB9[1].v;_TFBB=(union Cyc_YYSTYPE*)_TFBA;_TFBC=
# 2753 "parse.y"
Cyc_yyget_YY69(_TFBB);_TFBD=yyyvsp;_TFBE=& _TFBD[0].v;_TFBF=(union Cyc_YYSTYPE*)_TFBE;_TFC0=Cyc_yyget_YY9(_TFBF);_TFC1=Cyc_Parse_pat2exp(_TFC0);_TFC2=yyyvsp;_TFC3=& _TFC2[2].v;_TFC4=(union Cyc_YYSTYPE*)_TFC3;_TFC5=Cyc_yyget_Exp_tok(_TFC4);_TFC6=yyyvsp;_TFC7=_TFC6[0];_TFC8=_TFC7.l;_TFC9=_TFC8.first_line;_TFCA=Cyc_Position_loc_to_seg(_TFC9);_TFCB=_TFBC(_TFC1,_TFC5,_TFCA);_TFCC=Cyc_Absyn_exp_pat(_TFCB);yyval=Cyc_YY9(_TFCC);goto _LL0;case 405: _TFCD=yyyvsp;_TFCE=_TFCD[0];
# 2756
yyval=_TFCE.v;goto _LL0;case 406: _TFCF=yyyvsp;_TFD0=& _TFCF[1].v;_TFD1=(union Cyc_YYSTYPE*)_TFD0;_TFD2=
# 2758 "parse.y"
Cyc_yyget_YY69(_TFD1);_TFD3=yyyvsp;_TFD4=& _TFD3[0].v;_TFD5=(union Cyc_YYSTYPE*)_TFD4;_TFD6=Cyc_yyget_YY9(_TFD5);_TFD7=Cyc_Parse_pat2exp(_TFD6);_TFD8=yyyvsp;_TFD9=& _TFD8[2].v;_TFDA=(union Cyc_YYSTYPE*)_TFD9;_TFDB=Cyc_yyget_Exp_tok(_TFDA);_TFDC=yyyvsp;_TFDD=_TFDC[0];_TFDE=_TFDD.l;_TFDF=_TFDE.first_line;_TFE0=Cyc_Position_loc_to_seg(_TFDF);_TFE1=_TFD2(_TFD7,_TFDB,_TFE0);_TFE2=Cyc_Absyn_exp_pat(_TFE1);yyval=Cyc_YY9(_TFE2);goto _LL0;case 407: _TFE3=yyyvsp;_TFE4=_TFE3[0];
# 2761
yyval=_TFE4.v;goto _LL0;case 408: _TFE5=yyyvsp;_TFE6=& _TFE5[0].v;_TFE7=(union Cyc_YYSTYPE*)_TFE6;_TFE8=
# 2763 "parse.y"
Cyc_yyget_YY9(_TFE7);_TFE9=Cyc_Parse_pat2exp(_TFE8);_TFEA=yyyvsp;_TFEB=& _TFEA[2].v;_TFEC=(union Cyc_YYSTYPE*)_TFEB;_TFED=Cyc_yyget_Exp_tok(_TFEC);_TFEE=yyyvsp;_TFEF=_TFEE[0];_TFF0=_TFEF.l;_TFF1=_TFF0.first_line;_TFF2=Cyc_Position_loc_to_seg(_TFF1);_TFF3=Cyc_Absyn_prim2_exp(16U,_TFE9,_TFED,_TFF2);_TFF4=Cyc_Absyn_exp_pat(_TFF3);yyval=Cyc_YY9(_TFF4);goto _LL0;case 409: _TFF5=yyyvsp;_TFF6=& _TFF5[0].v;_TFF7=(union Cyc_YYSTYPE*)_TFF6;_TFF8=
# 2765 "parse.y"
Cyc_yyget_YY9(_TFF7);_TFF9=Cyc_Parse_pat2exp(_TFF8);_TFFA=yyyvsp;_TFFB=& _TFFA[2].v;_TFFC=(union Cyc_YYSTYPE*)_TFFB;_TFFD=Cyc_yyget_Exp_tok(_TFFC);_TFFE=yyyvsp;_TFFF=_TFFE[0];_T1000=_TFFF.l;_T1001=_T1000.first_line;_T1002=Cyc_Position_loc_to_seg(_T1001);_T1003=Cyc_Absyn_prim2_exp(17U,_TFF9,_TFFD,_T1002);_T1004=Cyc_Absyn_exp_pat(_T1003);yyval=Cyc_YY9(_T1004);goto _LL0;case 410: _T1005=yyyvsp;_T1006=_T1005[0];
# 2768
yyval=_T1006.v;goto _LL0;case 411: _T1007=yyyvsp;_T1008=& _T1007[1].v;_T1009=(union Cyc_YYSTYPE*)_T1008;_T100A=
# 2770 "parse.y"
Cyc_yyget_YY6(_T1009);_T100B=yyyvsp;_T100C=& _T100B[0].v;_T100D=(union Cyc_YYSTYPE*)_T100C;_T100E=Cyc_yyget_YY9(_T100D);_T100F=Cyc_Parse_pat2exp(_T100E);_T1010=yyyvsp;_T1011=& _T1010[2].v;_T1012=(union Cyc_YYSTYPE*)_T1011;_T1013=Cyc_yyget_Exp_tok(_T1012);_T1014=yyyvsp;_T1015=_T1014[0];_T1016=_T1015.l;_T1017=_T1016.first_line;_T1018=Cyc_Position_loc_to_seg(_T1017);_T1019=Cyc_Absyn_prim2_exp(_T100A,_T100F,_T1013,_T1018);_T101A=Cyc_Absyn_exp_pat(_T1019);yyval=Cyc_YY9(_T101A);goto _LL0;case 412: _T101B=yyyvsp;_T101C=_T101B[0];
# 2773
yyval=_T101C.v;goto _LL0;case 413: _T101D=yyyvsp;_T101E=& _T101D[1].v;_T101F=(union Cyc_YYSTYPE*)_T101E;_T1020=
# 2775 "parse.y"
Cyc_yyget_YY6(_T101F);_T1021=yyyvsp;_T1022=& _T1021[0].v;_T1023=(union Cyc_YYSTYPE*)_T1022;_T1024=Cyc_yyget_YY9(_T1023);_T1025=Cyc_Parse_pat2exp(_T1024);_T1026=yyyvsp;_T1027=& _T1026[2].v;_T1028=(union Cyc_YYSTYPE*)_T1027;_T1029=Cyc_yyget_Exp_tok(_T1028);_T102A=yyyvsp;_T102B=_T102A[0];_T102C=_T102B.l;_T102D=_T102C.first_line;_T102E=Cyc_Position_loc_to_seg(_T102D);_T102F=Cyc_Absyn_prim2_exp(_T1020,_T1025,_T1029,_T102E);_T1030=Cyc_Absyn_exp_pat(_T102F);yyval=Cyc_YY9(_T1030);goto _LL0;case 414: _T1031=yyyvsp;_T1032=_T1031[0];
# 2778
yyval=_T1032.v;goto _LL0;case 415: _T1033=yyyvsp;_T1034=& _T1033[1].v;_T1035=(union Cyc_YYSTYPE*)_T1034;_T1036=
# 2780 "parse.y"
Cyc_yyget_YY38(_T1035);_T1037=yyyvsp;_T1038=_T1037[1];_T1039=_T1038.l;_T103A=_T1039.first_line;_T103B=Cyc_Position_loc_to_seg(_T103A);{void*t=Cyc_Parse_type_name_to_type(_T1036,_T103B);_T103C=t;_T103D=yyyvsp;_T103E=& _T103D[3].v;_T103F=(union Cyc_YYSTYPE*)_T103E;_T1040=
Cyc_yyget_Exp_tok(_T103F);_T1041=yyyvsp;_T1042=_T1041[0];_T1043=_T1042.l;_T1044=_T1043.first_line;_T1045=Cyc_Position_loc_to_seg(_T1044);_T1046=Cyc_Absyn_cast_exp(_T103C,_T1040,1,0U,_T1045);_T1047=Cyc_Absyn_exp_pat(_T1046);yyval=Cyc_YY9(_T1047);goto _LL0;}case 416: _T1048=yyyvsp;_T1049=_T1048[0];
# 2786 "parse.y"
yyval=_T1049.v;goto _LL0;case 417: _T104A=yyyvsp;_T104B=& _T104A[0].v;_T104C=(union Cyc_YYSTYPE*)_T104B;_T104D=
# 2790 "parse.y"
Cyc_yyget_YY6(_T104C);_T104E=yyyvsp;_T104F=& _T104E[1].v;_T1050=(union Cyc_YYSTYPE*)_T104F;_T1051=Cyc_yyget_Exp_tok(_T1050);_T1052=yyyvsp;_T1053=_T1052[0];_T1054=_T1053.l;_T1055=_T1054.first_line;_T1056=Cyc_Position_loc_to_seg(_T1055);_T1057=Cyc_Absyn_prim1_exp(_T104D,_T1051,_T1056);_T1058=Cyc_Absyn_exp_pat(_T1057);yyval=Cyc_YY9(_T1058);goto _LL0;case 418: _T1059=yyyvsp;_T105A=& _T1059[2].v;_T105B=(union Cyc_YYSTYPE*)_T105A;_T105C=
# 2792 "parse.y"
Cyc_yyget_YY38(_T105B);_T105D=yyyvsp;_T105E=_T105D[2];_T105F=_T105E.l;_T1060=_T105F.first_line;_T1061=Cyc_Position_loc_to_seg(_T1060);{void*t=Cyc_Parse_type_name_to_type(_T105C,_T1061);_T1062=t;_T1063=yyyvsp;_T1064=_T1063[0];_T1065=_T1064.l;_T1066=_T1065.first_line;_T1067=
Cyc_Position_loc_to_seg(_T1066);_T1068=Cyc_Absyn_sizeoftype_exp(_T1062,_T1067);_T1069=Cyc_Absyn_exp_pat(_T1068);yyval=Cyc_YY9(_T1069);goto _LL0;}case 419: _T106A=yyyvsp;_T106B=& _T106A[1].v;_T106C=(union Cyc_YYSTYPE*)_T106B;_T106D=
# 2795 "parse.y"
Cyc_yyget_Exp_tok(_T106C);_T106E=yyyvsp;_T106F=_T106E[0];_T1070=_T106F.l;_T1071=_T1070.first_line;_T1072=Cyc_Position_loc_to_seg(_T1071);_T1073=Cyc_Absyn_sizeofexp_exp(_T106D,_T1072);_T1074=Cyc_Absyn_exp_pat(_T1073);yyval=Cyc_YY9(_T1074);goto _LL0;case 420: _T1075=yyyvsp;_T1076=& _T1075[2].v;_T1077=(union Cyc_YYSTYPE*)_T1076;_T1078=
# 2797 "parse.y"
Cyc_yyget_YY38(_T1077);_T1079=*_T1078;_T107A=_T1079.f2;_T107B=yyyvsp;_T107C=& _T107B[4].v;_T107D=(union Cyc_YYSTYPE*)_T107C;_T107E=Cyc_yyget_YY3(_T107D);_T107F=Cyc_List_imp_rev(_T107E);_T1080=yyyvsp;_T1081=_T1080[0];_T1082=_T1081.l;_T1083=_T1082.first_line;_T1084=Cyc_Position_loc_to_seg(_T1083);_T1085=Cyc_Absyn_offsetof_exp(_T107A,_T107F,_T1084);_T1086=Cyc_Absyn_exp_pat(_T1085);yyval=Cyc_YY9(_T1086);goto _LL0;case 421: _T1087=yyyvsp;_T1088=_T1087[0];
# 2802 "parse.y"
yyval=_T1088.v;goto _LL0;case 422: _T1089=yyyvsp;_T108A=_T1089[0];
# 2810 "parse.y"
yyval=_T108A.v;goto _LL0;case 423: _T108B=& Cyc_Absyn_Wild_p_val;_T108C=(struct Cyc_Absyn_Wild_p_Absyn_Raw_pat_struct*)_T108B;_T108D=(void*)_T108C;_T108E=yyyvsp;_T108F=_T108E[0];_T1090=_T108F.l;_T1091=_T1090.first_line;_T1092=
# 2814 "parse.y"
Cyc_Position_loc_to_seg(_T1091);_T1093=Cyc_Absyn_new_pat(_T108D,_T1092);yyval=Cyc_YY9(_T1093);goto _LL0;case 424: _T1094=yyyvsp;_T1095=& _T1094[1].v;_T1096=(union Cyc_YYSTYPE*)_T1095;_T1097=
# 2815 "parse.y"
Cyc_yyget_Exp_tok(_T1096);_T1098=Cyc_Absyn_exp_pat(_T1097);yyval=Cyc_YY9(_T1098);goto _LL0;case 425:{struct Cyc_Absyn_UnknownId_p_Absyn_Raw_pat_struct*_T18B2=_cycalloc(sizeof(struct Cyc_Absyn_UnknownId_p_Absyn_Raw_pat_struct));_T18B2->tag=14;_T109A=yyyvsp;_T109B=& _T109A[0].v;_T109C=(union Cyc_YYSTYPE*)_T109B;
# 2816 "parse.y"
_T18B2->f1=Cyc_yyget_QualId_tok(_T109C);_T1099=(struct Cyc_Absyn_UnknownId_p_Absyn_Raw_pat_struct*)_T18B2;}_T109D=(void*)_T1099;_T109E=yyyvsp;_T109F=_T109E[0];_T10A0=_T109F.l;_T10A1=_T10A0.first_line;_T10A2=Cyc_Position_loc_to_seg(_T10A1);_T10A3=Cyc_Absyn_new_pat(_T109D,_T10A2);yyval=Cyc_YY9(_T10A3);goto _LL0;case 426:{struct Cyc_Absyn_Pointer_p_Absyn_Raw_pat_struct*_T18B2=_cycalloc(sizeof(struct Cyc_Absyn_Pointer_p_Absyn_Raw_pat_struct));_T18B2->tag=5;_T10A5=yyyvsp;_T10A6=& _T10A5[1].v;_T10A7=(union Cyc_YYSTYPE*)_T10A6;
# 2817 "parse.y"
_T18B2->f1=Cyc_yyget_YY9(_T10A7);_T10A4=(struct Cyc_Absyn_Pointer_p_Absyn_Raw_pat_struct*)_T18B2;}_T10A8=(void*)_T10A4;_T10A9=yyyvsp;_T10AA=_T10A9[0];_T10AB=_T10AA.l;_T10AC=_T10AB.first_line;_T10AD=Cyc_Position_loc_to_seg(_T10AC);_T10AE=Cyc_Absyn_new_pat(_T10A8,_T10AD);yyval=Cyc_YY9(_T10AE);goto _LL0;case 427: _T10AF=yyyvsp;_T10B0=& _T10AF[0].v;_T10B1=(union Cyc_YYSTYPE*)_T10B0;{
# 2819 "parse.y"
struct Cyc_Absyn_Exp*e=Cyc_yyget_Exp_tok(_T10B1);_T10B2=e;{
void*_T18B2=_T10B2->r;struct _fat_ptr _T18B3;int _T18B4;short _T18B5;char _T18B6;enum Cyc_Absyn_Sign _T18B7;_T10B3=(int*)_T18B2;_T10B4=*_T10B3;if(_T10B4!=0)goto _TL2E8;_T10B5=(struct Cyc_Absyn_Const_e_Absyn_Raw_exp_struct*)_T18B2;_T10B6=_T10B5->f1;_T10B7=_T10B6.LongLong_c;_T10B8=_T10B7.tag;switch(_T10B8){case 2:{struct Cyc_Absyn_Const_e_Absyn_Raw_exp_struct*_T18B8=(struct Cyc_Absyn_Const_e_Absyn_Raw_exp_struct*)_T18B2;_T10B9=_T18B8->f1;_T10BA=_T10B9.Char_c;_T10BB=_T10BA.val;_T18B7=_T10BB.f0;_T10BC=_T18B8->f1;_T10BD=_T10BC.Char_c;_T10BE=_T10BD.val;_T18B6=_T10BE.f1;}{enum Cyc_Absyn_Sign s=_T18B7;char i=_T18B6;{struct Cyc_Absyn_Char_p_Absyn_Raw_pat_struct*_T18B8=_cycalloc(sizeof(struct Cyc_Absyn_Char_p_Absyn_Raw_pat_struct));_T18B8->tag=10;
# 2822
_T18B8->f1=i;_T10BF=(struct Cyc_Absyn_Char_p_Absyn_Raw_pat_struct*)_T18B8;}_T10C0=(void*)_T10BF;_T10C1=e;_T10C2=_T10C1->loc;_T10C3=Cyc_Absyn_new_pat(_T10C0,_T10C2);yyval=Cyc_YY9(_T10C3);goto _LL544;}case 4:{struct Cyc_Absyn_Const_e_Absyn_Raw_exp_struct*_T18B8=(struct Cyc_Absyn_Const_e_Absyn_Raw_exp_struct*)_T18B2;_T10C4=_T18B8->f1;_T10C5=_T10C4.Short_c;_T10C6=_T10C5.val;_T18B7=_T10C6.f0;_T10C7=_T18B8->f1;_T10C8=_T10C7.Short_c;_T10C9=_T10C8.val;_T18B5=_T10C9.f1;}{enum Cyc_Absyn_Sign s=_T18B7;short i=_T18B5;{struct Cyc_Absyn_Int_p_Absyn_Raw_pat_struct*_T18B8=_cycalloc(sizeof(struct Cyc_Absyn_Int_p_Absyn_Raw_pat_struct));_T18B8->tag=9;
# 2824
_T18B8->f1=s;_T10CB=i;_T18B8->f2=(int)_T10CB;_T10CA=(struct Cyc_Absyn_Int_p_Absyn_Raw_pat_struct*)_T18B8;}_T10CC=(void*)_T10CA;_T10CD=e;_T10CE=_T10CD->loc;_T10CF=Cyc_Absyn_new_pat(_T10CC,_T10CE);yyval=Cyc_YY9(_T10CF);goto _LL544;}case 5:{struct Cyc_Absyn_Const_e_Absyn_Raw_exp_struct*_T18B8=(struct Cyc_Absyn_Const_e_Absyn_Raw_exp_struct*)_T18B2;_T10D0=_T18B8->f1;_T10D1=_T10D0.Int_c;_T10D2=_T10D1.val;_T18B7=_T10D2.f0;_T10D3=_T18B8->f1;_T10D4=_T10D3.Int_c;_T10D5=_T10D4.val;_T18B4=_T10D5.f1;}{enum Cyc_Absyn_Sign s=_T18B7;int i=_T18B4;{struct Cyc_Absyn_Int_p_Absyn_Raw_pat_struct*_T18B8=_cycalloc(sizeof(struct Cyc_Absyn_Int_p_Absyn_Raw_pat_struct));_T18B8->tag=9;
# 2826
_T18B8->f1=s;_T18B8->f2=i;_T10D6=(struct Cyc_Absyn_Int_p_Absyn_Raw_pat_struct*)_T18B8;}_T10D7=(void*)_T10D6;_T10D8=e;_T10D9=_T10D8->loc;_T10DA=Cyc_Absyn_new_pat(_T10D7,_T10D9);yyval=Cyc_YY9(_T10DA);goto _LL544;}case 7:{struct Cyc_Absyn_Const_e_Absyn_Raw_exp_struct*_T18B8=(struct Cyc_Absyn_Const_e_Absyn_Raw_exp_struct*)_T18B2;_T10DB=_T18B8->f1;_T10DC=_T10DB.Float_c;_T10DD=_T10DC.val;_T18B3=_T10DD.f0;_T10DE=_T18B8->f1;_T10DF=_T10DE.Float_c;_T10E0=_T10DF.val;_T18B4=_T10E0.f1;}{struct _fat_ptr s=_T18B3;int i=_T18B4;{struct Cyc_Absyn_Float_p_Absyn_Raw_pat_struct*_T18B8=_cycalloc(sizeof(struct Cyc_Absyn_Float_p_Absyn_Raw_pat_struct));_T18B8->tag=11;
# 2828
_T18B8->f1=s;_T18B8->f2=i;_T10E1=(struct Cyc_Absyn_Float_p_Absyn_Raw_pat_struct*)_T18B8;}_T10E2=(void*)_T10E1;_T10E3=e;_T10E4=_T10E3->loc;_T10E5=Cyc_Absyn_new_pat(_T10E2,_T10E4);yyval=Cyc_YY9(_T10E5);goto _LL544;}case 1: _T10E6=& Cyc_Absyn_Null_p_val;_T10E7=(struct Cyc_Absyn_Null_p_Absyn_Raw_pat_struct*)_T10E6;_T10E8=(void*)_T10E7;_T10E9=e;_T10EA=_T10E9->loc;_T10EB=
# 2830
Cyc_Absyn_new_pat(_T10E8,_T10EA);yyval=Cyc_YY9(_T10EB);goto _LL544;case 8: goto _LL552;case 9: _LL552: _T10EC=yyyvsp;_T10ED=_T10EC[0];_T10EE=_T10ED.l;_T10EF=_T10EE.first_line;_T10F0=
# 2833
Cyc_Position_loc_to_seg(_T10EF);_T10F1=_tag_fat("strings cannot occur within patterns",sizeof(char),37U);_T10F2=_tag_fat(0U,sizeof(void*),0);Cyc_Warn_err(_T10F0,_T10F1,_T10F2);goto _LL544;case 6: _T10F3=yyyvsp;_T10F4=_T10F3[0];_T10F5=_T10F4.l;_T10F6=_T10F5.first_line;_T10F7=
# 2835
Cyc_Position_loc_to_seg(_T10F6);_T10F8=_tag_fat("long long's in patterns not yet implemented",sizeof(char),44U);_T10F9=_tag_fat(0U,sizeof(void*),0);Cyc_Warn_err(_T10F7,_T10F8,_T10F9);goto _LL544;default: goto _LL555;}goto _TL2E9;_TL2E8: _LL555: _T10FA=yyyvsp;_T10FB=_T10FA[0];_T10FC=_T10FB.l;_T10FD=_T10FC.first_line;_T10FE=
# 2837
Cyc_Position_loc_to_seg(_T10FD);_T10FF=_tag_fat("bad constant in case",sizeof(char),21U);_T1100=_tag_fat(0U,sizeof(void*),0);Cyc_Warn_err(_T10FE,_T10FF,_T1100);_TL2E9: _LL544:;}goto _LL0;}case 428: _T1101=yyyvsp;_T1102=& _T1101[1].v;_T1103=(union Cyc_YYSTYPE*)_T1102;_T1104=
# 2841 "parse.y"
Cyc_yyget_String_tok(_T1103);_T1105=_tag_fat("as",sizeof(char),3U);_T1106=Cyc_strcmp(_T1104,_T1105);if(_T1106==0)goto _TL2EB;_T1107=yyyvsp;_T1108=_T1107[1];_T1109=_T1108.l;_T110A=_T1109.first_line;_T110B=
Cyc_Position_loc_to_seg(_T110A);_T110C=_tag_fat("expecting `as'",sizeof(char),15U);_T110D=_tag_fat(0U,sizeof(void*),0);Cyc_Warn_err(_T110B,_T110C,_T110D);goto _TL2EC;_TL2EB: _TL2EC:{struct Cyc_Absyn_Var_p_Absyn_Raw_pat_struct*_T18B2=_cycalloc(sizeof(struct Cyc_Absyn_Var_p_Absyn_Raw_pat_struct));_T18B2->tag=1;_T110F=yyyvsp;_T1110=_T110F[0];_T1111=_T1110.l;_T1112=_T1111.first_line;_T1113=
Cyc_Position_loc_to_seg(_T1112);{struct _tuple0*_T18B3=_cycalloc(sizeof(struct _tuple0));_T18B3->f0=Cyc_Absyn_Loc_n();{struct _fat_ptr*_T18B4=_cycalloc(sizeof(struct _fat_ptr));_T1116=yyyvsp;_T1117=& _T1116[0].v;_T1118=(union Cyc_YYSTYPE*)_T1117;*_T18B4=Cyc_yyget_String_tok(_T1118);_T1115=(struct _fat_ptr*)_T18B4;}_T18B3->f1=_T1115;_T1114=(struct _tuple0*)_T18B3;}_T1119=Cyc_Absyn_void_type;_T18B2->f1=Cyc_Absyn_new_vardecl(_T1113,_T1114,_T1119,0,0);_T111A=yyyvsp;_T111B=& _T111A[2].v;_T111C=(union Cyc_YYSTYPE*)_T111B;
_T18B2->f2=Cyc_yyget_YY9(_T111C);_T110E=(struct Cyc_Absyn_Var_p_Absyn_Raw_pat_struct*)_T18B2;}_T111D=(void*)_T110E;_T111E=yyyvsp;_T111F=_T111E[0];_T1120=_T111F.l;_T1121=_T1120.first_line;_T1122=Cyc_Position_loc_to_seg(_T1121);_T1123=
# 2843
Cyc_Absyn_new_pat(_T111D,_T1122);yyval=Cyc_YY9(_T1123);goto _LL0;case 429: _T1124=yyyvsp;_T1125=& _T1124[0].v;_T1126=(union Cyc_YYSTYPE*)_T1125;_T1127=
# 2847 "parse.y"
Cyc_yyget_String_tok(_T1126);_T1128=_tag_fat("alias",sizeof(char),6U);_T1129=Cyc_strcmp(_T1127,_T1128);if(_T1129==0)goto _TL2ED;_T112A=yyyvsp;_T112B=_T112A[1];_T112C=_T112B.l;_T112D=_T112C.first_line;_T112E=
Cyc_Position_loc_to_seg(_T112D);_T112F=_tag_fat("expecting `alias'",sizeof(char),18U);_T1130=_tag_fat(0U,sizeof(void*),0);Cyc_Warn_err(_T112E,_T112F,_T1130);goto _TL2EE;_TL2ED: _TL2EE: _T1131=yyyvsp;_T1132=_T1131[0];_T1133=_T1132.l;_T1134=_T1133.first_line;{
unsigned location=Cyc_Position_loc_to_seg(_T1134);_T1135=yyyvsp;_T1136=& _T1135[2].v;_T1137=(union Cyc_YYSTYPE*)_T1136;_T1138=
Cyc_yyget_String_tok(_T1137);_T1139=location;Cyc_Parse_tvar_ok(_T1138,_T1139);{
struct Cyc_Absyn_Tvar*tv;tv=_cycalloc(sizeof(struct Cyc_Absyn_Tvar));_T113A=tv;{struct _fat_ptr*_T18B2=_cycalloc(sizeof(struct _fat_ptr));_T113C=yyyvsp;_T113D=& _T113C[2].v;_T113E=(union Cyc_YYSTYPE*)_T113D;*_T18B2=Cyc_yyget_String_tok(_T113E);_T113B=(struct _fat_ptr*)_T18B2;}_T113A->name=_T113B;_T113F=tv;_T113F->identity=- 1;_T1140=tv;{struct Cyc_Absyn_Eq_kb_Absyn_KindBound_struct*_T18B2=_cycalloc(sizeof(struct Cyc_Absyn_Eq_kb_Absyn_KindBound_struct));_T18B2->tag=0;_T1142=& Cyc_Kinds_ek;_T18B2->f1=(struct Cyc_Absyn_Kind*)_T1142;_T1141=(struct Cyc_Absyn_Eq_kb_Absyn_KindBound_struct*)_T18B2;}_T1140->kind=(void*)_T1141;_T1143=tv;_T1143->aquals_bound=0;_T1144=yyyvsp;_T1145=_T1144[0];_T1146=_T1145.l;_T1147=_T1146.first_line;_T1148=
Cyc_Position_loc_to_seg(_T1147);{struct _tuple0*_T18B2=_cycalloc(sizeof(struct _tuple0));_T18B2->f0=Cyc_Absyn_Loc_n();{struct _fat_ptr*_T18B3=_cycalloc(sizeof(struct _fat_ptr));_T114B=yyyvsp;_T114C=& _T114B[5].v;_T114D=(union Cyc_YYSTYPE*)_T114C;*_T18B3=Cyc_yyget_String_tok(_T114D);_T114A=(struct _fat_ptr*)_T18B3;}_T18B2->f1=_T114A;_T1149=(struct _tuple0*)_T18B2;}_T114E=yyyvsp;_T114F=& _T114E[4].v;_T1150=(union Cyc_YYSTYPE*)_T114F;_T1151=
Cyc_yyget_YY38(_T1150);_T1152=yyyvsp;_T1153=_T1152[4];_T1154=_T1153.l;_T1155=_T1154.first_line;_T1156=Cyc_Position_loc_to_seg(_T1155);_T1157=Cyc_Parse_type_name_to_type(_T1151,_T1156);{
# 2852
struct Cyc_Absyn_Vardecl*vd=Cyc_Absyn_new_vardecl(_T1148,_T1149,_T1157,0,0);{struct Cyc_Absyn_AliasVar_p_Absyn_Raw_pat_struct*_T18B2=_cycalloc(sizeof(struct Cyc_Absyn_AliasVar_p_Absyn_Raw_pat_struct));_T18B2->tag=2;
# 2854
_T18B2->f1=tv;_T18B2->f2=vd;_T1158=(struct Cyc_Absyn_AliasVar_p_Absyn_Raw_pat_struct*)_T18B2;}_T1159=(void*)_T1158;_T115A=location;_T115B=Cyc_Absyn_new_pat(_T1159,_T115A);yyval=Cyc_YY9(_T115B);goto _LL0;}}}case 430: _T115C=yyyvsp;_T115D=& _T115C[0].v;_T115E=(union Cyc_YYSTYPE*)_T115D;_T115F=
# 2857 "parse.y"
Cyc_yyget_String_tok(_T115E);_T1160=_tag_fat("alias",sizeof(char),6U);_T1161=Cyc_strcmp(_T115F,_T1160);if(_T1161==0)goto _TL2EF;_T1162=yyyvsp;_T1163=_T1162[1];_T1164=_T1163.l;_T1165=_T1164.first_line;_T1166=
Cyc_Position_loc_to_seg(_T1165);_T1167=_tag_fat("expecting `alias'",sizeof(char),18U);_T1168=_tag_fat(0U,sizeof(void*),0);Cyc_Warn_err(_T1166,_T1167,_T1168);goto _TL2F0;_TL2EF: _TL2F0: _T1169=yyyvsp;_T116A=_T1169[0];_T116B=_T116A.l;_T116C=_T116B.first_line;{
unsigned location=Cyc_Position_loc_to_seg(_T116C);_T116D=yyyvsp;_T116E=& _T116D[2].v;_T116F=(union Cyc_YYSTYPE*)_T116E;_T1170=
Cyc_yyget_String_tok(_T116F);_T1171=location;Cyc_Parse_tvar_ok(_T1170,_T1171);{
struct Cyc_Absyn_Tvar*tv;tv=_cycalloc(sizeof(struct Cyc_Absyn_Tvar));_T1172=tv;{struct _fat_ptr*_T18B2=_cycalloc(sizeof(struct _fat_ptr));_T1174=yyyvsp;_T1175=& _T1174[2].v;_T1176=(union Cyc_YYSTYPE*)_T1175;*_T18B2=Cyc_yyget_String_tok(_T1176);_T1173=(struct _fat_ptr*)_T18B2;}_T1172->name=_T1173;_T1177=tv;_T1177->identity=- 1;_T1178=tv;{struct Cyc_Absyn_Eq_kb_Absyn_KindBound_struct*_T18B2=_cycalloc(sizeof(struct Cyc_Absyn_Eq_kb_Absyn_KindBound_struct));_T18B2->tag=0;_T117A=& Cyc_Kinds_ek;_T18B2->f1=(struct Cyc_Absyn_Kind*)_T117A;_T1179=(struct Cyc_Absyn_Eq_kb_Absyn_KindBound_struct*)_T18B2;}_T1178->kind=(void*)_T1179;_T117B=tv;_T117B->aquals_bound=0;_T117C=yyyvsp;_T117D=_T117C[0];_T117E=_T117D.l;_T117F=_T117E.first_line;_T1180=
Cyc_Position_loc_to_seg(_T117F);{struct _tuple0*_T18B2=_cycalloc(sizeof(struct _tuple0));_T18B2->f0=Cyc_Absyn_Loc_n();{struct _fat_ptr*_T18B3=_cycalloc(sizeof(struct _fat_ptr));_T1183=yyyvsp;_T1184=& _T1183[5].v;_T1185=(union Cyc_YYSTYPE*)_T1184;*_T18B3=Cyc_yyget_String_tok(_T1185);_T1182=(struct _fat_ptr*)_T18B3;}_T18B2->f1=_T1182;_T1181=(struct _tuple0*)_T18B2;}_T1186=yyyvsp;_T1187=& _T1186[4].v;_T1188=(union Cyc_YYSTYPE*)_T1187;_T1189=
Cyc_yyget_YY38(_T1188);_T118A=yyyvsp;_T118B=_T118A[4];_T118C=_T118B.l;_T118D=_T118C.first_line;_T118E=Cyc_Position_loc_to_seg(_T118D);_T118F=Cyc_Parse_type_name_to_type(_T1189,_T118E);{
# 2862
struct Cyc_Absyn_Vardecl*vd=Cyc_Absyn_new_vardecl(_T1180,_T1181,_T118F,0,0);{struct Cyc_Absyn_AliasVar_p_Absyn_Raw_pat_struct*_T18B2=_cycalloc(sizeof(struct Cyc_Absyn_AliasVar_p_Absyn_Raw_pat_struct));_T18B2->tag=2;
# 2864
_T18B2->f1=tv;_T18B2->f2=vd;_T1190=(struct Cyc_Absyn_AliasVar_p_Absyn_Raw_pat_struct*)_T18B2;}_T1191=(void*)_T1190;_T1192=location;_T1193=Cyc_Absyn_new_pat(_T1191,_T1192);yyval=Cyc_YY9(_T1193);goto _LL0;}}}case 431: _T1194=yyyvsp;_T1195=& _T1194[2].v;_T1196=(union Cyc_YYSTYPE*)_T1195;_T1197=
# 2867 "parse.y"
Cyc_yyget_YY14(_T1196);{struct _tuple23 _T18B2=*_T1197;int _T18B3;struct Cyc_List_List*_T18B4;_T18B4=_T18B2.f0;_T18B3=_T18B2.f1;{struct Cyc_List_List*fps=_T18B4;int dots=_T18B3;{struct Cyc_Absyn_Aggr_p_Absyn_Raw_pat_struct*_T18B5=_cycalloc(sizeof(struct Cyc_Absyn_Aggr_p_Absyn_Raw_pat_struct));_T18B5->tag=6;
_T18B5->f1=0;_T18B5->f2=1;_T18B5->f3=0;_T18B5->f4=fps;_T18B5->f5=dots;_T1198=(struct Cyc_Absyn_Aggr_p_Absyn_Raw_pat_struct*)_T18B5;}_T1199=(void*)_T1198;_T119A=yyyvsp;_T119B=_T119A[0];_T119C=_T119B.l;_T119D=_T119C.first_line;_T119E=Cyc_Position_loc_to_seg(_T119D);_T119F=Cyc_Absyn_new_pat(_T1199,_T119E);yyval=Cyc_YY9(_T119F);goto _LL0;}}case 432: _T11A0=yyyvsp;_T11A1=& _T11A0[2].v;_T11A2=(union Cyc_YYSTYPE*)_T11A1;_T11A3=
# 2871 "parse.y"
Cyc_yyget_YY10(_T11A2);{struct _tuple23 _T18B2=*_T11A3;int _T18B3;struct Cyc_List_List*_T18B4;_T18B4=_T18B2.f0;_T18B3=_T18B2.f1;{struct Cyc_List_List*ps=_T18B4;int dots=_T18B3;{struct Cyc_Absyn_UnknownCall_p_Absyn_Raw_pat_struct*_T18B5=_cycalloc(sizeof(struct Cyc_Absyn_UnknownCall_p_Absyn_Raw_pat_struct));_T18B5->tag=15;_T11A5=yyyvsp;_T11A6=& _T11A5[0].v;_T11A7=(union Cyc_YYSTYPE*)_T11A6;
_T18B5->f1=Cyc_yyget_QualId_tok(_T11A7);_T18B5->f2=ps;_T18B5->f3=dots;_T11A4=(struct Cyc_Absyn_UnknownCall_p_Absyn_Raw_pat_struct*)_T18B5;}_T11A8=(void*)_T11A4;_T11A9=yyyvsp;_T11AA=_T11A9[0];_T11AB=_T11AA.l;_T11AC=_T11AB.first_line;_T11AD=Cyc_Position_loc_to_seg(_T11AC);_T11AE=Cyc_Absyn_new_pat(_T11A8,_T11AD);yyval=Cyc_YY9(_T11AE);goto _LL0;}}case 433: _T11AF=yyyvsp;_T11B0=& _T11AF[3].v;_T11B1=(union Cyc_YYSTYPE*)_T11B0;_T11B2=
# 2875 "parse.y"
Cyc_yyget_YY14(_T11B1);{struct _tuple23 _T18B2=*_T11B2;int _T18B3;struct Cyc_List_List*_T18B4;_T18B4=_T18B2.f0;_T18B3=_T18B2.f1;{struct Cyc_List_List*fps=_T18B4;int dots=_T18B3;_T11B4=Cyc_List_map_c;{
struct Cyc_List_List*(*_T18B5)(struct Cyc_Absyn_Tvar*(*)(unsigned,void*),unsigned,struct Cyc_List_List*)=(struct Cyc_List_List*(*)(struct Cyc_Absyn_Tvar*(*)(unsigned,void*),unsigned,struct Cyc_List_List*))_T11B4;_T11B3=_T18B5;}_T11B5=yyyvsp;_T11B6=_T11B5[2];_T11B7=_T11B6.l;_T11B8=_T11B7.first_line;_T11B9=Cyc_Position_loc_to_seg(_T11B8);_T11BA=yyyvsp;_T11BB=& _T11BA[2].v;_T11BC=(union Cyc_YYSTYPE*)_T11BB;_T11BD=Cyc_yyget_YY41(_T11BC);{struct Cyc_List_List*exist_ts=_T11B3(Cyc_Parse_typ2tvar,_T11B9,_T11BD);_T11BE=yyyvsp;_T11BF=& _T11BE[0].v;_T11C0=(union Cyc_YYSTYPE*)_T11BF;_T11C1=
Cyc_yyget_QualId_tok(_T11C0);{union Cyc_Absyn_AggrInfo ai=Cyc_Absyn_UnknownAggr(0U,_T11C1,0);
struct Cyc_Absyn_AppType_Absyn_Type_struct*typ;typ=_cycalloc(sizeof(struct Cyc_Absyn_AppType_Absyn_Type_struct));_T11C2=typ;_T11C2->tag=0;_T11C3=typ;{struct Cyc_Absyn_AggrCon_Absyn_TyCon_struct*_T18B5=_cycalloc(sizeof(struct Cyc_Absyn_AggrCon_Absyn_TyCon_struct));_T18B5->tag=24;_T18B5->f1=ai;_T11C4=(struct Cyc_Absyn_AggrCon_Absyn_TyCon_struct*)_T18B5;}_T11C3->f1=(void*)_T11C4;_T11C5=typ;_T11C5->f2=0;{struct Cyc_Absyn_Aggr_p_Absyn_Raw_pat_struct*_T18B5=_cycalloc(sizeof(struct Cyc_Absyn_Aggr_p_Absyn_Raw_pat_struct));_T18B5->tag=6;_T11C7=typ;
_T18B5->f1=(void*)_T11C7;_T18B5->f2=0;_T18B5->f3=exist_ts;_T18B5->f4=fps;_T18B5->f5=dots;_T11C6=(struct Cyc_Absyn_Aggr_p_Absyn_Raw_pat_struct*)_T18B5;}_T11C8=(void*)_T11C6;_T11C9=yyyvsp;_T11CA=_T11C9[0];_T11CB=_T11CA.l;_T11CC=_T11CB.first_line;_T11CD=Cyc_Position_loc_to_seg(_T11CC);_T11CE=Cyc_Absyn_new_pat(_T11C8,_T11CD);yyval=Cyc_YY9(_T11CE);goto _LL0;}}}}case 434: _T11CF=yyyvsp;_T11D0=& _T11CF[2].v;_T11D1=(union Cyc_YYSTYPE*)_T11D0;_T11D2=
# 2882 "parse.y"
Cyc_yyget_YY14(_T11D1);{struct _tuple23 _T18B2=*_T11D2;int _T18B3;struct Cyc_List_List*_T18B4;_T18B4=_T18B2.f0;_T18B3=_T18B2.f1;{struct Cyc_List_List*fps=_T18B4;int dots=_T18B3;_T11D4=Cyc_List_map_c;{
struct Cyc_List_List*(*_T18B5)(struct Cyc_Absyn_Tvar*(*)(unsigned,void*),unsigned,struct Cyc_List_List*)=(struct Cyc_List_List*(*)(struct Cyc_Absyn_Tvar*(*)(unsigned,void*),unsigned,struct Cyc_List_List*))_T11D4;_T11D3=_T18B5;}_T11D5=yyyvsp;_T11D6=_T11D5[1];_T11D7=_T11D6.l;_T11D8=_T11D7.first_line;_T11D9=Cyc_Position_loc_to_seg(_T11D8);_T11DA=yyyvsp;_T11DB=& _T11DA[1].v;_T11DC=(union Cyc_YYSTYPE*)_T11DB;_T11DD=Cyc_yyget_YY41(_T11DC);{struct Cyc_List_List*exist_ts=_T11D3(Cyc_Parse_typ2tvar,_T11D9,_T11DD);{struct Cyc_Absyn_Aggr_p_Absyn_Raw_pat_struct*_T18B5=_cycalloc(sizeof(struct Cyc_Absyn_Aggr_p_Absyn_Raw_pat_struct));_T18B5->tag=6;
_T18B5->f1=0;_T18B5->f2=0;_T18B5->f3=exist_ts;_T18B5->f4=fps;_T18B5->f5=dots;_T11DE=(struct Cyc_Absyn_Aggr_p_Absyn_Raw_pat_struct*)_T18B5;}_T11DF=(void*)_T11DE;_T11E0=yyyvsp;_T11E1=_T11E0[0];_T11E2=_T11E1.l;_T11E3=_T11E2.first_line;_T11E4=Cyc_Position_loc_to_seg(_T11E3);_T11E5=Cyc_Absyn_new_pat(_T11DF,_T11E4);yyval=Cyc_YY9(_T11E5);goto _LL0;}}}case 435:{struct Cyc_Absyn_Pointer_p_Absyn_Raw_pat_struct*_T18B2=_cycalloc(sizeof(struct Cyc_Absyn_Pointer_p_Absyn_Raw_pat_struct));_T18B2->tag=5;{struct Cyc_Absyn_Pointer_p_Absyn_Raw_pat_struct*_T18B3=_cycalloc(sizeof(struct Cyc_Absyn_Pointer_p_Absyn_Raw_pat_struct));_T18B3->tag=5;_T11E8=yyyvsp;_T11E9=& _T11E8[1].v;_T11EA=(union Cyc_YYSTYPE*)_T11E9;
# 2887 "parse.y"
_T18B3->f1=Cyc_yyget_YY9(_T11EA);_T11E7=(struct Cyc_Absyn_Pointer_p_Absyn_Raw_pat_struct*)_T18B3;}_T11EB=(void*)_T11E7;_T11EC=yyyvsp;_T11ED=_T11EC[0];_T11EE=_T11ED.l;_T11EF=_T11EE.first_line;_T11F0=Cyc_Position_loc_to_seg(_T11EF);_T18B2->f1=Cyc_Absyn_new_pat(_T11EB,_T11F0);_T11E6=(struct Cyc_Absyn_Pointer_p_Absyn_Raw_pat_struct*)_T18B2;}_T11F1=(void*)_T11E6;_T11F2=yyyvsp;_T11F3=_T11F2[0];_T11F4=_T11F3.l;_T11F5=_T11F4.first_line;_T11F6=Cyc_Position_loc_to_seg(_T11F5);_T11F7=Cyc_Absyn_new_pat(_T11F1,_T11F6);yyval=Cyc_YY9(_T11F7);goto _LL0;case 436:{struct Cyc_Absyn_Reference_p_Absyn_Raw_pat_struct*_T18B2=_cycalloc(sizeof(struct Cyc_Absyn_Reference_p_Absyn_Raw_pat_struct));_T18B2->tag=3;_T11F9=yyyvsp;_T11FA=_T11F9[0];_T11FB=_T11FA.l;_T11FC=_T11FB.first_line;_T11FD=
# 2889 "parse.y"
Cyc_Position_loc_to_seg(_T11FC);{struct _tuple0*_T18B3=_cycalloc(sizeof(struct _tuple0));_T18B3->f0=Cyc_Absyn_Loc_n();{struct _fat_ptr*_T18B4=_cycalloc(sizeof(struct _fat_ptr));_T1200=yyyvsp;_T1201=& _T1200[1].v;_T1202=(union Cyc_YYSTYPE*)_T1201;*_T18B4=Cyc_yyget_String_tok(_T1202);_T11FF=(struct _fat_ptr*)_T18B4;}_T18B3->f1=_T11FF;_T11FE=(struct _tuple0*)_T18B3;}_T1203=Cyc_Absyn_void_type;_T18B2->f1=Cyc_Absyn_new_vardecl(_T11FD,_T11FE,_T1203,0,0);_T1204=& Cyc_Absyn_Wild_p_val;_T1205=(struct Cyc_Absyn_Wild_p_Absyn_Raw_pat_struct*)_T1204;_T1206=(void*)_T1205;_T1207=yyyvsp;_T1208=_T1207[1];_T1209=_T1208.l;_T120A=_T1209.first_line;_T120B=
# 2891
Cyc_Position_loc_to_seg(_T120A);_T18B2->f2=Cyc_Absyn_new_pat(_T1206,_T120B);_T11F8=(struct Cyc_Absyn_Reference_p_Absyn_Raw_pat_struct*)_T18B2;}_T120C=(void*)_T11F8;_T120D=yyyvsp;_T120E=_T120D[0];_T120F=_T120E.l;_T1210=_T120F.first_line;_T1211=
Cyc_Position_loc_to_seg(_T1210);_T1212=
# 2889
Cyc_Absyn_new_pat(_T120C,_T1211);yyval=Cyc_YY9(_T1212);goto _LL0;case 437: _T1213=yyyvsp;_T1214=& _T1213[2].v;_T1215=(union Cyc_YYSTYPE*)_T1214;_T1216=
# 2894 "parse.y"
Cyc_yyget_String_tok(_T1215);_T1217=_tag_fat("as",sizeof(char),3U);_T1218=Cyc_strcmp(_T1216,_T1217);if(_T1218==0)goto _TL2F1;_T1219=yyyvsp;_T121A=_T1219[2];_T121B=_T121A.l;_T121C=_T121B.first_line;_T121D=
Cyc_Position_loc_to_seg(_T121C);_T121E=_tag_fat("expecting `as'",sizeof(char),15U);_T121F=_tag_fat(0U,sizeof(void*),0);Cyc_Warn_err(_T121D,_T121E,_T121F);goto _TL2F2;_TL2F1: _TL2F2:{struct Cyc_Absyn_Reference_p_Absyn_Raw_pat_struct*_T18B2=_cycalloc(sizeof(struct Cyc_Absyn_Reference_p_Absyn_Raw_pat_struct));_T18B2->tag=3;_T1221=yyyvsp;_T1222=_T1221[0];_T1223=_T1222.l;_T1224=_T1223.first_line;_T1225=
Cyc_Position_loc_to_seg(_T1224);{struct _tuple0*_T18B3=_cycalloc(sizeof(struct _tuple0));_T18B3->f0=Cyc_Absyn_Loc_n();{struct _fat_ptr*_T18B4=_cycalloc(sizeof(struct _fat_ptr));_T1228=yyyvsp;_T1229=& _T1228[1].v;_T122A=(union Cyc_YYSTYPE*)_T1229;*_T18B4=Cyc_yyget_String_tok(_T122A);_T1227=(struct _fat_ptr*)_T18B4;}_T18B3->f1=_T1227;_T1226=(struct _tuple0*)_T18B3;}_T122B=Cyc_Absyn_void_type;_T18B2->f1=Cyc_Absyn_new_vardecl(_T1225,_T1226,_T122B,0,0);_T122C=yyyvsp;_T122D=& _T122C[3].v;_T122E=(union Cyc_YYSTYPE*)_T122D;
# 2898
_T18B2->f2=Cyc_yyget_YY9(_T122E);_T1220=(struct Cyc_Absyn_Reference_p_Absyn_Raw_pat_struct*)_T18B2;}_T122F=(void*)_T1220;_T1230=yyyvsp;_T1231=_T1230[0];_T1232=_T1231.l;_T1233=_T1232.first_line;_T1234=Cyc_Position_loc_to_seg(_T1233);_T1235=
# 2896
Cyc_Absyn_new_pat(_T122F,_T1234);yyval=Cyc_YY9(_T1235);goto _LL0;case 438: _T1236=yyyvsp;_T1237=& _T1236[2].v;_T1238=(union Cyc_YYSTYPE*)_T1237;_T1239=
# 2901 "parse.y"
Cyc_yyget_String_tok(_T1238);_T123A=& Cyc_Kinds_ik;_T123B=(struct Cyc_Absyn_Kind*)_T123A;_T123C=Cyc_Kinds_kind_to_bound(_T123B);_T123D=yyyvsp;_T123E=_T123D[2];_T123F=_T123E.l;_T1240=_T123F.first_line;_T1241=Cyc_Position_loc_to_seg(_T1240);{void*tag=Cyc_Parse_id2type(_T1239,_T123C,0,_T1241);{struct Cyc_Absyn_TagInt_p_Absyn_Raw_pat_struct*_T18B2=_cycalloc(sizeof(struct Cyc_Absyn_TagInt_p_Absyn_Raw_pat_struct));_T18B2->tag=4;_T1243=yyyvsp;_T1244=_T1243[2];_T1245=_T1244.l;_T1246=_T1245.first_line;_T1247=
Cyc_Position_loc_to_seg(_T1246);_T1248=tag;_T18B2->f1=Cyc_Parse_typ2tvar(_T1247,_T1248);_T1249=yyyvsp;_T124A=_T1249[0];_T124B=_T124A.l;_T124C=_T124B.first_line;_T124D=
Cyc_Position_loc_to_seg(_T124C);{struct _tuple0*_T18B3=_cycalloc(sizeof(struct _tuple0));_T18B3->f0=Cyc_Absyn_Loc_n();{struct _fat_ptr*_T18B4=_cycalloc(sizeof(struct _fat_ptr));_T1250=yyyvsp;_T1251=& _T1250[0].v;_T1252=(union Cyc_YYSTYPE*)_T1251;*_T18B4=Cyc_yyget_String_tok(_T1252);_T124F=(struct _fat_ptr*)_T18B4;}_T18B3->f1=_T124F;_T124E=(struct _tuple0*)_T18B3;}_T1253=
Cyc_Absyn_tag_type(tag);
# 2903
_T18B2->f2=Cyc_Absyn_new_vardecl(_T124D,_T124E,_T1253,0,0);_T1242=(struct Cyc_Absyn_TagInt_p_Absyn_Raw_pat_struct*)_T18B2;}_T1254=(void*)_T1242;_T1255=yyyvsp;_T1256=_T1255[0];_T1257=_T1256.l;_T1258=_T1257.first_line;_T1259=
# 2905
Cyc_Position_loc_to_seg(_T1258);_T125A=
# 2902
Cyc_Absyn_new_pat(_T1254,_T1259);yyval=Cyc_YY9(_T125A);goto _LL0;}case 439: _T125B=& Cyc_Kinds_ik;_T125C=(struct Cyc_Absyn_Kind*)_T125B;_T125D=
# 2907 "parse.y"
Cyc_Kinds_kind_to_bound(_T125C);{struct Cyc_Absyn_Tvar*tv=Cyc_Tcutil_new_tvar(_T125D);{struct Cyc_Absyn_TagInt_p_Absyn_Raw_pat_struct*_T18B2=_cycalloc(sizeof(struct Cyc_Absyn_TagInt_p_Absyn_Raw_pat_struct));_T18B2->tag=4;
_T18B2->f1=tv;_T125F=yyyvsp;_T1260=_T125F[0];_T1261=_T1260.l;_T1262=_T1261.first_line;_T1263=
Cyc_Position_loc_to_seg(_T1262);{struct _tuple0*_T18B3=_cycalloc(sizeof(struct _tuple0));_T18B3->f0=Cyc_Absyn_Loc_n();{struct _fat_ptr*_T18B4=_cycalloc(sizeof(struct _fat_ptr));_T1266=yyyvsp;_T1267=& _T1266[0].v;_T1268=(union Cyc_YYSTYPE*)_T1267;*_T18B4=Cyc_yyget_String_tok(_T1268);_T1265=(struct _fat_ptr*)_T18B4;}_T18B3->f1=_T1265;_T1264=(struct _tuple0*)_T18B3;}_T1269=
Cyc_Absyn_var_type(tv);_T126A=Cyc_Absyn_tag_type(_T1269);
# 2909
_T18B2->f2=Cyc_Absyn_new_vardecl(_T1263,_T1264,_T126A,0,0);_T125E=(struct Cyc_Absyn_TagInt_p_Absyn_Raw_pat_struct*)_T18B2;}_T126B=(void*)_T125E;_T126C=yyyvsp;_T126D=_T126C[0];_T126E=_T126D.l;_T126F=_T126E.first_line;_T1270=
# 2911
Cyc_Position_loc_to_seg(_T126F);_T1271=
# 2908
Cyc_Absyn_new_pat(_T126B,_T1270);yyval=Cyc_YY9(_T1271);goto _LL0;}case 440:{struct _tuple23*_T18B2=_cycalloc(sizeof(struct _tuple23));_T1273=yyyvsp;_T1274=& _T1273[0].v;_T1275=(union Cyc_YYSTYPE*)_T1274;_T1276=
# 2915 "parse.y"
Cyc_yyget_YY11(_T1275);_T18B2->f0=Cyc_List_rev(_T1276);_T18B2->f1=0;_T1272=(struct _tuple23*)_T18B2;}yyval=Cyc_YY10(_T1272);goto _LL0;case 441:{struct _tuple23*_T18B2=_cycalloc(sizeof(struct _tuple23));_T1278=yyyvsp;_T1279=& _T1278[0].v;_T127A=(union Cyc_YYSTYPE*)_T1279;_T127B=
# 2916 "parse.y"
Cyc_yyget_YY11(_T127A);_T18B2->f0=Cyc_List_rev(_T127B);_T18B2->f1=1;_T1277=(struct _tuple23*)_T18B2;}yyval=Cyc_YY10(_T1277);goto _LL0;case 442:{struct _tuple23*_T18B2=_cycalloc(sizeof(struct _tuple23));
# 2917 "parse.y"
_T18B2->f0=0;_T18B2->f1=1;_T127C=(struct _tuple23*)_T18B2;}yyval=Cyc_YY10(_T127C);goto _LL0;case 443:{struct Cyc_List_List*_T18B2=_cycalloc(sizeof(struct Cyc_List_List));_T127E=yyyvsp;_T127F=& _T127E[0].v;_T1280=(union Cyc_YYSTYPE*)_T127F;
# 2920
_T18B2->hd=Cyc_yyget_YY9(_T1280);_T18B2->tl=0;_T127D=(struct Cyc_List_List*)_T18B2;}yyval=Cyc_YY11(_T127D);goto _LL0;case 444:{struct Cyc_List_List*_T18B2=_cycalloc(sizeof(struct Cyc_List_List));_T1282=yyyvsp;_T1283=& _T1282[2].v;_T1284=(union Cyc_YYSTYPE*)_T1283;
# 2921 "parse.y"
_T18B2->hd=Cyc_yyget_YY9(_T1284);_T1285=yyyvsp;_T1286=& _T1285[0].v;_T1287=(union Cyc_YYSTYPE*)_T1286;_T18B2->tl=Cyc_yyget_YY11(_T1287);_T1281=(struct Cyc_List_List*)_T18B2;}yyval=Cyc_YY11(_T1281);goto _LL0;case 445:{struct _tuple24*_T18B2=_cycalloc(sizeof(struct _tuple24));
# 2924
_T18B2->f0=0;_T1289=yyyvsp;_T128A=& _T1289[0].v;_T128B=(union Cyc_YYSTYPE*)_T128A;_T18B2->f1=Cyc_yyget_YY9(_T128B);_T1288=(struct _tuple24*)_T18B2;}yyval=Cyc_YY12(_T1288);goto _LL0;case 446:{struct _tuple24*_T18B2=_cycalloc(sizeof(struct _tuple24));_T128D=yyyvsp;_T128E=& _T128D[0].v;_T128F=(union Cyc_YYSTYPE*)_T128E;
# 2925 "parse.y"
_T18B2->f0=Cyc_yyget_YY42(_T128F);_T1290=yyyvsp;_T1291=& _T1290[1].v;_T1292=(union Cyc_YYSTYPE*)_T1291;_T18B2->f1=Cyc_yyget_YY9(_T1292);_T128C=(struct _tuple24*)_T18B2;}yyval=Cyc_YY12(_T128C);goto _LL0;case 447:{struct _tuple23*_T18B2=_cycalloc(sizeof(struct _tuple23));_T1294=yyyvsp;_T1295=& _T1294[0].v;_T1296=(union Cyc_YYSTYPE*)_T1295;_T1297=
# 2928
Cyc_yyget_YY13(_T1296);_T18B2->f0=Cyc_List_rev(_T1297);_T18B2->f1=0;_T1293=(struct _tuple23*)_T18B2;}yyval=Cyc_YY14(_T1293);goto _LL0;case 448:{struct _tuple23*_T18B2=_cycalloc(sizeof(struct _tuple23));_T1299=yyyvsp;_T129A=& _T1299[0].v;_T129B=(union Cyc_YYSTYPE*)_T129A;_T129C=
# 2929 "parse.y"
Cyc_yyget_YY13(_T129B);_T18B2->f0=Cyc_List_rev(_T129C);_T18B2->f1=1;_T1298=(struct _tuple23*)_T18B2;}yyval=Cyc_YY14(_T1298);goto _LL0;case 449:{struct _tuple23*_T18B2=_cycalloc(sizeof(struct _tuple23));
# 2930 "parse.y"
_T18B2->f0=0;_T18B2->f1=1;_T129D=(struct _tuple23*)_T18B2;}yyval=Cyc_YY14(_T129D);goto _LL0;case 450:{struct Cyc_List_List*_T18B2=_cycalloc(sizeof(struct Cyc_List_List));_T129F=yyyvsp;_T12A0=& _T129F[0].v;_T12A1=(union Cyc_YYSTYPE*)_T12A0;
# 2933
_T18B2->hd=Cyc_yyget_YY12(_T12A1);_T18B2->tl=0;_T129E=(struct Cyc_List_List*)_T18B2;}yyval=Cyc_YY13(_T129E);goto _LL0;case 451:{struct Cyc_List_List*_T18B2=_cycalloc(sizeof(struct Cyc_List_List));_T12A3=yyyvsp;_T12A4=& _T12A3[2].v;_T12A5=(union Cyc_YYSTYPE*)_T12A4;
# 2934 "parse.y"
_T18B2->hd=Cyc_yyget_YY12(_T12A5);_T12A6=yyyvsp;_T12A7=& _T12A6[0].v;_T12A8=(union Cyc_YYSTYPE*)_T12A7;_T18B2->tl=Cyc_yyget_YY13(_T12A8);_T12A2=(struct Cyc_List_List*)_T18B2;}yyval=Cyc_YY13(_T12A2);goto _LL0;case 452: _T12A9=yyyvsp;_T12AA=_T12A9[0];
# 2939 "parse.y"
yyval=_T12AA.v;goto _LL0;case 453: _T12AB=yyyvsp;_T12AC=& _T12AB[0].v;_T12AD=(union Cyc_YYSTYPE*)_T12AC;_T12AE=
# 2940 "parse.y"
Cyc_yyget_Exp_tok(_T12AD);_T12AF=yyyvsp;_T12B0=& _T12AF[2].v;_T12B1=(union Cyc_YYSTYPE*)_T12B0;_T12B2=Cyc_yyget_Exp_tok(_T12B1);_T12B3=yyyvsp;_T12B4=_T12B3[0];_T12B5=_T12B4.l;_T12B6=_T12B5.first_line;_T12B7=Cyc_Position_loc_to_seg(_T12B6);_T12B8=Cyc_Absyn_seq_exp(_T12AE,_T12B2,_T12B7);yyval=Cyc_Exp_tok(_T12B8);goto _LL0;case 454: _T12B9=yyyvsp;_T12BA=_T12B9[0];
# 2944 "parse.y"
yyval=_T12BA.v;goto _LL0;case 455: _T12BB=yyyvsp;_T12BC=& _T12BB[0].v;_T12BD=(union Cyc_YYSTYPE*)_T12BC;_T12BE=
# 2946 "parse.y"
Cyc_yyget_Exp_tok(_T12BD);_T12BF=yyyvsp;_T12C0=& _T12BF[1].v;_T12C1=(union Cyc_YYSTYPE*)_T12C0;_T12C2=Cyc_yyget_YY7(_T12C1);_T12C3=yyyvsp;_T12C4=& _T12C3[2].v;_T12C5=(union Cyc_YYSTYPE*)_T12C4;_T12C6=Cyc_yyget_Exp_tok(_T12C5);_T12C7=yyyvsp;_T12C8=_T12C7[0];_T12C9=_T12C8.l;_T12CA=_T12C9.first_line;_T12CB=Cyc_Position_loc_to_seg(_T12CA);_T12CC=Cyc_Absyn_assignop_exp(_T12BE,_T12C2,_T12C6,_T12CB);yyval=Cyc_Exp_tok(_T12CC);goto _LL0;case 456: _T12CD=yyyvsp;_T12CE=& _T12CD[0].v;_T12CF=(union Cyc_YYSTYPE*)_T12CE;_T12D0=
# 2948 "parse.y"
Cyc_yyget_Exp_tok(_T12CF);_T12D1=yyyvsp;_T12D2=& _T12D1[2].v;_T12D3=(union Cyc_YYSTYPE*)_T12D2;_T12D4=Cyc_yyget_Exp_tok(_T12D3);_T12D5=yyyvsp;_T12D6=_T12D5[0];_T12D7=_T12D6.l;_T12D8=_T12D7.first_line;_T12D9=Cyc_Position_loc_to_seg(_T12D8);_T12DA=Cyc_Absyn_swap_exp(_T12D0,_T12D4,_T12D9);yyval=Cyc_Exp_tok(_T12DA);goto _LL0;case 457:
# 2952 "parse.y"
 yyval=Cyc_YY7(0);goto _LL0;case 458:{struct Cyc_Core_Opt*_T18B2=_cycalloc(sizeof(struct Cyc_Core_Opt));
# 2953 "parse.y"
_T18B2->v=(void*)1U;_T12DB=(struct Cyc_Core_Opt*)_T18B2;}yyval=Cyc_YY7(_T12DB);goto _LL0;case 459:{struct Cyc_Core_Opt*_T18B2=_cycalloc(sizeof(struct Cyc_Core_Opt));
# 2954 "parse.y"
_T18B2->v=(void*)3U;_T12DC=(struct Cyc_Core_Opt*)_T18B2;}yyval=Cyc_YY7(_T12DC);goto _LL0;case 460:{struct Cyc_Core_Opt*_T18B2=_cycalloc(sizeof(struct Cyc_Core_Opt));
# 2955 "parse.y"
_T18B2->v=(void*)4U;_T12DD=(struct Cyc_Core_Opt*)_T18B2;}yyval=Cyc_YY7(_T12DD);goto _LL0;case 461:{struct Cyc_Core_Opt*_T18B2=_cycalloc(sizeof(struct Cyc_Core_Opt));
# 2956 "parse.y"
_T18B2->v=(void*)0U;_T12DE=(struct Cyc_Core_Opt*)_T18B2;}yyval=Cyc_YY7(_T12DE);goto _LL0;case 462:{struct Cyc_Core_Opt*_T18B2=_cycalloc(sizeof(struct Cyc_Core_Opt));
# 2957 "parse.y"
_T18B2->v=(void*)2U;_T12DF=(struct Cyc_Core_Opt*)_T18B2;}yyval=Cyc_YY7(_T12DF);goto _LL0;case 463:{struct Cyc_Core_Opt*_T18B2=_cycalloc(sizeof(struct Cyc_Core_Opt));
# 2958 "parse.y"
_T18B2->v=(void*)16U;_T12E0=(struct Cyc_Core_Opt*)_T18B2;}yyval=Cyc_YY7(_T12E0);goto _LL0;case 464:{struct Cyc_Core_Opt*_T18B2=_cycalloc(sizeof(struct Cyc_Core_Opt));
# 2959 "parse.y"
_T18B2->v=(void*)17U;_T12E1=(struct Cyc_Core_Opt*)_T18B2;}yyval=Cyc_YY7(_T12E1);goto _LL0;case 465:{struct Cyc_Core_Opt*_T18B2=_cycalloc(sizeof(struct Cyc_Core_Opt));
# 2960 "parse.y"
_T18B2->v=(void*)13U;_T12E2=(struct Cyc_Core_Opt*)_T18B2;}yyval=Cyc_YY7(_T12E2);goto _LL0;case 466:{struct Cyc_Core_Opt*_T18B2=_cycalloc(sizeof(struct Cyc_Core_Opt));
# 2961 "parse.y"
_T18B2->v=(void*)15U;_T12E3=(struct Cyc_Core_Opt*)_T18B2;}yyval=Cyc_YY7(_T12E3);goto _LL0;case 467:{struct Cyc_Core_Opt*_T18B2=_cycalloc(sizeof(struct Cyc_Core_Opt));
# 2962 "parse.y"
_T18B2->v=(void*)14U;_T12E4=(struct Cyc_Core_Opt*)_T18B2;}yyval=Cyc_YY7(_T12E4);goto _LL0;case 468: _T12E5=yyyvsp;_T12E6=_T12E5[0];
# 2966 "parse.y"
yyval=_T12E6.v;goto _LL0;case 469: _T12E7=yyyvsp;_T12E8=& _T12E7[0].v;_T12E9=(union Cyc_YYSTYPE*)_T12E8;_T12EA=
# 2968 "parse.y"
Cyc_yyget_Exp_tok(_T12E9);_T12EB=yyyvsp;_T12EC=& _T12EB[2].v;_T12ED=(union Cyc_YYSTYPE*)_T12EC;_T12EE=Cyc_yyget_Exp_tok(_T12ED);_T12EF=yyyvsp;_T12F0=& _T12EF[4].v;_T12F1=(union Cyc_YYSTYPE*)_T12F0;_T12F2=Cyc_yyget_Exp_tok(_T12F1);_T12F3=yyyvsp;_T12F4=_T12F3[0];_T12F5=_T12F4.l;_T12F6=_T12F5.first_line;_T12F7=Cyc_Position_loc_to_seg(_T12F6);_T12F8=Cyc_Absyn_conditional_exp(_T12EA,_T12EE,_T12F2,_T12F7);yyval=Cyc_Exp_tok(_T12F8);goto _LL0;case 470: _T12F9=yyyvsp;_T12FA=& _T12F9[1].v;_T12FB=(union Cyc_YYSTYPE*)_T12FA;_T12FC=
# 2970 "parse.y"
Cyc_yyget_Exp_tok(_T12FB);_T12FD=yyyvsp;_T12FE=_T12FD[0];_T12FF=_T12FE.l;_T1300=_T12FF.first_line;_T1301=Cyc_Position_loc_to_seg(_T1300);_T1302=Cyc_Absyn_throw_exp(_T12FC,_T1301);yyval=Cyc_Exp_tok(_T1302);goto _LL0;case 471: _T1303=yyyvsp;_T1304=& _T1303[1].v;_T1305=(union Cyc_YYSTYPE*)_T1304;_T1306=
# 2972 "parse.y"
Cyc_yyget_Exp_tok(_T1305);_T1307=yyyvsp;_T1308=_T1307[0];_T1309=_T1308.l;_T130A=_T1309.first_line;_T130B=Cyc_Position_loc_to_seg(_T130A);_T130C=Cyc_Absyn_New_exp(0,_T1306,0,_T130B);yyval=Cyc_Exp_tok(_T130C);goto _LL0;case 472: _T130D=yyyvsp;_T130E=& _T130D[1].v;_T130F=(union Cyc_YYSTYPE*)_T130E;_T1310=
# 2973 "parse.y"
Cyc_yyget_Exp_tok(_T130F);_T1311=yyyvsp;_T1312=_T1311[0];_T1313=_T1312.l;_T1314=_T1313.first_line;_T1315=Cyc_Position_loc_to_seg(_T1314);_T1316=Cyc_Absyn_New_exp(0,_T1310,0,_T1315);yyval=Cyc_Exp_tok(_T1316);goto _LL0;case 473: _T1317=yyyvsp;_T1318=& _T1317[4].v;_T1319=(union Cyc_YYSTYPE*)_T1318;_T131A=
# 2974 "parse.y"
Cyc_yyget_Exp_tok(_T1319);_T131B=yyyvsp;_T131C=& _T131B[2].v;_T131D=(union Cyc_YYSTYPE*)_T131C;_T131E=Cyc_yyget_Exp_tok(_T131D);_T131F=yyyvsp;_T1320=_T131F[0];_T1321=_T1320.l;_T1322=_T1321.first_line;_T1323=Cyc_Position_loc_to_seg(_T1322);_T1324=Cyc_Absyn_New_exp(0,_T131A,_T131E,_T1323);yyval=Cyc_Exp_tok(_T1324);goto _LL0;case 474: _T1325=yyyvsp;_T1326=& _T1325[4].v;_T1327=(union Cyc_YYSTYPE*)_T1326;_T1328=
# 2975 "parse.y"
Cyc_yyget_Exp_tok(_T1327);_T1329=yyyvsp;_T132A=& _T1329[2].v;_T132B=(union Cyc_YYSTYPE*)_T132A;_T132C=Cyc_yyget_Exp_tok(_T132B);_T132D=yyyvsp;_T132E=_T132D[0];_T132F=_T132E.l;_T1330=_T132F.first_line;_T1331=Cyc_Position_loc_to_seg(_T1330);_T1332=Cyc_Absyn_New_exp(0,_T1328,_T132C,_T1331);yyval=Cyc_Exp_tok(_T1332);goto _LL0;case 475: _T1333=yyyvsp;_T1334=& _T1333[2].v;_T1335=(union Cyc_YYSTYPE*)_T1334;_T1336=
# 2976 "parse.y"
Cyc_yyget_Exp_tok(_T1335);{struct _tuple16 _T18B2=Cyc_Parse_split_seq(_T1336);struct Cyc_Absyn_Exp*_T18B3;struct Cyc_Absyn_Exp*_T18B4;_T18B4=_T18B2.f0;_T18B3=_T18B2.f1;{struct Cyc_Absyn_Exp*rgn=_T18B4;struct Cyc_Absyn_Exp*qual=_T18B3;_T1337=rgn;_T1338=yyyvsp;_T1339=& _T1338[4].v;_T133A=(union Cyc_YYSTYPE*)_T1339;_T133B=Cyc_yyget_Exp_tok(_T133A);_T133C=qual;_T133D=yyyvsp;_T133E=_T133D[0];_T133F=_T133E.l;_T1340=_T133F.first_line;_T1341=Cyc_Position_loc_to_seg(_T1340);_T1342=Cyc_Absyn_New_exp(_T1337,_T133B,_T133C,_T1341);yyval=Cyc_Exp_tok(_T1342);goto _LL0;}}case 476: _T1343=yyyvsp;_T1344=& _T1343[2].v;_T1345=(union Cyc_YYSTYPE*)_T1344;_T1346=
# 2977 "parse.y"
Cyc_yyget_Exp_tok(_T1345);{struct _tuple16 _T18B2=Cyc_Parse_split_seq(_T1346);struct Cyc_Absyn_Exp*_T18B3;struct Cyc_Absyn_Exp*_T18B4;_T18B4=_T18B2.f0;_T18B3=_T18B2.f1;{struct Cyc_Absyn_Exp*rgn=_T18B4;struct Cyc_Absyn_Exp*qual=_T18B3;_T1347=rgn;_T1348=yyyvsp;_T1349=& _T1348[4].v;_T134A=(union Cyc_YYSTYPE*)_T1349;_T134B=Cyc_yyget_Exp_tok(_T134A);_T134C=qual;_T134D=yyyvsp;_T134E=_T134D[0];_T134F=_T134E.l;_T1350=_T134F.first_line;_T1351=Cyc_Position_loc_to_seg(_T1350);_T1352=Cyc_Absyn_New_exp(_T1347,_T134B,_T134C,_T1351);yyval=Cyc_Exp_tok(_T1352);goto _LL0;}}case 477: _T1353=yyyvsp;_T1354=_T1353[0];
# 2989 "parse.y"
yyval=_T1354.v;goto _LL0;case 478: _T1355=yyyvsp;_T1356=_T1355[0];
# 2992
yyval=_T1356.v;goto _LL0;case 479: _T1357=yyyvsp;_T1358=& _T1357[0].v;_T1359=(union Cyc_YYSTYPE*)_T1358;_T135A=
# 2994 "parse.y"
Cyc_yyget_Exp_tok(_T1359);_T135B=yyyvsp;_T135C=& _T135B[2].v;_T135D=(union Cyc_YYSTYPE*)_T135C;_T135E=Cyc_yyget_Exp_tok(_T135D);_T135F=yyyvsp;_T1360=_T135F[0];_T1361=_T1360.l;_T1362=_T1361.first_line;_T1363=Cyc_Position_loc_to_seg(_T1362);_T1364=Cyc_Absyn_or_exp(_T135A,_T135E,_T1363);yyval=Cyc_Exp_tok(_T1364);goto _LL0;case 480: _T1365=yyyvsp;_T1366=_T1365[0];
# 2997
yyval=_T1366.v;goto _LL0;case 481: _T1367=yyyvsp;_T1368=& _T1367[0].v;_T1369=(union Cyc_YYSTYPE*)_T1368;_T136A=
# 2999 "parse.y"
Cyc_yyget_Exp_tok(_T1369);_T136B=yyyvsp;_T136C=& _T136B[2].v;_T136D=(union Cyc_YYSTYPE*)_T136C;_T136E=Cyc_yyget_Exp_tok(_T136D);_T136F=yyyvsp;_T1370=_T136F[0];_T1371=_T1370.l;_T1372=_T1371.first_line;_T1373=Cyc_Position_loc_to_seg(_T1372);_T1374=Cyc_Absyn_and_exp(_T136A,_T136E,_T1373);yyval=Cyc_Exp_tok(_T1374);goto _LL0;case 482: _T1375=yyyvsp;_T1376=_T1375[0];
# 3002
yyval=_T1376.v;goto _LL0;case 483: _T1377=yyyvsp;_T1378=& _T1377[0].v;_T1379=(union Cyc_YYSTYPE*)_T1378;_T137A=
# 3004 "parse.y"
Cyc_yyget_Exp_tok(_T1379);_T137B=yyyvsp;_T137C=& _T137B[2].v;_T137D=(union Cyc_YYSTYPE*)_T137C;_T137E=Cyc_yyget_Exp_tok(_T137D);_T137F=yyyvsp;_T1380=_T137F[0];_T1381=_T1380.l;_T1382=_T1381.first_line;_T1383=Cyc_Position_loc_to_seg(_T1382);_T1384=Cyc_Absyn_prim2_exp(14U,_T137A,_T137E,_T1383);yyval=Cyc_Exp_tok(_T1384);goto _LL0;case 484: _T1385=yyyvsp;_T1386=_T1385[0];
# 3007
yyval=_T1386.v;goto _LL0;case 485: _T1387=yyyvsp;_T1388=& _T1387[0].v;_T1389=(union Cyc_YYSTYPE*)_T1388;_T138A=
# 3009 "parse.y"
Cyc_yyget_Exp_tok(_T1389);_T138B=yyyvsp;_T138C=& _T138B[2].v;_T138D=(union Cyc_YYSTYPE*)_T138C;_T138E=Cyc_yyget_Exp_tok(_T138D);_T138F=yyyvsp;_T1390=_T138F[0];_T1391=_T1390.l;_T1392=_T1391.first_line;_T1393=Cyc_Position_loc_to_seg(_T1392);_T1394=Cyc_Absyn_prim2_exp(15U,_T138A,_T138E,_T1393);yyval=Cyc_Exp_tok(_T1394);goto _LL0;case 486: _T1395=yyyvsp;_T1396=_T1395[0];
# 3012
yyval=_T1396.v;goto _LL0;case 487: _T1397=yyyvsp;_T1398=& _T1397[0].v;_T1399=(union Cyc_YYSTYPE*)_T1398;_T139A=
# 3014 "parse.y"
Cyc_yyget_Exp_tok(_T1399);_T139B=yyyvsp;_T139C=& _T139B[2].v;_T139D=(union Cyc_YYSTYPE*)_T139C;_T139E=Cyc_yyget_Exp_tok(_T139D);_T139F=yyyvsp;_T13A0=_T139F[0];_T13A1=_T13A0.l;_T13A2=_T13A1.first_line;_T13A3=Cyc_Position_loc_to_seg(_T13A2);_T13A4=Cyc_Absyn_prim2_exp(13U,_T139A,_T139E,_T13A3);yyval=Cyc_Exp_tok(_T13A4);goto _LL0;case 488: _T13A5=yyyvsp;_T13A6=_T13A5[0];
# 3017
yyval=_T13A6.v;goto _LL0;case 489: _T13A7=yyyvsp;_T13A8=& _T13A7[1].v;_T13A9=(union Cyc_YYSTYPE*)_T13A8;_T13AA=
# 3019 "parse.y"
Cyc_yyget_YY69(_T13A9);_T13AB=yyyvsp;_T13AC=& _T13AB[0].v;_T13AD=(union Cyc_YYSTYPE*)_T13AC;_T13AE=Cyc_yyget_Exp_tok(_T13AD);_T13AF=yyyvsp;_T13B0=& _T13AF[2].v;_T13B1=(union Cyc_YYSTYPE*)_T13B0;_T13B2=Cyc_yyget_Exp_tok(_T13B1);_T13B3=yyyvsp;_T13B4=_T13B3[0];_T13B5=_T13B4.l;_T13B6=_T13B5.first_line;_T13B7=Cyc_Position_loc_to_seg(_T13B6);_T13B8=_T13AA(_T13AE,_T13B2,_T13B7);yyval=Cyc_Exp_tok(_T13B8);goto _LL0;case 490: _T13B9=yyyvsp;_T13BA=_T13B9[0];
# 3022
yyval=_T13BA.v;goto _LL0;case 491: _T13BB=yyyvsp;_T13BC=& _T13BB[1].v;_T13BD=(union Cyc_YYSTYPE*)_T13BC;_T13BE=
# 3024 "parse.y"
Cyc_yyget_YY69(_T13BD);_T13BF=yyyvsp;_T13C0=& _T13BF[0].v;_T13C1=(union Cyc_YYSTYPE*)_T13C0;_T13C2=Cyc_yyget_Exp_tok(_T13C1);_T13C3=yyyvsp;_T13C4=& _T13C3[2].v;_T13C5=(union Cyc_YYSTYPE*)_T13C4;_T13C6=Cyc_yyget_Exp_tok(_T13C5);_T13C7=yyyvsp;_T13C8=_T13C7[0];_T13C9=_T13C8.l;_T13CA=_T13C9.first_line;_T13CB=Cyc_Position_loc_to_seg(_T13CA);_T13CC=_T13BE(_T13C2,_T13C6,_T13CB);yyval=Cyc_Exp_tok(_T13CC);goto _LL0;case 492: _T13CD=yyyvsp;_T13CE=_T13CD[0];
# 3027
yyval=_T13CE.v;goto _LL0;case 493: _T13CF=yyyvsp;_T13D0=& _T13CF[0].v;_T13D1=(union Cyc_YYSTYPE*)_T13D0;_T13D2=
# 3029 "parse.y"
Cyc_yyget_Exp_tok(_T13D1);_T13D3=yyyvsp;_T13D4=& _T13D3[2].v;_T13D5=(union Cyc_YYSTYPE*)_T13D4;_T13D6=Cyc_yyget_Exp_tok(_T13D5);_T13D7=yyyvsp;_T13D8=_T13D7[0];_T13D9=_T13D8.l;_T13DA=_T13D9.first_line;_T13DB=Cyc_Position_loc_to_seg(_T13DA);_T13DC=Cyc_Absyn_prim2_exp(16U,_T13D2,_T13D6,_T13DB);yyval=Cyc_Exp_tok(_T13DC);goto _LL0;case 494: _T13DD=yyyvsp;_T13DE=& _T13DD[0].v;_T13DF=(union Cyc_YYSTYPE*)_T13DE;_T13E0=
# 3031 "parse.y"
Cyc_yyget_Exp_tok(_T13DF);_T13E1=yyyvsp;_T13E2=& _T13E1[2].v;_T13E3=(union Cyc_YYSTYPE*)_T13E2;_T13E4=Cyc_yyget_Exp_tok(_T13E3);_T13E5=yyyvsp;_T13E6=_T13E5[0];_T13E7=_T13E6.l;_T13E8=_T13E7.first_line;_T13E9=Cyc_Position_loc_to_seg(_T13E8);_T13EA=Cyc_Absyn_prim2_exp(17U,_T13E0,_T13E4,_T13E9);yyval=Cyc_Exp_tok(_T13EA);goto _LL0;case 495: _T13EB=yyyvsp;_T13EC=_T13EB[0];
# 3034
yyval=_T13EC.v;goto _LL0;case 496: _T13ED=yyyvsp;_T13EE=& _T13ED[1].v;_T13EF=(union Cyc_YYSTYPE*)_T13EE;_T13F0=
# 3036 "parse.y"
Cyc_yyget_YY6(_T13EF);_T13F1=yyyvsp;_T13F2=& _T13F1[0].v;_T13F3=(union Cyc_YYSTYPE*)_T13F2;_T13F4=Cyc_yyget_Exp_tok(_T13F3);_T13F5=yyyvsp;_T13F6=& _T13F5[2].v;_T13F7=(union Cyc_YYSTYPE*)_T13F6;_T13F8=Cyc_yyget_Exp_tok(_T13F7);_T13F9=yyyvsp;_T13FA=_T13F9[0];_T13FB=_T13FA.l;_T13FC=_T13FB.first_line;_T13FD=Cyc_Position_loc_to_seg(_T13FC);_T13FE=Cyc_Absyn_prim2_exp(_T13F0,_T13F4,_T13F8,_T13FD);yyval=Cyc_Exp_tok(_T13FE);goto _LL0;case 497: _T13FF=yyyvsp;_T1400=_T13FF[0];
# 3039
yyval=_T1400.v;goto _LL0;case 498: _T1401=yyyvsp;_T1402=& _T1401[1].v;_T1403=(union Cyc_YYSTYPE*)_T1402;_T1404=
# 3041 "parse.y"
Cyc_yyget_YY6(_T1403);_T1405=yyyvsp;_T1406=& _T1405[0].v;_T1407=(union Cyc_YYSTYPE*)_T1406;_T1408=Cyc_yyget_Exp_tok(_T1407);_T1409=yyyvsp;_T140A=& _T1409[2].v;_T140B=(union Cyc_YYSTYPE*)_T140A;_T140C=Cyc_yyget_Exp_tok(_T140B);_T140D=yyyvsp;_T140E=_T140D[0];_T140F=_T140E.l;_T1410=_T140F.first_line;_T1411=Cyc_Position_loc_to_seg(_T1410);_T1412=Cyc_Absyn_prim2_exp(_T1404,_T1408,_T140C,_T1411);yyval=Cyc_Exp_tok(_T1412);goto _LL0;case 499: _T1413=Cyc_Absyn_eq_exp;
# 3044
yyval=Cyc_YY69(_T1413);goto _LL0;case 500: _T1414=Cyc_Absyn_neq_exp;
# 3045 "parse.y"
yyval=Cyc_YY69(_T1414);goto _LL0;case 501: _T1415=Cyc_Absyn_lt_exp;
# 3048
yyval=Cyc_YY69(_T1415);goto _LL0;case 502: _T1416=Cyc_Absyn_gt_exp;
# 3049 "parse.y"
yyval=Cyc_YY69(_T1416);goto _LL0;case 503: _T1417=Cyc_Absyn_lte_exp;
# 3050 "parse.y"
yyval=Cyc_YY69(_T1417);goto _LL0;case 504: _T1418=Cyc_Absyn_gte_exp;
# 3051 "parse.y"
yyval=Cyc_YY69(_T1418);goto _LL0;case 505:
# 3054
 yyval=Cyc_YY6(0U);goto _LL0;case 506:
# 3055 "parse.y"
 yyval=Cyc_YY6(2U);goto _LL0;case 507:
# 3058
 yyval=Cyc_YY6(1U);goto _LL0;case 508:
# 3059 "parse.y"
 yyval=Cyc_YY6(3U);goto _LL0;case 509:
# 3060 "parse.y"
 yyval=Cyc_YY6(4U);goto _LL0;case 510: _T1419=yyyvsp;_T141A=_T1419[0];
# 3064 "parse.y"
yyval=_T141A.v;goto _LL0;case 511: _T141B=yyyvsp;_T141C=& _T141B[1].v;_T141D=(union Cyc_YYSTYPE*)_T141C;_T141E=
# 3066 "parse.y"
Cyc_yyget_YY38(_T141D);_T141F=yyyvsp;_T1420=_T141F[1];_T1421=_T1420.l;_T1422=_T1421.first_line;_T1423=Cyc_Position_loc_to_seg(_T1422);{void*t=Cyc_Parse_type_name_to_type(_T141E,_T1423);_T1424=t;_T1425=yyyvsp;_T1426=& _T1425[3].v;_T1427=(union Cyc_YYSTYPE*)_T1426;_T1428=
Cyc_yyget_Exp_tok(_T1427);_T1429=yyyvsp;_T142A=_T1429[0];_T142B=_T142A.l;_T142C=_T142B.first_line;_T142D=Cyc_Position_loc_to_seg(_T142C);_T142E=Cyc_Absyn_cast_exp(_T1424,_T1428,1,0U,_T142D);yyval=Cyc_Exp_tok(_T142E);goto _LL0;}case 512: _T142F=yyyvsp;_T1430=_T142F[0];
# 3071 "parse.y"
yyval=_T1430.v;goto _LL0;case 513: _T1431=yyyvsp;_T1432=& _T1431[1].v;_T1433=(union Cyc_YYSTYPE*)_T1432;_T1434=
# 3072 "parse.y"
Cyc_yyget_Exp_tok(_T1433);_T1435=yyyvsp;_T1436=_T1435[0];_T1437=_T1436.l;_T1438=_T1437.first_line;_T1439=Cyc_Position_loc_to_seg(_T1438);_T143A=Cyc_Absyn_increment_exp(_T1434,0U,_T1439);yyval=Cyc_Exp_tok(_T143A);goto _LL0;case 514: _T143B=yyyvsp;_T143C=& _T143B[1].v;_T143D=(union Cyc_YYSTYPE*)_T143C;_T143E=
# 3073 "parse.y"
Cyc_yyget_Exp_tok(_T143D);_T143F=yyyvsp;_T1440=_T143F[0];_T1441=_T1440.l;_T1442=_T1441.first_line;_T1443=Cyc_Position_loc_to_seg(_T1442);_T1444=Cyc_Absyn_increment_exp(_T143E,2U,_T1443);yyval=Cyc_Exp_tok(_T1444);goto _LL0;case 515: _T1445=yyyvsp;_T1446=& _T1445[1].v;_T1447=(union Cyc_YYSTYPE*)_T1446;_T1448=
# 3074 "parse.y"
Cyc_yyget_Exp_tok(_T1447);_T1449=yyyvsp;_T144A=_T1449[0];_T144B=_T144A.l;_T144C=_T144B.first_line;_T144D=Cyc_Position_loc_to_seg(_T144C);_T144E=Cyc_Absyn_address_exp(_T1448,_T144D);yyval=Cyc_Exp_tok(_T144E);goto _LL0;case 516: _T144F=yyyvsp;_T1450=& _T144F[1].v;_T1451=(union Cyc_YYSTYPE*)_T1450;_T1452=
# 3075 "parse.y"
Cyc_yyget_Exp_tok(_T1451);_T1453=yyyvsp;_T1454=_T1453[0];_T1455=_T1454.l;_T1456=_T1455.first_line;_T1457=Cyc_Position_loc_to_seg(_T1456);_T1458=Cyc_Absyn_deref_exp(_T1452,_T1457);yyval=Cyc_Exp_tok(_T1458);goto _LL0;case 517: _T1459=yyyvsp;_T145A=& _T1459[0].v;_T145B=(union Cyc_YYSTYPE*)_T145A;_T145C=
# 3076 "parse.y"
Cyc_yyget_YY6(_T145B);_T145D=yyyvsp;_T145E=& _T145D[1].v;_T145F=(union Cyc_YYSTYPE*)_T145E;_T1460=Cyc_yyget_Exp_tok(_T145F);_T1461=yyyvsp;_T1462=_T1461[0];_T1463=_T1462.l;_T1464=_T1463.first_line;_T1465=Cyc_Position_loc_to_seg(_T1464);_T1466=Cyc_Absyn_prim1_exp(_T145C,_T1460,_T1465);yyval=Cyc_Exp_tok(_T1466);goto _LL0;case 518: _T1467=yyyvsp;_T1468=& _T1467[2].v;_T1469=(union Cyc_YYSTYPE*)_T1468;_T146A=
# 3078 "parse.y"
Cyc_yyget_YY38(_T1469);_T146B=yyyvsp;_T146C=_T146B[2];_T146D=_T146C.l;_T146E=_T146D.first_line;_T146F=Cyc_Position_loc_to_seg(_T146E);{void*t=Cyc_Parse_type_name_to_type(_T146A,_T146F);_T1470=t;_T1471=yyyvsp;_T1472=_T1471[0];_T1473=_T1472.l;_T1474=_T1473.first_line;_T1475=
Cyc_Position_loc_to_seg(_T1474);_T1476=Cyc_Absyn_sizeoftype_exp(_T1470,_T1475);yyval=Cyc_Exp_tok(_T1476);goto _LL0;}case 519: _T1477=yyyvsp;_T1478=& _T1477[1].v;_T1479=(union Cyc_YYSTYPE*)_T1478;_T147A=
# 3081 "parse.y"
Cyc_yyget_Exp_tok(_T1479);_T147B=yyyvsp;_T147C=_T147B[0];_T147D=_T147C.l;_T147E=_T147D.first_line;_T147F=Cyc_Position_loc_to_seg(_T147E);_T1480=Cyc_Absyn_sizeofexp_exp(_T147A,_T147F);yyval=Cyc_Exp_tok(_T1480);goto _LL0;case 520: _T1481=yyyvsp;_T1482=& _T1481[2].v;_T1483=(union Cyc_YYSTYPE*)_T1482;_T1484=
# 3083 "parse.y"
Cyc_yyget_YY38(_T1483);_T1485=yyyvsp;_T1486=_T1485[2];_T1487=_T1486.l;_T1488=_T1487.first_line;_T1489=Cyc_Position_loc_to_seg(_T1488);{void*t=Cyc_Parse_type_name_to_type(_T1484,_T1489);_T148A=t;_T148B=yyyvsp;_T148C=& _T148B[4].v;_T148D=(union Cyc_YYSTYPE*)_T148C;_T148E=
Cyc_yyget_YY3(_T148D);_T148F=Cyc_List_imp_rev(_T148E);_T1490=yyyvsp;_T1491=_T1490[0];_T1492=_T1491.l;_T1493=_T1492.first_line;_T1494=Cyc_Position_loc_to_seg(_T1493);_T1495=Cyc_Absyn_offsetof_exp(_T148A,_T148F,_T1494);yyval=Cyc_Exp_tok(_T1495);goto _LL0;}case 521:{struct Cyc_Absyn_Malloc_e_Absyn_Raw_exp_struct*_T18B2=_cycalloc(sizeof(struct Cyc_Absyn_Malloc_e_Absyn_Raw_exp_struct));_T18B2->tag=33;
# 3088
_T18B2->f1.mknd=0U;_T18B2->f1.rgn=0;_T18B2->f1.aqual=0;_T18B2->f1.elt_type=0;_T1497=yyyvsp;_T1498=& _T1497[2].v;_T1499=(union Cyc_YYSTYPE*)_T1498;_T18B2->f1.num_elts=Cyc_yyget_Exp_tok(_T1499);_T18B2->f1.fat_result=0;_T18B2->f1.inline_call=0;_T1496=(struct Cyc_Absyn_Malloc_e_Absyn_Raw_exp_struct*)_T18B2;}_T149A=(void*)_T1496;_T149B=yyyvsp;_T149C=_T149B[0];_T149D=_T149C.l;_T149E=_T149D.first_line;_T149F=
Cyc_Position_loc_to_seg(_T149E);_T14A0=
# 3088
Cyc_Absyn_new_exp(_T149A,_T149F);yyval=Cyc_Exp_tok(_T14A0);goto _LL0;case 522:{struct Cyc_Absyn_Malloc_e_Absyn_Raw_exp_struct*_T18B2=_cycalloc(sizeof(struct Cyc_Absyn_Malloc_e_Absyn_Raw_exp_struct));_T18B2->tag=33;
# 3091 "parse.y"
_T18B2->f1.mknd=0U;_T14A2=yyyvsp;_T14A3=& _T14A2[2].v;_T14A4=(union Cyc_YYSTYPE*)_T14A3;_T18B2->f1.rgn=Cyc_yyget_Exp_tok(_T14A4);_T14A5=yyyvsp;_T14A6=& _T14A5[4].v;_T14A7=(union Cyc_YYSTYPE*)_T14A6;_T18B2->f1.aqual=Cyc_yyget_Exp_tok(_T14A7);_T18B2->f1.elt_type=0;_T14A8=yyyvsp;_T14A9=& _T14A8[6].v;_T14AA=(union Cyc_YYSTYPE*)_T14A9;_T18B2->f1.num_elts=Cyc_yyget_Exp_tok(_T14AA);_T18B2->f1.fat_result=0;_T18B2->f1.inline_call=0;_T14A1=(struct Cyc_Absyn_Malloc_e_Absyn_Raw_exp_struct*)_T18B2;}_T14AB=(void*)_T14A1;_T14AC=yyyvsp;_T14AD=_T14AC[0];_T14AE=_T14AD.l;_T14AF=_T14AE.first_line;_T14B0=
Cyc_Position_loc_to_seg(_T14AF);_T14B1=
# 3091
Cyc_Absyn_new_exp(_T14AB,_T14B0);yyval=Cyc_Exp_tok(_T14B1);goto _LL0;case 523:{struct Cyc_Absyn_Malloc_e_Absyn_Raw_exp_struct*_T18B2=_cycalloc(sizeof(struct Cyc_Absyn_Malloc_e_Absyn_Raw_exp_struct));_T18B2->tag=33;
# 3094 "parse.y"
_T18B2->f1.mknd=0U;_T14B3=yyyvsp;_T14B4=& _T14B3[2].v;_T14B5=(union Cyc_YYSTYPE*)_T14B4;_T18B2->f1.rgn=Cyc_yyget_Exp_tok(_T14B5);_T18B2->f1.aqual=0;_T18B2->f1.elt_type=0;_T14B6=yyyvsp;_T14B7=& _T14B6[4].v;_T14B8=(union Cyc_YYSTYPE*)_T14B7;_T18B2->f1.num_elts=Cyc_yyget_Exp_tok(_T14B8);_T18B2->f1.fat_result=0;_T18B2->f1.inline_call=0;_T14B2=(struct Cyc_Absyn_Malloc_e_Absyn_Raw_exp_struct*)_T18B2;}_T14B9=(void*)_T14B2;_T14BA=yyyvsp;_T14BB=_T14BA[0];_T14BC=_T14BB.l;_T14BD=_T14BC.first_line;_T14BE=
Cyc_Position_loc_to_seg(_T14BD);_T14BF=
# 3094
Cyc_Absyn_new_exp(_T14B9,_T14BE);yyval=Cyc_Exp_tok(_T14BF);goto _LL0;case 524: _T14C0=yyyvsp;_T14C1=& _T14C0[2].v;_T14C2=(union Cyc_YYSTYPE*)_T14C1;_T14C3=
# 3097 "parse.y"
Cyc_yyget_Exp_tok(_T14C2);{struct _tuple16 _T18B2=Cyc_Parse_split_seq(_T14C3);struct Cyc_Absyn_Exp*_T18B3;struct Cyc_Absyn_Exp*_T18B4;_T18B4=_T18B2.f0;_T18B3=_T18B2.f1;{struct Cyc_Absyn_Exp*rgn=_T18B4;struct Cyc_Absyn_Exp*qual=_T18B3;{struct Cyc_Absyn_Malloc_e_Absyn_Raw_exp_struct*_T18B5=_cycalloc(sizeof(struct Cyc_Absyn_Malloc_e_Absyn_Raw_exp_struct));_T18B5->tag=33;
_T18B5->f1.mknd=2U;_T18B5->f1.rgn=rgn;_T18B5->f1.aqual=qual;_T18B5->f1.elt_type=0;_T14C5=yyyvsp;_T14C6=& _T14C5[4].v;_T14C7=(union Cyc_YYSTYPE*)_T14C6;_T18B5->f1.num_elts=Cyc_yyget_Exp_tok(_T14C7);_T18B5->f1.fat_result=0;_T18B5->f1.inline_call=0;_T14C4=(struct Cyc_Absyn_Malloc_e_Absyn_Raw_exp_struct*)_T18B5;}_T14C8=(void*)_T14C4;_T14C9=yyyvsp;_T14CA=_T14C9[0];_T14CB=_T14CA.l;_T14CC=_T14CB.first_line;_T14CD=
Cyc_Position_loc_to_seg(_T14CC);_T14CE=
# 3098
Cyc_Absyn_new_exp(_T14C8,_T14CD);yyval=Cyc_Exp_tok(_T14CE);goto _LL0;}}case 525: _T14CF=yyyvsp;_T14D0=& _T14CF[2].v;_T14D1=(union Cyc_YYSTYPE*)_T14D0;_T14D2=
# 3101 "parse.y"
Cyc_yyget_Exp_tok(_T14D1);{struct _tuple16 _T18B2=Cyc_Parse_split_seq(_T14D2);struct Cyc_Absyn_Exp*_T18B3;struct Cyc_Absyn_Exp*_T18B4;_T18B4=_T18B2.f0;_T18B3=_T18B2.f1;{struct Cyc_Absyn_Exp*rgn=_T18B4;struct Cyc_Absyn_Exp*qual=_T18B3;{struct Cyc_Absyn_Malloc_e_Absyn_Raw_exp_struct*_T18B5=_cycalloc(sizeof(struct Cyc_Absyn_Malloc_e_Absyn_Raw_exp_struct));_T18B5->tag=33;
_T18B5->f1.mknd=0U;_T18B5->f1.rgn=rgn;_T18B5->f1.aqual=qual;_T18B5->f1.elt_type=0;_T14D4=yyyvsp;_T14D5=& _T14D4[4].v;_T14D6=(union Cyc_YYSTYPE*)_T14D5;_T18B5->f1.num_elts=Cyc_yyget_Exp_tok(_T14D6);_T18B5->f1.fat_result=0;_T18B5->f1.inline_call=1;_T14D3=(struct Cyc_Absyn_Malloc_e_Absyn_Raw_exp_struct*)_T18B5;}_T14D7=(void*)_T14D3;_T14D8=yyyvsp;_T14D9=_T14D8[0];_T14DA=_T14D9.l;_T14DB=_T14DA.first_line;_T14DC=
Cyc_Position_loc_to_seg(_T14DB);_T14DD=
# 3102
Cyc_Absyn_new_exp(_T14D7,_T14DC);yyval=Cyc_Exp_tok(_T14DD);goto _LL0;}}case 526: _T14DE=yyyvsp;_T14DF=& _T14DE[6].v;_T14E0=(union Cyc_YYSTYPE*)_T14DF;_T14E1=
# 3105 "parse.y"
Cyc_yyget_YY38(_T14E0);_T14E2=yyyvsp;_T14E3=_T14E2[6];_T14E4=_T14E3.l;_T14E5=_T14E4.first_line;_T14E6=Cyc_Position_loc_to_seg(_T14E5);{void*t=Cyc_Parse_type_name_to_type(_T14E1,_T14E6);{struct Cyc_Absyn_Malloc_e_Absyn_Raw_exp_struct*_T18B2=_cycalloc(sizeof(struct Cyc_Absyn_Malloc_e_Absyn_Raw_exp_struct));_T18B2->tag=33;
_T18B2->f1.mknd=1U;_T18B2->f1.rgn=0;_T18B2->f1.aqual=0;{void**_T18B3=_cycalloc(sizeof(void*));*_T18B3=t;_T14E8=(void**)_T18B3;}_T18B2->f1.elt_type=_T14E8;_T14E9=yyyvsp;_T14EA=& _T14E9[2].v;_T14EB=(union Cyc_YYSTYPE*)_T14EA;_T18B2->f1.num_elts=Cyc_yyget_Exp_tok(_T14EB);_T18B2->f1.fat_result=0;_T18B2->f1.inline_call=0;_T14E7=(struct Cyc_Absyn_Malloc_e_Absyn_Raw_exp_struct*)_T18B2;}_T14EC=(void*)_T14E7;_T14ED=yyyvsp;_T14EE=_T14ED[0];_T14EF=_T14EE.l;_T14F0=_T14EF.first_line;_T14F1=
Cyc_Position_loc_to_seg(_T14F0);_T14F2=
# 3106
Cyc_Absyn_new_exp(_T14EC,_T14F1);yyval=Cyc_Exp_tok(_T14F2);goto _LL0;}case 527: _T14F3=yyyvsp;_T14F4=& _T14F3[10].v;_T14F5=(union Cyc_YYSTYPE*)_T14F4;_T14F6=
# 3109 "parse.y"
Cyc_yyget_YY38(_T14F5);_T14F7=yyyvsp;_T14F8=_T14F7[10];_T14F9=_T14F8.l;_T14FA=_T14F9.first_line;_T14FB=Cyc_Position_loc_to_seg(_T14FA);{void*t=Cyc_Parse_type_name_to_type(_T14F6,_T14FB);{struct Cyc_Absyn_Malloc_e_Absyn_Raw_exp_struct*_T18B2=_cycalloc(sizeof(struct Cyc_Absyn_Malloc_e_Absyn_Raw_exp_struct));_T18B2->tag=33;
_T18B2->f1.mknd=1U;_T14FD=yyyvsp;_T14FE=& _T14FD[2].v;_T14FF=(union Cyc_YYSTYPE*)_T14FE;_T18B2->f1.rgn=Cyc_yyget_Exp_tok(_T14FF);_T1500=yyyvsp;_T1501=& _T1500[4].v;_T1502=(union Cyc_YYSTYPE*)_T1501;_T18B2->f1.aqual=Cyc_yyget_Exp_tok(_T1502);{void**_T18B3=_cycalloc(sizeof(void*));*_T18B3=t;_T1503=(void**)_T18B3;}_T18B2->f1.elt_type=_T1503;_T1504=yyyvsp;_T1505=& _T1504[6].v;_T1506=(union Cyc_YYSTYPE*)_T1505;_T18B2->f1.num_elts=Cyc_yyget_Exp_tok(_T1506);_T18B2->f1.fat_result=0;_T18B2->f1.inline_call=0;_T14FC=(struct Cyc_Absyn_Malloc_e_Absyn_Raw_exp_struct*)_T18B2;}_T1507=(void*)_T14FC;_T1508=yyyvsp;_T1509=_T1508[0];_T150A=_T1509.l;_T150B=_T150A.first_line;_T150C=
Cyc_Position_loc_to_seg(_T150B);_T150D=
# 3110
Cyc_Absyn_new_exp(_T1507,_T150C);yyval=Cyc_Exp_tok(_T150D);goto _LL0;}case 528: _T150E=yyyvsp;_T150F=& _T150E[8].v;_T1510=(union Cyc_YYSTYPE*)_T150F;_T1511=
# 3114
Cyc_yyget_YY38(_T1510);_T1512=yyyvsp;_T1513=_T1512[8];_T1514=_T1513.l;_T1515=_T1514.first_line;_T1516=Cyc_Position_loc_to_seg(_T1515);{void*t=Cyc_Parse_type_name_to_type(_T1511,_T1516);{struct Cyc_Absyn_Malloc_e_Absyn_Raw_exp_struct*_T18B2=_cycalloc(sizeof(struct Cyc_Absyn_Malloc_e_Absyn_Raw_exp_struct));_T18B2->tag=33;
_T18B2->f1.mknd=1U;_T1518=yyyvsp;_T1519=& _T1518[2].v;_T151A=(union Cyc_YYSTYPE*)_T1519;_T18B2->f1.rgn=Cyc_yyget_Exp_tok(_T151A);_T18B2->f1.aqual=0;{void**_T18B3=_cycalloc(sizeof(void*));*_T18B3=t;_T151B=(void**)_T18B3;}_T18B2->f1.elt_type=_T151B;_T151C=yyyvsp;_T151D=& _T151C[4].v;_T151E=(union Cyc_YYSTYPE*)_T151D;_T18B2->f1.num_elts=Cyc_yyget_Exp_tok(_T151E);_T18B2->f1.fat_result=0;_T18B2->f1.inline_call=0;_T1517=(struct Cyc_Absyn_Malloc_e_Absyn_Raw_exp_struct*)_T18B2;}_T151F=(void*)_T1517;_T1520=yyyvsp;_T1521=_T1520[0];_T1522=_T1521.l;_T1523=_T1522.first_line;_T1524=
Cyc_Position_loc_to_seg(_T1523);_T1525=
# 3115
Cyc_Absyn_new_exp(_T151F,_T1524);yyval=Cyc_Exp_tok(_T1525);goto _LL0;}case 529:{struct Cyc_Absyn_Exp*_T18B2[1];_T1527=yyyvsp;_T1528=& _T1527[2].v;_T1529=(union Cyc_YYSTYPE*)_T1528;_T18B2[0]=
# 3118 "parse.y"
Cyc_yyget_Exp_tok(_T1529);_T152A=_tag_fat(_T18B2,sizeof(struct Cyc_Absyn_Exp*),1);_T1526=Cyc_List_list(_T152A);}_T152B=yyyvsp;_T152C=_T152B[0];_T152D=_T152C.l;_T152E=_T152D.first_line;_T152F=Cyc_Position_loc_to_seg(_T152E);_T1530=Cyc_Absyn_primop_exp(18U,_T1526,_T152F);yyval=Cyc_Exp_tok(_T1530);goto _LL0;case 530: _T1531=yyyvsp;_T1532=& _T1531[2].v;_T1533=(union Cyc_YYSTYPE*)_T1532;_T1534=
# 3120 "parse.y"
Cyc_yyget_Exp_tok(_T1533);_T1535=yyyvsp;_T1536=_T1535[0];_T1537=_T1536.l;_T1538=_T1537.first_line;_T1539=Cyc_Position_loc_to_seg(_T1538);_T153A=Cyc_Absyn_tagof_exp(_T1534,_T1539);yyval=Cyc_Exp_tok(_T153A);goto _LL0;case 531:{struct Cyc_Absyn_Tagcheck_e_Absyn_Raw_exp_struct*_T18B2=_cycalloc(sizeof(struct Cyc_Absyn_Tagcheck_e_Absyn_Raw_exp_struct));_T18B2->tag=37;_T153C=yyyvsp;_T153D=& _T153C[2].v;_T153E=(union Cyc_YYSTYPE*)_T153D;
# 3122 "parse.y"
_T18B2->f1=Cyc_yyget_Exp_tok(_T153E);{struct _fat_ptr*_T18B3=_cycalloc(sizeof(struct _fat_ptr));_T1540=yyyvsp;_T1541=& _T1540[4].v;_T1542=(union Cyc_YYSTYPE*)_T1541;*_T18B3=Cyc_yyget_String_tok(_T1542);_T153F=(struct _fat_ptr*)_T18B3;}_T18B2->f2=_T153F;_T153B=(struct Cyc_Absyn_Tagcheck_e_Absyn_Raw_exp_struct*)_T18B2;}_T1543=(void*)_T153B;_T1544=yyyvsp;_T1545=_T1544[0];_T1546=_T1545.l;_T1547=_T1546.first_line;_T1548=Cyc_Position_loc_to_seg(_T1547);_T1549=Cyc_Absyn_new_exp(_T1543,_T1548);yyval=Cyc_Exp_tok(_T1549);goto _LL0;case 532:{struct Cyc_Absyn_Tagcheck_e_Absyn_Raw_exp_struct*_T18B2=_cycalloc(sizeof(struct Cyc_Absyn_Tagcheck_e_Absyn_Raw_exp_struct));_T18B2->tag=37;_T154B=yyyvsp;_T154C=& _T154B[2].v;_T154D=(union Cyc_YYSTYPE*)_T154C;_T154E=
# 3124 "parse.y"
Cyc_yyget_Exp_tok(_T154D);_T154F=yyyvsp;_T1550=_T154F[2];_T1551=_T1550.l;_T1552=_T1551.first_line;_T1553=Cyc_Position_loc_to_seg(_T1552);_T18B2->f1=Cyc_Absyn_deref_exp(_T154E,_T1553);{struct _fat_ptr*_T18B3=_cycalloc(sizeof(struct _fat_ptr));_T1555=yyyvsp;_T1556=& _T1555[4].v;_T1557=(union Cyc_YYSTYPE*)_T1556;*_T18B3=Cyc_yyget_String_tok(_T1557);_T1554=(struct _fat_ptr*)_T18B3;}_T18B2->f2=_T1554;_T154A=(struct Cyc_Absyn_Tagcheck_e_Absyn_Raw_exp_struct*)_T18B2;}_T1558=(void*)_T154A;_T1559=yyyvsp;_T155A=_T1559[0];_T155B=_T155A.l;_T155C=_T155B.first_line;_T155D=Cyc_Position_loc_to_seg(_T155C);_T155E=Cyc_Absyn_new_exp(_T1558,_T155D);yyval=Cyc_Exp_tok(_T155E);goto _LL0;case 533: _T155F=yyyvsp;_T1560=& _T155F[2].v;_T1561=(union Cyc_YYSTYPE*)_T1560;_T1562=
# 3126 "parse.y"
Cyc_yyget_YY38(_T1561);_T1563=yyyvsp;_T1564=_T1563[2];_T1565=_T1564.l;_T1566=_T1565.first_line;_T1567=Cyc_Position_loc_to_seg(_T1566);{void*t=Cyc_Parse_type_name_to_type(_T1562,_T1567);_T1568=t;_T1569=yyyvsp;_T156A=_T1569[0];_T156B=_T156A.l;_T156C=_T156B.first_line;_T156D=
Cyc_Position_loc_to_seg(_T156C);_T156E=Cyc_Absyn_valueof_exp(_T1568,_T156D);yyval=Cyc_Exp_tok(_T156E);goto _LL0;}case 534: _T156F=yyyvsp;_T1570=& _T156F[1].v;_T1571=(union Cyc_YYSTYPE*)_T1570;_T1572=
# 3128 "parse.y"
Cyc_yyget_YY63(_T1571);_T1573=yyyvsp;_T1574=_T1573[0];_T1575=_T1574.l;_T1576=_T1575.first_line;_T1577=Cyc_Position_loc_to_seg(_T1576);_T1578=Cyc_Absyn_new_exp(_T1572,_T1577);yyval=Cyc_Exp_tok(_T1578);goto _LL0;case 535: _T1579=yyyvsp;_T157A=& _T1579[1].v;_T157B=(union Cyc_YYSTYPE*)_T157A;_T157C=
# 3129 "parse.y"
Cyc_yyget_Exp_tok(_T157B);_T157D=yyyvsp;_T157E=_T157D[0];_T157F=_T157E.l;_T1580=_T157F.first_line;_T1581=Cyc_Position_loc_to_seg(_T1580);_T1582=Cyc_Absyn_extension_exp(_T157C,_T1581);yyval=Cyc_Exp_tok(_T1582);goto _LL0;case 536: _T1583=yyyvsp;_T1584=& _T1583[2].v;_T1585=(union Cyc_YYSTYPE*)_T1584;_T1586=
# 3130 "parse.y"
Cyc_yyget_Exp_tok(_T1585);_T1587=yyyvsp;_T1588=_T1587[0];_T1589=_T1588.l;_T158A=_T1589.first_line;_T158B=Cyc_Position_loc_to_seg(_T158A);_T158C=Cyc_Absyn_assert_exp(_T1586,0,_T158B);yyval=Cyc_Exp_tok(_T158C);goto _LL0;case 537: _T158D=yyyvsp;_T158E=& _T158D[2].v;_T158F=(union Cyc_YYSTYPE*)_T158E;_T1590=
# 3131 "parse.y"
Cyc_yyget_Exp_tok(_T158F);_T1591=yyyvsp;_T1592=_T1591[0];_T1593=_T1592.l;_T1594=_T1593.first_line;_T1595=Cyc_Position_loc_to_seg(_T1594);_T1596=Cyc_Absyn_assert_exp(_T1590,1,_T1595);yyval=Cyc_Exp_tok(_T1596);goto _LL0;case 538: _T1597=yyyvsp;_T1598=& _T1597[2].v;_T1599=(union Cyc_YYSTYPE*)_T1598;_T159A=
# 3132 "parse.y"
Cyc_yyget_Exp_tok(_T1599);_T159B=yyyvsp;_T159C=_T159B[0];_T159D=_T159C.l;_T159E=_T159D.first_line;_T159F=Cyc_Position_loc_to_seg(_T159E);_T15A0=Cyc_Absyn_assert_false_exp(_T159A,_T159F);yyval=Cyc_Exp_tok(_T15A0);goto _LL0;case 539:
# 3136 "parse.y"
 yyval=Cyc_YY6(12U);goto _LL0;case 540:
# 3137 "parse.y"
 yyval=Cyc_YY6(11U);goto _LL0;case 541:
# 3138 "parse.y"
 yyval=Cyc_YY6(2U);goto _LL0;case 542:
# 3139 "parse.y"
 yyval=Cyc_YY6(0U);goto _LL0;case 543: _T15A1=yyyvsp;_T15A2=& _T15A1[3].v;_T15A3=(union Cyc_YYSTYPE*)_T15A2;{
# 3144 "parse.y"
struct _tuple31*_T18B2=Cyc_yyget_YY64(_T15A3);struct Cyc_List_List*_T18B3;struct Cyc_List_List*_T18B4;struct Cyc_List_List*_T18B5;{struct _tuple31 _T18B6=*_T18B2;_T18B5=_T18B6.f0;_T18B4=_T18B6.f1;_T18B3=_T18B6.f2;}{struct Cyc_List_List*outlist=_T18B5;struct Cyc_List_List*inlist=_T18B4;struct Cyc_List_List*clobbers=_T18B3;{struct Cyc_Absyn_Asm_e_Absyn_Raw_exp_struct*_T18B6=_cycalloc(sizeof(struct Cyc_Absyn_Asm_e_Absyn_Raw_exp_struct));_T18B6->tag=39;_T15A5=yyyvsp;_T15A6=& _T15A5[0].v;_T15A7=(union Cyc_YYSTYPE*)_T15A6;
_T18B6->f1=Cyc_yyget_YY32(_T15A7);_T15A8=yyyvsp;_T15A9=& _T15A8[2].v;_T15AA=(union Cyc_YYSTYPE*)_T15A9;_T18B6->f2=Cyc_yyget_String_tok(_T15AA);_T18B6->f3=outlist;_T18B6->f4=inlist;_T18B6->f5=clobbers;_T15A4=(struct Cyc_Absyn_Asm_e_Absyn_Raw_exp_struct*)_T18B6;}_T15AB=(void*)_T15A4;yyval=Cyc_YY63(_T15AB);goto _LL0;}}case 544:
# 3149 "parse.y"
 yyval=Cyc_YY32(0);goto _LL0;case 545:
# 3150 "parse.y"
 yyval=Cyc_YY32(1);goto _LL0;case 546:{struct _tuple31*_T18B2=_cycalloc(sizeof(struct _tuple31));
# 3154 "parse.y"
_T18B2->f0=0;_T18B2->f1=0;_T18B2->f2=0;_T15AC=(struct _tuple31*)_T18B2;}yyval=Cyc_YY64(_T15AC);goto _LL0;case 547: _T15AD=yyyvsp;_T15AE=& _T15AD[1].v;_T15AF=(union Cyc_YYSTYPE*)_T15AE;{
# 3156 "parse.y"
struct _tuple28*_T18B2=Cyc_yyget_YY65(_T15AF);struct Cyc_List_List*_T18B3;struct Cyc_List_List*_T18B4;{struct _tuple28 _T18B5=*_T18B2;_T18B4=_T18B5.f0;_T18B3=_T18B5.f1;}{struct Cyc_List_List*inlist=_T18B4;struct Cyc_List_List*clobbers=_T18B3;{struct _tuple31*_T18B5=_cycalloc(sizeof(struct _tuple31));
_T18B5->f0=0;_T18B5->f1=inlist;_T18B5->f2=clobbers;_T15B0=(struct _tuple31*)_T18B5;}yyval=Cyc_YY64(_T15B0);goto _LL0;}}case 548: _T15B1=yyyvsp;_T15B2=& _T15B1[2].v;_T15B3=(union Cyc_YYSTYPE*)_T15B2;{
# 3159 "parse.y"
struct _tuple28*_T18B2=Cyc_yyget_YY65(_T15B3);struct Cyc_List_List*_T18B3;struct Cyc_List_List*_T18B4;{struct _tuple28 _T18B5=*_T18B2;_T18B4=_T18B5.f0;_T18B3=_T18B5.f1;}{struct Cyc_List_List*inlist=_T18B4;struct Cyc_List_List*clobbers=_T18B3;{struct _tuple31*_T18B5=_cycalloc(sizeof(struct _tuple31));_T15B5=yyyvsp;_T15B6=& _T15B5[1].v;_T15B7=(union Cyc_YYSTYPE*)_T15B6;_T15B8=
Cyc_yyget_YY67(_T15B7);_T18B5->f0=Cyc_List_imp_rev(_T15B8);_T18B5->f1=inlist;_T18B5->f2=clobbers;_T15B4=(struct _tuple31*)_T18B5;}yyval=Cyc_YY64(_T15B4);goto _LL0;}}case 549:{struct Cyc_List_List*_T18B2=_cycalloc(sizeof(struct Cyc_List_List));_T15BA=yyyvsp;_T15BB=& _T15BA[0].v;_T15BC=(union Cyc_YYSTYPE*)_T15BB;
# 3163
_T18B2->hd=Cyc_yyget_YY68(_T15BC);_T18B2->tl=0;_T15B9=(struct Cyc_List_List*)_T18B2;}yyval=Cyc_YY67(_T15B9);goto _LL0;case 550:{struct Cyc_List_List*_T18B2=_cycalloc(sizeof(struct Cyc_List_List));_T15BE=yyyvsp;_T15BF=& _T15BE[2].v;_T15C0=(union Cyc_YYSTYPE*)_T15BF;
# 3164 "parse.y"
_T18B2->hd=Cyc_yyget_YY68(_T15C0);_T15C1=yyyvsp;_T15C2=& _T15C1[0].v;_T15C3=(union Cyc_YYSTYPE*)_T15C2;_T18B2->tl=Cyc_yyget_YY67(_T15C3);_T15BD=(struct Cyc_List_List*)_T18B2;}yyval=Cyc_YY67(_T15BD);goto _LL0;case 551:{struct _tuple28*_T18B2=_cycalloc(sizeof(struct _tuple28));
# 3168 "parse.y"
_T18B2->f0=0;_T18B2->f1=0;_T15C4=(struct _tuple28*)_T18B2;}yyval=Cyc_YY65(_T15C4);goto _LL0;case 552:{struct _tuple28*_T18B2=_cycalloc(sizeof(struct _tuple28));
# 3169 "parse.y"
_T18B2->f0=0;_T15C6=yyyvsp;_T15C7=& _T15C6[1].v;_T15C8=(union Cyc_YYSTYPE*)_T15C7;_T18B2->f1=Cyc_yyget_YY66(_T15C8);_T15C5=(struct _tuple28*)_T18B2;}yyval=Cyc_YY65(_T15C5);goto _LL0;case 553:{struct _tuple28*_T18B2=_cycalloc(sizeof(struct _tuple28));_T15CA=yyyvsp;_T15CB=& _T15CA[1].v;_T15CC=(union Cyc_YYSTYPE*)_T15CB;_T15CD=
# 3170 "parse.y"
Cyc_yyget_YY67(_T15CC);_T18B2->f0=Cyc_List_imp_rev(_T15CD);_T15CE=yyyvsp;_T15CF=& _T15CE[2].v;_T15D0=(union Cyc_YYSTYPE*)_T15CF;_T18B2->f1=Cyc_yyget_YY66(_T15D0);_T15C9=(struct _tuple28*)_T18B2;}yyval=Cyc_YY65(_T15C9);goto _LL0;case 554:{struct Cyc_List_List*_T18B2=_cycalloc(sizeof(struct Cyc_List_List));_T15D2=yyyvsp;_T15D3=& _T15D2[0].v;_T15D4=(union Cyc_YYSTYPE*)_T15D3;
# 3173
_T18B2->hd=Cyc_yyget_YY68(_T15D4);_T18B2->tl=0;_T15D1=(struct Cyc_List_List*)_T18B2;}yyval=Cyc_YY67(_T15D1);goto _LL0;case 555:{struct Cyc_List_List*_T18B2=_cycalloc(sizeof(struct Cyc_List_List));_T15D6=yyyvsp;_T15D7=& _T15D6[2].v;_T15D8=(union Cyc_YYSTYPE*)_T15D7;
# 3174 "parse.y"
_T18B2->hd=Cyc_yyget_YY68(_T15D8);_T15D9=yyyvsp;_T15DA=& _T15D9[0].v;_T15DB=(union Cyc_YYSTYPE*)_T15DA;_T18B2->tl=Cyc_yyget_YY67(_T15DB);_T15D5=(struct Cyc_List_List*)_T18B2;}yyval=Cyc_YY67(_T15D5);goto _LL0;case 556:{struct _tuple32*_T18B2=_cycalloc(sizeof(struct _tuple32));_T15DD=yyyvsp;_T15DE=& _T15DD[0].v;_T15DF=(union Cyc_YYSTYPE*)_T15DE;
# 3178 "parse.y"
_T18B2->f0=Cyc_yyget_String_tok(_T15DF);_T15E0=yyyvsp;_T15E1=& _T15E0[2].v;_T15E2=(union Cyc_YYSTYPE*)_T15E1;_T18B2->f1=Cyc_yyget_Exp_tok(_T15E2);_T15DC=(struct _tuple32*)_T18B2;}yyval=Cyc_YY68(_T15DC);goto _LL0;case 557:
# 3183 "parse.y"
 yyval=Cyc_YY66(0);goto _LL0;case 558:
# 3184 "parse.y"
 yyval=Cyc_YY66(0);goto _LL0;case 559: _T15E3=yyyvsp;_T15E4=& _T15E3[1].v;_T15E5=(union Cyc_YYSTYPE*)_T15E4;_T15E6=
# 3185 "parse.y"
Cyc_yyget_YY66(_T15E5);_T15E7=Cyc_List_imp_rev(_T15E6);yyval=Cyc_YY66(_T15E7);goto _LL0;case 560:{struct Cyc_List_List*_T18B2=_cycalloc(sizeof(struct Cyc_List_List));{struct _fat_ptr*_T18B3=_cycalloc(sizeof(struct _fat_ptr));_T15EA=yyyvsp;_T15EB=& _T15EA[0].v;_T15EC=(union Cyc_YYSTYPE*)_T15EB;
# 3188
*_T18B3=Cyc_yyget_String_tok(_T15EC);_T15E9=(struct _fat_ptr*)_T18B3;}_T18B2->hd=_T15E9;_T18B2->tl=0;_T15E8=(struct Cyc_List_List*)_T18B2;}yyval=Cyc_YY66(_T15E8);goto _LL0;case 561:{struct Cyc_List_List*_T18B2=_cycalloc(sizeof(struct Cyc_List_List));{struct _fat_ptr*_T18B3=_cycalloc(sizeof(struct _fat_ptr));_T15EF=yyyvsp;_T15F0=& _T15EF[2].v;_T15F1=(union Cyc_YYSTYPE*)_T15F0;
# 3189 "parse.y"
*_T18B3=Cyc_yyget_String_tok(_T15F1);_T15EE=(struct _fat_ptr*)_T18B3;}_T18B2->hd=_T15EE;_T15F2=yyyvsp;_T15F3=& _T15F2[0].v;_T15F4=(union Cyc_YYSTYPE*)_T15F3;_T18B2->tl=Cyc_yyget_YY66(_T15F4);_T15ED=(struct Cyc_List_List*)_T18B2;}yyval=Cyc_YY66(_T15ED);goto _LL0;case 562: _T15F5=yyyvsp;_T15F6=_T15F5[0];
# 3194 "parse.y"
yyval=_T15F6.v;goto _LL0;case 563: _T15F7=yyyvsp;_T15F8=& _T15F7[0].v;_T15F9=(union Cyc_YYSTYPE*)_T15F8;_T15FA=
# 3196 "parse.y"
Cyc_yyget_Exp_tok(_T15F9);_T15FB=yyyvsp;_T15FC=& _T15FB[2].v;_T15FD=(union Cyc_YYSTYPE*)_T15FC;_T15FE=Cyc_yyget_Exp_tok(_T15FD);_T15FF=yyyvsp;_T1600=_T15FF[0];_T1601=_T1600.l;_T1602=_T1601.first_line;_T1603=Cyc_Position_loc_to_seg(_T1602);_T1604=Cyc_Absyn_subscript_exp(_T15FA,_T15FE,_T1603);yyval=Cyc_Exp_tok(_T1604);goto _LL0;case 564: _T1605=yyyvsp;_T1606=& _T1605[0].v;_T1607=(union Cyc_YYSTYPE*)_T1606;_T1608=
# 3198 "parse.y"
Cyc_yyget_Exp_tok(_T1607);_T1609=yyyvsp;_T160A=_T1609[0];_T160B=_T160A.l;_T160C=_T160B.first_line;_T160D=Cyc_Position_loc_to_seg(_T160C);_T160E=Cyc_Absyn_unknowncall_exp(_T1608,0,_T160D);yyval=Cyc_Exp_tok(_T160E);goto _LL0;case 565: _T160F=yyyvsp;_T1610=& _T160F[0].v;_T1611=(union Cyc_YYSTYPE*)_T1610;_T1612=
# 3200 "parse.y"
Cyc_yyget_Exp_tok(_T1611);_T1613=yyyvsp;_T1614=& _T1613[2].v;_T1615=(union Cyc_YYSTYPE*)_T1614;_T1616=Cyc_yyget_YY4(_T1615);_T1617=yyyvsp;_T1618=_T1617[0];_T1619=_T1618.l;_T161A=_T1619.first_line;_T161B=Cyc_Position_loc_to_seg(_T161A);_T161C=Cyc_Absyn_unknowncall_exp(_T1612,_T1616,_T161B);yyval=Cyc_Exp_tok(_T161C);goto _LL0;case 566: _T161D=yyyvsp;_T161E=& _T161D[0].v;_T161F=(union Cyc_YYSTYPE*)_T161E;_T1620=
# 3202 "parse.y"
Cyc_yyget_Exp_tok(_T161F);{struct _fat_ptr*_T18B2=_cycalloc(sizeof(struct _fat_ptr));_T1622=yyyvsp;_T1623=& _T1622[2].v;_T1624=(union Cyc_YYSTYPE*)_T1623;*_T18B2=Cyc_yyget_String_tok(_T1624);_T1621=(struct _fat_ptr*)_T18B2;}_T1625=yyyvsp;_T1626=_T1625[0];_T1627=_T1626.l;_T1628=_T1627.first_line;_T1629=Cyc_Position_loc_to_seg(_T1628);_T162A=Cyc_Absyn_aggrmember_exp(_T1620,_T1621,_T1629);yyval=Cyc_Exp_tok(_T162A);goto _LL0;case 567: _T162B=yyyvsp;_T162C=& _T162B[0].v;_T162D=(union Cyc_YYSTYPE*)_T162C;_T162E=
# 3204 "parse.y"
Cyc_yyget_Exp_tok(_T162D);{struct _fat_ptr*_T18B2=_cycalloc(sizeof(struct _fat_ptr));_T1630=yyyvsp;_T1631=& _T1630[2].v;_T1632=(union Cyc_YYSTYPE*)_T1631;*_T18B2=Cyc_yyget_String_tok(_T1632);_T162F=(struct _fat_ptr*)_T18B2;}_T1633=yyyvsp;_T1634=_T1633[0];_T1635=_T1634.l;_T1636=_T1635.first_line;_T1637=Cyc_Position_loc_to_seg(_T1636);_T1638=Cyc_Absyn_aggrarrow_exp(_T162E,_T162F,_T1637);yyval=Cyc_Exp_tok(_T1638);goto _LL0;case 568: _T1639=yyyvsp;_T163A=& _T1639[0].v;_T163B=(union Cyc_YYSTYPE*)_T163A;_T163C=
# 3206 "parse.y"
Cyc_yyget_Exp_tok(_T163B);_T163D=yyyvsp;_T163E=_T163D[0];_T163F=_T163E.l;_T1640=_T163F.first_line;_T1641=Cyc_Position_loc_to_seg(_T1640);_T1642=Cyc_Absyn_increment_exp(_T163C,1U,_T1641);yyval=Cyc_Exp_tok(_T1642);goto _LL0;case 569: _T1643=yyyvsp;_T1644=& _T1643[0].v;_T1645=(union Cyc_YYSTYPE*)_T1644;_T1646=
# 3208 "parse.y"
Cyc_yyget_Exp_tok(_T1645);_T1647=yyyvsp;_T1648=_T1647[0];_T1649=_T1648.l;_T164A=_T1649.first_line;_T164B=Cyc_Position_loc_to_seg(_T164A);_T164C=Cyc_Absyn_increment_exp(_T1646,3U,_T164B);yyval=Cyc_Exp_tok(_T164C);goto _LL0;case 570:{struct Cyc_Absyn_CompoundLit_e_Absyn_Raw_exp_struct*_T18B2=_cycalloc(sizeof(struct Cyc_Absyn_CompoundLit_e_Absyn_Raw_exp_struct));_T18B2->tag=24;_T164E=yyyvsp;_T164F=& _T164E[1].v;_T1650=(union Cyc_YYSTYPE*)_T164F;
# 3210 "parse.y"
_T18B2->f1=Cyc_yyget_YY38(_T1650);_T18B2->f2=0;_T164D=(struct Cyc_Absyn_CompoundLit_e_Absyn_Raw_exp_struct*)_T18B2;}_T1651=(void*)_T164D;_T1652=yyyvsp;_T1653=_T1652[0];_T1654=_T1653.l;_T1655=_T1654.first_line;_T1656=Cyc_Position_loc_to_seg(_T1655);_T1657=Cyc_Absyn_new_exp(_T1651,_T1656);yyval=Cyc_Exp_tok(_T1657);goto _LL0;case 571:{struct Cyc_Absyn_CompoundLit_e_Absyn_Raw_exp_struct*_T18B2=_cycalloc(sizeof(struct Cyc_Absyn_CompoundLit_e_Absyn_Raw_exp_struct));_T18B2->tag=24;_T1659=yyyvsp;_T165A=& _T1659[1].v;_T165B=(union Cyc_YYSTYPE*)_T165A;
# 3212 "parse.y"
_T18B2->f1=Cyc_yyget_YY38(_T165B);_T165C=yyyvsp;_T165D=& _T165C[4].v;_T165E=(union Cyc_YYSTYPE*)_T165D;_T165F=Cyc_yyget_YY5(_T165E);_T18B2->f2=Cyc_List_imp_rev(_T165F);_T1658=(struct Cyc_Absyn_CompoundLit_e_Absyn_Raw_exp_struct*)_T18B2;}_T1660=(void*)_T1658;_T1661=yyyvsp;_T1662=_T1661[0];_T1663=_T1662.l;_T1664=_T1663.first_line;_T1665=Cyc_Position_loc_to_seg(_T1664);_T1666=Cyc_Absyn_new_exp(_T1660,_T1665);yyval=Cyc_Exp_tok(_T1666);goto _LL0;case 572:{struct Cyc_Absyn_CompoundLit_e_Absyn_Raw_exp_struct*_T18B2=_cycalloc(sizeof(struct Cyc_Absyn_CompoundLit_e_Absyn_Raw_exp_struct));_T18B2->tag=24;_T1668=yyyvsp;_T1669=& _T1668[1].v;_T166A=(union Cyc_YYSTYPE*)_T1669;
# 3214 "parse.y"
_T18B2->f1=Cyc_yyget_YY38(_T166A);_T166B=yyyvsp;_T166C=& _T166B[4].v;_T166D=(union Cyc_YYSTYPE*)_T166C;_T166E=Cyc_yyget_YY5(_T166D);_T18B2->f2=Cyc_List_imp_rev(_T166E);_T1667=(struct Cyc_Absyn_CompoundLit_e_Absyn_Raw_exp_struct*)_T18B2;}_T166F=(void*)_T1667;_T1670=yyyvsp;_T1671=_T1670[0];_T1672=_T1671.l;_T1673=_T1672.first_line;_T1674=Cyc_Position_loc_to_seg(_T1673);_T1675=Cyc_Absyn_new_exp(_T166F,_T1674);yyval=Cyc_Exp_tok(_T1675);goto _LL0;case 573:{struct Cyc_List_List*_T18B2=_cycalloc(sizeof(struct Cyc_List_List));{struct Cyc_Absyn_StructField_Absyn_OffsetofField_struct*_T18B3=_cycalloc(sizeof(struct Cyc_Absyn_StructField_Absyn_OffsetofField_struct));_T18B3->tag=0;{struct _fat_ptr*_T18B4=_cycalloc(sizeof(struct _fat_ptr));_T1679=yyyvsp;_T167A=& _T1679[0].v;_T167B=(union Cyc_YYSTYPE*)_T167A;
# 3219 "parse.y"
*_T18B4=Cyc_yyget_String_tok(_T167B);_T1678=(struct _fat_ptr*)_T18B4;}_T18B3->f1=_T1678;_T1677=(struct Cyc_Absyn_StructField_Absyn_OffsetofField_struct*)_T18B3;}_T18B2->hd=(void*)_T1677;_T18B2->tl=0;_T1676=(struct Cyc_List_List*)_T18B2;}yyval=Cyc_YY3(_T1676);goto _LL0;case 574: _T167C=yyyvsp;_T167D=_T167C[0];_T167E=_T167D.l;_T167F=_T167E.first_line;_T1680=
# 3222
Cyc_Position_loc_to_seg(_T167F);_T1681=yyyvsp;_T1682=& _T1681[0].v;_T1683=(union Cyc_YYSTYPE*)_T1682;_T1684=Cyc_yyget_Int_tok(_T1683);{unsigned i=Cyc_Parse_cnst2uint(_T1680,_T1684);_T1685=i;_T1686=(int)_T1685;{
struct Cyc_Absyn_FieldName_Absyn_Designator_struct*_T18B2=Cyc_Absyn_tuple_field_designator(_T1686);struct _fat_ptr*_T18B3;{struct Cyc_Absyn_FieldName_Absyn_Designator_struct*_T18B4=(struct Cyc_Absyn_FieldName_Absyn_Designator_struct*)_T18B2;_T18B3=_T18B4->f1;}{struct _fat_ptr*f=_T18B3;{struct Cyc_List_List*_T18B4=_cycalloc(sizeof(struct Cyc_List_List));{struct Cyc_Absyn_StructField_Absyn_OffsetofField_struct*_T18B5=_cycalloc(sizeof(struct Cyc_Absyn_StructField_Absyn_OffsetofField_struct));_T18B5->tag=0;
_T18B5->f1=f;_T1688=(struct Cyc_Absyn_StructField_Absyn_OffsetofField_struct*)_T18B5;}_T18B4->hd=(void*)_T1688;_T18B4->tl=0;_T1687=(struct Cyc_List_List*)_T18B4;}yyval=Cyc_YY3(_T1687);goto _LL0;}}}case 575:{struct Cyc_List_List*_T18B2=_cycalloc(sizeof(struct Cyc_List_List));{struct Cyc_Absyn_StructField_Absyn_OffsetofField_struct*_T18B3=_cycalloc(sizeof(struct Cyc_Absyn_StructField_Absyn_OffsetofField_struct));_T18B3->tag=0;{struct _fat_ptr*_T18B4=_cycalloc(sizeof(struct _fat_ptr));_T168C=yyyvsp;_T168D=& _T168C[2].v;_T168E=(union Cyc_YYSTYPE*)_T168D;
# 3226 "parse.y"
*_T18B4=Cyc_yyget_String_tok(_T168E);_T168B=(struct _fat_ptr*)_T18B4;}_T18B3->f1=_T168B;_T168A=(struct Cyc_Absyn_StructField_Absyn_OffsetofField_struct*)_T18B3;}_T18B2->hd=(void*)_T168A;_T168F=yyyvsp;_T1690=& _T168F[0].v;_T1691=(union Cyc_YYSTYPE*)_T1690;_T18B2->tl=Cyc_yyget_YY3(_T1691);_T1689=(struct Cyc_List_List*)_T18B2;}yyval=Cyc_YY3(_T1689);goto _LL0;case 576: _T1692=yyyvsp;_T1693=_T1692[2];_T1694=_T1693.l;_T1695=_T1694.first_line;_T1696=
# 3229
Cyc_Position_loc_to_seg(_T1695);_T1697=yyyvsp;_T1698=& _T1697[2].v;_T1699=(union Cyc_YYSTYPE*)_T1698;_T169A=Cyc_yyget_Int_tok(_T1699);{unsigned i=Cyc_Parse_cnst2uint(_T1696,_T169A);_T169B=i;_T169C=(int)_T169B;{
struct Cyc_Absyn_FieldName_Absyn_Designator_struct*_T18B2=Cyc_Absyn_tuple_field_designator(_T169C);struct _fat_ptr*_T18B3;{struct Cyc_Absyn_FieldName_Absyn_Designator_struct*_T18B4=(struct Cyc_Absyn_FieldName_Absyn_Designator_struct*)_T18B2;_T18B3=_T18B4->f1;}{struct _fat_ptr*f=_T18B3;{struct Cyc_List_List*_T18B4=_cycalloc(sizeof(struct Cyc_List_List));{struct Cyc_Absyn_StructField_Absyn_OffsetofField_struct*_T18B5=_cycalloc(sizeof(struct Cyc_Absyn_StructField_Absyn_OffsetofField_struct));_T18B5->tag=0;
_T18B5->f1=f;_T169E=(struct Cyc_Absyn_StructField_Absyn_OffsetofField_struct*)_T18B5;}_T18B4->hd=(void*)_T169E;_T169F=yyyvsp;_T16A0=& _T169F[0].v;_T16A1=(union Cyc_YYSTYPE*)_T16A0;_T18B4->tl=Cyc_yyget_YY3(_T16A1);_T169D=(struct Cyc_List_List*)_T18B4;}yyval=Cyc_YY3(_T169D);goto _LL0;}}}case 577: _T16A2=yyyvsp;_T16A3=& _T16A2[0].v;_T16A4=(union Cyc_YYSTYPE*)_T16A3;_T16A5=
# 3236 "parse.y"
Cyc_yyget_QualId_tok(_T16A4);_T16A6=yyyvsp;_T16A7=_T16A6[0];_T16A8=_T16A7.l;_T16A9=_T16A8.first_line;_T16AA=Cyc_Position_loc_to_seg(_T16A9);_T16AB=Cyc_Absyn_unknownid_exp(_T16A5,_T16AA);yyval=Cyc_Exp_tok(_T16AB);goto _LL0;case 578: _T16AC=yyyvsp;_T16AD=& _T16AC[2].v;_T16AE=(union Cyc_YYSTYPE*)_T16AD;_T16AF=
# 3237 "parse.y"
Cyc_yyget_String_tok(_T16AE);_T16B0=yyyvsp;_T16B1=_T16B0[0];_T16B2=_T16B1.l;_T16B3=_T16B2.first_line;_T16B4=Cyc_Position_loc_to_seg(_T16B3);_T16B5=Cyc_Absyn_pragma_exp(_T16AF,_T16B4);yyval=Cyc_Exp_tok(_T16B5);goto _LL0;case 579: _T16B6=yyyvsp;_T16B7=_T16B6[0];
# 3238 "parse.y"
yyval=_T16B7.v;goto _LL0;case 580: _T16B8=yyyvsp;_T16B9=& _T16B8[0].v;_T16BA=(union Cyc_YYSTYPE*)_T16B9;_T16BB=
# 3239 "parse.y"
Cyc_yyget_String_tok(_T16BA);_T16BC=yyyvsp;_T16BD=_T16BC[0];_T16BE=_T16BD.l;_T16BF=_T16BE.first_line;_T16C0=Cyc_Position_loc_to_seg(_T16BF);_T16C1=Cyc_Absyn_string_exp(_T16BB,_T16C0);yyval=Cyc_Exp_tok(_T16C1);goto _LL0;case 581: _T16C2=yyyvsp;_T16C3=& _T16C2[0].v;_T16C4=(union Cyc_YYSTYPE*)_T16C3;_T16C5=
# 3240 "parse.y"
Cyc_yyget_String_tok(_T16C4);_T16C6=yyyvsp;_T16C7=_T16C6[0];_T16C8=_T16C7.l;_T16C9=_T16C8.first_line;_T16CA=Cyc_Position_loc_to_seg(_T16C9);_T16CB=Cyc_Absyn_wstring_exp(_T16C5,_T16CA);yyval=Cyc_Exp_tok(_T16CB);goto _LL0;case 582: _T16CC=yyyvsp;_T16CD=_T16CC[1];
# 3241 "parse.y"
yyval=_T16CD.v;goto _LL0;case 583: _T16CE=yyyvsp;_T16CF=& _T16CE[0].v;_T16D0=(union Cyc_YYSTYPE*)_T16CF;_T16D1=
# 3245 "parse.y"
Cyc_yyget_Exp_tok(_T16D0);_T16D2=yyyvsp;_T16D3=_T16D2[0];_T16D4=_T16D3.l;_T16D5=_T16D4.first_line;_T16D6=Cyc_Position_loc_to_seg(_T16D5);_T16D7=Cyc_Absyn_noinstantiate_exp(_T16D1,_T16D6);yyval=Cyc_Exp_tok(_T16D7);goto _LL0;case 584: _T16D8=yyyvsp;_T16D9=& _T16D8[0].v;_T16DA=(union Cyc_YYSTYPE*)_T16D9;_T16DB=
# 3248
Cyc_yyget_Exp_tok(_T16DA);_T16DC=yyyvsp;_T16DD=& _T16DC[3].v;_T16DE=(union Cyc_YYSTYPE*)_T16DD;_T16DF=Cyc_yyget_YY41(_T16DE);_T16E0=Cyc_List_imp_rev(_T16DF);_T16E1=yyyvsp;_T16E2=_T16E1[0];_T16E3=_T16E2.l;_T16E4=_T16E3.first_line;_T16E5=Cyc_Position_loc_to_seg(_T16E4);_T16E6=Cyc_Absyn_instantiate_exp(_T16DB,_T16E0,_T16E5);yyval=Cyc_Exp_tok(_T16E6);goto _LL0;case 585:{struct Cyc_Absyn_Aggregate_e_Absyn_Raw_exp_struct*_T18B2=_cycalloc(sizeof(struct Cyc_Absyn_Aggregate_e_Absyn_Raw_exp_struct));_T18B2->tag=28;_T16E8=yyyvsp;_T16E9=& _T16E8[0].v;_T16EA=(union Cyc_YYSTYPE*)_T16E9;
# 3251
_T18B2->f1=Cyc_yyget_QualId_tok(_T16EA);_T16EB=yyyvsp;_T16EC=& _T16EB[2].v;_T16ED=(union Cyc_YYSTYPE*)_T16EC;_T18B2->f2=Cyc_yyget_YY41(_T16ED);_T16EE=yyyvsp;_T16EF=& _T16EE[3].v;_T16F0=(union Cyc_YYSTYPE*)_T16EF;_T16F1=Cyc_yyget_YY5(_T16F0);_T18B2->f3=Cyc_List_imp_rev(_T16F1);_T18B2->f4=0;_T16E7=(struct Cyc_Absyn_Aggregate_e_Absyn_Raw_exp_struct*)_T18B2;}_T16F2=(void*)_T16E7;_T16F3=yyyvsp;_T16F4=_T16F3[0];_T16F5=_T16F4.l;_T16F6=_T16F5.first_line;_T16F7=Cyc_Position_loc_to_seg(_T16F6);_T16F8=Cyc_Absyn_new_exp(_T16F2,_T16F7);yyval=Cyc_Exp_tok(_T16F8);goto _LL0;case 586: _T16F9=yyyvsp;_T16FA=& _T16F9[2].v;_T16FB=(union Cyc_YYSTYPE*)_T16FA;_T16FC=
# 3253 "parse.y"
Cyc_yyget_YY4(_T16FB);_T16FD=yyyvsp;_T16FE=_T16FD[0];_T16FF=_T16FE.l;_T1700=_T16FF.first_line;_T1701=Cyc_Position_loc_to_seg(_T1700);_T1702=Cyc_Absyn_tuple_exp(_T16FC,_T1701);yyval=Cyc_Exp_tok(_T1702);goto _LL0;case 587: _T1703=yyyvsp;_T1704=& _T1703[2].v;_T1705=(union Cyc_YYSTYPE*)_T1704;_T1706=
# 3255 "parse.y"
Cyc_yyget_Stmt_tok(_T1705);_T1707=yyyvsp;_T1708=_T1707[0];_T1709=_T1708.l;_T170A=_T1709.first_line;_T170B=Cyc_Position_loc_to_seg(_T170A);_T170C=Cyc_Absyn_stmt_exp(_T1706,_T170B);yyval=Cyc_Exp_tok(_T170C);goto _LL0;case 588: _T170D=yyyvsp;_T170E=& _T170D[0].v;_T170F=(union Cyc_YYSTYPE*)_T170E;_T1710=
# 3259 "parse.y"
Cyc_yyget_YY4(_T170F);_T1711=Cyc_List_imp_rev(_T1710);yyval=Cyc_YY4(_T1711);goto _LL0;case 589:{struct Cyc_List_List*_T18B2=_cycalloc(sizeof(struct Cyc_List_List));_T1713=yyyvsp;_T1714=& _T1713[0].v;_T1715=(union Cyc_YYSTYPE*)_T1714;
# 3264 "parse.y"
_T18B2->hd=Cyc_yyget_Exp_tok(_T1715);_T18B2->tl=0;_T1712=(struct Cyc_List_List*)_T18B2;}yyval=Cyc_YY4(_T1712);goto _LL0;case 590:{struct Cyc_List_List*_T18B2=_cycalloc(sizeof(struct Cyc_List_List));_T1717=yyyvsp;_T1718=& _T1717[2].v;_T1719=(union Cyc_YYSTYPE*)_T1718;
# 3266 "parse.y"
_T18B2->hd=Cyc_yyget_Exp_tok(_T1719);_T171A=yyyvsp;_T171B=& _T171A[0].v;_T171C=(union Cyc_YYSTYPE*)_T171B;_T18B2->tl=Cyc_yyget_YY4(_T171C);_T1716=(struct Cyc_List_List*)_T18B2;}yyval=Cyc_YY4(_T1716);goto _LL0;case 591: _T171D=yyyvsp;_T171E=& _T171D[0].v;_T171F=(union Cyc_YYSTYPE*)_T171E;_T1720=
# 3272 "parse.y"
Cyc_yyget_Int_tok(_T171F);_T1721=yyyvsp;_T1722=_T1721[0];_T1723=_T1722.l;_T1724=_T1723.first_line;_T1725=Cyc_Position_loc_to_seg(_T1724);_T1726=Cyc_Absyn_const_exp(_T1720,_T1725);yyval=Cyc_Exp_tok(_T1726);goto _LL0;case 592: _T1727=yyyvsp;_T1728=& _T1727[0].v;_T1729=(union Cyc_YYSTYPE*)_T1728;_T172A=
# 3273 "parse.y"
Cyc_yyget_Char_tok(_T1729);_T172B=yyyvsp;_T172C=_T172B[0];_T172D=_T172C.l;_T172E=_T172D.first_line;_T172F=Cyc_Position_loc_to_seg(_T172E);_T1730=Cyc_Absyn_char_exp(_T172A,_T172F);yyval=Cyc_Exp_tok(_T1730);goto _LL0;case 593: _T1731=yyyvsp;_T1732=& _T1731[0].v;_T1733=(union Cyc_YYSTYPE*)_T1732;_T1734=
# 3274 "parse.y"
Cyc_yyget_String_tok(_T1733);_T1735=yyyvsp;_T1736=_T1735[0];_T1737=_T1736.l;_T1738=_T1737.first_line;_T1739=Cyc_Position_loc_to_seg(_T1738);_T173A=Cyc_Absyn_wchar_exp(_T1734,_T1739);yyval=Cyc_Exp_tok(_T173A);goto _LL0;case 594: _T173B=yyyvsp;_T173C=_T173B[0];_T173D=_T173C.l;_T173E=_T173D.first_line;_T173F=
# 3276 "parse.y"
Cyc_Position_loc_to_seg(_T173E);_T1740=Cyc_Absyn_null_exp(_T173F);yyval=Cyc_Exp_tok(_T1740);goto _LL0;case 595: _T1741=yyyvsp;_T1742=& _T1741[0].v;_T1743=(union Cyc_YYSTYPE*)_T1742;{
# 3278 "parse.y"
struct _fat_ptr f=Cyc_yyget_String_tok(_T1743);_T1744=
Cyc_strlen(f);{int l=(int)_T1744;
int i=1;
if(l <= 0)goto _TL2F3;_T1745=f;_T1746=_T1745.curr;_T1747=(const char*)_T1746;_T1748=
_check_null(_T1747);_T1749=l - 1;{char c=_T1748[_T1749];_T174A=c;_T174B=(int)_T174A;
if(_T174B==102)goto _TL2F7;else{goto _TL2F8;}_TL2F8: _T174C=c;_T174D=(int)_T174C;if(_T174D==70)goto _TL2F7;else{goto _TL2F5;}_TL2F7: i=0;goto _TL2F6;
_TL2F5: _T174E=c;_T174F=(int)_T174E;if(_T174F==108)goto _TL2FB;else{goto _TL2FC;}_TL2FC: _T1750=c;_T1751=(int)_T1750;if(_T1751==76)goto _TL2FB;else{goto _TL2F9;}_TL2FB: i=2;goto _TL2FA;_TL2F9: _TL2FA: _TL2F6:;}goto _TL2F4;_TL2F3: _TL2F4: _T1752=f;_T1753=i;_T1754=yyyvsp;_T1755=_T1754[0];_T1756=_T1755.l;_T1757=_T1756.first_line;_T1758=
# 3286
Cyc_Position_loc_to_seg(_T1757);_T1759=Cyc_Absyn_float_exp(_T1752,_T1753,_T1758);yyval=Cyc_Exp_tok(_T1759);goto _LL0;}}case 596:{struct _tuple0*_T18B2=_cycalloc(sizeof(struct _tuple0));
# 3291 "parse.y"
_T18B2->f0=Cyc_Absyn_Rel_n(0);{struct _fat_ptr*_T18B3=_cycalloc(sizeof(struct _fat_ptr));_T175C=yyyvsp;_T175D=& _T175C[0].v;_T175E=(union Cyc_YYSTYPE*)_T175D;*_T18B3=Cyc_yyget_String_tok(_T175E);_T175B=(struct _fat_ptr*)_T18B3;}_T18B2->f1=_T175B;_T175A=(struct _tuple0*)_T18B2;}yyval=Cyc_QualId_tok(_T175A);goto _LL0;case 597: _T175F=yyyvsp;_T1760=_T175F[0];
# 3292 "parse.y"
yyval=_T1760.v;goto _LL0;case 598:{struct _tuple0*_T18B2=_cycalloc(sizeof(struct _tuple0));
# 3295
_T18B2->f0=Cyc_Absyn_Rel_n(0);{struct _fat_ptr*_T18B3=_cycalloc(sizeof(struct _fat_ptr));_T1763=yyyvsp;_T1764=& _T1763[0].v;_T1765=(union Cyc_YYSTYPE*)_T1764;*_T18B3=Cyc_yyget_String_tok(_T1765);_T1762=(struct _fat_ptr*)_T18B3;}_T18B2->f1=_T1762;_T1761=(struct _tuple0*)_T18B2;}yyval=Cyc_QualId_tok(_T1761);goto _LL0;case 599: _T1766=yyyvsp;_T1767=_T1766[0];
# 3296 "parse.y"
yyval=_T1767.v;goto _LL0;case 600: _T1768=yyyvsp;_T1769=_T1768[0];
# 3301 "parse.y"
yyval=_T1769.v;goto _LL0;case 601: _T176A=yyyvsp;_T176B=_T176A[0];
# 3302 "parse.y"
yyval=_T176B.v;goto _LL0;case 602: _T176C=yyyvsp;_T176D=_T176C[0];
# 3305
yyval=_T176D.v;goto _LL0;case 603: _T176E=yyyvsp;_T176F=_T176E[0];
# 3306 "parse.y"
yyval=_T176F.v;goto _LL0;case 604: goto _LL0;case 605: _T1770=yylex_buf;
# 3311 "parse.y"
_T1770->lex_curr_pos=_T1770->lex_curr_pos - 1;goto _LL0;case 606:
# 3315 "parse.y"
 yyval=Cyc_YY71(0);goto _LL0;case 607:{struct Cyc_List_List*_T18B2=_cycalloc(sizeof(struct Cyc_List_List));{struct _tuple35*_T18B3=_cycalloc(sizeof(struct _tuple35));_T1773=yyyvsp;_T1774=& _T1773[1].v;_T1775=(union Cyc_YYSTYPE*)_T1774;_T1776=
# 3317 "parse.y"
Cyc_yyget_String_tok(_T1775);_T1777=yyyvsp;_T1778=& _T1777[3].v;_T1779=(union Cyc_YYSTYPE*)_T1778;_T177A=Cyc_yyget_String_tok(_T1779);_T177B=_tag_fat("true",sizeof(char),5U);_T177C=Cyc_strcmp(_T177A,_T177B);_T177D=_T177C==0;_T177E=yyyvsp;_T177F=& _T177E[0].v;_T1780=(union Cyc_YYSTYPE*)_T177F;_T1781=Cyc_yyget_String_tok(_T1780);_T1782=Cyc_Parse_typevar2cvar(_T1781);_T18B3->f0=Cyc_Parse_assign_cvar_pos(_T1776,_T177D,_T1782);_T1783=yyyvsp;_T1784=& _T1783[5].v;_T1785=(union Cyc_YYSTYPE*)_T1784;_T18B3->f1=Cyc_yyget_YY72(_T1785);_T1772=(struct _tuple35*)_T18B3;}_T18B2->hd=_T1772;_T1786=yyyvsp;_T1787=& _T1786[7].v;_T1788=(union Cyc_YYSTYPE*)_T1787;_T18B2->tl=Cyc_yyget_YY71(_T1788);_T1771=(struct Cyc_List_List*)_T18B2;}yyval=Cyc_YY71(_T1771);goto _LL0;case 608:
# 3321 "parse.y"
 yyval=Cyc_YY72(0);goto _LL0;case 609: _T1789=yyyvsp;_T178A=& _T1789[0].v;_T178B=(union Cyc_YYSTYPE*)_T178A;_T178C=
# 3322 "parse.y"
Cyc_yyget_YY72(_T178B);yyval=Cyc_YY72(_T178C);goto _LL0;case 610:{struct Cyc_List_List*_T18B2=_cycalloc(sizeof(struct Cyc_List_List));_T178E=yyyvsp;_T178F=& _T178E[0].v;_T1790=(union Cyc_YYSTYPE*)_T178F;
# 3326 "parse.y"
_T18B2->hd=Cyc_yyget_YY73(_T1790);_T18B2->tl=0;_T178D=(struct Cyc_List_List*)_T18B2;}yyval=Cyc_YY72(_T178D);goto _LL0;case 611:{struct Cyc_List_List*_T18B2=_cycalloc(sizeof(struct Cyc_List_List));_T1792=yyyvsp;_T1793=& _T1792[0].v;_T1794=(union Cyc_YYSTYPE*)_T1793;
# 3327 "parse.y"
_T18B2->hd=Cyc_yyget_YY73(_T1794);_T1795=yyyvsp;_T1796=& _T1795[2].v;_T1797=(union Cyc_YYSTYPE*)_T1796;_T18B2->tl=Cyc_yyget_YY72(_T1797);_T1791=(struct Cyc_List_List*)_T18B2;}yyval=Cyc_YY72(_T1791);goto _LL0;case 612: _T1798=yyyvsp;_T1799=& _T1798[0].v;_T179A=(union Cyc_YYSTYPE*)_T1799;_T179B=
# 3331 "parse.y"
Cyc_yyget_String_tok(_T179A);_T179C=Cyc_Parse_typevar2cvar(_T179B);yyval=Cyc_YY45(_T179C);goto _LL0;case 613: _T179D=yyyvsp;_T179E=_T179D[0];_T179F=_T179E.l;_T17A0=_T179F.first_line;_T17A1=
# 3332 "parse.y"
Cyc_Position_loc_to_seg(_T17A0);_T17A2=yyyvsp;_T17A3=& _T17A2[0].v;_T17A4=(union Cyc_YYSTYPE*)_T17A3;_T17A5=Cyc_yyget_String_tok(_T17A4);_T17A6=Cyc_Parse_str2type(_T17A1,_T17A5);yyval=Cyc_YY45(_T17A6);goto _LL0;case 614: _T17A7=yyyvsp;_T17A8=& _T17A7[0].v;_T17A9=(union Cyc_YYSTYPE*)_T17A8;_T17AA=
# 3336 "parse.y"
Cyc_yyget_String_tok(_T17A9);_T17AB=yyyvsp;_T17AC=& _T17AB[3].v;_T17AD=(union Cyc_YYSTYPE*)_T17AC;_T17AE=Cyc_yyget_YY73(_T17AD);_T17AF=Cyc_BansheeIf_check_constraint(_T17AE);_T17B0=Cyc_BansheeIf_add_location(_T17AA,_T17AF);yyval=Cyc_YY73(_T17B0);goto _LL0;case 615: _T17B1=yyyvsp;_T17B2=& _T17B1[0].v;_T17B3=(union Cyc_YYSTYPE*)_T17B2;_T17B4=
# 3337 "parse.y"
Cyc_yyget_String_tok(_T17B3);_T17B5=yyyvsp;_T17B6=& _T17B5[3].v;_T17B7=(union Cyc_YYSTYPE*)_T17B6;_T17B8=Cyc_yyget_YY70(_T17B7);_T17B9=yyyvsp;_T17BA=& _T17B9[5].v;_T17BB=(union Cyc_YYSTYPE*)_T17BA;_T17BC=Cyc_yyget_YY45(_T17BB);_T17BD=yyyvsp;_T17BE=& _T17BD[7].v;_T17BF=(union Cyc_YYSTYPE*)_T17BE;_T17C0=Cyc_yyget_YY45(_T17BF);_T17C1=Cyc_Parse_comparison_constraint(_T17B8,_T17BC,_T17C0);_T17C2=Cyc_BansheeIf_add_location(_T17B4,_T17C1);yyval=Cyc_YY73(_T17C2);goto _LL0;case 616: _T17C3=yyyvsp;_T17C4=& _T17C3[0].v;_T17C5=(union Cyc_YYSTYPE*)_T17C4;_T17C6=
# 3338 "parse.y"
Cyc_yyget_String_tok(_T17C5);_T17C7=yyyvsp;_T17C8=& _T17C7[3].v;_T17C9=(union Cyc_YYSTYPE*)_T17C8;_T17CA=Cyc_yyget_YY45(_T17C9);_T17CB=yyyvsp;_T17CC=& _T17CB[5].v;_T17CD=(union Cyc_YYSTYPE*)_T17CC;_T17CE=Cyc_yyget_YY45(_T17CD);_T17CF=Cyc_BansheeIf_cond_equality_constraint(_T17CA,_T17CE);_T17D0=Cyc_BansheeIf_add_location(_T17C6,_T17CF);yyval=Cyc_YY73(_T17D0);goto _LL0;case 617: _T17D1=yyyvsp;_T17D2=& _T17D1[0].v;_T17D3=(union Cyc_YYSTYPE*)_T17D2;_T17D4=
# 3339 "parse.y"
Cyc_yyget_String_tok(_T17D3);_T17D5=yyyvsp;_T17D6=& _T17D5[3].v;_T17D7=(union Cyc_YYSTYPE*)_T17D6;_T17D8=Cyc_yyget_YY45(_T17D7);_T17D9=yyyvsp;_T17DA=& _T17D9[5].v;_T17DB=(union Cyc_YYSTYPE*)_T17DA;_T17DC=Cyc_yyget_YY45(_T17DB);_T17DD=Cyc_BansheeIf_equality_constraint(_T17D8,_T17DC);_T17DE=Cyc_BansheeIf_add_location(_T17D4,_T17DD);yyval=Cyc_YY73(_T17DE);goto _LL0;case 618: _T17DF=yyyvsp;_T17E0=& _T17DF[0].v;_T17E1=(union Cyc_YYSTYPE*)_T17E0;_T17E2=
# 3340 "parse.y"
Cyc_yyget_String_tok(_T17E1);_T17E3=yyyvsp;_T17E4=& _T17E3[3].v;_T17E5=(union Cyc_YYSTYPE*)_T17E4;_T17E6=Cyc_yyget_YY45(_T17E5);_T17E7=yyyvsp;_T17E8=& _T17E7[5].v;_T17E9=(union Cyc_YYSTYPE*)_T17E8;_T17EA=Cyc_yyget_YY45(_T17E9);_T17EB=Cyc_BansheeIf_inclusion_constraint(_T17E6,_T17EA);_T17EC=Cyc_BansheeIf_add_location(_T17E2,_T17EB);yyval=Cyc_YY73(_T17EC);goto _LL0;case 619: _T17ED=yyyvsp;_T17EE=& _T17ED[0].v;_T17EF=(union Cyc_YYSTYPE*)_T17EE;_T17F0=
# 3341 "parse.y"
Cyc_yyget_String_tok(_T17EF);_T17F1=yyyvsp;_T17F2=& _T17F1[3].v;_T17F3=(union Cyc_YYSTYPE*)_T17F2;_T17F4=Cyc_yyget_YY73(_T17F3);_T17F5=yyyvsp;_T17F6=& _T17F5[5].v;_T17F7=(union Cyc_YYSTYPE*)_T17F6;_T17F8=Cyc_yyget_YY73(_T17F7);_T17F9=Cyc_BansheeIf_implication_constraint(_T17F4,_T17F8);_T17FA=Cyc_BansheeIf_add_location(_T17F0,_T17F9);yyval=Cyc_YY73(_T17FA);goto _LL0;case 620: _T17FB=yyyvsp;_T17FC=& _T17FB[0].v;_T17FD=(union Cyc_YYSTYPE*)_T17FC;_T17FE=
# 3342 "parse.y"
Cyc_yyget_String_tok(_T17FD);_T17FF=yyyvsp;_T1800=& _T17FF[3].v;_T1801=(union Cyc_YYSTYPE*)_T1800;_T1802=Cyc_yyget_YY70(_T1801);_T1803=yyyvsp;_T1804=& _T1803[5].v;_T1805=(union Cyc_YYSTYPE*)_T1804;_T1806=Cyc_yyget_YY73(_T1805);_T1807=yyyvsp;_T1808=& _T1807[7].v;_T1809=(union Cyc_YYSTYPE*)_T1808;_T180A=Cyc_yyget_YY73(_T1809);_T180B=Cyc_Parse_composite_constraint(_T1802,_T1806,_T180A);_T180C=Cyc_BansheeIf_add_location(_T17FE,_T180B);yyval=Cyc_YY73(_T180C);goto _LL0;case 621:
# 3345
 yyval=Cyc_YY70(0U);goto _LL0;case 622:
# 3346 "parse.y"
 yyval=Cyc_YY70(1U);goto _LL0;case 623:
# 3347 "parse.y"
 yyval=Cyc_YY70(2U);goto _LL0;case 624:
# 3348 "parse.y"
 yyval=Cyc_YY70(3U);goto _LL0;case 625:
# 3349 "parse.y"
 yyval=Cyc_YY70(4U);goto _LL0;default: goto _LL0;}_LL0: _T180D=yylen;
# 375 "cycbison.simple"
yyvsp_offset=yyvsp_offset - _T180D;_T180E=yylen;
yyssp_offset=yyssp_offset - _T180E;_T180F=yyvs;
# 389 "cycbison.simple"
yyvsp_offset=yyvsp_offset + 1;_T1810=yyvsp_offset;_T1811=_check_fat_subscript(_T180F,sizeof(struct Cyc_Yystacktype),_T1810);_T1812=(struct Cyc_Yystacktype*)_T1811;(*_T1812).v=yyval;
# 392
if(yylen!=0)goto _TL2FD;_T1813=yyvs;_T1814=yyvsp_offset - 1;_T1815=
_fat_ptr_plus(_T1813,sizeof(struct Cyc_Yystacktype),_T1814);_T1816=_untag_fat_ptr_check_bound(_T1815,sizeof(struct Cyc_Yystacktype),2U);_T1817=_check_null(_T1816);{struct Cyc_Yystacktype*p=(struct Cyc_Yystacktype*)_T1817;_T1818=p;_T1819=yylloc;
_T1818[1].l.first_line=_T1819.first_line;_T181A=p;_T181B=yylloc;
_T181A[1].l.first_column=_T181B.first_column;_T181C=p;_T181D=p;_T181E=_T181D[0];_T181F=_T181E.l;
_T181C[1].l.last_line=_T181F.last_line;_T1820=p;_T1821=p;_T1822=_T1821[0];_T1823=_T1822.l;
_T1820[1].l.last_column=_T1823.last_column;}goto _TL2FE;
# 399
_TL2FD: _T1824=yyvs;_T1825=_T1824.curr;_T1826=(struct Cyc_Yystacktype*)_T1825;_T1827=yyvsp_offset;_T1828=yyvs;_T1829=yyvsp_offset + yylen;_T182A=_T1829 - 1;_T182B=_check_fat_subscript(_T1828,sizeof(struct Cyc_Yystacktype),_T182A);_T182C=(struct Cyc_Yystacktype*)_T182B;_T182D=*_T182C;_T182E=_T182D.l;_T1826[_T1827].l.last_line=_T182E.last_line;_T182F=yyvs;_T1830=_T182F.curr;_T1831=(struct Cyc_Yystacktype*)_T1830;_T1832=yyvsp_offset;_T1833=yyvs;_T1834=_T1833.curr;_T1835=(struct Cyc_Yystacktype*)_T1834;_T1836=yyvsp_offset + yylen;_T1837=_T1836 - 1;_T1838=_T1835[_T1837];_T1839=_T1838.l;
_T1831[_T1832].l.last_column=_T1839.last_column;_TL2FE: _T183A=Cyc_yyr1;_T183B=yyn;_T183C=_check_known_subscript_notnull(_T183A,626U,sizeof(short),_T183B);_T183D=(short*)_T183C;_T183E=*_T183D;
# 409
yyn=(int)_T183E;_T183F=Cyc_yypgoto;_T1840=yyn - 177;_T1841=_check_known_subscript_notnull(_T183F,188U,sizeof(short),_T1840);_T1842=(short*)_T1841;_T1843=*_T1842;_T1844=(int)_T1843;_T1845=yyss;_T1846=yyssp_offset;_T1847=_check_fat_subscript(_T1845,sizeof(short),_T1846);_T1848=(short*)_T1847;_T1849=*_T1848;_T184A=(int)_T1849;
# 411
yystate=_T1844 + _T184A;
if(yystate < 0)goto _TL2FF;if(yystate > 8407)goto _TL2FF;_T184B=Cyc_yycheck;_T184C=yystate;_T184D=_T184B[_T184C];_T184E=(int)_T184D;_T184F=yyss;_T1850=_T184F.curr;_T1851=(short*)_T1850;_T1852=yyssp_offset;_T1853=_T1851[_T1852];_T1854=(int)_T1853;if(_T184E!=_T1854)goto _TL2FF;_T1855=Cyc_yytable;_T1856=yystate;_T1857=_T1855[_T1856];
yystate=(int)_T1857;goto _TL300;
# 415
_TL2FF: _T1858=Cyc_yydefgoto;_T1859=yyn - 177;_T185A=_T1858[_T1859];yystate=(int)_T185A;_TL300: goto yynewstate;
# 419
yyerrlab:
# 421
 if(yyerrstatus!=0)goto _TL301;
# 424
yynerrs=yynerrs + 1;_T185B=Cyc_yypact;_T185C=yystate;_T185D=_check_known_subscript_notnull(_T185B,1270U,sizeof(short),_T185C);_T185E=(short*)_T185D;_T185F=*_T185E;
# 427
yyn=(int)_T185F;_T1860=yyn;_T1861=- 32768;
# 429
if(_T1860 <= _T1861)goto _TL303;if(yyn >= 8407)goto _TL303;{
# 431
int sze=0;
struct _fat_ptr msg;
int x;int count;
# 435
count=0;
# 437
if(yyn >= 0)goto _TL305;_T1862=- yyn;goto _TL306;_TL305: _T1862=0;_TL306: x=_T1862;
_TL30A: _T1863=x;_T1864=(unsigned)_T1863;_T1865=365U / sizeof(char*);
# 437
if(_T1864 < _T1865)goto _TL308;else{goto _TL309;}
# 439
_TL308: _T1866=Cyc_yycheck;_T1867=x + yyn;_T1868=_check_known_subscript_notnull(_T1866,8408U,sizeof(short),_T1867);_T1869=(short*)_T1868;_T186A=*_T1869;_T186B=(int)_T186A;_T186C=x;if(_T186B!=_T186C)goto _TL30B;_T186D=Cyc_yytname;_T186E=x;_T186F=_check_known_subscript_notnull(_T186D,365U,sizeof(struct _fat_ptr),_T186E);_T1870=(struct _fat_ptr*)_T186F;_T1871=*_T1870;_T1872=
Cyc_strlen(_T1871);_T1873=_T1872 + 15U;sze=sze + _T1873;count=count + 1;goto _TL30C;_TL30B: _TL30C:
# 438
 x=x + 1;goto _TL30A;_TL309: _T1875=sze + 15;_T1876=(unsigned)_T1875;{unsigned _T18B2=_T1876 + 1U;_T1878=yyregion;_T1879=_check_times(_T18B2,sizeof(char));{char*_T18B3=_region_malloc(_T1878,0U,_T1879);{unsigned _T18B4=_T18B2;unsigned i;i=0;_TL310: if(i < _T18B4)goto _TL30E;else{goto _TL30F;}_TL30E: _T187A=i;
# 441
_T18B3[_T187A]='\000';i=i + 1;goto _TL310;_TL30F: _T18B3[_T18B4]=0;}_T1877=(char*)_T18B3;}_T1874=_tag_fat(_T1877,sizeof(char),_T18B2);}msg=_T1874;_T187B=msg;_T187C=
_tag_fat("parse error",sizeof(char),12U);Cyc_strcpy(_T187B,_T187C);
# 444
if(count >= 5)goto _TL311;
# 446
count=0;
if(yyn >= 0)goto _TL313;_T187D=- yyn;goto _TL314;_TL313: _T187D=0;_TL314: x=_T187D;
_TL318: _T187E=x;_T187F=(unsigned)_T187E;_T1880=365U / sizeof(char*);
# 447
if(_T187F < _T1880)goto _TL316;else{goto _TL317;}
# 449
_TL316: _T1881=Cyc_yycheck;_T1882=x + yyn;_T1883=_check_known_subscript_notnull(_T1881,8408U,sizeof(short),_T1882);_T1884=(short*)_T1883;_T1885=*_T1884;_T1886=(int)_T1885;_T1887=x;if(_T1886!=_T1887)goto _TL319;_T1888=msg;
# 451
if(count!=0)goto _TL31B;_T188A=
_tag_fat(", expecting `",sizeof(char),14U);_T1889=_T188A;goto _TL31C;_TL31B: _T188B=
_tag_fat(" or `",sizeof(char),6U);_T1889=_T188B;_TL31C:
# 451
 Cyc_strcat(_T1888,_T1889);_T188C=msg;_T188D=Cyc_yytname;_T188E=x;_T188F=_check_known_subscript_notnull(_T188D,365U,sizeof(struct _fat_ptr),_T188E);_T1890=(struct _fat_ptr*)_T188F;_T1891=*_T1890;
# 454
Cyc_strcat(_T188C,_T1891);_T1892=msg;_T1893=
_tag_fat("'",sizeof(char),2U);Cyc_strcat(_T1892,_T1893);
count=count + 1;goto _TL31A;_TL319: _TL31A:
# 448
 x=x + 1;goto _TL318;_TL317: goto _TL312;_TL311: _TL312:
# 459
 Cyc_yyerror(msg,yystate,yychar);}goto _TL304;
# 463
_TL303: _T1894=_tag_fat("parse error",sizeof(char),12U);_T1895=yystate;_T1896=yychar;Cyc_yyerror(_T1894,_T1895,_T1896);_TL304: goto _TL302;_TL301: _TL302: goto yyerrlab1;
# 467
yyerrlab1:
# 469
 if(yyerrstatus!=3)goto _TL31D;
# 474
if(yychar!=0)goto _TL31F;{int _T18B2=1;_npop_handler(0);return _T18B2;}_TL31F:
# 483
 yychar=- 2;goto _TL31E;_TL31D: _TL31E:
# 489
 yyerrstatus=3;goto yyerrhandle;
# 493
yyerrdefault:
# 503 "cycbison.simple"
 yyerrpop:
# 505
 if(yyssp_offset!=0)goto _TL321;{int _T18B2=1;_npop_handler(0);return _T18B2;}_TL321:
 yyvsp_offset=yyvsp_offset + -1;_T1897=yyss;
yyssp_offset=yyssp_offset + -1;_T1898=yyssp_offset;_T1899=_check_fat_subscript(_T1897,sizeof(short),_T1898);_T189A=(short*)_T1899;_T189B=*_T189A;yystate=(int)_T189B;
# 521 "cycbison.simple"
yyerrhandle: _T189C=Cyc_yypact;_T189D=yystate;_T189E=_check_known_subscript_notnull(_T189C,1270U,sizeof(short),_T189D);_T189F=(short*)_T189E;_T18A0=*_T189F;
yyn=(int)_T18A0;_T18A1=yyn;_T18A2=- 32768;
if(_T18A1!=_T18A2)goto _TL323;goto yyerrdefault;_TL323:
# 525
 yyn=yyn + 1;
if(yyn < 0)goto _TL327;else{goto _TL329;}_TL329: if(yyn > 8407)goto _TL327;else{goto _TL328;}_TL328: _T18A3=Cyc_yycheck;_T18A4=yyn;_T18A5=_T18A3[_T18A4];_T18A6=(int)_T18A5;if(_T18A6!=1)goto _TL327;else{goto _TL325;}_TL327: goto yyerrdefault;_TL325: _T18A7=Cyc_yytable;_T18A8=yyn;_T18A9=_T18A7[_T18A8];
# 528
yyn=(int)_T18A9;
if(yyn >= 0)goto _TL32A;_T18AA=yyn;_T18AB=- 32768;
# 531
if(_T18AA!=_T18AB)goto _TL32C;goto yyerrpop;_TL32C:
 yyn=- yyn;goto yyreduce;
# 535
_TL32A: if(yyn!=0)goto _TL32E;goto yyerrpop;_TL32E:
# 537
 if(yyn!=1269)goto _TL330;{int _T18B2=0;_npop_handler(0);return _T18B2;}_TL330: _T18AC=yyvs;
# 546
yyvsp_offset=yyvsp_offset + 1;_T18AD=yyvsp_offset;_T18AE=_check_fat_subscript(_T18AC,sizeof(struct Cyc_Yystacktype),_T18AD);_T18AF=(struct Cyc_Yystacktype*)_T18AE;{struct Cyc_Yystacktype _T18B2;_T18B2.v=yylval;_T18B2.l=yylloc;_T18B0=_T18B2;}*_T18AF=_T18B0;goto yynewstate;}}}}_pop_region();}
# 3353 "parse.y"
void Cyc_yyprint(int i,union Cyc_YYSTYPE v){union Cyc_YYSTYPE _T0;struct _union_YYSTYPE_Stmt_tok _T1;unsigned _T2;union Cyc_YYSTYPE _T3;struct _union_YYSTYPE_Int_tok _T4;struct Cyc_String_pa_PrintArg_struct _T5;struct Cyc___cycFILE*_T6;struct _fat_ptr _T7;struct _fat_ptr _T8;union Cyc_YYSTYPE _T9;struct _union_YYSTYPE_Char_tok _TA;struct Cyc_Int_pa_PrintArg_struct _TB;char _TC;int _TD;struct Cyc___cycFILE*_TE;struct _fat_ptr _TF;struct _fat_ptr _T10;union Cyc_YYSTYPE _T11;struct _union_YYSTYPE_String_tok _T12;struct Cyc_String_pa_PrintArg_struct _T13;struct Cyc___cycFILE*_T14;struct _fat_ptr _T15;struct _fat_ptr _T16;union Cyc_YYSTYPE _T17;struct _union_YYSTYPE_QualId_tok _T18;struct Cyc_String_pa_PrintArg_struct _T19;struct Cyc___cycFILE*_T1A;struct _fat_ptr _T1B;struct _fat_ptr _T1C;union Cyc_YYSTYPE _T1D;struct _union_YYSTYPE_Exp_tok _T1E;struct Cyc_String_pa_PrintArg_struct _T1F;struct Cyc___cycFILE*_T20;struct _fat_ptr _T21;struct _fat_ptr _T22;union Cyc_YYSTYPE _T23;struct _union_YYSTYPE_Stmt_tok _T24;struct Cyc_String_pa_PrintArg_struct _T25;struct Cyc___cycFILE*_T26;struct _fat_ptr _T27;struct _fat_ptr _T28;struct Cyc___cycFILE*_T29;struct _fat_ptr _T2A;struct _fat_ptr _T2B;struct Cyc_Absyn_Stmt*_T2C;struct Cyc_Absyn_Exp*_T2D;struct _tuple0*_T2E;struct _fat_ptr _T2F;char _T30;union Cyc_Absyn_Cnst _T31;_T0=v;_T1=_T0.Stmt_tok;_T2=_T1.tag;switch(_T2){case 2: _T3=v;_T4=_T3.Int_tok;_T31=_T4.val;{union Cyc_Absyn_Cnst c=_T31;{struct Cyc_String_pa_PrintArg_struct _T32;_T32.tag=0;
# 3355
_T32.f1=Cyc_Absynpp_cnst2string(c);_T5=_T32;}{struct Cyc_String_pa_PrintArg_struct _T32=_T5;void*_T33[1];_T33[0]=& _T32;_T6=Cyc_stderr;_T7=_tag_fat("%s",sizeof(char),3U);_T8=_tag_fat(_T33,sizeof(void*),1);Cyc_fprintf(_T6,_T7,_T8);}goto _LL0;}case 3: _T9=v;_TA=_T9.Char_tok;_T30=_TA.val;{char c=_T30;{struct Cyc_Int_pa_PrintArg_struct _T32;_T32.tag=1;_TC=c;_TD=(int)_TC;
_T32.f1=(unsigned long)_TD;_TB=_T32;}{struct Cyc_Int_pa_PrintArg_struct _T32=_TB;void*_T33[1];_T33[0]=& _T32;_TE=Cyc_stderr;_TF=_tag_fat("%c",sizeof(char),3U);_T10=_tag_fat(_T33,sizeof(void*),1);Cyc_fprintf(_TE,_TF,_T10);}goto _LL0;}case 4: _T11=v;_T12=_T11.String_tok;_T2F=_T12.val;{struct _fat_ptr s=_T2F;{struct Cyc_String_pa_PrintArg_struct _T32;_T32.tag=0;
_T32.f1=s;_T13=_T32;}{struct Cyc_String_pa_PrintArg_struct _T32=_T13;void*_T33[1];_T33[0]=& _T32;_T14=Cyc_stderr;_T15=_tag_fat("\"%s\"",sizeof(char),5U);_T16=_tag_fat(_T33,sizeof(void*),1);Cyc_fprintf(_T14,_T15,_T16);}goto _LL0;}case 5: _T17=v;_T18=_T17.QualId_tok;_T2E=_T18.val;{struct _tuple0*q=_T2E;{struct Cyc_String_pa_PrintArg_struct _T32;_T32.tag=0;
_T32.f1=Cyc_Absynpp_qvar2string(q);_T19=_T32;}{struct Cyc_String_pa_PrintArg_struct _T32=_T19;void*_T33[1];_T33[0]=& _T32;_T1A=Cyc_stderr;_T1B=_tag_fat("%s",sizeof(char),3U);_T1C=_tag_fat(_T33,sizeof(void*),1);Cyc_fprintf(_T1A,_T1B,_T1C);}goto _LL0;}case 6: _T1D=v;_T1E=_T1D.Exp_tok;_T2D=_T1E.val;{struct Cyc_Absyn_Exp*e=_T2D;{struct Cyc_String_pa_PrintArg_struct _T32;_T32.tag=0;
_T32.f1=Cyc_Absynpp_exp2string(e);_T1F=_T32;}{struct Cyc_String_pa_PrintArg_struct _T32=_T1F;void*_T33[1];_T33[0]=& _T32;_T20=Cyc_stderr;_T21=_tag_fat("%s",sizeof(char),3U);_T22=_tag_fat(_T33,sizeof(void*),1);Cyc_fprintf(_T20,_T21,_T22);}goto _LL0;}case 7: _T23=v;_T24=_T23.Stmt_tok;_T2C=_T24.val;{struct Cyc_Absyn_Stmt*s=_T2C;{struct Cyc_String_pa_PrintArg_struct _T32;_T32.tag=0;
_T32.f1=Cyc_Absynpp_stmt2string(s);_T25=_T32;}{struct Cyc_String_pa_PrintArg_struct _T32=_T25;void*_T33[1];_T33[0]=& _T32;_T26=Cyc_stderr;_T27=_tag_fat("%s",sizeof(char),3U);_T28=_tag_fat(_T33,sizeof(void*),1);Cyc_fprintf(_T26,_T27,_T28);}goto _LL0;}default: _T29=Cyc_stderr;_T2A=
_tag_fat("?",sizeof(char),2U);_T2B=_tag_fat(0U,sizeof(void*),0);Cyc_fprintf(_T29,_T2A,_T2B);goto _LL0;}_LL0:;}
# 3365
struct _fat_ptr Cyc_token2string(int token){struct _fat_ptr _T0;struct _fat_ptr _T1;struct _fat_ptr _T2;int _T3;short*_T4;int _T5;short _T6;int _T7;unsigned _T8;struct _fat_ptr*_T9;int _TA;struct _fat_ptr _TB;struct _fat_ptr _TC;
if(token > 0)goto _TL333;_T0=
_tag_fat("end-of-file",sizeof(char),12U);return _T0;_TL333:
 if(token!=389)goto _TL335;_T1=Cyc_Lex_token_string;
return _T1;_TL335:
 if(token!=398)goto _TL337;_T2=
Cyc_Absynpp_qvar2string(Cyc_Lex_token_qvar);return _T2;_TL337:
 if(token <= 0)goto _TL339;if(token > 402)goto _TL339;_T4=Cyc_yytranslate;_T5=token;_T6=_T4[_T5];_T3=(int)_T6;goto _TL33A;_TL339: _T3=365;_TL33A: {int z=_T3;_T7=z;_T8=(unsigned)_T7;
if(_T8 >= 365U)goto _TL33B;_T9=Cyc_yytname;_TA=z;_TB=_T9[_TA];
return _TB;_TL33B: _TC=
_tag_fat(0,0,0);return _TC;}}
# 3379
struct Cyc_List_List*Cyc_Parse_parse_file(struct Cyc___cycFILE*f){struct _RegionHandle*_T0;struct Cyc_Lexing_lexbuf*_T1;
Cyc_Parse_parse_result=0;{struct _RegionHandle _T2=_new_region(0U,"yyr");struct _RegionHandle*yyr=& _T2;_push_region(yyr);_T0=yyr;_T1=
# 3382
Cyc_Lexing_from_file(f);Cyc_yyparse(_T0,_T1);{struct Cyc_List_List*_T3=Cyc_Parse_parse_result;_npop_handler(0);return _T3;}_pop_region();}}
# 3386
struct Cyc_List_List*Cyc_Parse_parse_constraint_file(struct Cyc___cycFILE*f){struct _RegionHandle*_T0;struct Cyc_Lexing_lexbuf*_T1;
Cyc_Parse_constraint_graph=0;{struct _RegionHandle _T2=_new_region(0U,"yyr");struct _RegionHandle*yyr=& _T2;_push_region(yyr);_T0=yyr;_T1=
# 3389
Cyc_Lexing_from_file(f);Cyc_yyparse(_T0,_T1);{struct Cyc_List_List*_T3=Cyc_Parse_constraint_graph;_npop_handler(0);return _T3;}_pop_region();}}
