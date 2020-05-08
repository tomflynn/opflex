/*
 * Test suite for class TableDropStatsManager.
 *
 * Copyright (c) 2020 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <sstream>
#include <boost/test/unit_test.hpp>
#include <boost/assign/list_of.hpp>
#include <boost/lexical_cast.hpp>
<<<<<<< HEAD
=======
#include <boost/optional.hpp>
>>>>>>> origin/master

#include <opflexagent/test/ModbFixture.h>
#include "ovs-ofputil.h"
#include "AccessFlowManager.h"
#include "IntFlowManager.h"
#include "TableDropStatsManager.h"
#include "TableState.h"
#include "ActionBuilder.h"
<<<<<<< HEAD
#include "RangeMask.h"
=======
>>>>>>> origin/master
#include "FlowConstants.h"
#include "PolicyStatsManagerFixture.h"
#include "eth.h"
#include "ovs-ofputil.h"

extern "C" {
#include <openvswitch/ofp-parse.h>
#include <openvswitch/ofp-print.h>
}

using boost::optional;

using std::shared_ptr;
using std::string;
using opflex::modb::URI;
using namespace modelgbp::gbp;
using namespace modelgbp::gbpe;
using modelgbp::observer::PolicyStatUniverse;

namespace opflexagent {

<<<<<<< HEAD
=======
class MockIntTableDropStatsManager : public BaseTableDropStatsManager {
public:
    MockIntTableDropStatsManager(Agent* agent, IdGenerator& idGen,
                         SwitchManager& switchManager,
                         long timer_interval = 30000):
                             BaseTableDropStatsManager(agent, idGen,
                                     switchManager, timer_interval){};

    void testInjectTxnId (uint32_t txn_id) {
        std::lock_guard<mutex> lock(txnMtx);
        txns.insert(txn_id);
    }
};

class MockAccessTableDropStatsManager : public BaseTableDropStatsManager {
public:
    MockAccessTableDropStatsManager(Agent* agent, IdGenerator& idGen,
                         SwitchManager& switchManager,
                         long timer_interval = 30000):
                             BaseTableDropStatsManager(agent, idGen,
                                     switchManager, timer_interval) {};

    void testInjectTxnId (uint32_t txn_id) {
        std::lock_guard<mutex> lock(txnMtx);
        txns.insert(txn_id);
    }
};

class MockTableDropStatsManager {
public:
    MockTableDropStatsManager(Agent* agent,
                          IdGenerator& idGen,
                          SwitchManager& intSwitchManager,
                          SwitchManager& accSwitchManager,
                          long timer_interval = 30000):
                             intTableDropStatsMgr(agent,
                                 idGen, intSwitchManager,
                                 timer_interval),
                             accTableDropStatsMgr(agent,
                                 idGen, accSwitchManager,
                                 timer_interval) {}
    void start() {
        intTableDropStatsMgr.start(false);
        accTableDropStatsMgr.start(false);
    }

    void stop() {
        intTableDropStatsMgr.stop(false);
        accTableDropStatsMgr.stop(false);
    }

    void setAgentUUID(const std::string& uuid) {
        intTableDropStatsMgr.setAgentUUID(uuid);
        accTableDropStatsMgr.setAgentUUID(uuid);
    }

    void registerConnection(SwitchConnection* intConnection,
            SwitchConnection* accessConnection) {
        intTableDropStatsMgr.registerConnection(intConnection);
        if(accessConnection) {
            accTableDropStatsMgr.registerConnection(accessConnection);
        }
    }

    MockIntTableDropStatsManager intTableDropStatsMgr;
    MockAccessTableDropStatsManager accTableDropStatsMgr;
};

>>>>>>> origin/master
class TableDropStatsManagerFixture : public PolicyStatsManagerFixture {

public:
    TableDropStatsManagerFixture() : PolicyStatsManagerFixture(),
<<<<<<< HEAD
                                  intBridge(),
                                  accBridge(opflex_elem_t::INVALID_MODE,
                                            false),
                                  tableDropStatsManager(&agent, idGen,
                                            intBridge.switchManager,
                                            accBridge.switchManager, 2000000) {
        intBridge.switchManager.setMaxFlowTables(IntFlowManager::NUM_FLOW_TABLES);
        accBridge.switchManager.setMaxFlowTables(AccessFlowManager::NUM_FLOW_TABLES);
        tableDropStatsManager.setAgentUUID(agent.getUuid());
    }
    virtual ~TableDropStatsManagerFixture() {
        intBridge.stop();
        accBridge.stop();
    }

    PolicyStatsManagerFixture intBridge, accBridge;
    TableDropStatsManager tableDropStatsManager;
=======
                                  accPortConn(TEST_CONN_TYPE_ACC),
                                  intPortConn(TEST_CONN_TYPE_INT),
                                  accBr(agent, exec, reader,
                                        accPortMapper),
                                  intFlowManager(agent, switchManager, idGen,
                                                 ctZoneManager, tunnelEpManager),
                                  accFlowManager(agent, accBr, idGen,
                                                 ctZoneManager),
                                  pktInHandler(agent, intFlowManager),
                                  tableDropStatsManager(&agent, idGen,
                                            switchManager, accBr, 20000000) {
        tableDropStatsManager.setAgentUUID(agent.getUuid());
    }
    virtual ~TableDropStatsManagerFixture() {
        tableDropStatsManager.stop();
        accFlowManager.stop();
        intFlowManager.stop();
        stop();
    }
    void start() {
        FlowManagerFixture::start();
        intFlowManager.setEncapType(IntFlowManager::ENCAP_VXLAN);
        intFlowManager.start();
        accFlowManager.start();
        setConnected();
        tableDropStatsManager.registerConnection(&intPortConn,
                &accPortConn);
        tableDropStatsManager.start();
    }
    PolicyStatsManager &getIntTableDropStatsManager() {
        return tableDropStatsManager.intTableDropStatsMgr;
    }
    PolicyStatsManager &getAccTableDropStatsManager() {
        return tableDropStatsManager.accTableDropStatsMgr;
    }
    MockConnection accPortConn, intPortConn;
    PortMapper accPortMapper;
    MockSwitchManager accBr;
    IntFlowManager intFlowManager;
    AccessFlowManager accFlowManager;
    PacketInHandler pktInHandler;
    MockTableDropStatsManager tableDropStatsManager;
>>>>>>> origin/master

    void createIntBridgeDropFlowList(uint32_t table_id,
             FlowEntryList& entryList);
    void createAccBridgeDropFlowList(uint32_t table_id,
             FlowEntryList& entryList);
<<<<<<< HEAD
    void testOneStaticDropFlow(MockConnection& portConn,
                               uint32_t table_id,
                               PolicyStatsManager &statsManager,
                               SwitchManager &swMgr);
=======
    template <typename cStatsManager>
    void testOneStaticDropFlow(MockConnection& portConn,
                               uint32_t table_id,
                               PolicyStatsManager &statsManager,
                               SwitchManager &swMgr,
                               bool refresh_aging=false);
>>>>>>> origin/master
    void verifyDropFlowStats(uint64_t exp_packet_count,
                             uint64_t exp_byte_count,
                             uint32_t table_id,
                             MockConnection& portConn,
                             PolicyStatsManager &statsManager);

<<<<<<< HEAD
};

=======
#ifdef HAVE_PROMETHEUS_SUPPORT
    void checkPrometheusCounters(uint64_t exp_packet_count,
                                 uint64_t exp_byte_count,
                                 const std::string &bridgeName,
                                 const std::string &tableName);
#endif
};

#ifdef HAVE_PROMETHEUS_SUPPORT
void TableDropStatsManagerFixture::checkPrometheusCounters(uint64_t exp_packet_count,
                             uint64_t exp_byte_count,
                             const std::string &bridgeName,
                             const std::string &tableName) {
    const string& output = BaseFixture::getOutputFromCommand(cmd);
    const string& promCtr= bridgeName+ "_" +tableName;
    string packets_key = "opflex_table_drop_packets{table=\"" + promCtr + "\"} "
        + boost::lexical_cast<std::string>(exp_packet_count) + ".000000";
    string bytes_key = "opflex_table_drop_bytes{table=\"" + promCtr + "\"} "
        + boost::lexical_cast<std::string>(exp_byte_count) + ".000000";
    string::size_type pos = output.find(packets_key);
    BOOST_CHECK_NE(pos, string::npos);
    pos = output.find(bytes_key);
    BOOST_CHECK_NE(pos, string::npos);
    return;
}
#endif

>>>>>>> origin/master
void TableDropStatsManagerFixture::verifyDropFlowStats (
                                    uint64_t exp_packet_count,
                                    uint64_t exp_byte_count,
                                    uint32_t table_id,
                                    MockConnection& portConn,
                                    PolicyStatsManager &statsManager) {
    optional<shared_ptr<PolicyStatUniverse>> su =
                PolicyStatUniverse::resolve(agent.getFramework());
<<<<<<< HEAD
    optional<shared_ptr<TableDropCounter>> tableDropCounter;
=======
    optional<shared_ptr<TableDropCounter>> tableDropCounter
        = boost::make_optional<shared_ptr<TableDropCounter>>(false,
                shared_ptr<TableDropCounter>(nullptr));
>>>>>>> origin/master
    if (su) {
        tableDropCounter = su.get()->resolveGbpeTableDropCounter(
                                        agent.getUuid(),
                                        portConn.getSwitchName(),
                                        table_id);
        WAIT_FOR_DO_ONFAIL(
                   (tableDropCounter && tableDropCounter.get()
                        && (tableDropCounter.get()->getPackets().get()
                                == exp_packet_count)
                        && (tableDropCounter.get()->getBytes().get()
                                == exp_byte_count)),
                   500, // usleep(1000) * 500 = 500ms
                   tableDropCounter = su.get()->resolveGbpeTableDropCounter(
                                            agent.getUuid(),
                                            portConn.getSwitchName(),
                                            table_id),
                   if (tableDropCounter && tableDropCounter.get()) {
                       BOOST_CHECK_EQUAL(
                               tableDropCounter.get()->getPackets().get(),
                               exp_packet_count);
                       BOOST_CHECK_EQUAL(
                               tableDropCounter.get()->getBytes().get(),
                               exp_byte_count);
                   } else {
                       LOG(DEBUG) << "TableDropCounter mo for "
                                  << portConn.getSwitchName()
                                  << "-" << table_id
                                  << " isn't present";
                   });
    }
}

void TableDropStatsManagerFixture::createAccBridgeDropFlowList(
        uint32_t table_id,
        FlowEntryList& entryList ) {
    FlowBuilder().priority(0).cookie(flow::cookie::TABLE_DROP_FLOW)
            .flags(OFPUTIL_FF_SEND_FLOW_REM)
            .action().dropLog(table_id)
            .go(AccessFlowManager::EXP_DROP_TABLE_ID)
            .parent().build(entryList);
}

void TableDropStatsManagerFixture::createIntBridgeDropFlowList(
        uint32_t table_id,
        FlowEntryList& entryList ) {
    if(table_id == IntFlowManager::SEC_TABLE_ID) {
        FlowBuilder().priority(25).cookie(flow::cookie::TABLE_DROP_FLOW)
                .flags(OFPUTIL_FF_SEND_FLOW_REM)
                .ethType(eth::type::ARP)
                .action().dropLog(IntFlowManager::SEC_TABLE_ID)
                .go(IntFlowManager::EXP_DROP_TABLE_ID).parent()
                .build(entryList);
        FlowBuilder().priority(25).cookie(flow::cookie::TABLE_DROP_FLOW)
                .flags(OFPUTIL_FF_SEND_FLOW_REM)
                .ethType(eth::type::IP)
                .action().dropLog(IntFlowManager::SEC_TABLE_ID)
                .go(IntFlowManager::EXP_DROP_TABLE_ID).parent()
                .build(entryList);
        FlowBuilder().priority(25).cookie(flow::cookie::TABLE_DROP_FLOW)
                .flags(OFPUTIL_FF_SEND_FLOW_REM)
                .ethType(eth::type::IPV6)
                .action().dropLog(IntFlowManager::SEC_TABLE_ID)
                .go(IntFlowManager::EXP_DROP_TABLE_ID).parent()
                .build(entryList);
    }
    FlowBuilder().priority(0).cookie(flow::cookie::TABLE_DROP_FLOW)
            .flags(OFPUTIL_FF_SEND_FLOW_REM)
            .action().dropLog(table_id)
            .go(IntFlowManager::EXP_DROP_TABLE_ID)
            .parent().build(entryList);

}

<<<<<<< HEAD
=======
template <typename cStatsManager>
>>>>>>> origin/master
void TableDropStatsManagerFixture::testOneStaticDropFlow (
        MockConnection& portConn,
        uint32_t table_id,
        PolicyStatsManager &statsManager,
<<<<<<< HEAD
	SwitchManager &swMgr)
{
    uint64_t expected_pkt_count = INITIAL_PACKET_COUNT,
            expected_byte_count = INITIAL_PACKET_COUNT * PACKET_SIZE;
=======
        SwitchManager &swMgr,
        bool refresh_aged_flow)
{
    uint64_t expected_pkt_count = INITIAL_PACKET_COUNT*2,
            expected_byte_count = INITIAL_PACKET_COUNT*2*PACKET_SIZE;
>>>>>>> origin/master
    FlowEntryList dropLogFlows;
    if(portConn.getSwitchName()=="int_conn") {
        createIntBridgeDropFlowList(table_id,
                dropLogFlows);
        if(table_id == IntFlowManager::SEC_TABLE_ID){
            expected_pkt_count *= 4;
            expected_byte_count *=4;
        }
    } else {
        createAccBridgeDropFlowList(table_id,
                        dropLogFlows);
    }
<<<<<<< HEAD
    FlowEntryList entryListCopy(dropLogFlows);
    swMgr.writeFlow("DropLogStatic", table_id, entryListCopy);
    // Call on_timer function to process the flow entries received from
    // switchManager.
    boost::system::error_code ec;
    ec = make_error_code(boost::system::errc::success);
    statsManager.on_timer(ec);
    LOG(DEBUG) << "Called on_timer";
=======
    int ctr = 1;
    if(refresh_aged_flow) {
        ctr++;
    }
    // Call on_timer function to process the flow entries received from
    // switchManager.
    boost::system::error_code ec;
    do{
        ec = make_error_code(boost::system::errc::success);
        statsManager.on_timer(ec);
        LOG(DEBUG) << "Called on_timer";
        ctr--;
    } while(ctr>0);
>>>>>>> origin/master
    // create first flow stats reply message
    struct ofpbuf *res_msg =
        makeFlowStatReplyMessage_2(&portConn,
                                   INITIAL_PACKET_COUNT,
                                   table_id,
                                   dropLogFlows);
    LOG(DEBUG) << "1 makeFlowStatReplyMessage created";
    BOOST_REQUIRE(res_msg!=0);
    ofp_header *msgHdr = (ofp_header *)res_msg->data;
<<<<<<< HEAD
    statsManager.testInjectTxnId(msgHdr->xid);
=======
    cStatsManager* pSM = dynamic_cast<cStatsManager*>(&statsManager);
    pSM->testInjectTxnId(msgHdr->xid);
>>>>>>> origin/master

    // send first flow stats reply message
    statsManager.Handle(&portConn,
                         OFPTYPE_FLOW_STATS_REPLY, res_msg);
    LOG(DEBUG) << "1 FlowStatsReplyMessage handled";
    ofpbuf_delete(res_msg);
<<<<<<< HEAD

=======
>>>>>>> origin/master
    ec = make_error_code(boost::system::errc::success);
    statsManager.on_timer(ec);
    LOG(DEBUG) << "Called on_timer";

    // create second flow stats reply message
    res_msg =
        makeFlowStatReplyMessage_2(&portConn,
                                   INITIAL_PACKET_COUNT*2,
                                   table_id,
                                   dropLogFlows);
    LOG(DEBUG) << "2 makeFlowStatReplyMessage created";
    BOOST_REQUIRE(res_msg!=0);
    msgHdr = (ofp_header *)res_msg->data;
<<<<<<< HEAD
    statsManager.testInjectTxnId(msgHdr->xid);
=======
    pSM->testInjectTxnId(msgHdr->xid);
>>>>>>> origin/master

    // send second flow stats reply message
    statsManager.Handle(&portConn,
                         OFPTYPE_FLOW_STATS_REPLY, res_msg);
    LOG(DEBUG) << "2 FlowStatsReplyMessage handled";
    ofpbuf_delete(res_msg);

    ec = make_error_code(boost::system::errc::success);
    statsManager.on_timer(ec);
    LOG(DEBUG) << "Called on_timer";

    verifyDropFlowStats(expected_pkt_count,
                        expected_byte_count,
                        table_id, portConn, statsManager);
<<<<<<< HEAD
    LOG(DEBUG) << "FlowStatsReplyMessage verification successful";
=======
#ifdef HAVE_PROMETHEUS_SUPPORT
    SwitchManager::TableDescriptionMap fwdTableMap;
    swMgr.getForwardingTableList(fwdTableMap);
    checkPrometheusCounters(expected_pkt_count,
                            expected_byte_count,
                            portConn.getSwitchName(),
                            fwdTableMap[table_id].first);
#endif
>>>>>>> origin/master

}

BOOST_AUTO_TEST_SUITE(TableDropStatsManager_test)

BOOST_FIXTURE_TEST_CASE(testStaticDropFlowsInt, TableDropStatsManagerFixture) {
<<<<<<< HEAD
    MockConnection accPortConn(TEST_CONN_TYPE_ACC);
    MockConnection intPortConn(TEST_CONN_TYPE_INT);
    tableDropStatsManager.registerConnection(&intPortConn, &accPortConn);
    tableDropStatsManager.start();
    for(int i=IntFlowManager::SEC_TABLE_ID ;
            i < IntFlowManager::EXP_DROP_TABLE_ID ; i++) {
        testOneStaticDropFlow(intPortConn, i,
            tableDropStatsManager.getIntTableDropStatsMgr(),
            intBridge.switchManager);
=======
    start();
    for(int i=IntFlowManager::SEC_TABLE_ID ;
            i < IntFlowManager::EXP_DROP_TABLE_ID ; i++) {
        testOneStaticDropFlow<MockIntTableDropStatsManager>(intPortConn, i,
                getIntTableDropStatsManager(),
                switchManager, (i>3));
>>>>>>> origin/master
    }
    tableDropStatsManager.stop();
}

BOOST_FIXTURE_TEST_CASE(testStaticDropFlowsAcc, TableDropStatsManagerFixture) {
<<<<<<< HEAD
    MockConnection accPortConn(TEST_CONN_TYPE_ACC);
    MockConnection intPortConn(TEST_CONN_TYPE_INT);
    tableDropStatsManager.registerConnection(&intPortConn, &accPortConn);
    tableDropStatsManager.start();
    for(int i = AccessFlowManager::GROUP_MAP_TABLE_ID;
            i < AccessFlowManager::EXP_DROP_TABLE_ID; i++) {
        testOneStaticDropFlow(accPortConn, i,
            tableDropStatsManager.getAccTableDropStatsMgr(),
            accBridge.switchManager);
=======
    start();
    for(int i = AccessFlowManager::GROUP_MAP_TABLE_ID;
            i < AccessFlowManager::EXP_DROP_TABLE_ID; i++) {
        testOneStaticDropFlow<MockAccessTableDropStatsManager>(accPortConn, i,
                              getAccTableDropStatsManager(),
                              accBr, (i>3));
>>>>>>> origin/master
    }
    tableDropStatsManager.stop();
}

BOOST_AUTO_TEST_SUITE_END()

}
