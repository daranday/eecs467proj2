#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <cmath>
#include <sys/select.h>
#include <sys/time.h>
#include <pthread.h>
#include <vector>
#include <iostream>

#include <lcm/lcm.h>
#include "lcmtypes/dynamixel_command_list_t.h"
#include "lcmtypes/dynamixel_command_t.h"
#include "lcmtypes/dynamixel_status_list_t.h"
#include "lcmtypes/dynamixel_status_t.h"

#include "common/getopt.h"
#include "common/timestamp.h"
// #include "math/math_util.h"

#define NUM_SERVOS 6
const double pi = 3.1415926;

using namespace std;

void move_joints(vector<double> joint_angles);
void move_to(double x, double y, double z, double wrist_tilt);

class Error {
    const char * msg;
    Error(const char * msg_) :msg(msg_) {}
};

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
    vector<double> cmd_angles;
    vector<double> cmd_position;

    vector<double> arm_joints;
    pthread_mutex_t arm_lock;
    bool running;
    state_t() : stages(0), cmd_angles(6), cmd_position(3), arm_joints(6), running(true) {}
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
    pthread_mutex_lock(&(state->arm_lock));
    for (int id = 0; id < msg->len; id++) {
        state->arm_joints[id] = msg->statuses[id].position_radians;
        // printf ("[id %d]=%6.3f ",id, state->arm_joints[id]);
    }
    pthread_mutex_unlock(&(state->arm_lock));
    // printf ("\n");
}

void *
status_loop (void *data)
{
    const int hz = 15;
    dynamixel_status_list_t_subscribe (state->lcm,
                                       state->status_channel,
                                       status_handler,
                                       state);
    while (state->running) {
        // Set up the LCM file descriptor for waiting. This lets us monitor it
        // until something is "ready" to happen. In this case, we are ready to
        // receive a message.
        int status = lcm_handle_timeout (state->lcm, 1000/hz);
        if (status <= 0)
            continue;
    }

    return NULL;
}

void move_to(double x, double y, double z, double wrist_tilt) {
    if (z < 0) {
        printf("z too low!\n");
        return;
    }
    printf("Moving to %g, %g, %g\n", x, y, z);

    vector<double> arm_length = {0.118, 0.1, 0.0985, 0.099}; // change this accordingly
    vector<double> max_angles = {pi,pi/12,pi/12,pi/12}; // change this accordingly
    

    double R = sqrt(pow(x, 2) + pow(y, 2));
    state->cmd_angles[0] = atan2(x, y); // x and y reversed because angle begins from y, not x axis.
    
    double M = sqrt(pow(R, 2) + pow(arm_length[3] + z - arm_length[0], 2));
    double alpha = atan2(arm_length[3] + z - arm_length[0], R);
    double beta  = acos((-pow(arm_length[2], 2) + pow(arm_length[1], 2) + pow(M, 2)) / (2 * arm_length[1] * M));
    double gamma = acos((+pow(arm_length[2], 2) + pow(arm_length[1], 2) - pow(M, 2)) / (2 * arm_length[1] * arm_length[2]));

    if (beta != beta || gamma != gamma) {
        beta = 0;
        gamma = pi;
    }

    printf("M: %g, alpha: %g, beta0: %g, gamma0: %g, R: %g\n", M, alpha, (-pow(arm_length[2], 2) + pow(arm_length[1], 2) + pow(M, 2)) / (2 * arm_length[1] * M), (+pow(arm_length[2], 2) + pow(arm_length[1], 2) - pow(M, 2)) / (2 * arm_length[1] * arm_length[2]), R);
    printf("d2: %g, d3: %g, M: %g\n", arm_length[1], arm_length[2], M);

    state->cmd_angles[1] = pi/2 - alpha - beta;
    state->cmd_angles[2] = pi - gamma;
    state->cmd_angles[3] = pi - state->cmd_angles[1] - state->cmd_angles[2] - wrist_tilt;

    // send angles command
    move_joints(state->cmd_angles);
}

void move_joints(vector<double> joint_angles) {
    // send angles command
    dynamixel_command_list_t cmds;
    cmds.len = NUM_SERVOS;
    cmds.commands = (dynamixel_command_t*)calloc (NUM_SERVOS, sizeof(dynamixel_command_t));
    for (int id = 0; id < NUM_SERVOS; id++) {
        cmds.commands[id].utime = utime_now ();
        cmds.commands[id].position_radians = -joint_angles[id];
        cmds.commands[id].speed = 0.05;
        cmds.commands[id].max_torque = 0.75;
    }
    // cmds.commands[5].position_radians *= -1;
    dynamixel_command_list_t_publish (state->lcm, state->command_channel, &cmds);
    while(1) {
        double error = 0;
        pthread_mutex_lock(&(state->arm_lock));
        for (int i = 0; i < NUM_SERVOS; ++i) {
            error += abs(state->arm_joints[i] + state->cmd_angles[i]);
            printf("(%g, %g)\n", state->arm_joints[i], state->cmd_angles[i]);
        }
        pthread_mutex_unlock(&(state->arm_lock));
        cout << "error is " << error << endl;
        if (error < 0.15)
            break;
        usleep(10000);
    }
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

    pthread_create (&state->status_thread, NULL, status_loop, (void*)NULL);

    const double claw_rest_angle_c = -(pi/2 * 4/5.);

    // vector<double> initial_joints = {0, pi/2, 0,0,0,0};
    // vector<double> initial_joints = {0.0713075, 0.565071, 0.513557, 2.06296, 0, claw_rest_angle_c};
    vector<double> initial_joints = {0, 0, 0, 0, 0, claw_rest_angle_c};
    state->cmd_angles = initial_joints;
    state->cmd_position = {0.01, 0.14, 0.15};
    move_joints(initial_joints);

    double interval_x = 0.065, interval_y = 0.065;
    double origin_x = 0.01 + interval_x , origin_y = 0.13 - interval_y;

    // pthread_create (&state->command_thread, NULL, command_loop, state);

    while(1) {
        double a, b;
        string cmd;
        cout << "command > ";
        cin >> cmd;
        if (cmd == "mv") {
            cin >> a >> b;
            state->cmd_position = {-a * interval_x + origin_x, b * interval_y + origin_y, 0.13};
            // state->cmd_position = {a, b, 0.15};
            state->cmd_angles[1] = -pi/6;
            move_joints(state->cmd_angles);
            move_to(state->cmd_position[0], state->cmd_position[1], 0.13);
        } else if (cmd == "fetch") {
            cout << "Fetching" << endl;
            // lower claw
            move_to(state->cmd_position[0], state->cmd_position[1], 0.1);

            // straighten shoulder
            state->cmd_angles[5] = -(pi/2);
            move_joints(state->cmd_angles);
            
            // raise claw
            move_to(state->cmd_position[0], state->cmd_position[1], 0.13);
        } else if (cmd == "drop") {
            cout << "Dropping" << endl;
            state->cmd_angles[5] = claw_rest_angle_c;
            move_joints(state->cmd_angles);
        } else if (cmd == "quit") {
            state->running = false;
            break;
        }
    }


    // Probably not needed, given how this operates
    pthread_join (state->status_thread, NULL);
    // pthread_join (state->command_thread, NULL);

    lcm_destroy (state->lcm);
    free (state);
    getopt_destroy (gopt);
}
