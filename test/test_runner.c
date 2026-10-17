#include "test_message.h"

GREATEST_MAIN_DEFS();

int main(int argc, char **argv) {
    // setup
    GREATEST_MAIN_BEGIN();

    // run suite
    RUN_SUITE(suite_message_serde);

    // results
    GREATEST_MAIN_END();
}