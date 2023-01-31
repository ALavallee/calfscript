# CalfScript 🐮 : Light & easy to integrate scripting engine


CalfScript 🐮 is a light scripting language written in C. It is designed to be  used as an event-base scripting systems. It follows the following principles :

- Simple api to facilitate the insertion into any systems 🔨
- Sandbox by default 📦
- Extremely light (LOC ~ 2000) 🪶
- Decent execution performance 💨
- Stack memory allocation (memory disciplined, efficient, easy to restraint, no garbage collector and without fragmentation)  💻
- Low / No dependencies (uses malloc and libc)  💪


# Examples 📢

Calling a script function from C :
```C
    // init the scripting engine
    CalfScript script;
    calf_init(&script);

    //load the script code to execute
    CalfModule *file = calf_load_module(&script, code);
    
    // Calling the function "example" for the script with no arguments
    CalfValue script_result = calf_execute(&script, file, "example", NULL, 0);
```


CalfScript 🐮 implementation of the recursive fibonnaci algoritm :
```Rust
fn fib(n)
{
    if n <= 1
    {
        return n
    }

    return fib(n - 1) + fib(n - 2)
}
```

# How to setup 🔨
The most common way to build it is probably to simply integrate the files into your project / build process. It's only two files to add (calf.h and calf.c).

# Implementation details ⚙️
For the parsing and byte code generation, CalfScript is doing the lexer / parsing / codegen all at the same time. It allows very efficient compilation of the code into bytecode.

When it comes to execution, scripts will only use the buffer allocated by default by the engine. A Script never frees memory during his execution. Memory used will be re-used at the next call. It's great for control of resources. Be aware that CalfScript can't be used in systems that needs complex and never-ending loop of scripts since it will run out of memory. That design has for goals to make it extremely efficient into an event-based system where calls are short and aim at a specific goals.

To make things clear. No memory is leaking from your script. When a script call ends, the memory used for the previous call will be re-used in the new call since the stack memory allocate resets.

# Question / Reaching out ❓

For anything related to this project (bugs 🐛, discussions 💬 , concerns 📙 ) feel free to simply open an issue, I'll be more than happy to answer it.


# License 📁


    Copyright c 2023 Antoine Lavallée
    
    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:
    
    The above copyright notice and this permission notice shall be included in all
    copies or substantial portions of the Software.
    
    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
    SOFTWARE.

# Todo 📝
List of what I will be implementing soon.
- Implement a way to free loaded scripts from memory
- Remove dependencies to libc
- Implement more robust syntax check
- For loop implementation
- Optimisation of script-to-script calls
- Use pointer to values instead of value directly to reduce copy into the stack
