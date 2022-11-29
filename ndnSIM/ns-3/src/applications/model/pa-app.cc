#include "pa-app.h"

#include "ns3/log.h"
#include "ns3/ipv4-header.h"
#include "ns3/address.h"
#include "ns3/node.h"
#include "ns3/nstime.h"
#include "ns3/socket.h"
#include "ns3/simulator.h"
#include "ns3/socket-factory.h"
#include "ns3/packet.h"
#include "ns3/uinteger.h"
#include "ns3/trace-source-accessor.h"
#include "ns3/ipv4-raw-socket-factory.h"
#include "ns3/loopback-net-device.h"
#include "ns3/net-device.h"
#include "ns3/simple-net-device.h"
#include "ns3/tcp-header.h"
#include "ns3/tcp-l4-protocol.h"
#include "ns3/double.h"
#include "pa-app.h"

#include <fstream>
#include <sstream>
#include <iostream>
#include <string>
#include <stdlib.h>
#include <set>


namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("PaApplication");

NS_OBJECT_ENSURE_REGISTERED (PaApplication);

int*			 		   PaApplication::m_topo;
uint32_t		  		   PaApplication::m_nnodes;
uint32_t 				  PaApplication::m_dstIpv4Addr;
uint32_t				  PaApplication::m_sourceIpv4Addr;
FlowForwardModule**		   PaApplication::ffMod = NULL;
Ptr<UniformRandomVariable> PaApplication::uniformRand = CreateObject<UniformRandomVariable>();
bool						PaApplication::needRes = true;

ofstream* dropEventFile = new ofstream("pa-trans-dropevent.csv");
ofstream* normTcpFlowFile = new ofstream("pa-trans-norm-tcpflow-id.csv");
ofstream* normTcpAckFlowFile = new ofstream("pa-trans-norm-tcpackflow-id.csv");

TypeId
PaApplication::GetTypeId() {
	static TypeId tid = TypeId("ns3::PaApplication")
					.SetParent<Application>()
					.SetGroupName("Applications")
					.AddConstructor<PaApplication>();

	return tid;
}

void PaApplication::initializeForwardingModules(int* topo, uint32_t nNodes, uint32_t dstIpv4Addr, uint32_t sourceIpv4Addr) {
	m_topo = topo;
	m_nnodes = nNodes;
	m_dstIpv4Addr = dstIpv4Addr;
	m_sourceIpv4Addr = sourceIpv4Addr;

	ffMod = new FlowForwardModule*[nNodes];
	for (int i = 1; i < (int)nNodes - 1; ++i) {
		ffMod[i] = new FlowForwardModule(topo, nNodes, i,
				dstIpv4Addr, sourceIpv4Addr, CACHE_QUEUE_N, false, "");
	}

	ffMod[0] = new FlowForwardModule(topo, nNodes, 0,
			dstIpv4Addr, sourceIpv4Addr,CACHE_QUEUE_N, true, "pa-trans-tcpackflow-id.csv");
	ffMod[nNodes - 1] = new FlowForwardModule(topo, nNodes, nNodes - 1,
			dstIpv4Addr, sourceIpv4Addr,CACHE_QUEUE_N, true, "pa-trans-tcpflow-id.csv");
}

PaApplication::PaApplication(uint32_t _nodeId) {
	NS_LOG_FUNCTION (this);
	m_nodeId = _nodeId;

	m_topo = NULL;
	m_nnodes = 0;
}

PaApplication::~PaApplication() {
	NS_LOG_FUNCTION (this);
	dropEventFile->close();
	normTcpFlowFile->close();
	normTcpAckFlowFile->close();

	if (ffMod != NULL) {
		for (int i = 0; i < (int)m_nnodes; ++i) {
			delete ffMod[i];
		}
	}
}

void PaApplication::setNeedRes(bool enable) {
	PaApplication::needRes = enable;
}


void PaApplication::StartApplication() {
	NS_LOG_FUNCTION (this);
	GetNode()->RegisterProtocolHandler(MakeCallback(&onDevRecv), 2048, NULL, true);
}


void PaApplication::dumpDevTable(Ptr<Node> node) {
	NS_LOG_INFO("+===================+");
	for (int i = 0; i < (int)node->GetNDevices(); ++i) {
		TypeId ti = node->GetDevice(i)->GetTypeId();
		NS_LOG_INFO(i << ", uid: " << ti.GetUid() << ", type: " << ti.GetName());
	}
	NS_LOG_INFO("+===================+");
}

void PaApplication::onDevRecv(Ptr<NetDevice> dev, Ptr<const Packet> packet, uint16_t proto,
		const Address & sender, const Address &receiver, NetDevice::PacketType type)
{

	uniformRand->SetAttribute("Min", DoubleValue (0));
	uniformRand->SetAttribute("Max", DoubleValue (1.0));

	Ptr<Application> gApp = dev->GetNode()->GetApplication(0);
	Ptr<PaApplication> app = DynamicCast<PaApplication>(gApp);

	int nodeId = (int)app->getNodeId();
	Ipv4Header ih;
	packet->PeekHeader(ih);
	uint8_t protocol = ih.GetProtocol();
	uint32_t addr = ih.GetDestination().Get();

	/*
	if (!((nodeId == 0 && addr == m_dstIpv4Addr) ||
			(nodeId == (int)m_nnodes && addr == m_sourceIpv4Addr))) {
		double dropX = uniformRand->GetValue();

		if (dropX <= 0.02) { //0.02, 1 - 0.999589589
			uint32_t id = ih.GetIdentification();
			*dropEventFile << Simulator::Now().GetMicroSeconds() << "," << id << "," << nodeId << endl;
			dropEventFile->flush();
			return;
		}
	}*/

	int TYPE = 0;

	if (TYPE == 0) {
		if (protocol == 0x6 || protocol == 17) {
			ffMod[nodeId]->processPacket(dev, packet, proto, receiver, type);
			return;
		} else if (protocol == 0xff) { // our hop-by-hop ack

		}
		NS_LOG_UNCOND("unexpected protocol: " << protocol);
	} else if (TYPE == 1) {
		int path[4] = {0, 3, 6, 10};
		ffMod[nodeId]->pathForward(dev, packet, proto, receiver, type, path, 4);
	}


}


void PaApplication::StopApplication() {
	NS_LOG_FUNCTION (this);
}

uint32_t PaApplication::getNodeId() {
	return m_nodeId;
}

int* PaApplication::getTopo() {
	return m_topo;
}

uint32_t PaApplication::getnNodes() {
	return m_nnodes;
}

//===============================================================================

bool PaList::checkPacket(uint16_t id) {
	int idx = findInterval(id);
	if (hasId(idx, id)) {
		return false;
	} else {
		addId(idx, id);
		return true;
	}
}

int PaList::findInterval(uint16_t id) {
	int n = paList.size();
	if (n == 0 || paList[0]->id > id)
		return -1;

	int i = 0;
	for (i = 0; i < n; ++i) {
		Interval* inter = paList[i];
		if (inter->id > id) {
			break;
		}
	}

	return i - 1;
}

bool PaList::hasId(int i, uint16_t id) {
	if (i == -1 || i == (int)paList.size()) return false;
	Interval* inter = paList[i];
	return inter->id <= id && (inter->id + inter->len) >= id;
}

void PaList::addId(int i, uint16_t id) {
	if (i == -1) {
		Interval* inter = createInterval(id, 0);
		paList.insert(paList.begin(), inter);
	} else if (i == (int)paList.size()) {
		paList.push_back(createInterval(id, 0));
	} else {
		Interval* inter = paList[i];
		if (inter->id + inter->len > id) {
			return;
		} else if (inter->id + inter->len == id - 1) {
			inter->len++;
			if (i < (int)paList.size() - 1 && id == paList[i + 1]->id - 1) {
				inter->len += paList[i + 1]->len + 1;
				paList.erase(paList.begin() + i + 1);
			}
		} else if (i < (int)paList.size() - 1 && paList[i + 1]->id - 1 == id) {
			if (paList[i + 1]->id - inter->id - inter->len == 2) {
				inter->len += paList[i + 1]->len + 2;
				paList.erase(paList.begin() + i + 1);
			} else {
				paList[i + 1]->id--;
				paList[i + 1]->len++;
			}
		} else {
			paList.insert(paList.begin() + i + 1, createInterval(id, 0));
		}
	}
}

PaList::Interval* PaList::createInterval(uint16_t id, int len) {
	Interval* inter = new Interval;
	inter->id = id;
	inter->len = len;
	return inter;
}



//===============================================================================
FlowForwardModule::FlowForwardModule(int* _topo, int _nNodes, int _nodeId,
		uint32_t _dstIpv4Addr, uint32_t _sourceIpv4Addr,
		uint32_t _nTmpQueue, bool enableLog, string logFile) {
	nTmpQueue = _nTmpQueue;
	tmpQueue = new PacketCacheQueue(CACHE_QUEUE_N, enableLog);
	tmpQueue->setFile(logFile);

	topo = _topo;
	nNodes = _nNodes;
	nodeId = _nodeId;
	dstIpv4Addr = _dstIpv4Addr;
	sourceIpv4Addr = _sourceIpv4Addr;

	lastNormId = -1;
	lastAckNormId = -1;
	prevId = -1;
	prevAckId = -1;

	paList = new PaList();
	ackPaList = new PaList();

	stringstream ss;
	ss << "pa-trans-node-" << nodeId << ".csv";
	string s;
	ss >> s;
	logRecvId = new ofstream(s.c_str());
	ss.clear();
	s.clear();
	ss << "pa-trans-node-ack-" << nodeId << ".csv";
	ss >> s;
	logAckRecvId = new ofstream(s.c_str());

	ss.clear();
	s.clear();
	ss << "pa-trans-node-arr-" << nodeId << ".csv";
	ss >> s;
	logArrId = new ofstream(s.c_str());
	ss.clear();
	s.clear();
	ss << "pa-trans-node-ack-arr-" << nodeId << ".csv";
	ss >> s;
	logAckArrId = new ofstream(s.c_str());
}

FlowForwardModule::~FlowForwardModule() {
	delete tmpQueue;
	logRecvId->close();
	logAckRecvId->close();
	logArrId->close();
	logAckArrId->close();
}

void FlowForwardModule::processPacket(Ptr<NetDevice> dev, Ptr<const Packet> packet, uint16_t proto,
		const Address &receiver, NetDevice::PacketType type) {
	Ipv4Header ipHeader;
	packet->PeekHeader(ipHeader);
	uint16_t id = ipHeader.GetIdentification();
	uint32_t addr = ipHeader.GetDestination().Get();
	//uint16_t p_prevId = getPacketPrevId(packet);

	//if (addr == dstIpv4Addr && lastNormId != p_prevId) {
		//sendRetransmitAck(lastNormId, p_prevId, dev, false);
	//} else if (addr == sourceIpv4Addr && lastAckNormId != p_prevId) {
		//sendRetransmitAck(lastAckNormId, p_prevId, dev, true);
	//}

	bool doForward;
	if (addr == dstIpv4Addr) {
		doForward = paList->checkPacket(id);
		*logArrId << nodeId << "," << dev->GetIfIndex() << "," << Simulator::Now().GetMicroSeconds() << "," << id << endl;
	} else if (addr == sourceIpv4Addr) {
		doForward = ackPaList->checkPacket(id);
		*logAckArrId << nodeId << "," << dev->GetIfIndex() << "," << Simulator::Now().GetMicroSeconds() << "," << id << endl;
	}

	int cacheResult;
	int intf;
	Ptr<Packet> packetSending;
	Ptr<NetDevice> eDev;

	if (doForward) {
		if (addr == dstIpv4Addr) {
			*logRecvId << nodeId << "," << Simulator::Now().GetMicroSeconds() << "," << id << endl;
		} else if (addr == sourceIpv4Addr) {
			*logAckRecvId << nodeId << "," << Simulator::Now().GetMicroSeconds() << "," << id << endl;
		}
		if ((nodeId == nNodes - 1 && addr == dstIpv4Addr)
				|| (nodeId == 0 && addr == sourceIpv4Addr)) {
			intf = dev->GetNode()->GetNDevices() - 2;

			if (PaApplication::needRes) {
				cacheResult = tmpQueue->rollIn(id, packet);

				switch (cacheResult) {
				case PacketCacheQueue::RES_FORWARD:
					packetSending = packet->Copy();
					eDev = dev->GetNode()->GetDevice(intf);
					eDev->Send(packetSending, receiver, proto);
					break;
				case PacketCacheQueue::RES_CACHE_FLUSH:
					eDev = dev->GetNode()->GetDevice(intf);
					flushQueue(eDev, tmpQueue, receiver, proto);
					break;
				case PacketCacheQueue::RES_CACHE_WAIT_START:
					Simulator::Schedule(MilliSeconds(CACHE_AUTO_FLUSH_MS), flushQueue,
							dev->GetNode()->GetDevice(intf), tmpQueue, receiver, proto);
					break;
				default:
					NS_LOG_UNCOND("wait!!! id: " << ipHeader.GetIdentification() <<
							", nodeId: " << nodeId << ", addr: " << addr);
				}
			} else {
				packetSending = packet->Copy();
				eDev = dev->GetNode()->GetDevice(intf);
				eDev->Send(packetSending, receiver, proto);
			}

		} else {
			vector<int> bcIntf;

			if (addr == dstIpv4Addr) {
				for (int i = nodeId + 1; i < nNodes; ++i) {
					int t = TOPO_VAL(nodeId, i);
					if (t == 1) {
						int in = getInterfaceNumber(i);
						bcIntf.push_back(in);
					}
				}
			} else if (addr == sourceIpv4Addr) {
				for (int i = 0; i < nodeId; ++i) {
					int t = TOPO_VAL(i, nodeId);
					if (t == 1) {
						int in = getInterfaceNumber(i);
						bcIntf.push_back(in);
					}
				}
			}

			for (int i = 0; i < (int)bcIntf.size(); ++i) {
				int in = bcIntf[i];
				packetSending = packet->Copy();
				eDev = dev->GetNode()->GetDevice(in);
				eDev->Send(packetSending, receiver, proto);
			}
		}

	}


}

void FlowForwardModule::pathForward(Ptr<NetDevice> dev, Ptr<const Packet> packet, uint16_t proto,
		const Address &receiver, NetDevice::PacketType type, int* path, int nPath) {
		int p = 0;
		for (; p < nPath; ++p) {
			if (path[p] == nodeId) {
				break;
			}
		}

		Ipv4Header ipHeader;
		packet->PeekHeader(ipHeader);
		uint16_t id = ipHeader.GetIdentification();
		uint32_t addr = ipHeader.GetDestination().Get();
		int intf;

		if (addr == dstIpv4Addr) {
			if (p < nPath - 1) {
				intf = getInterfaceNumber(path[p], path[p + 1]);
				*logRecvId << nodeId << "," << Simulator::Now().GetSeconds() << "," << id << endl;
			} else {
				intf = dev->GetNode()->GetNDevices() - 2;   // the last is loopback
				NS_LOG_UNCOND("forward id: " << id);

				Ptr<Packet> pc = packet->Copy();
				Ipv4Header ih;
				pc->RemoveHeader(ih);
				TcpHeader th;
				pc->PeekHeader(th);
				*normTcpFlowFile << Simulator::Now().GetMicroSeconds() << "," << id << "," <<
							th.GetSequenceNumber().GetValue()  << "," <<
								th.GetAckNumber().GetValue() << "\n";
				normTcpFlowFile->flush();
			}
		} else if (addr == sourceIpv4Addr) {
			if (p > 0) {
				intf = getInterfaceNumber(path[p], path[p - 1]);
				*logAckRecvId << nodeId << "," << Simulator::Now().GetSeconds() << "," << id << endl;
			} else {
				intf = dev->GetNode()->GetNDevices() - 2;

				Ptr<Packet> pc = packet->Copy();
				Ipv4Header ih;
				pc->RemoveHeader(ih);
				TcpHeader th;
				pc->PeekHeader(th);
				*normTcpAckFlowFile << Simulator::Now().GetMicroSeconds() << "," << id << "," <<
								th.GetSequenceNumber().GetValue()  << "," <<
									th.GetAckNumber().GetValue() << "\n";
				normTcpAckFlowFile->flush();
			}
		}
		NS_ASSERT(intf >= 0);

		Ptr<Packet> packetSending;
		Ptr<NetDevice> eDev;

		packetSending = packet->Copy();
		eDev = dev->GetNode()->GetDevice(intf);
		eDev->Send(packetSending, receiver, proto);
}

void FlowForwardModule::sendRetransmitAck(uint16_t lastNormId, uint16_t p_prevId, Ptr<NetDevice> dev, bool isAck) {
	Ptr<Packet> ack = Create<Packet>();
	AckTag tag;
	if (isAck) {
		tag.setDstAddr(sourceIpv4Addr);
		tag.setSourceAddr(dstIpv4Addr);
	} else {
		tag.setDstAddr(dstIpv4Addr);
		tag.setSourceAddr(sourceIpv4Addr);
	}
	ack->AddPacketTag(tag);
	Ipv4Address addr("0.0.0.0");
	dev->Send(ack, addr, 0xff);
}

void FlowForwardModule::flushQueue(Ptr<NetDevice> eDev, PacketCacheQueue* queue, const Address& receiver, uint16_t proto) {
	Ptr<const Packet> p;
	while ((p = queue->rollOut()) != NULL) {
		Ptr<Packet> packetSending = p->Copy();
		eDev->Send(packetSending, receiver, proto);
	}
}

int FlowForwardModule::getInterfaceNumber(int nextHopId) {
	return getInterfaceNumber(nodeId, nextHopId);
}

int FlowForwardModule::getInterfaceNumber(int curId, int nextHopId) {
	if (curId < nextHopId) {
		int val = TOPO_VAL(curId, nextHopId);
		if (val == 0) {
			return -1;
		}
		int k = 0;
		for (int i = 0; i < curId; ++i) {
			k += TOPO_VAL(i, curId);
		}
		for (int i = curId + 1; i <= nextHopId; ++i) {
			val = TOPO_VAL(curId, i);
			k += val;
		}
		return k - 1;
	} else if (curId > nextHopId) {
		int val = TOPO_VAL(nextHopId, curId);
		if (val == 0) {
			return -1;
		}

		int k = 0;
		for (int i = 0; i <= nextHopId; ++i) {
			k += TOPO_VAL(i, curId);
		}

		return k - 1;
	} else {
		return -1;
	}
}

uint16_t FlowForwardModule::getPacketPrevId(Ptr<const Packet> packet) {
	uint16_t prevId = -1;
	PacketTagIterator ptIter = packet->GetPacketTagIterator();
	if (ptIter.HasNext()) {
		PrevIdTag t;
		ptIter.Next().GetTag(t);
		prevId = t.getPrevId();
	}
	return prevId;
}

void FlowForwardModule::tagPacket(Ptr<Packet> packet, uint32_t prevId) {
	PrevIdTag tag;
	tag.setPrevid(prevId);
	packet->AddPacketTag(tag);
}


}


/*
			// ack IDs of lost packets from the last hop
			if (nodeId != nNodes - 1 && nodeId != 0) {
				uint16_t prevId = getPacketPrevId(packet);
				bool firstPacket = false;
				if (prevId != -1) {
					if (addr == m_dstIpv4Addr) {
						if (inLastNormId != -1) {
							if (prevId == inLastNormId) {
								inLastNormId = id;
							} else {
								Packet ackPacket = packet->Copy();
								AckTag tag;
								tag.setLastNormId(inLastNormId);
								tag.setPrevid(prevId);
								packet->AddPacketTag(tag);
								Ipv4Address addr("0.0.0.0");
								dev->Send(packet, addr, 0xff);
							}
						} else {
							firstPacket = true;
							inLastNormId = id;
						}
					} else if (addr == m_sourceIpv4Addr) {

					}
				}
			} else {

			}

			// tag packet with prev id
			packet = packet->Copy();
			if (!firstPacket && prevId == -1) {
				PrevIdTag tag;
				tag.setPrevid(prevId);
				packet->AddPacketTag(tag);
			} else {

			}*/
