#include "imagesource/image_u32.h"
#include "imagesource/image_u8x3.h"
#include <stack>
#include <vector>
#include <cmath>
#include <iostream>
#include <fstream>
#include <string>
#include "a2_blob_detector.h"
using namespace std;

blob_detect::blob_detect(){
  get_u8x3();
  region_83 = image_u8x3_create( image_83->width, image_83->height );
  region = 1;
}

blob_detect::~blob_detect(){
  image_u8x3_destroy( image_83 );
  image_u8x3_destroy( region_83 );
}

void blob_detect::run( string name ){
  file = name;
  get_colors();
  run_detector();
}

void blob_detect::get_colors(){
  thresh color;
  ifstream input;
  input.open( file );
  while( !input.eof() ){
    input >> color.Hmin >> color.Hmax >> color.Smin >> color.Smax >> color.Vmin >> color.Vmax;
    colors.push_back(color);
  }
  cout << "colors " << colors.size() << endl;
  cout << colors[1].Hmin << " " << colors[1].Hmax << " " << colors[1].Smin << " " << colors[1].Smax << " " << colors[1].Vmin << " " << colors[1].Vmax << endl;

}

int blob_detect::which_color( HSV input ){
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
void blob_detect::get_u8x3(){
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
bool blob_detect::good_pixel(int pixel, int c_index){
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

void blob_detect::connect_pixels(int pixel, int c_index){
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

void blob_detect::run_detector(){
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

int main(){
   blob_detect B;
   B.run("color_input.txt");
   return 0;
}
