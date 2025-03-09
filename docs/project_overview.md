# TypeScript Embedded Runtime

An embeddable TypeScript runtime for C++ applications, designed primarily for video games and hobby projects.

## Project Goals

- Create a lightweight, embeddable TypeScript/JavaScript runtime
- Integrate the TypeScript compiler to natively support executing TypeScript code
- Provide easy API binding between host C++ applications and the script environment
- Generate TypeScript definitions (`globals.d.ts`) automatically from bound APIs
- Support configurable features like file watching, compilation caching, and bytecode caching

## Key Components

### Core Engine

- V8 JavaScript engine integration
- TypeScript compiler embedded as bytecode/minified JavaScript
- Module loading and execution system
- Caching mechanisms for compiled TypeScript and V8 bytecode

### Threading Model

- Script execution in a dedicated thread
- Synchronization points with the host application
- Event-based communication between host and script environment
- Support for performance-critical operations (audio synthesis, rendering, etc.)

### Host Integration

- Custom "bind" API for exposing C++ functionality to scripts
- Callback mechanisms for scripts to trigger host functionality
- Efficient data sharing between host and script environments

### Built-in Libraries

- Minimal implementation of common Node.js-like APIs
- File system operations
- Path manipulation
- Timers and scheduling
- Console output (redirected to host application)

## Technical Considerations

- TypeScript compiler will be embedded directly rather than executed as a separate process
- Module system will balance simplicity of implementation with developer experience
- Performance is critical for real-time applications
- APIs will maintain an asynchronous nature similar to Node.js where appropriate

## Use Cases

- Game scripting and modding
- Application extensibility
- Real-time audio and visual processing
- Hobby projects requiring a scripting layer
