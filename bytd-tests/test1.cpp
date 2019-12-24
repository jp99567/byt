
#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
#include "../bytd/opentherm_protocol.h"

TEST_CASE( "Quick check", "[main]" ) {

	REQUIRE( 0 == parity(0) );
	REQUIRE( 0 == parity(0x80000000) );
	REQUIRE( 0x80'00'00'01 == parity(0x00000001) );
	REQUIRE( 0x80'00'00'01 == parity(0x80000001) );
}

