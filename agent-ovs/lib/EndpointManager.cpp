/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Implementation for EndpointManager class.
 *
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <set>
#include <algorithm>

#include <opflex/modb/Mutator.h>
#include <modelgbp/ascii/StringMatchTypeEnumT.hpp>
#include <modelgbp/gbp/RoutingModeEnumT.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <modelgbp/platform/RemoteInventoryTypeEnumT.hpp>
#include <modelgbp/gbpe/EncapTypeEnumT.hpp>

#include <opflexagent/Agent.h>
#include <opflexagent/EndpointManager.h>
#include <opflexagent/Network.h>
#include <opflexagent/logging.h>
#include <opflexagent/PrometheusManager.h>

namespace opflexagent {

using std::string;
using std::set;
using std::vector;
using std::unordered_set;
using std::shared_ptr;
using std::make_shared;
using std::unique_lock;
using std::mutex;
using std::pair;
using opflex::modb::class_id_t;
using opflex::modb::URI;
using opflex::modb::MAC;
using opflex::modb::Mutator;
using boost::optional;
using boost::algorithm::starts_with;
using boost::algorithm::ends_with;
using boost::algorithm::contains;
using boost::asio::ip::address;
using boost::asio::ip::address_v6;
typedef EndpointListener::uri_set_t uri_set_t;

static const string VM_NAME_ATTR("vm-name");
static const string NULL_MAC_ADDR("00:00:00:00:00:00");


EndpointManager::EndpointManager(Agent& agent_,
                                 opflex::ofcore::OFFramework& framework_,
                                 PolicyManager& policyManager_,
                                 AgentPrometheusManager& prometheusManager_)
    : agent(agent_), framework(framework_), policyManager(policyManager_),
      prometheusManager(prometheusManager_), epgMappingListener(*this) {

}

EndpointManager::~EndpointManager() {

}

EndpointManager::EndpointState::EndpointState() : endpoint(new Endpoint()) {

}

void EndpointManager::start() {
    using namespace modelgbp::gbpe;
    using namespace modelgbp::inv;

    LOG(DEBUG) << "Starting endpoint manager";

    EpgMapping::registerListener(framework, &epgMappingListener);
    EpAttributeSet::registerListener(framework, &epgMappingListener);
    RemoteEndpointInventory::registerListener(framework, &epgMappingListener);
    RemoteInventoryEp::registerListener(framework, &epgMappingListener);
    RemoteIp::registerListener(framework, &epgMappingListener);
    policyManager.registerListener(this);
}

void EndpointManager::stop() {
    using namespace modelgbp::inv;
    using namespace modelgbp::gbpe;
    LOG(DEBUG) << "Stopping endpoint manager";

    EpgMapping::unregisterListener(framework, &epgMappingListener);
    EpAttributeSet::unregisterListener(framework, &epgMappingListener);
    RemoteEndpointInventory::unregisterListener(framework, &epgMappingListener);
    RemoteInventoryEp::unregisterListener(framework, &epgMappingListener);

    policyManager.unregisterListener(this);

    unique_lock<mutex> guard(ep_mutex);
    ep_map.clear();
    ext_ep_map.clear();
    group_ep_map.clear();
    group_remote_ep_map.clear();
    remote_ep_group_map.clear();
    remote_ep_uuid_map.clear();
    secgrp_ep_map.clear();
    ipm_group_ep_map.clear();
    ipm_nexthop_if_ep_map.clear();
    iface_ep_map.clear();
    access_iface_ep_map.clear();
    access_uplink_ep_map.clear();
    epgmapping_ep_map.clear();
}

void EndpointManager::registerListener(EndpointListener* listener) {
    unique_lock<mutex> guard(listener_mutex);
    endpointListeners.push_back(listener);
}

void EndpointManager::unregisterListener(EndpointListener* listener) {
    unique_lock<mutex> guard(listener_mutex);
    endpointListeners.remove(listener);
}

void EndpointManager::notifyListeners(const string& uuid) {
    unique_lock<mutex> guard(listener_mutex);
    for (EndpointListener* listener : endpointListeners) {
        listener->endpointUpdated(uuid);
    }
}

void EndpointManager::notifyRemoteListeners(const string& uuid) {
    unique_lock<mutex> guard(listener_mutex);
    for (EndpointListener* listener : endpointListeners) {
        listener->remoteEndpointUpdated(uuid);
    }
}

void EndpointManager::notifyExternalEndpointListeners(
    const string& uuid) {
    unique_lock<mutex> guard(listener_mutex);
    for (EndpointListener* listener : endpointListeners) {
        listener->externalEndpointUpdated(uuid);
    }
}

void EndpointManager::notifyListeners(const uri_set_t& secGroups) {
    unique_lock<mutex> guard(listener_mutex);
    for (EndpointListener* listener : endpointListeners) {
        listener->secGroupSetUpdated(secGroups);
    }
}

void EndpointManager::notifyLocalExternalDomainListeners(
        const URI& uri) {
    unique_lock<mutex> guard(listener_mutex);
    for (EndpointListener* listener : endpointListeners) {
        listener->localExternalDomainUpdated(uri);
    }
}

Agent& EndpointManager::getAgent (void)
{
    return agent;
}

shared_ptr<const Endpoint> EndpointManager::getEndpoint(const string& uuid) {
    unique_lock<mutex> guard(ep_mutex);

    ep_map_t::const_iterator it = ep_map.find(uuid);
    if (it != ep_map.end())
        return it->second.endpoint;
    it = ext_ep_map.find(uuid);
    if (it != ext_ep_map.end())
        return it->second.endpoint;
    return shared_ptr<const Endpoint>();
}

optional<URI> EndpointManager::getComputedEPG(const string& uuid) {
    unique_lock<mutex> guard(ep_mutex);
    ep_map_t::const_iterator it = ep_map.find(uuid);
    if (it != ep_map.end())
        return it->second.egURI;
    it = ext_ep_map.find(uuid);
    if (it != ext_ep_map.end())
        return it->second.egURI;
    return boost::none;
}

static bool validateIp(const string& ip, bool allowLinkLocal = false) {
    boost::system::error_code ec;
    address addr = address::from_string(ip, ec);
    if (ec) return false;
    if (!allowLinkLocal && network::is_link_local(addr))
        return false;
    return true;
}

template <typename T>
static void updateEpMap(const optional<string>& oldVal,
                        const optional<string>& val,
                        T& val_map,
                        const string& uuid) {
    if (oldVal != val) {
        if (oldVal) {
            unordered_set<string>& eps = val_map[oldVal.get()];
            eps.erase(uuid);
            if (eps.empty())
                val_map.erase(oldVal.get());
        }
        if (val) {
            val_map[val.get()].insert(uuid);
        }
    }
}

void EndpointManager::updateEndpoint(const Endpoint& endpoint) {
    using namespace modelgbp::gbp;
    using namespace modelgbp::gbpe;
    using namespace modelgbp::epdr;

    unique_lock<mutex> guard(ep_mutex);
    const string& uuid = endpoint.getUUID();
    EndpointState& es = ep_map[uuid];
    unordered_set<uri_set_t> notifySecGroupSets;
    EndpointListener::uri_set_t notifyExtDomSets;

    // Refresh IP to EP map for this endpoint, to track delete/update
    // of this IP list
    for (const string& ip : es.endpoint->getIPs()) {
        if (!validateIp(ip))
            continue;
        ip_local_ep_map.erase(ip);
    }


    // update security group mapping
    const set<URI>& oldSecGroups = es.endpoint->getSecurityGroups();
    const set<URI>& secGroups = endpoint.getSecurityGroups();
    if (secGroups != oldSecGroups) {
        auto it = secgrp_ep_map.find(oldSecGroups);
        if (it != secgrp_ep_map.end()) {
            it->second.erase(uuid);

            if (it->second.empty()) {
                secgrp_ep_map.erase(it);
                notifySecGroupSets.insert(oldSecGroups);
            }
        }
    }
    str_uset_t& ep_set = secgrp_ep_map[secGroups];
    if (ep_set.find(uuid) == ep_set.end()) {
        ep_set.insert(uuid);
        notifySecGroupSets.insert(secGroups);
    }

    // update interface name to endpoint mapping
    const optional<string>& oldIface = es.endpoint->getInterfaceName();
    const optional<string>& iface = endpoint.getInterfaceName();
    updateEpMap(oldIface, iface, iface_ep_map, uuid);

    // update access interface name to endpoint mapping
    const optional<string>& oldAccess = es.endpoint->getAccessInterface();
    const optional<string>& access = endpoint.getAccessInterface();
    updateEpMap(oldAccess, access, access_iface_ep_map, uuid);

    // update access uplink interface name to endpoint mapping
    const optional<string>& oldUplink =
        es.endpoint->getAccessUplinkInterface();
    const optional<string>& uplink = endpoint.getAccessUplinkInterface();
    updateEpMap(oldUplink, uplink, access_uplink_ep_map, uuid);

    // Update IP Mapping next hop interface to endpoint mapping
    for (const Endpoint::IPAddressMapping& ipm :
             es.endpoint->getIPAddressMappings()) {
        if (!ipm.getNextHopIf()) continue;
        unordered_set<string>& eps =
            ipm_nexthop_if_ep_map[ipm.getNextHopIf().get()];
        eps.erase(uuid);
        if (eps.empty())
            ipm_nexthop_if_ep_map.erase(ipm.getNextHopIf().get());
    }
    for (const Endpoint::IPAddressMapping& ipm :
             endpoint.getIPAddressMappings()) {
        if (!ipm.getNextHopIf()) continue;
        ipm_nexthop_if_ep_map[ipm.getNextHopIf().get()].insert(uuid);
    }

    // update epg mapping alias to endpoint mapping
    const optional<string>& oldEpgmap = es.endpoint->getEgMappingAlias();
    const optional<string>& epgmap = endpoint.getEgMappingAlias();
    updateEpMap(oldEpgmap, epgmap, epgmapping_ep_map, uuid);

    es.endpoint = make_shared<const Endpoint>(endpoint);
    optional<EndpointListener::uri_set_t &> extDomSets(notifyExtDomSets);
    updateEndpointLocal(uuid, extDomSets);
    guard.unlock();
    for (auto& s : notifyExtDomSets) {
        notifyLocalExternalDomainListeners(s);
    }
    notifyListeners(uuid);

    for (auto& s : notifySecGroupSets) {
        notifyListeners(s);
    }
}

void EndpointManager::removeEndpoint(const string& uuid) {
    using namespace modelgbp::epdr;
    using namespace modelgbp::epr;
    using namespace modelgbp::gbpe;

    unique_lock<mutex> guard(ep_mutex);
    Mutator mutator(framework, "policyelement");
    unordered_set<uri_set_t> notifySecGroupSets;
    uri_set_t notifyExtDomSets;

    auto it = ep_map.find(uuid);
    if (it != ep_map.end()) {
        EndpointState& es = it->second;
        auto& ep_name = es.endpoint->getAccessInterface();
        if (ep_name)
            prometheusManager.removeEpCounter(uuid, ep_name.get());
        else
            LOG(ERROR) << "ep name not found for uuid:" << uuid;
        // remove any associated modb entries
        for (const URI& locall2ep : es.locall2EPs) {
            LocalL2Ep::remove(framework, locall2ep);
        }
        for (const URI& locall3ep : es.locall3EPs) {
            LocalL3Ep::remove(framework, locall3ep);
        }
        for (const string& ip : es.endpoint->getIPs()) {
            if (!validateIp(ip))
                continue;
            ip_local_ep_map.erase(ip);
        }
        for (const URI& l2ep : es.l2EPs) {
            // The contained objects dont get deleted during make check tests.
            // Free them up here.
            auto l2e = L2Ep::resolve(framework, l2ep);
            if (l2e) {
                vector<shared_ptr<SecurityGroupContext> > outSGC;
                l2e.get()->resolveEprSecurityGroupContext(outSGC);
                for (auto &sgc : outSGC)
                    sgc->remove();
                auto epas = l2e.get()->resolveGbpeReportedEpAttributeSet();
                if (epas) {
                    vector<shared_ptr<ReportedEpAttribute> > outEPA;
                    epas.get()->resolveGbpeReportedEpAttribute(outEPA);
                    for (auto &epa : outEPA)
                        epa->remove();
                    epas.get()->remove();
                }
            }
            L2Ep::remove(framework, l2ep);
        }
        for (const URI& l3ep : es.l3EPs) {
            // The contained objects dont get deleted during make check tests.
            // Free them up here.
            auto l3e = L3Ep::resolve(framework, l3ep);
            if (l3e) {
                vector<shared_ptr<SecurityGroupContext> > outSGC;
                l3e.get()->resolveEprSecurityGroupContext(outSGC);
                for (auto &sgc : outSGC)
                    sgc->remove();
            }
            L3Ep::remove(framework, l3ep);
        }
        EpCounter::remove(framework, uuid);
        if (es.egURI) {
            group_ep_map_t::iterator it = group_ep_map.find(es.egURI.get());
            if (it != group_ep_map.end()) {
                it->second.erase(uuid);
                if (it->second.empty()) {
                    group_ep_map.erase(it);
                    if(es.endpoint->isExternal()){
                        notifyExtDomSets.insert(es.egURI.get());
                        local_ext_dom_map.erase(es.egURI.get());
                    }
                }
            }
        }

        {
            const set<URI>& secGroups = es.endpoint->getSecurityGroups();
            auto sgit = secgrp_ep_map.find(secGroups);
            if (sgit != secgrp_ep_map.end()) {
                sgit->second.erase(uuid);

                if (sgit->second.empty()) {
                    secgrp_ep_map.erase(sgit);
                    notifySecGroupSets.insert(secGroups);
                }
            }
        }

        for (const URI& ipmGrp : es.ipMappingGroups) {
            auto it = ipm_group_ep_map.find(ipmGrp);
            if (it != ipm_group_ep_map.end()) {
                it->second.erase(uuid);
                if (it->second.empty()) {
                    ipm_group_ep_map.erase(it);
                }
            }
        }

        updateEpMap(es.endpoint->getInterfaceName(), boost::none,
                    iface_ep_map, uuid);
        updateEpMap(es.endpoint->getAccessInterface(), boost::none,
                    access_iface_ep_map, uuid);
        updateEpMap(es.endpoint->getAccessUplinkInterface(), boost::none,
                    access_uplink_ep_map, uuid);

        for (const Endpoint::IPAddressMapping& ipm :
                 es.endpoint->getIPAddressMappings()) {
            if (!ipm.getNextHopIf()) continue;
            unordered_set<string>& eps =
                ipm_nexthop_if_ep_map[ipm.getNextHopIf().get()];
            eps.erase(uuid);
            if (eps.empty())
                ipm_nexthop_if_ep_map.erase(ipm.getNextHopIf().get());
        }

        updateEpMap(es.endpoint->getEgMappingAlias(), boost::none,
                    epgmapping_ep_map, uuid);

        ep_map.erase(it);
    }
    mutator.commit();
    guard.unlock();
    notifyListeners(uuid);
    for (auto& s : notifySecGroupSets) {
        notifyListeners(s);
    }
    for(auto& s: notifyExtDomSets) {
        notifyLocalExternalDomainListeners(s);
    }
}

optional<URI> EndpointManager::resolveEpgMapping(EndpointState& es) {
    using namespace modelgbp::gbpe;
    using namespace modelgbp::ascii;
    if(es.endpoint->isExternal()) {
        return boost::none;
    }
    const optional<string>& mappingAlias = es.endpoint->getEgMappingAlias();
    if (!mappingAlias) return boost::none;
    optional<shared_ptr<EpgMapping> > mapping =
        EpgMapping::resolve(framework, mappingAlias.get());
    if (!mapping) return boost::none;

    vector<shared_ptr<AttributeMappingRule> > rules;
    mapping.get()->resolveGbpeAttributeMappingRule(rules);

    OrderComparator<shared_ptr<AttributeMappingRule> > ruleComp;
    stable_sort(rules.begin(), rules.end(), ruleComp);

    for (shared_ptr<AttributeMappingRule>& rule : rules) {
        optional<const string&> attrName = rule->getAttributeName();
        optional<const string&> matchString = rule->getMatchString();
        if (!attrName || !matchString) continue;
        uint8_t type = rule->getMatchType(StringMatchTypeEnumT::CONST_EQUALS);
        bool negated = 0 != rule->getNegated(0);

        // Get value of attribute from endpoint index
        string attrValue;
        auto it = es.endpoint->getAttributes().find(attrName.get());
        if (it != es.endpoint->getAttributes().end())
            attrValue = it->second;
        else {
            it = es.epAttrs.find(attrName.get());
            if (it != es.epAttrs.end())
                attrValue = it->second;
        }

        // apply the match
        bool matches;
        switch (type) {
        case StringMatchTypeEnumT::CONST_CONTAINS:
            matches = contains(attrValue, matchString.get());
            break;
        case StringMatchTypeEnumT::CONST_STARTSWITH:
            matches = starts_with(attrValue, matchString.get());
            break;
        case StringMatchTypeEnumT::CONST_ENDSWITH:
            matches = ends_with(attrValue, matchString.get());
            break;
        case StringMatchTypeEnumT::CONST_EQUALS:
            matches = (attrValue == matchString.get());
            break;
        default:
            // unknown match always fails
            matches = negated;
            break;
        }
        matches = (matches != negated);

        if (matches) {
            optional<shared_ptr<MappingRuleToGroupRSrc> > egSrc =
                rule->resolveGbpeMappingRuleToGroupRSrc();
            if (egSrc && egSrc.get()->isTargetSet())
                return egSrc.get()->getTargetURI().get();
        }
    }

    // No matching rule, use default mapping
    optional<shared_ptr<EpgMappingToDefaultGroupRSrc> > egSrc =
        mapping.get()->resolveGbpeEpgMappingToDefaultGroupRSrc();
    if (egSrc && egSrc.get()->isTargetSet())
        return egSrc.get()->getTargetURI().get();

    return boost::none;
}

void EndpointManager::updateEndpointRemote(const URI& uri) {
    LOG(DEBUG) << "Remote endpoint updated " << uri;
    auto ep = modelgbp::inv::RemoteInventoryEp::resolve(framework, uri);

    optional<string> uuid;

    unique_lock<mutex> guard(ep_mutex);
    if (!ep || !ep.get()->isUuidSet()) {
        auto it = remote_ep_uuid_map.find(uri);
        if (it != remote_ep_uuid_map.end()) {
            // removed endpoint
            uuid = it->second;
            auto git = remote_ep_group_map.find(uuid.get());
            if (git != remote_ep_group_map.end()) {
                group_remote_ep_map.erase(git->second);
                remote_ep_group_map.erase(git);
            }
            remote_ep_uuid_map.erase(it);
        }
    } else {
        // added or updated endpoint
        uuid = ep.get()->getUuid().get();
        remote_ep_uuid_map.emplace(uri, uuid.get());

        optional<URI> egUri, oldEgUri;
        auto epg = ep.get()->resolveInvRemoteInventoryEpToGroupRSrc();
        if (epg)
            egUri = epg.get()->getTargetURI();

        auto uit = remote_ep_group_map.find(uuid.get());
        if (uit != remote_ep_group_map.end())
            oldEgUri = uit->second;

        if (oldEgUri != egUri) {
            if (oldEgUri) {
                unordered_set<string>& eps =
                    group_remote_ep_map[oldEgUri.get()];
                eps.erase(uuid.get());
                if (eps.empty())
                    group_remote_ep_map.erase(oldEgUri.get());
            }
            if (egUri) {
                group_remote_ep_map[egUri.get()].insert(uuid.get());
                remote_ep_group_map.emplace(uuid.get(), egUri.get());
            } else {
                remote_ep_group_map.erase(uuid.get());
            }
        }
    }
    guard.unlock();
    if (uuid)
        notifyRemoteListeners(uuid.get());
}

void EndpointManager::updateEndpointExternal(const Endpoint& endpoint) {
    using namespace modelgbp::gbp;
    using namespace modelgbp::gbpe;
    using namespace modelgbp::epdr;

    unique_lock<mutex> guard(ep_mutex);
    const string& uuid = endpoint.getUUID();
    EndpointState& es = ext_ep_map[uuid];
    unordered_set<uri_set_t> notifySecGroupSets;

    // update security group mapping
    const set<URI>& oldSecGroups = es.endpoint->getSecurityGroups();
    const set<URI>& secGroups = endpoint.getSecurityGroups();
    if (secGroups != oldSecGroups) {
        auto it = secgrp_ep_map.find(oldSecGroups);
        if (it != secgrp_ep_map.end()) {
            it->second.erase(uuid);

            if (it->second.empty()) {
                secgrp_ep_map.erase(it);
                notifySecGroupSets.insert(oldSecGroups);
            }
        }
    }
    str_uset_t& ep_set = secgrp_ep_map[secGroups];
    if (ep_set.find(uuid) == ep_set.end()) {
        ep_set.insert(uuid);
        notifySecGroupSets.insert(secGroups);
    }

    // update endpoint group to endpoint mapping
    const optional<URI>& oldEgURI = es.egURI;

    optional<URI> egURI = endpoint.getEgURI();
   // update endpoint group to endpoint mapping
    if(oldEgURI != egURI) {
        if (oldEgURI) {
            unordered_set<string>& eps = group_ep_map[oldEgURI.get()];
            eps.erase(uuid);
            if (eps.empty())
                group_ep_map.erase(oldEgURI.get());
        }
        if (egURI) {
            group_ep_map[egURI.get()].insert(uuid);
        }
        es.egURI = std::move(egURI);
    }

    // update interface name to endpoint mapping
    const optional<string>& oldIface = es.endpoint->getInterfaceName();
    const optional<string>& iface = endpoint.getInterfaceName();
    updateEpMap(oldIface, iface, iface_ep_map, uuid);

    // update access interface name to endpoint mapping
    const optional<string>& oldAccess = es.endpoint->getAccessInterface();
    const optional<string>& access = endpoint.getAccessInterface();
    updateEpMap(oldAccess, access, access_iface_ep_map, uuid);

    // update access uplink interface name to endpoint mapping
    const optional<string>& oldUplink =
    es.endpoint->getAccessUplinkInterface();
    const optional<string>& uplink = endpoint.getAccessUplinkInterface();
    updateEpMap(oldUplink, uplink, access_uplink_ep_map, uuid);

    // TBD: SecurityGroups,FloatingIPs and VMM reporting are not required for
    // External EP

    shared_ptr<Endpoint> ep = make_shared<Endpoint>(endpoint);
    // Update ExternalEndpoint object in the MODB, which will trigger
    // resolution of the external interface and external domain, if
    // needed.
    Mutator mutator(framework, "policyelement");
    optional<shared_ptr<ExternalDiscovered> > extD =
        ExternalDiscovered::resolve(framework);
    if (extD) {
        shared_ptr<ExternalL3Ep> extL3Ep = extD.get()->addEpdrExternalL3Ep(uuid);
        extL3Ep->setMac(ep->getMAC().get());
        /*There should be a single IP for an external endpoint*/
        for (const string& ip : ep->getIPs()) {
            if (!validateIp(ip)) {
                LOG(ERROR) << "Invalid address: " << ip;
                continue;
            }
            extL3Ep->setIp(ip);
        }
        if(ep->getExtInterfaceURI()) {
            extL3Ep->addEpdrExternalL3EpToPathAttRSrc()
                   ->setTargetExternalInterface(ep->getExtInterfaceURI().get());
            optional<shared_ptr<RoutingDomain>> rd;
            rd = policyManager.getRDForExternalInterface(
                    ep->getExtInterfaceURI().get());
            if(rd) {
                ipmac_map_t &ip_mac_map = adj_ep_map[rd.get()->getURI()];
                for (const string& ip : ep->getIPs()) {
                    if (!validateIp(ip)) continue;
                    ip_mac_map[ip] = ep;
                }
            }
        } else {
            optional<shared_ptr<ExternalL3EpToPathAttRSrc>> ctx =
                    extL3Ep->resolveEpdrExternalL3EpToPathAttRSrc();
            if (ctx)
                ctx.get()->remove();
        }
        if(ep->getExtNodeURI()) {
            extL3Ep->addEpdrExternalL3EpToNodeAttRSrc()
                   ->setTargetExternalNode(ep->getExtNodeURI().get());
        } else {
            optional<shared_ptr<ExternalL3EpToNodeAttRSrc>> ctx =
                    extL3Ep->resolveEpdrExternalL3EpToNodeAttRSrc();
            if (ctx)
                ctx.get()->remove();
        }
        vector<shared_ptr<ExternalL3EpToSecGroupRSrc> > oldSecGrps;
        extL3Ep->resolveEpdrExternalL3EpToSecGroupRSrc(oldSecGrps);
        const set<URI>& secGrps = es.endpoint->getSecurityGroups();
        for (const shared_ptr<ExternalL3EpToSecGroupRSrc>& og :
                 oldSecGrps) {
            optional<URI> targ = og->getTargetURI();
            if (!targ || secGrps.find(targ.get()) == secGrps.end())
                og->remove();
        }
        for (const URI& sg : secGrps) {
            extL3Ep->addEpdrExternalL3EpToSecGroupRSrc(sg.toString());
        }
    }
    es.endpoint = ep;
    mutator.commit();
    guard.unlock();
    notifyExternalEndpointListeners(uuid);
    for (auto& s : notifySecGroupSets) {
        notifyListeners(s);
    }
}

void EndpointManager::removeEndpointExternal(const string& uuid) {
    using namespace modelgbp::epdr;
    using namespace modelgbp::epr;
    using namespace modelgbp::gbpe;
    using namespace modelgbp::gbp;
    unordered_set<uri_set_t> notifySecGroupSets;

    unique_lock<mutex> guard(ep_mutex);
    Mutator mutator(framework, "policyelement");

    auto it = ext_ep_map.find(uuid);
    optional<shared_ptr<RoutingDomain>> rd;
    if (it != ext_ep_map.end()) {
        EndpointState& es = it->second;
        // remove any associated modb entries
        auto& ep_name = es.endpoint->getAccessInterface();
        if (ep_name)
            prometheusManager.removeEpCounter(uuid, ep_name.get());
        else
            LOG(ERROR) << "ep name not found for uuid:" << uuid;
        ExternalL3Ep::remove(framework, uuid);
        EpCounter::remove(framework, uuid);
        rd = policyManager.getRDForExternalInterface(
                es.endpoint->getExtInterfaceURI().get());
        if(rd) {
            ipmac_map_t &ip_mac_map = adj_ep_map[rd.get()->getURI()];
            for (const string& ip : es.endpoint->getIPs()) {
                if (!validateIp(ip)) continue;
                auto ipm_it = ip_mac_map.find(ip);
                if(ipm_it != ip_mac_map.end()) {
                    ip_mac_map.erase(ipm_it);
                }
            }
        }
        if (es.egURI) {
            auto it = group_ep_map.find(es.egURI.get());
            if (it != group_ep_map.end()) {
                it->second.erase(uuid);
                if (it->second.empty()) {
                    group_ep_map.erase(it);
                }
            }
        }
        {
            const set<URI>& secGroups = es.endpoint->getSecurityGroups();
            auto sgit = secgrp_ep_map.find(secGroups);
            if (sgit != secgrp_ep_map.end()) {
                sgit->second.erase(uuid);

                if (sgit->second.empty()) {
                    secgrp_ep_map.erase(sgit);
                    notifySecGroupSets.insert(secGroups);
                }
            }
        }
        updateEpMap(es.endpoint->getInterfaceName(), boost::none,
                    iface_ep_map, uuid);
        updateEpMap(es.endpoint->getAccessInterface(), boost::none,
                    access_iface_ep_map, uuid);
        updateEpMap(es.endpoint->getAccessUplinkInterface(), boost::none,
                    access_uplink_ep_map, uuid);

        ext_ep_map.erase(it);
    }
    mutator.commit();
    guard.unlock();
    notifyExternalEndpointListeners(uuid);
    for (auto& s : notifySecGroupSets) {
        notifyListeners(s);
    }
}

bool EndpointManager::updateEndpointLocal(const string& uuid,
        const optional<EndpointListener::uri_set_t &> extDomSet) {
    using namespace modelgbp::gbp;
    using namespace modelgbp::gbpe;
    using namespace modelgbp::epdr;

    auto it = ep_map.find(uuid);
    if (it == ep_map.end()) return false;
    bool updated = false;

    EndpointState& es = it->second;

    // update endpoint group to endpoint mapping
    const optional<URI>& oldEgURI = es.egURI;

    // attempt to get endpoint group from a directly-configured group
    optional<URI> egURI = es.endpoint->getEgURI();
    if (!egURI) {
        // fall back to computing endpoint group from epg mapping
        egURI = resolveEpgMapping(es);
    }

    if (oldEgURI != egURI) {
        if (oldEgURI) {
            unordered_set<string>& eps = group_ep_map[oldEgURI.get()];
            eps.erase(uuid);
            if (eps.empty()) {
                group_ep_map.erase(oldEgURI.get());
                auto it = local_ext_dom_map.find(oldEgURI.get());
                if(it != local_ext_dom_map.end()) {
                    local_ext_dom_map.erase(it);
                    if(extDomSet) {
                        extDomSet.get().insert(oldEgURI.get());
                    }
                }
            }
        }
        if (egURI) {
            group_ep_map[egURI.get()].insert(uuid);
        }
        if(es.endpoint->isExternal()) {
            auto it = local_ext_dom_map.find(egURI.get());
            if(it == local_ext_dom_map.end()) {
                local_ext_dom_map.insert(std::make_pair(egURI.get(),
                                         es.endpoint->getExtEncapId()));
                if(extDomSet) {
                    extDomSet.get().insert(egURI.get());
                }
            }
        }
        es.egURI = egURI;
        updated = true;
    }

    unordered_set<URI> newlocall3eps;
    unordered_set<URI> newlocall2eps;
    unordered_set<URI> newipmgroups;

    Mutator mutator(framework, "policyelement");

    const optional<MAC>& mac = es.endpoint->getMAC();

    if (mac) {
        // Update LocalL2 objects in the MODB, which will trigger
        // resolution of the endpoint group and/or epg mapping, if
        // needed.
        optional<shared_ptr<L2Discovered> > l2d =
            L2Discovered::resolve(framework);
        if (l2d) {
            shared_ptr<LocalL2Ep> l2e = l2d.get()
                ->addEpdrLocalL2Ep(uuid);
            l2e->setMac(mac.get());
            if(es.endpoint->isExternal()) {
                l2e->setDom(egURI.get().toString());
                l2e->setExtEncapType(
                        modelgbp::gbpe::EncapTypeEnumT::CONST_VLAN);
                l2e->setExtEncapId(es.endpoint->getExtEncapId());
            } else {
                if (egURI) {
                    l2e->addEpdrEndPointToGroupRSrc()
                        ->setTargetEpGroup(egURI.get());
                } else {
                    optional<shared_ptr<EndPointToGroupRSrc> > ctx =
                        l2e->resolveEpdrEndPointToGroupRSrc();
                    if (ctx)
                        ctx.get()->remove();
                }

                const optional<string>& epgMapping =
                    es.endpoint->getEgMappingAlias();
                if (epgMapping) {
                    l2e->addGbpeEpgMappingCtx()
                        ->addGbpeEpgMappingCtxToEpgMappingRSrc()
                        ->setTargetEpgMapping(epgMapping.get());
                    l2e->addGbpeEpgMappingCtx()
                        ->addGbpeEpgMappingCtxToAttrSetRSrc()
                        ->setTargetEpAttributeSet(uuid);
                } else {
                    optional<shared_ptr<EpgMappingCtx> > ctx =
                        l2e->resolveGbpeEpgMappingCtx();
                    if (ctx)
                        ctx.get()->remove();
                }
            }
            newlocall2eps.insert(l2e->getURI());

            if (policyManager.useLocalNetpol()) {
                vector<shared_ptr<EndPointToLocalSecGroupRSrc> > oldSecGrps;
                l2e->resolveEpdrEndPointToLocalSecGroupRSrc(oldSecGrps);
                const set<URI>& secGrps = es.endpoint->getSecurityGroups();
                for (const shared_ptr<EndPointToLocalSecGroupRSrc>& og :
                         oldSecGrps) {
                    optional<URI> targ = og->getTargetURI();
                    if (!targ || secGrps.find(targ.get()) == secGrps.end())
                        og->remove();
                }
                for (const URI& sg : secGrps) {
                    l2e->addEpdrEndPointToLocalSecGroupRSrc(sg.toString());
                }
            } else {
                vector<shared_ptr<EndPointToSecGroupRSrc> > oldSecGrps;
                l2e->resolveEpdrEndPointToSecGroupRSrc(oldSecGrps);
                const set<URI>& secGrps = es.endpoint->getSecurityGroups();
                for (const shared_ptr<EndPointToSecGroupRSrc>& og :
                         oldSecGrps) {
                    optional<URI> targ = og->getTargetURI();
                    if (!targ || secGrps.find(targ.get()) == secGrps.end())
                        og->remove();
                }
                for (const URI& sg : secGrps) {
                    l2e->addEpdrEndPointToSecGroupRSrc(sg.toString());
                }
            }

            const optional<URI>& qosPol =
                    es.endpoint->getQosPolicy();
            if (qosPol) {
                l2e->addEpdrEndPointToQosRSrc()
                   ->setTargetRequirement(qosPol.get());
            } else {
                optional<shared_ptr<EndPointToQosRSrc>> qosRel =
                    l2e->resolveEpdrEndPointToQosRSrc();
                if (qosRel) {
                    qosRel.get()->remove();
                }
            }

            // Update LocalL2 objects in the MODB corresponding to
            // floating IP endpoints
            for (const Endpoint::IPAddressMapping& ipm :
                     es.endpoint->getIPAddressMappings()) {
                if (!ipm.getMappedIP() || !ipm.getEgURI())
                    continue;

                newipmgroups.insert(ipm.getEgURI().get());

                shared_ptr<LocalL2Ep> fl2e = l2d.get()
                    ->addEpdrLocalL2Ep(ipm.getUUID());
                fl2e->setMac(mac.get());
                fl2e->addEpdrEndPointToGroupRSrc()
                    ->setTargetEpGroup(ipm.getEgURI().get());
                newlocall2eps.insert(fl2e->getURI());
            }
        }

        // Update LocalL3 objects in the MODB, which, though it won't
        // cause any improved functionality, may cause overall
        // happiness in the universe to increase.
        optional<shared_ptr<L3Discovered> > l3d =
            L3Discovered::resolve(framework);
        if (l3d) {
            for (const string& ip : es.endpoint->getIPs()) {
                if (!validateIp(ip)) continue;
                shared_ptr<LocalL3Ep> l3e = l3d.get()
                    ->addEpdrLocalL3Ep(ip);
                l3e->setMac(mac.get());
                newlocall3eps.insert(l3e->getURI());
            }
        }
    }

    for (const string& ip : es.endpoint->getIPs()) {
        if (!validateIp(ip))
            continue;
        ip_local_ep_map[ip] = es.endpoint;
    }

    // remove any stale local EPs
    for (const URI& locall2ep : es.locall2EPs) {
        if (newlocall2eps.find(locall2ep) == newlocall2eps.end()) {
            LocalL2Ep::remove(framework, locall2ep);
        }
    }
    es.locall2EPs = std::move(newlocall2eps);
    for (const URI& locall3ep : es.locall3EPs) {
        if (newlocall3eps.find(locall3ep) == newlocall3eps.end()) {
            LocalL3Ep::remove(framework, locall3ep);
        }
    }
    es.locall3EPs = std::move(newlocall3eps);

    // Update IP address mapping group map
    for (const URI& ipmGrp : newipmgroups) {
        ipm_group_ep_map[ipmGrp].insert(uuid);
    }
    for (const URI& ipmGrp : es.ipMappingGroups) {
        if (newipmgroups.find(ipmGrp) == newipmgroups.end()) {
            unordered_set<string>& eps = ipm_group_ep_map[ipmGrp];
            eps.erase(uuid);
            if (eps.empty())
                ipm_group_ep_map.erase(ipmGrp);
        }
    }
    es.ipMappingGroups = std::move(newipmgroups);

    mutator.commit();

    if(es.endpoint->isExternal()) {
       return updated;
    }

    updated |= updateEndpointReg(uuid);

    return updated;
}

/* Form the URI of secGroup from the given security group.
 * This is needed since we want to generate URI without the EPR prefixes of the EP */
static URI formSecGroupURI (SecurityGroupContext& sgc) {
    optional<const string&> sgOpt = sgc.getSecGroup();
    if (!sgOpt)
        return URIBuilder().build();
    const auto& sg = sgOpt.get();
    size_t spaceStart = sg.find("PolicySpace") + 12;
    size_t gsgStart = sg.rfind("GbpSecGroup");
    size_t nameStart = gsgStart + 12;
    return URIBuilder()
               .addElement("PolicyUniverse")
               .addElement("PolicySpace")
               .addElement(sg.substr(spaceStart, gsgStart-spaceStart-1))
               .addElement("GbpSecGroup")
               .addElement(sg.substr(nameStart, sg.size()-nameStart-1))
               .build();
}

/* Form the URI of local secGroup from the given security group.
 * This is needed since we want to generate URI without the EPR prefixes of the EP */
static URI formLocalSecGroupURI (SecurityGroupContext& sgc) {
    optional<const string&> sgOpt = sgc.getSecGroup();
    if (!sgOpt)
        return URIBuilder().build();
    const auto& sg = sgOpt.get();
    size_t spaceStart = sg.find("PolicySpace") + 12;
    size_t gsgStart = sg.rfind("GbpLocalSecGroup");
    size_t nameStart = gsgStart + 17;
    return URIBuilder()
               .addElement("PolicyUniverse")
               .addElement("PolicySpace")
               .addElement(sg.substr(spaceStart, gsgStart-spaceStart-1))
               .addElement("GbpLocalSecGroup")
               .addElement(sg.substr(nameStart, sg.size()-nameStart-1))
               .build();
}

static shared_ptr<modelgbp::epr::L2Ep>
populateL2E(shared_ptr<modelgbp::epr::L2Universe>& l2u,
            shared_ptr<const Endpoint>& ep,
            const string& uuid,
            shared_ptr<modelgbp::gbp::BridgeDomain>& bd,
            const URI& egURI,
            const set<URI>& secGroups,
            bool localNetpolEnabled) {
    using namespace modelgbp::gbp;
    using namespace modelgbp::gbpe;
    using namespace modelgbp::epr;

    shared_ptr<L2Ep> l2e =
        l2u->addEprL2Ep(bd->getURI().toString(),
                        ep->getMAC().get());
    l2e->setUuid(uuid);
    l2e->setGroup(egURI.toString());

    // Free up deleted security group context
    vector<shared_ptr<SecurityGroupContext> > outSGC;
    l2e->resolveEprSecurityGroupContext(outSGC);
    for (auto &sgc : outSGC) {
        auto sgURI = localNetpolEnabled ? formLocalSecGroupURI(*sgc)
                                        : formSecGroupURI(*sgc);
        if (secGroups.find(sgURI) == secGroups.end())
            sgc->remove();
    }
    for (const URI& secGroup : secGroups) {
        l2e->addEprSecurityGroupContext(secGroup.toString());
    }

    if (ep->getAccessInterface())
        l2e->setInterfaceName(ep->getAccessInterface().get());
    else if (ep->getInterfaceName())
        l2e->setInterfaceName(ep->getInterfaceName().get());

    const Endpoint::attr_map_t& attr_map = ep->getAttributes();
    // Free up deleted attributes
    shared_ptr<ReportedEpAttributeSet> epas =
        l2e->addGbpeReportedEpAttributeSet();
    if (epas) {
        vector<shared_ptr<ReportedEpAttribute> > outEPA;
        epas->resolveGbpeReportedEpAttribute(outEPA);
        for (auto &epa : outEPA) {
            auto name = epa->getName();
            if (name)
                if (attr_map.find(name.get()) == attr_map.end())
                    epa->remove();
        }
        for (const pair<const string, string>& ap : attr_map) {
            shared_ptr<ReportedEpAttribute> epa =
                epas->addGbpeReportedEpAttribute(ap.first);
            epa->setName(ap.first);
            epa->setValue(ap.second);
            if (VM_NAME_ATTR == ap.first)
                l2e->setVmName(ap.second);
        }
    }

    return l2e;
}
static shared_ptr<modelgbp::epr::L3Ep>
populateL3E(shared_ptr<modelgbp::epr::L3Universe>& l3u,
            shared_ptr<const Endpoint>& ep,
            const string& uuid,
            shared_ptr<modelgbp::gbp::RoutingDomain>& rd,
            const string& ip,
            const URI& egURI,
            const set<URI>& secGroups,
            bool localNetpolEnabled) {
    using namespace modelgbp::gbp;
    using namespace modelgbp::epr;

    shared_ptr<L3Ep> l3e =
        l3u->addEprL3Ep(rd->getURI().toString(), ip);
    l3e->setMac(ep->getMAC().get())
        .setGroup(egURI.toString())
        .setUuid(uuid);

    // Free up deleted security group context
    vector<shared_ptr<SecurityGroupContext> > outSGC;
    l3e->resolveEprSecurityGroupContext(outSGC);
    for (auto &sgc : outSGC) {
        auto sgURI = localNetpolEnabled ? formLocalSecGroupURI(*sgc)
                                        : formSecGroupURI(*sgc);
        if (secGroups.find(sgURI) == secGroups.end())
            sgc->remove();
    }
    for (const URI& secGroup : secGroups) {
        l3e->addEprSecurityGroupContext(secGroup.toString());
    }

    return l3e;
}

bool EndpointManager::updateEndpointReg(const string& uuid) {
    using namespace modelgbp::gbp;
    using namespace modelgbp::epr;

    auto it = ep_map.find(uuid);
    if (it == ep_map.end()) return false;

    EndpointState& es = it->second;
    const optional<URI>& egURI = es.egURI;
    const optional<MAC>& mac = es.endpoint->getMAC();
    const set<URI>& secGroups =
        es.endpoint->getSecurityGroups();
    unordered_set<URI> newl3eps;
    unordered_set<URI> newl2eps;
    optional<shared_ptr<RoutingDomain> > rd;
    optional<shared_ptr<BridgeDomain> > bd;

    if (egURI) {
        // check whether the l2 and l3 routing contexts are already
        // resolved.
        rd = policyManager.getRDForGroup(egURI.get());
        bd = policyManager.getBDForGroup(egURI.get());
    }

    Mutator mutator(framework, "policyelement");

    optional<shared_ptr<L2Universe> > l2u =
        L2Universe::resolve(framework);
    if (l2u && bd && mac && (NULL_MAC_ADDR != mac.get().toString())) {
        // If the bridge domain is known, we can register the l2
        // endpoint
        {
            shared_ptr<L2Ep> l2e =
                populateL2E(l2u.get(), es.endpoint, uuid,
                            bd.get(), egURI.get(), secGroups,
                            policyManager.useLocalNetpol());

            newl2eps.insert(l2e->getURI());
        }

        for (const Endpoint::IPAddressMapping& ipm :
                 es.endpoint->getIPAddressMappings()) {
            if (!ipm.getFloatingIP() || !ipm.getMappedIP() || !ipm.getEgURI())
                continue;
            // don't declare endpoints if there's a next hop
            if (ipm.getNextHopIf())
                continue;

            optional<shared_ptr<BridgeDomain> > fbd =
                policyManager.getBDForGroup(ipm.getEgURI().get());
            if (!fbd) continue;

            shared_ptr<L2Ep> fl2e =
                populateL2E(l2u.get(), es.endpoint, ipm.getUUID(), fbd.get(),
                            ipm.getEgURI().get(),
                            secGroups,
                            policyManager.useLocalNetpol());
            newl2eps.insert(fl2e->getURI());
        }
    }

    optional<shared_ptr<L3Universe> > l3u =
        L3Universe::resolve(framework);
    if (l3u && bd && rd && mac) {
        uint8_t routingMode =
            policyManager.getEffectiveRoutingMode(egURI.get());

        if (routingMode == RoutingModeEnumT::CONST_ENABLED) {
            // If the routing domain is known, we can register the l3
            // endpoints in the endpoint registry
            for (const string& ip : es.endpoint->getIPs()) {
                if (!validateIp(ip)) continue;
                shared_ptr<L3Ep> l3e =
                    populateL3E(l3u.get(), es.endpoint, uuid,
                                rd.get(), ip, egURI.get(),
                                secGroups,
                                policyManager.useLocalNetpol());
                newl3eps.insert(l3e->getURI());
            }

            for (const Endpoint::IPAddressMapping& ipm :
                     es.endpoint->getIPAddressMappings()) {
                if (!ipm.getFloatingIP() || !ipm.getMappedIP() ||
                    !ipm.getEgURI())
                    continue;
                // don't declare endpoints if there's a next hop
                if (ipm.getNextHopIf())
                    continue;

                optional<shared_ptr<RoutingDomain> > frd =
                    policyManager.getRDForGroup(ipm.getEgURI().get());
                if (!frd) continue;

                shared_ptr<L3Ep> fl3e =
                    populateL3E(l3u.get(), es.endpoint, ipm.getUUID(),
                                frd.get(), ipm.getFloatingIP().get(),
                                ipm.getEgURI().get(),
                                secGroups,
                                policyManager.useLocalNetpol());
                newl3eps.insert(fl3e->getURI());
            }
        }
    }

    // remove any stale endpoint registry objects
    for (const URI& l2ep : es.l2EPs) {
        if (newl2eps.find(l2ep) == newl2eps.end()) {
            L2Ep::remove(framework, l2ep);
        }
    }
    es.l2EPs = std::move(newl2eps);
    for (const URI& l3ep : es.l3EPs) {
        if (newl3eps.find(l3ep) == newl3eps.end()) {
            L3Ep::remove(framework, l3ep);
        }
    }
    es.l3EPs = std::move(newl3eps);

    mutator.commit();
    return true;
}

void EndpointManager::egDomainUpdated(const URI& egURI) {
    unordered_set<string> notify;
    unordered_set<string> remoteNotify;
    unique_lock<mutex> guard(ep_mutex);

    group_ep_map_t::const_iterator it = group_ep_map.find(egURI);
    if (it != group_ep_map.end()) {
        for (const string& uuid : it->second) {
            if (updateEndpointReg(uuid))
                notify.insert(uuid);
        }
    }

    group_ep_map_t::const_iterator rit = group_remote_ep_map.find(egURI);
    if (rit != group_remote_ep_map.end()) {
        for (const string& uuid : rit->second) {
            remoteNotify.insert(uuid);
        }
    }

    it = ipm_group_ep_map.find(egURI);
    if (it != ipm_group_ep_map.end()) {
        for (const string& uuid : it->second) {
            if (updateEndpointReg(uuid))
                notify.insert(uuid);
        }
    }
    guard.unlock();

    for (const string& uuid : notify) {
        notifyListeners(uuid);
    }
    for (const string& uuid : remoteNotify) {
        notifyRemoteListeners(uuid);
    }
}

void EndpointManager::externalInterfaceUpdated(const URI& extIntURI) {
    using namespace modelgbp::gbp;
    unique_lock<mutex> guard(ep_mutex);
    group_ep_map_t::const_iterator gep_it = group_ep_map.find(extIntURI);
    optional<shared_ptr<RoutingDomain>> rd;
    unordered_set<string> notify;
    rd = policyManager.getRDForExternalInterface(extIntURI);
    if(!rd)
        return;
    ipmac_map_t &ip_mac_map = adj_ep_map[rd.get()->getURI()];
    if (gep_it == group_ep_map.end()) {
        return;
    }
    for (const string& uuid : gep_it->second) {
        auto eep_it = ext_ep_map.find(uuid);
        if (eep_it != ext_ep_map.end()) {
            notify.insert(uuid);
            for(const string &addr : eep_it->second.endpoint->getIPs()) {
                if (!validateIp(addr)) continue;
                ip_mac_map[addr] = eep_it->second.endpoint;
            }
        }
    }
    guard.unlock();
    for (const string& uuid : notify) {
        notifyExternalEndpointListeners(uuid);
    }

}

bool EndpointManager::getAdjacency(const URI& rdURI,
                                   const string& address,
                                   shared_ptr<const Endpoint> &ep) {
    unique_lock<mutex> guard(ep_mutex);
    auto aep_it = adj_ep_map.find(rdURI);
    if(aep_it == adj_ep_map.end())
        return false;
    auto ipm_it = aep_it->second.find(address);
    if(ipm_it == aep_it->second.end())
        return false;
    ep = ipm_it->second;
    return true;
}

template <typename K, typename M>
static void getEps(const K& key, const M& map,
                   /* out */ unordered_set<string>& eps) {
    auto it = map.find(key);
    if (it != map.end()) {
        eps.insert(it->second.begin(), it->second.end());
    }
}

template <typename M>
static void getEps(const M& map, /* out */ unordered_set<string>& eps) {
    for (const auto& elem : map) {
        eps.insert(elem.second.begin(), elem.second.end());
    }
}

void EndpointManager::getEndpointsForGroup(const URI& egURI,
                                           /*out*/ unordered_set<string>& eps) {
    unique_lock<mutex> guard(ep_mutex);
    getEps(egURI, group_ep_map, eps);
}

bool EndpointManager::secGrpSetEmpty(const uri_set_t& secGrps) {
    unique_lock<mutex> guard(ep_mutex);
    return secgrp_ep_map.find(secGrps) == secgrp_ep_map.end();
}

void EndpointManager::
getSecGrpSetsForSecGrp(const URI& secGrp,
                       /* out */ unordered_set<uri_set_t>& result) {
    unique_lock<mutex> guard(ep_mutex);
    for (const secgrp_ep_map_t::value_type& v : secgrp_ep_map) {
        if (v.first.find(secGrp) != v.first.end())
            result.insert(v.first);
    }
}

void EndpointManager::getEndpointsForIPMGroup(const URI& egURI,
                                              unordered_set<string>& eps) {
    unique_lock<mutex> guard(ep_mutex);
    getEps(egURI, ipm_group_ep_map, eps);
}

void EndpointManager::getEndpointsByIface(const string& ifaceName,
                                          /* out */ str_uset_t& eps) {
    unique_lock<mutex> guard(ep_mutex);
    getEps(ifaceName, iface_ep_map, eps);
}

shared_ptr<const Endpoint> EndpointManager::getEpFromLocalMap (const string& ip) {
    unique_lock<mutex> guard(ep_mutex);
    const auto& itr = ip_local_ep_map.find(ip);
    if (itr != ip_local_ep_map.end()) {
        return itr->second;
    }
    return nullptr;
}

void EndpointManager::getEndpointUUIDs( /* out */ str_uset_t& eps) {
    unique_lock<mutex> guard(ep_mutex);
    getEps(iface_ep_map, eps);
}

void EndpointManager::getEndpointsByAccessIface(const string& ifaceName,
                                                /* out */ str_uset_t& eps) {
    unique_lock<mutex> guard(ep_mutex);
    getEps(ifaceName, access_iface_ep_map, eps);
}

void EndpointManager::getEndpointsByAccessUplink(const string& ifaceName,
                                                 /* out */ str_uset_t& eps) {
    unique_lock<mutex> guard(ep_mutex);
    getEps(ifaceName, access_uplink_ep_map, eps);
}

void EndpointManager::getEndpointsByIpmNextHopIf(const string& ifaceName,
                                                 /* out */ str_uset_t& eps) {
    unique_lock<mutex> guard(ep_mutex);
    getEps(ifaceName, ipm_nexthop_if_ep_map, eps);
}

void EndpointManager::getLocalExternalDomains(unordered_set<URI>& domain) {
    unique_lock<mutex> guard(ep_mutex);
    for(auto &local_ext_dom: local_ext_dom_map) {
        domain.insert(local_ext_dom.first);
    }
}

bool EndpointManager::localExternalDomainExists(const URI& epgURI) {
    unique_lock<mutex> guard(ep_mutex);
    auto it = local_ext_dom_map.find(epgURI);
    return (it != local_ext_dom_map.end());
}

uint32_t EndpointManager::getExtEncapId(const URI& epgURI) {
    unique_lock<mutex> guard(ep_mutex);
    auto it = local_ext_dom_map.find(epgURI);
    if(it != local_ext_dom_map.end()) {
        return it->second;
    }
    return 0;
}

void EndpointManager::updateEndpointCounters(const string& uuid,
                                             EpCounters& newVals) {
    using namespace modelgbp::gbpe;
    using namespace modelgbp::observer;

    Mutator mutator(framework, "policyelement");
    optional<shared_ptr<EpStatUniverse> > su =
        EpStatUniverse::resolve(framework);
    if (su) {
        su.get()->addGbpeEpCounter(uuid)
            ->setRxPackets(newVals.rxPackets)
            .setTxPackets(newVals.txPackets)
            .setRxDrop(newVals.rxDrop)
            .setTxDrop(newVals.txDrop)
            .setRxBroadcast(newVals.rxBroadcast)
            .setTxBroadcast(newVals.txBroadcast)
            .setRxMulticast(newVals.rxMulticast)
            .setTxMulticast(newVals.txMulticast)
            .setRxUnicast(newVals.rxUnicast)
            .setTxUnicast(newVals.txUnicast)
            .setRxBytes(newVals.rxBytes)
            .setTxBytes(newVals.txBytes);
    }

    mutator.commit();
    lock_guard<mutex> guard(ep_mutex);
    auto it = ep_map.find(uuid);
    if (it != ep_map.end()) {
        EndpointState& es = it->second;
        auto& ep_name = es.endpoint->getAccessInterface();
        if (ep_name)
            prometheusManager.addNUpdateEpCounter(uuid, ep_name.get(),
                                                  es.endpoint->isAnnotateEpName(),
                                                  es.endpoint->getAttributeHash(),
                                                  es.endpoint->getAttributes(),
                                                  newVals);
        else
            LOG(ERROR) << "ep name not found for uuid:" << uuid;
    }
}

EndpointManager::EPGMappingListener::EPGMappingListener(EndpointManager& epmgr_)
    : epmanager(epmgr_) {}

EndpointManager::EPGMappingListener::~EPGMappingListener() {}

void EndpointManager::EPGMappingListener::objectUpdated(class_id_t classId,
                                                        const URI& uri) {
    using namespace modelgbp::gbpe;

    if (classId == EpAttributeSet::CLASS_ID) {
        unique_lock<mutex> guard(epmanager.ep_mutex);
        optional<shared_ptr<EpAttributeSet> > attrSet =
            EpAttributeSet::resolve(epmanager.framework, uri);
        if (!attrSet) return;

        optional<const string&> uuid = attrSet.get()->getUuid();
        if (!uuid) return;

        auto it = epmanager.ep_map.find(uuid.get());
        if (it == epmanager.ep_map.end()) return;

        EndpointState& es = it->second;
        es.epAttrs.clear();

        vector<shared_ptr<EpAttribute> > attrs;
        attrSet.get()->resolveGbpeEpAttribute(attrs);
        for (shared_ptr<EpAttribute>& attr : attrs) {
            optional<const string&> name = attr->getName();
            optional<const string&> value = attr->getValue();

            if (!name) continue;
            if (value)
                es.epAttrs[name.get()] = value.get();
            else
                es.epAttrs[name.get()] = "";
        }

        if (epmanager.updateEndpointLocal(uuid.get())) {
            guard.unlock();
            epmanager.notifyListeners(uuid.get());
        }
    } else if (classId == EpgMapping::CLASS_ID) {
        unique_lock<mutex> guard(epmanager.ep_mutex);
        optional<shared_ptr<EpgMapping> > epgMapping =
            EpgMapping::resolve(epmanager.framework, uri);
        if (!epgMapping) return;

        optional<const string&> name = epgMapping.get()->getName();
        if (!name) return;

        auto it = epmanager.epgmapping_ep_map.find(name.get());
        if (it == epmanager.epgmapping_ep_map.end()) return;

        unordered_set<string> notify;
        for (const string& uuid : it->second) {
            if (epmanager.updateEndpointLocal(uuid)) {
                notify.insert(uuid);
            }
        }

        guard.unlock();
        for (const string& uuid : notify) {
            epmanager.notifyListeners(uuid);
        }
    } else if (classId == modelgbp::inv::RemoteInventoryEp::CLASS_ID) {
        epmanager.updateEndpointRemote(uri);
    } else if (classId == modelgbp::inv::RemoteIp::CLASS_ID) {
        boost::filesystem::path puri(uri.toString());
        puri = puri.parent_path()
                   .parent_path()
                   .parent_path();
        URI curi(puri.string() + "/");
        epmanager.updateEndpointRemote(curi);
   }
}

void EndpointManager::configUpdated(const URI& uri) {
    using namespace modelgbp::platform;
    bool setRemoteEndpoint = false;
    Mutator mutator(framework, "init");
    optional<shared_ptr<modelgbp::domain::Config> >
        configD(modelgbp::domain::Config::resolve(framework));
    if (!configD) {
       LOG(WARNING) << "Domain config not available";
       return;
    }

    optional<shared_ptr<Config>> config = Config::resolve(framework, uri);
    if (config) {
        optional<const uint8_t> invType = config.get()->getInventoryType();
        if (invType) {
            auto on_link = RemoteInventoryTypeEnumT::CONST_ON_LINK;
            if (invType.get() == on_link) {
                LOG(DEBUG) << "setting remote endpoint discovery";
                configD.get()->addDomainConfigToRemoteEndpointInventoryRSrc()
                             ->setTargetRemoteEndpointInventory();
                setRemoteEndpoint = true;
            }
        }
    }
    if (!setRemoteEndpoint) {
        optional<shared_ptr<modelgbp::domain::ConfigToRemoteEndpointInventoryRSrc>> reInv =
                configD.get()->resolveDomainConfigToRemoteEndpointInventoryRSrc();
        if (reInv) {
            LOG(DEBUG) << "removing remote endpoint discovery";
            reInv.get()->remove();
        }
    }
    mutator.commit();

    if (!config) {
        LOG(WARNING) << "Platform config has been deleted. Disconnect from existing peers and fallback to configured list";
        agent.updateResetTime();
        framework.resetAllUnconfiguredPeers();
    }
}
} /* namespace opflexagent */
