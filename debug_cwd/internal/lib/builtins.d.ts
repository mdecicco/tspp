interface IntrinsicNumber<
    bits extends number,
    is_signed extends boolean,
    is_floating_point extends boolean
> extends Number {}
/** -128 */
declare const I8_MIN: number = -128;
/** 127 */
declare const I8_MAX: number = 127;
/** -32768 */
declare const I16_MIN: number = -32768;
/** 32767 */
declare const I16_MAX: number = 32767;
/** -2147483648 */
declare const I32_MIN: number = -2147483648;
/** 2147483647 */
declare const I32_MAX: number = 2147483647;
/** -9223372036854775808 */
declare const I64_MIN: number = -9223372036854775808;
/** 9223372036854775807 */
declare const I64_MAX: number = 9223372036854775807;
/** 0 */
declare const U8_MIN: number = 0;
/** 255 */
declare const U8_MAX: number = 255;
/** 0 */
declare const U16_MIN: number = 0;
/** 65535 */
declare const U16_MAX: number = 65535;
/** 0 */
declare const U32_MIN: number = 0;
/** -1 */
declare const U32_MAX: number = -1;
/** 0 */
declare const U64_MIN: number = 0;
/** 18446744073709551615 */
declare const U64_MAX: number = 18446744073709551615;
/** 1.175494351e-38 */
declare const F32_MIN: number = 1.175494351e-38;
/** 3.402823466e+38 */
declare const F32_MAX: number = 3.402823466e+38;
/** 2.2250738585072014e-308 */
declare const F64_MIN: number = 2.2250738585072014e-308;
/** 1.7976931348623158e+308 */
declare const F64_MAX: number = 1.7976931348623158e+308;
/**
 * 8-bit signed integer (min: -128, max: 127)
 */
declare type i8 = number & IntrinsicNumber<8, true, false>;
/**
 * 16-bit signed integer (min: -32768, max: 32767)
 */
declare type i16 = number & IntrinsicNumber<16, true, false>;
/**
 * 32-bit signed integer (min: -2147483648, max: 2147483647)
 */
declare type i32 = number & IntrinsicNumber<32, true, false>;
/**
 * 64-bit signed integer (min: -9223372036854775808, max: 9223372036854775807)
 */
declare type i64 = number & IntrinsicNumber<64, true, false>;
/**
 * 8-bit unsigned integer (min: 0, max: 255)
 */
declare type u8 = number & IntrinsicNumber<8, false, false>;
/**
 * 16-bit unsigned integer (min: 0, max: 65535)
 */
declare type u16 = number & IntrinsicNumber<16, false, false>;
/**
 * 32-bit unsigned integer (min: 0, max: -1)
 */
declare type u32 = number & IntrinsicNumber<32, false, false>;
/**
 * 64-bit unsigned integer (min: 0, max: 18446744073709551615)
 */
declare type u64 = number & IntrinsicNumber<64, false, false>;
/**
 * 32-bit floating point number (min: 1.175494351e-38, max: 3.402823466e+38)
 */
declare type f32 = number & IntrinsicNumber<32, true, true>;
/**
 * 64-bit floating point number (min: 2.2250738585072014e-308, max: 1.7976931348623158e+308)
 */
declare type f64 = number & IntrinsicNumber<64, true, true>;
declare module "__internal:buffer" {
    /**
     * Decodes an ArrayBuffer as a UTF-8 string
     * @param buffer The ArrayBuffer to decode
     * @return The decoded UTF-8 string
     */
    declare function decodeUTF8(buffer: ArrayBuffer): string;
}
declare module "__internal:fs" {
    /**
     * Represents the type of a directory entry
     */
    declare enum FileType {
        /**
         * The file is a regular file
         */
        Regular = 1,
        /**
         * The file is a directory
         */
        Directory = 2,
        /**
         * The file is a symbolic link
         */
        Symlink = 3,
        /**
         * The file is a block device
         */
        Block = 4,
        /**
         * The file is a character device
         */
        Character = 5,
        /**
         * The file is a named pipe
         */
        FIFO = 6,
        /**
         * The file is a named IPC socket
         */
        Socket = 7,
        /**
         * Implementation-defined value indicating an NT junction
         */
        Junction = 8,
        /**
         * The file is of an unknown type
         */
        Other = 9
    }
    /**
     * Represents the permissions of a file
     */
    declare enum FilePermissions {
        /**
         * No permissions
         */
        None = 0,
        /**
         * Owner has read permission
         */
        OwnerRead = 256,
        /**
         * Owner has write permission
         */
        OwnerWrite = 128,
        /**
         * Owner has execute permission
         */
        OwnerExec = 64,
        /**
         * Owner has all permissions
         */
        OwnerAll = 448,
        /**
         * Group has read permission
         */
        GroupRead = 32,
        /**
         * Group has write permission
         */
        GroupWrite = 16,
        /**
         * Group has execute permission
         */
        GroupExec = 8,
        /**
         * Group has all permissions
         */
        GroupAll = 56,
        /**
         * Others have read permission
         */
        OthersRead = 4,
        /**
         * Others have write permission
         */
        OthersWrite = 2,
        /**
         * Others have execute permission
         */
        OthersExec = 1,
        /**
         * Others have all permissions
         */
        OthersAll = 7,
        /**
         * All users have all permissions
         */
        All = 511,
        /**
         * Set UID bit
         */
        SetUid = 2048,
        /**
         * Set GID bit
         */
        SetGid = 1024,
        /**
         * Sticky bit
         */
        StickyBit = 512,
        /**
         * Mask of the file's permissions
         */
        Mask = 4095,
        /**
         * Unknown permissions
         */
        Unknown = 65535
    }
    /**
     * Represents the status of a file
     */
    declare type FileStatus = {
        /**
         * The type of the file
         */
        type: FileType;
        /**
         * The permissions of the file
         */
        permissions: FilePermissions;
        /**
         * The time the file was last modified as a Unix timestamp
         */
        modifiedOn: u64;
        /**
         * The size of the file in bytes
         */
        size: u64;
    }
    /**
     * Represents an entry in a directory
     */
    declare type DirEntry = {
        /**
         * The status of the directory entry
         */
        status: FileStatus;
        /**
         * The name of the directory entry
         */
        name: string;
        /**
         * The path of the directory entry
         */
        path: string;
        /**
         * The extension of the directory entry, if any
         */
        ext: string;
    }
    declare class BasicFileStream {
        /**
         * Creates a new BasicFileStream by copying another
         * @param other The BasicFileStream to copy
         */
        constructor(other: BasicFileStream);
        /**
         * Writes data to the file stream
         * @param offset The offset to write to in bytes
         * @param data The data to write
         */
        write(offset: u32, data: ArrayBuffer): void;
        /**
         * Reads data from the file stream
         * @param offset The offset to read from in bytes
         * @param size The size of the data to read in bytes
         * @return The data read from the file stream
         */
        read(offset: u32, size: u32): ArrayBuffer;
        /**
         * Gets the status of the file stream
         * @return The status of the file stream
         */
        status(): FileStatus;
    }
    /**
     * Synchronously checks if a file or directory exists
     * @param path The path to check
     * @return true if the file or directory exists, false otherwise
     */
    declare function existsSync(path: string): boolean;
    /**
     * Asynchronously checks if a file or directory exists
     * @param path The path to check
     * @return true if the file or directory exists, false otherwise
     */
    declare function exists(path: string): Promise<boolean>;
    /**
     * Synchronously gets the status of a file or directory
     * @param path The path to check
     * @return The status of the file or directory
     */
    declare function statSync(path: string): FileStatus;
    /**
     * Asynchronously gets the status of a file or directory
     * @param path The path to check
     * @return The status of the file or directory
     */
    declare function stat(path: string): Promise<FileStatus>;
    /**
     * Synchronously reads the contents of a directory
     * @param path The path to read
     * @return An array of DirEntry objects
     */
    declare function readDirSync(path: string): DirEntry[];
    /**
     * Asynchronously reads the contents of a directory
     * @param path The path to read
     * @return An array of DirEntry objects
     */
    declare function readDir(path: string): Promise<DirEntry[]>;
    /**
     * Synchronously reads the contents of a file
     * @param path The path to read
     * @return The contents of the file as an ArrayBuffer
     */
    declare function readFileSync(path: string): ArrayBuffer;
    /**
     * Asynchronously reads the contents of a file
     * @param path The path to read
     * @return The contents of the file as an ArrayBuffer
     */
    declare function readFile(path: string): Promise<ArrayBuffer>;
    /**
     * Synchronously reads the contents of a file as a UTF-8 string
     * @param path The path to read
     * @return The contents of the file as a UTF-8 string
     */
    declare function readFileTextSync(path: string): string;
    /**
     * Asynchronously reads the contents of a file as a UTF-8 string
     * @param path The path to read
     * @return The contents of the file as a UTF-8 string
     */
    declare function readFileText(path: string): Promise<string>;
    /**
     * Synchronously writes data to a file
     * @param path The path to write to
     * @param data The data to write
     */
    declare function writeFileSync(path: string, data: ArrayBuffer): void;
    /**
     * Asynchronously writes data to a file
     * @param path The path to write to
     * @param data The data to write
     */
    declare function writeFile(path: string, data: ArrayBuffer): Promise<void>;
    /**
     * Synchronously writes a UTF-8 string to a file
     * @param path The path to write to
     * @param text The UTF-8 string to write
     */
    declare function writeFileTextSync(path: string, text: string): void;
    /**
     * Asynchronously writes a UTF-8 string to a file
     * @param path The path to write to
     * @param text The UTF-8 string to write
     */
    declare function writeFileText(path: string, text: string): Promise<void>;
    /**
     * Synchronously creates a directory
     * @param path The path to create
     * @param recursive Whether to create the directory recursively
     */
    declare function mkdirSync(path: string, recursive: boolean): boolean;
    /**
     * Asynchronously creates a directory
     * @param path The path to create
     * @param recursive Whether to create the directory recursively
     */
    declare function mkdir(path: string, recursive: boolean): Promise<boolean>;
    /**
     * Opens a file for reading and writing
     * @param path The path to open
     * @return A BasicFileStream object
     */
    declare function openFile(path: string): BasicFileStream;
    /**
     * Closes a BasicFileStream
     * @param stream The BasicFileStream to close
     */
    declare function closeFile(stream: BasicFileStream): void;
    /**
     * Gets the canonical pathname of a file or directory
     * @param path The path to get the canonical pathname of
     * @return The canonical pathname of the file or directory
     */
    declare function realPath(path: string): string;
}
declare module "process" {
    /**
     * Gets the current working directory
     * @return The current working directory
     */
    declare function cwd(): string;
    declare const os: string;
    /**
     * String map of environment variables available when the process was started
     */
    declare const env: { readonly [key: string]: string };
}
declare module "path" {
    /**
     * Checks if a path is an absolute path
     * @param path The path to check
     * @return True if the path is an absolute path, false otherwise
     */
    declare function isAbsolute(path: string): boolean;
    /**
     * Normalizes a path
     * @param path The path to normalize
     * @return The normalized path
     */
    declare function normalize(path: string): string;
    /**
     * Gets the directory name of a path
     * @param path The path to get the directory name of
     * @return The directory name of the path
     */
    declare function dirname(path: string): string;
}
