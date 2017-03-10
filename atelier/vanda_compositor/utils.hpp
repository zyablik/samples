#pragma once
#include <string>
#include <sstream>
#include <seppo/protocol/MessageType.hpp>
#include <vanda/AbstractTextureConnection.hpp>

std::string vandaUserData2str(uint32_t * userData);
std::string vandaAbstractTextureConnectionType2str(vanda::AbstractTextureConnection::Type::Enum type);
std::string vandaMessageFactoryMessage2str(char * messageDataPointer);
std::string vandaMessageFactoryVolumeControlEnum2str(vanda::MessageFactory::VolumeControl::Enum type);

std::string seppoParcelMessage2str(seppo::ParcelMessage& msg) {
    std::stringstream str;
    str << "seppo msg sender = " << msg.senderId() << " id = " << msg.messageId() << " size = " << msg.messageDataByteSize();
    seppo::MessageType::Enum type = msg.type();
    switch(type) {
        case seppo::MessageType::Client_RequestShell:    str << " type = Client_RequestShell";    break;
        case seppo::MessageType::Server_ShellConnecting: str << " type = Server_ShellConnecting"; break;
        case seppo::MessageType::Client_ShellOpened:     str << " type = Client_ShellOpened";     break;
        case seppo::MessageType::Server_ShellOpened:     str << " type = Server_ShellOpened";     break;
        case seppo::MessageType::Server_ShellClose:      str << " type = Server_ShellClose";      break;
        case seppo::MessageType::Client_ShellClosed:     str << " type = Client_ShellClosed";     break;
        case seppo::MessageType::Parcel_Command:
            str << " type = Parcel_Command vanda msg: " << vandaMessageFactoryMessage2str(msg.messageDataPointer()); break;
        case seppo::MessageType::Parcel_CommandResult:
            str << " type = Parcel_CommandResult vanda msg: " << vandaMessageFactoryMessage2str(msg.messageDataPointer()); break;
        case seppo::MessageType::Parcel_Array:           str << " type = Parcel_Array";           break;
        case seppo::MessageType::Shell_EndOfMessages:    str << " type = Shell_EndOfMessages";    break;
        default:
            str << " UnknownSeppoMessageTypeEnum(" << (int)type << ")";
    }
    return str.str();
}

std::string vandaMessageFactoryMessage2str(char * messageDataPointer) {
    uint32_t * data_int = reinterpret_cast<uint32_t *>(messageDataPointer);
    float * data_float = reinterpret_cast<float*>(messageDataPointer);
    vanda::MessageFactory::MessageType::Enum type = (vanda::MessageFactory::MessageType::Enum) data_int[0];
    std::stringstream str;
    switch(type) {
        case vanda::MessageFactory::MessageType::Client_VolumeOpen:
            str << "Client_VolumeOpen x = " << data_float[1] << " y = " << data_float[2] << " z = " << data_float[3] ;    break;
        case vanda::MessageFactory::MessageType::Client_VolumeClose:                str << "Client_VolumeClose";    break;
        case vanda::MessageFactory::MessageType::Server_VolumeClose:                str << "Server_VolumeClose";    break;
        case vanda::MessageFactory::MessageType::Client_Input:                      str << "Client_Input";    break;
        case vanda::MessageFactory::MessageType::Server_Input:                      str << "Server_Input";    break;
        case vanda::MessageFactory::MessageType::Server_Update:                     str << "Server_Update";    break;
        case vanda::MessageFactory::MessageType::Server_UserData:
            str << "Server_UserData: " << vandaUserData2str(&data_int[1]); break;
        case vanda::MessageFactory::MessageType::Client_UpdateRenderObject:         str << "Client_UpdateRenderObject|VANDA_RENDER_MESSAGE";    break;
        case vanda::MessageFactory::MessageType::Client_UpdateTransformationMatrix: str << "Client_UpdateTransformationMatrix|VANDA_RENDER_MESSAGE";    break;
        case vanda::MessageFactory::MessageType::Client_UpdateMesh:
            str << "Client_UpdateMesh|VANDA_RENDER_MESSAGE id = " << data_int[1];
            break;
        case vanda::MessageFactory::MessageType::Client_UpdateTexture:
            str << "Client_UpdateTexture|VANDA_RENDER_MESSAGE id = " << data_int[1] << " type = "
                << vandaAbstractTextureConnectionType2str((vanda::AbstractTextureConnection::Type::Enum)data_int[2]);
            break;
        case vanda::MessageFactory::MessageType::Client_UpdateScissor:              str << "Client_UpdateScissor|VANDA_RENDER_MESSAGE";    break;
        case vanda::MessageFactory::MessageType::Client_UserData:
            str << "Client_UserData: " << vandaUserData2str(&data_int[1]); break;
        case vanda::MessageFactory::MessageType::Client_RemoveTexture:              str << "Client_RemoveTexture|VANDA_RENDER_MESSAGE";    break;
        case vanda::MessageFactory::MessageType::Client_RemoveMesh:                 str << "Client_RemoveMesh|VANDA_RENDER_MESSAGE";    break;
        case vanda::MessageFactory::MessageType::Client_RemoveTransformationMatrix: str << "Client_RemoveTransformationMatrix|VANDA_RENDER_MESSAGE";    break;
        case vanda::MessageFactory::MessageType::Client_RemoveScissor:              str << "Client_RemoveScissor|VANDA_RENDER_MESSAGE";    break;
        case vanda::MessageFactory::MessageType::Client_PackedMessages:             str << "Client_PackedMessages|VANDA_RENDER_MESSAGE";    break;
        case vanda::MessageFactory::MessageType::Client_BoundingVolumeOffset:       str << "Client_BoundingVolumeOffset";    break;
        case vanda::MessageFactory::MessageType::Client_VolumeControl:
            str << "Client_VolumeControl: " << vandaMessageFactoryVolumeControlEnum2str((vanda::MessageFactory::VolumeControl::Enum)data_int[1]); break;
        case vanda::MessageFactory::MessageType::Client_DataUpdated:                str << "Client_DataUpdated";    break;
        case vanda::MessageFactory::MessageType::Client_MessagesEnd:                str << "Client_MessagesEnd";    break;
        case vanda::MessageFactory::MessageType::Client_UpdateFragmentShader:       str << "Client_UpdateFragmentShader|VANDA_RENDER_MESSAGE";    break;
        case vanda::MessageFactory::MessageType::Client_UpdateUniforms:             str << "Client_UpdateUniforms|VANDA_RENDER_MESSAGE";    break;
        case vanda::MessageFactory::MessageType::Client_RemoveFragmentShader:       str << "Client_RemoveFragmentShader|VANDA_RENDER_MESSAGE";    break;
        case vanda::MessageFactory::MessageType::Server_Resize:
            str << "Server_Resize xRes = " << data_float[1] << " yRes = " << data_float[2] << " orient = " << reinterpret_cast<int*>(data_float + 4)[0]; break;
        case vanda::MessageFactory::MessageType::Client_LockOrientation:            str << "Client_LockOrientation|VANDA_RENDER_MESSAGE";    break;
        case vanda::MessageFactory::MessageType::Client_OrientationAck:             str << "Client_OrientationAck|VANDA_RENDER_MESSAGE";    break;
        default:
            str << " UnknownVandaFactoryMessageMessageType(" << (int)type << ")";
    }
    return str.str();
}

std::string vandaUserData2str(uint32_t * userData) {
    vanda::UserData::USER_DATA_TYPE type = (vanda::UserData::USER_DATA_TYPE)userData[0];
    std::stringstream str;
    switch(type) {
        case vanda::UserData::USER_DATA_FLUSH_CLIENT_MESSAGES: str << "USER_DATA_FLUSH_CLIENT_MESSAGES"; break;
        case vanda::UserData::USER_DATA_VKB_CONTROL: str << "USER_DATA_VKB_CONTROL"; break;
        case vanda::UserData::USER_DATA_ENABLE_BACK_BUTTON: str << "USER_DATA_ENABLE_BACK_BUTTON"; break;
        case vanda::UserData::USER_DATA_WIFI_STATUS_MESSAGE: str << "USER_DATA_WIFI_STATUS_MESSAGE"; break;
        case vanda::UserData::USER_DATA_CELLULAR_STATE_CHANGE: str << "USER_DATA_CELLULAR_STATE_CHANGE"; break;
        case vanda::UserData::USER_DATA_NETWORK_INFO: str << "USER_DATA_NETWORK_INFO"; break;
        case vanda::UserData::USER_DATA_CHAT_NOTIFICATION: str << "USER_DATA_CHAT_NOTIFICATION"; break;
        case vanda::UserData::USER_DATA_CHAT_NOTIFICATION_CANCEL: str << "USER_DATA_CHAT_NOTIFICATION_CANCEL"; break;
        case vanda::UserData::USER_DATA_GENERIC_NOTIFICATION: str << "USER_DATA_GENERIC_NOTIFICATION"; break;
        case vanda::UserData::USER_DATA_GENERIC_NOTIFICATION_CANCEL: str << "USER_DATA_GENERIC_NOTIFICATION_CANCEL"; break;
        case vanda::UserData::USER_DATA_BATTERY_CHARGING: str << "USER_DATA_BATTERY_CHARGING"; break;
        case vanda::UserData::USER_DATA_BATTERY_LEVEL: str << "USER_DATA_BATTERY_LEVEL"; break;
        case vanda::UserData::USER_DATA_FLIGHT_MODE: str << "USER_DATA_FLIGHT_MODE"; break;
        case vanda::UserData::USER_DATA_NFC_STATE: str << "USER_DATA_NFC_STATE"; break;
        case vanda::UserData::USER_DATA_BLUETOOTH_STATE: str << "USER_DATA_BLUETOOTH_STATE"; break;
        case vanda::UserData::USER_DATA_BRIGHTNESS: str << "USER_DATA_BRIGHTNESS"; break;
        case vanda::UserData::USER_DATA_REGISTER_POWERKEY_LISTENER: str << "USER_DATA_REGISTER_POWERKEY_LISTENER"; break;
        case vanda::UserData::USER_DATA_DEREGISTER_POWERKEY_LISTENER: str << "USER_DATA_DEREGISTER_POWERKEY_LISTENER"; break;
        case vanda::UserData::USER_DATA_REGISTER_INPUT_ACTIVITY_LISTENER: str << "USER_DATA_REGISTER_INPUT_ACTIVITY_LISTENER"; break;
        case vanda::UserData::USER_DATA_DEREGISTER_INPUT_ACTIVITY_LISTENER: str << "USER_DATA_DEREGISTER_INPUT_ACTIVITY_LISTENER"; break;
        case vanda::UserData::USER_DATA_REGISTER_NAVIGATION_BAR_LISTENER: str << "USER_DATA_REGISTER_NAVIGATION_BAR_LISTENER"; break;
        case vanda::UserData::USER_DATA_DEREGISTER_NAVIGATION_BAR_LISTENER: str << "USER_DATA_DEREGISTER_NAVIGATION_BAR_LISTENER"; break;
        case vanda::UserData::USER_DATA_NOTIFY_SERVICE: str << "USER_DATA_NOTIFY_SERVICE"; break;
        case vanda::UserData::USER_DATA_WEB_APP_SIZE: str << "USER_DATA_WEB_APP_SIZE"; break;
        case vanda::UserData::USER_DATA_ICON_INDICATOR_CHANGE: str << "USER_DATA_ICON_INDICATOR_CHANGE"; break;
        case vanda::UserData::USER_DATA_DISABLE_HOME_BUTTON: str << "USER_DATA_DISABLE_HOME_BUTTON"; break;
        case vanda::UserData::USER_DATA_ENABLE_HOME_BUTTON: str << "USER_DATA_ENABLE_HOME_BUTTON"; break;
        case vanda::UserData::USER_DATA_DISABLE_INFO_BUTTON: str << "USER_DATA_DISABLE_INFO_BUTTON"; break;
        case vanda::UserData::USER_DATA_ENABLE_INFO_BUTTON: str << "USER_DATA_ENABLE_INFO_BUTTON"; break;
        case vanda::UserData::USER_DATA_DISABLE_NAVIGATION_BAR: str << "USER_DATA_DISABLE_NAVIGATION_BAR"; break;
        case vanda::UserData::USER_DATA_ENABLE_NAVIGATION_BAR: str << "USER_DATA_ENABLE_NAVIGATION_BAR"; break;
        case vanda::UserData::USER_DATA_DISABLE_NOTIFICATION_APP: str << "USER_DATA_DISABLE_NOTIFICATION_APP"; break;
        case vanda::UserData::USER_DATA_ENABLE_NOTIFICATION_APP: str << "USER_DATA_ENABLE_NOTIFICATION_APP"; break;
        case vanda::UserData::USER_DATA_SHOW_NOTIFICATION_ICON: str << "USER_DATA_SHOW_NOTIFICATION_ICON"; break;
        case vanda::UserData::USER_DATA_HIDE_NOTIFICATION_ICON: str << "USER_DATA_HIDE_NOTIFICATION_ICON"; break;
        case vanda::UserData::USER_DATA_HIDE_ALL_NOTIFICATION_ICONS: str << "USER_DATA_HIDE_ALL_NOTIFICATION_ICONS"; break;
        case vanda::UserData::USER_DATA_DISABLE_STATUS_BAR: str << "USER_DATA_DISABLE_STATUS_BAR"; break;
        case vanda::UserData::USER_DATA_ENABLE_STATUS_BAR: str << "USER_DATA_ENABLE_STATUS_BAR"; break;
        case vanda::UserData::USER_DATA_SCREEN_LOCK_MSG: str << "USER_DATA_SCREEN_LOCK_MSG"; break;
        case vanda::UserData::USER_DATA_PHONE_CALL_MSG: str << "USER_DATA_PHONE_CALL_MSG"; break;
        case vanda::UserData::USER_DATA_DATA_CONNECTION_MESSAGE: str << "USER_DATA_DATA_CONNECTION_MESSAGE"; break;
        case vanda::UserData::USER_DATA_UPDATE_TIME: str << "USER_DATA_UPDATE_TIME"; break;
        case vanda::UserData::USER_DATA_SCREENSHOT: str << "USER_DATA_SCREENSHOT"; break;
        case vanda::UserData::USER_DATA_ORIENTATION_CHANGED: str << "USER_DATA_ORIENTATION_CHANGED"; break;
        case vanda::UserData::USER_DATA_TOAST: str << "USER_DATA_TOAST"; break;
        case vanda::UserData::USER_DATA_TOAST_WITH_DURATION: str << "USER_DATA_TOAST_WITH_DURATION"; break;
        case vanda::UserData::USER_DATA_SIM_STATE_CHANGE: str << "USER_DATA_SIM_STATE_CHANGE"; break;
        case vanda::UserData::USER_DATA_HEADPHONE_STATE: str << "USER_DATA_HEADPHONE_STATE"; break;
        default:
            str << " UnknownVandaUserDataType(" << (int)type << ")";
    }
    return str.str();
}

std::string vandaAbstractTextureConnectionType2str(vanda::AbstractTextureConnection::Type::Enum type) {
    std::stringstream str;
    switch(type) {
        case vanda::AbstractTextureConnection::Type::SharedMemory: str << "SharedMemory"; break;
        case vanda::AbstractTextureConnection::Type::EGLImage: str << "EGLImage"; break;
        case vanda::AbstractTextureConnection::Type::BufferQueue: str << "BufferQueue"; break;
        case vanda::AbstractTextureConnection::Type::HWLayer: str << "HWLayer"; break;
        default:
            str << " UnknownVandaAbstractTextureConnectionType(" << (int)type << ")";
    }
    return str.str();
}

std::string vandaMessageFactoryVolumeControlEnum2str(vanda::MessageFactory::VolumeControl::Enum type) {
    std::stringstream str;
    switch(type) {
        case vanda::MessageFactory::VolumeControl::Show: str << "Show"; break;
        case vanda::MessageFactory::VolumeControl::Hide: str << "Hide"; break;
        case vanda::MessageFactory::VolumeControl::Resume: str << "Resume"; break;
        default:
            str << " UnknownVandaMessageFactoryVolumeControlEnum(" << (int)type << ")";
    }
    return str.str();
}
