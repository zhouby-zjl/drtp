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

#include "lltc-strategy-common.hpp"

namespace nfd {
namespace fw {

// ================= Link Layer: Measurement & Control (Also Contained in Service Layer) ==================
void LltcStrategy::onReceiveEcho(const FaceEndpoint& ingress, const Data& data) {
	EchoUri eru = LltcMessagesHelper::parseEchoUri(LltcStrategyConfig::lltcPrefix, data.getName());

	shared_ptr<Data> newData = LltcMessagesHelper::constructBack(LltcStrategyConfig::lltcPrefix, eru.echoId,
									forwardingStateColl->localNodeId, eru.sourceNodeId);
	this->sendLltcNonPitData(*newData, ingress);
}

void LltcStrategy::onReceiveBack(const FaceEndpoint& ingress, const Data& data) {
	BackUri ebu = LltcMessagesHelper::parseBackUri(LltcStrategyConfig::lltcPrefix, data.getName());

	if (linkMeasurementState.echoMsgStateMap.find(ebu.echoId) != linkMeasurementState.echoMsgStateMap.end()) {
		EchoMessageState& ems = linkMeasurementState.echoMsgStateMap[ebu.echoId];
		ems.rtt = Simulator::Now() - linkMeasurementState.echoMsgStateMap[ebu.echoId].requestedTime;
		ems.status = ECHO_MSG_STATUS_NORMAL;
	}
}

void LltcStrategy::checkLinkConnectivityInPeriodic() {
	link_test_id linkTestID = linkMeasurementState.nextLinkTestID++;
	linkMeasurementState.EchoIDsInLinkTest[linkTestID];
	linkMeasurementState.isCheckingLinkStatesInPeriodInRunning = true;
	checkLinkConnectivityInPeriodicRecursive(linkTestID);
}

void LltcStrategy::checkLinkConnectivityInPeriodicRecursive(link_test_id linkTestID) {
	link_test_id nextLinkTestID = linkMeasurementState.nextLinkTestID++;
	linkMeasurementState.EchoIDsInLinkTest[nextLinkTestID];
	runLinkTestsAndDoUpdate(LltcStrategyConfig::timesForCheckPathConnectivity, nextLinkTestID);

	ns3::Simulator::Schedule(LltcStrategyConfig::waitingTimeForCheckLinkConnectivityInPerioid,
						&LltcStrategy::checkLinkConnectivityInPeriodicRecursive, this, linkTestID);
}

void LltcStrategy::checkLinkConnectivityInActiveManner() {
	link_test_id linkTestID = linkMeasurementState.nextLinkTestID++;
	linkMeasurementState.EchoIDsInLinkTest[linkTestID];
	runLinkTestsAndDoUpdate(LltcStrategyConfig::timesForCheckPathConnectivity, linkTestID);
}


void LltcStrategy::runLinkTestsAndDoUpdate(int _timesForCheckPathQoS, link_test_id linkTestID) {
	if (_timesForCheckPathQoS == 0) {
		updateFaceStatsWithLinkTestStates(linkTestID);
		broadcastingLsaTask();
		return;
	}
	this->broadcastEchos(linkTestID);
	ns3::Simulator::Schedule(LltcStrategyConfig::intervalTimeForCheckPathQos,
					&LltcStrategy::runLinkTestsAndDoUpdate, this, _timesForCheckPathQoS - 1, linkTestID);
}

void LltcStrategy::updateFaceStatsWithLinkTestStates(link_test_id linkTestID) {
	unordered_map<LltcFaceEntry*, FaceStatusInternal> faceStatusMap;
	vector<echo_id>& echoIDs = linkMeasurementState.EchoIDsInLinkTest[linkTestID];

	for (echo_id echoID : echoIDs) {
		EchoMessageState& ems = linkMeasurementState.echoMsgStateMap[echoID];
		if (faceStatusMap.find(ems.fe) == faceStatusMap.end()) {
			FaceStatusInternal& fsi = faceStatusMap[ems.fe];
			fsi.nBacks = 0;
			if (ems.status == ECHO_MSG_STATUS_NORMAL) {
				fsi.faceStatus = FACE_STATUS_NORMAL;
				++fsi.nBacks;
			} else {
				fsi.faceStatus = FACE_STATUS_FAILURE;
			}
		} else if (ems.status == ECHO_MSG_STATUS_NORMAL) {
			FaceStatusInternal& fsi = faceStatusMap[ems.fe];
			fsi.faceStatus = FACE_STATUS_NORMAL;
			++fsi.nBacks;
		}

		linkMeasurementState.echoMsgStateMap.erase(echoID);
	}

	linkMeasurementState.EchoIDsInLinkTest.erase(linkTestID);

	for (LltcFaceEntry* fe : forwardingStateColl->faceEntries) {
		if (fe->remoteLltcNodeId == -1) continue;
		if (faceStatusMap.find(fe) != faceStatusMap.end()) {
			FaceStatusInternal& fsi = faceStatusMap[fe];
			fe->status = fsi.faceStatus;
			fe->lastCheckTime = Simulator::Now();
		}
	}
}

void LltcStrategy::broadcastEchos(link_test_id linkTestID) {
	Ptr<UniformRandomVariable> rand = CreateObject<UniformRandomVariable>();

	vector<echo_id>& echoIDs = linkMeasurementState.EchoIDsInLinkTest[linkTestID];
	EchoMessageState es;
	for (list<LltcFaceEntry*>::const_iterator iterFa = forwardingStateColl->faceEntries.begin(); iterFa != forwardingStateColl->faceEntries.end(); ++iterFa) {
		LltcFaceEntry* fe = *iterFa;
		if (fe->remoteLltcNodeId == -1) continue;

		echo_id echoId = rand->GetInteger(0, UINT_MAX);
		shared_ptr<Data> newData = LltcMessagesHelper::constructEcho(LltcStrategyConfig::lltcPrefix, echoId, forwardingStateColl->localNodeId);
		this->sendLltcNonPitData(*newData, FaceEndpoint(*fe->face_nextHop, 0));

		es.requestedTime = Simulator::Now();
		es.fe = fe;
		es.status = ECHO_MSG_STATUS_WAITING;
		linkMeasurementState.echoMsgStateMap[echoId] = es;
		echoIDs.push_back(echoId);
	}
}


void LltcStrategy::broadcastingLsaTask() {
	shared_ptr<list<LinkState>> lsList = make_shared<list<LinkState>>();
	LinkState ls;
	bool diff = false;
	for (list<LltcFaceEntry*>::const_iterator iter = forwardingStateColl->faceEntries.begin(); iter != forwardingStateColl->faceEntries.end(); ++iter) {
		LltcFaceEntry* fa = *iter;
		if (fa->remoteLltcNodeId == -1) continue;

		ls.neighboredNodeId = fa->remoteLltcNodeId;
		ls.status = fa->status;

		if (!diff && linkMeasurementState.prevLsa != nullptr) {
			diff = false; bool found = false;
			for (list<LinkState>::const_iterator iterPrev = linkMeasurementState.prevLsa->begin(); iterPrev != linkMeasurementState.prevLsa->end(); ++iterPrev) {
				if ((*iterPrev).neighboredNodeId == ls.neighboredNodeId) {
					found = true;
					if ((*iterPrev).status != ls.status) {
						diff = true;
					}
					break;
				}
			}
			if (!found) diff = true;
		} else if (!diff && linkMeasurementState.prevLsa == nullptr && fa->status == FACE_STATUS_FAILURE) {
			diff = true;
		}
		lsList->push_back(ls);
	}

	if (!diff) return;

	updatePreferredPathWithLSA(forwardingStateColl->localNodeId, *lsList);

	linkMeasurementState.prevLsa = lsList;

	Ptr<UniformRandomVariable> rand = CreateObject<UniformRandomVariable>();
	uint32_t lsaId = rand->GetInteger(0, UINT_MAX);

	shared_ptr<Data> data = LltcMessagesHelper::constructLsa(LltcStrategyConfig::lltcPrefix, lsaId,
												forwardingStateColl->localNodeId, linkMeasurementState.lsaUpdateSeqNo, &(*lsList));
	++linkMeasurementState.lsaUpdateSeqNo;

	ofstream* log_failovers = LltcLog::getLogFailovers();
	*log_failovers << Simulator::Now() << ",broadcastingLsaTask,@" << forwardingStateColl->localNodeId << ","
			<< lsaId << "," << linkMeasurementState.lsaUpdateSeqNo << ",lsList: ";
	for (LinkState& ls : *lsList) {
		*log_failovers << "(" << ls.neighboredNodeId << "," << ls.status << ") ";
	}
	*log_failovers << endl;

	for (list<LltcFaceEntry*>::const_iterator iter = forwardingStateColl->faceEntries.begin(); iter != forwardingStateColl->faceEntries.end(); ++iter) {
		LltcFaceEntry* fa = *iter;
		this->sendNonPitData(*data, FaceEndpoint(*fa->face_nextHop, 0));
	}
}


void LltcStrategy::onReceiveLsa(const FaceEndpoint& ingress, const Data& data) {
	LsaUri flu = LltcMessagesHelper::parseLsaUri(LltcStrategyConfig::lltcPrefix, data.getName());

	if (linkMeasurementState.recentLsaIds.find(flu.lsaId) != linkMeasurementState.recentLsaIds.end()) {
		return;
	} else {
		linkMeasurementState.recentLsaIds.insert(flu.lsaId);
	}

	for (list<LltcFaceEntry*>::const_iterator iter = forwardingStateColl->faceEntries.begin(); iter != forwardingStateColl->faceEntries.end(); ++iter) {
		LltcFaceEntry* fa = *iter;
		this->sendNonPitData(data, FaceEndpoint(*fa->face_nextHop, 0));
	}

	list<LinkState> lsList;
	LltcMessagesHelper::extractLsa(data, &lsList);

	cout << "LSA UPATE received for ID: " << flu.lsaId << ", at node ID: " << forwardingStateColl->localNodeId << endl;

	ofstream* log_failovers = LltcLog::getLogFailovers();
	*log_failovers << Simulator::Now() << ",onReceiveFastLsaMessage,@" << forwardingStateColl->localNodeId
			<< "," << flu.sourceNodeId << "," << flu.lsaId << "," << flu.updateSeqNo << ",lsList: ";
	for (LinkState& ls : lsList) {
		*log_failovers << "(" << ls.neighboredNodeId << "," << ls.status << ") ";
	}
	*log_failovers << endl;

	updatePreferredPathWithLSA(flu.sourceNodeId, lsList);
}


void LltcStrategy::updatePreferredPathWithLSA(int32_t sourceNodeId, list<LinkState>& lsList) {
	for (unordered_map<rsg_id, ForwardingStateForRSG>::iterator iter = forwardingStateColl->fssMap.begin(); iter != forwardingStateColl->fssMap.end(); ++iter) {
		ForwardingStateForRSG& linkLayerForwardingState = iter->second;

		bool hasRetransPoint = false;
		for (auto fe : linkLayerForwardingState.fibEntries) {
			if (fe->isRetransPoint) {
				hasRetransPoint = true;
				break;
			}
		}
		if (!hasRetransPoint) return;

		for (LinkState ls: lsList) {
			rsg_vir_link_status virLinkStatus = ls.status == FACE_STATUS_FAILURE ? RSG_VIR_LINK_STATUS_FAILURE : RSG_VIR_LINK_STATUS_NORMAL;
			if (virLinkStatus == RSG_VIR_LINK_STATUS_FAILURE) {
				cout << "here" << endl;
			}
			LltcRrSubgraph::rsgUpdateLinkStatus(&linkLayerForwardingState.rsg, sourceNodeId, ls.neighboredNodeId, virLinkStatus);
		}

		preferred_link preferredPaths = LltcRrSubgraph::rsgComputePreferredPathID(&linkLayerForwardingState.rsg, forwardingStateColl->localNodeId);
		cout << "rsgComputePreferredPathID, downLinkID: " << preferredPaths.downLinkID << ", upLinkID: " << preferredPaths.upLinkID << " @ Node ID: " << forwardingStateColl->localNodeId << endl;

		ofstream* log_failovers = LltcLog::getLogFailovers();
		if (preferredPaths.downLinkID != UINT_MAX && preferredPaths.downLinkID != linkLayerForwardingState.preferredDownPathId) {
			linkLayerForwardingState.preferredDownPathId = preferredPaths.downLinkID;
			linkLayerForwardingState.isDownPathInRspState = (preferredPaths.downLinkID != linkLayerForwardingState.primaryPathId);
			cout << "PERFORM CHANGE ROUTE AT NODE ID: " << forwardingStateColl->localNodeId << " WITH PREFERRED DOWNSTREAM PATH ID: " << preferredPaths.downLinkID << " FOR RSG ID: " << iter->first << endl;
			*log_failovers << Simulator::Now() << ", PERFORM CHANGE ROUTE AT NODE ID: " << forwardingStateColl->localNodeId << " WITH PREFERRED DOWNSTREAM PATH ID: " << preferredPaths.downLinkID << " FOR RSG ID: " << iter->first << endl;
		}

		if (preferredPaths.upLinkID != UINT_MAX && preferredPaths.upLinkID != linkLayerForwardingState.preferredUpPathId) {
			linkLayerForwardingState.preferredUpPathId = preferredPaths.upLinkID;
			linkLayerForwardingState.isUpPathInRspState = (preferredPaths.upLinkID != linkLayerForwardingState.primaryPathId);
			cout << "PERFORM CHANGE ROUTE AT NODE ID: " << forwardingStateColl->localNodeId << " WITH PREFERRED UPSTREAM PATH ID: " << preferredPaths.upLinkID << " FOR RSG ID: " << iter->first << endl;
			*log_failovers << Simulator::Now() << ", PERFORM CHANGE ROUTE AT NODE ID: " << forwardingStateColl->localNodeId << " WITH PREFERRED UPSTREAM PATH ID: " << preferredPaths.upLinkID << " FOR RSG ID: " << iter->first << endl;
		}
	}

}

// =======================================================================================================
}
}
