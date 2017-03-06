#include <iostream>
#include <string>
#include <sstream>
#include <thread>
#include <chrono>

#include <Poco/Net/SocketAddress.h>
#include <Poco/Net/StreamSocket.h>
#include <Poco/Net/SocketStream.h>

#include <atomics/binary_stream.h>

#include <messages/kinect_messages.h>
#include <messages/binary_codec.h>
#include <messages/message_coder.h>

#include <messages/input_tcp_device.h>

#include <ros/ros.h>

#include <tf/transform_broadcaster.h>

#include <kinect_bridge2/KinectSpeech.h>
#include <kinect_bridge2/KinectBodies.h>
#include <kinect_bridge2/KinectDepthImage.h>

class KinectBridge2Client
{
public:
    typedef kinect_bridge2::KinectSpeech _KinectSpeechMsg;
    typedef kinect_bridge2::KinectSpeechPhrase _KinectSpeechPhraseMsg;

    typedef kinect_bridge2::KinectBodies _KinectBodiesMsg;
    typedef kinect_bridge2::KinectBody _KinectBodyMsg;
    typedef kinect_bridge2::KinectJoint _KinectJointMsg;

    typedef kinect_bridge2::KinectDepthImage _KinectDepthMsg;
    typedef kinect_bridge2::KinectDepthImageInfo _KinectDepthInfoMsg;

    ros::NodeHandle nh_rel_;

    ros::Publisher kinect_speech_pub_;
    ros::Publisher kinect_bodies_pub_;
    ros::Publisher kinect_depth_pub_;

    InputTCPDevice kinect_bridge_client_;

    uint32_t message_count_;

    MessageCoder<BinaryCodec<> > binary_message_coder_;

    tf::TransformBroadcaster transform_broadcaster_;

    KinectBridge2Client( ros::NodeHandle & nh_rel )
    :
        nh_rel_( nh_rel ),
        kinect_speech_pub_( nh_rel_.advertise<_KinectSpeechMsg>( "speech", 10 ) ),
        kinect_bodies_pub_( nh_rel_.advertise<_KinectBodiesMsg>( "bodies", 10 ) ),
        kinect_bodies_pub_( nh_rel_.advertise<_KinectDepthMsg>( "depth", 10 ) ),
        kinect_bridge_client_( getParam<std::string>( nh_rel_, "server_ip", "localhost" ), getParam<int>( nh_rel_, "server_port", 5903 ) ),
        message_count_( 0 )
    {
        //
    }

    template<class __Data>
    static __Data getParam( ros::NodeHandle & nh, std::string const & param_name, __Data const & default_value )
    {
        __Data result;
        if( nh.getParam( param_name, result ) ) return result;
        return default_value;
    }

    void spin()
    {
        auto last_update = std::chrono::high_resolution_clock::now();
        while( ros::ok() )
        {
            auto now = std::chrono::high_resolution_clock::now();

            if( std::chrono::duration_cast<std::chrono::milliseconds>( now - last_update ).count() >= 1000 )
            {
                last_update = now;
                std::cout << "processed " << message_count_ << " messages" << std::endl;
            }

            try
            {
                if( !kinect_bridge_client_.input_socket_.impl()->initialized() )
                {
                    std::cout << "no server connection" << std::endl;
                    std::this_thread::sleep_for( std::chrono::milliseconds( 100 ) );
                    kinect_bridge_client_.openInput();
                    continue;
                }

                CodedMessage<> binary_coded_message;
                kinect_bridge_client_.pull( binary_coded_message );
                processKinectMessage( binary_coded_message );
                message_count_ ++;
 //               std::cout << "message processed" << std::endl;
            }
            catch( messages::MessageException & e )
            {
                std::cout << e.what() << std::endl;
                std::this_thread::sleep_for( std::chrono::milliseconds( 100 ) );
            }
            catch( std::exception & e )
            {
                std::cout << e.what() << std::endl;
                std::this_thread::sleep_for( std::chrono::milliseconds( 100 ) );
            }

            ros::spinOnce();
        }
    }

    void processKinectMessage( CodedMessage<> & coded_message )
    {
        auto & coded_header = coded_message.header_;
//        std::cout << "processing message type: " << coded_message.header_.payload_type_ << std::endl;

        if( coded_header.payload_id_ == KinectSpeechMessage::ID() )
        {
            auto kinect_speech_message = binary_message_coder_.decode<KinectSpeechMessage>( coded_message );

            auto & header = kinect_speech_message.header_;
            auto & payload = kinect_speech_message.payload_;

            _KinectSpeechMsg ros_kinect_speech_message;

            for( size_t i = 0; i < payload.size(); ++i )
            {
                _KinectSpeechPhraseMsg ros_kinect_speech_phrase_message;
                ros_kinect_speech_phrase_message.tag = payload[i].tag_;
                ros_kinect_speech_phrase_message.confidence = payload[i].confidence_;
                ros_kinect_speech_message.phrases.emplace_back( std::move( ros_kinect_speech_phrase_message ) );
            }

            kinect_speech_pub_.publish( ros_kinect_speech_message );
        }
        if( coded_header.payload_id_ == KinectDepthImageMessage::ID() )
        {
            auto kinect_depth_message = binary_message_coder_.decode<KinectDepthImageMessage>( coded_message );

            auto & header = kinect_depth_message.header_;
            int & payload = kinect_depth_message.payload_;

            _KinectDepthMsg ros_kinect_depth_message;

            for( size_t i = 0; i < payload.size(); ++i )
            {
                _KinectDepthInfoMsg ros_kinect_depth_info_message;
                ros_kinect_depth_info_message.min_reliable_distance_ = payload[i].min_reliable_distance_;
                ros_kinect_depth_info_message.max_reliable_distance_ = payload[i].max_reliable_distance_;
                
                ros_kinect_depth_message.depthInfo = ros_kinect_depth_info_message;                
            }

            ros_kinect_depth_message.height_ = header.height_;
            ros_kinect_depth_message.width_ = header.width_;
            ros_kinect_depth_message.num_channels_ = header.num_channels_;
            ros_kinect_depth_message.pixel_depth_ = header.pixel_depth_;
            ros_kinect_depth_message.encoding_ = header.encoding_;

            kinect_depth_pub_.publish( ros_kinect_depth_message );
        }
        else if( coded_header.payload_id_ == KinectBodiesMessage::ID() )
        {
            auto bodies_msg = binary_message_coder_.decode<KinectBodiesMessage>( coded_message );

            auto const & header = bodies_msg.header_;
            auto const & payload = bodies_msg.payload_;

            _KinectBodiesMsg ros_bodies_msg;

            // get map of KinectJointMessage::JointType -> human-readable name
            auto const & joint_names_map = KinectJointMessage::getJointNamesMap();

            // for each body message
            for( size_t body_idx = 0; body_idx < payload.size(); ++body_idx )
            {
                _KinectBodyMsg ros_body_msg;
                auto const & body_msg = payload[body_idx];

                ros_body_msg.is_tracked = body_msg.is_tracked_;
                ros_body_msg.hand_state_left = static_cast<uint8_t>( body_msg.hand_state_left_ );
                ros_body_msg.hand_state_right = static_cast<uint8_t>( body_msg.hand_state_right_ );

                auto const & joints_msg = body_msg.joints_;
                auto & ros_joints_msg = ros_body_msg.joints;

                std::stringstream tf_frame_basename_ss;
                tf_frame_basename_ss << "/kinect_client/skeleton" << body_idx << "/";

                // for each joint message
                for( size_t joint_idx = 0; joint_idx < joints_msg.size(); ++joint_idx )
                {
                    auto const & joint_msg = joints_msg[joint_idx];
                    _KinectJointMsg ros_joint_msg;

                    ros_joint_msg.joint_type = static_cast<uint8_t>( joint_msg.joint_type_ );
                    ros_joint_msg.tracking_state = static_cast<uint8_t>( joint_msg.tracking_state_ );

                    ros_joint_msg.position.x = joint_msg.position_.x;
                    ros_joint_msg.position.y = joint_msg.position_.y;
                    ros_joint_msg.position.z = joint_msg.position_.z;

                    ros_joint_msg.orientation.x = joint_msg.orientation_.x;
                    ros_joint_msg.orientation.y = joint_msg.orientation_.y;
                    ros_joint_msg.orientation.z = joint_msg.orientation_.z;
                    ros_joint_msg.orientation.w = joint_msg.orientation_.w;

                    ros_joints_msg.emplace_back( std::move( ros_joint_msg ) );

                    tf::Transform joint_transform
                    (
                        joint_msg.tracking_state_ == KinectJointMessage::TrackingState::TRACKED ? tf::Quaternion( joint_msg.orientation_.x, joint_msg.orientation_.y, joint_msg.orientation_.z, joint_msg.orientation_.w ).normalized() : tf::Quaternion( 0, 0, 0, 1 ),
                        tf::Vector3( joint_msg.position_.x, joint_msg.position_.y, joint_msg.position_.z )
                    );

                    // if the rotation is nan, zero it out to make TF happy
                    if( std::isnan( joint_transform.getRotation().getAngle() ) ) joint_transform.setRotation( tf::Quaternion( 0, 0, 0, 1 ) );

                    static tf::Transform const trunk_norm_rotation_tf( tf::Quaternion( -M_PI_2, -M_PI_2, 0 ).normalized() );

                    switch( joint_msg.joint_type_ )
                    {
                    case KinectJointMessage::JointType::SPINE_BASE:
                    case KinectJointMessage::JointType::SPINE_MID:
                    case KinectJointMessage::JointType::NECK:
                    case KinectJointMessage::JointType::HEAD:
                    case KinectJointMessage::JointType::SPINE_SHOULDER:
                        joint_transform *= trunk_norm_rotation_tf;
                        break;
                    default:
                        break;
                    }

                    transform_broadcaster_.sendTransform( tf::StampedTransform( joint_transform, ros::Time::now(), "/kinect", tf_frame_basename_ss.str() + joint_names_map.find(joint_msg.joint_type_)->second ) );
                }
                ros_bodies_msg.bodies.emplace_back( std::move( ros_body_msg ) );
            }
            kinect_bodies_pub_.publish( ros_bodies_msg );
        }
    }
};

int main( int argc, char ** argv )
{
    ros::init( argc, argv, "kinect_client" );
    ros::NodeHandle nh_rel( "~" );

    KinectBridge2Client kinect_bridge2_client( nh_rel );

    kinect_bridge2_client.spin();

    return 0;
}
