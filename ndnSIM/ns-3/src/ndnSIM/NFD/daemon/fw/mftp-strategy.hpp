#ifndef SRC_NDNSIM_NFD_DAEMON_FW_MFTP_STRATEGY_HPP_
#define SRC_NDNSIM_NFD_DAEMON_FW_MFTP_STRATEGY_HPP_

#include "ns3/random-variable-stream.h"

#include "strategy.hpp"
#include "algorithm.hpp"
#include "common/global.hpp"
#include "common/logger.hpp"
#include "ndn-cxx/tag.hpp"
#include <list>
#include <unordered_map>

#include "generic-routes-manager.hpp"

using namespace nfd;
using namespace std;

namespace nfd {
namespace fw {

struct CsynEventState {
	uint32_t dataID;
	ns3::EventId retranEventID;
	uint32_t times;
};

class MftpStrategy : public Strategy {
public:
	MftpStrategy(Forwarder& forwarder, const Name& name = getStrategyName());
	virtual ~MftpStrategy() override;
    static const Name& getStrategyName();

    void afterReceiveInterest(const FaceEndpoint& ingress, const Interest& interest,
	                       const shared_ptr<pit::Entry>& pitEntry) override;

    void afterReceiveData(const shared_ptr<pit::Entry>& pitEntry,
                     const FaceEndpoint& ingress, const Data& data) override;
    void waitingCACK(CsynEventState* es, shared_ptr<Data> csyn, const shared_ptr<pit::Entry>& pitEntry);

    Face* lookupForwardingEntry(int direction);
    const Data* lookupInCs(string pitPrefix, uint32_t dataId);
    shared_ptr<Data> constructCSYN(string pitPrefixStr, list<uint32_t>* transDataIDs, uint32_t uid);
    shared_ptr<Data> constructCACK(string pitPrefixStr, list<uint32_t>* transDataIDs, uint32_t uid);
    void extractDataIDs(const Data& data, list<uint32_t>* dataIds);

    Cs* localCs;
    GenericRoutes* routes;
    unordered_set<uint32_t> transDataIDs;
    unordered_set<uint32_t> receivedDataIDs;
    unordered_map<uint32_t, CsynEventState*> csynStates;

    // map from node ID to a dict of <Message name, counter>
    static unordered_map<int, unordered_map<string, int>*> performance_res;
};

}
}

#endif /* SRC_NDNSIM_NFD_DAEMON_FW_MFTP_STRATEGY_HPP_ */
