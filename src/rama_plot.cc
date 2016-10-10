/* src/main.cc
 * 
 * Copyright 2002, 2003, 2004, 2005, 2006, 2007 by The University of York
 * Copyright 2005, 2015, 2016 by Bernhard Lohkamp
 * Copyright 2009 by The University of Oxford
 * Copyright 2013, 2015, 2016 by Medical Research Council
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
#include "Python.h"  // before system includes to stop "POSIX_C_SOURCE" redefined problems
#endif


#ifdef _MSC_VER
#define snprintf _snprintf
#endif

#if defined(HAVE_GTK_CANVAS) || defined(HAVE_GNOME_CANVAS)
 
#ifndef RAMA_PLOT
#define RAMA_PLOT

#include <iostream>
#include <algorithm>

#include <gdk/gdkkeysyms.h> // for keyboarding.

#include "utils/coot-utils.hh" // int to string

#include "rama_plot.hh" // has gtk/gtk.h which interface.h needs

#ifdef HAVE_GOOCANVAS
#include <goocanvas.h>
#endif
#include <cairo.h>
#if CAIRO_HAS_PDF_SURFACE
#include <cairo-pdf.h>
#endif
#include <cairo-svg.h>

#include "interface.h"
#ifndef HAVE_SUPPORT_H
#define HAVE_SUPPORT_H
#include "support.h"
#endif /* HAVE_SUPPORT_H */

#include "rama_mousey.hh"
#include "clipper/core/coords.h"

#include "c-interface.h"
#include "c-interface-gtk-widgets.h"


void
coot::rama_plot::init(int imol_in, const std::string &mol_name_in, float level_prefered, float level_allowed, float block_size, short int is_kleywegt_plot_flag_in) {

   imol = imol_in; // is this used? (yes, sort of, but not handling
		   // the click on a residue)
   molecule_numbers_.first = imol;
   phipsi_edit_flag = 0;
   backbone_edit_flag = 0;
   init_internal(mol_name_in, level_prefered, level_allowed, block_size, 0,
		 is_kleywegt_plot_flag_in);
   // is_kleywegt_plot_flag = is_kleywegt_plot_flag_in;

   gtk_widget_set_sensitive(rama_view_menu, TRUE);
}

// We could pass to this init the level_prefered and level_allowed
// here, if we wanted the phi/psi edit and backbone edit to have
// "non-standard" contour levels.
void
coot::rama_plot::init(const std::string &type) { 
 
   if (type == "phi/psi-edit") { 
      phipsi_edit_flag = 1;
      backbone_edit_flag = 0;
      imol = -9999; // magic number used in OK button callback.
      init_internal("Ramachandran Plot (Phi/Psi Edit Mode)", 0.02, 0.002, 1);
      hide_stats_frame();
   }
   if (type == "backbone-edit") { 
      phipsi_edit_flag = 0;
      backbone_edit_flag = 1;
      imol = -9999; // magic number used in OK button callback.
      short int hide_buttons = 1;
      init_internal("Ramachandran Plot (Backbone Edit Mode)", 0.02, 0.002, 1, hide_buttons);
      hide_stats_frame();
   }
   gtk_widget_set_sensitive(rama_view_menu, FALSE);
   green_box_item = NULL;
}

void
coot::rama_plot::resize_rama_canvas_internal(GtkWidget *widget, 
                                             GdkEventConfigure *event) {

   if (resize_canvas_with_window) {

      // try after ideas from gimp
      // scale proportionally to size difference
      // 50% in either direction equals 75% overall
      // 50% in both directions equals 50% overall
      // same for enlargement (I think)
      // if canvas smaller than scrolled win, max to scrolled window (when switched on)
      // use diagonals
      static int oldw = 400;
      static int oldh = 400;

      GtkAllocation    allocation;
      gint new_width;
      gint new_height;
      double zoom_factor;

      gtk_widget_get_allocation (widget, &allocation);


      new_width = event->width;
      new_height = event->height;

      zoom_factor = (sqrt(new_height*new_height*1. + new_width*new_width*1.)
                     / sqrt(oldh*oldh*1. + oldw*oldw*1.));

      zoom *= zoom_factor;
      if (zoom < 0.8) {
         zoom = 0.8;
         g_print("BL INFO:: already smallest size to fit the window, wont make canvas smaller.\n");
      }
      goo_canvas_set_scale(GOO_CANVAS(canvas), zoom);

      // save the size for the next round
      oldw = event->width;
      oldh = event->height;

   } else {
      // maybe need to save new window size.
      // I dont think so
   }

}

void
coot::rama_plot::resize_mode_changed(int state) {

   resize_canvas_with_window = state;

   // set the button (if not already done)
   if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(zoom_resize_togglebutton)) != state) {
      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(zoom_resize_togglebutton), state);
   }
   // same for menuitem
   if (gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(zoom_resize_menuitem)) != state) {
      gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(zoom_resize_menuitem), state);
   }
}


bool
coot::rama_plot::create_dynarama_window() {

   if (dynawin) {
      // we already have a window, probably hidden, so no need to make a new one
      return 1;
   }
   else {
   int status = 0;
   GtkWidget *widget = NULL;
   GtkBuilder *builder = NULL;
   std::string glade_file = "dynarama.glade";

   std::string glade_file_full = coot::package_data_dir();
   glade_file_full += "/";
   glade_file_full += glade_file;

   bool glade_file_exists = 0;
   struct stat buf;
   int err = stat(glade_file_full.c_str(), &buf);
   if (! err)
      glade_file_exists = 1;

   if (! glade_file_exists) {
      std::cout << "ERROR:: glade file " << glade_file_full << " not found" << std::endl;
   } else {

      // If we are using the graphics interface then we want non-null from the builder.
      // add_from_file_status should be good or we are in trouble.
      //
      // If not, we need not call gtk_builder_add_from_file().
      //

      int use_graphics_interface_flag = 1;
      if (use_graphics_interface_flag) {

         builder = gtk_builder_new ();
         guint add_from_file_status =
            gtk_builder_add_from_file (builder, glade_file_full.c_str(), NULL);
         if (! add_from_file_status) {

            // Handle error...

            std::cout << "ERROR:: gtk_builder_add_from_file() \"" << glade_file_full
                      << "\" failed." << std::endl;
            if (builder) {
               std::cout << "ERROR:: where builder was non-null" << std::endl;
            } else {
               std::cout << "ERROR:: where builder was NULL" << std::endl;
            }

         } else {

            // Happy Path

               coot::rama_plot *plot;
               dynawin = GTK_WIDGET(gtk_builder_get_object(builder, "dynarama2_window"));
               dynarama_ok_button = GTK_WIDGET(gtk_builder_get_object(builder, "dynarama2_ok_button"));
               dynarama_cancel_button = GTK_WIDGET(gtk_builder_get_object(builder, "dynarama2_cancel_button"));
               dynarama_label = GTK_WIDGET(gtk_builder_get_object(builder,"dynarama_label"));
               scrolled_window = GTK_WIDGET(gtk_builder_get_object(builder,"dynarama_scrolledwindow"));
               outliers_only_tooglebutton = GTK_WIDGET(gtk_builder_get_object(builder,
                                                                             "dynarama2_outliers_only_togglebutton"));
               zoom_resize_togglebutton = GTK_WIDGET(gtk_builder_get_object(builder,
                                                     "dynarama2_zoom_resize_togglebutton"));
               rama_stats_frame =  GTK_WIDGET(gtk_builder_get_object(builder, "rama_stats_frame"));
               rama_stats_label1 = GTK_WIDGET(gtk_builder_get_object(builder, "rama_stats_label_1"));
               rama_stats_label2 = GTK_WIDGET(gtk_builder_get_object(builder, "rama_stats_label_2"));
               rama_stats_label3 = GTK_WIDGET(gtk_builder_get_object(builder, "rama_stats_label_3"));
               kleywegt_chain_box = GTK_WIDGET(gtk_builder_get_object(builder, "kleywegt_select_chain_hbox"));
               rama_open_menuitem = GTK_WIDGET(gtk_builder_get_object(builder, "rama_open_menuitem"));
               about_dialog = GTK_WIDGET(gtk_builder_get_object(builder, "rama_aboutdialog1"));
               rama_export_as_pdf_filechooserdialog = GTK_WIDGET(gtk_builder_get_object(builder,
                                                                                        "rama_export_as_pdf_filechooserdialog"));
               rama_export_as_png_filechooserdialog = GTK_WIDGET(gtk_builder_get_object(builder,
                                                                                        "rama_export_as_png_filechooserdialog"));
               rama_open_filechooserdialog = GTK_WIDGET(gtk_builder_get_object(builder,
                                                                               "rama_open_filechooserdialog"));
               rama_view_menu = GTK_WIDGET(gtk_builder_get_object(builder,
                                                                  "menuitem_view"));
               rama_radiomenuitem = GTK_WIDGET(gtk_builder_get_object(builder,
                                                                      "rama_radiomenuitem"));
               kleywegt_radiomenuitem = GTK_WIDGET(gtk_builder_get_object(builder,
                                                                          "kleywegt_radiomenuitem"));
               outliers_only_menuitem = GTK_WIDGET(gtk_builder_get_object(builder,
                                                  "outliers_only_menuitem"));
               zoom_resize_menuitem = GTK_WIDGET(gtk_builder_get_object(builder,
                                                                        "zoom_resize_menuitem"));
               kleywegt_chain_combobox1 = GTK_WIDGET(gtk_builder_get_object(builder,
                                                                            "kleywegt_chain_combobox1"));
               kleywegt_chain_combobox2 = GTK_WIDGET(gtk_builder_get_object(builder,
                                                                            "kleywegt_chain_combobox2"));

               gtk_builder_connect_signals (builder, dynawin);
               g_object_unref (G_OBJECT (builder));
               status = add_from_file_status;
         }
      }
   }

   return status;

   }

}


// DEBUG:: function for debugging
void my_getsize(GtkWidget *widget, GtkAllocation *allocation, gpointer *data) {
    printf("BL DEBUG:: size alloc %s width = %d, height = %d\n",
           data, allocation->width, allocation->height);
}

//  The mapview entry point
//
// hide_buttons is optional arg
void
coot::rama_plot::init_internal(const std::string &mol_name,
			       float level_prefered, float level_allowed,
			       float step_in, 
			       short int hide_buttons,
			       short int is_kleywegt_plot_flag_local) {

   fixed_font_str = "fixed";
#if defined(WINDOWS_MINGW) || defined(_MSC_VER)
   fixed_font_str = "monospace";
#endif

   dragging = FALSE;
   drag_x = 0;
   drag_y = 0;

   bool init_status;
   init_status = create_dynarama_window();

   if (init_status) {
      if (hide_buttons == 1) {
         gtk_widget_hide(dynarama_ok_button);
         gtk_widget_hide(dynarama_cancel_button);
      }

      if (! is_kleywegt_plot_flag_local){
         gtk_widget_hide(kleywegt_chain_box);
         gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(rama_radiomenuitem),
                                        TRUE);
      } else {
         gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(kleywegt_radiomenuitem),
                                        TRUE);
      }


      if (is_stand_alone()) {
         gtk_widget_show(rama_open_menuitem);
      } else {
         gtk_widget_hide(rama_open_menuitem);
      }
      //      // set the title of of widget
      if (dynarama_label)
         gtk_label_set_text(GTK_LABEL(dynarama_label), mol_name.c_str());

      int ysize = 500;
      if (! is_kleywegt_plot_flag_local) // extra space needed
         ysize = 535;

      GtkAllocation alloc = { 0, 0, 400, ysize };
      gtk_widget_size_allocate(dynawin, &alloc);
      if (dynawin) {
         gtk_widget_show(dynawin);
      } else {
         std::cout<<"ELLOR:: no window, should bail out"<<std::endl;
      }

      gchar *txt;
      g_signal_connect(dynawin, "configure-event",
                       G_CALLBACK(rama_resize), this);

      allow_seqnum_offset_flag = 0;

      canvas = goo_canvas_new();
      root = goo_canvas_get_root_item (GOO_CANVAS(canvas));

      gtk_widget_set_size_request(canvas, 400, 400);
      gtk_container_add(GTK_CONTAINER(scrolled_window),
                        canvas);
      gtk_widget_ref(canvas);
      gtk_object_set_user_data(GTK_OBJECT(canvas), (gpointer) this);
      g_object_set_data(G_OBJECT(canvas), "user_data", (gpointer) this);
      gtk_object_set_user_data(GTK_OBJECT(dynawin), (gpointer) this);
      g_object_set(G_OBJECT(canvas),
                   "has-tooltip", TRUE,
                   NULL);

      gtk_widget_add_events(GTK_WIDGET(canvas),
                            GDK_EXPOSURE_MASK      |
                            GDK_BUTTON_PRESS_MASK  |
                            GDK_BUTTON_RELEASE_MASK|
                            GDK_POINTER_MOTION_MASK|
                            GDK_KEY_RELEASE_MASK   |
                            GDK_POINTER_MOTION_HINT_MASK);


      if (dialog_position_x > -1)
         gtk_widget_set_uposition(dynawin, dialog_position_x, dialog_position_y);

      gtk_widget_show (canvas);

      // Normally we have a plot from a molecule (and we communicate back
      // to graphics_info_t that we now have one), but occassionally we
      // want to edit the phipsi angle.
      //
      if (! phipsi_edit_flag && ! backbone_edit_flag)
         // a c-interface function
         set_dynarama_is_displayed(GTK_WIDGET(canvas), imol);

      setup_internal(level_prefered, level_allowed);
      step = step_in;
      kleywegt_plot_uses_chain_ids = 0;

      draw_outliers_only = false;

      green_box = coot::util::phi_psi_t(-999, -999, "", "", 0, "", ""); // set unsensible phi phi initially.
      setup_canvas();

      // Draw the basics here, why not?
      basic_white_underlay();
      if (level_allowed != current_level_allowed &&
          level_prefered != current_level_prefered) {
         current_level_allowed = level_allowed;
         current_level_prefered = level_prefered;

      }

      if (!current_bg) {
         setup_background();
      }
      draw_axes();
      draw_zero_lines();
      black_border();
      // green box? directly onto the canvas, out of sight.
      green_box_item = goo_canvas_rect_new(root,
                                           -999., -999.,
                                           8., 8.,
                                           "fill-color", "green",
                                           "stroke-color", "black",
                                           "tooltip", "dummy",
                                           NULL);

      // Hope everything is there before we resize?!
      //      g_signal_connect_after(dynawin, "size-allocate",

      g_signal_connect_after(dynawin, "configure-event",
                             G_CALLBACK(rama_resize), this);

   } else {
      // FIXME throw runtime error here as not to continue?
      std::cout<<"BL WARNING:: no dynawin, bailing out." <<std::endl;
   }
}


void
coot::rama_plot::setup_internal(float level_prefered, float level_allowed) {

   zoom = 0.8;
   have_sticky_labels = 0; 

   n_diffs = 50; // default value.
   drawing_differences = 0; 


   rama.init(clipper::Ramachandran::All5);
   displayed_rama_type = clipper::Ramachandran::All5;

   // cliper defaults: 
   rama_threshold_preferred = 0.01; 
   rama_threshold_allowed = 0.0005; 

   // values that make the plot look similar to a procheck plot:
   rama_threshold_preferred = 0.06; // 0.05 
   rama_threshold_allowed = 0.0012; // 0.002

   // Procheck is old and too liberal
   rama_threshold_preferred = 0.1;
   rama_threshold_allowed = 0.005;

   // cliper defaults: 
   rama_threshold_preferred = 0.01; 
   rama_threshold_allowed = 0.0005;

   // nipped clipper values:
   rama_threshold_preferred = 0.02; 
   rama_threshold_allowed = 0.0012;
   
   // Lovell et al. 2003, 50, 437 Protein Structure, Function and Genetics values:
   rama_threshold_preferred = 0.02; 
   rama_threshold_allowed = 0.002;
   
   //clipper defaults: 0.01 0.0005

   rama.set_thresholds(level_prefered, level_allowed);
   //
   r_gly.init(clipper::Ramachandran::Gly5);
   r_gly.set_thresholds(level_prefered, level_allowed);
   //
   r_pro.init(clipper::Ramachandran::Pro5);
   r_pro.set_thresholds(level_prefered, level_allowed);
   // 
   r_non_gly_pro.init(clipper::Ramachandran::NonGlyPro5);
   r_non_gly_pro.set_thresholds(level_prefered, level_allowed);
}

void
coot::rama_plot::set_n_diffs(int nd) {
   n_diffs = nd;
}

void 
coot::rama_plot::setup_canvas() {

   goo_canvas_set_scale(GOO_CANVAS(canvas), zoom);

   goo_canvas_set_bounds(GOO_CANVAS(canvas), -240.0, -220.0, 210.0, 230.0);
   g_signal_connect (G_OBJECT(canvas), "button_press_event",
                       G_CALLBACK(rama_button_press), NULL);
   
   g_signal_connect (G_OBJECT(canvas), "motion_notify_event",
                       G_CALLBACK(rama_motion_notify), NULL);

   // also need expose_event, configure_event, realise
   
   g_signal_connect(G_OBJECT(canvas), "key_release_event",
                      G_CALLBACK(rama_key_release_event), NULL);

   /* set focus to canvas - we need this to get key presses. */
  GTK_WIDGET_SET_FLAGS(canvas, GTK_CAN_FOCUS);
  gtk_widget_grab_focus(GTK_WIDGET(canvas));
      }

void
coot::rama_plot::draw_rect() {

      GooCanvasItem *item;
      item = goo_canvas_rect_new(goo_canvas_get_root_item(GOO_CANVAS(canvas)),
            -100.0,
            -100.0,
            300.0,
            300.0,
            "fill-color", "grey20",
            "stroke-color", "black",
            NULL);
   }

void
coot::rama_plot::draw_it(mmdb::Manager *mol) {

   if (mol) {
      clear_canvas_items();
      generate_phi_psis(mol);
      coot::rama_stats_container_t counts = draw_phi_psi_points();
      counts_to_stats_frame(counts);
      saved_counts = counts;
      mols_ = std::pair<mmdb::Manager *, mmdb::Manager *> (mol, mol);
      //resize_it = TRUE;  // only when done start resizing.
   }
}

void
coot::rama_plot::draw_it(mmdb::Manager *mol, int SelHnd, int primary) {

   if (mol) {
      clear_canvas_items();
      generate_phi_psis_by_selection(mol, primary, SelHnd);
      coot::rama_stats_container_t counts = draw_phi_psi_points();
      counts_to_stats_frame(counts);
      saved_counts = counts;
      //resize_it = TRUE;  // only when done start resizing.
   }
}

void
coot::rama_plot::setup_background(bool blocks, bool isolines) {

   bg_all = goo_canvas_group_new(root, NULL);
   bg_gly = goo_canvas_group_new(root, NULL);
   bg_pro = goo_canvas_group_new(root, NULL);
   bg_non_gly_pro = goo_canvas_group_new(root, NULL);

   // do at least one:
   if (! blocks && ! isolines)
      blocks = 1;
   if (blocks) {
      make_background(rama, bg_all);
      make_background(r_gly, bg_gly);
      make_background(r_pro, bg_pro);
      make_background(r_non_gly_pro, bg_non_gly_pro);
   }


   if (isolines) {
      make_isolines(rama, bg_all);
      make_isolines(r_gly, bg_gly);
      make_isolines(r_pro, bg_pro);
      make_isolines(r_non_gly_pro, bg_non_gly_pro);
   }

   hide_all_background();
   // upon init show all
   show_background(bg_all);

}

// return the canvas item (group) for the typed background
// pass Ramachandran by type, e.g. r_gly...
void
coot::rama_plot::make_background(clipper::Ramachandran rama_type, GooCanvasItem *bg_group) {


   GooCanvasItem *item;
   float x;
   float y;
   float d2step;
   short int doit;
   std::string colour; 

   for (float i= -180.0; i<180.0; i += step) { 
      for (float j= -180.0; j<180.0; j += step) {

	 x =  clipper::Util::d2rad(i+((float) step)/2.0); 
	 y =  clipper::Util::d2rad(-(j+((float) step)/2.0));
         d2step = clipper::Util::d2rad(step);
	 doit = 0; 

         if ( rama_type.favored(x,y) ) {
            colour = "grey";
            colour = "red";
            colour = "pink";
            colour = "HotPink";

            item = goo_canvas_rect_new(bg_group,
                                       i+0.0,
                                       j+0.0,
                                       step+0.0,
                                       step+0.0,
                                       "fill-color", colour.c_str(),
                                       "line-width", 0,
                                       NULL);
         } else {
            if ( rama_type.allowed(x,y) ) {
	    colour = "grey50";
	    colour = "yellow"; // Procheck colour
             	              // colour = "PaleGoldenrod";
	    colour = "khaki";

               item = goo_canvas_rect_new(bg_group,
                                          i+0.0,
                                          j+0.0,
                                          step+0.0,
                                          step+0.0,
                                          "fill-color", colour.c_str(),
                                          "stroke-color", colour.c_str(),
                                          "line-width",1.0,
					 NULL);
            }

	 }
      }
   }

}

void
coot::rama_plot::show_background(GooCanvasItem *new_bg) {

   if (current_bg != new_bg) {
      if (current_bg) {
         g_object_set (current_bg,
                       "visibility", GOO_CANVAS_ITEM_INVISIBLE,
                       NULL);
      }
      g_object_set (new_bg,
                    "visibility", GOO_CANVAS_ITEM_VISIBLE,
                    NULL);
      current_bg = new_bg;
   }
}

void
coot::rama_plot::hide_all_background() {
   // maybe have a vector?
   g_object_set (bg_all,
                 "visibility", GOO_CANVAS_ITEM_INVISIBLE,
                 NULL);
   g_object_set (bg_pro,
                 "visibility", GOO_CANVAS_ITEM_INVISIBLE,
                 NULL);
   g_object_set (bg_gly,
                 "visibility", GOO_CANVAS_ITEM_INVISIBLE,
                 NULL);
   g_object_set (bg_non_gly_pro,
                 "visibility", GOO_CANVAS_ITEM_INVISIBLE,
                 NULL);
}

void
coot::rama_plot::display_background() {

   GooCanvasItem *item;
   GooCanvasItem *bg_non_gly_pro;

   // std::cout << "new underlay background" << std::endl;
   // 
   basic_white_underlay();

   bg_non_gly_pro = goo_canvas_group_new(root, NULL);

   float x;
   float y;
   float d2step;
   short int doit;
   std::string colour;

   for (float i= -180.0; i<180.0; i += step) {
      for (float j= -180.0; j<180.0; j += step) {

	 x = clipper::Util::d2rad(i+((float) step)/2.0);
	 y = clipper::Util::d2rad(-(j+((float) step)/2.0)); 
         d2step = clipper::Util::d2rad(step);
	 doit = 0; 

	 if ( rama.favored(x,y) ) {
	    colour = "grey";
	    colour = "red";
	    colour = "pink";
	    colour = "HotPink";
	    colour = "LightPink";

            item = goo_canvas_rect_new(bg_non_gly_pro,
                                       i+0.0,
                                       j+0.0,
                                       step+0.0,
                                       step+0.0,
                                       "fill-color", colour.c_str(),
                                       "line-width", 0,
					 NULL);
         } else {
            if ( rama.allowed(x,y) ) {
               colour = "grey50";
               colour= "yellow"; // Procheck colour
               // colour = "PaleGoldenrod";
               colour = "khaki";

               item = goo_canvas_rect_new(bg_non_gly_pro,
                                          i+0.0,
                                          j+0.0,
                                          step+0.0,
                                          step+0.0,
                                          "fill-color", colour.c_str(),
                                          "stroke-color", colour.c_str(),
                                          "line-width",1.0,
                                          NULL);


            }

	 }
      }
   }

}

// returns an int to make line and a vector of (x1,y1,x2,y2) to make_isolines between
std::pair<int, std::vector<float> >
coot::rama_plot::make_isolines_internal(clipper::Ramachandran rama_type, double threshold,
                                        float x_in, float y_in) {

   std::pair<int, std::vector<float> > ret;
   std::vector<float> line;
   float x;
   float y;
   float x1, y1;
   float x2, y2;
   float d2step;

   x =  clipper::Util::d2rad(x_in+((float) step)/2.0);
   y =  clipper::Util::d2rad(-(y_in+((float) step)/2.0));
   x2 = clipper::Util::d2rad(x_in+((float) 3.*step)/2.0);
   y2 = clipper::Util::d2rad(-(y_in+((float) 3.*step)/2.0));

   int bl = rama_type.probability(x,y) >= threshold;
   int br = rama_type.probability(x2,y) >= threshold;
   int tl = rama_type.probability(x,y2) >= threshold;
   int tr = rama_type.probability(x2,y2) >= threshold;
   int config = bl | (br << 1) | (tl << 2) | (tr << 3);

   if (config > 7) {
      config = 15 - config;
}

   switch(config) {
   case 0:
      break;
   case 1:
      x1 = x_in;
      y1 = y_in + 0.5 * step;
      x2 = x_in + 0.5 * step;
      y2 = y_in;
      break;
   case 2:
      x1 = x_in + 0.5 * step;
      y1 = y_in;
      x2 = x_in + step;
      y2 = y_in + 0.5 * step;
      break;
   case 3:
      x1 = x_in;
      y1 = y_in + 0.5 * step;
      x2 = x_in + step;
      y2 = y_in + 0.5 * step;
      break;
   case 4:
      x1 = x_in;
      y1 = y_in + 0.5 * step;
      x2 = x_in + 0.5 * step;
      y2 = y_in + step;
      break;
   case 5:
      x1 = x_in + 0.5 * step;
      y1 = y_in;
      x2 = x_in + 0.5 * step;
      y2 = y_in + step;
      break;
   case 6:
      // Two lines, shouldnt happen?! Not correct
      std::cout<<"BL WARNING:: I dont expect this to happen for small blocks/steps?! Have x and y below.."<<config<<std::endl;
      x1 = x_in + 0.5 * step;
      y1 = y_in;
      x2 = x_in + 0.5 * step;
      y2 = y_in + step;
      break;
   case 7:
      x1 = x_in + 0.5 * step;
      y1 = y_in + step;
      x2 = x_in + step;
      y2 = y_in + 0.5 * step;
      break;
   }

   ret.first = config;
   line.push_back(x1);
   line.push_back(y1);
   line.push_back(x2);
   line.push_back(y2);
   ret.second = line;

   return ret;

}
void
coot::rama_plot::make_isolines(clipper::Ramachandran rama_type, GooCanvasItem *bg_group) {

   GooCanvasItem *item;
   float x;
   float y;
   float x1, y1;
   float x2, y2;
   float d2step;
   short int doit;
   std::string colour;
   std::pair<int, std::vector <float> > make_line;
   std::vector<float> line;
   int config;

   for (float i= -180.0; i<180.0; i += step) {
      for (float j= -180.0; j<180.0; j += step) {

         // Make isolines. cf http://www.twodee.org/blog/?p=7595
         make_line = make_isolines_internal(rama_type, rama_threshold_allowed, i, j);
         config = make_line.first;
         line = make_line.second;
         x1 = line[0];
         y1 = line[1];
         x2 = line[2];
         y2 = line[3];

         colour = "grey50";
         colour= "yellow"; // Procheck colour
         // colour = "PaleGoldenrod";
         colour = "khaki";

         if (config > 0) {
            // have isoline
            item = goo_canvas_polyline_new_line(bg_group,
                                                x1,y1,
                                                x2,y2,
                                                "stroke-color", "purple",
                                                "line-width", 2.0,
				NULL);
         }
      }
   }
   

   // We have 2 loops so that the favoured regions are drawn last and
   // that their borders are not stamped on by the allowed regions.
   //
   for (float i= -180; i<180; i += step) {
      for (float j= -180; j<180; j += step) {
   
         colour = "HotPink";
         colour = "grey50";
         colour= "yellow"; // Procheck colour
         // colour = "PaleGoldenrod";
         colour = "khaki";

         make_line = make_isolines_internal(rama_type, rama_threshold_preferred, i, j);
         config = make_line.first;
         line = make_line.second;
         x1 = line[0];
         y1 = line[1];
         x2 = line[2];
         y2 = line[3];

         if (config > 0) {
            // have isoline
            item = goo_canvas_polyline_new_line(bg_group,
                                                x1,y1,
                                                x2,y2,
                                                "stroke-color", "LightPink",
                                                "line-width", 2.0,
                                                NULL);

} 

      }
   }

}


// draw a big square after everything else for residue i:
//
void
coot::rama_plot::big_square(const std::string &chain_id,
                            int resno,
                            const std::string &ins_code) {

   big_square(1, chain_id, resno, ins_code);

}
   
void
coot::rama_plot::big_square(int model_number,
                            const std::string &chain_id,
                            int resno,
                            const std::string &ins_code) {

   coot::residue_spec_t res_spec(chain_id, resno, ins_code);
   if (model_number >= 1) {
      if (model_number < int(phi_psi_model_sets.size())) {
         coot::util::phi_psi_t pp = phi_psi_model_sets[model_number][res_spec];
         if (pp.is_filled()) {
            draw_phi_psi_point_internal(pp, 0, 4);
         }
      }
   }
}

void
coot::rama_plot::clear_canvas_items() {

   if (arrow_grp) {
      gint iarrow = goo_canvas_item_find_child(root, arrow_grp);
      goo_canvas_item_remove_child(root, iarrow);
   }
   if (residues_grp) {
      gint ires = goo_canvas_item_find_child(root, residues_grp);
      goo_canvas_item_remove_child(root, ires);
   }

   // and make new ones
   residues_grp = goo_canvas_group_new(root, NULL);
   arrow_grp = goo_canvas_group_new(root, NULL);

}

void
coot::rama_plot::destroy_yourself() {

   // I (BL) think we shouldnt destroy but just hide.
   if (dynawin)
      gtk_widget_hide(dynawin);
   else
      std::cout << "ERROR:: could not get dialog from canvas in rama_plot::destroy_yourself"
                << std::endl;
}
// not const because we modify canvas_item_vec
// 
void
coot::rama_plot::basic_white_underlay() {

   GooCanvasItem *item;

   // FIMXE:: think if not an outline on top later
   item = goo_canvas_rect_new(goo_canvas_get_root_item(GOO_CANVAS(canvas)),
            -180.0,
            -180.0,
            360.0,
            360.0,
            "fill-color", "grey95",
            "stroke-color", "black",
            NULL);  
   // orig grey 90; grey 100 is white

} 

gint
coot::rama_plot::key_release_event(GtkWidget *widget, GdkEventKey *event) {

   // Not needed any more glade is taking care of this...
   // Keep in case we want to bind other keys at some point.

//   switch (event->keyval) {
      
//   case GDK_plus:
//   case GDK_equal:  // unshifted plus, usually.

//      zoom_in();
//      break;

//   case GDK_minus:
//      zoom_out();
//      break;
//   }

//   /* prevent the default handler from being run */
//   gtk_signal_emit_stop_by_name(GTK_OBJECT(canvas),"key_release_event");

   return 0; 
}


void
coot::rama_plot::black_border() {

   
   GooCanvasPoints *points = goo_canvas_points_new(4);
   GooCanvasItem *item;
   
   points->coords[0] = -180.0; 
   points->coords[1] = -180.0; 

   points->coords[2] = -180.0; 
   points->coords[3] =  180.0; 

   points->coords[4] = 180.0; 
   points->coords[5] = 180.0; 

   points->coords[6] =  180.0; 
   points->coords[7] = -180.0; 

      item = goo_canvas_polyline_new(goo_canvas_get_root_item(GOO_CANVAS(canvas)),
                                     TRUE, 0,
				"points", points,
                                     "stroke-color", "black",
                                     "line-width", 2.0,
				NULL);
      goo_canvas_points_unref (points);

}


void
coot::rama_plot::cell_border(int i, int j, int step) {

   // FIXME; used?
   GooCanvasItem *item;
   GooCanvasPoints *points;
   points = goo_canvas_points_new(4);
   
   points->coords[0] = i+1;
   points->coords[1] = j+1;

   points->coords[2] = i+step+1;
   points->coords[3] = j+1;

   points->coords[4] = i+step+1;
   points->coords[5] = j+step+1;

   points->coords[6] = i+1;
   points->coords[7] = j+step+1;

   item = goo_canvas_polyline_new(goo_canvas_get_root_item(GOO_CANVAS(canvas)),
                                  TRUE, 0,
				"points", points,
                                  "line-width", 2,
                                  "fill-color", "grey50",
				NULL);

   goo_canvas_points_unref (points);

   
} 

// return the region of the point
int
coot::rama_plot::draw_phi_psi_point(const coot::util::phi_psi_t &phi_psi,
				    bool as_white_flag) {

   return draw_phi_psi_point_internal(phi_psi, as_white_flag, 2); 
}

int 
coot::rama_plot::draw_phi_psi_point_internal(const coot::util::phi_psi_t &phi_psi,
                                             bool as_white_flag,
                                             int box_size) {

   if (false)
      std::cout << "draw_phi_psi_point_internal() called with draw_outliers_only "
                << draw_outliers_only << std::endl;

   GooCanvasItem *item;
   int region = coot::rama_plot::RAMA_UNKNOWN; // initially unset
   
   std::string outline_color("DimGray");

   if (box_size == 4) {
      // IDEA:: could be via callback I think
      draw_green_box(phi_psi.phi(), phi_psi.psi(), phi_psi.label());
   } else {

      if (phi_psi.residue_name() == "GLY") {
         region = draw_phi_psi_as_gly(phi_psi);
      } else {
         std::string colour;
         double phi = phi_psi.phi();
         double psi = phi_psi.psi();

         if (rama.allowed(clipper::Util::d2rad(phi),
                          clipper::Util::d2rad(psi))) {
            colour = "DodgerBlue";
            region = coot::rama_plot::RAMA_ALLOWED;
            if (rama.favored(clipper::Util::d2rad(phi),
                             clipper::Util::d2rad(psi))) {
               region = coot::rama_plot::RAMA_PREFERRED;
            }
         } else {
            colour = "red";
            region = coot::rama_plot::RAMA_OUTLIER;
         }

         if ( as_white_flag == 1 ) {
            colour = "white";
         } else {
            if (phi_psi.residue_name() == "PRO") {
               outline_color = "grey";

               if (r_pro.allowed(clipper::Util::d2rad(phi),
                                 clipper::Util::d2rad(psi))) {
                  colour = "DodgerBlue";
                  region = coot::rama_plot::RAMA_ALLOWED;
                  if (r_pro.favored(clipper::Util::d2rad(phi),
                                    clipper::Util::d2rad(psi))) {
                     region = coot::rama_plot::RAMA_PREFERRED;
                  }
               } else {
                  colour = "red3";
                  region = coot::rama_plot::RAMA_OUTLIER;
               }
            } else {

               // conventional residue
               if (r_non_gly_pro.allowed(clipper::Util::d2rad(phi),
                                         clipper::Util::d2rad(psi))) {
                  region = coot::rama_plot::RAMA_ALLOWED;
                  colour = "DodgerBlue";
                  if (r_non_gly_pro.favored(clipper::Util::d2rad(phi),
                                            clipper::Util::d2rad(psi))) {
                     region = coot::rama_plot::RAMA_PREFERRED;
                  }
               } else {
                  colour = "red3";
                  region = coot::rama_plot::RAMA_OUTLIER;
               }
            }
         }
         //           std::cout << "      debug:: tooltip_like_box for "
         //                << phi_psi.label() << " phi: " << phi_psi.phi() << " psi: " << phi_psi.psi()
         //                << phi_psi.residue_name() << std::endl;

         if ((draw_outliers_only && region == RAMA_OUTLIER) ||
             ! draw_outliers_only) {
            std::string label = phi_psi.label();

            item = goo_canvas_rect_new(residues_grp,
                                       phi-box_size,
                                       -psi-box_size,
                                       2*box_size,
                                       2*box_size,
                                       "fill-color", colour.c_str(),
                                       "stroke-color", outline_color.c_str(),
                                       "line-width", 1.,
                                       "tooltip", label.c_str(),
                                       NULL);
            // Better make a array of new variables to put in the g_object
            gchar *g_label = (gchar *)label.c_str();
            gchar *g_res_name = (gchar *)phi_psi.residue_name().c_str();
            gint *g_res_number = (gint *)phi_psi.residue_number;
            gchar *g_chain_id = (gchar *)phi_psi.chain_id.c_str();
            g_object_set_data (G_OBJECT (item), "id", g_label);
            g_object_set_data (G_OBJECT (item), "res_name", g_res_name);
            g_object_set_data (G_OBJECT (item), "res_no", g_res_number);
            g_object_set_data (G_OBJECT (item), "chain", g_chain_id);
            g_object_set_data (G_OBJECT (item), "rama_plot", (gpointer) this);
            g_signal_connect (item, "button_press_event",
                              G_CALLBACK (rama_item_button_press), NULL);
            g_signal_connect (item, "button_release_event",
                              G_CALLBACK (rama_item_button_release), NULL);
            g_signal_connect (item, "enter_notify_event",
                              G_CALLBACK (rama_item_enter_event), NULL);
            g_signal_connect (item, "motion_notify_event",
                              G_CALLBACK (rama_item_motion_event), NULL);
         }
      }
   }


   return region;
}

// move the green box
void
coot::rama_plot::draw_green_box(double phi, double psi, std::string label) {


   // std::cout << "============ adding green box at " << phi << " " << psi << std::endl;
   std::string colour = "green";
   std::string outline_color = "black";
   int box_size = 4;
   
   g_object_set(green_box_item,
                "x", phi-box_size,
                "y", -psi-box_size,
					     NULL);
   // make a tooltip
   g_object_set(green_box_item,
                "tooltip", label.c_str(),
                NULL);
   // put on top of everything
   goo_canvas_item_raise(green_box_item, NULL);

}




int // return region
coot::rama_plot::draw_phi_psi_as_gly(const coot::util::phi_psi_t &phi_psi) {

   GooCanvasItem *item;

   std::string colour;
   int region;

   double phi = phi_psi.phi();
   double psi = phi_psi.psi();
   
   if (r_gly.allowed(clipper::Util::d2rad(phi), clipper::Util::d2rad(psi))) {
      colour = "DodgerBlue";
      region = coot::rama_plot::RAMA_ALLOWED;
      if (r_gly.favored(clipper::Util::d2rad(phi), clipper::Util::d2rad(psi))) {
         region = coot::rama_plot::RAMA_PREFERRED;
      }
   } else {
      colour = "red3";
      region = coot::rama_plot::RAMA_OUTLIER;
   }

   GooCanvasPoints *points;
   points = goo_canvas_points_new(3);

   points->coords[0] =  phi;
   points->coords[1] = -psi-3;

   points->coords[2] =  phi-3;
   points->coords[3] = -psi+3;

   points->coords[4] =  phi+3;
   points->coords[5] = -psi+3;
   
   if ((draw_outliers_only && region == RAMA_OUTLIER) ||
       ! draw_outliers_only) {
      std::string label = phi_psi.label();
      item = goo_canvas_polyline_new(residues_grp,
                                     TRUE, 0,
                                     "points", points,
                                     "line-width", 1.0,
                                     "stroke-color", colour.c_str(),
                                     "tooltip", label.c_str(),
                                     NULL);
      //                                  "fill-color", colour.c_str(),


      g_object_set_data (G_OBJECT (item), "id", (gchar *)label.c_str());
      g_object_set_data (G_OBJECT (item), "res_name", (gchar *)phi_psi.residue_name().c_str());
      g_object_set_data (G_OBJECT (item), "res_no", (gint *)phi_psi.residue_number);
      g_object_set_data (G_OBJECT (item), "chain", (gchar *)phi_psi.chain_id.c_str());
      g_object_set_data (G_OBJECT (item), "rama_plot", (gpointer) this);

      g_signal_connect (item, "button_press_event",
                        G_CALLBACK (rama_item_button_press), NULL);
      g_signal_connect (item, "enter_notify_event",
                        G_CALLBACK (rama_item_enter_event), NULL);
   }
   goo_canvas_points_unref (points);

   return region;
}


// Uses a class data member
coot::rama_stats_container_t 
coot::rama_plot::draw_phi_psi_points() {

   coot::rama_stats_container_t counts;
   short int as_white_flag = 0;

   for (unsigned int imod=1; imod<phi_psi_model_sets.size(); imod++) {
      counts += draw_phi_psi_points_for_model(phi_psi_model_sets[imod]);
   }
   return counts; 
}


coot::rama_stats_container_t
coot::rama_plot::draw_phi_psi_points_for_model(const coot::phi_psis_for_model_t &pp_set) {

   coot::rama_stats_container_t counts;
   bool as_white_flag = 0;

   std::map<coot::residue_spec_t, coot::util::phi_psi_t>::const_iterator it;
   
   for (it=pp_set.phi_psi.begin(); it!=pp_set.phi_psi.end(); it++) {
      int type = draw_phi_psi_point(it->second, as_white_flag);
      if (type != coot::rama_plot::RAMA_UNKNOWN) {
	 counts.n_ramas++; 
	 if (type == coot::rama_plot::RAMA_ALLOWED)
	    counts.n_allowed++;
	 if (type == coot::rama_plot::RAMA_PREFERRED)
	    counts.n_preferred++;
      }
   }
   return counts;
}

// fill phi_psi vector
//
void
coot::rama_plot::generate_phi_psis(mmdb::Manager *mol_in) {
   if (mol_in) { 
      bool is_primary = true;
      generate_phi_psis(mol_in, is_primary);
   }
}

// fill phi_psi vector
//
void
coot::rama_plot::generate_secondary_phi_psis(mmdb::Manager *mol_in) {
   if (mol_in) { 
      bool is_primary = 0;
      generate_phi_psis(mol_in, is_primary);
   }
}


// fill phi_psi vector
//
void
coot::rama_plot::generate_phi_psis(mmdb::Manager *mol_in, bool is_primary) {

   int n_models = mol_in->GetNumberOfModels();
   // add a place-holder for the "0-th" model
   coot::phi_psis_for_model_t empty(0);
   if (is_primary) {
      phi_psi_model_sets.clear();
   phi_psi_model_sets.push_back(empty);
   }
   else {
      secondary_phi_psi_model_sets.clear();
      secondary_phi_psi_model_sets.push_back(empty);
   }
   for (int imod=1; imod<=n_models; imod++) {
      coot::phi_psis_for_model_t model_phi_psis(imod);
      mmdb::Model *model_p = mol_in->GetModel(imod);
      if (model_p) { 
	 mmdb::Chain *chain_p;
	 int nchains = model_p->GetNumberOfChains();
	 for (int ichain=0; ichain<nchains; ichain++) {
	    chain_p = model_p->GetChain(ichain);
	    int nres = chain_p->GetNumberOfResidues();
	    mmdb::Residue *residue_p;
	    if (nres > 2) {
	       for (int ires=1; ires<(nres-1); ires++) { 
		  residue_p = chain_p->GetResidue(ires);

		  // this could be improved
		  mmdb::Residue *res_prev = chain_p->GetResidue(ires-1);
		  mmdb::Residue *res_next = chain_p->GetResidue(ires+1);

		  if (res_prev && residue_p && res_next) {
		     try {
			// coot::phi_psi_t constructor can throw an error
			// (e.g. bonding atoms too far apart).
			coot::residue_spec_t spec(residue_p);
			coot::util::phi_psi_t pp(res_prev, residue_p, res_next);
			model_phi_psis.add_phi_psi(spec, pp);
		     }
		     catch (const std::runtime_error &rte) {
			// nothing too bad, just don't add that residue
			// to the plot
		     }
		  }
	       }
	    }
	 }
      }
      if (is_primary) 
	 phi_psi_model_sets.push_back(model_phi_psis);
      else 
	 secondary_phi_psi_model_sets.push_back(model_phi_psis);
   }
}

void
coot::rama_plot::generate_phi_psis_by_selection(mmdb::Manager *mol,
						bool is_primary,
						int SelectionHandle) {

   if (is_primary) { 
      phi_psi_model_sets.clear();
      coot::phi_psis_for_model_t empty(0);
      phi_psi_model_sets.push_back(empty);
   } else {
      secondary_phi_psi_model_sets.clear();
      coot::phi_psis_for_model_t empty(0);
      secondary_phi_psi_model_sets.push_back(empty);
   }
   mmdb::PResidue *residues = NULL;
   int n_residues;
   mol->GetSelIndex(SelectionHandle, residues, n_residues);
   coot::phi_psis_for_model_t model_phi_psis(1); // model 1.
   for (int ires=1; ires<(n_residues-1); ires++) {
      mmdb::Residue *res_prev = residues[ires-1];
      mmdb::Residue *res_this = residues[ires];
      mmdb::Residue *res_next = residues[ires+1];
      std::string chain_id_1 = res_prev->GetChainID();
      std::string chain_id_2 = res_this->GetChainID();
      std::string chain_id_3 = res_next->GetChainID();
      if (chain_id_1 == chain_id_2) {
	 if (chain_id_2 == chain_id_3) {
	    try { 
	       coot::util::phi_psi_t pp(res_prev, res_this, res_next);
	       coot::residue_spec_t spec(res_this);
	       model_phi_psis.add_phi_psi(spec, pp);
	    }
	    catch (std::runtime_error rte) {
	       // nothing bad.
	    }
	 }
      }
   }
   if (is_primary) 
      phi_psi_model_sets.push_back(model_phi_psis);
   else 
      secondary_phi_psi_model_sets.push_back(model_phi_psis);
}

void
coot::rama_plot::generate_phi_psis_debug()
{
   // 
}





gint
coot::rama_plot::button_press (GtkWidget *Widget, GdkEventButton *event) {

   // Note:: not used?!
   return button_press_conventional(Widget, event);
}

gint
coot::rama_plot::button_item_release (GooCanvasItem *item, GdkEventButton *event) {

   // Only relevent for edit?!
   if (phipsi_edit_flag) {
      goo_canvas_pointer_ungrab (GOO_CANVAS(canvas), item, event->time);
      // get position and residue
      gchar *chain_id;
      gint res_no;
      float phi = event->x_root;
      float psi = -1.*event->y_root;
      chain_id = (gchar*)g_object_get_data(G_OBJECT(item), "chain");
      res_no = (gint)g_object_get_data(G_OBJECT(item), "res_no");
      dragging = FALSE;
	 } 

   // return what? for what?
   return 0;
}

gint
coot::rama_plot::button_item_press (GooCanvasItem *item, GdkEventButton *event) {

   if (phipsi_edit_flag)
      return button_press_editphipsi(item, event);
   else
      if (backbone_edit_flag)
         return button_press_backbone_edit(item, event);
      else
         return button_item_press_conventional(item, event);

	 }

gint
coot::rama_plot::button_press_backbone_edit (GooCanvasItem *item, GdkEventButton *event) {

   // not doing anything (yet). Needed?

   return 0;
}

gint
coot::rama_plot::button_press_editphipsi (GooCanvasItem *item, GdkEventButton *event) {


   // This is edit phi-psi by mouse dragging.
   GdkCursor *fleur;
   // int x_as_int, y_as_int;

   drag_x = event->x;
   drag_y = event->y;

   if (event->button == 1) {
//      fleur = gdk_cursor_new (GDK_FLEUR);
      fleur = gdk_cursor_new (GDK_TOP_LEFT_ARROW);

      GdkEventMask mask = GdkEventMask(GDK_POINTER_MOTION_MASK
                                       | GDK_POINTER_MOTION_HINT_MASK
                                       | GDK_BUTTON_RELEASE_MASK);
      goo_canvas_pointer_grab (GOO_CANVAS(canvas), item,
                               mask,
                               fleur,
                               event->time);
      gdk_cursor_unref (fleur);
      dragging = TRUE;
   }
      
   return 0;
} 

gint
coot::rama_plot::button_item_press_conventional (GooCanvasItem *item, GdkEventButton *event) {

   // FIXME:: this slightly shifts the residue on the plot,
   // maybe change x and y accordingly as well
      
   if (is_stand_alone()) {
      // This is clever but only works with stand alone.
      // a different approach is needed in case we update the ramachandran plot.
      // then we use the green square

      gchar *id;
      gdouble width, height;
      guint *colour;

      if (current_residue) {
         g_object_get(current_residue,
                      "width", &width,
                      "height", &height,
                      NULL);

         width = 0.5 * width;
         height = 0.5 * height;
         g_object_set(current_residue,
                      "fill-color-rgba", current_colour,
                      "width", width,
                      "height", height,
                      NULL);
      }


      // save the color
      g_object_get(item,
                   "fill_color_rgba", &colour,
                   "width", &width,
                   "height", &height,
                   NULL);
      current_colour = colour;
      // change colour and resize
      // probably should change position too (for a rainy day).
      width = 2. * width;
      height = 2. * height;
      g_object_set (item,
                    "fill-color", "green",
                    "width", width,
                    "height", height,
                    NULL);
   
      current_residue = item;

   }
   // do something, maybe pass some data for usefullness.
   // return?! handled?
   recentre_graphics_maybe(item);

   return 1;

}

gint
coot::rama_plot::button_press_conventional (GtkWidget *widget, GdkEventButton *event) {

      if (event->button == 3) {
	 zoom += 0.2;
      goo_canvas_set_scale(GOO_CANVAS(canvas), zoom);
      }
   if (event->button == 2) {
      zoom -= 0.2;
      goo_canvas_set_scale(GOO_CANVAS(canvas), zoom);
   }
   return 1; // Handled this, right?
}

void
coot::rama_plot::recentre_graphics_maybe(GooCanvasItem *item) {

   gchar *chain;
   gint resno;

   chain = (gchar*)g_object_get_data(G_OBJECT(item), "chain");
   resno = (gint)g_object_get_data(G_OBJECT(item), "res_no");

   if (is_stand_alone()) {
      g_print("BL INFO:: this would recentre on imol: %i chain_id: %s resno: %i\n", imol, chain, resno);
   } else {
      set_go_to_atom_molecule(imol);
      set_go_to_atom_chain_residue_atom_name(chain, resno, " CA ");
   }
}


void
coot::rama_plot::recentre_graphics_maybe(mouse_util_t t) {

   if (t.model_number != coot::mouse_util_t::MODEL_NUMBER_UNSET) {
      if (t.mouse_over_secondary_set) { 
	 set_go_to_atom_molecule(molecule_numbers_.second);
      } else {
	 set_go_to_atom_molecule(molecule_numbers_.first);
      }
      set_go_to_atom_chain_residue_atom_name(t.spec.chain_id.c_str(), t.spec.res_no, " CA ");
   }

}


gint
coot::rama_plot::item_enter_event(GooCanvasItem *item, GdkEventCrossing *event) {

   gchar *res_name;
   res_name = (gchar*)g_object_get_data(G_OBJECT(item), "res_name");
   //g_print ("%s received notify-enter event\n", res_name ? res_name : "unknown");

   if (strcmp(res_name, "GLY") == 0) {
      show_background(bg_gly);
   } else {
      if (strcmp(res_name, "PRO") == 0) {
         show_background(bg_pro);
      }  else {
         show_background(bg_non_gly_pro);
      }
   }
}

gint
coot::rama_plot::item_motion_event(GooCanvasItem *item, GdkEventMotion *event) {

   gchar *res_name;
   res_name = (gchar*)g_object_get_data(G_OBJECT(item), "res_name");
   //g_print ("%s received motion-notify event\n", res_name ? res_name : "unknown");

   if (phipsi_edit_flag) {
      if (dragging && (event->state & GDK_BUTTON1_MASK)){
          double new_x = event->x;
          double new_y = event->y;
          goo_canvas_item_translate (item, new_x - drag_x, new_y - drag_y);
        }

}

} 

bool
coot::rama_plot::is_outlier(const coot::util::phi_psi_t &phi_psi) const {

   bool r = false;

   double phi = clipper::Util::d2rad(phi_psi.phi());
   double psi = clipper::Util::d2rad(phi_psi.psi());
   if (phi_psi.residue_name() == "GLY") {
      if (! r_gly.allowed(phi, psi))
    if (! r_gly.favored(phi, psi))
       r = true;
   } else {
      if (phi_psi.residue_name() == "PRO") {
    if (! r_pro.allowed(phi, psi))
       if (! r_pro.favored(phi, psi))
          r = true;
      } else {
    if (! rama.allowed(phi, psi))
       if (! rama.favored(phi, psi))
          r = true;
      }
   }
   return r;
}


// redraw everything with the given background
// 
void
coot::rama_plot::residue_type_background_as(std::string res) {

   if (res == "GLY" &&
       (displayed_rama_type != clipper::Ramachandran::Gly5)) {
      all_plot(clipper::Ramachandran::Gly5);
   }
   if (res == "PRO" &&
       (displayed_rama_type != clipper::Ramachandran::Pro5)) {
      all_plot(clipper::Ramachandran::Pro5);
   }
   if ( res != "GLY"  ) {
      if (res != "PRO") {

	 // PRE-PRO fix up needed here.
	 
	 if (displayed_rama_type != clipper::Ramachandran::NonGlyPro5) {
	    all_plot(clipper::Ramachandran::NonGlyPro5);
	 }
      }
   }
}

// and bits means zero lines and axes labels and ticks.
// 
void
coot::rama_plot::all_plot_background_and_bits(clipper::Ramachandran::TYPE type) {

   rama.init(type);
   rama.set_thresholds(rama_threshold_preferred,rama_threshold_allowed);
   display_background();
   draw_axes();
   draw_zero_lines();

   displayed_rama_type = type;

} 

void
coot::rama_plot::all_plot(clipper::Ramachandran::TYPE type) {

   //
   all_plot_background_and_bits(type);
   if (drawing_differences) {
      // draws top n_diffs differences.
      draw_phi_psi_differences();
   } else {
      draw_phi_psi_points();
   }

}




// The helper type contains a flag the signals validity (this residue
// existed in the both the first and seconde molecules).
//
coot::util::phi_psi_pair_helper_t
coot::rama_plot::make_phi_psi_pair(mmdb::Manager *mol1,
				   mmdb::Manager *mol2,
				   const std::string &chain_id1,
				   const std::string &chain_id2,
				   int i_seq_num,
				   const std::string &ins_code) const {
   mmdb::PResidue *SelResidue1;
   mmdb::PResidue *SelResidue2;
   int nSelResidues1, nSelResidues2;
   coot::util::phi_psi_pair_helper_t r;
   r.is_valid_pair_flag = 0;
   
   int selHnd1 = mol1->NewSelection();
   mol1->Select ( selHnd1, mmdb::STYPE_RESIDUE, 1, // .. TYPE, iModel
		  (char *) chain_id1.c_str(), // Chain id
		  i_seq_num-1,"*",  // starting res
		  i_seq_num+1,"*",  // ending res
		  "*",  // residue name
		  "*",  // Residue must contain this atom name?
		  "*",  // Residue must contain this Element?
		  "*",  // altLocs
		  mmdb::SKEY_NEW // selection key
		  );
   mol1->GetSelIndex (selHnd1, SelResidue1, nSelResidues1);
   int i_seq_num2 = i_seq_num;
   if (allow_seqnum_offset_flag)
      i_seq_num2 = get_seqnum_2(i_seq_num);
   if (nSelResidues1 == 3) {
      int selHnd2 = mol2->NewSelection();
      mol2->Select ( selHnd2, mmdb::STYPE_RESIDUE, 1, // .. TYPE, iModel
		     (char *) chain_id2.c_str(), // Chain id
		     i_seq_num2-1,"*",  // starting res
		     i_seq_num2+1,"*",  // ending res
		     "*",  // residue name
		     "*",  // Residue must contain this atom name?
		     "*",  // Residue must contain this Element?
		     "*",  // altLocs
		     mmdb::SKEY_NEW // selection key
		     );
      mol2->GetSelIndex (selHnd2, SelResidue2, nSelResidues2);
      if (nSelResidues2 == 3) {
	 std::pair<bool, coot::util::phi_psi_t> phi_psi_info_1 = get_phi_psi(SelResidue1);
	 if (phi_psi_info_1.first) {
	    std::pair<bool, coot::util::phi_psi_t> phi_psi_info_2 = get_phi_psi(SelResidue2);
	    if (phi_psi_info_2.first) {
	       bool is_valid = 1;
	       r = coot::util::phi_psi_pair_helper_t(phi_psi_info_1.second,
						     phi_psi_info_2.second,
						     is_valid);
	    } else {
	       // std::cout << "didn't get good phi_psi_info_2 " << std::endl;
	    } 
	 } else {
	    // std::cout << "didn't get good phi_psi_info_1 " << std::endl;
	 } 
      } 
      mol2->DeleteSelection(selHnd2);
   }
   mol1->DeleteSelection(selHnd1);
   return r;
}


// SelResidue is guaranteed to have 3 residues (there is no protection
// for that in this function).
std::pair<bool, coot::util::phi_psi_t> coot::rama_plot::get_phi_psi(mmdb::PResidue *SelResidue) const {
   return coot::util::get_phi_psi(SelResidue);
}





void
coot::rama_plot::draw_phi_psis_on_canvas(char *filename) {

   
   // Do the background first, of course.
   //
   display_background();

   draw_axes();
   draw_zero_lines(); 

   // or however you want to get your mmdbmanager
   // 
   mmdb::Manager *mol = rama_get_mmdb_manager(filename); 

   if (mol) { 
      // put the results in the *primary* list
      generate_phi_psis(mol);
      
      draw_phi_psi_points();
      
      delete mol;
   }
}


void
coot::rama_plot::draw_it(int imol1, int imol2,
			 mmdb::Manager *mol1, mmdb::Manager *mol2) {

   clear_canvas_items();
   molecule_numbers_ = std::pair<int, int> (imol1, imol2); // save for later
   draw_2_phi_psi_sets_on_canvas(mol1, mol2);
   if (is_kleywegt_plot()) {
      hide_stats_frame();
      fill_kleywegt_comboboxes(mol1, mol2);
   }
}

void
coot::rama_plot::draw_it(int imol1, int imol2,
                         mmdb::Manager *mol1, mmdb::Manager *mol2,
                         int SelHnd1, int SelHnd2) {

   clear_canvas_items();
   molecule_numbers_ = std::pair<int, int> (imol1, imol2); // save for later
   draw_2_phi_psi_sets_on_canvas(mol1, mol2, SelHnd1, SelHnd2);
   if (is_kleywegt_plot()) {
      hide_stats_frame();
      fill_kleywegt_comboboxes(mol1, mol2);
   }
}

// the mmdb::Manager could have gone out of date when we come to redraw
// the widget after refinement, so we pass the imol1 and imol2 so that
// we can ask globjects if the molecule numbers are still valid.
// 
void
coot::rama_plot::draw_it(int imol1, int imol2,
			 mmdb::Manager *mol1, mmdb::Manager *mol2,
			 const std::string &chain_id_1, const std::string &chain_id_2) {

   clear_canvas_items();
   molecule_numbers_ = std::pair<int, int> (imol1, imol2); // save for later
   chain_ids_ = std::pair<std::string, std::string> (chain_id_1, chain_id_2);
   mols_ = std::pair<mmdb::Manager *, mmdb::Manager *> (mol1, mol2);
   if (allow_seqnum_offset_flag)
      set_seqnum_offset(imol1, imol2, mol1, mol2, chain_id_1, chain_id_2);
   draw_2_phi_psi_sets_on_canvas(mol1, mol2, chain_id_1, chain_id_2);
   if (is_kleywegt_plot()) {
      hide_stats_frame();
      fill_kleywegt_comboboxes(mol1, mol2);
   }
}

// Was from a shelx molecule with A 1->100 and B 201->300.
// For shelx molecule as above, what do we need to add to seqnum_1 to get the
// corresponding residue in the B chain (in the above example it is 100).
// 
void
coot::rama_plot::set_seqnum_offset(int imol1, int imol2,
				   mmdb::Manager *mol1,
				   mmdb::Manager *mol2,
				   const std::string &chain_id_1,
				   const std::string &chain_id_2) {

   seqnum_offset = mmdb::MinInt4;
   if (is_valid_model_molecule(imol1)) { 
      if (is_valid_model_molecule(imol2)) {

	 int imod = 1;
      
	 mmdb::Model *model_p_1 = mol1->GetModel(imod);
	 mmdb::Chain *chain_p_1;
	 // run over chains of the existing mol
	 int nchains_1 = model_p_1->GetNumberOfChains();
	 for (int ichain_1=0; ichain_1<nchains_1; ichain_1++) {
	    chain_p_1 = model_p_1->GetChain(ichain_1);
	    if (chain_id_1 == chain_p_1->GetChainID()) {
	       int nres_1 = chain_p_1->GetNumberOfResidues();
	       mmdb::PResidue residue_p_1;
	       if (nres_1 > 0) {
		  residue_p_1 = chain_p_1->GetResidue(0);
		  mmdb::Model *model_p_2 = mol2->GetModel(imod);
		  mmdb::Chain *chain_p_2;
		  int nchains_2 = model_p_2->GetNumberOfChains();
		  for (int ichain_2=0; ichain_2<nchains_2; ichain_2++) {
		     chain_p_2 = model_p_2->GetChain(ichain_2);
		     if (chain_id_2 == chain_p_2->GetChainID()) {
			int nres_2 = chain_p_2->GetNumberOfResidues();
			mmdb::PResidue residue_p_2;
			if (nres_2 > 0) {
			   residue_p_2 = chain_p_2->GetResidue(0);

			   seqnum_offset = residue_p_2->GetSeqNum() - residue_p_1->GetSeqNum();
			}
		     }
		  }
	       }
	    }
	 }
      }
   }

   //std::cout << "DEBUG:: seqnum_offset is: " << seqnum_offset << std::endl;
   
   if (seqnum_offset == mmdb::MinInt4) {
      std::cout << "WARNING:: Ooops! Failed to set the Chain Residue numbering different\n"
		<< "WARNING::        offset correctly." << std::endl;
      std::cout << "WARNING:: Ooops! Bad Kleywegts will result!" << std::endl;
      seqnum_offset = 0;
   }
} 

int
coot::rama_plot::get_seqnum_2(int seqnum_1) const {

   return seqnum_1 + seqnum_offset;

}

void
coot::rama_plot::hide_stats_frame() {

   if (canvas) {
      gtk_widget_hide(rama_stats_frame);
   } else {
      std::cout << "ERROR:: null widget in hide_stats_frame\n";
   } 
}

void
coot::rama_plot::counts_to_stats_frame(const coot::rama_stats_container_t &sc) {

   if (sc.n_ramas > 0) { 
      float pref_frac = float(sc.n_preferred)/float(sc.n_ramas);
      float allow_frac = float(sc.n_allowed)/float(sc.n_ramas);
      int n_outliers = sc.n_ramas - sc.n_preferred - sc.n_allowed;
      float outlr_frac = float(n_outliers)/float(sc.n_ramas);

      std::string pref_str = "In Preferred Regions:  ";
      pref_str += coot::util::int_to_string(sc.n_preferred);
      pref_str += "  (";
      pref_str += coot::util::float_to_string(100.0*pref_frac);
      pref_str += "%)";
	 
      std::string allow_str = "In Allowed Regions:  ";
      allow_str += coot::util::int_to_string(sc.n_allowed);
      allow_str += "  (";
      allow_str += coot::util::float_to_string(100.0*allow_frac);
      allow_str += "%)";
	 
      std::string outlr_str = "Outliers:  ";
      outlr_str += coot::util::int_to_string(n_outliers);
      outlr_str += "  (";
      outlr_str += coot::util::float_to_string(100.0*outlr_frac);
      outlr_str += "%)";

      gtk_label_set_text(GTK_LABEL(rama_stats_label1),  pref_str.c_str());
      gtk_label_set_text(GTK_LABEL(rama_stats_label2), allow_str.c_str());
      gtk_label_set_text(GTK_LABEL(rama_stats_label3), outlr_str.c_str());
	 
   } else {
      hide_stats_frame();
   } 
} 

// A canvas item (actually group), so that we can destroy it
//
void
coot::rama_plot::counts_to_canvas(cairo_t *cr) {

   coot::rama_stats_container_t sc = saved_counts;

   if (sc.n_ramas > 0) {

      cairo_set_source_rgb(cr, 0.8, 0.8, 0.8);
      cairo_rectangle(cr, -170, 280, 300, 120);
      cairo_fill(cr);

      cairo_set_source_rgb(cr, 0.1, 0.1, 0.1);

      cairo_select_font_face(cr, "Purisa",
                             CAIRO_FONT_SLANT_NORMAL,
                             CAIRO_FONT_WEIGHT_BOLD);

      cairo_set_font_size(cr, 14);

      float pref_frac = float(sc.n_preferred)/float(sc.n_ramas);
      float allow_frac = float(sc.n_allowed)/float(sc.n_ramas);
      int n_outliers = sc.n_ramas - sc.n_preferred - sc.n_allowed;
      float outlr_frac = float(n_outliers)/float(sc.n_ramas);

      std::string pref_str = "In Preferred Regions:  ";
      pref_str += coot::util::int_to_string(sc.n_preferred);
      pref_str += "  (";
      pref_str += coot::util::float_to_string(100.0*pref_frac);
      pref_str += "%)";

      cairo_move_to(cr, -160, 310);
      cairo_show_text(cr, pref_str.c_str());

      std::string allow_str = "In Allowed Regions:  ";
      allow_str += coot::util::int_to_string(sc.n_allowed);
      allow_str += "  (";
      allow_str += coot::util::float_to_string(100.0*allow_frac);
      allow_str += "%)";

      cairo_move_to(cr, -160, 340);
      cairo_show_text(cr, allow_str.c_str());

      std::string outlr_str = "Outliers:  ";
      outlr_str += coot::util::int_to_string(n_outliers);
      outlr_str += "  (";
      outlr_str += coot::util::float_to_string(100.0*outlr_frac);
      outlr_str += "%)";

      cairo_move_to(cr, -160, 370);
      cairo_show_text(cr, outlr_str.c_str());

   }
} 


void
coot::rama_plot::draw_it(const coot::util::phi_psi_t &phipsi) {

   clear_canvas_items();
   coot::phi_psis_for_model_t phi_psi_set(1);
   coot::residue_spec_t spec("", 0, "");
   phi_psi_set.add_phi_psi(spec, phipsi);
   phi_psi_model_sets.push_back(phi_psi_set);
   draw_phi_psi_points();
}


void
coot::rama_plot::draw_it(const std::vector<coot::util::phi_psi_t> &phi_psi_s) {

   clear_canvas_items();
   phi_psi_model_sets.clear();
   //clear_last_canvas_items(phi_psi_s.size());
   
   coot::phi_psis_for_model_t phi_psi_set_m0(0);
   coot::phi_psis_for_model_t phi_psi_set(1);
   phi_psi_model_sets.push_back(phi_psi_set_m0); // dummy/unused for model 0
   for (unsigned int i=0; i<phi_psi_s.size(); i++) {
      coot::residue_spec_t spec("", i, "");
      phi_psi_set.add_phi_psi(spec, phi_psi_s[i]);
   }
   phi_psi_model_sets.push_back(phi_psi_set);
   draw_phi_psi_points();
   
}

void 
coot::rama_plot::draw_2_phi_psi_sets_on_canvas(mmdb::Manager *mol1, 
                                               mmdb::Manager *mol2) {


   // Are you sure that you want to edit this function? (not the one below?)

   if (mol1 && mol2) { 

      bool primary = 1;
      generate_phi_psis(mol1,  primary);
      generate_phi_psis(mol2, !primary);
      
      // all_plot_background_and_bits(clipper::Ramachandran::All);
      // std::cout << "finding differences..." << std::endl;
      find_phi_psi_differences(); 
      // std::cout << "drawing differences..." << std::endl;
      draw_phi_psi_differences();
      
      // do we need to do something like this?
      //    for (int ich=0; ich<phi_psi_sets.size(); ich++) 
      //       draw_phi_psi_points(ich);
      
      drawing_differences = 1; // set flag for later drawing
   }
}

void
coot::rama_plot::draw_2_phi_psi_sets_on_canvas(mmdb::Manager *mol1,
                                               mmdb::Manager *mol2,
                                               int SelHnd1,
                                               int SelHnd2) {

   if (! mol1) return;
   if (! mol2) return;

   int imod = 1;

   phi_psi_model_sets.clear();
   secondary_phi_psi_model_sets.clear();

   generate_phi_psis_by_selection(mol1, 1, SelHnd1);
   generate_phi_psis_by_selection(mol2, 0, SelHnd2);

   mol1->DeleteSelection(SelHnd1);
   mol2->DeleteSelection(SelHnd2);

   // all_plot_background_and_bits(clipper::Ramachandran::All);
   // std::cout << "finding differences..." << std::endl;
   //FIXME
   find_phi_psi_differences();
   // std::cout << "drawing differences..." << std::endl;
   draw_phi_psi_differences();

   drawing_differences = 1; // set flag for later drawing
}

// Kleywegt plots call this function
void 
coot::rama_plot::draw_2_phi_psi_sets_on_canvas(mmdb::Manager *mol1, 
					       mmdb::Manager *mol2,
					       std::string chainid1,
					       std::string chainid2) {

   if (! mol1) return;
   if (! mol2) return;

   int imod = 1;

   phi_psi_model_sets.clear();
   secondary_phi_psi_model_sets.clear();

   int selhnd_1 = mol1->NewSelection();
   int selhnd_2 = mol2->NewSelection();

   mol1->Select(selhnd_1, mmdb::STYPE_RESIDUE, 0,
		chainid1.c_str(),
		mmdb::ANY_RES, "*",
		mmdb::ANY_RES, "*",
		"*",  // residue name
		"*",  // Residue must contain this atom name?
		"*",  // Residue must contain this Element?
		"*",   // altLocs
		mmdb::SKEY_NEW // selection key
		);

   mol2->Select(selhnd_2, mmdb::STYPE_RESIDUE, 0,
		chainid2.c_str(),
		mmdb::ANY_RES, "*",
		mmdb::ANY_RES, "*",
		"*",  // residue name
		"*",  // Residue must contain this atom name?
		"*",  // Residue must contain this Element?
		"*",   // altLocs
		mmdb::SKEY_NEW // selection key
		);
   
   generate_phi_psis_by_selection(mol1, 1, selhnd_1);
   generate_phi_psis_by_selection(mol2, 0, selhnd_2);

   mol1->DeleteSelection(selhnd_1);
   mol2->DeleteSelection(selhnd_2);
   
   // all_plot_background_and_bits(clipper::Ramachandran::All);
   // std::cout << "finding differences..." << std::endl;
   std::cout << "finding differences... for chains " <<chainid1 <<chainid2<< std::endl;
   find_phi_psi_differences(chainid1, chainid2); 
   std::cout << "drawing differences..." << std::endl;
   // std::cout << "drawing differences..." << std::endl;
   draw_phi_psi_differences();

   drawing_differences = 1; // set flag for later drawing
}

void
coot::rama_plot::find_phi_psi_differences() {
   std::string chain_id1;
   std::string chain_id2;
   find_phi_psi_differences_internal(chain_id1, chain_id2, 0);
}

void
coot::rama_plot::find_phi_psi_differences(const std::string &chain_id1,
					  const std::string &chain_id2) { 
   find_phi_psi_differences_internal(chain_id1, chain_id2, 1);
}


void
coot::rama_plot::find_phi_psi_differences_internal(const std::string &chain_id1,
						   const std::string &chain_id2,
						   bool use_chain_ids) {

   double d1, d2;

   std::map<coot::residue_spec_t, coot::util::phi_psi_t>::const_iterator it;

   diff_sq.clear();
   for (unsigned int imod=1; imod<phi_psi_model_sets.size(); imod++) {
	 std::cout << "in find_phi_psi_differences_internal with model number " << imod
		   << " primary size " << phi_psi_model_sets[imod].size() << std::endl;
      for (it=phi_psi_model_sets[imod].phi_psi.begin();
	   it!=phi_psi_model_sets[imod].phi_psi.end(); it++) {
	 coot::residue_spec_t spec_1 = it->first;
	 coot::residue_spec_t spec_2 = it->first;
	 // now (re)set chain_id of spec_2
	 // FIXME ???
	 if (use_chain_ids) {
	    spec_2.chain_id = chain_id2;
	 }
	 coot::util::phi_psi_t pp_1 = it->second;
	 coot::util::phi_psi_t pp_2 = secondary_phi_psi_model_sets[imod][spec_2];
	 
	 if (pp_2.is_filled()) {
	    d1 = fabs(pp_1.phi() - pp_2.phi());
	    d2 = fabs(pp_1.psi() - pp_2.psi());
	    if (d1 > 180) d1 -= 360; 
	    if (d2 > 180) d2 -= 360;
	    double v = sqrt(d1*d1 + d2*d2);
	    diff_sq_t ds(pp_1, pp_2, spec_1, spec_2, v); // something
	    diff_sq.push_back(ds);
	 } else {
	    std::cout << "WARNING:: Not found " << spec_2 << " from ref spec "
		      << spec_1 << " in " << secondary_phi_psi_model_sets[imod].size()
		      << " residue in secondary set" << std::endl;
	 } 
      }
   }
      
   // sort diff_sq

//   std::cout << " debug:: -- generated " << diff_sq.size() << " rama differences " << std::endl;
   std::sort(diff_sq.begin(), diff_sq.end(), compare_phi_psi_diffs);

}


bool coot::compare_phi_psi_diffs(const diff_sq_t &d1, const diff_sq_t d2) {
      return (d1.v() > d2.v());
}


// n_diffs is the max number of differences to be displayed, 50 typically.
// 
void
coot::rama_plot::draw_phi_psi_differences() {


   // First calculate the differences
   //
   // then sort the differences
   //
   // then plot the top n differences
   // 
   // The first point should be hollowed out (white centre), then an
   // arrow should point to a conventionally drawn point from the
   // second set.
   //
   // So we will need to change things so that the drawing routine
   // will know whether to put a white interior or not.
   //
   // Note that glycines will not be affected since they are made from
   // (canvas) lines, not rectangles.
   // 
   // Maybe I could make the starting
   // triangle be drawn in grey in that case.

   if (phi_psi_model_sets.size() == 0 ) {

      std::cout << "oops! No phi psi data!" << std::endl;

   } else {

      
      int n_vsize = diff_sq.size();

      // std::cout << "debug:: draw_phi_psi_differences() " << n_vsize << " " << n_diffs << std::endl;

      for (int j=0; j<n_diffs && j<n_vsize; j++) {

         coot::util::phi_psi_t pp1 = diff_sq[j].phi_psi_1();
         coot::util::phi_psi_t pp2 = diff_sq[j].phi_psi_2();

         draw_phi_psi_point(pp1, 1);
         draw_phi_psi_point(pp2, 0);
         GooCanvasPoints *points = goo_canvas_points_new(2);

         draw_kleywegt_arrow(pp1, pp2, points);
      }
   }
}


void
coot::rama_plot::draw_kleywegt_arrow(const coot::util::phi_psi_t &phi_psi_primary,
				     const coot::util::phi_psi_t &phi_psi_secondary,
                                     GooCanvasPoints *points) {

   
   coot::rama_kleywegt_wrap_info wi = test_kleywegt_wrap(phi_psi_primary,
							 phi_psi_secondary);
   

   GooCanvasItem *item;
   points->coords[0] =  phi_psi_primary.phi();
   points->coords[1] = -phi_psi_primary.psi();

   points->coords[2] =  phi_psi_secondary.phi();
   points->coords[3] = -phi_psi_secondary.psi();
   // check if one is outlier
   if ((draw_outliers_only && (is_outlier(phi_psi_primary) || is_outlier(phi_psi_secondary))) ||
       ! draw_outliers_only) {


      if (wi.is_wrapped == 0) {
         // the normal case
         item = goo_canvas_polyline_new_line(arrow_grp,
                                             phi_psi_primary.phi(),
                                             -phi_psi_primary.psi(),
                                             phi_psi_secondary.phi(),
                                             -phi_psi_secondary.psi(),
                                             "line-width", 1.0,
                                             "start-arrow", FALSE,
                                             "end-arrow", TRUE,
                                             "arrow-length", 8.0,
                                             "arrow-tip-length", 5.0,
                                             "arrow-width", 6.0,
                                             "fill-color", "black",
                                             "stroke-color", "black",
                                             NULL);
      } else {
         // border crosser:

         // line to the border
         points->coords[0] =  phi_psi_primary.phi();
         points->coords[1] = -phi_psi_primary.psi();

         points->coords[2] =  wi.primary_border_point.first;
         points->coords[3] = -wi.primary_border_point.second;

         if (0)
            std::cout << "Borderline 1 : "
                      << "(" << phi_psi_primary.phi() << "," << phi_psi_primary.psi() << ")"
                      << " to "
                      << "("
                      << wi.primary_border_point.first
                      << ", " << wi.primary_border_point.second
                      << ")" << std::endl;

         item = goo_canvas_polyline_new_line(arrow_grp,
                                             phi_psi_primary.phi(),
                                             -phi_psi_primary.psi(),
                                             wi.primary_border_point.first,
                                             -wi.primary_border_point.second,
                                             "line-width", 1.0,
                                             "fill-color", "black",
                                             NULL);

         // line from the border

         points->coords[0] =  wi.secondary_border_point.first;
         points->coords[1] = -wi.secondary_border_point.second;

         points->coords[2] =  phi_psi_secondary.phi();
         points->coords[3] = -phi_psi_secondary.psi();

         std::cout << "Borderline 2: "
                   << "(" << wi.secondary_border_point.first << ","
                   << wi.secondary_border_point.second << ")"
                   << " to "
                   << "("  << phi_psi_secondary.phi()
                   << ", " << phi_psi_secondary.psi()
                   << ")" << std::endl;

         item = goo_canvas_polyline_new_line(arrow_grp,
                                             wi.secondary_border_point.first,
                                             -wi.secondary_border_point.second,
                                             phi_psi_secondary.phi(),
                                             -phi_psi_secondary.psi(),
                                             "line-width", 1.0,
                                             "start-arrow", FALSE,
                                             "end-arrow", TRUE,
                                             "arrow-length", 8.0,
                                             "arrow-tip-length", 5.0,
                                             "arrow-width", 6.0,
                                             "fill-color", "black",
                                             "stroke-color", "black",
                                             NULL);

      }
   }
}

coot::rama_kleywegt_wrap_info
coot::rama_plot::test_kleywegt_wrap(const coot::util::phi_psi_t &phi_psi_primary,
				    const coot::util::phi_psi_t &phi_psi_secondary) const {

   coot::rama_kleywegt_wrap_info wi;


   if (fabs(phi_psi_primary.phi() - phi_psi_secondary.phi()) > 200.0) {

      wi.is_wrapped = 1;
      
      float phi_1 = phi_psi_primary.phi();
      float psi_1 = phi_psi_primary.psi();
      float phi_2 = phi_psi_secondary.phi();
      float psi_2 = phi_psi_secondary.psi();

      float psi_diff = psi_2 - psi_1; 
      float psi_gradient = 999999999.9;
      if (fabs(psi_diff) > 0.000000001)
	 psi_gradient = (180.0 - phi_1)/(phi_2 + 360.0 - phi_1);

      float psi_critical = psi_1 + psi_gradient * (psi_2 - psi_1);
      wi.primary_border_point.first = 180.0;
      wi.primary_border_point.second = psi_critical;
      wi.secondary_border_point.first = -180.0;
      wi.secondary_border_point.second = psi_critical;
   } 
   if (fabs(phi_psi_primary.psi() - phi_psi_secondary.psi()) > 200.0) {

      wi.is_wrapped = 1;

      float phi_1 = phi_psi_primary.phi();
      float psi_1 = phi_psi_primary.psi();
      float phi_2 = phi_psi_secondary.phi();
      float psi_2 = phi_psi_secondary.psi();

      float psi_diff = psi_2 - psi_1; 
      float psi_gradient = 999999999.9;
      if (fabs(psi_diff) > 0.000000001)
	 psi_gradient = (-180.0 - (psi_2 - 360.0))/(psi_1 - (psi_2 - 360.0));

      float phi_critical = phi_2 + psi_gradient * (phi_1 - phi_2);
      wi.primary_border_point.first = phi_critical;
      wi.primary_border_point.second = -180.0;
      wi.secondary_border_point.first = phi_critical;
      wi.secondary_border_point.second = 180.0;
   } 
   return wi;
} 


void
coot::rama_plot::draw_zero_lines() {

   GooCanvasItem *item;
   GooCanvasItem *zero_grp;
   zero_grp = goo_canvas_group_new(root, NULL);
   item = goo_canvas_polyline_new_line(zero_grp,
                                       0.0, -180.0,
                                       0.0, 180.0,
                                       "stroke-color", "grey",
                                       "line-width", 1.0,
			    NULL);

   item = goo_canvas_polyline_new_line(zero_grp,
                                       -180.0, 0.0,
                                       180.0, 0.0,
                                       "line-width", 1.0,
                                       "stroke-color", "grey",
				NULL);

}

// Tick marks and text labels for phi and phi axes.
// 
void
coot::rama_plot::draw_axes() {

   // First do the text for the axes labels.
   //

   GooCanvasItem *item;
   GooCanvasItem *axis_grp;

   axis_grp = goo_canvas_group_new(root, NULL);
   item = goo_canvas_text_new(axis_grp,
                              "Phi",
                              -10.0,
                              220.0,
                              -1,
                              GTK_ANCHOR_WEST,
				"font", fixed_font_str.c_str(),
                              "fill-color", "black",
				NULL);

   item = goo_canvas_text_new(axis_grp,
                              "Psi",
                              -230.0,
                              15.0,
                              -1,
                              GTK_ANCHOR_WEST,
			      "font", fixed_font_str.c_str(),
                              "fill-color", "black",
			      NULL);

   // Ticks
   std::vector<canvas_tick_t> pnts;

   // x axis
   pnts.push_back(canvas_tick_t(0,-180.0,180.0));
   pnts.push_back(canvas_tick_t(0,-120.0,180.0));
   pnts.push_back(canvas_tick_t(0,-60.0,180.0));
   pnts.push_back(canvas_tick_t(0,0.0,180.0));
   pnts.push_back(canvas_tick_t(0,60.0,180.0));
   pnts.push_back(canvas_tick_t(0,120.0,180.0));
   pnts.push_back(canvas_tick_t(0,180.0,180.0));
   

   // y axis
    pnts.push_back(canvas_tick_t(1,-180.0,-180.0));
    pnts.push_back(canvas_tick_t(1,-180.0,-120.0));
    pnts.push_back(canvas_tick_t(1,-180.0,-60.0));
    pnts.push_back(canvas_tick_t(1,-180.0,0.0));
    pnts.push_back(canvas_tick_t(1,-180.0,60.0));
    pnts.push_back(canvas_tick_t(1,-180.0,120.0));
    pnts.push_back(canvas_tick_t(1,-180.0,180.0));
   

   for (unsigned int i=0; i<pnts.size(); i++) { 

      item = goo_canvas_polyline_new_line(axis_grp,
                                          pnts[i].start_x(), pnts[i].start_y(),
                                          pnts[i].end_x(), pnts[i].end_y(),
                                          "line-width", 1.,
                                          "fill-color", "black",
				   NULL);

   }

   // Ticks text

   // x axis

   std::vector<int> tick_text;
   tick_text.push_back(-180); 
   tick_text.push_back(-120); 
   tick_text.push_back(-60); 
   tick_text.push_back(0); 
   tick_text.push_back(60); 
   tick_text.push_back(120); 
   tick_text.push_back(180);
   char text[20];

   for (unsigned int i=0; i<tick_text.size(); i++) {

      snprintf(text,19,"%d",tick_text[i]);
   
      item = goo_canvas_text_new(axis_grp,
                                 text,
                                 -230.0,
                                 -tick_text[i] +0.0,
                                 -1,
                                 GTK_ANCHOR_WEST,
				 "font", fixed_font_str.c_str(),
                                 "fill-color", "black",
				   NULL);


      
//    // y axis

      item = goo_canvas_text_new(axis_grp,
                                 text,
                                 tick_text[i] - 10.0,
                                 200.0,
                                 -1,
                                 GTK_ANCHOR_WEST,
				   "font", fixed_font_str.c_str(),
                                 "fill-color", "black",
				   NULL);

   }
} 




mmdb::Manager *
coot::rama_plot::rama_get_mmdb_manager(std::string pdb_name) {

   mmdb::ERROR_CODE err;
   mmdb::Manager* MMDBManager;

   // Needed for the error message printing: 
   // MMDBManager->GetInputBuffer(S, lcount);
   // Used by reference and as a pointer.  Grimness indeed.
   int  error_count;
   char error_buf[500];

   //   Make routine initializations
   //
   mmdb::InitMatType();

   MMDBManager = new mmdb::Manager;

   std::cout << "Reading coordinate file: " << pdb_name.c_str() << "\n";
   err = MMDBManager->ReadCoorFile(pdb_name.c_str());
   
   if (err) {
      // does_file_exist(pdb_name.c_str());
      std::cout << "There was an error reading " << pdb_name.c_str() << ". \n";
      std::cout << "ERROR " << err << " READ: "
		<< mmdb::GetErrorDescription(err) << std::endl;
      //
      // This makes my stomach churn too. Sorry.
      // 
      MMDBManager->GetInputBuffer(error_buf, error_count);
      if (error_count >= 0) { 
	 std::cout << "         LINE #" << error_count << "\n     "
		   << error_buf << std::endl << std::endl;
      } else {
	 if (error_count == -1) { 
	    std::cout << "       CIF ITEM: " << error_buf << std::endl << std::endl;
	 }
      }

      //
   } else {
      // we read the coordinate file OK.
      //
      switch (MMDBManager->GetFileType())  {
      case mmdb::MMDB_FILE_PDB    :  std::cout << " PDB"         ;
	 break;
      case mmdb::MMDB_FILE_CIF    :  std::cout << " mmCIF"       ; 
	 break;
      case mmdb::MMDB_FILE_Binary :  std::cout << " MMDB binary" ;
	 break;
      default:
	 std::cout << " Unknown (report as a bug!)\n";
      }
      std::cout << " file " << pdb_name.c_str() << " has been read.\n";
   }
   
    return MMDBManager;
}

void
coot::rama_plot::zoom_out() {

   // make sure it doesnt get smaller than the window
   if (zoom-0.2 < 0.8) {
      zoom = 0.8;
      g_print("BL INFO:: already smallest size to fit the window, wont make it smaller\n");
   } else {
   zoom -= 0.2;
   }
   goo_canvas_set_scale(GOO_CANVAS(canvas), zoom);


}
void
coot::rama_plot::zoom_in() {

   zoom += 0.2;
   goo_canvas_set_scale(GOO_CANVAS(canvas), zoom);
}

void
coot::rama_plot::allow_seqnum_offset() {

   allow_seqnum_offset_flag = 1;
   // debug();
} 

void
coot::rama_plot::write_pdf(std::string &file_name) {
#if CAIRO_HAS_PDF_SURFACE

   gdouble x1, y1, x2, y2;
   g_object_get(GOO_CANVAS(canvas),
                         "x1", &x1,
                         "y1", &y1,
                         "x2", &x2,
                         "y2", &y2,
                         NULL);
   cairo_surface_t *surface;
   cairo_t *cr;
   double size_x = x2 - x1;
   double size_y = y2 - y1;
   int add_text = 0;
   // Add some space for a title
   size_y += 80;
   // Increase the canvas if needed
   if (!is_kleywegt_plot()) {
      if (saved_counts.n_ramas > 0) {
      size_y += 200;
      add_text = 1;
      }
   }
   surface = cairo_pdf_surface_create(file_name.c_str(), size_x, size_y);
   cr = cairo_create (surface);
   // place in the middle?!
   cairo_translate (cr, 240, 280);
   // add title
   const gchar *title = gtk_label_get_text(GTK_LABEL(dynarama_label));
   cairo_set_source_rgb(cr, 0.1, 0.1, 0.1);
   cairo_select_font_face(cr, "Purisa",
       CAIRO_FONT_SLANT_NORMAL,
       CAIRO_FONT_WEIGHT_BOLD);
   cairo_set_font_size(cr, 20);
   cairo_move_to(cr, -180, -230); // FIXME, maybe dependent on text length?!
   cairo_show_text(cr, title);
   // add stats
   if (add_text) {
      counts_to_canvas(cr);
   }
   goo_canvas_render(GOO_CANVAS(canvas), cr, NULL, 1.0);
   cairo_show_page(cr);
   cairo_surface_destroy(surface);
   cairo_destroy(cr);

#else
   std::cout << "No PDF (no PDF Surface in Cairo)" << std::endl;
#endif


}

void
coot::rama_plot::write_png(std::string &file_name) {

   gdouble x1, y1, x2, y2;
   g_object_get(GOO_CANVAS(canvas),
                         "x1", &x1,
                         "y1", &y1,
                         "x2", &x2,
                         "y2", &y2,
                         NULL);

   int size_x = (int)x2 - (int)x1;
   int size_y = (int)y2 - (int)y1;
   int add_text = 0;
   // Add some space for a title
   size_y += 80;
   // Increase the canvas if needed
   if (!is_kleywegt_plot()) {
      if (saved_counts.n_ramas > 0) {
      size_y += 200;
      add_text = 1;
      }
   }
   cairo_surface_t *surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, size_x, size_y);
   cairo_t *cr = cairo_create (surface);
   /* move closer to the centre */
   cairo_translate (cr, 240, 280);

   // add title
   const gchar *title = gtk_label_get_text(GTK_LABEL(dynarama_label));
   cairo_set_source_rgb(cr, 0.1, 0.1, 0.1);
   cairo_select_font_face(cr, "Purisa",
       CAIRO_FONT_SLANT_NORMAL,
       CAIRO_FONT_WEIGHT_BOLD);
   cairo_set_font_size(cr, 20);
   cairo_move_to(cr, -180, -230); // FIXME, maybe dependent on text length?!
   cairo_show_text(cr, title);

   if (add_text) {
      counts_to_canvas(cr);
   }
   goo_canvas_render (GOO_CANVAS(canvas), cr, NULL, 1.0);
   cairo_surface_write_to_png(surface, file_name.c_str());
   cairo_surface_destroy (surface);
   cairo_destroy (cr);
   //goo_canvas_item_remove(tmp);

}

void
coot::rama_plot::open_pdb_file(const std::string &file_name) {

   mmdb::Manager *mol = rama_get_mmdb_manager(file_name);
   draw_it(mol);
}


void
coot::rama_plot::make_kleywegt_plot(int on_off) {

   if (on_off != is_kleywegt_plot()) {
      // we have a change
      if (on_off == 1 ) {
         // change to kleywegt
         gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(kleywegt_radiomenuitem), TRUE);
      } else {
         // change to normal
         gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(rama_radiomenuitem), TRUE);
      }
   }
}

void
coot::rama_plot::plot_type_changed() {

   if (gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(rama_radiomenuitem))) {
      // rama plot, i.e. hide the kleywegt box
      gtk_widget_hide(kleywegt_chain_box);
      if (mols().first) {
         // clear_canvas_items();
         set_kleywegt_plot_state(0);
         gtk_widget_show(rama_stats_frame);
         draw_it(mols().first);
      }
      else
         std::cout<< "BL INFO:: no molecule found, please read one in."<<std::endl;
   } else {
      // kleywegt plot
      gtk_widget_show(kleywegt_chain_box);
      gtk_widget_hide(rama_stats_frame);
      // either do a default kleywegt plot, or
      // dont do anything until things are selected and applied
      // better to do the latter. BUT what to do in the reverse direction?
      // should work with saved chains e.g. as well...

      // if not done, fill box with chains. How? Need to have a molecule...
      // should we save the molecule from the normal Rama?!
      // may be different in Coot compared to a stand alone version
      // FIXME:: very crude implementation, needs setting of chains! e.g.
      if (mols().first) {
         //clear_canvas_items();
         set_kleywegt_plot_state(1);
         std::vector<std::string> chains = coot::util::chains_in_molecule(mols().first);
         std::vector<std::string> chains2 = coot::util::chains_in_molecule(mols().second);
         int i_chain_id2 = 0;
         if (chains2.size() > 0)
            i_chain_id2 = 1;
         chain_ids_ = std::pair<std::string, std::string> (chains[0], chains2[i_chain_id2]);

         draw_it(molecule_numbers().first, molecule_numbers().second,
                 mols().first, mols().second,
                 chain_ids().first, chain_ids().second);
         fill_kleywegt_comboboxes(mols().first, mols().second);
      } else {
         std::cout<< "BL INFO:: no molecule found, please read one in."<<std::endl;
      }

   }
}

void
coot::rama_plot::fill_kleywegt_comboboxes(int imol) {

   // not for stand alone...
//   std::vector<std::vector<std::string> > ncs_ghost_chains =
//               graphics_info_t::molecules[imol].ncs_ghost_chains();

}

void
coot::rama_plot::fill_kleywegt_comboboxes(mmdb::Manager *mol1) {

   std::string r("no-chain");
   std::vector<std::string> chains = coot::util::chains_in_molecule(mol1);

   GtkListStore    *store, *store2;
   GtkTreeIter      iter, iter2;
   GtkCellRenderer *cell, *cell2;

   store = gtk_list_store_new( 1, G_TYPE_STRING );
   store2 = gtk_list_store_new( 1, G_TYPE_STRING );

   for (unsigned int i=0; i<chains.size(); i++) {
      gtk_list_store_append( store, &iter );
      gtk_list_store_set( store, &iter, 0, chains[i].c_str(), -1 );
      gtk_list_store_append( store2, &iter2 );
      gtk_list_store_set( store2, &iter2, 0, chains[i].c_str(), -1 );
   }
   gtk_combo_box_set_model(GTK_COMBO_BOX(kleywegt_chain_combobox1), GTK_TREE_MODEL(store));
   g_object_unref(G_OBJECT(store));
   cell = gtk_cell_renderer_text_new();
   gtk_cell_layout_pack_start( GTK_CELL_LAYOUT(kleywegt_chain_combobox1), cell, TRUE );
   gtk_cell_layout_set_attributes( GTK_CELL_LAYOUT(kleywegt_chain_combobox1), cell, "text", 0, NULL );

   gtk_combo_box_set_model(GTK_COMBO_BOX(kleywegt_chain_combobox2), GTK_TREE_MODEL(store2));
   g_object_unref(G_OBJECT(store2));
   cell2 = gtk_cell_renderer_text_new();
   gtk_cell_layout_pack_start( GTK_CELL_LAYOUT(kleywegt_chain_combobox2), cell2, TRUE );
   gtk_cell_layout_set_attributes( GTK_CELL_LAYOUT(kleywegt_chain_combobox2), cell2, "text", 0, NULL );

   // FIXME:: just set active first and second.
   gtk_combo_box_set_active(GTK_COMBO_BOX(kleywegt_chain_combobox1), 0);
   gtk_combo_box_set_active(GTK_COMBO_BOX(kleywegt_chain_combobox1), 1);

}

void
coot::rama_plot::fill_kleywegt_comboboxes(mmdb::Manager *mol1,
                                          mmdb::Manager *mol2) {

   std::string r("no-chain");
   std::vector<std::string> chains = coot::util::chains_in_molecule(mol1);
   std::vector<std::string> chains2 = coot::util::chains_in_molecule(mol2);
   int active1 = 0;
   int active2 = 1;

   GtkListStore    *store, *store2;
   GtkTreeIter      iter, iter2;
   GtkCellRenderer *cell, *cell2;

   store = gtk_list_store_new( 1, G_TYPE_STRING );
   store2 = gtk_list_store_new( 1, G_TYPE_STRING );

   for (unsigned int i=0; i<chains.size(); i++) {
      gtk_list_store_append( store, &iter );
      gtk_list_store_set( store, &iter, 0, chains[i].c_str(), -1 );
      if (chains[i] == chain_ids().first) {
         active1 = i;
      }
   }
   for (unsigned int i=0; i<chains2.size(); i++) {
      gtk_list_store_append( store2, &iter2 );
      gtk_list_store_set( store2, &iter2, 0, chains2[i].c_str(), -1 );
      if (chains2[i] == chain_ids().second) {
         active2 = i;
      }
   }

   gtk_combo_box_set_model(GTK_COMBO_BOX(kleywegt_chain_combobox1), GTK_TREE_MODEL(store));
   g_object_unref(G_OBJECT(store));
   gtk_cell_layout_clear(GTK_CELL_LAYOUT(kleywegt_chain_combobox1));
   cell = gtk_cell_renderer_text_new();
   gtk_cell_layout_pack_start( GTK_CELL_LAYOUT(kleywegt_chain_combobox1), cell, TRUE );
   gtk_cell_layout_set_attributes( GTK_CELL_LAYOUT(kleywegt_chain_combobox1), cell, "text", 0, NULL );

   gtk_combo_box_set_model(GTK_COMBO_BOX(kleywegt_chain_combobox2), GTK_TREE_MODEL(store2));
   g_object_unref(G_OBJECT(store2));
   gtk_cell_layout_clear(GTK_CELL_LAYOUT(kleywegt_chain_combobox2));
   cell2 = gtk_cell_renderer_text_new();
   gtk_cell_layout_pack_start( GTK_CELL_LAYOUT(kleywegt_chain_combobox2), cell2, TRUE );
   gtk_cell_layout_set_attributes( GTK_CELL_LAYOUT(kleywegt_chain_combobox2), cell2, "text", 0, NULL );

   gtk_combo_box_set_active(GTK_COMBO_BOX(kleywegt_chain_combobox1), active1);
   gtk_combo_box_set_active(GTK_COMBO_BOX(kleywegt_chain_combobox2), active2);

}

void
coot::rama_plot::update_kleywegt_plot() {

   // get chains, imols, mols
   int imol1;
   int imol2;
   gchar *chain_id1;
   gchar *chain_id2;
   mmdb::Manager *mol1;
   mmdb::Manager *mol2;

   imol1 = molecule_numbers().first;
   imol2 = molecule_numbers().second;
   GtkTreeIter iter;
   GtkTreeModel *model;

   if( gtk_combo_box_get_active_iter( GTK_COMBO_BOX(kleywegt_chain_combobox1), &iter ) ) {
          /* Obtain data model from combo box. */
       model = gtk_combo_box_get_model(GTK_COMBO_BOX(kleywegt_chain_combobox1));

       /* Obtain string from model. */
       gtk_tree_model_get( model, &iter, 0, &chain_id1, -1 );
   }

   if( gtk_combo_box_get_active_iter( GTK_COMBO_BOX(kleywegt_chain_combobox2), &iter ) ) {
          /* Obtain data model from combo box. */
       model = gtk_combo_box_get_model(GTK_COMBO_BOX(kleywegt_chain_combobox2));

       /* Obtain string from model. */
       gtk_tree_model_get( model, &iter, 0, &chain_id2, -1 );
   }


   mol1 = mols().first;
   mol2 = mols().second;
   // remove the old plot - usualy done in draw_it anyway
   // clear_canvas_items();

   draw_it(imol1, imol2, mol1, mol2, chain_id1, chain_id2);
   g_free(chain_id1);
   g_free(chain_id2);

}

void
coot::rama_plot::debug() const { 

   std::cout << std::endl;
   std::cout << "ramadebug: imol is " << imol << std::endl;
   std::cout << "ramadebug: canvas is " << canvas << std::endl;
   std::cout << "ramadebug: green_box_item is " << green_box_item << std::endl;
   std::cout << "ramadebug: step is " << step << std::endl;
   std::cout << "ramadebug: is ifirst_res size " << ifirst_res.size() << std::endl;
   std::cout << "ramadebug: is phi_psi_sets.size " << phi_psi_model_sets.size() << std::endl;
   std::cout << "ramadebug: allow_seqnum_offset_flag " << allow_seqnum_offset_flag << std::endl;
   //    std::cout << "ramadebug: is " << << std::endl;
}

void
coot::rama_plot::show_outliers_only(mmdb::Manager *mol, int state) {
   // draw_it(mol, state);

   draw_outliers_only = state;
   // std::cout << " in show_outliers_only() with state " << state << std::endl;
   all_plot(clipper::Ramachandran::NonGlyPro5); // seems reasonable
}

void
coot::rama_plot::show_outliers_only(int state) {

   draw_outliers_only = state;
   // std::cout << " in show_outliers_only() with state " << state << std::endl;
   clear_canvas_items();
   // then we have to draw again - maybe could be done cleverer?! FIXME

   if (drawing_differences) {
      draw_phi_psi_differences();
   } else {
      draw_phi_psi_points();
   }

   // set the button (if not already done)
   if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(outliers_only_tooglebutton)) != state) {
      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(outliers_only_tooglebutton), state);
   }
   // same for menuitem
   if (gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(outliers_only_menuitem)) != state) {
      gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(outliers_only_menuitem), state);
   }

}


// Notes for the workings of editphipsi
// 
// So, the motion callback function calls map_mouse_pos for this object
// 
// We need also need button_press member function and button_release
// function (we don't have that currently) for use when we "drop" the
// drag.  Hmmm... maybe not, actually, we just leave it where it was
// because the position will be updated on mouse motion.  We need to
// check on mouse motion if the left-button is being pressed (as I
// guess we do in globject.cc's glarea_motion_notify().
//
// So, we *do* need a mechanism to update the moving_atoms in the
// graphics window to the current phi/psi position.  
// 
// We need to know what phi/psi is given the mouse position - I guess
// that there is a function for that already.
// 
// We need to spot when we are over the green box (perhaps with a
// larger "margin of error" than we have with conventional points)
// when the mouse has been clicked.
//
// I think communication between the rama_plot and the graphics can be
// via a phi_psi_t
// 

#endif // RAMA_PLOT
#endif // HAVE_GTK_CANVAS

