/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Implementation for InspectorClientImpl class.
 *
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

/* This must be included before anything else */
#if HAVE_CONFIG_H
#  include <config.h>
#endif

#include <boost/optional.hpp>

#include "opflex/modb/internal/ObjectStore.h"
#include "opflex/engine/internal/InspectorClientHandler.h"
#include "opflex/engine/internal/OpflexConnection.h"
#include "opflex/engine/internal/OpflexMessage.h"
#include "opflex/engine/InspectorClientImpl.h"

namespace opflex {
namespace ofcore {

InspectorClient*
InspectorClient::newInstance(const std::string& name,
                             const modb::ModelMetadata& model,
                             long timeout) {
    return new engine::InspectorClientImpl(name, model, timeout);
}

}

namespace engine {

using std::string;
using std::unique_ptr;
using boost::optional;
using opflex::modb::ObjectStore;
using opflex::modb::PropertyInfo;
using opflex::modb::ClassInfo;
using opflex::modb::URI;
using modb::mointernal::ObjectInstance;
using modb::mointernal::StoreClient;
using internal::OpflexHandler;
using internal::InspectorClientHandler;
using internal::OpflexConnection;
using internal::OpflexMessage;

InspectorClientImpl::InspectorClientImpl(const std::string& name_,
                                         const modb::ModelMetadata& model,
                                         long timeout)
    : conn(*this, name_, timeout), db(threadManager),
      serializer(&db, this), pendingRequests(0),
      followRefs(false), recursive(false), unresolved(false),
      excludeObservables(false) {
    db.init(model);
    storeClient = &db.getStoreClient("_SYSTEM_");
}

class Cmd {
public:
    Cmd() {}
    virtual ~Cmd() {}

    virtual int execute(InspectorClientImpl& client) = 0;
};

InspectorClientImpl::~InspectorClientImpl() {
    for (const Cmd* command : commands) {
        delete command;
    }
}

OpflexHandler* InspectorClientImpl::newHandler(OpflexConnection* conn) {
    return new InspectorClientHandler(conn, this);
}

void InspectorClientImpl::execute() {
    db.start();
    conn.connect();
    db.stop();
}

void InspectorClientImpl::executeCommands() {
    while (!commands.empty()) {
        unique_ptr<Cmd> command(commands.front());
        commands.pop_front();
        pendingRequests += command->execute(*this);
    }
}

class Query : public Cmd {
public:
    Query(const string& subject_,
          optional<URI> uri_,
          bool recursive_ = true)
        : subject(subject_), uri(std::move(uri_)), recursive(recursive_) { }
    virtual ~Query() {}

    virtual int execute(InspectorClientImpl& client);

    string subject;
    optional<URI> uri;
    bool recursive;
};

class InspectorMessage : public OpflexMessage {
public:
    InspectorMessage(const std::string& method, MessageType type,
                     InspectorClientImpl& client_)
        :OpflexMessage(method, type), client(client_) {};
    virtual ~InspectorMessage() {};

    InspectorClientImpl& client;
};

class PolicyQueryReq : public InspectorMessage {
public:
    PolicyQueryReq(InspectorClientImpl& client,
                   const Query& query_)
        : InspectorMessage("custom", REQUEST, client),
          query(query_) {}

    virtual void serializePayload(yajr::rpc::SendHandler& writer) const {
        (*this)(writer);
    }

    virtual PolicyQueryReq* clone() {
        return new PolicyQueryReq(*this);
    }

    virtual bool operator()(yajr::rpc::SendHandler& writer) const {
        writer.StartArray();
        writer.StartObject();
        writer.String("method");
        writer.String("org.opendaylight.opflex.policy_query");
        writer.String("params");
        writer.StartArray();
        writer.StartObject();
        writer.String("subject");
        writer.String(query.subject.c_str());
        if (query.uri) {
            writer.String("policy_uri");
            writer.String(query.uri.get().toString().c_str());
        }
        writer.String("recursive");
        writer.Bool(query.recursive);
        writer.EndObject();
        writer.EndArray();
        writer.EndObject();
        writer.EndArray();
        return true;
    }

    const Query& query;
};

int Query::execute(InspectorClientImpl& client) {
    PolicyQueryReq* r = new PolicyQueryReq(client, *this);
    client.getConn().sendMessage(r, true);
    return 1;
}

void InspectorClientImpl::addQuery(const string& subject,
                                   const URI& uri) {
    commands.push_back(new Query(subject, optional<URI>(uri), recursive));
}

void InspectorClientImpl::addClassQuery(const string& subject) {
    commands.push_back(new Query(subject, boost::none, recursive));
}

void InspectorClientImpl::dumpToFile(FILE* file) {
    if (unresolved) {
        serializer.dumpUnResolvedMODB(file);
    } else {
        serializer.dumpMODB(file, excludeObservables);
    }
}

size_t InspectorClientImpl::loadFromFile(FILE* file) {
    return serializer.readMOs(file, *storeClient);
}

void InspectorClientImpl::prettyPrint(std::ostream& output,
                                      bool tree,
                                      bool includeProps,
                                      bool utf8,
                                      size_t truncate) {
    if (unresolved) {
        serializer.displayUnresolved(output, tree, utf8);
    } else {
        serializer.displayMODB(output, tree, includeProps, utf8, truncate, excludeObservables);
    }
}

void InspectorClientImpl::setFollowRefs(bool enabled) {
    followRefs = enabled;
}

void InspectorClientImpl::setRecursive(bool enabled) {
    recursive = enabled;
}

void InspectorClientImpl::setUnresolved(bool enabled) {
    unresolved = enabled;
    followRefs = enabled;
}

void InspectorClientImpl::setExcludeObservables(bool enabled) {
    excludeObservables = enabled;
}

static std::string getRefSubj(const modb::ObjectStore& store,
                              const modb::reference_t& ref) {
    try {
        const modb::ClassInfo& ref_class =
            store.getClassInfo(ref.first);
        return ref_class.getName();
    } catch (const std::out_of_range& e) {
        return "UNKNOWN: " + std::to_string(ref.first);
    }
}

void InspectorClientImpl::remoteObjectUpdated(modb::class_id_t class_id,
                                              const modb::URI& uri,
                                              gbp::PolicyUpdateOp op) {
    if (!followRefs) return;

    try {
        StoreClient& client = db.getReadOnlyStoreClient();
        std::shared_ptr<const ObjectInstance> oi = client.get(class_id, uri);
        const ClassInfo& ci = db.getClassInfo(class_id);
        for (const ClassInfo::property_map_t::value_type& p : ci.getProperties()) {
            if (p.second.getType() == PropertyInfo::REFERENCE) {
                if (p.second.getCardinality() == PropertyInfo::SCALAR) {
                    if (oi->isSet(p.first,
                                  PropertyInfo::REFERENCE,
                                  PropertyInfo::SCALAR)) {
                        modb::reference_t ref = oi->getReference(p.first);
                        addQuery(getRefSubj(db, ref), ref.second);
                    }
                } else {
                    size_t c = oi->getReferenceSize(p.first);
                    for (size_t i = 0; i < c; ++i) {
                        modb::reference_t ref = oi->getReference(p.first, i);
                        addQuery(getRefSubj(db, ref), ref.second);
                    }
                }
            }
        }
        executeCommands();
    } catch (const std::out_of_range& e) {}
}

} /* namespace engine */
} /* namespace opflex */
