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

// ===================================== Networks Layer: CS ===============================================
pair<LltcDataID, const Data*> LltcStrategy::lookupInCs(string pitPrefix, LltcDataID dataId, bool isExact) {
	Name lostDataName(LltcMessagesHelper::getCapsuleUri(pitPrefix, dataId));

	Cs::const_iterator iter = localCs->find(lostDataName);
	shared_ptr<Data> newData = nullptr;
	if (iter != localCs->end()) {
		return pair<LltcDataID, const Data*>(dataId, &iter->getData());
	} else if (!isExact) {
		pair<LltcDataID, const Name*> closestData = this->findClosestDataNameInCS(pitPrefix, dataId);

		if (closestData.first != CS_DATA_ID_NULL) {
			iter = localCs->find(*closestData.second);
			return  pair<LltcDataID, const Data*>(closestData.first, &iter->getData());
		}
	}
	return pair<LltcDataID, const Data*>(0, NULL);
}

pair<LltcDataID, const Name*> LltcStrategy::findClosestDataNameInCS(string pitPrefixStr, LltcDataID dataId) {
	stringstream ss;
	ss << pitPrefixStr << "/" << LLTC_MSG_OP_CAPSULE << "/";
	string matchStr = ss.str();
	uint32_t _minDataId = CS_DATA_ID_NULL;
	const Name* _minDataName = NULL;
	for (nfd::cs::Cs::const_iterator iter = localCs->begin(); iter != localCs->end(); ++iter) {
		const Name& _dataName = iter->getName();
		string _dataNameStr = _dataName.toUri(name::UriFormat::DEFAULT);
		if (_dataNameStr.substr(0, matchStr.length()).compare(matchStr) == 0) {
			string a = _dataNameStr.substr(matchStr.length());
			LltcDataID _dataId = 0;
			if (a.find("/") == a.npos) {
				_dataId = stoll(a);
			} else {
				_dataId = stoll(a.substr(0, a.find("/")));
			}

			//cout << "_dataName.get(_dataName.size() - 1).toUri(name::UriFormat::DEFAULT): " << _dataName.get(_dataName.size() - 1).toUri(name::UriFormat::DEFAULT) << endl;
			//cout << "_dataNameStr: " << _dataNameStr << endl;
			//LltcDataID _dataId = stoll(_dataName.get(_dataName.size() - 1).toUri(name::UriFormat::DEFAULT));
			if (_dataId > dataId) {
				if (_dataId < _minDataId) {
					_minDataId = _dataId;
					_minDataName = &_dataName;
				}
			}
		}
	}
	return pair<LltcDataID, const Name*>(_minDataId, _minDataName);
}

// =======================================================================================================

}
}
