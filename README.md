# tspp
Experimental library that combines [Google's v8  (13.5-lkgr)](https://github.com/v8/v8/tree/13.5-lkgr), the TypeScript
compiler, and [tsn-lang's bind API](https://github.com/tsn-lang/bind) for the purpose of using actual TypeScript as an
embedded scripting language

### Why
I have spent years working on building a compiler for a TypeScript-like language with the dream of using TypeScript's
type annotations to achieve near-native performance. I still intend to see that project through, but in the meantime I
think this could be fun. It may not be as performant as native code with no runtime type checks, but for some projects
that won't be a big deal anyway. The fact that it'll be actual TypeScript and not a knock-off is a bonus too.

### No, but _why_
I'm a TypeScript simp. I also like C++ and enabling users to extend applications without too much hacking or tooling
needed on their end. [Here](https://github.com/mdecicco/t4ext)'s an example use case of this exact concept. I've done
this idea once before, but was caught up in lots of details that are beyond the scope of this project. I didn't spend
as much time on the mechanisms that enabled scripting and binding as I could have. Many decisions were made that tied
the v8 integration and binding layer to that project specifically for the purpose of getting a usable product out quickly.


I liked how it felt and worked so much though that I wanted to do it again but better, and in such a way that I can drop
it into other projects without much thought.


### Danger
This is very much a WIP


### Okay, but how does it work?
Like this, currently
```c++
#include <tspp/tspp.h>
using namespace tspp;

class SomeObject {
    public:
        SomeObject() : a(5) {}
        SomeObject(i32 val) : a(val) {}
        ~SomeObject() { }

        void print() {
            printf("{ a: %d }\n", a);
        }

        i32 a;
};

int main() {
    Runtime rt;
    if (rt.initialize()) {
        // Namespaces become modules that can be imported
        bind::Namespace* mod = new bind::Namespace("test");
        bind::Registry::Add(mod);

        rt.commitBindings();
    
        // Data marshalling = annoying and tedius
        // Working with v8 = annoying and hopeless
        // With info provided by bind API, tspp can do both for you
        mod->function("someFunc", +[](const String& text) {
            // built-in support for internal String and Array<T> types
            // Will add built-in support for std::string and std::vector later
            printf("%s\n", text.c_str());
        });

        auto tp = mod->type<SomeObject>("SomeObject");
        tp.ctor();
        tp.ctor<i32>();
        tp.dtor();  // Destructor should be called either when the JS object is GC'd or the program
                    // terminates (untested)
        tp.prop("a", &SomeObject::a);
        tp.method("print", &SomeObject::print);
    
        // Raw JS, I haven't embedded the typescript compiler yet :(
        rt.executeString(
            "const mod = require('test');\n"
            "mod.someFunc('Hello, world.');\n"

            // Should automatically select the correct constructor, haven't tested yet though
            "const test0 = new mod.SomeObject();\n"
            "test0.print(); // { a: 5 }\n"
            "test0.a = 32;\n"
            "test0.print(); // { a: 32 }\n"

            "const test1 = new mod.SomeObject(69);\n"
            "test1.print(); // { a: 69 }\n"

            // Note: trivially constructible structs/classes don't need to be created with 'new',
            // you can simply pass a JS object with matching property names
            // unspecified properties will be initialized with zeroes and incorrect/unconvertible
            // props will cause a JS exception
        );

        rt.shutdown();
    }

    return 0;
}
```
