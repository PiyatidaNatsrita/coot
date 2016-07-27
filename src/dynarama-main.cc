/* lbg/lidia-main.cc
 *
 * Author: Bernhard Lohkamp
 * Copyright 2015
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

#include <iostream>

#ifdef HAVE_GOOCANVAS

#ifdef __GNU_LIBRARY__
#include "compat/coot-getopt.h"
#else
#define __GNU_LIBRARY__
#include "compat/coot-getopt.h"
#undef __GNU_LIBRARY__
#endif

#include <sys/stat.h>

#include <gtk/gtk.h>
#include <goocanvas.h>


#include "utils/coot-utils.hh"
#include "rama_plot.hh"
#include "coords/mmdb-extras.h"
#include "coords/mmdb.h"
#include <mmdb2/mmdb_manager.h>

#include "coot-surface/rgbreps.h"


// Dummy definitions for stand alone version
extern "C" {
void set_dynarama_is_displayed(GtkWidget *dynarama_widget, int imol) {}
void set_go_to_atom_molecule(int imol) {}
int set_go_to_atom_chain_residue_atom_name(const char *t1_chain_id, int iresno, const char *t3_atom_name) {}
short int is_valid_model_molecule(int imol) {}
void set_moving_atoms(double phi, double psi) {}
void accept_phi_psi_moving_atoms() {}
void clear_moving_atoms_object() {}
}

void setup_rgb_reps();

// needed?
void setup_rgb_reps() {

   // c.f. coot::package_data_dir() which now does this wrapping for us:

   // For binary installers, they use the environment variable:
   char *env = getenv("COOT_DATA_DIR");

   // Fall-back:
   std::string standard_file_name = PKGDATADIR; // xxx/share/coot

   if (env)
      standard_file_name = env;

   std::string colours_file = standard_file_name + "/";
   std::string colours_def = "colours.def";
   colours_file += colours_def;

   struct stat buf;
   int status = stat(colours_file.c_str(), &buf);
   if (status == 0) { // colours file was found in default location
      RGBReps r(colours_file);
      if (1)
         std::cout << "INFO:: Colours file: " << colours_file << " loaded"
                   << std::endl;

      // test:
      //       std::vector<std::string> test_col;
      //       test_col.push_back("blue");
      //       test_col.push_back("orange");
      //       test_col.push_back("magenta");
      //       test_col.push_back("cyan");

      //       std::vector<int> iv = r.GetColourNumbers(test_col);
      //       for (int i=0; i<iv.size(); i++) {
      // 	 std::cout << "Colour number: " << i << "  " << iv[i] << std::endl;
      //       }


   } else {
      std::cout << "WARNING! Can't find file: colours.def at " << standard_file_name
                << std::endl;
   }
}

int
main(int argc, char *argv[]) {


   // add selections and kleywegt
   if (argc < 2) {
      std::cout << "Usage: " << argv[0] << "\n"
                << "(--pdbin) pdb-in-filename"
                << "[--selection atom-selection-string]\n"
                << "[--chain chain-id]\n"
                << "[--chain2 chain-id2] (for kleywegt plot)"
                << "[--selection2 atom-selection-string] (for kleywegt plot)"
                << "[--pdbin2 pdb-in-filename2 (for kleywegt plot, otherwise assume pdbin)]"
                << "[--kleywegt (to make kleywegt, autoamtically for multiple selections, chains)]"
                << "[--edit (edit mode, currently debug only)]"
                << "\n";
      std::cout << "     where pdbin is the protein and pdbin2 a second one for a Kleywegt plot.\n";

   } else {

      std::string pdb_file_name;
      std::string pdb_file_name2;
      std::string selection;
      std::string selection2;
      std::string chain_id;
      std::string chain_id2;
      selection = "";
      selection2 = "";
      int edit_res_no = -9999;
      int n_used_args = 0;
      int is_kleywegt_plot_flag = 0;

      const char *optstr = "i:s:j:t:c:d:e:k";
      struct option long_options[] = {
      {"pdbin", 1, 0, 0},
      {"selection", 1, 0, 0},
      {"pdbin2", 1, 0, 0},
      {"selection2", 1, 0, 0},
      {"chain", 1, 0, 0},
      {"chain2", 1, 0, 0},
      {"edit", 1, 0, 0},   // BL Note:: maybe there should be an edit selection
      {"kleywegt", 0, 0, 0},
      {0, 0, 0, 0}
   };

      int ch;
      int option_index = 0;
      while ( -1 !=
              (ch = getopt_long(argc, argv, optstr, long_options, &option_index))) {
         switch(ch) {

         case 0:
            if (optarg) {
               std::string arg_str = long_options[option_index].name;

               if (arg_str == "pdbin") {
                  pdb_file_name = optarg;
                  n_used_args += 2;
               }
               if (arg_str == "pdbin2") {
                  pdb_file_name2 = optarg;
                  is_kleywegt_plot_flag = 1;
                  n_used_args += 2;
               }
               if (arg_str == "selection") {
                  selection = optarg;
                  n_used_args += 2;
               }
               if (arg_str == "selection2") {
                  selection2 = optarg;
                  is_kleywegt_plot_flag = 1;
                  n_used_args += 2;
               }
               if (arg_str == "chain") {
                  chain_id = optarg;
                  n_used_args += 2;
               }
               if (arg_str == "chain2") {
                  chain_id2 = optarg;
                  is_kleywegt_plot_flag = 1;
                  n_used_args += 2;
               }
               if (arg_str == "edit") {
                  edit_res_no = coot::util::string_to_int(optarg);
                  n_used_args += 2;
               }
            } else {

               // options without arguments:

               // long argument without parameter:
               std::string arg_str(long_options[option_index].name);

               if (arg_str == "kleywegt") {
                  is_kleywegt_plot_flag = 1;
                  n_used_args++;
               }
            }
            break;

         case 'i':
            pdb_file_name = optarg;
            n_used_args += 2;
            break;

         case 'j':
            pdb_file_name2 = optarg;
            n_used_args += 2;
            break;

         case 's':
            selection = optarg;
            n_used_args += 2;
            break;

         case 't':
            selection2 = optarg;
            n_used_args += 2;
            break;

         case 'c':
            chain_id = optarg;
            n_used_args += 2;
            break;

         case 'd':
            chain_id2 = optarg;
            n_used_args += 2;
            break;

         case 'e':
            edit_res_no = coot::util::string_to_int(optarg);
            n_used_args += 2;
            break;

         case 'k':
            is_kleywegt_plot_flag = 1;
            n_used_args++;
            break;


         default:
            std::cout << "default optarg: " << optarg << std::endl;
            break;
         }
      }

      std::string file = argv[1];
      if (coot::util::extension_is_for_coords(coot::util::file_name_extension(file)))
         pdb_file_name = file;

      mmdb::Manager *mol = new mmdb::Manager();
      mmdb::Manager *mol2 = new mmdb::Manager();

      if (pdb_file_name.length() == 0) {
         std::cout << "WARNING:: Missing input PDB file\n";
         //exit(1);

      } else {
         mol->ReadPDBASCII(pdb_file_name.c_str());
      }

      gtk_init (&argc, &argv);
      setup_rgb_reps();

      float level_prefered = 0.02;
      float level_allowed = 0.002;
      float block_size = 1;
      int imol = 0; // dummy for now
      int imol2 = 0;

      // edit plot?
      if (edit_res_no > -9999) {
         // make an edit plot
         coot::rama_plot *edit_phi_psi_plot = new coot::rama_plot;
         edit_phi_psi_plot->init("phi/psi-edit");
         edit_phi_psi_plot->set_stand_alone();

         if (chain_id.size() == 0)
            chain_id = "A";
         // make a mmdb res and the use
         mmdb::PResidue *SelResidue;
         int nRes;
         int SelHnd;
         std::vector <coot::util::phi_psi_t> vp;
         //g_print("BL DEBUG:: edit plot with chain %s and resno %i\n", chain_id.c_str(), edit_res_no);
         for (int resno=edit_res_no; resno<edit_res_no+2; resno++) {
            SelHnd= mol->NewSelection();
            mol->Select(SelHnd,
                        mmdb::STYPE_RESIDUE,  // select residues
                        0,              // any model
                        chain_id.c_str(),          // chains "A" and "B" only
                        resno-1,"*",resno+1,"*", // any residue in range 30 to 100,
                        // any insertion code
                        "*",            // any residue name
                        "*",            // any atom name
                        "*",           // any chemical element but sulphur
                        "*",            // any alternative location indicator
                        mmdb::SKEY_NEW);         // OR-selection
            mol->GetSelIndex(SelHnd, SelResidue, nRes);
            if (nRes == 3) {
               std::pair<bool, coot::util::phi_psi_t> phi_psi_all = coot::util::get_phi_psi(SelResidue);
               if (phi_psi_all.first)
                  vp.push_back(phi_psi_all.second);
            } else {
               std::cout<<"BL WARNING:: not 3 residues in selection"<<std::endl;
               break;
            }
            mol->DeleteSelection(SelHnd);
         }
         if (vp.size() == 2)
            edit_phi_psi_plot->draw_it(vp);
         else
            g_print("BL WARNING:: problem making phi psi for 2 residues %i and %i\n",
                    edit_res_no, edit_res_no+1);

      } else {
         coot::rama_plot *rama = new coot::rama_plot;
         // normal rama (or kleywy)
         if (is_kleywegt_plot_flag) {
            mol2 = mol;
            imol2 = imol + 1;
            if (chain_id.size() == 0 && chain_id2.size() == 0 &&
                selection.size() == 0 && selection2.size()== 0) {
               chain_id = "A";
               chain_id2 = "B";
            }
         }

         if (pdb_file_name2.length() > 0) {
            // can be same mol as well, usually...., so maybe another flag
            is_kleywegt_plot_flag = 1;
            mol2->ReadPDBASCII(pdb_file_name2.c_str());
            imol2 = imol + 1;
            // Make a double name
            pdb_file_name = pdb_file_name + " vs. \n" + pdb_file_name2;
            // what about selections? dont care if given ok,
            // if not then compare Rama of 2 structures
         }

         // Now select mols and pass the handle
         // Alternatively: pass the selection string
         // probably better to pass the Handle
         // OR even make a new mol and only work with these

         int selHnd = -1;
         int selHnd2 = -1;
         int nRes;
         mmdb::PResidue *SelResidue;
         mmdb::PChain *SelChain;

         if (selection.size() > 0) {
            selHnd = mol->NewSelection();
            //selection="//A";
            mol->Select(selHnd,
                        mmdb::STYPE_RESIDUE,
                        selection.c_str(),
                        mmdb::SKEY_NEW);
            mol->GetSelIndex(selHnd, SelResidue, nRes);
         } else {
            if (chain_id.size() > 0) {
               selHnd = mol->NewSelection();
               //selection="//A";
               mol->Select(selHnd,
                           mmdb::STYPE_RESIDUE,
                           0,              // any model
                           chain_id.c_str(),          // chains
                           mmdb::ANY_RES, "*",
                           mmdb::ANY_RES, "*", // all residues
                           // any insertion code
                           "*",            // any residue name
                           "*",            // any atom name
                           "*",           // any chemical element but sulphur
                           "*",            // any alternative location indicator
                           mmdb::SKEY_NEW);         // OR-selection
               mol->GetSelIndex(selHnd, SelResidue, nRes);
            }
         }
         if (selection2.size() > 0) {
            if (mol2) {
               selHnd2 = mol2->NewSelection();
               //selection="//A";
               mol2->Select(selHnd2,
                            mmdb::STYPE_RESIDUE,
                            selection2.c_str(),
                            mmdb::SKEY_NEW);
            } else {
               g_print("BL INFO:: no mol2, no 2nd selection.");
            }
         }

         rama->set_stand_alone();
         rama->init(imol,
                    pdb_file_name,
                    level_prefered,
                    level_allowed,
                    block_size,
                    is_kleywegt_plot_flag);
         if (is_kleywegt_plot_flag) {
            if (selHnd > -1 && selHnd2 > -1) {
               rama->draw_it(imol, imol2,
                             mol, mol2,
                             selHnd, selHnd2);
            } else {
               if (chain_id.size() != 0 && chain_id2.size() != 0)  {
                  rama->draw_it(imol, imol2,
                                mol, mol2,
                                chain_id, chain_id2);
               } else {
                  if (mol != mol2) {
                     rama->draw_it(imol, imol2,
                                   mol, mol2);
                  } else {
                     g_print("BL INFO:: have different imols but same selection. Should not happen. No idea what to do");
                  }
               }
            }
         } else {
            if (selHnd > -1)
               rama->draw_it(mol, selHnd, 1);
            else {
               if (mol) {
                  rama->draw_it(mol);
               } else {
                  g_print("BL INFO:: no mol and no selection, so no plot. Sorry.");
               }
            }
         }
      }

      gtk_main ();
      delete mol;
      if (mol2)
         delete mol2;
   }
   gtk_init(&argc, &argv);


}

#else

int
main(int argc, char *argv[]) {

   std::cout << "No goo canvas at compile-time, no dynarama " << std::endl;
   return 0;
}



#endif

