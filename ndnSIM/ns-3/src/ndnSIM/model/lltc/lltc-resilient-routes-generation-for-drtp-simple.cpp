#include "lltc-resilient-routes-generation-for-drtp-simple.hpp"

#include <iostream>
#include <algorithm>
#include <unordered_set>
#include "lltc-fibonacci-heap-key-funcs.hpp"

using namespace std;
using namespace lltc;

int ResilientRouteGenerationForDrtpSimple::_nextPathID = 0;

LltcResilientRouteVectors* ResilientRouteGenerationForDrtpSimple::genResilientRoutesVectors(LltcGraph* g,
																						int srcNodeId,
																						LltcConfiguration* config) {
	unordered_map<int, ResilientRoutes*>* all_rrs = new unordered_map<int, ResilientRoutes*>;
	int n = g->getNNodes();
	int* prevNodeIdx = genPathDijkstra(g, n, srcNodeId, NULL, NULL);
	Path* pp;
	Path* rsp_path;
	RedundantSubPath* rsp;
	unordered_map<int, vector<RedundantSubPath*>*>* hrsp;
	LltcLink* l_rsp;
	int curNode, prevNode;
	ResilientRoutes* rr = NULL;
	int total_pairs = 0;
	LltcNode* virDstNode;
	int* rsp_prevNodeIdx;
	int nodeA, nodeB;

	for (int dstNodeId = 0; dstNodeId < n; ++dstNodeId) {
		if (dstNodeId == srcNodeId) continue;
		pp = convertPrevToPath(prevNodeIdx, n, srcNodeId, dstNodeId, false);
		if (pp == NULL) continue;

		hrsp = new unordered_map<int, vector<RedundantSubPath*>*>();

		int pp_n = pp->nodeIds->size();
		vector<int> prevNodeIdsInPP;
		unordered_set<int> invalidLinksInPP;

		addPathNodeIdsArrayToLinkIdsSet(g, pp->nodeIds, &invalidLinksInPP);

		for (int i = 1; i < pp_n; ++i) {
			curNode = (*pp->nodeIds)[i];
			prevNode = (*pp->nodeIds)[i - 1];
			prevNodeIdsInPP.push_back(prevNode);

			unordered_set<int> invalidLinksInRSPs;
			for (int k = 0; k < config->beta; ++k) {
				virDstNode = g->addVirtNode(&prevNodeIdsInPP);
				rsp_prevNodeIdx = genPathDijkstra(g, n + 1, curNode, &invalidLinksInPP, &invalidLinksInRSPs);
				rsp_path = convertPrevToPath(rsp_prevNodeIdx, n, curNode, virDstNode->nodeId, true);
				if (rsp_path != NULL) {
					vector<RedundantSubPath*>* rsps = NULL;
					if (hrsp->find(curNode) == hrsp->end()) {
						rsps = new vector<RedundantSubPath*>;
						(*hrsp)[curNode] = rsps;
					} else {
						rsps = (*hrsp)[curNode];
					}
					rsp = new RedundantSubPath(rsp_path, 0, rsp_path->nodeIds->size() - 1);
					rsp->linkUpDelay = 0;
					rsp->linkDownDelay = 0;
					for (uint32_t m = 1; m < rsp_path->nodeIds->size(); ++m) {
						nodeA = (*rsp_path->nodeIds)[m - 1];
						nodeB = (*rsp_path->nodeIds)[m];
						l_rsp = g->getLinkByNodeIds(nodeA, nodeB);
						rsp->linkUpDelay += g->getLinkDelayTotal(l_rsp, nodeA, nodeB);
						rsp->linkDownDelay += g->getLinkDelayTotal(l_rsp, nodeB, nodeA);
					}

					rsp->retransDelayInMultiTimes = (rsp->linkUpDelay + rsp->linkDownDelay) *
													config->numRetransRequests;
					rsp->retransDelayInSingleTime = rsp->retransDelayInMultiTimes / config->numRetransRequests;

					rsps->push_back(rsp);

					addPathNodeIdsArrayToLinkIdsSet(g, rsp_path->nodeIds, &invalidLinksInRSPs);
				}

				g->removeVirtNode();

				if (rsp_path == NULL) {
					break;
				}
			}

			if (hrsp->find(curNode) == hrsp->end()) {
				(*hrsp)[curNode] = new vector<RedundantSubPath*>;
			}
		}

		rr = new ResilientRoutes(pp, hrsp);
		computeRetranTimes(g, rr);
		rr->maxRrDelay = computeResilientRouteEEDT_N_InStep(g, rr, config);
		(*all_rrs)[dstNodeId] = rr;
		total_pairs++;
	}

	LltcResilientRouteVectors* rrv = new LltcResilientRouteVectors(srcNodeId, total_pairs);
	rrv->all_rrs = all_rrs;

	return rrv;
}

double ResilientRouteGenerationForDrtpSimple::computeResilientRouteEEDT_N_InStep(LltcGraph* g, ResilientRoutes* rr, LltcConfiguration* config) {
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

double ResilientRouteGenerationForDrtpSimple::computePmuPiatDriftRange(double freq, double freqDriftRatio, int maxConsecutiveDriftPackets) {
	return  (double) maxConsecutiveDriftPackets * 2 * freqDriftRatio * (1000000.0 / freq) / (1 - freqDriftRatio * freqDriftRatio);
}

void ResilientRouteGenerationForDrtpSimple::computeRetranTimes(LltcGraph* g, ResilientRoutes* rr) {
	Path* primaryPath = rr->primaryPath;
	unordered_map<int, vector<RedundantSubPath*>*>* hrsp = rr->getHRSP();
	vector<int>* nodeIds = primaryPath->nodeIds;
	size_t n = nodeIds->size();
	int maxCulQueueTimesInUs[n];
	for (size_t i = 0; i < n; ++i) maxCulQueueTimesInUs[i] = 0;

	(*rr->retransTimeouts)[0] = 0;
	int culQueueTimeInUs = 0;
	rr->primPathDelay = 0;
	for (unsigned int i = 1; i < nodeIds->size(); ++i) {
		LltcLink* l = g->getLinkByNodeIds((*nodeIds)[i - 1], (*nodeIds)[i]);
		culQueueTimeInUs += g->getLinkDelayQueue(l, (*nodeIds)[i - 1], (*nodeIds)[i]);
		rr->primPathDelay += g->getLinkDelayTotal(l, (*nodeIds)[i - 1], (*nodeIds)[i]);
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

int* ResilientRouteGenerationForDrtpSimple::genPathDijkstra(LltcGraph* g, int n, int srcNodeId,
										unordered_set<int>* invalidLinkIDsInPP, unordered_set<int>* invalidLinkIDsInRSPs) {
	int* prevNodeIdx = new int[n];
	bool isNodeIdInQ[n];
	int* delays = new int[n];

	for (int i = 0; i < n; ++i) {
		prevNodeIdx[i] = -1;
		isNodeIdInQ[i] = true;
		delays[i] = INT_MAX;
	}
	delays[srcNodeId] = 0;

	FHKeyFuncDelay keyFunc(delays);
	FibonacciHeap q(&keyFunc);

	for (int i = 0; i < n; ++i) {
		q.insert((FHObjectPtr) g->nodes[i]);
	}

	while (q.size() > 0) {
		int uNodeId = *((int*) (q.removeMax()->getNodeObject()));
		LltcNode* uNode = g->getNodeById(uNodeId);
		isNodeIdInQ[uNode->nodeId] = false;

		for (unsigned int i = 0; i < uNode->eLinks->size(); ++i) {
			LltcLink* e = (*(uNode->eLinks))[i];
			if (invalidLinkIDsInPP != NULL &&
					invalidLinkIDsInPP->find(e->linkId) != invalidLinkIDsInPP->end()) continue;
			if (invalidLinkIDsInRSPs != NULL &&
					invalidLinkIDsInRSPs->find(e->linkId) != invalidLinkIDsInRSPs->end()) continue;
			LltcNode* vNode = LltcGraph::getLinkOpSide(e, uNode);
			int vNodeId = vNode->nodeId;
			if (!isNodeIdInQ[vNodeId]) continue;

			int overV = e->abDelay_total + delays[uNode->nodeId];
			if (overV < delays[vNodeId]) {
				prevNodeIdx[vNodeId] = uNode->nodeId;
				delays[vNodeId] = overV;
				q.updateKey((FHObjectPtr) &(vNode->nodeId), (FHObjectPtr) &(vNode->nodeId));
			}
		}
	}

	return prevNodeIdx;
}

Path* ResilientRouteGenerationForDrtpSimple::convertPrevToPath(int* prevNodeIdx, int n,
							int srcNodeId, int dstNodeId, bool dropLast) {
	int pathNodes = 1;
	int v = dstNodeId;
	while (v != srcNodeId) {
		v = prevNodeIdx[v];
		if (v == -1) break;
		++pathNodes;
	}

	Path* path = new Path;
	path->setLength(dropLast ? pathNodes - 1 : pathNodes);
	path->pathID = ResilientRouteGenerationForDrtpSimple::_nextPathID++;

	v = dstNodeId;
	int p = pathNodes - 1;
	do {
		if (dropLast && p < pathNodes - 1) {
			(*path->nodeIds)[p] = v;
		} else if (!dropLast) {
			(*path->nodeIds)[p] = v;
		}
		v = prevNodeIdx[v];
		--p;
	} while (p >= 0 && v != srcNodeId);

	if (p == -1 && v != srcNodeId) {
		delete path;
		return NULL;
	}

	if (pathNodes >= 2)
		(*path->nodeIds)[0] = srcNodeId;
	return path;
}


ResilientRoutes* ResilientRouteGenerationForDrtpSimple::constructResilientRoutes(LltcGraph* g,
		LltcResilientRouteVectors* rrv, int dstNodeId,
		LltcConfiguration* config) {

	unordered_map<int, ResilientRoutes*>* all_rrs = rrv->all_rrs;

	if (all_rrs->find(dstNodeId) != all_rrs->end()) {
		return (*all_rrs)[dstNodeId];
	}
	return NULL;
}


void ResilientRouteGenerationForDrtpSimple::addPathNodeIdsArrayToLinkIdsSet(LltcGraph* g,
								vector<int>* pathNodeIds, unordered_set<int>* linkIdSet) {
	for (unsigned int i = 1; i < pathNodeIds->size(); ++i) {
		int linkId = g->getLinkByNodeIds((*pathNodeIds)[i], (*pathNodeIds)[i - 1])->linkId;
		linkIdSet->insert(linkId);
	}
}
