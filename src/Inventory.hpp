#pragma once
#include <algorithm>
#include <cstdint>
#include "Registry.hpp"

constexpr int kStackMax=64;
struct InvSlot{ ItemId id=0; int count=0; bool empty()const{return id==0||count<=0;} void clear(){id=0;count=0;} };

// 36 slots: 0..8 = hotbar, 9..35 = main inventory (same order as vanilla).
struct Inventory{
 InvSlot s[36]; InvSlot armor[4]; int selected=0;   // armor: 0 helmet, 1 chestplate, 2 leggings, 3 boots
 // returns how many items did not fit
 int add(ItemId id,int n){
  for(int pass=0;pass<2&&n>0;pass++)
   for(int i=0;i<36&&n>0;i++){
    InvSlot&t=s[i];
    const int mx=Registry::get().maxStack(id);
    if(pass==0){ if(t.id==id&&t.count>0&&t.count<mx){int m=std::min(n,mx-t.count);t.count+=m;n-=m;} }
    else if(t.empty()){int m=std::min(n,mx);t.id=id;t.count=m;n-=m;}
   }
  return n;
 }
 const InvSlot& held()const{return s[selected];}
 bool consumeHeld(){ InvSlot&t=s[selected]; if(t.empty())return false; if(--t.count<=0)t.clear(); return true; }
};
inline int armorTotal(const Inventory&v){ int p=0; for(auto&a:v.armor)if(!a.empty())p+=Registry::get().armorPoints(a.id); return p; }
