
/* Parser.
   Copyright (C) 2001 Greg Morrisett, AT&T
   This file is part of the Cyclone compiler.

   The Cyclone compiler is free software; you can redistribute it
   and/or modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; either version 2 of
   the License, or (at your option) any later version.

   The Cyclone compiler is distributed in the hope that it will be
   useful, but WITHOUT ANY WARRANTY; without even the implied warranty
   of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with the Cyclone compiler; see the file COPYING. If not,
   write to the Free Software Foundation, Inc., 59 Temple Place -
   Suite 330, Boston, MA 02111-1307, USA. */

/*
An adaptation of the ISO C grammar for Cyclone.  This grammar
is adapted from the proposed C9X standard, but the productions are
arranged as in Kernighan and Ritchie's "The C Programming
Language (ANSI C)", Second Edition, pages 234-239.

The grammar has 19 shift-reduce conflicts, and 4 reduce-reduce conflicts:

There is 1 shift-reduce conflict due to the "dangling else"
problem, which (the Cyclone port of) Bison properly resolves.

There are 10 shift-reduce conflicts due to ambiguities in attribute parsing.

There is 1 shift-reduce conflict due to the treatment of && in patterns.

There are 6 shift-reduce conflicts and 4 reduce-reduce conflicts
due to specifier-qualifier-lists and the desire to allow
typedef names to be redeclared as identifiers.

There are 2 shift-reduce conflicts having to do with
@requires and @ensures clauses on function prototypes.
*/

%{
#define YYDEBUG 0 // 1 to debug, 0 otherwise
#define YYPRINT yyprint
#define YYERROR_VERBOSE
#define YYMAXDEPTH 30000

#if YYDEBUG==1
extern @tagged union YYSTYPE<`yy::E>;
extern void yyprint(int, union YYSTYPE<`yy>);
#endif

#include <lexing.h>
#include <string.h>
#include <hashtable.h>
#include "warn.h"
#include "flags.h"
#include "kinds.h"
#include "tcutil.h"
#include "attributes.h"
#include "currgn.h"
#include "absynpp.h"
#define IS_PARSE_Y
#define ALL_PARSE_H
#include "parse.h"
#include "position.h"
#include "bansheeif.h"
using Core;
using List;
using Absyn;
using BansheeIf;
using Hashtable;

// Typedef processing must be split between the parser and lexer.
// These functions are called by the parser to communicate typedefs
// to the lexer, so the lexer can distinguish typedefs from identifiers.
namespace Lex {
  void register_typedef(qvar_t);
  void enter_namespace(var_t);
  void leave_namespace();
  void enter_using(qvar_t);
  void leave_using();
  void enter_extern_c();
  void leave_extern_c();
  extern qvar_t token_qvar;
  extern string_t token_string;
}

// State that we thread through the lexer and parser.
#define YYLEX_PARAM_ARG Lexing::Lexbuf<Lexing::Function_lexbuf_state<FILE@>>,union YYSTYPE<`yy> @yylval_ptr, YYLTYPE @yylloc
#define YYLEX_ARG yylex_buf,&yylval,&yylloc
#define YYPARSE_PARAM_ARG region_t<`yy> yyr, Lexing::Lexbuf<Lexing::Function_lexbuf_state<FILE@>> yylex_buf

#define LOC(s,e) (Position::loc_to_seg(s.first_line))
#define SLOC(s) (Position::loc_to_seg(s.first_line))
#define DL 0

namespace Parse {
  // Flag that's turned on when processing tempest code instead of cyclone.
  // We interpreter "*" as a fat pointer there and intend for regions to
  // default to the heap (along with some other trickery).
  bool parsing_tempest = false; // FLAG

  // flattened lists
  struct FlatList<`a::A,`r> { struct FlatList<`a,`r> *`r tl; `a hd; };
  
  // imperatively reverse a flattened list
  flat_list_t<`a,`r> flat_imp_rev(flat_list_t<`a,`r> x) {
    if (x == NULL) return x;
    flat_list_t first  = x;
    flat_list_t second = x->tl;
    x->tl = NULL;
    while (second != NULL) {
      flat_list_t temp = second->tl;
      second->tl = first;
      first = second;
      second = temp;
    }
    return first;
  }

datatype exn {Exit};

////////////////////// Type definitions needed only during parsing ///////////
struct Type_specifier {
  bool Signed_spec      : 1;
  bool Unsigned_spec    : 1;
  bool Short_spec       : 1;
  bool Long_spec        : 1;
  bool Long_Long_spec   : 1;
  bool Complex_spec     : 1;
  bool Valid_type_spec  : 1;
  type_t Type_spec;
  seg_t loc;
};

// enum Storage_class {
//   Typedef_sc, Extern_sc, ExternC_sc, Static_sc, Auto_sc, Register_sc, Abstract_sc,
//   None_sc
// };

struct Declaration_spec {
  storage_class_t   sc;
  tqual_t           tq;
  type_specifier_t  type_specs;
  bool              is_inline;
  attributes_t      attributes;
};

struct Declarator<`yy> {
  qvar_t id;
  seg_t  varloc;
  list_t<type_modifier_t<`yy>,`yy> tms;
};

datatype Pointer_qual {
  Numelts_ptrqual(exp_t);
  Region_ptrqual(type_t);
  Effect_ptrqual(types_t);
  Thin_ptrqual;
  Fat_ptrqual;
  Zeroterm_ptrqual;
  Nozeroterm_ptrqual;
  Autoreleased_ptrqual;
  Notnull_ptrqual;
  Nullable_ptrqual;
  Alias_ptrqual(aqualtype_t);
};

struct Abstractdeclarator<`yy> {
  list_t<type_modifier_t<`yy>,`yy> tms;
};

////////////////////////// forward references //////////////////////
static type_t collapse_type_specifiers(type_specifier_t, seg_t);
static $(tqual_t,type_t,list_t<tvar_t>,list_t<attribute_t>)
  apply_tms(tqual_t,type_t,list_t<attribute_t,`H>,list_t<type_modifier_t>);

////////////////// global state (we're not re-entrant) ////////////////
static list_t<decl_t> parse_result = NULL;
static list_t<$(type_t, list_t<constraint_t,`H>)@`H,`H> constraint_graph = NULL;
static bool inside_function_definition = false;
static int inside_noinference_block = 0;
static `a parse_abort(seg_t loc, string_t<`H> msg) __attribute__((noreturn)) {
  Warn::err2(loc,msg);
  throw new Exit;
}

static type_t type_name_to_type($(var_opt_t,tqual_t,type_t)@ tqt,
                                seg_t loc) {
  let &$(_,tq,t) = tqt;
  if (tq.print_const || tq.q_volatile || tq.q_restrict) {
    if (tq.loc != 0) loc = tq.loc;
    Warn::warn(loc,"qualifier on type is ignored");
  }
  return t;
}
////////////////// Collapsing pointer qualifiers ///////////////////////////
static type_modifier_t<`r>
make_pointer_mod(region_t<`r> r, ptrloc_t loc,
		 booltype_t nullable, ptrbound_t bound, type_t eff,
		 pointer_quals_t pqs, tqual_t tqs) {
  // for now, the last qualifier wins and overrides previous ones
  booltype_t zeroterm = Tcutil::any_bool(NULL);
  booltype_t autoreleased = Tcutil::any_bool(NULL);
  type_opt_t aqual = NULL;//default determined by usage -- non-functions are `q <= RESTRICTED
  for (; pqs != NULL; pqs = pqs->tl)
    switch (pqs->hd) {
    case &Zeroterm_ptrqual:     zeroterm = true_type;            break;
    case &Nozeroterm_ptrqual:   zeroterm = false_type;           break;
    case &Autoreleased_ptrqual: autoreleased = true_type;        break;
    case &Nullable_ptrqual:     nullable = true_type;            break;
    case &Notnull_ptrqual:      nullable = false_type;           break;
    case &Fat_ptrqual:             bound = fat_bound_type;       break;
    case &Thin_ptrqual:            bound = bounds_one();         break;
    case &Numelts_ptrqual(e):      bound = thin_bounds_exp(e);   break;
    case &Alias_ptrqual(aq):       aqual=aq;                     break;
    case &Effect_ptrqual(ts):      eff = join_eff(ts);           break;
    case &Region_ptrqual(t):        Warn::impos("Found region pointer qual"); break;
    }
  return rnew(r) Pointer_mod(PtrAtts(eff,nullable,bound,zeroterm,
				     loc,autoreleased,
				     (aqual!=NULL ? aqual : aqual_var_type(new_evar(&Kinds::aqko, NULL), rtd_qual_type))),
			     tqs);
}

////////////////// Functions for creating abstract syntax //////////////////
  static void check_single_constraint(seg_t loc, string_t id) {
    if(zstrcmp(id, "single"))
      Warn::err(loc, id, " is not a valid effect constraint");
  }
  static type_t effect_from_atomic(types_t effs) {
    if(List::length(effs) == 1)
      return effs->hd;
    else
      return join_eff(effs);
  }

static qvar_t gensym_enum() { // a way to gensym an enum name
  static int enum_counter = 0;
  return new $(Rel_n(NULL),
	       new (string_t)aprintf("__anonymous_enum_%d__", enum_counter++));
}

static aggrfield_t
make_aggr_field(seg_t loc,
		$($(seg_t,qvar_t,tqual_t,type_t,list_t<tvar_t>,
		    list_t<attribute_t,`H>)@,$(exp_opt_t,exp_opt_t)@)@
                field_info) {
  let &$(&$(varloc,qid,tq,t,tvs,atts),&$(widthopt,reqopt)) = field_info;
  if (tvs != NULL)
    Warn::err(loc,"bad type params in struct field");
  if(is_qvar_qualified(qid))
    Warn::err(loc,"struct or union field cannot be qualified with a namespace");
  return new Aggrfield{.name = (*qid)[1], .tq = tq, .type = t,
		       .width = widthopt, .attributes = atts,
                       .requires_clause = reqopt};
}

static type_specifier_t empty_spec(seg_t loc) {
  return Type_specifier{.Signed_spec = false,
                        .Unsigned_spec = false,
                        .Short_spec = false,
                        .Long_spec = false,
                        .Long_Long_spec = false,
                        .Complex_spec = false,
                        .Valid_type_spec = false,
                        .Type_spec = sint_type,
                        .loc = loc};
}

static type_specifier_t type_spec(type_t t,seg_t loc) {
  let s = empty_spec(loc);
  s.Type_spec = t;
  s.Valid_type_spec = true;
  return s;
}
static type_specifier_t signed_spec(seg_t loc) {
  let s = empty_spec(loc);
  s.Signed_spec = true;
  return s;
}
static type_specifier_t unsigned_spec(seg_t loc) {
  let s = empty_spec(loc);
  s.Unsigned_spec = true;
  return s;
}
static type_specifier_t short_spec(seg_t loc) {
  let s = empty_spec(loc);
  s.Short_spec = true;
  return s;
}
static type_specifier_t long_spec(seg_t loc) {
  let s = empty_spec(loc);
  s.Long_spec = true;
  return s;
}
static type_specifier_t complex_spec(seg_t loc) {
  let s = empty_spec(loc);
  s.Complex_spec = true;
  return s;
}

// convert any array types to pointer types
static type_t array2ptr(type_t t, bool argposn) {
    // FIX: don't lose zero-term location
  return Tcutil::is_array_type(t) ?
    Tcutil::promote_array(t, argposn ? new_evar(&Kinds::eko, NULL) : heap_rgn_type, al_qual_type, false) : t;
}

// The next few functions are used when we have a function (or aggregate)
// where the type of one parameter appears to depend on the value of
// another.  Specifically, when we have:
//    void foo(tag_t<`i> x, int arr[x])
// we want to rewrite it to:
//    void foo(tag_t<`i> x, int arr[valueof(`i)])

// given a list of arguments, pull out those that have names and
// are given tag_t's
static list_t<$(var_t,type_t)@> get_arg_tags(list_t<$(var_opt_t,tqual_t,type_t) @>x) {
  let res = NULL;
  for (; x != NULL; x = x->tl)
    switch (x->hd) {
    case &$(v,_,&AppType(&TagCon,&List{i,NULL})) && v != NULL:
      switch (i) {
      case &Evar(_,*z,_,_):
        // using an evar here will mess things up since the evar will be
        // duplicated.  So, we pin the evar down to a type variable instead.
        stringptr_t nm = new ((string_t)aprintf("`%s",*((var_t)v)));
        *z = var_type(new Tvar{nm,-1,new Eq_kb(&Kinds::ik),NULL});
        break;
      default: break;
      }
      res = new List(new $((var_t)v,i),res); break;
      // while we're at it, give any anonymous regions_t's a name that
      // corresponds to the variable.
    case &$(&v,_,&AppType(&RgnHandleCon,&List{&Evar(_,*z,_,_),NULL})):
      stringptr_t nm = new ((string_t)aprintf("`%s",v));
      *z = var_type(new Tvar{nm,-1,new Eq_kb(&Kinds::ek),NULL});
      break;
    default: break;
    }
  return res;
}

// same as above, but for a list of aggregate fields
static list_t<$(var_t,type_t)@> get_aggrfield_tags(list_t<aggrfield_t> x) {
  let res = NULL;
  for (; x != NULL; x = x->tl)
    switch (x->hd->type) {
    case &AppType(&TagCon,&List{i,NULL}):
      res = new List(new $(x->hd->name,i), res); break;
    default: break;
    }
  return res;
}

// given a mapping of variables to tags (x->`i), replace the variable x with `i
static exp_t substitute_tags_exp(list_t<$(var_t,type_t)@> tags, exp_t e) {
  switch (e->r) {
  case &Var_e(&Unresolved_b(&$({.Rel_n = NULL},y))):
    for (let ts = tags; ts != NULL; ts = ts->tl) {
      let &$(x,i) = ts->hd;
      if (strptrcmp(x,y) == 0)
        return new_exp(new Valueof_e(Tcutil::copy_type(i)),e->loc);
    }
    break;
  default: break;
  }
  return e;
}

// we have a variable x in scope with type tag_t<`i> -- look for
// array or pointer bounds involving x and replace with valueof(`i).
static type_t substitute_tags(list_t<$(var_t,type_t)@> tags, type_t t) {
  switch (t) {
  case &ArrayType(ArrayInfo{et,tq,nelts,zt,ztloc}):
    exp_opt_t nelts2 = nelts;
    if (nelts != 0)
      nelts2 = substitute_tags_exp(tags,nelts);
    let et2 = substitute_tags(tags,et);
    if (nelts != nelts2 || et != et2)
      return array_type(et2,tq,nelts2,zt,ztloc);
    break;
  case &PointerType(PtrInfo{et,tq,PtrAtts{r,n,b,zt,pl,rel,aq}}):
    let et2 = substitute_tags(tags,et);
    let b2 = substitute_tags(tags,b);
    if (et2 != et || b2 != b)
      return pointer_type(PtrInfo{et2,tq,PtrAtts{r,n,b2,zt,pl,rel,aq}});
    break;
  case &AppType(&ThinCon, &List{t,NULL}):
    let t2 = substitute_tags(tags,t);
    if (t != t2) return thin_bounds_type(t2);
    break;
  case &ValueofType(e):
    let e2 = substitute_tags_exp(tags,e);
    if (e2 != e) return valueof_type(e2);
    break;
  default: break; // FIX: should go into other types too
  }
  return t;
}

// eliminate dependencies on tag fields by substituting the type-level
// value for the associated field name.
static void substitute_aggrfield_tags(list_t<$(var_t,type_t)@> tags, aggrfield_t x) {
  x->type = substitute_tags(tags,x->type);
}

// given an optional variable, tqual, type, and list of type
// variables, return the tqual and type and check that the type
// variables are NULL -- used when we have a tuple type specification.
static $(tqual_t,type_t)@ get_tqual_typ(seg_t loc, $(`a,tqual_t,type_t) @t) {
  return new $((*t)[1],(*t)[2]);
}

static bool is_typeparam(type_modifier_t tm) {
  switch (tm) {
  case &TypeParams_mod(...): return true;
  default: return false;
  }
}

// convert an identifier to a type -- if it's the special identifier
// `H then return HeapRgn, otherwise, return a type variable.
static type_t id2type(string_t<`H> s, kindbound_t k, type_opt_t aliashint,seg_t loc) {
  if (zstrcmp(s,"`H") == 0)
    return heap_rgn_type;
  //eventually parse these as regular type vars
  if (zstrcmp(s,"`U") == 0)
    return unique_rgn_shorthand_type;
  if (zstrcmp(s,"`RC") == 0)
    return refcnt_rgn_shorthand_type;
  if (zstrcmp(s,CurRgn::curr_rgn_name) == 0)
    return CurRgn::curr_rgn_type();
  aliashint = Kinds::consistent_aliashint(loc, k, aliashint);
  return var_type(new Tvar(new s,-1,k,aliashint));
}

static type_t id2aqual(seg_t loc, string_t<`H> s) {
  if(strlen(s)==2) {
    switch (s[1]) {
    case 'A': return al_qual_type;
    case 'U': return un_qual_type;
    case 'T': return rtd_qual_type;
    default: break;
    }
  }
  else if(strlen(s) == 3) {
    if(s[1] == 'R' && s[2] == 'C')
      return rc_qual_type;
  }
  Warn::err2(loc,"bad aqual bound ",s);
  return al_qual_type;
}

static pointer_quals_t<`yy> insert_aqual(region_t<`yy> yy, pointer_quals_t<`yy> qlist, aqualtype_t aq, seg_t loc) {
  for(_ l = qlist; l != NULL; l=l->tl) {
    switch(l->hd) {
    case &Alias_ptrqual(...):
      Warn::err2(loc, "Multiple alias qualifiers");
      return qlist;
    default:
      break;
    }
  }
  return rnew(yy) List{rnew(yy)Alias_ptrqual(aq), qlist};
}

static void tvar_ok(string_t<`H> s,seg_t loc) {
  if (zstrcmp(s,"`H") == 0)
    Warn::err(loc,"bad occurrence of heap region");
  if (zstrcmp(s,"`U") == 0)
    Warn::err(loc,"bad occurrence of unique region");
  if (zstrcmp(s,"`RC") == 0)
    Warn::err(loc, "bad occurrence of refcounted region");
  if (zstrcmp(s,CurRgn::curr_rgn_name) == 0)
    Warn::err(loc,"bad occurrence of \"current\" region");
}

// convert a list of types to a list of typevars -- the parser can't
// tell lists of types apart from lists of typevars easily so we parse
// them as types and then convert them back to typevars.  See
// productions "struct_or_union_specifier" and "datatype_specifier";
static tvar_t typ2tvar(seg_t loc, type_t t) {
  switch (t) {
  case &VarType(pr): return pr;
  default: parse_abort(loc,"expecting a list of type variables, not types");
  }
}

// if tvar's kind is unconstrained, set it to k
static void set_vartyp_kind(type_t t, kind_t k, bool leq) {
  switch(compress(t)) {
  case &VarType(&Tvar(_,_,*cptr,_)):
    switch(Kinds::compress_kb(*cptr)) {
    case &Unknown_kb(_):
      *cptr = leq ? new Less_kb(NULL,k) : Kinds::kind_to_bound(k); return;
    default: return;
    }
  default: return;
  }
}

// Convert an old-style function into a new-style function
static list_t<type_modifier_t<`yy>,`yy>
  oldstyle2newstyle(region_t<`yy> yy,
                    list_t<type_modifier_t<`yy>,`yy> tms,
                    List_t<decl_t> tds,
                    seg_t loc) {
  // If no function is found, or the function is not innermost, then
  // this is not a function definition; it is an error.  But, we
  // return silently.  The error will be caught by make_function below.
  if (tms==NULL) return NULL;

  switch (tms->hd) {
  case &Function_mod(args):
    // Is this the innermost function??
    if (tms->tl==NULL ||
	(is_typeparam(tms->tl->hd) && tms->tl->tl==NULL)) {
      // Yes
      switch (args) {
      case &WithTypes(...):
	Warn::warn(loc,"function declaration with both new- and old-style "
		   "parameter declarations; ignoring old-style");
	return tms;
      case &NoTypes(ids,_):
	if(length(ids) != length(tds))
	  parse_abort(loc, "wrong number of parameter declarations in old-style "
		"function declaration");
	// replace each parameter with the right typed version
	list_t<$(var_opt_t,tqual_t,type_t)@> rev_new_params = NULL;
	for(; ids != NULL; ids = ids->tl) {
	  list_t tds2 = tds;
	  for(; tds2 != NULL; tds2 = tds2->tl) {
	    let x = tds2->hd;
	    switch(x->r) {
	    case &Var_d(vd):
	      if(zstrptrcmp((*vd->name)[1],ids->hd)!=0)
		continue;
	      if(vd->initializer != NULL)
		parse_abort(x->loc, "initializer found in parameter declaration");
	      if(is_qvar_qualified(vd->name))
		parse_abort(x->loc, "namespaces forbidden in parameter declarations");
	      rev_new_params = new List(new $((*vd->name)[1], vd->tq, vd->type),
					rev_new_params);
	      goto L;
	    default: parse_abort(x->loc, "nonvariable declaration in parameter type");
	    }
	  }
	L: if(tds2 == NULL)
	  parse_abort(loc,strconcat(*ids->hd," is not given a type"));
	}
	return
	  rnew(yy) List(rnew(yy) Function_mod(rnew(yy) WithTypes(imp_rev(rev_new_params),
						  false,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL)),
		   NULL);
      }
    }
    // No, keep looking for the innermost function
    fallthru;
  default: return rnew(yy) List(tms->hd,oldstyle2newstyle(yy,tms->tl,tds,loc));
  }
}
// make a top-level function declaration out of a declaration-specifier
// (return type, etc.), a declarator (the function name and args),
// a declaration list (for old-style function definitions), and a statement.
static fndecl_t make_function(region_t<`yy> yy,
                              decl_spec_t *dso, declarator_t<`yy> d,
                              list_t<decl_t> tds, stmt_t body, seg_t loc) {
  // Handle old-style parameter declarations
  if (tds!=NULL)
    d = Declarator(d.id, d.varloc, oldstyle2newstyle(yy,d.tms,tds,loc));
  scope_t sc = Public;
  type_specifier_t tss = empty_spec(loc);
  tqual_t tq = empty_tqual(0);
  bool is_inline = false;
  list_t<attribute_t> atts = NULL;

  if (dso != NULL) {
    tss = dso->type_specs;
    tq  = dso->tq;
    is_inline = dso->is_inline;
    atts = dso->attributes;
    // Examine storage class; like C, we allow static and extern
    switch (dso->sc) {
    case None_sc: break;
    case Extern_sc: sc = Extern; break;
    case Static_sc: sc = Static; break;
    default: Warn::err(loc,"bad storage class on function"); break;
    }
  }
  let t = collapse_type_specifiers(tss,loc);
  let $(fn_tqual,fn_type,x,out_atts) = apply_tms(tq,t,atts,d.tms);
  // what to do with the left-over attributes out_atts?  I'm just
  // going to append them to the function declaration and let the
  // type-checker deal with it.
  if (x != NULL) // Example:   `a f<`b><`a>(`a x) {...}
    Warn::warn(loc,"bad type params, ignoring");

  switch (fn_type) { // fn_type had better be a FnType
  case &FnType(i):
    for(let args2 = i.args; args2 != NULL; args2 = args2->tl) {
      if((*args2->hd)[0] == NULL) {
	Warn::err(loc,"missing argument variable in function prototype");
	(*args2->hd)[0] = new "?";
      }
    }
    //     if(Flags::interproc) {
    //       Tcutil::introduce_cvars(fn_type);
    //     }
    // We don't fill in the cached type here because we may need
    // to figure out the bound type variables and the effect.
    i.attributes=append(i.attributes,out_atts);
    return new Fndecl {.sc=sc,.is_inline=is_inline,.name=d.id,.body=body, .i=i,
		       .cached_type=NULL,.param_vardecls=NULL,.fn_vardecl=NULL,
		       .orig_scope=sc,.escapes=false};
  default: parse_abort(loc,"declarator is not a function prototype");
  }
}

static string_t msg1 =
  "at most one type may appear within a type specifier \n\t(missing ';' or ','?)";
static string_t msg2 =
  "sign specifier may appear only once within a type specifier \n\t(missing ';' or ','?)";

// Given two partial type-specifiers, combine their information
// to produce a new type specifier.  Warns when you have duplicates
// such as "signed signed" but ignores conflicts such as "signed unsigned".
// These are caught when collapse_type_specifiers is called.
static type_specifier_t combine_specifiers(seg_t loc,
                                           type_specifier_t s1,
                                           type_specifier_t s2) {
  if (s1.Signed_spec && s2.Signed_spec)
    Warn::warn(loc,msg2);
  s1.Signed_spec |= s2.Signed_spec;
  if (s1.Unsigned_spec && s2.Unsigned_spec)
    Warn::warn(loc,msg2);
  s1.Unsigned_spec |= s2.Unsigned_spec;
  if (s1.Short_spec && s2.Short_spec)
    Warn::warn(loc,"too many occurrences of short in specifiers");
  s1.Short_spec |= s2.Short_spec;
  if (s1.Complex_spec && s2.Complex_spec)
    Warn::warn(loc,"too many occurrences of _Complex or __complex__ in specifiers");
  s1.Complex_spec |= s2.Complex_spec;
  if ((s1.Long_Long_spec && s2.Long_Long_spec) ||
      (s1.Long_Long_spec && s2.Long_spec) ||
      (s2.Long_Long_spec && s1.Long_spec))
    Warn::warn(loc,"too many occurrences of long in specifiers");
  s1.Long_Long_spec =
    (s1.Long_Long_spec || s2.Long_Long_spec || (s1.Long_spec && s2.Long_spec));
  s1.Long_spec = !s1.Long_Long_spec && (s1.Long_spec || s2.Long_spec);
  if (s1.Valid_type_spec && s2.Valid_type_spec)
    Warn::err(loc,msg1);
  else if (s2.Valid_type_spec) {
    s1.Type_spec = s2.Type_spec;
    s1.Valid_type_spec = true;
  }
  return s1;
}

// Given a type-specifier, determines the type.  This just really
// needs to look for conflicts (e.g., "signed unsigned") and collapse
// the specifier down to a type.
static type_t collapse_type_specifiers(type_specifier_t ts, seg_t loc) {
  bool      seen_type = ts.Valid_type_spec;
  bool      seen_sign = ts.Signed_spec || ts.Unsigned_spec;
  bool      seen_size = ts.Short_spec || ts.Long_spec || ts.Long_Long_spec;
  type_t    t         = seen_type ? ts.Type_spec : void_type;
  size_of_t sz        = Int_sz;
  sign_t    sgn       = Signed;

  if(seen_size || seen_sign) { // optimization: avoid these for non-ints
    if (ts.Signed_spec && ts.Unsigned_spec)
      Warn::err(loc,msg2);
    if (ts.Unsigned_spec) sgn = Unsigned;
    if ((ts.Short_spec && (ts.Long_spec || ts.Long_Long_spec)) ||
	(ts.Long_spec && ts.Long_Long_spec))
      Warn::err(loc,msg2);
    if (ts.Short_spec) sz = Short_sz;
    if (ts.Long_spec) sz = Long_sz;
    if (ts.Long_Long_spec) sz = LongLong_sz;
  }
  // it's okay to not have an explicit type as long as we have some
  // combination of signed, unsigned, short, long, longlong, or complex
  if (!seen_type) {
    if(!seen_sign && !seen_size) {
      if (ts.Complex_spec)
        return complex_type(double_type);
      Warn::warn(loc,"missing type within specifier");
    }
    if (ts.Complex_spec)
      return complex_type(int_type(sgn,sz));
    else return int_type(sgn,sz);
  }
  switch (t) {
  case &AppType(&IntCon(sgn2,sz2),_):
    if (seen_sign && sgn2 != sgn) {
      sgn2 = sgn;
      t = int_type(sgn,sz2);
    }
    if(seen_size && sz2 != sz)
      t = int_type(sgn2,sz);
    if (ts.Complex_spec)
      t = complex_type(t);
    break;
  case &AppType(&FloatCon(_),_):
    if(seen_size) // hack: if we've seen "long" then sz will be long_double_type
      t = long_double_type;
    if (ts.Complex_spec)
      t = complex_type(t);
    break;
  default:
    if(seen_sign)
      Warn::err(loc,"sign specification on non-integral type");
    if(seen_size)
      Warn::err(loc,"size qualifier on non-integral type");
    break;
  }
  return t;
}

static list_t<$(seg_t, qvar_t,tqual_t,type_t,list_t<tvar_t>,list_t<attribute_t>)@`r,`r>
  apply_tmss(region_t<`r> r, tqual_t tq, type_t t,declarators_t<`r> ds,
             attributes_t shared_atts)
{
  if (ds==NULL) return NULL;
  declarator_t d = ds->hd;
  let q = d.id;
  let varloc = d.varloc;
  let $(tq2,new_typ,tvs,atts) = apply_tms(tq,t,shared_atts,d.tms);
  // NB: we copy the type here to avoid sharing definitions
  // but we avoid the copy when ds->tl is NULL
  let tl = (ds->tl == NULL) ? NULL : apply_tmss(r,tq,Tcutil::copy_type(t),ds->tl,shared_atts);
  return rnew(r) List(rnew(r) $(varloc,q,tq2,new_typ,tvs,atts),tl);
}

static $(tqual_t,type_t,list_t<tvar_t>,list_t<attribute_t>)
  apply_tms(tqual_t tq, type_t t, list_t<attribute_t,`H> atts,
            list_t<type_modifier_t> tms) {
  if (tms==NULL) return $(tq,t,NULL,atts);
  switch (tms->hd) {
  case &Carray_mod(zeroterm,ztloc):
    return apply_tms(empty_tqual(0),
                     array_type(t,tq,NULL,zeroterm,ztloc),atts,tms->tl);
  case &ConstArray_mod(e,zeroterm,ztloc):
    return apply_tms(empty_tqual(0),
                     array_type(t,tq,e,zeroterm,ztloc),atts,tms->tl);
  case &Function_mod(args): {
    switch (args) {
    case &WithTypes(args2,c_vararg,cyc_vararg,eff,effc,qb,chks,req,ens,thrw):
      list_t<tvar_t> typvars = NULL;
      // function type attributes seen thus far get put in the function type
      attributes_t fn_atts = NULL, new_atts = NULL;
      for (_ as = atts; as != NULL; as = as->tl)
	if (Atts::fntype_att(as->hd))
	  fn_atts = new List(as->hd,fn_atts);
	else
	  new_atts = new List(as->hd,new_atts);
      // functions consume type parameters
      if (tms->tl != NULL)
	switch (tms->tl->hd) {
	case &TypeParams_mod(ts,_,_):
	  typvars = ts;
	  tms=tms->tl; // skip TypeParams on call of apply_tms below
	  break;
	default: break;
	}
      // special case where the parameters are void, e.g., int f(void)
      if (!c_vararg && cyc_vararg == NULL // not vararg function
	  && args2 != NULL      // not empty arg list
	  && args2->tl == NULL   // not >1 arg
	  && (*args2->hd)[0] == NULL // not f(void x)
	  && (*args2->hd)[2] == void_type) {
	args2 = NULL;
      }
      // eliminate C99 dependencies e.g., int f(int n, int a[n]) by
      // adding in a requires clause e.g.,
      // int f(int n, int a@) @requires(n <= numelts(a))
      list_t<exp_t> new_requires = NULL;
      for (let a = args2; a != NULL; a = a->tl) {
        let &$(vopt,tq,*t) = a->hd;
        switch (*t) {
        case &ArrayType(ArrayInfo{et,tq,neltsopt,zt,ztloc}):
          if (neltsopt != NULL && vopt != NULL) {
            qvar_t v = new $(Nmspace{.Loc_n = 0},(var_t)vopt);
            exp_t nelts = copy_exp((exp_t)neltsopt);
            exp_t e2 = primop_exp(Numelts,list(var_exp(v,DL)),DL);
            exp_t new_req = lte_exp(nelts, e2, DL);
            new_requires = new List{new_req,new_requires};
          }
          break;
        default: break;
        }
      }
      if (new_requires != NULL) {
        exp_t r;
        if (req != NULL)
          r = req;
        else {
          r = new_requires->hd;
          new_requires = new_requires->tl;
        }
        for (; new_requires != NULL; new_requires = new_requires->tl)
          r = and_exp(r,new_requires->hd,DL);
        req = r;
      }

      // pull out any tag_t variables and their associated tag_t types
      let tags = get_arg_tags(args2);
      // convert result type from array to pointer result
      if (tags != NULL)
        t = substitute_tags(tags,t);
      t = array2ptr(t,false);
      // convert any array arguments to suitable  pointer arguments
      // and substitute away any implicit tag arguments.
      for (let a = args2; a != NULL; a = a->tl) {
        let &$(vopt,tq,*t) = a->hd;
        if (tags != NULL)
          *t = substitute_tags(tags,*t);
        *t = array2ptr(*t,true);
      }
      // Note, we throw away the tqual argument.  An example where
      // this comes up is "const int f(char c)"; it doesn't really
      // make sense to think of the function as returning a const
      // (or volatile, or restrict).  The result will be copied
      // anyway.  TODO: maybe we should issue a warning.  But right
      // now we don't have a loc so the warning will be confusing.
      return apply_tms(empty_tqual(tq.loc),
		       function_type(typvars,eff,tq,t,args2,
                                     c_vararg,cyc_vararg,effc,qb,fn_atts,
                                     chks,req,ens,thrw),
		       new_atts,
		       tms->tl);
    case &NoTypes(_,loc):
      parse_abort(loc,"function declaration without parameter types");
    }
  }
  case &TypeParams_mod(ts,loc,_):
    // If we are the last type modifier, this could be the list of
    // type parameters to a typedef:
    // typedef struct foo<`a,int> foo_t<`a>
    if (tms->tl==NULL)
      return $(tq,t,ts,atts);
    // Otherwise, it is an error in the program if we get here;
    // TypeParams should already have been consumed by an outer
    // Function (see last case).
    parse_abort(loc, "type parameters must appear before function arguments "
                "in declarator");
  case &Pointer_mod(ptratts,tq2):
    return apply_tms(tq2,pointer_type(PtrInfo(t,tq,ptratts)),atts,tms->tl);
  case &Attributes_mod(loc,atts2):
    // FIX: get this in line with GCC
    // attributes get attached to function types -- I doubt that this
    // is GCC's behavior but what else to do?
    return apply_tms(tq,t,List::append(atts,atts2),tms->tl);
  }
}

// given a specifier-qualifier list, warn and ignore about any nested type
// definitions and return the collapsed type.
type_t speclist2typ(type_specifier_t tss, seg_t loc) {
  return collapse_type_specifiers(tss,loc);
}

// convert an (optional) variable, tqual, type, and type
// parameters to a typedef declaration.  As a side effect, register
// the typedef with the lexer.
// TJ: FIX the tqual should make it into the typedef as well,
// e.g., typedef const int CI;
static decl_t v_typ_to_typedef(seg_t loc, $(seg_t,qvar_t,tqual_t,type_t,list_t<tvar_t,`H>,list_t<attribute_t,`H>)@ t) {
  let &$(varloc,x,tq,typ,tvs,atts) = t;
  // tell the lexer that x is a typedef identifier
  Lex::register_typedef(x);
  // if the "type" is an evar, then the typedef is abstract
  opt_t<kind_t> kind;
  type_opt_t type;
  switch (typ) {
  case &Evar(kopt,...):
    type = NULL;
    kind = (kopt==NULL) ? &Kinds::bko : kopt;
    break;
  default: kind = NULL; type = typ; break;
  }
  return new_decl(new Typedef_d(new Typedefdecl{.name=x, .tvs=tvs, .kind=kind,
                                                .defn=type, .atts=atts,
                                                .tq=tq, .extern_c = false}),
		  loc);
}

// given a local declaration and a statement produce a decl statement
static stmt_t flatten_decl(decl_t d,stmt_t s) {
  return new_stmt(new Decl_s(d,s),d->loc);
}
//given a list of local declarations & a statement, produce a big decl statement
static stmt_t flatten_declarations(list_t<decl_t> ds, stmt_t s){
  return List::fold_right(flatten_decl,ds,s);
}

static void decl_split(region_t<`r> r,declarator_list_t<`yy> ds,
		       declarators_t<`r,`yy>@ decls,
		       list_t<exp_opt_t,`r>@ es,
		       list_t<exp_opt_t,`r>@ rs) {
  declarators_t<`r,`yy> declarators = NULL;
  list_t<exp_opt_t,`r>  exprs       = NULL;
  list_t<exp_opt_t,`r>  renames     = NULL;
  for (; ds != NULL; ds = ds->tl) {
    let $(d,e,rename) = ds->hd;
    declarators = rnew(r) FlatList{declarators,d};
    exprs = rnew(r) List(e,exprs);
    renames = rnew(r) List(rename,renames);
  }
  *es    = imp_rev(exprs);
  *rs    = imp_rev(renames);
  *decls = flat_imp_rev(declarators);
}

// Given a declaration specifier list (a combination of storage class
// [typedef, extern, static, etc.] and type specifiers (signed, int,
// `a, const, etc.), and a list of declarators and initializers,
// produce a list of top-level declarations.  By far, this is the most
// involved function and thus I expect a number of subtle errors.
static list_t<decl_t> make_declarations(decl_spec_t ds,
					declarator_list_t ids,
                                        seg_t tqual_loc, seg_t loc) {
 region mkrgn;
 let Declaration_spec(_,tq,tss,_,atts) = ds;
 if (tq.loc == 0) tq.loc = tqual_loc;
 if (ds.is_inline)
   Warn::warn(loc,"inline qualifier on non-function definition");

 scope_t s = Public;
 bool istypedef = false;
 switch (ds.sc) {
 case Typedef_sc:  istypedef = true; break;
 case Extern_sc:   s = Extern;   break;
 case ExternC_sc:  s = ExternC;  break;
 case Static_sc:   s = Static;   break;
 case Auto_sc:     s = Public;   break;
 case Register_sc: s = Flags::no_register ? Public : Register;   break;
 case Abstract_sc: s = Abstract; break;
 default: break;
 }

 // separate the declarators from their initializers
 // FIX: should abstract this out, but the code generator produces
 // bad code due to tuple return type.
 declarators_t<`mkrgn> declarators = NULL;
 list_t<exp_opt_t,`mkrgn> exprs = NULL;
 list_t<exp_opt_t,`mkrgn> renames = NULL;
 decl_split(mkrgn,ids,&declarators,&exprs,&renames);
 // check to see if there are no initializers -- useful later on
 bool exps_empty = true;
 for (list_t<exp_opt_t> es = exprs; es != NULL; es = es->tl)
   if (es->hd != NULL) {
     exps_empty = false;
     break;
   }

 // Collapse the type specifiers to get the base type.
 let base_type = collapse_type_specifiers(tss,loc);
 if (declarators == NULL)
   // here we should have a type declaration -- either a struct, union,
   // enum, or datatype as in: "struct Foo { ... };"
   switch (base_type) {
   case &TypeDeclType(&{.r = &Aggr_td(ad),...},_):
     ad->attributes = List::append(ad->attributes,atts);
     ad->sc = s;
     return new List(new_decl(new Aggr_d(ad),loc),NULL);
   case &TypeDeclType(&{.r = &Enum_td(ed),...},_):
     if (atts != NULL) Warn::err(loc,"attributes on enum not supported");
     ed->sc = s;
     return new List(new_decl(new Enum_d(ed),loc),NULL);
   case &TypeDeclType(&{.r = &Datatype_td(dd),...},_):
     if (atts != NULL) Warn::err(loc,"attributes on datatypes not supported");
     dd->sc = s;
     return new List(new_decl(new Datatype_d(dd),loc),NULL);
   case &AppType(&AggrCon({.UnknownAggr = $(k,n,_)}),ts):
     let ts2 = List::map_c(typ2tvar,loc,ts);
     let ad  = new Aggrdecl(k,s,n,ts2,NULL,NULL,false);
     if (atts != NULL) Warn::err(loc,"bad attributes on type declaration");
     return new List(new_decl(new Aggr_d(ad),loc),NULL);
   case &AppType(&DatatypeCon({.KnownDatatype = tudp}),_):
     if(atts != NULL) Warn::err(loc,"bad attributes on datatype");
     return new List(new_decl(new Datatype_d(*tudp),loc),NULL);
   case &AppType(&DatatypeCon({.UnknownDatatype = UnknownDatatypeInfo(n,isx)}),ts):
     let ts2 = List::map_c(typ2tvar,loc,ts);
     let tud = datatype_decl(s, n, ts2, NULL, isx, loc);
     if (atts != NULL) Warn::err(loc,"bad attributes on datatype");
     return new List(tud,NULL);
   case &AppType(&EnumCon(n,_),_):
     let ed = new Enumdecl{s,n,NULL};
     if (atts != NULL) Warn::err(loc,"bad attributes on enum");
     return new List(new Decl(new Enum_d(ed),loc),NULL);
   case &AppType(&AnonEnumCon(fs),_):
     // someone's written:  enum {A,B,C}; which is a perfectly good
     // way to declare symbolic constants A, B, and C.
     let ed = new Enumdecl{s,gensym_enum(),new Opt(fs)};
     if (atts != NULL) Warn::err(loc,"bad attributes on enum");
     return new List(new Decl(new Enum_d(ed),loc),NULL);
   default: Warn::err(loc,"missing declarator"); return NULL;
   }

 // declarators != NULL
 let fields = apply_tmss(mkrgn,tq,base_type,declarators,atts);
 if (istypedef) {
   // we can have a nested struct, union, or datatype
   // declaration within the typedef as in:
   // typedef struct Foo {...} t;
   if (!exps_empty)
     Warn::err(loc,"initializer in typedef declaration");
   list_t<decl_t> decls = List::map_c(v_typ_to_typedef,loc,fields);
   return decls;
 }
 // here, we have a bunch of variable declarations
 list_t<decl_t> decls = NULL;
 for (let ds = fields;
      ds != NULL;
      ds = ds->tl, exprs = exprs->tl, renames = renames->tl) {
   let &$(varloc,x,tq2,t2,tvs2,atts2) = ds->hd;
   if (tvs2 != NULL)
     Warn::warn(loc,"bad type params, ignoring");
   if (exprs == NULL)
     parse_abort(loc,"unexpected NULL in parse!");
   if (renames == NULL)
     parse_abort(loc,"unexpected NULL in parse!");
   let vd   = new_vardecl(varloc,x, t2, exprs->hd, renames->hd);
   vd->tq = tq2;
   vd->sc = s;
   vd->attributes = atts2;
   decls = new List(new Decl(new Var_d(vd),loc),decls);
 }
 return List::imp_rev(decls);
}

// extract an unsigned integer from a cnst_t
static unsigned cnst2uint(seg_t loc, cnst_t x) {
  switch (x) {
  case {.Int_c  = $(_,i)}: return i;
  case {.Char_c = $(_,c)}: return (unsigned)c;
  case {.LongLong_c = $(_,x)}:
    unsigned long long y = x;
    if (y > 0xffffffff)
      Warn::err(loc,"integer constant too large");
    return (unsigned)x;
  default:
    Warn::err(loc,"expected integer constant but found %s",Absynpp::cnst2string(x));
    return 0;
  }
}

// convert a pattern back into an expression
static exp_t pat2exp(pat_t p) {
  switch (p->r) {
  case &UnknownId_p(x): return unknownid_exp(x,p->loc);
  case &Reference_p(vd,&Pat{.r = &Wild_p,...}):
    return deref_exp(unknownid_exp(vd->name,p->loc),p->loc);
  case &Pointer_p(p2): return address_exp(pat2exp(p2),p->loc);
  case &Null_p: return null_exp(p->loc);
  case &Int_p(s,i): return int_exp(s,i,p->loc);
  case &Char_p(c): return char_exp(c,p->loc);
  case &Float_p(s,i): return float_exp(s,i,p->loc);
  case &UnknownCall_p(x,ps,false):
    exp_t e1 = unknownid_exp(x,p->loc);
    list_t<exp_t> es = List::map(pat2exp,ps);
    return unknowncall_exp(e1,es,p->loc);
  case &Exp_p(e): return e;
  default:
    Warn::err(p->loc,"cannot mix patterns and expressions in case");
    return null_exp(p->loc);
  }
}
static $(exp_t, exp_opt_t) split_seq(exp_t maybe_seq) {
  switch(maybe_seq->r) {
  case &SeqExp_e(e1, e2):
    return $(e1, e2);
  default:
    return $(maybe_seq, NULL);
  }
}
static exp_opt_t join_assn(exp_opt_t e1, exp_opt_t e2) {
  if (e1 != NULL && e2 != NULL) return and_exp((exp_t)e1,(exp_t)e2,0);
  else if (e1 != NULL) return e1;
  else return e2;
}
static $(exp_opt_t,exp_opt_t,exp_opt_t,exp_opt_t) join_assns($(exp_opt_t,exp_opt_t,exp_opt_t,exp_opt_t) a1, $(exp_opt_t,exp_opt_t,exp_opt_t,exp_opt_t) a2) {
  let $(c1,r1,e1,t1) = a1;
  let $(c2,r2,e2,t2) = a2;
  let c = join_assn(c1,c2);
  let r = join_assn(r1,r2);
  let e = join_assn(e1,e2);
  let t = join_assn(t1,t2);
  return $(c,r,e,t);
}

static type_t assign_cvar_pos(string_t<`H> posstr, bool ovfat, type_t cv) {
  switch(cv) {
  case &Cvar(_,_,_,_,_,*pos, *ov):
    *pos = (const char *)posstr;
    *ov = ovfat;
    return cv;
  default:
    return cv;
  }
}

static type_t typevar2cvar(string_t<`H> s) {
  //`C_<kind>_<name>
  //hash consing for Cvars
  static struct Table<stringptr_t, type_t> *cvmap = NULL;
  if(!cvmap)
    cvmap =create(101, strptrcmp, hash_stringptr);
  try { //sharing of Cvars is important
    return Hashtable::lookup(cvmap, new s);
  } catch {
  case &Not_found:
    let kind = strchr(s, '_');
    let name = strchr(kind+1, '_');
    name++;
    if(!strncmp(kind, "_PTRBND", 7)) {
      let t =  cvar_type_name(&Kinds::ptrbko, name);
      Hashtable::insert((_ @)cvmap, new s, t);
      return t;
    }
    else { //unknown kind for now
      parse_abort(0, "Constraint variable unknown kind");
    }
  }
}

  //cvar constants
static type_t str2type(seg_t loc, string_t<`H> s) {
  if(!strcmp(s, "@fat")) {
    return (type_t)Tcutil::ptrbnd_cvar_equivalent(fat_bound_type);
  }
  else if(!strcmp(s, "@thin @numelts{valueof_t(1U)}")){ 
    return (type_t)Tcutil::ptrbnd_cvar_equivalent(bounds_one());
  }
  parse_abort(loc, aprintf("Unknown type constant:: %s", s));
}
  
static constraint_t composite_constraint(enum ConstraintOps op, constraint_t t1, constraint_opt_t t2) {
  switch(op) {
  case C_AND_OP: return and_constraint(t1, (constraint_t)t2);
  case C_OR_OP: return or_constraint(t1, (constraint_t)t2);
  case C_NOT_OP: return not_constraint(t1);
  default: //bad op
    parse_abort(0, "Unexpected operator for composite constraint");
  }
}
  
static constraint_t comparison_constraint(enum ConstraintOps op, type_t t1,  type_t t2) {
  switch(op) {
  case C_EQ_OP: return cmpeq_constraint(t1, t2);
  case C_INCL_OP: return inclusion_constraint(t1, t2);
  default: //bad op
    parse_abort(0, "Unexpected operator for composite constraint");
  }
}
  
} // end namespace Parse
using Parse;
%}
%pure_parser

// ANSI C (or GCC) keywords
%token AUTO REGISTER STATIC EXTERN TYPEDEF VOID CHAR SHORT INT LONG FLOAT FLOAT128
%token DOUBLE SIGNED UNSIGNED CONST VOLATILE RESTRICT
%token STRUCT UNION CASE DEFAULT INLINE SIZEOF OFFSETOF
%token IF ELSE SWITCH WHILE DO FOR GOTO CONTINUE BREAK RETURN ENUM TYPEOF
%token BUILTIN_VA_LIST EXTENSION COMPLEX
// Cyc:  CYCLONE additional keywords
%token NULL_kw LET THROW TRY CATCH EXPORT OVERRIDE HIDE
%token NEW QNEW ABSTRACT FALLTHRU USING NAMESPACE NOINFERENCE DATATYPE
%token MALLOC RMALLOC RVMALLOC RMALLOC_INLINE QMALLOC CALLOC QCALLOC RCALLOC SWAP
%token ASSERT
%token REGION_T TAG_T REGION RNEW REGIONS
%token PORTON PORTOFF PRAGMA TEMPESTON TEMPESTOFF
%token AQ_ALIASABLE  AQ_REFCNT AQ_RESTRICTED AQ_UNIQUE AQUAL_T
%token NUMELTS TAGOF VALUEOF VALUEOF_T TAGCHECK NUMELTS_QUAL THIN_QUAL
%token FAT_QUAL NOTNULL_QUAL NULLABLE_QUAL REQUIRES_QUAL ENSURES_QUAL EFFECT_QUAL
%token THROWS_QUAL SUBSET_QUAL
// Cyc:  CYCLONE qualifiers (e.g., @zeroterm, @tagged, @aqual, aquals)
%token REGION_QUAL NOZEROTERM_QUAL ZEROTERM_QUAL TAGGED_QUAL ASSERT_QUAL ASSERT_FALSE_QUAL ALIAS_QUAL AQUALS CHECKS_QUAL
%token EXTENSIBLE_QUAL AUTORELEASED_QUAL
// double and triple-character tokens
%token PTR_OP INC_OP DEC_OP LEFT_OP RIGHT_OP LE_OP GE_OP EQ_OP NE_OP
%token AND_OP OR_OP MUL_ASSIGN DIV_ASSIGN MOD_ASSIGN ADD_ASSIGN
%token SUB_ASSIGN LEFT_ASSIGN RIGHT_ASSIGN AND_ASSIGN
%token XOR_ASSIGN OR_ASSIGN ELLIPSIS LEFT_RIGHT
// Cyc:  added COLON_COLON double token
%token COLON_COLON
// identifiers and constants
%token IDENTIFIER INTEGER_CONSTANT STRING WSTRING
%token CHARACTER_CONSTANT WCHARACTER_CONSTANT FLOATING_CONSTANT
// Cyc: type variables, qualified identifers and typedef names
%token TYPE_VAR TYPEDEF_NAME QUAL_IDENTIFIER QUAL_TYPEDEF_NAME AQUAL_SHORT_CONST
// Cyc: added __attribute__ keyword
%token ATTRIBUTE
// Cyc: removed ASM "token" -- completely uninterpreted text.  See lex.cyl
%token ASM_TOK
// specify tagged union constructors for types of semantic values that
// the lexer must produce.  The other constructors are generated implicitly.
%union <`yy::E> {
  Int_tok(cnst_t);
  Char_tok(char);
  String_tok(string_t<`H>);
  QualId_tok(qvar_t);
  Exp_tok(exp_t);   // just for yyprint
  Stmt_tok(stmt_t); // just for yyprint
}
/* types for productions */
%type <cnst_t> INTEGER_CONSTANT
%type <char> CHARACTER_CONSTANT
%type <string_t<`H>> FLOATING_CONSTANT namespace_action
%type <string_t<`H>> IDENTIFIER TYPEDEF_NAME TYPE_VAR AQUAL_SHORT_CONST
%type <string_t<`H>> STRING WSTRING WCHARACTER_CONSTANT field_name
%type <$(seg_t,booltype_t, ptrbound_t)@`H> pointer_null_and_bound
%type <ptrbound_t> pointer_bound
%type <list_t<offsetof_field_t,`H>> field_expression
%type <exp_t> primary_expression postfix_expression unary_expression
%type <exp_t> cast_expression constant multiplicative_expression
%type <exp_t> additive_expression shift_expression relational_expression
%type <exp_t> equality_expression and_expression exclusive_or_expression
%type <exp_t> inclusive_or_expression logical_and_expression
%type <exp_t> logical_or_expression conditional_expression
%type <exp_t> assignment_expression expression constant_expression
%type <exp_t> initializer array_initializer for_exp_opt
%type <list_t<exp_t,`H>> argument_expression_list argument_expression_list0
%type <list_t<$(list_t<designator_t,`H>,exp_t)@`H,`H>> initializer_list
%type <primop_t> unary_operator
%type <opt_t<primop_t,`H>> assignment_operator
%type <qvar_t> QUAL_IDENTIFIER QUAL_TYPEDEF_NAME
%type <qvar_t> qual_opt_identifier qual_opt_typedef
%type <qvar_t> using_action struct_union_name
%type <stmt_t> statement labeled_statement compound_statement fndef_compound_statement block_item_list
%type <stmt_t> expression_statement selection_statement iteration_statement
%type <stmt_t> jump_statement
%type <list_t<switch_clause_t,`H>> switch_clauses
%type <pat_t> pattern exp_pattern conditional_pattern logical_or_pattern
%type <pat_t> logical_and_pattern inclusive_or_pattern exclusive_or_pattern
%type <pat_t> and_pattern equality_pattern relational_pattern shift_pattern
%type <pat_t> additive_pattern multiplicative_pattern cast_pattern
%type <pat_t> unary_pattern postfix_pattern primary_pattern
%type <$(list_t<pat_t,`H>,bool)@`H> tuple_pattern_list
%type <list_t<pat_t,`H>> tuple_pattern_list0
%type <$(list_t<designator_t,`H>,pat_t)@`H> field_pattern
%type <list_t<$(list_t<designator_t,`H>,pat_t)@`H,`H>> field_pattern_list0
%type <$(list_t<$(list_t<designator_t,`H>,pat_t)@`H,`H>,bool)@`H> field_pattern_list
%type <fndecl_t> function_definition function_definition2
%type <list_t<decl_t,`H>> declaration declaration_list
%type <list_t<decl_t,`H>> prog translation_unit external_declaration
%type <list_t<decl_t,`H>> override_opt
%type <decl_spec_t> declaration_specifiers
%type <$(declarator_t<`yy>,exp_opt_t,exp_opt_t)> init_declarator
%type <declarator_list_t<`yy>> init_declarator_list_rev
%type <storage_class_t> storage_class_specifier
%type <type_specifier_t> type_specifier type_specifier_notypedef enum_specifier
%type <type_specifier_t> struct_or_union_specifier datatype_specifier
%type <aggr_kind_t> struct_or_union
%type <$(bool,aggr_kind_t)> maybe_tagged_struct_union
%type <tqual_t> type_qualifier tqual_list
%type <list_t<aggrfield_t,`H>> struct_declaration_list struct_declaration
%type <list_t<list_t<aggrfield_t,`H>,`H>> struct_declaration_list0
%type <list_t<type_modifier_t<`yy>,`yy>> pointer one_pointer
%type <declarator_t<`yy>> declarator direct_declarator declarator_withtypedef direct_declarator_withtypedef
%type <$(declarator_t<`yy>,exp_opt_t,exp_opt_t)@`yy> struct_declarator
%type <list_t<$(declarator_t<`yy>,exp_opt_t,exp_opt_t)@`yy,`yy>> struct_declarator_list_rev
%type <abstractdeclarator_t<`yy>> abstract_declarator direct_abstract_declarator
%type <bool> optional_inject qual_datatype extern_c_action volatile_opt
%type <scope_t> datatypefield_scope
%type <datatypefield_t> datatypefield
%type <list_t<datatypefield_t,`H>> datatypefield_list
%type <$(tqual_t,type_specifier_t,attributes_t)> specifier_qualifier_list notypedef_specifier_qualifier_list
%type <list_t<var_t,`H>> identifier_list identifier_list0
%type <$(var_opt_t,tqual_t,type_t)@`H> parameter_declaration type_name
%type <list_t<$(var_opt_t,tqual_t,type_t)@`H,`H>> parameter_list
%type <$(list_t<$(var_opt_t,tqual_t,type_t)@`H,`H>, bool,vararg_info_t *`H,type_opt_t, list_t<effconstr_t,`H>, list_t<$(type_t,type_t)@`H,`H>)@`H> parameter_type_list
%type <types_t> type_name_list type_params_opt effect_set region_set
%type <types_t> atomic_effect eff_set
%type <list_t<designator_t,`H>> designation designator_list
%type <designator_t> designator
%type <kind_t> kind
%type <type_t> any_type_name type_var  qual_bnd_term qual_bnd_const eff_opt //rgn_opt
%type <list_t<attribute_t,`H>> attributes_opt attributes attribute_list
%type <attribute_t> attribute
%type <enumfield_t> enum_field
%type <list_t<enumfield_t,`H>> enum_declaration_list
%type <type_opt_t> optional_effect opt_aqual_bnd
//%type <list_t<$(type_t,type_t)@`H,`H>> optional_rgn_order rgn_order
%type <$(list_t<effconstr_t,`H>, list_t<$(type_t,type_t)@`H,`H>)*`H> optional_effconstr_qualbnd effconstr_qualbnd
%type <effconstr_t> effconstr_elt
%type <$(type_t, type_t)@`H> qual_bnd_elt
%type <booltype_t> zeroterm_qual_opt
%type <list_t<$(seg_t,qvar_t,bool)@`H,`H>> export_list_values
%type <$(list_t<$(seg_t,qvar_t,bool)@`H,`H>, seg_t)@`H> export_list export_list_opt
%type <list_t<qvar_t,`H>> hide_list_opt hide_list_values
%type <aqualtype_t> aqual_specifier aqual_const aqual_opt
%type <pointer_qual_t<`yy>> pointer_qual
%type <pointer_quals_t<`yy>> pointer_quals
%type <exp_opt_t> requires_clause_opt open_opt
%type <$(exp_opt_t,exp_opt_t,exp_opt_t,exp_opt_t)> chk_req_ens_thr_opt
%type <$(exp_opt_t,exp_opt_t,exp_opt_t,exp_opt_t)> chk_req_ens_thr
%type <raw_exp_t> asm_expr
%type <$(list_t<$(string_t<`H>, exp_t)@`H, `H>, list_t<$(string_t<`H>, exp_t)@`H, `H>, list_t<string_t<`H>@`H, `H>)@`H> asm_out_opt
%type <$(list_t<$(string_t<`H>, exp_t)@`H, `H>, list_t<string_t<`H>@`H, `H>)@`H> asm_in_opt
%type <list_t<string_t<`H>@`H, `H>> asm_clobber_opt asm_clobber_list
%type <list_t<$(string_t<`H>, exp_t)@`H, `H>> asm_outlist asm_inlist
%type <$(string_t<`H>, exp_t)@`H> asm_io_elt
%type <exp_maker_fun_t> relational_op equality_op
%type <primop_t> additive_op multiplicative_op
%type <enum ConstraintOps> c_op
%type <list_t<$(type_t, list_t<BansheeIf::constraint_t,`H>)@`H,`H>> all_constraints
%type <list_t<BansheeIf::constraint_t,`H>> constraint_list constraint_list_opt
%type <BansheeIf::constraint_t> constraint
%type <type_t> tvar_or_string
%type <int> prog_or_constraints
%start prog_or_constraints
%%

prog_or_constraints:
  prog {$$=^$(0);}
| all_constraints 
{
  $$ = ^$(1);
  constraint_graph = $1;
}
;

prog:
  translation_unit
    { $$ = $!1;
      parse_result = $1;
    }
;

translation_unit:
  external_declaration translation_unit
    { $$=^$(List::imp_append($1,$2)); }
/* Cyc: added using and namespace */
/* NB: using_action calls Lex::enter_using */
| using_action ';' translation_unit
    { $$=^$(new List(new Decl(new Using_d($1,$3),LOC(@1,@3)),NULL));
      Lex::leave_using();
    }
| using_action '{' translation_unit unusing_action translation_unit
    { $$=^$(new List(new Decl(new Using_d($1,$3),LOC(@1,@4)),$5)); }
/* NB: namespace_action calls Lex::enter_namespace */
| namespace_action ';' translation_unit
    { $$=^$(new List(new Decl(new Namespace_d(new $1,$3),LOC(@1,@3)),NULL));
      Lex::leave_namespace();
    }
| namespace_action '{' translation_unit unnamespace_action translation_unit
    { $$=^$(new List(new Decl(new Namespace_d(new $1,$3),LOC(@1,@4)),$5)); }
| noinference_action '{' translation_unit unnoinference_action translation_unit
    { $$=^$(List::append($3, $5)); }
| extern_c_action '{' translation_unit end_extern_c override_opt export_list_opt hide_list_opt translation_unit
    { let is_c_include = $1;
      list_t<decl_t> cycdecls = $5;
      let &$(exs, wc) = $6;
      let hides = $7;
      if(exs != NULL && hides != NULL)
	Warn::err(LOC(@1,@2), "hide list can only be used with export { * }, or no export block");
      if(hides && !wc)
	wc = SLOC(@7);
      if (!is_c_include) {
	if (exs != NULL || cycdecls != NULL) {
	  Warn::err(LOC(@1,@2),"expecting \"C include\"");
	  $$=^$(new List(new Decl(new ExternCinclude_d($3,cycdecls,exs,new $(wc, hides)),LOC(@1,@6)),$8));
	}
	else
	  $$=^$(new List(new Decl(new ExternC_d($3),LOC(@1,@6)),$8));
      }
      else
      $$=^$(new List(new Decl(new ExternCinclude_d($3,cycdecls,exs,new $(wc, hides)),LOC(@1,@6)),$8));
   }
| PORTON ';' translation_unit
  { $$=^$(new List(new Decl(&Porton_d_val,SLOC(@1)),$3)); }
| PORTOFF ';' translation_unit
  { $$=^$(new List(new Decl(&Portoff_d_val,SLOC(@1)),$3)); }
| tempest_on_action ';' translation_unit
  { $$=^$(new List(new Decl(&Tempeston_d_val,SLOC(@1)),$3)); }
| tempest_off_action ';' translation_unit
  { $$=^$(new List(new Decl(&Tempestoff_d_val,SLOC(@1)),$3)); }
| /* empty */ { $$=^$(NULL); }
;

tempest_on_action:
  TEMPESTON { parsing_tempest = true; }
;
tempest_off_action:
  TEMPESTOFF { parsing_tempest = false; }
;

extern_c_action:
  EXTERN STRING
{ Lex::enter_extern_c();
  if (strcmp($2,"C") == 0)
    $$ = ^$(false);
  else if (strcmp($2,"C include") == 0)
    $$ = ^$(true);
  else {
    Warn::err(LOC(@1,@2),"expecting \"C\" or \"C include\"");
    $$ = ^$(true);
  }
}
;

end_extern_c:
  '}'  { Lex::leave_extern_c(); }
;

hide_list_opt:
                                { $$ = ^$(NULL); }
| HIDE '{' hide_list_values '}' { $$ = $!3;      }
;

hide_list_values:
  struct_union_name                      { $$=^$(new List($1,NULL)); }
| struct_union_name ';'                  { $$=^$(new List($1,NULL)); }
| struct_union_name ',' hide_list_values { $$=^$(new List($1,$3));   }
;

export_list_opt:
              { $$ = ^$(new $(NULL, 0)); }
| export_list { $$ = $!1; }
;

export_list:
  EXPORT '{' export_list_values '}' { $$= ^$(new $($3, 0)); } //<------- HERE
| EXPORT '{' '}'                    { $$=^$(new $(NULL, 0)); }
| EXPORT '{' '*' '}'                { $$=^$(new $(NULL, SLOC(@1))); }
;

export_list_values:
  struct_union_name optional_semi
  { $$=^$(new List(new $(SLOC(@1),$1,false),NULL)); }
| struct_union_name ',' export_list_values
  { $$=^$(new List(new $(SLOC(@1),$1,false),$3));}
;

override_opt:
                                    { $$ = ^$(NULL); }
| OVERRIDE '{' translation_unit '}' { $$ = $!3; }
;

external_declaration:
  function_definition {$$=^$(new List(new_decl(new Fn_d($1),SLOC(@1)),NULL));}
| declaration         {$$=$!1;}
| error               {$$=^$(NULL);}
;

optional_semi:
 ';'
|
;

//added some state so that we know we're in the inside of function definition
//this affects the introduction of Cvar in pointer types 
// function_definition:
//   declarator start_function fndef_compound_statement optional_semi end_function
//     { $$=^$(make_function(yyr,NULL,$1,NULL,$3,LOC(@1,@3))); }
// | declaration_specifiers declarator start_function compound_statement optional_semi end_function
//     { let d = $1;
//       $$=^$(make_function(yyr,&d,$2,NULL,$4,LOC(@1,@4))); }
// /* 2 shift-reduce conflicts come up because of the next two rules and
//    the final rule of function_definition2.  A declarator can be followed
//    by an attribute to become a declarator, and a declaration list can
//    begin with an attribute.  The default action of shift means the
//    attribute will end up attached to the declarator, which is probably
//    not the right decision.  However, this only comes up with old-style
//    function definitions, and, both attributes following declarators and
//    attributes beginning declarations are needed; so we ignore the issue for now
// */
// |  declarator declaration_list start_function compound_statement optional_semi end_function
//     { $$=^$(make_function(yyr,NULL,$1,$2,$4,LOC(@1,@4))); }
// |  declaration_specifiers declarator declaration_list start_function compound_statement optional_semi end_function
//     { let d = $1;
//     $$=^$(make_function(yyr,&d,$2,$3,$5,LOC(@1,@5))); }
// ;

function_definition:
  declarator fndef_compound_statement optional_semi
    { $$=^$(make_function(yyr,NULL,$1,NULL,$2,LOC(@1,@2))); }
| declaration_specifiers declarator fndef_compound_statement optional_semi
    { let d = $1;
      $$=^$(make_function(yyr,&d,$2,NULL,$3,LOC(@1,@3))); }
/* 2 shift-reduce conflicts come up because of the next two rules and
   the final rule of function_definition2.  A declarator can be followed
   by an attribute to become a declarator, and a declaration list can
   begin with an attribute.  The default action of shift means the
   attribute will end up attached to the declarator, which is probably
   not the right decision.  However, this only comes up with old-style
   function definitions, and, both attributes following declarators and
   attributes beginning declarations are needed; so we ignore the issue for now
*/
| declarator declaration_list fndef_compound_statement optional_semi
    { $$=^$(make_function(yyr,NULL,$1,$2,$3,LOC(@1,@3))); }
| declaration_specifiers declarator declaration_list fndef_compound_statement optional_semi
    { let d = $1;
      $$=^$(make_function(yyr,&d,$2,$3,$4,LOC(@1,@4))); }
;

/* Used for nested functions; the optional declaration_specifiers
   would cause parser conflicts */
function_definition2:
  declaration_specifiers declarator compound_statement
    { let d = $1;
      $$=^$(make_function(yyr,&d,$2,NULL,$3,LOC(@1,@3))); }
| declaration_specifiers declarator declaration_list compound_statement
    { let d = $1;
      $$=^$(make_function(yyr,&d,$2,$3,$4,LOC(@1,@4))); }
;

using_action:
  USING qual_opt_identifier { Lex::enter_using($2); $$=$!2; }
;
unusing_action:
  '}' { Lex::leave_using(); }
;
namespace_action:
  NAMESPACE IDENTIFIER { Lex::enter_namespace(new $2); $$=$!2; }
;
unnamespace_action:
  '}' { Lex::leave_namespace(); }
;
noinference_action: 
  NOINFERENCE {++inside_noinference_block;}
;
unnoinference_action:
  '}' {--inside_noinference_block;}
;
/***************************** DECLARATIONS *****************************/
declaration:
  declaration_specifiers ';'
    { $$=^$(make_declarations($1,NULL,SLOC(@1),SLOC(@1))); }
| declaration_specifiers init_declarator_list_rev ';'
    { $$=^$(make_declarations($1,flat_imp_rev($2),SLOC(@1),LOC(@1,@3))); }
/* Cyc: let declaration */
| LET pattern '=' expression ';'
    { $$=^$(new List(let_decl($2,$4,LOC(@1,@5)),NULL)); }
| LET identifier_list ';'
    { let vds = NULL;
      for (let ids = $2; ids != NULL; ids = ids->tl) {
        qvar_t qv = new $(Rel_n(NULL), ids->hd);
        vds = new List(new_vardecl(0,qv,wildtyp(NULL),NULL,NULL),vds);
      }
      $$=^$(new List(letv_decl(List::imp_rev(vds),LOC(@1,@3)),NULL));
    }
/* Cyc: region declaration */
/* region <`r> h;  */
| REGION '<' TYPE_VAR '>' IDENTIFIER ';'
  { tvar_ok($3,SLOC(@3));
    tvar_t tv = new Tvar(new $3,-1,Kinds::kind_to_bound(&Kinds::ek),NULL);
    type_t t  = var_type(tv);
    vardecl_t vd = new_vardecl(SLOC(@5), new $(Loc_n(),new $5),rgn_handle_type(t),NULL,NULL);
    $$ = ^$(new List(region_decl(tv,vd,NULL,LOC(@1,@6)),NULL));
  }
/* region h;  and region h = open(k); */
| REGION IDENTIFIER open_opt ';'
  { string_t tvstring = (string_t)aprintf("`%s",$2);
    tvar_ok($2,SLOC(@2));
    tvar_t tv = new Tvar(new tvstring, -1, Kinds::kind_to_bound(&Kinds::ek),NULL);
    type_t t = var_type(tv);
    vardecl_t vd = new_vardecl(SLOC(@2), new $(Loc_n(),new $2),rgn_handle_type(t),NULL,NULL);
    $$ = ^$(new List(region_decl(tv,vd,$3,LOC(@1,@4)),NULL));
  }
;

open_opt:
/* empty */ { $$=^$(NULL); }
| '=' IDENTIFIER '(' expression ')'
   {  if (strcmp($2,"open") != 0) Warn::err(SLOC(@4),"expecting `open'");
      $$ = ^$($4); }
;
declaration_list:
  declaration                  { $$=$!1; }
| declaration_list declaration { $$=^$(List::imp_append($1,$2)); }
;

/* Cyc: added optional attributes */
declaration_specifiers:
  storage_class_specifier
    { $$=^$(Declaration_spec($1,empty_tqual(SLOC(@1)),
                             empty_spec(0),false,NULL)); }
| storage_class_specifier declaration_specifiers
    { let two = $2;
      if (two.sc != None_sc)
        Warn::warn(LOC(@1,@2),
                     "Only one storage class is allowed in a declaration (missing ';' or ','?)");
      $$=^$(Declaration_spec($1,two.tq,two.type_specs,
                             two.is_inline,two.attributes));
    }
| EXTENSION declaration_specifiers
    { Warn::warn(LOC(@1,@1), "__extension__ keyword ignored in declaration");
      $$=$!2;
    }
| type_specifier
    { $$=^$(Declaration_spec(None_sc,empty_tqual(SLOC(@1)),
                             $1,false,NULL)); }
| type_specifier declaration_specifiers
    { let two = $2;
    $$=^$(Declaration_spec(two.sc,two.tq,
                           combine_specifiers(SLOC(@1),two.type_specs,$1),
                           two.is_inline, two.attributes));
    }
| type_qualifier
    { $$=^$(Declaration_spec(None_sc,$1,empty_spec(0),false,NULL)); }
| type_qualifier declaration_specifiers
    { let two = $2;
      $$=^$(Declaration_spec(two.sc,combine_tqual($1,two.tq),
                             two.type_specs, two.is_inline, two.attributes));
    }
| INLINE
    { $$=^$(Declaration_spec(None_sc,empty_tqual(SLOC(@1)),
                             empty_spec(0),true,NULL)); }
| INLINE declaration_specifiers
    { let two = $2;
      $$=^$(Declaration_spec(two.sc,two.tq,two.type_specs,true,two.attributes));
    }
| attributes
    { $$=^$(Declaration_spec(None_sc,empty_tqual(SLOC(@1)),
                             empty_spec(0),false,$1)); }
| attributes declaration_specifiers
    { let two = $2;
      $$=^$(Declaration_spec(two.sc,two.tq,
                             two.type_specs, two.is_inline,
                             List::imp_append($1,two.attributes))); }
;

storage_class_specifier:
  AUTO      { $$=^$(Auto_sc); }
| REGISTER  { $$=^$(Register_sc); }
| STATIC    { $$=^$(Static_sc); }
| EXTERN    { $$=^$(Extern_sc); }
| EXTERN STRING
  { if (strcmp($2,"C") != 0)
      Warn::err(LOC(@1,@2),"only extern or extern \"C\" is allowed");
    $$ = ^$(ExternC_sc);
  }
| TYPEDEF   { $$=^$(Typedef_sc); }
/* Cyc:  abstract specifier */
| ABSTRACT  { $$=^$(Abstract_sc); }
;

/* Cyc: added GCC attributes */
attributes_opt:
  /* empty */  { $$=^$(NULL); }
  | attributes { $$=$!1; }
;

attributes:
  ATTRIBUTE '(' '(' attribute_list ')' ')' { $$=$!4; }
;
attribute_list:
  attribute { $$=^$(new List($1,NULL)); }
| attribute ',' attribute_list { $$=^$(new List($1,$3)); }
;
attribute:
  IDENTIFIER { $$ = ^$(Atts::parse_nullary_att(SLOC(@1),$1)); }
| CONST      { $$ = ^$(&Atts::Const_att_val); } // because const a keyword!
| IDENTIFIER '(' assignment_expression ')'
             { $$ = ^$(Atts::parse_unary_att(SLOC(@1), $1, SLOC(@3), $3)); }
| IDENTIFIER '(' IDENTIFIER ',' INTEGER_CONSTANT ',' INTEGER_CONSTANT ')'
             { $$ = ^$(Atts::parse_format_att(LOC(@1,@8), SLOC(@3), $1, $3,
					      cnst2uint(SLOC(@5),$5),
					      cnst2uint(SLOC(@7),$7))); }
;

/***************************** TYPES *****************************/
/* we could be parsing a type or a type declaration (e.g., struct)
 * so we return a type_specifier value -- either a type, a type qualifier,
 * an integral type qualifier (signed, long, etc.) or a declaration.
 */
type_specifier:
  type_specifier_notypedef
    { $$ = $!1; }
| qual_opt_typedef type_params_opt
    { $$=^$(type_spec(typedef_type($1,$2,NULL,NULL),LOC(@1,@2))); }
;

type_specifier_notypedef:
  VOID      { $$=^$(type_spec(void_type,SLOC(@1))); }
| CHAR      { $$=^$(type_spec(char_type,SLOC(@1))); }
| SHORT     { $$=^$(short_spec(SLOC(@1))); }
| INT       { $$=^$(type_spec(sint_type,SLOC(@1))); }
| LONG      { $$=^$(long_spec(SLOC(@1))); }
| FLOAT     { $$=^$(type_spec(float_type,SLOC(@1))); }
| FLOAT128  { $$=^$(type_spec(float128_type,SLOC(@1))); }
| DOUBLE    { $$=^$(type_spec(double_type,SLOC(@1))); }
| SIGNED    { $$=^$(signed_spec(SLOC(@1))); }
| UNSIGNED  { $$=^$(unsigned_spec(SLOC(@1))); }
| COMPLEX   { $$=^$(complex_spec(SLOC(@1))); }
| enum_specifier            { $$=$!1; }
| struct_or_union_specifier { $$=$!1; }
/* GCC extension */
| TYPEOF '(' expression ')' { $$=^$(type_spec(typeof_type($3),LOC(@1,@4))); }
| BUILTIN_VA_LIST
   { $$=^$(type_spec(builtin_type("__builtin_va_list",&Kinds::bk),SLOC(@1))); }
/* Cyc: added datatypes */
| datatype_specifier { $$=$!1; }
/* Cyc: added type variables and optional type parameters to typedef'd names */
| type_var           { $$=^$(type_spec($1, SLOC(@1))); }
/* Cyc: everything below here is an addition */
| '_'                { $$=^$(type_spec(new_evar(NULL,NULL),SLOC(@1))); }
| '_' COLON_COLON kind
  { $$=^$(type_spec(new_evar(Kinds::kind_to_opt($3),NULL),LOC(@1,@3))); }
| '$' '(' parameter_list ')'
    { $$=^$(type_spec(tuple_type(map_c(get_tqual_typ,SLOC(@3),imp_rev($3))),
                      LOC(@1,@4))); }
| REGION_T '<' any_type_name right_angle
    { $$=^$(type_spec(rgn_handle_type($3),LOC(@1,@4))); }
| REGION_T
    { $$=^$(type_spec(rgn_handle_type(new_evar(&Kinds::eko, NULL)), SLOC(@1)));}
| AQUAL_T '<' aqual_specifier right_angle
    { $$=^$(type_spec(aqual_handle_type($3),LOC(@1,@4))); }
| TAG_T '<' any_type_name right_angle
    { $$=^$(type_spec(tag_type($3),LOC(@1,@4))); }
| TAG_T
    { $$=^$(type_spec(tag_type(new_evar(&Kinds::iko, NULL)),SLOC(@1))); }
| VALUEOF_T '(' expression ')'
    { $$=^$(type_spec(valueof_type($3),LOC(@1,@4))); }
| SUBSET_QUAL '(' specifier_qualifier_list declarator_withtypedef '|'
    constant_expression ')'
    { let $(tq,tspecs,atts) = $3;
      if (tq.loc == 0) tq.loc = SLOC(@3);
      let Declarator(qv,varloc,tms) = $4;
      let t = speclist2typ(tspecs, SLOC(@3));
      let $(tq2,t2,tvs,atts2) = apply_tms(tq,t,atts,tms);
      if (tvs != NULL) Warn::err2(SLOC(@4),"parameter with bad type params");
      if (is_qvar_qualified(qv))
        Warn::err2(SLOC(@1),"parameter cannot be qualified with a namespace");
      if (atts2 != NULL) Warn::warn2(LOC(@1,@4),"extra attributes on parameter, ignoring");
      let vd = new_vardecl(varloc, qv, t2, NULL, NULL);
      $$=^$(type_spec(new SubsetType(vd,$6,NULL),LOC(@1,@7))); 
    }
;

/* Cyc: new */
kind:
  field_name { $$=^$(Kinds::id_to_kind($1,SLOC(@1))); }
;

type_qualifier:
  CONST    { seg_t loc = (Flags::porting_c_code) ? SLOC(@1) : 0;
             $$=^$(Tqual(true,false,false,true,loc)); }
| VOLATILE { $$=^$(Tqual(false,true,false,false,0)); }
| RESTRICT { $$=^$(Tqual(false,false,true,false,0)); }
;

/* parsing of enum specifiers */
enum_specifier:
  ENUM qual_opt_identifier '{' enum_declaration_list '}'
  { let ed = new TypeDecl{new Enum_td(new Enumdecl(Public,$2,new Opt($4))),
                           LOC(@1,@5)};
    $$=^$(type_spec(new TypeDeclType(ed,NULL),LOC(@1,@5)));
  }
| ENUM qual_opt_identifier
  { $$=^$(type_spec(enum_type($2,NULL),LOC(@1,@2))); }
| ENUM '{' enum_declaration_list '}'
  { $$=^$(type_spec(anon_enum_type($3),LOC(@1,@4))); }
;

/* enum fields */
enum_field:
  qual_opt_identifier
  { $$ = ^$(new Enumfield($1,NULL,SLOC(@1))); }
| qual_opt_identifier '=' constant_expression
  { $$ = ^$(new Enumfield($1,$3,LOC(@1,@3))); }
;

enum_declaration_list:
  enum_field                           { $$ = ^$(new List($1,NULL)); }
| enum_field ','                       { $$ = ^$(new List($1,NULL)); }
| enum_field ',' enum_declaration_list { $$ = ^$(new List($1,$3));   }
;

/* parsing of struct and union specifiers. */
struct_or_union_specifier:
  struct_or_union '{' struct_declaration_list '}'
  { $$=^$(type_spec(new AnonAggrType($1,false,$3),LOC(@1,@4))); }
/* Cyc:  TAGGED_QUAL, type_params_opt are added */
| maybe_tagged_struct_union struct_union_name type_params_opt '{'
    type_params_opt optional_effconstr_qualbnd struct_declaration_list '}'
    { let ts = List::map_c(typ2tvar,SLOC(@3),$3);
      let exist_ts = List::map_c(typ2tvar,SLOC(@5),$5);
      let ec_qb = $6;
      let $(ec, qb) = ec_qb ? *ec_qb : $(NULL, NULL);
      let td = aggr_tdecl($1[1], Public, $2, ts,
                          aggrdecl_impl(exist_ts,ec,qb,$7,$1[0]), NULL,
                          LOC(@1,@8));
      $$ = ^$(type_spec(new TypeDeclType(td,NULL), LOC(@1,@8)));
   }
| maybe_tagged_struct_union struct_union_name type_params_opt
  { $$=^$(type_spec(aggr_type(UnknownAggr($1[1],$2,$1[0] ? new Opt(true) :NULL),
			      $3), LOC(@1,@3))); }
;

maybe_tagged_struct_union:
  TAGGED_QUAL struct_or_union { $$=^$($(true,$2));  }
| struct_or_union             { $$=^$($(false,$1)); }
;
struct_or_union:
  STRUCT { $$=^$(StructA); }
| UNION  { $$=^$(UnionA); }
;
type_params_opt:
  /* empty */                    { $$=^$(NULL); }
| '<' type_name_list right_angle { $$=^$(List::imp_rev($2)); }
;

struct_declaration_list:
  /* empty */
  { $$=^$(NULL); }
| struct_declaration_list0
  { list_t<aggrfield_t> decls = NULL;
    for (let x = $1; x != NULL; x = x->tl)
      decls = List::imp_append(x->hd,decls);
    //let decls = List::flatten(List::imp_rev($1));
    let tags = get_aggrfield_tags(decls);
    if (tags != NULL)
      List::iter_c(substitute_aggrfield_tags,tags,decls);
    $$=^$(decls);
  }
;

/* NB: returns list in reverse order */
struct_declaration_list0:
  struct_declaration                          { $$=^$(new List($1,NULL)); }
| struct_declaration_list0 struct_declaration { $$=^$(new List($2,$1));   }
;

/* NB: returns list in reverse order */
init_declarator_list_rev:
  init_declarator
    { $$=^$(rnew(yyr) FlatList(NULL,$1)); }
| init_declarator_list_rev ',' init_declarator
    { $$=^$(rnew(yyr) FlatList($1,$3)); }
;

init_declarator:
  declarator                 { $$=^$($($1,NULL,NULL)); }
| declarator ASM_TOK asm_expr { $$=^$($($1,NULL,new_exp($3,SLOC(@2)))); }
| declarator '=' initializer { $$=^$($($1,$3,NULL));   }
| declarator ASM_TOK asm_expr '=' initializer { $$=^$($($1,$5,new_exp($3,SLOC(@2))));   }
;

struct_declaration:
  specifier_qualifier_list struct_declarator_list_rev ';'
    { region temp;
      let $(tq,tspecs,atts) = $1;
      if (tq.loc == 0) tq.loc = SLOC(@1);
      declarators_t<`temp> decls = NULL;
      list_t<$(exp_opt_t,exp_opt_t)@`temp,`temp> widths_and_reqs = NULL;
      for (let x = $2; x != NULL; x = x->tl) {
	let &$(d,wd,wh) = x->hd;
	decls = rnew(temp) FlatList(decls,d);
	widths_and_reqs = rnew(temp) List(rnew(temp) $(wd,wh),widths_and_reqs);
      }
      let t = speclist2typ(tspecs, SLOC(@1));
      let info = List::rzip(temp,temp,
			    apply_tmss(temp,tq,t,decls,atts), widths_and_reqs);
      $$=^$(List::map_c(make_aggr_field,LOC(@1,@2),info));
   }
;

// we do this slightly differently:  at most one typedef name can occur
// within a specifier qualifier list, and that is useful since we can then
// allow certain declarators to use typedef names as the declared identifier.
specifier_qualifier_list:
  type_specifier
    { $$=^$($(empty_tqual(SLOC(@1)), $1, NULL)); }
| type_specifier notypedef_specifier_qualifier_list
    { let two=$2; $$=^$($(two[0], combine_specifiers(SLOC(@1),$1,two[1]), two[2]));}
| type_qualifier
    { $$=^$($($1,empty_spec(0),NULL)); }
| type_qualifier specifier_qualifier_list
    { let two=$2; $$=^$($(combine_tqual($1,two[0]), two[1], two[2])); }
| attributes
    { $$=^$($(empty_tqual(SLOC(@1)),empty_spec(0), $1)); }
| attributes specifier_qualifier_list
    { let two=$2; $$=^$($(two[0], two[1], append($1,two[2]))); }
;

// Same as above but does not allow typedef names as specifiers
notypedef_specifier_qualifier_list:
  type_specifier_notypedef
    { $$=^$($(empty_tqual(SLOC(@1)), $1, NULL)); }
| type_specifier_notypedef notypedef_specifier_qualifier_list
    { let two=$2; $$=^$($(two[0], combine_specifiers(SLOC(@1),$1,two[1]), two[2]));}
| type_qualifier
    { $$=^$($($1,empty_spec(0),NULL)); }
| type_qualifier notypedef_specifier_qualifier_list
    { let two=$2; $$=^$($(combine_tqual($1,two[0]), two[1], two[2])); }
| attributes
    { $$=^$($(empty_tqual(SLOC(@1)), empty_spec(0), $1)); }
| attributes notypedef_specifier_qualifier_list
    { let two=$2; $$=^$($(two[0], two[1], append($1,two[2]))); }
;

/* NB: returns list in reverse order */
struct_declarator_list_rev:
  struct_declarator
    { $$=^$(rnew(yyr) List($1,NULL)); }
| struct_declarator_list_rev ',' struct_declarator
    { $$=^$(rnew(yyr) List($3,$1)); }
;

struct_declarator:
  declarator_withtypedef requires_clause_opt
    { $$=^$(rnew (yyr) $($1,NULL,$2)); }
| ':' constant_expression
    { // HACK: give the field an empty name -- see elsewhere in the
      // compiler where we use this invariant
      $$=^$(rnew (yyr) $((Declarator(new $(Rel_n(NULL),new ""), 0, NULL)),$2,NULL));
    }
| /* empty */
    { // HACK: give the field an empty name -- see elsewhere in the
      // compiler where we use this invariant
      $$=^$(rnew (yyr) $((Declarator(new $(Rel_n(NULL),new ""),0, NULL)),NULL,NULL));
    }
| declarator_withtypedef ':' constant_expression
    { $$=^$(rnew (yyr) $($1,$3,NULL)); }
;

requires_clause_opt:
  REQUIRES_QUAL '(' constant_expression ')' { $$ = ^$($3); }
| /* empty */                               { $$ = ^$(NULL); }
;

// FIX: hack to have rgn_opt in 1st and 3rd cases
datatype_specifier:
  qual_datatype qual_opt_identifier type_params_opt '{' datatypefield_list '}'
    { let is_extensible = $1;
      let ts = List::map_c(typ2tvar,SLOC(@3),$3);
      let dd = datatype_tdecl(Public,$2,ts,new Opt($5),
                              is_extensible,LOC(@1,@6));
      $$ = ^$(type_spec(new TypeDeclType(dd,NULL),LOC(@1,@6)));
    }
| qual_datatype qual_opt_identifier type_params_opt
    { let is_extensible = $1;
      $$=^$(type_spec(datatype_type(UnknownDatatype(UnknownDatatypeInfo($2,is_extensible)), $3), LOC(@1,@3)));
    }
| qual_datatype qual_opt_identifier '.' qual_opt_identifier type_params_opt
   {  let is_extensible = $1;
      $$=^$(type_spec(datatype_field_type(UnknownDatatypefield(UnknownDatatypeFieldInfo($2,$4,is_extensible)),$5),LOC(@1,@5)));
    }
;

qual_datatype:
  DATATYPE                 { $$ = ^$(false); }
| EXTENSIBLE_QUAL DATATYPE { $$ = ^$(true);  }
;

datatypefield_list:
  datatypefield                        { $$=^$(new List($1,NULL)); }
| datatypefield ';'                    { $$=^$(new List($1,NULL)); }
| datatypefield ',' datatypefield_list { $$=^$(new List($1,$3));   }
| datatypefield ';' datatypefield_list { $$=^$(new List($1,$3));   }
;

datatypefield_scope:
         { $$=^$(Public);}
| EXTERN { $$=^$(Extern);}
| STATIC { $$=^$(Static);}
;
datatypefield:
  datatypefield_scope qual_opt_identifier
    { $$=^$(new Datatypefield($2,NULL,LOC(@1,@2),$1)); }
| datatypefield_scope qual_opt_identifier '(' parameter_list ')'
    { let typs = List::map_c(get_tqual_typ,SLOC(@4),List::imp_rev($4));
      $$=^$(new Datatypefield($2,typs,LOC(@1,@5),$1)); }
;

declarator:
  direct_declarator
    { $$=$!1; }
| pointer direct_declarator
    { let two = $2;
      $$=^$(Declarator(two.id, two.varloc, List::imp_append($1,two.tms))); }
;

// same as declarator but allows typedef name to occur as the variable
declarator_withtypedef:
  direct_declarator_withtypedef
    { $$=$!1; }
| pointer direct_declarator_withtypedef
    { let two = $2;
      $$=^$(Declarator(two.id, two.varloc, List::imp_append($1,two.tms))); }
;

direct_declarator:
  qual_opt_identifier { $$=^$(Declarator($1, SLOC(@1),NULL)); }
| '(' declarator ')'  { $$=$!2; }
/* the following rule causes a large number of shift/reduce conflicts
 * but seems necessary to parse some of the GCC header files. */
| '(' attributes declarator ')'
    { let d = $3;
      d.tms = rnew(yyr) List(rnew(yyr) Attributes_mod(SLOC(@2),$2),d.tms);
      $$=$!3;
    }
| direct_declarator '[' ']' zeroterm_qual_opt
    { $$=^$(Declarator($1.id, $1.varloc, rnew(yyr) List(rnew(yyr) Carray_mod($4,SLOC(@4)),$1.tms)));}
| direct_declarator '[' assignment_expression ']' zeroterm_qual_opt
{ $$=^$(Declarator($1.id, $1.varloc,
                       rnew(yyr) List(rnew(yyr) ConstArray_mod($3,$5,SLOC(@5)),$1.tms)));}
| direct_declarator '(' parameter_type_list ')' chk_req_ens_thr_opt
    { let &$(lis,b,c,eff,ec,qb) = $3;
      let $(chk,req,ens,thrws) = $5;
      $$=^$(Declarator($1.id, $1.varloc,rnew(yyr) List(rnew(yyr) Function_mod(rnew(yyr) WithTypes(lis,b,c,eff,ec,qb,chk,req,ens,thrws)),$1.tms)));
    }
| direct_declarator '(' identifier_list ')'
    { $$=^$(Declarator($1.id, $1.varloc, rnew(yyr) List(rnew(yyr) Function_mod(rnew(yyr) NoTypes($3,LOC(@1,@4))),$1.tms))); }
/* Cyc: added type parameters */
| direct_declarator '<' type_name_list right_angle
    { let ts = List::map_c(typ2tvar,LOC(@2,@4),List::imp_rev($3));
      $$=^$(Declarator($1.id, $1.varloc, rnew(yyr) List(rnew(yyr) TypeParams_mod(ts,LOC(@1,@4),false),$1.tms)));
    }
| direct_declarator attributes
  { $$=^$(Declarator($1.id, $1.varloc, rnew(yyr) List(rnew(yyr) Attributes_mod(SLOC(@2),$2),
                                         $1.tms)));
  }
;

// same as direct_declarator but allows typedef names to occur as the variable
direct_declarator_withtypedef:
  qual_opt_identifier            { $$=^$(Declarator($1, SLOC(@1), NULL)); }
| qual_opt_typedef               { $$=^$(Declarator($1, SLOC(@1), NULL));  }
| '(' declarator_withtypedef ')' { $$=$!2; }
/* the following rule causes a large number of shift/reduce conflicts
 * but seems necessary to parse some of the GCC header files. */
| '(' attributes declarator_withtypedef ')'
    { let d = $3;
      d.tms = rnew(yyr) List(rnew(yyr) Attributes_mod(SLOC(@2),$2),d.tms);
      $$=$!3;
    }
| direct_declarator_withtypedef '[' ']' zeroterm_qual_opt
    { let one=$1;
    $$=^$(Declarator(one.id, one.varloc,
                       rnew(yyr) List(rnew(yyr) Carray_mod($4,SLOC(@4)),one.tms)));}
| direct_declarator_withtypedef '[' assignment_expression ']' zeroterm_qual_opt
    { let one=$1;
    $$=^$(Declarator(one.id, one.varloc,
                       rnew(yyr) List(rnew(yyr) ConstArray_mod($3,$5,SLOC(@5)),
                                one.tms)));}
| direct_declarator_withtypedef '(' parameter_type_list ')' chk_req_ens_thr_opt
    { let &$(lis,b,c,eff,ec,qb) = $3;
      let $(chk,req,ens,thrws) = $5;
      let one=$1;
      $$=^$(Declarator(one.id, one.varloc, rnew(yyr) List(rnew(yyr) Function_mod(rnew(yyr) WithTypes(lis,b,c,eff,ec,qb,chk,req,ens,thrws)),one.tms)));
    }
| direct_declarator_withtypedef '(' identifier_list ')'
    { let one=$1;
      $$=^$(Declarator(one.id,one.varloc, rnew(yyr) List(rnew(yyr) Function_mod(rnew(yyr) NoTypes($3,LOC(@1,@4))),one.tms))); }
/* Cyc: added type parameters */
| direct_declarator_withtypedef '<' type_name_list right_angle
    { let ts = List::map_c(typ2tvar,LOC(@2,@4),List::imp_rev($3));
      let one=$1;
      $$=^$(Declarator(one.id,one.varloc, rnew(yyr) List(rnew(yyr) TypeParams_mod(ts,LOC(@1,@4),false),one.tms))); }
| direct_declarator_withtypedef attributes
  { let one=$1;
    $$=^$(Declarator(one.id, one.varloc, rnew(yyr) List(rnew(yyr) Attributes_mod(SLOC(@2),$2),one.tms))); }
;

/* CYC: region annotations allowed, as is zero-termination qualifier */
pointer:
  one_pointer         { $$ = $!1; }
| one_pointer pointer { $$ = ^$(imp_append($1,$2)); }
;
one_pointer:
  pointer_null_and_bound pointer_quals eff_opt attributes_opt tqual_list
  { list_t<type_modifier_t<`yy>,`yy> ans = NULL;
    if($4 != NULL)
      ans = rnew(yyr) List(rnew(yyr) Attributes_mod(SLOC(@4),$4), ans);
    // don't put location info on every pointer -- too expensive
    ptrloc_t ptrloc = NULL;
    let $(ploc,nullable,bound) = *$1;
    if (Flags::porting_c_code)
      ptrloc = new PtrLoc{.ptr_loc=ploc,.rgn_loc=SLOC(@3),.zt_loc=SLOC(@2)};
    let mod = make_pointer_mod(yyr,ptrloc,nullable,bound,$3,$2,$5);
    ans = rnew(yyr) List(mod, ans);
    $$=^$(ans);
  }
;
pointer_quals:
  /* empty */                { $$=^$(NULL); }
| pointer_qual pointer_quals { $$=^$(rnew(yyr) List($1,$2)); }
;

pointer_qual:
  NUMELTS_QUAL '(' assignment_expression ')'
  { $$ = ^$(rnew(yyr) Numelts_ptrqual($3)); }
| REGION_QUAL '(' any_type_name ')'
{  Warn::warn(SLOC(@1), "@region qualifiers are deprecated; use @effect instead");
 $$ = ^$(rnew(yyr) Effect_ptrqual(new List($3, NULL))); }
| EFFECT_QUAL '(' eff_set ')' { $$ = ^$(rnew(yyr) Effect_ptrqual($3)); }
| THIN_QUAL         { $$ = ^$(rnew(yyr) Thin_ptrqual); }
| FAT_QUAL          { $$ = ^$(rnew(yyr) Fat_ptrqual); }
| AUTORELEASED_QUAL { $$ = ^$(rnew(yyr) Autoreleased_ptrqual); }
| ZEROTERM_QUAL     { $$ = ^$(rnew(yyr) Zeroterm_ptrqual); }
| NOZEROTERM_QUAL   { $$ = ^$(rnew(yyr) Nozeroterm_ptrqual); }
| NOTNULL_QUAL      { $$ = ^$(rnew(yyr) Notnull_ptrqual); }
| NULLABLE_QUAL     { $$ = ^$(rnew(yyr) Nullable_ptrqual); }
| ALIAS_QUAL '(' aqual_specifier ')' { $$ = ^$(rnew(yyr) Alias_ptrqual($3)); }
| AQUAL_SHORT_CONST {_ aq = id2aqual(SLOC(@1), $1); $$ = ^$(rnew(yyr) Alias_ptrqual(aq)); }
;

aqual_specifier:
  aqual_const {$$ = ^$($1);}
| type_var
  {
    // tvar_ok($1,SLOC(@1));
    //     tvar_t tv = new Tvar(new $1,-1,Kinds::kind_to_bound(&Kinds::aqk),$2);
    //     type_t t  = var_type(tv);
    $$ = ^$(aqual_var_type($1, al_qual_type));//default aliasable -- real bound filled in by typechecker
  }
| AQUALS '(' any_type_name ')'
  {
    $$ = ^$(aqual_var_type(aqualsof_type($3), al_qual_type));
  }
;

aqual_opt:
{}
| AQUAL_SHORT_CONST {$$=^$(id2aqual(SLOC(@1), $1));}
;

pointer_null_and_bound:
  '*' pointer_bound
   { // avoid putting location info on here when not porting C code??
     $$=^$(new $(SLOC(@1),true_type,(parsing_tempest ? fat_bound_type : assign_cvar_pos(Position::string_of_segment(SLOC(@1)),false,$2)))); }
| '@' pointer_bound { $$=^$(new $(SLOC(@1), false_type, assign_cvar_pos(Position::string_of_segment(SLOC(@1)),false,$2))); }
| '?'               { $$=^$((Flags::override_fat && inside_noinference_block==0 && Flags::interproc) ?
			    new $(SLOC(@1),true_type,assign_cvar_pos(Position::string_of_segment(SLOC(@1)),true,cvar_type(&Kinds::ptrbko))): 
			    new $(SLOC(@1),true_type,  fat_bound_type)); }

pointer_bound:
/* empty */ { $$=^$((Flags::interproc && (inside_noinference_block==0)) ? cvar_type(&Kinds::ptrbko) : bounds_one()); }
| '{' assignment_expression '}' { $$=^$(thin_bounds_exp($2)); }
| '{' TYPE_VAR '}' 
{ 
  if (!Flags::resolve) 
    parse_abort(0, "Type variable not permitted in pointer bound");
  $$=^$(typevar2cvar($2));
}
;

zeroterm_qual_opt:
/* empty */       { $$ = ^$(Tcutil::any_bool(NULL)); }
| ZEROTERM_QUAL   { $$ = ^$(true_type);  }
| NOZEROTERM_QUAL { $$ = ^$(false_type); }
;

eff_set:
  type_var {$$ = ^$(new List($1, NULL));}
| type_var '+' eff_set {$$ = ^$(new List($1, $3));}
  ;

eff_opt:
/* empty */ { $$ = ^$(new_evar(&Kinds::eko,NULL)); }
| eff_set   { let es = $1; if (List::length(es) == 1) { $$ = ^$(es->hd);} else { $$ = ^$(join_eff($1));} }
| '_'       { $$ = ^$(new_evar(&Kinds::eko,NULL)); }
;

/* Always returns a type (possibly an evar) */
// rgn_opt:
// /* empty */ { $$ = ^$(new_evar(&Kinds::rko,NULL)); }
// | type_var  { set_vartyp_kind($1,&Kinds::rk,true); $$ = $!1; }
// | '_'       { $$ = ^$(new_evar(&Kinds::rko,NULL)); }
// ;

tqual_list:
/* empty */                 { $$ = ^$(empty_tqual(SLOC(@1))); }
| type_qualifier tqual_list { $$ = ^$(combine_tqual($1,$2));  }
;

parameter_type_list:
  optional_effect optional_effconstr_qualbnd
{ let ec_qb = $2;
  let $(ec, qb) = ec_qb ? *ec_qb : $(NULL, NULL);
  $$= ^$(new $(NULL,false,NULL,$1,ec,qb)); }
| parameter_list optional_effect optional_effconstr_qualbnd
    { let ec_qb = $3;
    let $(ec, qb) = ec_qb ? *ec_qb : $(NULL, NULL);
    $$=^$(new $(List::imp_rev($1),false,NULL,$2,ec,qb)); }
| parameter_list ',' ELLIPSIS optional_effect optional_effconstr_qualbnd
    { let ec_qb = $5;
    let $(ec, qb) = ec_qb ? *ec_qb : $(NULL, NULL);
    $$=^$(new $(List::imp_rev($1),true,NULL,$4,ec,qb)); }
| ELLIPSIS optional_inject parameter_declaration optional_effect
  optional_effconstr_qualbnd
{ let &$(n,tq,t) = $3;
  let v = new VarargInfo {.name = n,.tq = tq,.type = t,.inject = $2};
  let ec_qb = $5;
  let $(ec, qb) = ec_qb ? *ec_qb : $(NULL, NULL);
  $$=^$(new $(NULL,false,v,$4,ec,qb));
}
| parameter_list ',' ELLIPSIS optional_inject parameter_declaration
  optional_effect optional_effconstr_qualbnd
{ let &$(n,tq,t) = $5;
  let v = new VarargInfo {.name = n,.tq = tq,.type = t,.inject = $4};
  let ec_qb = $7;
  let $(ec, qb) = ec_qb ? *ec_qb : $(NULL, NULL);
  $$=^$(new $(List::imp_rev($1),false,v,$6,ec,qb));
}
;

/* CYC:  new */
opt_aqual_bnd:
{$$=^$(NULL);}
| AQUAL_SHORT_CONST {$$ = ^$(id2aqual(SLOC(@1), $1));}
;
type_var:
  TYPE_VAR opt_aqual_bnd  { $$ = ^$(id2type($1,new Unknown_kb(NULL),$2, SLOC(@1))); }
| TYPE_VAR COLON_COLON kind opt_aqual_bnd { $$ = ^$(id2type($1,Kinds::kind_to_bound($3),$4, SLOC(@1))); }
;
optional_effect:
  /* empty */    { $$=^$(NULL); }
| ';' effect_set { $$ = ^$(join_eff($2)); }
;

optional_effconstr_qualbnd:
  /* empty */    { $$ = ^$(NULL); }
| ':' effconstr_qualbnd  { $$ = $!2; }
;

effconstr_qualbnd:
  effconstr_elt
  { $$ = ^$(new $(list($1), NULL)); }
| qual_bnd_elt
  { $$ = ^$(new $(NULL, list($1))); }
| effconstr_elt ',' effconstr_qualbnd
  {
    let rest = $3;
    let $(rpo, _) = *rest;
    (*rest)[0] = new List($1, rpo);
    $$ = ^$(rest);
  }
| qual_bnd_elt ',' effconstr_qualbnd
  {
    let rest = $3;
    let $(_, qb) = *rest;
    (*rest)[1] = new List($1, qb);
    $$ = ^$(rest);
  }
;

effconstr_elt:
  atomic_effect LE_OP TYPE_VAR
  {
    let kb = new Less_kb(NULL,&Kinds::ek);
    let t = id2type($3,kb,NULL,SLOC(@3));
    $$ = ^$(new SubsetConstraint(effect_from_atomic($1), t));
  }
| atomic_effect '|' atomic_effect
{
  $$ = ^$(new DisjointConstraint(effect_from_atomic($1),
				 effect_from_atomic($3)));
}
| IDENTIFIER '(' atomic_effect ')'
{
  check_single_constraint(SLOC(@1), $1);
  $$ = ^$(new SingleConstraint(effect_from_atomic($3)));
}
;

qual_bnd_elt:
  qual_bnd_const GE_OP qual_bnd_term
{
  $$ = ^$(new $($3, $1));
}
;

aqual_const:
  AQ_ALIASABLE { $$ = ^$(al_qual_type);}
| AQ_UNIQUE { $$ = ^$(un_qual_type);}
| AQ_REFCNT { $$ = ^$(rc_qual_type);}
| AQ_RESTRICTED  { $$ = ^$(rtd_qual_type);}
| AQUAL_SHORT_CONST {$$ = ^$(id2aqual(SLOC(@1),$1));}
;

//allow aquals(`a) >= aquals(`b) ... not sure this is ever used
qual_bnd_const:
  aqual_const {$$ = ^$($1);}
| AQUALS '(' any_type_name ')'
{
  $$ = ^$(aqual_var_type(aqualsof_type($3), al_qual_type));
}
;

qual_bnd_term:
  TYPE_VAR
{
  let kb = new Eq_kb(&Kinds::aqk);
  $$ = ^$(id2type($1,kb,NULL,SLOC(@1)));
}
| AQUALS '(' any_type_name ')'
{
  $$ = ^$(aqualsof_type($3));
}
;

//not used anymore ... merged with qual_bnd
// optional_rgn_order:
//   /* empty */    { $$ = ^$(NULL); }
// | ':' rgn_order  { $$ = $!2; }
// ;

// rgn_order:
//   atomic_effect '>' TYPE_VAR
//   { // FIX: if we replace the following with:
//     // $$ = ^$(new List(new $(join_eff($1),id2type(id,new Less_kb(NULL,TopRgnKind))), NULL));
//     // then we get a core-dump.  I think it must be the gcc bug...
//     let kb = new Less_kb(NULL,&Kinds::trk);
//     let t = id2type($3,kb);
//     $$ = ^$(new List(new $(join_eff($1),t), NULL));
//   }
// | atomic_effect '>' TYPE_VAR ',' rgn_order
//   {
//     let kb = new Less_kb(NULL,&Kinds::trk);
//     let t = id2type($3,kb);
//     $$ = ^$(new List(new $(join_eff($1),t),$5));
//   }
// ;

optional_inject:
  /* empty */
  {$$ = ^$(false);}
| IDENTIFIER
     { if (zstrcmp($1,"inject") != 0)
         Warn::err2(SLOC(@1),"missing type in function declaration");
       $$ = ^$(true);
     }
;

effect_set:
  atomic_effect                { $$=$!1; }
| atomic_effect '+' effect_set { $$=^$(List::imp_append($1,$3)); }
;

atomic_effect:
  '{' '}'                       { $$=^$(NULL); }
| '{' region_set '}'            { $$=$!2; }
| REGIONS '(' any_type_name ')' { $$=^$(new List(regionsof_eff($3), NULL)); }
| type_var
  { set_vartyp_kind($1,&Kinds::ek,false);
    $$ = ^$(new List($1,NULL));
  }
;

/* CYC:  new */
region_set:
  type_name
{ $$ = ^$(new List(type_name_to_type($1,SLOC(@1)),NULL)); }
| type_name ',' region_set
{ $$ = ^$(new List(type_name_to_type($1,SLOC(@1)),$3)); }
;

/* NB: returns list in reverse order */
parameter_list:
  parameter_declaration                    { $$=^$(new List($1,NULL)); }
| parameter_list ',' parameter_declaration { $$=^$(new List($3,$1));   }
;

/* FIX: differs from grammar in K&R */
parameter_declaration:
  specifier_qualifier_list declarator_withtypedef
    { let $(tq,tspecs,atts) = $1;
      if (tq.loc == 0) tq.loc = SLOC(@1);
      let Declarator(qv,varloc,tms) = $2;
      let t = speclist2typ(tspecs, SLOC(@1));
      let $(tq2,t2,tvs,atts2) = apply_tms(tq,t,atts,tms);
      if (tvs != NULL)
        Warn::err2(SLOC(@2),"parameter with bad type params");
      if(is_qvar_qualified(qv))
        Warn::err2(SLOC(@1),"parameter cannot be qualified with a namespace");
      var_opt_t idopt = (*qv)[1];
      if (atts2 != NULL)
        Warn::warn2(LOC(@1,@2),"extra attributes on parameter, ignoring");
      $$=^$(new $(idopt,tq2,t2));
    }
| specifier_qualifier_list
    { let $(tq,tspecs,atts) = $1;
      if (tq.loc == 0) tq.loc = SLOC(@1);
      let t = speclist2typ(tspecs, SLOC(@1));
      if (atts != NULL)
        Warn::warn2(SLOC(@1),"bad attributes on parameter, ignoring");
      $$=^$(new $(NULL,tq,t));
    }
| specifier_qualifier_list abstract_declarator
    { let $(tq,tspecs,atts) = $1;
      if (tq.loc == 0) tq.loc = SLOC(@1);
      let t = speclist2typ(tspecs, SLOC(@1));
      let tms = $2.tms;
      let $(tq2,t2,tvs,atts2) = apply_tms(tq,t,atts,tms);
      if (tvs != NULL) // Ex: int (@)<`a>
        Warn::warn2(LOC(@1,@2),
                     "bad type parameters on formal argument, ignoring");
      if (atts2 != NULL)
        Warn::warn2(LOC(@1,@2),"bad attributes on parameter, ignoring");
      $$=^$(new $(NULL,tq2,t2));
    }
;

identifier_list:
  identifier_list0 { $$=^$(List::imp_rev($1)); }
;
identifier_list0:
  IDENTIFIER                      { $$=^$(new List(new $1,NULL)); }
| identifier_list0 ',' IDENTIFIER { $$=^$(new List(new $3,$1));   }
;

initializer:
  assignment_expression { $$=$!1; }
| array_initializer     { $$=$!1; }
;

array_initializer:
  '{' '}'
    { $$=^$(new_exp(new UnresolvedMem_e(NULL,NULL),LOC(@1,@2))); }
| '{' initializer_list '}'
    { $$=^$(new_exp(new UnresolvedMem_e(NULL,List::imp_rev($2)),LOC(@1,@3))); }
| '{' initializer_list ',' '}'
    { $$=^$(new_exp(new UnresolvedMem_e(NULL,List::imp_rev($2)),LOC(@1,@4))); }
| '{' FOR IDENTIFIER '<' expression ':' expression '}'
    { let vd = new_vardecl(SLOC(@3),new $(Loc_n(),new $3), uint_type,
                           uint_exp(0,SLOC(@3)),NULL);
      // make the index variable const
      vd->tq.real_const = true;
      $$=^$(new_exp(new Comprehension_e(vd, $5, $7, false),LOC(@1,@8)));
    }
| '{' FOR IDENTIFIER '<' expression ':' type_name '}'
{ // DJG: notice the parser drops the IDENTIFIER on the floor.
      let t = type_name_to_type($7,SLOC(@7));
      $$=^$(new_exp(new ComprehensionNoinit_e($5, t, false),LOC(@1,@8)));
    }
;

/* NB: returns list in reverse order */
initializer_list:
  initializer
    { $$=^$(new List(new $(NULL,$1),NULL)); }
| designation initializer
    { $$=^$(new List(new $($1,$2),NULL)); }
| initializer_list ',' initializer
    { $$=^$(new List(new $(NULL,$3),$1)); }
| initializer_list ',' designation initializer
    { $$=^$(new List(new $($3,$4),$1)); }
;

designation:
  designator_list '=' { $$=^$(List::imp_rev($1)); }
| field_name ':'      { $$=^$(new List(new FieldName(new $1),NULL)); }
;

/* NB: returns list in reverse order */
designator_list:
  designator                 { $$=^$(new List($1,NULL)); }
| designator_list designator { $$=^$(new List($2,$1));   }
;

designator:
  '[' constant_expression ']' { $$ = ^$(new ArrayElement($2));}
| '.' field_name              { $$ = ^$(new FieldName(new $2));}
;

type_name:
  specifier_qualifier_list
    { let $(tq,tss,atts) = $1;
      let t = speclist2typ(tss, SLOC(@1));
      if (atts != NULL)
        Warn::warn(SLOC(@1),"ignoring attributes in type");
      $$=^$(new $(NULL,tq,t));
    }
| specifier_qualifier_list abstract_declarator
    { let $(tq,tss,atts) = $1;
      let t = speclist2typ(tss, SLOC(@1));
      let tms = $2.tms;
      let t_info = apply_tms(tq,t,atts,tms);
      if (t_info[2] != NULL) // for example: int (@)<`a>
        Warn::warn(SLOC(@2),"bad type params, ignoring");
      if (t_info[3] != NULL)
        Warn::warn(SLOC(@2),"bad specifiers, ignoring");
      $$=^$(new $(NULL,t_info[0],t_info[1]));
    }
;

any_type_name:
  type_name                       { $$ = ^$(type_name_to_type($1,SLOC(@1))); }
| '{' '}'                         { $$ = ^$(join_eff(NULL)); }
| '{' region_set '}'              { $$ = ^$(join_eff($2)); }
| REGIONS '(' any_type_name ')'   { $$ = ^$(regionsof_eff($3)); }
| any_type_name '+' atomic_effect { $$ = ^$(join_eff(new List($1,$3))); }
| qual_bnd_const                 {$$ = ^$($1); }
;

/* Cyc: new */
/* NB: returns list in reverse order */
type_name_list:
  any_type_name                    { $$=^$(new List($1,NULL)); }
| type_name_list ',' any_type_name { $$=^$(new List($3,$1));   }
;

abstract_declarator:
  pointer                    { $$=^$(Abstractdeclarator($1)); }
| direct_abstract_declarator { $$=$!1; }
| pointer direct_abstract_declarator
    { $$=^$(Abstractdeclarator(List::imp_append($1,$2.tms))); }
;

direct_abstract_declarator:
  '(' abstract_declarator ')'
    { $$=$!2; }
| '[' ']' zeroterm_qual_opt
    { $$=^$(Abstractdeclarator(rnew(yyr) List(rnew(yyr) Carray_mod($3,SLOC(@3)),NULL))); }
| direct_abstract_declarator '[' ']' zeroterm_qual_opt
    { $$=^$(Abstractdeclarator(rnew(yyr) List(rnew(yyr) Carray_mod($4,SLOC(@4)),$1.tms)));}
| '[' assignment_expression ']' zeroterm_qual_opt
    { $$=^$(Abstractdeclarator(rnew(yyr) List(rnew(yyr) ConstArray_mod($2,$4,SLOC(@4)),NULL)));}
| direct_abstract_declarator '[' assignment_expression ']' zeroterm_qual_opt
    { $$=^$(Abstractdeclarator(rnew(yyr) List(rnew(yyr) ConstArray_mod($3,$5,SLOC(@5)),
                                            $1.tms)));
    }
| '(' parameter_type_list ')' chk_req_ens_thr_opt
    { let &$(lis,b,c,eff,ec,qb) = $2;
      let $(chk,req,ens,thrws) = $4;
      $$=^$(Abstractdeclarator(rnew(yyr) List(rnew(yyr) Function_mod(rnew(yyr) WithTypes(lis,b,c,eff,ec,qb,chk,req,ens,thrws)),NULL)));
    }
| direct_abstract_declarator '(' parameter_type_list ')' chk_req_ens_thr_opt
    { let &$(lis,b,c,eff,ec,qb) = $3;
      let $(chk,req,ens,thrws) = $5;
      $$=^$(Abstractdeclarator(rnew(yyr) List(rnew(yyr) Function_mod(rnew(yyr) WithTypes(lis,
                                                                           b,c,eff,ec,qb,chk,req,ens,thrws)),$1.tms)));
    }
/* Cyc: new */
| direct_abstract_declarator '<' type_name_list right_angle
    { let ts = List::map_c(typ2tvar,LOC(@2,@4),List::imp_rev($3));
      $$=^$(Abstractdeclarator(rnew(yyr) List(rnew(yyr) TypeParams_mod(ts,LOC(@2,@4),false),
                                      $1.tms)));
    }
| direct_abstract_declarator attributes
    { $$=^$(Abstractdeclarator(rnew(yyr) List(rnew(yyr) Attributes_mod(SLOC(@2),$2),$1.tms)));
    }
;

chk_req_ens_thr:
  CHECKS_QUAL '(' constant_expression ')'
{ $$ = ^$($($3,NULL,NULL,NULL)); }
| REQUIRES_QUAL '(' constant_expression ')'
{ $$ = ^$($(NULL,$3,NULL,NULL)); }
| ENSURES_QUAL '(' constant_expression ')'
{ $$ = ^$($(NULL,NULL,$3,NULL)); }
| THROWS_QUAL '(' constant_expression ')'
{ $$ = ^$($(NULL,NULL,NULL,$3)); }
| CHECKS_QUAL '(' constant_expression ')' chk_req_ens_thr
{ $$ = ^$(join_assns($($3,NULL,NULL,NULL),$5)); }
| REQUIRES_QUAL '(' constant_expression ')' chk_req_ens_thr
{ $$ = ^$(join_assns($(NULL,$3,NULL,NULL),$5)); }
| ENSURES_QUAL '(' constant_expression ')' chk_req_ens_thr
{ $$ = ^$(join_assns($(NULL,NULL,$3,NULL),$5)); }
| THROWS_QUAL '(' constant_expression ')' chk_req_ens_thr
{ $$ = ^$(join_assns($(NULL,NULL,NULL,$3),$5)); }
;

chk_req_ens_thr_opt:
  /* empty */               { $$=  ^$($(NULL,NULL,NULL,NULL)); }
| chk_req_ens_thr           { $$=$!1; }
;

/***************************** STATEMENTS *****************************/
statement:
  labeled_statement     { $$=$!1; }
| expression_statement  { $$=$!1; }
| compound_statement    { $$=$!1; }
| selection_statement   { $$=$!1; }
| iteration_statement   { $$=$!1; }
| jump_statement        { $$=$!1; }
;

/* Cyc: Unlike C, we do not treat case and default statements as labeled */
labeled_statement:
  IDENTIFIER ':' statement
    { $$=^$(new_stmt(new Label_s(new $1,$3),LOC(@1,@3))); }
;

expression_statement:
  ';'            { $$=^$(skip_stmt(SLOC(@1))); }
| expression ';' { $$=^$(exp_stmt($1,LOC(@1,@2))); }
//| error ';'      { $$=^$(skip_stmt(SLOC(@1))); }
;

start_fndef_block:
'{' {inside_function_definition=true;}
;

end_fndef_block:
'}' {inside_function_definition=false;}
;

fndef_compound_statement:
  start_fndef_block end_fndef_block  { $$=^$(skip_stmt(LOC(@1,@2))); }
| start_fndef_block block_item_list end_fndef_block { $$=$!2; }
//| '{' error '}'           { $$=^$(skip_stmt(LOC(@1,@2))); }
;

compound_statement:
  '{' '}'                 { $$=^$(skip_stmt(LOC(@1,@2))); }
| '{' block_item_list '}' { $$=$!2; }
//| '{' error '}'           { $$=^$(skip_stmt(LOC(@1,@2))); }
;

block_item_list:
  declaration { $$=^$(flatten_declarations($1,skip_stmt(SLOC(@1)))); }
| declaration block_item_list { $$=^$(flatten_declarations($1,$2)); }
| IDENTIFIER ':' declaration
{ $$=^$(new_stmt(new Label_s(new $1,flatten_declarations($3,skip_stmt(DL))),SLOC(@1))); }
| IDENTIFIER ':' declaration block_item_list
{ $$=^$(new_stmt(new Label_s(new $1,flatten_declarations($3,$4)),SLOC(@1))); }
| statement { $$=$!1; }
| statement block_item_list { $$=^$(seq_stmt($1,$2,LOC(@1,@2))); }
| function_definition2 { $$=^$(flatten_decl(new_decl(new Fn_d($1),SLOC(@1)),
					    skip_stmt(DL))); }
| function_definition2 block_item_list
   { $$=^$(flatten_decl(new_decl(new Fn_d($1),SLOC(@1)), $2)); }
;
/* This has the standard shift-reduce conflict which is properly resolved. */
selection_statement:
  IF '(' expression ')' statement
    { $$=^$(ifthenelse_stmt($3,$5,skip_stmt(DL),LOC(@1,@5))); }
| IF '(' expression ')' statement ELSE statement
    { $$=^$(ifthenelse_stmt($3,$5,$7,LOC(@1,@7))); }
//| IF error
//    { $$=^$(skip_stmt(LOC(@1,@2))); }
/* Cyc: the body of the switch statement cannot be an arbitrary
 * statement; it must be a list of switch_clauses */
| SWITCH '(' expression ')' '{' switch_clauses '}'
    { $$=^$(switch_stmt($3,$6,LOC(@1,@7))); }
/* Cyc: allow an identifier without the parens */
| SWITCH qual_opt_identifier '{' switch_clauses '}'
    { $$=^$(switch_stmt(unknownid_exp($2,SLOC(@2)),$4,LOC(@1,@5))); }
/* Cyc: allow tuples without the parens */
| SWITCH '$' '(' argument_expression_list ')' '{' switch_clauses '}'
    { $$=^$(switch_stmt(tuple_exp($4,LOC(@2,@5)),$7,LOC(@1,@8))); }
//| SWITCH error
//    { $$=^$(skip_stmt(LOC(@1,@2))); }
| TRY statement CATCH '{' switch_clauses '}'
    { $$=^$(trycatch_stmt($2,$5,LOC(@1,@6))); }
//| TRY error
//    { $$=^$(skip_stmt(LOC(@1,@2))); }
;

/* Cyc: unlike C, we only allow default or case statements within
 * switches.  Also unlike C, we support a more general form of pattern
 * matching within cases.
 * JGM: cases with an empty statement get an implicit fallthru.
 * Should also put in an implicit "break" for the last case but that's
 * better done by the control-flow-checking.
 */
switch_clauses:
  /* empty */
    { $$=^$(NULL); }
| DEFAULT ':' block_item_list switch_clauses
    { // JGM: some linux code has defaults coming before other cases.
      $$=^$(new List(new Switch_clause(new_pat(&Wild_p_val,SLOC(@1)),NULL,
                                       NULL,$3,LOC(@1,@3)), $4));}
| CASE exp_pattern ':' switch_clauses
    { $$=^$(new List(new Switch_clause($2,NULL,NULL,
				       fallthru_stmt(NULL,SLOC(@3)),
                                       LOC(@1,@4)),$4)); }
| CASE exp_pattern ':' block_item_list switch_clauses
    { $$=^$(new List(new Switch_clause($2,NULL,NULL,$4,LOC(@1,@4)),$5)); }
| CASE pattern AND_OP expression ':' switch_clauses
    { $$=^$(new List(new Switch_clause($2,NULL,$4,
                                       fallthru_stmt(NULL,SLOC(@5)),
                                       LOC(@1,@6)),$6)); }
| CASE pattern AND_OP expression ':' block_item_list switch_clauses
{ $$=^$(new List(new Switch_clause($2,NULL,$4,$6,LOC(@1,@7)),$7)); }
//| CASE error ':' block_item_list
//   { $$=^$(NULL); }
;

iteration_statement:
  WHILE '(' expression ')' statement
    { $$=^$(while_stmt($3,$5,LOC(@1,@5))); }
| DO statement WHILE '(' expression ')' ';'
    { $$=^$(do_stmt($2,$5,LOC(@1,@7))); }
| FOR '(' for_exp_opt ';' for_exp_opt ';' for_exp_opt ')' statement
    { $$=^$(for_stmt($3,$5,$7,$9,LOC(@1,@9))); }
| FOR '(' declaration for_exp_opt ';' for_exp_opt ')' statement
    {let s = for_stmt(false_exp(DL),$4,$6, $8,LOC(@1,@8));
      $$=^$(flatten_declarations($3,s)); }
;
for_exp_opt:
/* empty */  { $$=^$(true_exp(DL)); }
| expression { $$=$!1; }
;
jump_statement:
  GOTO IDENTIFIER ';'   { $$=^$(goto_stmt(new $2,LOC(@1,@2))); }
| CONTINUE ';'          { $$=^$(continue_stmt(SLOC(@1)));}
| BREAK ';'             { $$=^$(break_stmt(SLOC(@1)));}
| RETURN ';'            { $$=^$(return_stmt(NULL,SLOC(@1)));}
| RETURN expression ';' { $$=^$(return_stmt($2,LOC(@1,@2)));}
/* Cyc:  explicit fallthru for switches */
| FALLTHRU ';'          { $$=^$(fallthru_stmt(NULL,SLOC(@1)));}
| FALLTHRU '(' ')' ';'  { $$=^$(fallthru_stmt(NULL,SLOC(@1)));}
| FALLTHRU '(' argument_expression_list ')' ';'
                        { $$=^$(fallthru_stmt($3,SLOC(@1)));}
;

/***************************** PATTERNS *****************************/
/* Cyc:  patterns */

/* The next few productions replicate expressions except that the
 * terminal case is for patterns. */
exp_pattern:
  conditional_pattern { $$=$!1; }
;
conditional_pattern:
  logical_or_pattern { $$=$!1; }
| logical_or_pattern '?' expression ':' conditional_expression
{ $$=^$(exp_pat(conditional_exp(pat2exp($1),$3,$5,LOC(@1,@5)))); }
;
logical_or_pattern:
  logical_and_pattern {$$=$!1; }
| logical_or_pattern OR_OP logical_and_expression
{ $$=^$(exp_pat(or_exp(pat2exp($1),$3,LOC(@1,@3)))); }
;
logical_and_pattern:
  inclusive_or_pattern { $$=$!1; }
| inclusive_or_pattern AND_OP inclusive_or_expression
{ $$=^$(exp_pat(and_exp(pat2exp($1),$3,LOC(@1,@3)))); }
;
inclusive_or_pattern:
  exclusive_or_pattern { $$=$!1; }
| exclusive_or_pattern '|' exclusive_or_expression
   { $$=^$(exp_pat(prim2_exp(Bitor,pat2exp($1),$3,LOC(@1,@3)))); }
;
exclusive_or_pattern:
  and_pattern { $$=$!1; }
| and_pattern '^' exclusive_or_expression
   { $$=^$(exp_pat(prim2_exp(Bitxor,pat2exp($1),$3,LOC(@1,@3)))); }
;
and_pattern:
  equality_pattern { $$=$!1; }
| and_pattern '&' equality_expression
   { $$=^$(exp_pat(prim2_exp(Bitand,pat2exp($1),$3,LOC(@1,@3)))); }
;
equality_pattern:
  relational_pattern { $$=$!1; }
| equality_pattern equality_op relational_expression
{ $$=^$(exp_pat($2(pat2exp($1),$3,LOC(@1,@3)))); }
;
relational_pattern:
  shift_pattern { $$=$!1; }
| relational_pattern relational_op shift_expression
  { $$=^$(exp_pat(($2)(pat2exp($1),$3,LOC(@1,@3)))); }
;
shift_pattern:
  additive_pattern { $$=$!1; }
| shift_pattern LEFT_OP additive_expression
{$$=^$(exp_pat(prim2_exp(Bitlshift,pat2exp($1),$3,LOC(@1,@3)))); }
| shift_pattern RIGHT_OP additive_expression
{$$=^$(exp_pat(prim2_exp(Bitlrshift,pat2exp($1),$3,LOC(@1,@3))));}
;
additive_pattern:
  multiplicative_pattern { $$=$!1; }
| additive_pattern additive_op multiplicative_expression
  { $$=^$(exp_pat(prim2_exp($2,pat2exp($1),$3,LOC(@1,@3))));}
;
multiplicative_pattern:
  cast_pattern { $$=$!1; }
| multiplicative_pattern multiplicative_op cast_expression
  { $$=^$(exp_pat(prim2_exp($2,pat2exp($1),$3,LOC(@1,@3))));}
;
cast_pattern:
  unary_pattern { $$=$!1; }
| '(' type_name ')' cast_expression
{  let t = type_name_to_type($2,SLOC(@2));
   $$=^$(exp_pat(cast_exp(t,$4,true,Unknown_coercion,LOC(@1,@4))));
}
;

unary_pattern:
  postfix_pattern { $$=$!1; }
// disallow INC_OP and DEC_OP -- not constant expressions
// disallow & and * because we need them to mark patterns
| unary_operator cast_expression
  { $$=^$(exp_pat(prim1_exp($1,$2,LOC(@1,@2)))); }
| SIZEOF '(' type_name ')'
  { let t = type_name_to_type($3,SLOC(@3));
    $$=^$(exp_pat(sizeoftype_exp(t,LOC(@1,@4)))); }
| SIZEOF unary_expression
   { $$=^$(exp_pat(sizeofexp_exp($2,LOC(@1,@2)))); }
| OFFSETOF '(' type_name ',' field_expression ')'
   { $$=^$(exp_pat(offsetof_exp((*$3)[2],List::imp_rev($5),LOC(@1,@6)))); }
// disallow malloc, rmalloc, and cmalloc -- not constant expressions
;

postfix_pattern:
  primary_pattern { $$=$!1; }
// disallow subscript -- not constant expression
// disallow function call because we need them to mark patterns
// disallow x.f, x->f, x++, x-- -- not constant expressions
// disallow struct initializers -- not valid in a case anyway
;

primary_pattern:
  pattern { $$=$!1;}
;

pattern:
  '_'                 { $$=^$(new_pat(&Wild_p_val,SLOC(@1)));}
| '(' expression ')'  { $$=^$(exp_pat($2)); }
| qual_opt_identifier { $$=^$(new_pat(new UnknownId_p($1),SLOC(@1))); }
| '&' pattern         { $$=^$(new_pat(new Pointer_p($2),LOC(@1,@2))); }
| constant
  { exp_t e = $1;
    switch (e->r) { // FIX: need patterns for wchar_t
    case &Const_e({.Char_c = $(s,i)}):
      $$=^$(new_pat(new Char_p(i),e->loc)); break;
    case &Const_e({.Short_c = $(s,i)}):
      $$=^$(new_pat(new Int_p(s,i),e->loc)); break;
    case &Const_e({.Int_c = $(s,i)}):
      $$=^$(new_pat(new Int_p(s,i),e->loc)); break;
    case &Const_e({.Float_c = $(s,i)}):
      $$=^$(new_pat(new Float_p(s,i),e->loc)); break;
    case &Const_e({.Null_c = _}):
      $$=^$(new_pat(&Null_p_val,e->loc)); break;
    case &Const_e({.String_c = _}):
    case &Const_e({.Wstring_c = _}):
      Warn::err(SLOC(@1),"strings cannot occur within patterns"); break;
    case &Const_e({.LongLong_c = _}):
      Warn::err(SLOC(@1),"long long's in patterns not yet implemented"); break;
    default:
      Warn::err(SLOC(@1),"bad constant in case");
    }
  }
| IDENTIFIER IDENTIFIER pattern
    { if (strcmp($2,"as") != 0)
        Warn::err(SLOC(@2),"expecting `as'");
      $$=^$(new_pat(new Var_p(new_vardecl(SLOC(@1),new $(Loc_n(), new $1),void_type,NULL,NULL),
                              $3),SLOC(@1)));
    }
| IDENTIFIER '<' TYPE_VAR '>' type_name IDENTIFIER
    { if (strcmp($1,"alias") != 0)
        Warn::err(SLOC(@2),"expecting `alias'");
      let location = LOC(@1,@6);
      tvar_ok($3,location);
      tvar_t tv = new Tvar(new $3,-1,new Eq_kb(&Kinds::ek),NULL);
      vardecl_t vd = new_vardecl(SLOC(@1),new $(Loc_n(), new $6),
				 type_name_to_type($5,SLOC(@5)),NULL,NULL);
      $$ = ^$(new_pat(new AliasVar_p(tv,vd),location));
    }
| TYPEDEF_NAME '<' TYPE_VAR '>' type_name IDENTIFIER //why do we have this production twice?
    { if (strcmp($1,"alias") != 0)
        Warn::err(SLOC(@2),"expecting `alias'");
      let location = LOC(@1,@6);
      tvar_ok($3,location);
      tvar_t tv = new Tvar(new $3,-1,new Eq_kb(&Kinds::ek),NULL);
      vardecl_t vd = new_vardecl(SLOC(@1),new $(Loc_n(), new $6),
				 type_name_to_type($5,SLOC(@5)),NULL,NULL);
      $$ = ^$(new_pat(new AliasVar_p(tv,vd),location));
      }
| '$' '(' field_pattern_list ')'
    { let $(fps, dots) = *($3);
      $$=^$(new_pat(new Aggr_p(NULL,true,NULL,fps,dots),LOC(@1,@4)));
    }
| qual_opt_identifier '(' tuple_pattern_list ')'
    { let $(ps, dots) = *($3);
      $$=^$(new_pat(new UnknownCall_p($1,ps,dots),LOC(@1,@4)));
    }
| qual_opt_identifier '{' type_params_opt field_pattern_list '}'
   {  let $(fps, dots) = *($4);
      let exist_ts = List::map_c(typ2tvar,SLOC(@3),$3);
      let ai = UnknownAggr(StructA,$1,NULL);
      let typ = new AppType(new AggrCon(ai),NULL);
      $$=^$(new_pat(new Aggr_p(typ,false,exist_ts,fps,dots),LOC(@1,@5)));
   }
| '{' type_params_opt field_pattern_list '}'
   {  let $(fps, dots) = *($3);
      let exist_ts = List::map_c(typ2tvar,SLOC(@2),$2);
      $$=^$(new_pat(new Aggr_p(NULL,false,exist_ts,fps,dots),LOC(@1,@4)));
   }
| AND_OP pattern /* to allow &&p */
    { $$=^$(new_pat(new Pointer_p(new_pat(new Pointer_p($2),LOC(@1,@2))),LOC(@1,@2))); }
| '*' IDENTIFIER
    { $$=^$(new_pat(new Reference_p(new_vardecl(SLOC(@1), new $(Loc_n(), new $2),
						void_type,NULL,NULL),
                                    new_pat(&Wild_p_val,SLOC(@2))),
		    LOC(@1,@2))); }
| '*' IDENTIFIER IDENTIFIER pattern
    { if (strcmp($3,"as") != 0)
        Warn::err(SLOC(@3),"expecting `as'");
      $$=^$(new_pat(new Reference_p(new_vardecl(SLOC(@1),new $(Loc_n(), new $2),
						void_type,NULL,NULL),
                                    $4),LOC(@1,@2)));
    }
| IDENTIFIER '<' TYPE_VAR '>'
   { let tag = id2type($3,Kinds::kind_to_bound(&Kinds::ik),NULL,SLOC(@3));
     $$=^$(new_pat(new TagInt_p(typ2tvar(SLOC(@3),tag),
				new_vardecl(SLOC(@1),new $(Loc_n(),new $1),
					    tag_type(tag),NULL,NULL)),
		   LOC(@1,@4))); }
| IDENTIFIER '<' '_' '>'
   { let tv = Tcutil::new_tvar(Kinds::kind_to_bound(&Kinds::ik));
     $$=^$(new_pat(new TagInt_p(tv,
				new_vardecl(SLOC(@1), new $(Loc_n(),new $1),
					    tag_type(var_type(tv)),NULL,NULL)),
		   LOC(@1,@4))); }
;

tuple_pattern_list:
  tuple_pattern_list0 { $$=^$(new $(List::rev($1), false)); }
| tuple_pattern_list0 ',' ELLIPSIS { $$=^$(new $(List::rev($1), true)); }
| ELLIPSIS { $$=^$(new $(NULL, true)); }
;
tuple_pattern_list0:
  pattern                         { $$=^$(new List($1,NULL)); }
| tuple_pattern_list0 ',' pattern { $$=^$(new List($3,$1));   }
;
field_pattern:
  pattern             { $$=^$(new $(NULL,$1)); }
| designation pattern { $$=^$(new $($1,$2));   }
;
field_pattern_list:
  field_pattern_list0              { $$=^$(new $(List::rev($1), false)); }
| field_pattern_list0 ',' ELLIPSIS { $$=^$(new $(List::rev($1), true));  }
| ELLIPSIS                         { $$=^$(new $(NULL,          true));  }
;
field_pattern_list0:
  field_pattern                         { $$=^$(new List($1,NULL)); }
| field_pattern_list0 ',' field_pattern { $$=^$(new List($3,$1));   }
;

/***************************** EXPESSIONS *****************************/
expression:
  assignment_expression                { $$=$!1; }
| expression ',' assignment_expression { $$=^$(seq_exp($1,$3,LOC(@1,@3))); }
;

assignment_expression:
  conditional_expression { $$=$!1; }
| unary_expression assignment_operator assignment_expression
    { $$=^$(assignop_exp($1,$2,$3,LOC(@1,@3))); }
| unary_expression SWAP assignment_expression
    { $$=^$(swap_exp($1,$3,LOC(@1,@3))); }
;

assignment_operator:
  '='          { $$=^$(NULL); }
| MUL_ASSIGN   { $$=^$(new Opt(Times)); }
| DIV_ASSIGN   { $$=^$(new Opt(Div)); }
| MOD_ASSIGN   { $$=^$(new Opt(Mod)); }
| ADD_ASSIGN   { $$=^$(new Opt(Plus)); }
| SUB_ASSIGN   { $$=^$(new Opt(Minus)); }
| LEFT_ASSIGN  { $$=^$(new Opt(Bitlshift)); }
| RIGHT_ASSIGN { $$=^$(new Opt(Bitlrshift)); }
| AND_ASSIGN   { $$=^$(new Opt(Bitand)); }
| XOR_ASSIGN   { $$=^$(new Opt(Bitxor)); }
| OR_ASSIGN    { $$=^$(new Opt(Bitor)); }
;

conditional_expression:
  logical_or_expression   { $$=$!1; }
| logical_or_expression '?' expression ':' conditional_expression
    { $$=^$(conditional_exp($1,$3,$5,LOC(@1,@5))); }
/* Cyc: throw expressions */
| THROW conditional_expression { $$=^$(throw_exp($2,LOC(@1,@2))); }
/* Cyc: expressions to build heap-allocated objects and arrays */
| NEW array_initializer        { $$=^$(New_exp(NULL,$2,NULL,LOC(@1,@2))); }
| NEW logical_or_expression    { $$=^$(New_exp(NULL,$2,NULL,LOC(@1,@2))); }
| QNEW '(' expression ')' array_initializer        { $$=^$(New_exp(NULL,$5,$3,LOC(@1,@5))); }
| QNEW '(' expression ')' logical_or_expression    { $$=^$(New_exp(NULL,$5,$3,LOC(@1,@5))); }
| RNEW '(' expression ')' array_initializer {let $(rgn, qual) = split_seq($3); $$=^$(New_exp(rgn,$5,qual,LOC(@1,@5))); }
| RNEW '(' expression ')' logical_or_expression {   let $(rgn, qual) = split_seq($3); $$=^$(New_exp(rgn,$5,qual,LOC(@1,@5))); }
;

// opt_qual_exp:
// /* empty */ { $$=^$(NULL);}
// |  '<' primary_expression '>'
//   {
//     $$ = ^$($2);
//   }
// ;

constant_expression:
  conditional_expression { $$=$!1; }
;
logical_or_expression:
  logical_and_expression { $$=$!1; }
| logical_or_expression OR_OP logical_and_expression
    { $$=^$(or_exp($1,$3,LOC(@1,@3))); }
;
logical_and_expression:
  inclusive_or_expression { $$=$!1; }
| logical_and_expression AND_OP inclusive_or_expression
    { $$=^$(and_exp($1,$3,LOC(@1,@3))); }
;
inclusive_or_expression:
  exclusive_or_expression { $$=$!1; }
| inclusive_or_expression '|' exclusive_or_expression
    { $$=^$(prim2_exp(Bitor,$1,$3,LOC(@1,@3))); }
;
exclusive_or_expression:
  and_expression { $$=$!1; }
| exclusive_or_expression '^' and_expression
    { $$=^$(prim2_exp(Bitxor,$1,$3,LOC(@1,@3))); }
;
and_expression:
  equality_expression { $$=$!1; }
| and_expression '&' equality_expression
    { $$=^$(prim2_exp(Bitand,$1,$3,LOC(@1,@3))); }
;
equality_expression:
  relational_expression { $$=$!1; }
| equality_expression equality_op relational_expression
    { $$=^$($2($1,$3,LOC(@1,@3))); }
;
relational_expression:
  shift_expression { $$=$!1; }
| relational_expression relational_op shift_expression
   { $$=^$(($2)($1,$3,LOC(@1,@3))); }
;
shift_expression:
  additive_expression { $$=$!1; }
| shift_expression LEFT_OP additive_expression
    { $$=^$(prim2_exp(Bitlshift,$1,$3,LOC(@1,@3))); }
| shift_expression RIGHT_OP additive_expression
    { $$=^$(prim2_exp(Bitlrshift,$1,$3,LOC(@1,@3))); }
;
additive_expression:
  multiplicative_expression { $$=$!1; }
| additive_expression additive_op multiplicative_expression
    { $$=^$(prim2_exp($2,$1,$3,LOC(@1,@3))); }
;
multiplicative_expression:
  cast_expression { $$=$!1; }
| multiplicative_expression multiplicative_op cast_expression
    { $$=^$(prim2_exp($2,$1,$3,LOC(@1,@3))); }
;
equality_op:
  EQ_OP { $$=^$(eq_exp);  }
| NE_OP { $$=^$(neq_exp); }
;
relational_op:
  '<'   { $$=^$(lt_exp); }
| '>'   { $$=^$(gt_exp); }
| LE_OP { $$=^$(lte_exp); }
| GE_OP { $$=^$(gte_exp); }
;
additive_op:
  '+' { $$ = ^$(Plus); }
| '-' { $$ = ^$(Minus); }
;
multiplicative_op:
  '*' { $$ = ^$(Times); }
| '/' { $$ = ^$(Div); }
| '%' { $$ = ^$(Mod); }
;
cast_expression:
  unary_expression
    { $$=$!1; }
| '(' type_name ')' cast_expression
   { let t = type_name_to_type($2,SLOC(@2));
      $$=^$(cast_exp(t,$4,true,Unknown_coercion,LOC(@1,@4))); }
;

unary_expression:
  postfix_expression      { $$=$!1; }
| INC_OP unary_expression { $$=^$(increment_exp($2,PreInc,LOC(@1,@2))); }
| DEC_OP unary_expression { $$=^$(increment_exp($2,PreDec,LOC(@1,@2))); }
| '&' cast_expression     { $$=^$(address_exp($2,LOC(@1,@2))); }
| '*' cast_expression     { $$=^$(deref_exp  ($2,LOC(@1,@2))); }
| unary_operator cast_expression { $$=^$(prim1_exp($1,$2,LOC(@1,@2))); }
| SIZEOF '(' type_name ')'
  { let t = type_name_to_type($3,SLOC(@3));
    $$=^$(sizeoftype_exp(t,LOC(@1,@4)));
  }
| SIZEOF unary_expression        { $$=^$(sizeofexp_exp($2,LOC(@1,@2))); }
| OFFSETOF '(' type_name ',' field_expression ')'
   { let t = type_name_to_type($3,SLOC(@3));
     $$=^$(offsetof_exp(t,List::imp_rev($5),LOC(@1,@6)));
   }
/* Cyc: malloc, rmalloc, rvmalloc, qmalloc, qcalloc numelts, swap, etc. */
| MALLOC '(' assignment_expression ')'
   { $$=^$(new_exp(new Malloc_e(MallocInfo{Malloc,NULL,NULL,NULL,$3,false,false}),
                   LOC(@1,@4))); }
| QMALLOC '(' assignment_expression ',' assignment_expression ',' assignment_expression ')'
   { $$=^$(new_exp(new Malloc_e(MallocInfo{Malloc,$3,$5,NULL,$7,false,false}),
                   LOC(@1,@8))); }
| RMALLOC '(' expression ',' assignment_expression ')'
{  $$=^$(new_exp(new Malloc_e(MallocInfo{Malloc,$3,NULL,NULL,$5,false,false}),
	       LOC(@1,@6))); }
| RVMALLOC '(' expression ',' assignment_expression ')'
   { let $(rgn,qual) = split_seq($3);
     $$=^$(new_exp(new Malloc_e(MallocInfo{Vmalloc,rgn,qual,NULL,$5,false,false}),
                   LOC(@1,@6))); }
| RMALLOC_INLINE '(' expression ',' assignment_expression ')'
   { let $(rgn,qual) = split_seq($3);
     $$=^$(new_exp(new Malloc_e(MallocInfo{Malloc,rgn,qual,NULL,$5,false,true}),
                   LOC(@1,@6))); }
| CALLOC '(' assignment_expression ',' SIZEOF '(' type_name ')' ')'
   { let t = type_name_to_type($7,SLOC(@7));
     $$=^$(new_exp(new Malloc_e(MallocInfo{Calloc,NULL,NULL,new(t),$3,false,false}),
                   LOC(@1,@9))); }
| QCALLOC '(' assignment_expression ',' assignment_expression ',' assignment_expression ',' SIZEOF '(' type_name ')' ')'
   { let t = type_name_to_type($11,SLOC(@11));
     $$=^$(new_exp(new Malloc_e(MallocInfo{Calloc,$3,$5,new(t),$7,false,false}),
                   LOC(@1,@13))); }
| RCALLOC '(' assignment_expression ',' assignment_expression ','
              SIZEOF '(' type_name ')' ')'
   { let t = type_name_to_type($9,SLOC(@9));
     $$=^$(new_exp(new Malloc_e(MallocInfo{Calloc,$3,NULL,new(t),$5,false,false}),
                   LOC(@1,@11))); }
| NUMELTS '(' assignment_expression ')'
   { $$=^$(primop_exp(Numelts, list($3), LOC(@1,@4))); }
| TAGOF '(' assignment_expression ')'
   { $$=^$(tagof_exp($3, LOC(@1,@4))); }
| TAGCHECK '(' postfix_expression '.' field_name ')'
   { $$=^$(new_exp(new Tagcheck_e($3,new $5),LOC(@1,@6))); }
| TAGCHECK '(' postfix_expression PTR_OP field_name ')'
   { $$=^$(new_exp(new Tagcheck_e(deref_exp($3,SLOC(@3)),new $5),LOC(@1,@6))); }
| VALUEOF '(' type_name ')'
   { let t = type_name_to_type($3,SLOC(@3));
     $$=^$(valueof_exp(t, LOC(@1,@4))); }
| ASM_TOK   asm_expr         { $$=^$(new_exp($2, SLOC(@1))); }
| EXTENSION unary_expression { $$=^$(extension_exp($2,LOC(@1,@3))); }
| ASSERT '(' assignment_expression ')' { $$=^$(assert_exp($3,false,LOC(@1,@4)));}
| ASSERT_QUAL '(' assignment_expression ')' { $$=^$(assert_exp($3,true,LOC(@1,@4)));}
| ASSERT_FALSE_QUAL '(' assignment_expression ')' { $$=^$(assert_false_exp($3,LOC(@1,@4)));}
;

unary_operator:
  '~' { $$=^$(Bitnot); }
| '!' { $$=^$(Not);    }
| '-' { $$=^$(Minus);  }
| '+' { $$=^$(Plus);   }
;

asm_expr:
  volatile_opt '(' STRING asm_out_opt ')'
{  let &$(outlist, inlist, clobbers) = $4;
   $$ = ^$(new Asm_e($1, $3, outlist, inlist, clobbers)); }
;

volatile_opt:
           { $$ = ^$(false); }
| VOLATILE { $$ = ^$(true);  }
;

asm_out_opt:
{$$ = ^$(new $(NULL, NULL, NULL));}
| ':' asm_in_opt
{  let &$(inlist, clobbers) = $2;
   $$ = ^$(new $(NULL, inlist, clobbers)); }
| ':' asm_outlist asm_in_opt
{  let &$(inlist, clobbers) = $3;
   $$ = ^$(new $(List::imp_rev($2), inlist, clobbers));}
;
asm_outlist:
  asm_io_elt                 { $$ = ^$(new List($1, NULL)); }
| asm_outlist ',' asm_io_elt { $$ = ^$(new List($3, $1));   }//reversed
;

asm_in_opt:
/* empty */                       { $$ = ^$(new $(NULL, NULL)); }
| ':' asm_clobber_opt             { $$ = ^$(new $(NULL, $2));   }
| ':' asm_inlist asm_clobber_opt  { $$ = ^$(new $(List::imp_rev($2), $3)); }
;
asm_inlist:
  asm_io_elt                { $$ = ^$(new List($1, NULL)); }
| asm_inlist ',' asm_io_elt { $$ = ^$(new List($3, $1));} //reversed
;

asm_io_elt:
  STRING '(' expression ')' { $$ = ^$(new $($1, $3)); }
;

//contains only register names ... interpreted
asm_clobber_opt:
  /* empty */          { $$ = ^$(NULL); }
| ':'                  { $$ = ^$(NULL); }
| ':' asm_clobber_list { $$ = ^$(List::imp_rev($2)); }
;
asm_clobber_list:
  STRING                      { $$ = ^$(new List(new $1, NULL)); }
| asm_clobber_list ',' STRING { $$ = ^$(new List(new $3, $1));   }
;

postfix_expression:
  primary_expression
    { $$= $!1; }
| postfix_expression '[' expression ']'
    { $$=^$(subscript_exp($1,$3,LOC(@1,@4))); }
| postfix_expression '(' ')'
    { $$=^$(unknowncall_exp($1,NULL,LOC(@1,@3))); }
| postfix_expression '(' argument_expression_list ')'
    { $$=^$(unknowncall_exp($1,$3,LOC(@1,@4))); }
| postfix_expression '.' field_name
    { $$=^$(aggrmember_exp($1,new $3,LOC(@1,@3))); }
| postfix_expression PTR_OP field_name
    { $$=^$(aggrarrow_exp($1,new $3,LOC(@1,@3))); }
| postfix_expression INC_OP
    { $$=^$(increment_exp($1,PostInc,LOC(@1,@2))); }
| postfix_expression DEC_OP
    { $$=^$(increment_exp($1,PostDec,LOC(@1,@2))); }
| '(' type_name ')' '{' '}'
    { $$=^$(new_exp(new CompoundLit_e($2,NULL),LOC(@1,@5))); }
| '(' type_name ')' '{' initializer_list '}'
    { $$=^$(new_exp(new CompoundLit_e($2,List::imp_rev($5)),LOC(@1,@6))); }
| '(' type_name ')' '{' initializer_list ',' '}'
    { $$=^$(new_exp(new CompoundLit_e($2,List::imp_rev($5)),LOC(@1,@7))); }
;

field_expression:
  field_name
    { $$=^$(new List(new StructField(new $1),NULL)); }
/* not checking sign here...*/
| INTEGER_CONSTANT
   { let i = cnst2uint(SLOC(@1),$1);
     let &FieldName(f) = tuple_field_designator(i);
     $$ = ^$(new List(new StructField(f),NULL)); }
| field_expression '.' field_name
    { $$ = ^$(new List(new StructField(new $3),$1)); }
/* not checking sign here...*/
| field_expression '.' INTEGER_CONSTANT
    { let i = cnst2uint(SLOC(@3),$3);
      let &FieldName(f) = tuple_field_designator(i);
      $$ = ^$(new List(new StructField(f),$1)); }
;

primary_expression:
/* qual_opt_identifer could be an identifier, struct tag, or datatype constructor */
  qual_opt_identifier        { $$=^$(unknownid_exp($1,SLOC(@1))); }
| PRAGMA '(' IDENTIFIER ')'  { $$=^$(pragma_exp($3,LOC(@1,@4))); }
| constant                   { $$= $!1; }
| STRING                     { $$=^$(string_exp($1,SLOC(@1))); }
| WSTRING                    { $$=^$(wstring_exp($1,SLOC(@1))); }
| '(' expression ')'         { $$= $!2; }
//| '(' error ')'
//    { $$=^$(null_exp(SLOC(@2))); }
/* Cyc: stop instantiation */
| primary_expression LEFT_RIGHT { $$=^$(noinstantiate_exp($1, LOC(@1,@2)));}
 /* Cyc: explicit instantiation */
| primary_expression '@' '<' type_name_list right_angle
    { $$=^$(instantiate_exp($1, List::imp_rev($4),LOC(@1,@5))); }
/* Cyc: structure expressions */
| qual_opt_identifier '{' type_params_opt initializer_list '}'
    { $$=^$(new_exp(new Aggregate_e($1,$3,List::imp_rev($4),NULL),LOC(@1,@5))); }
/* Cyc: tuple expressions */
| '$' '(' argument_expression_list ')' { $$=^$(tuple_exp($3,LOC(@1,@4))); }
/* Cyc: compound statement expressions, as in gcc */
| '(' '{' block_item_list '}' ')'      { $$=^$(stmt_exp($3,LOC(@1,@5))); }
;

argument_expression_list:
  argument_expression_list0 { $$=^$(List::imp_rev($1)); }
;
/* NB: returns list in reverse order */
argument_expression_list0:
  assignment_expression
    { $$=^$(new List($1,NULL)); }
| argument_expression_list0 ',' assignment_expression
    { $$=^$(new List($3,$1)); }
;

/* NB: We've had to move enumeration constants into primary_expression
   because the lexer can't tell them from ordinary identifiers */
constant:
  INTEGER_CONSTANT   { $$=^$(const_exp($1, SLOC(@1))); }
| CHARACTER_CONSTANT { $$=^$(char_exp($1, SLOC(@1)));  }
| WCHARACTER_CONSTANT{ $$=^$(wchar_exp($1, SLOC(@1))); }
/* Cyc: NULL */
| NULL_kw            { $$=^$(null_exp(SLOC(@1)));}
| FLOATING_CONSTANT  {
     let f = $1;
     int l = strlen(f);
     int i = 1; // indicates double
     if (l>0) {
       char c = f[l-1];
       if (c=='f' || c=='F') i = 0;      // float
       else if (c=='l' || c=='L') i = 2; // long double
     }
     $$=^$(float_exp(f, i, SLOC(@1)));
   }
;

qual_opt_identifier:
  IDENTIFIER      { $$=^$(new $(Rel_n(NULL), new $1)); }
| QUAL_IDENTIFIER { $$=$!1; }
;
qual_opt_typedef:
  TYPEDEF_NAME      { $$=^$(new $(Rel_n(NULL), new $1)); }
| QUAL_TYPEDEF_NAME { $$=$!1; }
;
// Hacks to allow typedef nanmes to overlap with struct/union names and
// field names and kinds.  Still cannot be "inject"
struct_union_name:
  qual_opt_identifier { $$=$!1; }
| qual_opt_typedef    { $$=$!1; }
;
field_name:
  IDENTIFIER   { $$=$!1; }
| TYPEDEF_NAME { $$=$!1; }
;
// Hack for parsing >> as two > when dealing with nested type-parameter lists
right_angle:
  '>'      {}
| RIGHT_OP { yylex_buf->lex_curr_pos -= 1; }
;
//grammar for parsing a constraint graph
all_constraints:
{$$ = ^$(NULL);}
| TYPE_VAR STRING ',' STRING '(' constraint_list_opt ')'  all_constraints
{ $$ = ^$(new List(new $(assign_cvar_pos($2, (strcmp($4, "true")==0), typevar2cvar($1)), $6), $8)); }
;

constraint_list_opt:
{$$ = ^$(NULL);}
| constraint_list {$$ =^$($1);}
;

constraint_list:
constraint {$$ = ^$(new List($1, NULL));}
| constraint ';' constraint_list {$$ = ^$(new List($1, $3));}
;
 
tvar_or_string:
  TYPE_VAR {$$ = ^$(typevar2cvar($1));}
| STRING {$$ = ^$(str2type(SLOC(@1),$1));}
;

constraint:
  STRING '!' '(' constraint ')' {$$ = ^$(BansheeIf::add_location($1, BansheeIf::check_constraint($4)));}
| STRING '^' '(' c_op ',' tvar_or_string ',' tvar_or_string ')' {$$ = ^$(BansheeIf::add_location($1, comparison_constraint($4, $6, $8)));}
| STRING '?' '(' tvar_or_string ','  tvar_or_string ')' {$$ = ^$(BansheeIf::add_location($1,BansheeIf::cond_equality_constraint($4, $6)));}
| STRING '=' '(' tvar_or_string ','  tvar_or_string ')' {$$ = ^$(BansheeIf::add_location($1, BansheeIf::equality_constraint($4, $6)));}
| STRING '<' '(' tvar_or_string ','  tvar_or_string ')' {$$ = ^$(BansheeIf::add_location($1,BansheeIf::inclusion_constraint($4, $6)));}
| STRING '>' '(' constraint ','  constraint ')' {$$ = ^$(BansheeIf::add_location($1,BansheeIf::implication_constraint($4, $6)));}
| STRING '+' '(' c_op ',' constraint  ','  constraint  ')' {$$ = ^$(BansheeIf::add_location($1,composite_constraint($4, $6, $8)));}
  ;
c_op:
  'A' {$$ = ^$(C_AND_OP);}
| 'V' {$$ = ^$(C_OR_OP);}
| '!' {$$ = ^$(C_NOT_OP);}
| '=' {$$ = ^$(C_EQ_OP);}
| '<' {$$ = ^$(C_INCL_OP);}
;
%%

void yyprint(int i, union YYSTYPE<`yy> v) {
  switch (v) {
  case {.Int_tok = c}:    fprintf(stderr,"%s",Absynpp::cnst2string(c)); break;
  case {.Char_tok = c}:   fprintf(stderr,"%c",c); break;
  case {.String_tok = s}: fprintf(stderr,"\"%s\"",s); break;
  case {.QualId_tok = q}: fprintf(stderr,"%s",Absynpp::qvar2string(q)); break;
  case {.Exp_tok = e}:    fprintf(stderr,"%s",Absynpp::exp2string(e)); break;
  case {.Stmt_tok = s}:   fprintf(stderr,"%s",Absynpp::stmt2string(s)); break;
  default: fprintf(stderr,"?"); break;
  }
}

string_t token2string(int token) {
  if (token <= 0)
    return "end-of-file";
  if (token == IDENTIFIER)
    return Lex::token_string;
  if (token == QUAL_IDENTIFIER)
    return Absynpp::qvar2string(Lex::token_qvar);
  int z = YYTRANSLATE(token);
  if ((unsigned)z < numelts(yytname))
    return yytname[z];
  return NULL;
}

namespace Parse{
list_t<decl_t> parse_file(FILE @`H f) {
  parse_result = NULL;
  region yyr;
  yyparse(yyr,Lexing::from_file(f));
  return parse_result;
}

list_t<$(type_t, list_t<constraint_t>)@> parse_constraint_file(FILE @`H f) {
  constraint_graph = NULL;
  region yyr;
  yyparse(yyr,Lexing::from_file(f));
  return constraint_graph;
}
}
