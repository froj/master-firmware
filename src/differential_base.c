#include <ch.h>
#include <hal.h>
#include <math.h>
#include <chprintf.h>
#include "usbconf.h"
#include "priorities.h"
#include "main.h"
#include "parameter/parameter.h"
#include "robot_parameters.h"
#include "tracy-the-trajectory-tracker/src/trajectory_tracking.h"
#include "config.h"
#include "robot_pose.h"
#include "motor_manager.h"
#include "differential_base.h"

#define DIFFERENTIAL_BASE_TRACKING_THREAD_STACK_SZ 2048

trajectory_t diff_base_trajectory;
mutex_t diff_base_trajectory_lock;


void differential_base_init(void)
{
    static float trajectory_buffer[100][5];
    trajectory_init(&diff_base_trajectory, (float *)trajectory_buffer, 100, 5, 10*1000);

    chMtxObjectInit(&diff_base_trajectory_lock);
}


THD_WORKING_AREA(differential_base_tracking_thread_wa, DIFFERENTIAL_BASE_TRACKING_THREAD_STACK_SZ);
void differential_base_tracking_thread(void *p)
{
    (void) p;

    chRegSetThreadName("differential_base_tracker");

    parameter_namespace_t *base_config = parameter_namespace_find(&global_config, "/master/differential_base");
    if (base_config == NULL) {
        chSysHalt("base parameter not found");
    }
    parameter_namespace_t *tracy_config = parameter_namespace_find(&global_config, "/master/tracy");
    if (tracy_config == NULL) {
        chSysHalt("tracy parameter not found");
    }

    float motor_base;
    float radius_right;
    float radius_left;
    bool first_run = true;
    bool tracy_active = false;
    while (1) {
        if (parameter_namespace_contains_changed(base_config) || first_run) {
            motor_base = parameter_scalar_get(parameter_find(base_config, "wheelbase"));
            radius_right = parameter_scalar_get(parameter_find(base_config, "radius_right"));
            radius_left = parameter_scalar_get(parameter_find(base_config, "radius_left"));
        }
        first_run = false;

        float *point;
        float x, y, theta, speed, omega;
        uint64_t now;

        now = ST2US(chVTGetSystemTime());

        chMtxLock(&diff_base_trajectory_lock);
        point = trajectory_read(&diff_base_trajectory, now);
        if (point) {
            x = point[0];
            y = point[1];
            speed = point[2];
            theta = point[3];
            omega = point[4];
        }
        chMtxUnlock(&diff_base_trajectory_lock);

        palTogglePad(GPIOF, GPIOF_LED_GREEN_1);

        if (point) {
            tracy_active = true;
            struct tracking_error error;
            struct robot_velocity input, output;


            /* Get data from odometry. */
            chMtxLock(&robot_pose_lock);
                error.x_error = x - robot_pose.x;
                error.y_error = y - robot_pose.y;
                error.theta_error = theta - robot_pose.theta;

                theta = robot_pose.theta;
            chMtxUnlock(&robot_pose_lock);

            input.tangential_velocity = speed;
            input.angular_velocity = omega;

            if (parameter_namespace_contains_changed(tracy_config)) {
                tracy_set_controller_params(
                    parameter_scalar_get(parameter_find(tracy_config, "damping")),
                    parameter_scalar_get(parameter_find(tracy_config, "g")));
            }
            palSetPad(GPIOF, GPIOF_LED_DEBUG);

            /* Transform error to local frame. */
            tracy_global_error_to_local(&error, theta);

            /* Perform controller iteration */
            tracy_linear_controller(&error, &input, &output);

            /* Apply speed to wheels. */
            chprintf((BaseSequentialStream *)&SDU1 , "setpoint: %.2f output: %.2f\r\n", speed, output.tangential_velocity);

            float right_spd = ROBOT_RIGHT_WHEEL_DIRECTION * output.tangential_velocity / (radius_left * M_PI);

            float left_spd = ROBOT_LEFT_WHEEL_DIRECTION * output.tangential_velocity / (radius_left * M_PI);

            motor_manager_set_velocity(&motor_manager, "right-wheel", right_spd);
            motor_manager_set_velocity(&motor_manager, "left-wheel", left_spd);
            chprintf((BaseSequentialStream *)&SDU1 , "wheels %.4f %.4f\r\n", left_spd, right_spd);


        } else {
            if (tracy_active) {
                tracy_active = false;
                // todo control error here
                motor_manager_set_velocity(&motor_manager, "right-wheel", 0);
                motor_manager_set_velocity(&motor_manager, "left-wheel", 0);
            }
            palClearPad(GPIOF, GPIOF_LED_DEBUG);
        }

        chThdSleepMilliseconds(50);
    }
}


void differential_base_tracking_start(void)
{
    chThdCreateStatic(differential_base_tracking_thread_wa,
                      DIFFERENTIAL_BASE_TRACKING_THREAD_STACK_SZ,
                      DIFFERENTIAL_BASE_TRACKING_THREAD_PRIO,
                      differential_base_tracking_thread,
                      NULL);
}
