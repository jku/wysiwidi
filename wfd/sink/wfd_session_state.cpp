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

#include "wfd_session_state.h"

#include "wfd/public/media_manager.h"

#include "cap_negotiation_state.h"
#include "streaming_state.h"
#include "wfd/parser/play.h"
#include "wfd/parser/reply.h"
#include "wfd/parser/setup.h"
#include "wfd/parser/transportheader.h"

namespace wfd {
namespace sink {

M16Handler::M16Handler(const InitParams& init_params, uint& keep_alive_timer)
  : MessageReceiver<Request::M16>(init_params),
    keep_alive_timer_(keep_alive_timer) {}

bool M16Handler::HandleTimeoutEvent(uint timer_id) const {
  return timer_id == keep_alive_timer_;
}

std::unique_ptr<Reply> M16Handler::HandleMessage(Message* message) {
  // Reset keep alive timer;
  sender_->ReleaseTimer(keep_alive_timer_);
  keep_alive_timer_ = sender_->CreateTimer(60);

  return std::unique_ptr<Reply>(new Reply(200));
}

M6Handler::M6Handler(const InitParams& init_params, uint& keep_alive_timer)
  : SequencedMessageSender(init_params),
    keep_alive_timer_(keep_alive_timer) {}

std::unique_ptr<Message> M6Handler::CreateMessage() {
  auto setup = new Setup(ToSinkMediaManager(manager_)->PresentationUrl());
  auto transport = new TransportHeader();
  // we assume here that there is no coupled secondary sink
  transport->set_client_port(ToSinkMediaManager(manager_)->ListeningRtpPorts().first);
  setup->header().set_transport(transport);
  setup->header().set_cseq(send_cseq_++);
  setup->header().set_require_wfd_support(true);

  return std::unique_ptr<Message>(setup);
}

bool M6Handler::HandleReply(Reply* reply) {
  const std::string& session_id = reply->header().session();
  if(reply->response_code() == 200 && !session_id.empty()) {
    ToSinkMediaManager(manager_)->SetSession(session_id);
    // FIXME : take timeout value from session.
    keep_alive_timer_ = sender_->CreateTimer(60);
    return true;
  }

  return false;
}

class M7Handler final : public SequencedMessageSender {
 public:
    using SequencedMessageSender::SequencedMessageSender;
 private:
  virtual std::unique_ptr<Message> CreateMessage() override {
    Play* play = new Play(ToSinkMediaManager(manager_)->PresentationUrl());
    play->header().set_session(ToSinkMediaManager(manager_)->Session());
    play->header().set_cseq(send_cseq_++);
    play->header().set_require_wfd_support(true);

    return std::unique_ptr<Message>(play);
  }

  virtual bool HandleReply(Reply* reply) override {
    return (reply->response_code() == 200);
  }
};

WfdSessionState::WfdSessionState(const InitParams& init_params, MessageHandlerPtr m6_handler, MessageHandlerPtr m16_handler)
  : MessageSequenceWithOptionalSetHandler(init_params) {
  AddSequencedHandler(m6_handler);
  AddSequencedHandler(make_ptr(new M7Handler(init_params)));

  AddOptionalHandler(make_ptr(new M3Handler(init_params)));
  AddOptionalHandler(make_ptr(new M4Handler(init_params)));
  AddOptionalHandler(make_ptr(new TeardownHandler(init_params)));
  AddOptionalHandler(m16_handler);
}

}  // namespace sink
}  // namespace wfd
