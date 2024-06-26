/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Test suite for service manager
 *
 * Copyright (c) 2020 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <boost/test/unit_test.hpp>
#include <opflexagent/test/ModbFixture.h>
#include <opflexagent/test/BaseFixture.h>
#include <opflexagent/logging.h>
#include <modelgbp/svc/ServiceUniverse.hpp>
#include <modelgbp/svc/ServiceModeEnumT.hpp>
#include <modelgbp/observer/SvcStatUniverse.hpp>
#include <boost/filesystem/fstream.hpp>
#include <opflexagent/FSServiceSource.h>
#include <opflexagent/Service.h>
#include <opflexagent/ServiceManager.h>
#include <opflexagent/FSWatcher.h>
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <opflexagent/PrometheusManager.h>

namespace opflexagent {

using boost::optional;
using std::shared_ptr;
using std::string;
using std::size_t;
using namespace modelgbp::gbp;
using namespace modelgbp::gbpe;
using namespace modelgbp::svc;
using namespace opflex::modb;
using modelgbp::observer::SvcStatUniverse;
namespace fs = boost::filesystem;

class ServiceManagerFixture : public ModbFixture {
    typedef opflex::ofcore::OFConstants::OpflexElementMode opflex_elem_t;
public:
    ServiceManagerFixture(opflex_elem_t mode = opflex_elem_t::INVALID_MODE)
        : ModbFixture(mode) {
        createObjects();
    }

    virtual ~ServiceManagerFixture() {
    }

    void removeServiceObjects(void);
    void updateServices(bool isLB, bool isNodePort = false, bool isExternal = false);
    void checkServiceState(bool isAdd);
    void createServices(bool isLB, bool isNodePort=false, bool isExternal=false);
    Service as;
    void checkServicePromMetrics(bool isAdd, bool isExternal, bool isUpdate);
    void checkServiceTargetPromMetrics(bool isAdd, const string& ip, bool isExternal, bool isUpdate);
    void checkServiceExists(bool isAdd, bool isExternal=false, bool isUpdate=false);
    /**
     * Following commented commands didnt work in test environment, but will work from a regular terminal:
     * const string cmd = "curl --no-proxy \"*\" --compressed --silent http://127.0.0.1:9612/metrics 2>&1";
     * const string cmd = "curl --no-proxy \"127.0.0.1\" http://127.0.0.1:9612/metrics;";
     * --no-proxy, even though specified somehow tries to connect to SOCKS proxy and connection
     * fails to port 1080 :(
     */
    const string cmd = "curl --proxy \"\" --compressed --silent http://127.0.0.1:9612/metrics 2>&1;";

private:
    Service::ServiceMapping sm1;
    Service::ServiceMapping sm2;
};

/*
 * Service delete
 * - delete svc objects
 * - check if observer objects go away from caller
 */
void ServiceManagerFixture::removeServiceObjects (void)
{
    servSrc.removeService(as.getUUID());
}

void ServiceManagerFixture::createServices (bool isLB, bool isNodePort, bool isExternal)
{
    as.setUUID("ed84daef-1696-4b98-8c80-6b22d85f4dc2");
    as.setDomainURI(URI(rd0->getURI()));
    if (isLB)
        as.setServiceMode(Service::LOADBALANCER);
    else
        as.setServiceMode(Service::LOCAL_ANYCAST);

    if (isExternal)
        as.setServiceType(Service::LOAD_BALANCER);
    else
        as.setServiceType(Service::CLUSTER_IP);

    sm1.setServiceIP("169.254.169.254");
    sm1.setGatewayIP("169.254.1.1");
    sm1.setServiceProto("udp");
    sm1.setServicePort(53);
    sm1.addNextHopIP("10.20.44.2");
    sm1.addNextHopIP("169.254.169.2");
    sm1.setNextHopPort(5353);
    if (isNodePort)
        sm1.setNodePort(31001);
    else
        sm1.unsetNodePort();
    as.addServiceMapping(sm1);

    sm2.setServiceIP("fe80::a9:fe:a9:fe");
    sm2.setGatewayIP("fe80::1");
    sm2.setServiceProto("tcp");
    sm2.setServicePort(80);
    sm2.addNextHopIP("2001:db8::2");
    sm2.addNextHopIP("fe80::a9:fe:a9:2");
    if (isNodePort)
        sm2.setNodePort(31002);
    else
        sm2.unsetNodePort();
    as.addServiceMapping(sm2);

    as.addAttribute("name", "coredns");
    if (isExternal) {
        as.setInterfaceName("service-iface");
        as.setIfaceIP("1.1.1.1");
        as.setIfaceVlan(4003);
        as.addAttribute("scope", "ext");
    } else {
        as.addAttribute("scope", "cluster");
    }
    as.addAttribute("namespace", "kube-system");

    servSrc.updateService(as);
}

void ServiceManagerFixture::updateServices (bool isLB, bool isNodePort, bool isExternal)
{
    if (isLB)
        as.setServiceMode(Service::LOADBALANCER);
    else
        as.setServiceMode(Service::LOCAL_ANYCAST);

    if (isExternal)
        as.setServiceType(Service::LOAD_BALANCER);
    else
        as.setServiceType(Service::CLUSTER_IP);

    // simulating an update. without clearing the service mappings,
    // service mapping count will increase, since each new SM will
    // have a new service hash before getting added to sm_set
    as.clearServiceMappings();

    sm1.setServiceIP("169.254.169.254");
    sm1.setServiceProto("udp");
    sm1.setServicePort(54);
    sm1.addNextHopIP("10.20.44.2");
    sm1.addNextHopIP("169.254.169.4");
    sm1.setNextHopPort(5354);
    if (isNodePort)
        sm1.setNodePort(31001);
    else
        sm1.unsetNodePort();
    as.addServiceMapping(sm1);

    sm2.setServiceIP("fe80::a9:fe:a9:fe");
    sm2.setServiceProto("tcp");
    sm2.setServicePort(81);
    sm2.addNextHopIP("2001:db8::2");
    sm2.addNextHopIP("fe80::a9:fe:a9:4");
    if (isNodePort)
        sm2.setNodePort(31002);
    else
        sm2.unsetNodePort();
    as.addServiceMapping(sm2);

    as.clearAttributes();
    as.addAttribute("name", "nginx");
    if (isExternal)
        as.addAttribute("scope", "ext");
    else
        as.addAttribute("scope", "cluster");

    servSrc.updateService(as);
}

// Check service state during create vs update
void ServiceManagerFixture::checkServiceState (bool isCreate)
{
    optional<shared_ptr<ServiceUniverse> > su =
        ServiceUniverse::resolve(agent.getFramework());
    BOOST_CHECK(su);
    optional<shared_ptr<modelgbp::svc::Service> > opService =
                        su.get()->resolveSvcService(as.getUUID());
    BOOST_CHECK(opService);

    // Check if service props are fine
    uint8_t mode = ServiceModeEnumT::CONST_ANYCAST;
    if (Service::LOADBALANCER == as.getServiceMode()) {
        mode = ServiceModeEnumT::CONST_LB;
    }
    // Wait for the create/update to get reflected
    WAIT_FOR_DO_ONFAIL(
       (opService && (opService.get()->getMode(255) == mode)),
       500, // usleep(1000) * 500 = 500ms
       (opService = su.get()->resolveSvcService(as.getUUID())),
       if (opService) {
           BOOST_CHECK_EQUAL(opService.get()->getMode(255), mode);
       });

    shared_ptr<modelgbp::svc::Service> pService = opService.get();
    BOOST_CHECK_EQUAL(pService->getDom(""), rd0->getURI().toString());

    // Check if SM objects are created fine
    WAIT_FOR_DO_ONFAIL(pService->resolveSvcServiceMapping(sm1.getServiceIP().get(),
                                                          sm1.getServiceProto().get(),
                                                          sm1.getServicePort().get()),
                        500,,
                        LOG(ERROR) << "ServiceMapping Obj1 not resolved";);
    WAIT_FOR_DO_ONFAIL(pService->resolveSvcServiceMapping(sm2.getServiceIP().get(),
                                                          sm2.getServiceProto().get(),
                                                          sm2.getServicePort().get()),
                        500,,
                        LOG(ERROR) << "ServiceMapping Obj2 not resolved";);

    std::vector<shared_ptr<ServiceMapping> > outSM;
    pService->resolveSvcServiceMapping(outSM);
    if (isCreate) {
        BOOST_CHECK_EQUAL(outSM.size(), 2);
    } else {
        // remove + add can lead to bulking, so wait till the state becomes fine
        WAIT_FOR_DO_ONFAIL((outSM.size() == 2),
            500, // usleep(1000) * 500 = 500ms
            outSM.clear();pService->resolveSvcServiceMapping(outSM),
            BOOST_CHECK_EQUAL(outSM.size(), 2););
    }
    BOOST_CHECK_EQUAL(as.getServiceMappings().size(), 2);

    // Check if attr objects are resolved fine
    const Service::attr_map_t& attr_map = as.getAttributes();
    for (const std::pair<const string, string>& ap : attr_map) {
        WAIT_FOR_DO_ONFAIL(pService->resolveSvcServiceAttribute(ap.first),
                           500,,
                           LOG(ERROR) << "ServiceAttribute Obj not resolved";);
        auto sa = pService->resolveSvcServiceAttribute(ap.first);
        BOOST_CHECK(sa);
        BOOST_CHECK_EQUAL(sa.get()->getValue(""), ap.second);
    }

    std::vector<shared_ptr<ServiceAttribute> > outSA;
    pService->resolveSvcServiceAttribute(outSA);
    if (isCreate) {
        BOOST_CHECK_EQUAL(outSA.size(), 3);
        BOOST_CHECK_EQUAL(as.getAttributes().size(), 3);
    } else {
        // remove + add can lead to bulking, so wait till the state becomes fine
        WAIT_FOR_DO_ONFAIL((outSA.size() == 2),
            500, // usleep(1000) * 500 = 500ms
            outSA.clear();pService->resolveSvcServiceAttribute(outSA),
            BOOST_CHECK_EQUAL(outSA.size(), 2););
        BOOST_CHECK_EQUAL(as.getAttributes().size(), 2);
    }

    optional<shared_ptr<SvcStatUniverse> > ssu =
                          SvcStatUniverse::resolve(framework);
    BOOST_CHECK(ssu);
    // Check observer stats object for LB services only
    if (Service::LOADBALANCER == as.getServiceMode()) {
        optional<shared_ptr<SvcCounter> > opServiceCntr =
                        ssu.get()->resolveGbpeSvcCounter(as.getUUID());
        BOOST_CHECK(opServiceCntr);
        shared_ptr<SvcCounter> pServiceCntr = opServiceCntr.get();

        const Service::attr_map_t& svcAttr = as.getAttributes();

        auto nameItr = svcAttr.find("name");
        if (nameItr != svcAttr.end()) {
            BOOST_CHECK_EQUAL(pServiceCntr->getName(""),
                              nameItr->second);
        }

        auto nsItr = svcAttr.find("namespace");
        if (nsItr != svcAttr.end()) {
            BOOST_CHECK_EQUAL(pServiceCntr->getNamespace(""),
                              nsItr->second);
        }

        auto scopeItr = svcAttr.find("scope");
        if (scopeItr != svcAttr.end()) {
            BOOST_CHECK_EQUAL(pServiceCntr->getScope(""),
                              scopeItr->second);
        }
    } else {
        WAIT_FOR_DO_ONFAIL(!(ssu.get()->resolveGbpeSvcCounter(as.getUUID())),
                           500,,
                           LOG(ERROR) << "svc obj still present uuid: " << as.getUUID(););
    }
}

// Check prom dyn gauge service metrics
void ServiceManagerFixture::checkServicePromMetrics (bool isAdd, bool isExternal, bool isUpdate)
{
    const string& output = BaseFixture::getOutputFromCommand(cmd);
    size_t pos = std::string::npos;

    string scope;
    if (isExternal) {
        scope = "ext";
    } else
        scope = "cluster";

    string str, str2;
    if (isUpdate) {
        str = "name=\"nginx\",scope=\"" + scope + "\",uuid=\"" + as.getUUID() + "\"";
        str2 = "name=\"nginx\",scope=\"nodePort\",uuid=\"nodeport-" + as.getUUID() + "\"";
    } else {
        str = "name=\"coredns\",namespace=\"kube-system\",scope=\"" + scope + "\",uuid=\"" + as.getUUID() + "\"";
        str2 = "name=\"coredns\",namespace=\"kube-system\",scope=\"nodePort\",uuid=\"nodeport-" + as.getUUID() + "\"";
    }

    pos = output.find("opflex_svc_rx_bytes{" + str + "} 0");
    BaseFixture::expPosition(isAdd, pos);
    pos = output.find("opflex_svc_rx_packets{" + str + "} 0");
    BaseFixture::expPosition(isAdd, pos);
    pos = output.find("opflex_svc_tx_bytes{" + str + "} 0");
    BaseFixture::expPosition(isAdd, pos);
    pos = output.find("opflex_svc_tx_packets{" + str + "} 0");
    BaseFixture::expPosition(isAdd, pos);

    if (isExternal || !as.isNodePort()) {
        pos = output.find("opflex_svc_rx_bytes{" + str2 + "} 0");
        BaseFixture::expPosition(false, pos);
        pos = output.find("opflex_svc_rx_packets{" + str2 + "} 0");
        BaseFixture::expPosition(false, pos);
        pos = output.find("opflex_svc_tx_bytes{" + str2 + "} 0");
        BaseFixture::expPosition(false, pos);
        pos = output.find("opflex_svc_tx_packets{" + str2 + "} 0");
        BaseFixture::expPosition(false, pos);
    } else {
        pos = output.find("opflex_svc_rx_bytes{" + str2 + "} 0");
        BaseFixture::expPosition(isAdd, pos);
        pos = output.find("opflex_svc_rx_packets{" + str2 + "} 0");
        BaseFixture::expPosition(isAdd, pos);
        pos = output.find("opflex_svc_tx_bytes{" + str2 + "} 0");
        BaseFixture::expPosition(isAdd, pos);
        pos = output.find("opflex_svc_tx_packets{" + str2 + "} 0");
        BaseFixture::expPosition(isAdd, pos);
    }
}
// Check prom dyn gauge service target metrics
void ServiceManagerFixture::checkServiceTargetPromMetrics (bool isAdd,
                                                           const string& ip,
                                                           bool isExternal,
                                                           bool isUpdate)
{
    const string& output = BaseFixture::getOutputFromCommand(cmd);
    size_t pos = std::string::npos;

    string scope;
    if (isExternal)
        scope = "ext";
    else
        scope = "cluster";

    string str, str2;
    if (isUpdate) {
        str = "\",svc_name=\"nginx\",svc_scope=\"" + scope + "\",svc_uuid=\"" + as.getUUID() + "\"} 0";
        str2 = "\",svc_name=\"nginx\",svc_scope=\"nodePort\",svc_uuid=\"nodeport-" + as.getUUID() + "\"} 0";
    } else {
        str = "\",svc_name=\"coredns\",svc_namespace=\"kube-system\",svc_scope=\"" + scope + "\",svc_uuid=\"" + as.getUUID() + "\"} 0";
        str2 = "\",svc_name=\"coredns\",svc_namespace=\"kube-system\",svc_scope=\"nodePort\",svc_uuid=\"nodeport-" + as.getUUID() + "\"} 0";
    }

    pos = output.find("opflex_svc_target_rx_bytes{ip=\""+ip+str);
    BaseFixture::expPosition(isAdd, pos);
    pos = output.find("opflex_svc_target_rx_packets{ip=\""+ip+str);
    BaseFixture::expPosition(isAdd, pos);
    pos = output.find("opflex_svc_target_tx_bytes{ip=\""+ip+str);
    BaseFixture::expPosition(isAdd, pos);
    pos = output.find("opflex_svc_target_tx_packets{ip=\""+ip+str);
    BaseFixture::expPosition(isAdd, pos);

    if (isExternal || !as.isNodePort()) {
        pos = output.find("opflex_svc_target_rx_bytes{ip=\""+ip+str2);
        BaseFixture::expPosition(false, pos);
        pos = output.find("opflex_svc_target_rx_packets{ip=\""+ip+str2);
        BaseFixture::expPosition(false, pos);
        pos = output.find("opflex_svc_target_tx_bytes{ip=\""+ip+str2);
        BaseFixture::expPosition(false, pos);
        pos = output.find("opflex_svc_target_tx_packets{ip=\""+ip+str2);
        BaseFixture::expPosition(false, pos);
    } else {
        pos = output.find("opflex_svc_target_rx_bytes{ip=\""+ip+str2);
        BaseFixture::expPosition(isAdd, pos);
        pos = output.find("opflex_svc_target_rx_packets{ip=\""+ip+str2);
        BaseFixture::expPosition(isAdd, pos);
        pos = output.find("opflex_svc_target_tx_bytes{ip=\""+ip+str2);
        BaseFixture::expPosition(isAdd, pos);
        pos = output.find("opflex_svc_target_tx_packets{ip=\""+ip+str2);
        BaseFixture::expPosition(isAdd, pos);
    }
}

// Check service presence during create vs delete
void ServiceManagerFixture::checkServiceExists (bool isAdd, bool isExternal, bool isUpdate) {
    optional<shared_ptr<ServiceUniverse> > su =
        ServiceUniverse::resolve(agent.getFramework());
    BOOST_CHECK(su);

    optional<shared_ptr<SvcStatUniverse> > ssu =
                          SvcStatUniverse::resolve(framework);
    if (isAdd) {
        WAIT_FOR_DO_ONFAIL(su.get()->resolveSvcService(as.getUUID()),
                           500,,
                           LOG(ERROR) << "Service cfg Obj not resolved";);
        if (Service::LOADBALANCER == as.getServiceMode()) {
            WAIT_FOR_DO_ONFAIL(ssu.get()->resolveGbpeSvcCounter(as.getUUID()),
                               500,,
                               LOG(ERROR) << "SvcCounter obs Obj not resolved";);
            optional<shared_ptr<SvcCounter> > opSvcCounter =
                                ssu.get()->resolveGbpeSvcCounter(as.getUUID());
            BOOST_CHECK(opSvcCounter);
            checkServicePromMetrics(isAdd, isExternal, isUpdate);
            for (const auto& sm : as.getServiceMappings()) {
                for (const auto& ip : sm.getNextHopIPs()) {
                    WAIT_FOR_DO_ONFAIL(opSvcCounter.get()->resolveGbpeSvcTargetCounter(ip),
                                       500,,
                                       LOG(ERROR) << "SvcTargetCounter obs Obj not resolved";);
                    checkServiceTargetPromMetrics(isAdd, ip, isExternal, isUpdate);
                }
            }
        } else {
            BOOST_CHECK(!ssu.get()->resolveGbpeSvcCounter(as.getUUID()));
            checkServicePromMetrics(false, isExternal, isUpdate);
            checkServiceTargetPromMetrics(false, "10.20.44.2", isExternal, isUpdate);
            checkServiceTargetPromMetrics(false, "169.254.169.2", isExternal, isUpdate);
            checkServiceTargetPromMetrics(false, "2001:db8::2", isExternal, isUpdate);
            checkServiceTargetPromMetrics(false, "fe80::a9:fe:a9:2", isExternal, isUpdate);
        }
    } else {
        WAIT_FOR_DO_ONFAIL(!su.get()->resolveSvcService(as.getUUID()),
                           500,,
                           LOG(ERROR) << "Service cfg Obj still present";);
        WAIT_FOR_DO_ONFAIL(!ssu.get()->resolveGbpeSvcCounter(as.getUUID()),
                           500,,
                           LOG(ERROR) << "Service obs Obj still present";);
        checkServicePromMetrics(isAdd, isExternal, isUpdate);
        checkServiceTargetPromMetrics(isAdd, "10.20.44.2", isExternal, isUpdate);
        checkServiceTargetPromMetrics(isAdd, "169.254.169.2", isExternal, isUpdate);
        checkServiceTargetPromMetrics(isAdd, "2001:db8::2", isExternal, isUpdate);
        checkServiceTargetPromMetrics(isAdd, "fe80::a9:fe:a9:2", isExternal, isUpdate);
    }
}

BOOST_AUTO_TEST_SUITE(ServiceManager_test)

BOOST_FIXTURE_TEST_CASE(testCreateAnycast, ServiceManagerFixture) {
    LOG(DEBUG) << "############# SERVICE CREATE CHECK START ############";
    LOG(DEBUG) << "#### SERVICE CREATE START ####";
    createServices(false);
    LOG(DEBUG) << "#### SERVICE CREATE END ####";
    checkServiceExists(true);
    checkServiceState(true);
    const string& output = BaseFixture::getOutputFromCommand(cmd);
    size_t pos = std::string::npos;
    pos = output.find("opflex_svc_created_total 0");
    BOOST_CHECK_NE(pos, std::string::npos);
    pos = output.find("opflex_svc_removed_total 0");
    BOOST_CHECK_NE(pos, std::string::npos);
    LOG(DEBUG) << "############# SERVICE CREATE CHECK END ############";
}

BOOST_FIXTURE_TEST_CASE(testCreateLB, ServiceManagerFixture) {
    LOG(DEBUG) << "############# SERVICE CREATE CHECK START ############";
    LOG(DEBUG) << "#### SERVICE CREATE START ####";
    createServices(true);
    LOG(DEBUG) << "#### SERVICE CREATE END ####";
    checkServiceExists(true);
    checkServiceState(true);
    const string& output = BaseFixture::getOutputFromCommand(cmd);
    size_t pos = std::string::npos;
    pos = output.find("opflex_svc_created_total 1");
    BOOST_CHECK_NE(pos, std::string::npos);
    pos = output.find("opflex_svc_removed_total 0");
    BOOST_CHECK_NE(pos, std::string::npos);
    LOG(DEBUG) << "############# SERVICE CREATE CHECK END ############";
}

BOOST_FIXTURE_TEST_CASE(testCreateLBNodePort, ServiceManagerFixture) {
    LOG(DEBUG) << "############# SERVICE CREATE CHECK START ############";
    LOG(DEBUG) << "#### SERVICE CREATE START ####";
    createServices(true, true);
    LOG(DEBUG) << "#### SERVICE CREATE END ####";
    checkServiceExists(true);
    checkServiceState(true);
    const string& output = BaseFixture::getOutputFromCommand(cmd);
    size_t pos = std::string::npos;
    pos = output.find("opflex_svc_created_total 1");
    BOOST_CHECK_NE(pos, std::string::npos);
    pos = output.find("opflex_svc_removed_total 0");
    BOOST_CHECK_NE(pos, std::string::npos);
    LOG(DEBUG) << "############# SERVICE CREATE CHECK END ############";
}

BOOST_FIXTURE_TEST_CASE(testUpdate, ServiceManagerFixture) {
    LOG(DEBUG) << "#### SERVICE CREATE START ####";
    createServices(true);
    LOG(DEBUG) << "#### SERVICE CREATE END ####";
    LOG(DEBUG) << "############# SERVICE UPDATE START ############";
    checkServiceExists(true);

    // update to anycast
    updateServices(false);
    checkServiceState(false);
    const string& output1 = BaseFixture::getOutputFromCommand(cmd);
    size_t pos = std::string::npos;
    pos = output1.find("opflex_svc_created_total 1");
    BOOST_CHECK_NE(pos, std::string::npos);
    pos = output1.find("opflex_svc_removed_total 1");
    BOOST_CHECK_NE(pos, std::string::npos);

    // update to lb
    updateServices(true);
    checkServiceState(false);
    checkServiceExists(true, false, true);
    const string& output2 = BaseFixture::getOutputFromCommand(cmd);
    pos = std::string::npos;
    pos = output2.find("opflex_svc_created_total 2");
    BOOST_CHECK_NE(pos, std::string::npos);
    pos = output2.find("opflex_svc_removed_total 1");
    BOOST_CHECK_NE(pos, std::string::npos);

    // update to lb+nodeport
    updateServices(true, true);
    checkServiceState(false);
    checkServiceExists(true, false, true);
    const string& output3 = BaseFixture::getOutputFromCommand(cmd);
    pos = std::string::npos;
    pos = output3.find("opflex_svc_created_total 2");
    BOOST_CHECK_NE(pos, std::string::npos);
    pos = output3.find("opflex_svc_removed_total 1");
    BOOST_CHECK_NE(pos, std::string::npos);
    LOG(DEBUG) << "############# SERVICE UPDATE END ############";

    // update to lb without nodeport
    updateServices(true);
    checkServiceState(false);
    checkServiceExists(true, false, true);
    const string& output4 = BaseFixture::getOutputFromCommand(cmd);
    pos = std::string::npos;
    pos = output4.find("opflex_svc_created_total 2");
    BOOST_CHECK_NE(pos, std::string::npos);
    pos = output4.find("opflex_svc_removed_total 1");
    BOOST_CHECK_NE(pos, std::string::npos);
    LOG(DEBUG) << "############# SERVICE UPDATE END ############";
}

BOOST_FIXTURE_TEST_CASE(testDeleteLBNodePort, ServiceManagerFixture) {
    LOG(DEBUG) << "#### SERVICE CREATE START ####";
    createServices(true, true);
    LOG(DEBUG) << "#### SERVICE CREATE END ####";
    LOG(DEBUG) << "############# SERVICE DELETE START ############";
    checkServiceExists(true);
    removeServiceObjects();
    checkServiceExists(false);
    const string& output1 = BaseFixture::getOutputFromCommand(cmd);
    size_t pos = std::string::npos;
    pos = output1.find("opflex_svc_created_total 1");
    BOOST_CHECK_NE(pos, std::string::npos);
    pos = output1.find("opflex_svc_removed_total 1");
    BOOST_CHECK_NE(pos, std::string::npos);

    createServices(true, true);
    checkServiceExists(true);
    const string& output2 = BaseFixture::getOutputFromCommand(cmd);
    pos = output2.find("opflex_svc_created_total 2");
    BOOST_CHECK_NE(pos, std::string::npos);
    pos = output2.find("opflex_svc_removed_total 1");
    BOOST_CHECK_NE(pos, std::string::npos);

    removeServiceObjects();
    checkServiceExists(false);
    const string& output3 = BaseFixture::getOutputFromCommand(cmd);
    pos = output3.find("opflex_svc_created_total 2");
    BOOST_CHECK_NE(pos, std::string::npos);
    pos = output3.find("opflex_svc_removed_total 2");
    BOOST_CHECK_NE(pos, std::string::npos);
    LOG(DEBUG) << "############# SERVICE DELETE END ############";
}

BOOST_FIXTURE_TEST_CASE(testDeleteLB, ServiceManagerFixture) {
    LOG(DEBUG) << "#### SERVICE CREATE START ####";
    createServices(true);
    LOG(DEBUG) << "#### SERVICE CREATE END ####";
    LOG(DEBUG) << "############# SERVICE DELETE START ############";
    checkServiceExists(true);
    removeServiceObjects();
    checkServiceExists(false);
    const string& output1 = BaseFixture::getOutputFromCommand(cmd);
    size_t pos = std::string::npos;
    pos = output1.find("opflex_svc_created_total 1");
    BOOST_CHECK_NE(pos, std::string::npos);
    pos = output1.find("opflex_svc_removed_total 1");
    BOOST_CHECK_NE(pos, std::string::npos);

    createServices(true);
    checkServiceExists(true);
    const string& output2 = BaseFixture::getOutputFromCommand(cmd);
    pos = output2.find("opflex_svc_created_total 2");
    BOOST_CHECK_NE(pos, std::string::npos);
    pos = output2.find("opflex_svc_removed_total 1");
    BOOST_CHECK_NE(pos, std::string::npos);

    removeServiceObjects();
    checkServiceExists(false);
    const string& output3 = BaseFixture::getOutputFromCommand(cmd);
    pos = output3.find("opflex_svc_created_total 2");
    BOOST_CHECK_NE(pos, std::string::npos);
    pos = output3.find("opflex_svc_removed_total 2");
    BOOST_CHECK_NE(pos, std::string::npos);
    LOG(DEBUG) << "############# SERVICE DELETE END ############";
}

BOOST_FIXTURE_TEST_CASE(testDeleteAnycast, ServiceManagerFixture) {
    LOG(DEBUG) << "#### SERVICE CREATE START ####";
    createServices(false);
    LOG(DEBUG) << "#### SERVICE CREATE END ####";
    LOG(DEBUG) << "############# SERVICE DELETE START ############";
    checkServiceExists(true);
    removeServiceObjects();
    checkServiceExists(false);
    const string& output1 = BaseFixture::getOutputFromCommand(cmd);
    size_t pos = std::string::npos;
    pos = output1.find("opflex_svc_created_total 0");
    BOOST_CHECK_NE(pos, std::string::npos);
    pos = output1.find("opflex_svc_removed_total 0");
    BOOST_CHECK_NE(pos, std::string::npos);
    LOG(DEBUG) << "############# SERVICE DELETE END ############";
}

BOOST_FIXTURE_TEST_CASE(testCreateExternalLB, ServiceManagerFixture) {
    LOG(DEBUG) << "#### SERVICE EXT CREATE START ####";
    createServices(true, true, true);
    LOG(DEBUG) << "#### SERVICE EXT CREATE END ####";
    LOG(DEBUG) << "############# SERVICE EXT CREATE CHECK START ############";
    checkServiceState(true);
    checkServiceExists(true, true);
    const string& output = BaseFixture::getOutputFromCommand(cmd);
    size_t pos = std::string::npos;
    pos = output.find("opflex_svc_created_total 1");
    BOOST_CHECK_NE(pos, std::string::npos);
    pos = output.find("opflex_svc_removed_total 0");
    BOOST_CHECK_NE(pos, std::string::npos);
    LOG(DEBUG) << "############# SERVICE EXT CREATE CHECK END ############";
}

BOOST_FIXTURE_TEST_CASE(testUpdateExtLB, ServiceManagerFixture) {
    LOG(DEBUG) << "#### SERVICE EXT CREATE START ####";
    createServices(true, true, true);
    LOG(DEBUG) << "#### SERVICE EXT CREATE END ####";
    LOG(DEBUG) << "############# SERVICE EXT UPDATE START ############";
    checkServiceExists(true, true);

    // update to anycast
    updateServices(false, true, true);
    checkServiceState(false);
    const string& output1 = BaseFixture::getOutputFromCommand(cmd);
    size_t pos = std::string::npos;
    pos = output1.find("opflex_svc_created_total 1");
    BOOST_CHECK_NE(pos, std::string::npos);
    pos = output1.find("opflex_svc_removed_total 1");
    BOOST_CHECK_NE(pos, std::string::npos);

    // update to lb
    updateServices(true, true, true);
    checkServiceState(false);
    checkServiceExists(true, true, true);
    const string& output2 = BaseFixture::getOutputFromCommand(cmd);
    pos = std::string::npos;
    pos = output2.find("opflex_svc_created_total 2");
    BOOST_CHECK_NE(pos, std::string::npos);
    pos = output2.find("opflex_svc_removed_total 1");
    BOOST_CHECK_NE(pos, std::string::npos);
    LOG(DEBUG) << "############# SERVICE EXT UPDATE END ############";
}

BOOST_FIXTURE_TEST_CASE(testDeleteExtLB, ServiceManagerFixture) {
    LOG(DEBUG) << "#### SERVICE EXT CREATE START ####";
    createServices(true, true, true);
    LOG(DEBUG) << "#### SERVICE EXT CREATE END ####";
    LOG(DEBUG) << "############# SERVICE EXT DELETE START ############";
    checkServiceExists(true, true);
    removeServiceObjects();
    checkServiceExists(false, true);
    const string& output1 = BaseFixture::getOutputFromCommand(cmd);
    size_t pos = std::string::npos;
    pos = output1.find("opflex_svc_created_total 1");
    BOOST_CHECK_NE(pos, std::string::npos);
    pos = output1.find("opflex_svc_removed_total 1");
    BOOST_CHECK_NE(pos, std::string::npos);

    createServices(true, true, true);
    checkServiceExists(true, true);
    const string& output2 = BaseFixture::getOutputFromCommand(cmd);
    pos = output2.find("opflex_svc_created_total 2");
    BOOST_CHECK_NE(pos, std::string::npos);
    pos = output2.find("opflex_svc_removed_total 1");
    BOOST_CHECK_NE(pos, std::string::npos);

    removeServiceObjects();
    checkServiceExists(false, true);
    const string& output3 = BaseFixture::getOutputFromCommand(cmd);
    pos = output3.find("opflex_svc_created_total 2");
    BOOST_CHECK_NE(pos, std::string::npos);
    pos = output3.find("opflex_svc_removed_total 2");
    BOOST_CHECK_NE(pos, std::string::npos);
    LOG(DEBUG) << "############# SERVICE EXT DELETE END ############";
}

class FSServiceFixture : public BaseFixture {
public:
    FSServiceFixture()
        : BaseFixture(),
          temp(fs::temp_directory_path() / fs::unique_path()) {
        fs::create_directory(temp);
    }
    ~FSServiceFixture() {
        fs::remove_all(temp);
    }

    fs::path temp;
};

bool hasService(ServiceManager& serviceMgr, const string& uuid) {
    auto service = serviceMgr.getService(uuid);
    return service && service->getUUID() == uuid;
}

BOOST_FIXTURE_TEST_CASE( fsservice, FSServiceFixture ) {
   // check already existing service file
    const std::string uuid1 = "11310ce0-d8d3-41c9-9f1f-7e9aa5cf0a51";
    fs::path path1(temp / (uuid1 + ".service"));
    fs::ofstream os(path1);
    os  << "{"
        << "\"uuid\":\"" << uuid1 << "\","
        << "\"domain-policy-space\": \"common\","
        << "\"domain-name\": \"l3out_1_vrf\","
        << "\"service-mode\": \"loadbalancer\","
        << "\"service-mac\": \"88:1d:fc:f2:fb:59\","
        << "\"interface-name\": \"veth0\","
        << "\"interface-ip\": \"10.5.8.3\","
        << "\"interface-vlan\": 102,"
        << "\"service-type\": \"clusterIp\","
        << "\"service-mapping\":["
        << "{\"service-ip\": \"10.96.0.1\","
        << "\"service-proto\": \"tcp\","
        << "\"service-port\": 443,"
        << "\"next-hop-ips\":["
        << "\"1.100.101.11\""
        << "],"
        << "\"next-hop-port\": 6443,"
        << "\"conntrack-enabled\": true"
        << "}"
        << "],"
        << "\"attributes\": {"
        << "\"component\":\"apiserver\","
        << "\"name\":\"kubernetes\","
        << "\"namespace\":\"default\","
        << "\"provider\":\"kubernetes\","
        << "\"service-name\":\"default_kubernetes\""
        << "}"
        << "}" << std::endl;
    os.close();

    ServiceManager& serviceMgr = agent.getServiceManager();
    FSWatcher watcher;
    FSServiceSource source(&serviceMgr, watcher, temp.string());
    watcher.start();
    WAIT_FOR(hasService(serviceMgr, uuid1), 500);

    fs::path path2(temp / (uuid1 + ".service"));
    fs::ofstream os2(path2);
    os2 << "{"
        << "\"uuid\":\"" << uuid1 << "\","
        << "\"domain-policy-space\": \"common\","
        << "\"domain-name\": \"l3out_1_vrf\","
        << "\"service-mode\": \"local-anycast\","
        << "\"interface-name\": \"veth1\","
        << "\"interface-ip\": \"10.5.8.3\","
        << "\"interface-vlan\": 102,"
        << "\"service-type\": \"clusterIp\","
        << "\"service-mapping\":["
        << "{\"service-ip\": \"10.96.0.2\","
        << "\"service-proto\": \"tcp\","
        << "\"service-port\": 443,"
        << "\"next-hop-ips\":["
        << "\"1.100.101.11\""
        << "],"
        << "\"next-hop-port\": 6443,"
        << "\"conntrack-enabled\": true"
        << "}"
        << "],"
        << "\"attributes\": {"
        << "\"component\":\"apiserver\","
        << "\"name\":\"kubernetes\","
        << "\"namespace\":\"default\","
        << "\"provider\":\"kubernetes\","
        << "\"service-name\":\"default_kubernetes\""
        << "}"
        << "}" << std::endl;
    os2.close();

    WAIT_FOR(hasService(serviceMgr, uuid1)
             && (serviceMgr.getService(uuid1)->getServiceMode() \
                        == Service::ServiceMode::LOCAL_ANYCAST),
             500);
    watcher.stop();

}

BOOST_FIXTURE_TEST_CASE( fsdelservice, FSServiceFixture ) {

    // check for a new Service added to watch directory
    fs::path path1(temp / "11310ce0-d8d3-41c9-9f1f-7e9aa5cf0a52.service");
    fs::ofstream os(path1);
    const std::string uuid = "11310ce0-d8d3-41c9-9f1f-7e9aa5cf0a52";
    os  << "{"
        << "\"uuid\":\"" << uuid << "\","
        << "\"domain-policy-space\": \"common\","
        << "\"domain-name\": \"l3out_1_vrf\","
        << "\"service-mode\": \"loadbalancer\","
        << "\"interface-name\": \"veth0\","
        << "\"interface-ip\": \"10.5.8.3\","
        << "\"interface-vlan\": 102,"
        << "\"service-type\": \"clusterIp\","
        << "\"service-mapping\":["
        << "{\"service-ip\": \"10.96.0.1\","
        << "\"service-proto\": \"tcp\","
        << "\"service-port\": 443,"
        << "\"next-hop-ips\":["
        << "\"1.100.101.11\""
        << "],"
        << "\"next-hop-port\": 6443,"
        << "\"conntrack-enabled\": true"
        << "}"
        << "],"
        << "\"attributes\": {"
        << "\"component\":\"apiserver\","
        << "\"name\":\"kubernetes\","
        << "\"namespace\":\"default\","
        << "\"provider\":\"kubernetes\","
        << "\"service-name\":\"default_kubernetes\""
        << "}"
        << "}" << std::endl;
    os.close();

    FSWatcher watcher;
    ServiceManager& serviceMgr = agent.getServiceManager();
    FSServiceSource source(&serviceMgr, watcher,temp.string());
    watcher.start();
    WAIT_FOR((serviceMgr.getService(uuid) != nullptr), 500);
    auto extService = serviceMgr.getService(uuid);
    BOOST_CHECK(extService->getServiceMode() == Service::ServiceMode::LOADBALANCER);

    fs::remove(path1);
    WAIT_FOR((agent.getServiceManager().getService(uuid) == nullptr), 500);

    watcher.stop();
}

BOOST_AUTO_TEST_SUITE_END()
}
