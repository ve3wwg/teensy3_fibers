// Example Sketch illustrating the use of the fibers library.
// Warren W. Gay VE3WWG	 Sun May 4, 2014
// This is GPL code.

#include <fibers.h>

// Allocate up to main + 5 fibers:
Fibers<6> fibers(2000); // main loop() is allocated 2000 bytes of stack

int ledm = 13;          // Main LED on teensy 3.x
int led0 = 0;           // Digital pin 0
int led1 = 1;           // Digital pin 1 
int led2 = 2;           // Alter to suit
int led3 = 3;           //  your wiritng preference 

uint32_t ffoo = 0;      // fibers.create() index for the foo() fiber
uint32_t fbar = 0;      // fibers.create() index for bar() etc.
uint32_t fzoo = 0;

void yield() {
  fibers.yield();      // Yield cpu to next fiber in round-robin sequence
}

static void
toggle(int led,int& led_state) {
  led_state ^= 1;                              // Alternate state on/off
  digitalWrite(led,led_state&1 ? HIGH : LOW);  // LED on/off
}

// First fiber (coroutine)
static void foo(void *arg) {
  unsigned count = 0;
  int led0_state = 0;  
                
  while ( count++ < 35 ) {
    toggle(led0,led0_state);
    delay(230);
  }
  digitalWrite(led0,LOW);
}

// 2nd fiber (coroutine)
static void bar(void *arg) {
  unsigned count = 0;
  int led1_state = 0;  
                
  while ( count++ < 40 ) {
    toggle(led1,led1_state);
    delay(250);
  }
  digitalWrite(led1,LOW);
}

// 3rd fiber (coroutine)
static void zoo(void *arg) {
  unsigned count = 0;
  int led2_state = 0;  
                
  while ( count++ < 47 ) {
    toggle(led2,led2_state);
    delay(270);
  }
  digitalWrite(led2,LOW);
}

void setup() {
  pinMode(ledm,OUTPUT);
  pinMode(led0,OUTPUT);
  pinMode(led1,OUTPUT);
  pinMode(led2,OUTPUT);
  pinMode(led3,OUTPUT);
  
  digitalWrite(ledm,HIGH);
  digitalWrite(led0,LOW);
  digitalWrite(led1,LOW);
  digitalWrite(led2,LOW);
  digitalWrite(led3,LOW);
  
  ffoo = fibers.create(foo,0,1000);  // Create first coroutine foo(0)
  fbar = fibers.create(bar,0,1200);  // Create 2nd coroutine bar(0)
  fzoo = fibers.create(zoo,0,1100);  // Create 3rd coroutine zoo(0)
}

// main thread 
void loop() {
  unsigned count = 0;
  int ledm_state = 0;
  
  digitalWrite(ledm,LOW);        // Turn off Teensy LED while main loops
                
  while ( count++ < 20 ) {      // Loop only 20 times in main
    toggle(led3,ledm_state);    // Toggle led3 while main thread runs
    delay(350);
  }
  
  digitalWrite(ledm,HIGH);      // Teemsy LED on means it is doing joins
  digitalWrite(led3,HIGH);
        
  fibers.join(ffoo);            // Stop here until foo() returns
  fibers.join(fbar);            // Stop here until bar() returns
  fibers.join(fzoo);            // Stop here until zoo() returns
        
  digitalWrite(led3,LOW);       // Indicate pause before restarts 
        
  delay(3000); 
                
  fibers.restart(ffoo,foo,0);      // Restart foo(0) at next yield()
  fibers.restart(fbar,bar,0);      // Restart bar(0) at next yield()
  fibers.restart(fzoo,zoo,0);      // etc..
}

// End arduino_fibers.ino
