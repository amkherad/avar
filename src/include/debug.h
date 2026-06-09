#ifndef AVAR_DEBUG
#define AVAR_DEBUG

// #ifdef DEBUG

#define AVAR_TRACE

char *mg_event_message(int error) {
    switch (error) {
        case MG_EV_ERROR: return "Error";
        case MG_EV_OPEN: return "Connection created";
        case MG_EV_POLL: return "mg_mgr_poll iteration";
        case MG_EV_RESOLVE: return "Host name is resolved";
        case MG_EV_CONNECT: return "Connection established";
        case MG_EV_ACCEPT: return "Connection accepted";
        case MG_EV_TLS_HS: return "TLS handshake succeeded";
        case MG_EV_READ: return "Data received from socket";
        case MG_EV_WRITE: return "Data written to socket";
        case MG_EV_CLOSE: return "Connection closed";
        case MG_EV_HTTP_HDRS: return "HTTP headers";
        case MG_EV_HTTP_MSG: return "Full HTTP request/response";
        case MG_EV_WS_OPEN: return "Websocket handshake done";
        case MG_EV_WS_MSG: return "Websocket msg, text or bin";
        case MG_EV_WS_CTL: return "Websocket control msg";
        case MG_EV_MQTT_CMD: return "MQTT low-level command";
        case MG_EV_MQTT_MSG: return "MQTT PUBLISH received";
        case MG_EV_MQTT_OPEN: return "MQTT CONNACK received";
        case MG_EV_SNTP_TIME: return "SNTP time received";
        case MG_EV_WAKEUP: return "mg_wakeup() data received";
        case MG_EV_MDNS_REQ: return "mDNS request";
        case MG_EV_MDNS_RESP: return "mDNS response";
        case MG_EV_MODBUS_REQ: return "ModBus request";
        case MG_EV_USER: return "Starting ID for user events";
    }
}

#endif

// #endif
