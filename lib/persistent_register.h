
#ifndef _LIB_PERSISTENT_REGISTER_H_
#define _LIB_PERSISTENT_REGISTER_H_

#include <cstdio>

#include <string>

// A PersistentRegister is used to read and write a string that is persisted to
// disk. It's like a database for a single string value. Here's how you might
// use it.
//
//     // Persist x to the file "x.bin".
//     PersistentRegister x("x.bin");
//
//     if (!x.Initialized()) {
//         // If x has not yet been written, write "Hello, World!".
//         x.Write("Hello, World!");
//     } else {
//         // If x has been written, read and print the value of x.
//         std::cout << x.Read() << std::endl;
//     }
//
// The first time this program is called, it will detect that x has not been
// written and will write "Hello, World!". The second time it's called, it will
// read and print "Hello, World!".
class PersistentRegister {
public:
    PersistentRegister(const std::string &filename) : filename_(filename) {}

    // Returns whether a PersistentRegister is initalized (i.e. the file into
    // which the register is persisted exists).
    bool Initialized() const;

    // Read a value from the register, or return an empty string if the
    // register is not initalized. Read panics on error.
    std::string Read() const;

    // Write a value to the register. Write panics on error.
    void Write(const std::string &s);

    // Return the filename in which the register is persisted.
    std::string Filename();

private:
    // Note that using C++ file IO, there is not really a way to ensure that
    // data has been forced to disk [1]. Thus, our implementation of
    // PersistentRegister uses C file IO so that it can use primitives like
    // fsync.
    //
    // [1]: https://stackoverflow.com/q/676787/3187068

    // `OpenFile(f, m)` calls `std::fopen(f, m)` but calls `Panic` on error.
    static std::FILE *OpenFile(const std::string &filename,
                               const std::string &mode);

    // `CloseFile(f, m)` calls `std::fclose(f)` but calls `Panic` on error.
    static void CloseFile(std::FILE *file);

    // The filename of the file that contains the persisted data.
    const std::string filename_;
};

#endif  // _LIB_PERSISTENT_REGISTER_H_
