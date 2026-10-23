# Lost_Passage
# The Lost Passage

The Lost Passage is a 2D game developed using C++ and the Raylib graphics library. This project represents our collaborative effort to explore game development, strengthen our programming skills, and gain practical experience in building interactive applications.

## About the Project

The purpose of this project is to apply fundamental C++ programming concepts to game development using Raylib. Throughout the development process, we have worked on implementing game functionality, handling user input, and rendering graphics.

This project also provides an opportunity for us to learn more about game development, problem-solving, and collaborative software development.

## Technologies Used

- C++
- Raylib 5.0
- MinGW-w64 GCC
- Visual Studio Code

## Getting Started

### Prerequisites

Before running the project, make sure you have the following installed:

- A C++ compiler compatible with Raylib
- Raylib 5.0
- Visual Studio Code or another C++ development environment

### Installation

Clone this repository to your local machine:

```bash
git clone https://github.com/YOUR-USERNAME/Lost_Passage.git
cd Lost_Passage
```

Replace `YOUR-USERNAME` with the GitHub username associated with the repository.

### Compilation

Make sure Raylib is installed and its include and library paths are correctly configured for your compiler.

For a compatible MinGW-w64 toolchain, use the following command:

```bash
g++ -std=c++17 the_lost_passage_fixed.cpp -I"raylib-5.0_win64_mingw-w64/include" -L"raylib-5.0_win64_mingw-w64/lib" -o the_lost_passage_fixed.exe -lraylib -lopengl32 -lgdi32 -lwinmm -luser32 -lshell32
```

Depending on your local setup, you may need to adjust the file names and paths.

### Running the Game

After compilation completes successfully, run the executable:

```bash
./the_lost_passage_fixed.exe
```

## Project Structure

The repository may contain the following files and directories:

```text
Lost_Passage/
├── the_lost_passage_fixed.cpp
├── README.md
└── raylib-5.0_win64_mingw-w64/
```

The actual structure depends on which files are included in the repository. If the Raylib library is installed separately, its directory may not be present here.

## Future Improvements

We intend to continue improving the project as we gain more experience in game development. Future updates may include gameplay enhancements, visual improvements, additional features, and code refinements.

## Authors

**Tasnim Islam** 
GITHUB : https://github.com/25tasnimislam

**Evan Nirjon**  
GITHUB : https://github.com/evannirjon


This project was collaboratively developed by Tasnim Islam and Evan Nirjon as part of our learning experience in C++ programming and game development with Raylib.

## License

No license has been specified for this repository yet. Until a license is added, the project remains subject to the default copyright protections. If you intend to allow others to use, modify, or distribute the code, consider adding an appropriate open-source license.

## Dependencies and Development Environment

The Lost Passage was developed using C++ and Raylib on Windows. The development environment includes MSYS2 UCRT64, MinGW-w64 GCC, and Visual Studio Code.

### Required Tools

- **C++ Compiler (GCC):** Provided through MSYS2.
- **Raylib 5.0:** The graphics library used for the game.
- **Visual Studio Code:** The code editor used during development.

### Official Download Links

- **MSYS2:** https://www.msys2.org/
- **Raylib:** https://www.raylib.com/
- **Raylib Releases:** https://github.com/raysan5/raylib/releases
- **Visual Studio Code:** https://code.visualstudio.com/
- **GCC Documentation:** https://gcc.gnu.org/onlinedocs/

### Setting Up the Compiler

Install MSYS2 from its official website. Open the MSYS2 UCRT64 terminal and install the GCC compiler by running:

```bash
pacman -S --needed mingw-w64-ucrt-x86_64-gcc
```

Verify the installation:

```bash
g++ --version
g++ -dumpmachine
```

The expected compiler target is `x86_64-w64-mingw32`.

### Setting Up Raylib

Download Raylib from the official website or its GitHub releases page. Make sure the Raylib package is compatible with the installed compiler and target architecture.

Configure the Raylib include and library paths according to your local development environment before compiling the game.

### Building and Running the Project

Open the project directory in the MSYS2 UCRT64 terminal. Compile the C++ source file using a compatible Raylib library and the appropriate include and library paths.

After successful compilation, run the generated executable to launch the game.

**Note:** The compilation command and file paths may need to be adjusted for your local setup. This project was developed in a Windows environment using MSYS2 UCRT64 and Raylib 5.0.
