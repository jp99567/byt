#include "TxtEnum.h"
#include<map>

namespace kurenie {

std::string roomTopic(ROOM room)
{
  return std::string("rb/stat/tev/").append(roomTxt(room));
}

ROOM txtToRoom(std::string name)
{
  static std::map<std::string, ROOM> cRoomMap;
  if (cRoomMap.empty()) {
    for(int iroom = 0; iroom<(int)ROOM::_last; ++iroom){
      auto room = (ROOM)iroom;
      cRoomMap.emplace(std::string(roomTxt(room)), room);
    }
  }
  auto it = cRoomMap.find(name);
  if (it == std::cend(cRoomMap)) {
    return ROOM::_last;
  }
  return it->second;
}

} // namespace kurenie
