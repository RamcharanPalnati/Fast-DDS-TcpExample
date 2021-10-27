// Copyright 2016 Proyectos y Sistemas de Mantenimiento SL (eProsima).
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

/**
 * @file TcpClientPubSub.cpp
 *
 */

#include "TCPClientPubSub.h"
#include <fastdds/dds/domain/DomainParticipantFactory.hpp>
#include <fastdds/dds/subscriber/qos/DataReaderQos.hpp>
#include <fastdds/rtps/transport/TCPv4TransportDescriptor.h>
#include <fastrtps/utils/IPLocator.h>
#include <fastdds/dds/domain/DomainParticipant.hpp>
#include <fastdds/dds/publisher/Publisher.hpp>
#include <fastdds/dds/topic/Topic.hpp>
#include <fastdds/dds/publisher/DataWriter.hpp>
#include <fastdds/dds/publisher/DataWriterListener.hpp>


using namespace eprosima::fastdds::dds;
using namespace eprosima::fastdds::rtps;

using IPLocator = eprosima::fastrtps::rtps::IPLocator;

TcpClientPubSub::TcpClientPubSub()
    
    : participant_(nullptr)
    , publisher_(nullptr)
    , subscriber_(nullptr)
    , commandTopic_(nullptr)
    , writer_(nullptr)
    , reader_(nullptr)
    , responseTopic_(nullptr)
    , type_(new HelloWorldPubSubType())
    
    
{
}

bool TcpClientPubSub::init(
        const std::string& wan_ip,
        unsigned short port,
        bool use_tls,
        const std::vector<std::string>& whitelist)
{

    //CREATE THE PARTICIPANT
    DomainParticipantQos pqos;
    int32_t kind = LOCATOR_KIND_TCPv4;

    Locator initial_peer_locator;
    initial_peer_locator.kind = kind;

    std::shared_ptr<TCPv4TransportDescriptor> descriptor = std::make_shared<TCPv4TransportDescriptor>();

    for (std::string ip : whitelist)
    {
        descriptor->interfaceWhiteList.push_back(ip);
        std::cout << "Whitelisted " << ip << std::endl;
    }

    if (!wan_ip.empty())
    {
        IPLocator::setIPv4(initial_peer_locator, wan_ip);
        std::cout << wan_ip << ":" << port << std::endl;
    }
    else
    {
        IPLocator::setIPv4(initial_peer_locator, "127.0.0.1");
    }
    initial_peer_locator.port = port;
    pqos.wire_protocol().builtin.initialPeersList.push_back(initial_peer_locator); // Publisher's meta channel

    pqos.wire_protocol().builtin.discovery_config.leaseDuration = eprosima::fastrtps::c_TimeInfinite;
    pqos.wire_protocol().builtin.discovery_config.leaseDuration_announcementperiod = Duration_t(5, 0);
    pqos.name("Participant_client_ sub");

    pqos.transport().use_builtin_transports = false;


    pqos.transport().user_transports.push_back(descriptor);

    participant_ = DomainParticipantFactory::get_instance()->create_participant(0, pqos);

    if (participant_ == nullptr)
    {
        return false;
    }

    //REGISTER THE TYPE
    type_.register_type(participant_);

    //CREATE THE SUBSCRIBER
    subscriber_ = participant_->create_subscriber(SUBSCRIBER_QOS_DEFAULT);

    if (subscriber_ == nullptr)
    {
        return false;
    }

    //CREATE THE PUBLISHER
    publisher_ = participant_->create_publisher(PUBLISHER_QOS_DEFAULT);

    if (publisher_ == nullptr)
    {
        return false;
    }


    //CREATE THE TOPIC
    commandTopic_ = participant_->create_topic("tcp-command", "HelloWorld", TOPIC_QOS_DEFAULT);

    if (commandTopic_ == nullptr)
    {
        return false;
    }

    //CREATE THE DATAREADER
    DataReaderQos rqos;
    rqos.history().kind = eprosima::fastdds::dds::KEEP_LAST_HISTORY_QOS;
    rqos.history().depth = 30;
    rqos.resource_limits().max_samples = 50;
    rqos.resource_limits().allocated_samples = 20;
    rqos.reliability().kind = eprosima::fastdds::dds::RELIABLE_RELIABILITY_QOS;
    rqos.durability().kind = eprosima::fastdds::dds::TRANSIENT_LOCAL_DURABILITY_QOS;
    reader_ = subscriber_->create_datareader(commandTopic_, rqos, &listener_);

    if (reader_ == nullptr)
    {
        return false;
    }


     //CREATE THE TOPIC
    responseTopic_ = participant_->create_topic("tcp-response", "HelloWorld", TOPIC_QOS_DEFAULT);

    if (responseTopic_ == nullptr)
    {
        return false;
    }


     //CREATE THE DATAWRITER
    DataWriterQos wqos;
    wqos.history().kind = KEEP_LAST_HISTORY_QOS;
    wqos.history().depth = 30;
    wqos.resource_limits().max_samples = 50;
    wqos.resource_limits().allocated_samples = 20;
    wqos.reliable_writer_qos().times.heartbeatPeriod.seconds = 2;
    wqos.reliable_writer_qos().times.heartbeatPeriod.nanosec = 200 * 1000 * 1000;
    wqos.reliability().kind = RELIABLE_RELIABILITY_QOS;

    writer_ = publisher_->create_datawriter(responseTopic_, wqos, &pubListener_);

    if (writer_ == nullptr)
    {
        return false;
    }

    return true;
}

TcpClientPubSub::~TcpClientPubSub()
{
      if (reader_ != nullptr)
    {
        subscriber_->delete_datareader(reader_);
    }
    if (subscriber_ != nullptr)
    {
        participant_->delete_subscriber(subscriber_);
    }
    if (writer_ != nullptr)
    {
        publisher_->delete_datawriter(writer_);
    }
    if (publisher_ != nullptr)
    {
        participant_->delete_publisher(publisher_);
    }
    if (commandTopic_ != nullptr)
    {
        participant_->delete_topic(commandTopic_);
    }
    if (responseTopic_ != nullptr)
    {
        participant_->delete_topic(responseTopic_);
    }
    DomainParticipantFactory::get_instance()->delete_participant(participant_);
}

void TcpClientPubSub::SubListener::on_subscription_matched(
        DataReader*,
        const SubscriptionMatchedStatus& info)
{
    if (info.current_count_change == 1)
    {
        matched_ = info.total_count;
        std::cout << "tcp-command Subscriber matched" << std::endl;
    }
    else if (info.current_count_change == -1)
    {
        matched_ = info.total_count;
        std::cout << "tcp-command Subscriber unmatched" << std::endl;
    }
    else
    {
        std::cout << info.current_count_change
                  << " is not a valid value for SubscriptionMatchedStatus current count change-- tcp command " << std::endl;
    }
}

void TcpClientPubSub::SubListener::on_data_available(
        DataReader* reader)
{
    SampleInfo info;
    if (reader->take_next_sample(&hello_, &info) == ReturnCode_t::RETCODE_OK)
    {
        if (info.valid_data)
        {
            samples_++;
            // Print your structure data here.
            std::cout << "Message " << hello_.message() << " " << hello_.index() << " RECEIVED" << std::endl;
        }
    }
}

void TcpClientPubSub::run()
{
    std::cout << " Subscriber running. Please press enter to stop the Subscriber" << std::endl;
    std::cin.ignore();
}

void TcpClientPubSub::run(
        uint32_t number)
{
    std::cout << "[RTCP] Subscriber running until " << number << "samples have been received" << std::endl;
    while (number < this->listener_.samples_)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(15000)); //15secs
    }
}




void TcpClientPubSub::PubListener::on_publication_matched(
        eprosima::fastdds::dds::DataWriter*,
        const eprosima::fastdds::dds::PublicationMatchedStatus& info)
{
    if (info.current_count_change == 1)
    {
        matched_ = info.total_count;
        first_connected_ = true;
        std::cout << "tcp-response Publisher matched" << std::endl;
    }
    else if (info.current_count_change == -1)
    {
        matched_ = info.total_count;
        std::cout << "tcp-response Publisher unmatched" << std::endl;
    }
    else
    {
        std::cout << info.current_count_change
                  << " is not a valid value for PublicationMatchedStatus current count change --tcp-response" << std::endl;
    }
}