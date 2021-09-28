DEBUG STM8 programs compiled and build with SDCC compiler.

1. After building the program, transfer the .HEX file to STM8 Discovery and look for .RST file containing the list of the variables used with address and length.
2. Load the file into SmartSTLINK
3. Connect to yuor STM8 Discovery via USB
4. Enable refresh variables. You'll find the number of the variables found
and you can scroll them with the slide bar.
5. The central column indicate the nature and lenght of the variable.
(for now all variables are S=signed so you can see S1, S2 or S4)
6. To write values in a variable just click on it, digit the value and press enter key
7. When you write e variable the value won't refresh and if you scroll the table
the one that was written remain unscrollable. So you have to move out the cursor
clicking on other part (i.e. on file name)
