#include "ns3/nstime.h"
#include "ns3/ndnSIM/ndn-cxx/lp/tags.hpp"
#include "ns3/ndnSIM/helper/ndn-stack-helper.hpp"
#include "ns3/ndnSIM/ndn-cxx/encoding/block-helpers.hpp"
#include "ns3/ndnSIM/ndn-cxx/name.hpp"

#include "NFD/daemon/fw/forwarder.hpp"
#include "NFD/daemon/face/face-common.hpp"
#include "model/ndn-net-device-transport.hpp"
#include "NFD/daemon/table/pit-entry.hpp"
#include "NFD/daemon/table/pit-in-record.hpp"
#include "NFD/daemon/table/cs-policy-priority-fifo.hpp"

#include "ns3/ndnSIM/model/generic-log.hpp"

#include <algorithm>

#include "mftp-strategy.hpp"

using namespace nfd::cs;
using namespace nfd::cs::priority_fifo;
using namespace std;
using namespace ns3;
using namespace ns3::ndn;

namespace nfd {
namespace fw {

NFD_LOG_INIT(MftpStrategy);
NFD_REGISTER_STRATEGY(MftpStrategy);

unordered_map<int, unordered_map<string, int>*> MftpStrategy::performance_res;

MftpStrategy::MftpStrategy(Forwarder& forwarder, const Name& name) : Strategy(forwarder) {
	this->setInstanceName(makeInstanceName(name, getStrategyName()));
	routes = GenericRoutesManager::getRoutesByForwarder(&forwarder);
	localCs = &forwarder.getCs();
	MftpStrategy::performance_res[routes->nodeID] = new unordered_map<string, int>();
}

MftpStrategy::~MftpStrategy() {

}

const Name& MftpStrategy::getStrategyName() {
	static Name strategyName("ndn:/localhost/nfd/strategy/mftp/%FD%01");
	return strategyName;
}

void MftpStrategy::afterReceiveInterest(const FaceEndpoint& ingress, const Interest& interest,
		const shared_ptr<pit::Entry>& pitEntry) {
	std::cout << "received interest with nounce: " << interest.getNonce() << " at node: " << routes->nodeID << std::endl;

	if (hasPendingOutRecords(*pitEntry)) return;

	Face* face = this->lookupForwardingEntry(0);
	if (face == nullptr) {
		const fib::Entry& feLocal = this->lookupFib(*pitEntry);
		if (feLocal.getNextHops().size() > 0) {
			for (const auto& nexthop : feLocal.getNextHops()) {
				Face& outFace = nexthop.getFace();
				this->sendInterest(pitEntry, FaceEndpoint(outFace, 0), interest);
			}
		} else {
			this->rejectPendingInterest(pitEntry);
		}
		return;
	}

	if (MftpStrategy::performance_res.find(routes->nodeID) != MftpStrategy::performance_res.end()) {
		unordered_map<string, int>* res = MftpStrategy::performance_res[routes->nodeID];
		if (res->find("Interest") == res->end()) {
			(*res)["Interest"] = 1;
		} else {
			(*res)["Interest"] += 1;
		}
	}

	this->sendInterest(pitEntry, FaceEndpoint(*face, 0), interest);
}

void MftpStrategy::afterReceiveData(const shared_ptr<pit::Entry>& pitEntry,
		const FaceEndpoint& ingress, const Data& data) {
	Name dataName = data.getName();
	cout << "%%%%%%%>> " << dataName << " @ NodeID: " << routes->nodeID << ", time: " << Simulator::Now().GetSeconds() << endl;

	int nDataNameComponents = pitEntry->getName().size();
	string operationStr = dataName.get(nDataNameComponents).toUri(name::UriFormat::DEFAULT);
	string pitPrefixStr = pitEntry->getName().toUri(name::UriFormat::DEFAULT);
	stringstream ss;

	if (MftpStrategy::performance_res.find(routes->nodeID) != MftpStrategy::performance_res.end()) {
		unordered_map<string, int>* res = MftpStrategy::performance_res[routes->nodeID];
		if (res->find(operationStr) == res->end()) {
			(*res)[operationStr] = 1;
		} else {
			(*res)[operationStr] += 1;
		}
	}

	if (operationStr.compare("Data") == 0) {
		uint32_t dataID = stoull(dataName.get(nDataNameComponents + 1).toUri(name::UriFormat::DEFAULT));

		if (transDataIDs.find(dataID) != transDataIDs.end()) {
			return;
		}

		Face* face = lookupForwardingEntry(1);
		if (face == NULL) {
			this->sendData(pitEntry, data, FaceEndpoint(*routes->appFace, 0));
			transDataIDs.insert(dataID);
			return;
		}

		this->sendData(pitEntry, data, FaceEndpoint(*face, 0));
		transDataIDs.insert(dataID);

		Ptr<UniformRandomVariable> rand = CreateObject<UniformRandomVariable>();
		uint32_t uid = rand->GetInteger(0, UINT_MAX);

		list<uint32_t> transDataIDs;
		transDataIDs.push_back(dataID);
		shared_ptr<Data> csyn = constructCSYN(pitPrefixStr, &transDataIDs, uid);
		this->sendData(pitEntry, *csyn, FaceEndpoint(*face, 0));

		receivedDataIDs.insert(dataID);

		CsynEventState* es = new CsynEventState;
		es->dataID = dataID;
		es->times = 1;
		es->retranEventID = Simulator::Schedule(routes->roundTripTimeout, &MftpStrategy::waitingCACK, this, es, csyn, pitEntry);
		csynStates[uid] = es;


	} else if (operationStr.compare("CSYN") == 0) {
		uint32_t uid = stoull(dataName.get(nDataNameComponents + 1).toUri(name::UriFormat::DEFAULT));

		list<uint32_t> transDataIDs;
		extractDataIDs(data, &transDataIDs);
		uint32_t dataID = *transDataIDs.begin();
		list<uint32_t> recvDataID_cack;
		if (receivedDataIDs.find(dataID) != receivedDataIDs.end()) {
			recvDataID_cack.push_back(dataID);
		}
		shared_ptr<Data> cack = MftpStrategy::constructCACK(pitPrefixStr, &recvDataID_cack, uid);
		Face* face = lookupForwardingEntry(0);
		if (face != NULL) {
			this->sendData(pitEntry, *cack, FaceEndpoint(*face, 0));
		}

	} else if (operationStr.compare("CACK") == 0) {
		uint32_t uid = stoull(dataName.get(nDataNameComponents + 1).toUri(name::UriFormat::DEFAULT));
		list<uint32_t> recvDataIDs_cack;
		extractDataIDs(data, &recvDataIDs_cack);

		if (csynStates.find(uid) != csynStates.end()) {
			CsynEventState* es = csynStates[uid];
			Simulator::Remove(es->retranEventID);

			if (recvDataIDs_cack.size() > 0) {
				uint32_t dataID = *recvDataIDs_cack.begin();
				if (es->dataID == dataID) {
					csynStates.erase(uid);
					return;
				}
			}

			uint32_t lostDataID = es->dataID;
			if (es->times < routes->maxCSynTimes) {

				const Data* lostData = lookupInCs(pitPrefixStr, lostDataID);
				if (lostData != NULL) {
					Face* face = lookupForwardingEntry(1);
					if (face == NULL) return;

					this->sendData(pitEntry, *lostData, FaceEndpoint(*face, 0));
					transDataIDs.insert(lostDataID);

					list<uint32_t> transDataIDs;
					transDataIDs.push_back(lostDataID);
					Ptr<UniformRandomVariable> rand = CreateObject<UniformRandomVariable>();
					shared_ptr<Data> csyn = constructCSYN(pitPrefixStr, &transDataIDs, uid);
					this->sendData(pitEntry, *csyn, FaceEndpoint(*face, 0));

					++es->times;
					if (es->times < routes->maxCSynTimes) {
						es->retranEventID = Simulator::Schedule(routes->roundTripTimeout, &MftpStrategy::waitingCACK, this, es, csyn, pitEntry);
					}
				}
			}
		}

	}
}

void MftpStrategy::waitingCACK(CsynEventState* es, shared_ptr<Data> csyn, const shared_ptr<pit::Entry>& pitEntry) {
	Face* face = lookupForwardingEntry(1);
	if (face == NULL) return;
	++es->times;
	this->sendData(pitEntry, *csyn, FaceEndpoint(*face, 0));

	if (es->times < routes->maxCSynTimes) {
		es->retranEventID = Simulator::Schedule(routes->roundTripTimeout, &MftpStrategy::waitingCACK, this, es, csyn, pitEntry);
	}
}

Face* MftpStrategy::lookupForwardingEntry(int direction) {
	for (list<GenericFIBEntry*>::iterator iter = this->routes->fibEntries.begin(); iter != this->routes->fibEntries.end(); ++iter) {
		GenericFIBEntry* fe = *iter;
		if (fe->direction == direction) {
			return fe->face_nextHop;
		}
	}
	return NULL;
}

void MftpStrategy::extractDataIDs(const Data& data, list<uint32_t>* dataIds) {
	dataIds->clear();

	const Block& payload = data.getContent();
	const uint8_t* buf = payload.value();

	size_t* nDataIdRegion = (size_t*) buf;
	uint32_t* dataIdRegion = (uint32_t*) (buf + sizeof(size_t));
	size_t n = nDataIdRegion[0];

	for (size_t i = 0; i < n; ++i) {
		uint32_t dataId = dataIdRegion[i];
		dataIds->push_back(dataId);
	}
}

shared_ptr<Data> MftpStrategy::constructCSYN(string pitPrefixStr, list<uint32_t>* transDataIDs, uint32_t uid) {
	stringstream ss;
	ss << pitPrefixStr << "/CSYN/" << uid;
	size_t n = transDataIDs->size();
	size_t nBufBytes = sizeof(uint32_t) * n + sizeof(size_t);
	uint8_t* bufBytes = new uint8_t[nBufBytes];
	size_t* nDataIdRegion = (size_t*) bufBytes;
	nDataIdRegion[0] = n;
	uint32_t* dataIdRegion = (uint32_t*) (bufBytes + sizeof(size_t));
	int i = 0;
	for (list<uint32_t>::iterator iter = transDataIDs->begin(); iter != transDataIDs->end(); ++iter) {
		dataIdRegion[i] = *iter;
		++i;
	}

	auto data = std::make_shared<Data>(ss.str());
	data->setFreshnessPeriod(time::milliseconds(1000));
	shared_ptr<::ndn::Buffer> buf = std::make_shared<::ndn::Buffer>(nBufBytes);
	data->setContent(buf);
	for (size_t i = 0; i < nBufBytes; ++i) {
		(*buf)[i] = bufBytes[i];
	}
	StackHelper::getKeyChain().sign(*data);
	return data;
}

shared_ptr<Data> MftpStrategy::constructCACK(string pitPrefixStr, list<uint32_t>* transDataIDs, uint32_t uid) {
	stringstream ss;
	ss << pitPrefixStr << "/CACK/" << uid;
	size_t n = transDataIDs->size();
	size_t nBufBytes = sizeof(uint32_t) * n + sizeof(size_t);
	uint8_t* bufBytes = new uint8_t[nBufBytes];
	size_t* nDataIdRegion = (size_t*) bufBytes;
	nDataIdRegion[0] = n;
	uint32_t* dataIdRegion = (uint32_t*) (bufBytes + sizeof(size_t));
	int i = 0;
	for (list<uint32_t>::iterator iter = transDataIDs->begin(); iter != transDataIDs->end(); ++iter) {
		dataIdRegion[i] = *iter;
		++i;
	}

	auto data = std::make_shared<Data>(ss.str());
	data->setFreshnessPeriod(time::milliseconds(1000));
	shared_ptr<::ndn::Buffer> buf = std::make_shared<::ndn::Buffer>(nBufBytes);
	data->setContent(buf);
	for (size_t i = 0; i < nBufBytes; ++i) {
		(*buf)[i] = bufBytes[i];
	}
	StackHelper::getKeyChain().sign(*data);
	return data;
}

const Data* MftpStrategy::lookupInCs(string pitPrefix, uint32_t dataId) {
	stringstream ss;
	ss << pitPrefix << "/Data/" << dataId;

	Name lostDataName(ss.str());
	Cs::const_iterator iter = localCs->find(lostDataName);

	shared_ptr<Data> newData = nullptr;
	if (iter != localCs->end()) {
		return &iter->getData();
	} else {
		return NULL;
	}
}

}
}
