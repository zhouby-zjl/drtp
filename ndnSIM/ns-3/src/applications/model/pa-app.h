/*
 * pa-app.h
 *
 *  Created on: 2016-6-2
 *      Author: zby
 */

#ifndef PA_APP_H_
#define PA_APP_H_

#include "ns3/address.h"
#include "ns3/application.h"
#include "ns3/event-id.h"
#include "ns3/ptr.h"
#include "ns3/traced-callback.h"
#include "ns3/socket.h"
#include "ns3/net-device.h"
#include "ns3/log.h"
#include "ns3/simulator.h"
#include "ns3/ipv4-header.h"
#include "ns3/tcp-header.h"
#include "ns3/packet.h"
#include "ns3/tag.h"
#include "ns3/random-variable-stream.h"

#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>

#include <vector>
#include <set>
#include <queue>

using namespace std;

namespace ns3 {

#define TOPO_VAL(a, b)  (*(topo + a * nNodes + b));


#define CACHE_QUEUE_N 			3
#define CACHE_AUTO_FLUSH_MS 	20

class PacketCacheQueue;
class PacketCached;
class PrevIdTag;
class PaList;

class FlowForwardModule {
public:
	enum {
		FS_IDLE,
		FS_UNICAST,
		FS_BROADCAST,
		FS_CACHE
	};

	FlowForwardModule(int* topo, int nNodes, int nodeId,
			uint32_t dstIpv4Addr, uint32_t sourceIpv4Addr,
			uint32_t _nTmpQueue, bool enableLog, string logFile);
	void processPacket(Ptr<NetDevice> dev, Ptr<const Packet> packet, uint16_t proto,
			const Address &receiver, NetDevice::PacketType type);
	void pathForward(Ptr<NetDevice> dev, Ptr<const Packet> packet, uint16_t proto,
			const Address &receiver, NetDevice::PacketType type, int* path, int nPath);
	~FlowForwardModule();

private:
	int getInterfaceNumber(int nextHopId);
	int getInterfaceNumber(int curId, int nextHopId);
	void tagPacket(Ptr<Packet> packet, uint32_t prevId);
	uint16_t getPacketPrevId(Ptr<const Packet> packet);
	void sendRetransmitAck(uint16_t lastNormId, uint16_t p_prevId, Ptr<NetDevice> dev, bool isAck);

	static void flushQueue(Ptr<NetDevice> eDev, PacketCacheQueue* queue, const Address& receiver, uint16_t proto);

	uint16_t 		  lastNormId, lastAckNormId;
	uint16_t 		  prevId, prevAckId;

	PacketCacheQueue*		tmpQueue;
	uint32_t 				nTmpQueue;

	int* 					topo;
	int 					nNodes;
	int 					nodeId;

	uint32_t 				dstIpv4Addr;
	uint32_t				sourceIpv4Addr;

	PaList*					paList;
	PaList*					ackPaList;

	ofstream*				logRecvId;
	ofstream*				logAckRecvId;
	ofstream*				logArrId;
	ofstream*				logAckArrId;
};

class PaList {
public:
	bool checkPacket(uint16_t id);

private:
	typedef struct Interval {
		uint16_t id;
		int len;
	} Interval;

	int findInterval(uint16_t id);
	void addId(int i, uint16_t id);
	bool hasId(int i, uint16_t id);
	Interval* createInterval(uint16_t id, int len);

	vector<Interval*> 		  		paList;
};

class PacketCached {
public:
	int m_id;
	Ptr<const Packet> m_packet;

	PacketCached(int id, Ptr<const Packet> packet) {
		m_id = id;
		m_packet = packet;
	}
};

struct PacketCachedCmp {
	bool operator () (PacketCached* x, PacketCached* y) {
		return x->m_id > y->m_id;
	}
};

class PrevIdTag : public Tag {
public:
	PrevIdTag() : prevId(0) {}

	static TypeId GetTypeId ()
	{
		static TypeId tid = TypeId ("ns3::PrevIdTag")
	    		   .SetParent<Tag> ()
				   .SetGroupName("Network")
				   ;
		return tid;
	}

	virtual TypeId GetInstanceTypeId () const { return GetTypeId (); }

	virtual uint32_t GetSerializedSize () const {
		return 2;
	}

	virtual void Serialize (TagBuffer i) const {
		i.WriteU16(prevId);
	}

	virtual void Deserialize (TagBuffer i) {
		prevId = i.ReadU16();
	}

	virtual void Print (std::ostream &os) const {
		os << prevId;
	}

	uint16_t getPrevId() {
		return prevId;
	}

	void setPrevid(uint16_t id) {
		prevId = id;
	}

private:
	uint16_t prevId;
};

class AckTag : public Tag {
public:
	AckTag() {}

	static TypeId GetTypeId ()
	{
		static TypeId tid = TypeId ("ns3::AckTag")
	    		   .SetParent<Tag> ()
				   .SetGroupName("Network")
				   ;
		return tid;
	}

	virtual TypeId GetInstanceTypeId () const { return GetTypeId (); }

	virtual uint32_t GetSerializedSize () const {
		return 4;
	}

	virtual void Serialize (TagBuffer i) const {
		i.WriteU32(sourceAddr);
		i.WriteU32(dstAddr);
		i.WriteU16(lastNormId);
		i.WriteU16(prevId);
	}

	virtual void Deserialize (TagBuffer i) {
		sourceAddr = i.ReadU32();
		dstAddr = i.ReadU32();
		lastNormId = i.ReadU16();
		prevId = i.ReadU16();
	}

	virtual void Print (std::ostream &os) const {
		os << "(" << lastNormId << "," << prevId << ")";
	}

	uint16_t getLastNormId() {
		return prevId;
	}

	void setLastNormId(uint16_t id) {
		prevId = id;
	}

	uint16_t getPrevId() {
		return prevId;
	}

	void setPrevid(uint16_t id) {
		prevId = id;
	}

	uint32_t getSourceAddr() {
		return sourceAddr;
	}

	void setSourceAddr(uint32_t _sourceAddr) {
		sourceAddr = _sourceAddr;
	}

	uint32_t getDstAddr() {
		return dstAddr;
	}

	void setDstAddr(uint32_t _dstAddr) {
		dstAddr = _dstAddr;
	}

private:
	uint32_t sourceAddr, dstAddr;
	uint16_t lastNormId, prevId;
};


class PacketCacheQueue {
public:
	enum Result {
		RES_FORWARD,
		RES_CACHE_WAIT_START,
		RES_CACHE_WAIT,
		RES_CACHE_FLUSH
	};

	PacketCacheQueue(int _nQueue, bool _enableLog) : prevId(-1), maxId(-1), n(0), file(NULL) {
		nQueue = _nQueue;
		enableLog = _enableLog;
	}

	~PacketCacheQueue() {
		file->close();
	}

	void setFile(string filePath) {
		file = new ofstream(filePath.c_str());
	}


	Result rollIn(int id, Ptr<const Packet> packet) {
		if (ipQueue.empty() && (prevId == -1 || prevId + 1 == id)) {
			prevId = id;
			if (enableLog) NS_LOG_UNCOND("forward id: " << id);

			if (file != NULL) {
				Ptr<Packet> pc = packet->Copy();
				Ipv4Header ih;
				pc->RemoveHeader(ih);
				TcpHeader th;
				pc->PeekHeader(th);
				*file << Simulator::Now().GetMicroSeconds() << "," << id << "," <<
						th.GetSequenceNumber().GetValue()  << "," <<
						th.GetAckNumber().GetValue() << "\n";

				file->flush();
			}

			return RES_FORWARD;
		}

		if (id > maxId) maxId = id;
		if (prevId == -1) prevId = id;

		ipQueue.push(new PacketCached(id, packet));
		++n;

		if (n >= maxId - prevId || n >= nQueue) {
			return RES_CACHE_FLUSH;
		} else if (n == 1){
			return RES_CACHE_WAIT_START;
		} else {
			return RES_CACHE_WAIT;
		}
	}

	Ptr<const Packet> rollOut() {
		if (n == 0) return NULL;

		PacketCached* p = ipQueue.top();
		ipQueue.pop();
		--n;
		if (n == 0) {
			maxId = -1;
			prevId = -1;
		}
		if (enableLog) NS_LOG_UNCOND("flush id: " << p->m_id);

		if (file != NULL) {
			Ptr<Packet> pc = p->m_packet->Copy();
			Ipv4Header ih;
			pc->RemoveHeader(ih);
			TcpHeader th;
			pc->PeekHeader(th);

			*file << Simulator::Now().GetMicroSeconds() << "," << p->m_id << "," <<
					th.GetSequenceNumber().GetValue()  << "," <<
					th.GetAckNumber().GetValue() << "\n";
			NS_LOG_UNCOND( "seq no: "<< th.GetSequenceNumber().GetValue() << ", ack no: " << th.GetAckNumber().GetValue());
			file->flush();
		}

		return p->m_packet;
	}

private:
	int prevId, maxId, n, nQueue;
	bool enableLog;
	priority_queue<PacketCached*, vector<PacketCached*>, PacketCachedCmp>   ipQueue;
	ofstream* file;
};

class PaApplication : public Application
{
public:

  static TypeId GetTypeId (void);
  PaApplication (uint32_t nodeId);
  PaApplication () {PaApplication(0);};
  virtual ~PaApplication ();


  void SetNode (Ptr<Node> node);
  int* getTopo();
  uint32_t getNodeId();
  uint32_t getnNodes();

  static void initializeForwardingModules(int* topo, uint32_t nNodes, uint32_t dstIpv4Addr, uint32_t sourceIpv4Addr);

  static void onDevRecv(Ptr<NetDevice> dev, Ptr<const Packet> packet, uint16_t proto,
		  const Address & sender, const Address &receiver, NetDevice::PacketType type);

  static void dumpDevTable(Ptr<Node> node);
  static void setNeedRes(bool enable);

  static bool					  needRes;

private:
  virtual void StartApplication (void);
  virtual void StopApplication (void);

  TypeId        		  m_tid;
  uint32_t				  m_nodeId;

  static uint32_t 				  m_dstIpv4Addr;
  static uint32_t				  m_sourceIpv4Addr;
  static int*			 		  m_topo;
  static uint32_t		  		  m_nnodes;
  static FlowForwardModule**		  ffMod;
  static Ptr<UniformRandomVariable> uniformRand;

};

}


#endif /* PA_APP_H_ */
