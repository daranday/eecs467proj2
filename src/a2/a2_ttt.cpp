#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>
#include <math.h>
#include <fstream>
#include <iostream>
#include <vector>
// core api
#include "vx/vx.h"
#include "vx/vx_util.h"
#include "vx/vx_remote_display_source.h"
#include "vx/gtk/vx_gtk_display_source.h"

// drawables
#include "vx/vxo_drawables.h"

// common
#include "common/getopt.h"
#include "common/pg.h"
#include "common/zarray.h"

// imagesource
#include "imagesource/image_u32.h"
#include "imagesource/image_source.h"
#include "imagesource/image_convert.h"

#include "apps/eecs467_util.h"    // This is where a lot of the internals live

// a2 sources
#include "a2_blob_detector.h"
#include "a2_inverse_kinematics.h"
#include "a2_image_to_arm_coord.h"


using namespace std;

// It's good form for every application to keep its state in a struct.
struct state_t {
    bool running;

    getopt_t        *gopt;
    parameter_gui_t *pg;

    // image stuff
    char *img_url;
    int   img_height;
    int   img_width;

    // vx stuff
    vx_application_t    vxapp;
    vx_world_t         *vxworld;      // where vx objects are live
    vx_event_handler_t *vxeh; // for getting mouse, key, and touch events
    vx_mouse_event_t    last_mouse_event;

    // threads
    pthread_t animate_thread;

    // for accessing the arrays
    pthread_mutex_t mutex;
    int argc;
    char **argv;

    bool use_cached_bbox_colors;
    bool use_cached_calibration;
    image_u32_t* current_image;

    coord_convert converter;
    bool converter_initialized;

    int balls_placed;

    double origin_x;
    double origin_y;
    double interval_x;
    double interval_y;

    state_t() : current_image(NULL), converter_initialized(false), balls_placed(0) {
        use_cached_bbox_colors = (ifstream("Bbox_Colors.txt")) ? true : false;
        use_cached_calibration = (ifstream("ConversionMatrices.txt")) ? true : false;
        interval_x = 0.065; interval_y = 0.065;
        origin_x = 0.01 + interval_x; origin_y = 0.13 - interval_y;
    }
} state_obj;

state_t *state = &state_obj;

// === Parameter listener =================================================
// This function is handed to the parameter gui (via a parameter listener)
// and handles events coming from the parameter gui. The parameter listener
// also holds a void* pointer to "impl", which can point to a struct holding
// state, etc if need be.
static void
my_param_changed (parameter_listener_t *pl, parameter_gui_t *pg, const char *name)
{
    if (0==strcmp ("sl1", name))
        printf ("sl1 = %f\n", pg_gd (pg, name));
    else if (0==strcmp ("sl2", name))
        printf ("sl2 = %d\n", pg_gi (pg, name));
    else if (0==strcmp ("cb1", name) || 0==strcmp ("cb2", name))
        printf ("%s = %d\n", name, pg_gb (pg, name));
    else
        printf ("%s changed\n", name);
}

void get_image_coordinates(double x, double y, double& coord_x, double& coord_y) {
    coord_x = round((x + 1) * state->img_width / 2. + 0.5);
    coord_y = state->img_height - round((y + state->img_height / (state->img_width * 1.)) * state->img_width / 2. + 0.5);
    printf("Coords x = %d, y = %d\n", (int)coord_x, (int)coord_y);
}

static int
mouse_event (vx_event_handler_t *vxeh, vx_layer_t *vl, vx_camera_pos_t *pos, vx_mouse_event_t *mouse)
{
    // vx_camera_pos_t contains camera location, field of view, etc
    // vx_mouse_event_t contains scroll, x/y, and button click events

    if ((mouse->button_mask & VX_BUTTON1_MASK) &&
        !(state->last_mouse_event.button_mask & VX_BUTTON1_MASK)) {

        vx_ray3_t ray;
        vx_camera_pos_compute_ray (pos, mouse->x, mouse->y, &ray);

        double ground[3];
        vx_ray3_intersect_xy (&ray, 0, ground);

        double camera_vector[3] = {0,0,1};
        get_image_coordinates(ground[0], ground[1], camera_vector[0], camera_vector[1]);

        if (state->converter_initialized) {
            double arm_vector[3] = {0,0,1};
            state->converter.camera_to_arm(arm_vector, camera_vector);
            printf("Camera coords: %g, %g\nArm coords: %g, %g", camera_vector[0], camera_vector[1], arm_vector[0], arm_vector[1]);

            int a;
            move_to(arm_vector[0], arm_vector[1], 0.13);
            arm_fetch();
            move_to(-(state->balls_placed % 3) * state->interval_x + state->origin_x, (state->balls_placed / 3) * state->interval_y + state->origin_y, 0.13);
            arm_drop();

            state->balls_placed++;
        }
        // printf ("Mouse clicked at coords: [%8.3f, %8.3f]  Camera coordinates clicked at: [%6.3f, %6.3f]\n",
        //         mouse->x, mouse->y, camera_vector[0], camera_vector[1]);
        // printf("Height: %d, Width: %d\n", state->img_height, state->img_width);
    }

    // store previous mouse event to see if the user *just* clicked or released
    state->last_mouse_event = *mouse;

    return 0;
}

static int
key_event (vx_event_handler_t *vxeh, vx_layer_t *vl, vx_key_event_t *key)
{
    //state_t *state = vxeh->impl;
    return 0;
}

static int
touch_event (vx_event_handler_t *vh, vx_layer_t *vl, vx_camera_pos_t *pos, vx_touch_event_t *mouse)
{
    return 0; // Does nothing
}

// === Your code goes here ================================================
// The render loop handles your visualization updates. It is the function run
// by the animate_thread. It periodically renders the contents on the
// vx world contained by state
void *
animate_thread (void *data)
{
    const int fps = 60;

    // Set up the imagesource
    image_source_t *isrc = image_source_open (state->img_url);

    if (isrc == NULL)
        printf ("Error opening device.\n");
    else {
        // Print out possible formats. If no format was specified in the
        // url, then format 0 is picked by default.
        // e.g. of setting the format parameter to format 2:
        //
        // --url=dc1394://bd91098db0as9?fidx=2
        for (int i = 0; i < isrc->num_formats (isrc); i++) {
            image_source_format_t ifmt;
            isrc->get_format (isrc, i, &ifmt);
            printf ("%3d: %4d x %4d (%s)\n",
                    i, ifmt.width, ifmt.height, ifmt.format);
        }
        isrc->start (isrc);
    }

    // Continue running until we are signaled otherwise. This happens
    // when the window is closed/Ctrl+C is received.
    while (state->running) {

        // Get the most recent camera frame and render it to screen.
        if (isrc != NULL) {
            image_source_data_t *frmd = (image_source_data_t *)calloc (1, sizeof(*frmd));
            int res = isrc->get_frame (isrc, frmd);
            if (res < 0)
                printf ("get_frame fail: %d\n", res);
            else {
                // Handle frame
                pthread_mutex_lock(&state->mutex);    
                if (state->current_image != NULL) {
                    image_u32_destroy (state->current_image);
                    state->current_image = NULL;
                }
                state->current_image = image_convert_u32 (frmd);
                image_u32_t *im = state->current_image;
                if (im != NULL) {
                    vx_object_t *vim = vxo_image_from_u32(im,
                                                          VXO_IMAGE_FLIPY,
                                                          VX_TEX_MIN_FILTER | VX_TEX_MAG_FILTER);

                    // render the image centered at the origin and at a normalized scale of +/-1 unit in x-dir
                    const double scale = 2./im->width;
                    vx_buffer_add_back (vx_world_get_buffer (state->vxworld, "image"),
                                        vxo_chain (vxo_mat_scale3 (scale, scale, 1.0),
                                                   vxo_mat_translate3 (-im->width/2., -im->height/2., 0.),
                                                   vim));
                    vx_buffer_swap (vx_world_get_buffer (state->vxworld, "image"));
                    state->img_height = im->height;
                    state->img_width = im->width;
                    // image_u32_destroy (im);
                }
                pthread_mutex_unlock(&state->mutex);                    
            }
            fflush (stdout);
            isrc->release_frame (isrc, frmd);
        }


        // Draw a default set of coordinate axes
        vx_object_t *vxo_axe = vxo_chain (vxo_mat_scale (0.1), // 10 cm axes
                                          vxo_axes ());
        vx_buffer_add_back (vx_world_get_buffer (state->vxworld, "axes"), vxo_axe);


        // Now, we update both buffers
        // vx_buffer_swap (vx_world_get_buffer (state->vxworld, "rot-sphere"));
        // vx_buffer_swap (vx_world_get_buffer (state->vxworld, "osc-square"));
        vx_buffer_swap (vx_world_get_buffer (state->vxworld, "axes"));

        usleep (1000000/fps);
    }

    if (isrc != NULL)
        isrc->stop (isrc);

    return NULL;
}

state_t *
state_create (void)
{
    state->vxworld = vx_world_create ();
    state->vxeh = (vx_event_handler_t*)calloc (1, sizeof(*state->vxeh));
    state->vxeh->key_event = key_event;
    state->vxeh->mouse_event = mouse_event;
    state->vxeh->touch_event = touch_event;
    state->vxeh->dispatch_order = 100;
    state->vxeh->impl = state; // this gets passed to events, so store useful struct here!

    state->vxapp.display_started = eecs467_default_display_started;
    state->vxapp.display_finished = eecs467_default_display_finished;
    state->vxapp.impl = eecs467_default_implementation_create (state->vxworld, state->vxeh);

    state->running = 1;

    return state;
}

void
state_destroy (state_t *state)
{
    if (!state)
        return;

    if (state->vxeh)
        free (state->vxeh);

    if (state->gopt)
        getopt_destroy (state->gopt);

    if (state->pg)
        pg_destroy (state->pg);
}

// This is intended to give you a starting point to work with for any program
// requiring a GUI. This handles all of the GTK and vx setup, allowing you to
// fill in the functionality with your own code.

void* start_vx(void* user) {
    int argc = state->argc;
    char **argv = state->argv;

    eecs467_init (argc, argv);
    state_t *state = state_create ();

    // Parse arguments from the command line, showing the help
    // screen if required
    state->gopt = getopt_create ();
    getopt_add_bool   (state->gopt,  'h', "help", 0, "Show help");
    getopt_add_string (state->gopt, '\0', "url", "", "Camera URL");
    getopt_add_bool   (state->gopt,  'l', "list", 0, "Lists available camera URLs and exit");

    if (!getopt_parse (state->gopt, argc, argv, 1) || getopt_get_bool (state->gopt, "help")) {
        printf ("Usage: %s [--url=CAMERAURL] [other options]\n\n", argv[0]);
        getopt_do_usage (state->gopt);
        exit (EXIT_FAILURE);
    }

    // Set up the imagesource. This looks for a camera url specified on
    // the command line and, if none is found, enumerates a list of all
    // cameras imagesource can find and picks the first url it finds.
    if (strncmp (getopt_get_string (state->gopt, "url"), "", 1)) {
        state->img_url = strdup (getopt_get_string (state->gopt, "url"));
        printf ("URL: %s\n", state->img_url);
    }
    else {
        // No URL specified. Show all available and then use the first
        zarray_t *urls = image_source_enumerate ();
        printf ("Cameras:\n");
        for (int i = 0; i < zarray_size (urls); i++) {
            char *url;
            zarray_get (urls, i, &url);
            printf ("  %3d: %s\n", i, url);
        }

        if (0==zarray_size (urls)) {
            printf ("Found no cameras.\n");
            return NULL;
        }

        zarray_get (urls, 0, &state->img_url);
    }

    if (getopt_get_bool (state->gopt, "list")) {
        state_destroy (state);
        exit (EXIT_SUCCESS);
    }

    // Initialize this application as a remote display source. This allows
    // you to use remote displays to render your visualization. Also starts up
    // the animation thread, in which a render loop is run to update your display.
    vx_remote_display_source_t *cxn = vx_remote_display_source_create (&state->vxapp);

    // Initialize a parameter gui
    state->pg = pg_create ();
    pg_add_double_slider (state->pg, "sl1", "Slider 1", 0, 100, 50);
    pg_add_int_slider    (state->pg, "sl2", "Slider 2", 0, 100, 25);
    pg_add_check_boxes (state->pg,
                        "cb1", "Check Box 1", 0,
                        "cb2", "Check Box 2", 1,
                        NULL);
    pg_add_buttons (state->pg,
                    "but1", "Button 1",
                    "but2", "Button 2",
                    "but3", "Button 3",
                    NULL);

    parameter_listener_t *my_listener = (parameter_listener_t*)calloc (1, sizeof(*my_listener));
    my_listener->impl = state;
    my_listener->param_changed =    my_param_changed;
    pg_add_listener (state->pg, my_listener);

    // Launch our worker threads
    pthread_create (&state->animate_thread, NULL, animate_thread, state);

    // This is the main loop
    eecs467_gui_run (&state->vxapp, state->pg, 1024, 768);

    // Quit when GTK closes
    state->running = 0;
    pthread_join (state->animate_thread, NULL);

    // Cleanup
    free (my_listener);
    state_destroy (state);
    vx_remote_display_source_destroy (cxn);
    vx_global_destroy ();

    return NULL;
}

void read_bbox_and_colors(vector<int>& bbox, vector<vector<double> >& hsv_ranges) {
    // use cached values
    if (state->use_cached_bbox_colors) {
        ifstream fin("Bbox_Colors.txt");
        for (int i = 0; i < 4; ++i) {
            fin >> bbox[i];
        }
        for (int i = 0; i < 3; ++i) {
            for (int j = 0; j < 3; ++j) {
                fin >> hsv_ranges[i][j * 2] >> hsv_ranges[i][j * 2 + 1];
            }
        }
        fin.close();
    } else {
        // save these values to cache
        ofstream fout("Bbox_Colors.txt");

        //read mask
        system("./bin/a2_mask");
        ifstream fmask("Mask.txt");
        for (int i = 0; i < 4; ++i) {
            fmask >> bbox[i];
            fout << bbox[i] << " ";
        }
        fmask.close();
        cout << endl;
        fout << endl;

        // read colors for blue corner squares, our color, their color
        for (int i = 0; i < 3; ++i) {
            system("./bin/a2_color_picker");
            ifstream fhsv("HsvRange.txt");
            for (int j = 0; j < 3; ++j) {
                fhsv >> hsv_ranges[i][j * 2] >> hsv_ranges[i][j * 2 + 1];
                fout << hsv_ranges[i][j * 2] << " " << hsv_ranges[i][j * 2 + 1] << " ";
            }
            fout << endl;
            fhsv.close();
        }
        fout.close();
    }

    //output input values
    cout << "\n\nBBox Coordinates:" << endl;
    for (int i = 0; i < 4; ++i)
        cout << bbox[i] << " ";
    cout << "\nHSV colors:" << endl;
    vector<string> color_types = {"Blue Corner Squares", "Our Color", "Opponent Color"};
    for (int i = 0; i < 3; ++i) {
        cout << color_types[i] << ": " << endl;
        for (int j = 0; j < 3; ++j) {
            cout << hsv_ranges[i][j * 2] << " " << hsv_ranges[i][j * 2 + 1] << " ";
        }
        cout << endl;
    }
    cout << "\n\n" << endl;
}

bool is_color(r_data& hsv, vector<double>& color_range) {
    if (color_range[0] < hsv.H && hsv.H < color_range[1]
        && color_range[2] < hsv.S && hsv.S < color_range[3]
        && color_range[4] < hsv.V && hsv.V < color_range[5])
        return true;
    return false;
}

void* start_inverse_kinematics(void* user) {
    kin_main(state->argc, state->argv);
    return NULL;
}

void get_coordinates_from_joints(double& x, double& y) {
    double R = 0;
    vector<double> arm_length = {0.118, 0.1, 0.0985, 0.099}; // change this accordingly
    
    R = arm_length[1] * sin(kin_state->arm_joints[1]) + arm_length[2] * sin(kin_state->arm_joints[1] + kin_state->arm_joints[2]);
    R += arm_length[3] * sin(kin_state->arm_joints[1] + kin_state->arm_joints[2] + kin_state->arm_joints[3]);

    x = R * sin(kin_state->arm_joints[0]);
    y = - R * cos(kin_state->arm_joints[0]);
}

void read_matrices(double Amat[], double Cmat[]) {
    ifstream fin("ConversionMatrices.txt");
    for (int i = 0; i < 9; ++i) {
        fin >> Cmat[i];
    }
    for (int i = 0; i < 9; ++i) {
        fin >> Amat[i];
    }
    fin.close();
}

void save_matrices(double Amat[], double Cmat[]) {
    ofstream fout("ConversionMatrices.txt");
    for (int i = 0; i < 9; ++i) {
        fout << Cmat[i] << " ";
        if ((i - 2) % 3 == 0)
            fout << endl;
    }
    for (int i = 0; i < 9; ++i) {
        fout << Amat[i] << " ";
        if ((i - 2) % 3 == 0)
            fout << endl;
    }
    fout.close();
}

void calibrate_coordinate_converter(vector<int>& bbox, vector<vector<double> >& hsv_ranges) {
    cout << "Calibrating..." << endl;

    blob_detect B;
    B.get_mask(bbox);
    B.get_colors(hsv_ranges);
    pthread_mutex_lock(&state->mutex);
    B.run( state->current_image);
    pthread_mutex_unlock(&state->mutex);


    double Cmat[9] = {  0, 0, 0,
                        0, 0, 0,
                        1, 1, 1};
    double Amat[9] = {  0, 0, 0,
                        0, 0, 0,
                        1, 1, 1};

    if (state->use_cached_calibration) {
        cout << "Using cached calibration" << endl;
        read_matrices(Amat, Cmat);
    } else {
        cout << "Reading Camera Points..." << endl;
        printf("Detected %d blobs\n", int(B.region_data.size()));
        printf("Blue color HSV range is: [%g, %g, %g, %g, %g, %g]\n", hsv_ranges[0][0], hsv_ranges[0][1], hsv_ranges[0][2], hsv_ranges[0][3], hsv_ranges[0][4], hsv_ranges[0][5]);
        vector<int> blue_blobs_order = {0, 0, 0, 0};
        // showing blue square coordinates
        for(size_t i = 0, j = 0; i < B.region_data.size(); i++){
            if(B.region_data[i].area > 100 && is_color(B.region_data[i], hsv_ranges[0])){
                printf("Blue square %d -- x: %g, y: %g, area: %d\n", int(i), B.region_data[i].x, B.region_data[i].y, B.region_data[i].area);
            }
        }
        cout << "Please indicate the blue blobs order in letter Z order, -1 for blob not used: " << endl;
        cin >> blue_blobs_order[0] >> blue_blobs_order[1] >> blue_blobs_order[2] >> blue_blobs_order[3];

        for(size_t i = 0, j = 0; i < B.region_data.size(); i++){
            // printf("Blob %d, color: [%g, %g, %g], x: %g, y: %g\n", i, 
            //                             B.region_data[i].H, B.region_data[i].S, B.region_data[i].V,
            //                             B.region_data[i].x, B.region_data[i].y);
            if(B.region_data[i].area > 100 && is_color(B.region_data[i], hsv_ranges[0])){
                if (blue_blobs_order[j] != -1) {
                    Cmat[blue_blobs_order[j]] = B.region_data[i].x;
                    Cmat[blue_blobs_order[j]+3] = B.region_data[i].y;
                }
                j++;
                printf("Blue square %d -- x: %g, y: %g, area: %d\n", int(i), B.region_data[i].x, B.region_data[i].y, B.region_data[i].area);
            }
        }

        cout << "Reading Arm Points..." << endl;
        cout << "Please move arm tip to blue blobs" << endl;
        relax_arm();
        for (int i = 0; i < 3; ++i) {
            string garbage;
            cout << "Enter anything: ";
            cin >> garbage;
            cout << "Joint angles: ";
            for (int j = 0; j < kin_state->arm_joints.size(); ++j) {
                cout << kin_state->arm_joints[j] << ", ";
            }
            cout << endl;

            double x, y;
            get_coordinates_from_joints(x, y);
            printf("X: %g, Y: %g\n", x, y);
            Amat[i] = x;
            Amat[i+3] = y;
        }
        save_matrices(Amat, Cmat);
    }
    
    state->converter.c2a_get_factors(Amat, Cmat);

    // test if conversion works:
    double test_c[3] = {Cmat[0], Cmat[3], 1};
    double test_a[3] = {-69, -69, -69};
    state->converter.camera_to_arm(test_a, test_c);
    printf("The first arm coordinate should be %g, %g\n", test_a[0], test_a[1]);
    state->converter_initialized = true;
}

int main (int argc, char *argv[])
{
    //read bounding box and color hsv ranges
    //hsv_ranges example: [*blue squares color: [h_min, h_max, s_min, s_max, v_min, v_max], 
    //                      *our color [...], 
    //                      *opponent color [...]]
    vector<int> bbox(4);
    vector<vector<double> > hsv_ranges(3, vector<double>(6));

    //start vx
    state->argc = argc;
    state->argv = argv;
    pthread_t vx_thread, kinematics_thread;
    pthread_create (&vx_thread, NULL, start_vx, (void*)NULL);
    pthread_create (&kinematics_thread, NULL, start_inverse_kinematics, (void*)NULL);

    // wait till vx has started
    while(state->current_image == NULL) {
        usleep(100000);
    }

    read_bbox_and_colors(bbox, hsv_ranges);
    calibrate_coordinate_converter(bbox, hsv_ranges);

    // const double claw_rest_angle_c = -(pi/2 * 4/5.);

    // vector<double> initial_joints = {0, pi/2, 0,0,0,0};
    // vector<double> initial_joints = {0.0713075, 0.565071, 0.513557, 2.06296, 0, claw_rest_angle_c};
    // vector<double> initial_joints = {0, 0, 0, 0, 0, 0};
    // move_joints(initial_joints);

    // cout << "Whose turn is it? " << endl;
    // string turn;
    // cin >> turn;

    // if (turn != "ours") {
    //     // sleep 20 seconds
    //     usleep(20000000);
    // }


    pthread_join (vx_thread, NULL);
    return 0;
}
