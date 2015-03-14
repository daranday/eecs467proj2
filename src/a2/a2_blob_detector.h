#include "imagesource/image_u32.h"
#include "imagesource/image_u8x3.h"
#include <stack>
#include <vector>
#include <cmath>
#include <string>
#include <iostream>
#include <fstream>
using namespace std;

//this blob detector will only look for the blue calibration boxes

double Hmin = 168;
double Hmax = 205;
double Smin = 15;
double Smax = 55;
double Vmin = 65;
double Vmax = 95;

//is HSV for HSV and RGB for RGB, ABC is a placeholder
struct HSV{
  double H;
  double S;
  double V;
};

struct RGB{
  uint8_t R;
  uint8_t G;
  uint8_t B;
};

struct thresh{
  double Hmin;
  double Hmax;
  double Smin;
  double Smax;
  double Vmin;
  double Vmax;
};



void HSV_to_RGB( double H, double S, double V ){
    double hh, p, q, t, ff;
    double R, G, B;
    long        i;
    //V = V/100.0;
    S = S/100.0;

    if( S <= 0.0) {       // < is bogus, just shuts up warnings
        R = V;
        G = V;
        B = V;
        return;
    }
    hh = H;
    if(hh >= 360.0) hh = 0.0;
    hh /= 60.0;
    i = (long)hh;
    ff = hh - i;
    p = V * (1.0 - S);
    q = V * (1.0 - (S * ff));
    t = V * (1.0 - (S * (1.0 - ff)));

    switch(i) {
    case 0:
        R = V;
        G = t;
        B = p;
        break;
    case 1:
        R = q;
        G = V;
        B = p;
        break;
    case 2:
        R = p;
        G = V;
        B = t;
        break;

    case 3:
        R = p;
        G = q;
        B = V;
        break;
    case 4:
        R = t;
        G = p;
        B = V;
        break;
    case 5:
    default:
        R = V;
        G = p;
        B = q;
        break;
    }

    //cout << "R1: " << R*(255.0/100.0) << " G1: " << G*(255.0/100.0) << " B1: " << B*(255.0/100.0) << endl;

}

//convert HSV values to usable RGB
void RGB_to_HSV( uint8_t R, uint8_t G, uint8_t B, HSV &out ){
  double Cmax, Cmin, d;
  double R1, G1, B1;

  //cout << "R: " << int(R) << " G: " << int(G) << " B: " << int(B) << endl;

  //get into a %
  R1 = 100*double(R)/255.0;
  G1 = 100*double(G)/255.0;
  B1 = 100*double(B)/255.0;


  //find Cmax
  Cmax = (G1 < R1) ? R1 : G1;
  Cmax = (Cmax > B1) ? Cmax : B1; 

  //find Cmin
  Cmin = ( R1 < G1 ) ? R1 : G1;
  Cmin = ( Cmin < B1 ) ? Cmin : B1; 

  d = Cmax- Cmin;

  //calculate saturation
  if( Cmax )
    out.S = 100*(d/Cmax);
  else
    out.S = 0;

  out.V = Cmax;

  //calculate hue
  if( Cmax > 0.0 ) { 
    out.S = 100*(d / Cmax);                  
  } 
  else {
    out.S = 0.0;
    out.H = NAN;                           
    return;
  }
  if( R1 >= Cmax )                           
    out.H = ( G1 - B1 ) / d;        
  else if( G1 >= Cmax )
    out.H = 2.0 + ( B1 - R1 ) / d; 
  else
    out.H = 4.0 + ( R1 - G1 ) / d;

  out.H *=60.0;

  if( out.H < 0.0 )
    out.H += 360.0;

  HSV_to_RGB( out.H, out.S, out.V);
  //while(1){}
  
}

struct r_data{
  int area;
  double x, y;
  //center of mass will be intersection of these 4 points

  r_data(){
    area = 0;
    x = y = 0;
  }
};

class blob_detect{
public:
  image_u8x3 *image_83;
  image_u8x3 *region_83;
  int region;
  string file;
  //index in region is the region's area
  vector<r_data> region_data;
  vector<thresh> colors;

  blob_detect();
  ~blob_detect();
  void run( string name );
  void get_colors();
  int which_color( HSV input );
  void get_u8x3();
  bool good_pixel(int pixel, int c_index);
  void connect_pixels(int pixel, int c_index);
  void run_detector();

};
