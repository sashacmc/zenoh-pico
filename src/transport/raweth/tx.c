//
// Copyright (c) 2022 ZettaScale Technology
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Apache License, Version 2.0
// which is available at https://www.apache.org/licenses/LICENSE-2.0.
//
// SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
//
// Contributors:
//   ZettaScale Zenoh Team, <zenoh@zettascale.tech>
//

// #include "zenoh-pico/transport/link/tx.h"

#include "zenoh-pico/transport/common/tx.h"

#include "zenoh-pico/config.h"
#include "zenoh-pico/protocol/codec/core.h"
#include "zenoh-pico/protocol/codec/network.h"
#include "zenoh-pico/protocol/codec/transport.h"
#include "zenoh-pico/protocol/iobuf.h"
#include "zenoh-pico/system/link/raweth.h"
#include "zenoh-pico/transport/transport.h"
#include "zenoh-pico/transport/utils.h"
#include "zenoh-pico/utils/logging.h"

#if Z_FEATURE_RAWETH_TRANSPORT == 1

/**
 * This function is unsafe because it operates in potentially concurrent data.
 * Make sure that the following mutexes are locked before calling this function:
 *  - ztm->_mutex_inner
 */
static _z_zint_t __unsafe_z_raweth_get_sn(_z_transport_multicast_t *ztm, z_reliability_t reliability) {
    _z_zint_t sn;
    if (reliability == Z_RELIABILITY_RELIABLE) {
        sn = ztm->_sn_tx_reliable;
        ztm->_sn_tx_reliable = _z_sn_increment(ztm->_sn_res, ztm->_sn_tx_reliable);
    } else {
        sn = ztm->_sn_tx_best_effort;
        ztm->_sn_tx_best_effort = _z_sn_increment(ztm->_sn_res, ztm->_sn_tx_best_effort);
    }
    return sn;
}

/**
 * This function is unsafe because it operates in potentially concurrent data.
 * Make sure that the following mutexes are locked before calling this function:
 *  - ztm->_mutex_inner
 */
static void __unsafe_z_raweth_prepare_header(_z_transport_multicast_t *ztm) {
    // Reserve space for ethernet header
    if (&ztm->_link._socket._raweth.has_vlan) {
        _z_wbuf_set_wpos(&ztm->_wbuf, sizeof(_zp_eth_vlan_header_t));
    } else {
        _z_wbuf_set_wpos(&ztm->_wbuf, sizeof(_zp_eth_header_t));
    }
}

/**
 * This function is unsafe because it operates in potentially concurrent data.
 * Make sure that the following mutexes are locked before calling this function:
 *  - ztm->_mutex_inner
 */
static int8_t __unsafe_z_raweth_write_header(_z_transport_multicast_t *ztm) {
    size_t len = _z_wbuf_len(&ztm->_wbuf);
    _z_raweth_socket_t *resocket = &ztm->_link._socket._raweth;

    // FIXME config function call to set the correct dmac & vlan value
    // Write eth header in buffer
    if (resocket->has_vlan) {
        _zp_eth_vlan_header_t header;
        memset(&header, 0, sizeof(header));
        memcpy(&header.dmac, &resocket->_dmac, _ZP_MAC_ADDR_LENGTH);
        memcpy(&header.smac, &resocket->_smac, _ZP_MAC_ADDR_LENGTH);
        header.tag.tpid = _ZP_ETH_TYPE_VLAN;
        header.tag.pcp = 0;  // Get from locator/config
        header.tag.vid = 0;  // Get from locator/config
        header.length = (uint16_t)len;
        // Save wpos
        size_t final_wpos = _z_wbuf_get_wpos(&ztm->_wbuf);
        // Write header
        _z_wbuf_set_wpos(&ztm->_wbuf, 0);
        _Z_RETURN_IF_ERR(_z_wbuf_write_bytes(&ztm->_wbuf, (uint8_t *)&header, 0, sizeof(header)));
        // Restore wpos
        _z_wbuf_set_wpos(&ztm->_wbuf, final_wpos);
    } else {
        _zp_eth_header_t header;
        memcpy(&header.dmac, &resocket->_dmac, _ZP_MAC_ADDR_LENGTH);
        memcpy(&header.smac, &resocket->_smac, _ZP_MAC_ADDR_LENGTH);
        header.length = (uint16_t)len;
        // Save wpos
        size_t final_wpos = _z_wbuf_get_wpos(&ztm->_wbuf);
        // Write header
        _z_wbuf_set_wpos(&ztm->_wbuf, 0);
        _Z_RETURN_IF_ERR(_z_wbuf_write_bytes(&ztm->_wbuf, (uint8_t *)&header, 0, sizeof(header)));
        // Restore wpos
        _z_wbuf_set_wpos(&ztm->_wbuf, final_wpos);
    }
    return _Z_RES_OK;
}

static int8_t _z_raweth_link_send_wbuf(const _z_transport_multicast_t *ztm) {
    int8_t ret = _Z_RES_OK;
    for (size_t i = 0; (i < _z_wbuf_len_iosli(&ztm->_wbuf)) && (ret == _Z_RES_OK); i++) {
        _z_bytes_t bs = _z_iosli_to_bytes(_z_wbuf_get_iosli(&ztm->_wbuf, i));
        size_t n = bs.len;

        do {
            // Retrieve addr from config + vlan tag above (locator)
            size_t wb = _z_send_raweth(&ztm->_link._socket._raweth._sock, bs.start, n);  // Unix
            if (wb == SIZE_MAX) {
                return _Z_ERR_TRANSPORT_TX_FAILED;
            }
            n = n - wb;
            bs.start = bs.start + (bs.len - n);
        } while (n > (size_t)0);
    }
    return ret;
}

// static void _z_raweth_check_config(_z_transport_multicast_t *ztm, const _z_keyexpr_t *key) {
//     // Check config
//     if (ztm->_link._socket._raweth._config != NULL) {
//         // Check key
//         if ((key != NULL)) {
//             // Get send info from config keyexpr mapping
//         } else {
//             // Get send info from config default values
//         }
//     } else {
//         // Nothing to do
//     }
// }

int8_t _z_raweth_send_t_msg(_z_transport_multicast_t *ztm, const _z_transport_message_t *t_msg) {
    int8_t ret = _Z_RES_OK;
    _Z_DEBUG(">> send session message\n");

#if Z_FEATURE_MULTI_THREAD == 1
    _z_mutex_lock(&ztm->_mutex_tx);
#endif

    // Reserve space for eth header
    __unsafe_z_raweth_prepare_header(ztm);
    // Encode the session message
    _Z_RETURN_IF_ERR(_z_transport_message_encode(&ztm->_wbuf, t_msg));
    // Write the message header
    _Z_RETURN_IF_ERR(__unsafe_z_raweth_write_header(ztm));
    // Send the wbuf on the socket
    _Z_RETURN_IF_ERR(_z_raweth_link_send_wbuf(ztm));
    // Mark the session that we have transmitted data
    ztm->_transmitted = true;

#if Z_FEATURE_MULTI_THREAD == 1
    _z_mutex_unlock(&ztm->_mutex_tx);
#endif

    return ret;
}

int8_t _z_raweth_send_n_msg(_z_session_t *zn, const _z_network_message_t *n_msg, z_reliability_t reliability,
                            z_congestion_control_t cong_ctrl) {
    int8_t ret = _Z_RES_OK;
    _z_transport_multicast_t *ztm = &zn->_tp._transport._raweth;
    _Z_DEBUG(">> send network message\n");

    // Acquire the lock and drop the message if needed
#if Z_FEATURE_MULTI_THREAD == 1
    if (cong_ctrl == Z_CONGESTION_CONTROL_BLOCK) {
        _z_mutex_lock(&ztm->_mutex_tx);
    } else {
        if (_z_mutex_trylock(&ztm->_mutex_tx) != (int8_t)0) {
            _Z_INFO("Dropping zenoh message because of congestion control\n");
            // We failed to acquire the lock, drop the message
            return ret;
        }
    }
#else
    _ZP_UNUSED(cong_ctrl);
#endif  // Z_FEATURE_MULTI_THREAD == 1

    // Reserve space for eth header
    __unsafe_z_raweth_prepare_header(ztm);
    // Set the frame header
    _z_zint_t sn = __unsafe_z_raweth_get_sn(ztm, reliability);
    _z_transport_message_t t_msg = _z_t_msg_make_frame_header(sn, reliability);
    // Encode the frame header
    _Z_RETURN_IF_ERR(_z_transport_message_encode(&ztm->_wbuf, &t_msg));
    // Encode the network message
    ret = _z_network_message_encode(&ztm->_wbuf, n_msg);
    if (ret == _Z_RES_OK) {
        // Write the eth header
        _Z_RETURN_IF_ERR(__unsafe_z_raweth_write_header(ztm));
        // Send the wbuf on the socket
        _Z_RETURN_IF_ERR(_z_raweth_link_send_wbuf(ztm));
        // Mark the session that we have transmitted data
        ztm->_transmitted = true;
    } else {  // The message does not fit in the current batch, let's fragment it
        // Create an expandable wbuf for fragmentation
        _z_wbuf_t fbf = _z_wbuf_make(ztm->_wbuf._capacity - 12, true);
        // Encode the message on the expandable wbuf
        _Z_RETURN_IF_ERR(_z_network_message_encode(&fbf, n_msg));
        // Fragment and send the message
        _Bool is_first = true;
        while (_z_wbuf_len(&fbf) > 0) {
            if (is_first) {
                // Get the fragment sequence number
                sn = __unsafe_z_raweth_get_sn(ztm, reliability);
            }
            is_first = false;
            // Reserve space for eth header
            __unsafe_z_raweth_prepare_header(ztm);
            // Serialize one fragment
            _Z_RETURN_IF_ERR(__unsafe_z_serialize_zenoh_fragment(&ztm->_wbuf, &fbf, reliability, sn));
            // Write the eth header
            _Z_RETURN_IF_ERR(__unsafe_z_raweth_write_header(ztm));
            // Send the wbuf on the socket
            _Z_RETURN_IF_ERR(_z_raweth_link_send_wbuf(ztm));
            // Mark the session that we have transmitted data
            ztm->_transmitted = true;
        }
    }
#if Z_FEATURE_MULTI_THREAD == 1
    _z_mutex_unlock(&ztm->_mutex_tx);
#endif  // Z_FEATURE_MULTI_THREAD == 1
    return ret;
}

#else
int8_t _z_raweth_send_t_msg(_z_transport_multicast_t *ztm, const _z_transport_message_t *t_msg) {
    _ZP_UNUSED(ztm);
    _ZP_UNUSED(t_msg);
    return _Z_ERR_TRANSPORT_NOT_AVAILABLE;
}

int8_t _z_raweth_send_n_msg(_z_session_t *zn, const _z_network_message_t *n_msg, z_reliability_t reliability,
                            z_congestion_control_t cong_ctrl) {
    _ZP_UNUSED(zn);
    _ZP_UNUSED(n_msg);
    _ZP_UNUSED(reliability);
    _ZP_UNUSED(cong_ctrl);
    return _Z_ERR_TRANSPORT_NOT_AVAILABLE;
}
#endif  // Z_FEATURE_RAWETH_TRANSPORT == 1