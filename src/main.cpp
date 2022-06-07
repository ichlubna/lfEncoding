#include "arguments/arguments.hpp"
#include "analyzer.h"

int main(int argc, char** argv)
{
	try
	{
        std::string helpString{ "LF Random Access Video CoDec\n"
                                "Proof-of-concept implementation\n"
                                "--input        Input directory with LF images (0000.png, 0001.png...)\n"
                                "               OR\n"
                                "               with encoded files\n"
                                "--output       Output directory\n"
                                "               Contains when encoding:\n"
                                "               reference.ts - reference frame\n"
                                "               offsets.lfo - file with packet positions\n"
                                "               packets.lfp - packets data\n"
                                "               classic.ts - reference encoded video using the classic pipeline\n"
                                "               Contains when decoding:\n"
                                "               Decoded PNG frame(s)\n"
                                "--factor       Float [0.0-1.0] indicating which view in 1D LF will be decoded\n"
                                //"--method [NOT FULLY IMPLEMENTED]       0 - Decodes nearest frame, 1 - Decodes two neighbors and blends them\n"
                                };
        Arguments args(argc, argv);
        if(args.printHelpIfPresent(helpString))
            return EXIT_SUCCESS;

        if(!args["--input"])
        {
            std::cout << helpString;
            return EXIT_FAILURE;
        }
        
        if(args["--factor"])
            Analyzer analyzer(args["--input"], args["--output"], args["--factor"], args["--method"]);
        else 
            Analyzer analyzer(args["--input"], args["--output"]); 
	}
	catch (const std::exception &e)
	{
		std::cerr << e.what() << std::endl;
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
