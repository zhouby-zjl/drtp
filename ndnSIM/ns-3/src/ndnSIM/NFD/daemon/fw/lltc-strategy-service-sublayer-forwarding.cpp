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

// ============================== Network Layer: Data Forwarding ===========================================
void LltcStrategy::onReceiveCapsule(const shared_ptr<pit::Entry>& pitEntry,
		const FaceEndpoint& ingress, const Data& data, CapsuleForwardingStates* ds, uint32_t rsgId, uint32_t pathId) {
	ForwardingStateForRSG* forwardingSublayerForwardingState = this->getForwardingState(rsgId);

	string pitPrefixStr = pitEntry->getName().toUri(name::UriFormat::DEFAULT);
	Name dataName = data.getName();

	CapsuleUri du = LltcMessagesHelper::parseCapsuleUri(pitEntry->getName(), dataName);

	if (du.dataId == 101) {
		cout << "here" << endl;
	}

	if (LltcConfig::LLTC_DISABLE_RETRAN) {
		this->deliverCapsule(pitEntry, dataName, data, du.dataId, rsgId);
		return;
	}


	bool isNewData = checkAndRecordRetransmittedDataId(ds, du.dataId, Simulator::Now());
	if (!isNewData) return;

	if (this->forwardingStateColl->localNodeId == 100 && du.dataId == 113) {
		cout << "here" << endl;
	}

	if (ds->lostDataIdsFromPrevRouters.find(du.dataId) != ds->lostDataIdsFromPrevRouters.end()) {
		ds->lostDataIdsFromPrevRouters.erase(du.dataId);
	} else {
		if (du.dataId > ds->lastDataId + 1) {
			discoverLostCapsules(ds, du.dataId, pitEntry, rsgId);
		}
		if (du.dataId > ds->lastDataId) {
			ds->lastDataId = du.dataId;
		}
	}

	cleanWaitForNextEvent(ds, du.dataId);
	cleanRetranInSingleEvents(ds, du.dataId, rsgId);
	cleanWaitForRequestMEvents(ds, du.dataId);

	this->deliverCapsule(pitEntry, dataName, data, du.dataId, rsgId);

	if (du.dataId == ds->lastDataId) {
		LltcDataID nextDataId = this->computeNextDataId(ds, ds->lastDataId);
		cout << "waitForNextData: " << nextDataId << " @ node ID: " << forwardingStateColl->localNodeId << endl;
		waitForNextCapsule(nextDataId, pitPrefixStr, pitEntry, ds,	nextDataId - ds->lastDataId, rsgId, 0);
	}

	std::cout << "received data: " << data.getName() << " at lltcNodeId: " <<
			forwardingStateColl->localNodeId << " with data ID " << du.dataId << endl;
}

void LltcStrategy::onReceiveCapsuleOnRsp(const shared_ptr<pit::Entry>& pitEntry,
		const FaceEndpoint& ingress, const Data& data, CapsuleForwardingStates* ds, uint32_t rsgId, uint32_t pathId) {
	Name dataName = data.getName();
	CapsuleOnRSPUri dbu = LltcMessagesHelper::parseCapsuleOnRSPUri(pitEntry->getName(), dataName);

	shared_ptr<Data> newData = LltcMessagesHelper::constructCapsule(dbu.prefix, dbu.dataId,
									data, dbu.isRetrans, dbu.requestedDataId);

	this->onReceiveCapsule(pitEntry, ingress, *newData, ds, rsgId, pathId);
}


void LltcStrategy::onReceiveDownSideControl(const shared_ptr<pit::Entry>& pitEntry,
								const FaceEndpoint& ingress, const Data& data, CapsuleForwardingStates* dls, uint32_t rsgId, uint32_t pathId) {
	this->deliverNonPitMessage(data, 0, rsgId);
}

void LltcStrategy::onReceiveUpSideControl(const shared_ptr<pit::Entry>& pitEntry,
								const FaceEndpoint& ingress, const Data& data, CapsuleForwardingStates* dls, uint32_t rsgId, uint32_t pathId) {
	this->deliverNonPitMessage(data, 1, rsgId);
}


void LltcStrategy::afterContentStoreHit(const shared_ptr<pit::Entry>& pitEntry,
		const FaceEndpoint& ingress, const Data& data) {
	this->sendData(pitEntry, data, ingress);
}

void LltcStrategy::deliverCapsule(const shared_ptr<pit::Entry>& pitEntry, Name& dataName,
								const Data& data, LltcDataID dataId, uint32_t rsgId) {
	ForwardingStateForRSG* forwardingSublayerForwardingState = this->getForwardingState(rsgId);

	if (forwardingSublayerForwardingState->isDownPathInRspState) {
		shared_ptr<Data> dataBypass = LltcMessagesHelper::constructCapsuleOnRSP(pitEntry->getName().toUri(name::UriFormat::DEFAULT),
														dataId, data, false, 0);
		LltcFIBEntry* feDown = this->lookupForwardingEntry(rsgId, forwardingSublayerForwardingState->preferredDownPathId, 0);
		this->sendLltcNonPitData(*dataBypass, FaceEndpoint(*feDown->face_nextHop, 0), rsgId,
														forwardingSublayerForwardingState->preferredDownPathId);
	} else {
		LltcFIBEntry* feDown = this->lookupForwardingEntry(rsgId, forwardingSublayerForwardingState->preferredDownPathId, 0);

		if (feDown != NULL) {
			this->sendLltcData(pitEntry, data, FaceEndpoint(*feDown->face_nextHop, 0), rsgId, forwardingSublayerForwardingState->preferredDownPathId);
		} else {
			int nRecords = pitEntry->getInRecords().size();
			if (nRecords > 0) {    // deliver to a registered application in the upper layer
				Face& inFace = pitEntry->getInRecords().begin()->getFace();
				this->sendLltcData(pitEntry, data, FaceEndpoint(inFace, 0), rsgId, forwardingSublayerForwardingState->primaryPathId);
			}
		}
	}
}

bool LltcStrategy::forwardInNonPitRegionIfNecessary(const shared_ptr<pit::Entry>& pitEntry, const Data& data,
								uint32_t rsgId, uint32_t pathId, int direction) {
	LltcFIBEntry* fe = this->lookupForwardingEntry(rsgId, pathId, direction);
	if (fe != NULL) {
		this->sendLltcNonPitData(data, FaceEndpoint(*fe->face_nextHop, 0), rsgId, pathId);
		return true;
	}
	return false;
}

void LltcStrategy::deliverNonPitMessage(const Data& data, int direction, uint32_t rsgId) {
	ForwardingStateForRSG* forwardingSublayerForwardingState = this->getForwardingState(rsgId);

	bool inRspState = direction == 0 ? forwardingSublayerForwardingState->isDownPathInRspState : forwardingSublayerForwardingState->isUpPathInRspState;
	uint64_t pathID;
	if (inRspState) {
		pathID =  direction == 0 ? forwardingSublayerForwardingState->preferredDownPathId : forwardingSublayerForwardingState->preferredUpPathId;
	} else {
		pathID = forwardingSublayerForwardingState->primaryPathId;
	}
	LltcFIBEntry* fe = this->lookupForwardingEntry(rsgId, pathID, direction);
	if (fe == NULL) {
		if (forwardingStateColl->appFace != NULL) {
			this->sendLltcNonPitData(data, FaceEndpoint(*forwardingStateColl->appFace, 0), rsgId, forwardingSublayerForwardingState->primaryPathId);
		}
	} else {
		this->sendLltcNonPitData(data, FaceEndpoint(*fe->face_nextHop, 0), rsgId, pathID);
	}
}

// ======================================================================================================

}
}
