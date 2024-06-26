/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Test suite for Processor class.
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

#include <openssl/err.h>
#include <openssl/conf.h>

#include <yajr/transport/ZeroCopyOpenSSL.hpp>
#include <opflex/yajr/internal/comms.hpp>

#include <opflex/logging/StdOutLogHandler.h>
#include <opflex/logging/internal/logging.hpp>

#include <boost/test/unit_test_log.hpp>
#include <boost/test/unit_test.hpp>

#include <utility>

#define DEFAULT_COMMSTEST_TIMEOUT 7200
const uint16_t kPortOffset = 1;

using namespace yajr::comms;
using namespace yajr::transport;

BOOST_AUTO_TEST_SUITE(asynchronous_sockets)

struct CommsTests {

    CommsTests() {
        LOG(INFO) << "global setup\n";

        boost::unit_test::unit_test_log_t::instance()
            .set_threshold_level(::boost::unit_test::log_successful_tests);

        opflex::logging::OFLogHandler::registerHandler(commsTestLogger_);
    }

    ~CommsTests() {
        LOG(INFO) << "global teardown\n";
    }

    /* potentially subject to static initialization order fiasco */
    static opflex::logging::StdOutLogHandler commsTestLogger_;

};

opflex::logging::StdOutLogHandler CommsTests::commsTestLogger_(TRACE);

BOOST_GLOBAL_FIXTURE( CommsTests );

/**
 * A fixture for communications tests
 */

typedef std::pair<size_t, size_t> range_t;

class CommsFixture {
  public:
    static uv_loop_t * current_loop;
    static uv_loop_t * loopSelector(void *);
  private:
    uv_prepare_t prepare_;
    uv_timer_t timer_;
    uv_loop_t loop_;

  protected:
    CommsFixture() {

        CommsFixture::current_loop = &loop_;

        uv_loop_init(CommsFixture::current_loop);
        prepare_ = {};
        timer_ = {};
        prepare_.data = timer_.data = this;

        int rc = ::yajr::initLoop(CommsFixture::current_loop);

        BOOST_CHECK(!rc);

        for (size_t i=0; i < internal::Peer::LoopData::TOTAL_STATES; ++i) {
            BOOST_CHECK_EQUAL(internal::Peer::LoopData::getPeerCount(CommsFixture::current_loop,
                        internal::Peer::LoopData::PeerState(i)), 0u);
        }

        internal::Peer::LoopData::getLoopData(CommsFixture::current_loop)->up();

        ZeroCopyOpenSSL::initOpenSSL(false); // false because we don't use multiple threads

        eventCounter = 0;
    }

    static void down_on_close(uv_handle_t * h) {
        internal::Peer::LoopData::getLoopData(h->loop)->down();
    }

    void cleanup() {

        uv_timer_stop(&timer_);
        uv_prepare_stop(&prepare_);

        uv_close((uv_handle_t *)&timer_, down_on_close);
        uv_close((uv_handle_t *)&prepare_, down_on_close);

        CONF_modules_unload(1);
        ZeroCopyOpenSSL::finiOpenSSL();

        ::yajr::finiLoop(CommsFixture::current_loop);

    }

    ~CommsFixture() {
        uv_loop_close(CommsFixture::current_loop);
    }

    typedef void (*pc)(void);

    static range_t required_final_peers;
    static range_t required_transient_peers;
    static pc required_post_conditions;
    static bool expect_timeout;
  public:
    static size_t eventCounter;
  protected:
    static size_t required_event_counter;

    static std::pair<size_t, size_t> count_peers() {

        static std::string oldDbgLog;
        std::stringstream dbgLog;
        std::string newDbgLog;

        size_t final_peers = 0, transient_peers = 0;

        size_t m;
        if((m=internal::Peer::LoopData::getPeerCount(
                    CommsFixture::current_loop,
                    internal::Peer::LoopData::ONLINE))) {
            final_peers += m;
            dbgLog
                << " online: "
                << m
            ;
        }
        if((m=internal::Peer::LoopData::getPeerCount(
                    CommsFixture::current_loop,
                    internal::Peer::LoopData::LISTENING))) {
            final_peers += m;
            dbgLog
                << " listening: "
                << m
            ;
        }

        if((m=internal::Peer::LoopData::getPeerCount(
                    CommsFixture::current_loop,
                    internal::Peer::LoopData::TO_RESOLVE))) {
            transient_peers += m;
            dbgLog
                << " to_resolve: "
                << m
            ;
        }
        if((m=internal::Peer::LoopData::getPeerCount(
                    CommsFixture::current_loop,
                    internal::Peer::LoopData::TO_LISTEN))) {
            transient_peers += m;
            dbgLog
                << " to_listen: "
                << m
            ;
        }

        if((m=internal::Peer::LoopData::getPeerCount(
                    CommsFixture::current_loop,
                    internal::Peer::LoopData::RETRY_TO_CONNECT))) {
            final_peers += m;
            dbgLog
                << " retry-connecting: "
                << m
            ;
        }
        if((m=internal::Peer::LoopData::getPeerCount(
                    CommsFixture::current_loop,
                    internal::Peer::LoopData::RETRY_TO_LISTEN))) {
            final_peers += m;
            dbgLog
                << " retry-listening: "
                << m
            ;
        }
        if((m=internal::Peer::LoopData::getPeerCount(
                    CommsFixture::current_loop,
                    internal::Peer::LoopData::ATTEMPTING_TO_CONNECT))) {
            /* this is not a "final" state, from a test's perspective */
            transient_peers += m;
            dbgLog
                << " attempting: "
                << m
            ;
        }
        if((m=internal::Peer::LoopData::getPeerCount(
                    CommsFixture::current_loop,
                    internal::Peer::LoopData::PENDING_DELETE))) {
            /* this is not a "final" state, from a test's perspective */
            transient_peers += m;
            dbgLog
                << " pending_delete: "
                << m
            ;
        }

        dbgLog
            << " TOTAL TRANSIENT: "
            << transient_peers
            << " / "
            << required_transient_peers.first
            << "-"
            << required_transient_peers.second
            << "\0"
        ;
        dbgLog
            << " TOTAL FINAL: "
            << final_peers
            << " / "
            << required_final_peers.first
            << "-"
            << required_final_peers.second
            << "\0"
        ;
        dbgLog
            << " EVENTS: "
            << eventCounter
        ;

        newDbgLog = dbgLog.str();

        if (oldDbgLog != newDbgLog) {
            oldDbgLog = newDbgLog;
            LOG(TRACE)  << newDbgLog;
        }

        return std::make_pair(final_peers, transient_peers);
    }

    static void check_peer_db_cb(uv_prepare_t * handle) {

        /* at LEAST # required final peers must be in final state */
        if (
                (eventCounter < required_event_counter)
            ||
                (count_peers().first < required_final_peers.first)
            ||
                (count_peers().first > required_final_peers.second)
            ||
                (count_peers().second < required_transient_peers.first)
            ||
                (count_peers().second > required_transient_peers.second)
            ) {
            return;
        }

        if (required_post_conditions) {
            (*required_post_conditions)();
        }

        reinterpret_cast<CommsFixture*>(handle->data)->cleanup();

    }

    static void timeout_cb(uv_timer_t * handle) {

        if (!expect_timeout) {
            BOOST_CHECK(!"the test has timed out");
        }

        if (required_post_conditions) {
            (*required_post_conditions)();
        }

        reinterpret_cast<CommsFixture*>(handle->data)->cleanup();

    }

    static void destroy_listener_cb(uv_timer_t * handle) {

        reinterpret_cast< ::yajr::Listener * >(handle->data)->destroy();

        uv_timer_stop(handle);
        if (!uv_is_closing((uv_handle_t*)handle)) {
            uv_close((uv_handle_t *)handle, NULL);
        }

    }

    static void destroy_peer_cb(uv_timer_t * handle) {

        reinterpret_cast< ::yajr::Peer* >(handle->data)->destroy();

        uv_timer_stop(handle);
        if (!uv_is_closing((uv_handle_t*)handle)) {
            uv_close((uv_handle_t *)handle, NULL);
        }

    }

    void loop_until_final(
            range_t final_peers,
            pc post_conditions,
            range_t transient_peers = range_t(0, 0),
            bool should_timeout = false,
            unsigned int timeout = DEFAULT_COMMSTEST_TIMEOUT,
            size_t num_events = 0
            ) {

        required_final_peers = std::move(final_peers);
        required_transient_peers = std::move(transient_peers);
        required_post_conditions = post_conditions;
        required_event_counter = num_events;
        expect_timeout = should_timeout;

        internal::Peer::LoopData::getLoopData(CommsFixture::current_loop)->up();
        uv_prepare_init(CommsFixture::current_loop, &prepare_);
        uv_prepare_start(&prepare_, check_peer_db_cb);

        uv_timer_init(CommsFixture::current_loop, &timer_);
        uv_timer_start(&timer_, timeout_cb, timeout, 0);
        uv_unref((uv_handle_t*) &timer_);

        uv_run(CommsFixture::current_loop, UV_RUN_DEFAULT);

    }
};

range_t CommsFixture::required_final_peers;
range_t CommsFixture::required_transient_peers;
size_t CommsFixture::required_event_counter;
CommsFixture::pc CommsFixture::required_post_conditions;
bool CommsFixture::expect_timeout;
size_t CommsFixture::eventCounter;
uv_loop_t * CommsFixture::current_loop = NULL;
uv_loop_t * CommsFixture::loopSelector(void *) {
    return current_loop;
}

BOOST_FIXTURE_TEST_CASE( STABLE_test_initialization, CommsFixture ) {

    loop_until_final(range_t(0,0), NULL);
}

void pc_successful_connect(void) {

    /* empty */
    BOOST_CHECK_EQUAL(internal::Peer::LoopData::getPeerCount(CommsFixture::current_loop,
                internal::Peer::LoopData::RETRY_TO_CONNECT), 0);
    BOOST_CHECK_EQUAL(internal::Peer::LoopData::getPeerCount(CommsFixture::current_loop,
                internal::Peer::LoopData::RETRY_TO_LISTEN), 0);
    BOOST_CHECK_EQUAL(internal::Peer::LoopData::getPeerCount(CommsFixture::current_loop,
                internal::Peer::LoopData::ATTEMPTING_TO_CONNECT), 0);

    /* non-empty */
    BOOST_CHECK_EQUAL(internal::Peer::LoopData::getPeerCount(CommsFixture::current_loop,
                internal::Peer::LoopData::ONLINE), 2);
    BOOST_CHECK_EQUAL(internal::Peer::LoopData::getPeerCount(CommsFixture::current_loop,
                internal::Peer::LoopData::LISTENING), 1);

    /* no-transient guys */
    BOOST_CHECK_EQUAL(internal::Peer::LoopData::getPeerCount(CommsFixture::current_loop,
                internal::Peer::LoopData::TO_RESOLVE), 0);
    BOOST_CHECK_EQUAL(internal::Peer::LoopData::getPeerCount(CommsFixture::current_loop,
                internal::Peer::LoopData::TO_LISTEN), 0);
    BOOST_CHECK_EQUAL(internal::Peer::LoopData::getPeerCount(CommsFixture::current_loop,
                internal::Peer::LoopData::PENDING_DELETE), 0);

}

void DoNothingOnConnect (
        ::yajr::Peer * p,
        void * data,
        ::yajr::StateChange::To stateChange,
        int error) {
    switch(stateChange) {
        case ::yajr::StateChange::CONNECT:
            LOG(DEBUG) << "got a CONNECT notification on " << p;
            LOG(INFO) << "chill out, we just had a connection on " << p;
            break;
        case ::yajr::StateChange::DISCONNECT:
            LOG(DEBUG) << "got a DISCONNECT notification on " << p;
            break;
        case ::yajr::StateChange::FAILURE:
            ++CommsFixture::eventCounter;
            LOG(DEBUG) << "got a FAILURE notification on " << p;
            break;
        case ::yajr::StateChange::TRANSPORT_FAILURE:
            ++CommsFixture::eventCounter;
            LOG(DEBUG) << "got a TRANSPORT_FAILURE notification on " << p;
            break;
        case ::yajr::StateChange::DELETE:
            LOG(DEBUG) << "got a DELETE notification on " << p;
            break;
        default:
            LOG(DEBUG)
                << "got a notification number "
                << stateChange
                << " on "
                << p
            ;
            assert(0);
    }
}

::yajr::Peer::StateChangeCb doNothingOnConnect = DoNothingOnConnect;

void StartPingingOnConnect(
        ::yajr::Peer * p,
        void * data,
        ::yajr::StateChange::To stateChange,
        int error) {
    switch(stateChange) {
        case ::yajr::StateChange::CONNECT:
            LOG(DEBUG) << "got a CONNECT notification on " << p;
            LOG(INFO) << "starting keep-alive, as we just had a connection on " << p;
            p->startKeepAlive();
            break;
        case ::yajr::StateChange::DISCONNECT:
            LOG(DEBUG) << "got a DISCONNECT notification on " << p;
            break;
        case ::yajr::StateChange::FAILURE:
            ++CommsFixture::eventCounter;
            LOG(DEBUG) << "got a FAILURE notification on " << p;
            break;
        case ::yajr::StateChange::TRANSPORT_FAILURE:
            ++CommsFixture::eventCounter;
            LOG(DEBUG) << "got a TRANSPORT_FAILURE notification on " << p;
            break;
        case ::yajr::StateChange::DELETE:
            LOG(DEBUG) << "got a DELETE notification on " << p;
            break;
        default:
            LOG(DEBUG)
                << "got a notification number "
                << stateChange
                << " on "
                << p
            ;
            assert(0);
    }
}

::yajr::Peer::StateChangeCb startPingingOnConnect = StartPingingOnConnect;

BOOST_FIXTURE_TEST_CASE( STABLE_test_ipv4, CommsFixture ) {

    ::yajr::Listener * l = ::yajr::Listener::create(
            "127.0.0.1", 65535-kPortOffset, doNothingOnConnect,
            NULL, NULL, CommsFixture::current_loop, CommsFixture::loopSelector
    );

    BOOST_CHECK_EQUAL(!l, 0);

    ::yajr::Peer * p = ::yajr::Peer::create(
            "localhost", std::to_string(65535-kPortOffset), doNothingOnConnect,
            NULL, CommsFixture::loopSelector
    );

    BOOST_CHECK_EQUAL(!p, 0);

    loop_until_final(range_t(3,3), pc_successful_connect);

}

BOOST_FIXTURE_TEST_CASE( STABLE_test_ipv6, CommsFixture ) {

    ::yajr::Listener * l = ::yajr::Listener::create(
            "::1", 65534-kPortOffset, doNothingOnConnect,
            NULL, NULL, CommsFixture::current_loop, CommsFixture::loopSelector
    );

    BOOST_CHECK_EQUAL(!l, 0);

    ::yajr::Peer * p = ::yajr::Peer::create(
            "::", std::to_string(65534-kPortOffset), doNothingOnConnect,
            NULL, CommsFixture::loopSelector
    );

    BOOST_CHECK_EQUAL(!p, 0);

    loop_until_final(range_t(3,3), pc_successful_connect);

}


BOOST_FIXTURE_TEST_CASE( STABLE_test_pipe, CommsFixture ) {

    static const char * domainSocket = "/tmp/comms_test_test_pipe.sock";

    unlink(domainSocket);

    ::yajr::Listener * l = ::yajr::Listener::create(
            domainSocket, doNothingOnConnect,
            NULL, NULL, CommsFixture::current_loop, CommsFixture::loopSelector
    );

    BOOST_CHECK_EQUAL(!l, 0);

    ::yajr::Peer * p = ::yajr::Peer::create(
            domainSocket, doNothingOnConnect,
            NULL, CommsFixture::loopSelector
    );

    BOOST_CHECK_EQUAL(!p, 0);

    loop_until_final(range_t(3,3), pc_successful_connect);

}

static void pc_non_existent(void) {

    /* non-empty */
    BOOST_CHECK_EQUAL(internal::Peer::LoopData::getPeerCount(CommsFixture::current_loop,
                internal::Peer::LoopData::RETRY_TO_CONNECT), 1);

    /* empty */
    BOOST_CHECK_EQUAL(internal::Peer::LoopData::getPeerCount(CommsFixture::current_loop,
                internal::Peer::LoopData::RETRY_TO_LISTEN), 0);
    BOOST_CHECK_EQUAL(internal::Peer::LoopData::getPeerCount(CommsFixture::current_loop,
                internal::Peer::LoopData::ATTEMPTING_TO_CONNECT), 0);
    BOOST_CHECK_EQUAL(internal::Peer::LoopData::getPeerCount(CommsFixture::current_loop,
                internal::Peer::LoopData::ONLINE), 0);
    BOOST_CHECK_EQUAL(internal::Peer::LoopData::getPeerCount(CommsFixture::current_loop,
                internal::Peer::LoopData::LISTENING), 0);

    /* no-transient guys */
    BOOST_CHECK_EQUAL(internal::Peer::LoopData::getPeerCount(CommsFixture::current_loop,
                internal::Peer::LoopData::TO_RESOLVE), 0);
    BOOST_CHECK_EQUAL(internal::Peer::LoopData::getPeerCount(CommsFixture::current_loop,
                internal::Peer::LoopData::TO_LISTEN), 0);
    BOOST_CHECK_EQUAL(internal::Peer::LoopData::getPeerCount(CommsFixture::current_loop,
                internal::Peer::LoopData::PENDING_DELETE), 0);
}

BOOST_FIXTURE_TEST_CASE( STABLE_test_non_existent_host, CommsFixture ) {

    ::yajr::Peer * p = ::yajr::Peer::create(
            "non_existent_host.", std::to_string(65533-kPortOffset), doNothingOnConnect,
            NULL, CommsFixture::loopSelector
    );

    BOOST_CHECK_EQUAL(!p, 0);

    loop_until_final(range_t(1,1), pc_non_existent);

}

BOOST_FIXTURE_TEST_CASE( STABLE_test_non_existent_service, CommsFixture ) {

    ::yajr::Peer * p = ::yajr::Peer::create(
            "127.0.0.1", std::to_string(65533-kPortOffset), doNothingOnConnect,
            NULL, CommsFixture::loopSelector
    );

    BOOST_CHECK_EQUAL(!p, 0);

    loop_until_final(range_t(1,1), pc_non_existent);

}

BOOST_FIXTURE_TEST_CASE( STABLE_test_pipe_no_server, CommsFixture ) {

    static const char * domainSocket = "/tmp/comms_test_test_pipe_no_server.sock";

    unlink(domainSocket);

    ::yajr::Peer * p = ::yajr::Peer::create(
            domainSocket, doNothingOnConnect,
            NULL, CommsFixture::loopSelector
    );

    BOOST_CHECK_EQUAL(!p, 0);

    loop_until_final(range_t(1,1), pc_non_existent);

}

BOOST_FIXTURE_TEST_CASE( STABLE_test_keepalive, CommsFixture ) {

    LOG(DEBUG);

    ::yajr::Listener * l = ::yajr::Listener::create(
            "127.0.0.1", 65532-kPortOffset, startPingingOnConnect,
            NULL, NULL, CommsFixture::current_loop, CommsFixture::loopSelector
    );

    BOOST_CHECK_EQUAL(!l, 0);

    ::yajr::Peer * p = ::yajr::Peer::create(
            "127.0.0.1", std::to_string(65532-kPortOffset), startPingingOnConnect,
            NULL, CommsFixture::loopSelector
    );

    BOOST_CHECK_EQUAL(!p, 0);

    loop_until_final(range_t(4,4), pc_successful_connect, range_t(0,0), true, DEFAULT_COMMSTEST_TIMEOUT); // 4 is to cause a timeout

}

void pc_no_peers(void) {

    /* empty */
    BOOST_CHECK_EQUAL(internal::Peer::LoopData::getPeerCount(CommsFixture::current_loop,
                internal::Peer::LoopData::RETRY_TO_CONNECT), 0);
    BOOST_CHECK_EQUAL(internal::Peer::LoopData::getPeerCount(CommsFixture::current_loop,
                internal::Peer::LoopData::RETRY_TO_LISTEN), 0);
    BOOST_CHECK_EQUAL(internal::Peer::LoopData::getPeerCount(CommsFixture::current_loop,
                internal::Peer::LoopData::ATTEMPTING_TO_CONNECT), 0);
    BOOST_CHECK_EQUAL(internal::Peer::LoopData::getPeerCount(CommsFixture::current_loop,
                internal::Peer::LoopData::ONLINE), 0);
    BOOST_CHECK_EQUAL(internal::Peer::LoopData::getPeerCount(CommsFixture::current_loop,
                internal::Peer::LoopData::LISTENING), 0);

    /* no-transient guys */
    BOOST_CHECK_EQUAL(internal::Peer::LoopData::getPeerCount(CommsFixture::current_loop,
                internal::Peer::LoopData::TO_RESOLVE), 0);
    BOOST_CHECK_EQUAL(internal::Peer::LoopData::getPeerCount(CommsFixture::current_loop,
                internal::Peer::LoopData::TO_LISTEN), 0);
    BOOST_CHECK_EQUAL(internal::Peer::LoopData::getPeerCount(CommsFixture::current_loop,
                internal::Peer::LoopData::PENDING_DELETE), 0);

}

BOOST_FIXTURE_TEST_CASE( STABLE_test_destroy_listener_early, CommsFixture ) {

    LOG(DEBUG);

    ::yajr::Listener * l = ::yajr::Listener::create(
            "127.0.0.1", 65531-kPortOffset, doNothingOnConnect,
            NULL, NULL, CommsFixture::current_loop, CommsFixture::loopSelector
    );

    BOOST_CHECK_EQUAL(!l, 0);

    l->destroy();

    loop_until_final(range_t(0,0), pc_no_peers);

}

BOOST_FIXTURE_TEST_CASE( STABLE_test_destroy_listener_late, CommsFixture ) {

    LOG(DEBUG);

    ::yajr::Listener * l = ::yajr::Listener::create(
            "127.0.0.1", 65531-kPortOffset, doNothingOnConnect,
            NULL, NULL, CommsFixture::current_loop, CommsFixture::loopSelector
    );

    BOOST_CHECK_EQUAL(!l, 0);

    uv_timer_t destroy_timer;

    destroy_timer.data = l;

    uv_timer_init(CommsFixture::current_loop, &destroy_timer);
    uv_timer_start(&destroy_timer, destroy_listener_cb, 200, 0);
    uv_unref((uv_handle_t*) &destroy_timer);

    loop_until_final(range_t(0,0), pc_no_peers);

}

BOOST_FIXTURE_TEST_CASE( STABLE_test_destroy_client_early, CommsFixture ) {

    LOG(DEBUG);

    ::yajr::Peer * p = ::yajr::Peer::create(
            "localhost", std::to_string(65530-kPortOffset), doNothingOnConnect,
            NULL, CommsFixture::loopSelector
    );

    BOOST_CHECK_EQUAL(!p, 0);

    p->destroy();

    loop_until_final(range_t(0,0), pc_no_peers);

}

void pc_listening_peer(void) {

    LOG(DEBUG);

    /* one listener */
    BOOST_CHECK_EQUAL(internal::Peer::LoopData::getPeerCount(CommsFixture::current_loop,
                internal::Peer::LoopData::LISTENING), 1);

    /* empty */
    BOOST_CHECK_EQUAL(internal::Peer::LoopData::getPeerCount(CommsFixture::current_loop,
                internal::Peer::LoopData::RETRY_TO_CONNECT), 0);
    BOOST_CHECK_EQUAL(internal::Peer::LoopData::getPeerCount(CommsFixture::current_loop,
                internal::Peer::LoopData::RETRY_TO_LISTEN), 0);
    BOOST_CHECK_EQUAL(internal::Peer::LoopData::getPeerCount(CommsFixture::current_loop,
                internal::Peer::LoopData::ATTEMPTING_TO_CONNECT), 0);
    BOOST_CHECK_EQUAL(internal::Peer::LoopData::getPeerCount(CommsFixture::current_loop,
                internal::Peer::LoopData::ONLINE), 0);

    /* no-transient guys */
    BOOST_CHECK_EQUAL(internal::Peer::LoopData::getPeerCount(CommsFixture::current_loop,
                internal::Peer::LoopData::TO_RESOLVE), 0);
    BOOST_CHECK_EQUAL(internal::Peer::LoopData::getPeerCount(CommsFixture::current_loop,
                internal::Peer::LoopData::TO_LISTEN), 0);
    BOOST_CHECK_EQUAL(internal::Peer::LoopData::getPeerCount(CommsFixture::current_loop,
                internal::Peer::LoopData::PENDING_DELETE), 0);

}

BOOST_FIXTURE_TEST_CASE( STABLE_test_destroy_client_late, CommsFixture ) {

    LOG(DEBUG);

    ::yajr::Listener * l = ::yajr::Listener::create(
            "127.0.0.1", 65529-kPortOffset, doNothingOnConnect,
            NULL, NULL, CommsFixture::current_loop, CommsFixture::loopSelector
    );

    BOOST_CHECK_EQUAL(!l, 0);

    ::yajr::Peer * p = ::yajr::Peer::create(
            "localhost", std::to_string(65529-kPortOffset), doNothingOnConnect,
            NULL, CommsFixture::loopSelector
    );

    BOOST_CHECK_EQUAL(!p, 0);

    uv_timer_t destroy_timer;

    destroy_timer.data = p;

    uv_timer_init(CommsFixture::current_loop, &destroy_timer);

    uv_timer_start(&destroy_timer, destroy_peer_cb, 200, 0);
    uv_unref((uv_handle_t*) &destroy_timer);

    loop_until_final(range_t(1,1), pc_listening_peer);

}

void DisconnectOnCallback (
        ::yajr::Peer * p,
        void * data,
        ::yajr::StateChange::To stateChange,
        int error) {
    switch(stateChange) {
        case ::yajr::StateChange::CONNECT:
            LOG(DEBUG) << "got a CONNECT notification on " << p;
            LOG(INFO)
                << "we just had a connection on "
                << p
                << " and we are gonna Disconnect it"
            ;
            p->disconnect();
            break;
        case ::yajr::StateChange::DISCONNECT:
            LOG(DEBUG) << "got a DISCONNECT notification on " << p;
            LOG(INFO)
                << "we had a disconnection on "
                << p
                << " and we are gonna Disconnect it nevertheless to verify that it does no harm"
            ;
            p->disconnect();
            break;
        case ::yajr::StateChange::FAILURE:
            ++CommsFixture::eventCounter;
            LOG(DEBUG) << "got a FAILURE notification on " << p;
            LOG(INFO)
                << "we failed to have a connection on " << p
                << " and we are gonna Disconnect it nevertheless to verify that it does no harm"
            ;
            p->disconnect();
            break;
        case ::yajr::StateChange::TRANSPORT_FAILURE:
            ++CommsFixture::eventCounter;
            LOG(DEBUG)
                << "got a TRANSPORT_FAILURE notification on " << p
                << " and we are gonna Disconnect it nevertheless to verify that it does no harm"
            ;
            p->disconnect();
            break;
        case ::yajr::StateChange::DELETE:
            LOG(DEBUG) << "got a DELETE notification on " << p;
            break;
        default:
            LOG(DEBUG)
                << "got a notification number "
                << stateChange
                << " on "
                << p
            ;
            assert(0);
    }
}

::yajr::Peer::StateChangeCb disconnectOnCallback = DisconnectOnCallback;

void DestroyOnCallback (
        ::yajr::Peer * p,
        void * data,
        ::yajr::StateChange::To stateChange,
        int error) {
    switch(stateChange) {
        case ::yajr::StateChange::CONNECT:
            LOG(DEBUG) << "got a CONNECT notification on " << p;
            LOG(INFO)
                << "we just had a connection on "
                << p
                << " and we are gonna Destroy it"
            ;
            p->destroy();
            break;
        case ::yajr::StateChange::DISCONNECT:
            LOG(DEBUG)
                << "got a DISCONNECT notification on "
                << p;
            break;
        case ::yajr::StateChange::FAILURE:
            ++CommsFixture::eventCounter;
            LOG(DEBUG) << "got a FAILURE notification on " << p;
            LOG(INFO)
                << "we failed to have a connection on " << p
                << " and we are gonna Destroy it nevertheless to verify that it does no harm"
            ;
            p->destroy();
            break;
        case ::yajr::StateChange::TRANSPORT_FAILURE:
            ++CommsFixture::eventCounter;
            LOG(DEBUG)
                << "got a TRANSPORT_FAILURE notification on " << p
                << " and we are gonna Destroy it nevertheless to verify that it does no harm"
            ;
            p->destroy();
            break;
        case ::yajr::StateChange::DELETE:
            LOG(DEBUG)
                << "got a DELETE notification on "
                << p;
            break;
        default:
            LOG(DEBUG)
                << "got a notification number "
                << stateChange
                << " on "
                << p
            ;
            assert(0);
    }
}

::yajr::Peer::StateChangeCb destroyOnCallback = DestroyOnCallback;

void DestroyOnDisconnect (
        ::yajr::Peer * p,
        void * data,
        ::yajr::StateChange::To stateChange,
        int error) {
    switch(stateChange) {
        case ::yajr::StateChange::CONNECT:
            LOG(DEBUG) << "got a CONNECT notification on " << p;
            break;
        case ::yajr::StateChange::DISCONNECT:
            LOG(DEBUG) << "got a DISCONNECT notification on " << p;
            LOG(INFO)
                << "we had a disconnection on " << p
                << " and we are gonna Disconnect it nevertheless to verify that it does no harm"
            ;
            p->destroy();
            break;
        case ::yajr::StateChange::FAILURE:
            ++CommsFixture::eventCounter;
            LOG(DEBUG) << "got a FAILURE notification on " << p;
            LOG(INFO)
                << "we failed to have a connection on "
                << p
                << " and we are not gonna do anything with it"
            ;
            break;
        case ::yajr::StateChange::TRANSPORT_FAILURE:
            ++CommsFixture::eventCounter;
            LOG(DEBUG)
                << "got a TRANSPORT_FAILURE notification on "
                << p
                << " and we are not gonna do anything with it"
            ;
            break;
        case ::yajr::StateChange::DELETE:
            LOG(DEBUG) << "got a DELETE notification on " << p;
            break;
        default:
            LOG(DEBUG)
                << "got a notification number "
                << stateChange
                << " on "
                << p
            ;
            assert(0);
    }
}

::yajr::Peer::StateChangeCb destroyOnDisconnect = DestroyOnDisconnect;

BOOST_FIXTURE_TEST_CASE( STABLE_test_destroy_client_before_connect, CommsFixture ) {

    ::yajr::Peer * p = ::yajr::Peer::create(
            "localhost", std::to_string(65528-kPortOffset), destroyOnCallback,
            NULL, CommsFixture::loopSelector
    );

    BOOST_CHECK_EQUAL(!p, 0);

    loop_until_final(range_t(0,0), pc_no_peers);

}

void pc_retrying_client(void) {

    /* one listener */
    BOOST_CHECK_EQUAL(internal::Peer::LoopData::getPeerCount(CommsFixture::current_loop,
                internal::Peer::LoopData::RETRY_TO_CONNECT), 1);

    /* empty */
    BOOST_CHECK_EQUAL(internal::Peer::LoopData::getPeerCount(CommsFixture::current_loop,
                internal::Peer::LoopData::RETRY_TO_LISTEN), 0);
    BOOST_CHECK_EQUAL(internal::Peer::LoopData::getPeerCount(CommsFixture::current_loop,
                internal::Peer::LoopData::ATTEMPTING_TO_CONNECT), 0);
    BOOST_CHECK_EQUAL(internal::Peer::LoopData::getPeerCount(CommsFixture::current_loop,
                internal::Peer::LoopData::ONLINE), 0);
    BOOST_CHECK_EQUAL(internal::Peer::LoopData::getPeerCount(CommsFixture::current_loop,
                internal::Peer::LoopData::LISTENING), 0);

    /* no-transient guys */
    BOOST_CHECK_EQUAL(internal::Peer::LoopData::getPeerCount(CommsFixture::current_loop,
                internal::Peer::LoopData::TO_RESOLVE), 0);
    BOOST_CHECK_EQUAL(internal::Peer::LoopData::getPeerCount(CommsFixture::current_loop,
                internal::Peer::LoopData::TO_LISTEN), 0);
    BOOST_CHECK_EQUAL(internal::Peer::LoopData::getPeerCount(CommsFixture::current_loop,
                internal::Peer::LoopData::PENDING_DELETE), 0);
}

BOOST_FIXTURE_TEST_CASE( STABLE_test_disconnect_client_before_connect, CommsFixture ) {

    ::yajr::Peer * p = ::yajr::Peer::create(
            "localhost", std::to_string(65527-kPortOffset), disconnectOnCallback,
            NULL, CommsFixture::loopSelector
    );

    BOOST_CHECK_EQUAL(!p, 0);

    loop_until_final(range_t(1,1), pc_retrying_client);

}

BOOST_FIXTURE_TEST_CASE( STABLE_test_destroy_client_after_connect, CommsFixture ) {

    ::yajr::Listener * l = ::yajr::Listener::create(
            "127.0.0.1", 65526-kPortOffset, doNothingOnConnect,
            NULL, NULL, CommsFixture::current_loop, CommsFixture::loopSelector
    );

    BOOST_CHECK_EQUAL(!l, 0);

    ::yajr::Peer * p = ::yajr::Peer::create(
            "localhost", std::to_string(65526-kPortOffset), destroyOnCallback,
            NULL, CommsFixture::loopSelector
    );

    BOOST_CHECK_EQUAL(!p, 0);

    loop_until_final(range_t(1,1), pc_listening_peer);

}

void pc_retrying_peers(void) {

    /* one listener */
    BOOST_CHECK_EQUAL(internal::Peer::LoopData::getPeerCount(CommsFixture::current_loop,
                internal::Peer::LoopData::RETRY_TO_CONNECT), 1);
    BOOST_CHECK_EQUAL(internal::Peer::LoopData::getPeerCount(CommsFixture::current_loop,
                internal::Peer::LoopData::LISTENING), 1);

    /* empty */
    BOOST_CHECK_EQUAL(internal::Peer::LoopData::getPeerCount(CommsFixture::current_loop,
                internal::Peer::LoopData::RETRY_TO_LISTEN), 0);
    BOOST_CHECK_EQUAL(internal::Peer::LoopData::getPeerCount(CommsFixture::current_loop,
                internal::Peer::LoopData::ATTEMPTING_TO_CONNECT), 0);
    BOOST_CHECK_EQUAL(internal::Peer::LoopData::getPeerCount(CommsFixture::current_loop,
                internal::Peer::LoopData::ONLINE), 0);

    /* no-transient guys */
    BOOST_CHECK_EQUAL(internal::Peer::LoopData::getPeerCount(CommsFixture::current_loop,
                internal::Peer::LoopData::TO_RESOLVE), 0);
    BOOST_CHECK_EQUAL(internal::Peer::LoopData::getPeerCount(CommsFixture::current_loop,
                internal::Peer::LoopData::TO_LISTEN), 0);
    BOOST_CHECK_EQUAL(internal::Peer::LoopData::getPeerCount(CommsFixture::current_loop,
                internal::Peer::LoopData::PENDING_DELETE), 0);

}

BOOST_FIXTURE_TEST_CASE( STABLE_test_disconnect_client_after_connect, CommsFixture ) {

    ::yajr::Listener * l = ::yajr::Listener::create(
            "127.0.0.1", 65525-kPortOffset, doNothingOnConnect,
            NULL, NULL, CommsFixture::current_loop, CommsFixture::loopSelector
    );

    BOOST_CHECK_EQUAL(!l, 0);

    ::yajr::Peer * p = ::yajr::Peer::create(
            "localhost", std::to_string(65525-kPortOffset), disconnectOnCallback,
            NULL, CommsFixture::loopSelector
    );

    BOOST_CHECK_EQUAL(!p, 0);

    loop_until_final(range_t(2,2), pc_retrying_peers);

}

BOOST_FIXTURE_TEST_CASE( STABLE_test_destroy_server_after_connect, CommsFixture ) {

    ::yajr::Listener * l = ::yajr::Listener::create(
            "127.0.0.1", 65524-kPortOffset, destroyOnCallback,
            NULL, NULL, CommsFixture::current_loop, CommsFixture::loopSelector
    );
    BOOST_CHECK_EQUAL(!l, 0);

    ::yajr::Peer * p = ::yajr::Peer::create(
            "localhost", std::to_string(65524-kPortOffset), destroyOnDisconnect,
            NULL, CommsFixture::loopSelector
    );

    BOOST_CHECK_EQUAL(!p, 0);

    loop_until_final(range_t(1,1), pc_listening_peer);

}

BOOST_FIXTURE_TEST_CASE( STABLE_test_disconnect_server_after_connect, CommsFixture ) {

    ::yajr::Listener * l = ::yajr::Listener::create(
            "127.0.0.1", 65523-kPortOffset, disconnectOnCallback,
            NULL, NULL, CommsFixture::current_loop, CommsFixture::loopSelector
    );
    BOOST_CHECK_EQUAL(!l, 0);

    ::yajr::Peer * p = ::yajr::Peer::create(
            "localhost", std::to_string(65523-kPortOffset), destroyOnDisconnect,
            NULL, CommsFixture::loopSelector
    );

    BOOST_CHECK_EQUAL(!p, 0);

    loop_until_final(range_t(1,1), pc_listening_peer);

}

::yajr::Listener * flakyListener;

void DisconnectAndStopListeningOnCallback (
        ::yajr::Peer * p,
        void * data,
        ::yajr::StateChange::To stateChange,
        int error) {
    switch(stateChange) {
        case ::yajr::StateChange::CONNECT:
            LOG(DEBUG)
                << "got a CONNECT notification on "
                << p
            ;
            LOG(INFO)
                << "we just had a connection on "
                << p
                << " and we are gonna Disconnect it"
            ;
            p->disconnect();
            LOG(INFO)
                << "also, we are gonna stop listening!"
            ;
            flakyListener->destroy();
            break;
        case ::yajr::StateChange::DISCONNECT:
            LOG(DEBUG) << "got a DISCONNECT notification on " << p;
            LOG(INFO)
                << "we had a disconnection on "
                << p
                << " and we are gonna Disconnect it nevertheless"
                << " just to verify that it does no harm"
            ;
            p->disconnect();
            break;
        case ::yajr::StateChange::FAILURE:
            ++CommsFixture::eventCounter;
            LOG(DEBUG) << "got a FAILURE notification on " << p;
            LOG(INFO)
                << "we failed to have a connection on "
                << p
                << " and we are gonna Disconnect it nevertheless"
                << " just to verify that it does no harm"
            ;
            p->disconnect();
            break;
        case ::yajr::StateChange::TRANSPORT_FAILURE:
            ++CommsFixture::eventCounter;
            LOG(DEBUG)
                << "got a TRANSPORT_FAILURE notification on "
                << p
                << " and we are gonna Disconnect it nevertheless"
                << " just to verify that it does no harm"
            ;
            break;
        case ::yajr::StateChange::DELETE:
            LOG(DEBUG) << "got a DELETE notification on " << p;
            break;
        default:
            LOG(DEBUG)
                << "got a notification number "
                << stateChange
                << " on "
                << p
            ;
            assert(0);
    }
}

::yajr::Peer::StateChangeCb disconnectAndStopListeningOnCallback = DisconnectAndStopListeningOnCallback;

void CountdownAttemptsOnCallback (
        ::yajr::Peer * p,
        void * data,
        ::yajr::StateChange::To stateChange,
        int error) {

    static size_t attempts = 4;

    switch(stateChange) {
        case ::yajr::StateChange::CONNECT:
            LOG(DEBUG) << "got a CONNECT notification on " << p;
            break;
        case ::yajr::StateChange::DISCONNECT:
            LOG(DEBUG) << "got a DISCONNECT notification on " << p;
            break;
        case ::yajr::StateChange::FAILURE:
            ++CommsFixture::eventCounter;
            LOG(DEBUG) << "got a FAILURE notification on " << p;
            if (!--attempts) {
                p->destroy();
            }
            break;
        case ::yajr::StateChange::TRANSPORT_FAILURE:
            ++CommsFixture::eventCounter;
            LOG(DEBUG) << "got a TRANSPORT_FAILURE notification on " << p;
            if (!--attempts) {
                p->destroy();
            }
            break;
        case ::yajr::StateChange::DELETE:
            LOG(DEBUG) << "got a DELETE notification on " << p;
            break;
        default:
            LOG(DEBUG)
                << "got a notification number "
                << stateChange
                << " on "
                << p
            ;
            assert(0);
    }
}

::yajr::Peer::StateChangeCb countdownAttemptsOnCallback = CountdownAttemptsOnCallback;

void pc_no_server_and_client_gives_up(void) {

    /* no listener */
    BOOST_CHECK_EQUAL(internal::Peer::LoopData::getPeerCount(CommsFixture::current_loop,
                internal::Peer::LoopData::LISTENING), 0);

    /* empty */
    BOOST_CHECK_EQUAL(internal::Peer::LoopData::getPeerCount(CommsFixture::current_loop,
                internal::Peer::LoopData::RETRY_TO_CONNECT), 0);
    BOOST_CHECK_EQUAL(internal::Peer::LoopData::getPeerCount(CommsFixture::current_loop,
                internal::Peer::LoopData::RETRY_TO_LISTEN), 0);
    BOOST_CHECK_EQUAL(internal::Peer::LoopData::getPeerCount(CommsFixture::current_loop,
                internal::Peer::LoopData::ATTEMPTING_TO_CONNECT), 0);
    BOOST_CHECK_EQUAL(internal::Peer::LoopData::getPeerCount(CommsFixture::current_loop,
                internal::Peer::LoopData::ONLINE), 0);

    /* no-transient guys */
    BOOST_CHECK_EQUAL(internal::Peer::LoopData::getPeerCount(CommsFixture::current_loop,
                internal::Peer::LoopData::TO_RESOLVE), 0);
    BOOST_CHECK_EQUAL(internal::Peer::LoopData::getPeerCount(CommsFixture::current_loop,
                internal::Peer::LoopData::TO_LISTEN), 0);
    BOOST_CHECK_EQUAL(internal::Peer::LoopData::getPeerCount(CommsFixture::current_loop,
                internal::Peer::LoopData::PENDING_DELETE), 0);

}

BOOST_FIXTURE_TEST_CASE( STABLE_test_client_retry_more_than_once, CommsFixture ) {

    flakyListener = ::yajr::Listener::create(
            "127.0.0.1", 65522-kPortOffset, disconnectAndStopListeningOnCallback,
            NULL, NULL, CommsFixture::current_loop, CommsFixture::loopSelector
    );
    BOOST_CHECK_EQUAL(!flakyListener, 0);

    ::yajr::Peer * p = ::yajr::Peer::create(
            "localhost", std::to_string(65522-kPortOffset), countdownAttemptsOnCallback,
            NULL, CommsFixture::loopSelector
    );

    BOOST_CHECK_EQUAL(!p, 0);

    loop_until_final(range_t(0,0), pc_no_server_and_client_gives_up, range_t(0,0));

}

BOOST_FIXTURE_TEST_CASE( STABLE_test_destroy_client_for_non_existent_host, CommsFixture ) {

    ::yajr::Peer * p = ::yajr::Peer::create(
            "non_existent_host.", std::to_string(65521-kPortOffset), destroyOnCallback,
            NULL, CommsFixture::loopSelector
    );

    BOOST_CHECK_EQUAL(!p, 0);

    loop_until_final(range_t(1,1), pc_non_existent);

}

BOOST_FIXTURE_TEST_CASE( STABLE_test_destroy_client_for_non_existent_service, CommsFixture ) {

    ::yajr::Peer * p = ::yajr::Peer::create(
            "127.0.0.1", std::to_string(65520-kPortOffset), destroyOnCallback,
            NULL, CommsFixture::loopSelector
    );

    BOOST_CHECK_EQUAL(!p, 0);

    loop_until_final(range_t(0,0), pc_no_peers);
}

BOOST_FIXTURE_TEST_CASE( STABLE_test_disconnect_client_for_non_existent_host, CommsFixture ) {

    ::yajr::Peer * p = ::yajr::Peer::create(
            "localhost", std::to_string(65519-kPortOffset), disconnectOnCallback,
            NULL, CommsFixture::loopSelector
    );

    BOOST_CHECK_EQUAL(!p, 0);

    loop_until_final(range_t(1,1), pc_retrying_client);

}

BOOST_FIXTURE_TEST_CASE( STABLE_test_disconnect_client_for_non_existent_service, CommsFixture ) {

    ::yajr::Peer * p = ::yajr::Peer::create(
            "localhost", std::to_string(65518-kPortOffset), disconnectOnCallback,
            NULL, CommsFixture::loopSelector
    );

    BOOST_CHECK_EQUAL(!p, 0);

    loop_until_final(range_t(1,1), pc_retrying_client);
}

void AttachPassiveSslTransportOnConnect(
        ::yajr::Peer * p,
        void * data,
        ::yajr::StateChange::To stateChange,
        int error) {
    switch(stateChange) {
        case ::yajr::StateChange::CONNECT:
            LOG(DEBUG) << "got a CONNECT notification on " << p;
            LOG(INFO)
                << "attaching passive SSL transport "
                << data
                << " to "
                << p;

            BOOST_CHECK_EQUAL(

            ZeroCopyOpenSSL::attachTransport(
                    p,
                    static_cast<ZeroCopyOpenSSL::Ctx *>(data))

            , true);

            break;
        case ::yajr::StateChange::DISCONNECT:
            LOG(DEBUG) << "got a DISCONNECT notification on " << p;
            break;
        case ::yajr::StateChange::FAILURE:
            ++CommsFixture::eventCounter;
            LOG(DEBUG) << "got a FAILURE notification on " << p;
            break;
        case ::yajr::StateChange::TRANSPORT_FAILURE:
            ++CommsFixture::eventCounter;
            LOG(DEBUG) << "got a TRANSPORT_FAILURE notification on " << p;
            break;
        case ::yajr::StateChange::DELETE:
            LOG(DEBUG) << "got a DELETE notification on " << p;
            break;
        default:
            LOG(DEBUG)
                << "got a notification number "
                << stateChange
                << " on "
                << p
            ;
            assert(0);
    }
}

::yajr::Peer::StateChangeCb attachPassiveSslTransportOnConnect = AttachPassiveSslTransportOnConnect;

void * passthroughAccept(yajr::Listener *, void * data, int) {
    return data;
}

BOOST_FIXTURE_TEST_CASE( STABLE_test_no_message_on_SSL, CommsFixture ) {

    LOG(DEBUG);

    std::unique_ptr< ::yajr::transport::ZeroCopyOpenSSL::Ctx > serverCtx(
        ::yajr::transport::ZeroCopyOpenSSL::Ctx::createCtx(
            NULL,
            SRCDIR"/test/server.pem",
            "password123"
        )
    );

    BOOST_CHECK_EQUAL(!serverCtx, 0);

    if (!serverCtx) {
        return;
    }

    ::yajr::Listener * l = ::yajr::Listener::create(
            "127.0.0.1",
            65508-kPortOffset,
            attachPassiveSslTransportOnConnect,
            passthroughAccept,
            serverCtx.get(),
            CommsFixture::current_loop, CommsFixture::loopSelector
    );

    BOOST_CHECK_EQUAL(!l, 0);

    ::yajr::Peer * p = ::yajr::Peer::create(
            "127.0.0.1",
            std::to_string(65508-kPortOffset),
            doNothingOnConnect,
            NULL, CommsFixture::loopSelector
    );

    BOOST_CHECK_EQUAL(!p, 0);

    std::unique_ptr< ::yajr::transport::ZeroCopyOpenSSL::Ctx > clientCtx(
        ::yajr::transport::ZeroCopyOpenSSL::Ctx::createCtx(
            SRCDIR"/test/ca.pem",
            NULL
        )
    );

    BOOST_CHECK_EQUAL(!clientCtx, 0);

    if (!clientCtx) {
        return;
    }

    bool ok = ZeroCopyOpenSSL::attachTransport(p, clientCtx.get());

    BOOST_CHECK_EQUAL(ok, true);

    if (!ok) {
        return;
    }

    loop_until_final(range_t(4,4), pc_successful_connect, range_t(0,0), true, 800); // 4 is to cause a timeout

}

void SinglePingOnConnect(
        ::yajr::Peer * p,
        void * data,
        ::yajr::StateChange::To stateChange,
        int error) {
    switch(stateChange) {
        case ::yajr::StateChange::CONNECT:
            LOG(DEBUG) << "got a CONNECT notification on " << p;
            LOG(INFO)
                << "sending a single Ping, as we just had a connection on "
                << p;

            dynamic_cast< ::yajr::comms::internal::CommunicationPeer *>(p)
                ->sendEchoReq(); /* just one! */
            break;
        case ::yajr::StateChange::DISCONNECT:
            LOG(DEBUG) << "got a DISCONNECT notification on " << p;
            break;
        case ::yajr::StateChange::FAILURE:
            ++CommsFixture::eventCounter;
            LOG(DEBUG) << "got a FAILURE notification on " << p;
            break;
        case ::yajr::StateChange::TRANSPORT_FAILURE:
            ++CommsFixture::eventCounter;
            LOG(DEBUG) << "got a TRANSPORT_FAILURE notification on " << p;
            break;
        case ::yajr::StateChange::DELETE:
            LOG(DEBUG) << "got a DELETE notification on " << p;
            break;
        default:
            LOG(DEBUG)
                << "got a notification number "
                << stateChange
                << " on "
                << p
            ;
            assert(0);
    }
}

::yajr::Peer::StateChangeCb singlePingOnConnect = SinglePingOnConnect;

BOOST_FIXTURE_TEST_CASE( STABLE_test_single_message_on_SSL, CommsFixture ) {

    LOG(DEBUG);

    std::unique_ptr< ::yajr::transport::ZeroCopyOpenSSL::Ctx > serverCtx(
        ::yajr::transport::ZeroCopyOpenSSL::Ctx::createCtx(
            NULL,
            SRCDIR"/test/server.pem",
            "password123"
        )
    );

    BOOST_CHECK_EQUAL(!serverCtx, 0);

    if (!serverCtx) {
        return;
    }

    ::yajr::Listener * l = ::yajr::Listener::create(
            "127.0.0.1",
            65507-kPortOffset,
            attachPassiveSslTransportOnConnect,
            passthroughAccept,
            serverCtx.get(),
            CommsFixture::current_loop, CommsFixture::loopSelector
    );

    BOOST_CHECK_EQUAL(!l, 0);

    ::yajr::Peer * p = ::yajr::Peer::create(
            "127.0.0.1",
            std::to_string(65507-kPortOffset),
            singlePingOnConnect,
            NULL, CommsFixture::loopSelector
    );

    BOOST_CHECK_EQUAL(!p, 0);

    std::unique_ptr< ::yajr::transport::ZeroCopyOpenSSL::Ctx > clientCtx(
        ::yajr::transport::ZeroCopyOpenSSL::Ctx::createCtx(
            SRCDIR"/test/ca.pem",
            NULL
        )
    );

    BOOST_CHECK_EQUAL(!clientCtx, 0);

    if (!clientCtx) {
        return;
    }

    bool ok = ZeroCopyOpenSSL::attachTransport(p, clientCtx.get());

    BOOST_CHECK_EQUAL(ok, true);

    if (!ok) {
        return;
    }

    loop_until_final(range_t(4,4), pc_successful_connect, range_t(0,0), true, 800); // 4 is to cause a timeout

}

BOOST_FIXTURE_TEST_CASE( STABLE_test_single_message_with_client_cert_on_SSL, CommsFixture ) {

    LOG(DEBUG);

    std::unique_ptr< ::yajr::transport::ZeroCopyOpenSSL::Ctx > serverCtx(
        ::yajr::transport::ZeroCopyOpenSSL::Ctx::createCtx(
            SRCDIR"/test/ca.pem",
            SRCDIR"/test/server.pem",
            "password123"
        )
    );

    BOOST_CHECK_EQUAL(!serverCtx, 0);

    if (!serverCtx) {
        return;
    }

    serverCtx->setVerify();

    ::yajr::Listener * l = ::yajr::Listener::create(
            "127.0.0.1",
            65506-kPortOffset,
            attachPassiveSslTransportOnConnect,
            passthroughAccept,
            serverCtx.get(),
            CommsFixture::current_loop, CommsFixture::loopSelector
    );

    BOOST_CHECK_EQUAL(!l, 0);

    ::yajr::Peer * p = ::yajr::Peer::create(
            "127.0.0.1",
            std::to_string(65506-kPortOffset),
            singlePingOnConnect,
            NULL, CommsFixture::loopSelector
    );

    BOOST_CHECK_EQUAL(!p, 0);

    std::unique_ptr< ::yajr::transport::ZeroCopyOpenSSL::Ctx > clientCtx(
        ::yajr::transport::ZeroCopyOpenSSL::Ctx::createCtx(
            SRCDIR"/test/ca.pem",
            SRCDIR"/test/server.pem",
            "password123"
        )
    );

    BOOST_CHECK_EQUAL(!clientCtx, 0);

    if (!clientCtx) {
        return;
    }

    bool ok = ZeroCopyOpenSSL::attachTransport(p, clientCtx.get());

    BOOST_CHECK_EQUAL(ok, true);

    if (!ok) {
        return;
    }

    loop_until_final(range_t(4,4), pc_successful_connect, range_t(0,0), true, 800); // 4 is to cause a timeout

}

BOOST_FIXTURE_TEST_CASE( STABLE_test_keepalive_on_SSL, CommsFixture ) {

    LOG(DEBUG);

    std::unique_ptr< ::yajr::transport::ZeroCopyOpenSSL::Ctx > serverCtx(
        ::yajr::transport::ZeroCopyOpenSSL::Ctx::createCtx(
            NULL,
            SRCDIR"/test/server.pem",
            "password123"
        )
    );


    BOOST_CHECK_EQUAL(!serverCtx, 0);

    if (!serverCtx) {
        return;
    }

    ::yajr::Listener * l = ::yajr::Listener::create(
            "127.0.0.1",
            65505-kPortOffset,
            attachPassiveSslTransportOnConnect,
            passthroughAccept,
            serverCtx.get(),
            CommsFixture::current_loop, CommsFixture::loopSelector
    );

    BOOST_CHECK_EQUAL(!l, 0);

    ::yajr::Peer * p = ::yajr::Peer::create(
            "127.0.0.1",
            std::to_string(65505-kPortOffset),
            startPingingOnConnect,
            NULL, CommsFixture::loopSelector
    );

    BOOST_CHECK_EQUAL(!p, 0);

    std::unique_ptr< ::yajr::transport::ZeroCopyOpenSSL::Ctx > clientCtx(
        ::yajr::transport::ZeroCopyOpenSSL::Ctx::createCtx(
            SRCDIR"/test/ca.pem",
            NULL
        )
    );

    BOOST_CHECK_EQUAL(!clientCtx, 0);

    if (!clientCtx) {
        return;
    }

    bool ok = ZeroCopyOpenSSL::attachTransport(p, clientCtx.get());

    BOOST_CHECK_EQUAL(ok, true);

    if (!ok) {
        return;
    }

    loop_until_final(range_t(4,4), pc_successful_connect, range_t(0,0), true, DEFAULT_COMMSTEST_TIMEOUT); // 4 is to cause a timeout

}

void pc_successful_connect200(void) {

    LOG(DEBUG);

    /* empty */
    BOOST_CHECK_EQUAL(internal::Peer::LoopData::getPeerCount(CommsFixture::current_loop,
                internal::Peer::LoopData::RETRY_TO_CONNECT), 0);
    BOOST_CHECK_EQUAL(internal::Peer::LoopData::getPeerCount(CommsFixture::current_loop,
                internal::Peer::LoopData::RETRY_TO_LISTEN), 0);
    BOOST_CHECK_EQUAL(internal::Peer::LoopData::getPeerCount(CommsFixture::current_loop,
                internal::Peer::LoopData::ATTEMPTING_TO_CONNECT), 0);

    /* non-empty */
    BOOST_CHECK_EQUAL(internal::Peer::LoopData::getPeerCount(CommsFixture::current_loop,
                internal::Peer::LoopData::ONLINE), 400);
    BOOST_CHECK_EQUAL(internal::Peer::LoopData::getPeerCount(CommsFixture::current_loop,
                internal::Peer::LoopData::LISTENING), 200);

    /* no-transient guys */
    BOOST_CHECK_EQUAL(internal::Peer::LoopData::getPeerCount(CommsFixture::current_loop,
                internal::Peer::LoopData::TO_RESOLVE), 0);
    BOOST_CHECK_EQUAL(internal::Peer::LoopData::getPeerCount(CommsFixture::current_loop,
                internal::Peer::LoopData::TO_LISTEN), 0);
    BOOST_CHECK_EQUAL(internal::Peer::LoopData::getPeerCount(CommsFixture::current_loop,
                internal::Peer::LoopData::PENDING_DELETE), 0);
}

BOOST_FIXTURE_TEST_CASE( STABLE_test_several_SSL_peers, CommsFixture ) {

    LOG(DEBUG);

    std::unique_ptr< ::yajr::transport::ZeroCopyOpenSSL::Ctx > serverCtx(
        ::yajr::transport::ZeroCopyOpenSSL::Ctx::createCtx(
            NULL,
            SRCDIR"/test/server.pem",
            "password123"
        )
    );

    BOOST_CHECK_EQUAL(!serverCtx, 0);

    if (!serverCtx) {
        return;
    }

    std::unique_ptr< ::yajr::transport::ZeroCopyOpenSSL::Ctx > clientCtx(
        ::yajr::transport::ZeroCopyOpenSSL::Ctx::createCtx(
            SRCDIR"/test/ca.pem",
            NULL
        )
    );

    BOOST_CHECK_EQUAL(!clientCtx, 0);

    if (!clientCtx) {
        return;
    }

    for(size_t n = 0; n < 200; ++n) {
        ::yajr::Listener * l = ::yajr::Listener::create(
                "127.0.0.1",
                65504-kPortOffset-n,
                attachPassiveSslTransportOnConnect,
                passthroughAccept,
                serverCtx.get(),
                CommsFixture::current_loop, CommsFixture::loopSelector
        );

        BOOST_CHECK_EQUAL(!l, 0);

        ::yajr::Peer * p = ::yajr::Peer::create(
                "127.0.0.1",
                std::to_string(65504-kPortOffset-n),
                singlePingOnConnect,
                NULL, CommsFixture::loopSelector
        );

        BOOST_CHECK_EQUAL(!p, 0);

        bool ok = ZeroCopyOpenSSL::attachTransport(p, clientCtx.get());

        BOOST_CHECK_EQUAL(ok, true);

        if (!ok) {
            return;
        }
    }

    loop_until_final(range_t(401,401), pc_successful_connect200, range_t(0,0), true, 800); // 401 is to cause a timeout

}


BOOST_AUTO_TEST_SUITE_END()

