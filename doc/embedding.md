# lx embedding guide

Embedding lx is incredbily straight forwards, requiring just a couple function calls to get going.
The api is still pretty deep though, every function is documented in `lx.h` for reference.
Alternatively, look at the cli implementation at the bottom of `lx.c` for inspiration.

## Minimal example
```c
#include <stdio.h>

void lx_print(const char* msg) {
    printf("%s", msg);
}

int main(int argc, char** argv) {
    // 1k is enough for anyone
    char lx_memory[1024];

    // Pass our memory, the size of the program section, the cell section, and then the print function
    lx_Ctx* ctx = lx_open(lx_memory, sizeof(lx_memory) / 2, sizeof(lx_memory) / 2, lx_print);
    lx_Value* result = lx_run(ctx, NULL, "= x + 10 10 , \"x is \" , x;" x); // prints "x is 20" to the console, and returns x

    if(lx_isnumber(result)) {
        printf("We got a number! %.3f\n", lx_getnumber(result));
    }

    return 0;
}
```

## Using environments
Creating an environment outside of your context and passing it along to `lx_run` is a great way to enable some persistence, this is how the REPL works.

```c
lx_Ctx* ctx = ...;
lx_Value* env = lx_makenv(ctx);
lx_persist(env); // mark env as persistent, stopping it from being garbage collected

lx_setenv(ctx, env, lx_symbol(ctx, "x", 1), lx_number(ctx, 10)); // set env through symbol references
lx_setenvc(ctx, env, "y", lx_number(ctx, 20)); // ... or strings

lx_run(ctx, env, ", + x y"); // pass along env, and print 30!
```

## Binding functions
You can easily define functions inline in your native code, if you want them to be availible to the lx context.

```c
lx_Ctx* ctx = ...
lx_Value* env = ...

lx_setenvc(ctx, env, "add", lx_fn(ctx, "(a b)", "+ a b")); // syntax is exactly the same as in lx

lx_run(ctx, env, ", add 100 200") // prints 300!
```

Even completely native functions can be provided for more advanced functionality!

```c
lx_Value* my_lx_sqrt(lx_Ctx* ctx, lx_Value* env) {
    lx_Value* x = lx_getenvc(env, "x");

    if(!lx_isnumber(x)) return lx_nil();

    double to_sqrt = lx_getnumber(x);
    return lx_number(ctx, sqrtd(to_sqrt));
}

lx_Ctx* ctx = ...
lx_Value* env = ...

lx_setenvc(ctx, env, "sqrt", lx_cfn(ctx, "x", my_lx_sqrt)); // note that you still pass an args string

lx_run(ctx, env, ", sqrt 9") // prints 3!
```