/*
 * Test suite for class NetFlowRenderer
 *
 * Copyright (c) 2020 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <boost/test/unit_test.hpp>

#include <opflexagent/logging.h>
#include <opflexagent/test/BaseFixture.h>
#include <NetFlowRenderer.h>
<<<<<<< HEAD
#include "MockJsonRpc.h"
=======
#include "MockRpcConnection.h"

#include <modelgbp/netflow/CollectorVersionEnumT.hpp>
>>>>>>> origin/master

namespace opflexagent {

using namespace std;
using namespace rapidjson;

BOOST_AUTO_TEST_SUITE(NetFlowRenderer_test)

class NetFlowRendererFixture : public BaseFixture {
public:
    NetFlowRendererFixture() : BaseFixture() {
        nfr = make_shared<NetFlowRenderer>(agent);
        initLogging("debug", false, "");
        conn.reset(new MockRpcConnection());
        nfr->start("br-int", conn.get());
        nfr->connect();
    }

<<<<<<< HEAD
    virtual ~NetFlowRendererFixture() {};
=======
    virtual ~NetFlowRendererFixture() {
        nfr->stop();
    };
>>>>>>> origin/master

    shared_ptr<NetFlowRenderer> nfr;
    unique_ptr<OvsdbConnection> conn;
};

static bool verifyCreateDestroy(const shared_ptr<NetFlowRenderer>& nfr) {
<<<<<<< HEAD
    nfr->jRpc->setNextId(2000);
    string bridgeUuid;
    nfr->jRpc->getBridgeUuid("br-int", bridgeUuid);

    bool result = nfr->jRpc->createNetFlow(bridgeUuid, "5.5.5.6", 10, true);
    result = result && nfr->jRpc->deleteNetFlow(bridgeUuid);

    nfr->jRpc->setNextId(2001);
    result = result && nfr->jRpc->createIpfix(bridgeUuid, "5.5.5.5", 500);
    result = result && nfr->jRpc->deleteIpfix(bridgeUuid);
=======
    nfr->setNextId(2000);

    bool result = nfr->createNetFlow("5.5.5.6", 10);
    URI exporterURI("/PolicyUniverse/");
    ExporterConfigState state(exporterURI, "test");
    state.setVersion(1); // modelgbp::netflow::CollectorVersionEnumT::CONST_V5
    shared_ptr<ExporterConfigState> statePtr = make_shared<ExporterConfigState>(state);
    nfr->exporterDeleted(statePtr);

    result = result && nfr->createIpfix("5.5.5.5", 500);
    statePtr->setVersion(2); // modelgbp::netflow::CollectorVersionEnumT::CONST_V9
    nfr->exporterDeleted(statePtr);
>>>>>>> origin/master
    return result;
}

BOOST_FIXTURE_TEST_CASE( verify_createdestroy, NetFlowRendererFixture ) {
<<<<<<< HEAD
    WAIT_FOR(verifyCreateDestroy(nfr), 1);
=======
    BOOST_CHECK_EQUAL(true, verifyCreateDestroy(nfr));
>>>>>>> origin/master
}
BOOST_AUTO_TEST_SUITE_END()

}