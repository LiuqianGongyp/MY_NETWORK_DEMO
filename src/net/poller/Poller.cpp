//
// Created by lq on 2024/6/25.
//

#include "Poller.h"
bool Poller::hasChannel(Channel *channel) const {
    auto it = channels_.find(channel->fd());
    return it != channels_.end() && it->second == channel;
}