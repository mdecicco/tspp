#pragma once
#include <tspp/types.h>
#include <tspp/builtin/databuffer.h>
#include <utils/String.h>
#include <utils/Array.h>

#include <fstream>

namespace tspp::builtin::fs {
    enum class FileType : u32 {
        NotFound = 0,
        Regular,
        Directory,
        Symlink,
        Block,
        Character,
        FIFO,
        Socket,
        Junction,
        Other
    };

    enum class FilePermissions : u32 {
        None = 0,
        OwnerRead = 0400,
        OwnerWrite = 0200,
        OwnerExec = 0100,
        OwnerAll = 0700,

        GroupRead = 0040,
        GroupWrite = 0020,
        GroupExec = 0010,
        GroupAll = 0070,

        OthersRead = 0004,
        OthersWrite = 0002,
        OthersExec = 0001,
        OthersAll = 0007,

        All = 0777,
        SetUid = 04000,
        SetGid = 02000,
        StickyBit = 01000,
        Mask = 07777,
        Unknown = 0xFFFF,
    };

    struct FileStatus {
        FileType type;
        FilePermissions permissions;
        u64 modifiedOn;
        u64 size;
    };

    struct DirEntry {
        FileStatus status;
        String name;
        String path;
        String ext;
    };

    class BasicFileStream {
        public:
            BasicFileStream(const String& path);
            BasicFileStream(const BasicFileStream& other);
            ~BasicFileStream();

            void write(u32 offset, const builtin::databuffer::DataBuffer& data);
            builtin::databuffer::DataBuffer read(u32 offset, u32 size);
            const FileStatus& status() const;

        protected:
            std::fstream m_file;
            String m_path;
            FileStatus m_status;
    };

    void init();
}

