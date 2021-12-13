
#include "ble_listening_mode_handler.h"
#include "system_ble_prov.h"
#include "core_hal.h"
#include "system_error.h"
#include "logging.h"
#include "ble_control_request_channel.h"
#include "system_control_internal.h"
#include "check.h"

using namespace particle::system;

#if HAL_PLATFORM_BLE

int system_ble_prov_mode(bool enabled, void* reserved) {
    // TODO: Should this call be inside the if(enabled) block?
    if (!HAL_Feature_Get(FEATURE_DISABLE_LISTENING_MODE)) {
        LOG(TRACE, "Listening mode is not disabled. Cannot enter prov mode");
        return SYSTEM_ERROR_NOT_ALLOWED;
    }
    BleListeningModeHandler::instance()->setProvModeStatus(enabled);
    if (enabled) {
        LOG(TRACE, "Ble prov mode enabled");
        BleControlRequestChannel::instance(SystemControl::instance())->init();
        return BleListeningModeHandler::instance()->enter();
    } else {
        LOG(TRACE, "Ble prov mode disabled");
        // Should we void all of the UUIDs upon exiting the prov mode
        // The next time they enter prov mode, they might wanna use these?
        // if (previously advertising) -> figure this out somehow - think it's OK now
        // if (BleListeningModeHandler::instance()->getPreAdvertisingFlag()) {
        //     CHECK(BleListeningModeHandler::instance()->restoreUserConfigurations()); // exit and restore OR restore and exit?
        //     // I think restore and exit because after exiting it might start off with the other adv
        // }
        return BleListeningModeHandler::instance()->exit();
    }
}

bool system_get_ble_prov_status(void* reserved) {
    LOG(TRACE, "Ble prov mode status: %d", BleListeningModeHandler::instance()->getProvModeStatus());
    return BleListeningModeHandler::instance()->getProvModeStatus();
}

int system_set_provisioning_uuid(const char* serviceUuid, const char* txUuid, const char* rxUuid, void* reserved) {
    // set member variables to non null values
    // if invalid values entered by user, set them to defaults
    // how to store uuids so that it is shared by BLEControlRequestChannel
    if (!HAL_Feature_Get(FEATURE_DISABLE_LISTENING_MODE)) {
        LOG(TRACE, "Listening mode is not disabled. Cannot enter prov mode");
        return SYSTEM_ERROR_NOT_ALLOWED;
    }

    LOG(TRACE, "tp1 ble channel uuid: %d", BleControlRequestChannel::instance(SystemControl::instance())->getUuid());

    BleControlRequestChannel::instance(SystemControl::instance())->setUuid(5);

    LOG(TRACE, "tp2 ble channel uuid: %d", BleControlRequestChannel::instance(SystemControl::instance())->getUuid());

    return BleControlRequestChannel::instance(SystemControl::instance())->getUuid();
    //return SYSTEM_ERROR_NONE;
}

int system_set_prov_blah_me(const uint8_t* buf, size_t len, void* reserved) {
    for (unsigned i=0; i<len; i++) {
        LOG(TRACE, "blah[%u]: %d", i, buf[i]);
    }
    return SYSTEM_ERROR_NONE;
}

int system_set_prov_svc_uuid(const uint8_t* buf, size_t len, void* reserved) {
    if (!HAL_Feature_Get(FEATURE_DISABLE_LISTENING_MODE)) {
        LOG(TRACE, "Listening mode is not disabled. Cannot enter prov mode");
        return SYSTEM_ERROR_NOT_ALLOWED;
    }
    for (unsigned i=0; i<len; i++) {
        LOG(TRACE, "svc[%u]: %d", i, buf[i]);
    }
    if (BleControlRequestChannel::instance(SystemControl::instance())->getProfInitStatus()) {   // need to do this for all three at a time... how?
        LOG(TRACE, "Ble control req channel already initialized");
        return SYSTEM_ERROR_NOT_ALLOWED;
    }
    BleControlRequestChannel::instance(SystemControl::instance())->setProvSvcUuid(buf, len);
    // TODO: Store this in a relevant member variable
    return SYSTEM_ERROR_NONE;
}

int system_set_prov_tx_uuid(const uint8_t* buf, size_t len, void* reserved) {
    if (!HAL_Feature_Get(FEATURE_DISABLE_LISTENING_MODE)) {
        LOG(TRACE, "Listening mode is not disabled. Cannot enter prov mode");
        return SYSTEM_ERROR_NOT_ALLOWED;
    }
    for (unsigned i=0; i<len; i++) {
        LOG(TRACE, "tx[%u]: %d", i, buf[i]);
    }
    if (BleControlRequestChannel::instance(SystemControl::instance())->getProfInitStatus()) {
        LOG(TRACE, "Ble control req channel already initialized");
        return SYSTEM_ERROR_NOT_ALLOWED;
    }
    BleControlRequestChannel::instance(SystemControl::instance())->setProvTxUuid(buf, len);
    // TODO: Store this in a relevant member variable
    return SYSTEM_ERROR_NONE;
}

int system_set_prov_rx_uuid(const uint8_t* buf, size_t len, void* reserved) {
    if (!HAL_Feature_Get(FEATURE_DISABLE_LISTENING_MODE)) {
        LOG(TRACE, "Listening mode is not disabled. Cannot enter prov mode");
        return SYSTEM_ERROR_NOT_ALLOWED;
    }
    for (unsigned i=0; i<len; i++) {
        LOG(TRACE, "rx[%u]: %d", i, buf[i]);
    }
    if (BleControlRequestChannel::instance(SystemControl::instance())->getProfInitStatus()) {   // IMPORTANT: make it a single API call so that initialized_ is only called once
        LOG(TRACE, "Ble control req channel already initialized");
        return SYSTEM_ERROR_NOT_ALLOWED;
    }
    BleControlRequestChannel::instance(SystemControl::instance())->setProvRxUuid(buf, len);
    // TODO: Store this in a relevant member variable
    return SYSTEM_ERROR_NONE;   // FIXME: Return other errors
}

int system_set_prov_adv_svc_uuid(const uint8_t* buf, size_t len, void* reserved) {
    if (!HAL_Feature_Get(FEATURE_DISABLE_LISTENING_MODE)) {
        LOG(TRACE, "Listening mode is not disabled. Cannot enter prov mode");
        return SYSTEM_ERROR_NOT_ALLOWED;
    }
    for (unsigned i=0; i<len; i++) {
        LOG(TRACE, "advSvc[%u]: %d", i, buf[i]);
    }
    BleListeningModeHandler::instance()->setCtrlSvcUuid(buf, len);
    return SYSTEM_ERROR_NONE;
}

int system_clear_prov_adv_svc_uuid(size_t len, void* reserved) {
    BleListeningModeHandler::instance()->clearCtrlSvcUuid(len);
    return SYSTEM_ERROR_NONE;
}

#endif // HAL_PLATFORM_BLE