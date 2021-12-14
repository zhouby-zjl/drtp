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

#include "lltc-messages-helper.hpp"
#include "ns3/nstime.h"
#include "ns3/ndnSIM/helper/ndn-stack-helper.hpp"
#include "ns3/ndnSIM/ndn-cxx/encoding/block-helpers.hpp"
#include <iostream>
#include <cmath>

using namespace nfd;
using namespace std;
using namespace ns3::ndn;

namespace nfd {
namespace fw {

shared_ptr<Data> LltcMessagesHelper::constructReport(string pitPrefixStr,
										list<LltcDataID>* lostDataIds, int nodeId, unsigned int reportUID) {
	stringstream ss;
	ss << pitPrefixStr << "/" << LLTC_MSG_OP_REPORT << "/" << nodeId << "/" << reportUID;
	size_t n = lostDataIds->size();
	size_t nBufBytes = sizeof(LltcDataID) * n + sizeof(size_t);
	uint8_t* bufBytes = new uint8_t[nBufBytes];
	size_t* nDataIdRegion = (size_t*) bufBytes;
	nDataIdRegion[0] = n;
	LltcDataID* dataIdRegion = (LltcDataID*) (bufBytes + sizeof(size_t));
	int i = 0;
	for (list<LltcDataID>::iterator iter = lostDataIds->begin(); iter != lostDataIds->end(); ++iter) {
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

shared_ptr<Data> LltcMessagesHelper::constructRequestS(string pitPrefixStr, LltcDataID lostDataId, int nodeId,
					bool findClosestDataIfCsMissed) {
	if (lostDataId == 227) {
		cout << "here" << endl;
	}
	stringstream ss;
	ss << pitPrefixStr << "/" << LLTC_MSG_OP_REQUEST_IN_SINGLE << "/" << lostDataId << "/" << nodeId << "/" <<
							(findClosestDataIfCsMissed ? "Exact" : "Closest");

	auto data = std::make_shared<Data>(ss.str());
	data->setFreshnessPeriod(time::milliseconds(1000));
	StackHelper::getKeyChain().sign(*data);
	return data;
}

shared_ptr<Data> LltcMessagesHelper::constructRequestM(string pitPrefixStr, RetransInMultipleID rib,
								list<LltcDataID>* lostDataIds, int nodeId,	bool findClosestDataIfCsMissed) {
	stringstream ss;
	ss << pitPrefixStr << "/" << LLTC_MSG_OP_REQUEST_IN_MULTIPLE << "/" << rib << "/" << nodeId << "/" <<
							(findClosestDataIfCsMissed ? "Exact" : "Closest");

	size_t n = lostDataIds->size();
	size_t nBufBytes = sizeof(LltcDataID) * n + sizeof(size_t);
	uint8_t* bufBytes = new uint8_t[nBufBytes];
	size_t* nDataIdRegion = (size_t*) bufBytes;
	nDataIdRegion[0] = n;
	LltcDataID* dataIdRegion = (LltcDataID*) (bufBytes + sizeof(size_t));
	int i = 0;
	for (list<LltcDataID>::iterator iter = lostDataIds->begin(); iter != lostDataIds->end(); ++iter) {
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

shared_ptr<Data> LltcMessagesHelper::constructEcho(string lltcPrefixStr, uint32_t echoId, uint32_t sourceNodeId) {
	stringstream ss;
	ss << lltcPrefixStr << "/" << LLTC_MSG_OP_ECHO << "/" << echoId << "/" << sourceNodeId << "/Unused";
	string prefix = ss.str();
	auto data = std::make_shared<Data>();
	data->setName(prefix);
	data->setFreshnessPeriod(time::milliseconds(1000));
	StackHelper::getKeyChain().sign(*data);
	return data;
}

shared_ptr<Data> LltcMessagesHelper::constructBack(string lltcPrefixStr, uint32_t echoId, uint32_t sourceNodeId, uint32_t destNodeId) {
	stringstream ss;
	ss << lltcPrefixStr << "/" << LLTC_MSG_OP_BACK << "/" << echoId << "/" << sourceNodeId << "/" << destNodeId;
	string prefix = ss.str();
	auto data = std::make_shared<Data>();
	data->setName(prefix);
	data->setFreshnessPeriod(time::milliseconds(1000));
	StackHelper::getKeyChain().sign(*data);
	return data;
}

shared_ptr<Data> LltcMessagesHelper::constructDownSideControl(string pitPrefixStr, uint32_t sourceNodeId, uint32_t destNodeId, shared_ptr<::ndn::Buffer> buf) {
	stringstream ss;
	ss << pitPrefixStr << "/" << LLTC_MSG_OP_DOWN_SIDE_CONTROL << "/" << sourceNodeId << "/" << destNodeId << "/Unused";
	string prefix = ss.str();
	auto data = std::make_shared<Data>();
	data->setName(prefix);
	data->setContent(buf);
	data->setFreshnessPeriod(time::milliseconds(1000));
	StackHelper::getKeyChain().sign(*data);
	return data;
}

shared_ptr<Data> LltcMessagesHelper::constructUpSideControl(string pitPrefixStr, uint32_t sourceNodeId, uint32_t destNodeId, shared_ptr<::ndn::Buffer> buf) {
	stringstream ss;
	ss << pitPrefixStr << "/" << LLTC_MSG_OP_UP_SIDE_CONTROL << "/" << sourceNodeId << "/" << destNodeId << "/Unused";
	string prefix = ss.str();
	auto data = std::make_shared<Data>();
	data->setName(prefix);
	data->setContent(buf);
	data->setFreshnessPeriod(time::milliseconds(1000));
	StackHelper::getKeyChain().sign(*data);
	return data;
}

void LltcMessagesHelper::extractReport(const Data& data, list<LltcDataID>* dataIds) {
	dataIds->clear();

	const Block& payload = data.getContent();
	const uint8_t* buf = payload.value();

	size_t* nDataIdRegion = (size_t*) buf;
	LltcDataID* dataIdRegion = (LltcDataID*) (buf + sizeof(size_t));
	size_t n = nDataIdRegion[0];

	for (size_t i = 0; i < n; ++i) {
		LltcDataID dataId = dataIdRegion[i];
		dataIds->push_back(dataId);
	}
}

shared_ptr<Data> LltcMessagesHelper::constructRetranS(string pitPrefixStr, LltcDataID lostDataID,
												uint32_t nodeId, int type, uint32_t closestDataID,
												const Data* origData, uint64_t time) {
	stringstream ss;
	ss.str("");
	if (type == DATA_RETRANS_IN_SINGLE_TYPE_EXACT) {
		ss << pitPrefixStr << "/" << LLTC_MSG_OP_RETRANS_IN_SINGLE << "/" << lostDataID << "/" << nodeId << "/ExactD" << time;
	} else if (type == DATA_RETRANS_IN_SINGLE_TYPE_CLOSEST) {
		ss << pitPrefixStr << "/" << LLTC_MSG_OP_RETRANS_IN_SINGLE << "/" << lostDataID << "/" << nodeId << "/Closest-" << closestDataID << "D" << time;
	} else if (type == DATA_RETRANS_IN_SINGLE_TYPE_MISS) {
		ss << pitPrefixStr << "/" << LLTC_MSG_OP_RETRANS_IN_SINGLE << "/" << lostDataID << "/" << nodeId << "/Miss";
	} else {
		return NULL;
	}

	shared_ptr<Data> newData = make_shared<Data>(string(ss.str()));

	if (type == DATA_RETRANS_IN_SINGLE_TYPE_EXACT || type == DATA_RETRANS_IN_SINGLE_TYPE_CLOSEST) {
		newData->setFreshnessPeriod(origData->getFreshnessPeriod());
		newData->setContent(origData->getContent());
	}
	StackHelper::getKeyChain().sign(*newData);
	return newData;
}

shared_ptr<Data> LltcMessagesHelper::constructRetranM(string pitPrefixStr, RetransInMultipleID bri,
						uint32_t nodeId, bool isExact, uint32_t dataId, uint32_t requestedDataId, const Data& origData) {
	stringstream ss;
	ss.str("");
	if (isExact) {
		ss << pitPrefixStr << "/" << LLTC_MSG_OP_RETRANS_IN_MULTIPLE << "/" << bri << "/" << nodeId << "/Exact-" << dataId;
	} else {
		ss << pitPrefixStr << "/" << LLTC_MSG_OP_RETRANS_IN_MULTIPLE << "/" << bri << "/" << nodeId << "/Closest-" << dataId << "-" << requestedDataId;
	}

	shared_ptr<Data> newData = make_shared<Data>(string(ss.str()));
	newData->setFreshnessPeriod(origData.getFreshnessPeriod());
	newData->setContent(origData.getContent());
	StackHelper::getKeyChain().sign(*newData);
	return newData;
}


string LltcMessagesHelper::getCapsuleUri(string pitPrefixStr, uint32_t dataId) {
	stringstream ss;
	ss.str("");
	ss << pitPrefixStr << "/" << LLTC_MSG_OP_CAPSULE << "/" << dataId;
	return ss.str();
}

shared_ptr<Data> LltcMessagesHelper::constructCapsule(string pitPrefixStr, LltcDataID dataID, const Data& origData,
												bool isRetrans, LltcDataID origDataID) {
	stringstream ss;
	ss.str("");
	if (isRetrans) {
		ss << pitPrefixStr << "/" << LLTC_MSG_OP_CAPSULE << "/" << dataID;// << "/Retrans/" << origDataID;
	} else {
		ss << pitPrefixStr << "/" << LLTC_MSG_OP_CAPSULE << "/" << dataID;// << "/Normal/Unused";
	}

	shared_ptr<Data> newData = make_shared<Data>(string(ss.str()));
	newData->setFreshnessPeriod(origData.getFreshnessPeriod());
	newData->setContent(origData.getContent());
	StackHelper::getKeyChain().sign(*newData);

	return newData;
}

shared_ptr<Data> LltcMessagesHelper::constructCapsuleOnRSP(string pitPrefixStr, LltcDataID dataID, const Data& origData,
												bool isRetrans, LltcDataID origDataID) {
	stringstream ss;
	ss.str("");

	if (isRetrans) {
		ss << pitPrefixStr << "/" << LLTC_MSG_OP_CAPSULE_ON_RSP << "/" << dataID << "/Retrans/" << origDataID;
	} else {
		ss << pitPrefixStr << "/" << LLTC_MSG_OP_CAPSULE_ON_RSP << "/" << dataID << "/Normal/Unused";
	}

	shared_ptr<Data> newData = make_shared<Data>(string(ss.str()));
	newData->setFreshnessPeriod(origData.getFreshnessPeriod());
	newData->setContent(origData.getContent());
	StackHelper::getKeyChain().sign(*newData);

	return newData;
}

RequestSUri LltcMessagesHelper::parseRequestSUri(const Name& prefixName, const Name& dataName) {
	RequestSUri rru;

	int nDataNameComponents = prefixName.size();
	string lostDataIdStr = dataName.get(nDataNameComponents + 1).toUri(name::UriFormat::DEFAULT);
	rru.lostDataId = (LltcDataID) stoull(lostDataIdStr);
	string sourceStr = dataName.get(nDataNameComponents + 2).toUri(name::UriFormat::DEFAULT);
	rru.sourceNodeId = stoll(sourceStr);
	rru.type = dataName.get(nDataNameComponents + 3).toUri(name::UriFormat::DEFAULT);
	rru.prefix = prefixName.toUri(name::UriFormat::DEFAULT);

	return rru;
}

RetranSUri LltcMessagesHelper::parseRetranSUri(const Name& prefixName, const Name& dataName) {
	RetranSUri dru;

	int nDataNameComponents = prefixName.size();
	dru.dataId = (LltcDataID) stoull(dataName.get(nDataNameComponents + 1).toUri(name::UriFormat::DEFAULT));
	dru.sourceNodeId = (uint32_t) stoll(dataName.get(nDataNameComponents + 2).toUri(name::UriFormat::DEFAULT));
	dru.prefix = prefixName.toUri(name::UriFormat::DEFAULT);
	dru.driftingDelta = 0;

	string type =  dataName.get(nDataNameComponents + 3).toUri(name::UriFormat::DEFAULT);
	if (type.substr(0, 5).compare("Exact") == 0) {
		dru.type = DATA_RETRANS_IN_SINGLE_TYPE_EXACT;
		dru.requestedDataId = dru.dataId;
		if (type.length() > 5 && type.substr(5, 1).compare("D") == 0) {
			dru.driftingDelta = stoll(type.substr(6, type.size() - 6));
		}
	} else if (type.substr(0, 8).compare("Closest-") == 0) {
		dru.type = DATA_RETRANS_IN_SINGLE_TYPE_CLOSEST;
		size_t pos = type.find("D");
		if (pos == string::npos) {
			dru.requestedDataId = (LltcDataID) stoll(type.substr(8, type.size() - 8));
		} else {
			dru.requestedDataId = (LltcDataID) stoll(type.substr(8, pos - 8));
			dru.driftingDelta = stoll(type.substr(pos + 1, type.size() - pos - 1));
		}
	} else if (type.substr(0, 4).compare("Miss") == 0) {
		dru.type = DATA_RETRANS_IN_SINGLE_TYPE_MISS;
		dru.requestedDataId = dru.dataId;
	}

	return dru;
}

RetranMUri LltcMessagesHelper::parseRetranMUri(const Name& prefixName, const Name& dataName) {
	RetranMUri dru;

	int nDataNameComponents = prefixName.size();
	dru.bri = (RetransInMultipleID) stoull(dataName.get(nDataNameComponents + 1).toUri(name::UriFormat::DEFAULT));
	dru.sourceNodeId = (uint32_t) stoll(dataName.get(nDataNameComponents + 2).toUri(name::UriFormat::DEFAULT));
	dru.prefix = prefixName.toUri(name::UriFormat::DEFAULT);

	string type =  dataName.get(nDataNameComponents + 3).toUri(name::UriFormat::DEFAULT);
	if (type.substr(0, 6).compare("Exact-") == 0) {
		dru.isExact = true;
		dru.dataId = (LltcDataID) stoll(type.substr(6, type.size() - 6));
		dru.requestedDataId = dru.dataId;
	} else if (type.substr(0, 8).compare("Closest-") == 0) {
		dru.isExact = false;
		string a = type.substr(8, type.size() - 8);
		size_t pos = a.find("-");
		dru.dataId = (LltcDataID) stoll(a.substr(0, pos));
		dru.requestedDataId = (LltcDataID) stoll(a.substr(pos + 1, a.length() - pos - 1));
	}

	return dru;
}

RequestMUri LltcMessagesHelper::parseRequestMUri(const Name& prefixName, const Name& dataName) {
	RequestMUri rrbu;

	int nDataNameComponents = prefixName.size();
	string lostDataIdStr = dataName.get(nDataNameComponents + 1).toUri(name::UriFormat::DEFAULT);
	rrbu.rbi = (RetransInMultipleID) stoull(lostDataIdStr);
	string sourceStr = dataName.get(nDataNameComponents + 2).toUri(name::UriFormat::DEFAULT);
	rrbu.sourceNodeId = stoll(sourceStr);
	rrbu.type = dataName.get(nDataNameComponents + 3).toUri(name::UriFormat::DEFAULT);
	rrbu.prefix = prefixName.toUri(name::UriFormat::DEFAULT);

	return rrbu;
}

CapsuleUri LltcMessagesHelper::parseCapsuleUri(const Name& prefixName, const Name& dataName) {
	CapsuleUri du;
	int nDataNameComponents = prefixName.size();
	du.dataId = (LltcDataID) stoull(dataName.get(nDataNameComponents + 1).toUri(name::UriFormat::DEFAULT));
	if (dataName.size() - nDataNameComponents == 4) {
		string type = dataName.get(nDataNameComponents + 2).toUri(name::UriFormat::DEFAULT);
		if (type.compare("Retrans") == 0) {
			du.isRetrans = true;
			du.requestedDataId = stoll(dataName.get(nDataNameComponents + 3).toUri(name::UriFormat::DEFAULT));
		}
	} else {
		du.isRetrans = false;
		du.requestedDataId = du.dataId;
	}
	du.prefix = prefixName.toUri(name::UriFormat::DEFAULT);
	return du;
}

ChangePathUri LltcMessagesHelper::parseChangePathUri(const Name& prefixName, const Name& dataName) {
	ChangePathUri cpu;

	int nDataNameComponents = prefixName.size();
	cpu.prefix = prefixName.toUri(name::UriFormat::DEFAULT);
	cpu.nodeIdToChangePath = stoll(dataName.get(nDataNameComponents + 1).toUri(name::UriFormat::DEFAULT));
	cpu.newPathId = stoll(dataName.get(nDataNameComponents + 2).toUri(name::UriFormat::DEFAULT));
	cpu.ununused = dataName.get(nDataNameComponents + 3).toUri(name::UriFormat::DEFAULT);

	return cpu;
}

ReportUri LltcMessagesHelper::parseReportUri(const Name& prefixName, const Name& dataName) {
	ReportUri rdru;

	int nDataNameComponents = prefixName.size();
	rdru.prefix = prefixName.toUri(name::UriFormat::DEFAULT);
	rdru.sourceNodeId = stoll(dataName.get(nDataNameComponents + 1).toUri(name::UriFormat::DEFAULT));
	rdru.reportUID = stoll(dataName.get(nDataNameComponents + 2).toUri(name::UriFormat::DEFAULT));

	return rdru;
}

EchoUri LltcMessagesHelper::parseEchoUri(const Name& lltcPrefixName, const Name& dataName) {
	EchoUri eru;

	int nDataNameComponents = lltcPrefixName.size();
	eru.echoId = stoll(dataName.get(nDataNameComponents + 1).toUri(name::UriFormat::DEFAULT));
	eru.sourceNodeId = stoll(dataName.get(nDataNameComponents + 2).toUri(name::UriFormat::DEFAULT));

	return eru;
}

BackUri LltcMessagesHelper::parseBackUri(const Name& lltcPrefixName, const Name& dataName) {
	BackUri ebu;

	int nDataNameComponents = lltcPrefixName.size();
	ebu.echoId = stoll(dataName.get(nDataNameComponents + 1).toUri(name::UriFormat::DEFAULT));
	ebu.sourceNodeId = stoll(dataName.get(nDataNameComponents + 2).toUri(name::UriFormat::DEFAULT));

	return ebu;
}


CapsuleOnRSPUri LltcMessagesHelper::parseCapsuleOnRSPUri(const Name& prefixName, const Name& dataName) {
	CapsuleOnRSPUri dbu;
	int nDataNameComponents = prefixName.size();
	dbu.dataId = (LltcDataID) stoull(dataName.get(nDataNameComponents + 1).toUri(name::UriFormat::DEFAULT));
	if (dataName.size() - nDataNameComponents == 4) {
		string type = dataName.get(nDataNameComponents + 2).toUri(name::UriFormat::DEFAULT);
		if (type.compare("Retrans") == 0) {
			dbu.isRetrans = true;
			dbu.requestedDataId = stoll(dataName.get(nDataNameComponents + 4).toUri(name::UriFormat::DEFAULT));
		} else if (type.compare("Normal") == 0) {
			dbu.isRetrans = false;
			dbu.requestedDataId = dbu.dataId;
		}
	} else {
		dbu.isRetrans = false;
		dbu.requestedDataId = dbu.dataId;
	}
	dbu.prefix = prefixName.toUri(name::UriFormat::DEFAULT);
	return dbu;
}

void LltcMessagesHelper::extractRequestM(const Data& data, list<LltcDataID>* lostDataIds) {
	lostDataIds->clear();

	const Block& payload = data.getContent();
	const uint8_t* buf = payload.value();

	size_t* nDataIdRegion = (size_t*) buf;
	LltcDataID* dataIdRegion = (LltcDataID*) (buf + sizeof(size_t));
	size_t n = nDataIdRegion[0];

	for (size_t i = 0; i < n; ++i) {
		LltcDataID dataId = dataIdRegion[i];
		lostDataIds->push_back(dataId);
	}
}

DownSideControlUri LltcMessagesHelper::parseDownSideControlUri(const Name& lltcPrefixName, const Name& dataName) {
	DownSideControlUri dsc;

	int nDataNameComponents = lltcPrefixName.size();
	dsc.sourceNodeId = stoll(dataName.get(nDataNameComponents + 1).toUri(name::UriFormat::DEFAULT));
	dsc.dstNodeId = stoll(dataName.get(nDataNameComponents + 2).toUri(name::UriFormat::DEFAULT));

	return dsc;
}

UpSideControlUri LltcMessagesHelper::parseUpSideControlUri(const Name& lltcPrefixName, const Name& dataName) {
	UpSideControlUri usc;

	int nDataNameComponents = lltcPrefixName.size();
	usc.sourceNodeId = stoll(dataName.get(nDataNameComponents + 1).toUri(name::UriFormat::DEFAULT));
	usc.dstNodeId = stoll(dataName.get(nDataNameComponents + 2).toUri(name::UriFormat::DEFAULT));

	return usc;
}

shared_ptr<Data> LltcMessagesHelper::constructLsa(string lltcPrefixStr, uint32_t lsaId, uint32_t sourceNodeId, uint32_t updateSeqNo, list<LinkState>* lsList) {
	stringstream ss;
	ss.str("");
	ss << lltcPrefixStr << "/" << LLTC_MSG_OP_LSA << "/" << lsaId << "/" << sourceNodeId << "/" << updateSeqNo;

	size_t n = lsList->size();
	size_t nBufBytes = sizeof(LinkState) * n + sizeof(size_t);
	uint8_t* bufBytes = new uint8_t[nBufBytes];
	size_t* nLsRegion = (size_t*) bufBytes;
	nLsRegion[0] = n;
	LinkState* dataIdRegion = (LinkState*) (bufBytes + sizeof(size_t));
	int i = 0;
	for (list<LinkState>::iterator iter = lsList->begin(); iter != lsList->end(); ++iter) {
		dataIdRegion[i].neighboredNodeId = iter->neighboredNodeId;
		dataIdRegion[i].status = iter->status;
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

LsaUri LltcMessagesHelper::parseLsaUri(const Name& lltcPrefixName, const Name& dataName) {
	LsaUri flu;

	int nDataNameComponents = lltcPrefixName.size();
	flu.lsaId = stoll(dataName.get(nDataNameComponents + 1).toUri(name::UriFormat::DEFAULT));
	flu.sourceNodeId = stoll(dataName.get(nDataNameComponents + 2).toUri(name::UriFormat::DEFAULT));
	flu.updateSeqNo = stoll(dataName.get(nDataNameComponents + 3).toUri(name::UriFormat::DEFAULT));

	return flu;
}

void LltcMessagesHelper::extractLsa(const Data& data, list<LinkState>* lsList) {
	lsList->clear();

	const Block& payload = data.getContent();
	const uint8_t* buf = payload.value();

	size_t* nDataIdRegion = (size_t*) buf;
	LinkState* lsRegion = (LinkState*) (buf + sizeof(size_t));
	size_t n = nDataIdRegion[0];

	for (size_t i = 0; i < n; ++i) {
		LinkState ls = lsRegion[i];
		lsList->push_back(ls);
	}
}

}
}
