# 🔹 lx - a miniscule dynamic language

Mostly an experiment to make a language as small as possible while feeling complete.

If you can do something cool with it, even better!

```
` Creates a new list by applying fn to all elements of xs
= map '(xs fn) (
	= ys [] ` Create empty list
	%xs x (#ys fn x) ` For each x in xs, append fn of x to ys
	ys `Return ys
)

= double 'x * x 2

` Create list of doubled values
= dbs map [1 2 3 4 5 6] @double

` Print the result, separated by newlines
%dbs d (, d;)
```

## Features
* **TINY** implementation, cloc'ing in at 500 lines of dense, unreadable C11.
	* It even compiles down to <20kb!
* **ZERO** dependencies, doesn't include a single header
* **NO** allocations, operates on a fixed memory arena
* nil, numbers, lists, environments, functions, native bindings, and symbols!
* Garbage collection
* Complete C api, with documented header
* Extremely easy to bind functionality
* Honestly not too slow for what it is


## Building
Simply build `lx.c` with any C11-compatible compiler. Since there are absolutely no dependencies, this should Just Work.
If you want to include the bundled CLI, set `LX_BUILD_CLI` in your build flags.

If you simply want to test lx out, a prebuilt CLI is provided under releases.

## The CLI
The lx cli can be launched standalone to fire up a REPL environment, where each line fed to stdin is evaluated in a persistent scope through the lifetime of the program.

Alternatively, a path can be provided as the whole argument to load that file and run it (`lx test.lx`)

When running the CLI, two additional functions are defined for convenience:
*  `cells` - returns the total number of cells available to the interpreter
* `load <path>` - loads the source file at the given path, and returns the environment created by it

## Writing and embedding lx
See `doc/` for more details, or `examples/` for examples!

## Contributing
This very much is meant to be a for-fun project, and I'm not considering any serious development here. Any PR made that doesn't really adhere to the core concept is likely to be denied.

## License
This software is distributed under the MIT license, see `LICENSE`for more details.
