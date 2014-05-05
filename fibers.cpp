//////////////////////////////////////////////////////////////////////
// fibers.cpp -- Implementation of asm fibers code
// Date: Thu May  1 21:00:28 2014
///////////////////////////////////////////////////////////////////////

#include <stdint.h>
#include <fibers.h>

extern "C" {
	static void fiber_start();
	extern void yield();
}

static volatile uint32_t headroom = 8192;	// Headroom of main() or last fiber created
static volatile uint32_t stackroot = 0;		// Address of the stack root for last fiber (else 0)

//////////////////////////////////////////////////////////////////////
// Configure how much stack to allocate to the main() thread
//////////////////////////////////////////////////////////////////////

void
fiber_set_main(uint32_t stack_size) {

	if ( !stackroot ) 			// If not too late..
		headroom = stack_size;		// Set the main program's stack size
}

//////////////////////////////////////////////////////////////////////
// This routine is the fiber's launch pad routine
//////////////////////////////////////////////////////////////////////

static void
fiber_start() {
	fiber_t *fiber;

	asm("mov %[result], r12\n" : [result] "=r" (fiber));		// r12 points to fiber initially

	fiber->state = FiberExecuting;

	asm("mov r0, %[value]\n" : : [value] "r" (fiber->arg));		// Supply void *arg to fiber call
	asm("mov r1, %[value]\n" : : [value] "r" (fiber->funcptr));	// r1 now holds the function ptr to call

	asm("push {r2-r12}");
	asm("blx  r1\n");			// func(arg) call
	asm("pop  {r2-r12}");

	fiber->state = FiberReturned;		// Fiber has returned from its function

	for (;;) {
		::yield();
	}
}

//////////////////////////////////////////////////////////////////////
// Set up a fiber to execute (but don't launch it)
//////////////////////////////////////////////////////////////////////

void
fiber_create(volatile fiber_t *fiber,uint32_t stack_size,fiber_func_t func,void *arg) {

	asm("stmia r0,{r1-r11}\n");		// Save r1-r11 to fiber struct

	fiber->stack_size = stack_size;		// In r1
	fiber->funcptr 	= func;			// In r2
	fiber->arg 	= arg;			// In r3
	fiber->r12	= (uint32_t)fiber;	// Overwrite r12 with fiber ptr

	if ( !stackroot )
		asm (	"mov %[result], sp\n" : [result] "=r" (stackroot) );

	stackroot -= stack_size;		// This is the new root of the stack
	headroom = stack_size;			// Save this stack size for creation of the next fiber

	fiber->sp = stackroot;			// Save as Fiber's sp
	fiber->lr = (void *) fiber_start;	// Fiber startup code
	fiber->state = FiberCreated;		// Set state of this fiber
	fiber->initial_sp = stackroot;		// Save sp for restart()
}

//////////////////////////////////////////////////////////////////////
// Swap one fiber context for another
//////////////////////////////////////////////////////////////////////

void
fiber_swap(volatile fiber_t *nextfibe,volatile fiber_t *prevfibe) {
	
	asm("mov %[result], sp\n" : [result] "=r" (prevfibe->sp));	// Save current sp
	asm("add r1, #4\n");						// &prevfibe->r2
	asm("stmia r1!,{r2-r12,lr}\n");					// Save r2-r12 + lr

	asm("ldmia r0,{r1-r12,lr}\n");					// Restore r1(sp) and r2-r12, lr
	asm("mov sp,r1\n");						// Set new sp
	// asm("bx  lr\n");						// Return to where we left off in this fiber
}

//////////////////////////////////////////////////////////////////////
// Restart a terminated fiber
//////////////////////////////////////////////////////////////////////

void
fiber_restart(volatile fiber_t *fiber,fiber_func_t func,void *arg) {

	fiber->r12	= (uint32_t) fiber;				// Tell fiber_start() where the fiber_t struct is
	fiber->funcptr	= func;						// New fiber routine
	fiber->arg	= arg;						// New arg value
	fiber->state	= FiberCreated;					// Fiber is being launched
	fiber->lr	= (void *)fiber_start;				// Address of next instruction
	fiber->sp	= fiber->initial_sp;				// Reset sp
}

// End fibers.cpp


