#ifndef ROBOT_PARAMETERS_H
#define ROBOT_PARAMETERS_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Robot base parameters
 */
#define DEBRA_RIGHT_WHEEL_RADIUS 	0.156f
#define DEBRA_LEFT_WHEEL_RADIUS		0.156f
#define DEBRA_WHEELBASE 			0.030f

#define NASTYA_RIGHT_WHEEL_RADIUS 	0.156f
#define NASTYA_LEFT_WHEEL_RADIUS	0.156f
#define NASTYA_WHEELBASE 			0.030f

#ifdef DEBRA
#define ROBOT_RIGHT_WHEEL_RADIUS 	DEBRA_RIGHT_WHEEL_RADIUS
#define ROBOT_LEFT_WHEEL_RADIUS	    DEBRA_LEFT_WHEEL_RADIUS
#define ROBOT_WHEELBASE 			DEBRA_WHEELBASE
#else
#define ROBOT_RIGHT_WHEEL_RADIUS 	NASTYA_RIGHT_WHEEL_RADIUS
#define ROBOT_LEFT_WHEEL_RADIUS	    NASTYA_LEFT_WHEEL_RADIUS
#define ROBOT_WHEELBASE 			NASTYA_WHEELBASE
#endif

#ifdef __cplusplus
}
#endif

#endif /* ROBOT_PARAMETERS_H */