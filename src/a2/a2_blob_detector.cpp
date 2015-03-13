#include "imagesource/image_u32.h"
#include "imagesource/image_u8x3.h"
#include <stack>
#include <vector>
#include <cmath>
#include <iostream>
#include <fstream>
using namespace std;

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
  //index in region is the region's area
  vector<r_data> region_data;
  vector<thresh> colors;

  blob_detect(){
    get_u8x3();
    region_83 = image_u8x3_create( image_83->width, image_83->height );
    region = 1;
  }

  ~blob_detect(){
    image_u8x3_destroy( image_83 );
    image_u8x3_destroy( region_83 );
  }

  void get_colors(){
    thresh color;
    ifstream input;
    input.open( "color_input.txt" );
    while( !input.eof() ){
      input >> color.Hmin >> color.Hmax >> color.Smin >> color.Smax >> color.Vmin >> color.Vmax;
      colors.push_back(color);
    }
    cout << "colors " << colors.size() << endl;
    cout << colors[1].Hmin << " " << colors[1].Hmax << " " << colors[1].Smin << " " << colors[1].Smax << " " << colors[1].Vmin << " " << colors[1].Vmax << endl;

  }

  int which_color( HSV input ){
    for(int i = 0; i < int(colors.size()); i++){
      if( input.H >= colors[i].Hmin && input.H <= colors[i].Hmax &&
	  input.S >= colors[i].Smin && input.S <= colors[i].Smax &&
	  input.V >= colors[i].Vmin && input.V <= colors[i].Vmax){
	return i;
      }
    }
    return -1;

  }

  //read in some image and convert it to u8x3 format
  void get_u8x3(){
    image_u32 *image_32;
    uint8_t R,G,B;

    image_32 = image_u32_create_from_pnm( "pic.ppm" );
    image_83 = image_u8x3_create( image_32->width, image_32->height );

    //convert u32 image to a u8x3
    //u8x3 is format R, G, B
    //u32 is a, g, r, b
    char *bufc = (char*)image_32->buf;

    for(int y = 0; y < image_32->height; y++){
      for(int x = 0; x < image_32->width; x++){
	B = bufc[4*(image_32->stride*y+x)+ 0];
	G = bufc[4*(image_32->stride*y+x)+ 1];
	R = bufc[4*(image_32->stride*y+x)+ 2];
      
	image_83->buf[3*x+y*image_83->stride + 2] = R;
	image_83->buf[3*x+y*image_83->stride + 1] = G;
	image_83->buf[3*x+y*image_83->stride + 0] = B;
      }
    }

    //output for debugging
    //image_u8x3_write_pnm( image_83, "pic_out.ppm" );

    //destroy image
    image_u32_destroy( image_32 );
  }
  

  //returns true if the pixel is inbounds and the color we're looking for
  bool good_pixel(int pixel, int c_index){
    HSV temp;
    if (pixel >= 0 && pixel < (image_83->width*3 * image_83->height*image_83->stride) && region_83->buf[pixel] == 0 ){
      RGB_to_HSV( image_83->buf[pixel], image_83->buf[pixel+1], image_83->buf[pixel+2], temp);
      
      if( colors[c_index].Hmin <= temp.H && colors[c_index].Hmax >= temp.H &&
	  colors[c_index].Smin <= temp.S && colors[c_index].Smax >= temp.S &&
	  colors[c_index].Vmin <= temp.V && colors[c_index].Vmax >= temp.V ){
	//cout << "here" << endl;
	return true;
      }
    }
    return false;

  }

  void connect_pixels(int pixel, int c_index){
    int temp = 0;
    int x_sum = 0, y_sum = 0, area = 0;
    r_data t;
    stack<int> pixels;
    pixels.push( pixel );
	
    //loop runs until the stack is empty
    while( !pixels.empty() ) {
      temp = pixels.top();
      pixels.pop();
      //update the area of this region
      area++;
      //add the region to the popped pixel
      region_83->buf[temp] = region;

      //Find the neighboring pixels
      //NW
      if( good_pixel( temp - image_83->stride - 3, c_index ) ){
	pixels.push( temp - image_83->stride - 3);
	y_sum -=image_83->stride;
	x_sum -= 3;
      }
      //N
      if( good_pixel( temp - image_83->stride, c_index ) ){
	pixels.push( temp - image_83->stride);
	y_sum -=image_83->stride;
      }
      //NE
      if( good_pixel( temp - image_83->stride + 3, c_index ) ){
	pixels.push( temp - image_83->stride + 3 );
	y_sum -=image_83->stride;
	x_sum += 3;
      }
      //E
      if( good_pixel( temp + 3, c_index ) ){
	pixels.push( temp + 3 );
	x_sum += 3;
      }
      //SE
      if( good_pixel( temp + image_83->stride + 3, c_index ) ){
	pixels.push( temp + image_83->stride + 3 );
	y_sum +=image_83->stride;
	x_sum += 3;
      }
      //S
      if( good_pixel( temp + image_83->stride, c_index ) ){
	pixels.push( temp + image_83->stride );
	y_sum +=image_83->stride;
      }
      //SW
      if( good_pixel( temp + image_83->stride - 3, c_index ) ){
	pixels.push( temp + image_83->stride - 3 );
	y_sum +=image_83->stride;
	x_sum -= 3;
      }
      //W
      if( good_pixel( temp - 3, c_index ) ){
	pixels.push( temp - 3 );
	x_sum -= 3;
      }
      
	
    }
    
    t.area = area;
    t.x = double(x_sum) / double(area);
    t.y = double(y_sum) / double(area);
    area = 0;
    
    region_data.push_back( t );

  }

  void run_detector(){
    int index = 0;
    int color_index = -1;
    HSV temp;
    //iterate through all elements of the buffer to find all pixels
    for(int y = 0; y < image_83->height; y++){
      for(int x = 0; x < image_83->width; x++){
	//looking for just green now
	index = 3*x + y*image_83->stride;
	if( region_83->buf[ index] == 0 ){
	  
	  RGB_to_HSV( image_83->buf[index], image_83->buf[index+1], image_83->buf[index+2], temp);
	  color_index = which_color(temp);
	  
	  if( color_index != -1){
	    connect_pixels( index, color_index);
	    region+=40;
	  }
		
	}
		
      }
	
    }
	
     //output for debugging
    image_u8x3_write_pnm( region_83, "pic_out.ppm" );

    for(int i = 1; i < region_data.size(); i++)
	cout << "region: " << i << " area: " << region_data[i].area << " x: " << region_data[i].x << " y: " << region_data[i].y << endl;
  }

};

int main(){
  blob_detect B;
  
  //read in all the colors we want to detect
  B.get_colors();
  B.run_detector();

  return 0;
}
