//////////////////////////////////////////////////////////////////////
// fibers.h -- Fibers Class Support
// Date: Sat Apr 19 00:14:27 2014   (C) Warren Gay VE3WWG
///////////////////////////////////////////////////////////////////////
// DESCRIPTION:
// 
// This is a very light weight fibers (aka coroutines) library,
// managed by a singular instance of class Fibers. This module
// depends upon two symbols in your ld script, which is usually
// mk20dx256.ld (or in my case mk20dx256_sbrk.ld).
//
// See the end of this file for more information
///////////////////////////////////////////////////////////////////////

#ifndef FIBERS_H
#define FIBERS_H

#include <stdint.h>

extern "C" {
	// Your coroutine functions will use this function prototype
	typedef void (*fiber_func_t)(void *arg);
}

enum FiberState {
	FiberCreated,			// Fiber created, but not yet started
	FiberExecuting,			// Fiber is executing
	FiberReturned,			// Fiber has terminated
	FiberInvalid			// Invalid fiberx value
};

struct fiber_t {
	// Saved Registers
	uint32_t	sp;		// Saved sp register
	uint32_t	r2;		//  after the function has begun
	uint32_t	r3;		// Saved r3 etc.
	uint32_t	r4;
	uint32_t	r5;
	uint32_t	r6;
	uint32_t	r7;
	uint32_t	r8;
	uint32_t	r9;
	uint32_t	r10;
	uint32_t	r11;
	uint32_t	r12;
	void		*lr;		// Return address (pc)
	// Parameters
	fiber_func_t	funcptr;	// Start function ptr
	void		*arg;		// Startup arg value
	uint32_t	stack_size;	// Stack size for this main/coroutine
	uint32_t	initial_sp;	// Initial stack pointer (for restart)
	// State
	FiberState	state;		// Current fiber state
};

//////////////////////////////////////////////////////////////////////
// There must only be ONE instance of this class defined
//////////////////////////////////////////////////////////////////////

template <unsigned max_fibers=16>
class Fibers {
	volatile fiber_t	fibers[max_fibers];	// Collection of fiber states
	volatile uint16_t	n_fibers;		// Number of active fibers
	volatile uint16_t	cur_crx;		// Index to currently executing coroutine
	bool			instrument;		// Instrument stack for stack size measurement
	uint32_t		pattern;		// Pattern to use on stack for instrumentation

	uint32_t *end_stack();				// Return end of stack address (requires _sstack)

public:	Fibers(uint32_t main_stack=8192,bool instrument=false,uint32_t pattern_override=0xA5A5A5A5);
	inline uint32_t size() { return n_fibers; }	// Return the current # of coroutines

	unsigned create(fiber_func_t func,void *arg,uint32_t stack_size);
	inline unsigned current() { return cur_crx; }	// Return current coroutine number (0 == main)
	void yield();					// Yield CPU to next coroutine

	FiberState state(uint32_t fiberx);		// Return the state of a fiber
	FiberState join(uint32_t fiberx);		// Return when the specified fiber has ended

	FiberState restart(uint32_t fiberx,fiber_func_t func,void *arg); // Restart fiber after it has terminated

	uint32_t stack_size(uint32_t fiberx);		// Return the approximate stack usage for specified coroutine 
};


//////////////////////////////////////////////////////////////////////
// Assembler routines used:
//////////////////////////////////////////////////////////////////////

extern "C" {
	extern uint32_t *fiber_getsp();
	extern void fiber_set_main(uint32_t stack_size);
	extern void fiber_create(volatile fiber_t *fcb,uint32_t stack_size,fiber_func_t func,void *arg);
	extern void fiber_swap(volatile fiber_t *new_cr,volatile fiber_t *cur_cr);
	extern void fiber_restart(volatile fiber_t *fiber,fiber_func_t func,void *arg);
}

//////////////////////////////////////////////////////////////////////
// Constructor for Fibers:
//
//	main_stack	Indicates # of stack bytes to reserve for main
//	instrument	When true, the stack is cleared with a pattern
//			so that stack_size() can determine stack
//			usage. (Default false)
//	pattern_override Default: 0xA5A5A5A5, but can be overriden by
//			the user, if false matches have been a problem
//////////////////////////////////////////////////////////////////////

template <unsigned max_fibers>
Fibers<max_fibers>::Fibers(uint32_t main_stack,bool instrument,uint32_t pattern_override) {

	pattern = pattern_override;	// Fill pattern for stack (when enabled)

	fiber_set_main(main_stack);	// Set stack size needed by the main coroutine

	n_fibers = 1;			// Reserve fibers[0] for main thread
	cur_crx = 0;			// Main is currently in control
	fibers[0].stack_size = main_stack; // Save main's stack size

	this->instrument = instrument;	// Enable/disable instrumentation

	if ( instrument ) {
		volatile uint32_t *ep = end_stack();
		uint32_t *sp = fiber_getsp();

		while ( --sp > ep )
			*sp = pattern;	// Fill stack with pattern
	}
}

//////////////////////////////////////////////////////////////////////
// Create a new fiber:
//
// RETURNS:
//	1-n for each fiber (coroutine) created
//	~0 if we exceeded the maximum number of fibers
//
// NOTES:
//	1. Can be called from any fiber (aka coroutine)
//	2. Returns a value 1-n, for nth coroutine
//	3. Zero represents the main fiber (coroutine)
//	4. This code does not check if there is enough stack space
//	   for the request. Choose stack size carefully.
//////////////////////////////////////////////////////////////////////

template <unsigned max_fibers>
unsigned
Fibers<max_fibers>::create(fiber_func_t func,void *arg,uint32_t stack_size) {

	// Round stack size to a word multiple
	stack_size = ( stack_size + sizeof (uint32_t) ) / sizeof (uint32_t) * sizeof (uint32_t);

	if ( n_fibers >= max_fibers )
		return 0;		// Failed: too many fibers

	volatile fiber_t &new_cr = fibers[n_fibers++];

	fiber_create(&new_cr,stack_size,func,arg);
	new_cr.stack_size = stack_size;

	return n_fibers - 1;		// Return zero-based coroutine # (main == 0)
}

//////////////////////////////////////////////////////////////////////
// Yield CPU to next coroutine, round-robin 
//
// NOTES:
//	1. Normally, this causes control to pass to the next fiber,
//	   in round-robin fashion, returning to the caller after we
// 	   get the CPU passed back to us again.
//	2. Except when there is only the main context running, which
//	   then simply returns immediately to the caller again.
//////////////////////////////////////////////////////////////////////

template <unsigned max_fibers>
void
Fibers<max_fibers>::yield() {

	if ( n_fibers <= 1 )
		return;			// There is only the main context running

	uint16_t nextx = cur_crx + 1;	// Compute the coroutine # of the next fiber
	if ( nextx >= n_fibers )
		nextx = 0;		// Wrap around to main context at the end

	volatile fiber_t& last_cr = fibers[cur_crx];	// Current context
	volatile fiber_t& next_cr = fibers[nextx];	// Next context
	cur_crx = nextx;			// Change the context no.

	fiber_swap(&next_cr,&last_cr);		// Make the switch

	// cur_crx has been restored by the last yield() caller
}

//////////////////////////////////////////////////////////////////////
// Determine the coroutine's stack size in bytes
//
//	1. Must have been instrumented (see constructor)
//	2. Returns approx size in bytes
//	3. Determined on a 'best effort' basis, since a false
//	   match on the instrumented pattern may cause a smaller
//	   size to be returned.
//////////////////////////////////////////////////////////////////////

template <unsigned max_fibers>
uint32_t
Fibers<max_fibers>::stack_size(uint32_t fiberx) {

	if ( !instrument )				// Not instrumented
		return ~0;				// .. so size unknown

	if ( fiberx >= n_fibers )
		return ~0;				// Invalid fiber/coroutine #

	fiber_t& crentry = fibers[fiberx];		// Access requested coroutine
	uint32_t *sp = (uint32_t *)crentry.sp;		// Get it's sp
	uint32_t *ep = sp - crentry.stack_size/4;	// stack_size has been rounded to a word multiple
	uint32_t count = 0;				// Initialize count

	for ( ; sp >= ep && *sp != pattern; --sp )
		count += 4;

	return count;					// Bytes
}


//////////////////////////////////////////////////////////////////////
// Return the state of a fiber:
//
// RETURNS:
//	FiberCreated		Fiber created, but not yet started
//	FiberExecuting		Fiber is executing
//	FiberReturned		Fiber has terminated (returned)
//	FiberInvalid		Invalid fiberx value
//////////////////////////////////////////////////////////////////////

template <unsigned max_fibers>
FiberState
Fibers<max_fibers>::state(uint32_t fiberx) {

	if ( fiberx >= n_fibers )
		return FiberInvalid;			// Invalid fiberx value given

	volatile fiber_t& crentry = fibers[fiberx];	// Access requested coroutine
	return crentry.state;				// Return its state
}

//////////////////////////////////////////////////////////////////////
// Join with the executing fiber.
//
// RETURNS:
//	FiberInvalid		Fiberx does not refer to a valid fiber
//	FiberReturned		Fiber has terminated (successful)
//////////////////////////////////////////////////////////////////////

template <unsigned max_fibers>
FiberState
Fibers<max_fibers>::join(uint32_t fiberx) {
	FiberState s;

	if ( fiberx >= n_fibers )
		return FiberInvalid;			// Invalid fiberx: Nothing to join with

	while ( (s = state(fiberx)) != FiberReturned )
		this->yield();				// Keep waiting

	return s;
}

//////////////////////////////////////////////////////////////////////
// Restart a terminated fiber
//////////////////////////////////////////////////////////////////////

template <unsigned max_fibers>
FiberState
Fibers<max_fibers>::restart(uint32_t fiberx,fiber_func_t func,void *arg) {

	if ( fiberx >= n_fibers )
		return FiberInvalid;			// Invalid fiberx: Nothing to join with

	volatile fiber_t& fiber = fibers[fiberx];
	fiber_restart(&fiber,func,arg);
	return fiber.state;
}


//////////////////////////////////////////////////////////////////////
// Protected: Return address of end of stack
//////////////////////////////////////////////////////////////////////

template <unsigned max_fibers>
uint32_t *
Fibers<max_fibers>::end_stack() {
	extern uint32_t _sstack;			// This comes from mk20dx256*.ld
							// See fibers.hpp at end.
	return &_sstack;
}

#endif // FIBERS_H

//////////////////////////////////////////////////////////////////////
// DESCRIPTION:
// 
// This is a very light weight fibers (aka coroutines) library,
// managed by a singular instance of class Fibers. This module
// depends upon two symbols in your ld script, which is usually
// mk20dx256.ld (or in my case mk20dx256_sbrk.ld).
// 
// The two required symbols are:
// 
// 	_estack		End stack: Which should already be defined for you
// 	_sstack		Start stack: Which is probably not defined
// 
// To add the _sstack symbol to your linking, locate your
// mk20dx256.ld (or equivalent) script:
// 
//     $ find <root_dir> -name 'mk20dx256*'
// 
// where <root_dir> varies by platform. Using my teensy3_lib
// structure, the root_dir is likely /opt/teensy3_lib.
// 
// Otherwise, using the Arduino IDE, use
// /Applications/Arduino.app for your root_dir on the Mac. On
// Windows, you'll have to sort that out for yourself.
// 
// Look towards the end if the file for the section labelled .stack :
// 
// 	.stack :
// 	{
// 	    . = ALIGN(4);
//             _sstack = .; /* Add this line to define the start of the stack */
// 	    . = . + _minimum_stack_size;
// 	    . = ALIGN(4);
// 	} >RAM
// 
// 	_test_estack = ORIGIN(RAM) + LENGTH(RAM);
// 	_estack = ORIGIN(RAM) + 64 * 1024;
// 
// The symbol for _estack should already be defined. Add the line
// above for _sstack where the comment says "Add this line", if
// necessary.
// 
// The purpose of the _sstack symbol is to tell the Fibers class
// where the stack begins. This is only required if you are
// "instrumenting" the stack to later find out how much stack
// space was actually used by a given coroutine (or main
// routine). Unfortunately, this symbol is required whether you
// plan to use this feature or not.
// 
// COMPILING:
// ----------
// 
// When using the teensy3_lib structure, the class header should be
// includable in your C++ code as:
// 
// //  #include <fibers.hpp>
// 
// Arduino IDE users may be out of luck, since one assembler routine
// is required (it can be done, but messing with the IDE build would
// be necessary to include that one assembled object file).
// 
// LINKING:
// --------
// 
// When using the teensy3_lib structure, the fibers library just needs
// the linker option added:
// 
//     -lfibers
// 
// Fibers API Usage:
// -----------------
// 
// Somewhere it is recommended that you declare one instance of
// Fibers globally. Global access makes it available from other
// modules when it is time to "yield".
//
// Global yield() Routine:
// -----------------------
//
// To make maximum use of CPU, declare this routine somewhere:
//
//	void yield() {
//		fibers.yield();
//	}
//
// The Fibers class is a template class, requiring a template parameter,
// that declares the maximum number of fibers you intend to create.
// By default 15 fibers + main are assumed.
//
//     Fibers<> fibers;            // Defaults to 15 + main fibers max
//     Fibers<4> fibers;           // Up to 3 fibers + main
//
// By default 8K of stack is reserved for the main fiber. To change
// the allocation for main (only), specify a stack size:
//
//     Fibers<4> fibers(3000);     // Allocate 3000 bytes to main stack
//
// To instrument the stack for determining stack size, add the
// boolean true parameter:
//
//     Fibers<4> fibers(3000,true); // Use a main stack of 3000 bytes, and
//                                  // instrument stack for size checks    
// 
// Once this has been done, stack space has been reserved for the
// main thread.
// 
// To create a fiber, you invoke the Fibers::create() method:
// 
//     fibers.create(foo,foo_arg,4000); // Stack for foo is 4000 bytes
// 
// It returns an unsigned number > 0 for identification if you
// need it (main is always zero). This example will start function:
// 
//     void foo(void *foo_arg)
// 
// with an allocated stack of 4000 bytes.
// 
// Additional fiber (coroutines) can be created from any executing 
// fiber context.
// 
// From this point on, the main routine or the executing coroutine
// (in foo) call the yield method to give up the CPU:
// 
//     fibers.yield();
// 
// or the global yield() function, if you set that up.
// 
// Scheduling is done in round-robin fashion, giving the CPU to
// the next coroutine in sequence.
// 
// FIBERS THAT "RETURN":
// =====================
// 
// If a coroutine (other than main) "returns", it will simply
// it will cease to execute (it actually does execute in
// a loop which just calls yield). It's state is marked as
// FiberReturned.
//
// The fiber can be restarted using the Fibers::restart()
// method, allowing a worker fiber to be controlled.
// 
// DETERMINING STACK SIZE:
// =======================
// 
// The first question that arises when creating threads or fibers
// is "how much stack space do I need?" To estimate this, the
// Fibers class has the optional "instrument" boolean, which
// when enabled, causes the stack to be initialized with a "pattern".
// By default this feature is disabled (see constructor). When
// enabled the default pattern used is 0xA5A5A5A5.
// 
// Allow your coroutines to run for a time and then you can:
// 
//     bytes = fibers.stack_size(x);
// 
// where x is the coroutine number (x==0 is the main routine).
// 
// This estimates usage based upon the stack pattern words being
// overwritten with something other than "pattern". Even if you
// don't use any coroutines, you should be able to estimate the
// stack use of the main program.
// 
// If you suspect a false match on "pattern" has occurred,
// causing a smaller size to be returned, repeat the test using a
// different uint32_t pattern value, that is unlikely to appear
// on the stack.
//
// WARNING:
// ========
// When allocating stack space, be sure to include an extra
// amount for use by interrupt service routines.
//
// LDSCRIPT CHANGE:
// ================
//
// This package needs to know where the bottom of your stack is
// through the use of the symbol _sstack. Your current ldscript
// should already define an _estack symbol.
//
// Towards the end of the ldscript that I use, I have:
//
//	/* this just checks there is enough RAM for the stack */
//	.stack :
//	{
//	    . = ALIGN(4);
//          _sstack = .;
//	    . = . + _minimum_stack_size;
//	    . = ALIGN(4);
//	} >RAM
//
//	_estack = ORIGIN(RAM) + 64 * 1024;
//
// Notice how the _sstack symbol was inserted to identify the low
// (bottom) point of the stack.
//
//////////////////////////////////////////////////////////////////////

// End fibers.h
