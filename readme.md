# 6502Linter
This project is a linter that solves a very specific formatting issue we ran into while developing programs for the Nintendo Entertainment System in 6502 assembly.

## Setup & Usage
If you're confused about what this does/why it exists, read on from [The Problem](https://github.com/Akadeax/6502-linter?tab=readme-ov-file#the-problem).

### Setup
Either compile the program yourself (MSVC & VS2022 Solution) or find the precompiled binaries for Windows under Releases.

### Usage
To use, just call `6502Linter.exe <PATH_TO_MAIN_FILE> <TEMP_FORMAT>`.


`PATH_TO_MAIN_FILE` is the path to your main assembly file; it will resolve all `.include`s automatically.

`TEMP_FORMAT` is the format for your temporary variables. The only requirement is that it ends in `x`, indicating the variable number in the name, e.g. `zp_temp_x`.

For my projects, this is a command ran in my `build.bat` script. See [NESPad](https://github.com/Akadeax/nespad/blob/main/build.bat)'s build script as an example.

[Image of console output using the linker](./RepoIMG/LintOutput.png)

## The Problem
In 6502 assembly, it is common to use temporary variables stored in **zero-page memory** (the first 256 bytes of RAM). This is due to the 6502 having only 3 actually usable registers.

However, imagine the following situation:
```x86asm
.proc my_first_function
; store something into temporary variable #1
jsr my_second_function ; call my_second_function
; do something with temporary variable #1
.endproc

.proc my_second_function
; do some operations with temporary variable #1
.endproc
```
Calling `my_second_function` has a side effect that isn't very obvious at all without inspecting the function itself!
<br>
In a larger program, if we wanted to call a function, we wouldn’t want to check which temporaries that function uses, or if it calls other functions, track which temporaries those use, and so on.
<br>
What's the alternative? Structure!

## Structure
### Zero-Page
start by defining your temporary variables in your zero-page memory:
```x86asm
.segment "ZEROPAGE"
zp_temp_0: .res 1
zp_temp_1: .res 1
zp_temp_2: .res 1
...
zp_temp_n: .res 1 ; up to 99 if necessary; usually up to '_9' is more than enough
```
These can have any format as long as they end in a number. See [Usage](https://github.com/Akadeax/6502-linter?tab=readme-ov-file#usage).

### When using temporaries
When a function uses any of your zero-page temporaries, name the function based on how many it uses:
```x86asm
.proc my_function_T1
; store something in zp_temp_0
; do some calculation with zp_temp_1
.endproc
```
This function modifies `zp_temp_0` and `zp_temp_1`. When calling `my_function_T1`, assume that all temporaries until `zp_temp_1` will be invalidated. 
<br>
If your hypothetical function `my_multiplier_T7` is called, assume that calling it invalidates `zp_temp_0` - `zp_temp_7`.

If you wish for a function to ignore the linter's naming convention, end the line you define it on with `;LINTIGNORE` (handy for your interrupts, for example).

Using this format turns debugging of this kind from `"Let's read all functions we call here, and everything that is called by that?"` into `"Oh, this call invalidates my temporaries!"`.



## What if I mess up the format?
That's where this project comes in. It serves as a linter for confirming that your functions are correctly suffixed after how many temps are used. See [Setup](https://github.com/Akadeax/6502-linter?tab=readme-ov-file#setup).

# License
This is public domain, [Unlicense](https://unlicense.org/) specifically. Do whatever you want with it.
