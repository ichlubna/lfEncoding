#include "arguments/arguments.hpp"
#include "analyzer.h"

int main(int argc, char** argv)
{
	try
	{
        std::string helpString{ "LF Random Access Video CoDec\n"
                                "Proof-of-concept implementation supporting only 1D row light field\n"
                                "--input        Input directory with LF images (0000.png, 0001.png...)\n"
                                "               OR\n"
                                "               with encoded files\n"
                                "--output       Output directory\n"
                                "--viewFactor   Float [0.0-1.0] indicating which view in 1D LF will be decoded\n"
                                };
        Arguments args(argc, argv);
        args.printHelpIfPresent(helpString);
        Analyzer analyzer(args["--input"], args["--output"]); 
	}
	catch (const std::exception &e)
	{
		std::cerr << e.what() << std::endl;
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
