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
 * @file TcpClientPubSub.h
 *
 */

#ifndef TcpClientPubSub_H_
#define TcpClientPubSub_H_

#include "HelloWorldPubSubTypes.h"

#include <fastdds/dds/domain/DomainParticipant.hpp>
#include <fastdds/dds/subscriber/Subscriber.hpp>
#include <fastdds/dds/subscriber/DataReader.hpp>
#include <fastdds/dds/subscriber/DataReaderListener.hpp>
#include <fastdds/dds/subscriber/SampleInfo.hpp>

#include <fastdds/dds/publisher/Publisher.hpp>
#include <fastdds/dds/topic/Topic.hpp>
#include <fastdds/dds/publisher/DataWriter.hpp>
#include <fastdds/dds/publisher/DataWriterListener.hpp>


#include "HelloWorld.h"

#include <vector>

class TcpClientPubSub
{

    eprosima::fastdds::dds::DomainParticipant* participant_;

    eprosima::fastdds::dds::Subscriber* subscriber_;

    eprosima::fastdds::dds::Topic* commandTopic_;
    
    eprosima::fastdds::dds::DataReader* reader_;

    eprosima::fastdds::dds::TypeSupport type_;


    eprosima::fastdds::dds::Publisher* publisher_;


    eprosima::fastdds::dds::DataWriter* writer_;

    eprosima::fastdds::dds::Topic* responseTopic_;

    
public:

    class SubListener : public eprosima::fastdds::dds::DataReaderListener
    {
    public:

        SubListener()
            : matched_(0)
            , samples_(0)
        {
        }

        ~SubListener() override
        {
        }

        void on_data_available(
                eprosima::fastdds::dds::DataReader* reader) override;

        void on_subscription_matched(
                eprosima::fastdds::dds::DataReader* reader,
                const eprosima::fastdds::dds::SubscriptionMatchedStatus& info) override;

        HelloWorld hello_;

        int matched_;

        uint32_t samples_;
    }
    listener_;



    class PubListener : public eprosima::fastdds::dds::DataWriterListener {
    public:
      PubListener() : matched_(0), first_connected_(false) {}

      ~PubListener() override {}

      void on_publication_matched(
          eprosima::fastdds::dds::DataWriter *writer,
          const eprosima::fastdds::dds::PublicationMatchedStatus &info)
          override;

      int matched_;

      bool first_connected_;

    } pubListener_;

    TcpClientPubSub();

    virtual ~TcpClientPubSub();

    //!Initialize the subscriber
    bool init(
            const std::string& wan_ip,
            unsigned short port,
            bool use_tls,
            const std::vector<std::string>& whitelist);

    //!RUN the subscriber
    void run();

    //!Run the subscriber until number samples have been received.
    void run(
            uint32_t number);
};

#endif /* TcpClientPubSub_H_ */
