// This is a C header file to be used by the output of the Cyclone
// to C translator.  The corresponding definitions are in
// the file lib/runtime_cyc.c

#ifndef _CYC_INCLUDE_H_
#define _CYC_INCLUDE_H_

#include <setjmp.h>

#ifdef NO_CYC_PREFIX
#define ADD_PREFIX(x) x
#else
#define ADD_PREFIX(x) Cyc_##x
#endif

#ifndef offsetof
// should be size_t, but int is fine.
#define offsetof(t,n) ((int)(&(((t *)0)->n)))
#endif

//// Tagged arrays
struct _tagged_arr { 
  unsigned char *curr; 
  unsigned char *base; 
  unsigned char *last_plus_one; 
};
struct _tagged_string {  // delete after bootstrapping
  unsigned char *curr; 
  unsigned char *base; 
  unsigned char *last_plus_one; 
};

//// Discriminated Unions
struct _xtunion_struct { char *tag; };

// The runtime maintains a stack that contains either _handler_cons
// structs or _RegionHandle structs.  The tag is 0 for a handler_cons
// and 1 for a region handle.  
struct _RuntimeStack {
  int tag; // 0 for an exception handler, 1 for a region handle
  struct _RuntimeStack *next;
};

//// Regions
struct _RegionPage {
  struct _RegionPage *next;
#ifdef CYC_REGION_PROFILE
  unsigned int total_bytes;
  unsigned int free_bytes;
#endif
  char data[0];
};

struct _RegionHandle {
  struct _RuntimeStack s;
  struct _RegionPage *curr;
  char               *offset;
  char               *last_plus_one;
};

extern struct _RegionHandle _new_region();
extern void * _region_malloc(struct _RegionHandle *, unsigned int);
extern void   _free_region(struct _RegionHandle *);

//// Exceptions 
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
extern int _throw(void * e);
#endif

extern struct _xtunion_struct *_exn_thrown;

//// Built-in Exceptions
extern struct _xtunion_struct ADD_PREFIX(Null_Exception_struct);
extern struct _xtunion_struct * ADD_PREFIX(Null_Exception);
extern struct _xtunion_struct ADD_PREFIX(Array_bounds_struct);
extern struct _xtunion_struct * ADD_PREFIX(Array_bounds);
extern struct _xtunion_struct ADD_PREFIX(Match_Exception_struct);
extern struct _xtunion_struct * ADD_PREFIX(Match_Exception);
extern struct _xtunion_struct ADD_PREFIX(Bad_alloc_struct);
extern struct _xtunion_struct * ADD_PREFIX(Bad_alloc);

//// Built-in Run-time Checks and company
static inline 
void * _check_null(void * ptr) {
  if(!ptr)
    _throw_null();
  return ptr;
}
static inline 
char * _check_known_subscript_null(void * ptr, unsigned bound, 
				   unsigned elt_sz, unsigned index) {
  if(!ptr || index >= bound)
    _throw_null();
  return ((char *)ptr) + elt_sz*index;
}
static inline 
unsigned _check_known_subscript_notnull(unsigned bound, unsigned index) {
  if(index >= bound)
    _throw_arraybounds();
  return index;
}
static inline 
char * _check_unknown_subscript(struct _tagged_arr arr,
				unsigned elt_sz, unsigned index) {
  // caller casts first argument and result
  // multiplication looks inefficient, but C compiler has to insert it otherwise
  // by inlining, it should be able to avoid actual multiplication
  unsigned char * ans = arr.curr + elt_sz * index;
  // might be faster not to distinguish these two cases. definitely would be
  // smaller.
  if(!arr.base) 
    _throw_null();
  if(ans < arr.base || ans >= arr.last_plus_one)
    _throw_arraybounds();
  return ans;
}
static inline 
struct _tagged_arr _tag_arr(const void * curr, 
			    unsigned elt_sz, unsigned num_elts) {
  // beware the gcc bug, can happen with *temp = _tag_arr(...) in weird places!
  struct _tagged_arr ans;
  ans.base = (void *)curr;
  ans.curr = (void *)curr;
  ans.last_plus_one = ((char *)curr) + elt_sz * num_elts;
  return ans;
}
static inline
struct _tagged_arr * _init_tag_arr(struct _tagged_arr * arr_ptr, void * arr, 
				   unsigned elt_sz, unsigned num_elts) {
  // we use this to (hopefully) avoid the gcc bug
  arr_ptr->base = arr_ptr->curr = arr;
  arr_ptr->last_plus_one = ((char *)arr) + elt_sz * num_elts;
  return arr_ptr;
}

static inline
char * _untag_arr(struct _tagged_arr arr, unsigned elt_sz, unsigned num_elts) {
  // Note: if arr is "null" base and curr should both be null, so this
  //       is correct (caller checks for null if untagging to @ type)
  // base may not be null if you use t ? pointer subtraction to get 0 -- oh well
  if(arr.curr < arr.base || arr.curr + elt_sz * num_elts > arr.last_plus_one)
    _throw_arraybounds();
  return arr.curr;
}
static inline 
unsigned _get_arr_size(struct _tagged_arr arr, unsigned elt_sz) {
  return (arr.last_plus_one - arr.curr) / elt_sz;
}
static inline
struct _tagged_arr _tagged_arr_plus(struct _tagged_arr arr, unsigned elt_sz,
				    int change) {
  struct _tagged_arr ans = arr;
  ans.curr += ((int)elt_sz)*change;
  return ans;
}
static inline
struct _tagged_arr _tagged_arr_inplace_plus(struct _tagged_arr * arr_ptr,
					    unsigned elt_sz, int change) {
  arr_ptr->curr += ((int)elt_sz)*change;
  return *arr_ptr;
}
static inline
struct _tagged_arr _tagged_arr_inplace_plus_post(struct _tagged_arr * arr_ptr,
						 unsigned elt_sz, int change) {
  struct _tagged_arr ans = *arr_ptr;
  arr_ptr->curr += ((int)elt_sz)*change;
  return ans;
}
				  
//// Allocation
extern void * GC_malloc(int);
extern void * GC_malloc_atomic(int);

static inline void * _cycalloc(int n) {
  void * ans = GC_malloc(n);
  if(!ans)
    _throw_badalloc();
  return ans;
}
static inline void * _cycalloc_atomic(int n) {
  void * ans = GC_malloc(n);
  if(!ans)
    _throw_badalloc();
  return ans;
}
#define MAX_MALLOC_SIZE (1 << 28)
static inline unsigned int _check_times(unsigned int x, unsigned int y) {
  unsigned long long whole_ans = 
    ((unsigned long long)x)*((unsigned long long)y);
  unsigned int word_ans = (unsigned int)whole_ans;
  if(word_ans < whole_ans || word_ans > MAX_MALLOC_SIZE)
    _throw_badalloc();
  return word_ans;
}

#ifdef CYC_REGION_PROFILE
extern void * _profile_GC_malloc(int,char *file,int lineno);
extern void * _profile_GC_malloc_atomic(int,char *file,int lineno);
extern void * _profile_region_malloc(struct _RegionHandle *, unsigned int,
                                     char *file,int lineno);
#define _cycalloc(n) _profile_cycalloc(n,__FUNCTION__,__LINE__)
#define _cycalloc_atomic(n) _profile_cycalloc_atomic(n,__FUNCTION__,__LINE__)
#define _region_malloc(rh,n) _profile_region_malloc(rh,n,__FUNCTION__,__LINE__)
#endif

#endif
 extern void exit( int); extern void* abort(); struct Cyc_Core_Opt{ void* v; } ;
extern unsigned char Cyc_Core_Invalid_argument[ 21u]; struct Cyc_Core_Invalid_argument_struct{
unsigned char* tag; struct _tagged_arr f1; } ; extern unsigned char Cyc_Core_Failure[
12u]; struct Cyc_Core_Failure_struct{ unsigned char* tag; struct _tagged_arr f1;
} ; extern unsigned char Cyc_Core_Impossible[ 15u]; struct Cyc_Core_Impossible_struct{
unsigned char* tag; struct _tagged_arr f1; } ; extern unsigned char Cyc_Core_Not_found[
14u]; extern unsigned char Cyc_Core_Unreachable[ 16u]; struct Cyc_Core_Unreachable_struct{
unsigned char* tag; struct _tagged_arr f1; } ; extern unsigned char*
string_to_Cstring( struct _tagged_arr); extern unsigned char* underlying_Cstring(
struct _tagged_arr); extern struct _tagged_arr Cstring_to_string( unsigned char*);
extern struct _tagged_arr wrap_Cstring_as_string( unsigned char*, unsigned int);
extern struct _tagged_arr ntCsl_to_ntsl( unsigned char**); struct Cyc_Std___sFILE;
extern struct Cyc_Std___sFILE* Cyc_Std_stdout; extern int Cyc_Std_fputc( int __c,
struct Cyc_Std___sFILE* __stream); extern unsigned char Cyc_Std_FileCloseError[
19u]; extern unsigned char Cyc_Std_FileOpenError[ 18u]; struct Cyc_Std_FileOpenError_struct{
unsigned char* tag; struct _tagged_arr f1; } ; extern int Cyc_Std_file_string_write(
struct Cyc_Std___sFILE* fd, struct _tagged_arr src, int src_offset, int
max_count); static const int Cyc_Std_String_pa= 0; struct Cyc_Std_String_pa_struct{
int tag; struct _tagged_arr f1; } ; static const int Cyc_Std_Int_pa= 1; struct
Cyc_Std_Int_pa_struct{ int tag; unsigned int f1; } ; static const int Cyc_Std_Double_pa=
2; struct Cyc_Std_Double_pa_struct{ int tag; double f1; } ; static const int Cyc_Std_ShortPtr_pa=
3; struct Cyc_Std_ShortPtr_pa_struct{ int tag; short* f1; } ; static const int
Cyc_Std_IntPtr_pa= 4; struct Cyc_Std_IntPtr_pa_struct{ int tag; unsigned int* f1;
} ; extern int Cyc_Std_fprintf( struct Cyc_Std___sFILE*, struct _tagged_arr fmt,
struct _tagged_arr); extern struct _tagged_arr Cyc_Std_aprintf( struct
_tagged_arr fmt, struct _tagged_arr); static const int Cyc_Std_ShortPtr_sa= 0;
struct Cyc_Std_ShortPtr_sa_struct{ int tag; short* f1; } ; static const int Cyc_Std_UShortPtr_sa=
1; struct Cyc_Std_UShortPtr_sa_struct{ int tag; unsigned short* f1; } ; static
const int Cyc_Std_IntPtr_sa= 2; struct Cyc_Std_IntPtr_sa_struct{ int tag; int*
f1; } ; static const int Cyc_Std_UIntPtr_sa= 3; struct Cyc_Std_UIntPtr_sa_struct{
int tag; unsigned int* f1; } ; static const int Cyc_Std_StringPtr_sa= 4; struct
Cyc_Std_StringPtr_sa_struct{ int tag; struct _tagged_arr f1; } ; static const
int Cyc_Std_DoublePtr_sa= 5; struct Cyc_Std_DoublePtr_sa_struct{ int tag; double*
f1; } ; static const int Cyc_Std_FloatPtr_sa= 6; struct Cyc_Std_FloatPtr_sa_struct{
int tag; float* f1; } ; struct Cyc_List_List{ void* hd; struct Cyc_List_List* tl;
} ; extern int Cyc_List_length( struct Cyc_List_List* x); extern struct Cyc_List_List*
Cyc_List_map( void*(* f)( void*), struct Cyc_List_List* x); extern unsigned char
Cyc_List_List_mismatch[ 18u]; extern struct Cyc_List_List* Cyc_List_imp_rev(
struct Cyc_List_List* x); extern struct Cyc_List_List* Cyc_List_imp_append(
struct Cyc_List_List* x, struct Cyc_List_List* y); extern unsigned char Cyc_List_Nth[
8u]; struct Cyc_Lineno_Pos{ struct _tagged_arr logical_file; struct _tagged_arr
line; int line_no; int col; } ; extern unsigned char Cyc_Position_Exit[ 9u];
struct Cyc_Position_Segment; static const int Cyc_Position_Lex= 0; static const
int Cyc_Position_Parse= 1; static const int Cyc_Position_Elab= 2; struct Cyc_Position_Error{
struct _tagged_arr source; struct Cyc_Position_Segment* seg; void* kind; struct
_tagged_arr desc; } ; extern unsigned char Cyc_Position_Nocontext[ 14u]; static
const int Cyc_Absyn_Loc_n= 0; static const int Cyc_Absyn_Rel_n= 0; struct Cyc_Absyn_Rel_n_struct{
int tag; struct Cyc_List_List* f1; } ; static const int Cyc_Absyn_Abs_n= 1;
struct Cyc_Absyn_Abs_n_struct{ int tag; struct Cyc_List_List* f1; } ; struct
_tuple0{ void* f1; struct _tagged_arr* f2; } ; struct Cyc_Absyn_Conref; static
const int Cyc_Absyn_Static= 0; static const int Cyc_Absyn_Abstract= 1; static
const int Cyc_Absyn_Public= 2; static const int Cyc_Absyn_Extern= 3; static
const int Cyc_Absyn_ExternC= 4; struct Cyc_Absyn_Tqual{ int q_const: 1; int
q_volatile: 1; int q_restrict: 1; } ; static const int Cyc_Absyn_B1= 0; static
const int Cyc_Absyn_B2= 1; static const int Cyc_Absyn_B4= 2; static const int
Cyc_Absyn_B8= 3; static const int Cyc_Absyn_AnyKind= 0; static const int Cyc_Absyn_MemKind=
1; static const int Cyc_Absyn_BoxKind= 2; static const int Cyc_Absyn_RgnKind= 3;
static const int Cyc_Absyn_EffKind= 4; static const int Cyc_Absyn_Signed= 0;
static const int Cyc_Absyn_Unsigned= 1; struct Cyc_Absyn_Conref{ void* v; } ;
static const int Cyc_Absyn_Eq_constr= 0; struct Cyc_Absyn_Eq_constr_struct{ int
tag; void* f1; } ; static const int Cyc_Absyn_Forward_constr= 1; struct Cyc_Absyn_Forward_constr_struct{
int tag; struct Cyc_Absyn_Conref* f1; } ; static const int Cyc_Absyn_No_constr=
0; struct Cyc_Absyn_Tvar{ struct _tagged_arr* name; int* identity; struct Cyc_Absyn_Conref*
kind; } ; static const int Cyc_Absyn_Unknown_b= 0; static const int Cyc_Absyn_Upper_b=
0; struct Cyc_Absyn_Upper_b_struct{ int tag; struct Cyc_Absyn_Exp* f1; } ;
struct Cyc_Absyn_PtrInfo{ void* elt_typ; void* rgn_typ; struct Cyc_Absyn_Conref*
nullable; struct Cyc_Absyn_Tqual tq; struct Cyc_Absyn_Conref* bounds; } ; struct
Cyc_Absyn_VarargInfo{ struct Cyc_Core_Opt* name; struct Cyc_Absyn_Tqual tq; void*
type; int inject; } ; struct Cyc_Absyn_FnInfo{ struct Cyc_List_List* tvars;
struct Cyc_Core_Opt* effect; void* ret_typ; struct Cyc_List_List* args; int
c_varargs; struct Cyc_Absyn_VarargInfo* cyc_varargs; struct Cyc_List_List*
rgn_po; struct Cyc_List_List* attributes; } ; struct Cyc_Absyn_UnknownTunionInfo{
struct _tuple0* name; int is_xtunion; } ; static const int Cyc_Absyn_UnknownTunion=
0; struct Cyc_Absyn_UnknownTunion_struct{ int tag; struct Cyc_Absyn_UnknownTunionInfo
f1; } ; static const int Cyc_Absyn_KnownTunion= 1; struct Cyc_Absyn_KnownTunion_struct{
int tag; struct Cyc_Absyn_Tuniondecl** f1; } ; struct Cyc_Absyn_TunionInfo{ void*
tunion_info; struct Cyc_List_List* targs; void* rgn; } ; struct Cyc_Absyn_UnknownTunionFieldInfo{
struct _tuple0* tunion_name; struct _tuple0* field_name; int is_xtunion; } ;
static const int Cyc_Absyn_UnknownTunionfield= 0; struct Cyc_Absyn_UnknownTunionfield_struct{
int tag; struct Cyc_Absyn_UnknownTunionFieldInfo f1; } ; static const int Cyc_Absyn_KnownTunionfield=
1; struct Cyc_Absyn_KnownTunionfield_struct{ int tag; struct Cyc_Absyn_Tuniondecl*
f1; struct Cyc_Absyn_Tunionfield* f2; } ; struct Cyc_Absyn_TunionFieldInfo{ void*
field_info; struct Cyc_List_List* targs; } ; static const int Cyc_Absyn_VoidType=
0; static const int Cyc_Absyn_Evar= 0; struct Cyc_Absyn_Evar_struct{ int tag;
struct Cyc_Core_Opt* f1; struct Cyc_Core_Opt* f2; int f3; struct Cyc_Core_Opt*
f4; } ; static const int Cyc_Absyn_VarType= 1; struct Cyc_Absyn_VarType_struct{
int tag; struct Cyc_Absyn_Tvar* f1; } ; static const int Cyc_Absyn_TunionType= 2;
struct Cyc_Absyn_TunionType_struct{ int tag; struct Cyc_Absyn_TunionInfo f1; } ;
static const int Cyc_Absyn_TunionFieldType= 3; struct Cyc_Absyn_TunionFieldType_struct{
int tag; struct Cyc_Absyn_TunionFieldInfo f1; } ; static const int Cyc_Absyn_PointerType=
4; struct Cyc_Absyn_PointerType_struct{ int tag; struct Cyc_Absyn_PtrInfo f1; }
; static const int Cyc_Absyn_IntType= 5; struct Cyc_Absyn_IntType_struct{ int
tag; void* f1; void* f2; } ; static const int Cyc_Absyn_FloatType= 1; static
const int Cyc_Absyn_DoubleType= 2; static const int Cyc_Absyn_ArrayType= 6;
struct Cyc_Absyn_ArrayType_struct{ int tag; void* f1; struct Cyc_Absyn_Tqual f2;
struct Cyc_Absyn_Exp* f3; } ; static const int Cyc_Absyn_FnType= 7; struct Cyc_Absyn_FnType_struct{
int tag; struct Cyc_Absyn_FnInfo f1; } ; static const int Cyc_Absyn_TupleType= 8;
struct Cyc_Absyn_TupleType_struct{ int tag; struct Cyc_List_List* f1; } ; static
const int Cyc_Absyn_StructType= 9; struct Cyc_Absyn_StructType_struct{ int tag;
struct _tuple0* f1; struct Cyc_List_List* f2; struct Cyc_Absyn_Structdecl** f3;
} ; static const int Cyc_Absyn_UnionType= 10; struct Cyc_Absyn_UnionType_struct{
int tag; struct _tuple0* f1; struct Cyc_List_List* f2; struct Cyc_Absyn_Uniondecl**
f3; } ; static const int Cyc_Absyn_EnumType= 11; struct Cyc_Absyn_EnumType_struct{
int tag; struct _tuple0* f1; struct Cyc_Absyn_Enumdecl* f2; } ; static const int
Cyc_Absyn_AnonStructType= 12; struct Cyc_Absyn_AnonStructType_struct{ int tag;
struct Cyc_List_List* f1; } ; static const int Cyc_Absyn_AnonUnionType= 13;
struct Cyc_Absyn_AnonUnionType_struct{ int tag; struct Cyc_List_List* f1; } ;
static const int Cyc_Absyn_AnonEnumType= 14; struct Cyc_Absyn_AnonEnumType_struct{
int tag; struct Cyc_List_List* f1; } ; static const int Cyc_Absyn_RgnHandleType=
15; struct Cyc_Absyn_RgnHandleType_struct{ int tag; void* f1; } ; static const
int Cyc_Absyn_TypedefType= 16; struct Cyc_Absyn_TypedefType_struct{ int tag;
struct _tuple0* f1; struct Cyc_List_List* f2; struct Cyc_Core_Opt* f3; } ;
static const int Cyc_Absyn_HeapRgn= 3; static const int Cyc_Absyn_AccessEff= 17;
struct Cyc_Absyn_AccessEff_struct{ int tag; void* f1; } ; static const int Cyc_Absyn_JoinEff=
18; struct Cyc_Absyn_JoinEff_struct{ int tag; struct Cyc_List_List* f1; } ;
static const int Cyc_Absyn_RgnsEff= 19; struct Cyc_Absyn_RgnsEff_struct{ int tag;
void* f1; } ; static const int Cyc_Absyn_NoTypes= 0; struct Cyc_Absyn_NoTypes_struct{
int tag; struct Cyc_List_List* f1; struct Cyc_Position_Segment* f2; } ; static
const int Cyc_Absyn_WithTypes= 1; struct Cyc_Absyn_WithTypes_struct{ int tag;
struct Cyc_List_List* f1; int f2; struct Cyc_Absyn_VarargInfo* f3; struct Cyc_Core_Opt*
f4; struct Cyc_List_List* f5; } ; static const int Cyc_Absyn_NonNullable_ps= 0;
struct Cyc_Absyn_NonNullable_ps_struct{ int tag; struct Cyc_Absyn_Exp* f1; } ;
static const int Cyc_Absyn_Nullable_ps= 1; struct Cyc_Absyn_Nullable_ps_struct{
int tag; struct Cyc_Absyn_Exp* f1; } ; static const int Cyc_Absyn_TaggedArray_ps=
0; static const int Cyc_Absyn_Printf_ft= 0; static const int Cyc_Absyn_Scanf_ft=
1; static const int Cyc_Absyn_Regparm_att= 0; struct Cyc_Absyn_Regparm_att_struct{
int tag; int f1; } ; static const int Cyc_Absyn_Stdcall_att= 0; static const int
Cyc_Absyn_Cdecl_att= 1; static const int Cyc_Absyn_Fastcall_att= 2; static const
int Cyc_Absyn_Noreturn_att= 3; static const int Cyc_Absyn_Const_att= 4; static
const int Cyc_Absyn_Aligned_att= 1; struct Cyc_Absyn_Aligned_att_struct{ int tag;
int f1; } ; static const int Cyc_Absyn_Packed_att= 5; static const int Cyc_Absyn_Section_att=
2; struct Cyc_Absyn_Section_att_struct{ int tag; struct _tagged_arr f1; } ;
static const int Cyc_Absyn_Nocommon_att= 6; static const int Cyc_Absyn_Shared_att=
7; static const int Cyc_Absyn_Unused_att= 8; static const int Cyc_Absyn_Weak_att=
9; static const int Cyc_Absyn_Dllimport_att= 10; static const int Cyc_Absyn_Dllexport_att=
11; static const int Cyc_Absyn_No_instrument_function_att= 12; static const int
Cyc_Absyn_Constructor_att= 13; static const int Cyc_Absyn_Destructor_att= 14;
static const int Cyc_Absyn_No_check_memory_usage_att= 15; static const int Cyc_Absyn_Format_att=
3; struct Cyc_Absyn_Format_att_struct{ int tag; void* f1; int f2; int f3; } ;
static const int Cyc_Absyn_Carray_mod= 0; static const int Cyc_Absyn_ConstArray_mod=
0; struct Cyc_Absyn_ConstArray_mod_struct{ int tag; struct Cyc_Absyn_Exp* f1; }
; static const int Cyc_Absyn_Pointer_mod= 1; struct Cyc_Absyn_Pointer_mod_struct{
int tag; void* f1; void* f2; struct Cyc_Absyn_Tqual f3; } ; static const int Cyc_Absyn_Function_mod=
2; struct Cyc_Absyn_Function_mod_struct{ int tag; void* f1; } ; static const int
Cyc_Absyn_TypeParams_mod= 3; struct Cyc_Absyn_TypeParams_mod_struct{ int tag;
struct Cyc_List_List* f1; struct Cyc_Position_Segment* f2; int f3; } ; static
const int Cyc_Absyn_Attributes_mod= 4; struct Cyc_Absyn_Attributes_mod_struct{
int tag; struct Cyc_Position_Segment* f1; struct Cyc_List_List* f2; } ; static
const int Cyc_Absyn_Char_c= 0; struct Cyc_Absyn_Char_c_struct{ int tag; void* f1;
unsigned char f2; } ; static const int Cyc_Absyn_Short_c= 1; struct Cyc_Absyn_Short_c_struct{
int tag; void* f1; short f2; } ; static const int Cyc_Absyn_Int_c= 2; struct Cyc_Absyn_Int_c_struct{
int tag; void* f1; int f2; } ; static const int Cyc_Absyn_LongLong_c= 3; struct
Cyc_Absyn_LongLong_c_struct{ int tag; void* f1; long long f2; } ; static const
int Cyc_Absyn_Float_c= 4; struct Cyc_Absyn_Float_c_struct{ int tag; struct
_tagged_arr f1; } ; static const int Cyc_Absyn_String_c= 5; struct Cyc_Absyn_String_c_struct{
int tag; struct _tagged_arr f1; } ; static const int Cyc_Absyn_Null_c= 0; static
const int Cyc_Absyn_Plus= 0; static const int Cyc_Absyn_Times= 1; static const
int Cyc_Absyn_Minus= 2; static const int Cyc_Absyn_Div= 3; static const int Cyc_Absyn_Mod=
4; static const int Cyc_Absyn_Eq= 5; static const int Cyc_Absyn_Neq= 6; static
const int Cyc_Absyn_Gt= 7; static const int Cyc_Absyn_Lt= 8; static const int
Cyc_Absyn_Gte= 9; static const int Cyc_Absyn_Lte= 10; static const int Cyc_Absyn_Not=
11; static const int Cyc_Absyn_Bitnot= 12; static const int Cyc_Absyn_Bitand= 13;
static const int Cyc_Absyn_Bitor= 14; static const int Cyc_Absyn_Bitxor= 15;
static const int Cyc_Absyn_Bitlshift= 16; static const int Cyc_Absyn_Bitlrshift=
17; static const int Cyc_Absyn_Bitarshift= 18; static const int Cyc_Absyn_Size=
19; static const int Cyc_Absyn_PreInc= 0; static const int Cyc_Absyn_PostInc= 1;
static const int Cyc_Absyn_PreDec= 2; static const int Cyc_Absyn_PostDec= 3;
struct Cyc_Absyn_VarargCallInfo{ int num_varargs; struct Cyc_List_List*
injectors; struct Cyc_Absyn_VarargInfo* vai; } ; static const int Cyc_Absyn_StructField=
0; struct Cyc_Absyn_StructField_struct{ int tag; struct _tagged_arr* f1; } ;
static const int Cyc_Absyn_TupleIndex= 1; struct Cyc_Absyn_TupleIndex_struct{
int tag; unsigned int f1; } ; static const int Cyc_Absyn_Const_e= 0; struct Cyc_Absyn_Const_e_struct{
int tag; void* f1; } ; static const int Cyc_Absyn_Var_e= 1; struct Cyc_Absyn_Var_e_struct{
int tag; struct _tuple0* f1; void* f2; } ; static const int Cyc_Absyn_UnknownId_e=
2; struct Cyc_Absyn_UnknownId_e_struct{ int tag; struct _tuple0* f1; } ; static
const int Cyc_Absyn_Primop_e= 3; struct Cyc_Absyn_Primop_e_struct{ int tag; void*
f1; struct Cyc_List_List* f2; } ; static const int Cyc_Absyn_AssignOp_e= 4;
struct Cyc_Absyn_AssignOp_e_struct{ int tag; struct Cyc_Absyn_Exp* f1; struct
Cyc_Core_Opt* f2; struct Cyc_Absyn_Exp* f3; } ; static const int Cyc_Absyn_Increment_e=
5; struct Cyc_Absyn_Increment_e_struct{ int tag; struct Cyc_Absyn_Exp* f1; void*
f2; } ; static const int Cyc_Absyn_Conditional_e= 6; struct Cyc_Absyn_Conditional_e_struct{
int tag; struct Cyc_Absyn_Exp* f1; struct Cyc_Absyn_Exp* f2; struct Cyc_Absyn_Exp*
f3; } ; static const int Cyc_Absyn_SeqExp_e= 7; struct Cyc_Absyn_SeqExp_e_struct{
int tag; struct Cyc_Absyn_Exp* f1; struct Cyc_Absyn_Exp* f2; } ; static const
int Cyc_Absyn_UnknownCall_e= 8; struct Cyc_Absyn_UnknownCall_e_struct{ int tag;
struct Cyc_Absyn_Exp* f1; struct Cyc_List_List* f2; } ; static const int Cyc_Absyn_FnCall_e=
9; struct Cyc_Absyn_FnCall_e_struct{ int tag; struct Cyc_Absyn_Exp* f1; struct
Cyc_List_List* f2; struct Cyc_Absyn_VarargCallInfo* f3; } ; static const int Cyc_Absyn_Throw_e=
10; struct Cyc_Absyn_Throw_e_struct{ int tag; struct Cyc_Absyn_Exp* f1; } ;
static const int Cyc_Absyn_NoInstantiate_e= 11; struct Cyc_Absyn_NoInstantiate_e_struct{
int tag; struct Cyc_Absyn_Exp* f1; } ; static const int Cyc_Absyn_Instantiate_e=
12; struct Cyc_Absyn_Instantiate_e_struct{ int tag; struct Cyc_Absyn_Exp* f1;
struct Cyc_List_List* f2; } ; static const int Cyc_Absyn_Cast_e= 13; struct Cyc_Absyn_Cast_e_struct{
int tag; void* f1; struct Cyc_Absyn_Exp* f2; } ; static const int Cyc_Absyn_Address_e=
14; struct Cyc_Absyn_Address_e_struct{ int tag; struct Cyc_Absyn_Exp* f1; } ;
static const int Cyc_Absyn_New_e= 15; struct Cyc_Absyn_New_e_struct{ int tag;
struct Cyc_Absyn_Exp* f1; struct Cyc_Absyn_Exp* f2; } ; static const int Cyc_Absyn_Sizeoftyp_e=
16; struct Cyc_Absyn_Sizeoftyp_e_struct{ int tag; void* f1; } ; static const int
Cyc_Absyn_Sizeofexp_e= 17; struct Cyc_Absyn_Sizeofexp_e_struct{ int tag; struct
Cyc_Absyn_Exp* f1; } ; static const int Cyc_Absyn_Offsetof_e= 18; struct Cyc_Absyn_Offsetof_e_struct{
int tag; void* f1; void* f2; } ; static const int Cyc_Absyn_Gentyp_e= 19; struct
Cyc_Absyn_Gentyp_e_struct{ int tag; struct Cyc_List_List* f1; void* f2; } ;
static const int Cyc_Absyn_Deref_e= 20; struct Cyc_Absyn_Deref_e_struct{ int tag;
struct Cyc_Absyn_Exp* f1; } ; static const int Cyc_Absyn_StructMember_e= 21;
struct Cyc_Absyn_StructMember_e_struct{ int tag; struct Cyc_Absyn_Exp* f1;
struct _tagged_arr* f2; } ; static const int Cyc_Absyn_StructArrow_e= 22; struct
Cyc_Absyn_StructArrow_e_struct{ int tag; struct Cyc_Absyn_Exp* f1; struct
_tagged_arr* f2; } ; static const int Cyc_Absyn_Subscript_e= 23; struct Cyc_Absyn_Subscript_e_struct{
int tag; struct Cyc_Absyn_Exp* f1; struct Cyc_Absyn_Exp* f2; } ; static const
int Cyc_Absyn_Tuple_e= 24; struct Cyc_Absyn_Tuple_e_struct{ int tag; struct Cyc_List_List*
f1; } ; static const int Cyc_Absyn_CompoundLit_e= 25; struct _tuple1{ struct Cyc_Core_Opt*
f1; struct Cyc_Absyn_Tqual f2; void* f3; } ; struct Cyc_Absyn_CompoundLit_e_struct{
int tag; struct _tuple1* f1; struct Cyc_List_List* f2; } ; static const int Cyc_Absyn_Array_e=
26; struct Cyc_Absyn_Array_e_struct{ int tag; struct Cyc_List_List* f1; } ;
static const int Cyc_Absyn_Comprehension_e= 27; struct Cyc_Absyn_Comprehension_e_struct{
int tag; struct Cyc_Absyn_Vardecl* f1; struct Cyc_Absyn_Exp* f2; struct Cyc_Absyn_Exp*
f3; } ; static const int Cyc_Absyn_Struct_e= 28; struct Cyc_Absyn_Struct_e_struct{
int tag; struct _tuple0* f1; struct Cyc_Core_Opt* f2; struct Cyc_List_List* f3;
struct Cyc_Absyn_Structdecl* f4; } ; static const int Cyc_Absyn_AnonStruct_e= 29;
struct Cyc_Absyn_AnonStruct_e_struct{ int tag; void* f1; struct Cyc_List_List*
f2; } ; static const int Cyc_Absyn_Tunion_e= 30; struct Cyc_Absyn_Tunion_e_struct{
int tag; struct Cyc_Core_Opt* f1; struct Cyc_Core_Opt* f2; struct Cyc_List_List*
f3; struct Cyc_Absyn_Tuniondecl* f4; struct Cyc_Absyn_Tunionfield* f5; } ;
static const int Cyc_Absyn_Enum_e= 31; struct Cyc_Absyn_Enum_e_struct{ int tag;
struct _tuple0* f1; struct Cyc_Absyn_Enumdecl* f2; struct Cyc_Absyn_Enumfield*
f3; } ; static const int Cyc_Absyn_AnonEnum_e= 32; struct Cyc_Absyn_AnonEnum_e_struct{
int tag; struct _tuple0* f1; void* f2; struct Cyc_Absyn_Enumfield* f3; } ;
static const int Cyc_Absyn_Malloc_e= 33; struct Cyc_Absyn_Malloc_e_struct{ int
tag; struct Cyc_Absyn_Exp* f1; void* f2; } ; static const int Cyc_Absyn_UnresolvedMem_e=
34; struct Cyc_Absyn_UnresolvedMem_e_struct{ int tag; struct Cyc_Core_Opt* f1;
struct Cyc_List_List* f2; } ; static const int Cyc_Absyn_StmtExp_e= 35; struct
Cyc_Absyn_StmtExp_e_struct{ int tag; struct Cyc_Absyn_Stmt* f1; } ; static const
int Cyc_Absyn_Codegen_e= 36; struct Cyc_Absyn_Codegen_e_struct{ int tag; struct
Cyc_Absyn_Fndecl* f1; } ; static const int Cyc_Absyn_Fill_e= 37; struct Cyc_Absyn_Fill_e_struct{
int tag; struct Cyc_Absyn_Exp* f1; } ; struct Cyc_Absyn_Exp{ struct Cyc_Core_Opt*
topt; void* r; struct Cyc_Position_Segment* loc; void* annot; } ; static const
int Cyc_Absyn_Skip_s= 0; static const int Cyc_Absyn_Exp_s= 0; struct Cyc_Absyn_Exp_s_struct{
int tag; struct Cyc_Absyn_Exp* f1; } ; static const int Cyc_Absyn_Seq_s= 1;
struct Cyc_Absyn_Seq_s_struct{ int tag; struct Cyc_Absyn_Stmt* f1; struct Cyc_Absyn_Stmt*
f2; } ; static const int Cyc_Absyn_Return_s= 2; struct Cyc_Absyn_Return_s_struct{
int tag; struct Cyc_Absyn_Exp* f1; } ; static const int Cyc_Absyn_IfThenElse_s=
3; struct Cyc_Absyn_IfThenElse_s_struct{ int tag; struct Cyc_Absyn_Exp* f1;
struct Cyc_Absyn_Stmt* f2; struct Cyc_Absyn_Stmt* f3; } ; static const int Cyc_Absyn_While_s=
4; struct _tuple2{ struct Cyc_Absyn_Exp* f1; struct Cyc_Absyn_Stmt* f2; } ;
struct Cyc_Absyn_While_s_struct{ int tag; struct _tuple2 f1; struct Cyc_Absyn_Stmt*
f2; } ; static const int Cyc_Absyn_Break_s= 5; struct Cyc_Absyn_Break_s_struct{
int tag; struct Cyc_Absyn_Stmt* f1; } ; static const int Cyc_Absyn_Continue_s= 6;
struct Cyc_Absyn_Continue_s_struct{ int tag; struct Cyc_Absyn_Stmt* f1; } ;
static const int Cyc_Absyn_Goto_s= 7; struct Cyc_Absyn_Goto_s_struct{ int tag;
struct _tagged_arr* f1; struct Cyc_Absyn_Stmt* f2; } ; static const int Cyc_Absyn_For_s=
8; struct Cyc_Absyn_For_s_struct{ int tag; struct Cyc_Absyn_Exp* f1; struct
_tuple2 f2; struct _tuple2 f3; struct Cyc_Absyn_Stmt* f4; } ; static const int
Cyc_Absyn_Switch_s= 9; struct Cyc_Absyn_Switch_s_struct{ int tag; struct Cyc_Absyn_Exp*
f1; struct Cyc_List_List* f2; } ; static const int Cyc_Absyn_SwitchC_s= 10;
struct Cyc_Absyn_SwitchC_s_struct{ int tag; struct Cyc_Absyn_Exp* f1; struct Cyc_List_List*
f2; } ; static const int Cyc_Absyn_Fallthru_s= 11; struct Cyc_Absyn_Fallthru_s_struct{
int tag; struct Cyc_List_List* f1; struct Cyc_Absyn_Switch_clause** f2; } ;
static const int Cyc_Absyn_Decl_s= 12; struct Cyc_Absyn_Decl_s_struct{ int tag;
struct Cyc_Absyn_Decl* f1; struct Cyc_Absyn_Stmt* f2; } ; static const int Cyc_Absyn_Cut_s=
13; struct Cyc_Absyn_Cut_s_struct{ int tag; struct Cyc_Absyn_Stmt* f1; } ;
static const int Cyc_Absyn_Splice_s= 14; struct Cyc_Absyn_Splice_s_struct{ int
tag; struct Cyc_Absyn_Stmt* f1; } ; static const int Cyc_Absyn_Label_s= 15;
struct Cyc_Absyn_Label_s_struct{ int tag; struct _tagged_arr* f1; struct Cyc_Absyn_Stmt*
f2; } ; static const int Cyc_Absyn_Do_s= 16; struct Cyc_Absyn_Do_s_struct{ int
tag; struct Cyc_Absyn_Stmt* f1; struct _tuple2 f2; } ; static const int Cyc_Absyn_TryCatch_s=
17; struct Cyc_Absyn_TryCatch_s_struct{ int tag; struct Cyc_Absyn_Stmt* f1;
struct Cyc_List_List* f2; } ; static const int Cyc_Absyn_Region_s= 18; struct
Cyc_Absyn_Region_s_struct{ int tag; struct Cyc_Absyn_Tvar* f1; struct Cyc_Absyn_Vardecl*
f2; struct Cyc_Absyn_Stmt* f3; } ; struct Cyc_Absyn_Stmt{ void* r; struct Cyc_Position_Segment*
loc; struct Cyc_List_List* non_local_preds; int try_depth; void* annot; } ;
static const int Cyc_Absyn_Wild_p= 0; static const int Cyc_Absyn_Var_p= 0;
struct Cyc_Absyn_Var_p_struct{ int tag; struct Cyc_Absyn_Vardecl* f1; } ; static
const int Cyc_Absyn_Null_p= 1; static const int Cyc_Absyn_Int_p= 1; struct Cyc_Absyn_Int_p_struct{
int tag; void* f1; int f2; } ; static const int Cyc_Absyn_Char_p= 2; struct Cyc_Absyn_Char_p_struct{
int tag; unsigned char f1; } ; static const int Cyc_Absyn_Float_p= 3; struct Cyc_Absyn_Float_p_struct{
int tag; struct _tagged_arr f1; } ; static const int Cyc_Absyn_Tuple_p= 4;
struct Cyc_Absyn_Tuple_p_struct{ int tag; struct Cyc_List_List* f1; } ; static
const int Cyc_Absyn_Pointer_p= 5; struct Cyc_Absyn_Pointer_p_struct{ int tag;
struct Cyc_Absyn_Pat* f1; } ; static const int Cyc_Absyn_Reference_p= 6; struct
Cyc_Absyn_Reference_p_struct{ int tag; struct Cyc_Absyn_Vardecl* f1; } ; static
const int Cyc_Absyn_Struct_p= 7; struct Cyc_Absyn_Struct_p_struct{ int tag;
struct Cyc_Absyn_Structdecl* f1; struct Cyc_Core_Opt* f2; struct Cyc_List_List*
f3; struct Cyc_List_List* f4; } ; static const int Cyc_Absyn_Tunion_p= 8; struct
Cyc_Absyn_Tunion_p_struct{ int tag; struct Cyc_Absyn_Tuniondecl* f1; struct Cyc_Absyn_Tunionfield*
f2; struct Cyc_List_List* f3; struct Cyc_List_List* f4; } ; static const int Cyc_Absyn_Enum_p=
9; struct Cyc_Absyn_Enum_p_struct{ int tag; struct Cyc_Absyn_Enumdecl* f1;
struct Cyc_Absyn_Enumfield* f2; } ; static const int Cyc_Absyn_AnonEnum_p= 10;
struct Cyc_Absyn_AnonEnum_p_struct{ int tag; void* f1; struct Cyc_Absyn_Enumfield*
f2; } ; static const int Cyc_Absyn_UnknownId_p= 11; struct Cyc_Absyn_UnknownId_p_struct{
int tag; struct _tuple0* f1; } ; static const int Cyc_Absyn_UnknownCall_p= 12;
struct Cyc_Absyn_UnknownCall_p_struct{ int tag; struct _tuple0* f1; struct Cyc_List_List*
f2; struct Cyc_List_List* f3; } ; static const int Cyc_Absyn_UnknownFields_p= 13;
struct Cyc_Absyn_UnknownFields_p_struct{ int tag; struct _tuple0* f1; struct Cyc_List_List*
f2; struct Cyc_List_List* f3; } ; struct Cyc_Absyn_Pat{ void* r; struct Cyc_Core_Opt*
topt; struct Cyc_Position_Segment* loc; } ; struct Cyc_Absyn_Switch_clause{
struct Cyc_Absyn_Pat* pattern; struct Cyc_Core_Opt* pat_vars; struct Cyc_Absyn_Exp*
where_clause; struct Cyc_Absyn_Stmt* body; struct Cyc_Position_Segment* loc; } ;
struct Cyc_Absyn_SwitchC_clause{ struct Cyc_Absyn_Exp* cnst_exp; struct Cyc_Absyn_Stmt*
body; struct Cyc_Position_Segment* loc; } ; static const int Cyc_Absyn_Unresolved_b=
0; static const int Cyc_Absyn_Global_b= 0; struct Cyc_Absyn_Global_b_struct{ int
tag; struct Cyc_Absyn_Vardecl* f1; } ; static const int Cyc_Absyn_Funname_b= 1;
struct Cyc_Absyn_Funname_b_struct{ int tag; struct Cyc_Absyn_Fndecl* f1; } ;
static const int Cyc_Absyn_Param_b= 2; struct Cyc_Absyn_Param_b_struct{ int tag;
struct Cyc_Absyn_Vardecl* f1; } ; static const int Cyc_Absyn_Local_b= 3; struct
Cyc_Absyn_Local_b_struct{ int tag; struct Cyc_Absyn_Vardecl* f1; } ; static
const int Cyc_Absyn_Pat_b= 4; struct Cyc_Absyn_Pat_b_struct{ int tag; struct Cyc_Absyn_Vardecl*
f1; } ; struct Cyc_Absyn_Vardecl{ void* sc; struct _tuple0* name; struct Cyc_Absyn_Tqual
tq; void* type; struct Cyc_Absyn_Exp* initializer; struct Cyc_Core_Opt* rgn;
struct Cyc_List_List* attributes; } ; struct Cyc_Absyn_Fndecl{ void* sc; int
is_inline; struct _tuple0* name; struct Cyc_List_List* tvs; struct Cyc_Core_Opt*
effect; void* ret_type; struct Cyc_List_List* args; int c_varargs; struct Cyc_Absyn_VarargInfo*
cyc_varargs; struct Cyc_List_List* rgn_po; struct Cyc_Absyn_Stmt* body; struct
Cyc_Core_Opt* cached_typ; struct Cyc_Core_Opt* param_vardecls; struct Cyc_List_List*
attributes; } ; struct Cyc_Absyn_Structfield{ struct _tagged_arr* name; struct
Cyc_Absyn_Tqual tq; void* type; struct Cyc_Absyn_Exp* width; struct Cyc_List_List*
attributes; } ; struct Cyc_Absyn_Structdecl{ void* sc; struct Cyc_Core_Opt* name;
struct Cyc_List_List* tvs; struct Cyc_Core_Opt* fields; struct Cyc_List_List*
attributes; } ; struct Cyc_Absyn_Uniondecl{ void* sc; struct Cyc_Core_Opt* name;
struct Cyc_List_List* tvs; struct Cyc_Core_Opt* fields; struct Cyc_List_List*
attributes; } ; struct Cyc_Absyn_Tunionfield{ struct _tuple0* name; struct Cyc_List_List*
tvs; struct Cyc_List_List* typs; struct Cyc_Position_Segment* loc; void* sc; } ;
struct Cyc_Absyn_Tuniondecl{ void* sc; struct _tuple0* name; struct Cyc_List_List*
tvs; struct Cyc_Core_Opt* fields; int is_xtunion; } ; struct Cyc_Absyn_Enumfield{
struct _tuple0* name; struct Cyc_Absyn_Exp* tag; struct Cyc_Position_Segment*
loc; } ; struct Cyc_Absyn_Enumdecl{ void* sc; struct _tuple0* name; struct Cyc_Core_Opt*
fields; } ; struct Cyc_Absyn_Typedefdecl{ struct _tuple0* name; struct Cyc_List_List*
tvs; void* defn; } ; static const int Cyc_Absyn_Var_d= 0; struct Cyc_Absyn_Var_d_struct{
int tag; struct Cyc_Absyn_Vardecl* f1; } ; static const int Cyc_Absyn_Fn_d= 1;
struct Cyc_Absyn_Fn_d_struct{ int tag; struct Cyc_Absyn_Fndecl* f1; } ; static
const int Cyc_Absyn_Let_d= 2; struct Cyc_Absyn_Let_d_struct{ int tag; struct Cyc_Absyn_Pat*
f1; struct Cyc_Core_Opt* f2; struct Cyc_Core_Opt* f3; struct Cyc_Absyn_Exp* f4;
int f5; } ; static const int Cyc_Absyn_Letv_d= 3; struct Cyc_Absyn_Letv_d_struct{
int tag; struct Cyc_List_List* f1; } ; static const int Cyc_Absyn_Struct_d= 4;
struct Cyc_Absyn_Struct_d_struct{ int tag; struct Cyc_Absyn_Structdecl* f1; } ;
static const int Cyc_Absyn_Union_d= 5; struct Cyc_Absyn_Union_d_struct{ int tag;
struct Cyc_Absyn_Uniondecl* f1; } ; static const int Cyc_Absyn_Tunion_d= 6;
struct Cyc_Absyn_Tunion_d_struct{ int tag; struct Cyc_Absyn_Tuniondecl* f1; } ;
static const int Cyc_Absyn_Enum_d= 7; struct Cyc_Absyn_Enum_d_struct{ int tag;
struct Cyc_Absyn_Enumdecl* f1; } ; static const int Cyc_Absyn_Typedef_d= 8;
struct Cyc_Absyn_Typedef_d_struct{ int tag; struct Cyc_Absyn_Typedefdecl* f1; }
; static const int Cyc_Absyn_Namespace_d= 9; struct Cyc_Absyn_Namespace_d_struct{
int tag; struct _tagged_arr* f1; struct Cyc_List_List* f2; } ; static const int
Cyc_Absyn_Using_d= 10; struct Cyc_Absyn_Using_d_struct{ int tag; struct _tuple0*
f1; struct Cyc_List_List* f2; } ; static const int Cyc_Absyn_ExternC_d= 11;
struct Cyc_Absyn_ExternC_d_struct{ int tag; struct Cyc_List_List* f1; } ; struct
Cyc_Absyn_Decl{ void* r; struct Cyc_Position_Segment* loc; } ; static const int
Cyc_Absyn_ArrayElement= 0; struct Cyc_Absyn_ArrayElement_struct{ int tag; struct
Cyc_Absyn_Exp* f1; } ; static const int Cyc_Absyn_FieldName= 1; struct Cyc_Absyn_FieldName_struct{
int tag; struct _tagged_arr* f1; } ; extern unsigned char Cyc_Absyn_EmptyAnnot[
15u]; extern struct Cyc_Absyn_Conref* Cyc_Absyn_compress_conref( struct Cyc_Absyn_Conref*
x); extern struct _tagged_arr Cyc_Absyn_attribute2string( void*); struct Cyc_PP_Ppstate;
struct Cyc_PP_Out; struct Cyc_PP_Doc; struct Cyc_Absynpp_Params{ int
expand_typedefs: 1; int qvar_to_Cids: 1; int add_cyc_prefix: 1; int to_VC: 1;
int decls_first: 1; int rewrite_temp_tvars: 1; int print_all_tvars: 1; int
print_all_kinds: 1; int print_using_stmts: 1; int print_externC_stmts: 1; int
print_full_evars: 1; int use_curr_namespace: 1; struct Cyc_List_List*
curr_namespace; } ; extern void Cyc_Absynpp_set_params( struct Cyc_Absynpp_Params*
fs); extern struct _tagged_arr* Cyc_Absynpp_cyc_stringptr; extern int Cyc_Absynpp_exp_prec(
struct Cyc_Absyn_Exp*); extern struct _tagged_arr Cyc_Absynpp_char_escape(
unsigned char); extern struct _tagged_arr Cyc_Absynpp_string_escape( struct
_tagged_arr); extern struct _tagged_arr Cyc_Absynpp_prim2str( void* p); extern
int Cyc_Absynpp_is_declaration( struct Cyc_Absyn_Stmt* s); struct _tuple3{
struct _tagged_arr* f1; struct Cyc_Absyn_Tqual f2; void* f3; } ; extern struct
_tuple1* Cyc_Absynpp_arg_mk_opt( struct _tuple3* arg); struct _tuple4{ struct
Cyc_Absyn_Tqual f1; void* f2; struct Cyc_List_List* f3; } ; extern struct
_tuple4 Cyc_Absynpp_to_tms( struct Cyc_Absyn_Tqual tq, void* t); extern
unsigned int Cyc_Evexp_eval_const_uint_exp( struct Cyc_Absyn_Exp* e); struct Cyc_Set_Set;
extern unsigned char Cyc_Set_Absent[ 11u]; struct Cyc_Dict_Dict; extern
unsigned char Cyc_Dict_Present[ 12u]; extern unsigned char Cyc_Dict_Absent[ 11u];
static const int Cyc_Tcenv_VarRes= 0; struct Cyc_Tcenv_VarRes_struct{ int tag;
void* f1; } ; static const int Cyc_Tcenv_StructRes= 1; struct Cyc_Tcenv_StructRes_struct{
int tag; struct Cyc_Absyn_Structdecl* f1; } ; static const int Cyc_Tcenv_TunionRes=
2; struct Cyc_Tcenv_TunionRes_struct{ int tag; struct Cyc_Absyn_Tuniondecl* f1;
struct Cyc_Absyn_Tunionfield* f2; } ; static const int Cyc_Tcenv_EnumRes= 3;
struct Cyc_Tcenv_EnumRes_struct{ int tag; struct Cyc_Absyn_Enumdecl* f1; struct
Cyc_Absyn_Enumfield* f2; } ; static const int Cyc_Tcenv_AnonEnumRes= 4; struct
Cyc_Tcenv_AnonEnumRes_struct{ int tag; void* f1; struct Cyc_Absyn_Enumfield* f2;
} ; struct Cyc_Tcenv_Genv{ struct Cyc_Set_Set* namespaces; struct Cyc_Dict_Dict*
structdecls; struct Cyc_Dict_Dict* uniondecls; struct Cyc_Dict_Dict* tuniondecls;
struct Cyc_Dict_Dict* enumdecls; struct Cyc_Dict_Dict* typedefs; struct Cyc_Dict_Dict*
ordinaries; struct Cyc_List_List* availables; } ; struct Cyc_Tcenv_Fenv; static
const int Cyc_Tcenv_NotLoop_j= 0; static const int Cyc_Tcenv_CaseEnd_j= 1;
static const int Cyc_Tcenv_FnEnd_j= 2; static const int Cyc_Tcenv_Stmt_j= 0;
struct Cyc_Tcenv_Stmt_j_struct{ int tag; struct Cyc_Absyn_Stmt* f1; } ; static
const int Cyc_Tcenv_Outermost= 0; struct Cyc_Tcenv_Outermost_struct{ int tag;
void* f1; } ; static const int Cyc_Tcenv_Frame= 1; struct Cyc_Tcenv_Frame_struct{
int tag; void* f1; void* f2; } ; static const int Cyc_Tcenv_Hidden= 2; struct
Cyc_Tcenv_Hidden_struct{ int tag; void* f1; void* f2; } ; struct Cyc_Tcenv_Tenv{
struct Cyc_List_List* ns; struct Cyc_Dict_Dict* ae; struct Cyc_Core_Opt* le; } ;
extern unsigned char Cyc_Tcutil_TypeErr[ 12u]; extern void* Cyc_Tcutil_compress(
void* t); static int Cyc_Absyndump_expand_typedefs; static int Cyc_Absyndump_qvar_to_Cids;
static int Cyc_Absyndump_add_cyc_prefix; static int Cyc_Absyndump_to_VC; void
Cyc_Absyndump_set_params( struct Cyc_Absynpp_Params* fs){ Cyc_Absyndump_expand_typedefs=
fs->expand_typedefs; Cyc_Absyndump_qvar_to_Cids= fs->qvar_to_Cids; Cyc_Absyndump_add_cyc_prefix=
fs->add_cyc_prefix; Cyc_Absyndump_to_VC= fs->to_VC; Cyc_Absynpp_set_params( fs);}
void Cyc_Absyndump_dumptyp( void*); void Cyc_Absyndump_dumpntyp( void* t); void
Cyc_Absyndump_dumpexp( struct Cyc_Absyn_Exp*); void Cyc_Absyndump_dumpexp_prec(
int, struct Cyc_Absyn_Exp*); void Cyc_Absyndump_dumppat( struct Cyc_Absyn_Pat*);
void Cyc_Absyndump_dumpstmt( struct Cyc_Absyn_Stmt*); void Cyc_Absyndump_dumpdecl(
struct Cyc_Absyn_Decl*); void Cyc_Absyndump_dumptms( struct Cyc_List_List* tms,
void(* f)( void*), void* a); void Cyc_Absyndump_dumptqtd( struct Cyc_Absyn_Tqual,
void*, void(* f)( void*), void*); void Cyc_Absyndump_dumpstructfields( struct
Cyc_List_List* fields); void Cyc_Absyndump_dumpenumfields( struct Cyc_List_List*
fields); struct Cyc_Std___sFILE** Cyc_Absyndump_dump_file=& Cyc_Std_stdout; void
Cyc_Absyndump_ignore( void* x){ return;} static unsigned int Cyc_Absyndump_pos=
0; void Cyc_Absyndump_dump( struct _tagged_arr s){ int sz=( int) _get_arr_size(
s, sizeof( unsigned char)); if( !(( int)*(( const unsigned char*)
_check_unknown_subscript( s, sizeof( unsigned char), sz -  1)))){ -- sz;} Cyc_Absyndump_pos
+= sz +  1; if( Cyc_Absyndump_pos >  80){ Cyc_Absyndump_pos=( unsigned int) sz;
Cyc_Std_fputc(( int)'\n',* Cyc_Absyndump_dump_file);} else{ Cyc_Std_fputc(( int)' ',*
Cyc_Absyndump_dump_file);} Cyc_Std_file_string_write(* Cyc_Absyndump_dump_file,
s, 0, sz);} void Cyc_Absyndump_dump_nospace( struct _tagged_arr s){ int sz=( int)
_get_arr_size( s, sizeof( unsigned char)); if( !(( int)*(( const unsigned char*)
_check_unknown_subscript( s, sizeof( unsigned char), sz -  1)))){ -- sz;} Cyc_Absyndump_pos
+= sz; Cyc_Std_file_string_write(* Cyc_Absyndump_dump_file, s, 0, sz);} void Cyc_Absyndump_dump_char(
int c){ ++ Cyc_Absyndump_pos; Cyc_Std_fputc( c,* Cyc_Absyndump_dump_file);} void
Cyc_Absyndump_dump_str( struct _tagged_arr* s){ Cyc_Absyndump_dump(* s);} void
Cyc_Absyndump_dump_semi(){ Cyc_Absyndump_dump_char(( int)';');} void Cyc_Absyndump_dump_sep(
void(* f)( void*), struct Cyc_List_List* l, struct _tagged_arr sep){ if( l ==  0){
return;} for( 0; l->tl !=  0; l= l->tl){ f(( void*) l->hd); Cyc_Absyndump_dump_nospace(
sep);} f(( void*) l->hd);} void Cyc_Absyndump_dump_sep_c( void(* f)( void*, void*),
void* env, struct Cyc_List_List* l, struct _tagged_arr sep){ if( l ==  0){
return;} for( 0; l->tl !=  0; l= l->tl){ f( env,( void*) l->hd); Cyc_Absyndump_dump_nospace(
sep);} f( env,( void*) l->hd);} void Cyc_Absyndump_group( void(* f)( void*),
struct Cyc_List_List* l, struct _tagged_arr start, struct _tagged_arr end,
struct _tagged_arr sep){ Cyc_Absyndump_dump_nospace( start); Cyc_Absyndump_dump_sep(
f, l, sep); Cyc_Absyndump_dump_nospace( end);} void Cyc_Absyndump_group_c( void(*
f)( void*, void*), void* env, struct Cyc_List_List* l, struct _tagged_arr start,
struct _tagged_arr end, struct _tagged_arr sep){ Cyc_Absyndump_dump_nospace(
start); Cyc_Absyndump_dump_sep_c( f, env, l, sep); Cyc_Absyndump_dump_nospace(
end);} void Cyc_Absyndump_egroup( void(* f)( void*), struct Cyc_List_List* l,
struct _tagged_arr start, struct _tagged_arr end, struct _tagged_arr sep){ if( l
!=  0){ Cyc_Absyndump_group( f, l, start, end, sep);}} void Cyc_Absyndump_dumpqvar(
struct _tuple0* v){ struct Cyc_List_List* _temp0= 0;{ void* _temp1=(* v).f1;
struct Cyc_List_List* _temp9; struct Cyc_List_List* _temp11; _LL3: if( _temp1 == (
void*) Cyc_Absyn_Loc_n){ goto _LL4;} else{ goto _LL5;} _LL5: if(( unsigned int)
_temp1 >  1u?*(( int*) _temp1) ==  Cyc_Absyn_Rel_n: 0){ _LL10: _temp9=(( struct
Cyc_Absyn_Rel_n_struct*) _temp1)->f1; goto _LL6;} else{ goto _LL7;} _LL7: if((
unsigned int) _temp1 >  1u?*(( int*) _temp1) ==  Cyc_Absyn_Abs_n: 0){ _LL12:
_temp11=(( struct Cyc_Absyn_Abs_n_struct*) _temp1)->f1; goto _LL8;} else{ goto
_LL2;} _LL4: _temp9= 0; goto _LL6; _LL6: _temp0= _temp9; goto _LL2; _LL8: _temp0=(
Cyc_Absyndump_qvar_to_Cids? Cyc_Absyndump_add_cyc_prefix: 0)?({ struct Cyc_List_List*
_temp13=( struct Cyc_List_List*) _cycalloc( sizeof( struct Cyc_List_List));
_temp13->hd=( void*) Cyc_Absynpp_cyc_stringptr; _temp13->tl= _temp11; _temp13;}):
_temp11; goto _LL2; _LL2:;} if( _temp0 !=  0){ Cyc_Absyndump_dump_str(( struct
_tagged_arr*) _temp0->hd); for( _temp0= _temp0->tl; _temp0 !=  0; _temp0= _temp0->tl){
if( Cyc_Absyndump_qvar_to_Cids){ Cyc_Absyndump_dump_char(( int)'_');} else{ Cyc_Absyndump_dump_nospace(
_tag_arr("::", sizeof( unsigned char), 3u));} Cyc_Absyndump_dump_nospace(*((
struct _tagged_arr*) _temp0->hd));} if( Cyc_Absyndump_qvar_to_Cids){ Cyc_Absyndump_dump_nospace(
_tag_arr("_", sizeof( unsigned char), 2u));} else{ Cyc_Absyndump_dump_nospace(
_tag_arr("::", sizeof( unsigned char), 3u));} Cyc_Absyndump_dump_nospace(*(* v).f2);}
else{ Cyc_Absyndump_dump_str((* v).f2);}} void Cyc_Absyndump_dumptq( struct Cyc_Absyn_Tqual
tq){ if( tq.q_restrict){ Cyc_Absyndump_dump( _tag_arr("restrict", sizeof(
unsigned char), 9u));} if( tq.q_volatile){ Cyc_Absyndump_dump( _tag_arr("volatile",
sizeof( unsigned char), 9u));} if( tq.q_const){ Cyc_Absyndump_dump( _tag_arr("const",
sizeof( unsigned char), 6u));}} void Cyc_Absyndump_dumpscope( void* sc){ void*
_temp14= sc; _LL16: if( _temp14 == ( void*) Cyc_Absyn_Static){ goto _LL17;}
else{ goto _LL18;} _LL18: if( _temp14 == ( void*) Cyc_Absyn_Public){ goto _LL19;}
else{ goto _LL20;} _LL20: if( _temp14 == ( void*) Cyc_Absyn_Extern){ goto _LL21;}
else{ goto _LL22;} _LL22: if( _temp14 == ( void*) Cyc_Absyn_ExternC){ goto _LL23;}
else{ goto _LL24;} _LL24: if( _temp14 == ( void*) Cyc_Absyn_Abstract){ goto
_LL25;} else{ goto _LL15;} _LL17: Cyc_Absyndump_dump( _tag_arr("static", sizeof(
unsigned char), 7u)); return; _LL19: return; _LL21: Cyc_Absyndump_dump( _tag_arr("extern",
sizeof( unsigned char), 7u)); return; _LL23: Cyc_Absyndump_dump( _tag_arr("extern \"C\"",
sizeof( unsigned char), 11u)); return; _LL25: Cyc_Absyndump_dump( _tag_arr("abstract",
sizeof( unsigned char), 9u)); return; _LL15:;} void Cyc_Absyndump_dumpkind( void*
k){ void* _temp26= k; _LL28: if( _temp26 == ( void*) Cyc_Absyn_AnyKind){ goto
_LL29;} else{ goto _LL30;} _LL30: if( _temp26 == ( void*) Cyc_Absyn_MemKind){
goto _LL31;} else{ goto _LL32;} _LL32: if( _temp26 == ( void*) Cyc_Absyn_BoxKind){
goto _LL33;} else{ goto _LL34;} _LL34: if( _temp26 == ( void*) Cyc_Absyn_RgnKind){
goto _LL35;} else{ goto _LL36;} _LL36: if( _temp26 == ( void*) Cyc_Absyn_EffKind){
goto _LL37;} else{ goto _LL27;} _LL29: Cyc_Absyndump_dump( _tag_arr("A", sizeof(
unsigned char), 2u)); return; _LL31: Cyc_Absyndump_dump( _tag_arr("M", sizeof(
unsigned char), 2u)); return; _LL33: Cyc_Absyndump_dump( _tag_arr("B", sizeof(
unsigned char), 2u)); return; _LL35: Cyc_Absyndump_dump( _tag_arr("R", sizeof(
unsigned char), 2u)); return; _LL37: Cyc_Absyndump_dump( _tag_arr("E", sizeof(
unsigned char), 2u)); return; _LL27:;} void Cyc_Absyndump_dumptps( struct Cyc_List_List*
ts){ Cyc_Absyndump_egroup( Cyc_Absyndump_dumptyp, ts, _tag_arr("<", sizeof(
unsigned char), 2u), _tag_arr(">", sizeof( unsigned char), 2u), _tag_arr(",",
sizeof( unsigned char), 2u));} void Cyc_Absyndump_dumptvar( struct Cyc_Absyn_Tvar*
tv){ Cyc_Absyndump_dump_str( tv->name);} void Cyc_Absyndump_dumpkindedtvar(
struct Cyc_Absyn_Tvar* tv){ Cyc_Absyndump_dump_str( tv->name);{ void* _temp38=(
void*)( Cyc_Absyn_compress_conref( tv->kind))->v; void* _temp46; void* _temp48;
_LL40: if(( unsigned int) _temp38 >  1u?*(( int*) _temp38) ==  Cyc_Absyn_Eq_constr:
0){ _LL47: _temp46=( void*)(( struct Cyc_Absyn_Eq_constr_struct*) _temp38)->f1;
if( _temp46 == ( void*) Cyc_Absyn_BoxKind){ goto _LL41;} else{ goto _LL42;}}
else{ goto _LL42;} _LL42: if(( unsigned int) _temp38 >  1u?*(( int*) _temp38) == 
Cyc_Absyn_Eq_constr: 0){ _LL49: _temp48=( void*)(( struct Cyc_Absyn_Eq_constr_struct*)
_temp38)->f1; goto _LL43;} else{ goto _LL44;} _LL44: goto _LL45; _LL41: goto
_LL39; _LL43: Cyc_Absyndump_dump( _tag_arr("::", sizeof( unsigned char), 3u));
Cyc_Absyndump_dumpkind( _temp48); goto _LL39; _LL45: Cyc_Absyndump_dump(
_tag_arr("::?", sizeof( unsigned char), 4u)); goto _LL39; _LL39:;}} void Cyc_Absyndump_dumptvars(
struct Cyc_List_List* tvs){(( void(*)( void(* f)( struct Cyc_Absyn_Tvar*),
struct Cyc_List_List* l, struct _tagged_arr start, struct _tagged_arr end,
struct _tagged_arr sep)) Cyc_Absyndump_egroup)( Cyc_Absyndump_dumptvar, tvs,
_tag_arr("<", sizeof( unsigned char), 2u), _tag_arr(">", sizeof( unsigned char),
2u), _tag_arr(",", sizeof( unsigned char), 2u));} void Cyc_Absyndump_dumpkindedtvars(
struct Cyc_List_List* tvs){(( void(*)( void(* f)( struct Cyc_Absyn_Tvar*),
struct Cyc_List_List* l, struct _tagged_arr start, struct _tagged_arr end,
struct _tagged_arr sep)) Cyc_Absyndump_egroup)( Cyc_Absyndump_dumpkindedtvar,
tvs, _tag_arr("<", sizeof( unsigned char), 2u), _tag_arr(">", sizeof(
unsigned char), 2u), _tag_arr(",", sizeof( unsigned char), 2u));} struct _tuple5{
struct Cyc_Absyn_Tqual f1; void* f2; } ; void Cyc_Absyndump_dumparg( struct
_tuple5* pr){(( void(*)( struct Cyc_Absyn_Tqual, void*, void(* f)( int), int))
Cyc_Absyndump_dumptqtd)((* pr).f1,(* pr).f2,( void(*)( int x)) Cyc_Absyndump_ignore,
0);} void Cyc_Absyndump_dumpargs( struct Cyc_List_List* ts){(( void(*)( void(* f)(
struct _tuple5*), struct Cyc_List_List* l, struct _tagged_arr start, struct
_tagged_arr end, struct _tagged_arr sep)) Cyc_Absyndump_group)( Cyc_Absyndump_dumparg,
ts, _tag_arr("(", sizeof( unsigned char), 2u), _tag_arr(")", sizeof(
unsigned char), 2u), _tag_arr(",", sizeof( unsigned char), 2u));} void Cyc_Absyndump_dump_callconv(
struct Cyc_List_List* atts){ for( 0; atts !=  0; atts= atts->tl){ void* _temp50=(
void*) atts->hd; _LL52: if( _temp50 == ( void*) Cyc_Absyn_Stdcall_att){ goto
_LL53;} else{ goto _LL54;} _LL54: if( _temp50 == ( void*) Cyc_Absyn_Cdecl_att){
goto _LL55;} else{ goto _LL56;} _LL56: if( _temp50 == ( void*) Cyc_Absyn_Fastcall_att){
goto _LL57;} else{ goto _LL58;} _LL58: goto _LL59; _LL53: Cyc_Absyndump_dump(
_tag_arr("_stdcall", sizeof( unsigned char), 9u)); return; _LL55: Cyc_Absyndump_dump(
_tag_arr("_cdecl", sizeof( unsigned char), 7u)); return; _LL57: Cyc_Absyndump_dump(
_tag_arr("_fastcall", sizeof( unsigned char), 10u)); return; _LL59: goto _LL51;
_LL51:;}} void Cyc_Absyndump_dump_noncallconv( struct Cyc_List_List* atts){ int
hasatt= 0;{ struct Cyc_List_List* atts2= atts; for( 0; atts2 !=  0; atts2= atts2->tl){
void* _temp60=( void*)(( struct Cyc_List_List*) _check_null( atts))->hd; _LL62:
if( _temp60 == ( void*) Cyc_Absyn_Stdcall_att){ goto _LL63;} else{ goto _LL64;}
_LL64: if( _temp60 == ( void*) Cyc_Absyn_Cdecl_att){ goto _LL65;} else{ goto
_LL66;} _LL66: if( _temp60 == ( void*) Cyc_Absyn_Fastcall_att){ goto _LL67;}
else{ goto _LL68;} _LL68: goto _LL69; _LL63: goto _LL61; _LL65: goto _LL61;
_LL67: goto _LL61; _LL69: hasatt= 1; goto _LL61; _LL61:;}} if( ! hasatt){
return;} Cyc_Absyndump_dump( _tag_arr("__declspec(", sizeof( unsigned char), 12u));
for( 0; atts !=  0; atts= atts->tl){ void* _temp70=( void*) atts->hd; _LL72: if(
_temp70 == ( void*) Cyc_Absyn_Stdcall_att){ goto _LL73;} else{ goto _LL74;}
_LL74: if( _temp70 == ( void*) Cyc_Absyn_Cdecl_att){ goto _LL75;} else{ goto
_LL76;} _LL76: if( _temp70 == ( void*) Cyc_Absyn_Fastcall_att){ goto _LL77;}
else{ goto _LL78;} _LL78: goto _LL79; _LL73: goto _LL71; _LL75: goto _LL71;
_LL77: goto _LL71; _LL79: Cyc_Absyndump_dump( Cyc_Absyn_attribute2string(( void*)
atts->hd)); goto _LL71; _LL71:;} Cyc_Absyndump_dump_char(( int)')');} void Cyc_Absyndump_dumpatts(
struct Cyc_List_List* atts){ if( atts ==  0){ return;} if( Cyc_Absyndump_to_VC){
Cyc_Absyndump_dump_noncallconv( atts); return;} Cyc_Absyndump_dump( _tag_arr(" __attribute__((",
sizeof( unsigned char), 17u)); for( 0; atts !=  0; atts= atts->tl){ Cyc_Absyndump_dump(
Cyc_Absyn_attribute2string(( void*) atts->hd)); if( atts->tl !=  0){ Cyc_Absyndump_dump(
_tag_arr(",", sizeof( unsigned char), 2u));}} Cyc_Absyndump_dump( _tag_arr(")) ",
sizeof( unsigned char), 4u));} int Cyc_Absyndump_next_is_pointer( struct Cyc_List_List*
tms){ if( tms ==  0){ return 0;}{ void* _temp80=( void*) tms->hd; _LL82: if((
unsigned int) _temp80 >  1u?*(( int*) _temp80) ==  Cyc_Absyn_Pointer_mod: 0){
goto _LL83;} else{ goto _LL84;} _LL84: goto _LL85; _LL83: return 1; _LL85:
return 0; _LL81:;}} static void Cyc_Absyndump_dumprgn( void* t){ void* _temp86=
Cyc_Tcutil_compress( t); _LL88: if( _temp86 == ( void*) Cyc_Absyn_HeapRgn){ goto
_LL89;} else{ goto _LL90;} _LL90: goto _LL91; _LL89: Cyc_Absyndump_dump(
_tag_arr("`H", sizeof( unsigned char), 3u)); goto _LL87; _LL91: Cyc_Absyndump_dumpntyp(
t); goto _LL87; _LL87:;} struct _tuple6{ struct Cyc_List_List* f1; struct Cyc_List_List*
f2; } ; static struct _tuple6 Cyc_Absyndump_effects_split( void* t){ struct Cyc_List_List*
rgions= 0; struct Cyc_List_List* effects= 0;{ void* _temp92= Cyc_Tcutil_compress(
t); void* _temp100; struct Cyc_List_List* _temp102; _LL94: if(( unsigned int)
_temp92 >  4u?*(( int*) _temp92) ==  Cyc_Absyn_AccessEff: 0){ _LL101: _temp100=(
void*)(( struct Cyc_Absyn_AccessEff_struct*) _temp92)->f1; goto _LL95;} else{
goto _LL96;} _LL96: if(( unsigned int) _temp92 >  4u?*(( int*) _temp92) ==  Cyc_Absyn_JoinEff:
0){ _LL103: _temp102=(( struct Cyc_Absyn_JoinEff_struct*) _temp92)->f1; goto
_LL97;} else{ goto _LL98;} _LL98: goto _LL99; _LL95: rgions=({ struct Cyc_List_List*
_temp104=( struct Cyc_List_List*) _cycalloc( sizeof( struct Cyc_List_List));
_temp104->hd=( void*) _temp100; _temp104->tl= rgions; _temp104;}); goto _LL93;
_LL97: for( 0; _temp102 !=  0; _temp102= _temp102->tl){ struct Cyc_List_List*
_temp107; struct Cyc_List_List* _temp109; struct _tuple6 _temp105= Cyc_Absyndump_effects_split((
void*) _temp102->hd); _LL110: _temp109= _temp105.f1; goto _LL108; _LL108:
_temp107= _temp105.f2; goto _LL106; _LL106: rgions= Cyc_List_imp_append(
_temp109, rgions); effects= Cyc_List_imp_append( _temp107, effects);} goto _LL93;
_LL99: effects=({ struct Cyc_List_List* _temp111=( struct Cyc_List_List*)
_cycalloc( sizeof( struct Cyc_List_List)); _temp111->hd=( void*) t; _temp111->tl=
effects; _temp111;}); goto _LL93; _LL93:;} return({ struct _tuple6 _temp112;
_temp112.f1= rgions; _temp112.f2= effects; _temp112;});} static void Cyc_Absyndump_dumpeff(
void* t){ struct Cyc_List_List* _temp115; struct Cyc_List_List* _temp117; struct
_tuple6 _temp113= Cyc_Absyndump_effects_split( t); _LL118: _temp117= _temp113.f1;
goto _LL116; _LL116: _temp115= _temp113.f2; goto _LL114; _LL114: _temp117= Cyc_List_imp_rev(
_temp117); _temp115= Cyc_List_imp_rev( _temp115); for( 0; _temp115 !=  0;
_temp115= _temp115->tl){ Cyc_Absyndump_dumpntyp(( void*) _temp115->hd); Cyc_Absyndump_dump_char((
int)'+');} Cyc_Absyndump_dump_char(( int)'{'); for( 0; _temp117 !=  0; _temp117=
_temp117->tl){ Cyc_Absyndump_dumprgn(( void*) _temp117->hd); if( _temp117->tl != 
0){ Cyc_Absyndump_dump_char(( int)',');}} Cyc_Absyndump_dump_char(( int)'}');}
void Cyc_Absyndump_dumpntyp( void* t){ void* _temp119= t; struct Cyc_Absyn_Tvar*
_temp193; int _temp195; struct Cyc_Core_Opt* _temp197; struct Cyc_Core_Opt*
_temp199; int _temp201; struct Cyc_Core_Opt* _temp203; struct Cyc_Core_Opt
_temp205; void* _temp206; struct Cyc_Core_Opt* _temp208; struct Cyc_Absyn_TunionInfo
_temp210; void* _temp212; struct Cyc_List_List* _temp214; void* _temp216; struct
Cyc_Absyn_TunionFieldInfo _temp218; struct Cyc_List_List* _temp220; void*
_temp222; struct _tuple0* _temp224; void* _temp226; void* _temp228; void*
_temp230; void* _temp232; void* _temp234; void* _temp236; void* _temp238; void*
_temp240; void* _temp242; void* _temp244; void* _temp246; void* _temp248; void*
_temp251; void* _temp253; void* _temp255; void* _temp257; void* _temp260; void*
_temp262; void* _temp264; void* _temp266; struct Cyc_List_List* _temp268; struct
Cyc_List_List* _temp270; struct _tuple0* _temp272; struct Cyc_List_List*
_temp274; struct _tuple0* _temp276; struct Cyc_List_List* _temp278; struct
_tuple0* _temp280; struct Cyc_List_List* _temp282; struct _tuple0* _temp284;
struct Cyc_List_List* _temp286; struct Cyc_List_List* _temp288; struct Cyc_List_List*
_temp290; struct Cyc_Core_Opt* _temp292; struct Cyc_List_List* _temp294; struct
_tuple0* _temp296; void* _temp298; _LL121: if(( unsigned int) _temp119 >  4u?*((
int*) _temp119) ==  Cyc_Absyn_ArrayType: 0){ goto _LL122;} else{ goto _LL123;}
_LL123: if(( unsigned int) _temp119 >  4u?*(( int*) _temp119) ==  Cyc_Absyn_FnType:
0){ goto _LL124;} else{ goto _LL125;} _LL125: if(( unsigned int) _temp119 >  4u?*((
int*) _temp119) ==  Cyc_Absyn_PointerType: 0){ goto _LL126;} else{ goto _LL127;}
_LL127: if( _temp119 == ( void*) Cyc_Absyn_VoidType){ goto _LL128;} else{ goto
_LL129;} _LL129: if(( unsigned int) _temp119 >  4u?*(( int*) _temp119) ==  Cyc_Absyn_VarType:
0){ _LL194: _temp193=(( struct Cyc_Absyn_VarType_struct*) _temp119)->f1; goto
_LL130;} else{ goto _LL131;} _LL131: if(( unsigned int) _temp119 >  4u?*(( int*)
_temp119) ==  Cyc_Absyn_Evar: 0){ _LL200: _temp199=(( struct Cyc_Absyn_Evar_struct*)
_temp119)->f1; goto _LL198; _LL198: _temp197=(( struct Cyc_Absyn_Evar_struct*)
_temp119)->f2; if( _temp197 ==  0){ goto _LL196;} else{ goto _LL133;} _LL196:
_temp195=(( struct Cyc_Absyn_Evar_struct*) _temp119)->f3; goto _LL132;} else{
goto _LL133;} _LL133: if(( unsigned int) _temp119 >  4u?*(( int*) _temp119) == 
Cyc_Absyn_Evar: 0){ _LL209: _temp208=(( struct Cyc_Absyn_Evar_struct*) _temp119)->f1;
goto _LL204; _LL204: _temp203=(( struct Cyc_Absyn_Evar_struct*) _temp119)->f2;
if( _temp203 ==  0){ goto _LL135;} else{ _temp205=* _temp203; _LL207: _temp206=(
void*) _temp205.v; goto _LL202;} _LL202: _temp201=(( struct Cyc_Absyn_Evar_struct*)
_temp119)->f3; goto _LL134;} else{ goto _LL135;} _LL135: if(( unsigned int)
_temp119 >  4u?*(( int*) _temp119) ==  Cyc_Absyn_TunionType: 0){ _LL211:
_temp210=(( struct Cyc_Absyn_TunionType_struct*) _temp119)->f1; _LL217: _temp216=(
void*) _temp210.tunion_info; goto _LL215; _LL215: _temp214= _temp210.targs; goto
_LL213; _LL213: _temp212=( void*) _temp210.rgn; goto _LL136;} else{ goto _LL137;}
_LL137: if(( unsigned int) _temp119 >  4u?*(( int*) _temp119) ==  Cyc_Absyn_TunionFieldType:
0){ _LL219: _temp218=(( struct Cyc_Absyn_TunionFieldType_struct*) _temp119)->f1;
_LL223: _temp222=( void*) _temp218.field_info; goto _LL221; _LL221: _temp220=
_temp218.targs; goto _LL138;} else{ goto _LL139;} _LL139: if(( unsigned int)
_temp119 >  4u?*(( int*) _temp119) ==  Cyc_Absyn_EnumType: 0){ _LL225: _temp224=((
struct Cyc_Absyn_EnumType_struct*) _temp119)->f1; goto _LL140;} else{ goto
_LL141;} _LL141: if(( unsigned int) _temp119 >  4u?*(( int*) _temp119) ==  Cyc_Absyn_IntType:
0){ _LL229: _temp228=( void*)(( struct Cyc_Absyn_IntType_struct*) _temp119)->f1;
if( _temp228 == ( void*) Cyc_Absyn_Signed){ goto _LL227;} else{ goto _LL143;}
_LL227: _temp226=( void*)(( struct Cyc_Absyn_IntType_struct*) _temp119)->f2; if(
_temp226 == ( void*) Cyc_Absyn_B4){ goto _LL142;} else{ goto _LL143;}} else{
goto _LL143;} _LL143: if(( unsigned int) _temp119 >  4u?*(( int*) _temp119) == 
Cyc_Absyn_IntType: 0){ _LL233: _temp232=( void*)(( struct Cyc_Absyn_IntType_struct*)
_temp119)->f1; if( _temp232 == ( void*) Cyc_Absyn_Signed){ goto _LL231;} else{
goto _LL145;} _LL231: _temp230=( void*)(( struct Cyc_Absyn_IntType_struct*)
_temp119)->f2; if( _temp230 == ( void*) Cyc_Absyn_B1){ goto _LL144;} else{ goto
_LL145;}} else{ goto _LL145;} _LL145: if(( unsigned int) _temp119 >  4u?*(( int*)
_temp119) ==  Cyc_Absyn_IntType: 0){ _LL237: _temp236=( void*)(( struct Cyc_Absyn_IntType_struct*)
_temp119)->f1; if( _temp236 == ( void*) Cyc_Absyn_Unsigned){ goto _LL235;} else{
goto _LL147;} _LL235: _temp234=( void*)(( struct Cyc_Absyn_IntType_struct*)
_temp119)->f2; if( _temp234 == ( void*) Cyc_Absyn_B1){ goto _LL146;} else{ goto
_LL147;}} else{ goto _LL147;} _LL147: if(( unsigned int) _temp119 >  4u?*(( int*)
_temp119) ==  Cyc_Absyn_IntType: 0){ _LL241: _temp240=( void*)(( struct Cyc_Absyn_IntType_struct*)
_temp119)->f1; if( _temp240 == ( void*) Cyc_Absyn_Signed){ goto _LL239;} else{
goto _LL149;} _LL239: _temp238=( void*)(( struct Cyc_Absyn_IntType_struct*)
_temp119)->f2; if( _temp238 == ( void*) Cyc_Absyn_B2){ goto _LL148;} else{ goto
_LL149;}} else{ goto _LL149;} _LL149: if(( unsigned int) _temp119 >  4u?*(( int*)
_temp119) ==  Cyc_Absyn_IntType: 0){ _LL245: _temp244=( void*)(( struct Cyc_Absyn_IntType_struct*)
_temp119)->f1; if( _temp244 == ( void*) Cyc_Absyn_Unsigned){ goto _LL243;} else{
goto _LL151;} _LL243: _temp242=( void*)(( struct Cyc_Absyn_IntType_struct*)
_temp119)->f2; if( _temp242 == ( void*) Cyc_Absyn_B2){ goto _LL150;} else{ goto
_LL151;}} else{ goto _LL151;} _LL151: if(( unsigned int) _temp119 >  4u?*(( int*)
_temp119) ==  Cyc_Absyn_IntType: 0){ _LL249: _temp248=( void*)(( struct Cyc_Absyn_IntType_struct*)
_temp119)->f1; if( _temp248 == ( void*) Cyc_Absyn_Unsigned){ goto _LL247;} else{
goto _LL153;} _LL247: _temp246=( void*)(( struct Cyc_Absyn_IntType_struct*)
_temp119)->f2; if( _temp246 == ( void*) Cyc_Absyn_B4){ goto _LL152;} else{ goto
_LL153;}} else{ goto _LL153;} _LL153: if(( unsigned int) _temp119 >  4u?*(( int*)
_temp119) ==  Cyc_Absyn_IntType: 0){ _LL254: _temp253=( void*)(( struct Cyc_Absyn_IntType_struct*)
_temp119)->f1; if( _temp253 == ( void*) Cyc_Absyn_Signed){ goto _LL252;} else{
goto _LL155;} _LL252: _temp251=( void*)(( struct Cyc_Absyn_IntType_struct*)
_temp119)->f2; if( _temp251 == ( void*) Cyc_Absyn_B8){ goto _LL250;} else{ goto
_LL155;}} else{ goto _LL155;} _LL250: if( Cyc_Absyndump_to_VC){ goto _LL154;}
else{ goto _LL155;} _LL155: if(( unsigned int) _temp119 >  4u?*(( int*) _temp119)
==  Cyc_Absyn_IntType: 0){ _LL258: _temp257=( void*)(( struct Cyc_Absyn_IntType_struct*)
_temp119)->f1; if( _temp257 == ( void*) Cyc_Absyn_Signed){ goto _LL256;} else{
goto _LL157;} _LL256: _temp255=( void*)(( struct Cyc_Absyn_IntType_struct*)
_temp119)->f2; if( _temp255 == ( void*) Cyc_Absyn_B8){ goto _LL156;} else{ goto
_LL157;}} else{ goto _LL157;} _LL157: if(( unsigned int) _temp119 >  4u?*(( int*)
_temp119) ==  Cyc_Absyn_IntType: 0){ _LL263: _temp262=( void*)(( struct Cyc_Absyn_IntType_struct*)
_temp119)->f1; if( _temp262 == ( void*) Cyc_Absyn_Unsigned){ goto _LL261;} else{
goto _LL159;} _LL261: _temp260=( void*)(( struct Cyc_Absyn_IntType_struct*)
_temp119)->f2; if( _temp260 == ( void*) Cyc_Absyn_B8){ goto _LL259;} else{ goto
_LL159;}} else{ goto _LL159;} _LL259: if( Cyc_Absyndump_to_VC){ goto _LL158;}
else{ goto _LL159;} _LL159: if(( unsigned int) _temp119 >  4u?*(( int*) _temp119)
==  Cyc_Absyn_IntType: 0){ _LL267: _temp266=( void*)(( struct Cyc_Absyn_IntType_struct*)
_temp119)->f1; if( _temp266 == ( void*) Cyc_Absyn_Unsigned){ goto _LL265;} else{
goto _LL161;} _LL265: _temp264=( void*)(( struct Cyc_Absyn_IntType_struct*)
_temp119)->f2; if( _temp264 == ( void*) Cyc_Absyn_B8){ goto _LL160;} else{ goto
_LL161;}} else{ goto _LL161;} _LL161: if( _temp119 == ( void*) Cyc_Absyn_FloatType){
goto _LL162;} else{ goto _LL163;} _LL163: if( _temp119 == ( void*) Cyc_Absyn_DoubleType){
goto _LL164;} else{ goto _LL165;} _LL165: if(( unsigned int) _temp119 >  4u?*((
int*) _temp119) ==  Cyc_Absyn_TupleType: 0){ _LL269: _temp268=(( struct Cyc_Absyn_TupleType_struct*)
_temp119)->f1; goto _LL166;} else{ goto _LL167;} _LL167: if(( unsigned int)
_temp119 >  4u?*(( int*) _temp119) ==  Cyc_Absyn_StructType: 0){ _LL273:
_temp272=(( struct Cyc_Absyn_StructType_struct*) _temp119)->f1; if( _temp272 == 
0){ goto _LL271;} else{ goto _LL169;} _LL271: _temp270=(( struct Cyc_Absyn_StructType_struct*)
_temp119)->f2; goto _LL168;} else{ goto _LL169;} _LL169: if(( unsigned int)
_temp119 >  4u?*(( int*) _temp119) ==  Cyc_Absyn_StructType: 0){ _LL277:
_temp276=(( struct Cyc_Absyn_StructType_struct*) _temp119)->f1; goto _LL275;
_LL275: _temp274=(( struct Cyc_Absyn_StructType_struct*) _temp119)->f2; goto
_LL170;} else{ goto _LL171;} _LL171: if(( unsigned int) _temp119 >  4u?*(( int*)
_temp119) ==  Cyc_Absyn_UnionType: 0){ _LL281: _temp280=(( struct Cyc_Absyn_UnionType_struct*)
_temp119)->f1; if( _temp280 ==  0){ goto _LL279;} else{ goto _LL173;} _LL279:
_temp278=(( struct Cyc_Absyn_UnionType_struct*) _temp119)->f2; goto _LL172;}
else{ goto _LL173;} _LL173: if(( unsigned int) _temp119 >  4u?*(( int*) _temp119)
==  Cyc_Absyn_UnionType: 0){ _LL285: _temp284=(( struct Cyc_Absyn_UnionType_struct*)
_temp119)->f1; goto _LL283; _LL283: _temp282=(( struct Cyc_Absyn_UnionType_struct*)
_temp119)->f2; goto _LL174;} else{ goto _LL175;} _LL175: if(( unsigned int)
_temp119 >  4u?*(( int*) _temp119) ==  Cyc_Absyn_AnonStructType: 0){ _LL287:
_temp286=(( struct Cyc_Absyn_AnonStructType_struct*) _temp119)->f1; goto _LL176;}
else{ goto _LL177;} _LL177: if(( unsigned int) _temp119 >  4u?*(( int*) _temp119)
==  Cyc_Absyn_AnonUnionType: 0){ _LL289: _temp288=(( struct Cyc_Absyn_AnonUnionType_struct*)
_temp119)->f1; goto _LL178;} else{ goto _LL179;} _LL179: if(( unsigned int)
_temp119 >  4u?*(( int*) _temp119) ==  Cyc_Absyn_AnonEnumType: 0){ _LL291:
_temp290=(( struct Cyc_Absyn_AnonEnumType_struct*) _temp119)->f1; goto _LL180;}
else{ goto _LL181;} _LL181: if(( unsigned int) _temp119 >  4u?*(( int*) _temp119)
==  Cyc_Absyn_TypedefType: 0){ _LL297: _temp296=(( struct Cyc_Absyn_TypedefType_struct*)
_temp119)->f1; goto _LL295; _LL295: _temp294=(( struct Cyc_Absyn_TypedefType_struct*)
_temp119)->f2; goto _LL293; _LL293: _temp292=(( struct Cyc_Absyn_TypedefType_struct*)
_temp119)->f3; goto _LL182;} else{ goto _LL183;} _LL183: if(( unsigned int)
_temp119 >  4u?*(( int*) _temp119) ==  Cyc_Absyn_RgnHandleType: 0){ _LL299:
_temp298=( void*)(( struct Cyc_Absyn_RgnHandleType_struct*) _temp119)->f1; goto
_LL184;} else{ goto _LL185;} _LL185: if( _temp119 == ( void*) Cyc_Absyn_HeapRgn){
goto _LL186;} else{ goto _LL187;} _LL187: if(( unsigned int) _temp119 >  4u?*((
int*) _temp119) ==  Cyc_Absyn_AccessEff: 0){ goto _LL188;} else{ goto _LL189;}
_LL189: if(( unsigned int) _temp119 >  4u?*(( int*) _temp119) ==  Cyc_Absyn_RgnsEff:
0){ goto _LL190;} else{ goto _LL191;} _LL191: if(( unsigned int) _temp119 >  4u?*((
int*) _temp119) ==  Cyc_Absyn_JoinEff: 0){ goto _LL192;} else{ goto _LL120;}
_LL122: return; _LL124: return; _LL126: return; _LL128: Cyc_Absyndump_dump(
_tag_arr("void", sizeof( unsigned char), 5u)); return; _LL130: Cyc_Absyndump_dump_str(
_temp193->name); return; _LL132: Cyc_Absyndump_dump( _tag_arr("%", sizeof(
unsigned char), 2u)); if( _temp199 ==  0){ Cyc_Absyndump_dump( _tag_arr("?",
sizeof( unsigned char), 2u));} else{ Cyc_Absyndump_dumpkind(( void*) _temp199->v);}
Cyc_Absyndump_dump(( struct _tagged_arr)({ struct Cyc_Std_Int_pa_struct _temp301;
_temp301.tag= Cyc_Std_Int_pa; _temp301.f1=( int)(( unsigned int) _temp195);{
void* _temp300[ 1u]={& _temp301}; Cyc_Std_aprintf( _tag_arr("(%d)", sizeof(
unsigned char), 5u), _tag_arr( _temp300, sizeof( void*), 1u));}})); return;
_LL134: Cyc_Absyndump_dumpntyp( _temp206); return; _LL136:{ void* _temp302=
_temp216; struct Cyc_Absyn_UnknownTunionInfo _temp308; int _temp310; struct
_tuple0* _temp312; struct Cyc_Absyn_Tuniondecl** _temp314; struct Cyc_Absyn_Tuniondecl*
_temp316; struct Cyc_Absyn_Tuniondecl _temp317; int _temp318; struct _tuple0*
_temp320; _LL304: if(*(( int*) _temp302) ==  Cyc_Absyn_UnknownTunion){ _LL309:
_temp308=(( struct Cyc_Absyn_UnknownTunion_struct*) _temp302)->f1; _LL313:
_temp312= _temp308.name; goto _LL311; _LL311: _temp310= _temp308.is_xtunion;
goto _LL305;} else{ goto _LL306;} _LL306: if(*(( int*) _temp302) ==  Cyc_Absyn_KnownTunion){
_LL315: _temp314=(( struct Cyc_Absyn_KnownTunion_struct*) _temp302)->f1;
_temp316=* _temp314; _temp317=* _temp316; _LL321: _temp320= _temp317.name; goto
_LL319; _LL319: _temp318= _temp317.is_xtunion; goto _LL307;} else{ goto _LL303;}
_LL305: _temp320= _temp312; _temp318= _temp310; goto _LL307; _LL307: if(
_temp318){ Cyc_Absyndump_dump( _tag_arr("xtunion ", sizeof( unsigned char), 9u));}
else{ Cyc_Absyndump_dump( _tag_arr("tunion ", sizeof( unsigned char), 8u));}{
void* _temp322= Cyc_Tcutil_compress( _temp212); _LL324: if( _temp322 == ( void*)
Cyc_Absyn_HeapRgn){ goto _LL325;} else{ goto _LL326;} _LL326: goto _LL327;
_LL325: goto _LL323; _LL327: Cyc_Absyndump_dumptyp( _temp212); Cyc_Absyndump_dump(
_tag_arr(" ", sizeof( unsigned char), 2u)); goto _LL323; _LL323:;} Cyc_Absyndump_dumpqvar(
_temp320); Cyc_Absyndump_dumptps( _temp214); goto _LL303; _LL303:;} goto _LL120;
_LL138:{ void* _temp328= _temp222; struct Cyc_Absyn_UnknownTunionFieldInfo
_temp334; int _temp336; struct _tuple0* _temp338; struct _tuple0* _temp340;
struct Cyc_Absyn_Tunionfield* _temp342; struct Cyc_Absyn_Tunionfield _temp344;
struct _tuple0* _temp345; struct Cyc_Absyn_Tuniondecl* _temp347; struct Cyc_Absyn_Tuniondecl
_temp349; int _temp350; struct _tuple0* _temp352; _LL330: if(*(( int*) _temp328)
==  Cyc_Absyn_UnknownTunionfield){ _LL335: _temp334=(( struct Cyc_Absyn_UnknownTunionfield_struct*)
_temp328)->f1; _LL341: _temp340= _temp334.tunion_name; goto _LL339; _LL339:
_temp338= _temp334.field_name; goto _LL337; _LL337: _temp336= _temp334.is_xtunion;
goto _LL331;} else{ goto _LL332;} _LL332: if(*(( int*) _temp328) ==  Cyc_Absyn_KnownTunionfield){
_LL348: _temp347=(( struct Cyc_Absyn_KnownTunionfield_struct*) _temp328)->f1;
_temp349=* _temp347; _LL353: _temp352= _temp349.name; goto _LL351; _LL351:
_temp350= _temp349.is_xtunion; goto _LL343; _LL343: _temp342=(( struct Cyc_Absyn_KnownTunionfield_struct*)
_temp328)->f2; _temp344=* _temp342; _LL346: _temp345= _temp344.name; goto _LL333;}
else{ goto _LL329;} _LL331: _temp352= _temp340; _temp350= _temp336; _temp345=
_temp338; goto _LL333; _LL333: if( _temp350){ Cyc_Absyndump_dump( _tag_arr("xtunion ",
sizeof( unsigned char), 9u));} else{ Cyc_Absyndump_dump( _tag_arr("tunion ",
sizeof( unsigned char), 8u));} Cyc_Absyndump_dumpqvar( _temp352); Cyc_Absyndump_dump(
_tag_arr(".", sizeof( unsigned char), 2u)); Cyc_Absyndump_dumpqvar( _temp345);
Cyc_Absyndump_dumptps( _temp220); goto _LL329; _LL329:;} goto _LL120; _LL140:
Cyc_Absyndump_dump( _tag_arr("enum ", sizeof( unsigned char), 6u)); Cyc_Absyndump_dumpqvar(
_temp224); return; _LL142: Cyc_Absyndump_dump( _tag_arr("int", sizeof(
unsigned char), 4u)); return; _LL144: Cyc_Absyndump_dump( _tag_arr("signed char",
sizeof( unsigned char), 12u)); return; _LL146: Cyc_Absyndump_dump( _tag_arr("unsigned char",
sizeof( unsigned char), 14u)); return; _LL148: Cyc_Absyndump_dump( _tag_arr("short",
sizeof( unsigned char), 6u)); return; _LL150: Cyc_Absyndump_dump( _tag_arr("unsigned short",
sizeof( unsigned char), 15u)); return; _LL152: Cyc_Absyndump_dump( _tag_arr("unsigned int",
sizeof( unsigned char), 13u)); return; _LL154: Cyc_Absyndump_dump( _tag_arr("__int64",
sizeof( unsigned char), 8u)); return; _LL156: Cyc_Absyndump_dump( _tag_arr("long long",
sizeof( unsigned char), 10u)); return; _LL158: Cyc_Absyndump_dump( _tag_arr("unsigned __int64",
sizeof( unsigned char), 17u)); return; _LL160: Cyc_Absyndump_dump( _tag_arr("unsigned long long",
sizeof( unsigned char), 19u)); return; _LL162: Cyc_Absyndump_dump( _tag_arr("float",
sizeof( unsigned char), 6u)); return; _LL164: Cyc_Absyndump_dump( _tag_arr("double",
sizeof( unsigned char), 7u)); return; _LL166: Cyc_Absyndump_dump_char(( int)'$');
Cyc_Absyndump_dumpargs( _temp268); return; _LL168: Cyc_Absyndump_dump( _tag_arr("struct",
sizeof( unsigned char), 7u)); Cyc_Absyndump_dumptps( _temp270); return; _LL170:
Cyc_Absyndump_dump( _tag_arr("struct", sizeof( unsigned char), 7u)); Cyc_Absyndump_dumpqvar((
struct _tuple0*) _check_null( _temp276)); Cyc_Absyndump_dumptps( _temp274);
return; _LL172: Cyc_Absyndump_dump( _tag_arr("union", sizeof( unsigned char), 6u));
Cyc_Absyndump_dumptps( _temp278); return; _LL174: Cyc_Absyndump_dump( _tag_arr("union",
sizeof( unsigned char), 6u)); Cyc_Absyndump_dumpqvar(( struct _tuple0*)
_check_null( _temp284)); Cyc_Absyndump_dumptps( _temp282); return; _LL176: Cyc_Absyndump_dump(
_tag_arr("struct {", sizeof( unsigned char), 9u)); Cyc_Absyndump_dumpstructfields(
_temp286); Cyc_Absyndump_dump( _tag_arr("}", sizeof( unsigned char), 2u));
return; _LL178: Cyc_Absyndump_dump( _tag_arr("union {", sizeof( unsigned char),
8u)); Cyc_Absyndump_dumpstructfields( _temp288); Cyc_Absyndump_dump( _tag_arr("}",
sizeof( unsigned char), 2u)); return; _LL180: Cyc_Absyndump_dump( _tag_arr("enum {",
sizeof( unsigned char), 7u)); Cyc_Absyndump_dumpenumfields( _temp290); Cyc_Absyndump_dump(
_tag_arr("}", sizeof( unsigned char), 2u)); return; _LL182:( Cyc_Absyndump_dumpqvar(
_temp296), Cyc_Absyndump_dumptps( _temp294)); return; _LL184: Cyc_Absyndump_dumprgn(
_temp298); return; _LL186: return; _LL188: return; _LL190: return; _LL192:
return; _LL120:;} void Cyc_Absyndump_dumpvaropt( struct Cyc_Core_Opt* vo){ if(
vo !=  0){ Cyc_Absyndump_dump_str(( struct _tagged_arr*) vo->v);}} void Cyc_Absyndump_dumpfunarg(
struct _tuple1* t){(( void(*)( struct Cyc_Absyn_Tqual, void*, void(* f)( struct
Cyc_Core_Opt*), struct Cyc_Core_Opt*)) Cyc_Absyndump_dumptqtd)((* t).f2,(* t).f3,
Cyc_Absyndump_dumpvaropt,(* t).f1);} struct _tuple7{ void* f1; void* f2; } ;
void Cyc_Absyndump_dump_rgncmp( struct _tuple7* cmp){ struct _tuple7 _temp356;
void* _temp357; void* _temp359; struct _tuple7* _temp354= cmp; _temp356=*
_temp354; _LL360: _temp359= _temp356.f1; goto _LL358; _LL358: _temp357= _temp356.f2;
goto _LL355; _LL355: Cyc_Absyndump_dumptyp( _temp359); Cyc_Absyndump_dump_char((
int)'<'); Cyc_Absyndump_dumptyp( _temp357);} void Cyc_Absyndump_dump_rgnpo(
struct Cyc_List_List* rgn_po){(( void(*)( void(* f)( struct _tuple7*), struct
Cyc_List_List* l, struct _tagged_arr sep)) Cyc_Absyndump_dump_sep)( Cyc_Absyndump_dump_rgncmp,
rgn_po, _tag_arr(",", sizeof( unsigned char), 2u));} void Cyc_Absyndump_dumpfunargs(
struct Cyc_List_List* args, int c_varargs, struct Cyc_Absyn_VarargInfo*
cyc_varargs, struct Cyc_Core_Opt* effopt, struct Cyc_List_List* rgn_po){ Cyc_Absyndump_dump_char((
int)'('); for( 0; args !=  0; args= args->tl){ Cyc_Absyndump_dumpfunarg(( struct
_tuple1*) args->hd); if(( args->tl !=  0? 1: c_varargs)? 1: cyc_varargs !=  0){
Cyc_Absyndump_dump_char(( int)',');}} if( c_varargs){ Cyc_Absyndump_dump(
_tag_arr("...", sizeof( unsigned char), 4u));} else{ if( cyc_varargs !=  0){
struct _tuple1* _temp361=({ struct _tuple1* _temp362=( struct _tuple1*)
_cycalloc( sizeof( struct _tuple1)); _temp362->f1= cyc_varargs->name; _temp362->f2=
cyc_varargs->tq; _temp362->f3=( void*) cyc_varargs->type; _temp362;}); Cyc_Absyndump_dump(
_tag_arr("...", sizeof( unsigned char), 4u)); if( cyc_varargs->inject){ Cyc_Absyndump_dump(
_tag_arr(" inject ", sizeof( unsigned char), 9u));} Cyc_Absyndump_dumpfunarg(
_temp361);}} if( effopt !=  0){ Cyc_Absyndump_dump_semi(); Cyc_Absyndump_dumpeff((
void*) effopt->v);} if( rgn_po !=  0){ Cyc_Absyndump_dump_char(( int)':'); Cyc_Absyndump_dump_rgnpo(
rgn_po);} Cyc_Absyndump_dump_char(( int)')');} void Cyc_Absyndump_dumptyp( void*
t){(( void(*)( struct Cyc_Absyn_Tqual, void*, void(* f)( int), int)) Cyc_Absyndump_dumptqtd)(({
struct Cyc_Absyn_Tqual _temp363; _temp363.q_const= 0; _temp363.q_volatile= 0;
_temp363.q_restrict= 0; _temp363;}), t,( void(*)( int x)) Cyc_Absyndump_ignore,
0);} void Cyc_Absyndump_dumpdesignator( void* d){ void* _temp364= d; struct Cyc_Absyn_Exp*
_temp370; struct _tagged_arr* _temp372; _LL366: if(*(( int*) _temp364) ==  Cyc_Absyn_ArrayElement){
_LL371: _temp370=(( struct Cyc_Absyn_ArrayElement_struct*) _temp364)->f1; goto
_LL367;} else{ goto _LL368;} _LL368: if(*(( int*) _temp364) ==  Cyc_Absyn_FieldName){
_LL373: _temp372=(( struct Cyc_Absyn_FieldName_struct*) _temp364)->f1; goto
_LL369;} else{ goto _LL365;} _LL367: Cyc_Absyndump_dump( _tag_arr(".[", sizeof(
unsigned char), 3u)); Cyc_Absyndump_dumpexp( _temp370); Cyc_Absyndump_dump_char((
int)']'); goto _LL365; _LL369: Cyc_Absyndump_dump_char(( int)'.'); Cyc_Absyndump_dump_nospace(*
_temp372); goto _LL365; _LL365:;} struct _tuple8{ struct Cyc_List_List* f1;
struct Cyc_Absyn_Exp* f2; } ; void Cyc_Absyndump_dumpde( struct _tuple8* de){
Cyc_Absyndump_egroup( Cyc_Absyndump_dumpdesignator,(* de).f1, _tag_arr("",
sizeof( unsigned char), 1u), _tag_arr("=", sizeof( unsigned char), 2u), _tag_arr("=",
sizeof( unsigned char), 2u)); Cyc_Absyndump_dumpexp((* de).f2);} void Cyc_Absyndump_dumpexps_prec(
int inprec, struct Cyc_List_List* es){(( void(*)( void(* f)( int, struct Cyc_Absyn_Exp*),
int env, struct Cyc_List_List* l, struct _tagged_arr start, struct _tagged_arr
end, struct _tagged_arr sep)) Cyc_Absyndump_group_c)( Cyc_Absyndump_dumpexp_prec,
inprec, es, _tag_arr("", sizeof( unsigned char), 1u), _tag_arr("", sizeof(
unsigned char), 1u), _tag_arr(",", sizeof( unsigned char), 2u));} void Cyc_Absyndump_dumpexp_prec(
int inprec, struct Cyc_Absyn_Exp* e){ int myprec= Cyc_Absynpp_exp_prec( e); if(
inprec >=  myprec){ Cyc_Absyndump_dump_nospace( _tag_arr("(", sizeof(
unsigned char), 2u));}{ void* _temp374=( void*) e->r; void* _temp474;
unsigned char _temp476; void* _temp478; void* _temp480; short _temp482; void*
_temp484; void* _temp486; int _temp488; void* _temp490; void* _temp492; int
_temp494; void* _temp496; void* _temp498; long long _temp500; void* _temp502;
void* _temp504; struct _tagged_arr _temp506; void* _temp508; void* _temp510;
struct _tagged_arr _temp512; struct _tuple0* _temp514; struct _tuple0* _temp516;
struct Cyc_List_List* _temp518; void* _temp520; struct Cyc_Absyn_Exp* _temp522;
struct Cyc_Core_Opt* _temp524; struct Cyc_Absyn_Exp* _temp526; void* _temp528;
struct Cyc_Absyn_Exp* _temp530; void* _temp532; struct Cyc_Absyn_Exp* _temp534;
void* _temp536; struct Cyc_Absyn_Exp* _temp538; void* _temp540; struct Cyc_Absyn_Exp*
_temp542; struct Cyc_Absyn_Exp* _temp544; struct Cyc_Absyn_Exp* _temp546; struct
Cyc_Absyn_Exp* _temp548; struct Cyc_Absyn_Exp* _temp550; struct Cyc_Absyn_Exp*
_temp552; struct Cyc_List_List* _temp554; struct Cyc_Absyn_Exp* _temp556; struct
Cyc_List_List* _temp558; struct Cyc_Absyn_Exp* _temp560; struct Cyc_Absyn_Exp*
_temp562; struct Cyc_Absyn_Exp* _temp564; struct Cyc_Absyn_Exp* _temp566; struct
Cyc_Absyn_Exp* _temp568; void* _temp570; struct Cyc_Absyn_Exp* _temp572; struct
Cyc_Absyn_Exp* _temp574; struct Cyc_Absyn_Exp* _temp576; void* _temp578; struct
Cyc_Absyn_Exp* _temp580; void* _temp582; struct _tagged_arr* _temp584; void*
_temp586; void* _temp588; unsigned int _temp590; void* _temp592; void* _temp594;
struct Cyc_List_List* _temp596; struct Cyc_Absyn_Exp* _temp598; struct
_tagged_arr* _temp600; struct Cyc_Absyn_Exp* _temp602; struct _tagged_arr*
_temp604; struct Cyc_Absyn_Exp* _temp606; struct Cyc_Absyn_Exp* _temp608; struct
Cyc_Absyn_Exp* _temp610; struct Cyc_List_List* _temp612; struct Cyc_List_List*
_temp614; struct _tuple1* _temp616; struct Cyc_List_List* _temp618; struct Cyc_Absyn_Exp*
_temp620; struct Cyc_Absyn_Exp* _temp622; struct Cyc_Absyn_Vardecl* _temp624;
struct Cyc_List_List* _temp626; struct _tuple0* _temp628; struct Cyc_List_List*
_temp630; struct Cyc_Absyn_Tunionfield* _temp632; struct Cyc_List_List* _temp634;
struct _tuple0* _temp636; struct _tuple0* _temp638; void* _temp640; struct Cyc_Absyn_Exp*
_temp642; struct Cyc_List_List* _temp644; struct Cyc_Core_Opt* _temp646; struct
Cyc_Absyn_Stmt* _temp648; struct Cyc_Absyn_Fndecl* _temp650; struct Cyc_Absyn_Exp*
_temp652; _LL376: if(*(( int*) _temp374) ==  Cyc_Absyn_Const_e){ _LL475:
_temp474=( void*)(( struct Cyc_Absyn_Const_e_struct*) _temp374)->f1; if((
unsigned int) _temp474 >  1u?*(( int*) _temp474) ==  Cyc_Absyn_Char_c: 0){
_LL479: _temp478=( void*)(( struct Cyc_Absyn_Char_c_struct*) _temp474)->f1; goto
_LL477; _LL477: _temp476=(( struct Cyc_Absyn_Char_c_struct*) _temp474)->f2; goto
_LL377;} else{ goto _LL378;}} else{ goto _LL378;} _LL378: if(*(( int*) _temp374)
==  Cyc_Absyn_Const_e){ _LL481: _temp480=( void*)(( struct Cyc_Absyn_Const_e_struct*)
_temp374)->f1; if(( unsigned int) _temp480 >  1u?*(( int*) _temp480) ==  Cyc_Absyn_Short_c:
0){ _LL485: _temp484=( void*)(( struct Cyc_Absyn_Short_c_struct*) _temp480)->f1;
goto _LL483; _LL483: _temp482=(( struct Cyc_Absyn_Short_c_struct*) _temp480)->f2;
goto _LL379;} else{ goto _LL380;}} else{ goto _LL380;} _LL380: if(*(( int*)
_temp374) ==  Cyc_Absyn_Const_e){ _LL487: _temp486=( void*)(( struct Cyc_Absyn_Const_e_struct*)
_temp374)->f1; if(( unsigned int) _temp486 >  1u?*(( int*) _temp486) ==  Cyc_Absyn_Int_c:
0){ _LL491: _temp490=( void*)(( struct Cyc_Absyn_Int_c_struct*) _temp486)->f1;
if( _temp490 == ( void*) Cyc_Absyn_Signed){ goto _LL489;} else{ goto _LL382;}
_LL489: _temp488=(( struct Cyc_Absyn_Int_c_struct*) _temp486)->f2; goto _LL381;}
else{ goto _LL382;}} else{ goto _LL382;} _LL382: if(*(( int*) _temp374) ==  Cyc_Absyn_Const_e){
_LL493: _temp492=( void*)(( struct Cyc_Absyn_Const_e_struct*) _temp374)->f1; if((
unsigned int) _temp492 >  1u?*(( int*) _temp492) ==  Cyc_Absyn_Int_c: 0){ _LL497:
_temp496=( void*)(( struct Cyc_Absyn_Int_c_struct*) _temp492)->f1; if( _temp496
== ( void*) Cyc_Absyn_Unsigned){ goto _LL495;} else{ goto _LL384;} _LL495:
_temp494=(( struct Cyc_Absyn_Int_c_struct*) _temp492)->f2; goto _LL383;} else{
goto _LL384;}} else{ goto _LL384;} _LL384: if(*(( int*) _temp374) ==  Cyc_Absyn_Const_e){
_LL499: _temp498=( void*)(( struct Cyc_Absyn_Const_e_struct*) _temp374)->f1; if((
unsigned int) _temp498 >  1u?*(( int*) _temp498) ==  Cyc_Absyn_LongLong_c: 0){
_LL503: _temp502=( void*)(( struct Cyc_Absyn_LongLong_c_struct*) _temp498)->f1;
goto _LL501; _LL501: _temp500=(( struct Cyc_Absyn_LongLong_c_struct*) _temp498)->f2;
goto _LL385;} else{ goto _LL386;}} else{ goto _LL386;} _LL386: if(*(( int*)
_temp374) ==  Cyc_Absyn_Const_e){ _LL505: _temp504=( void*)(( struct Cyc_Absyn_Const_e_struct*)
_temp374)->f1; if(( unsigned int) _temp504 >  1u?*(( int*) _temp504) ==  Cyc_Absyn_Float_c:
0){ _LL507: _temp506=(( struct Cyc_Absyn_Float_c_struct*) _temp504)->f1; goto
_LL387;} else{ goto _LL388;}} else{ goto _LL388;} _LL388: if(*(( int*) _temp374)
==  Cyc_Absyn_Const_e){ _LL509: _temp508=( void*)(( struct Cyc_Absyn_Const_e_struct*)
_temp374)->f1; if( _temp508 == ( void*) Cyc_Absyn_Null_c){ goto _LL389;} else{
goto _LL390;}} else{ goto _LL390;} _LL390: if(*(( int*) _temp374) ==  Cyc_Absyn_Const_e){
_LL511: _temp510=( void*)(( struct Cyc_Absyn_Const_e_struct*) _temp374)->f1; if((
unsigned int) _temp510 >  1u?*(( int*) _temp510) ==  Cyc_Absyn_String_c: 0){
_LL513: _temp512=(( struct Cyc_Absyn_String_c_struct*) _temp510)->f1; goto
_LL391;} else{ goto _LL392;}} else{ goto _LL392;} _LL392: if(*(( int*) _temp374)
==  Cyc_Absyn_UnknownId_e){ _LL515: _temp514=(( struct Cyc_Absyn_UnknownId_e_struct*)
_temp374)->f1; goto _LL393;} else{ goto _LL394;} _LL394: if(*(( int*) _temp374)
==  Cyc_Absyn_Var_e){ _LL517: _temp516=(( struct Cyc_Absyn_Var_e_struct*)
_temp374)->f1; goto _LL395;} else{ goto _LL396;} _LL396: if(*(( int*) _temp374)
==  Cyc_Absyn_Primop_e){ _LL521: _temp520=( void*)(( struct Cyc_Absyn_Primop_e_struct*)
_temp374)->f1; goto _LL519; _LL519: _temp518=(( struct Cyc_Absyn_Primop_e_struct*)
_temp374)->f2; goto _LL397;} else{ goto _LL398;} _LL398: if(*(( int*) _temp374)
==  Cyc_Absyn_AssignOp_e){ _LL527: _temp526=(( struct Cyc_Absyn_AssignOp_e_struct*)
_temp374)->f1; goto _LL525; _LL525: _temp524=(( struct Cyc_Absyn_AssignOp_e_struct*)
_temp374)->f2; goto _LL523; _LL523: _temp522=(( struct Cyc_Absyn_AssignOp_e_struct*)
_temp374)->f3; goto _LL399;} else{ goto _LL400;} _LL400: if(*(( int*) _temp374)
==  Cyc_Absyn_Increment_e){ _LL531: _temp530=(( struct Cyc_Absyn_Increment_e_struct*)
_temp374)->f1; goto _LL529; _LL529: _temp528=( void*)(( struct Cyc_Absyn_Increment_e_struct*)
_temp374)->f2; if( _temp528 == ( void*) Cyc_Absyn_PreInc){ goto _LL401;} else{
goto _LL402;}} else{ goto _LL402;} _LL402: if(*(( int*) _temp374) ==  Cyc_Absyn_Increment_e){
_LL535: _temp534=(( struct Cyc_Absyn_Increment_e_struct*) _temp374)->f1; goto
_LL533; _LL533: _temp532=( void*)(( struct Cyc_Absyn_Increment_e_struct*)
_temp374)->f2; if( _temp532 == ( void*) Cyc_Absyn_PreDec){ goto _LL403;} else{
goto _LL404;}} else{ goto _LL404;} _LL404: if(*(( int*) _temp374) ==  Cyc_Absyn_Increment_e){
_LL539: _temp538=(( struct Cyc_Absyn_Increment_e_struct*) _temp374)->f1; goto
_LL537; _LL537: _temp536=( void*)(( struct Cyc_Absyn_Increment_e_struct*)
_temp374)->f2; if( _temp536 == ( void*) Cyc_Absyn_PostInc){ goto _LL405;} else{
goto _LL406;}} else{ goto _LL406;} _LL406: if(*(( int*) _temp374) ==  Cyc_Absyn_Increment_e){
_LL543: _temp542=(( struct Cyc_Absyn_Increment_e_struct*) _temp374)->f1; goto
_LL541; _LL541: _temp540=( void*)(( struct Cyc_Absyn_Increment_e_struct*)
_temp374)->f2; if( _temp540 == ( void*) Cyc_Absyn_PostDec){ goto _LL407;} else{
goto _LL408;}} else{ goto _LL408;} _LL408: if(*(( int*) _temp374) ==  Cyc_Absyn_Conditional_e){
_LL549: _temp548=(( struct Cyc_Absyn_Conditional_e_struct*) _temp374)->f1; goto
_LL547; _LL547: _temp546=(( struct Cyc_Absyn_Conditional_e_struct*) _temp374)->f2;
goto _LL545; _LL545: _temp544=(( struct Cyc_Absyn_Conditional_e_struct*)
_temp374)->f3; goto _LL409;} else{ goto _LL410;} _LL410: if(*(( int*) _temp374)
==  Cyc_Absyn_SeqExp_e){ _LL553: _temp552=(( struct Cyc_Absyn_SeqExp_e_struct*)
_temp374)->f1; goto _LL551; _LL551: _temp550=(( struct Cyc_Absyn_SeqExp_e_struct*)
_temp374)->f2; goto _LL411;} else{ goto _LL412;} _LL412: if(*(( int*) _temp374)
==  Cyc_Absyn_UnknownCall_e){ _LL557: _temp556=(( struct Cyc_Absyn_UnknownCall_e_struct*)
_temp374)->f1; goto _LL555; _LL555: _temp554=(( struct Cyc_Absyn_UnknownCall_e_struct*)
_temp374)->f2; goto _LL413;} else{ goto _LL414;} _LL414: if(*(( int*) _temp374)
==  Cyc_Absyn_FnCall_e){ _LL561: _temp560=(( struct Cyc_Absyn_FnCall_e_struct*)
_temp374)->f1; goto _LL559; _LL559: _temp558=(( struct Cyc_Absyn_FnCall_e_struct*)
_temp374)->f2; goto _LL415;} else{ goto _LL416;} _LL416: if(*(( int*) _temp374)
==  Cyc_Absyn_Throw_e){ _LL563: _temp562=(( struct Cyc_Absyn_Throw_e_struct*)
_temp374)->f1; goto _LL417;} else{ goto _LL418;} _LL418: if(*(( int*) _temp374)
==  Cyc_Absyn_NoInstantiate_e){ _LL565: _temp564=(( struct Cyc_Absyn_NoInstantiate_e_struct*)
_temp374)->f1; goto _LL419;} else{ goto _LL420;} _LL420: if(*(( int*) _temp374)
==  Cyc_Absyn_Instantiate_e){ _LL567: _temp566=(( struct Cyc_Absyn_Instantiate_e_struct*)
_temp374)->f1; goto _LL421;} else{ goto _LL422;} _LL422: if(*(( int*) _temp374)
==  Cyc_Absyn_Cast_e){ _LL571: _temp570=( void*)(( struct Cyc_Absyn_Cast_e_struct*)
_temp374)->f1; goto _LL569; _LL569: _temp568=(( struct Cyc_Absyn_Cast_e_struct*)
_temp374)->f2; goto _LL423;} else{ goto _LL424;} _LL424: if(*(( int*) _temp374)
==  Cyc_Absyn_Address_e){ _LL573: _temp572=(( struct Cyc_Absyn_Address_e_struct*)
_temp374)->f1; goto _LL425;} else{ goto _LL426;} _LL426: if(*(( int*) _temp374)
==  Cyc_Absyn_New_e){ _LL577: _temp576=(( struct Cyc_Absyn_New_e_struct*)
_temp374)->f1; goto _LL575; _LL575: _temp574=(( struct Cyc_Absyn_New_e_struct*)
_temp374)->f2; goto _LL427;} else{ goto _LL428;} _LL428: if(*(( int*) _temp374)
==  Cyc_Absyn_Sizeoftyp_e){ _LL579: _temp578=( void*)(( struct Cyc_Absyn_Sizeoftyp_e_struct*)
_temp374)->f1; goto _LL429;} else{ goto _LL430;} _LL430: if(*(( int*) _temp374)
==  Cyc_Absyn_Sizeofexp_e){ _LL581: _temp580=(( struct Cyc_Absyn_Sizeofexp_e_struct*)
_temp374)->f1; goto _LL431;} else{ goto _LL432;} _LL432: if(*(( int*) _temp374)
==  Cyc_Absyn_Offsetof_e){ _LL587: _temp586=( void*)(( struct Cyc_Absyn_Offsetof_e_struct*)
_temp374)->f1; goto _LL583; _LL583: _temp582=( void*)(( struct Cyc_Absyn_Offsetof_e_struct*)
_temp374)->f2; if(*(( int*) _temp582) ==  Cyc_Absyn_StructField){ _LL585:
_temp584=(( struct Cyc_Absyn_StructField_struct*) _temp582)->f1; goto _LL433;}
else{ goto _LL434;}} else{ goto _LL434;} _LL434: if(*(( int*) _temp374) ==  Cyc_Absyn_Offsetof_e){
_LL593: _temp592=( void*)(( struct Cyc_Absyn_Offsetof_e_struct*) _temp374)->f1;
goto _LL589; _LL589: _temp588=( void*)(( struct Cyc_Absyn_Offsetof_e_struct*)
_temp374)->f2; if(*(( int*) _temp588) ==  Cyc_Absyn_TupleIndex){ _LL591:
_temp590=(( struct Cyc_Absyn_TupleIndex_struct*) _temp588)->f1; goto _LL435;}
else{ goto _LL436;}} else{ goto _LL436;} _LL436: if(*(( int*) _temp374) ==  Cyc_Absyn_Gentyp_e){
_LL597: _temp596=(( struct Cyc_Absyn_Gentyp_e_struct*) _temp374)->f1; goto
_LL595; _LL595: _temp594=( void*)(( struct Cyc_Absyn_Gentyp_e_struct*) _temp374)->f2;
goto _LL437;} else{ goto _LL438;} _LL438: if(*(( int*) _temp374) ==  Cyc_Absyn_Deref_e){
_LL599: _temp598=(( struct Cyc_Absyn_Deref_e_struct*) _temp374)->f1; goto _LL439;}
else{ goto _LL440;} _LL440: if(*(( int*) _temp374) ==  Cyc_Absyn_StructMember_e){
_LL603: _temp602=(( struct Cyc_Absyn_StructMember_e_struct*) _temp374)->f1; goto
_LL601; _LL601: _temp600=(( struct Cyc_Absyn_StructMember_e_struct*) _temp374)->f2;
goto _LL441;} else{ goto _LL442;} _LL442: if(*(( int*) _temp374) ==  Cyc_Absyn_StructArrow_e){
_LL607: _temp606=(( struct Cyc_Absyn_StructArrow_e_struct*) _temp374)->f1; goto
_LL605; _LL605: _temp604=(( struct Cyc_Absyn_StructArrow_e_struct*) _temp374)->f2;
goto _LL443;} else{ goto _LL444;} _LL444: if(*(( int*) _temp374) ==  Cyc_Absyn_Subscript_e){
_LL611: _temp610=(( struct Cyc_Absyn_Subscript_e_struct*) _temp374)->f1; goto
_LL609; _LL609: _temp608=(( struct Cyc_Absyn_Subscript_e_struct*) _temp374)->f2;
goto _LL445;} else{ goto _LL446;} _LL446: if(*(( int*) _temp374) ==  Cyc_Absyn_Tuple_e){
_LL613: _temp612=(( struct Cyc_Absyn_Tuple_e_struct*) _temp374)->f1; goto _LL447;}
else{ goto _LL448;} _LL448: if(*(( int*) _temp374) ==  Cyc_Absyn_CompoundLit_e){
_LL617: _temp616=(( struct Cyc_Absyn_CompoundLit_e_struct*) _temp374)->f1; goto
_LL615; _LL615: _temp614=(( struct Cyc_Absyn_CompoundLit_e_struct*) _temp374)->f2;
goto _LL449;} else{ goto _LL450;} _LL450: if(*(( int*) _temp374) ==  Cyc_Absyn_Array_e){
_LL619: _temp618=(( struct Cyc_Absyn_Array_e_struct*) _temp374)->f1; goto _LL451;}
else{ goto _LL452;} _LL452: if(*(( int*) _temp374) ==  Cyc_Absyn_Comprehension_e){
_LL625: _temp624=(( struct Cyc_Absyn_Comprehension_e_struct*) _temp374)->f1;
goto _LL623; _LL623: _temp622=(( struct Cyc_Absyn_Comprehension_e_struct*)
_temp374)->f2; goto _LL621; _LL621: _temp620=(( struct Cyc_Absyn_Comprehension_e_struct*)
_temp374)->f3; goto _LL453;} else{ goto _LL454;} _LL454: if(*(( int*) _temp374)
==  Cyc_Absyn_Struct_e){ _LL629: _temp628=(( struct Cyc_Absyn_Struct_e_struct*)
_temp374)->f1; goto _LL627; _LL627: _temp626=(( struct Cyc_Absyn_Struct_e_struct*)
_temp374)->f3; goto _LL455;} else{ goto _LL456;} _LL456: if(*(( int*) _temp374)
==  Cyc_Absyn_AnonStruct_e){ _LL631: _temp630=(( struct Cyc_Absyn_AnonStruct_e_struct*)
_temp374)->f2; goto _LL457;} else{ goto _LL458;} _LL458: if(*(( int*) _temp374)
==  Cyc_Absyn_Tunion_e){ _LL635: _temp634=(( struct Cyc_Absyn_Tunion_e_struct*)
_temp374)->f3; goto _LL633; _LL633: _temp632=(( struct Cyc_Absyn_Tunion_e_struct*)
_temp374)->f5; goto _LL459;} else{ goto _LL460;} _LL460: if(*(( int*) _temp374)
==  Cyc_Absyn_Enum_e){ _LL637: _temp636=(( struct Cyc_Absyn_Enum_e_struct*)
_temp374)->f1; goto _LL461;} else{ goto _LL462;} _LL462: if(*(( int*) _temp374)
==  Cyc_Absyn_AnonEnum_e){ _LL639: _temp638=(( struct Cyc_Absyn_AnonEnum_e_struct*)
_temp374)->f1; goto _LL463;} else{ goto _LL464;} _LL464: if(*(( int*) _temp374)
==  Cyc_Absyn_Malloc_e){ _LL643: _temp642=(( struct Cyc_Absyn_Malloc_e_struct*)
_temp374)->f1; goto _LL641; _LL641: _temp640=( void*)(( struct Cyc_Absyn_Malloc_e_struct*)
_temp374)->f2; goto _LL465;} else{ goto _LL466;} _LL466: if(*(( int*) _temp374)
==  Cyc_Absyn_UnresolvedMem_e){ _LL647: _temp646=(( struct Cyc_Absyn_UnresolvedMem_e_struct*)
_temp374)->f1; goto _LL645; _LL645: _temp644=(( struct Cyc_Absyn_UnresolvedMem_e_struct*)
_temp374)->f2; goto _LL467;} else{ goto _LL468;} _LL468: if(*(( int*) _temp374)
==  Cyc_Absyn_StmtExp_e){ _LL649: _temp648=(( struct Cyc_Absyn_StmtExp_e_struct*)
_temp374)->f1; goto _LL469;} else{ goto _LL470;} _LL470: if(*(( int*) _temp374)
==  Cyc_Absyn_Codegen_e){ _LL651: _temp650=(( struct Cyc_Absyn_Codegen_e_struct*)
_temp374)->f1; goto _LL471;} else{ goto _LL472;} _LL472: if(*(( int*) _temp374)
==  Cyc_Absyn_Fill_e){ _LL653: _temp652=(( struct Cyc_Absyn_Fill_e_struct*)
_temp374)->f1; goto _LL473;} else{ goto _LL375;} _LL377: Cyc_Absyndump_dump_char((
int)'\''); Cyc_Absyndump_dump_nospace( Cyc_Absynpp_char_escape( _temp476)); Cyc_Absyndump_dump_char((
int)'\''); goto _LL375; _LL379: Cyc_Absyndump_dump(( struct _tagged_arr)({
struct Cyc_Std_Int_pa_struct _temp655; _temp655.tag= Cyc_Std_Int_pa; _temp655.f1=(
int)(( unsigned int)(( int) _temp482));{ void* _temp654[ 1u]={& _temp655}; Cyc_Std_aprintf(
_tag_arr("%d", sizeof( unsigned char), 3u), _tag_arr( _temp654, sizeof( void*),
1u));}})); goto _LL375; _LL381: Cyc_Absyndump_dump(( struct _tagged_arr)({
struct Cyc_Std_Int_pa_struct _temp657; _temp657.tag= Cyc_Std_Int_pa; _temp657.f1=(
int)(( unsigned int) _temp488);{ void* _temp656[ 1u]={& _temp657}; Cyc_Std_aprintf(
_tag_arr("%d", sizeof( unsigned char), 3u), _tag_arr( _temp656, sizeof( void*),
1u));}})); goto _LL375; _LL383: Cyc_Absyndump_dump(( struct _tagged_arr)({
struct Cyc_Std_Int_pa_struct _temp659; _temp659.tag= Cyc_Std_Int_pa; _temp659.f1=(
int)(( unsigned int) _temp494);{ void* _temp658[ 1u]={& _temp659}; Cyc_Std_aprintf(
_tag_arr("%d", sizeof( unsigned char), 3u), _tag_arr( _temp658, sizeof( void*),
1u));}})); Cyc_Absyndump_dump_nospace( _tag_arr("u", sizeof( unsigned char), 2u));
goto _LL375; _LL385: Cyc_Absyndump_dump( _tag_arr("<<FIX LONG LONG CONSTANT>>",
sizeof( unsigned char), 27u)); goto _LL375; _LL387: Cyc_Absyndump_dump( _temp506);
goto _LL375; _LL389: Cyc_Absyndump_dump( _tag_arr("NULL", sizeof( unsigned char),
5u)); goto _LL375; _LL391: Cyc_Absyndump_dump_char(( int)'"'); Cyc_Absyndump_dump_nospace(
Cyc_Absynpp_string_escape( _temp512)); Cyc_Absyndump_dump_char(( int)'"'); goto
_LL375; _LL393: _temp516= _temp514; goto _LL395; _LL395: Cyc_Absyndump_dumpqvar(
_temp516); goto _LL375; _LL397: { struct _tagged_arr _temp660= Cyc_Absynpp_prim2str(
_temp520); switch((( int(*)( struct Cyc_List_List* x)) Cyc_List_length)(
_temp518)){ case 1: _LL661: if( _temp520 == ( void*) Cyc_Absyn_Size){ Cyc_Absyndump_dumpexp_prec(
myprec,( struct Cyc_Absyn_Exp*)(( struct Cyc_List_List*) _check_null( _temp518))->hd);
Cyc_Absyndump_dump( _tag_arr(".size", sizeof( unsigned char), 6u));} else{ Cyc_Absyndump_dump(
_temp660); Cyc_Absyndump_dumpexp_prec( myprec,( struct Cyc_Absyn_Exp*)(( struct
Cyc_List_List*) _check_null( _temp518))->hd);} break; case 2: _LL662: Cyc_Absyndump_dumpexp_prec(
myprec,( struct Cyc_Absyn_Exp*)(( struct Cyc_List_List*) _check_null( _temp518))->hd);
Cyc_Absyndump_dump( _temp660); Cyc_Absyndump_dump_char(( int)' '); Cyc_Absyndump_dumpexp_prec(
myprec,( struct Cyc_Absyn_Exp*)(( struct Cyc_List_List*) _check_null( _temp518->tl))->hd);
break; default: _LL663:( int) _throw(( void*)({ struct Cyc_Core_Failure_struct*
_temp665=( struct Cyc_Core_Failure_struct*) _cycalloc( sizeof( struct Cyc_Core_Failure_struct));
_temp665[ 0]=({ struct Cyc_Core_Failure_struct _temp666; _temp666.tag= Cyc_Core_Failure;
_temp666.f1= _tag_arr("Absyndump -- Bad number of arguments to primop", sizeof(
unsigned char), 47u); _temp666;}); _temp665;}));} goto _LL375;} _LL399: Cyc_Absyndump_dumpexp_prec(
myprec, _temp526); if( _temp524 !=  0){ Cyc_Absyndump_dump( Cyc_Absynpp_prim2str((
void*) _temp524->v));} Cyc_Absyndump_dump_nospace( _tag_arr("=", sizeof(
unsigned char), 2u)); Cyc_Absyndump_dumpexp_prec( myprec, _temp522); goto _LL375;
_LL401: Cyc_Absyndump_dump( _tag_arr("++", sizeof( unsigned char), 3u)); Cyc_Absyndump_dumpexp_prec(
myprec, _temp530); goto _LL375; _LL403: Cyc_Absyndump_dump( _tag_arr("--",
sizeof( unsigned char), 3u)); Cyc_Absyndump_dumpexp_prec( myprec, _temp534);
goto _LL375; _LL405: Cyc_Absyndump_dumpexp_prec( myprec, _temp538); Cyc_Absyndump_dump(
_tag_arr("++", sizeof( unsigned char), 3u)); goto _LL375; _LL407: Cyc_Absyndump_dumpexp_prec(
myprec, _temp542); Cyc_Absyndump_dump( _tag_arr("--", sizeof( unsigned char), 3u));
goto _LL375; _LL409: Cyc_Absyndump_dumpexp_prec( myprec, _temp548); Cyc_Absyndump_dump_char((
int)'?'); Cyc_Absyndump_dumpexp_prec( 0, _temp546); Cyc_Absyndump_dump_char((
int)':'); Cyc_Absyndump_dumpexp_prec( myprec, _temp544); goto _LL375; _LL411:
Cyc_Absyndump_dump_char(( int)'('); Cyc_Absyndump_dumpexp_prec( myprec, _temp552);
Cyc_Absyndump_dump_char(( int)','); Cyc_Absyndump_dumpexp_prec( myprec, _temp550);
Cyc_Absyndump_dump_char(( int)')'); goto _LL375; _LL413: _temp560= _temp556;
_temp558= _temp554; goto _LL415; _LL415: Cyc_Absyndump_dumpexp_prec( myprec,
_temp560); Cyc_Absyndump_dump_nospace( _tag_arr("(", sizeof( unsigned char), 2u));
Cyc_Absyndump_dumpexps_prec( 20, _temp558); Cyc_Absyndump_dump_nospace( _tag_arr(")",
sizeof( unsigned char), 2u)); goto _LL375; _LL417: Cyc_Absyndump_dump( _tag_arr("throw",
sizeof( unsigned char), 6u)); Cyc_Absyndump_dumpexp_prec( myprec, _temp562);
goto _LL375; _LL419: _temp566= _temp564; goto _LL421; _LL421: Cyc_Absyndump_dumpexp_prec(
inprec, _temp566); goto _LL375; _LL423: Cyc_Absyndump_dump_char(( int)'('); Cyc_Absyndump_dumptyp(
_temp570); Cyc_Absyndump_dump_char(( int)')'); Cyc_Absyndump_dumpexp_prec(
myprec, _temp568); goto _LL375; _LL425: Cyc_Absyndump_dump_char(( int)'&'); Cyc_Absyndump_dumpexp_prec(
myprec, _temp572); goto _LL375; _LL427: Cyc_Absyndump_dump( _tag_arr("new ",
sizeof( unsigned char), 5u)); Cyc_Absyndump_dumpexp_prec( myprec, _temp574);
goto _LL375; _LL429: Cyc_Absyndump_dump( _tag_arr("sizeof(", sizeof(
unsigned char), 8u)); Cyc_Absyndump_dumptyp( _temp578); Cyc_Absyndump_dump_char((
int)')'); goto _LL375; _LL431: Cyc_Absyndump_dump( _tag_arr("sizeof(", sizeof(
unsigned char), 8u)); Cyc_Absyndump_dumpexp_prec( myprec, _temp580); Cyc_Absyndump_dump_char((
int)')'); goto _LL375; _LL433: Cyc_Absyndump_dump( _tag_arr("offsetof(", sizeof(
unsigned char), 10u)); Cyc_Absyndump_dumptyp( _temp586); Cyc_Absyndump_dump_char((
int)','); Cyc_Absyndump_dump_nospace(* _temp584); Cyc_Absyndump_dump_char(( int)')');
goto _LL375; _LL435: Cyc_Absyndump_dump( _tag_arr("offsetof(", sizeof(
unsigned char), 10u)); Cyc_Absyndump_dumptyp( _temp592); Cyc_Absyndump_dump_char((
int)','); Cyc_Absyndump_dump(( struct _tagged_arr)({ struct Cyc_Std_Int_pa_struct
_temp668; _temp668.tag= Cyc_Std_Int_pa; _temp668.f1=( int) _temp590;{ void*
_temp667[ 1u]={& _temp668}; Cyc_Std_aprintf( _tag_arr("%d", sizeof(
unsigned char), 3u), _tag_arr( _temp667, sizeof( void*), 1u));}})); Cyc_Absyndump_dump_char((
int)')'); goto _LL375; _LL437: Cyc_Absyndump_dump( _tag_arr("__gen(", sizeof(
unsigned char), 7u)); Cyc_Absyndump_dumptvars( _temp596); Cyc_Absyndump_dumptyp(
_temp594); Cyc_Absyndump_dump_char(( int)')'); goto _LL375; _LL439: Cyc_Absyndump_dump_char((
int)'*'); Cyc_Absyndump_dumpexp_prec( myprec, _temp598); goto _LL375; _LL441:
Cyc_Absyndump_dumpexp_prec( myprec, _temp602); Cyc_Absyndump_dump_char(( int)'.');
Cyc_Absyndump_dump_nospace(* _temp600); goto _LL375; _LL443: Cyc_Absyndump_dumpexp_prec(
myprec, _temp606); Cyc_Absyndump_dump_nospace( _tag_arr("->", sizeof(
unsigned char), 3u)); Cyc_Absyndump_dump_nospace(* _temp604); goto _LL375;
_LL445: Cyc_Absyndump_dumpexp_prec( myprec, _temp610); Cyc_Absyndump_dump_char((
int)'['); Cyc_Absyndump_dumpexp( _temp608); Cyc_Absyndump_dump_char(( int)']');
goto _LL375; _LL447: Cyc_Absyndump_dump( _tag_arr("$(", sizeof( unsigned char),
3u)); Cyc_Absyndump_dumpexps_prec( 20, _temp612); Cyc_Absyndump_dump_char(( int)')');
goto _LL375; _LL449: Cyc_Absyndump_dump_char(( int)'('); Cyc_Absyndump_dumptyp((*
_temp616).f3); Cyc_Absyndump_dump_char(( int)')');(( void(*)( void(* f)( struct
_tuple8*), struct Cyc_List_List* l, struct _tagged_arr start, struct _tagged_arr
end, struct _tagged_arr sep)) Cyc_Absyndump_group)( Cyc_Absyndump_dumpde,
_temp614, _tag_arr("{", sizeof( unsigned char), 2u), _tag_arr("}", sizeof(
unsigned char), 2u), _tag_arr(",", sizeof( unsigned char), 2u)); goto _LL375;
_LL451:(( void(*)( void(* f)( struct _tuple8*), struct Cyc_List_List* l, struct
_tagged_arr start, struct _tagged_arr end, struct _tagged_arr sep)) Cyc_Absyndump_group)(
Cyc_Absyndump_dumpde, _temp618, _tag_arr("{", sizeof( unsigned char), 2u),
_tag_arr("}", sizeof( unsigned char), 2u), _tag_arr(",", sizeof( unsigned char),
2u)); goto _LL375; _LL453: Cyc_Absyndump_dump( _tag_arr("new {for", sizeof(
unsigned char), 9u)); Cyc_Absyndump_dump_str((* _temp624->name).f2); Cyc_Absyndump_dump_char((
int)'<'); Cyc_Absyndump_dumpexp( _temp622); Cyc_Absyndump_dump_char(( int)':');
Cyc_Absyndump_dumpexp( _temp620); Cyc_Absyndump_dump_char(( int)'}'); goto
_LL375; _LL455: Cyc_Absyndump_dumpqvar( _temp628);(( void(*)( void(* f)( struct
_tuple8*), struct Cyc_List_List* l, struct _tagged_arr start, struct _tagged_arr
end, struct _tagged_arr sep)) Cyc_Absyndump_group)( Cyc_Absyndump_dumpde,
_temp626, _tag_arr("{", sizeof( unsigned char), 2u), _tag_arr("}", sizeof(
unsigned char), 2u), _tag_arr(",", sizeof( unsigned char), 2u)); goto _LL375;
_LL457:(( void(*)( void(* f)( struct _tuple8*), struct Cyc_List_List* l, struct
_tagged_arr start, struct _tagged_arr end, struct _tagged_arr sep)) Cyc_Absyndump_group)(
Cyc_Absyndump_dumpde, _temp630, _tag_arr("{", sizeof( unsigned char), 2u),
_tag_arr("}", sizeof( unsigned char), 2u), _tag_arr(",", sizeof( unsigned char),
2u)); goto _LL375; _LL459: Cyc_Absyndump_dumpqvar( _temp632->name); if( _temp634
!=  0){(( void(*)( void(* f)( struct Cyc_Absyn_Exp*), struct Cyc_List_List* l,
struct _tagged_arr start, struct _tagged_arr end, struct _tagged_arr sep)) Cyc_Absyndump_group)(
Cyc_Absyndump_dumpexp, _temp634, _tag_arr("(", sizeof( unsigned char), 2u),
_tag_arr(")", sizeof( unsigned char), 2u), _tag_arr(",", sizeof( unsigned char),
2u));} goto _LL375; _LL461: Cyc_Absyndump_dumpqvar( _temp636); goto _LL375;
_LL463: Cyc_Absyndump_dumpqvar( _temp638); goto _LL375; _LL465: if( _temp642 != 
0){ Cyc_Absyndump_dump( _tag_arr("rmalloc(", sizeof( unsigned char), 9u)); Cyc_Absyndump_dumpexp((
struct Cyc_Absyn_Exp*) _check_null( _temp642)); Cyc_Absyndump_dump( _tag_arr(",",
sizeof( unsigned char), 2u));} else{ Cyc_Absyndump_dump( _tag_arr("malloc(",
sizeof( unsigned char), 8u));} Cyc_Absyndump_dump( _tag_arr("sizeof(", sizeof(
unsigned char), 8u)); Cyc_Absyndump_dumptyp( _temp640); Cyc_Absyndump_dump(
_tag_arr("))", sizeof( unsigned char), 3u)); goto _LL375; _LL467:(( void(*)(
void(* f)( struct _tuple8*), struct Cyc_List_List* l, struct _tagged_arr start,
struct _tagged_arr end, struct _tagged_arr sep)) Cyc_Absyndump_group)( Cyc_Absyndump_dumpde,
_temp644, _tag_arr("{", sizeof( unsigned char), 2u), _tag_arr("}", sizeof(
unsigned char), 2u), _tag_arr(",", sizeof( unsigned char), 2u)); goto _LL375;
_LL469: Cyc_Absyndump_dump_nospace( _tag_arr("({", sizeof( unsigned char), 3u));
Cyc_Absyndump_dumpstmt( _temp648); Cyc_Absyndump_dump_nospace( _tag_arr("})",
sizeof( unsigned char), 3u)); goto _LL375; _LL471: Cyc_Absyndump_dump( _tag_arr("codegen(",
sizeof( unsigned char), 9u)); Cyc_Absyndump_dumpdecl(({ struct Cyc_Absyn_Decl*
_temp669=( struct Cyc_Absyn_Decl*) _cycalloc( sizeof( struct Cyc_Absyn_Decl));
_temp669->r=( void*)(( void*)({ struct Cyc_Absyn_Fn_d_struct* _temp670=( struct
Cyc_Absyn_Fn_d_struct*) _cycalloc( sizeof( struct Cyc_Absyn_Fn_d_struct));
_temp670[ 0]=({ struct Cyc_Absyn_Fn_d_struct _temp671; _temp671.tag= Cyc_Absyn_Fn_d;
_temp671.f1= _temp650; _temp671;}); _temp670;})); _temp669->loc= e->loc;
_temp669;})); Cyc_Absyndump_dump( _tag_arr(")", sizeof( unsigned char), 2u));
goto _LL375; _LL473: Cyc_Absyndump_dump( _tag_arr("fill(", sizeof( unsigned char),
6u)); Cyc_Absyndump_dumpexp( _temp652); Cyc_Absyndump_dump( _tag_arr(")",
sizeof( unsigned char), 2u)); goto _LL375; _LL375:;} if( inprec >=  myprec){ Cyc_Absyndump_dump_char((
int)')');}} void Cyc_Absyndump_dumpexp( struct Cyc_Absyn_Exp* e){ Cyc_Absyndump_dumpexp_prec(
0, e);} void Cyc_Absyndump_dumpswitchclauses( struct Cyc_List_List* scs){ for( 0;
scs !=  0; scs= scs->tl){ struct Cyc_Absyn_Switch_clause* _temp672=( struct Cyc_Absyn_Switch_clause*)
scs->hd; if( _temp672->where_clause ==  0?( void*)( _temp672->pattern)->r == (
void*) Cyc_Absyn_Wild_p: 0){ Cyc_Absyndump_dump( _tag_arr("default:", sizeof(
unsigned char), 9u));} else{ Cyc_Absyndump_dump( _tag_arr("case", sizeof(
unsigned char), 5u)); Cyc_Absyndump_dumppat( _temp672->pattern); if( _temp672->where_clause
!=  0){ Cyc_Absyndump_dump( _tag_arr("&&", sizeof( unsigned char), 3u)); Cyc_Absyndump_dumpexp((
struct Cyc_Absyn_Exp*) _check_null( _temp672->where_clause));} Cyc_Absyndump_dump_nospace(
_tag_arr(":", sizeof( unsigned char), 2u));} Cyc_Absyndump_dumpstmt( _temp672->body);}}
void Cyc_Absyndump_dumpstmt( struct Cyc_Absyn_Stmt* s){ void* _temp673=( void*)
s->r; struct Cyc_Absyn_Exp* _temp719; struct Cyc_Absyn_Stmt* _temp721; struct
Cyc_Absyn_Stmt* _temp723; struct Cyc_Absyn_Exp* _temp725; struct Cyc_Absyn_Exp*
_temp727; struct Cyc_Absyn_Stmt* _temp729; struct Cyc_Absyn_Stmt* _temp731;
struct Cyc_Absyn_Exp* _temp733; struct Cyc_Absyn_Stmt* _temp735; struct _tuple2
_temp737; struct Cyc_Absyn_Exp* _temp739; struct _tagged_arr* _temp741; struct
Cyc_Absyn_Stmt* _temp743; struct _tuple2 _temp745; struct Cyc_Absyn_Exp*
_temp747; struct _tuple2 _temp749; struct Cyc_Absyn_Exp* _temp751; struct Cyc_Absyn_Exp*
_temp753; struct Cyc_List_List* _temp755; struct Cyc_Absyn_Exp* _temp757; struct
Cyc_Absyn_Stmt* _temp759; struct Cyc_Absyn_Decl* _temp761; struct Cyc_Absyn_Stmt*
_temp763; struct _tagged_arr* _temp765; struct _tuple2 _temp767; struct Cyc_Absyn_Exp*
_temp769; struct Cyc_Absyn_Stmt* _temp771; struct Cyc_List_List* _temp773;
struct Cyc_Absyn_Exp* _temp775; struct Cyc_List_List* _temp777; struct Cyc_List_List*
_temp779; struct Cyc_List_List* _temp781; struct Cyc_Absyn_Stmt* _temp783;
struct Cyc_Absyn_Stmt* _temp785; struct Cyc_Absyn_Vardecl* _temp787; struct Cyc_Absyn_Tvar*
_temp789; struct Cyc_Absyn_Stmt* _temp791; struct Cyc_Absyn_Stmt* _temp793;
_LL675: if( _temp673 == ( void*) Cyc_Absyn_Skip_s){ goto _LL676;} else{ goto
_LL677;} _LL677: if(( unsigned int) _temp673 >  1u?*(( int*) _temp673) ==  Cyc_Absyn_Exp_s:
0){ _LL720: _temp719=(( struct Cyc_Absyn_Exp_s_struct*) _temp673)->f1; goto
_LL678;} else{ goto _LL679;} _LL679: if(( unsigned int) _temp673 >  1u?*(( int*)
_temp673) ==  Cyc_Absyn_Seq_s: 0){ _LL724: _temp723=(( struct Cyc_Absyn_Seq_s_struct*)
_temp673)->f1; goto _LL722; _LL722: _temp721=(( struct Cyc_Absyn_Seq_s_struct*)
_temp673)->f2; goto _LL680;} else{ goto _LL681;} _LL681: if(( unsigned int)
_temp673 >  1u?*(( int*) _temp673) ==  Cyc_Absyn_Return_s: 0){ _LL726: _temp725=((
struct Cyc_Absyn_Return_s_struct*) _temp673)->f1; if( _temp725 ==  0){ goto
_LL682;} else{ goto _LL683;}} else{ goto _LL683;} _LL683: if(( unsigned int)
_temp673 >  1u?*(( int*) _temp673) ==  Cyc_Absyn_Return_s: 0){ _LL728: _temp727=((
struct Cyc_Absyn_Return_s_struct*) _temp673)->f1; goto _LL684;} else{ goto
_LL685;} _LL685: if(( unsigned int) _temp673 >  1u?*(( int*) _temp673) ==  Cyc_Absyn_IfThenElse_s:
0){ _LL734: _temp733=(( struct Cyc_Absyn_IfThenElse_s_struct*) _temp673)->f1;
goto _LL732; _LL732: _temp731=(( struct Cyc_Absyn_IfThenElse_s_struct*) _temp673)->f2;
goto _LL730; _LL730: _temp729=(( struct Cyc_Absyn_IfThenElse_s_struct*) _temp673)->f3;
goto _LL686;} else{ goto _LL687;} _LL687: if(( unsigned int) _temp673 >  1u?*((
int*) _temp673) ==  Cyc_Absyn_While_s: 0){ _LL738: _temp737=(( struct Cyc_Absyn_While_s_struct*)
_temp673)->f1; _LL740: _temp739= _temp737.f1; goto _LL736; _LL736: _temp735=((
struct Cyc_Absyn_While_s_struct*) _temp673)->f2; goto _LL688;} else{ goto _LL689;}
_LL689: if(( unsigned int) _temp673 >  1u?*(( int*) _temp673) ==  Cyc_Absyn_Break_s:
0){ goto _LL690;} else{ goto _LL691;} _LL691: if(( unsigned int) _temp673 >  1u?*((
int*) _temp673) ==  Cyc_Absyn_Continue_s: 0){ goto _LL692;} else{ goto _LL693;}
_LL693: if(( unsigned int) _temp673 >  1u?*(( int*) _temp673) ==  Cyc_Absyn_Goto_s:
0){ _LL742: _temp741=(( struct Cyc_Absyn_Goto_s_struct*) _temp673)->f1; goto
_LL694;} else{ goto _LL695;} _LL695: if(( unsigned int) _temp673 >  1u?*(( int*)
_temp673) ==  Cyc_Absyn_For_s: 0){ _LL754: _temp753=(( struct Cyc_Absyn_For_s_struct*)
_temp673)->f1; goto _LL750; _LL750: _temp749=(( struct Cyc_Absyn_For_s_struct*)
_temp673)->f2; _LL752: _temp751= _temp749.f1; goto _LL746; _LL746: _temp745=((
struct Cyc_Absyn_For_s_struct*) _temp673)->f3; _LL748: _temp747= _temp745.f1;
goto _LL744; _LL744: _temp743=(( struct Cyc_Absyn_For_s_struct*) _temp673)->f4;
goto _LL696;} else{ goto _LL697;} _LL697: if(( unsigned int) _temp673 >  1u?*((
int*) _temp673) ==  Cyc_Absyn_Switch_s: 0){ _LL758: _temp757=(( struct Cyc_Absyn_Switch_s_struct*)
_temp673)->f1; goto _LL756; _LL756: _temp755=(( struct Cyc_Absyn_Switch_s_struct*)
_temp673)->f2; goto _LL698;} else{ goto _LL699;} _LL699: if(( unsigned int)
_temp673 >  1u?*(( int*) _temp673) ==  Cyc_Absyn_Decl_s: 0){ _LL762: _temp761=((
struct Cyc_Absyn_Decl_s_struct*) _temp673)->f1; goto _LL760; _LL760: _temp759=((
struct Cyc_Absyn_Decl_s_struct*) _temp673)->f2; goto _LL700;} else{ goto _LL701;}
_LL701: if(( unsigned int) _temp673 >  1u?*(( int*) _temp673) ==  Cyc_Absyn_Label_s:
0){ _LL766: _temp765=(( struct Cyc_Absyn_Label_s_struct*) _temp673)->f1; goto
_LL764; _LL764: _temp763=(( struct Cyc_Absyn_Label_s_struct*) _temp673)->f2;
goto _LL702;} else{ goto _LL703;} _LL703: if(( unsigned int) _temp673 >  1u?*((
int*) _temp673) ==  Cyc_Absyn_Do_s: 0){ _LL772: _temp771=(( struct Cyc_Absyn_Do_s_struct*)
_temp673)->f1; goto _LL768; _LL768: _temp767=(( struct Cyc_Absyn_Do_s_struct*)
_temp673)->f2; _LL770: _temp769= _temp767.f1; goto _LL704;} else{ goto _LL705;}
_LL705: if(( unsigned int) _temp673 >  1u?*(( int*) _temp673) ==  Cyc_Absyn_SwitchC_s:
0){ _LL776: _temp775=(( struct Cyc_Absyn_SwitchC_s_struct*) _temp673)->f1; goto
_LL774; _LL774: _temp773=(( struct Cyc_Absyn_SwitchC_s_struct*) _temp673)->f2;
goto _LL706;} else{ goto _LL707;} _LL707: if(( unsigned int) _temp673 >  1u?*((
int*) _temp673) ==  Cyc_Absyn_Fallthru_s: 0){ _LL778: _temp777=(( struct Cyc_Absyn_Fallthru_s_struct*)
_temp673)->f1; if( _temp777 ==  0){ goto _LL708;} else{ goto _LL709;}} else{
goto _LL709;} _LL709: if(( unsigned int) _temp673 >  1u?*(( int*) _temp673) == 
Cyc_Absyn_Fallthru_s: 0){ _LL780: _temp779=(( struct Cyc_Absyn_Fallthru_s_struct*)
_temp673)->f1; goto _LL710;} else{ goto _LL711;} _LL711: if(( unsigned int)
_temp673 >  1u?*(( int*) _temp673) ==  Cyc_Absyn_TryCatch_s: 0){ _LL784:
_temp783=(( struct Cyc_Absyn_TryCatch_s_struct*) _temp673)->f1; goto _LL782;
_LL782: _temp781=(( struct Cyc_Absyn_TryCatch_s_struct*) _temp673)->f2; goto
_LL712;} else{ goto _LL713;} _LL713: if(( unsigned int) _temp673 >  1u?*(( int*)
_temp673) ==  Cyc_Absyn_Region_s: 0){ _LL790: _temp789=(( struct Cyc_Absyn_Region_s_struct*)
_temp673)->f1; goto _LL788; _LL788: _temp787=(( struct Cyc_Absyn_Region_s_struct*)
_temp673)->f2; goto _LL786; _LL786: _temp785=(( struct Cyc_Absyn_Region_s_struct*)
_temp673)->f3; goto _LL714;} else{ goto _LL715;} _LL715: if(( unsigned int)
_temp673 >  1u?*(( int*) _temp673) ==  Cyc_Absyn_Cut_s: 0){ _LL792: _temp791=((
struct Cyc_Absyn_Cut_s_struct*) _temp673)->f1; goto _LL716;} else{ goto _LL717;}
_LL717: if(( unsigned int) _temp673 >  1u?*(( int*) _temp673) ==  Cyc_Absyn_Splice_s:
0){ _LL794: _temp793=(( struct Cyc_Absyn_Splice_s_struct*) _temp673)->f1; goto
_LL718;} else{ goto _LL674;} _LL676: Cyc_Absyndump_dump_semi(); goto _LL674;
_LL678: Cyc_Absyndump_dumpexp( _temp719); Cyc_Absyndump_dump_semi(); goto _LL674;
_LL680: if( Cyc_Absynpp_is_declaration( _temp723)){ Cyc_Absyndump_dump_char((
int)'{'); Cyc_Absyndump_dumpstmt( _temp723); Cyc_Absyndump_dump_char(( int)'}');}
else{ Cyc_Absyndump_dumpstmt( _temp723);} if( Cyc_Absynpp_is_declaration(
_temp721)){ Cyc_Absyndump_dump_char(( int)'{'); Cyc_Absyndump_dumpstmt( _temp721);
Cyc_Absyndump_dump_char(( int)'}');} else{ Cyc_Absyndump_dumpstmt( _temp721);}
goto _LL674; _LL682: Cyc_Absyndump_dump( _tag_arr("return;", sizeof(
unsigned char), 8u)); goto _LL674; _LL684: Cyc_Absyndump_dump( _tag_arr("return",
sizeof( unsigned char), 7u)); Cyc_Absyndump_dumpexp(( struct Cyc_Absyn_Exp*)
_check_null( _temp727)); Cyc_Absyndump_dump_semi(); goto _LL674; _LL686: Cyc_Absyndump_dump(
_tag_arr("if(", sizeof( unsigned char), 4u)); Cyc_Absyndump_dumpexp( _temp733);
Cyc_Absyndump_dump_nospace( _tag_arr("){", sizeof( unsigned char), 3u)); Cyc_Absyndump_dumpstmt(
_temp731); Cyc_Absyndump_dump_char(( int)'}');{ void* _temp795=( void*) _temp729->r;
_LL797: if( _temp795 == ( void*) Cyc_Absyn_Skip_s){ goto _LL798;} else{ goto
_LL799;} _LL799: goto _LL800; _LL798: goto _LL796; _LL800: Cyc_Absyndump_dump(
_tag_arr("else{", sizeof( unsigned char), 6u)); Cyc_Absyndump_dumpstmt( _temp729);
Cyc_Absyndump_dump_char(( int)'}'); goto _LL796; _LL796:;} goto _LL674; _LL688:
Cyc_Absyndump_dump( _tag_arr("while(", sizeof( unsigned char), 7u)); Cyc_Absyndump_dumpexp(
_temp739); Cyc_Absyndump_dump_nospace( _tag_arr(") {", sizeof( unsigned char), 4u));
Cyc_Absyndump_dumpstmt( _temp735); Cyc_Absyndump_dump_char(( int)'}'); goto
_LL674; _LL690: Cyc_Absyndump_dump( _tag_arr("break;", sizeof( unsigned char), 7u));
goto _LL674; _LL692: Cyc_Absyndump_dump( _tag_arr("continue;", sizeof(
unsigned char), 10u)); goto _LL674; _LL694: Cyc_Absyndump_dump( _tag_arr("goto",
sizeof( unsigned char), 5u)); Cyc_Absyndump_dump_str( _temp741); Cyc_Absyndump_dump_semi();
goto _LL674; _LL696: Cyc_Absyndump_dump( _tag_arr("for(", sizeof( unsigned char),
5u)); Cyc_Absyndump_dumpexp( _temp753); Cyc_Absyndump_dump_semi(); Cyc_Absyndump_dumpexp(
_temp751); Cyc_Absyndump_dump_semi(); Cyc_Absyndump_dumpexp( _temp747); Cyc_Absyndump_dump_nospace(
_tag_arr("){", sizeof( unsigned char), 3u)); Cyc_Absyndump_dumpstmt( _temp743);
Cyc_Absyndump_dump_char(( int)'}'); goto _LL674; _LL698: Cyc_Absyndump_dump(
_tag_arr("switch(", sizeof( unsigned char), 8u)); Cyc_Absyndump_dumpexp(
_temp757); Cyc_Absyndump_dump_nospace( _tag_arr("){", sizeof( unsigned char), 3u));
Cyc_Absyndump_dumpswitchclauses( _temp755); Cyc_Absyndump_dump_char(( int)'}');
goto _LL674; _LL700: Cyc_Absyndump_dumpdecl( _temp761); Cyc_Absyndump_dumpstmt(
_temp759); goto _LL674; _LL702: if( Cyc_Absynpp_is_declaration( _temp763)){ Cyc_Absyndump_dump_str(
_temp765); Cyc_Absyndump_dump_nospace( _tag_arr(": {", sizeof( unsigned char), 4u));
Cyc_Absyndump_dumpstmt( _temp763); Cyc_Absyndump_dump_char(( int)'}');} else{
Cyc_Absyndump_dump_str( _temp765); Cyc_Absyndump_dump_char(( int)':'); Cyc_Absyndump_dumpstmt(
_temp763);} goto _LL674; _LL704: Cyc_Absyndump_dump( _tag_arr("do {", sizeof(
unsigned char), 5u)); Cyc_Absyndump_dumpstmt( _temp771); Cyc_Absyndump_dump_nospace(
_tag_arr("} while (", sizeof( unsigned char), 10u)); Cyc_Absyndump_dumpexp(
_temp769); Cyc_Absyndump_dump_nospace( _tag_arr(");", sizeof( unsigned char), 3u));
goto _LL674; _LL706: Cyc_Absyndump_dump( _tag_arr("switch \"C\" (", sizeof(
unsigned char), 13u)); Cyc_Absyndump_dumpexp( _temp775); Cyc_Absyndump_dump_nospace(
_tag_arr("){", sizeof( unsigned char), 3u)); for( 0; _temp773 !=  0; _temp773=
_temp773->tl){ struct Cyc_Absyn_SwitchC_clause _temp803; struct Cyc_Absyn_Stmt*
_temp804; struct Cyc_Absyn_Exp* _temp806; struct Cyc_Absyn_SwitchC_clause*
_temp801=( struct Cyc_Absyn_SwitchC_clause*) _temp773->hd; _temp803=* _temp801;
_LL807: _temp806= _temp803.cnst_exp; goto _LL805; _LL805: _temp804= _temp803.body;
goto _LL802; _LL802: if( _temp806 ==  0){ Cyc_Absyndump_dump( _tag_arr("default: ",
sizeof( unsigned char), 10u));} else{ Cyc_Absyndump_dump( _tag_arr("case ",
sizeof( unsigned char), 6u)); Cyc_Absyndump_dumpexp(( struct Cyc_Absyn_Exp*)
_check_null( _temp806)); Cyc_Absyndump_dump_char(( int)':');} Cyc_Absyndump_dumpstmt(
_temp804);} Cyc_Absyndump_dump_char(( int)'}'); goto _LL674; _LL708: Cyc_Absyndump_dump(
_tag_arr("fallthru;", sizeof( unsigned char), 10u)); goto _LL674; _LL710: Cyc_Absyndump_dump(
_tag_arr("fallthru(", sizeof( unsigned char), 10u)); Cyc_Absyndump_dumpexps_prec(
20, _temp779); Cyc_Absyndump_dump_nospace( _tag_arr(");", sizeof( unsigned char),
3u)); goto _LL674; _LL712: Cyc_Absyndump_dump( _tag_arr("try", sizeof(
unsigned char), 4u)); Cyc_Absyndump_dumpstmt( _temp783); Cyc_Absyndump_dump(
_tag_arr("catch {", sizeof( unsigned char), 8u)); Cyc_Absyndump_dumpswitchclauses(
_temp781); Cyc_Absyndump_dump_char(( int)'}'); goto _LL674; _LL714: Cyc_Absyndump_dump(
_tag_arr("region<", sizeof( unsigned char), 8u)); Cyc_Absyndump_dumptvar(
_temp789); Cyc_Absyndump_dump( _tag_arr("> ", sizeof( unsigned char), 3u)); Cyc_Absyndump_dumpqvar(
_temp787->name); Cyc_Absyndump_dump( _tag_arr("{", sizeof( unsigned char), 2u));
Cyc_Absyndump_dumpstmt( _temp785); Cyc_Absyndump_dump( _tag_arr("}", sizeof(
unsigned char), 2u)); goto _LL674; _LL716: Cyc_Absyndump_dump( _tag_arr("cut",
sizeof( unsigned char), 4u)); Cyc_Absyndump_dumpstmt( _temp791); goto _LL674;
_LL718: Cyc_Absyndump_dump( _tag_arr("splice", sizeof( unsigned char), 7u)); Cyc_Absyndump_dumpstmt(
_temp793); goto _LL674; _LL674:;} struct _tuple9{ struct Cyc_List_List* f1;
struct Cyc_Absyn_Pat* f2; } ; void Cyc_Absyndump_dumpdp( struct _tuple9* dp){
Cyc_Absyndump_egroup( Cyc_Absyndump_dumpdesignator,(* dp).f1, _tag_arr("",
sizeof( unsigned char), 1u), _tag_arr("=", sizeof( unsigned char), 2u), _tag_arr("=",
sizeof( unsigned char), 2u)); Cyc_Absyndump_dumppat((* dp).f2);} void Cyc_Absyndump_dumppat(
struct Cyc_Absyn_Pat* p){ void* _temp808=( void*) p->r; int _temp844; void*
_temp846; int _temp848; void* _temp850; unsigned char _temp852; struct
_tagged_arr _temp854; struct Cyc_Absyn_Vardecl* _temp856; struct Cyc_List_List*
_temp858; struct Cyc_Absyn_Pat* _temp860; struct Cyc_Absyn_Vardecl* _temp862;
struct _tuple0* _temp864; struct Cyc_List_List* _temp866; struct Cyc_List_List*
_temp868; struct _tuple0* _temp870; struct Cyc_List_List* _temp872; struct Cyc_List_List*
_temp874; struct _tuple0* _temp876; struct Cyc_List_List* _temp878; struct Cyc_List_List*
_temp880; struct Cyc_Absyn_Structdecl* _temp882; struct Cyc_List_List* _temp884;
struct Cyc_List_List* _temp886; struct Cyc_Absyn_Tunionfield* _temp888; struct
Cyc_Absyn_Enumfield* _temp890; struct Cyc_Absyn_Enumfield* _temp892; _LL810: if(
_temp808 == ( void*) Cyc_Absyn_Wild_p){ goto _LL811;} else{ goto _LL812;} _LL812:
if( _temp808 == ( void*) Cyc_Absyn_Null_p){ goto _LL813;} else{ goto _LL814;}
_LL814: if(( unsigned int) _temp808 >  2u?*(( int*) _temp808) ==  Cyc_Absyn_Int_p:
0){ _LL847: _temp846=( void*)(( struct Cyc_Absyn_Int_p_struct*) _temp808)->f1;
if( _temp846 == ( void*) Cyc_Absyn_Signed){ goto _LL845;} else{ goto _LL816;}
_LL845: _temp844=(( struct Cyc_Absyn_Int_p_struct*) _temp808)->f2; goto _LL815;}
else{ goto _LL816;} _LL816: if(( unsigned int) _temp808 >  2u?*(( int*) _temp808)
==  Cyc_Absyn_Int_p: 0){ _LL851: _temp850=( void*)(( struct Cyc_Absyn_Int_p_struct*)
_temp808)->f1; if( _temp850 == ( void*) Cyc_Absyn_Unsigned){ goto _LL849;} else{
goto _LL818;} _LL849: _temp848=(( struct Cyc_Absyn_Int_p_struct*) _temp808)->f2;
goto _LL817;} else{ goto _LL818;} _LL818: if(( unsigned int) _temp808 >  2u?*((
int*) _temp808) ==  Cyc_Absyn_Char_p: 0){ _LL853: _temp852=(( struct Cyc_Absyn_Char_p_struct*)
_temp808)->f1; goto _LL819;} else{ goto _LL820;} _LL820: if(( unsigned int)
_temp808 >  2u?*(( int*) _temp808) ==  Cyc_Absyn_Float_p: 0){ _LL855: _temp854=((
struct Cyc_Absyn_Float_p_struct*) _temp808)->f1; goto _LL821;} else{ goto _LL822;}
_LL822: if(( unsigned int) _temp808 >  2u?*(( int*) _temp808) ==  Cyc_Absyn_Var_p:
0){ _LL857: _temp856=(( struct Cyc_Absyn_Var_p_struct*) _temp808)->f1; goto
_LL823;} else{ goto _LL824;} _LL824: if(( unsigned int) _temp808 >  2u?*(( int*)
_temp808) ==  Cyc_Absyn_Tuple_p: 0){ _LL859: _temp858=(( struct Cyc_Absyn_Tuple_p_struct*)
_temp808)->f1; goto _LL825;} else{ goto _LL826;} _LL826: if(( unsigned int)
_temp808 >  2u?*(( int*) _temp808) ==  Cyc_Absyn_Pointer_p: 0){ _LL861: _temp860=((
struct Cyc_Absyn_Pointer_p_struct*) _temp808)->f1; goto _LL827;} else{ goto
_LL828;} _LL828: if(( unsigned int) _temp808 >  2u?*(( int*) _temp808) ==  Cyc_Absyn_Reference_p:
0){ _LL863: _temp862=(( struct Cyc_Absyn_Reference_p_struct*) _temp808)->f1;
goto _LL829;} else{ goto _LL830;} _LL830: if(( unsigned int) _temp808 >  2u?*((
int*) _temp808) ==  Cyc_Absyn_UnknownId_p: 0){ _LL865: _temp864=(( struct Cyc_Absyn_UnknownId_p_struct*)
_temp808)->f1; goto _LL831;} else{ goto _LL832;} _LL832: if(( unsigned int)
_temp808 >  2u?*(( int*) _temp808) ==  Cyc_Absyn_UnknownCall_p: 0){ _LL871:
_temp870=(( struct Cyc_Absyn_UnknownCall_p_struct*) _temp808)->f1; goto _LL869;
_LL869: _temp868=(( struct Cyc_Absyn_UnknownCall_p_struct*) _temp808)->f2; goto
_LL867; _LL867: _temp866=(( struct Cyc_Absyn_UnknownCall_p_struct*) _temp808)->f3;
goto _LL833;} else{ goto _LL834;} _LL834: if(( unsigned int) _temp808 >  2u?*((
int*) _temp808) ==  Cyc_Absyn_UnknownFields_p: 0){ _LL877: _temp876=(( struct
Cyc_Absyn_UnknownFields_p_struct*) _temp808)->f1; goto _LL875; _LL875: _temp874=((
struct Cyc_Absyn_UnknownFields_p_struct*) _temp808)->f2; goto _LL873; _LL873:
_temp872=(( struct Cyc_Absyn_UnknownFields_p_struct*) _temp808)->f3; goto _LL835;}
else{ goto _LL836;} _LL836: if(( unsigned int) _temp808 >  2u?*(( int*) _temp808)
==  Cyc_Absyn_Struct_p: 0){ _LL883: _temp882=(( struct Cyc_Absyn_Struct_p_struct*)
_temp808)->f1; goto _LL881; _LL881: _temp880=(( struct Cyc_Absyn_Struct_p_struct*)
_temp808)->f3; goto _LL879; _LL879: _temp878=(( struct Cyc_Absyn_Struct_p_struct*)
_temp808)->f4; goto _LL837;} else{ goto _LL838;} _LL838: if(( unsigned int)
_temp808 >  2u?*(( int*) _temp808) ==  Cyc_Absyn_Tunion_p: 0){ _LL889: _temp888=((
struct Cyc_Absyn_Tunion_p_struct*) _temp808)->f2; goto _LL887; _LL887: _temp886=((
struct Cyc_Absyn_Tunion_p_struct*) _temp808)->f3; goto _LL885; _LL885: _temp884=((
struct Cyc_Absyn_Tunion_p_struct*) _temp808)->f4; goto _LL839;} else{ goto
_LL840;} _LL840: if(( unsigned int) _temp808 >  2u?*(( int*) _temp808) ==  Cyc_Absyn_Enum_p:
0){ _LL891: _temp890=(( struct Cyc_Absyn_Enum_p_struct*) _temp808)->f2; goto
_LL841;} else{ goto _LL842;} _LL842: if(( unsigned int) _temp808 >  2u?*(( int*)
_temp808) ==  Cyc_Absyn_AnonEnum_p: 0){ _LL893: _temp892=(( struct Cyc_Absyn_AnonEnum_p_struct*)
_temp808)->f2; goto _LL843;} else{ goto _LL809;} _LL811: Cyc_Absyndump_dump_char((
int)'_'); goto _LL809; _LL813: Cyc_Absyndump_dump( _tag_arr("NULL", sizeof(
unsigned char), 5u)); goto _LL809; _LL815: Cyc_Absyndump_dump(( struct
_tagged_arr)({ struct Cyc_Std_Int_pa_struct _temp895; _temp895.tag= Cyc_Std_Int_pa;
_temp895.f1=( int)(( unsigned int) _temp844);{ void* _temp894[ 1u]={& _temp895};
Cyc_Std_aprintf( _tag_arr("%d", sizeof( unsigned char), 3u), _tag_arr( _temp894,
sizeof( void*), 1u));}})); goto _LL809; _LL817: Cyc_Absyndump_dump(( struct
_tagged_arr)({ struct Cyc_Std_Int_pa_struct _temp897; _temp897.tag= Cyc_Std_Int_pa;
_temp897.f1=( unsigned int) _temp848;{ void* _temp896[ 1u]={& _temp897}; Cyc_Std_aprintf(
_tag_arr("%u", sizeof( unsigned char), 3u), _tag_arr( _temp896, sizeof( void*),
1u));}})); goto _LL809; _LL819: Cyc_Absyndump_dump( _tag_arr("'", sizeof(
unsigned char), 2u)); Cyc_Absyndump_dump_nospace( Cyc_Absynpp_char_escape(
_temp852)); Cyc_Absyndump_dump_nospace( _tag_arr("'", sizeof( unsigned char), 2u));
goto _LL809; _LL821: Cyc_Absyndump_dump( _temp854); goto _LL809; _LL823: Cyc_Absyndump_dumpqvar(
_temp856->name); goto _LL809; _LL825:(( void(*)( void(* f)( struct Cyc_Absyn_Pat*),
struct Cyc_List_List* l, struct _tagged_arr start, struct _tagged_arr end,
struct _tagged_arr sep)) Cyc_Absyndump_group)( Cyc_Absyndump_dumppat, _temp858,
_tag_arr("$(", sizeof( unsigned char), 3u), _tag_arr(")", sizeof( unsigned char),
2u), _tag_arr(",", sizeof( unsigned char), 2u)); goto _LL809; _LL827: Cyc_Absyndump_dump(
_tag_arr("&", sizeof( unsigned char), 2u)); Cyc_Absyndump_dumppat( _temp860);
goto _LL809; _LL829: Cyc_Absyndump_dump( _tag_arr("*", sizeof( unsigned char), 2u));
Cyc_Absyndump_dumpqvar( _temp862->name); goto _LL809; _LL831: Cyc_Absyndump_dumpqvar(
_temp864); goto _LL809; _LL833: Cyc_Absyndump_dumpqvar( _temp870); Cyc_Absyndump_dumptvars(
_temp868);(( void(*)( void(* f)( struct Cyc_Absyn_Pat*), struct Cyc_List_List* l,
struct _tagged_arr start, struct _tagged_arr end, struct _tagged_arr sep)) Cyc_Absyndump_group)(
Cyc_Absyndump_dumppat, _temp866, _tag_arr("(", sizeof( unsigned char), 2u),
_tag_arr(")", sizeof( unsigned char), 2u), _tag_arr(",", sizeof( unsigned char),
2u)); goto _LL809; _LL835: Cyc_Absyndump_dumpqvar( _temp876); Cyc_Absyndump_dumptvars(
_temp874);(( void(*)( void(* f)( struct _tuple9*), struct Cyc_List_List* l,
struct _tagged_arr start, struct _tagged_arr end, struct _tagged_arr sep)) Cyc_Absyndump_group)(
Cyc_Absyndump_dumpdp, _temp872, _tag_arr("{", sizeof( unsigned char), 2u),
_tag_arr("}", sizeof( unsigned char), 2u), _tag_arr(",", sizeof( unsigned char),
2u)); goto _LL809; _LL837: if( _temp882->name !=  0){ Cyc_Absyndump_dumpqvar((
struct _tuple0*)(( struct Cyc_Core_Opt*) _check_null( _temp882->name))->v);} Cyc_Absyndump_dumptvars(
_temp880);(( void(*)( void(* f)( struct _tuple9*), struct Cyc_List_List* l,
struct _tagged_arr start, struct _tagged_arr end, struct _tagged_arr sep)) Cyc_Absyndump_group)(
Cyc_Absyndump_dumpdp, _temp878, _tag_arr("{", sizeof( unsigned char), 2u),
_tag_arr("}", sizeof( unsigned char), 2u), _tag_arr(",", sizeof( unsigned char),
2u)); goto _LL809; _LL839: Cyc_Absyndump_dumpqvar( _temp888->name); Cyc_Absyndump_dumptvars(
_temp886); if( _temp884 !=  0){(( void(*)( void(* f)( struct Cyc_Absyn_Pat*),
struct Cyc_List_List* l, struct _tagged_arr start, struct _tagged_arr end,
struct _tagged_arr sep)) Cyc_Absyndump_group)( Cyc_Absyndump_dumppat, _temp884,
_tag_arr("(", sizeof( unsigned char), 2u), _tag_arr(")", sizeof( unsigned char),
2u), _tag_arr(",", sizeof( unsigned char), 2u));} goto _LL809; _LL841: Cyc_Absyndump_dumpqvar(
_temp890->name); goto _LL809; _LL843: Cyc_Absyndump_dumpqvar( _temp892->name);
goto _LL809; _LL809:;} void Cyc_Absyndump_dumptunionfield( struct Cyc_Absyn_Tunionfield*
ef){ Cyc_Absyndump_dumpqvar( ef->name); if( ef->typs !=  0){ Cyc_Absyndump_dumpargs(
ef->typs);}} void Cyc_Absyndump_dumptunionfields( struct Cyc_List_List* fields){((
void(*)( void(* f)( struct Cyc_Absyn_Tunionfield*), struct Cyc_List_List* l,
struct _tagged_arr sep)) Cyc_Absyndump_dump_sep)( Cyc_Absyndump_dumptunionfield,
fields, _tag_arr(",", sizeof( unsigned char), 2u));} void Cyc_Absyndump_dumpenumfield(
struct Cyc_Absyn_Enumfield* ef){ Cyc_Absyndump_dumpqvar( ef->name); if( ef->tag
!=  0){ Cyc_Absyndump_dump( _tag_arr(" = ", sizeof( unsigned char), 4u)); Cyc_Absyndump_dumpexp((
struct Cyc_Absyn_Exp*) _check_null( ef->tag));}} void Cyc_Absyndump_dumpenumfields(
struct Cyc_List_List* fields){(( void(*)( void(* f)( struct Cyc_Absyn_Enumfield*),
struct Cyc_List_List* l, struct _tagged_arr sep)) Cyc_Absyndump_dump_sep)( Cyc_Absyndump_dumpenumfield,
fields, _tag_arr(",", sizeof( unsigned char), 2u));} void Cyc_Absyndump_dumpstructfields(
struct Cyc_List_List* fields){ for( 0; fields !=  0; fields= fields->tl){ struct
Cyc_Absyn_Structfield _temp900; struct Cyc_List_List* _temp901; struct Cyc_Absyn_Exp*
_temp903; void* _temp905; struct Cyc_Absyn_Tqual _temp907; struct _tagged_arr*
_temp909; struct Cyc_Absyn_Structfield* _temp898=( struct Cyc_Absyn_Structfield*)
fields->hd; _temp900=* _temp898; _LL910: _temp909= _temp900.name; goto _LL908;
_LL908: _temp907= _temp900.tq; goto _LL906; _LL906: _temp905=( void*) _temp900.type;
goto _LL904; _LL904: _temp903= _temp900.width; goto _LL902; _LL902: _temp901=
_temp900.attributes; goto _LL899; _LL899:(( void(*)( struct Cyc_Absyn_Tqual,
void*, void(* f)( struct _tagged_arr*), struct _tagged_arr*)) Cyc_Absyndump_dumptqtd)(
_temp907, _temp905, Cyc_Absyndump_dump_str, _temp909); Cyc_Absyndump_dumpatts(
_temp901); if( _temp903 !=  0){ Cyc_Absyndump_dump_char(( int)':'); Cyc_Absyndump_dumpexp((
struct Cyc_Absyn_Exp*) _check_null( _temp903));} Cyc_Absyndump_dump_semi();}}
void Cyc_Absyndump_dumptypedefname( struct Cyc_Absyn_Typedefdecl* td){ Cyc_Absyndump_dumpqvar(
td->name); Cyc_Absyndump_dumptvars( td->tvs);} static void Cyc_Absyndump_dump_atts_qvar(
struct Cyc_Absyn_Fndecl* fd){ Cyc_Absyndump_dumpatts( fd->attributes); Cyc_Absyndump_dumpqvar(
fd->name);} struct _tuple10{ void* f1; struct _tuple0* f2; } ; static void Cyc_Absyndump_dump_callconv_qvar(
struct _tuple10* pr){{ void* _temp911=(* pr).f1; _LL913: if( _temp911 == ( void*)
Cyc_Absyn_Unused_att){ goto _LL914;} else{ goto _LL915;} _LL915: if( _temp911 == (
void*) Cyc_Absyn_Stdcall_att){ goto _LL916;} else{ goto _LL917;} _LL917: if(
_temp911 == ( void*) Cyc_Absyn_Cdecl_att){ goto _LL918;} else{ goto _LL919;}
_LL919: if( _temp911 == ( void*) Cyc_Absyn_Fastcall_att){ goto _LL920;} else{
goto _LL921;} _LL921: goto _LL922; _LL914: goto _LL912; _LL916: Cyc_Absyndump_dump(
_tag_arr("_stdcall", sizeof( unsigned char), 9u)); goto _LL912; _LL918: Cyc_Absyndump_dump(
_tag_arr("_cdecl", sizeof( unsigned char), 7u)); goto _LL912; _LL920: Cyc_Absyndump_dump(
_tag_arr("_fastcall", sizeof( unsigned char), 10u)); goto _LL912; _LL922: goto
_LL912; _LL912:;} Cyc_Absyndump_dumpqvar((* pr).f2);} static void Cyc_Absyndump_dump_callconv_fdqvar(
struct Cyc_Absyn_Fndecl* fd){ Cyc_Absyndump_dump_callconv( fd->attributes); Cyc_Absyndump_dumpqvar(
fd->name);} static void Cyc_Absyndump_dumpids( struct Cyc_List_List* vds){ for(
0; vds !=  0; vds= vds->tl){ Cyc_Absyndump_dumpqvar((( struct Cyc_Absyn_Vardecl*)
vds->hd)->name); if( vds->tl !=  0){ Cyc_Absyndump_dump_char(( int)',');}}} void
Cyc_Absyndump_dumpdecl( struct Cyc_Absyn_Decl* d){ void* _temp923=( void*) d->r;
struct Cyc_Absyn_Fndecl* _temp949; struct Cyc_Absyn_Structdecl* _temp951; struct
Cyc_Absyn_Uniondecl* _temp953; struct Cyc_Absyn_Vardecl* _temp955; struct Cyc_Absyn_Vardecl
_temp957; struct Cyc_List_List* _temp958; struct Cyc_Absyn_Exp* _temp960; void*
_temp962; struct Cyc_Absyn_Tqual _temp964; struct _tuple0* _temp966; void*
_temp968; struct Cyc_Absyn_Tuniondecl* _temp970; struct Cyc_Absyn_Tuniondecl
_temp972; int _temp973; struct Cyc_Core_Opt* _temp975; struct Cyc_List_List*
_temp977; struct _tuple0* _temp979; void* _temp981; struct Cyc_Absyn_Enumdecl*
_temp983; struct Cyc_Absyn_Enumdecl _temp985; struct Cyc_Core_Opt* _temp986;
struct _tuple0* _temp988; void* _temp990; struct Cyc_Absyn_Exp* _temp992; struct
Cyc_Absyn_Pat* _temp994; struct Cyc_List_List* _temp996; struct Cyc_Absyn_Typedefdecl*
_temp998; struct Cyc_List_List* _temp1000; struct _tagged_arr* _temp1002; struct
Cyc_List_List* _temp1004; struct _tuple0* _temp1006; struct Cyc_List_List*
_temp1008; _LL925: if(*(( int*) _temp923) ==  Cyc_Absyn_Fn_d){ _LL950: _temp949=((
struct Cyc_Absyn_Fn_d_struct*) _temp923)->f1; goto _LL926;} else{ goto _LL927;}
_LL927: if(*(( int*) _temp923) ==  Cyc_Absyn_Struct_d){ _LL952: _temp951=((
struct Cyc_Absyn_Struct_d_struct*) _temp923)->f1; goto _LL928;} else{ goto
_LL929;} _LL929: if(*(( int*) _temp923) ==  Cyc_Absyn_Union_d){ _LL954: _temp953=((
struct Cyc_Absyn_Union_d_struct*) _temp923)->f1; goto _LL930;} else{ goto _LL931;}
_LL931: if(*(( int*) _temp923) ==  Cyc_Absyn_Var_d){ _LL956: _temp955=(( struct
Cyc_Absyn_Var_d_struct*) _temp923)->f1; _temp957=* _temp955; _LL969: _temp968=(
void*) _temp957.sc; goto _LL967; _LL967: _temp966= _temp957.name; goto _LL965;
_LL965: _temp964= _temp957.tq; goto _LL963; _LL963: _temp962=( void*) _temp957.type;
goto _LL961; _LL961: _temp960= _temp957.initializer; goto _LL959; _LL959:
_temp958= _temp957.attributes; goto _LL932;} else{ goto _LL933;} _LL933: if(*((
int*) _temp923) ==  Cyc_Absyn_Tunion_d){ _LL971: _temp970=(( struct Cyc_Absyn_Tunion_d_struct*)
_temp923)->f1; _temp972=* _temp970; _LL982: _temp981=( void*) _temp972.sc; goto
_LL980; _LL980: _temp979= _temp972.name; goto _LL978; _LL978: _temp977= _temp972.tvs;
goto _LL976; _LL976: _temp975= _temp972.fields; goto _LL974; _LL974: _temp973=
_temp972.is_xtunion; goto _LL934;} else{ goto _LL935;} _LL935: if(*(( int*)
_temp923) ==  Cyc_Absyn_Enum_d){ _LL984: _temp983=(( struct Cyc_Absyn_Enum_d_struct*)
_temp923)->f1; _temp985=* _temp983; _LL991: _temp990=( void*) _temp985.sc; goto
_LL989; _LL989: _temp988= _temp985.name; goto _LL987; _LL987: _temp986= _temp985.fields;
goto _LL936;} else{ goto _LL937;} _LL937: if(*(( int*) _temp923) ==  Cyc_Absyn_Let_d){
_LL995: _temp994=(( struct Cyc_Absyn_Let_d_struct*) _temp923)->f1; goto _LL993;
_LL993: _temp992=(( struct Cyc_Absyn_Let_d_struct*) _temp923)->f4; goto _LL938;}
else{ goto _LL939;} _LL939: if(*(( int*) _temp923) ==  Cyc_Absyn_Letv_d){ _LL997:
_temp996=(( struct Cyc_Absyn_Letv_d_struct*) _temp923)->f1; goto _LL940;} else{
goto _LL941;} _LL941: if(*(( int*) _temp923) ==  Cyc_Absyn_Typedef_d){ _LL999:
_temp998=(( struct Cyc_Absyn_Typedef_d_struct*) _temp923)->f1; goto _LL942;}
else{ goto _LL943;} _LL943: if(*(( int*) _temp923) ==  Cyc_Absyn_Namespace_d){
_LL1003: _temp1002=(( struct Cyc_Absyn_Namespace_d_struct*) _temp923)->f1; goto
_LL1001; _LL1001: _temp1000=(( struct Cyc_Absyn_Namespace_d_struct*) _temp923)->f2;
goto _LL944;} else{ goto _LL945;} _LL945: if(*(( int*) _temp923) ==  Cyc_Absyn_Using_d){
_LL1007: _temp1006=(( struct Cyc_Absyn_Using_d_struct*) _temp923)->f1; goto
_LL1005; _LL1005: _temp1004=(( struct Cyc_Absyn_Using_d_struct*) _temp923)->f2;
goto _LL946;} else{ goto _LL947;} _LL947: if(*(( int*) _temp923) ==  Cyc_Absyn_ExternC_d){
_LL1009: _temp1008=(( struct Cyc_Absyn_ExternC_d_struct*) _temp923)->f1; goto
_LL948;} else{ goto _LL924;} _LL926: if( Cyc_Absyndump_to_VC){ Cyc_Absyndump_dumpatts(
_temp949->attributes);} if( _temp949->is_inline){ if( Cyc_Absyndump_to_VC){ Cyc_Absyndump_dump(
_tag_arr("__inline", sizeof( unsigned char), 9u));} else{ Cyc_Absyndump_dump(
_tag_arr("inline", sizeof( unsigned char), 7u));}} Cyc_Absyndump_dumpscope((
void*) _temp949->sc);{ void* t=( void*)({ struct Cyc_Absyn_FnType_struct*
_temp1011=( struct Cyc_Absyn_FnType_struct*) _cycalloc( sizeof( struct Cyc_Absyn_FnType_struct));
_temp1011[ 0]=({ struct Cyc_Absyn_FnType_struct _temp1012; _temp1012.tag= Cyc_Absyn_FnType;
_temp1012.f1=({ struct Cyc_Absyn_FnInfo _temp1013; _temp1013.tvars= _temp949->tvs;
_temp1013.effect= _temp949->effect; _temp1013.ret_typ=( void*)(( void*) _temp949->ret_type);
_temp1013.args=(( struct Cyc_List_List*(*)( struct _tuple1*(* f)( struct _tuple3*),
struct Cyc_List_List* x)) Cyc_List_map)( Cyc_Absynpp_arg_mk_opt, _temp949->args);
_temp1013.c_varargs= _temp949->c_varargs; _temp1013.cyc_varargs= _temp949->cyc_varargs;
_temp1013.rgn_po= _temp949->rgn_po; _temp1013.attributes= 0; _temp1013;});
_temp1012;}); _temp1011;});(( void(*)( struct Cyc_Absyn_Tqual, void*, void(* f)(
struct Cyc_Absyn_Fndecl*), struct Cyc_Absyn_Fndecl*)) Cyc_Absyndump_dumptqtd)(({
struct Cyc_Absyn_Tqual _temp1010; _temp1010.q_const= 0; _temp1010.q_volatile= 0;
_temp1010.q_restrict= 0; _temp1010;}), t, Cyc_Absyndump_to_VC? Cyc_Absyndump_dump_callconv_fdqvar:
Cyc_Absyndump_dump_atts_qvar, _temp949); Cyc_Absyndump_dump_char(( int)'{'); Cyc_Absyndump_dumpstmt(
_temp949->body); Cyc_Absyndump_dump_char(( int)'}'); goto _LL924;} _LL928: Cyc_Absyndump_dumpscope((
void*) _temp951->sc); Cyc_Absyndump_dump( _tag_arr("struct", sizeof(
unsigned char), 7u)); if( _temp951->name !=  0){ Cyc_Absyndump_dumpqvar(( struct
_tuple0*)(( struct Cyc_Core_Opt*) _check_null( _temp951->name))->v);} Cyc_Absyndump_dumptvars(
_temp951->tvs); if( _temp951->fields ==  0){ Cyc_Absyndump_dump_semi();} else{
Cyc_Absyndump_dump_char(( int)'{'); Cyc_Absyndump_dumpstructfields(( struct Cyc_List_List*)((
struct Cyc_Core_Opt*) _check_null( _temp951->fields))->v); Cyc_Absyndump_dump(
_tag_arr("}", sizeof( unsigned char), 2u)); Cyc_Absyndump_dumpatts( _temp951->attributes);
Cyc_Absyndump_dump( _tag_arr(";", sizeof( unsigned char), 2u));} goto _LL924;
_LL930: Cyc_Absyndump_dumpscope(( void*) _temp953->sc); Cyc_Absyndump_dump(
_tag_arr("union", sizeof( unsigned char), 6u)); if( _temp953->name !=  0){ Cyc_Absyndump_dumpqvar((
struct _tuple0*)(( struct Cyc_Core_Opt*) _check_null( _temp953->name))->v);} Cyc_Absyndump_dumptvars(
_temp953->tvs); if( _temp953->fields ==  0){ Cyc_Absyndump_dump_semi();} else{
Cyc_Absyndump_dump_char(( int)'{'); Cyc_Absyndump_dumpstructfields(( struct Cyc_List_List*)((
struct Cyc_Core_Opt*) _check_null( _temp953->fields))->v); Cyc_Absyndump_dump(
_tag_arr("}", sizeof( unsigned char), 2u)); Cyc_Absyndump_dumpatts( _temp953->attributes);
Cyc_Absyndump_dump( _tag_arr(";", sizeof( unsigned char), 2u));} goto _LL924;
_LL932: if( Cyc_Absyndump_to_VC){ Cyc_Absyndump_dumpatts( _temp958); Cyc_Absyndump_dumpscope(
_temp968);{ struct Cyc_List_List* _temp1016; void* _temp1018; struct Cyc_Absyn_Tqual
_temp1020; struct _tuple4 _temp1014= Cyc_Absynpp_to_tms( _temp964, _temp962);
_LL1021: _temp1020= _temp1014.f1; goto _LL1019; _LL1019: _temp1018= _temp1014.f2;
goto _LL1017; _LL1017: _temp1016= _temp1014.f3; goto _LL1015; _LL1015: { void*
call_conv=( void*) Cyc_Absyn_Unused_att;{ struct Cyc_List_List* tms2= _temp1016;
for( 0; tms2 !=  0; tms2= tms2->tl){ void* _temp1022=( void*) tms2->hd; struct
Cyc_List_List* _temp1028; _LL1024: if(( unsigned int) _temp1022 >  1u?*(( int*)
_temp1022) ==  Cyc_Absyn_Attributes_mod: 0){ _LL1029: _temp1028=(( struct Cyc_Absyn_Attributes_mod_struct*)
_temp1022)->f2; goto _LL1025;} else{ goto _LL1026;} _LL1026: goto _LL1027;
_LL1025: for( 0; _temp1028 !=  0; _temp1028= _temp1028->tl){ void* _temp1030=(
void*) _temp1028->hd; _LL1032: if( _temp1030 == ( void*) Cyc_Absyn_Stdcall_att){
goto _LL1033;} else{ goto _LL1034;} _LL1034: if( _temp1030 == ( void*) Cyc_Absyn_Cdecl_att){
goto _LL1035;} else{ goto _LL1036;} _LL1036: if( _temp1030 == ( void*) Cyc_Absyn_Fastcall_att){
goto _LL1037;} else{ goto _LL1038;} _LL1038: goto _LL1039; _LL1033: call_conv=(
void*) Cyc_Absyn_Stdcall_att; goto _LL1031; _LL1035: call_conv=( void*) Cyc_Absyn_Cdecl_att;
goto _LL1031; _LL1037: call_conv=( void*) Cyc_Absyn_Fastcall_att; goto _LL1031;
_LL1039: goto _LL1031; _LL1031:;} goto _LL1023; _LL1027: goto _LL1023; _LL1023:;}}
Cyc_Absyndump_dumptq( _temp1020); Cyc_Absyndump_dumpntyp( _temp1018);{ struct
_tuple10 _temp1040=({ struct _tuple10 _temp1041; _temp1041.f1= call_conv;
_temp1041.f2= _temp966; _temp1041;});(( void(*)( struct Cyc_List_List* tms, void(*
f)( struct _tuple10*), struct _tuple10* a)) Cyc_Absyndump_dumptms)( Cyc_List_imp_rev(
_temp1016), Cyc_Absyndump_dump_callconv_qvar,& _temp1040);}}}} else{ Cyc_Absyndump_dumpscope(
_temp968);(( void(*)( struct Cyc_Absyn_Tqual, void*, void(* f)( struct _tuple0*),
struct _tuple0*)) Cyc_Absyndump_dumptqtd)( _temp964, _temp962, Cyc_Absyndump_dumpqvar,
_temp966); Cyc_Absyndump_dumpatts( _temp958);} if( _temp960 !=  0){ Cyc_Absyndump_dump_char((
int)'='); Cyc_Absyndump_dumpexp(( struct Cyc_Absyn_Exp*) _check_null( _temp960));}
Cyc_Absyndump_dump_semi(); goto _LL924; _LL934: Cyc_Absyndump_dumpscope(
_temp981); if( _temp973){ Cyc_Absyndump_dump( _tag_arr("xtunion ", sizeof(
unsigned char), 9u));} else{ Cyc_Absyndump_dump( _tag_arr("tunion ", sizeof(
unsigned char), 8u));} Cyc_Absyndump_dumpqvar( _temp979); Cyc_Absyndump_dumptvars(
_temp977); if( _temp975 ==  0){ Cyc_Absyndump_dump_semi();} else{ Cyc_Absyndump_dump_char((
int)'{'); Cyc_Absyndump_dumptunionfields(( struct Cyc_List_List*) _temp975->v);
Cyc_Absyndump_dump_nospace( _tag_arr("};", sizeof( unsigned char), 3u));} goto
_LL924; _LL936: Cyc_Absyndump_dumpscope( _temp990); Cyc_Absyndump_dump( _tag_arr("enum ",
sizeof( unsigned char), 6u)); Cyc_Absyndump_dumpqvar( _temp988); if( _temp986 == 
0){ Cyc_Absyndump_dump_semi();} else{ Cyc_Absyndump_dump_char(( int)'{'); Cyc_Absyndump_dumpenumfields((
struct Cyc_List_List*) _temp986->v); Cyc_Absyndump_dump_nospace( _tag_arr("};",
sizeof( unsigned char), 3u));} return; _LL938: Cyc_Absyndump_dump( _tag_arr("let",
sizeof( unsigned char), 4u)); Cyc_Absyndump_dumppat( _temp994); Cyc_Absyndump_dump_char((
int)'='); Cyc_Absyndump_dumpexp( _temp992); Cyc_Absyndump_dump_semi(); goto
_LL924; _LL940: Cyc_Absyndump_dump( _tag_arr("let ", sizeof( unsigned char), 5u));
Cyc_Absyndump_dumpids( _temp996); Cyc_Absyndump_dump_semi(); goto _LL924; _LL942:
if( ! Cyc_Absyndump_expand_typedefs){ Cyc_Absyndump_dump( _tag_arr("typedef",
sizeof( unsigned char), 8u));(( void(*)( struct Cyc_Absyn_Tqual, void*, void(* f)(
struct Cyc_Absyn_Typedefdecl*), struct Cyc_Absyn_Typedefdecl*)) Cyc_Absyndump_dumptqtd)(({
struct Cyc_Absyn_Tqual _temp1042; _temp1042.q_const= 0; _temp1042.q_volatile= 0;
_temp1042.q_restrict= 0; _temp1042;}),( void*) _temp998->defn, Cyc_Absyndump_dumptypedefname,
_temp998); Cyc_Absyndump_dump_semi();} goto _LL924; _LL944: Cyc_Absyndump_dump(
_tag_arr("namespace", sizeof( unsigned char), 10u)); Cyc_Absyndump_dump_str(
_temp1002); Cyc_Absyndump_dump_char(( int)'{'); for( 0; _temp1000 !=  0;
_temp1000= _temp1000->tl){ Cyc_Absyndump_dumpdecl(( struct Cyc_Absyn_Decl*)
_temp1000->hd);} Cyc_Absyndump_dump_char(( int)'}'); goto _LL924; _LL946: Cyc_Absyndump_dump(
_tag_arr("using", sizeof( unsigned char), 6u)); Cyc_Absyndump_dumpqvar(
_temp1006); Cyc_Absyndump_dump_char(( int)'{'); for( 0; _temp1004 !=  0;
_temp1004= _temp1004->tl){ Cyc_Absyndump_dumpdecl(( struct Cyc_Absyn_Decl*)
_temp1004->hd);} Cyc_Absyndump_dump_char(( int)'}'); goto _LL924; _LL948: Cyc_Absyndump_dump(
_tag_arr("extern \"C\" {", sizeof( unsigned char), 13u)); for( 0; _temp1008 != 
0; _temp1008= _temp1008->tl){ Cyc_Absyndump_dumpdecl(( struct Cyc_Absyn_Decl*)
_temp1008->hd);} Cyc_Absyndump_dump_char(( int)'}'); goto _LL924; _LL924:;}
static void Cyc_Absyndump_dump_upperbound( struct Cyc_Absyn_Exp* e){
unsigned int i= Cyc_Evexp_eval_const_uint_exp( e); if( i !=  1){ Cyc_Absyndump_dump_char((
int)'{'); Cyc_Absyndump_dumpexp( e); Cyc_Absyndump_dump_char(( int)'}');}} void
Cyc_Absyndump_dumptms( struct Cyc_List_List* tms, void(* f)( void*), void* a){
if( tms ==  0){ f( a); return;}{ void* _temp1043=( void*) tms->hd; struct Cyc_Absyn_Tqual
_temp1061; void* _temp1063; void* _temp1065; struct Cyc_Absyn_Exp* _temp1067;
struct Cyc_Absyn_Tqual _temp1069; void* _temp1071; void* _temp1073; struct Cyc_Absyn_Exp*
_temp1075; struct Cyc_Absyn_Tqual _temp1077; void* _temp1079; void* _temp1081;
struct Cyc_Absyn_Tqual _temp1083; void* _temp1085; struct Cyc_Absyn_Tvar*
_temp1087; void* _temp1089; struct Cyc_Absyn_Exp* _temp1091; struct Cyc_Absyn_Tqual
_temp1093; void* _temp1095; struct Cyc_Absyn_Tvar* _temp1097; void* _temp1099;
struct Cyc_Absyn_Exp* _temp1101; struct Cyc_Absyn_Tqual _temp1103; void*
_temp1105; struct Cyc_Absyn_Tvar* _temp1107; void* _temp1109; _LL1045: if((
unsigned int) _temp1043 >  1u?*(( int*) _temp1043) ==  Cyc_Absyn_Pointer_mod: 0){
_LL1066: _temp1065=( void*)(( struct Cyc_Absyn_Pointer_mod_struct*) _temp1043)->f1;
if(( unsigned int) _temp1065 >  1u?*(( int*) _temp1065) ==  Cyc_Absyn_Nullable_ps:
0){ _LL1068: _temp1067=(( struct Cyc_Absyn_Nullable_ps_struct*) _temp1065)->f1;
goto _LL1064;} else{ goto _LL1047;} _LL1064: _temp1063=( void*)(( struct Cyc_Absyn_Pointer_mod_struct*)
_temp1043)->f2; if( _temp1063 == ( void*) Cyc_Absyn_HeapRgn){ goto _LL1062;}
else{ goto _LL1047;} _LL1062: _temp1061=(( struct Cyc_Absyn_Pointer_mod_struct*)
_temp1043)->f3; goto _LL1046;} else{ goto _LL1047;} _LL1047: if(( unsigned int)
_temp1043 >  1u?*(( int*) _temp1043) ==  Cyc_Absyn_Pointer_mod: 0){ _LL1074:
_temp1073=( void*)(( struct Cyc_Absyn_Pointer_mod_struct*) _temp1043)->f1; if((
unsigned int) _temp1073 >  1u?*(( int*) _temp1073) ==  Cyc_Absyn_NonNullable_ps:
0){ _LL1076: _temp1075=(( struct Cyc_Absyn_NonNullable_ps_struct*) _temp1073)->f1;
goto _LL1072;} else{ goto _LL1049;} _LL1072: _temp1071=( void*)(( struct Cyc_Absyn_Pointer_mod_struct*)
_temp1043)->f2; if( _temp1071 == ( void*) Cyc_Absyn_HeapRgn){ goto _LL1070;}
else{ goto _LL1049;} _LL1070: _temp1069=(( struct Cyc_Absyn_Pointer_mod_struct*)
_temp1043)->f3; goto _LL1048;} else{ goto _LL1049;} _LL1049: if(( unsigned int)
_temp1043 >  1u?*(( int*) _temp1043) ==  Cyc_Absyn_Pointer_mod: 0){ _LL1082:
_temp1081=( void*)(( struct Cyc_Absyn_Pointer_mod_struct*) _temp1043)->f1; if(
_temp1081 == ( void*) Cyc_Absyn_TaggedArray_ps){ goto _LL1080;} else{ goto
_LL1051;} _LL1080: _temp1079=( void*)(( struct Cyc_Absyn_Pointer_mod_struct*)
_temp1043)->f2; if( _temp1079 == ( void*) Cyc_Absyn_HeapRgn){ goto _LL1078;}
else{ goto _LL1051;} _LL1078: _temp1077=(( struct Cyc_Absyn_Pointer_mod_struct*)
_temp1043)->f3; goto _LL1050;} else{ goto _LL1051;} _LL1051: if(( unsigned int)
_temp1043 >  1u?*(( int*) _temp1043) ==  Cyc_Absyn_Pointer_mod: 0){ _LL1090:
_temp1089=( void*)(( struct Cyc_Absyn_Pointer_mod_struct*) _temp1043)->f1; if((
unsigned int) _temp1089 >  1u?*(( int*) _temp1089) ==  Cyc_Absyn_Nullable_ps: 0){
_LL1092: _temp1091=(( struct Cyc_Absyn_Nullable_ps_struct*) _temp1089)->f1; goto
_LL1086;} else{ goto _LL1053;} _LL1086: _temp1085=( void*)(( struct Cyc_Absyn_Pointer_mod_struct*)
_temp1043)->f2; if(( unsigned int) _temp1085 >  4u?*(( int*) _temp1085) ==  Cyc_Absyn_VarType:
0){ _LL1088: _temp1087=(( struct Cyc_Absyn_VarType_struct*) _temp1085)->f1; goto
_LL1084;} else{ goto _LL1053;} _LL1084: _temp1083=(( struct Cyc_Absyn_Pointer_mod_struct*)
_temp1043)->f3; goto _LL1052;} else{ goto _LL1053;} _LL1053: if(( unsigned int)
_temp1043 >  1u?*(( int*) _temp1043) ==  Cyc_Absyn_Pointer_mod: 0){ _LL1100:
_temp1099=( void*)(( struct Cyc_Absyn_Pointer_mod_struct*) _temp1043)->f1; if((
unsigned int) _temp1099 >  1u?*(( int*) _temp1099) ==  Cyc_Absyn_NonNullable_ps:
0){ _LL1102: _temp1101=(( struct Cyc_Absyn_NonNullable_ps_struct*) _temp1099)->f1;
goto _LL1096;} else{ goto _LL1055;} _LL1096: _temp1095=( void*)(( struct Cyc_Absyn_Pointer_mod_struct*)
_temp1043)->f2; if(( unsigned int) _temp1095 >  4u?*(( int*) _temp1095) ==  Cyc_Absyn_VarType:
0){ _LL1098: _temp1097=(( struct Cyc_Absyn_VarType_struct*) _temp1095)->f1; goto
_LL1094;} else{ goto _LL1055;} _LL1094: _temp1093=(( struct Cyc_Absyn_Pointer_mod_struct*)
_temp1043)->f3; goto _LL1054;} else{ goto _LL1055;} _LL1055: if(( unsigned int)
_temp1043 >  1u?*(( int*) _temp1043) ==  Cyc_Absyn_Pointer_mod: 0){ _LL1110:
_temp1109=( void*)(( struct Cyc_Absyn_Pointer_mod_struct*) _temp1043)->f1; if(
_temp1109 == ( void*) Cyc_Absyn_TaggedArray_ps){ goto _LL1106;} else{ goto
_LL1057;} _LL1106: _temp1105=( void*)(( struct Cyc_Absyn_Pointer_mod_struct*)
_temp1043)->f2; if(( unsigned int) _temp1105 >  4u?*(( int*) _temp1105) ==  Cyc_Absyn_VarType:
0){ _LL1108: _temp1107=(( struct Cyc_Absyn_VarType_struct*) _temp1105)->f1; goto
_LL1104;} else{ goto _LL1057;} _LL1104: _temp1103=(( struct Cyc_Absyn_Pointer_mod_struct*)
_temp1043)->f3; goto _LL1056;} else{ goto _LL1057;} _LL1057: if(( unsigned int)
_temp1043 >  1u?*(( int*) _temp1043) ==  Cyc_Absyn_Pointer_mod: 0){ goto _LL1058;}
else{ goto _LL1059;} _LL1059: goto _LL1060; _LL1046: Cyc_Absyndump_dump_char((
int)'*'); Cyc_Absyndump_dump_upperbound( _temp1067); Cyc_Absyndump_dumptms( tms->tl,
f, a); return; _LL1048: Cyc_Absyndump_dump_char(( int)'@'); Cyc_Absyndump_dump_upperbound(
_temp1075); Cyc_Absyndump_dumptms( tms->tl, f, a); return; _LL1050: Cyc_Absyndump_dump_char((
int)'?'); Cyc_Absyndump_dumptms( tms->tl, f, a); return; _LL1052: Cyc_Absyndump_dump_char((
int)'*'); Cyc_Absyndump_dump_upperbound( _temp1091); Cyc_Absyndump_dump_str(
_temp1087->name); Cyc_Absyndump_dumptms( tms->tl, f, a); return; _LL1054: Cyc_Absyndump_dump_char((
int)'@'); Cyc_Absyndump_dump_upperbound( _temp1101); Cyc_Absyndump_dump_str(
_temp1097->name); Cyc_Absyndump_dumptms( tms->tl, f, a); return; _LL1056: Cyc_Absyndump_dump_char((
int)'?'); Cyc_Absyndump_dump_str( _temp1107->name); Cyc_Absyndump_dumptms( tms->tl,
f, a); return; _LL1058:( int) _throw(( void*)({ struct Cyc_Core_Impossible_struct*
_temp1111=( struct Cyc_Core_Impossible_struct*) _cycalloc( sizeof( struct Cyc_Core_Impossible_struct));
_temp1111[ 0]=({ struct Cyc_Core_Impossible_struct _temp1112; _temp1112.tag= Cyc_Core_Impossible;
_temp1112.f1= _tag_arr("dumptms: bad Pointer_mod", sizeof( unsigned char), 25u);
_temp1112;}); _temp1111;})); _LL1060: { int next_is_pointer= 0; if( tms->tl != 
0){ void* _temp1113=( void*)(( struct Cyc_List_List*) _check_null( tms->tl))->hd;
_LL1115: if(( unsigned int) _temp1113 >  1u?*(( int*) _temp1113) ==  Cyc_Absyn_Pointer_mod:
0){ goto _LL1116;} else{ goto _LL1117;} _LL1117: goto _LL1118; _LL1116:
next_is_pointer= 1; goto _LL1114; _LL1118: goto _LL1114; _LL1114:;} if(
next_is_pointer){ Cyc_Absyndump_dump_char(( int)'(');} Cyc_Absyndump_dumptms(
tms->tl, f, a); if( next_is_pointer){ Cyc_Absyndump_dump_char(( int)')');}{ void*
_temp1119=( void*) tms->hd; struct Cyc_Absyn_Exp* _temp1135; void* _temp1137;
struct Cyc_List_List* _temp1139; struct Cyc_Core_Opt* _temp1141; struct Cyc_Absyn_VarargInfo*
_temp1143; int _temp1145; struct Cyc_List_List* _temp1147; void* _temp1149;
struct Cyc_Position_Segment* _temp1151; struct Cyc_List_List* _temp1153; int
_temp1155; struct Cyc_Position_Segment* _temp1157; struct Cyc_List_List*
_temp1159; struct Cyc_List_List* _temp1161; void* _temp1163; void* _temp1165;
_LL1121: if( _temp1119 == ( void*) Cyc_Absyn_Carray_mod){ goto _LL1122;} else{
goto _LL1123;} _LL1123: if(( unsigned int) _temp1119 >  1u?*(( int*) _temp1119)
==  Cyc_Absyn_ConstArray_mod: 0){ _LL1136: _temp1135=(( struct Cyc_Absyn_ConstArray_mod_struct*)
_temp1119)->f1; goto _LL1124;} else{ goto _LL1125;} _LL1125: if(( unsigned int)
_temp1119 >  1u?*(( int*) _temp1119) ==  Cyc_Absyn_Function_mod: 0){ _LL1138:
_temp1137=( void*)(( struct Cyc_Absyn_Function_mod_struct*) _temp1119)->f1; if(*((
int*) _temp1137) ==  Cyc_Absyn_WithTypes){ _LL1148: _temp1147=(( struct Cyc_Absyn_WithTypes_struct*)
_temp1137)->f1; goto _LL1146; _LL1146: _temp1145=(( struct Cyc_Absyn_WithTypes_struct*)
_temp1137)->f2; goto _LL1144; _LL1144: _temp1143=(( struct Cyc_Absyn_WithTypes_struct*)
_temp1137)->f3; goto _LL1142; _LL1142: _temp1141=(( struct Cyc_Absyn_WithTypes_struct*)
_temp1137)->f4; goto _LL1140; _LL1140: _temp1139=(( struct Cyc_Absyn_WithTypes_struct*)
_temp1137)->f5; goto _LL1126;} else{ goto _LL1127;}} else{ goto _LL1127;}
_LL1127: if(( unsigned int) _temp1119 >  1u?*(( int*) _temp1119) ==  Cyc_Absyn_Function_mod:
0){ _LL1150: _temp1149=( void*)(( struct Cyc_Absyn_Function_mod_struct*)
_temp1119)->f1; if(*(( int*) _temp1149) ==  Cyc_Absyn_NoTypes){ _LL1154:
_temp1153=(( struct Cyc_Absyn_NoTypes_struct*) _temp1149)->f1; goto _LL1152;
_LL1152: _temp1151=(( struct Cyc_Absyn_NoTypes_struct*) _temp1149)->f2; goto
_LL1128;} else{ goto _LL1129;}} else{ goto _LL1129;} _LL1129: if(( unsigned int)
_temp1119 >  1u?*(( int*) _temp1119) ==  Cyc_Absyn_TypeParams_mod: 0){ _LL1160:
_temp1159=(( struct Cyc_Absyn_TypeParams_mod_struct*) _temp1119)->f1; goto
_LL1158; _LL1158: _temp1157=(( struct Cyc_Absyn_TypeParams_mod_struct*)
_temp1119)->f2; goto _LL1156; _LL1156: _temp1155=(( struct Cyc_Absyn_TypeParams_mod_struct*)
_temp1119)->f3; goto _LL1130;} else{ goto _LL1131;} _LL1131: if(( unsigned int)
_temp1119 >  1u?*(( int*) _temp1119) ==  Cyc_Absyn_Attributes_mod: 0){ _LL1162:
_temp1161=(( struct Cyc_Absyn_Attributes_mod_struct*) _temp1119)->f2; goto
_LL1132;} else{ goto _LL1133;} _LL1133: if(( unsigned int) _temp1119 >  1u?*((
int*) _temp1119) ==  Cyc_Absyn_Pointer_mod: 0){ _LL1166: _temp1165=( void*)((
struct Cyc_Absyn_Pointer_mod_struct*) _temp1119)->f1; goto _LL1164; _LL1164:
_temp1163=( void*)(( struct Cyc_Absyn_Pointer_mod_struct*) _temp1119)->f2; goto
_LL1134;} else{ goto _LL1120;} _LL1122: Cyc_Absyndump_dump( _tag_arr("[]",
sizeof( unsigned char), 3u)); goto _LL1120; _LL1124: Cyc_Absyndump_dump_char((
int)'['); Cyc_Absyndump_dumpexp( _temp1135); Cyc_Absyndump_dump_char(( int)']');
goto _LL1120; _LL1126: Cyc_Absyndump_dumpfunargs( _temp1147, _temp1145,
_temp1143, _temp1141, _temp1139); goto _LL1120; _LL1128:(( void(*)( void(* f)(
struct _tagged_arr*), struct Cyc_List_List* l, struct _tagged_arr start, struct
_tagged_arr end, struct _tagged_arr sep)) Cyc_Absyndump_group)( Cyc_Absyndump_dump_str,
_temp1153, _tag_arr("(", sizeof( unsigned char), 2u), _tag_arr(")", sizeof(
unsigned char), 2u), _tag_arr(",", sizeof( unsigned char), 2u)); goto _LL1120;
_LL1130: if( _temp1155){ Cyc_Absyndump_dumpkindedtvars( _temp1159);} else{ Cyc_Absyndump_dumptvars(
_temp1159);} goto _LL1120; _LL1132: Cyc_Absyndump_dumpatts( _temp1161); goto
_LL1120; _LL1134:( int) _throw(( void*)({ struct Cyc_Core_Impossible_struct*
_temp1167=( struct Cyc_Core_Impossible_struct*) _cycalloc( sizeof( struct Cyc_Core_Impossible_struct));
_temp1167[ 0]=({ struct Cyc_Core_Impossible_struct _temp1168; _temp1168.tag= Cyc_Core_Impossible;
_temp1168.f1= _tag_arr("dumptms", sizeof( unsigned char), 8u); _temp1168;});
_temp1167;})); _LL1120:;} return;} _LL1044:;}} void Cyc_Absyndump_dumptqtd(
struct Cyc_Absyn_Tqual tq, void* t, void(* f)( void*), void* a){ struct Cyc_List_List*
_temp1171; void* _temp1173; struct Cyc_Absyn_Tqual _temp1175; struct _tuple4
_temp1169= Cyc_Absynpp_to_tms( tq, t); _LL1176: _temp1175= _temp1169.f1; goto
_LL1174; _LL1174: _temp1173= _temp1169.f2; goto _LL1172; _LL1172: _temp1171=
_temp1169.f3; goto _LL1170; _LL1170: Cyc_Absyndump_dumptq( _temp1175); Cyc_Absyndump_dumpntyp(
_temp1173); Cyc_Absyndump_dumptms( Cyc_List_imp_rev( _temp1171), f, a);} void
Cyc_Absyndump_dumpdecllist2file( struct Cyc_List_List* tdl, struct Cyc_Std___sFILE*
f){ Cyc_Absyndump_pos= 0;* Cyc_Absyndump_dump_file= f; for( 0; tdl !=  0; tdl=
tdl->tl){ Cyc_Absyndump_dumpdecl(( struct Cyc_Absyn_Decl*) tdl->hd);}({ void*
_temp1177[ 0u]={}; Cyc_Std_fprintf( f, _tag_arr("\n", sizeof( unsigned char), 2u),
_tag_arr( _temp1177, sizeof( void*), 0u));});}