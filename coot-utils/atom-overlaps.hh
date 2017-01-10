/* coot-utils/atom-overlaps.hh
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


#ifndef ATOM_OVERLAPS_HH
#define ATOM_OVERLAPS_HH

// #include <mmdb2/mmdb_manager.h>
// #include <vector>

#include "compat/coot-sysdep.h"
#include "geometry/protein-geometry.hh"

namespace coot {

   class atom_overlaps_dots_container_t {
   public:
      class spikes_t {
      public:
	 std::string type;
	 std::vector<std::pair<clipper::Coord_orth, clipper::Coord_orth> > positions;
	 const std::pair<clipper::Coord_orth, clipper::Coord_orth> &operator[](unsigned int idx) {
	    return positions[idx];
	 }
	 unsigned int size() const { return positions.size(); }
      };
      class dot_t {
      public:
	 double overlap;
	 clipper::Coord_orth pos;
	 std::string col;
	 dot_t(double o, const std::string &col_in, const clipper::Coord_orth &pos_in) {
	    overlap = o;
	    pos = pos_in;
	    col = col_in;
	 }
      };
      atom_overlaps_dots_container_t() {}
      std::map<std::string, std::vector<dot_t> > dots;
      spikes_t clashes;
      double score() const {
	 // std::map<std::string, std::vector<clipper::Coord_orth> >::const_iterator it;
	 std::map<std::string, std::vector<dot_t> >::const_iterator it;
	 // do these match the types in overlap_delta_to_contact_type()?
	 double r = 0;
	 it = dots.find("H-bond");
	 if (it != dots.end()) r += it->second.size();
	 it = dots.find("wide-contact");
	 if (it != dots.end()) r += 0.1 * it->second.size();
	 it = dots.find("close-contact");
	 if (it != dots.end()) r -= 0.0 * it->second.size();
	 it = dots.find("small-overlap");
	 if (it != dots.end()) r -= 0.1 * it->second.size();
	 it = dots.find("big-overlap");
	 if (it != dots.end()) r -= it->second.size();
	 r -= clashes.size();
	 return r;
      }
   };

   class atom_overlap_t {
   public:
      atom_overlap_t(mmdb::Atom *a1, mmdb::Atom *a2) {
	 atom_1 = a1;
	 atom_2 = a2;
	 overlap_volume = -1;
	 r_1 = -1;
	 r_2 = -1;
      }
      atom_overlap_t(int ligand_atom_index_in,
		     mmdb::Atom *a1, mmdb::Atom *a2, const double &r_1_in, const double &r_2_in,
		     const double &o) {
	 ligand_atom_index = ligand_atom_index_in;
	 atom_1 = a1;
	 atom_2 = a2;
	 r_1 = r_1_in;
	 r_2 = r_2_in;
	 overlap_volume = o;
      }
      int ligand_atom_index;
      double r_1, r_2;
      mmdb::Atom *atom_1; 
      mmdb::Atom *atom_2; 
      double overlap_volume;
      bool is_h_bond;
   };

   class atom_overlaps_container_t {

      void init();
      void init_for_all_atom();
      enum overlap_mode_t { CENTRAL_RESIDUE, ALL_ATOM };
      overlap_mode_t overlap_mode;
      mmdb::Manager *mol;
      bool have_dictionary; // for central residue (or should it be all residues?)
      mmdb::Residue *res_central;
      std::vector<mmdb::Residue *> neighbours;
      int udd_h_bond_type_handle;
      int udd_residue_index_handle;
      double probe_radius;
      
      // for energy types -> vdw radius and h-bond type
      std::map<std::string, double> type_to_vdw_radius_map;
      std::map<mmdb::Atom *, double> central_residue_atoms_vdw_radius_map; // ligand atoms
      std::map<mmdb::Atom *, double> neighbour_atoms_vdw_radius_map; // neighbouring atoms
      dictionary_residue_restraints_t central_residue_dictionary;
      // for ligand and environment neighbours
      std::vector<dictionary_residue_restraints_t> neighb_dictionaries;
      // for all atom
      std::map<std::string, dictionary_residue_restraints_t> dictionary_map;

      double get_vdw_radius_ligand_atom(mmdb::Atom *at);
      double get_vdw_radius_neighb_atom(mmdb::Atom *at, unsigned int idx_neighb_res);
      double get_vdw_radius_neighb_atom(int idx_neighb_atom) const;
      double get_overlap_volume(const double &dist, const double &r_1, const double &r_2) const; // in A^3
      const protein_geometry *geom_p;

      bool clashable_alt_confs(mmdb::Atom *at_1, mmdb::Atom *at_2) const;
      std::vector<double> env_residue_radii;
      void setup_env_residue_atoms_radii(int i_sel_hnd_env_atoms); // fill above
      // which calls:
      double type_energy_to_radius(const std::string &te) const;
      
      void add_residue_neighbour_index_to_neighbour_atoms();
      std::vector<double> neighb_atom_radius;

      // first is yes/no, second is if the H is on the ligand.
      // also allow water to be return true values.
      std::pair<bool, bool> is_h_bond_H_and_acceptor(mmdb::Atom *ligand_atom,
						     mmdb::Atom *env_atom) const;

      hb_t get_h_bond_type(mmdb::Atom *at);
      // store the results of a contact search.
      //
      // when we draw the surface of the ligand, for each atom, we don't want to draw
      // surface points that are nearer to another atom than the "central" atom.  So keep
      // a quick store of what's close to what and the radius.
      //
      std::map<int, std::vector<std::pair<mmdb::Atom *, double> > > ligand_atom_neighbour_map;
      std::map<int, std::vector<int> > ligand_to_env_atom_neighbour_map;
      // adds radii too.
      void fill_ligand_atom_neighbour_map();
      // include inner cusps (ugly/simple)
      bool is_inside_another_ligand_atom(int idx,
					 const clipper::Coord_orth &pt_idx_at) const;
      // exclude inner cusps (modern/pretty)
      bool is_inside_another_ligand_atom(int idx,
					 const clipper::Coord_orth &probe_pos,
					 const clipper::Coord_orth &pt_idx_at) const;
      // for all-atom contacts
      bool is_inside_another_atom_to_which_its_bonded(int atom_idx,
						      mmdb::Atom *at,
						      const clipper::Coord_orth &pt_on_surface,
						      const std::vector<int> &bonded_neighb_indices,
						      mmdb::Atom **atom_selection) const;
      bool is_inside_an_env_atom_to_which_its_bonded(int idx,
						     const std::vector<int> &bonded_neighb_indices,
						     mmdb::Atom **env_residue_atoms,
						     const clipper::Coord_orth &pt_at_surface);
      double clash_spike_length;
      void mark_donors_and_acceptors();
      void mark_donors_and_acceptors_central_residue(int udd_h_bond_type_handle);
      void mark_donors_and_acceptors_for_neighbours(int udd_h_bond_type_handle);
      // return a contact-type and a colour
      std::pair<std::string, std::string> overlap_delta_to_contact_type(double delta, bool is_h_bond) const;
      const dictionary_residue_restraints_t &get_dictionary(mmdb::Residue *r, unsigned int idx) const;
      // where BONDED here means bonded/1-3-angle/ring related
      enum atom_interaction_type { CLASHABLE, BONDED, IGNORED };
      atom_interaction_type
      bonded_angle_or_ring_related(mmdb::Manager *mol,
				   mmdb::Atom *at_1,
				   mmdb::Atom *at_2,
				   bool exclude_mainchain_also,
				   std::map<std::string, std::vector<std::pair<std::string, std::string> > > *bonded_neighbours,
				   std::map<std::string, std::vector<std::vector<std::string> > > *ring_list_map);
      bool are_bonded_residues(mmdb::Residue *res_1, mmdb::Residue *res_2) const;
      bool in_same_ring(mmdb::Atom *at_1, mmdb::Atom *at_2,
			std::map<std::string, std::vector<std::vector<std::string> > > &ring_list_map) const;
      bool is_linked(mmdb::Atom *at_1, mmdb::Atom *at_2) const;
//       bool in_same_ring(const std::string &atom_name_1,
// 			const std::string &atom_name_2,
// 			const std::vector<std::vector<std::string> > &ring_list) const;
      std::vector<std::vector<std::string> > phe_ring_list() const;
      std::vector<std::vector<std::string> > his_ring_list() const;
      std::vector<std::vector<std::string> > trp_ring_list() const;
      std::vector<std::vector<std::string> > pro_ring_list() const;

      atom_overlaps_dots_container_t all_atom_contact_dots_internal(double dot_density_in,
								    mmdb::Manager *mol,
								    int i_sel_hnd_1,
								    int i_sel_hnd_2,
								    mmdb::realtype min_dist,
								    mmdb::realtype max_dist,
								    bool make_vdw_surface);

   public:
      // we need mol to use UDDs to mark the HB donors and acceptors (using coot-h-bonds.hh)
      atom_overlaps_container_t(mmdb::Residue *res_central_in,
				const std::vector<mmdb::Residue *> &neighbours_in,
				mmdb::Manager *mol,
				const protein_geometry *geom_p_in);
      atom_overlaps_container_t(mmdb::Residue *res_central_in,
				mmdb::Residue *neighbour,
				mmdb::Manager *mol,
				const protein_geometry *geom_p_in);
      // this one for contact dots (around central ligand)
      atom_overlaps_container_t(mmdb::Residue *res_central_in,
				const std::vector<mmdb::Residue *> &neighbours_in,
				mmdb::Manager *mol,
				const protein_geometry *geom_p_in,
				double clash_spike_length_in,
				double probe_radius_in = 0.25);
      atom_overlaps_container_t(mmdb::Manager *mol_in,
				const protein_geometry *geom_p_in,
				double clash_spike_length_in = 0.5,
				double probe_radius_in = 0.25);

      std::vector<atom_overlap_t> overlaps;
      void make_overlaps();
      void contact_dots_for_overlaps() const; // old
      atom_overlaps_dots_container_t contact_dots_for_ligand();
      atom_overlaps_dots_container_t all_atom_contact_dots(double dot_density = 0.5,
							   bool make_vdw_surface = false);

   };

}



#endif // ATOM_OVERLAPS_HH
