#include <tspp/builtin/fs.h>
#include <tspp/bind.h>
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
            throw Exception(e.what());
        }
    }

    FileStatus stat(const String& path) {
        FileStatus s;

        try {
            std::filesystem::file_status status = std::filesystem::status(path.c_str());
            s.type = convertToFileType(status.type());
            s.permissions = FilePermissions(status.permissions());
            s.modifiedOn = std::filesystem::last_write_time(path.c_str()).time_since_epoch().count();
            s.size = std::filesystem::file_size(path.c_str());
        } catch (const std::filesystem::filesystem_error& e) {
            throw Exception(e.what());
        }

        return s;
    }

    Array<DirEntry> readDir(const String& path) {
        Array<DirEntry> entries;

        try {
            for (const auto& entry : std::filesystem::directory_iterator(path.c_str())) {
                FileStatus s;

                s.type = convertToFileType(entry.status().type());
                s.permissions = FilePermissions(entry.status().permissions());
                s.modifiedOn = entry.last_write_time().time_since_epoch().count();
                s.size = entry.file_size();

                entries.push(DirEntry{
                    s,
                    entry.path().filename().string(),
                    entry.path().string(),
                    entry.path().extension().string()
                });
            }
        } catch (const std::filesystem::filesystem_error& e) {
            throw Exception(e.what());
        }

        return entries;
    }

    Array<u8> readFile(const String& path) {
        Array<u8> data;

        try {
            std::ifstream file(path.c_str(), std::ios::in | std::ios::binary);
            file.seekg(0, std::ios::end);
            u64 size = file.tellg();
            if (size > UINT32_MAX) throw Exception("File is too large");

            if (size == 0) return data;
            data.setSizeUnsafe((u32)size);

            file.seekg(0, std::ios::beg);
            file.read((char*)data.data(), data.size());

            return data;
        } catch (const std::exception& e) {
            throw Exception(e.what());
        }
    }

    void writeFile(const String& path, const Array<u8>& data) {
        try {
            std::ofstream file(path.c_str(), std::ios::out | std::ios::binary);
            file.write((char*)data.data(), data.size());
        } catch (const std::exception& e) {
            throw Exception(e.what());
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
        if (m_file.is_open()) m_file.close();
    }

    void BasicFileStream::write(u32 offset, const Array<u8>& data) {
        try {
            m_file.seekp(offset);
            m_file.write((char*)data.data(), data.size());
        } catch (const std::exception& e) {
            throw Exception(e.what());
        }
    }

    Array<u8> BasicFileStream::read(u32 offset, u32 size) {
        try {
            m_file.seekg(offset);
            Array<u8> data(size);
            data.setSizeUnsafe(size);
            m_file.read((char*)data.data(), size);
            return data;
        } catch (const std::exception& e) {
            throw Exception(e.what());
        }
    }

    const FileStatus& BasicFileStream::status() const {
        return m_status;
    }

    BasicFileStream* openFile(const String& path) {
        if (!exists(path)) throw Exception("File does not exist");
        return new BasicFileStream(path);
    }

    void closeFile(BasicFileStream* stream) {
        if (!stream) return;
        delete stream;
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
    }

    void bindStatus(Namespace* ns) {
        ObjectTypeBuilder<FileStatus> builder = ns->type<FileStatus>("FileStatus");
        builder.prop("type", &FileStatus::type);
        builder.prop("permissions", &FileStatus::permissions);
        builder.prop("modifiedOn", &FileStatus::modifiedOn);
        builder.prop("size", &FileStatus::size);
    }

    void bindDirEntry(Namespace* ns) {
        ObjectTypeBuilder<DirEntry> builder = ns->type<DirEntry>("DirEntry");
        builder.prop("status", &DirEntry::status);
        builder.prop("name", &DirEntry::name);
        builder.prop("path", &DirEntry::path);
        builder.prop("ext", &DirEntry::ext);
        builder.getMeta().is_trivially_constructible = 1;
    }

    void bindBasicFileStream(Namespace* ns) {
        ObjectTypeBuilder<BasicFileStream> builder = ns->type<BasicFileStream>("BasicFileStream");
        builder.ctor<const BasicFileStream&>();
        builder.dtor();
        builder.method("write", &BasicFileStream::write);
        builder.method("read", &BasicFileStream::read);
        builder.method("status", &BasicFileStream::status);
    }

    void init() {
        Namespace* ns = new Namespace("__internal:fs");
        Registry::Add(ns);

        bindFileType(ns);
        bindFilePermissions(ns);
        bindStatus(ns);
        bindDirEntry(ns);
        bindBasicFileStream(ns);

        ns->function("exists", exists);
        ns->function("stat", stat);
        ns->function("readDir", readDir);
        ns->function("readFile", readFile);
        ns->function("writeFile", writeFile);
        ns->function("openFile", openFile);
        ns->function("closeFile", closeFile);
    }
}