#pragma once

#include <cstdint>
#include "../RR/Offsets.h"

namespace LocalTestPresence {
    inline int64_t Project(void* response, void* presence, int32_t playerId,
                           void (*writeBarrier)(void**, void*)) {
        if (!response || !presence || playerId <= 0 || !writeBarrier) {
            return 0;
        }
        auto bytes = static_cast<unsigned char*>(response);
        if (*reinterpret_cast<int32_t*>(bytes + RR::Offsets::LocalTesting::PlayerId) != playerId ||
            !bytes[RR::Offsets::LocalTesting::IsOnline]) {
            return 0;
        }
        auto local = *reinterpret_cast<unsigned char**>(static_cast<unsigned char*>(presence) +
                                                        RR::Offsets::LocalTesting::PresenceRoomInstance);
        auto server = reinterpret_cast<void**>(bytes + RR::Offsets::LocalTesting::PresenceRoomInstance);
        if (!local || !*server) {
            return 0;
        }
        const auto id = *reinterpret_cast<int64_t*>(local + RR::Offsets::LocalTesting::InstanceId);
        if ((id != -3 && id != -4 && id != -5) ||
            *reinterpret_cast<int64_t*>(local + RR::Offsets::LocalTesting::RoomId) <= 0 ||
            *reinterpret_cast<int64_t*>(local + RR::Offsets::LocalTesting::SubRoomId) <= 0) {
            return 0;
        }
        // IL2CPP's barrier marks the reference slot; assignment is the caller's job.
        *server = nullptr;
        writeBarrier(server, nullptr);
        return id;
    }
}
