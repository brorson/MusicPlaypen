#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <signal.h>
#include <string.h>
#include <math.h>
#include "cblas.h"
#include <lapacke.h>
#include <unistd.h>
#include <sys/types.h>

#include "spidriver_host.h"
#include "adcdriver_host.h"
#include "matrix_utils.h"

#define PI 3.1415926535

// Length of data buffer
#define NUMPTS 128

// Number of signal vectors
#define PSIG 2

// Sampling frequency.  Must match the sampling frequency commanded
// to the A/D.
#define FSAMP 15625

// Parameters used in searching for peak
#define MAXRECURSIONS 5
#define NGRID 25

//===========================================================
// Utility functions for MUSIC

//-----------------------------------------------------
void find_bracket(int N, float *u, int *ileft, int *iright) {
  // Given input vector y, this finds the max element,
  // then returns the indices of the vectors to its
  // left and right.

  int i;

  i = maxeltf(N, u);
  if (i == 0) {
    *ileft = 0;
    *iright = 1;
  } else if (i == (N-1)) {
    *ileft = N-2;
    *iright = N-1;
  } else {
    *ileft = i-1;
    *iright = i+1;
  }
  return;
}


//-----------------------------------------------------
void extract_noise_vectors(float *A, int m, int n, int c, float *E) {
  // This fcn takes input matrix A of size mxn.  It extracts the vectors
  // of A to the right, starting at col c, and puts the extracted
  // vectors into E.  E has size [m, n-c]

  //printf("Entered extract_noise_vectors, m = %d, n = %d, c = %d\n", m, n, c);
  int i, j, l;
  for (i = 0; i < m; i++) {
    for (j = c; j < n; j++) {
      // printf("i = %d, j = %d, A[i,j] = %f\n", i, j, MATRIX_ELEMENT(A, m, n, i, j));
      l = lindex(m, n-c, i, j-c);
      // printf("lindex(m, n-c, i,j-c) = %d\n", l);
      E[l] = MATRIX_ELEMENT(A, m, n, i, j);
    }
  }
}


//-----------------------------------------------------
float music_sum(float f, float *v, int Mr, int Mc) {
  // This performs the sum over noise space vectors.
  // Since complex numbers are not supported, I split
  // the computation into two parts, real and imag.

  int i, j;

  float *er;
  float *ei;
  float *vi;
  float s, tr, ti;

  // printf("--> Entered music_sum, [row, col] = [%d, %d]\n", Mr, Mc);

  er = (float*) malloc(Mr*sizeof(float));
  ei = (float*) malloc(Mr*sizeof(float));
  vi = (float*) malloc(Mr*sizeof(float));

  // Create e vector
  for(i=0; i<Mr; i++) {
    er[i] = cos(2*PI*i*f);
    ei[i] = sin(2*PI*i*f);
  }

  // Compute denominator
  s = 0;
  for(i=0; i<Mc; i++) {     // iterate over columns.

    // v is a matrix, so extract noise vector vi as column vector.
    for(j=0; j<Mr; j++) {   // iterate over rows
      vi[j] = v[lindex(Mr, Mc, j, i)]; 
    }
    tr = cblas_sdot(Mr, er, 1, vi, 1);
    ti = cblas_sdot(Mr, ei, 1, vi, 1);
    s = s + tr*tr + ti*ti;
  }

  free(er);
  free(ei);
  free(vi);

  // Must guard against returning inf or nan.
  return (1.0f/s);
}


//-----------------------------------------------------
// Called when Ctrl+C is pressed - triggers the program to stop.
void stopHandler(int sig) {
  adc_quit();
  exit(0);
}


//==========================================================
// This is the main program.  It runs a loop, takes a buffer
// of data from the A/D, then uses the MUSIC algorithm to
// compute the frequency of the input sine wave.
int main (void)
{
  // Loop variables
  uint32_t i, j;

  // Buffers for tx and rx data from A/D registers.
  uint32_t tx_buf[3];
  uint32_t rx_buf[4];

  // Used in LAPACK computations
  int m = NUMPTS;
  int info;
  float superb[NUMPTS-1];

  // Measured voltages from A/D
  float v[NUMPTS];           // Vector of measurements 
  float Rxx[NUMPTS*NUMPTS];  // Covariance matrix.
  float U[NUMPTS * NUMPTS];
  float S[NUMPTS];
  float VT[NUMPTS * NUMPTS];

  float Nu[NUMPTS * (NUMPTS-PSIG)];  // Vector of noises

  // Used in finding peak corresponding to dominant frequency
  float f[NGRID];
  float Pmu[NGRID];
  int ileft, iright;
  float fleft, fright, fpeak;

  // Stuff used with "hit return when ready..." 
  char dummy[8];

  printf("------------   Starting main.....   -------------\n");

  // Run until Ctrl+C pressed:
  signal(SIGINT, stopHandler);

  // Sanity check user.
  if(getuid()!=0){
     printf("You must run this program as root. Exiting.\n");
     exit(EXIT_FAILURE);
  }

  // Initialize A/D converter
  adc_config();
  adc_set_samplerate(SAMP_RATE_15625);
  adc_set_chan0();

  // Now loop forever, read buffer, and compute frequency.
  // printf("--------------------------------------------------\n");
  while(1) {

    // Do A/D read to fill buffer with NUMPTS measurements.
    adc_read_multiple(NUMPTS, v);
    //printf("Values read = \n");
    //for (i=0; i<NUMPTS; i++) {
    //  printf("i = %d, v = %e\n", i, v[i]);
    //}
 
    // Zero out Rxx prior to filling it using sger
    zeros(m, m, Rxx);  

    // Create covariance matrix by doing outer product of v with
    // itself.
    cblas_sger(CblasRowMajor,    /* Row-major storage */
               m,                /* Row count for Rxx  */
               m,                /* Col count for Rxx  */
               1.0f,             /* scale factor to apply to v*v' */
               v,
               1,                /* stride between elements of v. */
               v,
               1,                /* stride between elements of v. */
               Rxx,
               m);               /* leading dimension of matrix Rxx. */
    //printf("\nMatrix Rxx (%d x %d) =\n", m, m);
    //print_matrix(Rxx, m, m);

    // Now compute SVD of Rxx.
    info = LAPACKE_sgesvd(LAPACK_ROW_MAJOR, 'A', 'A', 
                  m, m, Rxx, 
                  m, S, U, m, 
                  VT, m, superb);
    if (info != 0)  {
      fprintf(stderr, "Error: dgesvd returned with a non-zero status (info = %d)\n", info);
      return(-1);
    }

    //printf("\nMatrix U (%d x %d) is:\n", m, m);
    //print_matrix(U, m, m);

    //printf("\nVector S (%d x %d) is:\n", m, 1);
    //print_matrix(S, m, 1);

    //printf("\nMatrix VT (%d x %d) is:\n", m, m);
    //print_matrix(VT, m, m);

    // Extract noise vectors here.  The noise vectors are held in Nu
    extract_noise_vectors(U, m, m, PSIG, Nu); 
    //printf("\nMatrix Nu (%d x %d) is:\n", m, NUMPTS-PSIG);
    //print_matrix(Nu, m, NUMPTS-PSIG);

    // Now find max freq.  Set up initial grid endpoints.  Freqs are
    // in units of Hz.
    fleft = 0.0f;
    fright = FSAMP/2.0f;

    for (j=0; j<MAXRECURSIONS; j++) {
      // printf("fleft = %f, fright = %f\n", fleft, fright);

      // Set up search grid
      linspace(fleft, fright, NGRID, f);
      //printf("\nVector f =\n");
      //print_matrix(f, NGRID, 1);

      // Compute vector of amplitudes Pmu on grid.  music_sum wants normalized
      // frequencies 
      for (i = 0; i < NGRID; i++) {
        Pmu[i] = music_sum(f[i]/FSAMP, Nu, NUMPTS, NUMPTS-PSIG);
      }
      //printf("\nVector Pmu =\n");
      //print_matrix(Pmu, NGRID, 1);

      find_bracket(NGRID, Pmu, &ileft, &iright); 
      fleft = f[ileft];
      fright = f[iright];
    }
    fpeak = (fleft+fright)/2.0f;   // Assume peak is average of fleft and fright
    printf("Peak frequency found at f = %f Hz\n", fpeak);

    // usleep(500000);   // delay 1/2 sec.
  }

}

