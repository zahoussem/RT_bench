#ifndef PPC
typedef int        TOTAL_TYPE; /* this is faster for "int" but should be "float" for large d masks */
#else
typedef float      TOTAL_TYPE; /* for my PowerPC accelerator only */
#endif

#ifndef __USE_GNU
#define __USE_GNU
#endif
#define _GNU_SOURCE
#include <sched.h>

/*#define FOPENB*/           /* uncomment if using djgpp gnu C for DOS or certain Win95 compilers */
#define SEVEN_SUPP           /* size for non-max corner suppression; SEVEN_SUPP or FIVE_SUPP */
#define MAX_CORNERS   15000  /* max corners per frame */

/* ********** Leave the rest - but you may need to remove one or both of sys/file.h and malloc.h lines */
#include <sched.h>
#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <sys/file.h>    /* may want to remove this line */
#include <malloc.h>  
#include <pthread.h>
#include <unistd.h>    /* may want to remove this line */
#define  exit_error(IFB,IFC) { fprintf(stderr,IFB,IFC); exit(0); }
#define  FTOI(a) ( (a) < 0 ? ((int)(a-0.5)) : ((int)(a+0.5)) )
typedef  unsigned char uchar;
typedef  struct {int x,y,info, dx, dy, I;} CORNER_LIST[MAX_CORNERS];

/* }}} */
/* {{{ usage() */
struct repetitive {
  int index; 
  int priority;
  int proc;
  int itSize;
  char* label;
};
//add compare time spec
int comp_spec(struct timespec a, struct timespec b){
  if (a.tv_sec > b.tv_sec) return 1;
  else if (a.tv_sec < b.tv_sec) return 0;
  else if (a.tv_nsec > b.tv_nsec) return 1;
  else if (a.tv_nsec == b.tv_nsec) return 1;
  else return 0; 
}

// add to spectimes
struct timespec add_spec(struct timespec End, struct timespec Begin)
{
  long a_nsec = End.tv_nsec + Begin.tv_nsec;
  long a_sec  = End.tv_sec  + Begin.tv_sec;
  if (a_nsec > 1000000000 ) {
    a_sec = a_sec  + 1 ; 
    a_nsec= a_nsec - 1000000000  ;
  }
  struct timespec add; 
  add.tv_sec = a_sec; 
  add.tv_nsec = a_nsec;
  return (add);
}
// differance between two spectimes
struct timespec diff_spec(struct timespec End, struct timespec Begin)
{
  long d_nsec = End.tv_nsec - Begin.tv_nsec;
  long d_sec  = End.tv_sec  - Begin.tv_sec;
  if (d_nsec < 0 ) {
    d_sec = d_sec  - 1 ; 
    d_nsec= 1000000000 + d_nsec;
  }
  struct timespec diff; 
  diff.tv_sec = d_sec; 
  diff.tv_nsec = d_nsec;
  return (diff);
}

// convert spectime to time in second
double toSec(struct timespec t){
  return (double) (t.tv_sec) + (t.tv_nsec)/(double) 1000000000;
} 

// convert time in second to spectime
struct timespec toSpec (double s){
  long s_sec = (long)(floor(s));
  long s_nsec = (s  - floor(s)) * 1000000000;
  struct timespec  returned;
  returned.tv_sec = s_sec;
  returned.tv_nsec= s_nsec;
  return returned;
}


usage()
{
  printf("Usage: susan <in.pgm> <out.pgm> [options]\n\n");

  printf("-s : Smoothing mode (default)\n");
  printf("-e : Edges mode\n");
  printf("-c : Corners mode\n\n");

  printf("See source code for more information about setting the thresholds\n");
  printf("-t <thresh> : Brightness threshold, all modes (default=20)\n");
  printf("-d <thresh> : Distance threshold, smoothing mode, (default=4) (use next option instead for flat 3x3 mask)\n");
  printf("-3 : Use flat 3x3 mask, edges or smoothing mode\n");
  printf("-n : No post-processing on the binary edge map (runs much faster); edges mode\n");
  printf("-q : Use faster (and usually stabler) corner mode; edge-like corner suppression not carried out; corners mode\n");
  printf("-b : Mark corners/edges with single black points instead of black with white border; corners or edges mode\n");
  printf("-p : Output initial enhancement image only; corners or edges mode (default is edges mode)\n");

  printf("\nSUSAN Version 2l (C) 1995-1997 Stephen Smith, DRA UK. steve@fmrib.ox.ac.uk\n");

  exit(0);
}

/* }}} */
/* {{{ get_image(filename,in,x_size,y_size) */

/* {{{ int getint(fp) derived from XV */

int getint(fd)
  FILE *fd;
{
  int c, i;
  char dummy[10000];

  c = getc(fd);
  while (1) /* find next integer */
  {
    if (c=='#')    /* if we're at a comment, read to end of line */
      fgets(dummy,9000,fd);
    if (c==EOF)
      exit_error("Image %s not binary PGM.\n","is");
    if (c>='0' && c<='9')
      break;   /* found what we were looking for */
    c = getc(fd);
  }

  /* we're at the start of a number, continue until we hit a non-number */
  i = 0;
  while (1) {
    i = (i*10) + (c - '0');
    c = getc(fd);
    if (c==EOF) return (i);
    if (c<'0' || c>'9') break;
  }

  return (i);
}

/* }}} */

void get_image(filename,in,x_size,y_size)
  char           filename[200];
  unsigned char  **in;
  int            *x_size, *y_size;
{
FILE  *fd;
char header [100];
int  tmp;

#ifdef FOPENB
  if ((fd=fopen(filename,"rb")) == NULL)
#else
  if ((fd=fopen(filename,"r")) == NULL)
#endif
    exit_error("Can't input image %s.\n",filename);

  /* {{{ read header */

  header[0]=fgetc(fd);
  header[1]=fgetc(fd);
  if(!(header[0]=='P' && header[1]=='5'))
    exit_error("Image %s does not have binary PGM header.\n",filename);

  *x_size = getint(fd);
  *y_size = getint(fd);
  tmp = getint(fd);

/* }}} */

  *in = (uchar *) malloc(*x_size * *y_size);

  if (fread(*in,1,*x_size * *y_size,fd) == 0)
    exit_error("Image %s is wrong size.\n",filename);

  fclose(fd);
}

/* }}} */
/* {{{ put_image(filename,in,x_size,y_size) */

put_image(filename,in,x_size,y_size)
  char filename [100],
       *in;
  int  x_size,
       y_size;
{
FILE  *fd;

#ifdef FOPENB
  if ((fd=fopen(filename,"wb")) == NULL) 
#else
  if ((fd=fopen(filename,"w")) == NULL) 
#endif
    exit_error("Can't output image%s.\n",filename);

  fprintf(fd,"P5\n");
  fprintf(fd,"%d %d\n",x_size,y_size);
  fprintf(fd,"255\n");
  
  if (fwrite(in,x_size*y_size,1,fd) != 1)
    exit_error("Can't write image %s.\n",filename);

  fclose(fd);
}

/* }}} */
/* {{{ int_to_uchar(r,in,size) */

int_to_uchar(r,in,size)
  uchar *in;
  int   *r, size;
{
int i,
    max_r=r[0],
    min_r=r[0];

  for (i=0; i<size; i++)
    {
      if ( r[i] > max_r )
        max_r=r[i];
      if ( r[i] < min_r )
        min_r=r[i];
    }

  /*printf("min=%d max=%d\n",min_r,max_r);*/

  max_r-=min_r;

  for (i=0; i<size; i++)
    in[i] = (uchar)((int)((int)(r[i]-min_r)*255)/max_r);
}

/* }}} */
/* {{{ setup_brightness_lut(bp,thresh,form) */

void setup_brightness_lut(bp,thresh,form)
  uchar **bp;
  int   thresh, form;
{
int   k;
float temp;

  *bp=(unsigned char *)malloc(516);
  *bp=*bp+258;

  for(k=-256;k<257;k++)
  {
    temp=((float)k)/((float)thresh);
    temp=temp*temp;
    if (form==6)
      temp=temp*temp*temp;
    temp=100.0*exp(-temp);
    *(*bp+k)= (uchar)temp;
  }
}

/* }}} */
/* {{{ susan principle */

/* {{{ susan_principle(in,r,bp,max_no,x_size,y_size) */

susan_principle(in,r,bp,max_no,x_size,y_size)
  uchar *in, *bp;
  int   *r, max_no, x_size, y_size;
{
int   i, j, n;
uchar *p,*cp;

  memset (r,0,x_size * y_size * sizeof(int));

  for (i=3;i<y_size-3;i++)
    for (j=3;j<x_size-3;j++)
    {
      n=100;
      p=in + (i-3)*x_size + j - 1;
      cp=bp + in[i*x_size+j];

      n+=*(cp-*p++);
      n+=*(cp-*p++);
      n+=*(cp-*p);
      p+=x_size-3; 

      n+=*(cp-*p++);
      n+=*(cp-*p++);
      n+=*(cp-*p++);
      n+=*(cp-*p++);
      n+=*(cp-*p);
      p+=x_size-5;

      n+=*(cp-*p++);
      n+=*(cp-*p++);
      n+=*(cp-*p++);
      n+=*(cp-*p++);
      n+=*(cp-*p++);
      n+=*(cp-*p++);
      n+=*(cp-*p);
      p+=x_size-6;

      n+=*(cp-*p++);
      n+=*(cp-*p++);
      n+=*(cp-*p);
      p+=2;
      n+=*(cp-*p++);
      n+=*(cp-*p++);
      n+=*(cp-*p);
      p+=x_size-6;

      n+=*(cp-*p++);
      n+=*(cp-*p++);
      n+=*(cp-*p++);
      n+=*(cp-*p++);
      n+=*(cp-*p++);
      n+=*(cp-*p++);
      n+=*(cp-*p);
      p+=x_size-5;

      n+=*(cp-*p++);
      n+=*(cp-*p++);
      n+=*(cp-*p++);
      n+=*(cp-*p++);
      n+=*(cp-*p);
      p+=x_size-3;

      n+=*(cp-*p++);
      n+=*(cp-*p++);
      n+=*(cp-*p);

      if (n<=max_no)
        r[i*x_size+j] = max_no - n;
    }
}

/* }}} */
/* {{{ susan_principle_small(in,r,bp,max_no,x_size,y_size) */

susan_principle_small(in,r,bp,max_no,x_size,y_size)
  uchar *in, *bp;
  int   *r, max_no, x_size, y_size;
{
int   i, j, n;
uchar *p,*cp;

  memset (r,0,x_size * y_size * sizeof(int));

  max_no = 730; /* ho hum ;) */

  for (i=1;i<y_size-1;i++)
    for (j=1;j<x_size-1;j++)
    {
      n=100;
      p=in + (i-1)*x_size + j - 1;
      cp=bp + in[i*x_size+j];

      n+=*(cp-*p++);
      n+=*(cp-*p++);
      n+=*(cp-*p);
      p+=x_size-2; 

      n+=*(cp-*p);
      p+=2;
      n+=*(cp-*p);
      p+=x_size-2;

      n+=*(cp-*p++);
      n+=*(cp-*p++);
      n+=*(cp-*p);

      if (n<=max_no)
        r[i*x_size+j] = max_no - n;
    }
}

/* }}} */

/* }}} */
/* {{{ smoothing */

/* {{{ median(in,i,j,x_size) */

uchar median(in,i,j,x_size)
  uchar *in;
  int   i, j, x_size;
{
int p[8],k,l,tmp;

  p[0]=in[(i-1)*x_size+j-1];
  p[1]=in[(i-1)*x_size+j  ];
  p[2]=in[(i-1)*x_size+j+1];
  p[3]=in[(i  )*x_size+j-1];
  p[4]=in[(i  )*x_size+j+1];
  p[5]=in[(i+1)*x_size+j-1];
  p[6]=in[(i+1)*x_size+j  ];
  p[7]=in[(i+1)*x_size+j+1];

  for(k=0; k<7; k++)
    for(l=0; l<(7-k); l++)
      if (p[l]>p[l+1])
      {
        tmp=p[l]; p[l]=p[l+1]; p[l+1]=tmp;
      }

  return( (p[3]+p[4]) / 2 );
}

/* }}} */
/* {{{ enlarge(in,tmp_image,x_size,y_size,border) */

/* this enlarges "in" so that borders can be dealt with easily */

enlarge(in,tmp_image,x_size,y_size,border)
  uchar **in;
  uchar *tmp_image;
  int   *x_size, *y_size, border;
{
int   i, j;

  for(i=0; i<*y_size; i++)   /* copy *in into tmp_image */
    memcpy(tmp_image+(i+border)*(*x_size+2*border)+border, *in+i* *x_size, *x_size);

  for(i=0; i<border; i++) /* copy top and bottom rows; invert as many as necessary */
  {
    memcpy(tmp_image+(border-1-i)*(*x_size+2*border)+border,*in+i* *x_size,*x_size);
    memcpy(tmp_image+(*y_size+border+i)*(*x_size+2*border)+border,*in+(*y_size-i-1)* *x_size,*x_size);
  }

  for(i=0; i<border; i++) /* copy left and right columns */
    for(j=0; j<*y_size+2*border; j++)
    {
      tmp_image[j*(*x_size+2*border)+border-1-i]=tmp_image[j*(*x_size+2*border)+border+i];
      tmp_image[j*(*x_size+2*border)+ *x_size+border+i]=tmp_image[j*(*x_size+2*border)+ *x_size+border-1-i];
    }

  *x_size+=2*border;  /* alter image size */
  *y_size+=2*border;
  *in=tmp_image;      /* repoint in */
}

/* }}} */
/* {{{ void susan_smoothing(three_by_three,in,dt,x_size,y_size,bp) */

void susan_smoothing(three_by_three,in,dt,x_size,y_size,bp)
  int   three_by_three, x_size, y_size;
  uchar *in, *bp;
  float dt;
{
/* {{{ vars */

float temp;
int   n_max, increment, mask_size,
      i,j,x,y,area,brightness,tmp,centre;
uchar *ip, *dp, *dpt, *cp, *out=in,
      *tmp_image;
TOTAL_TYPE total;

/* }}} */

  /* {{{ setup larger image and border sizes */

  if (three_by_three==0)
    mask_size = ((int)(1.5 * dt)) + 1;
  else
    mask_size = 1;

  total=0.1; /* test for total's type */
  if ( (dt>15) && (total==0) )
  {
    printf("Distance_thresh (%f) too big for integer arithmetic.\n",dt);
    printf("Either reduce it to <=15 or recompile with variable \"total\"\n");
    printf("as a float: see top \"defines\" section.\n");
    exit(0);
  }

  if ( (2*mask_size+1>x_size) || (2*mask_size+1>y_size) )
  {
    printf("Mask size (1.5*distance_thresh+1=%d) too big for image (%dx%d).\n",mask_size,x_size,y_size);
    exit(0);
  }

  tmp_image = (uchar *) malloc( (x_size+mask_size*2) * (y_size+mask_size*2) );
  enlarge(&in,tmp_image,&x_size,&y_size,mask_size);

/* }}} */

  if (three_by_three==0)
  {     /* large Gaussian masks */
    /* {{{ setup distance lut */

  n_max = (mask_size*2) + 1;

  increment = x_size - n_max;

  dp     = (unsigned char *)malloc(n_max*n_max);
  dpt    = dp;
  temp   = -(dt*dt);

  for(i=-mask_size; i<=mask_size; i++)
    for(j=-mask_size; j<=mask_size; j++)
    {
      x = (int) (100.0 * exp( ((float)((i*i)+(j*j))) / temp ));
      *dpt++ = (unsigned char)x;
    }

/* }}} */
    /* {{{ main section */

  for (i=mask_size;i<y_size-mask_size;i++)
  {
    for (j=mask_size;j<x_size-mask_size;j++)
    {
      area = 0;
      total = 0;
      dpt = dp;
      ip = in + ((i-mask_size)*x_size) + j - mask_size;
      centre = in[i*x_size+j];
      cp = bp + centre;
      for(y=-mask_size; y<=mask_size; y++)
      {
        for(x=-mask_size; x<=mask_size; x++)
	{
          brightness = *ip++;
          tmp = *dpt++ * *(cp-brightness);
          area += tmp;
          total += tmp * brightness;
        }
        ip += increment;
      }
      tmp = area-10000;
      if (tmp==0)
        *out++=median(in,i,j,x_size);
      else
        *out++=((total-(centre*10000))/tmp);
    }
  }

/* }}} */
  }
  else
  {     /* 3x3 constant mask */
    /* {{{ main section */

  for (i=1;i<y_size-1;i++)
  {
    for (j=1;j<x_size-1;j++)
    {
      area = 0;
      total = 0;
      ip = in + ((i-1)*x_size) + j - 1;
      centre = in[i*x_size+j];
      cp = bp + centre;

      brightness=*ip++; tmp=*(cp-brightness); area += tmp; total += tmp * brightness;
      brightness=*ip++; tmp=*(cp-brightness); area += tmp; total += tmp * brightness;
      brightness=*ip; tmp=*(cp-brightness); area += tmp; total += tmp * brightness;
      ip += x_size-2;
      brightness=*ip++; tmp=*(cp-brightness); area += tmp; total += tmp * brightness;
      brightness=*ip++; tmp=*(cp-brightness); area += tmp; total += tmp * brightness;
      brightness=*ip; tmp=*(cp-brightness); area += tmp; total += tmp * brightness;
      ip += x_size-2;
      brightness=*ip++; tmp=*(cp-brightness); area += tmp; total += tmp * brightness;
      brightness=*ip++; tmp=*(cp-brightness); area += tmp; total += tmp * brightness;
      brightness=*ip; tmp=*(cp-brightness); area += tmp; total += tmp * brightness;

      tmp = area-100;
      if (tmp==0)
        *out++=median(in,i,j,x_size);
      else
        *out++=(total-(centre*100))/tmp;
    }
  }

/* }}} */
  }
}

/* }}} */

/* }}} */
/* {{{ edges */

/* {{{ edge_draw(in,corner_list,drawing_mode) */

edge_draw(in,mid,x_size,y_size,drawing_mode)
  uchar *in, *mid;
  int x_size, y_size, drawing_mode;
{
int   i;
uchar *inp, *midp;

  if (drawing_mode==0)
  {
    /* mark 3x3 white block around each edge point */
    midp=mid;
    for (i=0; i<x_size*y_size; i++)
    {
      if (*midp<8) 
      {
        inp = in + (midp - mid) - x_size - 1;
        *inp++=255; *inp++=255; *inp=255; inp+=x_size-2;
        *inp++=255; *inp++;     *inp=255; inp+=x_size-2;
        *inp++=255; *inp++=255; *inp=255;
      }
      midp++;
    }
  }

  /* now mark 1 black pixel at each edge point */
  midp=mid;
  for (i=0; i<x_size*y_size; i++)
  {
    if (*midp<8) 
      *(in + (midp - mid)) = 0;
    midp++;
  }
}

/* }}} */
/* {{{ susan_thin(r,mid,x_size,y_size) */

/* only one pass is needed as i,j are decremented if necessary to go
   back and do bits again */

susan_thin(r,mid,x_size,y_size)
  uchar *mid;
  int   *r, x_size, y_size;
{
int   l[9], centre, nlinks, npieces,
      b01, b12, b21, b10,
      p1, p2, p3, p4,
      b00, b02, b20, b22,
      m, n, a, b, x, y, i, j;
uchar *mp;

  for (i=4;i<y_size-4;i++)
    for (j=4;j<x_size-4;j++)
      if (mid[i*x_size+j]<8)
      {
        centre = r[i*x_size+j];
        /* {{{ count number of neighbours */

        mp=mid + (i-1)*x_size + j-1;

        n = (*mp<8) +
            (*(mp+1)<8) +
            (*(mp+2)<8) +
            (*(mp+x_size)<8) +
            (*(mp+x_size+2)<8) +
            (*(mp+x_size+x_size)<8) +
            (*(mp+x_size+x_size+1)<8) +
            (*(mp+x_size+x_size+2)<8);

/* }}} */
        /* {{{ n==0 no neighbours - remove point */

        if (n==0)
          mid[i*x_size+j]=100;

/* }}} */
        /* {{{ n==1 - extend line if I can */

        /* extension is only allowed a few times - the value of mid is used to control this */

        if ( (n==1) && (mid[i*x_size+j]<6) )
        {
          /* find maximum neighbour weighted in direction opposite the
             neighbour already present. e.g.
             have: O O O  weight r by 0 2 3
                   X X O              0 0 4
                   O O O              0 2 3     */

          l[0]=r[(i-1)*x_size+j-1]; l[1]=r[(i-1)*x_size+j]; l[2]=r[(i-1)*x_size+j+1];
          l[3]=r[(i  )*x_size+j-1]; l[4]=0;                 l[5]=r[(i  )*x_size+j+1];
          l[6]=r[(i+1)*x_size+j-1]; l[7]=r[(i+1)*x_size+j]; l[8]=r[(i+1)*x_size+j+1];

          if (mid[(i-1)*x_size+j-1]<8)        { l[0]=0; l[1]=0; l[3]=0; l[2]*=2; 
                                                l[6]*=2; l[5]*=3; l[7]*=3; l[8]*=4; }
          else { if (mid[(i-1)*x_size+j]<8)   { l[1]=0; l[0]=0; l[2]=0; l[3]*=2; 
                                                l[5]*=2; l[6]*=3; l[8]*=3; l[7]*=4; }
          else { if (mid[(i-1)*x_size+j+1]<8) { l[2]=0; l[1]=0; l[5]=0; l[0]*=2; 
                                                l[8]*=2; l[3]*=3; l[7]*=3; l[6]*=4; }
          else { if (mid[(i)*x_size+j-1]<8)   { l[3]=0; l[0]=0; l[6]=0; l[1]*=2; 
                                                l[7]*=2; l[2]*=3; l[8]*=3; l[5]*=4; }
          else { if (mid[(i)*x_size+j+1]<8)   { l[5]=0; l[2]=0; l[8]=0; l[1]*=2; 
                                                l[7]*=2; l[0]*=3; l[6]*=3; l[3]*=4; }
          else { if (mid[(i+1)*x_size+j-1]<8) { l[6]=0; l[3]=0; l[7]=0; l[0]*=2; 
                                                l[8]*=2; l[1]*=3; l[5]*=3; l[2]*=4; }
          else { if (mid[(i+1)*x_size+j]<8)   { l[7]=0; l[6]=0; l[8]=0; l[3]*=2; 
                                                l[5]*=2; l[0]*=3; l[2]*=3; l[1]*=4; }
          else { if (mid[(i+1)*x_size+j+1]<8) { l[8]=0; l[5]=0; l[7]=0; l[6]*=2; 
                                                l[2]*=2; l[1]*=3; l[3]*=3; l[0]*=4; } }}}}}}}

          m=0;     /* find the highest point */
          for(y=0; y<3; y++)
            for(x=0; x<3; x++)
              if (l[y+y+y+x]>m) { m=l[y+y+y+x]; a=y; b=x; }

          if (m>0)
          {
            if (mid[i*x_size+j]<4)
              mid[(i+a-1)*x_size+j+b-1] = 4;
            else
              mid[(i+a-1)*x_size+j+b-1] = mid[i*x_size+j]+1;
            if ( (a+a+b) < 3 ) /* need to jump back in image */
	    {
              i+=a-1;
              j+=b-2;
              if (i<4) i=4;
              if (j<4) j=4;
	    }
	  }
        }

/* }}} */
        /* {{{ n==2 */

        if (n==2)
	{
          /* put in a bit here to straighten edges */
          b00 = mid[(i-1)*x_size+j-1]<8; /* corners of 3x3 */
          b02 = mid[(i-1)*x_size+j+1]<8;
	  b20 = mid[(i+1)*x_size+j-1]<8;
          b22 = mid[(i+1)*x_size+j+1]<8;
          if ( ((b00+b02+b20+b22)==2) && ((b00|b22)&(b02|b20)))
	  {  /* case: move a point back into line.
                e.g. X O X  CAN  become X X X
                     O X O              O O O
                     O O O              O O O    */
            if (b00) 
	    {
              if (b02) { x=0; y=-1; }
              else     { x=-1; y=0; }
	    }
            else
	    {
              if (b02) { x=1; y=0; }
              else     { x=0; y=1; }
	    }
            if (((float)r[(i+y)*x_size+j+x]/(float)centre) > 0.7)
	    {
              if ( ( (x==0) && (mid[(i+(2*y))*x_size+j]>7) && (mid[(i+(2*y))*x_size+j-1]>7) && (mid[(i+(2*y))*x_size+j+1]>7) ) ||
                   ( (y==0) && (mid[(i)*x_size+j+(2*x)]>7) && (mid[(i+1)*x_size+j+(2*x)]>7) && (mid[(i-1)*x_size+j+(2*x)]>7) ) )
	      {
                mid[(i)*x_size+j]=100;
                mid[(i+y)*x_size+j+x]=3;  /* no jumping needed */
	      }
	    }
	  }
          else
          {
            b01 = mid[(i-1)*x_size+j  ]<8;
            b12 = mid[(i  )*x_size+j+1]<8;
            b21 = mid[(i+1)*x_size+j  ]<8;
            b10 = mid[(i  )*x_size+j-1]<8;
            /* {{{ right angle ends - not currently used */

#ifdef IGNORETHIS
            if ( (b00&b01)|(b00&b10)|(b02&b01)|(b02&b12)|(b20&b10)|(b20&b21)|(b22&b21)|(b22&b12) )
	    { /* case; right angle ends. clean up.
                 e.g.; X X O  CAN  become X X O
                       O X O              O O O
                       O O O              O O O        */
              if ( ((b01)&(mid[(i-2)*x_size+j-1]>7)&(mid[(i-2)*x_size+j]>7)&(mid[(i-2)*x_size+j+1]>7)&
                                    ((b00&((2*r[(i-1)*x_size+j+1])>centre))|(b02&((2*r[(i-1)*x_size+j-1])>centre)))) |
                   ((b10)&(mid[(i-1)*x_size+j-2]>7)&(mid[(i)*x_size+j-2]>7)&(mid[(i+1)*x_size+j-2]>7)&
                                    ((b00&((2*r[(i+1)*x_size+j-1])>centre))|(b20&((2*r[(i-1)*x_size+j-1])>centre)))) |
                   ((b12)&(mid[(i-1)*x_size+j+2]>7)&(mid[(i)*x_size+j+2]>7)&(mid[(i+1)*x_size+j+2]>7)&
                                    ((b02&((2*r[(i+1)*x_size+j+1])>centre))|(b22&((2*r[(i-1)*x_size+j+1])>centre)))) |
                   ((b21)&(mid[(i+2)*x_size+j-1]>7)&(mid[(i+2)*x_size+j]>7)&(mid[(i+2)*x_size+j+1]>7)&
                                    ((b20&((2*r[(i+1)*x_size+j+1])>centre))|(b22&((2*r[(i+1)*x_size+j-1])>centre)))) )
	      {
                mid[(i)*x_size+j]=100;
                if (b10&b20) j-=2;
                if (b00|b01|b02) { i--; j-=2; }
  	      }
	    }
#endif

/* }}} */
            if ( ((b01+b12+b21+b10)==2) && ((b10|b12)&(b01|b21)) &&
                 ((b01&((mid[(i-2)*x_size+j-1]<8)|(mid[(i-2)*x_size+j+1]<8)))|(b10&((mid[(i-1)*x_size+j-2]<8)|(mid[(i+1)*x_size+j-2]<8)))|
                (b12&((mid[(i-1)*x_size+j+2]<8)|(mid[(i+1)*x_size+j+2]<8)))|(b21&((mid[(i+2)*x_size+j-1]<8)|(mid[(i+2)*x_size+j+1]<8)))) )
	    { /* case; clears odd right angles.
                 e.g.; O O O  becomes O O O
                       X X O          X O O
                       O X O          O X O     */
              mid[(i)*x_size+j]=100;
              i--;               /* jump back */
              j-=2;
              if (i<4) i=4;
              if (j<4) j=4;
	    }
	  }
	}

/* }}} */
        /* {{{ n>2 the thinning is done here without breaking connectivity */

        if (n>2)
        {
          b01 = mid[(i-1)*x_size+j  ]<8;
          b12 = mid[(i  )*x_size+j+1]<8;
          b21 = mid[(i+1)*x_size+j  ]<8;
          b10 = mid[(i  )*x_size+j-1]<8;
          if((b01+b12+b21+b10)>1)
          {
            b00 = mid[(i-1)*x_size+j-1]<8;
            b02 = mid[(i-1)*x_size+j+1]<8;
	    b20 = mid[(i+1)*x_size+j-1]<8;
	    b22 = mid[(i+1)*x_size+j+1]<8;
            p1 = b00 | b01;
            p2 = b02 | b12;
            p3 = b22 | b21;
            p4 = b20 | b10;

            if( ((p1 + p2 + p3 + p4) - ((b01 & p2)+(b12 & p3)+(b21 & p4)+(b10 & p1))) < 2)
            {
              mid[(i)*x_size+j]=100;
              i--;
              j-=2;
              if (i<4) i=4;
              if (j<4) j=4;
            }
          }
        }

/* }}} */
      }
}

/* }}} */
/* {{{ susan_edges(in,r,sf,max_no,out) */

susan_edges(in,r,mid,bp,max_no,x_size,y_size)
  uchar *in, *bp, *mid;
  int   *r, max_no, x_size, y_size;
{
float z;
int   do_symmetry, i, j, m, n, a, b, x, y, w;
uchar c,*p,*cp;

  memset (r,0,x_size * y_size * sizeof(int));

  for (i=3;i<y_size-3;i++)
    for (j=3;j<x_size-3;j++)
    {
      n=100;
      p=in + (i-3)*x_size + j - 1;
      cp=bp + in[i*x_size+j];

      n+=*(cp-*p++);
      n+=*(cp-*p++);
      n+=*(cp-*p);
      p+=x_size-3; 

      n+=*(cp-*p++);
      n+=*(cp-*p++);
      n+=*(cp-*p++);
      n+=*(cp-*p++);
      n+=*(cp-*p);
      p+=x_size-5;

      n+=*(cp-*p++);
      n+=*(cp-*p++);
      n+=*(cp-*p++);
      n+=*(cp-*p++);
      n+=*(cp-*p++);
      n+=*(cp-*p++);
      n+=*(cp-*p);
      p+=x_size-6;

      n+=*(cp-*p++);
      n+=*(cp-*p++);
      n+=*(cp-*p);
      p+=2;
      n+=*(cp-*p++);
      n+=*(cp-*p++);
      n+=*(cp-*p);
      p+=x_size-6;

      n+=*(cp-*p++);
      n+=*(cp-*p++);
      n+=*(cp-*p++);
      n+=*(cp-*p++);
      n+=*(cp-*p++);
      n+=*(cp-*p++);
      n+=*(cp-*p);
      p+=x_size-5;

      n+=*(cp-*p++);
      n+=*(cp-*p++);
      n+=*(cp-*p++);
      n+=*(cp-*p++);
      n+=*(cp-*p);
      p+=x_size-3;

      n+=*(cp-*p++);
      n+=*(cp-*p++);
      n+=*(cp-*p);

      if (n<=max_no)
        r[i*x_size+j] = max_no - n;
    }

  for (i=4;i<y_size-4;i++)
    for (j=4;j<x_size-4;j++)
    {
      if (r[i*x_size+j]>0)
      {
        m=r[i*x_size+j];
        n=max_no - m;
        cp=bp + in[i*x_size+j];

        if (n>600)
        {
          p=in + (i-3)*x_size + j - 1;
          x=0;y=0;

          c=*(cp-*p++);x-=c;y-=3*c;
          c=*(cp-*p++);y-=3*c;
          c=*(cp-*p);x+=c;y-=3*c;
          p+=x_size-3; 
    
          c=*(cp-*p++);x-=2*c;y-=2*c;
          c=*(cp-*p++);x-=c;y-=2*c;
          c=*(cp-*p++);y-=2*c;
          c=*(cp-*p++);x+=c;y-=2*c;
          c=*(cp-*p);x+=2*c;y-=2*c;
          p+=x_size-5;
    
          c=*(cp-*p++);x-=3*c;y-=c;
          c=*(cp-*p++);x-=2*c;y-=c;
          c=*(cp-*p++);x-=c;y-=c;
          c=*(cp-*p++);y-=c;
          c=*(cp-*p++);x+=c;y-=c;
          c=*(cp-*p++);x+=2*c;y-=c;
          c=*(cp-*p);x+=3*c;y-=c;
          p+=x_size-6;

          c=*(cp-*p++);x-=3*c;
          c=*(cp-*p++);x-=2*c;
          c=*(cp-*p);x-=c;
          p+=2;
          c=*(cp-*p++);x+=c;
          c=*(cp-*p++);x+=2*c;
          c=*(cp-*p);x+=3*c;
          p+=x_size-6;
    
          c=*(cp-*p++);x-=3*c;y+=c;
          c=*(cp-*p++);x-=2*c;y+=c;
          c=*(cp-*p++);x-=c;y+=c;
          c=*(cp-*p++);y+=c;
          c=*(cp-*p++);x+=c;y+=c;
          c=*(cp-*p++);x+=2*c;y+=c;
          c=*(cp-*p);x+=3*c;y+=c;
          p+=x_size-5;

          c=*(cp-*p++);x-=2*c;y+=2*c;
          c=*(cp-*p++);x-=c;y+=2*c;
          c=*(cp-*p++);y+=2*c;
          c=*(cp-*p++);x+=c;y+=2*c;
          c=*(cp-*p);x+=2*c;y+=2*c;
          p+=x_size-3;

          c=*(cp-*p++);x-=c;y+=3*c;
          c=*(cp-*p++);y+=3*c;
          c=*(cp-*p);x+=c;y+=3*c;

          z = sqrt((float)((x*x) + (y*y)));
          if (z > (0.9*(float)n)) /* 0.5 */
	  {
            do_symmetry=0;
            if (x==0)
              z=1000000.0;
            else
              z=((float)y) / ((float)x);
            if (z < 0) { z=-z; w=-1; }
            else w=1;
            if (z < 0.5) { /* vert_edge */ a=0; b=1; }
            else { if (z > 2.0) { /* hor_edge */ a=1; b=0; }
            else { /* diag_edge */ if (w>0) { a=1; b=1; }
                                   else { a=-1; b=1; }}}
            if ( (m > r[(i+a)*x_size+j+b]) && (m >= r[(i-a)*x_size+j-b]) &&
                 (m > r[(i+(2*a))*x_size+j+(2*b)]) && (m >= r[(i-(2*a))*x_size+j-(2*b)]) )
              mid[i*x_size+j] = 1;
          }
          else
            do_symmetry=1;
        }
        else 
          do_symmetry=1;

        if (do_symmetry==1)
	{ 
          p=in + (i-3)*x_size + j - 1;
          x=0; y=0; w=0;

          /*   |      \
               y  -x-  w
               |        \   */

          c=*(cp-*p++);x+=c;y+=9*c;w+=3*c;
          c=*(cp-*p++);y+=9*c;
          c=*(cp-*p);x+=c;y+=9*c;w-=3*c;
          p+=x_size-3; 
  
          c=*(cp-*p++);x+=4*c;y+=4*c;w+=4*c;
          c=*(cp-*p++);x+=c;y+=4*c;w+=2*c;
          c=*(cp-*p++);y+=4*c;
          c=*(cp-*p++);x+=c;y+=4*c;w-=2*c;
          c=*(cp-*p);x+=4*c;y+=4*c;w-=4*c;
          p+=x_size-5;
    
          c=*(cp-*p++);x+=9*c;y+=c;w+=3*c;
          c=*(cp-*p++);x+=4*c;y+=c;w+=2*c;
          c=*(cp-*p++);x+=c;y+=c;w+=c;
          c=*(cp-*p++);y+=c;
          c=*(cp-*p++);x+=c;y+=c;w-=c;
          c=*(cp-*p++);x+=4*c;y+=c;w-=2*c;
          c=*(cp-*p);x+=9*c;y+=c;w-=3*c;
          p+=x_size-6;

          c=*(cp-*p++);x+=9*c;
          c=*(cp-*p++);x+=4*c;
          c=*(cp-*p);x+=c;
          p+=2;
          c=*(cp-*p++);x+=c;
          c=*(cp-*p++);x+=4*c;
          c=*(cp-*p);x+=9*c;
          p+=x_size-6;
    
          c=*(cp-*p++);x+=9*c;y+=c;w-=3*c;
          c=*(cp-*p++);x+=4*c;y+=c;w-=2*c;
          c=*(cp-*p++);x+=c;y+=c;w-=c;
          c=*(cp-*p++);y+=c;
          c=*(cp-*p++);x+=c;y+=c;w+=c;
          c=*(cp-*p++);x+=4*c;y+=c;w+=2*c;
          c=*(cp-*p);x+=9*c;y+=c;w+=3*c;
          p+=x_size-5;
 
          c=*(cp-*p++);x+=4*c;y+=4*c;w-=4*c;
          c=*(cp-*p++);x+=c;y+=4*c;w-=2*c;
          c=*(cp-*p++);y+=4*c;
          c=*(cp-*p++);x+=c;y+=4*c;w+=2*c;
          c=*(cp-*p);x+=4*c;y+=4*c;w+=4*c;
          p+=x_size-3;

          c=*(cp-*p++);x+=c;y+=9*c;w-=3*c;
          c=*(cp-*p++);y+=9*c;
          c=*(cp-*p);x+=c;y+=9*c;w+=3*c;

          if (y==0)
            z = 1000000.0;
          else
            z = ((float)x) / ((float)y);
          if (z < 0.5) { /* vertical */ a=0; b=1; }
          else { if (z > 2.0) { /* horizontal */ a=1; b=0; }
          else { /* diagonal */ if (w>0) { a=-1; b=1; }
                                else { a=1; b=1; }}}
          if ( (m > r[(i+a)*x_size+j+b]) && (m >= r[(i-a)*x_size+j-b]) &&
               (m > r[(i+(2*a))*x_size+j+(2*b)]) && (m >= r[(i-(2*a))*x_size+j-(2*b)]) )
            mid[i*x_size+j] = 2;	
        }
      }
    }
}

/* }}} */
/* {{{ susan_edges_small(in,r,sf,max_no,out) */

susan_edges_small(in,r,mid,bp,max_no,x_size,y_size)
  uchar *in, *bp, *mid;
  int   *r, max_no, x_size, y_size;
{
float z;
int   do_symmetry, i, j, m, n, a, b, x, y, w;
uchar c,*p,*cp;

  memset (r,0,x_size * y_size * sizeof(int));

  max_no = 730; /* ho hum ;) */

  for (i=1;i<y_size-1;i++)
    for (j=1;j<x_size-1;j++)
    {
      n=100;
      p=in + (i-1)*x_size + j - 1;
      cp=bp + in[i*x_size+j];

      n+=*(cp-*p++);
      n+=*(cp-*p++);
      n+=*(cp-*p);
      p+=x_size-2; 

      n+=*(cp-*p);
      p+=2;
      n+=*(cp-*p);
      p+=x_size-2;

      n+=*(cp-*p++);
      n+=*(cp-*p++);
      n+=*(cp-*p);

      if (n<=max_no)
        r[i*x_size+j] = max_no - n;
    }

  for (i=2;i<y_size-2;i++)
    for (j=2;j<x_size-2;j++)
    {
      if (r[i*x_size+j]>0)
      {
        m=r[i*x_size+j];
        n=max_no - m;
        cp=bp + in[i*x_size+j];

        if (n>250)
	{
          p=in + (i-1)*x_size + j - 1;
          x=0;y=0;

          c=*(cp-*p++);x-=c;y-=c;
          c=*(cp-*p++);y-=c;
          c=*(cp-*p);x+=c;y-=c;
          p+=x_size-2; 

          c=*(cp-*p);x-=c;
          p+=2;
          c=*(cp-*p);x+=c;
          p+=x_size-2;

          c=*(cp-*p++);x-=c;y+=c;
          c=*(cp-*p++);y+=c;
          c=*(cp-*p);x+=c;y+=c;

          z = sqrt((float)((x*x) + (y*y)));
          if (z > (0.4*(float)n)) /* 0.6 */
          {
            do_symmetry=0;
            if (x==0)
	      z=1000000.0;
	    else
	      z=((float)y) / ((float)x);
	    if (z < 0) { z=-z; w=-1; }
            else w=1;
            if (z < 0.5) { /* vert_edge */ a=0; b=1; }
            else { if (z > 2.0) { /* hor_edge */ a=1; b=0; }
            else { /* diag_edge */ if (w>0) { a=1; b=1; }
                                   else { a=-1; b=1; }}}
            if ( (m > r[(i+a)*x_size+j+b]) && (m >= r[(i-a)*x_size+j-b]) )
              mid[i*x_size+j] = 1;
          }
          else
            do_symmetry=1;
        }
        else
          do_symmetry=1;

        if (do_symmetry==1)
	{ 
          p=in + (i-1)*x_size + j - 1;
          x=0; y=0; w=0;

          /*   |      \
               y  -x-  w
               |        \   */

          c=*(cp-*p++);x+=c;y+=c;w+=c;
          c=*(cp-*p++);y+=c;
          c=*(cp-*p);x+=c;y+=c;w-=c;
          p+=x_size-2; 

          c=*(cp-*p);x+=c;
          p+=2;
          c=*(cp-*p);x+=c;
          p+=x_size-2;

          c=*(cp-*p++);x+=c;y+=c;w-=c;
          c=*(cp-*p++);y+=c;
          c=*(cp-*p);x+=c;y+=c;w+=c;

          if (y==0)
            z = 1000000.0;
          else
            z = ((float)x) / ((float)y);
          if (z < 0.5) { /* vertical */ a=0; b=1; }
          else { if (z > 2.0) { /* horizontal */ a=1; b=0; }
          else { /* diagonal */ if (w>0) { a=-1; b=1; }
                                else { a=1; b=1; }}}
          if ( (m > r[(i+a)*x_size+j+b]) && (m >= r[(i-a)*x_size+j-b]) )
            mid[i*x_size+j] = 2;	
        }
      }
    }
}

/* }}} */

/* }}} */
/* {{{ corners */

/* {{{ corner_draw(in,corner_list,drawing_mode) */

corner_draw(in,corner_list,x_size,drawing_mode)
  uchar *in;
  CORNER_LIST corner_list;
  int x_size, drawing_mode;
{
uchar *p;
int   n=0;

  while(corner_list[n].info != 7)
  {
    if (drawing_mode==0)
    {
      p = in + (corner_list[n].y-1)*x_size + corner_list[n].x - 1;
      *p++=255; *p++=255; *p=255; p+=x_size-2;
      *p++=255; *p++=0;   *p=255; p+=x_size-2;
      *p++=255; *p++=255; *p=255;
      n++;
    }
    else
    {
      p = in + corner_list[n].y*x_size + corner_list[n].x;
      *p=0;
      n++;
    }
  }
}

/* }}} */
/* {{{ susan(in,r,sf,max_no,corner_list) */

susan_corners(in,r,bp,max_no,corner_list,x_size,y_size)
  uchar       *in, *bp;
  int         *r, max_no, x_size, y_size;
  CORNER_LIST corner_list;
{
int   n,x,y,sq,xx,yy,
      i,j,*cgx,*cgy;
float divide;
uchar c,*p,*cp;

  memset (r,0,x_size * y_size * sizeof(int));

  cgx=(int *)malloc(x_size*y_size*sizeof(int));
  cgy=(int *)malloc(x_size*y_size*sizeof(int));

  for (i=5;i<y_size-5;i++)
    for (j=5;j<x_size-5;j++) {
        n=100;
        p=in + (i-3)*x_size + j - 1;
        cp=bp + in[i*x_size+j];

        n+=*(cp-*p++);
        n+=*(cp-*p++);
        n+=*(cp-*p);
        p+=x_size-3; 

        n+=*(cp-*p++);
        n+=*(cp-*p++);
        n+=*(cp-*p++);
        n+=*(cp-*p++);
        n+=*(cp-*p);
        p+=x_size-5;

        n+=*(cp-*p++);
        n+=*(cp-*p++);
        n+=*(cp-*p++);
        n+=*(cp-*p++);
        n+=*(cp-*p++);
        n+=*(cp-*p++);
        n+=*(cp-*p);
        p+=x_size-6;

        n+=*(cp-*p++);
        n+=*(cp-*p++);
        n+=*(cp-*p);
      if (n<max_no){    /* do this test early and often ONLY to save wasted computation */
        p+=2;
        n+=*(cp-*p++);
      if (n<max_no){
        n+=*(cp-*p++);
      if (n<max_no){
        n+=*(cp-*p);
      if (n<max_no){
        p+=x_size-6;

        n+=*(cp-*p++);
      if (n<max_no){
        n+=*(cp-*p++);
      if (n<max_no){
        n+=*(cp-*p++);
      if (n<max_no){
        n+=*(cp-*p++);
      if (n<max_no){
        n+=*(cp-*p++);
      if (n<max_no){
        n+=*(cp-*p++);
      if (n<max_no){
        n+=*(cp-*p);
      if (n<max_no){
        p+=x_size-5;

        n+=*(cp-*p++);
      if (n<max_no){
        n+=*(cp-*p++);
      if (n<max_no){
        n+=*(cp-*p++);
      if (n<max_no){
        n+=*(cp-*p++);
      if (n<max_no){
        n+=*(cp-*p);
      if (n<max_no){
        p+=x_size-3;

        n+=*(cp-*p++);
      if (n<max_no){
        n+=*(cp-*p++);
      if (n<max_no){
        n+=*(cp-*p);

        if (n<max_no)
        {
            x=0;y=0;
            p=in + (i-3)*x_size + j - 1;

            c=*(cp-*p++);x-=c;y-=3*c;
            c=*(cp-*p++);y-=3*c;
            c=*(cp-*p);x+=c;y-=3*c;
            p+=x_size-3; 
    
            c=*(cp-*p++);x-=2*c;y-=2*c;
            c=*(cp-*p++);x-=c;y-=2*c;
            c=*(cp-*p++);y-=2*c;
            c=*(cp-*p++);x+=c;y-=2*c;
            c=*(cp-*p);x+=2*c;y-=2*c;
            p+=x_size-5;
    
            c=*(cp-*p++);x-=3*c;y-=c;
            c=*(cp-*p++);x-=2*c;y-=c;
            c=*(cp-*p++);x-=c;y-=c;
            c=*(cp-*p++);y-=c;
            c=*(cp-*p++);x+=c;y-=c;
            c=*(cp-*p++);x+=2*c;y-=c;
            c=*(cp-*p);x+=3*c;y-=c;
            p+=x_size-6;

            c=*(cp-*p++);x-=3*c;
            c=*(cp-*p++);x-=2*c;
            c=*(cp-*p);x-=c;
            p+=2;
            c=*(cp-*p++);x+=c;
            c=*(cp-*p++);x+=2*c;
            c=*(cp-*p);x+=3*c;
            p+=x_size-6;
    
            c=*(cp-*p++);x-=3*c;y+=c;
            c=*(cp-*p++);x-=2*c;y+=c;
            c=*(cp-*p++);x-=c;y+=c;
            c=*(cp-*p++);y+=c;
            c=*(cp-*p++);x+=c;y+=c;
            c=*(cp-*p++);x+=2*c;y+=c;
            c=*(cp-*p);x+=3*c;y+=c;
            p+=x_size-5;

            c=*(cp-*p++);x-=2*c;y+=2*c;
            c=*(cp-*p++);x-=c;y+=2*c;
            c=*(cp-*p++);y+=2*c;
            c=*(cp-*p++);x+=c;y+=2*c;
            c=*(cp-*p);x+=2*c;y+=2*c;
            p+=x_size-3;

            c=*(cp-*p++);x-=c;y+=3*c;
            c=*(cp-*p++);y+=3*c;
            c=*(cp-*p);x+=c;y+=3*c;

            xx=x*x;
            yy=y*y;
            sq=xx+yy;
            if ( sq > ((n*n)/2) )
            {
              if(yy<xx) {
                divide=(float)y/(float)abs(x);
                sq=abs(x)/x;
                sq=*(cp-in[(i+FTOI(divide))*x_size+j+sq]) +
                   *(cp-in[(i+FTOI(2*divide))*x_size+j+2*sq]) +
                   *(cp-in[(i+FTOI(3*divide))*x_size+j+3*sq]);}
              else {
                divide=(float)x/(float)abs(y);
                sq=abs(y)/y;
                sq=*(cp-in[(i+sq)*x_size+j+FTOI(divide)]) +
                   *(cp-in[(i+2*sq)*x_size+j+FTOI(2*divide)]) +
                   *(cp-in[(i+3*sq)*x_size+j+FTOI(3*divide)]);}

              if(sq>290){
                r[i*x_size+j] = max_no-n;
                cgx[i*x_size+j] = (51*x)/n;
                cgy[i*x_size+j] = (51*y)/n;}
            }
	}
}}}}}}}}}}}}}}}}}}}

  /* to locate the local maxima */
  n=0;
  for (i=5;i<y_size-5;i++)
    for (j=5;j<x_size-5;j++) {
       x = r[i*x_size+j];
       if (x>0)  {
          /* 5x5 mask */
#ifdef FIVE_SUPP
          if (
              (x>r[(i-1)*x_size+j+2]) &&
              (x>r[(i  )*x_size+j+1]) &&
              (x>r[(i  )*x_size+j+2]) &&
              (x>r[(i+1)*x_size+j-1]) &&
              (x>r[(i+1)*x_size+j  ]) &&
              (x>r[(i+1)*x_size+j+1]) &&
              (x>r[(i+1)*x_size+j+2]) &&
              (x>r[(i+2)*x_size+j-2]) &&
              (x>r[(i+2)*x_size+j-1]) &&
              (x>r[(i+2)*x_size+j  ]) &&
              (x>r[(i+2)*x_size+j+1]) &&
              (x>r[(i+2)*x_size+j+2]) &&
              (x>=r[(i-2)*x_size+j-2]) &&
              (x>=r[(i-2)*x_size+j-1]) &&
              (x>=r[(i-2)*x_size+j  ]) &&
              (x>=r[(i-2)*x_size+j+1]) &&
              (x>=r[(i-2)*x_size+j+2]) &&
              (x>=r[(i-1)*x_size+j-2]) &&
              (x>=r[(i-1)*x_size+j-1]) &&
	      (x>=r[(i-1)*x_size+j  ]) &&
	      (x>=r[(i-1)*x_size+j+1]) &&
	      (x>=r[(i  )*x_size+j-2]) &&
	      (x>=r[(i  )*x_size+j-1]) &&
	      (x>=r[(i+1)*x_size+j-2]) )
#endif
#ifdef SEVEN_SUPP
          if ( 
                (x>r[(i-3)*x_size+j-3]) &&
                (x>r[(i-3)*x_size+j-2]) &&
                (x>r[(i-3)*x_size+j-1]) &&
                (x>r[(i-3)*x_size+j  ]) &&
                (x>r[(i-3)*x_size+j+1]) &&
                (x>r[(i-3)*x_size+j+2]) &&
                (x>r[(i-3)*x_size+j+3]) &&

                (x>r[(i-2)*x_size+j-3]) &&
                (x>r[(i-2)*x_size+j-2]) &&
                (x>r[(i-2)*x_size+j-1]) &&
                (x>r[(i-2)*x_size+j  ]) &&
                (x>r[(i-2)*x_size+j+1]) &&
                (x>r[(i-2)*x_size+j+2]) &&
                (x>r[(i-2)*x_size+j+3]) &&

                (x>r[(i-1)*x_size+j-3]) &&
                (x>r[(i-1)*x_size+j-2]) &&
                (x>r[(i-1)*x_size+j-1]) &&
                (x>r[(i-1)*x_size+j  ]) &&
                (x>r[(i-1)*x_size+j+1]) &&
                (x>r[(i-1)*x_size+j+2]) &&
                (x>r[(i-1)*x_size+j+3]) &&

                (x>r[(i)*x_size+j-3]) &&
                (x>r[(i)*x_size+j-2]) &&
                (x>r[(i)*x_size+j-1]) &&
                (x>=r[(i)*x_size+j+1]) &&
                (x>=r[(i)*x_size+j+2]) &&
                (x>=r[(i)*x_size+j+3]) &&

                (x>=r[(i+1)*x_size+j-3]) &&
                (x>=r[(i+1)*x_size+j-2]) &&
                (x>=r[(i+1)*x_size+j-1]) &&
                (x>=r[(i+1)*x_size+j  ]) &&
                (x>=r[(i+1)*x_size+j+1]) &&
                (x>=r[(i+1)*x_size+j+2]) &&
                (x>=r[(i+1)*x_size+j+3]) &&

                (x>=r[(i+2)*x_size+j-3]) &&
                (x>=r[(i+2)*x_size+j-2]) &&
                (x>=r[(i+2)*x_size+j-1]) &&
                (x>=r[(i+2)*x_size+j  ]) &&
                (x>=r[(i+2)*x_size+j+1]) &&
                (x>=r[(i+2)*x_size+j+2]) &&
                (x>=r[(i+2)*x_size+j+3]) &&

                (x>=r[(i+3)*x_size+j-3]) &&
                (x>=r[(i+3)*x_size+j-2]) &&
                (x>=r[(i+3)*x_size+j-1]) &&
                (x>=r[(i+3)*x_size+j  ]) &&
                (x>=r[(i+3)*x_size+j+1]) &&
                (x>=r[(i+3)*x_size+j+2]) &&
                (x>=r[(i+3)*x_size+j+3]) )
#endif
{
corner_list[n].info=0;
corner_list[n].x=j;
corner_list[n].y=i;
corner_list[n].dx=cgx[i*x_size+j];
corner_list[n].dy=cgy[i*x_size+j];
corner_list[n].I=in[i*x_size+j];
n++;
if(n==MAX_CORNERS){
      fprintf(stderr,"Too many corners.\n");
      exit(1);
         }}}}
corner_list[n].info=7;

free(cgx);
free(cgy);

}

/* }}} */
/* {{{ susan_quick(in,r,sf,max_no,corner_list) */

susan_corners_quick(in,r,bp,max_no,corner_list,x_size,y_size)
  uchar       *in, *bp;
  int         *r, max_no, x_size, y_size;
  CORNER_LIST corner_list;
{
int   n,x,y,i,j;
uchar *p,*cp;

  memset (r,0,x_size * y_size * sizeof(int));

  for (i=7;i<y_size-7;i++)
    for (j=7;j<x_size-7;j++) {
        n=100;
        p=in + (i-3)*x_size + j - 1;
        cp=bp + in[i*x_size+j];

        n+=*(cp-*p++);
        n+=*(cp-*p++);
        n+=*(cp-*p);
        p+=x_size-3;

        n+=*(cp-*p++);
        n+=*(cp-*p++);
        n+=*(cp-*p++);
        n+=*(cp-*p++);
        n+=*(cp-*p);
        p+=x_size-5;

        n+=*(cp-*p++);
        n+=*(cp-*p++);
        n+=*(cp-*p++);
        n+=*(cp-*p++);
        n+=*(cp-*p++);
        n+=*(cp-*p++);
        n+=*(cp-*p);
        p+=x_size-6;

        n+=*(cp-*p++);
        n+=*(cp-*p++);
        n+=*(cp-*p);
      if (n<max_no){
        p+=2;
        n+=*(cp-*p++);
      if (n<max_no){
        n+=*(cp-*p++);
      if (n<max_no){
        n+=*(cp-*p);
      if (n<max_no){
        p+=x_size-6;

        n+=*(cp-*p++);
      if (n<max_no){
        n+=*(cp-*p++);
      if (n<max_no){
        n+=*(cp-*p++);
      if (n<max_no){
        n+=*(cp-*p++);
      if (n<max_no){
        n+=*(cp-*p++);
      if (n<max_no){
        n+=*(cp-*p++);
      if (n<max_no){
        n+=*(cp-*p);
      if (n<max_no){
        p+=x_size-5;

        n+=*(cp-*p++);
      if (n<max_no){
        n+=*(cp-*p++);
      if (n<max_no){
        n+=*(cp-*p++);
      if (n<max_no){
        n+=*(cp-*p++);
      if (n<max_no){
        n+=*(cp-*p);
      if (n<max_no){
        p+=x_size-3;

        n+=*(cp-*p++);
      if (n<max_no){
        n+=*(cp-*p++);
      if (n<max_no){
        n+=*(cp-*p);

        if (n<max_no)
          r[i*x_size+j] = max_no-n;
}}}}}}}}}}}}}}}}}}}

  /* to locate the local maxima */
  n=0;
  for (i=7;i<y_size-7;i++)
    for (j=7;j<x_size-7;j++) {
       x = r[i*x_size+j];
       if (x>0)  {
          /* 5x5 mask */
#ifdef FIVE_SUPP
          if (
              (x>r[(i-1)*x_size+j+2]) &&
              (x>r[(i  )*x_size+j+1]) &&
              (x>r[(i  )*x_size+j+2]) &&
              (x>r[(i+1)*x_size+j-1]) &&
              (x>r[(i+1)*x_size+j  ]) &&
              (x>r[(i+1)*x_size+j+1]) &&
              (x>r[(i+1)*x_size+j+2]) &&
              (x>r[(i+2)*x_size+j-2]) &&
              (x>r[(i+2)*x_size+j-1]) &&
              (x>r[(i+2)*x_size+j  ]) &&
              (x>r[(i+2)*x_size+j+1]) &&
              (x>r[(i+2)*x_size+j+2]) &&
              (x>=r[(i-2)*x_size+j-2]) &&
              (x>=r[(i-2)*x_size+j-1]) &&
              (x>=r[(i-2)*x_size+j  ]) &&
              (x>=r[(i-2)*x_size+j+1]) &&
              (x>=r[(i-2)*x_size+j+2]) &&
              (x>=r[(i-1)*x_size+j-2]) &&
              (x>=r[(i-1)*x_size+j-1]) &&
	      (x>=r[(i-1)*x_size+j  ]) &&
	      (x>=r[(i-1)*x_size+j+1]) &&
	      (x>=r[(i  )*x_size+j-2]) &&
	      (x>=r[(i  )*x_size+j-1]) &&
	      (x>=r[(i+1)*x_size+j-2]) )
#endif
#ifdef SEVEN_SUPP
          if ( 
                (x>r[(i-3)*x_size+j-3]) &&
                (x>r[(i-3)*x_size+j-2]) &&
                (x>r[(i-3)*x_size+j-1]) &&
                (x>r[(i-3)*x_size+j  ]) &&
                (x>r[(i-3)*x_size+j+1]) &&
                (x>r[(i-3)*x_size+j+2]) &&
                (x>r[(i-3)*x_size+j+3]) &&

                (x>r[(i-2)*x_size+j-3]) &&
                (x>r[(i-2)*x_size+j-2]) &&
                (x>r[(i-2)*x_size+j-1]) &&
                (x>r[(i-2)*x_size+j  ]) &&
                (x>r[(i-2)*x_size+j+1]) &&
                (x>r[(i-2)*x_size+j+2]) &&
                (x>r[(i-2)*x_size+j+3]) &&

                (x>r[(i-1)*x_size+j-3]) &&
                (x>r[(i-1)*x_size+j-2]) &&
                (x>r[(i-1)*x_size+j-1]) &&
                (x>r[(i-1)*x_size+j  ]) &&
                (x>r[(i-1)*x_size+j+1]) &&
                (x>r[(i-1)*x_size+j+2]) &&
                (x>r[(i-1)*x_size+j+3]) &&

                (x>r[(i)*x_size+j-3]) &&
                (x>r[(i)*x_size+j-2]) &&
                (x>r[(i)*x_size+j-1]) &&
                (x>=r[(i)*x_size+j+1]) &&
                (x>=r[(i)*x_size+j+2]) &&
                (x>=r[(i)*x_size+j+3]) &&

                (x>=r[(i+1)*x_size+j-3]) &&
                (x>=r[(i+1)*x_size+j-2]) &&
                (x>=r[(i+1)*x_size+j-1]) &&
                (x>=r[(i+1)*x_size+j  ]) &&
                (x>=r[(i+1)*x_size+j+1]) &&
                (x>=r[(i+1)*x_size+j+2]) &&
                (x>=r[(i+1)*x_size+j+3]) &&

                (x>=r[(i+2)*x_size+j-3]) &&
                (x>=r[(i+2)*x_size+j-2]) &&
                (x>=r[(i+2)*x_size+j-1]) &&
                (x>=r[(i+2)*x_size+j  ]) &&
                (x>=r[(i+2)*x_size+j+1]) &&
                (x>=r[(i+2)*x_size+j+2]) &&
                (x>=r[(i+2)*x_size+j+3]) &&

                (x>=r[(i+3)*x_size+j-3]) &&
                (x>=r[(i+3)*x_size+j-2]) &&
                (x>=r[(i+3)*x_size+j-1]) &&
                (x>=r[(i+3)*x_size+j  ]) &&
                (x>=r[(i+3)*x_size+j+1]) &&
                (x>=r[(i+3)*x_size+j+2]) &&
                (x>=r[(i+3)*x_size+j+3]) )
#endif
{
corner_list[n].info=0;
corner_list[n].x=j;
corner_list[n].y=i;
x = in[(i-2)*x_size+j-2] + in[(i-2)*x_size+j-1] + in[(i-2)*x_size+j] + in[(i-2)*x_size+j+1] + in[(i-2)*x_size+j+2] +
    in[(i-1)*x_size+j-2] + in[(i-1)*x_size+j-1] + in[(i-1)*x_size+j] + in[(i-1)*x_size+j+1] + in[(i-1)*x_size+j+2] +
    in[(i  )*x_size+j-2] + in[(i  )*x_size+j-1] + in[(i  )*x_size+j] + in[(i  )*x_size+j+1] + in[(i  )*x_size+j+2] +
    in[(i+1)*x_size+j-2] + in[(i+1)*x_size+j-1] + in[(i+1)*x_size+j] + in[(i+1)*x_size+j+1] + in[(i+1)*x_size+j+2] +
    in[(i+2)*x_size+j-2] + in[(i+2)*x_size+j-1] + in[(i+2)*x_size+j] + in[(i+2)*x_size+j+1] + in[(i+2)*x_size+j+2];

corner_list[n].I=x/25;
/*corner_list[n].I=in[i*x_size+j];*/
x = in[(i-2)*x_size+j+2] + in[(i-1)*x_size+j+2] + in[(i)*x_size+j+2] + in[(i+1)*x_size+j+2] + in[(i+2)*x_size+j+2] -
   (in[(i-2)*x_size+j-2] + in[(i-1)*x_size+j-2] + in[(i)*x_size+j-2] + in[(i+1)*x_size+j-2] + in[(i+2)*x_size+j-2]);
x += x + in[(i-2)*x_size+j+1] + in[(i-1)*x_size+j+1] + in[(i)*x_size+j+1] + in[(i+1)*x_size+j+1] + in[(i+2)*x_size+j+1] -
        (in[(i-2)*x_size+j-1] + in[(i-1)*x_size+j-1] + in[(i)*x_size+j-1] + in[(i+1)*x_size+j-1] + in[(i+2)*x_size+j-1]);

y = in[(i+2)*x_size+j-2] + in[(i+2)*x_size+j-1] + in[(i+2)*x_size+j] + in[(i+2)*x_size+j+1] + in[(i+2)*x_size+j+2] -
   (in[(i-2)*x_size+j-2] + in[(i-2)*x_size+j-1] + in[(i-2)*x_size+j] + in[(i-2)*x_size+j+1] + in[(i-2)*x_size+j+2]);
y += y + in[(i+1)*x_size+j-2] + in[(i+1)*x_size+j-1] + in[(i+1)*x_size+j] + in[(i+1)*x_size+j+1] + in[(i+1)*x_size+j+2] -
        (in[(i-1)*x_size+j-2] + in[(i-1)*x_size+j-1] + in[(i-1)*x_size+j] + in[(i-1)*x_size+j+1] + in[(i-1)*x_size+j+2]);
corner_list[n].dx=x/15;
corner_list[n].dy=y/15;
n++;
if(n==MAX_CORNERS){
      fprintf(stderr,"Too many corners.\n");
      exit(1);
         }}}}
corner_list[n].info=7;
}

/* }}} */

/* }}} */
/* {{{ main(argc, argv) */



void *rtRepCode(void *arg)
{
  struct repetitive *params = (struct repetitive *) arg;                                   	
  int index = params->index;	
  int priority = params -> priority;
  int cpu = params->proc;
  int itSize = params->itSize;
  cpu_set_t cpuset; 
  CPU_ZERO(&cpuset);     
  CPU_SET( cpu , &cpuset); 
  //pthread_setaffinity_np(pthread_self(), sizeof(cpuset), &cpuset);
  sched_setaffinity(0, sizeof(cpuset), &cpuset);
  int it=0;
  struct timespec b,e;

    
  FILE   *ofp;
  char   filename [80],
    *tcp;
  uchar  *in, *bp, *mid;
  float  dt=4.0;
  int    *r,
    argindex=3,
    bt=20,
    principle=0,
    thin_post_proc=1,
    three_by_three=0,
    drawing_mode=0,
    susan_quick=0,
    max_no_corners=1850,
    max_no_edges=2650,
    mode = 0, i,
    x_size, y_size;
  CORNER_LIST corner_list;
  get_image("input_large.pgm",&in,&x_size,&y_size);
  r   = (int *) malloc(x_size * y_size * sizeof(int));
  setup_brightness_lut(&bp,bt,6);

  for (it =0 ; it< itSize ; it++ ){
    clock_gettime(CLOCK_REALTIME, &b);
    //call my rt function here
    
    susan_corners(in,r,bp,max_no_corners,corner_list,x_size,y_size);
    //corner_draw(in,corner_list,x_size,drawing_mode);
    //put_image("out.pgm",in,x_size,y_size);
    
    clock_gettime(CLOCK_REALTIME, &e);
    printf("%s \t %lf \n",params->label,toSec(diff_spec(e,b)));//<<endl;
  }
  return NULL;
}


int main(int argc, char ** argv)
{
  if(argc < 9){
    printf("usage : [executableName] -p [priority] -i [NumberOfrepetinion]  -proc [processorID] -s [processingLabel] [applicationSpecificParameters] \n"); 
    exit(1);
  }

  struct repetitive rep;
  rep.index = 0;
  rep.priority=atoi(argv[2]);
  rep.proc=atoi(argv[6]);
  rep.itSize=atoi(argv[4]);
  rep.label=argv[8];
  pthread_t th;
  pthread_attr_t myattr;
  struct sched_param myparam;
  pthread_attr_init(&myattr);  
  pthread_attr_setschedpolicy(&myattr, SCHED_FIFO);
  myparam.sched_priority = rep.priority;
  pthread_attr_setschedparam(&myattr, &myparam);   
  pthread_create(&th, &myattr, rtRepCode, &rep); 
  if(pthread_join(th, NULL)) {
    printf("Error joining thread \n");//<<endl;
    return 2;
  }
  return (0);
}
