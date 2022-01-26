#include "mbed.h"
#include <stdio.h>
 
SPI spi(PF_9, PF_8, PF_7); // mosi, miso, sclk
DigitalOut cs(PC_1); // chip select pin = PC_1
int16_t xbuffer[100], ybuffer[100], zbuffer[100];
// initialize the offset for x axis
int xoffset, xavg;
// initialize the flag to be 0
int sample = 0;

/** this function is for taking samples. It's reused twice in this program 
 * first time is for calculating the offset 
 * second time is for actual sampling of the gyroscope values to calculate the distance travelled. 
 * when sampling, we are only concerned with the x value because of the orientation of our gyroscope
 */
void sampling(int zeroRate) {
  int xcount = 0, counter = 0;

  while (counter < 100) {
    if (zeroRate == 1 || sample == 1) {
      cs = 1;
      int count = 0;

      for (int i = 0xA8; i <= 0xA9; i++)  {
        cs = 0;
        int firstByte;
        int16_t bytesVal;
        spi.write(i);
        int value = spi.write(0x00);
        count++;
  
        if (count == 2) { // case x
          bytesVal = (firstByte << 8) | value;
          xbuffer[xcount] = bytesVal;
          xcount ++;
        } 
        else {
          firstByte = value;
        }

        cs = 1;
      }
      sample = 0; // set flag back to 0
      counter ++;
    }
  }
}

// this is the helper function for calculating the offset using the initial 100 samples
void getOffset() {
  int xsum = 0;
  for (int i=0; i<100; i++) {
    xsum += xbuffer[i];
  }
  xoffset = xsum / 100;
  printf("Offset : %d\n", xoffset);
}

/** this is the helper function for cleaning up the data by looping through the raw data we collected,
 * and taking only the negative measurements and minus the offset value
 * and then we sum these negative measurements up, which we will then calculate the average angular velocity
*/
void dataCleanUp() {
  int xsum = 0, negativeX = 0;
  printf("x values:");
  for (int i=0; i<100; i++) {
    printf(" %d,", xbuffer[i]);
    int result = xbuffer[i] - xoffset;
    // taking only the positive measurements on the gyroscope
    if(result < 0) {
      xsum += abs(result);
      negativeX ++;
    }

  }
  xavg = xsum / negativeX;
}

/**
 * this is the function where we take the average angular velocity and calculate the linear velocity
 * and then we use the result to calculate the distance travelled
 */
void angularToLinear() {
  /** xavg * 8.75 * 0.001 is the dps of x axis, and then /360 is to convert it to rotation per second. 
  * 3.14 * 2 * 30 is the circumference with r as 30 cm, 
  * (r is the height at which the gyroscope is strapped with respect to the groud)
  */
  int xLinearV = (xavg * 8.75 * 0.001 / 360) * (3.14 * 2 * 30); 
  //   int xLinearV = (xavg * 8.75 * 0.001)/(2*3.14) *  30;

  // distance travelled is equal to the the linear velocity on the x axis times the 20 second period
  int distance = xLinearV * 20; 
  printf("X linear Velocity = %d\n", xLinearV);
  printf("distant travelled %d\n", distance);
}

// ticker will trigger this function every 0.2 seconds to set the flag for sampling
void setFlags() {
  sample = 1;
}

int main() {
    // Setup the spi for 8 bit data, high steady state clock,
    // second edge capture, with a 1MHz clock rate
    spi.format(8,3);
    spi.frequency(1000000);
    cs = 1;

    // initialize the ticker and call the set the flags to enable sampling every 0.2 seconds
    Ticker t;
    t.attach(&setFlags, 200000us);

    cs = 0;

    spi.write(0x20); // Writing to the Ctrl_Reg4 register address.
    spi.write(0xF); // Setting the value of Ctrl_reg4 as 0xF(Basically setting the PD bit as 1 which is 0 by default.)

    cs = 1;

    wait_us(100);

    // get 100 samples for zero rate offset calculation
    sampling(1);
    
    // get the offset when the board is at rest
    getOffset();

    // to signal the start of gyroscope sampling
    printf("GO!!!!!!!!\n");
    wait_us(1000000);

    // get 100 samples since we are sampling every 0.2 seconds, over a 20-second period
    sampling(0);

    // clean up the data after the sampling
    dataCleanUp();

    // calculate the linear velocity to get the distance travelled
    angularToLinear();
}