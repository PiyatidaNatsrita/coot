
#ifdef MAKE_ENHANCED_LIGAND_TOOLS

#include <boost/python.hpp>

#include "coot-utils/coot-coord-utils.hh"
#include "lidia-core/rdkit-interface.hh"
#include "graphics-info.h"

namespace coot {
   int import_rdkit_molecule(const RDKit::ROMol &m, int conf_id, const std::string &new_comp_id);
   boost::python::list extract_ligands_from_coords_file(const std::string &file_name);
   boost::python::object get_ligand_interactions(const std::string &file_name,
						 PyObject *ligand_spec);
}


//                      Functions that are importable only from coot

BOOST_PYTHON_MODULE(coot_embedded) {

   boost::python::def("import_rdkit_molecule", coot::import_rdkit_molecule);

}

// if the m has an Property "ResName" then use that for the residue,
// else
int coot::import_rdkit_molecule(const RDKit::ROMol &m, int conf_id, const std::string &new_comp_id) {

   graphics_info_t g;
   int imol = -1;
   int n_conf  = m.getNumConformers();
   if (n_conf == 0) {
      std::cout << "WARNING:: no conformers in input rdkit molecule " << std::endl;
   } else {
      mmdb::Residue *residue_p = coot::make_residue(m, conf_id, new_comp_id);
      if (residue_p) {
	 mmdb::Manager *mol = coot::util::create_mmdbmanager_from_residue(residue_p);
	 atom_selection_container_t asc = make_asc(mol);
	 imol = graphics_info_t::create_molecule();
	 std::string label = "Imported-mol " + new_comp_id;
	 g.molecules[imol].install_model(imol, asc, g.Geom_p(), label, 1);
      } else {
	 std::cout << "WARNING:: null residue" << std::endl;
      }
   }
   return imol;
}


#endif // MAKE_ENHANCED_LIGAND_TOOLS
