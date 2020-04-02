
#include "Python.h"
#include "graphics-info.h"

bool graphics_info_t::residue_type_selection_was_user_picked_residue_range = false;

bool graphics_info_t::make_auto_h_bond_restraints_flag = false;

bool graphics_info_t::do_rotamer_restraints = false;

bool graphics_info_t::do_debug_refinement = false;

std::atomic<bool> graphics_info_t::on_going_updating_map_lock(false);

std::string graphics_info_t::mtz_file_for_refmac;

bool graphics_info_t::convert_dictionary_planes_to_improper_dihedrals_flag = false;


