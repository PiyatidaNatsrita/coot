
#include "loop-path.hh"
#include "coot-utils/coot-coord-utils.hh"

std::vector<coot::CartesianPair>
coot::loop_path(mmdb::Atom *start_back_2,
		mmdb::Atom *start,
		mmdb::Atom *end,
		mmdb::Atom *end_plus_2,
		unsigned int n_line_segments) {

   std::vector<CartesianPair> loop_line_segments;

   if (! start_back_2) return loop_line_segments;
   if (! start) return loop_line_segments;
   if (! end) return loop_line_segments;
   if (! end_plus_2) return loop_line_segments;

   // sane input

   clipper::Coord_orth P0 = co(start_back_2);
   clipper::Coord_orth P1 = co(start);
   clipper::Coord_orth P4 = co(end);
   clipper::Coord_orth P5 = co(end_plus_2);

   // Now make P2 = P1 + s(P1 - P0);
   // Now make P3 = P4 + s(P4 - P5);

   double d2 = clipper::Coord_orth(P1-P4).lengthsq();
   double d = 0.3 * sqrt(d2); // this number could be optimized
   if (d<0.5) d = 0.5; // and this one
   if (d>3.0) d = 3.0; // and this one

   double s = sqrt(d);
   clipper::Coord_orth P2 = P1 + s * P1 - s * P0;
   clipper::Coord_orth P3 = P4 + s * P4 - s * P5;

   if (false) {
      std::cout << "oo = new_generic_object_number('points')" << std::endl;
      std::cout << "to_generic_object_add_point(oo, \"green\", 4, " << P1.x() << ", " << P1.y() << ", " << P1.z() << ")" << std::endl;
      std::cout << "to_generic_object_add_point(oo, \"green\", 4, " << P2.x() << ", " << P2.y() << ", " << P2.z() << ")" << std::endl;
      std::cout << "to_generic_object_add_point(oo, \"green\", 4, " << P3.x() << ", " << P3.y() << ", " << P3.z() << ")" << std::endl;
      std::cout << "to_generic_object_add_point(oo, \"green\", 4, " << P4.x() << ", " << P4.y() << ", " << P4.z() << ")" << std::endl;
      std::cout << "set_display_generic_object(oo, 1)" << std::endl;
   }

   unsigned int n_pts = 2 * n_line_segments;

   for (unsigned int i=0; i<n_pts; i+=2) {
      double t = static_cast<float>(i)/static_cast<float>(n_pts);
      clipper::Coord_orth comp_1 = (1.0-t)*(1.0-t)*(1.0-t)*P1;
      clipper::Coord_orth comp_2 = 3.0*(1.0-t)*(1.0-t)*t*P2;
      clipper::Coord_orth comp_3 = 3.0*(1.0-t)*t*t*P3;
      clipper::Coord_orth comp_4 = t*t*t*P4;
      clipper::Coord_orth ls_start = comp_1 + comp_2 + comp_3 + comp_4;
      t = static_cast<float>(i+1)/static_cast<float>(n_pts);
      comp_1 = (1.0-t)*(1.0-t)*(1.0-t)*P1;
      comp_2 = 3.0*(1.0-t)*(1.0-t)*t*P2;
      comp_3 = 3.0*(1.0-t)*t*t*P3;
      comp_4 = t*t*t*P4;
      clipper::Coord_orth ls_end = comp_1 + comp_2 + comp_3 + comp_4;
      Cartesian ls_start_c(ls_start);
      Cartesian ls_end_c(ls_end);
      loop_line_segments.push_back(CartesianPair(ls_start_c, ls_end_c));
   }
   return loop_line_segments;
}
