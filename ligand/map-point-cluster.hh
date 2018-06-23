

#ifndef MAP_POINT_CLUSTER_HH
#define MAP_POINT_CLUSTER_HH

namespace coot {

   class map_point_cluster {
   public:
      map_point_cluster() { score = 0.0;}; 
      std::vector<clipper::Coord_grid> map_grid;
      float score;
      // clipper::Coord_orth centre;
      // clipper::Mat33<double> eigenvectors;
      clipper::Coord_orth std_dev;
      clipper::RTop_orth eigenvectors_and_centre;
      std::vector<double> eigenvalues;
      bool operator==(const map_point_cluster &mpc) const {
	 return (mpc.map_grid == map_grid);
      }
      double volume(const clipper::Xmap<float> &xmap_ref) const;
   }; 

   bool compare_clusters(const map_point_cluster &a,
			 const map_point_cluster &b);

}

#endif // MAP_POINT_CLUSTER_HH
