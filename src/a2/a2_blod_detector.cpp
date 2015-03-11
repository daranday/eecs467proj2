#include "image_32.h"


void convert_to_u8x3(){
  image_u32 *image_32;
  image_u8x3 *image_83;
  uint8_t R,G,B;

  image_32 = image_u32_create_from_pnm( "pic.ppm" );
  image_83 = image_u8x3_create( image_32->width, image_32->height);

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

  image_u8x3_write_pnm( image_83, "pic_out.ppm" );

  //destroy image

  image_u32_destroy( image_32 );
  image_u8x3_destroy(image_83);
}

int main(){

  


  return 0;
}
