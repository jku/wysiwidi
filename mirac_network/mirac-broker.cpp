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

#include <glib-unix.h>
#include <algorithm>

#include "mirac-broker.hpp"

struct TimerCallbackData {
  TimerCallbackData(MiracBroker* delegate)
    : delegate_(delegate), timer_id_(0) {}
  MiracBroker* delegate_;
  uint timer_id_;
};

/* static C callback wrappers */
gboolean MiracBroker::connection_send_cb (gint fd, GIOCondition condition, gpointer data_ptr)
{
    auto data = static_cast<SourceCallbackData*> (data_ptr);
    return data->broker->connection_send_cb(fd, condition, data);
}

gboolean MiracBroker::connection_receive_cb (gint fd, GIOCondition condition, gpointer data_ptr)
{
    auto data = static_cast<SourceCallbackData*> (data_ptr);
    return data->broker->connection_receive_cb(fd, condition, data);
}

gboolean MiracBroker::network_send_cb (gint fd, GIOCondition condition, gpointer data_ptr)
{
    auto data = static_cast<SourceCallbackData*> (data_ptr);
    return data->broker->network_send_cb(fd, condition, data);
}

gboolean MiracBroker::network_listen_cb (gint fd, GIOCondition condition, gpointer data_ptr)
{
    auto data = static_cast<SourceCallbackData*> (data_ptr);
    return data->broker->network_listen_cb(fd, condition, data);
}

gboolean MiracBroker::network_connect_cb (gint fd, GIOCondition condition, gpointer data_ptr)
{
    auto data = static_cast<SourceCallbackData*> (data_ptr);
    return data->broker->network_connect_cb(fd, condition, data);
}

gboolean MiracBroker::try_connect (gpointer data_ptr)
{
    auto broker = static_cast<MiracBroker*> (data_ptr);
    broker->try_connect();
    return false;
}


gboolean MiracBroker::connection_send_cb (gint fd, GIOCondition condition, SourceCallbackData *data)
{
    try {
        if (!connection_->Send())
            return G_SOURCE_CONTINUE;
    } catch (MiracConnectionLostException &exception) {
        on_connection_failure(CONNECTION_LOST);
    } catch (std::exception &x) {
        g_warning("exception: %s", x.what());
    }

    connection_sources_.erase(data->source);
    return G_SOURCE_REMOVE;
}

gboolean MiracBroker::connection_receive_cb (gint fd, GIOCondition condition, SourceCallbackData *data)
{
    std::string msg;
    try {
        if (connection_->Receive(msg)) {
            g_log("rtsp", G_LOG_LEVEL_DEBUG, "Received RTSP message:\n%s", msg.c_str());
            got_message (msg);
        }
    } catch (MiracConnectionLostException &exception) {
        on_connection_failure(CONNECTION_LOST);
        connection_sources_.erase(data->source);
        return G_SOURCE_REMOVE;
    }
    return G_SOURCE_CONTINUE;
}

gboolean MiracBroker::network_send_cb (gint fd, GIOCondition condition, SourceCallbackData *data)
{
    try {
        if (!connection_->Send())
            return G_SOURCE_CONTINUE;
    } catch (MiracConnectionLostException &exception) {
        on_connection_failure(CONNECTION_LOST);
    } catch (std::exception &x) {
        g_warning("exception: %s", x.what());
    }

    network_sources_.erase(data->source);
    return G_SOURCE_REMOVE;
}

gboolean MiracBroker::network_listen_cb (gint fd, GIOCondition condition, SourceCallbackData *data)
{
    try {
        reset_connection(network_->Accept());
        on_connected();
    } catch (std::exception &x) {
        g_warning("exception: %s", x.what());
    }

    return G_SOURCE_CONTINUE;
}

gboolean MiracBroker::network_connect_cb (gint fd, GIOCondition condition, SourceCallbackData *data)
{
    try {
        if (!network_->Connect(NULL, NULL))
            return G_SOURCE_CONTINUE;

        reset_connection(network_.release());
        reset_network(NULL);
        on_connected();
    } catch (std::exception &x) {
        gdouble elapsed = 1000 * g_timer_elapsed(connect_timer_, NULL);
        if (elapsed + connect_wait_ > connect_timeout_) {
            on_connection_failure(CONNECTION_TIMEOUT);
        } else {
            connect_wait_id_ = g_timeout_add (connect_wait_, try_connect, this);
        }

        network_sources_.erase(data->source);
    }

    return G_SOURCE_REMOVE;
}

void MiracBroker::reset_network(MiracNetwork *network)
{
    for (auto it = network_sources_.begin(); it != network_sources_.end(); ++it)
        g_source_remove(it->first);
    network_sources_.clear();

    network_.reset(network);
}

void MiracBroker::reset_connection(MiracNetwork *connection)
{
    for (auto it = connection_sources_.begin(); it != connection_sources_.end(); ++it)
        g_source_remove(it->first);
    connection_sources_.clear();

    connection_.reset(connection);

    if (connection_) {
        auto data = new SourceCallbackData(this);
        data->source = g_unix_fd_add(connection_->GetHandle(), G_IO_IN,
                                    connection_receive_cb, data);
        connection_sources_[data->source].reset(data);
    }
}

void MiracBroker::try_connect()
{
    g_message("Trying to connect...");

    connect_wait_id_ = 0;
    reset_network(new MiracNetwork());

    auto data = new SourceCallbackData(this);
    if (network_->Connect(peer_address_.c_str(), peer_port_.c_str())) {
        data->source = g_unix_fd_add(network_->GetHandle(), G_IO_OUT,
                                    MiracBroker::network_send_cb, data);
    } else {
        data->source = g_unix_fd_add(network_->GetHandle(), G_IO_OUT,
                                    MiracBroker::network_connect_cb, data);
    }
    network_sources_[data->source].reset (data);
}

unsigned short MiracBroker::get_host_port() const
{
    return network_->GetHostPort();
}

std::string MiracBroker::get_peer_address() const
{
    return connection_->GetPeerAddress();
}

MiracBroker::MiracBroker (const std::string& listen_port):
    connect_timer_(NULL)
{
    reset_network(new MiracNetwork());

    network_->Bind(NULL, listen_port.c_str());

    auto data = new SourceCallbackData(this);
    data->source = g_unix_fd_add(network_->GetHandle(), G_IO_IN,
                                 MiracBroker::network_listen_cb, data);
    network_sources_[data->source].reset(data);
}

MiracBroker::MiracBroker(const std::string& peer_address, const std::string& peer_port, uint timeout):
    peer_address_(peer_address),
    peer_port_(peer_port),
    connect_timeout_(timeout)
{
    connect_timer_ = g_timer_new();
    try_connect();
}

MiracBroker::~MiracBroker ()
{
    reset_network(NULL);
    reset_connection(NULL);

    if (connect_timer_) {
        g_timer_destroy(connect_timer_);
        connect_timer_ = NULL;
    }

    if (connect_wait_id_ > 0) {
        g_source_remove(connect_wait_id_);
        connect_wait_id_ = 0;
    }
}

void MiracBroker::SendRTSPData(const std::string& data) {
  g_log("rtsp", G_LOG_LEVEL_DEBUG, "Sending RTSP message:\n%s", data.c_str());

  g_return_if_fail (connection_);

  if (!connection_->Send(data)) {
    auto data = new SourceCallbackData(this);
    data->source = g_unix_fd_add(connection_->GetHandle(), G_IO_OUT,
                                 connection_send_cb, data);
    connection_sources_[data->source].reset(data);
  }
}

static gboolean on_timeout(gpointer user_data) {
  TimerCallbackData* data = static_cast<TimerCallbackData*>(user_data);
  data->delegate_->OnTimeout(data->timer_id_);
  delete data;
  return FALSE;
}

void MiracBroker::OnTimeout(uint timer_id) {
  if (std::find(timers_.begin(), timers_.end(), timer_id) != timers_.end())
    Peer()->OnTimerEvent(timer_id);
}

uint MiracBroker::CreateTimer(int seconds) {
  TimerCallbackData* data = new TimerCallbackData(this);
  uint timer_id = g_timeout_add_seconds(
                        seconds,
                        on_timeout,
                        data);
  if (timer_id > 0) {
    data->timer_id_ = timer_id;
    timers_.push_back(timer_id);
  } else {
    delete data;
  }

  return timer_id;
}

void MiracBroker::ReleaseTimer(uint timer_id) {
  if (timer_id > 0) {
    auto it = std::find(timers_.begin(), timers_.end(), timer_id);
    if (it != timers_.end() )
      timers_.erase(it);
  }
}

