Exchange works as follows:

FIFO/Signals:
File descriptors and FIFOs are dynamically allocated and signal handlers for both SIGCHLD and SIGUSR1 are setup with flags SIGINFO (to store sending PID). Next, any prior FIFOs are cleaned and two new FIFOs are created for each trader, exchange_fifo and trader_fifo. Next, fork(). Child process: -> trader executes their binary. Parent process: -> stores child PID and opens exchange_FIFO for writing and trader_FIFO for reading. 

Send market open to all.

If/once SIGUSR1 is received, a global flag is set to true and sending PID stored. For SIGCHLD, a counter keeps track of how many traders are disconnected and another stores sending PID. Once flag is true, logic occurs.

Logic:
It utilises two singly linkedlists buybook and sellbook which store orders. Also, a multidimensional array stores matches -> matchbook[trader][product][match_idx]. In event loop, first an order from sending PID is added and if successful, then positions are attempted to be matched, orderbook and positions printed.

Tests work as follows:

E2E Tests:

I have enclosed a tradertest.c, this can be complied using the compile script. To use, just enclose orders within the file and compile and run. The tradertest.c itself has a simple interface to add orders. Using the ./compile in terminal will compile everything, simply run the spx_exchange and the trader will interact with exchange based on whatever orders were put through. 

Cmocka:
Utilise " gcc -o unit-tests unit-tests.c libcmocka-static.a -lm " to compile and "./unit-tests" to run tests. Please ensure an #ifndef TESTING and #endif is included in spx_exchange.c surrounding the main function to avoid a redefinition.