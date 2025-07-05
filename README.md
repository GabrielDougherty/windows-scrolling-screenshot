
# Native Scrolling Screenshot

A Windows application for capturing scrolling screenshots using native Windows APIs and OpenCV for image stitching.

## Prerequisites

- Visual Studio 2022 Community (or higher)
- vcpkg package manager
- OpenCV 4 (automatically installed via vcpkg)

## Building in Visual Studio Code

### 1. Open the Project
Open the project folder in VS Code.

### 2. Build the Project
Use one of these methods to build:

**Method A: Use VS Code Build Task (Recommended)**
- Press `Ctrl+Shift+P` to open the command palette
- Type "Tasks: Run Build Task" and select it
- Choose the "build" task
- This will automatically:
  - Install vcpkg dependencies (OpenCV)
  - Build the project using MSBuild
  - Copy required DLLs to the output directory

**Method B: Use Keyboard Shortcut**
- Press `Ctrl+Shift+B` to run the default build task

**Method C: Use Terminal**
- Open a terminal in VS Code (`Ctrl+``)
- Run: `.\build_project.bat`

### 3. Run/Debug the Application
- Press `F5` to start debugging, or
- Press `Ctrl+F5` to run without debugging
- Choose "(Windows) Launch" from the debug configuration dropdown

### Build Configuration
The project is configured to build in Debug mode for x64 architecture by default. The build process includes:

1. **Dependency Installation**: vcpkg automatically installs OpenCV 4 with contrib features
2. **Compilation**: MSBuild compiles the C++ source files
3. **DLL Copying**: Required OpenCV DLLs are copied to the output directory
4. **Ready to Run**: The executable is ready to launch with all dependencies

### Troubleshooting
- If you get DLL missing errors, run the build task again to ensure all dependencies are copied
- Make sure Visual Studio 2022 is installed with C++ development tools
- Ensure vcpkg is properly installed and integrated

## Manual Installation Commands

If you need to manually install dependencies:

```
vcpkg install opencv4[contrib] --triplet x64-windows
```