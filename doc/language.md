# lx language guide

## Overview
lx is highly inspired by more minimalist dialects of lisp, although employing a different style of parsing and with some more novel concepts like indexable scopes (env). Instead of evaluating s-expressions, lx's parser greedily evaluates tokens from left to right, moving on once all arguments are sated. The order of operations might seem a alittle unintuitive due to this.

Every primitive operation in lx consists of one or two special characters. lx is a dynamic language to the core, code is evaluated as text, so keeping primitives short makes that simpler.

Memory is split into a dynamic section, called the `cell` memory, and a static section, where program code is stored. Because code is evaluated directly in lx, program memory needs to be persistent, and will eventually run out. Cell memory is garbage collected, though.

All expressions in lx **must** evaluate to some value.

## Numbers and symbols
These are the two special types of values in lx that aren't fixed-length.
Any expression that starts with a digit is considered a number.
Any expression that starts with an ASCII letter is considered a symbol. Symbols can contain underscores, but not start with them.

## Truthyness
lx does not have a direct boolean representation. 0 and `<nil>` are considered false, and everything else is true.
Logical operators return either 1 or 0.

## Understanding expressions
Since expressions in lx are greedily parsed, you don't need to group things with parentheses as much as in something like common lisp.
However, this can make it hard to grok some code at first glance, so here's a few examples:

```
+ * 10 2 - / 10 5 1      <=> (+ (* 10 2) (- (/ 10 5) 1))
+ . lst + a b . env x    <=> (+ (. lst (+ a b)) (. env x))
```

## Primitives

There are currently **32** primitive operations in lx.

* `~` - creates a `<nil>`
* `"(string)"` - captures all characters between quotes as a string
* `+ (a) (b)` - adds two numbers together 
* `- (a) (b)` - subtracts `b` from `a` 
* `/ (a) (b)` - divies `a` by `b` 
* `* (a) (b)` - multiplies two numbers together
* `_ (a)` - rounds the number `a`
* `== (a) (b)` - returns `1` if `a` and `b` are equal, otherwise `0`
* `< (a) (b)` - returns `1` if `a` is less than `b`, otherwise `0`
* `<= (a) (b)` - returns `1` if `a` is less than or equal to `b`, otherwise `0`
* `> (a) (b)` - returns `1` if `a` is greater than `b`, otherwise `0`
* `>= (a) (b)` - returns `1` if `a` is greather than or equal to `b`, otherwise `0`
* `& (a) (b)` - returns `1` if `a` and `b` are truthy, otherwise `0`
* `| (a) (b)` - returns `1` if either `a` or `b` is truthy, otherwise `0`
* `! (a)` - returns `0` if `a` is truthy, otherwise `1`
* `= sym (val)` - set the local variable `sym` to `val`
* `@ sym` - pass `sym` along without evaluating it
* `$ (val)` - return the length of `val`, or `<nil>` if it has none
* `, (val)` - print `val` to the console
* `;` - print a newline to the console
* ` - backticks are used for comments, everything until end of line is ignored

### `( ... )`
Enclosed parentheses are considered "bodies" in lx. They evaluate all encapsulated expressions in order, returning the result of the last one.

```
= x (
    = a 10
    = b 20
    + a b
)

, x;    `30
```

### `{ ... }`
Braces work much like parentheses, but instead return the scope object itself. This is called an "env".

```
= pos {
    = x 10
    = y 20
}

, (. pos x);    `10
```

### `[ ... ]`
Brackets collect all the expressions between them and put them into a new list.
```
= nums [ 1 2 3 4 5 ]
%nums n (, n , " ";)

` 1 2 3 4 5
```

### `. (obj) idx`
The period attempts to index into `obj` using `idx`, returning `<nil>` on failure.
env indices are symbols, while list indices are numbers

```
= lst [10 20 30]
= env { = x 10 }

, . list 1;       `20
, . env x;        `10
```

### `: (obj) idx (val)`
Stores `val` into `obj` at `idx` - this follows the same rules for indices as `.`.
Note that this will simply fail if out of bounds of a list, rather than extend it.

```
= env {}
, . env x;        `<nil>
: env x 10
, . env x;        `10
```

### `'(arg(s)) (body)`
Function definitions in lx follow this form, where `args` can be either a single symbol, a list of symbols in parentheses, or an empty set of parentheses. `body` is any single expression, the function returning it's result.

```
= double 'x * x 2
= add '(x y) + x y
= make_hundred '() 100
= complex_expr '(x y z) (
    = w * y z
    + x * w w
)

, double 10         ;  `20
, add 5 5           ;  `10
, make_hundred      ;  `100
, complex_expr 1 2 3;  `37
```

### `? (cond) (true) (false)`
After `cond` is evaluated, either `true` or `false` is evalued as well, and the result is the result of the `?` operator.
Both branches **must** be present, even if one is empty.

```
= x 10

? > x 5 (, "this will print";) (, "this will not";)

? < x 5 (
    , "you will never see this";
) () `Empty false branch
```

### `# (lst) (val)`
This appends `val` to the end of `lst`.

```
= x []
, $x;     `0

#x 1
#x 2
#x some_function 10 20 30

, $x;     `3
```

### `\ (lst)`
This pops the last value from `lst`, returning it

```
= x [1 2 3]
, \x;       `3
, \x;       `2
, \x;       `1
, \x;       `<nil>
```

### `^ (cond) (body)`
This is essentually a C-style while loop, where `cond` is evaluated after every execution of `body`, until it is false.

```
= i 0
^ (< i 10) (
    , i , " ";
    = i + i 1
) `1 2 3 4 5 6 7 8 9

= lst [10 20 30]
= i 0
^ (< i $lst) (
    , . lst i , " ";
    = i + i 1
) `10 20 30
```

### `% (lst) sym (body)`
Think of this as a foreach-style loop, where `sym` is bound to each element of `lst` before executing `body`

```
= lst [10 20 30]
%lst x (, x;)

`10
`20
`30
```