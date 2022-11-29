/*
 * lltc-resilient-routes-generation-for-reroute.hpp
 *
 *  Created on: 2022年9月19日
 *      Author: zby
 */

#ifndef SRC_NDNSIM_MODEL_LLTC_LLTC_RESILIENT_ROUTES_GENERATION_FOR_DRTP_SIMPLE_HPP_
#define SRC_NDNSIM_MODEL_LLTC_LLTC_RESILIENT_ROUTES_GENERATION_FOR_DRTP_SIMPLE_HPP_



#include "ns3/ndnSIM/model/lltc/lltc-resilient-routes-generation.hpp"

using namespace std;

namespace lltc {

class ResilientRouteGenerationForDrtpSimple : public ResilientRouteGenerationGeneric {
public:
	LltcResilientRouteVectors* genResilientRoutesVectors(LltcGraph* g, int srcNodeId, LltcConfiguration* config);
	ResilientRoutes* constructResilientRoutes(LltcGraph* g, LltcResilientRouteVectors* rrv, int dstNodeId,
																LltcConfiguration* config);

private:
	int* genPathDijkstra(LltcGraph* g, int n, int srcNodeId,
			unordered_set<int>* invalidLinkIDsInPP, unordered_set<int>* invalidLinkIDsInRSPs);


	void computeRetranTimes(LltcGraph* g, ResilientRoutes* rr);

	Path* convertPrevToPath(int* prevNodeIdx, int n,
								int srcNodeId, int dstNodeId, bool dropLast);

	double computePmuPiatDriftRange(double freq, double freqDriftRatio, int maxConsecutiveDriftPackets);
	double computeResilientRouteEEDT_N_InStep(LltcGraph* g, ResilientRoutes* rr, LltcConfiguration* config);

	void addPathNodeIdsArrayToLinkIdsSet(LltcGraph* g, vector<int>* pathNodeIds, unordered_set<int>* linkIdSet);

	static int _nextPathID;
};

}



#endif /* SRC_NDNSIM_MODEL_LLTC_LLTC_RESILIENT_ROUTES_GENERATION_FOR_DRTP_SIMPLE_HPP__ */
