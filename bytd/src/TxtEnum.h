#pragma once

#include<string>

using ust = std::size_t;

namespace kurenie {

enum class ROOM { Obyvka, Spalna, Kuchyna, Izba, Kupelka, Podlahovka, _last };

constexpr const char *roomTxt(ROOM room)
{
  switch (room) {
  case ROOM::Obyvka:
    return "Obyvka";
  case ROOM::Spalna:
    return "Spalna";
  case ROOM::Kuchyna:
    return "Kuchyna";
  case ROOM::Izba:
    return "Izba";
  case ROOM::Kupelka:
    return "Kupelna";
  case ROOM::Podlahovka:
    return "Podlahovka";
  default:
    return "invalid";
  }
}

constexpr const char *roomTxt(int room_idx)
{
  return roomTxt((ROOM)room_idx);
}

std::string roomTopic(ROOM room);
ROOM txtToRoom(std::string name);

} // namespace kurenie
