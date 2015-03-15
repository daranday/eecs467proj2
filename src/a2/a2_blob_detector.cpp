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
  x_mask_min =  y_mask_min =  0;
  x_mask_max = image_83->width;
  y_mask_max = image_83->height;
  region = 1;
}

blob_detect::~blob_detect(){
  image_u8x3_destroy( image_83 );
  image_u8x3_destroy( region_83 );
}

void blob_detect::run(vector<vector<double>> &input){
  get_colors(input);
  run_detector();
}

void blob_detect::get_mask( vector<int> &input ){
  x_mask_min = input[0];
  x_mask_max = input[2];
  y_mask_min = input[1];
  y_mask_max = input[3];

}

void blob_detect::get_colors( vector<vector<double>> &input){
  thresh color;
  for(size_t i = 0; i < input.size(); i++){
    color.Hmin = input[i][0];
    color.Hmax = input[i][1];
    color.Smin = input[i][2];
    color.Smax = input[i][3];
    color.Vmin = input[i][4];
    color.Vmax = input[i][5];

  }

  colors.push_back(color);
  
  /*
  ifstream input
  input.open( file );
  while( !input.eof() ){
    input >> color.Hmin >> color.Hmax >> color.Smin >> color.Smax >> color.Vmin >> color.Vmax;
    colors.push_back(color);
  }
  cout << "colors " << colors.size() << endl;
  cout << colors[1].Hmin << " " << colors[1].Hmax << " " << colors[1].Smin << " " << colors[1].Smax << " " << colors[1].Vmin << " " << colors[1].Vmax << endl;
  */
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
  int sum = pixel, sum_x = (pixel % image_83->stride), area = 0;
  r_data t;
  stack<int> pixels;
  pixels.push( pixel );
  region_83->buf[pixel] = region;
  area++;
  
  

  while( !pixels.empty() ) {
    temp = pixels.top();
    pixels.pop();

    //Find the neighboring pixels
    //NW
    if( good_pixel( temp - image_83->stride - 3, c_index ) ){
      pixels.push( temp - image_83->stride - 3);
      region_83->buf[ pixels.top() ] = region;
      area++;

      sum += pixels.top();
      sum_x += (pixels.top() % image_83->stride);
      
    }
    //N
    if( good_pixel( temp - image_83->stride, c_index ) ){
      pixels.push( temp - image_83->stride);
      region_83->buf[ pixels.top() ] = region;
      area++;
      
      sum += pixels.top();
      sum_x += (pixels.top() % image_83->stride);
      
    }
    //NE
    if( good_pixel( temp - image_83->stride + 3, c_index ) ){
      pixels.push( temp - image_83->stride + 3 );
      region_83->buf[ pixels.top() ] = region;
      area++;

      sum += pixels.top();
      sum_x += (pixels.top() % image_83->stride);
      
    }
    //E
    if( good_pixel( temp + 3, c_index ) ){
      pixels.push( temp + 3 );
      region_83->buf[ pixels.top() ] = region;
      area++;

      sum += pixels.top();
      sum_x += (pixels.top() % image_83->stride);
      
    }
    //SE
    if( good_pixel( temp + image_83->stride + 3, c_index ) ){
      pixels.push( temp + image_83->stride + 3 );
      region_83->buf[ pixels.top() ] = region;
      area++;

      sum += pixels.top();
      sum_x += (pixels.top() % image_83->stride);
     
    }
    //S
    if( good_pixel( temp + image_83->stride, c_index ) ){
      pixels.push( temp + image_83->stride );
      region_83->buf[ pixels.top() ] = region;
      area++;

      sum += pixels.top();
      sum_x += (pixels.top() % image_83->stride);
      
    }
    //SW
    if( good_pixel( temp + image_83->stride - 3, c_index ) ){
      pixels.push( temp + image_83->stride - 3 );
      region_83->buf[ pixels.top() ] = region;
      area++;

      sum += pixels.top();
      sum_x += (pixels.top() % image_83->stride);
      
    }
    //W
    if( good_pixel( temp - 3, c_index ) ){
      pixels.push( temp - 3 );
      region_83->buf[ pixels.top() ] = region;
      area++;

      sum += pixels.top();
      sum_x += (pixels.top() % image_83->stride);
 
    }
      
	
  }


  double center = double(sum/3)/double(area);
  double x_center = double(sum_x)/double(area);

  

  t.area = area;
  t.x = x_center / 3.0;
  t.y = center / double(image_83->stride/3);
  area = 0;
    
  region_data.push_back( t );

}

void blob_detect::run_detector(){
  int index = 0;
  int color_index = -1;
  HSV temp;
  //iterate through all elements of the buffer to find all pixels
  for(int y = y_mask_min; y < y_mask_max; y++){
    for(int x = x_mask_min; x < x_mask_max; x++){
      //looking for just green now
      index = 3*x + y*image_83->stride;
      if( region_83->buf[ index] == 0 ){
	  
	RGB_to_HSV( image_83->buf[index], image_83->buf[index+1], image_83->buf[index+2], temp);
	color_index = which_color(temp);
	  
	if( color_index != -1){
	  connect_pixels( index, color_index);
	  region_data[region_data.size()-1].H = (colors[color_index].Hmin + colors[color_index].Hmax)/2.0;
	  region_data[region_data.size()-1].S = (colors[color_index].Smin + colors[color_index].Smax)/2.0;
	  region_data[region_data.size()-1].V = (colors[color_index].Vmin + colors[color_index].Vmax)/2.0;
	  region+=40;
	}
		
      }
		
    }
	
  }
	
  //output for debugging
  image_u8x3_write_pnm( region_83, "pic_out.ppm" );

  for(int i = 0; i < region_data.size(); i++)
    cout << "region: " << i << " area: " << region_data[i].area << " x: " << region_data[i].x << " y: " << region_data[i].y << endl;
}

int main(){
  vector<vector<double>> color(1);
  color[0].resize(6);
  color[0][0] = 160;
  color[0][1] = 210;
  color[0][2] = 15;
  color[0][3] = 60;
  color[0][4] = 60; 
  color[0][5] = 100;

   blob_detect B;
   B.run(color);
   return 0;
}
