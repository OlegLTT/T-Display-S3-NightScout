#pragma once

namespace nightscout_update {
#if __cplusplus >= 201402L
#define UPDATE_CONSTEXPR14 constexpr
#else
#define UPDATE_CONSTEXPR14
#endif

// Источник текущего запроса отделён от следующего: LAN может прийти во время HTTP.
class Source {
public:
    UPDATE_CONSTEXPR14 void request(bool from_lan) { pending_lan_ = pending_lan_ || from_lan; }
    UPDATE_CONSTEXPR14 void start() {
        active_lan_ = pending_lan_;
        pending_lan_ = false;
    }
    UPDATE_CONSTEXPR14 void retry() { request(active_lan_); }
    constexpr bool from_lan() const { return active_lan_; }

private:
    bool pending_lan_ = false, active_lan_ = false;
};

#undef UPDATE_CONSTEXPR14
} // namespace nightscout_update
