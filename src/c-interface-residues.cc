/* src/c-interface-residues.cc
 * 
 * Copyright 2012 by The University of Oxford
 * Copyright 2015 by Medical Research Council
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or (at
 * your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc.,  51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 
 */

#if defined (USE_PYTHON)
#include "Python.h"  // before system includes to stop "POSIX_C_SOURCE" redefined problems
#endif

#include "compat/coot-sysdep.h"

#ifdef USE_GUILE
#include <cstddef> // define std::ptrdiff_t for clang
#include <libguile.h>
#endif

#include <vector>
#include "named-rotamer-score.hh"

#include "cc-interface.hh"
#include "graphics-info.h"

#include "c-interface.h" // for is_valid_model_molecule()

#include "c-interface-python.hh"

std::vector<coot::named_rotamer_score>
score_rotamers(int imol, 
	       const char *chain_id, 
	       int res_no, 
	       const char *ins_code, 
	       const char *alt_conf, 
	       int imol_map, 
	       int clash_flag,
	       float lowest_probability) {

   std::vector<coot::named_rotamer_score> v;
   if (is_valid_model_molecule(imol)) {
      if (is_valid_map_molecule(imol_map)) {
	 graphics_info_t g;
	 const clipper::Xmap<float> &xmap = g.molecules[imol_map].xmap;
	 v = graphics_info_t::molecules[imol].score_rotamers(chain_id, res_no, ins_code,
							     alt_conf,
							     clash_flag, lowest_probability,
							     xmap, *g.Geom_p());
      }
   }
   return v; 
} 
 

#ifdef USE_GUILE
SCM score_rotamers_scm(int imol, 
		       const char *chain_id, 
		       int res_no, 
		       const char *ins_code, 
		       const char *alt_conf, 
		       int imol_map, 
		       int clash_flag, 
		       float lowest_probability) {

   SCM r = SCM_EOL;
   std::vector<coot::named_rotamer_score> v =
      score_rotamers(imol, chain_id, res_no, ins_code, alt_conf,
		     imol_map, clash_flag, lowest_probability);
   for (unsigned int i=0; i<v.size(); i++) {
      SCM name_scm  = scm_from_locale_string(v[i].name.c_str());
      SCM prob_scm  = scm_double2num(v[i].rotamer_probability_score);
      SCM fit_scm   = scm_double2num(v[i].density_fit_score);
      SCM clash_scm = scm_double2num(v[i].clash_score);
      SCM atom_list_scm = SCM_EOL;
      for (unsigned int iat=0; iat<v[i].density_score_for_atoms.size(); iat++) {
	 SCM p1 = scm_from_locale_string(v[i].density_score_for_atoms[iat].first.c_str());
	 SCM p2 = scm_double2num(v[i].density_score_for_atoms[iat].second);
	 SCM atom_item = scm_list_2(p1,p2);
	 atom_list_scm = scm_cons(atom_item, atom_list_scm);
      }
      atom_list_scm = scm_reverse(atom_list_scm);
      SCM item = scm_list_5(name_scm, prob_scm, atom_list_scm, fit_scm, clash_scm);
      
      r = scm_cons(item, r);
   }
   r = scm_reverse(r);
   return r;
} 
#endif



#ifdef USE_PYTHON
// return a list (possibly empty)
PyObject *score_rotamers_py(int imol, 
			    const char *chain_id, 
			    int res_no, 
			    const char *ins_code, 
			    const char *alt_conf, 
			    int imol_map, 
			    int clash_flag, 
			    float lowest_probability) {
   
   std::vector<coot::named_rotamer_score> v =
      score_rotamers(imol, chain_id, res_no, ins_code, alt_conf,
		     imol_map, clash_flag, lowest_probability);
   PyObject *r = PyList_New(v.size());
   for (unsigned int i=0; i<v.size(); i++) { 
      PyObject *item = PyList_New(5);
      PyObject *name_py  = PyString_FromString(v[i].name.c_str());
      PyObject *prob_py  = PyFloat_FromDouble(v[i].rotamer_probability_score);;
      PyObject *fit_py   = PyFloat_FromDouble(v[i].density_fit_score);;
      PyObject *clash_py = PyFloat_FromDouble(v[i].clash_score);;
      PyObject *atom_list_py = PyList_New(v[i].density_score_for_atoms.size());
      for (unsigned int iat=0; iat<v[i].density_score_for_atoms.size(); iat++) {
	 PyObject *atom_item = PyList_New(2);
	 PyObject *p0 = PyString_FromString(v[i].density_score_for_atoms[iat].first.c_str());
	 PyObject *p1 = PyFloat_FromDouble(v[i].density_score_for_atoms[iat].second);
	 PyList_SetItem(atom_item, 0, p0);
	 PyList_SetItem(atom_item, 1, p1);
	 PyList_SetItem(atom_list_py, iat, atom_item);
      }
      PyList_SetItem(item, 0, name_py);
      PyList_SetItem(item, 1, prob_py);
      PyList_SetItem(item, 2, fit_py);
      PyList_SetItem(item, 3, atom_list_py);
      PyList_SetItem(item, 4, clash_py);
      PyList_SetItem(r, i, item);
   }
   return r;
} 
#endif


void
print_glyco_tree(int imol, const std::string &chain_id, int res_no, const std::string &ins_code) {

   if (is_valid_model_molecule(imol)) {
      graphics_info_t g;
      mmdb::Residue *r = g.molecules[imol].get_residue(chain_id, res_no, ins_code);
      mmdb::Manager *mol = g.molecules[imol].atom_sel.mol;
	 
      if (r) {

	 std::vector<std::string> types_with_no_dictionary =
	    g.molecules[imol].no_dictionary_for_residue_type_as_yet(*g.Geom_p());
	 for (unsigned int i=0; i<types_with_no_dictionary.size(); i++)
	    g.Geom_p()->try_dynamic_add(types_with_no_dictionary[i], 41);

	 coot::glyco_tree_t t(r, mol, g.Geom_p());

      } 
   } 
} 


/* ------------------------------------------------------------------------- */
/*                      interesting positions list                           */
/* ------------------------------------------------------------------------- */

#ifdef USE_GUILE
// pass a list of (position,string) pairs
void register_interesting_positions_list_scm(SCM pos_list) {

   std::vector<std::pair<clipper::Coord_orth, std::string> > v;
   graphics_info_t g;

   if (scm_is_true(scm_list_p(pos_list))) {
      unsigned int pos_length = scm_to_int(scm_length(pos_list));
      for (unsigned int i=0; i<pos_length; i++) { 
	 SCM item = scm_list_ref(pos_list, SCM_MAKINUM(i));
	 if (scm_is_true(scm_list_p(item))) {
	    // pos, label pair
	    unsigned int item_length = scm_to_int(scm_length(item));
	    if (item_length == 2) {
	       SCM item_item_0 = scm_list_ref(item, 0);
	       SCM item_item_1 = scm_list_ref(item, 0);

	       if (scm_is_true(scm_list_p(item_item_0))) {
		  unsigned int l_p = scm_to_int(scm_length(item_item_0));
		  if (l_p == 3) {
		     SCM x = scm_list_ref(item_item_0, SCM_MAKINUM(0));
		     SCM y = scm_list_ref(item_item_0, SCM_MAKINUM(1));
		     SCM z = scm_list_ref(item_item_0, SCM_MAKINUM(2));
		     if (scm_number_p(x)) { 
			if (scm_number_p(y)) { 
			   if (scm_number_p(z)) {
			      clipper::Coord_orth pos(scm_to_double(x),
						      scm_to_double(y),
						      scm_to_double(z));

			      if (scm_is_true(scm_string_p(item_item_1))) { 
				 std::string s = scm_to_locale_string(item_item_1);
				 std::pair<clipper::Coord_orth, std::string> p(pos,s);
				 v.push_back(p);
			      }
			   }
			}
		     }
		  }
	       }
	    }
	 }
      }
   } 
   
   g.register_user_defined_interesting_positions(v);
}
#endif // USE_GUILE

#ifdef USE_PYTHON
void register_interesting_positions_list_py(PyObject *pos_list) {

   std::vector<std::pair<clipper::Coord_orth, std::string> > v;

   if (PyList_Check(pos_list)) {
      unsigned int pos_length = PyObject_Length(pos_list);
      for (unsigned int i=0; i<pos_length; i++) {
	 PyObject *item = PyList_GetItem(pos_list, i);
	 if (PyList_Check(item)) {
	    unsigned int l_item = PyObject_Length(item);
	    if (l_item == 2) {
	       PyObject *item_item_0 = PyList_GetItem(item, 0);
	       PyObject *item_item_1 = PyList_GetItem(item, 1);

	       if (PyString_Check(item_item_1)) {
		  if (PyList_Check(item_item_0)) {

		     unsigned int l_item_item = PyObject_Length(item_item_0);
		     if (l_item_item == 3) {
			PyObject *x = PyList_GetItem(item_item_0, 0);
			PyObject *y = PyList_GetItem(item_item_0, 1);
			PyObject *z = PyList_GetItem(item_item_0, 2);

			if (PyFloat_Check(x)) { 
			   if (PyFloat_Check(y)) { 
			      if (PyFloat_Check(z)) {

				 clipper::Coord_orth pos(PyFloat_AsDouble(x),
							 PyFloat_AsDouble(y),
							 PyFloat_AsDouble(z));
				 std::string s = PyString_AsString(item_item_1);
				 std::pair<clipper::Coord_orth, std::string> p(pos,s);
				 v.push_back(p);
			      }
			   }
			}
		     }
		  }
	       }
	    }
	 }
      }
   }

   graphics_info_t g;
   g.register_user_defined_interesting_positions(v);
}
#endif // USE_PYTHON 


#ifdef USE_GUILE
SCM glyco_tree_scm(int imol, SCM active_residue_scm) {

   SCM r = SCM_BOOL_F;

   if (is_valid_model_molecule(imol)) {

      coot::residue_spec_t residue_spec = residue_spec_from_scm(active_residue_scm);
      graphics_info_t g;
      mmdb::Residue *residue_p = g.molecules[imol].get_residue(residue_spec);
      mmdb::Manager *mol = g.molecules[imol].atom_sel.mol;
      std::vector<std::string> types_with_no_dictionary =
	 g.molecules[imol].no_dictionary_for_residue_type_as_yet(*g.Geom_p());
      for (unsigned int i=0; i<types_with_no_dictionary.size(); i++)
	 g.Geom_p()->try_dynamic_add(types_with_no_dictionary[i], 41);
      coot::glyco_tree_t t(residue_p, mol, g.Geom_p());
   }
   return r;
}
#endif

#ifdef USE_PYTHON
PyObject *glyco_tree_py(int imol, PyObject *active_residue_py) {

   PyObject *r = Py_False;
   if (is_valid_model_molecule(imol)) {

      coot::residue_spec_t residue_spec = residue_spec_from_py(active_residue_py);
      graphics_info_t g;
      mmdb::Residue *residue_p = g.molecules[imol].get_residue(residue_spec);
      mmdb::Manager *mol = g.molecules[imol].atom_sel.mol;
      std::vector<std::string> types_with_no_dictionary =
	 g.molecules[imol].no_dictionary_for_residue_type_as_yet(*g.Geom_p());
      for (unsigned int i=0; i<types_with_no_dictionary.size(); i++)
	 g.Geom_p()->try_dynamic_add(types_with_no_dictionary[i], 41);
      coot::glyco_tree_t t(residue_p, mol, g.Geom_p());

   }

   if (PyBool_Check(r))
     Py_INCREF(r);

   return r;

}
#endif /* PYTHON */

#ifdef USE_PYTHON
PyObject *glyco_tree_residues_py(int imol, PyObject *active_residue_py) {

   PyObject *r = Py_False;
   if (is_valid_model_molecule(imol)) {

      coot::residue_spec_t residue_spec = residue_spec_from_py(active_residue_py);
      graphics_info_t g;
      mmdb::Residue *residue_p = g.molecules[imol].get_residue(residue_spec);
      mmdb::Manager *mol = g.molecules[imol].atom_sel.mol;
      std::vector<std::string> types_with_no_dictionary =
	 g.molecules[imol].no_dictionary_for_residue_type_as_yet(*g.Geom_p());
      for (unsigned int i=0; i<types_with_no_dictionary.size(); i++)
	 g.Geom_p()->try_dynamic_add(types_with_no_dictionary[i], 41);
      coot::glyco_tree_t t(residue_p, mol, g.Geom_p());
      std::vector<mmdb::Residue *> v_residues;
      std::vector<coot::residue_spec_t> v(v_residues.size());
      r = PyList_New(v_residues.size());
      for (unsigned int i=0; i<v_residues.size(); i++) {
	 coot::residue_spec_t spec(v_residues[i]);
	 PyList_SetItem(r, i, residue_spec_to_py(spec));
      }
   }

   if (PyBool_Check(r))
     Py_INCREF(r);

   return r;

}
#endif /* PYTHON */
