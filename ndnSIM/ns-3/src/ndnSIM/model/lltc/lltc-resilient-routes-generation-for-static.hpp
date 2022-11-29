/*
 * lltc-resilient-routes-generation-for-static.hpp
 *
 *  Created on: 2022年11月4日
 *      Author: zby
 */

#ifndef SRC_NDNSIM_MODEL_LLTC_LLTC_RESILIENT_ROUTES_GENERATION_FOR_STATIC_HPP_
#define SRC_NDNSIM_MODEL_LLTC_LLTC_RESILIENT_ROUTES_GENERATION_FOR_STATIC_HPP_


#include "ns3/ndnSIM/model/lltc/lltc-resilient-routes-generation.hpp"
#include <unordered_map>

using namespace std;

namespace lltc {

class ResilientRouteGenerationForStatic : public ResilientRouteGenerationGeneric {
public:
	LltcResilientRouteVectors* genResilientRoutesVectors(LltcGraph* g, int srcNodeId, LltcConfiguration* config);
	ResilientRoutes* constructResilientRoutes(LltcGraph* g, LltcResilientRouteVectors* rrv, int dstNodeId,
																LltcConfiguration* config);

	double computeResilientRouteEEDT_N_InStep(LltcGraph* g,
									ResilientRoutes* rr, LltcConfiguration* config);
	double computePmuPiatDriftRange(double freq,
									double freqDriftRatio, int maxConsecutiveDriftPackets);

private:
	Path* convertToPath(string nodeIdsStr);
	void computeRetranTimes(LltcGraph* g, ResilientRoutes* rr);

	static int _nextPathID;
};

}


#endif /* SRC_NDNSIM_MODEL_LLTC_LLTC_RESILIENT_ROUTES_GENERATION_FOR_STATIC_HPP_ */
