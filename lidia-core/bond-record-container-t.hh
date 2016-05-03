
#include "cod-atom-type-t.hh"
#include "bond-table-record-t.hh"
// for validation
#include "geometry/protein-geometry.hh"

namespace cod {

      // --------------- record container ---------------------

   class bond_record_container_t {
      std::string::size_type get_max_atom_type_width() const;
      std::map<std::string, unsigned int> atom_types_map; // convert from acedrg-table 
                                                          // cod string to its index
      bool write_atom_type_indices(const std::string &file_name) const;

      void sort() { std::sort(bonds.begin(), bonds.end()); }
      void fill_atom_map();

      std::vector<std::string>
      read_atom_type_indices(const std::string &atom_type_indices_file_name) const;
      
      bool read_bonds(const std::string &bonds_file_name,
		      const std::vector<std::string> &types);

      // return the consolidated table
      // 
      void read_acedrg_table_dir(const std::string &table_dir_name);

      // can throw std::runtime_error
      unsigned int get_atom_index(const std::string &at_name_1,
				  const coot::dictionary_residue_restraints_t &rest) const;
      unsigned int get_atom_index(const std::string &at_name_1,
				  const RDKit::RWMol &mol) const;

      std::vector<bool> get_is_hydrogen_flags(const RDKit::RWMol &rdkm) const;
      
      double get_bond_distance_from_model(const std::string &at_name_1,
					  const std::string &at_name_2,
					  mmdb::Residue *res) const;

      bond_table_record_t
      get_cod_bond_from_table(const atom_type_t &cod_type_1,
			      const atom_type_t &cod_type_2) const;
      
      // which uses
      bond_table_record_t
      make_bond_from_level_3_vector(const atom_type_t &cod_type_1,
				    const atom_type_t &cod_type_2,
				    const std::vector<bond_table_record_t> &v) const;
      

      bond_table_record_t
      consolidate_bonds(const atom_type_t &cod_type_1,
			const atom_type_t &cod_type_2,
			const std::vector<bond_table_record_t> &lb) const;
      
    public:
      bond_record_container_t() {}
      bond_record_container_t(const std::string &table_dir_name) {
	 read_acedrg_table_dir(table_dir_name);
      }
      bool read_acedrg_table(const std::string &file_name);

      // do I want bonds?
      std::vector<bond_table_record_t> bonds;

      // Level-4 atom type is the (outer) key and (outer) value
      // is a map for which the key is the second level-4 atom type,
      // returning a bond record.
      // std::map<std::string, std::map<std::string, bond_table_record_t> > bonds_map;

      // the key is a level-3 atom type, the value is
      // a map for which the key is a level-3 atom type and the value of that
      // map is a vector of bond_table_record_t.
      // 
      std::map<std::string, std::map<std::string, std::vector<bond_table_record_t> > >
      bonds_map;
      
      void add(const bond_table_record_t &rec) {
	 bonds.push_back(rec);
      }
      void add_table(const bond_record_container_t &brc) {
	 for (unsigned int i=0; i<brc.bonds.size(); i++) { 
	    bonds.push_back(brc.bonds[i]);
	 }
      }
      unsigned int size() { return bonds.size(); }
      bool write(const std::string file_name) const;

      bool read(const std::string &atom_type_indices_file_name,
		const std::string &bonds_file_name);
      void check() const;
      void validate(mmdb::Residue *res, const coot::dictionary_residue_restraints_t &rest) const;
   };
}
