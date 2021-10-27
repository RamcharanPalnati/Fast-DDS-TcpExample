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
 * @file TcpServerPubSub.cpp
 *
 */


#include "TcpServerPubSub.h"
#include <fastdds/dds/domain/DomainParticipantFactory.hpp>
#include <fastdds/rtps/transport/TCPv4TransportDescriptor.h>
#include <fastrtps/utils/IPLocator.h>

#include <thread>


using namespace eprosima::fastdds::dds;
using namespace eprosima::fastdds::rtps;

TcpServerPubSub::TcpServerPubSub()
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

bool TcpServerPubSub::init(
        const std::string& wan_ip,
        unsigned short port,
        bool use_tls,
        const std::vector<std::string>& whitelist)
{
    stop_ = false;
    hello_.index(0);
    hello_.message("HelloWorld");

    //CREATE THE PARTICIPANT
    DomainParticipantQos pqos;
    pqos.wire_protocol().builtin.discovery_config.leaseDuration = eprosima::fastrtps::c_TimeInfinite;
    pqos.wire_protocol().builtin.discovery_config.leaseDuration_announcementperiod =
            eprosima::fastrtps::Duration_t(5, 0);
    pqos.name("Participant_server_pub");

    pqos.transport().use_builtin_transports = false;

    std::shared_ptr<TCPv4TransportDescriptor> descriptor = std::make_shared<TCPv4TransportDescriptor>();

    for (std::string ip : whitelist)
    {
        descriptor->interfaceWhiteList.push_back(ip);
        std::cout << "Whitelisted " << ip << std::endl;
    }


    descriptor->sendBufferSize = 0;
    descriptor->receiveBufferSize = 0;

    if (!wan_ip.empty())
    {
        descriptor->set_WAN_address(wan_ip);
        std::cout << wan_ip << ":" << port << std::endl;
    }
    descriptor->add_listener_port(port);
    pqos.transport().user_transports.push_back(descriptor);

    participant_ = DomainParticipantFactory::get_instance()->create_participant(0, pqos);

    if (participant_ == nullptr)
    {
        return false;
    }

    //REGISTER THE TYPE
    type_.register_type(participant_);

    //CREATE THE PUBLISHER
    publisher_ = participant_->create_publisher(PUBLISHER_QOS_DEFAULT);

    if (publisher_ == nullptr)
    {
        return false;
    }

    //CREATE THE SUBSCRIBER
    subscriber_ = participant_->create_subscriber(SUBSCRIBER_QOS_DEFAULT);

    if (subscriber_ == nullptr)
    {
        return false;
    }


    //CREATE THE TOPIC
    commandTopic_ = participant_->create_topic("tcp-command", "HelloWorld", TOPIC_QOS_DEFAULT);

    if (commandTopic_ == nullptr)
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

    writer_ = publisher_->create_datawriter(commandTopic_, wqos, &listener_);

    if (writer_ == nullptr)
    {
        return false;
    }



    //CREATE THE TOPIC
    responseTopic_ = participant_->create_topic("tcp-response", "HelloWorld", TOPIC_QOS_DEFAULT);

    if (responseTopic_ == nullptr)
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
    reader_ = subscriber_->create_datareader(responseTopic_, rqos, &subListener_);

    if (reader_ == nullptr)
    {
        return false;
    }

    return true;
}

TcpServerPubSub::~TcpServerPubSub()
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

void TcpServerPubSub::PubListener::on_publication_matched(
        eprosima::fastdds::dds::DataWriter*,
        const eprosima::fastdds::dds::PublicationMatchedStatus& info)
{
    if (info.current_count_change == 1)
    {
        matched_ = info.total_count;
        first_connected_ = true;
        std::cout << "tcp-command Publisher matched" << std::endl;
    }
    else if (info.current_count_change == -1)
    {
        matched_ = info.total_count;
        std::cout << "tcp-command Publisher unmatched" << std::endl;
    }
    else
    {
        std::cout << info.current_count_change
                  << " is not a valid value for PublicationMatchedStatus current count change --tcp-command" << std::endl;
    }
}

void TcpServerPubSub::runThread(
        uint32_t samples,
        long sleep_ms)
{
    if (samples == 0)
    {
        while (!stop_)
        {
            if (publish(false))
            {
                //logError(HW, "SENT " <<  hello_.index());
                std::cout << "tcp-command Message: " << hello_.message() << " with index: "
                          << hello_.index() << " SENT" << std::endl;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10000));
        }
    }
    else
    {
        for (uint32_t i = 0; i < samples; ++i)
        {
            if (!publish())
            {
                --i;
            }
            else
            {
                std::cout << "tcp-command  Message: " << hello_.message() << " with index: "
                          << hello_.index() << " SENT" << std::endl;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10000));
        }
    }
}

void TcpServerPubSub::run(
        uint32_t samples,
        long sleep_ms)
{
    std::thread thread(&TcpServerPubSub::runThread, this, samples, sleep_ms);
    if (samples == 0)
    {
        std::cout << "tcp-command Publisher running. Please press enter to stop_ the Publisher at any time." << std::endl;
        std::cin.ignore();
        stop_ = true;
    }
    else
    {
        std::cout << "tcp-command Publisher running " << samples << " samples." << std::endl;
    }
    thread.join();
}

bool TcpServerPubSub::publish(
        bool waitForListener)
{
    if (listener_.first_connected_ || !waitForListener || listener_.matched_ > 0)
    {
        hello_.index(hello_.index() + 1);
        writer_->write((void*)&hello_);
        return true;
    }
    return false;
}



void TcpServerPubSub::SubListener::on_subscription_matched(
        DataReader*,
        const SubscriptionMatchedStatus& info)
{
    if (info.current_count_change == 1)
    {
        matched_ = info.total_count;
        std::cout << "tcp-response Subscriber matched" << std::endl;
    }
    else if (info.current_count_change == -1)
    {
        matched_ = info.total_count;
        std::cout << "tcp-response Subscriber unmatched" << std::endl;
    }
    else
    {
        std::cout << info.current_count_change
                  << " is not a valid value for SubscriptionMatchedStatus current count change-- tcp response " << std::endl;
    }
}



void TcpServerPubSub::SubListener::on_data_available(
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