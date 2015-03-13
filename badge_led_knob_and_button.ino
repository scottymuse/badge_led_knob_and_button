/*

AUTHOR: Scott Nielsen (scotty)

Thanks to Klint Holmes. I used a lot of his original badge code here and to learn this stuff.
And Thanks to Luke Jenkins, Klint Holmes, Matt Lorimer, and Jonathan Karras for these badges!

To use:
Load this program onto the SaintCON 2014 badge.

The neat thing about this program is the leds are entirely controlled by timer interrupts
on the processor, leaving the main loop empty.

A known issue is the interrupts are controlled by a timer on the processor which when used
disables the PWM ability on some of the pins. Specifically in this program, the pins
controlling the top leds on the name plate are affected, removing their ability to fade
easily with analogWrite(). So I do not include the top leds in the fade patterns.
*/

int leds_ordered[4];

const int tl = leds_ordered[0] = 10;
const int tr = leds_ordered[1] = 9;;
const int br = leds_ordered[2] = 6;
const int bl = leds_ordered[3] = 11;

#define POTPIN A1
//#define HZSCALER 0.1466
#define HZSCALER 0.0733

volatile int led_state = 0; // an int between 0 and 15, 4 bits, each representing an led

#define ALL_STATES ((1 << 3) | (1 << 2) | (1 << 1) | (1 << 0))
#define TL (1 << 0)
#define TR (1 << 1)
#define BR (1 << 2)
#define BL (1 << 3)

// If you add another pattern, you have to change NUM_PATTERNS below
const int pattern_all_blink = 0;
const int pattern_rotate = 1;
const int pattern_cc_rotate = 2;
const int pattern_sync_fade = 3;
const int pattern_alt_fade = 4;
const int pattern_up_down = 5;
const int pattern_side_side = 6;
const int pattern_diagonals = 7;
const int pattern_random_toggles = 8;
const int pattern_all_off = 9;
const int pattern_all_on = 10;

const int NUM_PATTERNS = 11;

volatile int pattern = pattern_random_toggles;

boolean fade_initialized = false;
boolean random_toggles_initialized = false;

const long prescaler = 62500;
volatile float hz;

void enforce_state() {
  // Check the bits in led_state, any 1's turn the led on. any 0's turn the led off.
  for (int i = 0; i < 4; i++) {
    if (led_state & 1 << i)
      digitalWrite(leds_ordered[i], HIGH);
    else
      digitalWrite(leds_ordered[i], LOW);
  }
}

void all_on() {
  led_state = BR | BL | TL | TR;
  enforce_state();
}

void all_off() {
  led_state = 0;
  enforce_state();
}

void all_blink() {
  if (led_state == 0)
    all_on();
  else
    all_off();
}

void rotate_leds() {
  if (led_state == TL || led_state == TR || led_state == BR)
    led_state = led_state << 1;
    
  else if (led_state == BL)
    led_state = led_state >> 3;
    
  else
    led_state = 1;
    
  enforce_state();
}

void rotate_cc_leds() {
  if (led_state == TR || led_state == BR || led_state == BL)
    led_state = led_state >> 1;
    
  else if (led_state == TL)
    led_state = led_state << 3;
    
  else
    led_state = 1;
    
  enforce_state();
}

void fade(boolean synchronous) {
  // Sigh....the timing interrupt used disables PWM on the top two leds. Fading is not an easy option for them. :(
  static bool moving;
  static bool rising;
  static int intensity;
  static unsigned long previous;
  float hold_interval = 10000/hz;
  if (!fade_initialized) {
    moving = true;
    rising = true;
    intensity = 0;
    previous = 0;
    fade_initialized = true;
    all_off();
  }
  
  if(moving) {
    OCR1A = prescaler/hz/60;
    if (rising)
      intensity += 1;
    else
      intensity -= 1;
    analogWrite(leds_ordered[2], intensity);
    if (synchronous)
      analogWrite(leds_ordered[3], intensity);
    else
      analogWrite(leds_ordered[3], 255 - intensity);
    if (intensity == 255) {
      moving = false;
      rising = false;
      OCR1A = prescaler/hz;
      if (synchronous)
        led_state = BR | BL;
      else
        led_state = BR;
      enforce_state();
    }
    else if (intensity == 0) {
      moving = false;
      rising = true;
      OCR1A = prescaler/hz;
      if (synchronous)
        led_state = 0;
      else
        led_state = BL;
      enforce_state();
    }
    return;
  }
  
  unsigned long current = millis();
  if(current - previous > hold_interval) {
    moving = true;
    previous = current;
  }
}

void doubles(int pattern) {
  int initialState;
  if (pattern == pattern_up_down)
    initialState = (TL | TR);
  if (pattern == pattern_side_side)
    initialState = (TL | BL);
  if (pattern == pattern_diagonals)
    initialState = (TL | BR);
    
  if (led_state == initialState || led_state == (initialState ^ ALL_STATES))
    led_state = led_state ^ ALL_STATES;
  else
    led_state = initialState;
  enforce_state();
}

void random_toggles() {
  static unsigned long previous;
  static float hold_interval;
  if (!random_toggles_initialized) {
    previous = 0;
    hold_interval = random(1000/hz, 5000/hz);
    random_toggles_initialized = true;
  }
  unsigned long current = millis();
  if (current - previous > hold_interval) {
    led_state = (led_state ^ random(1, 16));
    enforce_state();
    hold_interval = random(1000/hz, 5000/hz);
    previous = current;
  }
}

ISR(TIMER1_COMPA_vect) { // Function that runs when the interrupt is thrown
  hz = HZSCALER * analogRead(POTPIN) + 1;
  OCR1A = prescaler/hz;
  if (pattern == pattern_all_blink)
    all_blink();
  else if (pattern == pattern_rotate)
    rotate_leds();
  else if (pattern == pattern_cc_rotate)
    rotate_cc_leds();
  else if (pattern == pattern_sync_fade)
    fade(true);
  else if (pattern == pattern_alt_fade)
    fade(false);
  else if (pattern == pattern_up_down)
    doubles(pattern_up_down);
  else if (pattern == pattern_side_side)
    doubles(pattern_side_side);
  else if (pattern == pattern_diagonals)
    doubles(pattern_diagonals);
  else if (pattern == pattern_random_toggles)
    random_toggles();
}

void change_pattern() {
  // If the previous pattern turned off the timing interrupt, turn it back on.
  if (pattern == pattern_all_off || pattern == pattern_all_on) {
    TIMSK1 |= (1 << OCIE1A);
  }
  pattern = (pattern + 1) % NUM_PATTERNS;
  if (pattern == pattern_sync_fade || pattern == pattern_alt_fade) {
    fade_initialized = false;
    OCR1A = prescaler/hz;
  }
  else if (pattern == pattern_all_on || pattern == pattern_all_off) {
    TIMSK1 &= (0 << OCIE1A);
    if (pattern == pattern_all_on)
      all_on();
    else
      all_off();
  }
}

void setup() {
  pinMode(tl, OUTPUT);
  pinMode(bl, OUTPUT);
  pinMode(tr, OUTPUT);
  pinMode(br, OUTPUT);
  pinMode(13, OUTPUT);
    
  randomSeed(analogRead(0));
  
  noInterrupts();
  TCCR1A = 0; // Timer1 A register
  TCCR1B = 0; // Timer1 B register
  TCNT1 = 0; // Timer1 counter.
  
  /* EXPLANATION OF THE INTERRUPT VARIABLES
     When Timer1 counter (TCNT1) hits the value in OCR1A it will throw the TIMER1_COMPA_vect interrupt.
     To calculate OCR1A: 16MHz / TCCR1B prescaler value / desired Hz - to throw the interrupt.
     For example, if you want your interrupt to run 2 times every second ( 2 Hz ) do this:
     16MHz / 256 / 2Hz ---- That's 16000000 / 256 / 2.
     16MHz because that is the speed of the processor
     256 because TCCR1B has the CS12 bit set, which means 256 prescaler. The counter for Timer1 is only 16 bits so OCR1A has to be < 65535.
       This scalar allows greater flexability for when the interrupt throws. But in this program I keep it at 256.
       Timer0 and Timer2 are only 8 bit (i.e. OCRxA < 256), so the scalar is probably much more important when using those timers.
     2 because we want the interrupt to run twice every second i.e. 2 Hz.
     I had to read this page about 157 times before I achieved my minimal understanding of this stuff:
     http://letsmakerobots.com/node/28278
  */
  hz = HZSCALER * analogRead(POTPIN) + 1;
  OCR1A = prescaler/hz;  // Here prescaler = 16MHz/256. I mixed the two numbers because I could. hz is variable, but starts at 2.
  TCCR1B |= (1 << WGM12); // This bit means throw the exception when TCNT1 == OCR1A
  TCCR1B |= (1 << CS12); // This bit means our prescaler is 256
  TIMSK1 |= (1 << OCIE1A); // This bit actually turns on the interrupt. If this is not set, the interrupt will not be thrown.
  // You will notice this bit gets unset for the all_on and all_off patterns. I didn't have to turn it off, but I could. Why not?
  interrupts();
  attachInterrupt(0, change_pattern, RISING);
}

void loop() {

}

