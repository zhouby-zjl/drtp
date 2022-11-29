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

#include "ns3/ndnSIM/model/lltc/lltc-resilient-routes-generation.hpp"

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

class PcapWriter {
public:
	PcapWriter(const std::string& file) {
		PcapHelper helper;
		m_pcap = helper.CreateFile(file, std::ios::out, PcapHelper::DLT_PPP);
	}

	void
	TracePacket(Ptr<const Packet> packet)
	{
		static PppHeader pppHeader;
		pppHeader.SetProtocol(0x0077);

		m_pcap->Write(Simulator::Now(), pppHeader, packet);
	}

private:
	Ptr<PcapFileWrapper> m_pcap;
};

void setLinksErrorRatesTask(vector<LltcLink*>* links, NodeContainer* nodes, double errRate) {
	for (std::vector<LltcLink*>::iterator iter = links->begin(); iter != links->end(); ++iter) {
		LltcLink* l = *iter;
		Ptr<Node> nodeA = nodes->Get(l->nodeA->nodeId);
		Ptr<Node> nodeB = nodes->Get(l->nodeB->nodeId);

		LinkControlHelper::setErrorRate(nodeA, nodeB, errRate);
		LinkControlHelper::setErrorRate(nodeB, nodeA, errRate);
	}
}

void simulateLinkErrors(double timeInSecs, vector<LltcLink*>* links, NodeContainer* nodes, double errRate) {
	Simulator::Schedule(Seconds(timeInSecs), setLinksErrorRatesTask, links, nodes, errRate);
}

#define RAND_SEED_1 50


void writePmuIds(int pmuID, int pdcID, int* output, const char* outputFilePath) {
	unordered_set<uint32_t> nodeIds;
	nodeIds.insert(pmuID);

	std::ofstream outputFile;
	outputFile.open(outputFilePath, ios::trunc);
	outputFile << pdcID << endl;
	size_t i = 0;
	size_t nPMUs = nodeIds.size();
	for (unordered_set<uint32_t>::iterator iter = nodeIds.begin(); iter != nodeIds.end(); ++iter) {
		output[i] = *iter;
		outputFile << *iter;
		if (i < nPMUs - 1) outputFile << ",";
		++i;
	}
	outputFile.flush();
	outputFile.close();
}


vector<LltcLink*>* findFaultLinks(LltcGraph* g, ResilientRoutes* rr, int numPPLinks,
									int numNonPPLinks) {
	vector<int>* nodeIds = rr->primaryPath->nodeIds;
	unordered_set<uint64_t> linkIDSet;
	vector<LltcLink*>* links = new vector<LltcLink*>();

	for (size_t i = 1; i < nodeIds->size(); ++i) {
		int prevNodeID = (*nodeIds)[i - 1];
		int curNodeID = (*nodeIds)[i];
		uint64_t linkID = (uint64_t) prevNodeID << 32 | (uint64_t) curNodeID;
		if (linkIDSet.find(linkID) != linkIDSet.end()) continue;

		LltcLink* l = g->getLinkByNodeIds(prevNodeID, curNodeID);
		links->push_back(l);
	}

	vector<LltcLink*>* links_n = new vector<LltcLink*>();
	srand((unsigned) ::time(NULL));

	for (int i = 0; i < numPPLinks; ++i) {
		size_t m = links->size();
		if (m == 0) break;
		size_t j = rand() % m;
		LltcLink* l = (*links)[j];
		links_n->push_back(l);
		links->erase(links->begin() + j);
	}

	vector<LltcLink*>* links_RSPs = new vector<LltcLink*>();
	unordered_set<uint64_t> linkIDSet_RSPs;

	for (size_t i = 1; i < nodeIds->size(); ++i) {
		int curPPNodeID = (*nodeIds)[i];
		vector<RedundantSubPath*>* rsps = rr->getRSPsByNodeId(curPPNodeID);
		if (rsps == NULL) continue;
		for (vector<RedundantSubPath*>::iterator iter = rsps->begin(); iter != rsps->end(); ++iter) {
			RedundantSubPath* rsp = *iter;
			for (size_t j = 1; j < rsp->path->nodeIds->size(); ++j) {
				int prevRSPNodeID = (*rsp->path->nodeIds)[j - 1];
				int curRSPNodeID = (*rsp->path->nodeIds)[j];
				uint64_t linkID = (uint64_t) prevRSPNodeID << 32 | (uint64_t) curRSPNodeID;
				if (linkIDSet.find(linkID) != linkIDSet.end() ||
						linkIDSet_RSPs.find(linkID) != linkIDSet_RSPs.end()) continue;

				LltcLink* l = g->getLinkByNodeIds(prevRSPNodeID, curRSPNodeID);
				links_RSPs->push_back(l);
			}
		}
	}

	for (int i = 0; i < numNonPPLinks; ++i) {
		size_t m = links_RSPs->size();
		if (m == 0) break;
		size_t j = rand() % m;
		LltcLink* l = (*links_RSPs)[j];
		links_n->push_back(l);
		links_RSPs->erase(links_RSPs->begin() + j);
	}

	return links_n;
}


void simulateLinkErrors(double timeInSecs, double flapPeriod, int flapTimes,
					vector<LltcLink*>* links, NodeContainer* nodes, double errRate) {
	ofstream* log = LltcLog::getLogFailureEvents();

	stringstream ss;

	for (vector<LltcLink*>::iterator iter = links->begin(); iter != links->end(); ++iter) {
		ss << (*iter)->nodeA->nodeId << "-" << (*iter)->nodeB->nodeId << "#";
	}

	for (int i = 0; i < flapTimes; ++i) {
		double errRate_to_set = i % 2 == 0 ? errRate : 0.0;
		double delay = timeInSecs + flapPeriod * i;
		Simulator::Schedule(Seconds(delay), setLinksErrorRatesTask, links, nodes, errRate_to_set);

		*log << delay << "," << errRate_to_set << "," << ss.str() << endl;
	}
}

int main(int argc, char** argv) {
	if (argc != 2) {
		cerr << "Usage: " << argv[0] << " [config-file-path]" << endl;
		return -1;
	}
	const char* configFilePath = argv[1];
	if (!LltcConfig::loadConfigFile(configFilePath)) {
		cerr << "Error to load the configuration file. Please Check." << endl;
		return -1;
	}

	LltcLog::setLogDirPath(LltcConfig::SIM_LOG_DIR.c_str());
	LltcLog::cleanLogDir();
	LltcLog::openLogs(LltcConfig::NETWORK_NUM_SUBSTATIONS);

	int NODE_ID_PMUS[1];
	NODE_ID_PMUS[0] = LltcConfig::NETWORK_NODE_ID_PMU_STATIC;
	stringstream ss;
	ss << LltcConfig::SIM_LOG_DIR << "pdc-pmu-ids";
	writePmuIds(LltcConfig::NETWORK_NODE_ID_PMU_STATIC, LltcConfig::NETWORK_NODE_ID_PDC,
						NODE_ID_PMUS, ss.str().c_str());

	// PMU terminal configuration
	LltcConfiguration* config = new LltcConfiguration(LltcConfig::LLTC_NUM_RETRANS_REQUESTS, LltcConfig::LLTC_BETA,
			LltcConfig::NETWORK_PMU_DATA_FREQ, LltcConfig::LLTC_MAX_DRIFT_RANGE_RATIO_FOR_PMU_FREQ,
			LltcConfig::LLTC_MAX_CONSECUTIVE_DRIFT_PACKETS, LltcConfig::LLTC_MAX_PATH_DELAY_US);

	// data plane configuration
	uint64_t intervalTimeForCheckPathQosInUs = (LltcConfig::NETWORK_NODE_PROCESS_DELAY_US + LltcConfig::NETWORK_LINK_QUEUE_DELAY_US
									+ LltcConfig::NETWORK_LINK_TRANS_DELAY_US) * 2;

	LltcStrategyConfig::setLltcPrefix(LltcConfig::LLTC_PREFIX.c_str());
	LltcStrategyConfig::setPmuFreq(LltcConfig::NETWORK_PMU_DATA_FREQ);
	LltcStrategyConfig::setNumRetransRequests(LltcConfig::LLTC_NUM_RETRANS_REQUESTS);
	LltcStrategyConfig::setIntervalTimeForCheckPathQos((double) intervalTimeForCheckPathQosInUs / 1000000.0);
	LltcStrategyConfig::setWaitingTimeForCheckLinkConnectivityInPerioid(LltcConfig::LLTC_PERIOD_FOR_CHECK_LINK_CONNECTIVITY_SECS);
	LltcStrategyConfig::setDelayTimeForRunningCheckLinkConnectivity(LltcConfig::LLTC_INITIAL_DELAY_FOR_CHECK_LINK_CONNECTIVITY_SECS);
	LltcStrategyConfig::setTimesForCheckPathConnectivity(LltcConfig::LLTC_TIMES_FOR_CHECK_PATH_CONNECTIVITY);
	LltcStrategyConfig::setQueueSizeForTransmittedDataIds(LltcConfig::LLTC_QUEUE_SIZE_FOR_TRANSMITTED_DATA_IDS);
	LltcStrategyConfig::setCsLimitInEntries(LltcConfig::LLTC_CS_LIMITS_IN_ENTRIES);
	LltcStrategyConfig::setMaxDriftRatioForPmuFreq(LltcConfig::LLTC_MAX_DRIFT_RANGE_RATIO_FOR_PMU_FREQ);
	LltcStrategyConfig::setEnableFailover(LltcConfig::LLTC_ENABLE_FAILOVER);
	LltcStrategyConfig::setNumDataRetransReportsToSend(LltcConfig::LLTC_NUM_DATA_RETRANS_REPORTS_TO_SEND);


	// configuration for error or failure events simulation
	int faultSim_Type = LltcConfig::FAULT_SIM_TYPE;

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

	Config::SetDefault("ns3::PointToPointNetDevice::DataRate", StringValue(LltcConfig::NETWORK_LINK_BAND));
	Config::SetDefault("ns3::QueueBase::MaxSize", StringValue(LltcConfig::NETWORK_LINK_QUEUE_SIZE));

	CommandLine cmd;
	cmd.Parse(argc, argv);

	NodeContainer nodes;
	size_t nNodes = g->getNNodes();
	nodes.Create(nNodes);
	PointToPointHelper p2p;

	for (std::vector<LltcLink*>::iterator iter = g->links->begin(); iter != g->links->end(); ++iter) {
		LltcLink* l = *iter;
		Ptr<Node> nodeA = nodes.Get(l->nodeA->nodeId);
		Ptr<Node> nodeB = nodes.Get(l->nodeB->nodeId);
		ss.str("");
		ss << (l->abDelay_trans + l->abDelay_process) << "us";
		p2p.SetChannelAttribute("Delay", StringValue(ss.str()));
		p2p.Install(nodeA, nodeB);
	}

	p2p.EnablePcap("drtp-dijkstra", LltcConfig::NETWORK_NODE_ID_PDC, 0, true);

	StackHelper ndnHelper;
	ndnHelper.InstallAll();

	Ptr<Node> pdcNode = nodes.Get(LltcConfig::NETWORK_NODE_ID_PDC);
	AppHelper pdcAppHelper("lltc::LltcPDCApp");
	ApplicationContainer pdcAC = pdcAppHelper.Install(pdcNode);
	Ptr<LltcPDCApp> pdcApp = ns3::DynamicCast<lltc::LltcPDCApp, Application>(pdcAC.Get(0));
	pdcApp->setPDCName("/lltc/pdc");
	pdcApp->setNodeID(LltcConfig::NETWORK_NODE_ID_PDC);
	pdcApp->SetStartTime(Seconds(LltcConfig::SIM_PDC_START_TIME_SECS));
	pdcApp->SetStopTime(Seconds(LltcConfig::SIM_TIME_SECS));

	for (size_t i = 0; i < (size_t) 1; ++i) {
		ss.str("");
		ss << "/lltc/pmu" << NODE_ID_PMUS[i];
		const char* pmuName = ss.str().c_str();
		size_t nodeId = NODE_ID_PMUS[i];
		pdcApp->addPMU(nodeId, pmuName, ns3::MicroSeconds(LltcStrategyConfig::dataPiatInUs), LltcConfig::LLTC_PDC_RESEQNEUCE_RANGE);

		Ptr<Node> pmuNode = nodes.Get(nodeId);
		AppHelper pmuAppHelper("lltc::LltcPMUApp");
		ApplicationContainer pmuAC = pmuAppHelper.Install(pmuNode);
		Ptr<LltcPMUApp> pmuApp = ns3::DynamicCast<lltc::LltcPMUApp, Application>(pmuAC.Get(0));
		pmuApp->setPmuName(pmuName);
		pmuApp->setFreq(LltcConfig::NETWORK_PMU_DATA_FREQ);
		pmuApp->setNodeID(nodeId);
		pmuApp->SetStartTime(Seconds(LltcConfig::SIM_PMU_START_TIMES_SECS));
		pmuApp->SetStopTime(Seconds(LltcConfig::SIM_TIME_SECS));
	}

	LltcRoutingHelper lltcRoutingHelper(g, config, ROUTING_TYPE_DRTP_DIJKSTRA);
	lltcRoutingHelper.InstallAll();
	for (size_t i = 0; i < (size_t) 1; ++i) {
		size_t nodeId = NODE_ID_PMUS[i];
		ss.str("");
		ss << "/lltc/pmu" << nodeId;
		const char* pmuName = ss.str().c_str();

		Ptr<Node> pmuNode = nodes.Get(nodeId);
		lltcRoutingHelper.AddOrigin(pmuName, pmuNode);
		lltcRoutingHelper.addCommPair(pmuNode, pdcNode);
	}

	lltcRoutingHelper.computeRoutesVectors();
	lltcRoutingHelper.computeRoutesForCommPairs();
	lltcRoutingHelper.dumpRouterLinkFaceIDs();

	StrategyChoiceHelper::InstallAll<nfd::fw::LltcStrategy>("/lltc");

	Forwarder::setExtensionLltcPrefix(new string("/lltc"));
	Forwarder::setExtensionLltcPitDurationInMilliSecs(LltcConfig::LLTC_PIT_DURATION_MS);

	Ptr<L3Protocol> proto = pdcNode->GetObject<L3Protocol>();
	shared_ptr<nfd::Forwarder> forwarder = proto->getForwarder();

	nfd::fib::Fib& fib = forwarder->getFib();
	std::cout << "================ FIB Dump =================" << std::endl;
	for (nfd::fib::Fib::const_iterator iter = fib.begin(); iter != fib.end(); ++iter) {
		for (const auto& nexthop : iter->getNextHops()) {
			nfd::Face& outFace = nexthop.getFace();
			std::cout << iter->getPrefix().toUri(name::UriFormat::DEFAULT) << ": " << outFace.getLocalUri() << std::endl;
		}
	}
	std::cout << "===========================================" << std::endl;

	PcapWriter trace("lltc-sim.pcap");
	Config::ConnectWithoutContext("/NodeList/*/DeviceList/*/$ns3::PointToPointNetDevice/MacTx",
			MakeCallback(&PcapWriter::TracePacket, &trace));

	list<comm_pair*>* commPairs = NULL;
	switch (faultSim_Type) {
		case FAULT_SIM_TYPE_ERR_FOR_FULL_RR: {
			simulateLinkErrors(LltcConfig::FAULT_SIM_TYPE_ERR_FOR_FULL_RR_START_TIME, g->links, &nodes,
					LltcConfig::FAULT_SIM_TYPE_ERR_FOR_FULL_RR_ERR_RATE);
			break;
		}

		case FAULT_SIM_TYPE_ERR_FOR_FULL_PP: {
			commPairs = lltcRoutingHelper.getCommPairs();
			list<comm_pair*>::iterator iter = commPairs->begin();
			vector<int>* nodeIds = (*iter)->rr->primaryPath->nodeIds;
			vector<LltcLink*>* ppLinks = new vector<LltcLink*>();

			for (uint32_t i = 1; i < nodeIds->size(); ++i) {
				LltcLink* l = g->getLinkByNodeIds((*nodeIds)[i - 1], (*nodeIds)[i]);
				ppLinks->push_back(l);
			}

			simulateLinkErrors(LltcConfig::FAULT_SIM_TYPE_ERR_FOR_FULL_PP_START_TIME, ppLinks, &nodes,
					LltcConfig::FAULT_SIM_TYPE_ERR_FOR_FULL_PP_ERR_RATE);
			break;
		}

		case FAULT_SIM_TYPE_ERR_FOR_SINGLE_PP_LINK: {
			commPairs = lltcRoutingHelper.getCommPairs();
			list<comm_pair*>::iterator iter = commPairs->begin();
			vector<int>* nodeIds = (*iter)->rr->primaryPath->nodeIds;
			vector<LltcLink*>* singleLink = new vector<LltcLink*>();

			LltcLink* l = g->getLinkByNodeIds(LltcConfig::FAULT_SIM_TYPE_ERR_FOR_SINGLE_PP_LINK_ROUTER_1,
											LltcConfig::FAULT_SIM_TYPE_ERR_FOR_SINGLE_PP_LINK_ROUTER_2);
			singleLink->push_back(l);
			simulateLinkErrors(LltcConfig::FAULT_SIM_TYPE_ERR_FOR_SINGLE_PP_LINK_START_TIME, singleLink, &nodes,
					LltcConfig::FAULT_SIM_TYPE_ERR_FOR_SINGLE_PP_LINK_ERR_RATE);
			break;
		}

		case FAULT_SIM_TYPE_ERR_FOR_MULTIPLE_LINKS: {
			if (LltcConfig::FAULT_SIM_TYPE_ERR_FOR_MULTIPLE_LINKS_N_RAND_DISRUPTED_PP_LINKS == 0 &&
					LltcConfig::FAULT_SIM_TYPE_ERR_FOR_MULTIPLE_LINKS_N_RAND_DISRUPTED_NON_PP_LINKS == 0)
				break;

			commPairs = lltcRoutingHelper.getCommPairs();
			list<comm_pair*>::iterator iter = commPairs->begin();
			ResilientRoutes* rr = (*iter)->rr;

			vector<LltcLink*>* linksToMakeFault = findFaultLinks(g, rr,
					LltcConfig::FAULT_SIM_TYPE_ERR_FOR_MULTIPLE_LINKS_N_RAND_DISRUPTED_PP_LINKS,
					LltcConfig::FAULT_SIM_TYPE_ERR_FOR_MULTIPLE_LINKS_N_RAND_DISRUPTED_NON_PP_LINKS);

			simulateLinkErrors(LltcConfig::FAULT_SIM_TYPE_ERR_FOR_MULTIPLE_LINKS_START_TIME,
					LltcConfig::FAULT_SIM_TYPE_ERR_FOR_MULTIPLE_LINKS_FLAP_PERIOD,
					LltcConfig::FAULT_SIM_TYPE_ERR_FOR_MULTIPLE_LINKS_FLAP_TIMES,
					linksToMakeFault, &nodes,
					LltcConfig::FAULT_SIM_TYPE_ERR_FOR_MULTIPLE_LINKS_ERR_RATE);
			break;
		}
	}

	Simulator::Stop(Seconds(LltcConfig::SIM_TIME_SECS));
	Simulator::Run();
	Simulator::Destroy();

	list<retran_event>* retranEvents = LltcLog::getRetranEvents();
	list<num_retran_paths>* numRetranPaths = LltcLog::getNumRetranPaths();
	rsg_id rsgId = 0;
	unordered_map<uint32_t, unordered_map<uint32_t, uint32_t>> nodeMap;
	for (list<retran_event>::iterator iter = retranEvents->begin(); iter != retranEvents->end(); ++iter) {
		//cout << iter->nodeId << "," << iter->dataId << "," << iter->pathId << "," << iter->type << endl;
		if (iter->rsgId != rsgId) continue;
		 unordered_map<uint32_t, uint32_t>& count = nodeMap[iter->nodeId];
		 if (count.find(iter->dataId) == count.end()) {
			 count[iter->dataId] = 1;
		 } else {
			 count[iter->dataId]++;
		 }
	}

	unordered_map<uint32_t, uint32_t> nodeEtaMap;
	for (unordered_map<uint32_t, unordered_map<uint32_t, uint32_t>>::iterator iter = nodeMap.begin();
			iter != nodeMap.end(); ++iter) {
		unordered_map<uint32_t, uint32_t>& count = iter->second;

		uint32_t maxCount = 0;
		for (unordered_map<uint32_t, uint32_t>::iterator iter2 = count.begin();
				iter2 != count.end(); ++iter2) {
			if (iter2->second > maxCount) {
				maxCount = iter2->second;
			}
			//cout << iter->first << "," << iter2->first << "," << iter2->second << endl;
		}

		uint32_t nPaths = 0;
		for (list<num_retran_paths>::iterator iter2 = numRetranPaths->begin();
					iter2 != numRetranPaths->end(); iter2++) {
			if (iter2->nodeId == iter->first && iter2->rsgId == rsgId) {
				nPaths = iter2->numRetranPaths;
			}
		}
		nodeEtaMap[iter->first] = maxCount / nPaths;
	}


	ofstream* logOut_relia = LltcLog::getLogRrReliaEvaluation();
	list<comm_pair*>* pairs = lltcRoutingHelper.getCommPairs();

	for (list<comm_pair*>::iterator iter = pairs->begin(); iter != pairs->end(); ++iter) {
		comm_pair* pair = *iter;
		unordered_map<int, vector<RedundantSubPath*>*>* hrsp = pair->rr->getHRSP();
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
		*logOut_relia << pp_len << endl;
		for (int i = 0; i < nRSPs_in_pp.size(); ++i) {
			*logOut_relia << nRSPs_in_pp[i] << (i == (nRSPs_in_pp.size() - 1) ? "" : ",");
		}
		*logOut_relia << endl;

		for (int i = 0; i < RSP_len_in_pp.size(); ++i) {
			*logOut_relia << RSP_len_in_pp[i] << (i == (RSP_len_in_pp.size() - 1) ? "" : ",");
		}
		*logOut_relia << endl;
	}
	logOut_relia->flush();
	logOut_relia->close();

	return 0;
}
