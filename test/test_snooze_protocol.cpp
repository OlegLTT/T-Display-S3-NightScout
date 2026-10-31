#include "snooze_protocol.h"

static_assert(snooze_protocol::port == 50555, "M5_NightscoutMon port");
static_assert(snooze_protocol::user_crc("123456789") == 0xBB3D, "CRC16 polynomial A001, initial value 0");
static_assert(snooze_protocol::user_crc("") == 0, "Empty CRC seed");

constexpr char packet[] = "M5_Nightscout SNOOZE: USR=47933, SnoozeUntil=1700001800";
constexpr char off[] = "M5_Nightscout SNOOZE: USR=47933, SnoozeUntil=0";
constexpr char overflow[] = "M5_Nightscout SNOOZE: USR=47933, SnoozeUntil=4294967296";
constexpr char negative[] = "M5_Nightscout SNOOZE: USR=47933, SnoozeUntil=-1";
constexpr char trailing[] = "M5_Nightscout SNOOZE: USR=47933, SnoozeUntil=1700001800junk";
constexpr char missing[] = "M5_Nightscout SNOOZE: USR=47933, SnoozeUntil=";

constexpr bool valid_packets() {
    uint32_t until = 0, seconds = 0;
    if (!snooze_protocol::decode(packet, sizeof(packet), 0xBB3D, until) || until != 1700001800) return false;
    if (!snooze_protocol::decode(packet, sizeof(packet)-1, 0xBB3D, until)) return false;
    if (!snooze_protocol::seconds_left(until, 1700000000, seconds) || seconds != 1800) return false;
    if (!snooze_protocol::seconds_left(until, 1700001799, seconds) || seconds != 1) return false;
    if (!snooze_protocol::seconds_left(until, until, seconds) || seconds) return false;
    if (!snooze_protocol::seconds_left(until, until+1, seconds) || seconds) return false;
    if (!snooze_protocol::decode(off, sizeof(off), 0xBB3D, until) || until) return false;
    return snooze_protocol::seconds_left(until, 0, seconds) && !seconds;
}

constexpr bool invalid_packets() {
    uint32_t until = 42, seconds = 0;
    if (snooze_protocol::decode(packet, sizeof(packet), 123, until) || until != 42) return false;
    if (snooze_protocol::decode(packet, 0, 0xBB3D, until)) return false;
    // Every truncation before the first timestamp digit must be rejected.
    for (unsigned size = 1; size < sizeof(missing)-1; ++size)
        if (snooze_protocol::decode(packet, size, 0xBB3D, until)) return false;
    if (snooze_protocol::decode(overflow, sizeof(overflow), 0xBB3D, until)) return false;
    if (snooze_protocol::decode(negative, sizeof(negative), 0xBB3D, until)) return false;
    if (snooze_protocol::decode(trailing, sizeof(trailing), 0xBB3D, until)) return false;
    if (snooze_protocol::decode(missing, sizeof(missing), 0xBB3D, until)) return false;
    if (snooze_protocol::seconds_left(1700001800, 0, seconds)) return false;
    if (snooze_protocol::seconds_left(1700086401, 1700000000, seconds)) return false;
    return snooze_protocol::seconds_left(1700086400, 1700000000, seconds) && seconds == 86400;
}
static_assert(valid_packets(), "Accept original packets with/without NUL, countdown, expiry and OFF without clock");
static_assert(invalid_packets(), "Reject wrong users, malformed/overflowed packets, unknown clock and excessive duration");

constexpr char refresh[] = "M5_Nightscout REFRESH: USR=47933";
constexpr bool refresh_packets() {
    if (!snooze_protocol::decode_refresh(refresh, sizeof(refresh), 0xBB3D)) return false;
    if (!snooze_protocol::decode_refresh(refresh, sizeof(refresh)-1, 0xBB3D)) return false;
    if (snooze_protocol::decode_refresh(refresh, sizeof(refresh), 123)) return false;
    if (snooze_protocol::decode_refresh(refresh, 0, 0xBB3D)) return false;
    if (snooze_protocol::decode_refresh(refresh, snooze_protocol::packet_capacity, 0xBB3D)) return false;
    constexpr char missing_crc[] = "M5_Nightscout REFRESH: USR=";
    if (snooze_protocol::decode_refresh(missing_crc, sizeof(missing_crc), 0)) return false;
    constexpr char negative_crc[] = "M5_Nightscout REFRESH: USR=-1";
    if (snooze_protocol::decode_refresh(negative_crc, sizeof(negative_crc), 0xFFFF)) return false;
    constexpr char overflow_crc[] = "M5_Nightscout REFRESH: USR=4294967296";
    if (snooze_protocol::decode_refresh(overflow_crc, sizeof(overflow_crc), 0)) return false;
    constexpr char trailing_crc[] = "M5_Nightscout REFRESH: USR=47933junk";
    if (snooze_protocol::decode_refresh(trailing_crc, sizeof(trailing_crc), 0xBB3D)) return false;
    constexpr char extra_nul[] = "M5_Nightscout REFRESH: USR=47933\0";
    if (snooze_protocol::decode_refresh(extra_nul, sizeof(extra_nul), 0xBB3D)) return false;
    if (snooze_protocol::decode_refresh(packet, sizeof(packet), 0xBB3D)) return false;
    uint32_t until = 42;
    return !snooze_protocol::decode(refresh, sizeof(refresh), 0xBB3D, until) && until == 42;
}
static_assert(refresh_packets(), "Refresh needs a matching user, no clock; reject malformed and snooze packets");
