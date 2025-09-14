#include <tspp/bind.h>
#include <tspp/builtin/databuffer.h>
#include <tspp/builtin/fs.h>
#include <tspp/utils/Docs.h>
#include <utils/Array.hpp>

#include <filesystem>
#include <fstream>

using namespace bind;

namespace tspp::builtin::fs {
    FileType convertToFileType(std::filesystem::file_type type) {
        switch (type) {
            case std::filesystem::file_type::regular: return FileType::Regular;
            case std::filesystem::file_type::directory: return FileType::Directory;
            case std::filesystem::file_type::symlink: return FileType::Symlink;
            case std::filesystem::file_type::block: return FileType::Block;
            case std::filesystem::file_type::character: return FileType::Character;
            case std::filesystem::file_type::fifo: return FileType::FIFO;
            case std::filesystem::file_type::socket: return FileType::Socket;
            case std::filesystem::file_type::junction: return FileType::Junction;
            default: return FileType::Other;
        }
    }

    void normalizePath(String& path) {
        std::filesystem::path p(path.c_str());
        path = p.lexically_normal().string();
    }

    bool exists(const String& path) {
        try {
            return std::filesystem::exists(path.c_str());
        } catch (const std::filesystem::filesystem_error& e) {
            throw FileException(e.what());
        }
    }

    FileStatus stat(const String& path) {
        FileStatus s;

        try {
            std::filesystem::file_status status = std::filesystem::status(path.c_str());
            s.type                              = convertToFileType(status.type());
            s.permissions                       = FilePermissions(status.permissions());
            s.modifiedOn = std::filesystem::last_write_time(path.c_str()).time_since_epoch().count();
            s.size       = std::filesystem::file_size(path.c_str());
        } catch (const std::filesystem::filesystem_error& e) {
            throw FileException(e.what());
        }

        return s;
    }

    Array<DirEntry> readDir(const String& path) {
        Array<DirEntry> entries;

        try {
            for (const auto& entry : std::filesystem::directory_iterator(path.c_str())) {
                FileStatus s;

                s.type        = convertToFileType(entry.status().type());
                s.permissions = FilePermissions(entry.status().permissions());
                s.modifiedOn  = entry.last_write_time().time_since_epoch().count();
                s.size        = entry.file_size();

                entries.push(DirEntry{
                    s, entry.path().filename().string(), entry.path().string(), entry.path().extension().string()
                });
            }
        } catch (const std::filesystem::filesystem_error& e) {
            throw FileException(e.what());
        }

        return entries;
    }

    builtin::databuffer::DataBuffer readFile(const String& path) {
        try {
            std::ifstream file(path.c_str(), std::ios::in | std::ios::binary);
            if (!file.is_open()) {
                throw FileException("File not found");
            }

            file.seekg(0, std::ios::end);
            u64 size = file.tellg();
            if (size > UINT32_MAX) {
                throw RangeException("File is too large");
            }

            if (size == 0) {
                return builtin::databuffer::DataBuffer(0);
            }

            builtin::databuffer::DataBuffer data(size);

            file.seekg(0, std::ios::beg);
            file.read((char*)data.data(), data.size());

            return data;
        } catch (const std::exception& e) {
            throw FileException(e.what());
        }
    }

    void writeFile(const String& path, const builtin::databuffer::DataBuffer& data) {
        try {
            std::ofstream file(path.c_str(), std::ios::out | std::ios::binary);
            file.write((char*)data.data(), data.size());
        } catch (const std::exception& e) {
            throw FileException(e.what());
        }
    }

    void writeFileText(const String& path, const String& text) {
        try {
            std::ofstream file(path.c_str(), std::ios::out);
            file.write(text.c_str(), text.size());
        } catch (const std::exception& e) {
            throw FileException(e.what());
        }
    }

    String readFileText(const String& path) {
        try {
            std::ifstream file(path.c_str(), std::ios::in);
            std::stringstream buffer;
            buffer << file.rdbuf();
            return buffer.str();
        } catch (const std::exception& e) {
            throw FileException(e.what());
        }
    }

    BasicFileStream::BasicFileStream(const String& path) {
        m_path = path;
        m_file.open(path.c_str(), std::ios::in | std::ios::out | std::ios::binary);
    }

    BasicFileStream::BasicFileStream(const BasicFileStream& other) {
        m_file.open(other.m_path.c_str(), std::ios::in | std::ios::out | std::ios::binary);
    }

    BasicFileStream::~BasicFileStream() {
        if (m_file.is_open()) {
            m_file.close();
        }
    }

    void BasicFileStream::write(u32 offset, const builtin::databuffer::DataBuffer& data) {
        try {
            m_file.seekp(offset);
            m_file.write((char*)data.data(), data.size());
        } catch (const std::exception& e) {
            throw FileException(e.what());
        }
    }

    builtin::databuffer::DataBuffer BasicFileStream::read(u32 offset, u32 size) {
        try {
            m_file.seekg(offset);
            builtin::databuffer::DataBuffer data(size);
            m_file.read((char*)data.data(), size);
            return data;
        } catch (const std::exception& e) {
            throw FileException(e.what());
        }
    }

    const FileStatus& BasicFileStream::status() const {
        return m_status;
    }

    BasicFileStream* openFile(const String& path) {
        if (!exists(path)) {
            throw FileException("File does not exist");
        }
        return new BasicFileStream(path);
    }

    void closeFile(BasicFileStream* stream) {
        if (!stream) {
            return;
        }
        delete stream;
    }

    String realPath(const String& path) {
        return std::filesystem::canonical(path.c_str()).string();
    }

    bool mkdir(const String& path, bool recursive) {
        if (recursive) {
            return std::filesystem::create_directories(path.c_str());
        }

        return std::filesystem::create_directory(path.c_str());
    }

    /*
     * Bindings
     */

    void bindFileType(Namespace* ns) {
        EnumTypeBuilder<FileType> builder = ns->type<FileType>("FileType");
        builder.addEnumValue("Regular", FileType::Regular);
        builder.addEnumValue("Directory", FileType::Directory);
        builder.addEnumValue("Symlink", FileType::Symlink);
        builder.addEnumValue("Block", FileType::Block);
        builder.addEnumValue("Character", FileType::Character);
        builder.addEnumValue("FIFO", FileType::FIFO);
        builder.addEnumValue("Socket", FileType::Socket);
        builder.addEnumValue("Junction", FileType::Junction);
        builder.addEnumValue("Other", FileType::Other);
        describe(builder.getType())
            .desc("Represents the type of a directory entry")
            .property("Regular", "The file is a regular file")
            .property("Directory", "The file is a directory")
            .property("Symlink", "The file is a symbolic link")
            .property("Block", "The file is a block device")
            .property("Character", "The file is a character device")
            .property("FIFO", "The file is a named pipe")
            .property("Socket", "The file is a named IPC socket")
            .property("Junction", "Implementation-defined value indicating an NT junction")
            .property("Other", "The file is of an unknown type");
    }

    void bindFilePermissions(Namespace* ns) {
        EnumTypeBuilder<FilePermissions> builder = ns->type<FilePermissions>("FilePermissions");
        builder.addEnumValue("None", FilePermissions::None);
        builder.addEnumValue("OwnerRead", FilePermissions::OwnerRead);
        builder.addEnumValue("OwnerWrite", FilePermissions::OwnerWrite);
        builder.addEnumValue("OwnerExec", FilePermissions::OwnerExec);
        builder.addEnumValue("OwnerAll", FilePermissions::OwnerAll);
        builder.addEnumValue("GroupRead", FilePermissions::GroupRead);
        builder.addEnumValue("GroupWrite", FilePermissions::GroupWrite);
        builder.addEnumValue("GroupExec", FilePermissions::GroupExec);
        builder.addEnumValue("GroupAll", FilePermissions::GroupAll);
        builder.addEnumValue("OthersRead", FilePermissions::OthersRead);
        builder.addEnumValue("OthersWrite", FilePermissions::OthersWrite);
        builder.addEnumValue("OthersExec", FilePermissions::OthersExec);
        builder.addEnumValue("OthersAll", FilePermissions::OthersAll);
        builder.addEnumValue("All", FilePermissions::All);
        builder.addEnumValue("SetUid", FilePermissions::SetUid);
        builder.addEnumValue("SetGid", FilePermissions::SetGid);
        builder.addEnumValue("StickyBit", FilePermissions::StickyBit);
        builder.addEnumValue("Mask", FilePermissions::Mask);
        builder.addEnumValue("Unknown", FilePermissions::Unknown);

        describe(builder.getType())
            .desc("Represents the permissions of a file")
            .property("None", "No permissions")
            .property("OwnerRead", "Owner has read permission")
            .property("OwnerWrite", "Owner has write permission")
            .property("OwnerExec", "Owner has execute permission")
            .property("OwnerAll", "Owner has all permissions")
            .property("GroupRead", "Group has read permission")
            .property("GroupWrite", "Group has write permission")
            .property("GroupExec", "Group has execute permission")
            .property("GroupAll", "Group has all permissions")
            .property("OthersRead", "Others have read permission")
            .property("OthersWrite", "Others have write permission")
            .property("OthersExec", "Others have execute permission")
            .property("OthersAll", "Others have all permissions")
            .property("All", "All users have all permissions")
            .property("SetUid", "Set UID bit")
            .property("SetGid", "Set GID bit")
            .property("StickyBit", "Sticky bit")
            .property("Mask", "Mask of the file's permissions")
            .property("Unknown", "Unknown permissions");
    }

    void bindStatus(Namespace* ns) {
        ObjectTypeBuilder<FileStatus> builder = ns->type<FileStatus>("FileStatus");
        builder.prop("type", &FileStatus::type);
        builder.prop("permissions", &FileStatus::permissions);
        builder.prop("modifiedOn", &FileStatus::modifiedOn);
        builder.prop("size", &FileStatus::size);
        describe(builder.getType())
            .desc("Represents the status of a file")
            .property("type", "The type of the file")
            .property("permissions", "The permissions of the file")
            .property("modifiedOn", "The time the file was last modified as a Unix timestamp")
            .property("size", "The size of the file in bytes");
    }

    void bindDirEntry(Namespace* ns) {
        ObjectTypeBuilder<DirEntry> builder = ns->type<DirEntry>("DirEntry");
        builder.prop("status", &DirEntry::status);
        builder.prop("name", &DirEntry::name);
        builder.prop("path", &DirEntry::path);
        builder.prop("ext", &DirEntry::ext);
        builder.getMeta().is_trivially_constructible = 1;

        describe(builder.getType())
            .desc("Represents an entry in a directory")
            .property("status", "The status of the directory entry")
            .property("name", "The name of the directory entry")
            .property("path", "The path of the directory entry")
            .property("ext", "The extension of the directory entry, if any");
    }

    void bindBasicFileStream(Namespace* ns) {
        ObjectTypeBuilder<BasicFileStream> builder = ns->type<BasicFileStream>("BasicFileStream");
        describe(builder.ctor<const BasicFileStream&>())
            .desc("Creates a new BasicFileStream by copying another")
            .param(0, "other", "The BasicFileStream to copy");

        builder.dtor();

        describe(builder.method("write", &BasicFileStream::write))
            .desc("Writes data to the file stream")
            .param(0, "offset", "The offset to write to in bytes")
            .param(1, "data", "The data to write");

        describe(builder.method("read", &BasicFileStream::read))
            .desc("Reads data from the file stream")
            .param(0, "offset", "The offset to read from in bytes")
            .param(1, "size", "The size of the data to read in bytes")
            .returns("The data read from the file stream", false);

        describe(builder.method("status", &BasicFileStream::status))
            .desc("Gets the status of the file stream")
            .returns("The status of the file stream", false);
    }

    void init() {
        Namespace* ns = new Namespace("__internal:fs");
        Registry::Add(ns);

        bindFileType(ns);
        bindFilePermissions(ns);
        bindStatus(ns);
        bindDirEntry(ns);
        bindBasicFileStream(ns);

        describe(ns->function("existsSync", exists))
            .desc("Synchronously checks if a file or directory exists")
            .param(0, "path", "The path to check")
            .returns("true if the file or directory exists, false otherwise", false);

        describe(ns->function("exists", exists))
            .desc("Asynchronously checks if a file or directory exists")
            .param(0, "path", "The path to check")
            .returns("true if the file or directory exists, false otherwise", false)
            .async();

        describe(ns->function("statSync", stat))
            .desc("Synchronously gets the status of a file or directory")
            .param(0, "path", "The path to check")
            .returns("The status of the file or directory", false);

        describe(ns->function("stat", stat))
            .desc("Asynchronously gets the status of a file or directory")
            .param(0, "path", "The path to check")
            .returns("The status of the file or directory", false)
            .async();

        describe(ns->function("readDirSync", readDir))
            .desc("Synchronously reads the contents of a directory")
            .param(0, "path", "The path to read")
            .returns("An array of DirEntry objects", false);

        describe(ns->function("readDir", readDir))
            .desc("Asynchronously reads the contents of a directory")
            .param(0, "path", "The path to read")
            .returns("An array of DirEntry objects", false)
            .async();

        describe(ns->function("readFileSync", readFile))
            .desc("Synchronously reads the contents of a file")
            .param(0, "path", "The path to read")
            .returns("The contents of the file as an ArrayBuffer", false);

        describe(ns->function("readFile", readFile))
            .desc("Asynchronously reads the contents of a file")
            .param(0, "path", "The path to read")
            .returns("The contents of the file as an ArrayBuffer", false)
            .async();

        describe(ns->function("readFileTextSync", readFileText))
            .desc("Synchronously reads the contents of a file as a UTF-8 string")
            .param(0, "path", "The path to read")
            .returns("The contents of the file as a UTF-8 string", false);

        describe(ns->function("readFileText", readFileText))
            .desc("Asynchronously reads the contents of a file as a UTF-8 string")
            .param(0, "path", "The path to read")
            .returns("The contents of the file as a UTF-8 string", false)
            .async();

        describe(ns->function("writeFileSync", writeFile))
            .desc("Synchronously writes data to a file")
            .param(0, "path", "The path to write to")
            .param(1, "data", "The data to write");

        describe(ns->function("writeFile", writeFile))
            .desc("Asynchronously writes data to a file")
            .param(0, "path", "The path to write to")
            .param(1, "data", "The data to write")
            .async();

        describe(ns->function("writeFileTextSync", writeFileText))
            .desc("Synchronously writes a UTF-8 string to a file")
            .param(0, "path", "The path to write to")
            .param(1, "text", "The UTF-8 string to write");

        describe(ns->function("writeFileText", writeFileText))
            .desc("Asynchronously writes a UTF-8 string to a file")
            .param(0, "path", "The path to write to")
            .param(1, "text", "The UTF-8 string to write")
            .async();

        describe(ns->function("mkdirSync", mkdir))
            .desc("Synchronously creates a directory")
            .param(0, "path", "The path to create")
            .param(1, "recursive", "Whether to create the directory recursively");

        describe(ns->function("mkdir", mkdir))
            .desc("Asynchronously creates a directory")
            .param(0, "path", "The path to create")
            .param(1, "recursive", "Whether to create the directory recursively")
            .async();

        describe(ns->function("openFile", openFile))
            .desc("Opens a file for reading and writing")
            .param(0, "path", "The path to open")
            .returns("A BasicFileStream object", false);

        describe(ns->function("closeFile", closeFile))
            .desc("Closes a BasicFileStream")
            .param(0, "stream", "The BasicFileStream to close");

        describe(ns->function("realPath", realPath))
            .desc("Gets the canonical pathname of a file or directory")
            .param(0, "path", "The path to get the canonical pathname of")
            .returns("The canonical pathname of the file or directory", false);
    }
}