#pragma once

#include<string>

namespace kurenie {

enum class ROOM { Obyvka, Spalna, Kuchyna, Izba, Kupelka, Podlahovka, _last };

constexpr const char* roomTxt(ROOM room);
std::string roomTopic(ROOM room);
ROOM txtToRoom(std::string name);

} // namespace kurenie
