/*
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


#include <yajr/rpc/methods.hpp>

namespace yajr {

namespace comms {
    namespace internal {
        class CommunicationPeer;
    }
}

namespace rpc {

yajr::rpc::InboundMessage *
MessageFactory::InboundMessage(
        yajr::Peer& peer,
        rapidjson::Document const & doc) {

    /* IDs are tricky because they can be any JSON entity.
     * We need to preserve them as such.
     *
     * Robustness Principle: "Be liberal in what you accept,
     * and conservative in what you send".
     */
    if (!doc.IsObject()) {
        LOG(ERROR) << &peer << " Received frame that is not a JSON object.";
        goto error;
    }

    /* we don't accept any notifications */
    if (!doc.HasMember("id")) {
        LOG(ERROR) << &peer << " Received frame without id.";
        goto error;
    }

    {
        if (doc.HasMember("method")) {
            const rapidjson::Value& methodValue = doc["method"];

            if (!methodValue.IsString()) {
                LOG(ERROR) << &peer << " Received request with non-string method. Dropping";
                goto error;
            }

            const char * method = methodValue.GetString();

            /* Once again, be liberal with input... On a case by case
             * basis, the handler will respond with an error if it is
             * unacceptable to have no parameters for the invoked method.
             */
            const rapidjson::Value& params = doc[Message::kPayloadKey.params];
            return MessageFactory::InboundRequest(peer, params, method, doc["id"]);
        }

        /* id must be an array for replies, and its first element must be a string */
        if (doc["id"].IsNull()) {
            LOG(ERROR) << &peer << " Received frame with null id.";
            goto error;
        }
        const rapidjson::Value& id = doc["id"];
        if (!id.IsArray() || !id[rapidjson::SizeType(0)].IsString()) {
            LOG(ERROR) << &peer << " Received frame with an id that is not an array of strings.";
            goto error;
        }

        if (doc.HasMember(Message::kPayloadKey.result)) {
            const rapidjson::Value& result = doc[Message::kPayloadKey.result];
            // OVSDB returns an error key inside the result instead of using the top-level error
            if (result.IsArray() && result.Size() > 0 && result[0].IsObject() &&
                result[0].HasMember("error")) {
                return MessageFactory::InboundError(peer, result[0], id);
            } else {
                return MessageFactory::InboundResult(peer, result, id);
            }
        }

        if (doc.HasMember("error")) {
            const rapidjson::Value& error = doc["error"];
            if (!error.IsObject()) {
                LOG(ERROR) << &peer << " Received error frame with an error that is not an object.";
                goto error;
            }

            return MessageFactory::InboundError(peer, error, id);
        }
    }
error:
    LOG(ERROR) << &peer << " Dropping client because of protocol error.";

    auto commPeer = dynamic_cast< ::yajr::comms::internal::CommunicationPeer const *>(&peer);
    if (!commPeer) {
        LOG(ERROR) << "Unable to convert to CommunicationPeer";
        assert(false);
        return NULL;
    }
    commPeer->onError(UV_EPROTO);
    const_cast<::yajr::comms::internal::CommunicationPeer *>(commPeer)
        ->onDisconnect();

    return NULL;
}

yajr::rpc::InboundMessage *
MessageFactory::getInboundMessage(
        yajr::Peer& peer,
        rapidjson::Document const & doc
        ) {

    yajr::rpc::InboundMessage * msg =
        MessageFactory::InboundMessage(peer, doc);

    return msg;

}

}
}

