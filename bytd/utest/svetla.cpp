#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "../src/candata.h"

class OutputControlMock : public can::IOutputControl
{
public:
    void update(std::size_t idx, can::IOutputItem& item) override
    {
        ++calls;
        lastIdx = idx;
        lastItem = &item;
    }

    std::size_t calls = 0;
    std::size_t lastIdx = 0;
    can::IOutputItem* lastItem = nullptr;
};

TEST_CASE("DaliConv zero and negative", "[dali]")
{
    DaliConv conv(255);
    CHECK(conv.conv(0) == 0);
    CHECK(conv.conv(-1) == 0);
}

TEST_CASE("DaliConv monotonic growth", "[dali]")
{
    DaliConv conv(12345);
    const double v1 = conv.conv(1);
    const double v10 = conv.conv(10);
    const double v100 = conv.conv(100);
    CHECK(v1 > 0);
    CHECK(v1 < v10);
    CHECK(v10 < v100);
    CHECK(v100 == Catch::Approx(12345));
    CHECK(0 == conv.conv(0));
}

TEST_CASE("DaliConv top scaling", "[dali]")
{
    const double vin = 50;
    DaliConv conv255(255);
    DaliConv conv510(510);
    const double v255 = conv255.conv(vin);
    const double v510 = conv510.conv(vin);
    CHECK(v510 == Catch::Approx(v255 * 2.0));
}

TEST_CASE("CanPwm16Out", "[can]")
{
    CanPwm16Out out(12345);
    auto outControl = std::make_shared<OutputControlMock>();
    can::Data data;
    out.getCanItem().offset = 0;
    out.getCanItem().busOutControl = outControl;
    CHECK(out(100) == 100);
    out.getCanItem().update(data);
    CHECK(*reinterpret_cast<uint16_t*>(&data[0]) == 12345);
    CHECK(out(0) == 0);
    out.getCanItem().update(data);
    CHECK(*reinterpret_cast<uint16_t*>(&data[0]) == 0);
}
