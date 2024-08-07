/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */


#ifndef OPFLEXAGENT_TEST_MODBFIXTURE_H_
#define OPFLEXAGENT_TEST_MODBFIXTURE_H_

#include "BaseFixture.h"
#include "MockEndpointSource.h"
#include <opflexagent/EndpointSource.h>
#include <opflexagent/ServiceSource.h>
#include <opflexagent/EndpointManager.h>

#include <modelgbp/dmtree/Root.hpp>
#include <modelgbp/l2/EtherTypeEnumT.hpp>
#include <modelgbp/l4/TcpFlagsEnumT.hpp>
#include <modelgbp/gbp/DirectionEnumT.hpp>
#include <modelgbp/gbp/UnknownFloodModeEnumT.hpp>
#include <modelgbp/gbp/ConnTrackEnumT.hpp>
#include <opflex/modb/Mutator.h>

#include <memory>

namespace opflexagent {

/**
 * A dummy service source for testing
 */
class DummyServiceSrc : public ServiceSource {
public:
    /**
     * Construct a dummy service source
     */
    DummyServiceSrc(ServiceManager *manager)
        : ServiceSource(manager) { }
    virtual ~DummyServiceSrc() { }
};

/**
 * A test fixture that will set up some sample MODB data
 */
class ModbFixture : public BaseFixture {
    //! @cond Doxygen_Suppress
public:
    ModbFixture(opflex_elem_t mode = opflex_elem_t::INVALID_MODE) :
                    BaseFixture(mode),
                    epSrc(&agent.getEndpointManager()),
                    servSrc(&agent.getServiceManager()),
                    policyOwner("policyreg") {
        universe = modelgbp::policy::Universe::resolve(framework).get();
        opflex::modb::Mutator mutator(framework, policyOwner);
        space = universe->addPolicySpace("tenant0");
        mutator.commit();
    }

    virtual ~ModbFixture() {
    }

    MockEndpointSource epSrc;
    DummyServiceSrc servSrc;
    std::shared_ptr<modelgbp::policy::Universe> universe;
    std::shared_ptr<modelgbp::policy::Space> space;
    std::shared_ptr<modelgbp::platform::Config> config;
    std::shared_ptr<Endpoint> ep0, ep1, ep2, ep3, ep4, ep5, vethhostac;
    std::shared_ptr<modelgbp::gbp::EpGroup> epg0, epg1, epg2, epg3, epg4;
    std::shared_ptr<modelgbp::gbp::FloodDomain> fd0, fd1;
    std::shared_ptr<modelgbp::gbpe::FloodContext> fd0ctx;
    std::shared_ptr<modelgbp::gbp::RoutingDomain> rd0;
    std::shared_ptr<modelgbp::gbp::BridgeDomain> bd0, bd1;
    std::shared_ptr<modelgbp::gbp::Subnets> subnetsfd0, subnetsfd1,
        subnetsbd0, subnetsbd1;
    std::shared_ptr<modelgbp::gbp::Subnet> subnetsfd0_1, subnetsfd0_2,
        subnetsfd1_1, subnetsbd0_1, subnetsbd1_1, subnet;

    std::shared_ptr<modelgbp::gbpe::L24Classifier> classifier0;
    std::shared_ptr<modelgbp::gbpe::L24Classifier> classifier1;
    std::shared_ptr<modelgbp::gbpe::L24Classifier> classifier2;
    std::shared_ptr<modelgbp::gbpe::L24Classifier> classifier3;
    std::shared_ptr<modelgbp::gbpe::L24Classifier> classifier4;
    std::shared_ptr<modelgbp::gbpe::L24Classifier> classifier5;
    std::shared_ptr<modelgbp::gbpe::L24Classifier> classifier6;
    std::shared_ptr<modelgbp::gbpe::L24Classifier> classifier7;
    std::shared_ptr<modelgbp::gbpe::L24Classifier> classifier8;
    std::shared_ptr<modelgbp::gbpe::L24Classifier> classifier9;
    std::shared_ptr<modelgbp::gbpe::L24Classifier> classifier10;

    std::shared_ptr<modelgbp::gbpe::LocalL24Classifier> local_classifier0;
    std::shared_ptr<modelgbp::gbpe::LocalL24Classifier> local_classifier1;
    std::shared_ptr<modelgbp::gbpe::LocalL24Classifier> local_classifier2;
    std::shared_ptr<modelgbp::gbpe::LocalL24Classifier> local_classifier5;
    std::shared_ptr<modelgbp::gbpe::LocalL24Classifier> local_classifier6;
    std::shared_ptr<modelgbp::gbpe::LocalL24Classifier> local_classifier7;
    std::shared_ptr<modelgbp::gbpe::LocalL24Classifier> local_classifier8;
    std::shared_ptr<modelgbp::gbpe::LocalL24Classifier> local_classifier9;

     // order 10
    std::shared_ptr<modelgbp::gbpe::L24Classifier> classifier11;
    std::shared_ptr<modelgbp::gbpe::L24Classifier> classifier12;
    std::shared_ptr<modelgbp::gbpe::L24Classifier> classifier13;
    std::shared_ptr<modelgbp::gbpe::L24Classifier> classifier14;
    std::shared_ptr<modelgbp::gbp::AllowDenyAction> action1;
    std::shared_ptr<modelgbp::gbp::LocalAllowDenyAction> local_action1;
       // order 20
    std::shared_ptr<modelgbp::gbpe::L24Classifier> classifier15;
    std::shared_ptr<modelgbp::gbpe::L24Classifier> classifier16;
    std::shared_ptr<modelgbp::gbpe::L24Classifier> classifier17;

    //order 30
    std::shared_ptr<modelgbp::gbpe::L24Classifier> classifier18;

    //order 40
    std::shared_ptr<modelgbp::gbpe::L24Classifier> classifier19;
    std::shared_ptr<modelgbp::gbpe::L24Classifier> classifier20;

    std::shared_ptr<modelgbp::gbp::LogAction> action2;
    std::shared_ptr<modelgbp::gbp::LocalLogAction> local_action2;

    std::shared_ptr<modelgbp::gbp::AllowDenyAction> action3;
    std::shared_ptr<modelgbp::gbp::LogAction> action4;

    std::shared_ptr<modelgbp::gbp::Contract> con1;
    std::shared_ptr<modelgbp::gbp::Contract> con2;
    std::shared_ptr<modelgbp::gbp::Contract> con3;
    std::shared_ptr<modelgbp::gbp::Contract> con4;
    std::shared_ptr<modelgbp::gbp::Contract> con5;
    std::shared_ptr<modelgbp::gbp::Contract> con6;
    std::shared_ptr<modelgbp::gbp::Contract> con7;
    std::shared_ptr<modelgbp::gbp::Contract> con8;
    std::shared_ptr<modelgbp::gbp::Contract> con9;
    std::shared_ptr<modelgbp::gbp::Contract> con10;
    std::string policyOwner;
protected:

    void createObjects() {
        using opflex::modb::Mutator;
        using namespace modelgbp;
        using namespace modelgbp::gbp;
        using namespace modelgbp::gbpe;

        /* create EPGs and forwarding objects */
        Mutator mutator(framework, policyOwner);
        config = universe->addPlatformConfig("default");
        config->setMulticastGroupIP("224.1.1.1");

        fd0 = space->addGbpFloodDomain("fd0");
        fd1 = space->addGbpFloodDomain("fd1");
        fd1->setUnknownFloodMode(UnknownFloodModeEnumT::CONST_FLOOD);
        bd0 = space->addGbpBridgeDomain("bd0");
        bd1 = space->addGbpBridgeDomain("bd1");
        rd0 = space->addGbpRoutingDomain("rd0");

        fd0->addGbpFloodDomainToNetworkRSrc()
            ->setTargetBridgeDomain(bd0->getURI());
        fd0ctx = fd0->addGbpeFloodContext();

        fd1->addGbpFloodDomainToNetworkRSrc()
            ->setTargetBridgeDomain(bd0->getURI());
        bd0->addGbpBridgeDomainToNetworkRSrc()
            ->setTargetRoutingDomain(rd0->getURI());
        bd1->addGbpBridgeDomainToNetworkRSrc()
            ->setTargetRoutingDomain(rd0->getURI());

        subnetsfd0 = space->addGbpSubnets("subnetsfd0");
        subnetsfd0_1 = subnetsfd0->addGbpSubnet("subnetsfd0_1");
        subnetsfd0_1->setAddress("10.20.44.1")
            .setPrefixLen(24)
            .setVirtualRouterIp("10.20.44.1");
        subnetsfd0_2 = subnetsfd0->addGbpSubnet("subnetsfd0_2");
        subnetsfd0_2->setAddress("2001:db8::")
            .setPrefixLen(32)
            .setVirtualRouterIp("2001:db8::1");
        fd0->addGbpForwardingBehavioralGroupToSubnetsRSrc()
            ->setTargetSubnets(subnetsfd0->getURI());
        rd0->addGbpRoutingDomainToIntSubnetsRSrc(subnetsfd0->getURI().toString());

        subnetsfd1 = space->addGbpSubnets("subnetsfd1");
        subnetsfd1_1 = subnetsfd0->addGbpSubnet("subnetsfd1_1");
        subnetsfd1_1->setAddress("10.20.45.0")
            .setPrefixLen(24)
            .setVirtualRouterIp("10.20.45.1");
        fd1->addGbpForwardingBehavioralGroupToSubnetsRSrc()
            ->setTargetSubnets(subnetsfd1->getURI());
        rd0->addGbpRoutingDomainToIntSubnetsRSrc(subnetsfd1->getURI().toString());

        subnetsbd0 = space->addGbpSubnets("subnetsbd0");
        subnetsbd0_1 = subnetsbd0->addGbpSubnet("subnetsbd0_1");
        bd0->addGbpForwardingBehavioralGroupToSubnetsRSrc()
            ->setTargetSubnets(subnetsbd0->getURI());
        rd0->addGbpRoutingDomainToIntSubnetsRSrc(subnetsbd0->getURI().toString());

        subnetsbd1 = space->addGbpSubnets("subnetsbd1");
        subnetsbd1_1 = subnetsbd1->addGbpSubnet("subnetsbd1_1");
        bd1->addGbpForwardingBehavioralGroupToSubnetsRSrc()
            ->setTargetSubnets(subnetsbd1->getURI());
        rd0->addGbpRoutingDomainToIntSubnetsRSrc(subnetsbd1->getURI().toString());

        epg0 = space->addGbpEpGroup("epg0");
        epg0->addGbpEpGroupToNetworkRSrc()
            ->setTargetBridgeDomain(bd0->getURI());
        epg0->addGbpeInstContext()->setEncapId(0xA0A);

        epg1 = space->addGbpEpGroup("epg1");
        epg1->addGbpEpGroupToNetworkRSrc()
            ->setTargetBridgeDomain(bd1->getURI());
        epg1->addGbpeInstContext()->setEncapId(0xA0B);

        epg2 = space->addGbpEpGroup("epg2");
        epg2->addGbpEpGroupToNetworkRSrc()
            ->setTargetFloodDomain(fd0->getURI());
        epg2->addGbpeInstContext()->setEncapId(0xD0A)
            .setMulticastGroupIP("224.5.1.1");

        epg3 = space->addGbpEpGroup("epg3");
        epg3->addGbpEpGroupToNetworkRSrc()
            ->setTargetFloodDomain(fd1->getURI());
        epg3->addGbpeInstContext()->setEncapId(0xD0B);

        epg4 = space->addGbpEpGroup("epg4");
        epg4->addGbpeInstContext()->setEncapId(0xE0E);
        epg4->addGbpEpGroupToNetworkRSrc()
            ->setTargetRoutingDomain(rd0->getURI());

        mutator.commit();

        /* create endpoints */
        ep0.reset(new Endpoint("0-0-0-0"));
        ep0->setInterfaceName("port80");
        ep0->setMAC(opflex::modb::MAC("00:00:00:00:80:00"));
        ep0->addIP("10.20.44.2");
        ep0->addIP("10.20.44.3");
        ep0->addIP("2001:db8::2");
        ep0->addIP("2001:db8::3");
        ep0->addAnycastReturnIP("10.20.44.2");
        ep0->addAnycastReturnIP("2001:db8::2");
        ep0->setEgURI(epg0->getURI());
        ep0->addAttribute("vm-name", "coredns");
        ep0->addAttribute("namespace", "default");
        epSrc.updateEndpoint(*ep0);

        ep1.reset(new Endpoint("0-0-0-1"));
        ep1->setMAC(opflex::modb::MAC("00:00:00:00:00:01"));
        ep1->setEgURI(epg1->getURI());
        epSrc.updateEndpoint(*ep1);

        ep2.reset(new Endpoint("0-0-0-2"));
        ep2->setMAC(opflex::modb::MAC("00:00:00:00:00:02"));
        ep2->addIP("10.20.44.21");
        ep2->setInterfaceName("port11");
        ep2->setEgURI(epg0->getURI());
        epSrc.updateEndpoint(*ep2);

        ep3.reset(new Endpoint("0-0-0-3"));
        ep3->setMAC(opflex::modb::MAC("00:00:00:00:00:03"));
        ep3->addIP("10.20.45.31");
        ep3->setInterfaceName("eth3");
        ep3->setEgURI(epg3->getURI());
        epSrc.updateEndpoint(*ep3);

        ep4.reset(new Endpoint("0-0-0-4"));
        ep4->setMAC(opflex::modb::MAC("00:00:00:00:00:04"));
        ep4->addIP("10.20.45.32");
        ep4->setInterfaceName("eth4");
        ep4->setEgURI(epg3->getURI());
        ep4->setPromiscuousMode(true);
        epSrc.updateEndpoint(*ep4);

        ep5.reset(new Endpoint("0-0-0-5"));
        ep5->setMAC(opflex::modb::MAC("00:00:00:00:00:05"));
        ep5->addIP("10.20.46.3");
        ep5->setInterfaceName("eth5");
        URI extSviBD("/tenant0/extSvi1");
        ep5->setEgURI(extSviBD);
        ep5->setExternal();
        ep5->setExtEncap(1000);
        epSrc.updateEndpoint(*ep5);
    }

    // Add vethhostac for nodeport tests
    void createVethHostAccessObjects () {
        vethhostac.reset(new Endpoint("veth_host_ac"));
        vethhostac->setInterfaceName("veth_host_ac");
        vethhostac->setAccessInterface("veth_host_ac");
        vethhostac->setDisableAdv(true);
        vethhostac->setMAC(opflex::modb::MAC("00:00:00:00:00:11"));
        vethhostac->addIP("1.100.201.11");
        vethhostac->setEgURI(epg0->getURI());
        vethhostac->addAttribute("vm-name", "host-access");
        vethhostac->addAttribute("namespace", "default");
        epSrc.updateEndpoint(*vethhostac);
    }

    void createPolicyObjects() {
        using opflex::modb::Mutator;
        using namespace modelgbp;
        using namespace modelgbp::gbp;
        using namespace modelgbp::gbpe;
        Mutator mutator_local(framework, "policyelement");
        /* blank classifier */
        local_classifier0 = space->addGbpeLocalL24Classifier("classifier0");
        local_classifier0->setOrder(10);


        /* allow TCP to dst port 80 cons->prov */
        local_classifier1 = space->addGbpeLocalL24Classifier("classifier1");
        local_classifier1->setEtherT(l2::EtherTypeEnumT::CONST_IPV4)
            .setProt(6 /* TCP */).setDFromPort(80);

        /* allow ARP from prov->cons */
        local_classifier2 = space->addGbpeLocalL24Classifier("classifier2");
        local_classifier2->setEtherT(l2::EtherTypeEnumT::CONST_ARP);

        /* allow bidirectional FCoE */
        local_classifier5 = space->addGbpeLocalL24Classifier("classifier5");
        local_classifier5->setOrder(20).setEtherT(l2::EtherTypeEnumT::CONST_FCOE);

        /* allow SSH from port 22 with ACK+SYN */
        local_classifier6 = space->addGbpeLocalL24Classifier("classifier6");
        local_classifier6->setEtherT(l2::EtherTypeEnumT::CONST_IPV4)
            .setProt(6 /* TCP */)
            .setSFromPort(22)
            .setTcpFlags(l4::TcpFlagsEnumT::CONST_ACK |
                         l4::TcpFlagsEnumT::CONST_SYN);

        /* allow SSH from port 22 with EST */
        local_classifier7 = space->addGbpeLocalL24Classifier("classifier7");
        local_classifier7->setEtherT(l2::EtherTypeEnumT::CONST_IPV4)
            .setProt(6 /* TCP */)
            .setSFromPort(21)
            .setTcpFlags(l4::TcpFlagsEnumT::CONST_ESTABLISHED);

        /* allow TCPv6 to dst port 80 cons->prov */
        local_classifier8 = space->addGbpeLocalL24Classifier("classifier8");
        local_classifier8->setEtherT(l2::EtherTypeEnumT::CONST_IPV6)
            .setProt(6 /* TCP */).setDFromPort(80);

        /* Allow 22 reflexive */
        local_classifier9 = space->addGbpeLocalL24Classifier("classifier9");
        local_classifier9->setOrder(10)
            .setEtherT(l2::EtherTypeEnumT::CONST_IPV4)
            .setProt(6 /* TCP */)
            .setDFromPort(22)
            .setConnectionTracking(ConnTrackEnumT::CONST_REFLEXIVE);

        local_action1 = space->addGbpLocalAllowDenyAction("action1");
        mutator_local.commit();

        Mutator mutator(framework, policyOwner);

        // deny action
        action1 = space->addGbpAllowDenyAction("action1");

        /* blank classifier */
        classifier0 = space->addGbpeL24Classifier("classifier0");
        classifier0->setOrder(10);

        /* allow bidirectional FCoE */
        classifier5 = space->addGbpeL24Classifier("classifier5");
        classifier5->setOrder(20).setEtherT(l2::EtherTypeEnumT::CONST_FCOE);

        /* allow TCP to dst port 80 cons->prov */
        classifier1 = space->addGbpeL24Classifier("classifier1");
        classifier1->setEtherT(l2::EtherTypeEnumT::CONST_IPV4)
            .setProt(6 /* TCP */).setDFromPort(80);

        /* allow TCPv6 to dst port 80 cons->prov */
        classifier8 = space->addGbpeL24Classifier("classifier8");
        classifier8->setEtherT(l2::EtherTypeEnumT::CONST_IPV6)
            .setProt(6 /* TCP */).setDFromPort(80);

        /* allow ARP from prov->cons */
        classifier2 = space->addGbpeL24Classifier("classifier2");
        classifier2->setEtherT(l2::EtherTypeEnumT::CONST_ARP);

        /* allow SSH from port 22 with ACK+SYN */
        classifier6 = space->addGbpeL24Classifier("classifier6");
        classifier6->setEtherT(l2::EtherTypeEnumT::CONST_IPV4)
            .setProt(6 /* TCP */)
            .setSFromPort(22)
            .setTcpFlags(l4::TcpFlagsEnumT::CONST_ACK |
                         l4::TcpFlagsEnumT::CONST_SYN);

        /* allow SSH from port 22 with EST */
        classifier7 = space->addGbpeL24Classifier("classifier7");
        classifier7->setEtherT(l2::EtherTypeEnumT::CONST_IPV4)
            .setProt(6 /* TCP */)
            .setSFromPort(21)
            .setTcpFlags(l4::TcpFlagsEnumT::CONST_ESTABLISHED);

        /* Allow 22 reflexive */
        classifier9 = space->addGbpeL24Classifier("classifier9");
        classifier9->setOrder(10)
            .setEtherT(l2::EtherTypeEnumT::CONST_IPV4)
            .setProt(6 /* TCP */)
            .setDFromPort(22)
            .setConnectionTracking(ConnTrackEnumT::CONST_REFLEXIVE);

        classifier11 = space->addGbpeL24Classifier("classifier11");
        classifier11->setOrder(10).setEtherT(l2::EtherTypeEnumT::CONST_IPV4)
            .setProt(6 /* TCP */)
            .setSFromPort(22)
            .setTcpFlags(l4::TcpFlagsEnumT::CONST_ESTABLISHED);

        classifier12 = space->addGbpeL24Classifier("classifier12");
        classifier12->setOrder(10).setEtherT(l2::EtherTypeEnumT::CONST_IPV4)
            .setProt(6 /* TCP */).setDFromPort(80).setSFromPort(80);

        classifier13 = space->addGbpeL24Classifier("classifier13");
        classifier13->setOrder(10).setEtherT(l2::EtherTypeEnumT::CONST_IPV4)
            .setProt(6 /* TCP */).setSFromPort(22);

        classifier14 = space->addGbpeL24Classifier("classifier14");
        classifier14->setOrder(10).setEtherT(l2::EtherTypeEnumT::CONST_IPV4)
            .setProt(6 /* TCP */)
            .setConnectionTracking(ConnTrackEnumT::CONST_REFLEXIVE);

        classifier15 = space->addGbpeL24Classifier("classifier15");
        classifier15->setOrder(20).setEtherT(l2::EtherTypeEnumT::CONST_IPV4)
                    .setProt(6 /* TCP */).setSFromPort(22);

        classifier16 = space->addGbpeL24Classifier("classifier16");
        classifier16->setOrder(20).setEtherT(l2::EtherTypeEnumT::CONST_IPV4)
                     .setProt(6 /* TCP */)
                     .setSFromPort(22)
                     .setTcpFlags(l4::TcpFlagsEnumT::CONST_ESTABLISHED);

        classifier17 = space->addGbpeL24Classifier("classifier17");
        classifier17->setOrder(20).setEtherT(l2::EtherTypeEnumT::CONST_IPV4)
                      .setProt(6 /* TCP */).setDFromPort(80);

        classifier18 = space->addGbpeL24Classifier("classifier18");
        classifier18->setOrder(30).setEtherT(l2::EtherTypeEnumT::CONST_IPV4)
                      .setProt(6 /* TCP */).setDFromPort(80)
                      .setSFromPort(22);

        classifier19 = space->addGbpeL24Classifier("classifier19");
        classifier19->setOrder(40).setEtherT(l2::EtherTypeEnumT::CONST_IPV4)
                      .setProt(6 /* TCP */).setDFromPort(80);

        classifier20 = space->addGbpeL24Classifier("classifier20");
        classifier20->setOrder(40).setEtherT(l2::EtherTypeEnumT::CONST_IPV4)
                      .setProt(6 /* TCP */).setDFromPort(80)
                      .setSFromPort(22);

        con1 = space->addGbpContract("contract1");
        con1->addGbpSubject("1_subject1")->addGbpRule("1_1_rule1")
            ->setDirection(DirectionEnumT::CONST_IN).setOrder(100)
            .addGbpRuleToClassifierRSrc(classifier1->getURI().toString());
        con1->addGbpSubject("1_subject1")->addGbpRule("1_1_rule2")
            ->setDirection(DirectionEnumT::CONST_OUT).setOrder(200)
            .addGbpRuleToClassifierRSrc(classifier2->getURI().toString());
        con1->addGbpSubject("1_subject1")->addGbpRule("1_1_rule3")
            ->setDirection(DirectionEnumT::CONST_IN).setOrder(300)
            .addGbpRuleToClassifierRSrc(classifier6->getURI().toString());
        con1->addGbpSubject("1_subject1")->addGbpRule("1_1_rule4")
            ->setDirection(DirectionEnumT::CONST_IN).setOrder(400)
            .addGbpRuleToClassifierRSrc(classifier7->getURI().toString());

        con2 = space->addGbpContract("contract2");
        con2->addGbpSubject("2_subject1")->addGbpRule("2_1_rule1")
            ->addGbpRuleToClassifierRSrc(classifier0->getURI().toString());
        con2->addGbpSubject("2_subject1")->addGbpRule("2_1_rule2")
            ->setDirection(DirectionEnumT::CONST_BIDIRECTIONAL).setOrder(20)
            .addGbpRuleToClassifierRSrc(classifier5->getURI().toString());

        epg0->addGbpEpGroupToProvContractRSrc(con1->getURI().toString());
        epg1->addGbpEpGroupToProvContractRSrc(con1->getURI().toString());

        epg2->addGbpEpGroupToConsContractRSrc(con1->getURI().toString());
        epg3->addGbpEpGroupToConsContractRSrc(con1->getURI().toString());

        epg2->addGbpEpGroupToIntraContractRSrc(con2->getURI().toString());
        epg3->addGbpEpGroupToIntraContractRSrc(con2->getURI().toString());

        /* classifiers with port ranges */
        classifier3 = space->addGbpeL24Classifier("classifier3");
        classifier3->setOrder(10)
            .setEtherT(l2::EtherTypeEnumT::CONST_IPV4).setProt(6 /* TCP */)
            .setDFromPort(80).setDToPort(85);

        classifier4 = space->addGbpeL24Classifier("classifier4");
        classifier4->setOrder(20)
            .setEtherT(l2::EtherTypeEnumT::CONST_IPV4).setProt(6 /* TCP */)
            .setSFromPort(66).setSToPort(69)
            .setDFromPort(94).setDToPort(95);
        classifier10 = space->addGbpeL24Classifier("classifier10");
        classifier10->setOrder(30)
            .setEtherT(l2::EtherTypeEnumT::CONST_IPV4).setProt(1 /* ICMP */)
            .setIcmpType(10).setIcmpCode(5);
        con3 = space->addGbpContract("contract3");
        con3->addGbpSubject("3_subject1")->addGbpRule("3_1_rule1")
            ->setOrder(1)
            .setDirection(DirectionEnumT::CONST_IN)
            .addGbpRuleToClassifierRSrc(classifier3->getURI().toString())
            ->setTargetL24Classifier(classifier3->getURI());
        con3->addGbpSubject("3_subject1")->addGbpRule("3_1_rule1")
            ->addGbpRuleToActionRSrcAllowDenyAction(
                action1->getURI().toString())
            ->setTargetAllowDenyAction(action1->getURI());
        con3->addGbpSubject("3_subject1")->addGbpRule("3_1_rule2")
            ->setOrder(2)
            .setDirection(DirectionEnumT::CONST_IN)
            .addGbpRuleToClassifierRSrc(classifier4->getURI().toString())
            ->setTargetL24Classifier(classifier4->getURI());
        con3->addGbpSubject("3_subject1")->addGbpRule("3_1_rule3")
            ->setOrder(3)
            .setDirection(DirectionEnumT::CONST_IN)
            .addGbpRuleToClassifierRSrc(classifier10->getURI().toString())
            ->setTargetL24Classifier(classifier10->getURI());
        epg0->addGbpEpGroupToProvContractRSrc(con3->getURI().toString());
        epg1->addGbpEpGroupToConsContractRSrc(con3->getURI().toString());

           //action 3      
        action3 = space->addGbpAllowDenyAction("action3");
        action3->setAllow(0);
          //action 4
        action4 =  space->addGbpLogAction("action4");

        con4 = space->addGbpContract("contract4");
        con4->addGbpSubject("4_subject1")->addGbpRule("4_1_rule1")
            ->setOrder(100)
            .setDirection(DirectionEnumT::CONST_IN)
            .addGbpRuleToClassifierRSrc(classifier1->getURI().toString())
            ->setTargetL24Classifier(classifier1->getURI());
        con4->addGbpSubject("4_subject1")->addGbpRule("4_1_rule2")
            ->setOrder(200)
            .setDirection(DirectionEnumT::CONST_OUT)
            .addGbpRuleToClassifierRSrc(classifier2->getURI().toString())
            ->setTargetL24Classifier(classifier2->getURI());
        con4->addGbpSubject("4_subject1")->addGbpRule("4_1_rule1")
            ->addGbpRuleToActionRSrcAllowDenyAction(action3->getURI().toString())
            ->setTargetAllowDenyAction(action3->getURI());
        con4->addGbpSubject("4_subject1")->addGbpRule("4_1_rule1")
            ->addGbpRuleToActionRSrcLogAction(action4->getURI().toString())
            ->setTargetLogAction(action4->getURI());
        con4->addGbpSubject("4_subject1")->addGbpRule("4_1_rule2")
            ->addGbpRuleToActionRSrcAllowDenyAction(action3->getURI().toString())
            ->setTargetAllowDenyAction(action3->getURI());
        con4->addGbpSubject("4_subject1")->addGbpRule("4_1_rule2")
            ->addGbpRuleToActionRSrcLogAction(action4->getURI().toString())
            ->setTargetLogAction(action4->getURI());

        epg0->addGbpEpGroupToProvContractRSrc(con4->getURI().toString());
        epg1->addGbpEpGroupToConsContractRSrc(con4->getURI().toString()); 

        con5 = space->addGbpContract("contract5");
        con5->addGbpSubject("5_subject1")->addGbpRule("5_rule1")
            ->setOrder(300)
            .setDirection(DirectionEnumT::CONST_IN)
            .addGbpRuleToClassifierRSrc(classifier11->getURI().toString())
            ->setTargetL24Classifier(classifier11->getURI());
        con5->addGbpSubject("5_subject1")->addGbpRule("5_rule1")
            ->addGbpRuleToClassifierRSrc(classifier12->getURI().toString())
           ->setTargetL24Classifier(classifier12->getURI());
        con5->addGbpSubject("5_subject1")->addGbpRule("5_rule1")
            ->addGbpRuleToClassifierRSrc(classifier13->getURI().toString())
            ->setTargetL24Classifier(classifier13->getURI());
        con5->addGbpSubject("5_subject1")->addGbpRule("5_rule1")
            ->addGbpRuleToActionRSrcAllowDenyAction(action3->getURI().toString())
            ->setTargetAllowDenyAction(action3->getURI());
        con5->addGbpSubject("5_subject1")->addGbpRule("5_rule1")
            ->addGbpRuleToClassifierRSrc(classifier14->getURI().toString())
            ->setTargetL24Classifier(classifier14->getURI());
        con5->addGbpSubject("5_subject1")->addGbpRule("5_rule1")
            ->addGbpRuleToActionRSrcAllowDenyAction(action3->getURI().toString())
            ->setTargetAllowDenyAction(action3->getURI());
        con5->addGbpSubject("5_subject1")->addGbpRule("5_rule1")
            ->addGbpRuleToActionRSrcLogAction(action4->getURI().toString())
            ->setTargetLogAction(action4->getURI());

        epg0->addGbpEpGroupToProvContractRSrc(con5->getURI().toString());
        epg1->addGbpEpGroupToConsContractRSrc(con5->getURI().toString());
      
        con6 = space->addGbpContract("contract6");
        con6->addGbpSubject("6_subject1")->addGbpRule("6_rule1")
             ->setOrder(400)
             .setDirection(DirectionEnumT::CONST_OUT)
             .addGbpRuleToClassifierRSrc(classifier11->getURI().toString())
             ->setTargetL24Classifier(classifier11->getURI());
        con6->addGbpSubject("6_subject1")->addGbpRule("6_rule1")
             ->addGbpRuleToClassifierRSrc(classifier12->getURI().toString())
            ->setTargetL24Classifier(classifier12->getURI());
        con6->addGbpSubject("6_subject1")->addGbpRule("6_rule1")
             ->addGbpRuleToClassifierRSrc(classifier13->getURI().toString())
             ->setTargetL24Classifier(classifier13->getURI());
        con6->addGbpSubject("6_subject1")->addGbpRule("6_rule1")
             ->addGbpRuleToClassifierRSrc(classifier14->getURI().toString())
            ->setTargetL24Classifier(classifier14->getURI());
        con6->addGbpSubject("6_subject1")->addGbpRule("6_rule1")
             ->addGbpRuleToClassifierRSrc(classifier15->getURI().toString())
             ->setTargetL24Classifier(classifier15->getURI());
        con6->addGbpSubject("6_subject1")->addGbpRule("6_rule1")
             ->addGbpRuleToClassifierRSrc(classifier16->getURI().toString())
             ->setTargetL24Classifier(classifier16->getURI());
        con6->addGbpSubject("6_subject1")->addGbpRule("6_rule1")
             ->addGbpRuleToActionRSrcAllowDenyAction(action3->getURI().toString())
             ->setTargetAllowDenyAction(action3->getURI());
        con6->addGbpSubject("6_subject1")->addGbpRule("6_rule1")
             ->addGbpRuleToActionRSrcLogAction(action4->getURI().toString())
             ->setTargetLogAction(action4->getURI());

        epg0->addGbpEpGroupToProvContractRSrc(con6->getURI().toString());
        epg1->addGbpEpGroupToConsContractRSrc(con6->getURI().toString());
   
        con7 = space->addGbpContract("contract7");
        con7->addGbpSubject("7_subject1")->addGbpRule("7_rule1")
             ->setOrder(500)
             .setDirection(DirectionEnumT::CONST_OUT)
             .addGbpRuleToClassifierRSrc(classifier11->getURI().toString())
             ->setTargetL24Classifier(classifier11->getURI());
        con7->addGbpSubject("7_subject1")->addGbpRule("7_rule1")
             ->addGbpRuleToClassifierRSrc(classifier15->getURI().toString())
            ->setTargetL24Classifier(classifier15->getURI());
        con7->addGbpSubject("7_subject1")->addGbpRule("7_rule1")
             ->addGbpRuleToClassifierRSrc(classifier16->getURI().toString())
             ->setTargetL24Classifier(classifier16->getURI());
        con7->addGbpSubject("7_subject1")->addGbpRule("7_rule1")
             ->addGbpRuleToActionRSrcAllowDenyAction(action3->getURI().toString())
             ->setTargetAllowDenyAction(action3->getURI());
        con7->addGbpSubject("7_subject1")->addGbpRule("7_rule1")
             ->addGbpRuleToActionRSrcLogAction(action4->getURI().toString())
             ->setTargetLogAction(action4->getURI());

        epg0->addGbpEpGroupToProvContractRSrc(con7->getURI().toString());
        epg1->addGbpEpGroupToConsContractRSrc(con7->getURI().toString());

        con8 = space->addGbpContract("contract8");
        con8->addGbpSubject("8_subject1")->addGbpRule("8_rule1")
             ->setOrder(600)
             .setDirection(DirectionEnumT::CONST_IN)
             .addGbpRuleToClassifierRSrc(classifier11->getURI().toString())
             ->setTargetL24Classifier(classifier11->getURI());
        con8->addGbpSubject("8_subject1")->addGbpRule("8_rule1")
             ->addGbpRuleToClassifierRSrc(classifier15->getURI().toString())
             ->setTargetL24Classifier(classifier15->getURI());
        con8->addGbpSubject("8_subject1")->addGbpRule("8_rule1")
             ->addGbpRuleToClassifierRSrc(classifier16->getURI().toString())
             ->setTargetL24Classifier(classifier16->getURI());
        con8->addGbpSubject("8_subject1")->addGbpRule("8_rule1")
             ->addGbpRuleToClassifierRSrc(classifier18->getURI().toString())
             ->setTargetL24Classifier(classifier18->getURI());
        con8->addGbpSubject("8_subject1")->addGbpRule("8_rule1")
             ->addGbpRuleToActionRSrcAllowDenyAction(action3->getURI().toString())
             ->setTargetAllowDenyAction(action3->getURI());
        con8->addGbpSubject("8_subject1")->addGbpRule("8_rule1")
             ->addGbpRuleToActionRSrcLogAction(action4->getURI().toString())
            ->setTargetLogAction(action4->getURI());
        epg2->addGbpEpGroupToProvContractRSrc(con8->getURI().toString());
        epg3->addGbpEpGroupToConsContractRSrc(con8->getURI().toString());

        con9 = space->addGbpContract("contract9");
        con9->addGbpSubject("9_subject1")->addGbpRule("9_rule1")
             ->setOrder(700)
             .setDirection(DirectionEnumT::CONST_IN)
             .addGbpRuleToClassifierRSrc(classifier11->getURI().toString())
             ->setTargetL24Classifier(classifier11->getURI());
        con9->addGbpSubject("9_subject1")->addGbpRule("9_rule1")
             ->addGbpRuleToClassifierRSrc(classifier15->getURI().toString())
             ->setTargetL24Classifier(classifier15->getURI());
        con9->addGbpSubject("9_subject1")->addGbpRule("9_rule1")
             ->addGbpRuleToClassifierRSrc(classifier16->getURI().toString())
             ->setTargetL24Classifier(classifier16->getURI());
        con9->addGbpSubject("9_subject1")->addGbpRule("9_rule1")
             ->addGbpRuleToClassifierRSrc(classifier18->getURI().toString())
             ->setTargetL24Classifier(classifier18->getURI());
        con9->addGbpSubject("9_subject1")->addGbpRule("9_rule1")
             ->addGbpRuleToClassifierRSrc(classifier19->getURI().toString())
             ->setTargetL24Classifier(classifier19->getURI());
        con9->addGbpSubject("9_subject1")->addGbpRule("9_rule1")
             ->addGbpRuleToClassifierRSrc(classifier20->getURI().toString())
             ->setTargetL24Classifier(classifier20->getURI());
        con9->addGbpSubject("9_subject1")->addGbpRule("9_rule1")
             ->addGbpRuleToActionRSrcAllowDenyAction(action3->getURI().toString())
             ->setTargetAllowDenyAction(action3->getURI());
        con9->addGbpSubject("9_subject1")->addGbpRule("9_rule1")
             ->addGbpRuleToActionRSrcLogAction(action4->getURI().toString())
             ->setTargetLogAction(action4->getURI());
        epg2->addGbpEpGroupToProvContractRSrc(con9->getURI().toString());
        epg3->addGbpEpGroupToConsContractRSrc(con9->getURI().toString());


        con10 = space->addGbpContract("contract10");
        con10->addGbpSubject("10_subject1")->addGbpRule("10_rule1")
             ->setOrder(800)
             .setDirection(DirectionEnumT::CONST_IN)
             .addGbpRuleToClassifierRSrc(classifier11->getURI().toString())
             ->setTargetL24Classifier(classifier11->getURI());
        con10->addGbpSubject("10_subject1")->addGbpRule("10_rule1")
             ->addGbpRuleToClassifierRSrc(classifier15->getURI().toString())
             ->setTargetL24Classifier(classifier15->getURI());
        con10->addGbpSubject("10_subject1")->addGbpRule("10_rule1")
             ->addGbpRuleToClassifierRSrc(classifier18->getURI().toString())
             ->setTargetL24Classifier(classifier18->getURI());
        con10->addGbpSubject("10_subject1")->addGbpRule("10_rule1")
             ->addGbpRuleToActionRSrcAllowDenyAction(action3->getURI().toString())
             ->setTargetAllowDenyAction(action3->getURI());
        con10->addGbpSubject("10_subject1")->addGbpRule("10_rule1")
             ->addGbpRuleToActionRSrcLogAction(action4->getURI().toString())
             ->setTargetLogAction(action4->getURI());
       epg2->addGbpEpGroupToProvContractRSrc(con10->getURI().toString());
       epg3->addGbpEpGroupToConsContractRSrc(con10->getURI().toString());

        mutator.commit();
    }
    //! @endcond
};

} // namespace opflexagent

#endif /* OPFLEXAGENT_TEST_MODBFIXTURE_H_ */
