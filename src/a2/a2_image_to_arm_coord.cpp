#include "a2_image_to_arm_coord.h"
#include <gsl/gsl_linalg.h>
#include <stack>
#include <vector>
#include <cmath>
#include <iostream>
#include <fstream>
#include "a2_image_to_arm_coord.h"
using namespace std;

/*things to think about
1- the dimensions of the blue calibration squares should be inputted for each board 
2- program will take the coordinates from the arm's frame and translate them to and from the board's frame

For now - measure out the board frame, and hard code it. Create the program that converts board Arm XY to board XY
*/

coord_convert::coord_convert(){
}

void coord_convert::c2b_get_factors(double b[], double c[]){
	
  int n = 3;

  matd_t *B = matd_create_data(n, n, b);
  matd_t *C = matd_create_data(n, n, c);
  matd_t *invC = matd_inverse(C);
  c2b_Conv = matd_multiply(B, invC);
}

void coord_convert::camera_to_board( double b[], double c[]){
  	
  int n = 3;

  matd_t *C = matd_create_data(n, 1, c);
  matd_t *B = matd_multiply(c2b_Conv, C);
  b[0] = matd_get(B, 0, 0);
  b[1] = matd_get(B, 0, 1);
  b[2] = matd_get(B, 0, 2);
}

void coord_convert::b2a_get_factors(double a[], double b[]){
	
  int n = 3;
  matd_t *A = matd_create_data(n, n, a);
  matd_t *B = matd_create_data(n, n, b);
  // cout << "B: " << endl;
  // matd_print(B, "%7f");
  matd_t *invB = matd_inverse(B);
  // cout << "Should be I: " << endl;
  // matd_t *prod = matd_multiply(invB, B);
  // matd_print(prod, "%7f");
  b2a_Conv = matd_multiply(A, invB);
}

void coord_convert::board_to_arm( double a[], double b[]){
  int n = 3;

  matd_t *B = matd_create_data(n, 1, b);
  matd_t *A = matd_multiply(b2a_Conv, B);
  cout << "b2a_Conv" << endl;
  matd_print(b2a_Conv, "%7f");
  cout << "B:" << endl;
  matd_print(B, "%7f");
  cout << "A: " << endl;
  matd_print(A, "%7f");

  a[0] = matd_get(A, 0, 0);
  a[1] = matd_get(A, 0, 1);
  a[2] = matd_get(A, 0, 2);
}

int main(){
  coord_convert C;
  double A[9] = { -125.0, -125.0, 125.0,
		  0.0, 250.0, 0.0,
		  1.0, 1.0, 1.0};

  double B[9] = { 0.0, 0.0, 250.0,
		  0.0, 250.0, 0.0,
		  1.0, 1.0, 1.0};

  C.b2a_get_factors( A, B);

  double b1[3] = { 125.0, 125.0, 1.0 };
  double a1[3] = { 0.0, 125, 1.0};
  double a[3] = {-1, -1, -1};
  
  C.board_to_arm( a, b1 );

  cout << "Following two lines should be identical" << endl;
  cout << a[0] << " " << a[1] << " " << a[2] << endl;
  cout << a1[0] << " " << a1[1] << " " << a1[2] << endl;

  

  return 0;
}
