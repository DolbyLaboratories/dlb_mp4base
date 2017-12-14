# dlb_mp4base v1.0.0

The Dolby MP4 streaming muxer (dlb_mp4base) is a software implementation of a demuxer of fragmented or unfragmented ISO base media file (mp4) format. It supports muxing of Dolby Digital (AC-3), Dolby Digital Plus (E-AC-3) audio formats as well as Dolby Vision.

## Getting Started

These instructions will help you get a copy of the project up and running on your local machine for development and testing purposes. 

### Folder Structure

The "dlb_mp4base" folder consists of:

- README.md         This file.
- doc/              Doxygen documentation of the dlb_mp4base.
- frontend/         MP4Muxer frontend with corresponding EMA interface as source code.
- include/          Necessary header files of the dlb_mp4base library.
- make/             Makefiles and Visual Studio projects/solutions for building the Dolby MP4 multiplexer library with frontends and test harnesses.
- src/              Contains the MP4 multiplexer source code.
- test/             Test harnesses for unit and developer system tests including test signals.

### Prerequisites

For Windows platform development, Visual Studio 2010 must be installed with SP1.

### Building instructions

#### Using the makefiles (on Linux)

    After cloning the dlb_mp4base repository to your local machine, go to the appropriate directory, depending on your machine OS and architecture, such as:
    "cd dlb_mp4base/make/mp4muxer<architecture>"

    Then build one of the following make targets:
    "make mp4muxer_release"
    "make mp4muxer_debug"

#### Using the Visual Studio Solutions(on Windows)

    From a Visual Studio 2010 command line window:
    Go to a directory of your choice
    "cd dlb_mp4base\make\mp4muxer\mp4muxer<architecture>"
    "devenv mp4muxer_2010.sln /rebuild debug/release"

## Release Notes

See the [Release Notes](ReleaseNotes.md) file for details

## License

This project is licensed under the BSD-3 License - see the [LICENSE](LICENSE) file for details


