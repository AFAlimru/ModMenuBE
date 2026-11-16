#include "modmenu/ModMenuWorkspace.h"
#include "Runtime.h"
#include "Service.h"
#include "compat/NativeBridge.h"
namespace {
using namespace modmenu::workspace;
int32_t ready(){return modmenu::compat::nativeReady()?1:0;}
int32_t publish(uint64_t session,char const*raw,uint32_t n){if(!raw||!n||n>262144)return -1;if(!ready())return -2;try{return service().publish(session,std::string(raw,n))?0:-3;}catch(...){return -3;}}
int32_t poll(uint64_t session,char*out,uint32_t capacity,uint32_t*required){if(!required)return -1;*required=0;if(capacity>32768||(!out&&capacity))return -1;if(!ready())return -2;try{if(!service().poll(session,out,capacity,*required,milliseconds()))return -3;return *required>capacity?1:0;}catch(...){return -3;}}
int32_t unregisterProvider(uint64_t session){try{return service().unregisterProvider(session)?0:-3;}catch(...){return -3;}}
int32_t acknowledge(uint64_t session,uint64_t request,int32_t success){if(success!=0&&success!=1)return -1;try{return service().acknowledge(session,request,success!=0,milliseconds())?0:-3;}catch(...){return -3;}}
int32_t back(uint64_t session,uint64_t request){try{return service().requestReturn(session,request,milliseconds())?0:-3;}catch(...){return -3;}}
ModMenuWorkspaceApi const api={MODMENU_WORKSPACE_VERSION,sizeof(ModMenuWorkspaceApi),ready,registerLoadedProvider,publish,poll,unregisterProvider,acknowledge,back};
}
extern "C" const ModMenuWorkspaceApi* ModMenu_GetWorkspaceApi(uint32_t minVersion){return minVersion<=api.version?&api:nullptr;}
