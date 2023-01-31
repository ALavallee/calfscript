#include <stdio.h>
#include "calf/calf.h"

#include <sys/time.h>

int main() {

    char code[] = "fn fib(n)"
                  "{"
                  "if n <= 1"
                  "{"
                  "    return n"
                  "}"
                  "   return fib(n - 1) + fib(n - 2)"
                  "}"
                  "fn test()"
                  "{"
                  "return fib(34)"
                  "}";

    CalfScript script;
    calf_init(&script);

    CalfModule *file = calf_load_module(&script, (char *) &code);

    CalfValue script_result = calf_execute(&script, file, "test", NULL, 0);


    if (script_result.type == CALF_VALUE_TYPE_ERROR) {
        printf("Error happened during calf call : ");
        printf(script_result.error);
    } else {
        printf("The fib result is : %d\n", script_result.int_value);
    }

    return 0;
}
