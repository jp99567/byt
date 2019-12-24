
#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
#include "../bytd/opentherm_protocol.h"

TEST_CASE( "Quick check", "[main]" ) {

	CHECK( 0 == parity(0) );
	CHECK( 0 == parity(0x80000000) );
	CHECK( 0x80'00'00'01 == parity(0x00000001) );
	CHECK( 0x80'00'00'01 == parity(0x80000001) );
        CHECK( 0xF0'00'00'00 == parity(0xF0'00'00'00));
        CHECK( 0xF0'00'00'00 == parity(0x70'00'00'00));
        CHECK( 0x30'00'00'00 == parity(0x30'00'00'00));
        CHECK( 0x30'00'00'00 == parity(0xB0'00'00'00));
        CHECK( 0x00'00'00'A0 == parity(0x00'00'00'A0));
        CHECK( 0x5AAA5555 == parity(0x5AAA5555));
        CHECK( 0xAAAA5555 == parity(0xAAAA5555));
        CHECK( 0x9AAA5555 == parity(0x1AAA5555));
        CHECK( 0xFFFFFFFF == parity(0xFFFFFFFF));
}

