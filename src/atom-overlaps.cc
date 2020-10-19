/* src/atom-overlaps.cc
 * 
 * Copyright 2015 by Medical Research Council
 * Author: Paul Emsley
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA
 */


#ifdef USE_PYTHON
#include <Python.h>
#endif // USE_PYTHON

// #include <stdio.h>
// #include <string.h>

// #include <string>
// #include <vector>
#include <iostream>
// #include <algorithm>
// #include <map>

// #include "coot-utils/coot-coord-utils.hh"
// #include "utils/coot-utils.hh"

#include <gtk/gtk.h>
#include "graphics-info.h"
#include "c-interface.h"   // is_valid_model_molecule()
#include "cc-interface.hh" // residue_spec_from_scm()
#include "c-interface-ligands-swig.hh" // where these functions are declared.

#include "coot-utils/atom-overlaps.hh"

#include "guile-fixups.h"

#ifdef USE_GUILE
// internal bumps scoring, sphere overlap
SCM ligand_atom_overlaps_scm(int imol, SCM ligand_spec, double neighb_radius) {

   SCM r = SCM_BOOL_F;
   if (is_valid_model_molecule(imol)) {
      graphics_info_t g;
      coot::residue_spec_t rs = residue_spec_from_scm(ligand_spec);
      mmdb::Residue *residue_p = g.molecules[imol].get_residue(rs);
      if (residue_p) {
	 mmdb::Manager *mol = g.molecules[imol].atom_sel.mol;
	 std::vector<mmdb::Residue *> neighb_residues =
	    coot::residues_near_residue(residue_p, mol, neighb_radius);
	 
	 if (neighb_residues.size()) {
	    coot::atom_overlaps_container_t ol(residue_p, neighb_residues, mol, g.Geom_p());
	    ol.make_overlaps();
	    r = SCM_EOL;
	    for (unsigned int i=0; i<ol.overlaps.size(); i++) {
	       SCM spec_1_scm = atom_spec_to_scm(coot::atom_spec_t(ol.overlaps[i].atom_1));
	       SCM spec_2_scm = atom_spec_to_scm(coot::atom_spec_t(ol.overlaps[i].atom_2));
	       SCM sb = SCM_BOOL_F;
	       if (ol.overlaps[i].is_h_bond) sb = SCM_BOOL_T;
	       SCM l = scm_list_4(spec_1_scm,
				  spec_2_scm,
				  scm_double2num(ol.overlaps[i].overlap_volume),
				  sb);
	       r = scm_cons(l, r);
	    }
	    r = scm_reverse(r);
	 }
      }
   }; 
   return r;
} 
#endif

#ifdef USE_PYTHON
// internal bumps scoring, sphere overlap
PyObject *ligand_atom_overlaps_py(int imol, PyObject *ligand_spec, double neighb_radius) {

   PyObject *r = Py_False;

   if (is_valid_model_molecule(imol)) {
      graphics_info_t g;
      coot::residue_spec_t rs = residue_spec_from_py(ligand_spec);
      mmdb::Residue *residue_p = g.molecules[imol].get_residue(rs);
      if (residue_p) {
	 mmdb::Manager *mol = g.molecules[imol].atom_sel.mol;
	 std::vector<mmdb::Residue *> neighb_residues =
	    coot::residues_near_residue(residue_p, mol, neighb_radius);
	 
	 if (neighb_residues.size()) {
	    coot::atom_overlaps_container_t ol(residue_p, neighb_residues, mol, g.Geom_p());
	    ol.make_overlaps();

	    // convert ol to a python object.  FIXME
	 }
      } 
   };

   if (PyBool_Check(r)) {
     Py_INCREF(r);
   }
   return r;
} 
#endif

#ifdef USE_GUILE
SCM molecule_atom_overlaps_scm(int imol) {

   // if the return list is null then that's possibly because there was a missing dictionary.
   // return a string in that case.

   SCM r = SCM_BOOL_F;
   if (is_valid_model_molecule(imol)) {
      mmdb::Manager *mol = graphics_info_t::molecules[imol].atom_sel.mol;
      bool ignore_waters = false;

      coot::atom_overlaps_container_t overlaps(mol, graphics_info_t::Geom_p(), ignore_waters, 0.5, 0.25);
      overlaps.make_all_atom_overlaps();
      std::vector<coot::atom_overlap_t> olv = overlaps.overlaps;
      r = SCM_EOL;
      for (std::size_t ii=0; ii<olv.size(); ii++) {
	 const coot::atom_overlap_t &o = olv[ii];
	 coot::atom_spec_t spec_1(o.atom_1);
	 coot::atom_spec_t spec_2(o.atom_2);
	 SCM spec_1_scm = atom_spec_to_scm(spec_1);
	 SCM spec_2_scm = atom_spec_to_scm(spec_2);
	 SCM r_1_scm = scm_double2num(o.r_1);
	 SCM r_2_scm = scm_double2num(o.r_2);
	 SCM ov_scm  = scm_double2num(o.overlap_volume);
	 SCM item_scm = scm_list_5(spec_1_scm, spec_2_scm, r_1_scm, r_2_scm, ov_scm);
	 r = scm_cons(item_scm, r);
      }
      r = scm_reverse(r);

      // if the list is null then that's possibly because there was a missing dictionary.
      // return a string in that case.
      //
      if (olv.empty()) {
         if (!overlaps.get_have_dictionary()) {
            r = scm_from_locale_string("WARNING:: No-dictionary (something missing) ");
         }
      }
   }
   return r;
}
#endif // USE_GUILE


#ifdef USE_PYTHON
PyObject *molecule_atom_overlaps_py(int imol) {

   PyObject *r = Py_False;

   if (is_valid_model_molecule(imol)) {

      mmdb::Manager *mol = graphics_info_t::molecules[imol].atom_sel.mol;
      bool ignore_waters = false;

      coot::atom_overlaps_container_t overlaps(mol, graphics_info_t::Geom_p(), ignore_waters, 0.5, 0.25);
      overlaps.make_all_atom_overlaps();
      std::vector<coot::atom_overlap_t> olv = overlaps.overlaps;
      PyObject *o_py = PyList_New(olv.size());
      for (std::size_t ii=0; ii<olv.size(); ii++) {
	 const coot::atom_overlap_t &o = olv[ii];
	 if (false) // debug
	    std::cout << "Overlap " << ii << " "
		      << coot::atom_spec_t(o.atom_1) << " "
		      << coot::atom_spec_t(o.atom_2) << " overlap-vol "
		      << o.overlap_volume << " r_1 "
		      << o.r_1 << " r_2 " << o.r_2 << std::endl;
	 PyObject *item_dict_py = PyDict_New();
	 coot::atom_spec_t spec_1(o.atom_1);
	 coot::atom_spec_t spec_2(o.atom_2);
	 PyObject *spec_1_py = atom_spec_to_py(spec_1);
	 PyObject *spec_2_py = atom_spec_to_py(spec_2);
	 PyObject *r_1_py = PyFloat_FromDouble(o.r_1);
	 PyObject *r_2_py = PyFloat_FromDouble(o.r_2);
	 PyObject *ov_py  = PyFloat_FromDouble(o.overlap_volume);
	 PyDict_SetItemString(item_dict_py, "atom-1-spec", spec_1_py);
	 PyDict_SetItemString(item_dict_py, "atom-2-spec", spec_2_py);
	 PyDict_SetItemString(item_dict_py, "overlap-volume", ov_py);
	 PyDict_SetItemString(item_dict_py, "radius-1", r_1_py);
	 PyDict_SetItemString(item_dict_py, "radius-2", r_2_py);
	 PyList_SetItem(o_py, ii, item_dict_py);
      }
      r = o_py;
   }
   if (PyBool_Check(r)) {
     Py_INCREF(r);
   }
   return r;
}
#endif // USE_PYTHON

// not const because it manipulated generic graphics objects
void
graphics_info_t::do_interactive_coot_probe() {

   if (moving_atoms_asc->n_selected_atoms > 0) {
      if (moving_atoms_asc->mol) {

	 graphics_info_t g;
	 bool ignore_waters = true;
	 coot::atom_overlaps_container_t ao(moving_atoms_asc->mol, Geom_p(), ignore_waters);

	 // dot density
	 coot::atom_overlaps_dots_container_t c = ao.all_atom_contact_dots(0.4);

	 std::map<std::string, std::vector<coot::atom_overlaps_dots_container_t::dot_t> >::const_iterator it;

	 // for quick colour lookups.
	 std::map<std::string, coot::colour_holder> colour_map;
	 colour_map["blue"      ] = coot::generic_display_object_t::colour_values_from_colour_name("blue");
	 colour_map["sky"       ] = coot::generic_display_object_t::colour_values_from_colour_name("sky");
	 colour_map["sea"       ] = coot::generic_display_object_t::colour_values_from_colour_name("sea");
	 colour_map["greentint" ] = coot::generic_display_object_t::colour_values_from_colour_name("greentint");
	 colour_map["green"     ] = coot::generic_display_object_t::colour_values_from_colour_name("green");
	 colour_map["orange"    ] = coot::generic_display_object_t::colour_values_from_colour_name("orange");
	 colour_map["orangered" ] = coot::generic_display_object_t::colour_values_from_colour_name("orangered");
	 colour_map["yellow"    ] = coot::generic_display_object_t::colour_values_from_colour_name("yellow");
	 colour_map["yellowtint"] = coot::generic_display_object_t::colour_values_from_colour_name("yellowtint");
	 colour_map["red"       ] = coot::generic_display_object_t::colour_values_from_colour_name("red");
	 colour_map["#55dd55"   ] = coot::generic_display_object_t::colour_values_from_colour_name("#55dd55");
	 colour_map["hotpink"   ] = coot::generic_display_object_t::colour_values_from_colour_name("hotpink");
	 colour_map["grey"      ] = coot::generic_display_object_t::colour_values_from_colour_name("grey");
	 colour_map["magenta"   ] = coot::generic_display_object_t::colour_values_from_colour_name("magenta");

	 for (it=c.dots.begin(); it!=c.dots.end(); it++) {
	    const std::string &type = it->first;
	    const std::vector<coot::atom_overlaps_dots_container_t::dot_t> &v = it->second;
	    std::string obj_name = std::string("Intermediate Atoms ") + type;
	    int obj_idx = g.generic_object_index(obj_name.c_str());
	    if (obj_idx == -1) {
	       obj_idx = new_generic_object_number(obj_name.c_str());
	       g.generic_objects_p->at(obj_idx).attach_to_intermediate_atoms();
	       if (type != "vdw-surface")
		  g.generic_objects_p->at(obj_idx).is_displayed_flag = true;
	    } else {
	       g.generic_objects_p->at(obj_idx).clear();
	    }
	    // std::string col = "#445566";
	    int point_size = 2;
	    if (type == "vdw-surface") point_size = 1;
	    for (unsigned int i=0; i<v.size(); i++) {
	       const std::string &col_inner = v[i].col;
	       g.generic_objects_p->at(obj_idx).add_point(colour_map[col_inner], col_inner, point_size, v[i].pos);
	    }
	 }

	 std::string clashes_object_name = "Intermediate atom clashes";
	 int clashes_obj_idx = g.generic_object_index(clashes_object_name); // find or set
	 if (clashes_obj_idx == -1) {
	    clashes_obj_idx = new_generic_object_number(clashes_object_name);
	    g.generic_objects_p->at(clashes_obj_idx).attach_to_intermediate_atoms();
	    g.generic_objects_p->at(clashes_obj_idx).is_displayed_flag = true;
	 } else {
	    // just clear the old one, but don't over-ride the current displayed status
	    g.generic_objects_p->at(clashes_obj_idx).clear();
	 }
	 std::string cn =  "#ff59b4";
	 coot::colour_holder ch(cn);
	 for (unsigned int i=0; i<c.clashes.size(); i++) {
	    g.generic_objects_p->at(clashes_obj_idx).add_line(ch, cn, 2, c.clashes[i]);
	 }

	 // do we need to draw here?
	 // graphics_draw();
      }
   }
}
