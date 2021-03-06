/*
 * This file is part of wysiwidi
 *
 * Copyright (C) 2014 Intel Corporation.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 */

#ifndef STREAMING_STATE_H_
#define STREAMING_STATE_H_

#include "wfd/common/message_handler.h"

namespace wfd {
namespace sink {

// Streaming state for RTSP sink.
// Includes M8 message handling and optionally can handle M3, M4, M7 and M9
class StreamingState : public MessageSequenceWithOptionalSetHandler {
 public:
  StreamingState(const InitParams& init_params, MessageHandlerPtr m16_handler);
};

class TeardownHandler : public MessageSequenceHandler {
 public:
  explicit TeardownHandler(const InitParams& init_params);
};

}  // sink
}  // wfd

#endif // STREAMING_STATE_H_
