#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <math.h>
#include <sys/select.h>
#include <sys/time.h>
#include <pthread.h>
#include <vector>

#include <lcm/lcm.h>
#include "lcmtypes/dynamixel_command_list_t.h"
#include "lcmtypes/dynamixel_command_t.h"
#include "lcmtypes/dynamixel_status_list_t.h"
#include "lcmtypes/dynamixel_status_t.h"

#include "common/getopt.h"
#include "common/timestamp.h"
// #include "math/math_util.h"

#define NUM_SERVOS 6

using namespace std;

struct state_t
{
    getopt_t *gopt;

    // LCM
    lcm_t *lcm;
    const char *command_channel;
    const char *status_channel;

    pthread_t status_thread;
    pthread_t command_thread;

    int stages;
    state_t() : stages(0) {}
} state_obj;

state_t *state = &state_obj;

void move_to(double x, double y, double z, double wrist_tilt = 0);

static void
status_handler (const lcm_recv_buf_t *rbuf,
                const char *channel,
                const dynamixel_status_list_t *msg,
                void *user)
{
    // Print out servo positions
    for (int id = 0; id < msg->len; id++) {
        dynamixel_status_t stat = msg->statuses[id];
        printf ("[id %d]=%6.3f ",id, stat.position_radians);
    }
    printf ("\n");
}

void *
status_loop (void *data)
{
    dynamixel_status_list_t_subscribe (state->lcm,
                                       state->status_channel,
                                       status_handler,
                                       state);
    const int hz = 15;
    while (1) {
        // Set up the LCM file descriptor for waiting. This lets us monitor it
        // until something is "ready" to happen. In this case, we are ready to
        // receive a message.
        int status = lcm_handle_timeout (state->lcm, 1000/hz);
        if (status <= 0)
            continue;

        // LCM has events ready to be processed
    }

    return NULL;
}

void *
command_loop (void *user)
{
    const int hz = 30;

    dynamixel_command_list_t cmds;
    cmds.len = NUM_SERVOS;
    cmds.commands = (dynamixel_command_t*)calloc (NUM_SERVOS, sizeof(dynamixel_command_t));
	float pi = 3.1415;
    for (int id = 0; id < NUM_SERVOS; id++) {
            cmds.commands[id].utime = utime_now ();
            cmds.commands[id].position_radians = 0.0;
            cmds.commands[id].speed = 0.05;
            cmds.commands[id].max_torque = 0.35;
    		dynamixel_command_list_t_publish (state->lcm, state->command_channel, &cmds);
        	usleep (1000000/hz);
     }

    while (1) {
        // Send LCM commands to arm. Normally, you would update positions, etc,
        // but here, we will just home the arm.
        for (int id = 0; id < NUM_SERVOS; id++) {
            if (getopt_get_bool (state->gopt, "idle")) {
                cmds.commands[id].utime = utime_now ();
                cmds.commands[id].position_radians = 0.0;
                cmds.commands[id].speed = 0.0;
                cmds.commands[id].max_torque = 0.5;
        		dynamixel_command_list_t_publish (state->lcm, state->command_channel, &cmds);
            }
            else {
                //wave servo back and forth once 
				cmds.commands[id].utime = utime_now ();
                cmds.commands[id].position_radians = 0.0;
                cmds.commands[id].speed = 0.05;
                cmds.commands[id].max_torque = 0.5;
        		dynamixel_command_list_t_publish (state->lcm, state->command_channel, &cmds);
        		usleep (5*1000000/hz);
                
				cmds.commands[id].position_radians = pi/12.0;
				cmds.commands[id].utime = utime_now ();
        		dynamixel_command_list_t_publish (state->lcm, state->command_channel, &cmds);
        		usleep (100*1000000/hz);
				
				cmds.commands[id].position_radians = 0.0;
				cmds.commands[id].utime = utime_now ();
        		dynamixel_command_list_t_publish (state->lcm, state->command_channel, &cmds);
        		usleep (5*1000000/hz);
            }
        }
       // dynamixel_command_list_t_publish (state->lcm, state->command_channel, &cmds);

        usleep (500000/hz);
    }

    free (cmds.commands);

    return NULL;
}

void move_to(double x, double y, double z, double wrist_tilt) {
    double pi = 3.1415926;
    vector<double> arm_length = {0.118, 0.1, 0.0985, 0.099}; // change this accordingly
    vector<double> max_angles = {pi,pi/12,pi/12,pi/12}; // change this accordingly
    vector<double> cmd_angles = {0, 0, 0, 0};

    double R = sqrt(pow(x, 2) + pow(y, 2));
    cmd_angles[0] = atan2(x, y); // x and y reversed because angle begins from y, not x axis.
    
    double M = sqrt(pow(R, 2) + pow(arm_length[3] + z - arm_length[0], 2));
    double alpha = atan2(arm_length[3] + z - arm_length[0], R);
    double beta  = acos((-pow(arm_length[2], 2) + pow(arm_length[1], 2) + pow(M, 2)) / (2 * arm_length[1] * M));
    double gamma = acos((+pow(arm_length[2], 2) + pow(arm_length[1], 2) - pow(M, 2)) / (2 * arm_length[1] * M));

    cmd_angles[1] = pi/2 - alpha - beta;
    cmd_angles[2] = pi - gamma;
    cmd_angles[3] = pi - cmd_angles[1] - cmd_angles[2] - wrist_tilt;

    // send angles command
    dynamixel_command_list_t cmds;
    cmds.len = NUM_SERVOS;
    cmds.commands = (dynamixel_command_t*)calloc (NUM_SERVOS, sizeof(dynamixel_command_t));
    for (int id = 0; id < NUM_SERVOS; id++) {
        cmds.commands[id].utime = utime_now ();
        cmds.commands[id].position_radians = cmd_angles[id];
        cmds.commands[id].speed = 0.05;
        cmds.commands[id].max_torque = 0.5;
    }
    dynamixel_command_list_t_publish (state->lcm, state->command_channel, &cmds);
}

void move_joints(vector<double> joint_angles) {
    // send angles command
    dynamixel_command_list_t cmds;
    cmds.len = NUM_SERVOS;
    cmds.commands = (dynamixel_command_t*)calloc (NUM_SERVOS, sizeof(dynamixel_command_t));
    for (int id = 0; id < NUM_SERVOS; id++) {
        cmds.commands[id].utime = utime_now ();
        cmds.commands[id].position_radians = joint_angles[id];
        cmds.commands[id].speed = 0.05;
        cmds.commands[id].max_torque = 0.5;
    }
    dynamixel_command_list_t_publish (state->lcm, state->command_channel, &cmds);
}

// This subscribes to the status messages sent out by the arm, displaying servo
// state in the terminal. It also sends messages to the arm ordering it to the
// "home" position (all servos at 0 radians).
int
main (int argc, char *argv[])
{
    getopt_t *gopt = getopt_create ();
    getopt_add_bool (gopt, 'h', "help", 0, "Show this help screen");
    getopt_add_bool (gopt, 'i', "idle", 0, "Command all servos to idle");
    getopt_add_string (gopt, '\0', "status-channel", "ARM_STATUS", "LCM status channel");
    getopt_add_string (gopt, '\0', "command-channel", "ARM_COMMAND", "LCM command channel");

    if (!getopt_parse (gopt, argc, argv, 1) || getopt_get_bool (gopt, "help")) {
        getopt_do_usage (gopt);
        exit (EXIT_FAILURE);
    }


    state->gopt = gopt;
    state->lcm = lcm_create (NULL);
    state->command_channel = getopt_get_string (gopt, "command-channel");
    state->status_channel = getopt_get_string (gopt, "status-channel");

    vector<double> initial_joints = {0, 0, 0, 0, 0, 0};
    move_joints(initial_joints);

    while(1) {
        double x,y,z;
        printf("X Y Z : ");
        scanf("%lf %lf %lf", &x, &y, &z);
        move_to(x, y, z, 0);
    }

    // pthread_create (&state->status_thread, NULL, status_loop, state);
    // pthread_create (&state->command_thread, NULL, command_loop, state);

    // Probably not needed, given how this operates
    // pthread_join (state->status_thread, NULL);
    // pthread_join (state->command_thread, NULL);

    lcm_destroy (state->lcm);
    free (state);
    getopt_destroy (gopt);
}
