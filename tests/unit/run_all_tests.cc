#include <cppunit/extensions/TestFactoryRegistry.h>
#include <cppunit/ui/text/TestRunner.h>

using CppUnit::TestFactoryRegistry;

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

const char NOSLOW_ARG[] = "noslow";

void usage(char *argv[])
{
    fprintf(stderr, "Usage: %s [%s]\n",
            argv[0], NOSLOW_ARG);
    exit(EXIT_FAILURE);
}

int main(int argc, char *argv[])
{
    CppUnit::TextUi::TestRunner runner;
    runner.addTest(TestFactoryRegistry::getRegistry().makeTest());

    if (argc == 1 || strcmp(argv[1], NOSLOW_ARG) != 0) {
        runner.addTest(TestFactoryRegistry::getRegistry("slow").makeTest());
    }
    
    runner.run();
    return 0;
}
