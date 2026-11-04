#pragma once
#include "Registry.hpp"
// Vanilla-style survival mining: hardness per block, tool class / tier / speed, "needs the right tool to drop".
struct MineInfo{ float seconds=1.f; bool canHarvest=true; bool unbreakable=false; };
MineInfo mineInfo(const Registry&R,BlockId block,ItemId heldTool);   // heldTool 0 = bare hand
