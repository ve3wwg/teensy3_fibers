teensy3_fibers
==============

This is a Teensy-3.x Lightweight Fibers Project, usable by Arduino IDE or a Makefile build,
such as my teensy3_lib project on github.

HOW TO:
=======

Install this directory in your Arduino library folder. On a Mac, this will be in the directory
~/Documents/Arduino/libraries. In other words, install these files in the directory (on Mac)
~/Documents/Arduino/libraries/fibers.

    $ cd ~/Documents/Arduino/libraries
    $ git clone git@github.com:ve3wwg/teensy3_fibers.git
    $ mv teensy3_fibers fibers  # Rename the subdirectory simply as fibers


Then start your Arduino IDE, and pull down the menu Sketch->Import Library. At the bottom
of the menu, underneath "Contributed", select the entry "fibers". If successful, the line

	#include <fibers.h> 

should appear in your sketch.

Fibers API Usage:
-----------------
 
Somewhere it is recommended that you declare one instance of
Fibers globally. Global access makes it available from other
modules when it is time to "yield". Alternatively, your code
could call the globally available C function yield(), if you
define it as follows:

      void yield() {
              fibers.yield();
      }

The Fibers class is a template class, requiring a template parameter,
that declares the maximum number of fibers you intend to create.
By default 15 fibers + main are assumed.
 
     Fibers<> fibers;            // Defaults to 15 + main fibers max
     Fibers<4> fibers;           // Up to 3 fibers + main
 
By default 8K of stack is reserved for the main fiber. To change
the allocation for main (only), specify a stack size:
 
     Fibers<4> fibers(3000);     // Allocate 3000 bytes to main stack
     
For instrumenting the stack to determine stack usage, view the documentation
found at the end of the source file fibers.h.

CREATING FIBERS:

To create a fiber, you invoke the Fibers::create() method:
 
     fibers.create(foo,foo_arg,4000); // Stack for foo is 4000 bytes
     
It returns an unsigned number > 0 for identification if you
need it (main is always zero). This example will start function:
 
     void foo(void *foo_arg)
 
with an allocated stack of 4000 bytes.
 
Additional fiber (coroutines) can be created from any executing
fiber context.

From this point on, the main routine or the executing coroutine
(in foo) call the yield method to give up the CPU:
 
     fibers.yield();
              
or the global yield() function, if you set that up. 

The fibers.create() method returns an index representing the fiber number,
where 0 represents the main thread, and values 1 and greater are the
created fiber threads. This index can be used in API calls like join().

FIBERS THAT RETURN:

If a fiber routine like foo() above "returns", it changes to a FiberReturned
state. It simply enters a loop that just yields() when this happens. It also
informs the fibers.join() method, that the fiber has terminated.  

Terminated fibers can be restarted, by use of the fibers.restart() method.

CLASS API:

    enum FiberState {
        FiberCreated,                   // Fiber created, but not yet started
        FiberExecuting,                 // Fiber is executing
        FiberReturned,                  // Fiber has terminated
        FiberInvalid                    // Invalid fiberx value
    };

    template <unsigned max_fibers=16>
    class Fibers {
        ...
    public: Fibers(uint32_t main_stack=8192,bool instrument=false,uint32_t pattern_override=0xA5A5A5A5);
        inline uint32_t size() { return n_fibers; }     // Return the current # of coroutines
  
        unsigned create(fiber_func_t func,void *arg,uint32_t stack_size);
        inline unsigned current() { return cur_crx; }   // Return current coroutine number (0 == main)
        void yield();                                   // Yield CPU to next coroutine
        
        FiberState state(uint32_t fiberx);              // Return the state of a fiber
        FiberState join(uint32_t fiberx);               // Return when the specified fiber has ended
        
        FiberState restart(uint32_t fiberx,fiber_func_t func,void *arg); // Restart fiber after it has terminated
        
        uint32_t stack_size(uint32_t fiberx);           // Return the approximate stack usage for specified coroutine
    };




