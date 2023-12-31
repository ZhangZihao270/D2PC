
#include "lib/persistent_register.h"

#include <cstdio>
#include <cstring>
#include <unistd.h>

#include <fstream>
#include <memory>

#include "lib/message.h"

bool PersistentRegister::Initialized() const
{
    // Check to see if the file exists. If it doesn't, then we default to
    // returning an empty string. Refer to [1] for some ways to check if a file
    // exists in C++.
    //
    // [1]: https://stackoverflow.com/a/12774387/3187068
    std::ifstream f(filename_.c_str());
    return f.good();
}

std::string PersistentRegister::Read() const
{
    if (!Initialized()) {
        return "";
    }

    std::FILE *file = OpenFile(filename_, "rb");

    // Seek to the end of the file and get it's size.
    int success = std::fseek(file, 0, SEEK_END);
    if (success != 0) {
        Panic("Unable to fseek file %s", filename_.c_str());
    }
    long length = ftell(file);
    if (length == -1) {
        Panic("%s", std::strerror(errno));
    }

    // Seek back to the beginning of the file and read its contents. Now that
    // we know the size, we can allocate an appropriately sized buffer.
    success = std::fseek(file, 0, SEEK_SET);
    if (success != 0) {
        Panic("Unable to fseek file %s", filename_.c_str());
    }
    std::unique_ptr<char[]> buffer(new char[length]);
    std::size_t num_read = std::fread(buffer.get(), length, 1, file);
    if (num_read != 1) {
        Panic("Unable to read file %s", filename_.c_str());
    }

    CloseFile(file);
    return std::string(buffer.get(), length);
}

void PersistentRegister::Write(const std::string &s)
{
    // Perform the write.
    std::FILE *file = OpenFile(filename_, "wb");
    std::size_t num_written =
        std::fwrite(s.c_str(), sizeof(char), s.size(), file);
    if (num_written != s.size()) {
        Panic("Unable to write to file %s", filename_.c_str());
    }

    // Persist the write.
    int fd = fileno(file);
    if (fd == -1) {
        Panic("%s", std::strerror(errno));
    }
    int success = fsync(fd);
    if (success != 0) {
        Panic("%s", std::strerror(errno));
    }

    CloseFile(file);
}

std::string PersistentRegister::Filename() { return filename_; }

std::FILE *PersistentRegister::OpenFile(const std::string &filename,
                                        const std::string &mode)
{
    std::FILE *file = std::fopen(filename.c_str(), mode.c_str());
    if (file == nullptr) {
        Panic("%s", std::strerror(errno));
    }
    return file;
}

void PersistentRegister::CloseFile(std::FILE *file)
{
    int success = std::fclose(file);
    if (success != 0) {
        Panic("Unable to close file.");
    }
}
