#include "Ventil4w.h"
#include <array>
#include <iostream>

constexpr std::string_view statusTxt[] = {"Kotol", "StudenaVoda", "Boiler", "KotolBoiler"};

Ventil4w::Ventil4w()
    :chip("2"),
    lines_in(chip.get_lines({27, 9}))
{
    lines_in.request({"bytd-ventil", gpiod::line_request::EVENT_BOTH_EDGES, 0});
}

bool Ventil4w::waitEvent()
{
    auto lswe = lines_in.event_wait(std::chrono::nanoseconds(1000000000));
    std::cout << "empty:" << lswe.empty() << "size:" << lswe.size() << "\n";
    for(int i=0; i<lswe.size(); ++i){
        auto e = lswe[i].event_read();
        std::cout << "event" << i << ": " << e.source.offset() << "/" << e.event_type << '\n';
    }
    return not lswe.empty();
}
