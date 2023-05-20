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

//========================== Network Layer: Retransmissions Control ========================================
// -------------------------------- Retransmissions In Single ---------------------------------------------
void LltcStrategy::onReceiveRequestS(const shared_ptr<pit::Entry>& pitEntry,
		const FaceEndpoint& ingress, const Data& data, CapsuleForwardingStates* ds, uint32_t rsgId, uint32_t pathId) {
	string pitPrefixStr = pitEntry->getName().toUri(name::UriFormat::DEFAULT);
	Name dataName = data.getName();
	RequestSUri rru = LltcMessagesHelper::parseRequestSUri(pitEntry->getName(), dataName);

	cout << "--> receive RequestS at node " << forwardingStateColl->localNodeId << " for lostData ID " << rru.lostDataId  << " from "
			<< rru.sourceNodeId << " along with the path " << pathId << endl;

	if (rru.lostDataId == 102) {
		cout << "here" << endl;
	}

//	uint64_t start_time = get_cur_time_ns();
	bool isExact = rru.type.compare("Closest") != 0;
	pair<LltcDataID, const Data*> dataFound = lookupInCs(pitPrefixStr, rru.lostDataId, isExact);

//	uint64_t time_ns = get_cur_time_ns() - start_time;

/*	if (LltcConfig::LLTC_PROCESSING_COSTS_LOG_ENABLE) {
		ofstream* logProcessingCosts = LltcLog::getLogProcessingCosts();
		*logProcessingCosts << this->forwardingStateColl->localNodeId << ",RequestS," << time_ns << endl;
	}*/


	if (dataFound.second == NULL) {
		shared_ptr<Data> newData = LltcMessagesHelper::constructRetranS(pitPrefixStr, dataFound.first, forwardingStateColl->localNodeId,
															DATA_RETRANS_IN_SINGLE_TYPE_MISS, rru.lostDataId, NULL, 0);
		this->sendLltcNonPitData(*newData, ingress, rsgId, pathId);
/*		cout << "rru.lostDataId: " << rru.lostDataId << endl;
		for (nfd::cs::Cs::const_iterator iter = localCs->begin(); iter != localCs->end(); ++iter) {
			const Name& _dataName = iter->getName();
			string _dataNameStr = _dataName.toUri(name::UriFormat::DEFAULT);
			cout << _dataNameStr << endl;
		}*/

	} else {
		uint64_t driftingDelta = Simulator::Now().GetMicroSeconds() - ds->transmittedDataIds.getDataIdAddTime(dataFound.first);
		shared_ptr<Data> newData = LltcMessagesHelper::constructRetranS(pitPrefixStr, dataFound.first, forwardingStateColl->localNodeId,
													isExact ? DATA_RETRANS_IN_SINGLE_TYPE_EXACT : DATA_RETRANS_IN_SINGLE_TYPE_CLOSEST,
													rru.lostDataId, dataFound.second, driftingDelta);

		this->sendLltcNonPitData(*newData, ingress, rsgId, pathId);
	}
}

void LltcStrategy::onReceiveRetranS(const shared_ptr<pit::Entry>& pitEntry,
		const FaceEndpoint& ingress, const Data& data, CapsuleForwardingStates* ds, uint32_t rsgId, uint32_t pathId) {
	Name dataName = data.getName();
	RetranSUri dru = LltcMessagesHelper::parseRetranSUri(pitEntry->getName(), dataName);

	cout << "--> receive RetranS at node " << forwardingStateColl->localNodeId << " for Data ID " << dru.dataId
				<< " along with path " << pathId << " from source node ID " << dru.sourceNodeId << "with type: " << dru.type << endl;

	if (dru.type == DATA_RETRANS_IN_SINGLE_TYPE_MISS) {
		// Here is logics to deal with the condition to check if there is a lost on the recently sent RetransReport message.
		// Accordingly, there is a possibility if the current router can retrieve the lost packet after a certain time.
		cleanRetranInSingleEvents(ds, dru.requestedDataId, rsgId);


		if (ds->waitForNextDataEvents.find(dru.dataId) != ds->waitForNextDataEvents.end() ||
				ds->transmittedDataIds.contains(dru.dataId)) {
			return;
		}

		ForwardingStateForRSG* forwardingSublayerForwardingState = this->getForwardingState(rsgId);
		ns3::Time timeoutInWaitingForNextData =	ns3::MicroSeconds(forwardingSublayerForwardingState->waitingTimeoutOnNextPacketForLostReport);

		ns3::EventId eventId = Simulator::Schedule(timeoutInWaitingForNextData,
				&LltcStrategy::waitForNextCapsuleDelayedTask, this, dru.dataId, pitEntry, ds, rsgId, false);
		ds->waitForNextDataEvents[dru.dataId].e = eventId;
		ds->waitForNextDataEvents[dru.dataId].reqTime = ns3::MicroSeconds(0);

		return;
	}

	bool isNewData = this->checkAndRecordRetransmittedDataId(ds, dru.dataId, Simulator::Now());
	if (!isNewData) return;

	uint64_t retransTime = (Simulator::Now() - ds->waitForNextDataEvents[dru.dataId].reqTime).GetMicroSeconds();

	cleanWaitForNextEvent(ds, dru.requestedDataId);
	cleanRetranInSingleEvents(ds, dru.requestedDataId, rsgId);

	shared_ptr<Data> newData = LltcMessagesHelper::constructCapsule(dru.prefix, dru.dataId,
															data, true, dru.requestedDataId);

	if (localCs->find(newData->getName()) == localCs->end()) {
		localCs->insert(*newData);
	}

	this->deliverCapsule(pitEntry, dataName, *newData, dru.requestedDataId, rsgId);
	cout << "--> convert DataRetransInSingle to Data at node " << forwardingStateColl->localNodeId << " for Data ID " << dru.dataId
				<< " along with path " << pathId << " from source node ID " << dru.sourceNodeId << endl;

	if (dru.dataId > ds->lastDataId) {
		ds->lastDataId = dru.dataId;
		LltcDataID nextDataId = this->computeNextDataId(ds, ds->lastDataId);

		waitForNextCapsule(nextDataId, dru.prefix,	pitEntry, ds, nextDataId - ds->lastDataId, rsgId, retransTime + dru.driftingDelta);
	}
}

// the function sends the control messages with the lost packets report. It waits and executes in each iteration.
void LltcStrategy::waitForNextCapsule(LltcDataID nextDataId, string pitPrefixStr,
									const shared_ptr<pit::Entry>& pitEntry,	CapsuleForwardingStates* ds,
									int nSkippedDatas, uint32_t rsgId, uint64_t penaltyTime) {
	if (ds->waitForNextDataEvents.find(nextDataId) != ds->waitForNextDataEvents.end()) {
		return;
	}

	ForwardingStateForRSG* forwardingSublayerForwardingState = this->getForwardingState(rsgId);

	ns3::Time timeoutInWaitingForNextData =
			ns3::MicroSeconds((LltcStrategyConfig::dataPiatInUs / (1.0 - LltcStrategyConfig::maxDriftRatioForPmuFreq)) *
					nSkippedDatas + forwardingSublayerForwardingState->waitingTimeoutOnNextPacket - penaltyTime);


	cout << "timeoutInWaitingForNextData: " << timeoutInWaitingForNextData.GetMicroSeconds() << endl;

	if (timeoutInWaitingForNextData.GetMicroSeconds() < 0) {
		timeoutInWaitingForNextData = ns3::MicroSeconds(0);
	}

	ns3::EventId eventId = Simulator::Schedule(timeoutInWaitingForNextData,
			&LltcStrategy::waitForNextCapsuleDelayedTask, this, nextDataId, pitEntry, ds, rsgId, true);
	ds->waitForNextDataEvents[nextDataId].e = eventId;
	ds->waitForNextDataEvents[nextDataId].reqTime = ns3::MicroSeconds(0);
}

void LltcStrategy::waitForNextCapsuleDelayedTask(LltcDataID lostDataId, const shared_ptr<pit::Entry>& pitEntry,
									CapsuleForwardingStates* ds, uint32_t rsgId, bool needToCheckLinkStates) {
	ForwardingStateForRSG* forwardingSublayerForwardingState = this->getForwardingState(rsgId);

	list<LltcDataID> lostDataIds;
	lostDataIds.push_back(lostDataId);
	string pitPrefixStr = pitEntry->getName().toUri(name::UriFormat::DEFAULT);

	sendReport(&lostDataIds, pitPrefixStr, pitEntry, forwardingSublayerForwardingState->preferredDownPathId, rsgId);

	ds->waitForNextDataEvents[lostDataId].reqTime = Simulator::Now();
	for (list<LltcFIBEntry*>::iterator iter = forwardingSublayerForwardingState->fibEntries.begin();
										iter != forwardingSublayerForwardingState->fibEntries.end(); ++iter) {
		LltcFIBEntry* fe = *iter;
		if (fe->direction != 1 || !fe->isRetransPoint) continue;
		RequestCallingParam rrcp;
		rrcp.restRetransRequests = LltcStrategyConfig::numRetransRequests;
		rrcp.findClosestDataIfCsMissed = false;
		rrcp.rsgId = rsgId;

		cout << "waitForNextDataDelayedTaskIterative: " << lostDataId << endl;
		waitForNextCapsuleDelayedTaskIterative(lostDataId, rrcp, pitEntry, fe, ds, fe->retransTimeout);

	}

	if (this->forwardingStateColl->localNodeId == 135 && lostDataId == 0) {
			cout << "here" << endl;
	}
}


void LltcStrategy::waitForNextCapsuleDelayedTaskIterative
(LltcDataID lostDataId, RequestCallingParam rrcp, const shared_ptr<pit::Entry>& pitEntry,
		LltcFIBEntry* fe, CapsuleForwardingStates* ds, bool waitForNextDataDelayedTask) {
	if (rrcp.restRetransRequests == 0) {
		if (waitForNextDataDelayedTask && LltcStrategyConfig::enableFailover) {
			checkLinkConnectivityInActiveManner(fe->face_nextHop->getId());
		}
		return;
	}
	string pitPrefixStr = pitEntry->getName().toUri(name::UriFormat::DEFAULT);
	shared_ptr<Data> rreqData = LltcMessagesHelper::constructRequestS(pitPrefixStr,
			lostDataId, forwardingStateColl->localNodeId, rrcp.findClosestDataIfCsMissed);
	sendLltcNonPitData(*rreqData, FaceEndpoint(*fe->face_nextHop, 0), rrcp.rsgId, fe->pathId);

	DataAndPathID dri = this->computeDataRetransID(lostDataId, fe->pathId);
	--rrcp.restRetransRequests;
	ns3::EventId eventId = Simulator::Schedule(ns3::MicroSeconds(fe->retransTimeout),
			&LltcStrategy::waitForNextCapsuleDelayedTaskIterative, this,
			lostDataId, rrcp, pitEntry, fe, ds, waitForNextDataDelayedTask);
	ds->retransInSingleEvents[dri].e = eventId;
	ds->retransInSingleEvents[dri].reqTime = Simulator::Now();

	list<retran_event>* retranEvents = LltcLog::getRetranEvents();
	retran_event re;
	re.nodeId = forwardingStateColl->localNodeId;
	re.rsgId = rrcp.rsgId;
	re.pathId = fe->pathId;
	re.dataId = lostDataId;
	re.type = 0;
	retranEvents->push_back(re);
}

bool LltcStrategy::cleanWaitForNextEvent(CapsuleForwardingStates* ds, LltcDataID dataId) {
	bool inWaitingForNextData = false;
	if (ds->waitForNextDataEvents.find(dataId) != ds->waitForNextDataEvents.end()) {
		ns3::EventId& e = ds->waitForNextDataEvents[dataId].e;
		Simulator::Remove(e);
		ds->waitForNextDataEvents.erase(dataId);
		inWaitingForNextData = true;
	}
	return inWaitingForNextData;
}

bool LltcStrategy::cleanRetranInSingleEvents(CapsuleForwardingStates* ds, LltcDataID dataId, uint32_t rsgId) {
	cout << "cleanDataRetransInSingleEvents for Data ID: " << dataId << endl;

	if (dataId == 389 && this->forwardingStateColl->localNodeId == 136) {
		cout << "here" << endl;
	}

	ForwardingStateForRSG* forwardingSublayerForwardingState = this->getForwardingState(rsgId);

	bool inRequesting = false;
	for (LltcFIBEntry* fe : forwardingSublayerForwardingState->fibEntries) {
		DataAndPathID dpi = this->computeDataRetransID(dataId, fe->pathId);

		if (ds->retransInSingleEvents.find(dpi) != ds->retransInSingleEvents.end()) {
			ns3::EventId& e = ds->retransInSingleEvents[dpi].e;
			Simulator::Remove(e);
			ds->retransInSingleEvents.erase(dpi);

			inRequesting = true;
		}
	}
	return inRequesting;
}


LltcDataID LltcStrategy::computeNextDataId(CapsuleForwardingStates* ds, LltcDataID curDataId) {
	LltcDataID nextDataIdForWaiting = curDataId + 1;
	while (ds->lostDataIdsFromPrevRouters.find(nextDataIdForWaiting) != ds->lostDataIdsFromPrevRouters.end() ||
			ds->transmittedDataIds.contains(nextDataIdForWaiting)) {
		++nextDataIdForWaiting;
	}
	return nextDataIdForWaiting;
}

// -------------------------------- Retransmissions In Batch ------------------------------------------------
void LltcStrategy::onReceiveRequestM(const shared_ptr<pit::Entry>& pitEntry,
		const FaceEndpoint& ingress, const Data& data, CapsuleForwardingStates* ds, uint32_t rsgId, uint32_t pathId) {

	string pitPrefixStr = pitEntry->getName().toUri(name::UriFormat::DEFAULT);
	Name dataName = data.getName();
	RequestMUri rrbu = LltcMessagesHelper::parseRequestMUri(pitEntry->getName(), dataName);

	list<LltcDataID> lostDataIds;
	LltcMessagesHelper::extractRequestM(data, &lostDataIds);

	cout << "--> receive RequestM at node " << forwardingStateColl->localNodeId << " for RetransInBatch ID " << rrbu.rbi  << " from "
			<< rrbu.sourceNodeId << " along with the path " << pathId << " with lostDataIds: ";
	for (LltcDataID _lostDataId : lostDataIds) {
		cout << _lostDataId << " ";
	}
	cout << endl;

	if (this->forwardingStateColl->localNodeId == 111 && rrbu.rbi == 0) {
		cout << "here" << endl;
	}

	bool isExact = rrbu.type.compare("Closest") != 0;
	for (LltcDataID lostDataId : lostDataIds) {
		Name lostDataName(LltcMessagesHelper::getCapsuleUri(pitPrefixStr, lostDataId));

		pair<LltcDataID, const Data*> dataFound = lookupInCs(pitPrefixStr, lostDataId, isExact);

		if (dataFound.second == NULL) {
			continue;
		}
		shared_ptr<Data> newData = LltcMessagesHelper::constructRetranM(pitPrefixStr, rrbu.rbi,
						forwardingStateColl->localNodeId, isExact, dataFound.first, lostDataId, *dataFound.second);

		this->sendLltcNonPitData(*newData, ingress, rsgId, pathId);
	}

}


void LltcStrategy::onReceiveRetranM(const shared_ptr<pit::Entry>& pitEntry,
		const FaceEndpoint& ingress, const Data& data, CapsuleForwardingStates* ds, uint32_t rsgId, uint32_t pathId) {
	Name dataName = data.getName();
	RetranMUri drbu = LltcMessagesHelper::parseRetranMUri(pitEntry->getName(), dataName);

	bool isNewData = this->checkAndRecordRetransmittedDataId(ds, drbu.dataId, Simulator::Now());
	if (!isNewData) return;
	cleanRetranMEvents(ds, drbu.bri, drbu.dataId);

	shared_ptr<Data> newData = LltcMessagesHelper::constructCapsule(drbu.prefix, drbu.dataId,
			data, true, drbu.requestedDataId);

	if (localCs->find(newData->getName()) == localCs->end()) {
		localCs->insert(*newData);
	}

	this->deliverCapsule(pitEntry, dataName, *newData, drbu.requestedDataId, rsgId);
	cout << "--> convert DataRetransInBatch to Data at node " << forwardingStateColl->localNodeId << " for Data ID " << drbu.dataId
				<< " along with path " << pathId << " from source node ID " << drbu.sourceNodeId << endl;

	if (drbu.dataId > ds->lastDataId) {
		ds->lastDataId = drbu.dataId;
		LltcDataID nextDataId = this->computeNextDataId(ds, ds->lastDataId);
		waitForNextCapsule(nextDataId, drbu.prefix, pitEntry, ds, nextDataId - ds->lastDataId, rsgId, 0);
	}
}


void LltcStrategy::broadcastRequestMAndWaitForRetranM
				(list<LltcDataID> lostDataIds, const shared_ptr<pit::Entry>& pitEntry, CapsuleForwardingStates* ds,
						bool findClosestDataIfCsMissed, uint32_t rsgId) {
	RetransInMultipleID rbi = ds->curRetransInBatchID++;
	RetransInMultipleEvent& rbe = getRetranMEvent(ds, rbi);
	for (LltcDataID lostDataId : lostDataIds) {
		rbe.retransDataIds.push_back(lostDataId);
	}

	this->broadcastRequestMAndWaitForRetranMRecursive(rbi, LltcStrategyConfig::numRetransRequests,
									findClosestDataIfCsMissed, pitEntry, ds, rsgId);
}

void LltcStrategy::broadcastRequestMAndWaitForRetranMRecursive
				(RetransInMultipleID rbi, int restRetransRequests, bool findClosestDataIfCsMissed,
						const shared_ptr<pit::Entry>& pitEntry,	CapsuleForwardingStates* ds, uint32_t rsgId) {
	ForwardingStateForRSG* forwardingSublayerForwardingState = this->getForwardingState(rsgId);

	if (restRetransRequests == 0) return;
	string pitPrefixStr = pitEntry->getName().toUri(name::UriFormat::DEFAULT);
	RetransInMultipleEvent& rbe = getRetranMEvent(ds, rbi);
	if (rbe.retransDataIds.size() == 0) {
		ds->retransInBatchEvents.erase(rbi);
		return;
	}

	shared_ptr<Data> rreqData = LltcMessagesHelper::constructRequestM(pitPrefixStr,
					rbi, &rbe.retransDataIds, forwardingStateColl->localNodeId, findClosestDataIfCsMissed);

	int maxRetransTimeoutInUs = -1;
	list<retran_event>* retranEvents = LltcLog::getRetranEvents();

	for (list<LltcFIBEntry*>::iterator iter = forwardingSublayerForwardingState->fibEntries.begin();
										iter != forwardingSublayerForwardingState->fibEntries.end(); ++iter) {
		LltcFIBEntry* fe = *iter;
		if (fe->direction != 1 || !fe->isRetransPoint) continue;

		sendLltcNonPitData(*rreqData, FaceEndpoint(*fe->face_nextHop, 0), rsgId, fe->pathId);
		if (fe->retransTimeout > maxRetransTimeoutInUs) {
			maxRetransTimeoutInUs = fe->retransTimeout;
		}

		for (uint32_t dataId : rbe.retransDataIds) {
			retran_event re;
			re.nodeId = forwardingStateColl->localNodeId;
			re.rsgId = rsgId;
			re.pathId = fe->pathId;
			re.dataId = dataId;
			re.type = 1;

			if (re.nodeId == 137 && re.dataId == 229) {
				cout << "here" << endl;
			}
			retranEvents->push_back(re);
		}
	}

	if (maxRetransTimeoutInUs == -1) {
		return;
	}

	restRetransRequests--;
	ns3::EventId eventId = Simulator::Schedule(ns3::MicroSeconds(maxRetransTimeoutInUs),
			&LltcStrategy::broadcastRequestMAndWaitForRetranMRecursive, this,
			rbi, restRetransRequests, findClosestDataIfCsMissed, pitEntry, ds, rsgId);

	rbe.e = eventId;
	rbe.time = Simulator::Now();
}

RetransInMultipleEvent& LltcStrategy::getRetranMEvent(CapsuleForwardingStates* ds, RetransInMultipleID rbi) {
	return ds->retransInBatchEvents[rbi];
}

void LltcStrategy::cleanWaitForRequestMEvents(CapsuleForwardingStates* ds, LltcDataID dataId) {
	for (list<WaitForRetransInBatchEvent>::iterator iter = ds->WaitForRetransInBatchEvents.begin();
			iter != ds->WaitForRetransInBatchEvents.end(); ++iter) {
		if (iter->dataIDs->find(dataId) != iter->dataIDs->end()) {
			iter->dataIDs->erase(dataId);
			if (iter->dataIDs->size() == 0) {
				Simulator::Remove(iter->e);
			}
			ds->WaitForRetransInBatchEvents.erase(iter);
			break;
		}
	}
}

void LltcStrategy::cleanRetranMEvents(CapsuleForwardingStates* ds, RetransInMultipleID bri,
											LltcDataID retransDataId) {
	if (ds->retransInBatchEvents.find(bri) != ds->retransInBatchEvents.end()) {
		RetransInMultipleEvent& rbe = getRetranMEvent(ds, bri);
		if (rbe.retransDataIds.size() == 0) {
			Simulator::Remove(rbe.e);
			ds->retransInBatchEvents.erase(bri);
		} else {
			list<LltcDataID>::const_iterator iter = std::find(rbe.retransDataIds.begin(),
													rbe.retransDataIds.end(), retransDataId);
			if (iter != rbe.retransDataIds.end()) {
				rbe.retransDataIds.erase(iter);
			}
		}
	}
}

void LltcStrategy::cleanRetranMEvents(CapsuleForwardingStates* ds, LltcDataID retransDataId) {
	list<RetransInMultipleID> eventsToRemove;
	for (auto& iter : ds->retransInBatchEvents) {
		auto iter2 = std::find(iter.second.retransDataIds.begin(), iter.second.retransDataIds.end(), retransDataId);
		if (iter2 != iter.second.retransDataIds.end()) {
			iter.second.retransDataIds.erase(iter2);
		}

		if (iter.second.retransDataIds.size() == 0) {
			eventsToRemove.push_back(iter.first);
			Simulator::Remove(iter.second.e);
		}
	}
	for (RetransInMultipleID id : eventsToRemove) {
		ds->retransInBatchEvents.erase(id);
	}
}

void LltcStrategy::discoverLostCapsules(CapsuleForwardingStates* ds, LltcDataID curDataId,
						const shared_ptr<pit::Entry>& pitEntry, uint32_t rsgId) {
	ForwardingStateForRSG* forwardingSublayerForwardingState = this->getForwardingState(rsgId);

	list<LltcDataID> newLostDataIDs;
	for (LltcDataID lostDataId = ds->lastDataId + 1; lostDataId < curDataId; ++lostDataId) {
		if (ds->lostDataIdsFromPrevRouters.find(lostDataId) == ds->lostDataIdsFromPrevRouters.end()) {
			if (ds->waitForNextDataEvents.find(lostDataId) != ds->waitForNextDataEvents.end() || ds->transmittedDataIds.contains(lostDataId)) {
				//if (ds->retransInSingleEvents.find(lostDataId) == ds->retransInSingleEvents.end()) {
					// clean those awaiting next data retransmission requests that are not yet send
					//this->cleanWaitForNextEvent(ds, lostDataId);
				//} else {
					// jump over those next data retransmission requests that have been sent
					//continue;
				//}
				continue;
			}
			newLostDataIDs.push_back(lostDataId);
		}
	}
	if (newLostDataIDs.size() > 0) {
		string pitPrefixStr = pitEntry->getName().toUri(name::UriFormat::DEFAULT);
		broadcastRequestMAndWaitForRetranM(newLostDataIDs, pitEntry, ds, true, rsgId);
		sendReport(&newLostDataIDs, pitPrefixStr, pitEntry, rsgId, forwardingSublayerForwardingState->preferredDownPathId);
	}
}

// ----------------------------- Retrans. or Lost Data State Report -----------------------------------------
void LltcStrategy::onReceiveReport(const shared_ptr<pit::Entry>& pitEntry,
		const FaceEndpoint& ingress, const Data& data, CapsuleForwardingStates* sls, uint32_t rsgId, uint32_t pathId) {
	ForwardingStateForRSG* forwardingSublayerForwardingState = this->getForwardingState(rsgId);

	list<LltcDataID> dataIdsInRetransPacketsReport;
	ReportUri rdru = LltcMessagesHelper::parseReportUri(pitEntry->getName(), data.getName());

	if (sls->recentDataRetransReportUIDs.find(rdru.reportUID) != sls->recentDataRetransReportUIDs.end()) {
		return;
	} else {
		sls->recentDataRetransReportUIDs.insert(rdru.reportUID);
	}

	LltcMessagesHelper::extractReport(data, &dataIdsInRetransPacketsReport);

	if (dataIdsInRetransPacketsReport.size() > 0) {
		unordered_set<LltcDataID>* newRetransDataIds_upNodes = setMinus(&dataIdsInRetransPacketsReport, &sls->lostDataIdsFromPrevRouters);

		if (newRetransDataIds_upNodes->size() > 0) {
			ForwardingStateForRSG* forwardingSublayerForwardingState = this->getForwardingState(rsgId);

			if (forwardingSublayerForwardingState->waitingTimeoutOnRetransReport.find(rdru.sourceNodeId) !=
					forwardingSublayerForwardingState->waitingTimeoutOnRetransReport.end()) {
				WaitForRetransInBatchEvent event;
				event.eventID = sls->curWaitForRetransInBatchEventID++;
				event.dataIDs = newRetransDataIds_upNodes;
				event.time = Simulator::Now();

				int timeout_waitingRetransFromUpRouter = forwardingSublayerForwardingState->waitingTimeoutOnRetransReport[rdru.sourceNodeId];

				EventId e = Simulator::Schedule(ns3::MicroSeconds(timeout_waitingRetransFromUpRouter),
							&LltcStrategy::waitToRequestM, this,
							event.eventID, newRetransDataIds_upNodes, pitEntry, sls, rsgId);

				event.e = e;
				sls->WaitForRetransInBatchEvents.push_back(event);
			} else {
				cout << "exception!" << endl;
			}
		}

		addToSet(&dataIdsInRetransPacketsReport, &sls->lostDataIdsFromPrevRouters);
		int64_t maxDataIdInWaiting = -1;

		for (LltcDataID lostDataId : sls->lostDataIdsFromPrevRouters) {
			if (sls->waitForNextDataEvents.find(lostDataId) != sls->waitForNextDataEvents.end()) {
				bool inWaitingForNextData = cleanWaitForNextEvent(sls, lostDataId);
				cleanRetranInSingleEvents(sls, lostDataId, rsgId);
				if (inWaitingForNextData) {
					if (lostDataId > maxDataIdInWaiting) {
						maxDataIdInWaiting = lostDataId;
					}
				}
			}
			cleanRetranMEvents(sls, lostDataId);
		}

		if (maxDataIdInWaiting != -1) {
			LltcDataID nextDataIdForWaiting = computeNextDataId(sls, maxDataIdInWaiting);
			string pitPrefixStr = pitEntry->getName().toUri(name::UriFormat::DEFAULT);

			waitForNextCapsule(nextDataIdForWaiting, pitPrefixStr, pitEntry, sls,
										nextDataIdForWaiting - maxDataIdInWaiting, rsgId, 0);
		}
	}

	LltcFIBEntry* fe = this->lookupForwardingEntry(rsgId, forwardingSublayerForwardingState->preferredDownPathId, 0);
	if (fe != NULL) {
		for (int i = 0; i < LltcStrategyConfig::numDataRetransReportsToSend; ++i) {
			this->sendLltcNonPitData(data, FaceEndpoint(*fe->face_nextHop, 0), rsgId, forwardingSublayerForwardingState->preferredDownPathId);
		}
	}
}

void LltcStrategy::waitToRequestM(uint32_t eventID, unordered_set<LltcDataID>* newRetransDataIds_upNodes,
					const shared_ptr<pit::Entry>& pitEntry, CapsuleForwardingStates* sls, uint32_t rsgId) {
	for (list<WaitForRetransInBatchEvent>::iterator iter = sls->WaitForRetransInBatchEvents.begin();
				iter != sls->WaitForRetransInBatchEvents.end(); ++iter) {
		if (iter->eventID == eventID) {
			sls->WaitForRetransInBatchEvents.erase(iter);
			break;
		}
	}

	list<LltcDataID> dataIDs;
	for (LltcDataID dataID : *newRetransDataIds_upNodes) {
		if (!sls->transmittedDataIds.contains(dataID)) {
			dataIDs.push_back(dataID);
		}
	}
	if (dataIDs.size() == 0) return;
	broadcastRequestMAndWaitForRetranM(dataIDs, pitEntry, sls, true, rsgId);
}

void LltcStrategy::addToSet(list<LltcDataID>* dataIDs, unordered_set<LltcDataID>* list) {
	for (std::list<LltcDataID>::iterator iter = dataIDs->begin(); iter != dataIDs->end(); ++iter) {
		LltcDataID dataId = *iter;
		list->insert(dataId);
	}
}

unordered_set<LltcDataID>* LltcStrategy::setMinus(list<LltcDataID>* dataIDs, unordered_set<LltcDataID>* list) {
	std::unordered_set<LltcDataID>* restDataIDs = new std::unordered_set<LltcDataID>();
	for (LltcDataID dataId : *dataIDs) {
		if (list->find(dataId) == list->end()) {
			restDataIDs->insert(dataId);
		}
	}
	return restDataIDs;
}

void LltcStrategy::sendReport(list<LltcDataID>* lostDataIDs, string pitPrefixStr,
		const shared_ptr<pit::Entry>& pitEntry, uint32_t rsgId, uint32_t pathId) {
	LltcFIBEntry* fe = lookupForwardingEntry(rsgId, pathId, 0);
	if (fe == NULL) return;
	Ptr<UniformRandomVariable> rand = CreateObject<UniformRandomVariable>();
	uint32_t uid = rand->GetInteger(0, UINT_MAX);
	for (int i = 0; i < LltcStrategyConfig::numDataRetransReportsToSend; ++i) {
		shared_ptr<Data> rreqData = LltcMessagesHelper::constructReport(
				pitPrefixStr, lostDataIDs, forwardingStateColl->localNodeId, uid);
		list<LltcDataID>::iterator lp = lostDataIDs->begin();
		if (*lp == 389 && this->forwardingStateColl->localNodeId == 138) {
			cout << "here" << endl;
		}

		sendLltcNonPitData(*rreqData, FaceEndpoint(*fe->face_nextHop, 0), rsgId, pathId);
	}
}


// --------------------------------------- State Management -------------------------------------------------
bool LltcStrategy::checkAndRecordRetransmittedDataId(CapsuleForwardingStates* ds, LltcDataID retransDataId, ns3::Time time) {
	if (this->forwardingStateColl->localNodeId == 83 && retransDataId == 0) {
		cout << "here!!" << endl;
	}
	if (ds->transmittedDataIds.contains(retransDataId)) {
		return false;
	}
	ds->transmittedDataIds.push(retransDataId, time.GetMicroSeconds());
	return true;
}

DataAndPathID LltcStrategy::computeDataRetransID(LltcDataID dataId, uint32_t pathId) {
	DataAndPathID x = ((uint64_t) dataId) << 32 | (uint64_t) pathId;
	return x;
}

CapsuleForwardingStates* LltcStrategy::createOrGetCapsuleForwardingStates(string prefix) {
	CapsuleForwardingStates* ds = nullptr;
	if (capsuleForwardingStates_map.find(prefix) == capsuleForwardingStates_map.end()) {
		ds = new CapsuleForwardingStates;
		ds->lastDataId = 0;
		ds->prefix = prefix;
		ds->transmittedDataIds.setLen(LltcStrategyConfig::queueSizeForTransmittedDataIds);
		ds->curRetransInBatchID = 0;
		ds->curWaitForRetransInBatchEventID = 0;
		capsuleForwardingStates_map[prefix] = ds;
	} else {
		ds = capsuleForwardingStates_map[prefix];
	}
	return ds;
}

// =========================================================================================================

}
}
