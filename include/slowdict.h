#ifndef _SLOWDICT_H_
#define _SLOWDICT_H_
////////////////////////////////////////////////////////////////////////////
// Popcorn library, file slowdict.h                                       //
// Copyright Greg Morrisett, Dan Grossman                                 //
// January 1999, all rights reserved                                      //
//                                                                        //
////////////////////////////////////////////////////////////////////////////
#include <core.h>
#include <list.h>

namespace SlowDict {
using List;

// slowdict.h:   defines polymorphic, functional, finite maps from types 'a to 
//           'b, where the domain must have a total order.  Follows the
//           conventions of the Ocaml dict library as much as possible.
// unlike Dict, we support delete.

// TODO: add region support

extern struct Dict<`a,`b,`e::E>;
typedef struct Dict<`a,`b,`e> @ hdict_t<`a,`b,`e>;
typedef hdict_t<`a,`b,{}> dict_t<`a,`b>;

// Raised when a key is present but not expected (e.g., insert_new) 
extern xtunion exn {extern Present};
// Raised when a key is not present but expected (e.g., lookup) 
extern xtunion exn {extern Absent};

// Given a comparison function, return an empty dict. 
extern hdict_t<`a,`b,`e> empty(int comp(`a,`a;`e));

// Determine whether a dict is empty 
extern bool is_empty(hdict_t<`a,`b,`e> d);

// Return true if entry indexed by key is present, false otherwise 
extern bool member(hdict_t<`a,`b,`e> d,`a key);

// Inserts a key/data pair into a dictionary, replacing any existing
// pair with the same key. 
extern hdict_t<`a,`b,`e> insert(hdict_t<`a,`b,`e> d,`a key,`b data);

// Inserts a key/data pair into a dictionary, raising Present if
// there is any existing pair with the same key.
extern hdict_t<`a,`b,`e> insert_new(hdict_t<`a,`b,`e> d,`a key,`b data);

// Insert a list of key/data pairs into a dictionary, replacing
// duplicate keys.
extern hdict_t<`a,`b,`e> 
  inserts(hdict_t<`a,`b,`e> d,glist_t<$(`a,`b)@`r1,`r2> kds);

// Return a dictionary containing exactly one key/data pair. 
extern hdict_t<`a,`b,`e> singleton(int comp(`a,`a;`e),`a key,`b data);

// Lookup a key in the dictionary, returning its associated data. If the key
// is not present, raise Absent.
extern `b lookup(hdict_t<`a,`b,`e> d,`a key);

// Same as lookup but doesnt raise an exception -- rather, returns an
// option.
extern Core::opt_t<`b> lookup_opt(hdict_t<`a,`b,`e> d,`a key);

// Delete a key/pair from the dict if present. 
extern hdict_t<`a,`b,`e> delete(hdict_t<`a,`b,`e> d,`a key);

// Delete a key/pair from the dict.  Raise Absent if key doesn't exist 
extern hdict_t<`a,`b,`e> delete_present(hdict_t<`a,`b,`e> d,`a key);

// Fold a function f across the dictionary yielding an accumulator. 
extern `c fold(`c f(`a,`b,`c),hdict_t<`a,`b,`e> d,`c accum);
// Same but fold an unboxed closure across the dictionary
extern `c fold_c(`c f(`d,`a,`b,`c),`d env, hdict_t<`a,`b,`e> dict,`c accum);

// Apply function f to every element in the dictionary.  Ignore result. 
extern void app(`c f(`a,`b),hdict_t<`a,`b,`e> d);
// Same but apply an unboxed closure across the dictionary
extern void app_c(`c f(`d,`a,`b),`d env,hdict_t<`a,`b,`e> d);
// void versions of the above
extern void iter(void f(`a,`b),hdict_t<`a,`b,`e> d);
extern void iter_c(void f(`c,`a,`b),`c env,hdict_t<`a,`b,`e> d);

// Given a function that maps 'b values to 'c values, convert an
// hdict_t<'a,'b> to a hdict_t<'a,'c> by applying the function to each
// data item.
extern hdict_t<`a,`c,`e> map(`c f(`b),hdict_t<`a,`b,`e> d);
// Same but map an unboxed closure across the dictionary
extern hdict_t<`a,`c,`e> map_c(`c f(`d,`b),`d env,hdict_t<`a,`b,`e> d);

// Return a key/data pair (in this case -- the first one in the dict).
// If the dict is empty, raise Absent.
extern $(`a,`b)@ choose(hdict_t<`a,`b,`e> d);

// Return an association list containing all the elements
extern list_t<$(`a,`b)@> to_list(hdict_t<`a,`b,`e> d);

}
#endif