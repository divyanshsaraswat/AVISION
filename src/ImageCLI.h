#pragma once

#include <string>
#include <vector>

class ImageCLI {
public:
    static int run(int argc, char** argv);

private:
    static int handleSegment(int argc, char** argv);
    static int handleFill(int argc, char** argv);
    static void printHelp();
};
