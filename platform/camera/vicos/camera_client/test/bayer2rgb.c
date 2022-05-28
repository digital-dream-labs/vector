#include <fcntl.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <getopt.h>

// Forward declare image processing methods
void bayer_bggr10_downsample(const uint8_t *bayer, uint8_t *rgb, int bayer_sx, int bayer_sy, int bpp);

// tiff types: short = 3, int = 4
// Tags: ( 2-byte tag ) ( 2-byte type ) ( 4-byte count ) ( 4-byte data )
//    0100 0003 0000 0001 0064 0000
//       |        |    |         |
// tag --+        |    |         |
// short int -----+    |         |
// one value ----------+         |
// value of 100 -----------------+
//
#define TIFF_HDR_NUM_ENTRY 8
#define TIFF_HDR_SIZE 10+TIFF_HDR_NUM_ENTRY*12 
uint8_t tiff_header[TIFF_HDR_SIZE] = {
	// I     I     42    
	  0x49, 0x49, 0x2a, 0x00,
	// ( offset to tags, 0 )  
	  0x08, 0x00, 0x00, 0x00, 
	// ( num tags )  
	  0x08, 0x00, 
	// ( newsubfiletype, 0 full-image )
	  0xfe, 0x00, 0x04, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	// ( image width )
	  0x00, 0x01, 0x03, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	// ( image height )
	  0x01, 0x01, 0x03, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	// ( bits per sample )
	  0x02, 0x01, 0x03, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	// ( Photometric Interpretation, 2 = RGB )
	  0x06, 0x01, 0x03, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 
	// ( Strip offsets, 8 )
	  0x11, 0x01, 0x03, 0x00, 0x01, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 
	// ( samples per pixel, 3 - RGB)
	  0x15, 0x01, 0x03, 0x00, 0x01, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00,
	// ( Strip byte count )
	  0x17, 0x01, 0x04, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
};

uint8_t * put_tiff(uint8_t * rgb, uint32_t width, uint32_t height, uint16_t bpp)
{
	uint32_t ulTemp=0;
	uint16_t sTemp=0;
	memcpy(rgb, tiff_header, TIFF_HDR_SIZE);

	sTemp = TIFF_HDR_NUM_ENTRY;
	memcpy(rgb + 8, &sTemp, 2);

	memcpy(rgb + 10 + 1*12 + 8, &width, 4);
	memcpy(rgb + 10 + 2*12 + 8, &height, 4);
	memcpy(rgb + 10 + 3*12 + 8, &bpp, 2);

	// strip byte count
	ulTemp = width * height * (bpp / 8) * 3;
	memcpy(rgb + 10 + 7*12 + 8, &ulTemp, 4);

	//strip offset
	sTemp = TIFF_HDR_SIZE;
	memcpy(rgb + 10 + 5*12 + 8, &sTemp, 2);

	return rgb + TIFF_HDR_SIZE;
};

void
usage( char * name )
{
  printf("usage: %s\n", name);
  printf("   --input,-i     input file\n");
  printf("   --output,-o    output file\n");
  printf("   --width,-w     image width (pixels)\n");
  printf("   --height,-v    image height (pixels)\n");
  printf("   --bpp,-b       input image bits-per-pixel\n");
  printf("   --tiff,-t      add a tiff header\n");
  printf("   --help,-h      this helpful message\n");
}

int main(int argc, char* argv[])
{
  uint32_t in_size = 0, out_size = 0;
  uint32_t in_width = 1600;
  uint32_t in_height = 720;
  uint32_t in_bpp = 10;
  uint32_t out_bpp = 8;
	int tiff = 0;
  char *infile = NULL, *outfile = NULL;
  int input_fd = 0;
  int output_fd = 0;
  void * bayer = NULL;
  void * rgb = NULL, *rgb_start = NULL;
  char c;
  int optidx = 0;

  struct option longopt[] = {
    {"input",1,NULL,'i'},
    {"output",1,NULL,'o'},
    {"width",1,NULL,'w'},
    {"height",1,NULL,'v'},
    {"bpp",1,NULL,'b'},
    {"help",0,NULL,'h'},
    {"tiff",0,NULL,'t'},
    {0,0,0,0}
  };

  while ((c=getopt_long(argc,argv,"i:o:w:v:b:th",longopt,&optidx)) != -1) {
      switch ( c )
      {
        case 'i':
            infile = strdup( optarg );
            break;
        case 'o':
            outfile = strdup( optarg );
            break;
        case 'w':
            in_width = strtol( optarg, NULL, 10 );
            break;
        case 'v':
            in_height = strtol( optarg, NULL, 10 );
            break;
        case 'b':
            in_bpp = strtol( optarg, NULL, 10 );
            break;
        case 't':
          tiff = TIFF_HDR_SIZE;
          break;
        case 'h':
          usage(argv[0]);
          return 0;
          break;
        default:
          printf("bad arg\n");
          usage(argv[0]);
          return 1;
      }
  }

  // arguments: infile outfile width height bpp first_color
  if( infile == NULL || outfile == NULL || in_bpp == 0 || in_width == 0 || in_height == 0 )
  {
      printf("Bad parameter\n");
      usage(argv[0]);
      return 1;
  }

  input_fd = open(infile, O_RDONLY);
  if(input_fd < 0)
  {
      printf("Problem opening input: %s\n", infile);
      return 1;
  }

  output_fd = open(outfile, O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH );
  if(output_fd < 0)
  {
      printf("Problem opening output: %s\n", outfile);
      return 1;
  }

  in_size = lseek(input_fd, 0, SEEK_END );
  lseek(input_fd, 0, 0);

  uint32_t out_width = in_width * out_bpp / in_bpp / 2;
  uint32_t out_height = in_height / 2;

  out_size = out_width * out_height * (out_bpp / 8) * 3 + tiff;

  ftruncate(output_fd, out_size );

  bayer = mmap(NULL, in_size, PROT_READ | PROT_WRITE, MAP_PRIVATE /*| MAP_POPULATE*/, input_fd, 0);
  if( bayer == MAP_FAILED )
  {
      perror("Faild mmaping input");
      return 1;
  }
  rgb_start = rgb = mmap(NULL, out_size, PROT_READ | PROT_WRITE, MAP_SHARED /*| MAP_POPULATE*/, output_fd, 0);
  if( rgb == MAP_FAILED )
  {
      perror("Faild mmaping output");
      return 1;
  }
#ifndef NDEBUG
  printf("%p -> %p\n", bayer, rgb);

  printf("%s: %s(%d) %s(%d) %d %d %d\n", argv[0], infile, in_size, outfile, out_size, out_width, out_height, out_bpp );
#endif

	if(tiff)
	{
		rgb_start = put_tiff(rgb, out_width, out_height, out_bpp);
	}

  bayer_bggr10_downsample((const uint8_t*)bayer, (uint8_t*)rgb_start, in_width, in_height, in_bpp);

#ifndef NDEBUG
	printf("Last few In: %x %x %x %x\n", 
			((uint32_t*)bayer)[0],
			((uint32_t*)bayer)[1],
			((uint32_t*)bayer)[2],
			((uint32_t*)bayer)[3]);

	printf("Last few Out: %x %x %x %x\n", 
			((uint32_t*)rgb)[0],
			((uint32_t*)rgb)[1],
			((uint32_t*)rgb)[2],
			((uint32_t*)rgb)[3]);
#endif

  munmap(bayer,in_size);
  close(input_fd);

  if( msync(rgb, out_size, MS_INVALIDATE|MS_SYNC) != 0 )
  perror("Problem msyncing");
  munmap(rgb,out_size);
  if( fsync(output_fd) != 0 )
  perror("Problem fsyncing");
  close(output_fd);

  return 0;

}

#define CLIP(in__, out__) out__ = ( ((in__) < 0) ? 0 : ((in__) > 255 ? 255 : (in__)) )

//
// raw RDI pixel format is CAM_FORMAT_BAYER_MIPI_RAW_10BPP_BGGR
// This appears to be the same as:
// https://www.linuxtv.org/downloads/v4l-dvb-apis-old/pixfmt-srggb10p.html
//
// 4 pixels stored in 5 bytes. Each of the first 4 bytes contain the 8 high order bits
// of the pixel. The 5th byte contains the two least significant bits of each pixel in the
// same order.
//
void bayer_bggr10_downsample(const uint8_t *bayer, uint8_t *rgb, int bayer_sx, int bayer_sy, int bpp)
{
  uint8_t *outR, *outG, *outB;
  register int i, j;
  int tmp;

  outB = &rgb[0];
  outG = &rgb[1];
  outR = &rgb[2];

  // // Raw image are reported as 1280x720, 10bpp BGGR MIPI Bayer format
  // // Based on inspection, the image dimensions are actually 1600x576 10bpp pixels.
  // // Simple conversion + downsample to RGB yields: 640x288 images
  const int dim = (bayer_sx*bayer_sy);

  // output rows have 1280 8-bit pixels
  const int out_sx = bayer_sx * 8/bpp;
  const int out_sy = bayer_sy;

  const int dim_step = (bayer_sx << 1);
  const int out_dim_step = (out_sx << 1);

  int out_i, out_j;
  
  for (i = 0, out_i = 0; i < dim; i += dim_step, out_i += out_dim_step) {
    // process 2 rows at a time
    for (j = 0, out_j = 0; j < bayer_sx; j += 5, out_j += 4) {
      // process 4 col at a time
      // A B A_ B_ -> B G B G
      // C D C_ D_    G R G R

      // Read 4 10-bit px from row0
      const int r0_idx = i + j;
      uint16_t px_A  = (bayer[r0_idx+0] << 2) | ((bayer[r0_idx+4] & 0xc0) >> 6);
      uint16_t px_B  = (bayer[r0_idx+1] << 2) | ((bayer[r0_idx+4] & 0x30) >> 4);
      uint16_t px_A_ = (bayer[r0_idx+2] << 2) | ((bayer[r0_idx+4] & 0x0c) >> 2);
      uint16_t px_B_ = (bayer[r0_idx+3] << 2) |  (bayer[r0_idx+4] & 0x03);

      // Read 4 10-bit px from row1
      const int r1_idx = (i + bayer_sx) + j;
      uint16_t px_C  = (bayer[r1_idx+0] << 2) | ((bayer[r1_idx+4] & 0xc0) >> 6);
      uint16_t px_D  = (bayer[r1_idx+1] << 2) | ((bayer[r1_idx+4] & 0x30) >> 4);
      uint16_t px_C_ = (bayer[r1_idx+2] << 2) | ((bayer[r1_idx+4] & 0x0c) >> 2);
      uint16_t px_D_ = (bayer[r1_idx+3] << 2) |  (bayer[r1_idx+4] & 0x03);

      // output index:
      // out_i represents 2 rows -> divide by 4
      // out_j represents 1 col  -> divide by 2
      const int rgb_0_idx = ((out_i >> 2) + (out_j >> 1)) * 3;
      tmp = (px_C + px_B) >> 1;
      CLIP(tmp, outG[rgb_0_idx]);
      tmp = px_D;
      CLIP(tmp, outR[rgb_0_idx]);
      tmp = px_A;
      CLIP(tmp, outB[rgb_0_idx]);

      const int rgb_1_idx = ((out_i >> 2) + ((out_j+2) >> 1)) * 3;
      tmp = (px_C_ + px_B_) >> 1;
      CLIP(tmp, outG[rgb_1_idx]);
      tmp = px_D_;
      CLIP(tmp, outR[rgb_1_idx]);
      tmp = px_A_;
      CLIP(tmp, outB[rgb_1_idx]);
    }
  }
}