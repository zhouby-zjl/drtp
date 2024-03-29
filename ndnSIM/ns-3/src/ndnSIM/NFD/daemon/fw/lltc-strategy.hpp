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

#ifndef SRC_NDNSIM_NFD_DAEMON_FW_LLTC_STRATEGY_HPP_
#define SRC_NDNSIM_NFD_DAEMON_FW_LLTC_STRATEGY_HPP_

#include "ns3/random-variable-stream.h"

#include "strategy.hpp"
#include "algorithm.hpp"
#include "common/global.hpp"
#include "common/logger.hpp"
#include "ndn-cxx/tag.hpp"
#include <list>
#include <unordered_map>
#include <unordered_set>
#include <queue>
#include <vector>
#include <fstream>

#include "lltc-common.hpp"
#include "lltc-fs.hpp"
#include "lltc-messages-helper.hpp"

#include "ns3/ndnSIM/model/lltc/lltc-resilient-routes-generation.hpp"
#include "ns3/ndnSIM/model/lltc/lltc-fibonacci-heap.hpp"

using namespace nfd;
using namespace std;
using namespace lltc;

namespace nfd {
namespace fw {

struct ForwardingStateForRSG;

struct RequestCallingParam {
	int restRetransRequests;
	bool findClosestDataIfCsMissed;
	uint32_t rsgId;
};

#define ECHO_MSG_STATUS_WAITING 1
#define ECHO_MSG_STATUS_NORMAL 0

struct EchoMessageState {
	ns3::Time requestedTime;
	ns3::Time rtt;
	LltcFaceEntry* fe;
	int status;
};

typedef uint32_t echo_id;
typedef uint32_t link_test_id;


struct LinkMeasurementState {
	link_test_id nextLinkTestID;
	bool isCheckingLinkStatesInPeriodInRunning;
	unordered_map<link_test_id, vector<echo_id>> EchoIDsInLinkTest;
	unordered_map<echo_id, EchoMessageState> echoMsgStateMap;

	uint32_t lsaUpdateSeqNo;
	shared_ptr<list<LinkState>> prevLsa;
	unordered_set<uint32_t> recentLsaIds;

	unordered_map<FaceId, WaitCapsuleForActiveFailoverEvent*> waitCapForActiveFailoverEvents;
};

struct FaceStatusInternal {
	int faceStatus;
	//ns3::Time rttCulmulative;
	uint32_t nBacks;
};

struct WaitForRetransInBatchEvent {
	uint32_t eventID;
	unordered_set<LltcDataID>* dataIDs;
	ns3::EventId e;
	ns3::Time time;
};

struct CapsuleForwardingStates {
	string prefix;
	LltcDataID lastDataId;
	LltcDataIdQueue transmittedDataIds;
	unordered_set<LltcDataID> lostDataIdsFromPrevRouters;
	unordered_map<LltcDataID, RetransInSingleEvent> waitForNextDataEvents;
	unordered_map<DataAndPathID, RetransInSingleEvent> retransInSingleEvents;
	unordered_map<RetransInMultipleID, RetransInMultipleEvent> retransInBatchEvents;
	list<WaitForRetransInBatchEvent> WaitForRetransInBatchEvents;
	unordered_set<unsigned int> recentDataRetransReportUIDs;
	RetransInMultipleID curRetransInBatchID;
	uint32_t curWaitForRetransInBatchEventID;
};

#define CS_DATA_ID_NULL UINT_MAX

typedef void(*LinkTestCallBackFunc)(link_test_id);

class LltcStrategy : public Strategy {
public:
	LltcStrategy(Forwarder& forwarder, const Name& name = getStrategyName());
	virtual ~LltcStrategy() override;
    static const Name& getStrategyName();

	// =========================== Service Sub-Layer: Forwarding Interest ======================================
    void afterReceiveInterest(const FaceEndpoint& ingress, const Interest& interest,
	                       const shared_ptr<pit::Entry>& pitEntry) override;
    // ======================================================================================================

    // ================================ Forwarding Sub-Layer: Forwarding ====================================
    void afterReceiveData(const shared_ptr<pit::Entry>& pitEntry,
                     const FaceEndpoint& ingress, const Data& data) override;
    void afterReceiveNonPitData(const FaceEndpoint& ingress, const Data& data);
    void afterReceiveNack(const FaceEndpoint& ingress, const lp::Nack& nack,
                               const shared_ptr<pit::Entry>& pitEntry) override;
    // ======================================================================================================

private:
    // ============================== Service Sub-Layer: Data Forwarding ===========================================
    void onReceiveCapsule(const shared_ptr<pit::Entry>& pitEntry, const FaceEndpoint& ingress,
    						const Data& data, CapsuleForwardingStates* dls, uint32_t rsgId, uint32_t pathId);
    void onReceiveCapsuleOnRsp(const shared_ptr<pit::Entry>& pitEntry,
    						const FaceEndpoint& ingress, const Data& data, CapsuleForwardingStates* dls, uint32_t rsgId, uint32_t pathId);
    void onReceiveDownSideControl(const shared_ptr<pit::Entry>& pitEntry,
							const FaceEndpoint& ingress, const Data& data, CapsuleForwardingStates* dls, uint32_t rsgId, uint32_t pathId);
    void onReceiveUpSideControl(const shared_ptr<pit::Entry>& pitEntry,
							const FaceEndpoint& ingress, const Data& data, CapsuleForwardingStates* dls, uint32_t rsgId, uint32_t pathId);
    void afterContentStoreHit(const shared_ptr<pit::Entry>& pitEntry,
                         const FaceEndpoint& ingress, const Data& data) override;

	void deliverCapsule(const shared_ptr<pit::Entry>& pitEntry, Name& dataName,
									const Data& data, LltcDataID dataId, uint32_t rsgId);
	void deliverNonPitMessage(const Data& data, int direction, uint32_t rsgId);

	bool forwardInNonPitRegionIfNecessary(const shared_ptr<pit::Entry>& pitEntry, const Data& data,
									uint32_t rsgId, uint32_t pathId, int direction);
	// ========================================================================================================

	//========================== Service Sub-Layer: Retransmissions Control ========================================
	// -------------------------------- Retransmissions In Single ---------------------------------------------
    void onReceiveRequestS(const shared_ptr<pit::Entry>& pitEntry,
							const FaceEndpoint& ingress, const Data& data, CapsuleForwardingStates* dls, uint32_t rsgId, uint32_t pathId);
    void onReceiveRetranS(const shared_ptr<pit::Entry>& pitEntry, const FaceEndpoint& ingress,
    						const Data& data, CapsuleForwardingStates* dls, uint32_t rsgId, uint32_t pathId);

	void waitForNextCapsule(LltcDataID nextDataId,	string pitPrefixStr, const shared_ptr<pit::Entry>& pitEntry,
						CapsuleForwardingStates* ds, int nSkippedDatas, uint32_t rsgId, uint64_t penaltyTime);
	void waitForNextCapsuleDelayedTask(LltcDataID lostDataId, const shared_ptr<pit::Entry>& pitEntry,
									CapsuleForwardingStates* ds, uint32_t rsgId, bool needToCheckLinkStates);
	void waitForNextCapsuleDelayedTaskIterative
				(LltcDataID lostDataId, RequestCallingParam rrcp, const shared_ptr<pit::Entry>& pitEntry,
						LltcFIBEntry* fe, CapsuleForwardingStates* ds, bool waitForNextDataDelayedTask);

	bool cleanWaitForNextEvent(CapsuleForwardingStates* dls, LltcDataID dataId);
	bool cleanRetranInSingleEvents(CapsuleForwardingStates* dls, LltcDataID dataId, uint32_t rsgId);
	LltcDataID computeNextDataId(CapsuleForwardingStates* dls, LltcDataID curDataId);

    // -------------------------------- Retransmissions In Batch ------------------------------------------------
    void onReceiveRequestM(const shared_ptr<pit::Entry>& pitEntry,
							const FaceEndpoint& ingress, const Data& data, CapsuleForwardingStates* dls, uint32_t rsgId, uint32_t pathId);
    void onReceiveRetranM(const shared_ptr<pit::Entry>& pitEntry, const FaceEndpoint& ingress,
    						const Data& data, CapsuleForwardingStates* dls, uint32_t rsgId, uint32_t pathId);
	void broadcastRequestMAndWaitForRetranM
					(list<LltcDataID> lostDataIds, const shared_ptr<pit::Entry>& pitEntry, CapsuleForwardingStates* dls,
							bool findClosestDataIfCsMissed, uint32_t rsgId);
	void broadcastRequestMAndWaitForRetranMRecursive
					(RetransInMultipleID rbi, int restRetransRequests, bool findClosestDataIfCsMissed,
									const shared_ptr<pit::Entry>& pitEntry, CapsuleForwardingStates* dls, uint32_t rsgId);
	void discoverLostCapsules(CapsuleForwardingStates* dls, LltcDataID curDataId, const shared_ptr<pit::Entry>& pitEntry, uint32_t rsgId);

	void cleanRetranMEvents(CapsuleForwardingStates* dls, RetransInMultipleID bri, LltcDataID retransDataId);
	void cleanRetranMEvents(CapsuleForwardingStates* dls, LltcDataID retransDataId);
	RetransInMultipleEvent& getRetranMEvent(CapsuleForwardingStates* ds, RetransInMultipleID rbi);

	// ----------------------------- Retrans. or Lost Data State Report -----------------------------------------
    void onReceiveReport(const shared_ptr<pit::Entry>& pitEntry,
							const FaceEndpoint& ingress, const Data& data, CapsuleForwardingStates* dls, uint32_t rsgId, uint32_t pathId);
	void sendReport(list<LltcDataID>* lostDataIDs, string pitPrefixStr, const shared_ptr<pit::Entry>& pitEntry,
										uint32_t rsgId, uint32_t pathId);
	void waitToRequestM(uint32_t eventID, unordered_set<LltcDataID>* newRetransDataIds_upNodes,
							const shared_ptr<pit::Entry>& pitEntry,	CapsuleForwardingStates* sls, uint32_t rsgId);
	void addToSet(list<LltcDataID>* dataIDs, unordered_set<LltcDataID>* list);
	unordered_set<LltcDataID>* setMinus(list<LltcDataID>* dataIDs, unordered_set<LltcDataID>* list);
	void cleanWaitForRequestMEvents(CapsuleForwardingStates* ds, LltcDataID dataId);

	// --------------------------------------- State Management -------------------------------------------------
	bool checkAndRecordRetransmittedDataId(CapsuleForwardingStates* dls, LltcDataID retransDataId, ns3::Time time);
	DataAndPathID computeDataRetransID(LltcDataID dataId, uint32_t pathId);
	CapsuleForwardingStates* createOrGetCapsuleForwardingStates(string prefix);

	unordered_map<string, CapsuleForwardingStates*> capsuleForwardingStates_map;
	// =======================================================================================================

	// ===================================== Service Sub-Layer: CS ===============================================
	pair<LltcDataID, const Data*> lookupInCs(string pitPrefix, LltcDataID dataId, bool isExact);
	pair<LltcDataID, const Name*> findClosestDataNameInCS(string pitPrefixStr, LltcDataID dataId);
	Cs* localCs;
	// =======================================================================================================

	// ================= Forwarding Sub-Layer: Measurement & Control (Also Contained in Service Layer) ==================
    void onReceiveEcho(const FaceEndpoint& ingress, const Data& data);
    void onReceiveBack(const FaceEndpoint& ingress, const Data& data);
    void onReceiveLsa(const FaceEndpoint& ingress, const Data& data);

	void checkLinkConnectivityInPeriodic();
	void checkLinkConnectivityInActiveManner(FaceId faceID);
	void doActiveLinkConnectivityCheck(const FaceEndpoint& ingress);
	void runLinkTestsAndDoUpdate(int _timesForCheckPathQoS, link_test_id linkTestID,
								bool isFull, FaceId faceID, bool isActive);
	void updateFaceStatsWithLinkTestStates(link_test_id linkTestID);
	void broadcastEchos(link_test_id linkTestID, bool isFull, FaceId faceID);
	void broadcastingLsaTask(bool isActive);
	void updatePreferredPathWithLSA(int32_t nodeId, list<LinkState>& lsList, bool isActive);

	LltcRrSubgraph* setLltcRrSubgraph(ResilientRoutes* rr, LltcGraph* g);

	LinkMeasurementState linkMeasurementState;
	// =======================================================================================================

	// ===================================== Forwarding Sub-Layer: Forward ==========================================
	void sendLltcInterest(const shared_ptr<pit::Entry>& pitEntry, const Interest& interest,
							const FaceEndpoint& egress, uint32_t rsgId);
	void sendLltcData(const shared_ptr<pit::Entry>& pitEntry, const Data& data,
							const FaceEndpoint& egress, uint32_t rsgId, uint32_t pathId);
	void sendLltcNonPitData(const Data& data, const FaceEndpoint& egress, uint32_t rsgId, uint32_t pathId);
	void sendLltcNonPitData(const Data& data, const FaceEndpoint& egress);

	LltcFIBEntry* lookupForwardingEntry(uint32_t rsgId, uint32_t pathId, int direction);
	uint32_t extractPathId(const Data& data, uint32_t rsgId);
	uint32_t extractRsgId(const Interest& interest);
	uint32_t extractRsgId(const Data& data);
	uint32_t lookupRsgID(string prefix);
	ForwardingStateForRSG* getForwardingState(uint32_t rsgId);

	ForwardingStateCollection* forwardingStateColl;
	// =======================================================================================================


	bool isPPNode();
	bool isRSPNode();

	ofstream* outLog_in_msgs;
	ofstream* outLog_out_msgs;
};

}
}


#endif /* SRC_NDNSIM_NFD_DAEMON_FW_LLTC_STRATEGY_HPP_ */
