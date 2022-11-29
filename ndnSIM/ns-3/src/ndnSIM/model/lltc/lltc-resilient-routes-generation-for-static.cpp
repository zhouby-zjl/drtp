#include "lltc-resilient-routes-generation-for-static.hpp"

#include <iostream>
#include <algorithm>
#include <unordered_set>
#include "lltc-fibonacci-heap-key-funcs.hpp"
#include "lltc-config.hpp"

#include <iostream>
#include <fstream>
#include <regex>
#include <stdlib.h>
#include <unordered_map>
#include <cmath>
#include <sstream>


using namespace std;
using namespace lltc;
using namespace __gnu_cxx;

int ResilientRouteGenerationForStatic::_nextPathID = 0;

string trimstr_(string s){
  size_t n = s.find_last_not_of(" \r\n\t");
    if (n != string::npos){
        s.erase(n + 1, s.size() - n);
    }
    n = s.find_first_not_of(" \r\n\t");
    if (n != string::npos){
        s.erase(0, n);
    }
    return s;
}

vector<string>* split(string str, char splitter)
{
    vector<std::string>* res = new vector<std::string>();
    string s;
    stringstream ss(str);
    while (getline(ss, s, splitter))
    {
        res->push_back(s);
    }

    if (!str.empty() && str.back() == splitter)
    {
        res->push_back(s);
    }
    return res;
}

LltcResilientRouteVectors*
ResilientRouteGenerationForStatic::genResilientRoutesVectors(LltcGraph* g,
															int srcNodeId,
															LltcConfiguration* config) {
	unordered_map<int, ResilientRoutes*>* all_rrs = new unordered_map<int, ResilientRoutes*>;

	string rr_static_file = LltcConfig::RR_STATIC_FILE;
	ifstream ifs;
	ifs.open(rr_static_file.c_str());
	if (!ifs.is_open()) {
		cerr << "open file err" << endl;
		return NULL;
	}

	string line;
	int src = 0, dst = 0;
	ResilientRoutes* rr = NULL;
	Path* primaryPath = NULL;
	Path* rsp_path = NULL;
	unordered_map<int, vector<RedundantSubPath*>*>* hrsp = NULL;
	int total_pairs = 0;
	while (getline(ifs, line)) {
		line = trimstr_(line);
		if (line.size() == 0) {
			continue;
		}
		string c = line.substr(0, 1);
		if (c == "#") {
			vector<string>* pairStr = split(line.substr(1, line.length()), ',');
			src = atoi((*pairStr)[0].c_str());
			dst = atoi((*pairStr)[1].c_str());
			hrsp = new unordered_map<int, vector<RedundantSubPath*>*>();
			delete pairStr;
		} else if (c == "$") {
			primaryPath = convertToPath(line.substr(1, line.length()));

		} else if (c == "*") {
			vector<string>* parts = split(line.substr(1, line.length()), '|');
			int nodeIdOnPP = atoi((*parts)[0].c_str());
			rsp_path = convertToPath((*parts)[1]);
			delete parts;
			vector<RedundantSubPath*>* rsps = NULL;
			if (hrsp->find(nodeIdOnPP) == hrsp->end()) {
				rsps = new vector<RedundantSubPath*>;
				(*hrsp)[nodeIdOnPP] = rsps;
			} else {
				rsps = (*hrsp)[nodeIdOnPP];
			}
			RedundantSubPath* rsp = new RedundantSubPath(rsp_path, 0,
										rsp_path->nodeIds->size() - 1);
			vector<int>* rsp_nodeIds = rsp->path->nodeIds;
			double maxRetransDelay = 0;
			for (unsigned int j = 1; j < rsp_nodeIds->size(); ++j) {
				int node_cur = (*rsp_nodeIds)[j];
				int node_prev = (*rsp_nodeIds)[j - 1];
				LltcLink* e = g->getLinkByNodeIds(node_prev, node_cur);
				maxRetransDelay += (double) (e->abDelay_total + e->baDelay_total);
			}
			maxRetransDelay *= (double) (config->numRetransRequests);
			rsp->retransDelayInMultiTimes = maxRetransDelay;
			rsps->push_back(rsp);

		} else if (c == "-") {
			rr = new ResilientRoutes(primaryPath, hrsp);
			computeRetranTimes(g, rr);
			(*all_rrs)[src * 0xffff + dst] = rr;
			total_pairs++;
		}
	}

	LltcResilientRouteVectors* rrv = new LltcResilientRouteVectors(-1, total_pairs);
	rrv->all_rrs = all_rrs;
	return rrv;
}

void ResilientRouteGenerationForStatic::computeRetranTimes(LltcGraph* g, ResilientRoutes* rr) {
	Path* primaryPath = rr->primaryPath;
	unordered_map<int, vector<RedundantSubPath*>*>* hrsp = rr->getHRSP();
	vector<int>* nodeIds = primaryPath->nodeIds;
	size_t n = nodeIds->size();
	int maxCulQueueTimesInUs[n];
	for (size_t i = 0; i < n; ++i) maxCulQueueTimesInUs[i] = 0;

	(*rr->retransTimeouts)[0] = 0;
	int culQueueTimeInUs = 0;
	for (unsigned int i = 1; i < nodeIds->size(); ++i) {
		LltcLink* l = g->getLinkByNodeIds((*nodeIds)[i - 1], (*nodeIds)[i]);
		culQueueTimeInUs += g->getLinkDelayQueue(l, (*nodeIds)[i - 1], (*nodeIds)[i]);
		maxCulQueueTimesInUs[i] = culQueueTimeInUs;

		int nodeId = (*nodeIds)[i];
		vector<RedundantSubPath*>* rsps = (*hrsp)[nodeId];
		if (rsps == NULL || rsps->size() == 0) continue;
		int maxTimeoutForRetrans = -1;
		for (RedundantSubPath* rsp : *rsps) {
			if (rsp->retransDelayInMultiTimes > maxTimeoutForRetrans) {
				maxTimeoutForRetrans = rsp->retransDelayInMultiTimes;
			}
		}
		(*rr->retransTimeouts)[i] = maxTimeoutForRetrans;
	}

	(*rr->waitingTimeoutOnNextPacketInUs)[0] = 0;
	(*rr->waitingTimeoutOnNextPacketForLostReportInUs)[0] = 0;
	int culRetransTimeout = 0;
	int maxPrimPathDelays = 0;
	for (unsigned int i = 1; i < nodeIds->size(); ++i) {
		culRetransTimeout += (*rr->retransTimeouts)[i - 1];

		(*rr->waitingTimeoutOnNextPacketInUs)[i] = maxCulQueueTimesInUs[i];
		LltcLink* e = g->getLinkByNodeIds((*nodeIds)[i - 1], (*nodeIds)[i]);
		maxPrimPathDelays += e->abDelay_total;

		(*rr->waitingTimeoutOnNextPacketForLostReportInUs)[i] = maxPrimPathDelays +
											culRetransTimeout;
								//(double) ((*rr->retransTimeouts)[i]) * (1.0 / (double) config->numRetransRequests);

		int culRetransTimeoutAtNodeJ = 0;
		for (unsigned int j = i - 1; j >= 1; --j) {
			culRetransTimeoutAtNodeJ += (*rr->retransTimeouts)[j];
			(*(rr->waitingTimeoutOnRetransReportInUs[i]))[(*nodeIds)[j]] = culRetransTimeoutAtNodeJ +
								maxCulQueueTimesInUs[i] - maxCulQueueTimesInUs[j];
		}
	}

}

Path* ResilientRouteGenerationForStatic::convertToPath(string nodeIdsStr) {
	vector<string>* nodeIds = split(nodeIdsStr, ',');
	Path* p = new Path;
	p->setLength((*nodeIds).size());
	p->pathID = ResilientRouteGenerationForStatic::_nextPathID++;
	for (unsigned int i = 0; i < nodeIds->size(); ++i) {
		(*p->nodeIds)[i] = atoi((*nodeIds)[i].c_str());
	}
	delete nodeIds;
	return p;
}

ResilientRoutes*
ResilientRouteGenerationForStatic::constructResilientRoutes(LltcGraph* g,
										LltcResilientRouteVectors* rrv, int dstNodeId,
										LltcConfiguration* config) {
	unordered_map<int, ResilientRoutes*>* all_rrs = rrv->all_rrs;

	if (all_rrs->find(dstNodeId) != all_rrs->end()) {
		return (*all_rrs)[dstNodeId];
	}
	return NULL;
}

double ResilientRouteGenerationForStatic::computeResilientRouteEEDT_N_InStep(LltcGraph* g, ResilientRoutes* rr, LltcConfiguration* config) {
	int nHopPrimPath = rr->primaryPath->nodeIds->size();
	double delay = computePmuPiatDriftRange(config->pmuFreq, config->maxDriftRatioForPmuFreq, config->maxConsecutiveDriftPackets);

	for (int i = 1; i < nHopPrimPath; ++i) {
		int nodeA_id = (*(rr->primaryPath->nodeIds))[i - 1];
		int nodeB_id = (*(rr->primaryPath->nodeIds))[i];
		LltcLink* l = g->getLinkByNodeIds(nodeA_id, nodeB_id);

		int primLinkDelay_down = g->getLinkDelayTotal(l, nodeA_id, nodeB_id);
		int primLinkQueueDelay_down = g->getLinkDelayQueue(l, nodeA_id, nodeB_id);

		int maxTimeoutForRetransInUs = (l->abDelay_total + l->baDelay_total) * (config->numRetransRequests - 1);
		vector<RedundantSubPath*>* X = rr->getRSPsByNodeId(nodeB_id);
		if (X != NULL && X->size() > 1) {
			for (unsigned int s = 0; s < X->size(); ++s) {
				RedundantSubPath* sp = (*X)[s];
				if (sp->retransDelayInMultiTimes > maxTimeoutForRetransInUs) {
					maxTimeoutForRetransInUs = sp->retransDelayInMultiTimes;
				}
			}
		}

		delay += primLinkDelay_down + primLinkQueueDelay_down + maxTimeoutForRetransInUs;
	}

	return delay;
}

double ResilientRouteGenerationForStatic::computePmuPiatDriftRange(double freq, double freqDriftRatio, int maxConsecutiveDriftPackets) {
	return  (double) maxConsecutiveDriftPackets * 2 * freqDriftRatio * (1000000.0 / freq) / (1 - freqDriftRatio * freqDriftRatio);
}
