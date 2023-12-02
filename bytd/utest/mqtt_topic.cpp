#include <catch2/catch_test_macros.hpp>
#include <cmath>

#include "../src/OwTempSensor.h"

class PublisherMock : public IMqttPublisher
{
public:
    bool publish(const std::string&, const double, bool retain = true) override {return true;}
    bool publish(const std::string&, const int, bool retain = true) override {return true;}
    bool publish(const std::string&, const std::string&, bool retain = true) override {return true;}
    void publish_ensured(const std::string&, const std::string&) override {}
};

TEST_CASE( "Factorials are computed", "[factorial]" ) {

    std::string topic = "rb/ctrl/dev/Vetranie";
    std::string topic2 = "rb/ctrl/devx/Vetranie";

    REQUIRE(topic.substr(0,std::char_traits<char>::length(mqtt::devicesTopic)) == mqtt::devicesTopic);
    REQUIRE_FALSE(topic2.substr(0,std::char_traits<char>::length(mqtt::devicesTopic)) == mqtt::devicesTopic);

    const auto name = topic.substr(topic.find_last_of('/') + 1);
    REQUIRE( name == "Vetranie" );
}

TEST_CASE("Ow", "int values")
{
    bool called = false;
    auto mqtt = std::make_shared<PublisherMock>();
    ow::Sensor s1("s1", mqtt);
    s1.Changed().subscribe(event::subscr([&called](float){
        called = true;
    }));
    int16_t iv = -55 * 16;
    CHECK(std::isnan(s1.value()));
    s1.setValue(0x0550);
    CHECK(std::isnan(s1.value()));
    s1.setValue(iv);
    CHECK(s1.value() == -55);
    s1.setValue(0);
    CHECK(s1.value() == 0);
    iv = 125 * 16;
    s1.setValue(iv);
    CHECK(s1.value() == 125);
    iv = -1.25 * 16;
    s1.setValue(iv);
    CHECK(s1.value() == -1.25);
    s1.setValue(0x0550);
    CHECK(std::isnan(s1.value()));
    CHECK(s1.value() != std::numeric_limits<float>::quiet_NaN());
    called = false;
    s1.setValue(0x0550);
    CHECK_FALSE(called);
    CHECK(std::isnan(s1.value()));
    iv = 85.125 * 16;
    s1.setValue(iv);
    CHECK(called);
    CHECK(s1.value() == 85.125);
    CHECK(s1.value() != std::numeric_limits<float>::quiet_NaN());
    s1.setValue(0x0550);
    CHECK(s1.value() == 85);
}
