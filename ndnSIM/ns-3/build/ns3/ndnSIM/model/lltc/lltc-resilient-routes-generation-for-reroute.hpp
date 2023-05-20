/*
 * lltc-resilient-routes-generation-for-reroute.hpp
 *
 *  Created on: 2022年9月19日
 *      Author: zby
 */

#ifndef SRC_NDNSIM_MODEL_LLTC_LLTC_RESILIENT_ROUTES_GENERATION_FOR_REROUTE_HPP_
#define SRC_NDNSIM_MODEL_LLTC_LLTC_RESILIENT_ROUTES_GENERATION_FOR_REROUTE_HPP_



#include "ns3/ndnSIM/model/lltc/lltc-resilient-routes-generation.hpp"

using namespace std;

namespace lltc {

class ResilientRouteGenerationForReroute : public ResilientRouteGenerationGeneric {
public:
	LltcResilientRouteVectors* genResilientRoutesVectors(LltcGraph* g, int srcNodeId, LltcConfiguration* config);
	ResilientRoutes* constructResilientRoutes(LltcGraph* g, LltcResilientRouteVectors* rrv, int dstNodeId,
																LltcConfiguration* config);

private:
	vector<RedundantSubPath*>* genRedundantSubPaths(LltcGraph* g, int srcNodeId, vector<int>* dstNodeIds,
					unordered_set<int>* invalidLinkIds, LltcConfiguration* config, double maxAllowedDelay);

	RedundantSubPath* genPath(LltcGraph* g, int srcNodeId, vector<int>* dstNodeIds,
			unordered_set<int>* expelledLinkIds, unordered_set<int>* invalidLinkIds,
			LltcConfiguration* config, double maxAllowedDelay);

	vector<int>* convertPrevArrayToPathNodeIdsArray(int* prevNodeIdx, int srcNodeId, int uNodeId);
	unordered_set<int>* convertPathNodeIdsArrayToLinkIdsSet(LltcGraph* g, vector<int>* retransDstNodeIds);

	void convertPathNodeIdsArrayToLinkIdsSet(LltcGraph* g, vector<int>* retransDstNodeIds, unordered_set<int>* output);

	bool isInArray(vector<RedundantSubPath*>* rsps, RedundantSubPath* rsp);

	static int _nextPathID;
};

}



#endif /* SRC_NDNSIM_MODEL_LLTC_LLTC_RESILIENT_ROUTES_GENERATION_FOR_REROUTE_HPP_ */
