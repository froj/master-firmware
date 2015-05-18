
#include <uavcan/uavcan.hpp>
#include <uavcan/protocol/NodeStatus.hpp>
#include <cvra/motor/config/VelocityPID.hpp>
#include <cvra/motor/config/PositionPID.hpp>
#include <cvra/motor/config/CurrentPID.hpp>
#include <cvra/motor/config/LoadConfiguration.hpp>
#include <cvra/motor/config/EnableMotor.hpp>
#include <cvra/motor/config/FeedbackStream.hpp>
#include <cvra/motor/control/Velocity.hpp>
#include <cvra/motor/control/Position.hpp>
#include <cvra/motor/control/Torque.hpp>
#include <cvra/motor/control/Voltage.hpp>
#include <cvra/motor/control/Trajectory.hpp>
#include "uavcan_node.h"
#include "motor_driver.h"
#include "motor_driver_uavcan.h"
#include "uavcan_node_private.hpp"

using namespace uavcan_node;

struct can_driver_s {
    uavcan::ServiceClient<cvra::motor::config::VelocityPID> speed_pid_client;
    uavcan::ServiceClient<cvra::motor::config::PositionPID> position_pid_client;
    uavcan::ServiceClient<cvra::motor::config::CurrentPID> current_pid_client;
    uavcan::ServiceClient<cvra::motor::config::LoadConfiguration> config_client;
    uavcan::ServiceClient<cvra::motor::config::EnableMotor> enable_client;
    uavcan::Publisher<cvra::motor::config::FeedbackStream> feedback_stream_pub;
    uavcan::Publisher<cvra::motor::control::Velocity> velocity_pub;
    uavcan::Publisher<cvra::motor::control::Position> position_pub;
    uavcan::Publisher<cvra::motor::control::Torque> torque_pub;
    uavcan::Publisher<cvra::motor::control::Voltage> voltage_pub;
    uavcan::Publisher<cvra::motor::control::Trajectory> trajectory_pub;
    bool enabled; // state of the motor board
    can_driver_s():
        speed_pid_client(getNode()),
        position_pid_client(getNode()),
        current_pid_client(getNode()),
        config_client(getNode()),
        enable_client(getNode()),
        feedback_stream_pub(getNode()),
        velocity_pub(getNode()),
        position_pub(getNode()),
        torque_pub(getNode()),
        voltage_pub(getNode()),
        trajectory_pub(getNode())
    {
        enabled = false;

        speed_pid_client.setCallback([](const uavcan::ServiceCallResult<cvra::motor::config::VelocityPID>& call_result)
                {
                if (call_result.status != uavcan::ServiceCallResult<cvra::motor::config::VelocityPID>::Success) {
                    chSysHalt("Service call failed !\r\n");
                }
                });

        position_pid_client.setCallback([](const uavcan::ServiceCallResult<cvra::motor::config::PositionPID>& call_result)
                {
                if (call_result.status != uavcan::ServiceCallResult<cvra::motor::config::PositionPID>::Success) {
                    chSysHalt("Service call failed !\r\n");
                }
                });

        current_pid_client.setCallback([](const uavcan::ServiceCallResult<cvra::motor::config::CurrentPID>& call_result)
                {
                if (call_result.status != uavcan::ServiceCallResult<cvra::motor::config::CurrentPID>::Success) {
                    chSysHalt("Service call failed !\r\n");
                }
                });

        config_client.setCallback([](const uavcan::ServiceCallResult<cvra::motor::config::LoadConfiguration>& call_result)
                {
                if (call_result.status != uavcan::ServiceCallResult<cvra::motor::config::LoadConfiguration>::Success) {
                    chSysHalt("Service call failed !\r\n");
                }
                });

        enable_client.setCallback([](const uavcan::ServiceCallResult<cvra::motor::config::EnableMotor>& call_result)
                {
                if (call_result.status != uavcan::ServiceCallResult<cvra::motor::config::EnableMotor>::Success) {
                    chSysHalt("Service call failed !\r\n");
                }
                });
    }
};


static void driver_allocation(motor_driver_t *d)
{
    if (d->can_driver == NULL) {
        d->can_driver = new struct can_driver_s;
        if (d->can_driver == NULL) {
            chSysHalt("motor driver memory allocation failed");
        }
    }
}


static void update_motor_can_id(motor_driver_t *d)
{
    int node_id = motor_driver_get_can_id(d);
    if (node_id == CAN_ID_NOT_SET) {
        node_id = bus_enumerator_get_can_id(&bus_enumerator, motor_driver_get_id(d));
        if (node_id != BUS_ENUMERATOR_CAN_ID_NOT_SET
            && node_id != BUS_ENUMERATOR_STRING_ID_NOT_FOUND) {
            motor_driver_set_can_id(d, node_id);
        }
    }
}

static void send_stream_config(struct can_driver_s *can_drv, int node_id, uint8_t stream, float frequency)
{
    cvra::motor::config::FeedbackStream feedback_stream_config;

    feedback_stream_config.stream = stream;
    if (frequency == 0) {
        feedback_stream_config.enabled = false;
    } else {
        feedback_stream_config.enabled = true;
    }
    feedback_stream_config.frequency = frequency;

    can_drv->feedback_stream_pub.unicast(feedback_stream_config, node_id);
}


extern "C"
void motor_driver_send_initial_config(motor_driver_t *d)
{
    cvra::motor::config::LoadConfiguration::Request config_msg;

    update_motor_can_id(d);
    int node_id = motor_driver_get_can_id(d);
    if (node_id == CAN_ID_NOT_SET) {
        return;
    }
    driver_allocation(d);
    struct can_driver_s *can_drv = (struct can_driver_s*)d->can_driver;

    config_msg.position_pid.kp = parameter_scalar_get(&d->config.position_pid.kp);
    config_msg.position_pid.ki = parameter_scalar_get(&d->config.position_pid.ki);
    config_msg.position_pid.kd = parameter_scalar_get(&d->config.position_pid.kd);
    config_msg.position_pid.ilimit = parameter_scalar_get(&d->config.position_pid.ilimit);
    config_msg.velocity_pid.kp = parameter_scalar_get(&d->config.velocity_pid.kp);
    config_msg.velocity_pid.ki = parameter_scalar_get(&d->config.velocity_pid.ki);
    config_msg.velocity_pid.kd = parameter_scalar_get(&d->config.velocity_pid.kd);
    config_msg.velocity_pid.ilimit = parameter_scalar_get(&d->config.velocity_pid.ilimit);
    config_msg.current_pid.kp = parameter_scalar_get(&d->config.current_pid.kp);
    config_msg.current_pid.ki = parameter_scalar_get(&d->config.current_pid.ki);
    config_msg.current_pid.kd = parameter_scalar_get(&d->config.current_pid.kd);
    config_msg.current_pid.ilimit = parameter_scalar_get(&d->config.current_pid.ilimit);

    config_msg.torque_limit = parameter_scalar_get(&d->config.torque_limit);
    config_msg.velocity_limit = parameter_scalar_get(&d->config.velocity_limit);
    config_msg.acceleration_limit = parameter_scalar_get(&d->config.acceleration_limit);
    config_msg.low_batt_th = parameter_scalar_get(&d->config.low_batt_th);

    config_msg.thermal_capacity = parameter_scalar_get(&d->config.thermal_capacity);
    config_msg.thermal_resistance = parameter_scalar_get(&d->config.thermal_resistance);
    config_msg.thermal_current_gain = parameter_scalar_get(&d->config.thermal_current_gain);
    config_msg.max_temperature = parameter_scalar_get(&d->config.max_temperature);

    config_msg.torque_constant = parameter_scalar_get(&d->config.torque_constant);
    config_msg.transmission_ratio_p = parameter_integer_get(&d->config.transmission_ratio_p);
    config_msg.transmission_ratio_q = parameter_integer_get(&d->config.transmission_ratio_q);
    config_msg.motor_encoder_steps_per_revolution = parameter_integer_get(&d->config.motor_encoder_steps_per_revolution);
    config_msg.second_encoder_steps_per_revolution = parameter_integer_get(&d->config.second_encoder_steps_per_revolution);
    config_msg.potentiometer_gain = parameter_scalar_get(&d->config.potentiometer_gain);

    config_msg.mode = parameter_integer_get(&d->config.mode); // todo !

    can_drv->config_client.call(node_id, config_msg);

    can_drv->enabled = false;

}

extern "C"
void motor_driver_uavcan_update_config(motor_driver_t *d)
{

    update_motor_can_id(d);
    int node_id = motor_driver_get_can_id(d);
    if (node_id == CAN_ID_NOT_SET) {
        return;
    }
    driver_allocation(d);
    struct can_driver_s *can_drv = (struct can_driver_s*)d->can_driver;

    if (parameter_namespace_contains_changed(&d->config.control)) {
        if (parameter_namespace_contains_changed(&d->config.position_pid.root)) {
            cvra::motor::config::PositionPID::Request request;
            request.pid.kp = parameter_scalar_get(&d->config.position_pid.kp);
            request.pid.ki = parameter_scalar_get(&d->config.position_pid.ki);
            request.pid.kd = parameter_scalar_get(&d->config.position_pid.kd);
            request.pid.ilimit = parameter_scalar_get(&d->config.position_pid.ilimit);
            can_drv->position_pid_client.call(node_id, request);
        }

        if (parameter_namespace_contains_changed(&d->config.velocity_pid.root)) {
            cvra::motor::config::VelocityPID::Request request;
            request.pid.kp = parameter_scalar_get(&d->config.velocity_pid.kp);
            request.pid.ki = parameter_scalar_get(&d->config.velocity_pid.ki);
            request.pid.kd = parameter_scalar_get(&d->config.velocity_pid.kd);
            request.pid.ilimit = parameter_scalar_get(&d->config.velocity_pid.ilimit);
            can_drv->speed_pid_client.call(node_id, request);
        }

        if (parameter_namespace_contains_changed(&d->config.current_pid.root)) {
            cvra::motor::config::CurrentPID::Request request;
            request.pid.kp = parameter_scalar_get(&d->config.current_pid.kp);
            request.pid.ki = parameter_scalar_get(&d->config.current_pid.ki);
            request.pid.kd = parameter_scalar_get(&d->config.current_pid.kd);
            request.pid.ilimit = parameter_scalar_get(&d->config.current_pid.ilimit);
            can_drv->current_pid_client.call(node_id, request);
        }
    }
    if (parameter_namespace_contains_changed(&d->config.stream)) {
        if (parameter_changed(&d->config.current_pid_stream)) {
            send_stream_config(can_drv,
                               node_id,
                               cvra::motor::config::FeedbackStream::STREAM_CURRENT_PID,
                               parameter_scalar_get(&d->config.current_pid_stream));
        }
        if (parameter_changed(&d->config.velocity_pid_stream)) {
            send_stream_config(can_drv,
                               node_id,
                               cvra::motor::config::FeedbackStream::STREAM_VELOCITY_PID,
                               parameter_scalar_get(&d->config.velocity_pid_stream));
        }
        if (parameter_changed(&d->config.position_pid_stream)) {
            send_stream_config(can_drv,
                               node_id,
                               cvra::motor::config::FeedbackStream::STREAM_POSITION_PID,
                               parameter_scalar_get(&d->config.position_pid_stream));
        }
        if (parameter_changed(&d->config.index_stream)) {
            send_stream_config(can_drv,
                               node_id,
                               cvra::motor::config::FeedbackStream::STREAM_INDEX,
                               parameter_scalar_get(&d->config.index_stream));
        }
        if (parameter_changed(&d->config.encoder_pos_stream)) {
            send_stream_config(can_drv,
                               node_id,
                               cvra::motor::config::FeedbackStream::STREAM_MOTOR_ENCODER,
                               parameter_scalar_get(&d->config.encoder_pos_stream));
        }
        if (parameter_changed(&d->config.motor_pos_stream)) {
            send_stream_config(can_drv,
                               node_id,
                               cvra::motor::config::FeedbackStream::STREAM_MOTOR_POSITION,
                               parameter_scalar_get(&d->config.motor_pos_stream));
        }
        if (parameter_changed(&d->config.motor_torque_stream)) {
            send_stream_config(can_drv,
                               node_id,
                               cvra::motor::config::FeedbackStream::STREAM_MOTOR_TORQUE,
                               parameter_scalar_get(&d->config.motor_torque_stream));
        }
    }
    if (parameter_namespace_contains_changed(&d->config.root)) {
        // still some changed parameters: need to resend full config
        motor_driver_send_initial_config(d);
    }
}

void motor_enable(can_driver_s *d, int node_id)
{
    cvra::motor::config::EnableMotor::Request enable_msg;
    if (!d->enabled) {
        d->enabled = true;
        enable_msg.enable = true;
        d->enable_client.call(node_id, enable_msg);
    }
}

void motor_disable(can_driver_s *d, int node_id)
{
    cvra::motor::config::EnableMotor::Request enable_msg;
    if (d->enabled) {
        d->enabled = false;
        enable_msg.enable = false;
        d->enable_client.call(node_id, enable_msg);
    }
}

extern "C"
void motor_driver_uavcan_send_setpoint(motor_driver_t *d)
{
    cvra::motor::control::Position position_setpoint;
    cvra::motor::control::Velocity velocity_setpoint;
    cvra::motor::control::Torque torque_setpoint;
    cvra::motor::control::Voltage voltage_setpoint;
    cvra::motor::control::Trajectory trajectory_setpoint;

    update_motor_can_id(d);
    int node_id = motor_driver_get_can_id(d);
    if (node_id == CAN_ID_NOT_SET) {
        return;
    }
    driver_allocation(d);
    can_driver_s *can_drv = (can_driver_s*)d->can_driver;

    motor_driver_lock(d);
    switch(d->control_mode) {
        case MOTOR_CONTROL_MODE_VELOCITY: {
            motor_enable(can_drv, node_id);
            velocity_setpoint.velocity = motor_driver_get_velocity_setpt(d);
            can_drv->velocity_pub.unicast(velocity_setpoint, node_id);
        } break;

        case MOTOR_CONTROL_MODE_POSITION: {
            motor_enable(can_drv, node_id);
            position_setpoint.position = motor_driver_get_position_setpt(d);
            can_drv->position_pub.unicast(position_setpoint, node_id);
        } break;

        case MOTOR_CONTROL_MODE_TORQUE: {
            motor_enable(can_drv, node_id);
            torque_setpoint.torque = motor_driver_get_torque_setpt(d);
            can_drv->torque_pub.unicast(torque_setpoint, node_id);
        } break;

        case MOTOR_CONTROL_MODE_VOLTAGE: {
            motor_enable(can_drv, node_id);
            voltage_setpoint.voltage = motor_driver_get_voltage_setpt(d);
            can_drv->voltage_pub.unicast(voltage_setpoint, node_id);
        } break;

        case MOTOR_CONTROL_MODE_TRAJECTORY: {
            motor_enable(can_drv, node_id);
            uint64_t timestamp_us = ST2US(chVTGetSystemTime());
            float position, velocity, acceleration, torque;
            motor_driver_get_trajectory_point(d,
                                              timestamp_us,
                                              &position,
                                              &velocity,
                                              &acceleration,
                                              &torque);
            trajectory_setpoint.position = position;
            trajectory_setpoint.velocity = velocity;
            trajectory_setpoint.acceleration = acceleration;
            trajectory_setpoint.torque = torque;
            can_drv->trajectory_pub.unicast(trajectory_setpoint, node_id);
        } break;

        case MOTOR_CONTROL_MODE_DISABLED: {
            motor_disable(can_drv, node_id);
        } break;

        default:
            /* TODO */
            break;
    }
    motor_driver_unlock(d);
}
