/*
 * This work is licensed under CC BY-NC-SA 4.0
 * (https://creativecommons.org/licenses/by-nc-sa/4.0/).
 * Copyright (c) 2021 Boyang Zhou
 *
 * This file is a part of "Disruption Resilient Transport Protocol"
 * (https://github.com/zhouby-zjl/drtp/).
 * Written by Boyang Zhou (zhouby@zhejianglab.com)
 *
 * This software is protected by the patents numbered with PCT/CN2021/075891,
 * ZL202110344405.7 and ZL202110144836.9, as well as the software copyrights
 * numbered with 2020SR1875227 and 2020SR1875228.
 */

#include "ns3/ndnSIM/model/lltc/lltc-resilient-routes-generation-for-drtp.hpp"

#include "ns3/nstime.h"
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/point-to-point-layout-module.h"
#include "ns3/ndnSIM-module.h"

#include "ns3/ndnSIM/helper/ndn-stack-helper.hpp"
#include "ns3/ndnSIM/helper/ndn-app-helper.hpp"
#include "ns3/ndnSIM/helper/lltc/lltc-routing-helper.hpp"
#include "ns3/ndnSIM/NFD/daemon/fw/lltc-strategy.hpp"
#include "ns3/ndnSIM/NFD/daemon/fw/lltc-strategy-config.hpp"
#include "ns3/ndnSIM/NFD/daemon/fw/forwarder.hpp"
#include "ns3/ndnSIM/model/ndn-l3-protocol.hpp"
#include "ns3/ndnSIM/ndn-cxx/name-component.hpp"
#include "ns3/ndnSIM/ndn-cxx/name.hpp"
#include "ns3/ndnSIM/helper/ndn-link-control-helper.hpp"

#include "ns3/ndnSIM/apps/ndn-app.hpp"
#include "ns3/ndnSIM/apps/lltc-pdc.hpp"
#include "ns3/ndnSIM/apps/lltc-pmu.hpp"

#include "ns3/ndnSIM/model/lltc/lltc-config.hpp"
#include "ns3/ndnSIM/model/lltc/lltc-utils.hpp"

#include <stdlib.h>
#include <fstream>
#include <random>
#include <time.h>

using namespace lltc;
using namespace ns3;
using namespace ns3::ndn;


vector<int>* genRandomPeers(int n_peers, int n_nodes, int srcNodeId) {
	vector<int>* r = new vector<int>();
	srand(20);

	unordered_set<int> peer_ids;
	int n_peers_ = n_peers > n_nodes - 1 ? n_nodes - 1 : n_peers;
	for (int i = 0; i < n_peers_; ++i) {
		while (true) {
			int x = rand() % n_nodes;
			if (peer_ids.find(x) == peer_ids.end() && x != srcNodeId) {
				peer_ids.insert(x);
				r->push_back(x);
				break;
			}

		}
	}
	return r;
}

int main(int argc, char** argv) {
	if (argc != 6) {
		cerr << "Usage: " << argv[0] << " [config-file-path] [src-node-id] [n-peers] [beta] [output-file]" << endl;
		return -1;
	}
	const char* configFilePath = argv[1];
	if (!LltcConfig::loadConfigFile(configFilePath)) {
		cerr << "Error to load the configuration file. Please Check." << endl;
		return -1;
	}
	int srcNodeId = atoi(argv[2]);
	int n_peers = atoi(argv[3]);
	int beta = atoi(argv[4]);
	string output_file = string(argv[5]);

	int NODE_ID_PMUS[1];
	NODE_ID_PMUS[0] = LltcConfig::NETWORK_NODE_ID_PMU_STATIC;

	// PMU terminal configuration
	LltcConfiguration* config = new LltcConfiguration(LltcConfig::LLTC_NUM_RETRANS_REQUESTS, LltcConfig::LLTC_BETA,
			LltcConfig::NETWORK_PMU_DATA_FREQ, LltcConfig::LLTC_MAX_DRIFT_RANGE_RATIO_FOR_PMU_FREQ,
			LltcConfig::LLTC_MAX_CONSECUTIVE_DRIFT_PACKETS, LltcConfig::LLTC_MAX_PATH_DELAY_US,
			LltcConfig::LLTC_NUM_DATA_RETRANS_REPORTS_TO_SEND);

	LltcGraph* g = new LltcGraph(LltcConfig::NETWORK_NODE_PROCESS_DELAY_US, LltcConfig::NETWORK_LINK_QUEUE_DELAY_US,
								LltcConfig::NETWORK_LINK_TRANS_DELAY_US);
	string filePath = LltcConfig::NETWORK_TOPO_FILE;
	bool succ = false;
	if (LltcConfig::NETWORK_TOPO_FILE_TYPE.compare("IEEE") == 0) {
		succ = g->loadFromIEEEDataFile(&filePath, false, LltcConfig::LLTC_ASSUMED_LINK_RELIA_RATE, 0);
	} else if (LltcConfig::NETWORK_TOPO_FILE_TYPE.compare("CSV") == 0) {
		succ = g->loadFromCSVDataFile(&filePath, false, LltcConfig::LLTC_ASSUMED_LINK_RELIA_RATE, 0);
	}
	if (!succ) return -1;

	CommandLine cmd;
	cmd.Parse(argc, argv);
	int n_nodes = g->getNNodes();

	ofstream* logOut = new ofstream;
	logOut->open(output_file, ios::trunc);

	ResilientRouteGenerationForDRTP* rrg = new ResilientRouteGenerationForDRTP();

	vector<int>* peers = genRandomPeers(n_peers, n_nodes, srcNodeId);

	config->beta = beta;

	clock_t startTime = clock();
	LltcResilientRouteVectors* rrv = rrg->genResilientRoutesVectors(g, srcNodeId, config);
	uint64_t exeTime = clock() - startTime;
	if (rrv == NULL)  {
		cout << "computation for beta = " << beta << " has been failed, no path exists" << endl;
		return -1;
	}
	for (vector<int>::iterator iter = peers->begin(); iter != peers->end(); ++iter) {
		int peerNodeID = *iter;

		ResilientRoutes* rr = rrg->constructResilientRoutes(g, rrv, peerNodeID, config);
		unordered_map<int, vector<RedundantSubPath*>*>* hrsp = rr->getHRSP();
		int pp_len = 0;
		vector<size_t> nRSPs_in_pp;
		vector<size_t> RSP_len_in_pp;
		for (unordered_map<int, vector<RedundantSubPath*>*>::iterator iter2 = hrsp->begin();
				iter2 != hrsp->end(); ++iter2) {
			++pp_len;
			size_t nRSPs = iter2->second->size();
			nRSPs_in_pp.push_back(nRSPs);
			for (vector<RedundantSubPath*>::iterator iter3 = iter2->second->begin();
					iter3 != iter2->second->end(); ++iter3) {
				RSP_len_in_pp.push_back((*iter3)->path->nodeIds->size());
			}
		}

		*logOut << srcNodeId << "," << peerNodeID << "," << beta << ","
				<< pp_len << "," << rr->expectedRrLossRate << "," << rr->maxRrDelay << "," << exeTime << "|";
		for (int i = 0; i < nRSPs_in_pp.size(); ++i) {
			*logOut << nRSPs_in_pp[i] << (i == (nRSPs_in_pp.size() - 1) ? "" : ",");
		}
		*logOut << "|";

		for (int i = 0; i < RSP_len_in_pp.size(); ++i) {
			*logOut << RSP_len_in_pp[i] << (i == (RSP_len_in_pp.size() - 1) ? "" : ",");
		}
		*logOut << endl;

		logOut->flush();


	}

	logOut->close();

	cout << "computation for beta = " << beta << " has finished" << endl;

	return 0;
}
