#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#pragma comment(lib, "Ws2_32.lib")
#include <cstdint>
#include <cstring>
#include <string>
#include <algorithm>
#include <vector>
#include <map>
#include <mutex>
#include <intrin.h>
#include "../../Utils/DesktopCapture.h"
#include "../../Utils/LocalTestPresence.h"
#include "../../Utils/ScreenShareWire.h"
#include "../../Utils/Logging.h"
#include "../Config.h"
#include "../Methods.h"
#include "../Offsets.h"
#include "Patches.h"
#include "../../Utils/deps/minhook/include/MinHook.h"

namespace RR::Patches {

    using PatchLogging::Log;
    using PatchLogging::LogVerbose;
    using PatchLogging::verboseLogging;

    // This address table is exclusively for the fingerprinted 20260323.14 build.
    static uintptr_t game = 0, unity = 0;
    static unsigned gateCount = 0;
    static void (*removeCircuitFunctionOriginal)(void*, int32_t, void*) = nullptr;
    static RR::Config::Network networkConfig;
    static std::string tachyonServerId;
    static void* Gate(void* source) {
        // These zero-filled executable bytes were verified in the runtime Unity image.
        // Each 32-byte slot calls one GameAssembly target with Windows x64 shadow
        // space and restores the stack before returning. This stub forwards only
        // register arguments: do not route signatures with stack arguments through
        // it. A used/nonzero slot or exhausted arena fails instead of overwriting
        // Unity code. Startup and lazy RSA import share this arena; the allocator
        // is not synchronized. Parallelizing gate creation requires a separate
        // synchronization/ABI review, not merely increasing the slot limit.
        if (gateCount >= 60) {
            return nullptr;
        }
        auto gate =
            reinterpret_cast<unsigned char*>(unity + RR::Methods::Unity::CallGateArena + 32 * gateCount++);
        for (int i = 0; i < 32; ++i) {
            if (gate[i]) {
                return nullptr;
            }
        }
        unsigned char code[22] = {0x48, 0x83, 0xEC, 0x28, 0x48, 0xB8, 0,    0,    0,    0,   0,
                                  0,    0,    0,    0xFF, 0xD0, 0x48, 0x83, 0xC4, 0x28, 0xC3};
        memcpy(code + 6, &source, 8);
        DWORD old = 0, ignored = 0;
        if (!VirtualProtect(gate, 32, PAGE_EXECUTE_READWRITE, &old)) {
            return nullptr;
        }
        memcpy(gate, code, sizeof(code));
        FlushInstructionCache(GetCurrentProcess(), gate, sizeof(code));
        VirtualProtect(gate, 32, old, &ignored);
        return gate;
    }
    static void* Api(unsigned slot) {
        // Unity's table can exist before its IL2CPP entries are populated. Only
        // create a gate for a pointer inside the verified GameAssembly image;
        // the startup wait below is bounded and does not authorize other modules.
        auto source = reinterpret_cast<void**>(unity + RR::Methods::Unity::ApiTable)[slot];
        const uintptr_t address = reinterpret_cast<uintptr_t>(source);
        return address >= game && address < game + RR::Methods::GameAssembly::ImageSize ? Gate(source)
                                                                                        : nullptr;
    }
    static void* (*stringNew)(const char*) = nullptr;
    static void* (*objectClass)(void*) = nullptr;
    static void* (*objectNew)(void*) = nullptr;
    static DesktopCapture::Bounds* (*desktopBounds)(DesktopCapture::Bounds*, int, void*) = nullptr;
    static void* (*byteArrayNew)(void*, size_t) = nullptr;
    static void* byteArrayClass = nullptr;
    static void (*resolveCaptureType)(void**) = nullptr;
    static void (*initializeClass)(void*) = nullptr;
    static void (*writeBarrier)(void**, void*) = nullptr;
    static int32_t (*localAccountId)(void*) = nullptr;
    static void* (*localPlayerPresence)(int32_t, void*) = nullptr;
    static void (*heartbeatOriginal)(void*, void*) = nullptr;
    static void (*heartbeatInvoke)(void*, void*) = nullptr;

    static void ApplyHeartbeat(void* response, void* method) {
        // Fingerprinted 20260323.14 WMGYGEVJQHF at 9500040 checks a local negative
        // instance against a NULL server room (95001FB..9500228). A server-backed
        // dorm returned while locally testing cancels the load or ejects the player.
        // Project this field for local Maker Room (-3), Maker Room 2 (-4), or Studio (-5).
        // Keep normal online presence, account checks and all other modes unchanged.
        __try {
            if (response && localAccountId && localPlayerPresence && writeBarrier) {
                const int32_t playerId = localAccountId(nullptr);
                if (playerId > 0) {
                    const auto id = LocalTestPresence::Project(
                        response, localPlayerPresence(playerId, nullptr), playerId, writeBarrier);
                    static LONG reported = 0;
                    if (id && InterlockedCompareExchange(&reported, 1, 0) == 0) {
                        Log("[Studio] projected heartbeat for local test instance %lld", id);
                    }
                }
            }
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            static LONG reported = 0;
            if (InterlockedCompareExchange(&reported, 1, 0) == 0) {
                Log("[Studio] local test heartbeat projection fault; continuing native handling");
            }
        }
        // Keep native handling outside the override's fault guard.
        heartbeatInvoke(response, method);
    }
    static void* (*sendFriendRequest)(int32_t, int32_t, void*) = nullptr;
    static void* (*friendRequestOriginal)(void*, int32_t, int32_t, bool, void*) = nullptr;
    static void* FriendRequest(void* instance, int32_t playerId, int32_t source, bool showFeedback,
                               void* method) {
        // 20260323.14 live schema + read-only runtime verification: YJKLVDJUOMK
        // WFCTJYRUBPY (2065C70) unconditionally displays "Friend requests are
        // disabled." and returns a rejected promise, ignoring all three inputs.
        // RecNet.Relationships.ZDBIOZVXKET (953F250) still sends the ordinary
        // request and updates the relationship cache through its native promise.
        // RRUI.TrySendOrAcceptFriendRequest retains pending/friend checks and its
        // acceptance path through FKBZEVTMEAM (2065B40 -> 953CCD0).
        // Use the acceptance wrapper's type slot/init; the caller owns feedback.
        if (!instance || playerId <= 0) {
            return friendRequestOriginal(instance, playerId, source, showFeedback, method);
        }
        __try {
            auto slot = reinterpret_cast<void**>(game + RR::Methods::Metadata::RelationshipsTypeSlot);
            resolveCaptureType(slot);
            initializeClass(*slot);
            LogVerbose("[Friends] dispatching native friend request source=%d", source);
            return sendFriendRequest(playerId, source, nullptr);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            Log("[Friends] native request fault; retaining original failure response");
        }
        return friendRequestOriginal(instance, playerId, source, showFeedback, method);
    }
    static void (*importSelectedOriginal)(void*, void*) = nullptr;
    static void (*importCleanup)(void*, void*) = nullptr;
    static void (*importError)(void*, void*, void*) = nullptr;
    static bool (*importIsSelected)(void*, int32_t, void*) = nullptr;
    static void (*importToggleSelected)(void*, int32_t, void*) = nullptr;
    static void (*importEventInvoke)(void*, int32_t, void*) = nullptr;
    static void (*importActionConstructor)(void*, void*, void*, void*) = nullptr;
    static void (*importErrorActionConstructor)(void*, void*, void*, void*) = nullptr;
    static void (*allPromiseConstructor)(void*, void*, void*) = nullptr;
    static void* (*promiseThen)(void*, void*, void*) = nullptr;
    static void* (*promiseError)(void*, void*, void*) = nullptr;
    static void* (*classMethods)(void*, void**) = nullptr;
    static const char* (*methodName)(void*) = nullptr;
    static std::mutex importLock;
    // The native promise delegates retain the model. This map contains only
    // the selected IDs/count snapshot; it owns no unrooted managed references.
    static std::map<void*, std::vector<int32_t>> pendingImports;

    static void* ImportMetadata(uintptr_t offset) {
        auto slot = reinterpret_cast<void**>(game + offset);
        resolveCaptureType(slot);
        return *slot;
    }
    static void ImportSucceededWork(void* model) {
        std::vector<int32_t> ids;
        {
            std::lock_guard<std::mutex> guard(importLock);
            auto pending = pendingImports.find(model);
            if (pending == pendingImports.end()) {
                return;
            }
            ids = std::move(pending->second);
            pendingImports.erase(pending);
        }
        // Remove only the dispatched selections, retaining any newly selected
        // accounts. The original toggle emits the model's change notification.
        for (auto id : ids) {
            if (importIsSelected(model, id, nullptr)) {
                importToggleSelected(model, id, nullptr);
            }
        }
        importCleanup(model, nullptr);
        auto klass = ImportMetadata(RR::Methods::Metadata::FriendImportTypeSlot);
        initializeClass(klass);
        auto fields = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(klass) +
                                                RR::Offsets::FriendImport::StaticFields);
        auto event = *reinterpret_cast<void**>(fields);
        void* iterator = nullptr;
        void* invoke = nullptr;
        while (auto candidate = classMethods(objectClass(event), &iterator)) {
            if (!strcmp(methodName(candidate), "Invoke") &&
                *reinterpret_cast<uintptr_t*>(reinterpret_cast<uintptr_t>(candidate) +
                                              RR::Offsets::MethodInfo::Body) ==
                    game + RR::Methods::Friends::ImportEventInvoke) {
                invoke = candidate;
                break;
            }
        }
        if (!invoke) {
            Log("[Friends] import completed but completion event method unavailable");
            return;
        }
        Log("[Friends] import requests completed count=%zu", ids.size());
        // OnFriendsImported is DQCNOGDGYBL<int>, not System.Action. Its
        // inflated Invoke reads MethodInfo's generic context, so retain it.
        importEventInvoke(event, static_cast<int32_t>(ids.size()), invoke);
    }
    static void ImportSucceeded(void* model, void*) {
        __try {
            ImportSucceededWork(model);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            Log("[Friends] import completion fault");
        }
    }
    static void ImportFailedWork(void* model, void* error) {
        {
            std::lock_guard<std::mutex> guard(importLock);
            pendingImports.erase(model);
        }
        Log("[Friends] import request failed; retaining native error feedback");
        importError(model, error, nullptr);
        importCleanup(model, nullptr);
    }
    static void ImportFailed(void* model, void* error, void*) {
        __try {
            ImportFailedWork(model, error);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            Log("[Friends] import error feedback fault");
        }
    }
    static void* ImportCallback(void* model, bool failure) {
        auto klass = ImportMetadata(failure ? RR::Methods::Metadata::ImportErrorActionTypeSlot
                                            : RR::Methods::Metadata::ImportActionTypeSlot);
        auto method = ImportMetadata(failure ? RR::Methods::Metadata::ImportErrorMethodSlot
                                             : RR::Methods::Metadata::ImportActionMethodSlot);
        auto methodAddress = reinterpret_cast<uintptr_t>(method);
        if (!method ||
            *reinterpret_cast<uintptr_t*>(methodAddress + RR::Offsets::MethodInfo::Body) !=
                game + (failure ? RR::Methods::Friends::ImportError : RR::Methods::Friends::ImportCleanup) ||
            *reinterpret_cast<uint8_t*>(methodAddress + RR::Offsets::MethodInfo::ParameterCount) !=
                (failure ? 1 : 0)) {
            Log("[Friends] import callback signature unavailable");
            return nullptr;
        }
        auto action = objectNew(klass);
        if (!action) {
            return nullptr;
        }
        // Native constructors keep the original Action/Action<string> ABI,
        // target and GC write barrier. Only this newly allocated delegate's
        // two code pointers are replaced; shared metadata remains untouched.
        if (failure) {
            importErrorActionConstructor(action, model, method, nullptr);
        } else {
            importActionConstructor(action, model, method, nullptr);
        }
        auto callback =
            failure ? reinterpret_cast<void*>(ImportFailed) : reinterpret_cast<void*>(ImportSucceeded);
        auto address = reinterpret_cast<uintptr_t>(action);
        *reinterpret_cast<void**>(address + RR::Offsets::Delegate::MethodPointer) = callback;
        *reinterpret_cast<void**>(address + RR::Offsets::Delegate::InvokeImplementation) = callback;
        return action;
    }
    static bool ImportSelectedWork(void* model, bool* dispatched) {
        auto selected = *reinterpret_cast<uintptr_t*>(reinterpret_cast<uintptr_t>(model) +
                                                      RR::Offsets::FriendImport::SelectedAccounts);
        if (!selected) {
            return false;
        }
        auto items = *reinterpret_cast<uintptr_t*>(selected + RR::Offsets::IntList::Items);
        auto count = *reinterpret_cast<int32_t*>(selected + RR::Offsets::IntList::Count);
        if (!items || count <= 0 ||
            static_cast<size_t>(count) >
                *reinterpret_cast<size_t*>(items + RR::Offsets::ManagedArray::Length)) {
            return false;
        }
        auto first = reinterpret_cast<int32_t*>(items + RR::Offsets::ManagedArray::Data);
        std::vector<int32_t> ids(first, first + count);
        if (std::any_of(ids.begin(), ids.end(), [](int32_t id) { return id <= 0; })) {
            return false;
        }
        auto success = ImportCallback(model, false);
        auto failure = ImportCallback(model, true);
        auto arrayType = ImportMetadata(RR::Methods::Metadata::PromiseArrayTypeSlot);
        auto allType = ImportMetadata(RR::Methods::Metadata::AllPromiseTypeSlot);
        auto promises = byteArrayNew(arrayType, ids.size());
        auto all = objectNew(allType);
        if (!success || !failure || !promises || !all) {
            return false;
        }
        auto relationships = ImportMetadata(RR::Methods::Metadata::RelationshipsTypeSlot);
        initializeClass(relationships);
        {
            std::lock_guard<std::mutex> guard(importLock);
            if (!pendingImports.emplace(model, ids).second) {
                LogVerbose("[Friends] import already pending");
                return true;
            }
        }
        *dispatched = true;
        Log("[Friends] import dispatching selected requests count=%zu", ids.size());
        for (size_t i = 0; i < ids.size(); ++i) {
            // FriendRequestSource.FailedPlatformFriendImport = 1. Use normal
            // requests rather than the direct bulk-add endpoint, which grants
            // friendship without an acceptance step. Cache updates stay native.
            auto request = sendFriendRequest(ids[i], 1, nullptr);
            if (!request) {
                ImportFailedWork(model, stringNew("Unable to send imported friend requests."));
                return true;
            }
            auto field = reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(promises) +
                                                  RR::Offsets::ManagedArray::Data + i * sizeof(void*));
            *field = request;
            writeBarrier(field, request);
        }
        // JYBCNXVQTDI joins NXPIPFBMDQY[]. Its inherited MRDAKWBRKMV.Then
        // runs only when every real request succeeds; Error keeps the native
        // import feedback. Both retain model targets until their callbacks run.
        allPromiseConstructor(all, promises, nullptr);
        promiseError(all, failure, nullptr);
        promiseThen(all, success, nullptr);
        return true;
    }
    static void ImportSelectedFriends(void* model, void* method) {
        // Both the NUX/sign-up and menu controllers use this FriendImportModel
        // action. 26EAE90 ignores selectedAccounts and only constructs the
        // disabled dialog (26EAF5D/26EB014), never calling RequestWrapper.
        bool dispatched = false;
        __try {
            if (model && ImportSelectedWork(model, &dispatched)) {
                return;
            }
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            Log("[Friends] import action fault dispatched=%d", dispatched);
            if (dispatched) {
                ImportFailed(model, stringNew("Unable to complete imported friend requests."), nullptr);
                return;
            }
        }
        importSelectedOriginal(model, method);
    }
    static void (*toggleRelationshipOriginal)(void*, void*) = nullptr;
    static void (*gestureFriendOriginal)(void*, void*, void*, bool, void*) = nullptr;
    static void (*receiveGestureFriend)(void*, void*, void*, bool) = nullptr;
    struct FriendGesturePosition {
        float x, y, z;
    };
    static_assert(sizeof(FriendGesturePosition) == 12, "Unity Vector3 ABI");
    static bool (*gestureOwnedLocally)(void*, void*) = nullptr;
    static bool (*gestureObjectExists)(void*, void*, void*) = nullptr;
    static void* (*gestureHandTransform)(void*, void*) = nullptr;
    static FriendGesturePosition* (*gestureTransformPosition)(FriendGesturePosition*, void*, void*) = nullptr;
    static void* (*gestureNetworkPlayer)(void*, void*) = nullptr;
    static void (*partyGestureOriginal)(void*, void*, void*, int32_t, void*, void*, bool, void*) = nullptr;
    static void PartyGesture(void* instance, void* firstHand, void* secondHand, int32_t gesture,
                             void* firstPlayer, void* secondPlayer, bool offerer, void* method) {
        // XBVGIVRYHFQ (1E30980) inlines SendFriendRequest for gesture 4 at
        // 1E30B7F..1E30CBE, so hooking SendFriendRequest alone never sees it.
        // Mirror this branch's local-owner/live-hand checks and midpoint exactly;
        // the already-completed gesture detection remains the trigger. All other
        // gestures retain their original function, including its stack arguments.
        if (gesture == 4 && instance && firstHand && secondHand && secondPlayer) {
            __try {
                LogVerbose("[Friends] detected handshake gesture offerer=%d", offerer);
                if (!gestureOwnedLocally(instance, nullptr)) {
                    return;
                }
                if (!gestureObjectExists(secondHand, nullptr, nullptr)) {
                    return;
                }
                auto firstTransform = gestureHandTransform(firstHand, nullptr);
                auto secondTransform = gestureHandTransform(secondHand, nullptr);
                auto player = gestureNetworkPlayer(secondPlayer, nullptr);
                if (firstTransform && secondTransform && player) {
                    FriendGesturePosition first{}, second{};
                    gestureTransformPosition(&first, firstTransform, nullptr);
                    gestureTransformPosition(&second, secondTransform, nullptr);
                    FriendGesturePosition midpoint{first.x + (second.x - first.x) * 0.5f,
                                                   first.y + (second.y - first.y) * 0.5f,
                                                   first.z + (second.z - first.z) * 0.5f};
                    LogVerbose("[Friends] detected handshake entering native restriction/gesture flow");
                    receiveGestureFriend(instance, player, &midpoint, offerer);
                    return;
                }
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                Log("[Friends] detected handshake fault; retaining original gesture response");
            }
        }
        partyGestureOriginal(instance, firstHand, secondHand, gesture, firstPlayer, secondPlayer, offerer,
                             method);
    }
    static void GestureFriendRequest(void* instance, void* player, void* position, bool offerer,
                                     void* method) {
        // Fingerprinted 20260323.14: PlayerParty.SendFriendRequest (1E2E3C0)
        // only allocates a closure and displays the disabled toast. Its surviving
        // RpcMakeFriendRequest (1E2DD90) checks IsPlayerCommunicationRestricted,
        // then AWMWPGJGDBW (1E3A6B0) checks network identity/local ownership and
        // invokes VLFGRJXQPOS (1E2FB40). That method preserves blocked/already-
        // friend checks, adds through Relationships.KMFKPYMSQLW (953D7F0), with
        // the native offerer/acceptor sources 2/3, and plays gesture feedback.
        // Also cover direct callers; the detector's inlined copy is hooked above.
        // The receive handler does not read its hidden MethodInfo argument; use
        // four arguments because Unity call gates do not forward stack arguments.
        if (instance && player && position) {
            __try {
                LogVerbose("[Friends] handshake entering native restriction/gesture flow offerer=%d",
                           offerer);
                receiveGestureFriend(instance, player, position, offerer);
                return;
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                Log("[Friends] handshake flow fault; retaining original failure response");
            }
        }
        gestureFriendOriginal(instance, player, position, offerer, method);
    }
    static void ToggleRelationship(void* instance, void* method) {
        // BaseAccountModel.ToggleRelationship (2456CC0) has a second, inlined
        // disabled-request dialog at 2457328, bypassing the 2065C70 wrapper.
        // Runtime schema places Relationship at +120 and account Id at +290.
        // Only replace the None/null branch; incoming requests (2), outgoing
        // requests (1), friends (3) and their confirmation dialogs stay native.
        __try {
            if (instance) {
                auto model = reinterpret_cast<uintptr_t>(instance);
                auto relationship =
                    *reinterpret_cast<uintptr_t*>(model + RR::Offsets::AccountModel::Relationship);
                auto id = *reinterpret_cast<int32_t*>(model + RR::Offsets::AccountModel::Id);
                if (id > 0 && (!relationship || !*reinterpret_cast<int32_t*>(
                                                    relationship + RR::Offsets::Relationship::Type))) {
                    auto slot = reinterpret_cast<void**>(game + RR::Methods::Metadata::RelationshipsTypeSlot);
                    resolveCaptureType(slot);
                    initializeClass(*slot);
                    LogVerbose("[Friends] profile dispatching native friend request");
                    // The native promise dispatches eagerly and its success delegate
                    // updates Relationships; normal RelationshipChanged notifications
                    // refresh this model. Never manufacture a successful relationship.
                    sendFriendRequest(id, 0, nullptr);
                    return;
                }
            }
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            Log("[Friends] profile request fault; retaining original action");
        }
        toggleRelationshipOriginal(instance, method);
    }
    static ULONG_PTR captureGdiToken = 0;
    static int screenShareFps = 0;
    static void (*desktopUpdateOriginal)(void*, void*) = nullptr;
    static void DesktopUpdate(void* instance, void* method) {
        // Live schema + 16CB710: both presets store a float Hz at +10. Retain
        // the existing adaptive multiplier and unsent-frame backpressure.
        if (screenShareFps && instance) {
            auto screen = reinterpret_cast<uintptr_t>(instance);
            for (auto offset :
                 {RR::Offsets::ScreenShare::PrimaryPreset, RR::Offsets::ScreenShare::SecondaryPreset}) {
                auto preset = *reinterpret_cast<uintptr_t*>(screen + offset);
                if (preset) {
                    *reinterpret_cast<float*>(preset + RR::Offsets::ScreenSharePreset::FramesPerSecond) =
                        static_cast<float>(screenShareFps);
                }
            }
        }
        desktopUpdateOriginal(instance, method);
    }
    static void RemoveCircuitFunction(void* instance, int32_t functionId, void* method) {
        // 20260323.14: NFXWIKIZYTG (114FEB0) removes a declaration from the
        // registry at +78. LifecycleDidInitialize (114E2C0, store at 114EA60)
        // builds that registry from the completed circuit state. Initial graph
        // construction can destroy temporary declarations before this callback;
        // there is no registered declaration to remove until the registry exists.
        // The archived Studio scene reaches this path in GAZDYIJZLZA and otherwise
        // aborts CV2 initialization, followed by PlayerLogicComponent failures.
        // Keep all initialized-state removals, and all their errors, unchanged.
        if (instance && !*reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(instance) +
                                                   RR::Offsets::CircuitRegistry::Declarations)) {
            LogVerbose("[Circuits] pre-initialization function removal skipped id=%d", functionId);
            return;
        }
        removeCircuitFunctionOriginal(instance, functionId, method);
    }
    static bool (*roomieHasNoEnergyOriginal)(void*, void*) = nullptr;
    static bool (*roomieIsInNoCreditMode)(void*, void*) = nullptr;
    static __declspec(noinline) bool RoomieWheelAccess(void* instance, void*) {
        // This getter is a shared IL2CPP body reading byte +4A. Preserve that
        // read for every caller except MRWEFGVREGP.AEBAWBHPOGT's equipment-wheel
        // check. Its call at 1F99713 dispatches through the tail-jumping 2350
        // interface helper, so the return address remains 1F99718. Unlike
        // PlayerBackpack.CanLocalPlayerEnableTool, this callback only checks
        // conversation access and silently ignores the supported no-credit path.
        const auto address = reinterpret_cast<uintptr_t>(instance);
        const bool access =
            instance && *reinterpret_cast<bool*>(address + RR::Offsets::RoomieManager::ConversationAccess);
        if (!access &&
            reinterpret_cast<uintptr_t>(_ReturnAddress()) == game + RR::Methods::Roomie::WheelAccessReturn &&
            instance && *reinterpret_cast<bool*>(address + RR::Offsets::RoomieManager::CompanionAccess) &&
            roomieIsInNoCreditMode(instance, nullptr)) {
            LogVerbose("[Roomie] equipment wheel accepted no-credit companion fallback");
            return true;
        }
        return access;
    }
    static bool RoomieHasNoEnergy(void* instance, void* method) {
        // Fingerprinted 20260323.14 schema: RoomieTool.roomieAIController +80,
        // RoomieAIController._roomieManager +70. RefreshActivities (2774C70)
        // installs activity 12 when HasNoEnergy is true; IsAllowedToMove
        // (1F8BE60) explicitly blocks that activity. Keep local activities usable
        // for the recognized no-credit fallback, without granting conversation
        // access or changing the energy DTO/session lifecycle.
        if (instance) {
            auto controller = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(instance) +
                                                        RR::Offsets::RoomieTool::Controller);
            auto manager = controller ? *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(controller) +
                                                                  RR::Offsets::RoomieController::Manager)
                                      : nullptr;
            if (manager && roomieIsInNoCreditMode(manager, nullptr)) {
                static LONG logged = 0;
                if (InterlockedCompareExchange(&logged, 1, 0) == 0) {
                    Log("[Roomie] no-credit companion activities enabled; conversation access unchanged");
                }
                return false;
            }
        }
        return roomieHasNoEnergyOriginal(instance, method);
    }
    static thread_local bool sendingScreenChunk = false;
    static int (*sendScreenChunkOriginal)(void*, void*) = nullptr;
    static void (*serializeArrayOriginal)(void*, void*, ScreenShareWire::WriterHandle*, void*) = nullptr;
    static bool (*writerReserve)(ScreenShareWire::WriterHandle**, int, void*) = nullptr;
    static LONG screenByteCode = 0;
    static int SendScreenChunk(void* instance, void* method) {
        // Scope the fast array path to the native screen-share packet dispatch.
        // Keep routing, delivery, fragmentation, counters and backpressure native.
        const bool previous = sendingScreenChunk;
        sendingScreenChunk = true;
        LARGE_INTEGER begin{}, end{};
        QueryPerformanceCounter(&begin);
        int result = 0;
        __try {
            result = sendScreenChunkOriginal(instance, method);
        } __finally {
            sendingScreenChunk = previous;
        }
        QueryPerformanceCounter(&end);
        static LONG count = 0;
        const LONG frame = InterlockedIncrement(&count);
        if (frame <= 3 || frame % 30 == 1) {
            LogVerbose("[DesktopCapture] send chunk=%ld serialize/dispatch=%.2fms thread=%lu", frame,
                       DesktopCapture::Milliseconds(begin, end), GetCurrentThreadId());
        }
        return result;
    }
    static void SerializeScreenArray(void* object, void* type, ScreenShareWire::WriterHandle* handle,
                                     void* method) {
        // 9AD4260 receives FastBufferWriter's eight-byte handle by value in R8.
        // Its byte-array path writes int32 count, one element-type code, then
        // obtains/boxes every byte through IList before invoking its byte writer.
        // The first native packet verifies the exact encoding before bulk copying
        // subsequent packets. All other arrays/callers retain the native path.
        if (!sendingScreenChunk || !byteArrayClass || !object ||
            *reinterpret_cast<void**>(object) != byteArrayClass || !handle) {
            serializeArrayOriginal(object, type, handle, method);
            return;
        }
        const auto array = reinterpret_cast<uintptr_t>(object);
        const uint64_t length = *reinterpret_cast<uint64_t*>(array + RR::Offsets::ByteArray::Length);
        const int size = ScreenShareWire::PayloadSize(length);
        auto bytes = reinterpret_cast<const unsigned char*>(array + RR::Offsets::ByteArray::Data);
        const LONG verified = InterlockedCompareExchange(&screenByteCode, 0, 0);
        if (size > 0 && verified > 0 && handle->position >= 0 && handle->position <= handle->maxCapacity &&
            size <= handle->maxCapacity - handle->position && writerReserve(&handle, size, nullptr) &&
            ScreenShareWire::Write(handle, bytes, static_cast<int>(length),
                                   static_cast<unsigned char>(verified - 1))) {
            return;
        }
        const int before = handle->position;
        LARGE_INTEGER begin{}, end{};
        QueryPerformanceCounter(&begin);
        serializeArrayOriginal(object, type, handle, method);
        QueryPerformanceCounter(&end);
        if (!verified && size > 0 && before >= 0 && handle->position >= before &&
            handle->position <= handle->capacity && handle->buffer &&
            ScreenShareWire::Matches(handle->buffer + before, handle->position - before, bytes,
                                     static_cast<int>(length))) {
            const unsigned char code = handle->buffer[before + 4];
            InterlockedCompareExchange(&screenByteCode, static_cast<LONG>(code) + 1, 0);
            Log("[DesktopCapture] native byte-array wire verified; bytes=%llu code=%u baseline=%.2fms; "
                "bulk screen-share serialization enabled",
                length, code, DesktopCapture::Milliseconds(begin, end));
        } else if (!verified) {
            static LONG logged = 0;
            if (!InterlockedCompareExchange(&logged, 1, 0)) {
                Log("[DesktopCapture] byte-array wire verification failed; native serialization retained");
            }
        }
    }
    static void (*receiveScreenFrameOriginal)(void*, void*, void*) = nullptr;
    static void ReceiveScreenFrame(void* instance, void* bytes, void* method) {
        // 20260323.14 schema: DesktopScreenSharingDisplay.NUCNWALGMIP(Byte[]).
        // 16CA010 schedules its native delegate then calls LoadImage at B4778D0
        // for the texture at +A8. Preserve all three register arguments and the
        // entire native behavior; only verbose builds install this timing hook.
        LARGE_INTEGER begin{}, end{};
        QueryPerformanceCounter(&begin);
        receiveScreenFrameOriginal(instance, bytes, method);
        QueryPerformanceCounter(&end);
        static LONG count = 0;
        const LONG frame = InterlockedIncrement(&count);
        if (frame <= 3 || frame % 30 == 1) {
            LogVerbose("[DesktopCapture] receive frame=%ld dispatch/decode/upload=%.2fms thread=%lu", frame,
                       DesktopCapture::Milliseconds(begin, end), GetCurrentThreadId());
        }
    }
    static void* CaptureDesktop(int display, int width, int quality, void*) {
        LARGE_INTEGER begin{}, allocated{}, end{};
        QueryPerformanceCounter(&begin);
        // Resolve managed metadata on this attached capture task, never on the DLL's
        // unmanaged initialization thread. The client serializes capture tasks.
        if (!byteArrayClass) {
            auto typeSlot = reinterpret_cast<void**>(game + RR::Methods::Metadata::ByteArrayTypeSlot);
            resolveCaptureType(typeSlot);
            byteArrayClass = *typeSlot;
            Gdiplus::GdiplusStartupInput input;
            if (Gdiplus::GdiplusStartup(&captureGdiToken, &input, nullptr) != Gdiplus::Ok) {
                Log("[DesktopCapture] Windows encoder initialization failed");
            }
        }
        DesktopCapture::Bounds bounds{};
        desktopBounds(&bounds, display, nullptr);
        std::vector<unsigned char> jpeg;
        DesktopCapture::Timing timing;
        const bool success = captureGdiToken && DesktopCapture::Encode(bounds, width, quality, jpeg, &timing);
        static LONG frameCount = 0;
        // Keep the first-frame proof useful without logging every captured frame.
        // Repeated encoding failures are sampled even when Verbose is disabled.
        const LONG frame = InterlockedIncrement(&frameCount);
        if (frame == 1 || (!success && frame % 120 == 1)) {
            Log("[DesktopCapture] frame=%ld display=%d source=%dx%d width=%d quality=%d JPEG=%zu success=%d",
                frame, display, bounds.width, bounds.height, width, quality, jpeg.size(), success);
        }
        QueryPerformanceCounter(&allocated);
        auto array = byteArrayNew(byteArrayClass, jpeg.size());
        if (array && !jpeg.empty()) {
            memcpy(reinterpret_cast<unsigned char*>(array) + RR::Offsets::ByteArray::Data, jpeg.data(),
                   jpeg.size());
        }
        QueryPerformanceCounter(&end);
        static int previousBackend = -1;
        const int backend = timing.duplication ? 1 : 0;
        if (backend != previousBackend || (verboseLogging && (frame <= 3 || frame % 30 == 1))) {
            Log("[DesktopCapture] backend=%s DXGI-status=%08lX capture=%.2fms readback=%.2fms resize=%.2fms "
                "JPEG=%.2fms managed=%.2fms total=%.2fms thread=%lu",
                timing.duplication ? "DXGI" : "GDI", timing.duplicationStatus, timing.captureMs,
                timing.readbackMs, timing.resizeMs, timing.encodeMs,
                DesktopCapture::Milliseconds(allocated, end), DesktopCapture::Milliseconds(begin, end),
                GetCurrentThreadId());
            previousBackend = backend;
        }
        return array;
    }
    static void* (*uriString)(void*, void*) = nullptr;
    static void (*uriCtor)(void*, void*, void*) = nullptr;
    static void* (*sendOriginal)(void*, void*) = nullptr;
    static void (*certificateOriginal)(void*, void*, void*) = nullptr;
    static void (*callbackOriginal)(void*, void*) = nullptr;
    static void (*offlineOriginal)(void*, bool, void*) = nullptr;
    static void (*clearTestCasesAndRuns)(void*) = nullptr;
    static void (*invalidateTestPlans)(void*) = nullptr;
    static std::string tachyonEndpoint;
    static std::string tachyonPublicKey;
    static void (*tachyonCtorOriginal)(void*, void*, void*, void*) = nullptr;
    static void TachyonCtor(void* instance, void* networking, void* dependencies, void* method) {
        // Let the client construct its provider first, then import the configured
        // public key through that provider's verified virtual method slot. The
        // matching private key never belongs in this process or its configuration.
        tachyonCtorOriginal(instance, networking, dependencies, method);
        if (!tachyonPublicKey.empty()) {
            auto rsa = *reinterpret_cast<uintptr_t*>(reinterpret_cast<uintptr_t>(instance) +
                                                     RR::Offsets::Tachyon::RsaProvider);
            if (!rsa) {
                return;
            }
            auto klass = *reinterpret_cast<uintptr_t*>(rsa);
            static void (*importXml)(void*, void*, void*) = nullptr;
            if (!importXml) {
                importXml = reinterpret_cast<decltype(importXml)>(
                    Gate(*reinterpret_cast<void**>(klass + RR::Offsets::RsaClass::ImportXml)));
            }
            if (importXml) {
                importXml(reinterpret_cast<void*>(rsa), stringNew(tachyonPublicKey.c_str()),
                          *reinterpret_cast<void**>(klass + RR::Offsets::RsaClass::ImportXmlMethod));
                Log("[Networking] Tachyon public key imported into verified RSA provider");
            }
        }
    }
    // These native callbacks represent completed transport approval and failures.
    // The title-screen address getter (9A972F0) reads the room transport and returns
    // "none" before it exists. Do not fabricate an early connection or change its
    // ClientState: Tachyon requires the room DTO/authentication from matchmaking.
    static void (*connectedOriginal)(void*, uint64_t, void*) = nullptr;
    static void TransportConnected(void* instance, uint64_t clientId, void* method) {
        connectedOriginal(instance, clientId, method);
        Log("[Networking] Tachyon room transport connected; client=%llu endpoint=%s", clientId,
            tachyonEndpoint.empty() ? "matchmaking" : tachyonEndpoint.c_str());
    }
    static void (*disconnectedOriginal)(void*, uint64_t, void*) = nullptr;
    static void TransportDisconnected(void* instance, uint64_t clientId, void* method) {
        Log("[Networking] Tachyon room transport disconnected; client=%llu", clientId);
        disconnectedOriginal(instance, clientId, method);
    }
    static void (*transportFailureOriginal)(void*, void*) = nullptr;
    static void TransportFailure(void* instance, void* method) {
        Log("[Networking] Tachyon room transport failed; endpoint=%s", tachyonEndpoint.c_str());
        transportFailureOriginal(instance, method);
    }
    static void (*connectOriginal)(void*, void*) = nullptr;
    // Unboxed MoveNext body: targetRoomInstance at +0x30; DTO voice address at +0x40.
    // The configured endpoint affects only this process, never matchmaking.
    static void ConnectRoom(void* state, void* method) {
        if (!tachyonEndpoint.empty() && state && *static_cast<int*>(state) == -1) {
            auto room = *reinterpret_cast<uintptr_t*>(reinterpret_cast<uintptr_t>(state) +
                                                      RR::Offsets::ConnectRoomState::TargetRoom);
            if (room) {
                void* endpoint = stringNew(tachyonEndpoint.c_str());
                void* id = tachyonServerId.empty() ? nullptr : stringNew(tachyonServerId.c_str());
                if (endpoint && (tachyonServerId.empty() || id)) {
                    auto address = reinterpret_cast<void**>(room + RR::Offsets::RoomInstance::VoiceAddress);
                    auto server = reinterpret_cast<void**>(room + RR::Offsets::RoomInstance::VoiceServerId);
                    *address = endpoint;
                    writeBarrier(address, endpoint);
                    if (id) {
                        *server = id;
                        writeBarrier(server, id);
                    }
                    Log("[Networking] selected Tachyon endpoint %s", tachyonEndpoint.c_str());
                }
            }
        }
        connectOriginal(state, method);
    }

    // Record the observed service-arena crash without swallowing the exception or
    // changing Referee control flow. No request bodies or authentication data are logged.
    static LONG CALLBACK NativeFault(EXCEPTION_POINTERS* fault) {
        const uintptr_t referee = reinterpret_cast<uintptr_t>(GetModuleHandleW(L"Referee.dll"));
        const uintptr_t address = reinterpret_cast<uintptr_t>(fault->ExceptionRecord->ExceptionAddress);
        if (fault->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION && referee &&
            (address == referee + RR::Methods::Referee::ObservedFault1 ||
             address == referee + RR::Methods::Referee::ObservedFault2)) {
            const auto* context = fault->ContextRecord;
            Log("[NativeFault] Referee+%llX access=%llu target=%llX rax=%llX rdx=%llX r8=%llX r9=%llX "
                "r10=%llX "
                "r11=%llX",
                address - referee, fault->ExceptionRecord->ExceptionInformation[0],
                fault->ExceptionRecord->ExceptionInformation[1], context->Rax, context->Rdx, context->R8,
                context->R9, context->R10, context->R11);
        }
        return EXCEPTION_CONTINUE_SEARCH;
    }

    // Captured from this build's Netcode implementation: EEYJFFZNJED(bool).
    // Observation only; forcing this flag would hide the missing room transport.
    static void SetOffline(void* networking, bool offline, void* method) {
        LogVerbose("[Networking] Netcode offline=%d", offline);
        offlineOriginal(networking, offline, method);
    }

    static bool StringUtf8(void* string, std::string& output) {
        // Read the measured IL2CPP string layout, not an assumed C string. Bound
        // the character count and reject invalid UTF-16 before using a URI for
        // routing or certificate scope. The surrounding caller owns pointer validity.
        if (!string) {
            return false;
        }
        const int length =
            *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(string) + RR::Offsets::String::Length);
        if (length < 0 || length > 8192) {
            return false;
        }
        const auto chars = reinterpret_cast<const wchar_t*>(reinterpret_cast<uintptr_t>(string) +
                                                            RR::Offsets::String::Characters);
        const int bytes =
            WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, chars, length, nullptr, 0, nullptr, nullptr);
        if (!bytes) {
            return false;
        }
        output.resize(bytes);
        return WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, chars, length, output.data(), bytes,
                                   nullptr, nullptr) == bytes;
    }
    // Runtime schema and disassembly verify this static signature on 20260323.14.
    // Observe the reason before the client discards its message; preserve shutdown.
    static void (*fatalQuitOriginal)(int, void*, void*) = nullptr;
    static void FatalQuit(int reason, void* message, void* method) {
        Log("[Exit] SessionManager.FatalApplicationQuit reason=%d", reason);
        void* frames[16]{};
        const USHORT count = CaptureStackBackTrace(0, 16, frames, nullptr);
        for (USHORT i = 0; i < count; ++i) {
            const auto address = reinterpret_cast<uintptr_t>(frames[i]);
            if (address >= game && address < game + RR::Methods::GameAssembly::ImageSize) {
                Log("[Exit] caller GameAssembly+%llX", address - game);
            }
        }
        fatalQuitOriginal(reason, message, method);
    }
    // 20260323.14 module-scan closure: +0x10 holds the module list; its only
    // action is FatalApplicationQuit(533223478, list). Live tracing confirmed this
    // exit during development room entry. Replace only this
    // callback; all other fatal reasons and user-requested exits retain their flow.
    static void ModuleScanDetected(void*, void*) {
        static LONG logged = 0;
        if (InterlockedCompareExchange(&logged, 1, 0) == 0) {
            Log("[Exit] patch module-scan exit suppressed (533223478)");
        }
    }
    static void* Send(void* request, void* method) {
        // Preserve the client's Uri type and constructor semantics. Replacing
        // the managed reference also requires IL2CPP's write barrier; a raw field
        // store alone can make the new object invisible to the collector.
        // C++ catch contains C++ failures here, not access violations under /EHsc.
        try {
            auto field = reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(request) +
                                                  RR::Offsets::HTTPRequest::Uri);
            if (request && *field) {
                std::string url;
                if (StringUtf8(uriString(*field, nullptr), url)) {
                    const bool rewritten = networkConfig.Rewrite(url);
                    if (rewritten) {
                        void* value = stringNew(url.c_str());
                        void* replacement = objectNew(objectClass(*field));
                        if (value && replacement) {
                            uriCtor(replacement, value, nullptr);
                            *field = replacement;
                            writeBarrier(field, replacement);
                            LogVerbose("[HTTP] redirected configured endpoint");
                        }
                    }
                    // Paths, queries and headers may contain account data.
                    RR::Config::Url parsed;
                    if (RR::Config::ParseUrl(url, parsed)) {
                        LogVerbose("[HTTP] host=%s", parsed.host.c_str());
                    }
                }
            }
        } catch (...) {
            LogVerbose("[HTTP] rewrite failed; request forwarded");
        }
        return sendOriginal(request, method);
    }
    // TargetUri +0x20 is verified by the runtime field capture. Keep the client's
    // original certificate policy for hosts outside the configured service domain.
    static void Certificate(void* instance, void* certificate, void* method) {
        try {
            void* uri = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(instance) +
                                                  RR::Offsets::TlsAuthentication::TargetUri);
            std::string url;
            if (uri && StringUtf8(uriString(uri, nullptr), url) && networkConfig.disableCertificatePinning &&
                networkConfig.Owns(url)) {
                return;
            }
        } catch (...) {
            Log("[TLS] target unreadable; original policy retained");
        }
        certificateOriginal(instance, certificate, method);
    }
    static void Callback(void* request, void* method) {
        try {
            auto address = reinterpret_cast<uintptr_t>(request);
            if (!request) {
                callbackOriginal(request, method);
                return;
            }
            auto response = *reinterpret_cast<uintptr_t*>(address + RR::Offsets::HTTPRequest::Response);
            auto uri = *reinterpret_cast<void**>(address + RR::Offsets::HTTPRequest::Uri);
            std::string url;
            if (uri && StringUtf8(uriString(uri, nullptr), url)) {
                const int status =
                    response ? *reinterpret_cast<int*>(response + RR::Offsets::HTTPResponse::StatusCode) : -1;
                LogVerbose("[Response] status=%d", status);
                // 20260323.14 caches case detail and run category lists separately.
                // Successful writes previously invalidated only the detail, leaving
                // the run's old TestCases until the whole menu was reopened. Clear
                // both native caches before delivering the completion to the UI.
                const std::string path = url.substr(0, url.find_first_of("?#"));
                RR::Config::Url parsed;
                const bool configured = RR::Config::ParseUrl(path, parsed) && networkConfig.Owns(path);
                const std::string route = configured ? path.substr(parsed.authorityEnd) : std::string();
                const std::string casePrefix = "/api/testcasemanagement/v1/testcase/";
                const auto action = path.substr(path.find_last_of('/') + 1);
                if (status >= 200 && status < 300 && configured &&
                    route.compare(0, casePrefix.size(), casePrefix) == 0 &&
                    (action == "claim" || action == "unclaim" || action == "status" || action == "comment") &&
                    clearTestCasesAndRuns && invalidateTestPlans) {
                    clearTestCasesAndRuns(nullptr);
                    invalidateTestPlans(nullptr);
                    LogVerbose("[TCM] successful %s invalidated case, run and plan caches", action.c_str());
                }
            }
        } catch (...) {
            Log("[Response] diagnostic unavailable");
        }
        callbackOriginal(request, method);
    }
    // RecNet.Images.BNYRIKRSADD(byte[], byte[]) is the build's private RSA verifier.
    // Optional compatibility mode for images not signed by the retired service.
    static bool ImageVerify(void* data, void*, void*) {
        return data != nullptr;
    }
    static bool Hook(uintptr_t rva, void* callback, void** original, const char* name) {
        // Create and queue only; callbacks must not run while their dependencies
        // are still being resolved. Initialize applies the queue at the end.
        // Failure before application removes created hooks via MH_Uninitialize.
        auto target = reinterpret_cast<void*>(game + rva);
        auto result = MH_CreateHook(target, callback, original);
        if (result == MH_OK) {
            result = MH_QueueEnableHook(target);
        }
        if (result != MH_OK) {
            Log("[Hook] %s failed status=%d", name, result);
        } else {
            LogVerbose("[Hook] %s rva=%llX status=%d", name, rva, result);
        }
        return result == MH_OK;
    }
    DWORD Initialize(const RR::Config::Settings& settings) {
        // main.cpp has validated this snapshot and the loaded fingerprints. Copy
        // configuration before resolving or enabling callbacks; initialization
        // is once per process, with no concurrent settings reload or live unload.
        networkConfig = settings.network;
        tachyonServerId = settings.tachyonServerId;
        tachyonPublicKey = settings.voiceKeyXml;
        screenShareFps = settings.framesPerSecond;
        game = reinterpret_cast<uintptr_t>(GetModuleHandleW(L"GameAssembly.dll"));
        unity = reinterpret_cast<uintptr_t>(GetModuleHandleW(L"UnityPlayer.dll"));
        if (!game || !unity) {
            return 2;
        }
        if (verboseLogging) {
            AddVectoredExceptionHandler(1, NativeFault);
        }
        // Injection can arrive during IL2CPP startup. Wait at most five seconds
        // for the known string allocator slot, then validate each API separately.
        // This native worker is not an attached managed thread: do not initialize
        // capture metadata or allocate managed capture objects here.
        auto table = reinterpret_cast<uintptr_t*>(unity + RR::Methods::Unity::ApiTable);
        auto deadline = GetTickCount64() + 5000;
        while ((table[176] < game || table[176] >= game + RR::Methods::GameAssembly::ImageSize) &&
               GetTickCount64() < deadline) {
            Sleep(1);
        }
        stringNew = reinterpret_cast<decltype(stringNew)>(Api(176));
        objectClass = reinterpret_cast<decltype(objectClass)>(Api(132));
        objectNew = reinterpret_cast<decltype(objectNew)>(Api(135));
        writeBarrier = reinterpret_cast<decltype(writeBarrier)>(
            Gate(reinterpret_cast<void*>(game + RR::Methods::Il2Cpp::WriteBarrier)));
        uriString = reinterpret_cast<decltype(uriString)>(
            Gate(reinterpret_cast<void*>(game + RR::Methods::Uri::ToString)));
        uriCtor = reinterpret_cast<decltype(uriCtor)>(
            Gate(reinterpret_cast<void*>(game + RR::Methods::Uri::Constructor)));
        // Runtime schema and disassembly: XLTUBXECNWL.Clear clears both
        // case/run dictionaries; TIDOPNTGJQL expires the plan-list timestamp.
        clearTestCasesAndRuns = reinterpret_cast<decltype(clearTestCasesAndRuns)>(
            Gate(reinterpret_cast<void*>(game + RR::Methods::TestCases::ClearCasesAndRuns)));
        invalidateTestPlans = reinterpret_cast<decltype(invalidateTestPlans)>(
            Gate(reinterpret_cast<void*>(game + RR::Methods::TestCases::InvalidatePlans)));
        if (!writeBarrier || !stringNew || !objectClass || !objectNew || !uriString || !uriCtor ||
            !clearTestCasesAndRuns || !invalidateTestPlans) {
            Log("[Build] API gates unavailable");
            return 3;
        }
        if (MH_Initialize() != MH_OK) {
            return 4;
        }
        // Fresh live schema: HFPKPTLHCFN.PLWVBUBSAGX(int display,int width,int quality)
        // returns JPEG byte[]. LRXOTPJJTEJ returns plain Rectangle(x,y,w,h).
        // MemoryStream.ToArray at 8A8CF70 verifies Byte[] type slot FCCC8F0,
        // metadata resolver C400D0 and array allocator C3F460 on this fingerprint.
        desktopBounds = reinterpret_cast<decltype(desktopBounds)>(
            Gate(reinterpret_cast<void*>(game + RR::Methods::DesktopCapture::Bounds)));
        byteArrayNew = reinterpret_cast<decltype(byteArrayNew)>(
            Gate(reinterpret_cast<void*>(game + RR::Methods::Il2Cpp::ByteArrayNew)));
        resolveCaptureType = reinterpret_cast<decltype(resolveCaptureType)>(
            Gate(reinterpret_cast<void*>(game + RR::Methods::Il2Cpp::ResolveType)));
        initializeClass = reinterpret_cast<decltype(initializeClass)>(
            Gate(reinterpret_cast<void*>(game + RR::Methods::Il2Cpp::InitializeClass)));
        sendFriendRequest = reinterpret_cast<decltype(sendFriendRequest)>(
            Gate(reinterpret_cast<void*>(game + RR::Methods::Friends::SendRequest)));
        receiveGestureFriend = reinterpret_cast<decltype(receiveGestureFriend)>(
            Gate(reinterpret_cast<void*>(game + RR::Methods::Friends::ReceiveGestureRequest)));
        gestureOwnedLocally = reinterpret_cast<decltype(gestureOwnedLocally)>(
            Gate(reinterpret_cast<void*>(game + RR::Methods::Friends::OwnedLocally)));
        gestureObjectExists = reinterpret_cast<decltype(gestureObjectExists)>(
            Gate(reinterpret_cast<void*>(game + RR::Methods::Friends::ObjectExists)));
        gestureHandTransform = reinterpret_cast<decltype(gestureHandTransform)>(
            Gate(reinterpret_cast<void*>(game + RR::Methods::Friends::HandTransform)));
        gestureTransformPosition = reinterpret_cast<decltype(gestureTransformPosition)>(
            Gate(reinterpret_cast<void*>(game + RR::Methods::Friends::TransformPosition)));
        gestureNetworkPlayer = reinterpret_cast<decltype(gestureNetworkPlayer)>(
            Gate(reinterpret_cast<void*>(game + RR::Methods::Friends::NetworkPlayer)));
        classMethods = reinterpret_cast<decltype(classMethods)>(Api(124));
        methodName = reinterpret_cast<decltype(methodName)>(Api(110));
        importCleanup = reinterpret_cast<decltype(importCleanup)>(
            Gate(reinterpret_cast<void*>(game + RR::Methods::Friends::ImportCleanup)));
        importError = reinterpret_cast<decltype(importError)>(
            Gate(reinterpret_cast<void*>(game + RR::Methods::Friends::ImportError)));
        importIsSelected = reinterpret_cast<decltype(importIsSelected)>(
            Gate(reinterpret_cast<void*>(game + RR::Methods::Friends::ImportIsSelected)));
        importToggleSelected = reinterpret_cast<decltype(importToggleSelected)>(
            Gate(reinterpret_cast<void*>(game + RR::Methods::Friends::ImportToggleSelected)));
        importEventInvoke = reinterpret_cast<decltype(importEventInvoke)>(
            Gate(reinterpret_cast<void*>(game + RR::Methods::Friends::ImportEventInvoke)));
        importActionConstructor = reinterpret_cast<decltype(importActionConstructor)>(
            Gate(reinterpret_cast<void*>(game + RR::Methods::Friends::ActionConstructor)));
        importErrorActionConstructor = reinterpret_cast<decltype(importErrorActionConstructor)>(
            Gate(reinterpret_cast<void*>(game + RR::Methods::Friends::ErrorActionConstructor)));
        allPromiseConstructor = reinterpret_cast<decltype(allPromiseConstructor)>(
            Gate(reinterpret_cast<void*>(game + RR::Methods::Friends::AllPromiseConstructor)));
        promiseThen = reinterpret_cast<decltype(promiseThen)>(
            Gate(reinterpret_cast<void*>(game + RR::Methods::Friends::PromiseThen)));
        promiseError = reinterpret_cast<decltype(promiseError)>(
            Gate(reinterpret_cast<void*>(game + RR::Methods::Friends::PromiseError)));
        if (!resolveCaptureType || !initializeClass || !sendFriendRequest || !receiveGestureFriend ||
            !gestureOwnedLocally || !gestureObjectExists || !gestureHandTransform ||
            !gestureTransformPosition || !gestureNetworkPlayer || !byteArrayNew || !classMethods ||
            !methodName || !importCleanup || !importError || !importIsSelected || !importToggleSelected ||
            !importEventInvoke || !importActionConstructor || !importErrorActionConstructor ||
            !allPromiseConstructor || !promiseThen || !promiseError ||
            !Hook(RR::Methods::Friends::ImportSelected, reinterpret_cast<void*>(ImportSelectedFriends),
                  reinterpret_cast<void**>(&importSelectedOriginal),
                  "FriendImportModel.NativeSelectedRequests") ||
            !Hook(RR::Methods::Friends::RequestWrapper, reinterpret_cast<void*>(FriendRequest),
                  reinterpret_cast<void**>(&friendRequestOriginal), "Relationships.NativeFriendRequests") ||
            !Hook(RR::Methods::Friends::ToggleRelationship, reinterpret_cast<void*>(ToggleRelationship),
                  reinterpret_cast<void**>(&toggleRelationshipOriginal),
                  "BaseAccountModel.NativeFriendRequests") ||
            !Hook(RR::Methods::Friends::SendGestureRequest, reinterpret_cast<void*>(GestureFriendRequest),
                  reinterpret_cast<void**>(&gestureFriendOriginal), "PlayerParty.NativeHandshakeFriends") ||
            !Hook(RR::Methods::Friends::PartyGesture, reinterpret_cast<void*>(PartyGesture),
                  reinterpret_cast<void**>(&partyGestureOriginal), "PlayerParty.DetectedHandshakeFriends")) {
            MH_Uninitialize();
            return 5;
        }
        if (desktopBounds && byteArrayNew && resolveCaptureType) {
            if (!Hook(RR::Methods::DesktopCapture::Capture, reinterpret_cast<void*>(CaptureDesktop), nullptr,
                      "DesktopCapture.WindowsJPEG")) {
                MH_Uninitialize();
                return 5;
            }
        } else {
            Log("[DesktopCapture] Windows capture initialization failed");
        }
        localAccountId = reinterpret_cast<decltype(localAccountId)>(
            Gate(reinterpret_cast<void*>(game + RR::Methods::LocalTesting::LocalAccountId)));
        localPlayerPresence = reinterpret_cast<decltype(localPlayerPresence)>(
            Gate(reinterpret_cast<void*>(game + RR::Methods::LocalTesting::PlayerPresence)));
        if (!localAccountId || !localPlayerPresence ||
            !Hook(RR::Methods::LocalTesting::ApplyHeartbeat, reinterpret_cast<void*>(ApplyHeartbeat),
                  reinterpret_cast<void**>(&heartbeatOriginal), "Matchmaking.LocalTestHeartbeat")) {
            MH_Uninitialize();
            return 5;
        }
        heartbeatInvoke =
            reinterpret_cast<decltype(heartbeatInvoke)>(Gate(reinterpret_cast<void*>(heartbeatOriginal)));
        if (!heartbeatInvoke) {
            MH_Uninitialize();
            return 5;
        }
        if (!Hook(RR::Methods::HTTP::SendRequest, reinterpret_cast<void*>(Send),
                  reinterpret_cast<void**>(&sendOriginal), "HTTPManager.SendRequest") ||
            !Hook(RR::Methods::HTTP::NotifyServerCertificate, reinterpret_cast<void*>(Certificate),
                  reinterpret_cast<void**>(&certificateOriginal),
                  "LegacyTlsAuthentication.NotifyServerCertificate") ||
            !Hook(RR::Methods::HTTP::CallCallback, reinterpret_cast<void*>(Callback),
                  reinterpret_cast<void**>(&callbackOriginal), "HTTPRequest.CallCallback") ||
            (networkConfig.allowUnsignedImages &&
             !Hook(RR::Methods::Images::Verify, reinterpret_cast<void*>(ImageVerify), nullptr,
                   "Images.BNYRIKRSADD")) ||
            (verboseLogging && !Hook(RR::Methods::Netcode::SetOffline, reinterpret_cast<void*>(SetOffline),
                                     reinterpret_cast<void**>(&offlineOriginal), "Netcode.SetOffline")) ||
            !Hook(RR::Methods::Session::FatalQuit, reinterpret_cast<void*>(FatalQuit),
                  reinterpret_cast<void**>(&fatalQuitOriginal),
                  "SessionManager.FatalApplicationQuit.Diagnostic") ||
            !Hook(RR::Methods::Session::ModuleScanDetected, reinterpret_cast<void*>(ModuleScanDetected),
                  nullptr, "CheatManager.ModuleScanDetected") ||
            !Hook(RR::Methods::Circuits::RemoveFunction, reinterpret_cast<void*>(RemoveCircuitFunction),
                  reinterpret_cast<void**>(&removeCircuitFunctionOriginal),
                  "CircuitsV2.RemoveFunction.Lifecycle")) {
            MH_Uninitialize();
            return 5;
        }
        roomieIsInNoCreditMode = reinterpret_cast<decltype(roomieIsInNoCreditMode)>(
            Gate(reinterpret_cast<void*>(game + RR::Methods::Roomie::IsInNoCreditMode)));
        if (!roomieIsInNoCreditMode ||
            !Hook(RR::Methods::Roomie::HasNoEnergy, reinterpret_cast<void*>(RoomieHasNoEnergy),
                  reinterpret_cast<void**>(&roomieHasNoEnergyOriginal),
                  "RoomieTool.NoCreditCompanionActivities") ||
            !Hook(RR::Methods::Roomie::WheelAccess, reinterpret_cast<void*>(RoomieWheelAccess), nullptr,
                  "Roomie.EquipmentWheelNoCreditAccess")) {
            MH_Uninitialize();
            return 5;
        }
        // MinHook's trampoline lives outside the verified GameAssembly call path.
        // Wrap this particular original call in the same measured Unity gate;
        // preserve the callback signature and the noinline wheel caller check.
        roomieHasNoEnergyOriginal = reinterpret_cast<decltype(roomieHasNoEnergyOriginal)>(
            Gate(reinterpret_cast<void*>(roomieHasNoEnergyOriginal)));
        if (!roomieHasNoEnergyOriginal) {
            MH_Uninitialize();
            return 5;
        }
        writerReserve = reinterpret_cast<decltype(writerReserve)>(
            Gate(reinterpret_cast<void*>(game + RR::Methods::DesktopCapture::WriterReserve)));
        if (!writerReserve ||
            !Hook(RR::Methods::DesktopCapture::SendChunk, reinterpret_cast<void*>(SendScreenChunk),
                  reinterpret_cast<void**>(&sendScreenChunkOriginal), "DesktopCapture.SendChunk") ||
            !Hook(RR::Methods::DesktopCapture::SerializeArray, reinterpret_cast<void*>(SerializeScreenArray),
                  reinterpret_cast<void**>(&serializeArrayOriginal), "DesktopCapture.BulkByteArray")) {
            MH_Uninitialize();
            return 5;
        }
        sendScreenChunkOriginal = reinterpret_cast<decltype(sendScreenChunkOriginal)>(
            Gate(reinterpret_cast<void*>(sendScreenChunkOriginal)));
        serializeArrayOriginal = reinterpret_cast<decltype(serializeArrayOriginal)>(
            Gate(reinterpret_cast<void*>(serializeArrayOriginal)));
        if (!sendScreenChunkOriginal || !serializeArrayOriginal) {
            MH_Uninitialize();
            return 5;
        }
        if (settings.verbose) {
            if (!Hook(RR::Methods::DesktopCapture::ReceiveFrame, reinterpret_cast<void*>(ReceiveScreenFrame),
                      reinterpret_cast<void**>(&receiveScreenFrameOriginal),
                      "DesktopCapture.ReceiveFrameTiming")) {
                MH_Uninitialize();
                return 5;
            }
            receiveScreenFrameOriginal = reinterpret_cast<decltype(receiveScreenFrameOriginal)>(
                Gate(reinterpret_cast<void*>(receiveScreenFrameOriginal)));
            if (!receiveScreenFrameOriginal) {
                MH_Uninitialize();
                return 5;
            }
        }
        // Zero is native behavior: do not install a refresh-rate hook at all.
        // Explicit overrides alter presets while keeping adaptive/backpressure logic.
        if (screenShareFps) {
            if (!Hook(RR::Methods::DesktopCapture::Update, reinterpret_cast<void*>(DesktopUpdate),
                      reinterpret_cast<void**>(&desktopUpdateOriginal), "DesktopCapture.RefreshRate")) {
                MH_Uninitialize();
                return 5;
            }
        }
        Log("[DesktopCapture] requested refresh=%d FPS (0 preserves original presets)", screenShareFps);
        if (!settings.tachyonHost.empty()) {
            std::string host, port;
            RR::Config::Endpoint(settings.tachyonHost, host, port);
            tachyonEndpoint = settings.tachyonHost;
            {
                // Tachyon.STMBWJUYIQW (9AA3540) splits host:port then calls
                // IPAddress.Parse at AD86D30; it does not resolve DNS hostnames.
                WSADATA data{};
                addrinfo hints{}, *addresses = nullptr;
                hints.ai_family = AF_INET;
                hints.ai_socktype = SOCK_DGRAM;
                if (WSAStartup(MAKEWORD(2, 2), &data) != 0) {
                    MH_Uninitialize();
                    return 7;
                }
                int error = getaddrinfo(host.c_str(), nullptr, &hints, &addresses);
                char numeric[INET_ADDRSTRLEN]{};
                if (!error && addresses) {
                    InetNtopA(AF_INET, &reinterpret_cast<sockaddr_in*>(addresses->ai_addr)->sin_addr, numeric,
                              sizeof(numeric));
                }
                if (addresses) {
                    freeaddrinfo(addresses);
                }
                WSACleanup();
                if (!numeric[0]) {
                    Log("[Networking] Tachyon hostname resolution failed (%d)", error);
                    MH_Uninitialize();
                    return 7;
                }
                tachyonEndpoint = std::string(numeric) + ":" + port;
                Log("[Networking] resolved %s to %s", host.c_str(), numeric);
            }
        }
        // Key replacement can accompany matchmaking's address without overriding it.
        if (!tachyonPublicKey.empty()) {
            if (!Hook(RR::Methods::Tachyon::Constructor, reinterpret_cast<void*>(TachyonCtor),
                      reinterpret_cast<void**>(&tachyonCtorOriginal), "Tachyon.ctor.ConfiguredKey")) {
                MH_Uninitialize();
                return 9;
            }
        }
        if (!tachyonEndpoint.empty() || !tachyonPublicKey.empty()) {
            if (!writeBarrier ||
                !Hook(RR::Methods::Tachyon::ConnectRoom, reinterpret_cast<void*>(ConnectRoom),
                      reinterpret_cast<void**>(&connectOriginal), "Tachyon.ConnectRoom.MoveNext") ||
                !Hook(RR::Methods::Tachyon::Connected, reinterpret_cast<void*>(TransportConnected),
                      reinterpret_cast<void**>(&connectedOriginal), "Tachyon.OnClientConnected") ||
                !Hook(RR::Methods::Tachyon::Disconnected, reinterpret_cast<void*>(TransportDisconnected),
                      reinterpret_cast<void**>(&disconnectedOriginal), "Tachyon.OnClientDisconnected") ||
                !Hook(RR::Methods::Tachyon::Failure, reinterpret_cast<void*>(TransportFailure),
                      reinterpret_cast<void**>(&transportFailureOriginal), "Tachyon.OnTransportFailure")) {
                MH_Uninitialize();
                return 8;
            }
        }
        // All function pointers, configuration and endpoint preparation are now
        // complete. Apply queued hooks together. A failed apply is reported; the
        // return value must not be interpreted as a transactional rollback guarantee.
        auto result = MH_ApplyQueued();
        Log("[Build] hooks enabled status=%d", result);
        const bool hookReady = result == MH_OK;
        if (hookReady && (!tachyonEndpoint.empty() || !tachyonPublicKey.empty())) {
            Log("[Networking] configured endpoint ready before room join: %s; public key configured=%d",
                tachyonEndpoint.empty() ? "matchmaking (no address override)" : tachyonEndpoint.c_str(),
                !tachyonPublicKey.empty());
            Log("[Networking] room transport connects during room join; title-screen Photon:none is expected "
                "before that connection");
        }
        return hookReady ? 0 : 6;
    }

}
