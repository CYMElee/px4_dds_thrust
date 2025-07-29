#include <rclcpp/rclcpp.hpp>
#include <geometry_msgs/msg/pose_stamped.hpp>
#include <std_msgs/msg/int16.hpp>
#include <std_msgs/msg/float64_multi_array.hpp>
#include <std_msgs/msg/float32_multi_array.hpp>
#include <px4_msgs/msg/offboard_control_mode.hpp>
#include <px4_msgs/msg/vehicle_attitude_setpoint.hpp>
#include "px4_msgs/msg/vehicle_attitude.hpp"
#include <px4_msgs/msg/vehicle_command.hpp>
#include <getch.h>
#include <Eigen/Dense>
#include <string>
#include <cmath>



class MAVControl_Node : public rclcpp::Node {
public:
    MAVControl_Node() : Node("mav_control_node") {
        rmw_qos_profile_t qos_profile = rmw_qos_profile_sensor_data;
        auto qos = rclcpp::QoS(rclcpp::QoSInitialization(qos_profile.history, 5), qos_profile);
        // Publishers
        T_pub_ = this->create_publisher<px4_msgs::msg::VehicleAttitudeSetpoint>(
            "/MAV1/fmu/in/vehicle_attitude_setpoint", qos);
        offboard_control_mode_publisher_ = this->create_publisher<px4_msgs::msg::OffboardControlMode>(
            "/MAV1/fmu/in/offboard_control_mode", qos);
        vehicle_command_publisher_ = this->create_publisher<px4_msgs::msg::VehicleCommand>(
            "/MAV1/fmu/in/vehicle_command", qos);


        offboard_setpoint_counter_ = 0;
        T = 0.05;
        
        timer2_ = this->create_wall_timer(std::chrono::milliseconds(10),std::bind(&MAVControl_Node::timer2_callback, this));
        timer_ = this->create_wall_timer(std::chrono::milliseconds(100),std::bind(&MAVControl_Node::timer_callback, this));
    }

private:
    std_msgs::msg::Float64MultiArray T_cmd_;
    px4_msgs::msg::VehicleAttitudeSetpoint T_;
    px4_msgs::msg::VehicleAttitudeSetpoint T_PREARM_;
    float T;
    uint64_t offboard_setpoint_counter_;

    rclcpp::TimerBase::SharedPtr timer_, timer2_;
    rclcpp::Publisher<px4_msgs::msg::VehicleAttitudeSetpoint>::SharedPtr T_pub_;
    rclcpp::Publisher<px4_msgs::msg::OffboardControlMode>::SharedPtr offboard_control_mode_publisher_;
    rclcpp::Publisher<px4_msgs::msg::VehicleCommand>::SharedPtr vehicle_command_publisher_;




    void initialize() {
        T_cmd_.data.resize(3);
        T_PREARM_.q_d[0] = 1; // w
        T_PREARM_.q_d[1] = 0; // x
        T_PREARM_.q_d[2] = 0; // y
        T_PREARM_.q_d[3] = 0; // z
        T_PREARM_.thrust_body[0] = 0.0;
        T_PREARM_.thrust_body[1] = 0.0;
        T_PREARM_.thrust_body[2] = -T; // Negative for upward thrust
        RCLCPP_INFO(this->get_logger(), "The index is %f",T);
        T_pub_->publish(T_PREARM_);
    }


    void publish_vehicle_command(uint16_t command, float param1 = 0.0, float param2 = 0.0) {
        px4_msgs::msg::VehicleCommand msg{};
        msg.param1 = param1;
        msg.param2 = param2;
        msg.command = command;
        msg.target_system = 1;
        msg.target_component = 1;
        msg.source_system = 1;
        msg.source_component = 1;
        msg.from_external = true;
        msg.timestamp = this->get_clock()->now().nanoseconds() / 1000;
        vehicle_command_publisher_->publish(msg);
    }

    void publish_offboard_control_mode() {
        px4_msgs::msg::OffboardControlMode msg{};
        msg.position = false;
        msg.velocity = false;
        msg.acceleration = false;
        msg.attitude = true;
        msg.body_rate = false;
        msg.timestamp = this->get_clock()->now().nanoseconds() / 1000;
        offboard_control_mode_publisher_->publish(msg);
    }

    void arm() {
        publish_vehicle_command(px4_msgs::msg::VehicleCommand::VEHICLE_CMD_COMPONENT_ARM_DISARM, 1.0, 0.0);
        RCLCPP_INFO(this->get_logger(), "Arm command send");
    }
    void thrust_seeting(){

        int c = getch();

        if (c != EOF) {
            switch (c)
            {
            case 'A':
                T=T+0.05;
                break;
            case 'D':
                T=T-0.05;
                break;

            }
        }


    }

    void timer_callback() {
        if (offboard_setpoint_counter_ == 10) {
            // Change to Offboard mode after 10 setpoints
            this->publish_vehicle_command(px4_msgs::msg::VehicleCommand::VEHICLE_CMD_DO_SET_MODE, 1, 6);
            // Arm the vehicle
            this->arm();
            offboard_setpoint_counter_ = 12;
        }

        publish_offboard_control_mode();
        // stop the counter after reaching 11
        if (offboard_setpoint_counter_ < 11) {
            offboard_setpoint_counter_++;
        }
    }

    void timer2_callback() {
        if (offboard_setpoint_counter_ == 12) {
                thrust_seeting();
                initialize();
            
        }
    }
};

int main(int argc, char **argv) {
    rclcpp::init(argc, argv);
    auto node = std::make_shared<MAVControl_Node>();
    rclcpp::spin(node);
    rclcpp::shutdown();
    return 0;
}