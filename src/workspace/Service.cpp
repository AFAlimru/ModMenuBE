#include "Service.h"
#include "Entry.h"
#include <algorithm>
#include <chrono>
#include <cstring>
#include <set>
namespace modmenu::workspace {
std::uint64_t milliseconds(){return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();}
Service& service(){static Service instance;return instance;}
void Service::expire(std::uint64_t now){if(external_.deadline&&now>=external_.deadline&&(external_.phase==HandoffPhase::Prepared||external_.phase==HandoffPhase::Queued||external_.phase==HandoffPhase::Delivered)){external_.phase=HandoffPhase::Failed;external_.message="External UI acknowledgement timed out; no automatic retry.";}}
std::uint64_t Service::registerProvider(std::string const& raw,std::string const& id,std::string const& entry,std::string const& version){
 try{auto j=menu::StrictJson(raw,65536);std::vector<std::string> targets;
 if(j.contains("externalTargets")){auto const&t=j.at("externalTargets");if(!t.is_array()||t.size()>16)return 0;std::set<std::string> ids;for(auto const&v:t){if(!v.is_string())return 0;auto name=v.get<std::string>();if(!identifier(name)||!ids.insert(name).second)return 0;targets.push_back(name);}j.erase("externalTargets");}
 bool externalOnly=j.contains("pages")&&j["pages"].is_array()&&j["pages"].empty();
 if(externalOnly){if(targets.empty())return 0;j["pages"]={{{"id","internal_placeholder"},{"label","Internal"},{"controls",{{{"id","internal_status"},{"type","status"},{"label","Internal"}}}}}};}
 auto definition=menu::ParseWorkspace(j.dump(),id,entry,version);if(externalOnly)definition.pages.clear();definition.externalTargets=targets;
 std::lock_guard lock(mutex_);auto session=broker_.Register(std::move(definition));if(session)targets_.emplace(session,std::move(targets));return session;
 }catch(...){return 0;}
}
bool Service::unregisterProvider(std::uint64_t s){std::lock_guard lock(mutex_);if(!targets_.erase(s))return false;if(external_.session==s){external_.phase=HandoffPhase::Failed;external_.message="Provider session ended.";}return broker_.Unregister(s);}
bool Service::publish(std::uint64_t s,std::string const& raw){return broker_.Publish(s,raw);}
std::optional<menu::WorkspaceSnapshot> Service::snapshot(std::string const&id)const{std::lock_guard lock(mutex_);for(auto const&s:broker_.Snapshots())if(s.definition->id==id)return s;return {};}
bool Service::hasExternal(std::uint64_t s,std::string const&t)const{std::lock_guard lock(mutex_);auto i=targets_.find(s);return i!=targets_.end()&&std::find(i->second.begin(),i->second.end(),t)!=i->second.end();}
bool Service::submit(std::uint64_t s,std::string const&p,std::string const&a,std::uint64_t r,menu::WorkspaceJson v,std::string const&row,std::string&err){std::lock_guard lock(mutex_);return broker_.Submit(s,p,a,r,std::move(v),row,err);}
std::uint64_t Service::beginExternal(std::uint64_t s,std::string const&t,std::uint64_t now){std::lock_guard lock(mutex_);expire(now);if(external_.phase!=HandoffPhase::None)return 0;auto i=targets_.find(s);if(i==targets_.end()||std::find(i->second.begin(),i->second.end(),t)==i->second.end())return 0;external_={s,++nextExternal_,now+15000,t,{},HandoffPhase::Prepared};return external_.request;}
bool Service::releaseExternal(std::uint64_t s,std::uint64_t r,std::uint64_t now){std::lock_guard lock(mutex_);expire(now);if(external_.session!=s||external_.request!=r||external_.phase!=HandoffPhase::Prepared)return false;external_.phase=HandoffPhase::Queued;external_.deadline=now+15000;return true;}
bool Service::poll(std::uint64_t s,char*out,std::uint32_t cap,std::uint32_t&required,std::uint64_t now){std::lock_guard lock(mutex_);required=0;expire(now);if(!targets_.count(s)||cap>32768||(!out&&cap))return false;
 if(external_.session==s&&external_.phase==HandoffPhase::Queued){auto command=menu::WorkspaceJson{{"protocolVersion",1},{"session",s},{"requestId",external_.request},{"kind","openExternal"},{"target",external_.target}}.dump();required=std::uint32_t(command.size()+1);if(out&&cap>=required){std::memcpy(out,command.c_str(),required);external_.phase=HandoffPhase::Delivered;}return true;}
 return broker_.Poll(s,out,cap,required);}
bool Service::acknowledge(std::uint64_t s,std::uint64_t r,bool ok,std::uint64_t now){std::lock_guard lock(mutex_);expire(now);if(external_.session!=s||external_.request!=r||external_.phase!=HandoffPhase::Delivered)return false;external_.phase=ok?HandoffPhase::Active:HandoffPhase::Failed;external_.deadline=0;if(!ok)external_.message="Provider could not open the external UI.";return true;}
bool Service::requestReturn(std::uint64_t s,std::uint64_t r,std::uint64_t now){std::lock_guard lock(mutex_);expire(now);if(external_.session!=s||external_.request!=r||external_.phase!=HandoffPhase::Active||!targets_.count(s))return false;external_.phase=HandoffPhase::Returned;return true;}
Handoff Service::handoff(std::uint64_t now){std::lock_guard lock(mutex_);expire(now);return external_;}
void Service::cancelExternal(std::uint64_t r){std::lock_guard lock(mutex_);if(external_.request==r&&external_.phase!=HandoffPhase::None){external_.phase=HandoffPhase::Failed;external_.deadline=0;external_.message="External UI return cancelled: menu origin or provider changed.";}}
void Service::finishExternal(std::uint64_t r){std::lock_guard lock(mutex_);if(external_.request==r&&(external_.phase==HandoffPhase::Returned||external_.phase==HandoffPhase::Failed))external_={};}
}
