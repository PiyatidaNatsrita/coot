

// some-header

#include <iostream>
#include <iomanip>

#include "bond-table-record-t.hh"

void
cod::bond_table_record_t::write(std::ostream &s,
				std::string::size_type max_atom_type_width) const { 
   // v1 
   if (false) { 
      s << std::setw(10) << mean;
      s << std::setw(10) << std_dev;
      s << std::setw(10) << count;
      s << std::setw(max_atom_type_width+2) << cod_type_1;
      s << std::setw(max_atom_type_width+2) << cod_type_2;
      s << "\n";
   }

   // v2
      s << std::setw(10) << mean;
      s << std::setw(10) << std_dev;
      s << std::setw(6) << count;

      std::string::size_type s1 = cod_type_1.length();
      std::string::size_type s2 = cod_type_2.length();

      s << std::setw(4) << s1;
      s << std::setw(4) << s2;

      s << " ";
      s << cod_type_1;
      s << " ";
      s << cod_type_2;
      s << "\n";
}

void
cod::bond_table_record_t::write(std::ostream &s,
				unsigned int type_index_1,
				unsigned int type_index_2) const {

      s << std::setw(10) << mean;
      s << std::setw(10) << std_dev;
      s << std::setw(6) << count;

      s << " ";
      
      s << std::setw(7) << type_index_1 << " ";
      s << std::setw(7) << type_index_2 <<"\n";
}


std::ostream &
cod::operator<<(std::ostream &s, const cod::bond_table_record_t &btr) {

   s << btr.cod_type_1 << " " << btr.cod_type_2 << " " << btr.mean
   << " " << btr.std_dev << " " << btr.count;
   return s;
}

