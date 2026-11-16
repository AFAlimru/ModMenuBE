#pragma once
#include <string>
#include <string_view>
namespace menu {
std::string_view DefinitionNamespace(std::string_view path);
// 26.60 removed the callback's resource path. Only these exact audited
// vanilla payloads can authorize automatic insertion in that build.
std::string_view VerifiedMenu60(std::string_view source);
bool InsertModsDefinition(std::string const& source,std::string& output,std::string_view expectedNamespace={});
inline constexpr char ModsEventName[]="button.mmb_open_mods";
inline constexpr unsigned ModsEventId=[] {unsigned h=0x811c9dc5;for(char c:ModsEventName){if(!c)break;h=(h^static_cast<unsigned char>(c))*0x1000193;}return h;}();
inline constexpr unsigned ResourcePackEntryApiVersion=1;
inline constexpr char ResourcePackOpenEventName[]="button.modmenu.open";
inline constexpr unsigned ResourcePackOpenEventId=[] {unsigned h=0x811c9dc5;for(char c:ResourcePackOpenEventName){if(!c)break;h=(h^static_cast<unsigned char>(c))*0x1000193;}return h;}();
inline constexpr char ResourcePackEntryMarker[]="mmb_resource_pack_entry_v1";
inline constexpr bool IsModsOpenEvent(unsigned id){return id==ModsEventId || id==ResourcePackOpenEventId;}
}
