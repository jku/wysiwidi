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


#ifndef MIRAC_BROKER_HPP
#define MIRAC_BROKER_HPP

#include <glib.h>
#include <memory>
#include <map>
#include <vector>

#include "wfd/public/peer.h"
#include "mirac-network.hpp"

class MiracBroker : public wfd::Peer::Delegate
{
    public:
        MiracBroker (const std::string& listen_port);
        MiracBroker(const std::string& peer_address, const std::string& peer_port, uint timeout = 3000);
        virtual ~MiracBroker ();
        unsigned short get_host_port() const;
        std::string get_peer_address() const;
        virtual wfd::Peer* Peer() const = 0;
        void OnTimeout(uint timer_id);

    protected:
        enum ConnectionFailure {
            CONNECTION_TIMEOUT,
            CONNECTION_LOST,
        };

        // wfd::Peer::Delegate
        virtual void SendRTSPData(const std::string& data) override;
        virtual uint CreateTimer(int seconds);
        virtual void ReleaseTimer(uint timer_id);

        virtual void got_message(const std::string& data) {}
        virtual void on_connected() {};
        virtual void on_connection_failure(ConnectionFailure failure) {};

    private:
        struct SourceCallbackData {
            SourceCallbackData (MiracBroker *new_broker)
              : broker(new_broker), source(0) {}
            MiracBroker *broker;
            uint source;
        };

        static gboolean network_send_cb (gint fd, GIOCondition condition, gpointer data_ptr);
        static gboolean network_listen_cb (gint fd, GIOCondition condition, gpointer data_ptr);
        static gboolean network_connect_cb (gint fd, GIOCondition condition, gpointer data_ptr);
        static gboolean connection_send_cb (gint fd, GIOCondition condition, gpointer data_ptr);
        static gboolean connection_receive_cb (gint fd, GIOCondition condition, gpointer data_ptr);
        static gboolean try_connect(gpointer data_ptr);

        gboolean network_send_cb (gint fd, GIOCondition condition, SourceCallbackData *data);
        gboolean network_listen_cb (gint fd, GIOCondition condition, SourceCallbackData *data);
        gboolean network_connect_cb (gint fd, GIOCondition condition, SourceCallbackData *data);
        gboolean connection_send_cb (gint fd, GIOCondition condition, SourceCallbackData *data);
        gboolean connection_receive_cb (gint fd, GIOCondition condition, SourceCallbackData *data);
        void try_connect();

        void reset_network(MiracNetwork* connection);
        void reset_connection(MiracNetwork* connection);

        void handle_body(const std::string msg);
        void handle_header(const std::string msg);

        std::unique_ptr<MiracNetwork> network_;
        std::map<uint,std::unique_ptr<SourceCallbackData>>network_sources_;

        std::unique_ptr<MiracNetwork> connection_;
        std::map<uint,std::unique_ptr<SourceCallbackData>>connection_sources_;

        std::vector<uint> timers_;

        std::string peer_address_;
        std::string peer_port_;

        GTimer *connect_timer_;
        uint connect_wait_id_;
        uint connect_timeout_;
        static const uint connect_wait_ = 200;
};


#endif  /* MIRAC_BROKER_HPP */

