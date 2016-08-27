//
// Arduino OLED Fountains Demo
// see https://github.com/vwegert/arduino-oled-fountains/ for more information
//
// This is free and unencumbered software released into the public domain.
//
// Anyone is free to copy, modify, publish, use, compile, sell, or
// distribute this software, either in source code form or as a compiled
// binary, for any purpose, commercial or non-commercial, and by any
// means.
//
// In jurisdictions that recognize copyright laws, the author or authors
// of this software dedicate any and all copyright interest in the
// software to the public domain. We make this dedication for the benefit
// of the public at large and to the detriment of our heirs and
// successors. We intend this dedication to be an overt act of
// relinquishment in perpetuity of all present and future rights to this
// software under copyright law.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
// IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
// OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
// ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
// OTHER DEALINGS IN THE SOFTWARE.
//
// For more information, please refer to <http://unlicense.org>
//
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// The OLED I2C address
#define OLED_ADDRESS 0x3C

// The OLED Reset Pin assignment - -1 if you don't have one...
#define OLED_RESET -1

// How many fountains to display
#define NUM_FOUNTAINS 2

// --- If you change anything below this point, things might get unstable, so beware ---

#if (SSD1306_LCDHEIGHT != 64)
#error("Height incorrect, please fix Adafruit_SSD1306.h!");
#endif

// A single spark released by a fountain, represented by a single pixel.
struct spark {
  // The age of the spark - used to derive its position.
  unsigned int age;

  // The internal coordinates used to calculate the movement of the spark.
  // Note that these coordinates are relative to the starting point. The
  // Y component should range between 0 and 1.
  double xi, yi;

  // The velocity of the spark.
  double vx, vy;

  // The actual coordinates used to draw the pixel.
  int xd, yd;

  // The next spark in a null-terminated linked list.
  spark *next;
};

// The horizontal velocity range of a spark.
#define VX_MIN    -0.1
#define VX_MAX     0.1

// The vertical velocity range of a spark.
#define VY_MIN     0.3
#define VY_MAX     0.9

// Auxiliary routine to check whether the spark is currently inside
// the display field.
int isSparkVisible(spark *theSpark) {
  return (theSpark->xd >= 0) && (theSpark->xd < SSD1306_LCDWIDTH) && (theSpark->yd >= 0) && (theSpark->yd < SSD1306_LCDHEIGHT);
}

struct fountain {
  // The horizontal position of the fountain.
  unsigned int x;

  // The height of the fountain.
  unsigned int height;

  // The width of the fountain at its base; should be an even number.
  unsigned int width;

  // The activity of the fountain: How often a new spark should be spawned (255 = every
  // iteration, which would be way too much.
  unsigned int activity;

  // The activity change - positive values increase activity, negative values decrease it.
  unsigned int actdelta;

  // The linked list of sparks emitted from this fountain.
  spark *sparks;
};

// The height of the fountain cone in pixels.
#define F_HEIGHT 6

// The width of the fountain cone at its base.
#define F_WIDTH 4

// The maximum activity of the fountain (see above).
#define F_ACTIVITY_MAX 100

// A simplified factor for the influence of gravity.
#define Y_FACTOR  0.25

// This factor is used to scale down the time resolution.
#define TIME_FACTOR 10

// This factor is used to scale the floating-point coordinates to the screen coordinates.
#define DISPLAY_SCALE SSD1306_LCDHEIGHT

// The instance to access the display.
Adafruit_SSD1306 display(OLED_RESET);

// The list of fountains.
fountain fountains[NUM_FOUNTAINS];

void setup() {
  // initialize the RNG
  randomSeed(analogRead(0));

  // initialize and clean up the display
  display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS);
  display.clearDisplay();
  display.display();

  // initialize the fountains
  int horizontal_spacing = SSD1306_LCDWIDTH / (NUM_FOUNTAINS + 1);
  for (int i = 0; i < NUM_FOUNTAINS; i++) {
    fountains[i].x        = (i + 1) * horizontal_spacing;
    fountains[i].height   = F_HEIGHT;
    fountains[i].width    = F_WIDTH;
    fountains[i].activity = (i % 2) == 0 ? F_ACTIVITY_MAX : 0;
    fountains[i].sparks   = NULL;
  }
  delay(1000);
}


void loop() {
  for (int i = 0; i < NUM_FOUNTAINS; i++) {

    // determine randomly if a new spark is to be created
    if (random(255) < fountains[i].activity) {
      spark *newSpark = new spark;
      // caution - memory is very limited, so check whether this has actually worked:
      if (newSpark > 0) {
        newSpark->age      = 0;
        newSpark->xi       = 0.0;
        newSpark->yi       = 0.0;
        newSpark->vx       = VX_MIN + random(1000) * (VX_MAX - VX_MIN) / 1000;
        newSpark->vy       = VY_MIN + random(1000) * (VY_MAX - VY_MIN) / 1000;
        // for simplicity, the new spark is added to the beginning of the list
        newSpark->next = fountains[i].sparks;
        fountains[i].sparks = newSpark;
      }
    }

    // traverse the list of sparks - we need two pointers since we will be removing sparks in the process
    spark *prevSpark = NULL;
    spark *thisSpark = fountains[i].sparks;
    while (thisSpark != NULL) {

      // delete the spark from its previous position, unless it is brand new and has never been drawn before
      if ((thisSpark->age > 0) && (isSparkVisible(thisSpark)))
      {
        display.drawPixel(thisSpark->xd, thisSpark->yd, BLACK);
      }

      // calculate the new relative position
      thisSpark->xi = thisSpark->vx * (double)thisSpark->age / TIME_FACTOR;
      thisSpark->yi = thisSpark->vy * (double)thisSpark->age / TIME_FACTOR - Y_FACTOR * sq((double)thisSpark->age / TIME_FACTOR);

      // from that, calculate the new absolute position (in relation to the fountain tip)
      thisSpark->xd = fountains[i].x + round(thisSpark->xi * DISPLAY_SCALE);
      thisSpark->yd = SSD1306_LCDHEIGHT - (fountains[i].height + round(thisSpark->yi * DISPLAY_SCALE));

      // let the spark age for a bit
      thisSpark->age++;

      // if the spark has reached the ground level, remove it
      if (thisSpark->yd > SSD1306_LCDHEIGHT) {
        if (prevSpark == NULL) {
          fountains[i].sparks = thisSpark->next;
          delete thisSpark;
          thisSpark = fountains[i].sparks;
        } else {
          prevSpark->next = thisSpark->next;
          delete thisSpark;
          thisSpark = prevSpark->next;
        }
      } else {
        // draw the pixel (if the spark is on the screen at the moment!) and move on
        if (isSparkVisible(thisSpark)) {
          display.drawPixel(thisSpark->xd, thisSpark->yd, WHITE);
        }
        prevSpark = thisSpark;
        thisSpark = thisSpark->next;
      }
    }

    // adjust the activity of the fountain
    if (fountains[i].activity <= 0) {
      fountains[i].actdelta = 1;
    } else if (fountains[i].activity >= F_ACTIVITY_MAX) {
      fountains[i].actdelta = -1;
    }
    fountains[i].activity += fountains[i].actdelta;
  }

  // (Re-)Draw the fountain bases. This might seem superfluous, but it might happen that the base
  // gets "damaged" by sparks that fly through it.
  for (int i = 0; i < NUM_FOUNTAINS; i++) {
    display.fillTriangle(fountains[i].x,                          SSD1306_LCDHEIGHT - fountains[i].height - 1, // tip
                         fountains[i].x - fountains[i].width / 2, SSD1306_LCDHEIGHT - 1,                       // lower left
                         fountains[i].x + fountains[i].width / 2, SSD1306_LCDHEIGHT - 1,                       // lower right
                         WHITE);
  }

  display.display();
  delay(5);
}
