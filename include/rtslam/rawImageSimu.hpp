/**
 *
 *  \file rawImageSimu.hpp
 *
 *  Header file for simulated image
 *
 * \date 14/03/2010
 * \author jmcodol@laas.fr
 *
 * \ingroup rtslam
 */

#ifndef RAWIMAGESIMU_HPP_
#define RAWIMAGESIMU_HPP_

#include "rtslam/rawAbstract.hpp"
#include "rtslam/featurePointSimu.hpp"
#include "image/Image.hpp"
#include "boost/shared_ptr.hpp"

namespace jafar {
	namespace rtslam {
		using namespace std;
		using namespace jafar::image;

		class RawImageSimu;
		typedef boost::shared_ptr<RawImageSimu> rawimagesimu_ptr_t;

		/**
		 * Class of simulated image
		 * \author jmcodol
		 * \ingroup rtslam
		 */
		class RawImageSimu: public RawAbstract {

			private:
				map<int,FeaturePointSimu*> features;

			public:

				/**
				 * Constructor
				 */
				RawImageSimu() :
					RawAbstract() {
				}

				void setImage(jafar::image::Image* img){
					this->image = img;
				}

				int coord;
				int app  ;
				Image* image;

				/**
				 * Add a point-typed feature on a pixel.
				 */
				 void addFeature(const FeatureAbstract);
				 friend ostream& operator <<(ostream & s, jafar::rtslam::RawImageSimu& rawA);

			private:

		};
	}
}

#endif /* RAWIMAGESIMU_HPP_ */
