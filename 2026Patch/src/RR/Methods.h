#pragma once

#include <cstdint>

// Verified RVAs and metadata slots for fingerprinted client 20260323.14 only.
// Unity and Referee groups use their named module bases; the remaining groups use GameAssembly.

namespace RR::Methods::Unity {
    // ApiTable holds runtime function pointers, not executable method bodies.
    // CallGateArena is the measured zero-filled executable region used by Gate.
    inline constexpr uintptr_t ApiTable = 0x1E9D890;
    inline constexpr uintptr_t CallGateArena = 0x1981840;
}

namespace RR::Methods::GameAssembly {
    inline constexpr uintptr_t ImageSize = 0x1158F000;
}

namespace RR::Methods::Il2Cpp {
    inline constexpr uintptr_t WriteBarrier = 0xC3F3E0;
    inline constexpr uintptr_t ByteArrayNew = 0xC3F460;
    inline constexpr uintptr_t ResolveType = 0xC400D0;
    inline constexpr uintptr_t InitializeClass = 0xC40430;
}

namespace RR::Methods::Metadata {
    // These are pointer slots resolved by IL2CPP, not functions to call. Resolve
    // them on the appropriate managed task before class initialization/allocation.
    inline constexpr uintptr_t RelationshipsTypeSlot = 0xFC81278;
    inline constexpr uintptr_t ByteArrayTypeSlot = 0xFCCC8F0;
    inline constexpr uintptr_t FriendImportTypeSlot = 0xFCF4A50;
    inline constexpr uintptr_t PromiseArrayTypeSlot = 0xFD14970;
    inline constexpr uintptr_t AllPromiseTypeSlot = 0xFCE6A38;
    inline constexpr uintptr_t ImportActionTypeSlot = 0xFDDA050;
    inline constexpr uintptr_t ImportActionMethodSlot = 0xFC1F310;
    inline constexpr uintptr_t ImportErrorActionTypeSlot = 0xFCAEAF8;
    inline constexpr uintptr_t ImportErrorMethodSlot = 0xFC1EFC0;
}

namespace RR::Methods::Uri {
    inline constexpr uintptr_t ToString = 0xAC4D810;
    inline constexpr uintptr_t Constructor = 0xAC4EAC0;
}

namespace RR::Methods::LocalTesting {
    // RecNet static methods: VPLUWCANMZA(), GUDYYSHFFKE(Int32),
    // WMGYGEVJQHF(HeartbeatResponseDTO), each with hidden MethodInfo.
    inline constexpr uintptr_t LocalAccountId = 0x9423480;
    inline constexpr uintptr_t PlayerPresence = 0x94F74E0;
    inline constexpr uintptr_t ApplyHeartbeat = 0x9500040;
}

namespace RR::Methods::Friends {
    inline constexpr uintptr_t SendRequest = 0x953F250;
    inline constexpr uintptr_t RequestWrapper = 0x2065C70;
    inline constexpr uintptr_t ToggleRelationship = 0x2456CC0;
    inline constexpr uintptr_t SendGestureRequest = 0x1E2E3C0;
    inline constexpr uintptr_t PartyGesture = 0x1E30980;
    inline constexpr uintptr_t ReceiveGestureRequest = 0x1E2DD90;
    inline constexpr uintptr_t OwnedLocally = 0x9AC8750;
    inline constexpr uintptr_t ObjectExists = 0xB415990;
    inline constexpr uintptr_t HandTransform = 0x1CC6C20;
    inline constexpr uintptr_t TransformPosition = 0xB43BEC0;
    inline constexpr uintptr_t NetworkPlayer = 0x19FEC20;
    inline constexpr uintptr_t ImportSelected = 0x26EAE90;
    inline constexpr uintptr_t ImportCleanup = 0x26EC320;
    inline constexpr uintptr_t ImportError = 0x26EC1D0;
    inline constexpr uintptr_t ImportIsSelected = 0x26EB5B0;
    inline constexpr uintptr_t ImportToggleSelected = 0x26EBCB0;
    inline constexpr uintptr_t ImportEventInvoke = 0x54E7FA0;
    inline constexpr uintptr_t ActionConstructor = 0x148B9E0;
    inline constexpr uintptr_t ErrorActionConstructor = 0x10E7D50;
    inline constexpr uintptr_t AllPromiseConstructor = 0x9DA2470;
    inline constexpr uintptr_t PromiseThen = 0x9DA3190;
    inline constexpr uintptr_t PromiseError = 0x9DA3020;
}

namespace RR::Methods::DesktopCapture {
    inline constexpr uintptr_t Bounds = 0x1A034E0;
    inline constexpr uintptr_t Capture = 0x1A03670;
    inline constexpr uintptr_t ReceiveFrame = 0x16CA010;   // NUCNWALGMIP(Byte[]); diagnostic only.
    inline constexpr uintptr_t SendChunk = 0x16C97B0;      // JDHGUYZICKL(); native packet dispatch.
    inline constexpr uintptr_t SerializeArray = 0x9AD4260; // RYVCIIOUZLU.DIJQDPKNCFT.
    // BC8640 is the boxed adjustor thunk; use the measured unboxed three-register body.
    inline constexpr uintptr_t WriterReserve = 0x3D55BC0;
    inline constexpr uintptr_t Update = 0x16CAFE0;
}

namespace RR::Methods::TestCases {
    inline constexpr uintptr_t ClearCasesAndRuns = 0x9569FE0;
    inline constexpr uintptr_t InvalidatePlans = 0x956AA00;
}

namespace RR::Methods::HTTP {
    inline constexpr uintptr_t SendRequest = 0x8FA14B0;
    inline constexpr uintptr_t NotifyServerCertificate = 0x8FA51E0;
    inline constexpr uintptr_t CallCallback = 0x8FAADC0;
}

namespace RR::Methods::Images {
    inline constexpr uintptr_t Verify = 0x949C580;
}

namespace RR::Methods::Netcode {
    inline constexpr uintptr_t SetOffline = 0x9A95950;
}

namespace RR::Methods::Tachyon {
    inline constexpr uintptr_t Constructor = 0x9AA3DD0;
    inline constexpr uintptr_t ConnectRoom = 0x9AA42B0;
    inline constexpr uintptr_t Connected = 0x9AA2CE0;
    inline constexpr uintptr_t Disconnected = 0x9AA3390;
    inline constexpr uintptr_t Failure = 0x9AA3470;
}

namespace RR::Methods::Session {
    inline constexpr uintptr_t FatalQuit = 0x2056EE0;
    inline constexpr uintptr_t ModuleScanDetected = 0x203C270;
}

namespace RR::Methods::Circuits {
    inline constexpr uintptr_t RemoveFunction = 0x114FEB0;
}

namespace RR::Methods::Roomie {
    inline constexpr uintptr_t IsInNoCreditMode = 0x14C9290;
    inline constexpr uintptr_t HasNoEnergy = 0x2778370;
    inline constexpr uintptr_t WheelAccess = 0x127D460;
    // Return site, not hook target: restrict the shared getter override to the
    // equipment wheel. The callback must remain noinline for _ReturnAddress.
    inline constexpr uintptr_t WheelAccessReturn = 0x1F99718;
}

namespace RR::Methods::Referee {
    // Injector bootstrap slot/instruction and diagnostic fault sites. Use the
    // Referee base, never the GameAssembly base or another client's address table.
    inline constexpr uintptr_t ArenaSlot = 0x6C10858;
    inline constexpr uintptr_t BootstrapInstruction = 0x69839D;
    inline constexpr uintptr_t ObservedFault1 = 0x62BB0B;
    inline constexpr uintptr_t ObservedFault2 = 0x216D035;
}
