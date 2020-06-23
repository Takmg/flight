# flight
flight is a lightweight programming language.

## summary
The source code of this program is for learning programming languages.  
Also, it's not practical because arrays is not supported and only Japanese are supported.

---

## how to use
The fastest way to call a scripting language from C++ is to see 'flight-test/flight-test.cpp' and 'flight/flight.h'.

```cpp
#include "flight.h"

void main()
{
    flight f;

    do {
        // load script file
        if(!f.load_file("script.f")){ return; }
        if(!f.load_file("script2.f")){ return; }

        // call when all scripts are loaded
        if(!f.load_complete()){ return; }
        
        // script execution
        f.exec();
    } while(false);

    // Get the error
    if (f.get_last_error() != flight_errors::error_none)
    {
      printf("%s", f.get_last_error_string().c_str());
    }
}
```

---

## script
See'flight-test/script/test.f' for a sample script.   
It can be written like C language. 

The details are described below.  

### variable

```cpp
var a;
var b = 10;
var c = a * b + 3;
var str = "string";
var str2 = str + "aaaa";
var ch = 'a';
var oct = 0123;
var hex = 0xff1;
```

### comment

```cpp
// comment
/* comment */
```

### if

```cpp
var a = 10;
if( a == 10 ){ /* execute */ }
```

### for

```cpp
for( var i = 0 ; i < 10 ; i++ ) { /* execute */ }
```

### while 

```cpp
var count = 0;
while( count < 5 ){ count += 1; }
```

### function

```cpp
func fuctorial( var value )
{
    if(value <= 1) { return 1; }
    return value * fuctorial( value - 1);
}

factorial(5);
```

### `print` function (embedded)

```cpp
print( 5 );
print( "abc" );
print( "value => " + 10 );
```

### yield

yield returns processing to host.

```
yield;
yield 2;
```

### function pointer

```cpp
func mul( var a , var b ) { return a * b; }
var fp_prt = print
var fp_mul = mul;

fp_prt( fp_mul( 2 , 3 ) );
```



---

## Addition of embedded functions

#### cpp

Call regisiter_function method.

```cpp
#include "flight.h"

void add(flight_variable& ret_var, const flight_variable& arg1, const flight_variable& arg2)
{
    ret_var.value = std::to_string(atoi(arg1.value.c_str()) + atoi(arg2.value.c_str()));
    ret_var.type = flight_data_type::FDT_VARNUMBER;
    return;
}


void main()
{
    flight f;

    do {
        // load script file
        if(!f.load_file("script.f")){ return; }
        if(!f.load_file("script2.f")){ return; }

        // call when all scripts are loaded
        if(!f.load_complete()){ return; }

        // !!! add embedded function
        if (!f.register_function("add", add)) { break; }

        // script execution
        f.exec();
```

#### script 

```
// call host function
print( "value => " + add( 1 , 2 ) );
```

---

## References

http://www.hpcs.cs.tsukuba.ac.jp/~msato/lecture-note/comp-lecture/note1.html

---

Explanation end
