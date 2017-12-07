# dlb_mp4base

The dlb_mp4base is a library which  developed by Dolby under the BSD license. The library can cooperates with front-end program to form a tool to efficiently mux Dolby Technologies(including AC3, EC3 and DoVi ) into ISO base media file format (aka mp4 container).

## Getting Started

These instructions will get you a copy of the project up and running on your local machine for development and testing purposes. See deployment for notes on how to deploy the project on a live system.

### Folder Structure

The "dlb_mp4base" folder comprises:

- README.md         This file
- doc/                       Doxygen documentation of dlb_mp4base (planned)
- frontend/               MP4Muxer frontend with corresponding "EMA" interface as source code
- include/                 necessary header files of dlb_mp4base
- make/                    Makefiles and Visual Studio projects/solutions for building the Dolby MP4 Muxer lib with frontends and test harnesses.
- src/                        Contains the MP4 Muxer source code
- test/                       Test harnesses for unit and developer system tests including belonging test signals.

### Prerequisites

For Windows platfrom development, Visual Studio 10 should be installed with SP1.

### Building instructions

#### Using make (on Linux)

    After cloning dlb_mp4base repo to local, go to the appropriate directory, dependent on your machine OS and architecture:
    "cd dlb_mp4base/make/mp4muxer<architecture>"

    Then build one of the following make targets
    "make mp4muxer_release"
    "make mp4muxer_debug"

    In the above, mp4author can be one of the following
    "mp4muxer"
    "libmp4base"
    "utils_test"

#### Using Visual Studio Solution(on	Windows)

    From a VS2010 CMD window:
    Go to a directory of your choice
    "cd dlb_mp4base\make\mp4muxer\windows_amd64"
    "devenv mp4muxer_2010.sln /rebuild debug/release"


## License

This project is licensed under the BSD License - see the [LICENSE](LICENSE.md) file for details


