#include "Runtime.h"
#include "Service.h"
#include "Entry.h"
#include "compat/NativeBridge.h"
#ifdef _WIN32
#include <Windows.h>
#include <filesystem>
#include <fstream>
namespace modmenu::workspace {
namespace {
bool ordinary(std::filesystem::path p){if(!p.is_absolute())return false;for(;;){auto a=GetFileAttributesW(p.c_str());if(a==INVALID_FILE_ATTRIBUTES||(a&FILE_ATTRIBUTE_REPARSE_POINT))return false;auto parent=p.parent_path();if(parent==p)break;p=parent;}return true;}
std::string read(std::filesystem::path const&p,std::size_t max){if(!ordinary(p)||!std::filesystem::is_regular_file(p))return {};auto n=std::filesystem::file_size(p);if(!n||n>max)return {};std::ifstream f(p,std::ios::binary);std::string s(std::size_t(n),'\0');return f.read(s.data(),s.size())?s:std::string();}
std::filesystem::path modulePath(HMODULE m){wchar_t p[32768]{};auto n=GetModuleFileNameW(m,p,32768);return n&&n<32768?std::filesystem::path(p):std::filesystem::path();}
}
std::int32_t registerLoadedProvider(void const*owner,char const*raw,std::uint32_t bytes,std::uint64_t*out){
 if(!out)return -1;*out=0;if(!owner||!raw||!bytes||bytes>65536)return -1;if(!compat::nativeReady())return -2;
 try{MEMORY_BASIC_INFORMATION memory{};if(!VirtualQuery(owner,&memory,sizeof(memory))||memory.Type!=MEM_IMAGE||memory.State!=MEM_COMMIT||(memory.Protect&(PAGE_GUARD|PAGE_NOACCESS))||!(memory.Protect&(PAGE_EXECUTE|PAGE_EXECUTE_READ|PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY)))return -3;
 HMODULE module{},self{};auto flags=GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT;
 if(!GetModuleHandleExW(flags,reinterpret_cast<LPCWSTR>(owner),&module)||module==GetModuleHandleW(nullptr)||!GetModuleHandleExW(flags,reinterpret_cast<LPCWSTR>(&registerLoadedProvider),&self)||module==self)return -3;
 auto path=modulePath(module),own=modulePath(self);if(path.empty()||own.empty()||!ordinary(path)||!ordinary(own)||!std::filesystem::equivalent(path.parent_path().parent_path(),own.parent_path().parent_path()))return -3;
 auto folder=path.parent_path();auto manifest=menu::StrictJson(read(folder/L"manifest.json",65536),65536);auto name=manifest.at("name").get<std::string>(),entry=manifest.at("entry").get<std::string>(),version=manifest.at("version").get<std::string>();
 if(!identifier(name)||folder.filename().string()!=name||path.filename().string()!=entry||!manifest.value("enabled",true))return -3;
 auto descriptorPath=folder/L"modmenu.json";std::string descriptor;
 if(std::filesystem::exists(descriptorPath))descriptor=read(descriptorPath,1048576);
 else{auto community=own.parent_path()/L"adapters"/(std::filesystem::path(name).wstring()+L".json");descriptor=read(community,1048576);}
 auto routing=parseEntry(descriptor);if(!routing.visible())return -3;
 HMODULE pinned{};if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,reinterpret_cast<LPCWSTR>(owner),&pinned)||pinned!=module)return -3;
 *out=service().registerProvider(std::string(raw,bytes),name,entry,version);return *out?0:-3;
 }catch(...){return -3;}
}
}
#else
namespace modmenu::workspace {std::int32_t registerLoadedProvider(void const*,char const*,std::uint32_t,std::uint64_t*out){if(out)*out=0;return -2;}}
#endif
