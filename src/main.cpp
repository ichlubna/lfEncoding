#include "arguments/arguments.hpp"
#include "decoder.h"
#include "encoder.h"

int main(int argc, char** argv)
{
	try
	{
        Arguments args(argc, argv);
	}
	catch (const std::exception &e)
	{
		std::cerr << e.what() << std::endl;
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
