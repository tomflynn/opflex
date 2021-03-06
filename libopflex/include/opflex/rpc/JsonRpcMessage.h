/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*!
 * @file JsonRpcMessage.h
 * @brief Interface definition for various JSON/RPC messages used by the
 * engine
 */
/*
 * Copyright (c) 2020 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#pragma once
#ifndef RPC_JSONRPCMESSAGE_H
#define RPC_JSONRPCMESSAGE_H

#include <rapidjson/document.h>
#include <rapidjson/writer.h>

#include <opflex/yajr/rpc/message_factory.hpp>

namespace opflex {
namespace jsonrpc {

/**
 * Represent an Opflex message and allow serializing to the socket
 */
class JsonRpcMessage  {
public:
    /**
     * The type of the message
     */
    enum MessageType {
        /** A request */
        REQUEST,
        /** a response message */
        RESPONSE,
        /** an error response */
        ERROR_RESPONSE
    };

    /**
     * Construct a new JSON-RPC message
     *
     * @param method_ the method for the message
     * @param type_ the type of message
     * @param id_ if specified, use as the ID of the message.  The
     * memory is owned by the caller, and must be set for responses.
     */
    JsonRpcMessage(const std::string& method_, MessageType type_,
                   const rapidjson::Value* id_ = NULL) : method(method_), type(type_), id(id_) {}

    /**
     * Destroy the message
     */
    virtual ~JsonRpcMessage() {}

    /**
     * Get the request method for this message
     * @return the method for the message
     */
    const std::string& getMethod() const { return method; }

    /**
     * Get the message type for this message
     *
     * @return the type for the message
     */
    MessageType getType() const { return type; }

    /**
     * Get the ID for this message.  Must only be called on a response
     * or error.
     * @return the ID for the message
     */
    const rapidjson::Value& getId() const { return *id; }

    /**
     * Serialize payload
     * @param writer writer
     */
    virtual void serializePayload(yajr::rpc::SendHandler& writer) const = 0;

    /**
     * Operator to serialize a generic empty payload to any writer
     * @param writer the writer to serialize to
     */
    virtual bool operator()(yajr::rpc::SendHandler& writer) const {
        switch (type) {
        case REQUEST:
            writer.StartArray();
            writer.EndArray();
            break;
        default:
            writer.StartObject();
            writer.EndObject();
        }
        return true;
    }

    /**
     * Get a transaction ID for a request.  If nonzero, allocate a
     * transaction ID using a counter
     *
     * @return the transaction ID for the request
     */
    virtual uint64_t getReqXid() const { return 0; }

private:
    /**
     * The request method associated with the message
     */
    std::string method;
    /**
     * The message type of the message
     */
    MessageType type;

    /**
     * The ID associated with the message; may be NULL for a request
     * message, but a response must always set it
     */
    const rapidjson::Value* id;
};

} /* namespace rpc */
} /* namespace opflex */

#endif