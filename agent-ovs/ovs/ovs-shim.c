/*
 * Copyright (c) 2016 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include "ovs-shim.h"
#include "ovs-ofputil.h"

#include <openvswitch/ofp-actions.h>
#include <openvswitch/ofp-port.h>
#include <openvswitch/meta-flow.h>
#include <openvswitch/lib/byte-order.h>
#include <openvswitch/lib/dp-packet.h>
#include <openvswitch/ofp-msgs.h>

void format_action(const struct ofpact* acts, size_t ofpacts_len,
                   struct ds* str) {
    struct ofpact_format_params fp = { .s = str };
    ofpacts_format(acts, ofpacts_len, &fp);
}

int action_equal(const struct ofpact* lhs, size_t lhs_len,
                 const struct ofpact* rhs, size_t rhs_len) {
    return ofpacts_equal(lhs, lhs_len, rhs, rhs_len);
}

int group_mod_equal(struct ofputil_group_mod* lgm,
                    struct ofputil_group_mod* rgm) {
    if (lgm->group_id == rgm->group_id &&
        lgm->type == rgm->type) {
        int lempty = ovs_list_is_empty(&lgm->buckets);
        int rempty = ovs_list_is_empty(&rgm->buckets);
        if (lempty && rempty) {
            return 1;
        }
        if (lempty || rempty) {
            return 0;
        }
        struct ofputil_bucket *lhead = (struct ofputil_bucket *)&lgm->buckets;
        struct ofputil_bucket *rhead = (struct ofputil_bucket *)&rgm->buckets;
        struct ofputil_bucket *lbkt = ofputil_bucket_list_front(&lgm->buckets);
        struct ofputil_bucket *rbkt = ofputil_bucket_list_front(&rgm->buckets);
        while (lbkt != lhead && rbkt != rhead) {
            /* Buckets IDs are not compared because they are assigned by the
             * switch prior to OpenFlow 1.5
             */
            if (!ofpacts_equal(lbkt->ofpacts, lbkt->ofpacts_len,
                               rbkt->ofpacts, rbkt->ofpacts_len)) {
                return 0;
            }
            lbkt = ofputil_bucket_list_front(&lbkt->list_node);
            rbkt = ofputil_bucket_list_front(&rbkt->list_node);
        }
        return (lbkt == lhead && rbkt == rhead);
    }
    return 0;
}

uint64_t ovs_htonll(uint64_t v) {
    return htonll(v);
}

uint64_t ovs_ntohll(uint64_t v) {
    return ntohll(v);
}

void override_raw_actions(const struct ofpact* acts, size_t len) {
    const struct ofpact *act;
    OFPACT_FOR_EACH(act, acts, len) {
        if (act->type == OFPACT_OUTPUT_REG ||
            act->type == OFPACT_REG_MOVE ||
            act->type == OFPACT_RESUBMIT) {
            ((struct ofpact*)act)->raw = (uint8_t)(-1);
        }
    }
}

void override_flow_has_vlan(const struct ofpact* acts, size_t len) {
    // unset flow_has_vlan field for any fields where the flow parser
    // sets the field in a situation where the physical OVS doesn't
    const struct ofpact *act;
    OFPACT_FOR_EACH(act, acts, len) {
        if (act->type == OFPACT_SET_FIELD) {
            struct ofpact_set_field* sf = (struct ofpact_set_field*)act;
            if (sf->field->id != MFF_ETH_DST &&
                sf->field->id != MFF_ETH_SRC) {
                sf->flow_has_vlan = false;
            }
        }
    }
}

static void initSubFieldExt(struct mf_subfield *sf, enum mf_field_id id,
                            uint8_t start, uint8_t nBits) {
    const struct mf_field *field = mf_from_id(id);
    sf->field = field;
    sf->ofs = start;
    if (nBits)
        sf->n_bits = nBits;
    else
        sf->n_bits = field->n_bits;
}

static void initSubField(struct mf_subfield *sf, enum mf_field_id id) {
    initSubFieldExt(sf, id, 0, 0);
}

static int min(int a, int b) { return a < b ? a : b; }

void act_reg_move(struct ofpbuf* buf,
                  int srcRegId, int dstRegId,
                  uint8_t sourceOffset, uint8_t destOffset,
                  uint8_t nBits) {
    struct ofpact_reg_move *move = ofpact_put_REG_MOVE(buf);
    initSubFieldExt(&move->src, srcRegId, sourceOffset, nBits);
    initSubFieldExt(&move->dst, dstRegId, destOffset, nBits);

    int bitsToMove = min(move->src.n_bits, move->dst.n_bits);
    move->src.n_bits = bitsToMove;
    move->dst.n_bits  = bitsToMove;
}

void act_reg_load(struct ofpbuf* buf,
                  int regId, const void* regValue, const void* mask) {
    ofpact_put_reg_load(buf, mf_from_id(regId), regValue, mask);
}

void act_metadata(struct ofpbuf* buf, uint64_t metadata, uint64_t mask) {
    struct ofpact_metadata* meta = ofpact_put_WRITE_METADATA(buf);
    meta->metadata = htonll(metadata);
    meta->mask = htonll(mask);
}

void act_eth_src(struct ofpbuf* buf, const uint8_t *srcMac, int flowHasVlan) {
    if (srcMac) {
        uint8_t mask[6];
        memset(mask, 0xff, sizeof(mask));
        struct ofpact_set_field *sf =
            ofpact_put_set_field(buf, mf_from_id(MFF_ETH_SRC), srcMac, mask);
        sf->flow_has_vlan = flowHasVlan;
    }
}

void act_eth_dst(struct ofpbuf* buf, const uint8_t *dstMac, int flowHasVlan) {
    if (dstMac) {
        uint8_t mask[6];
        memset(mask, 0xff, sizeof(mask));
        struct ofpact_set_field *sf =
            ofpact_put_set_field(buf, mf_from_id(MFF_ETH_DST), dstMac, mask);
        sf->flow_has_vlan = flowHasVlan;
    }
}

void act_ip_src_v4(struct ofpbuf* buf, uint32_t addr) {
    struct ofpact_ipv4 *set = ofpact_put_SET_IPV4_SRC(buf);
    set->ipv4 = addr;
}

void act_ip_src_v6(struct ofpbuf* buf, uint8_t* addr) {
    uint8_t mask[16];
    memset(mask, 0xff, sizeof(mask));
    ofpact_put_set_field(buf, mf_from_id(MFF_IPV6_SRC), addr, mask);
}

void act_ip_dst_v4(struct ofpbuf* buf, uint32_t addr) {
    struct ofpact_ipv4 *set = ofpact_put_SET_IPV4_DST(buf);
    set->ipv4 = addr;
}

void act_ip_dst_v6(struct ofpbuf* buf, uint8_t* addr) {
    uint8_t mask[16];
    memset(mask, 0xff, sizeof(mask));
    ofpact_put_set_field(buf, mf_from_id(MFF_IPV6_DST), addr, mask);
}

void act_l4_src(struct ofpbuf* buf, uint16_t port, uint8_t proto) {
    struct ofpact_l4_port* act = ofpact_put_SET_L4_SRC_PORT(buf);
    act->port = port;
    act->flow_ip_proto = proto;
}

void act_l4_dst(struct ofpbuf* buf, uint16_t port, uint8_t proto) {
    struct ofpact_l4_port* act = ofpact_put_SET_L4_DST_PORT(buf);
    act->port = port;
    act->flow_ip_proto = proto;
}

void act_decttl(struct ofpbuf* buf) {
    struct ofpact_cnt_ids *ctlr = ofpact_put_DEC_TTL(buf);
    uint16_t ctlrId = 0;
    ofpbuf_put(buf, &ctlrId, sizeof(ctlrId));
    ctlr = (struct ofpact_cnt_ids*)buf->header; // needed because of put() above
    ctlr->n_controllers = 1;
    ofpact_finish(buf, &ctlr->ofpact);
}

void act_go(struct ofpbuf* buf, uint8_t tableId) {
    struct ofpact_goto_table *goTab = ofpact_put_GOTO_TABLE(buf);
    goTab->table_id = tableId;
}

void act_resubmit(struct ofpbuf* buf, uint32_t inPort, uint8_t tableId) {
    struct ofpact_resubmit *resubmit = ofpact_put_RESUBMIT(buf);
    resubmit->in_port = inPort;
    resubmit->table_id = tableId;
}

void act_output(struct ofpbuf* buf, uint32_t port) {
    struct ofpact_output *output = ofpact_put_OUTPUT(buf);
    output->port = port;
}

void act_output_reg(struct ofpbuf* buf, int srcRegId) {
    struct ofpact_output_reg *outputReg = ofpact_put_OUTPUT_REG(buf);
    outputReg->max_len = UINT16_MAX;
    //assert(outputReg->ofpact.raw == (uint8_t)(-1));
    initSubField(&outputReg->src, srcRegId);
}

void act_group(struct ofpbuf* buf, uint32_t groupId) {
    struct ofpact_group *group = ofpact_put_GROUP(buf);
    group->group_id = groupId;
}

void act_controller(struct ofpbuf* buf, uint16_t max_len) {
    struct ofpact_output *contr = ofpact_put_OUTPUT(buf);
    contr->port = OFPP_CONTROLLER;
    contr->max_len = max_len;
}

void act_push_vlan(struct ofpbuf* buf) {
    struct ofpact_push_vlan *act = ofpact_put_PUSH_VLAN(buf);
    /* In OVS 2.6.7, encode_PUSH_VLAN() sets the ethertype as well.
     * In OVS 2.11.2, this can be done only via encode_SET_VLAN_VID
     * with flow_has_vlan=0 and push_vlan_if_needed=1.*/
    act->ethertype = htons(0x8100);
}

void act_set_vlan_vid(struct ofpbuf* buf, uint16_t vlan) {
    struct ofpact_vlan_vid* act = ofpact_put_SET_VLAN_VID(buf);
    act->vlan_vid = (0xfff & vlan);
    act->push_vlan_if_needed = 0;
    act->flow_has_vlan = true;

}

void act_mod_nw_tos(struct ofpbuf* buf, uint8_t dscp) {
    struct ofpact_dscp* act = ofpact_put_SET_IP_DSCP(buf);
    /*
     * mark needs to be bit shifted 2 left to not overwrite the
     * lower 2 bits of type of service packet header.
     * source: man ovs-ofctl (/mod_nw_tos)
     */
    act->dscp = (dscp << 2);
}

void act_pop_vlan(struct ofpbuf* buf) {
    /* ugly hack to avoid the fact that there's no way in the API to
       make a pop vlan action */
    ofpact_put_STRIP_VLAN(buf)->ofpact.raw = 8;
}

void act_nat(struct ofpbuf* buf,
             uint32_t ip_min, /* network order */
             uint32_t ip_max, /* network order */
             uint16_t proto_min,
             uint16_t proto_max,
             bool snat) {
    uint16_t flags = (snat) ? NX_NAT_F_SRC : NX_NAT_F_DST;

    flags |= NX_NAT_F_PERSISTENT;
    struct ofpact_nat *act = ofpact_put_NAT(buf);
    act->flags = flags;
    act->range_af = AF_INET;
    act->range.addr.ipv4.min = ip_min;
    act->range.addr.ipv4.max = ip_max;
    act->range.proto.min = proto_min;
    act->range.proto.max = proto_max;
}

void act_unnat(struct ofpbuf* buf) {
    struct ofpact_nat *act = ofpact_put_NAT(buf);
    act->flags = 0;
    act->range_af = AF_UNSPEC;
}

void act_conntrack(struct ofpbuf* buf,
                   uint16_t flags,
                   uint16_t zoneImm,
                   int zoneSrc,
                   uint8_t recircTable,
                   uint16_t alg,
                   struct ofpact* actions,
                   size_t actsLen) {
    struct ofpact_conntrack* act = ofpact_put_CT(buf);
    act->flags = flags;
    act->zone_imm = zoneImm;
    if (zoneSrc) {
        const struct mf_field *field = mf_from_id(zoneSrc);
        act->zone_src.field = field;
        act->zone_src.ofs = 0;
        act->zone_src.n_bits = 16;
    }
    act->alg = alg;
    act->recirc_table = recircTable;
    if (actions) {
        ofpbuf_put(buf, actions, actsLen);
        act = (struct ofpact_conntrack*)buf->header;
    }
    ofpact_finish_CT(buf, &act);
}

void act_multipath(struct ofpbuf* buf,
                   int fields,
                   uint16_t basis,
                   int algorithm,
                   uint16_t maxLink,
                   uint32_t arg,
                   int dst) {
    struct ofpact_multipath* act = ofpact_put_MULTIPATH(buf);
    act->fields = fields;
    act->basis = basis;
    act->algorithm = algorithm;
    act->max_link = maxLink;
    act->arg = arg;
    initSubField(&act->dst, dst);
}

void act_macvlan_learn(struct ofpbuf* ofpacts,
                       uint16_t prio,
                       uint64_t cookie,
                       uint8_t table) {
    struct ofpact_learn *learn = ofpact_put_LEARN(ofpacts);
    learn->priority = prio;
    learn->cookie = htonll(cookie);
    learn->table_id = table;

    learn->idle_timeout = 300;
    learn->hard_timeout = OFP_FLOW_PERMANENT;
    learn->flags = NX_LEARN_F_DELETE_LEARNED;

    {
        // Learn/match on src VLAN
        struct ofpact_learn_spec *spec =
            ofpbuf_put_zeros(ofpacts, sizeof(struct ofpact_learn_spec));
        initSubField(&spec->dst, MFF_VLAN_TCI);
        spec->dst.n_bits = 13;
        spec->src = spec->dst;
        spec->n_bits = spec->dst.n_bits;
        spec->src_type = NX_LEARN_SRC_FIELD;
        spec->dst_type = NX_LEARN_DST_MATCH;
        learn = ofpacts->header;
    }
    {
        // Match on dst ethernet address and learn on src ethernet
        // address
        struct ofpact_learn_spec *spec =
            ofpbuf_put_zeros(ofpacts, sizeof(struct ofpact_learn_spec));
        initSubField(&spec->dst, MFF_ETH_DST);
        initSubField(&spec->src, MFF_ETH_SRC);
        spec->n_bits = spec->src.n_bits;
        spec->src_type = NX_LEARN_SRC_FIELD;
        spec->dst_type = NX_LEARN_DST_MATCH;
        learn = ofpacts->header;
    }
    {
        // Output to source port
        struct ofpact_learn_spec *spec =
            ofpbuf_put_zeros(ofpacts, sizeof(struct ofpact_learn_spec));
        initSubField(&spec->src, MFF_IN_PORT);
        spec->n_bits = spec->src.n_bits;
        spec->src_type = NX_LEARN_SRC_FIELD;
        spec->dst_type = NX_LEARN_DST_OUTPUT;
        learn = ofpacts->header;
    }
    ofpact_finish_LEARN(ofpacts, &learn);
}

struct dp_packet* alloc_dpp() {
    return (struct dp_packet*)malloc(sizeof(struct dp_packet));
}

size_t dpp_l4_size(const struct dp_packet* pkt) {
    return dp_packet_l4_size(pkt);
}

void *dpp_l4(const struct dp_packet* pkt) {
    return dp_packet_l4(pkt);
}

void *dpp_l3(const struct dp_packet* pkt) {
    return dp_packet_l3(pkt);
}

void *dpp_l2(const struct dp_packet* pkt) {
    return dp_packet_eth(pkt);
}

void *dpp_data(const struct dp_packet* pkt) {
    return dp_packet_data(pkt);
}

uint32_t dpp_size(const struct dp_packet* pkt) {
    return dp_packet_size(pkt);
}

void *dpp_at(const struct dp_packet* pkt, size_t offset, size_t size) {
    return dp_packet_at(pkt, offset, size);
}

void dpp_delete(struct dp_packet* pkt) {
    dp_packet_delete(pkt);
}

struct ds* alloc_ds() {
    struct ds* o = (struct ds*)malloc(sizeof(struct ds));
    ds_init(o);
    return o;
}

void clean_ds(void* p) {
    ds_destroy((struct ds*)p);
    free(p);
}

struct ofputil_phy_port* alloc_phy_port() {
    return (struct ofputil_phy_port*)malloc(sizeof(struct ofputil_phy_port));
}

struct ofpbuf * encode_tlv_table_request(enum ofp_version ofp_version) {
    return ofpraw_alloc(OFPRAW_NXT_TLV_TABLE_REQUEST,
            ofp_version, 0);
}

void print_tlv_map(struct ds *s, const struct ofputil_tlv_map *map) {
    ds_put_char(s, '\n');
    ds_put_format(s, " 0x%"PRIx16"\t0x%"PRIx8"\t%"PRIu8"\ttun_metadata%"PRIu16,
                          map->option_class, map->option_type, map->option_len,
                          map->index);

}

void act_tun_metadata_load(struct ofpbuf* buf,
        int regId, const void* regValue, const void* mask) {
    struct ofpact_set_field *sf =
        ofpact_put_reg_load(buf, mf_from_id(regId), NULL, NULL);
    memcpy(sf->value, regValue, 4);
    memcpy(ofpact_set_field_mask(sf),mask,4);
}

void act_tun_metadata_load_64(struct ofpbuf* buf,
        int regId, const void* regValue, const void* mask) {
    struct ofpact_set_field *sf =
        ofpact_put_reg_load(buf, mf_from_id(regId), NULL, NULL);
    memcpy(sf->value, regValue, 8);
    memcpy(ofpact_set_field_mask(sf),mask,8);
}

void act_clone(struct ofpbuf* buf, struct ofpbuf* nestedBuf) {
    size_t ofs = buf->size;
    ofpact_put_CLONE(buf);
    if(nestedBuf && nestedBuf->size>0) {
	ofpbuf_put(buf, nestedBuf->data, nestedBuf->size);
    }
    struct ofpact_nest *clone;
    clone = ofpbuf_at(buf, ofs, sizeof *clone);
    ofpact_finish_CLONE(buf, &clone);
}
