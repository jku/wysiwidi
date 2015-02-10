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

#include <iostream>
#include <netinet/in.h> // htons()

#include "source-app.h"

void SourceApp::on_initialized(P2P::Client *client)
{
    scan();
}

void SourceApp::on_peer_added(P2P::Client *client, std::shared_ptr<P2P::Peer> peer)
{
    if (peer->device_type() == P2P::PRIMARY_SINK) {
        peer->set_observer(this);
    }
}

void SourceApp::on_peer_removed(P2P::Client *client, std::shared_ptr<P2P::Peer> peer)
{
    for (auto it = peers_.begin(); it != peers_.end();) {
        if (it->second == peer.get()) {
            peers_.erase (it++);
            return;
        } else {
            ++it;
        }
    }
}

void SourceApp::on_initialized(P2P::Peer *peer)
{
    peers_[peer_index_] = peer;
    std::cout << "  "<< peer_index_ << " : " << peer->name() << std::endl;
    peer_index_++;
}

void SourceApp::scan()
{
    std::cout << "* Now scanning for sinks..." << std::endl;
    p2p_client_->scan();
}

bool SourceApp::connect(uint peer_index)
{
    auto it = peers_.find (peer_index);
    if (it == peers_.end()) {
        std::cout << "No such peer" << std::endl;
        return false;
    }

    it->second->connect ();
    p2p_client_->set_peer_service_available (false);

    return true;
}

void SourceApp::on_availability_changed(P2P::Peer *peer)
{
    if (!peer->is_available()) {
        std::cout << "* Sink " << peer->remote_host() << " is no longer available."  << std::endl;
        p2p_client_->set_peer_service_available (true);
    } else {
        std::cout << "* Sink " << peer->remote_host() << " is now available, waiting for RTSP connection." << std::endl;
    }
}

SourceApp::SourceApp(int port) :
    peer_index_(0)
{
    // Create a information element for a simple WFD Source
    P2P::InformationElement ie;
    auto sub_element = P2P::new_subelement(P2P::DEVICE_INFORMATION);
    auto dev_info = (P2P::DeviceInformationSubelement*)sub_element;

    // TODO InformationElement could have constructors for this stuff...
    dev_info->session_management_control_port = htons(port);
    dev_info->maximum_throughput = htons(50);
    dev_info->field1.device_type = P2P::SOURCE;
    dev_info->field1.session_availability = true;
    ie.add_subelement(sub_element);

    std::cout << "* Registering Wi-Fi Display Source with IE " << ie.to_string() <<  std::endl;

    // register the P2P service with connman
    p2p_client_.reset(new P2P::Client(&ie, this));

    source_.reset(new MiracBrokerSource(port));
}

SourceApp::~SourceApp()
{
}
