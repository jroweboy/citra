// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common/common_types.h"

namespace Service {

class Interface;

namespace FRD {

struct FriendKey {
    u32 friend_id;
    u32 padding;
    u64 friend_code;
};

struct MyPresence {
    u8 unknown[0x12C];
};

struct MiiData {
    u8 data[0x60];
};

struct Profile {
    u8 region;
    u8 country;
    u8 area;
    u8 language;
    u32 unknown;
};

/**
 * FRD::HasLoggedIn service function
 *  Inputs:
 *      0 : Header Code[0x00010000]
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 *      2 : True, if FRD service is logged into server
 */
void HasLoggedIn(Service::Interface* self);

/**
 * FRD::Login service function
 *  Inputs:
 *      0 : Header Code[0x00030002]
 *      1 : Copy handle descriptor
 *      2 : Completion event handle
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 */
void Login(Service::Interface* self);

/**
 * FRD::Logout service function
 *  Inputs:
 *      0 : Header Code[0x00040000]
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 */
void Logout(Service::Interface* self);

/**
 * FRD::GetMyFriendKey service function
 *  Inputs:
 *      0 : Header Code[0x00050000]
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 *      2-5 : FriendKey
 */

void GetMyFriendKey(Service::Interface* self);
/**
 * FRD::GetMyProfile service function
 *  Inputs:
 *      0 : Header Code[0x00070000]
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 *      2-3 : My Profile
 */
void GetMyProfile(Service::Interface* self);

/**
 * FRD::GetMyPresence service function
 *  Inputs:
 *      0 : Header Code[0x00080000]
 *      64 : sizeof (MyPresence) << 14 | 2
 *      65 : Address of MyPresence structure
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 */
void GetMyPresence(Service::Interface* self);

/**
 * FRD::GetMyScreenName service function
 *  Inputs:
 *      0 : Header Code[0x00090000]
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 *      2 : UTF16 encoded name (max 11 symbols)
 */
void GetMyScreenName(Service::Interface* self);

/**
 * FRD::GetMyMii service function
 *  Inputs:
 *      0 : Header Code[0x000A0000]
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 *      2-26 : My MiiData content
 */
void GetMyMii(Service::Interface* self);

/**
 * FRD::GetFriendKeyList service function
 *  Inputs:
 *      0 : Header Code[0x00110080]
 *      1 : Offset
 *      2 : Max friends count
 *      64 : (count * sizeof (FriendKey)) << 14 | 2
 *      65 : Address of FriendKey List
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 *      2 : FriendKey count filled
 */
void GetFriendKeyList(Service::Interface* self);

/**
 * FRD::GetFriendProfile service function
 *  Inputs:
 *      0 : Header Code[0x00150042]
 *      1 : Friends count
 *      2 : Friends count << 18 | 2
 *      3 : Address of FriendKey List
 *      64 : (count * sizeof (Profile)) << 14 | 2
 *      65 : Address of Profiles List
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 */
void GetFriendProfile(Service::Interface* self);

/**
 * FRD::GetFriendAttributeFlags service function
 *  Inputs:
 *      0 : Header Code[0x00170042]
 *      1 : Friends count
 *      2 : Friends count << 18 | 2
 *      3 : Address of FriendKey List
 *      64 : (count * sizeof (Profile)) << 14 | 2
 *      65 : Address of AttributeFlags
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 */
void GetFriendAttributeFlags(Service::Interface* self);

void AttachToEventNotification(Service::Interface* self);

/**
 * FRD::UpdateGameModeDescription service function
 *  Inputs:
 *      0 : Header Code[0x001D0002]
 *      1 : Description mode string size (translated)
 *      2 : Description mode string pointer
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 */
void UpdateGameModeDescription(Service::Interface* self);

void GetLastResponseResult(Service::Interface* self);

/**
 * FRD::ResultToErrorCode service function
 *  Inputs:
 *      0 : Header Code[0x00270040]
 *      1 : FRD specific Result code
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 *      2 : OS level Error Code
 */
void ResultToErrorCode(Service::Interface* self);

void SetNotificationMask(Service::Interface* self);

void GetGameAuthenticationData(Service::Interface* self);

void RequestGameAuthentication(Service::Interface* self);

/// Initialize FRD service(s)
void Init();

/// Shutdown FRD service(s)
void Shutdown();

} // namespace FRD
} // namespace Service
