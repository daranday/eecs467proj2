#include "imagesource/image_u32.h"
#include "imagesource/image_u8x3.h"
#include <stack>
#include <vector>
#include <cmath>
#include <iostream>
using namespace std;

uint8_t Rmin = 100;
uint8_t Rmax = 255;
uint8_t Gmin = 100;
uint8_t Gmax = 255;
uint8_t Bmin = 100;
uint8_t Bmax = 255;

//is HSV for HSV and RGB for RGB, ABC is a placeholder
struct HSV{
  uint32_t H;
  double S;
  double V;
};

struct RGB{
  uint8_t R;
  uint8_t G;
  uint8_t B;
};

//convert HSV values to usable RGB
void HSV_to_RGB( HSV imin, HSV imax, RGB &omin, RGB &omax ){
  double m = 0, C = 0, X = 0;
  double R1, G1, B1;

  //for min
  C = imin.V*imin.S;
  X = C*( 1- abs( (imin.H/ 60 )%2 -1) );
  m = imin.V - C;

  if( imin.H >= 0 && imin.H < 60 ){
    R1 = C; G1 = X; B1 = 0;
  }
  else if( imin.H >= 60 && imin.H < 120 ){
    R1 = X; G1 = C; B1 = 0;
  }
  else if( imin.H >= 120 && imin.H < 180 ){
    R1 = 0; G1 = C; B1 = X;
  }
  else if( imin.H >= 180 && imin.H < 240 ){
    R1 = 0; G1 = X; B1 = C;
  }
  else if( imin.H >= 240 && imin.H < 300 ){
    R1 = X; G1 = 0; B1 = C;
  }
  else{
    R1 = C; G1 = 0; B1 = X;
  }

  omin.R = R1+m; omin.G = G1+m; omin.B = B1+m;

  //for max
  C = imax.V*imax.S;
  X = C*( 1- abs( (imax.H/ 60 )%2 -1) );
  m = imax.V - C;

  if( imax.H >= 0 && imax.H < 60 ){
    R1 = C; G1 = X; B1 = 0;
  }
  else if( imax.H >= 60 && imax.H < 120 ){
    R1 = X; G1 = C; B1 = 0;
  }
  else if( imax.H >= 120 && imax.H < 180 ){
    R1 = 0; G1 = C; B1 = X;
  }
  else if( imax.H >= 180 && imax.H < 240 ){
    R1 = 0; G1 = X; B1 = C;
  }
  else if( imax.H >= 240 && imax.H < 300 ){
    R1 = X; G1 = 0; B1 = C;
  }
  else{
    R1 = C; G1 = 0; B1 = X;
  }

  omax.R = R1+m; omax.G = G1+m; omax.B = B1+m;
  
}


class blob_detect{
public:
  image_u32 *image_32;
  image_u32 *region_32;
  int region;

  blob_detect(){
    //get_u8x3();
    image_32 = image_32 = image_u32_create_from_pnm( "pic.ppm" );
    region_32 = image_u32_create( image_83->width, image_83->height );
    image_u8x3_write_pnm( image_83, "pic_out.ppm" );
    region = 0;
  }

  ~blob_detect(){
    image_u32_destroy( image_32 );
    image_u32_destroy( region_32 );
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
	B = bufc[4*(image_32->stride*y+x)+ 1];
	G = bufc[4*(image_32->stride*y+x)+ 2];
	R = bufc[4*(image_32->stride*y+x)+ 3];
      
	image_83->buf[3*x+y*image_83->stride + 0] = R;
	image_83->buf[3*x+y*image_83->stride + 1] = G;
	image_83->buf[3*x+y*image_83->stride + 2] = B;
      }
    }

    //output for debugging
    //image_u8x3_write_pnm( image_83, "pic_out.ppm" );

    //destroy image
    image_u32_destroy( image_32 );
  }
  

  //returns true if the pixel is inbounds and the color we're looking for
  bool good_pixel(int pixel){
    return pixel >= 0 && pixel < (image_83->width*3 * image_83->height*image_83->stride) && image_83->buf[pixel] == 0 &&
           image_83->buf[pixel] >= Rmin && image_83->buf[pixel+1] >= Gmin && image_83->buf[pixel+2] >= Bmin &&
           image_83->buf[pixel] <= Rmax && image_83->buf[pixel+1] <= Gmax && image_83->buf[pixel+2] <= Bmax;

  }

  void connect_pixels(int pixel){
    int temp = 0;
    stack<int> pixels;
    pixels.push( image_83->buf[pixel] );
	
    //loop runs until the stack is empty
    while( !pixels.empty() ) {
      temp = pixels.top();
      pixels.pop();
		
      //add the region to the popped pixel
      region_83->buf[temp] = region;
      cout << region << endl;
      //NW
      if( good_pixel( temp - image_83->stride - 3 ) ){
	pixels.push( temp - image_83->stride - 3 );
      }
      //N
      if( good_pixel( temp - image_83->stride ) ){
	pixels.push( temp - image_83->stride);
      }
      //NE
      if( good_pixel( temp - image_83->stride + 3 ) ){
	pixels.push( temp - image_83->stride + 3 );
      }
      //E
      if( good_pixel( temp + 3 ) ){
	pixels.push( temp + 3 );
      }
      //SE
      if( good_pixel( temp + image_83->stride + 3 ) ){
	pixels.push( temp + image_83->stride + 3 );
      }
      //S
      if( good_pixel( temp + image_83->stride ) ){
	pixels.push( temp + image_83->stride );
      }
      //SW
      if( good_pixel( temp + image_83->stride - 3 ) ){
	pixels.push( temp + image_83->stride - 3 );
      }
	
	
    }

  }

  void run_detector(){
    int index = 0;
    //iterate through all elements of the buffer to find all pixels
    for(int y = 0; y < image_83->height; y++){
      for(int x = 0; x < image_83->width; x++){
	//looking for just green now
	index = 3*x + y*image_83->stride;
	if( image_83->buf[index] >= Rmin && image_83->buf[index+1] >= Gmin && image_83->buf[index+2] >= Bmin &&
	    image_83->buf[index] <= Rmax && image_83->buf[index+1] <= Gmax && image_83->buf[index+2] <= Bmax &&
	    region_83->buf[ index] == 0 ){

	  //cout << "try to connect" << endl;
	  connect_pixels( index);
	  if(region < 255)
	    region++;
		
	}
		
      }
	
    }
	
     //output for debugging
    image_u8x3_write_pnm( region_83, "pic_out.ppm" );
  }

};

int main(){
  blob_detect B;
  B.run_detector();

  return 0;
}
