#pragma once

#include <cstdint>

// Field offsets verified for fingerprinted client 20260323.14 only.
// Hook-specific schema and disassembly evidence remains beside each use.
// These are byte displacements, not RVAs. Equal numbers in different groups
// describe different objects; do not merge them into a generic shared offset.
// The cast at each use supplies the measured field width/reference type.

namespace RR::Offsets::LocalTesting {
    // PlayerPresence fields, also inherited by HeartbeatResponseDTO.
    inline constexpr uintptr_t PlayerId = 0x10;
    inline constexpr uintptr_t PresenceRoomInstance = 0x28;
    inline constexpr uintptr_t IsOnline = 0x44;
    // RoomInstance fields; all three identifiers are signed Int64.
    inline constexpr uintptr_t InstanceId = 0x10;
    inline constexpr uintptr_t RoomId = 0x18;
    inline constexpr uintptr_t SubRoomId = 0x20;
}

namespace RR::Offsets::AccountModel {
    inline constexpr uintptr_t Relationship = 0x120;
    inline constexpr uintptr_t Id = 0x290;
}

namespace RR::Offsets::Relationship {
    inline constexpr uintptr_t Type = 0x14;
}

namespace RR::Offsets::FriendImport {
    inline constexpr uintptr_t SelectedAccounts = 0xD8;
    inline constexpr uintptr_t StaticFields = 0x20;
}

namespace RR::Offsets::IntList {
    inline constexpr uintptr_t Items = 0x10;
    inline constexpr uintptr_t Count = 0x18;
}

namespace RR::Offsets::ManagedArray {
    inline constexpr uintptr_t Length = 0x18;
    inline constexpr uintptr_t Data = 0x20;
}

namespace RR::Offsets::Delegate {
    inline constexpr uintptr_t MethodPointer = 0x10;
    inline constexpr uintptr_t InvokeImplementation = 0x18;
}

namespace RR::Offsets::MethodInfo {
    inline constexpr uintptr_t Body = 0x58;
    inline constexpr uintptr_t ParameterCount = 0x21;
}

namespace RR::Offsets::ScreenShare {
    inline constexpr uintptr_t PrimaryPreset = 0x80;
    inline constexpr uintptr_t SecondaryPreset = 0x88;
}

namespace RR::Offsets::ScreenSharePreset {
    inline constexpr uintptr_t FramesPerSecond = 0x10;
}

namespace RR::Offsets::CircuitRegistry {
    inline constexpr uintptr_t Declarations = 0x78;
}

namespace RR::Offsets::RoomieManager {
    inline constexpr uintptr_t ConversationAccess = 0x4A;
    inline constexpr uintptr_t CompanionAccess = 0x4C;
}

namespace RR::Offsets::RoomieTool {
    inline constexpr uintptr_t Controller = 0x80;
}

namespace RR::Offsets::RoomieController {
    inline constexpr uintptr_t Manager = 0x70;
}

namespace RR::Offsets::ByteArray {
    inline constexpr uintptr_t Length = 0x18;
    inline constexpr uintptr_t Data = 0x20;
}

namespace RR::Offsets::Tachyon {
    inline constexpr uintptr_t RsaProvider = 0x48;
}

namespace RR::Offsets::RsaClass {
    inline constexpr uintptr_t ImportXml = 0x208;
    inline constexpr uintptr_t ImportXmlMethod = 0x210;
}

namespace RR::Offsets::ConnectRoomState {
    inline constexpr uintptr_t TargetRoom = 0x30;
}

namespace RR::Offsets::RoomInstance {
    inline constexpr uintptr_t VoiceAddress = 0x40;
    inline constexpr uintptr_t VoiceServerId = 0x48;
}

namespace RR::Offsets::String {
    inline constexpr uintptr_t Length = 0x10;
    inline constexpr uintptr_t Characters = 0x14;
}

namespace RR::Offsets::HTTPRequest {
    inline constexpr uintptr_t Uri = 0x10;
    inline constexpr uintptr_t Response = 0x70;
}

namespace RR::Offsets::TlsAuthentication {
    inline constexpr uintptr_t TargetUri = 0x20;
}

namespace RR::Offsets::HTTPResponse {
    inline constexpr uintptr_t StatusCode = 0x18;
}
