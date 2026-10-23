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

**Evan Nirjon**  


This project was collaboratively developed by Tasnim Islam and Evan Nirjon as part of our learning experience in C++ programming and game development with Raylib.

## License

No license has been specified for this repository yet. Until a license is added, the project remains subject to the default copyright protections. If you intend to allow others to use, modify, or distribute the code, consider adding an appropriate open-source license.
