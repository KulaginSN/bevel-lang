# Bevel Programming Language

**Bevel** is a language that combines the syntax of C and Python.
This language is just an experiment. The compiler was written using Qwen AI in pure C. Currently, the language does not work fully. The compiler does not compile most constructs.
The language was conceived as very C-like, but with additional features. Bevel lacks classical OOP; instead, it has interfaces, methods, and structs.

I deliberately abandoned some operators and names, returning to C syntax.

I got carried away with developing geometric kernels — C++ seemed too complex to me, and C was not well-suited for this task, so I wanted something of my own.

Here you won't find syntax like:

```
fn main() -> i32 {
    let a, b = 5;
    return a + b;
}
```

Instead, I decided to do it like this:

```
i32 main():
    i32 a, b = 5;
    return a + b;
```
Whether this is good or bad, you decide for yourselves. I was just experimenting. Maybe someone will like this language and want to develop it further. I will only work on it in my free time.

Missing:
- import

Not fully implemented:
- Generics
- something else, but I don't remember anymore.

The language compiles to C code. Therefore, after compilation, you need to compile it separately using a C compiler.

## How to Run
Just run the following commands:
```
$ git clone https://github.com/KulaginSN/bevel-lang
$ cd bevel-lang
$ make
```

If you need to clean and rebuild the project, run the following command:
```
$ make clean && make && clear
```

## Examples
In the tests/ folder you will find syntax examples — maybe you'll like them.

Functions:

```
i32 add(i32 a, i32 b):
    return a + b;

i32 main():
    return add(10, 20);
```

Loops:
```
i32 main():
    i32 sum = 0;
    for (i32 i = 0; i < 5; i = i + 1):
        sum = sum + i;
    return sum;

---

int main():
    i32 i = 0;
    i32 sum = 0;
    
    while (i < 10):
        i = i + 1;
        if (i == 5):
            continue;
        if (i == 8):
            break;
        sum = sum + i;
        
    return sum;
```
Structs and enums:
```
struct Point:
    i32 x;
    i32 y;

i32 main():
    Point p;
    *Point ptr = &p;
    ptr.x = 100;
    return p.x;

---

enum i32 Status:
    Ok = 0;
    Err = 1;

i32 main():
    i32 s = Status.Ok;
    return s;
```

Interfaces and methods:
```
interface Shape:
    f64 area(self);

struct Circle:
    f64 radius;

methods Circle:
    f64 area(self):
        return 3.14 * self.radius * self.radius;

i32 main():
    Circle c;
    c.radius = 5.0;
    Shape s = c;
    f64 a = s.area();
    if (a > 78.0):
        return 1;
    return 0;

```

Generics:
```
generic(T)
T max_val(T a, T b):
    if (a > b):
        return a;
    return b;

i32 main():
    i32 x = max_val(i32, 5, 10);
    if (x == 10):
        return 1;
    return 0;

---

interface Printable:
    void print(self);

generic(T) where T: Printable
void show(T item):
    item.print();

struct Number:
    i32 value;

methods Number:
    void print(self):
        return;

i32 main():
    Number n;
    n.value = 42;
    show(n);
    return 1;

---

generic(T, E)
struct Result:
    T value;
    E error;
    bool is_ok;

i32 main():
    Result(f64, string) res1;
    Result(i32, i32) res2;
    
    res1.value = 3.14;
    res1.is_ok = true;
    
    res2.value = 42;
    res2.error = 0;
    res2.is_ok = true;
    
    if (res1.is_ok && res2.is_ok):
        return 1;
        
    return 0;
```

To compile some Bevel code and run it, you need to enter the following commands:

```
cd bevel-lang
./bevel_test [FILE_NAME.bv]
gcc [FILE_NAME].c -o program
./program
```

There are debug messages here, so don't pay attention to them.

## Development Plans
1. Bug fixes
2. Bringing the compiler to a working state.
3. A package manager bforge (like cargo).
4. Standard Library.
5. Memory management via arena and allocators, as well as separate ownership.

## Project Status

The project is probably not even at version 0.1 yet.
A lot needs to be reworked and functionality added.
If you want to contribute, write to me — I'll add you to the contributors.
