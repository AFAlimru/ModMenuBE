#include "ModMenu/Workspace.h"
#include "ModMenu/Storage.h"
#include "ModMenu/TextEditor.h"
#include <algorithm>
#include <set>
#include <stdexcept>
#include <cstring>
namespace menu {
namespace {
using J=WorkspaceJson;
void Need(bool ok,char const* message){if(!ok)throw std::runtime_error(message);}
void Members(J const&j,std::initializer_list<char const*> names){Need(j.is_object(),"Expected workspace object");for(auto const&[k,v]:j.items()){(void)v;Need(std::any_of(names.begin(),names.end(),[&](auto n){return k==n;}),"Unknown workspace property");}}
std::string Str(J const&j,std::size_t max,bool empty=false){Need(j.is_string(),"Expected workspace string");auto s=j.get<std::string>();Need(PlainText(s,empty?0:1,max)&&s.find("\xc2\xa7")==std::string::npos,"Invalid workspace string");return s;}
std::string Id(J const&j){auto s=Str(j,64);Need(SafeSettingsId(s),"Invalid workspace identifier");return s;}
std::uint64_t Uint(J const&j){Need(j.is_number_unsigned()||(j.is_number_integer()&&j.get<std::int64_t>()>=0),"Expected unsigned workspace integer");return j.get<std::uint64_t>();}
}
WorkspaceJson StrictJson(std::string const&raw,std::size_t limit){
 Need(!raw.empty()&&raw.size()<=limit&&ValidUtf8(raw),"Invalid workspace size/UTF8");std::vector<std::set<std::string>> stack;
 return J::parse(raw,[&](int depth,J::parse_event_t event,J&v){Need(depth<=16,"Workspace nesting limit");if(event==J::parse_event_t::object_start)stack.emplace_back();if(event==J::parse_event_t::key)Need(!stack.empty()&&stack.back().insert(v.get<std::string>()).second,"Duplicate workspace member");if(event==J::parse_event_t::object_end)stack.pop_back();return true;});
}
SettingTranslation WorkspaceCaption(J const&j,std::size_t limit){if(j.is_string())return {Str(j,limit),{},{} };Members(j,{"en_US","zh_TW","zh_CN"});SettingTranslation t{Str(j.at("en_US"),limit),{},{} };if(j.contains("zh_TW"))t.tw=Str(j.at("zh_TW"),limit);if(j.contains("zh_CN"))t.cn=Str(j.at("zh_CN"),limit);return t;}
WorkspaceDefinition ParseWorkspace(std::string const&raw,std::string const&id,std::string const&entry,std::string const&version){
 auto j=StrictJson(raw,65536);Members(j,{"schemaVersion","plugin","pages"});Need(j.at("schemaVersion")==1,"Unsupported workspace version");
 auto const&p=j.at("plugin");Members(p,{"id","entry","versions"});Need(SafeSettingsId(id)&&Str(p.at("id"),120)==id&&Str(p.at("entry"),120)==entry,"Workspace identity mismatch");
 Need(p.at("versions").is_array()&&!p.at("versions").empty()&&p.at("versions").size()<=32,"Workspace version list limit");bool found=false;std::set<std::string>versions;
 for(auto const&v:p.at("versions")){auto s=Str(v,80);Need(s.find('*')==std::string::npos&&versions.insert(s).second,"Invalid workspace exact version");found|=s==version;}Need(found,"Workspace manifest version mismatch");
 WorkspaceDefinition d{id,entry,version,{}};auto const&pages=j.at("pages");Need(pages.is_array()&&!pages.empty()&&pages.size()<=8,"Workspace page limit");std::set<std::string>pageIds,ids;unsigned images=0;
 for(auto const&pg:pages){Members(pg,{"id","label","controls"});WorkspacePage page;page.id=Id(pg.at("id"));Need(pageIds.insert(page.id).second,"Duplicate workspace page");page.label=WorkspaceCaption(pg.at("label"));
 auto const&controls=pg.at("controls");Need(controls.is_array()&&!controls.empty(),"Empty workspace page");for(auto const&c:controls){Members(c,{"id","type","label","minBytes","maxBytes","validation","confirmation","selectAction","emptyText"});WorkspaceControl control;control.id=Id(c.at("id"));Need(ids.insert(control.id).second&&ids.size()<=64,"Workspace control ID/count limit");control.type=Str(c.at("type"),16);control.label=WorkspaceCaption(c.at("label"));
 Need(control.type=="text"||control.type=="boolean"||control.type=="action"||control.type=="status"||control.type=="progress"||control.type=="list"||control.type=="image","Unknown workspace control");
 if(control.type=="text"){control.minBytes=Uint(c.at("minBytes"));control.maxBytes=Uint(c.at("maxBytes"));Need(control.minBytes<=control.maxBytes&&control.maxBytes>0&&control.maxBytes<=4096,"Workspace text limit");if(c.contains("validation")){auto v=Str(c.at("validation"),16);Need(v=="none"||v=="identifier","Unknown workspace validator");control.identifier=v=="identifier";}}
 else Need(!c.contains("minBytes")&&!c.contains("maxBytes")&&!c.contains("validation"),"Non-text workspace properties");
 if(c.contains("confirmation")){Need(control.type=="action","Confirmation needs action");control.confirmation=WorkspaceCaption(c.at("confirmation"),1024);}
 if(c.contains("selectAction")){Need(control.type=="list","Selection needs list");control.selectAction=Id(c.at("selectAction"));}
 if(c.contains("emptyText")){Need(control.type=="list","Empty text needs list");control.emptyText=WorkspaceCaption(c.at("emptyText"));}
 if(control.type=="image")Need(++images<=1,"Workspace image limit");page.controls.push_back(std::move(control));}d.pages.push_back(std::move(page));}
 return d;
}
WorkspaceControl const* FindWorkspaceControl(WorkspaceDefinition const&d,std::string const&id){for(auto const&p:d.pages)for(auto const&c:p.controls)if(c.id==id)return &c;return nullptr;}
WorkspaceJson ParseWorkspaceState(std::string const&raw,WorkspaceDefinition const&d){
 auto j=StrictJson(raw,262144);Members(j,{"protocolVersion","revision","controls","results"});Need(j.at("protocolVersion")==1&&Uint(j.at("revision"))>0,"Invalid workspace state version/revision");auto const&controls=j.at("controls");Need(controls.is_object()&&controls.size()<=64,"Invalid workspace state controls");unsigned rowCount=0;
 for(auto const&[id,v]:controls.items()){auto c=FindWorkspaceControl(d,id);Need(c!=nullptr,"Undeclared workspace control");Members(v,{"value","text","enabled","busy","reason","error","total","rows","width","height","pixels"});
 for(auto name:{"text","reason","error"})if(v.contains(name))WorkspaceCaption(v.at(name),1024);
 for(auto name:{"enabled","busy"})if(v.contains(name))Need(v.at(name).is_boolean(),"Invalid workspace flag");
 if(v.contains("value")){if(c->type=="text"){auto value=v.at("value");Need(value.is_string()&&PlainText(value.get<std::string>(),c->minBytes,c->maxBytes,c->identifier),"Invalid workspace text value");}else if(c->type=="boolean")Need(v.at("value").is_boolean(),"Invalid workspace boolean");else {Need(c->type=="progress","Unexpected workspace value");Uint(v.at("value"));}}
 if(v.contains("total")){Need(c->type=="progress"&&v.contains("value")&&Uint(v.at("total"))>0&&Uint(v.at("value"))<=Uint(v.at("total")),"Invalid progress bounds");}
 if(v.contains("rows")){Need(c->type=="list"&&v.at("rows").is_array()&&v.at("rows").size()<=128,"Workspace list limit");std::set<std::string>ids;for(auto const&r:v.at("rows")){Members(r,{"id","label","text","enabled","reason"});Need(ids.insert(Id(r.at("id"))).second&&++rowCount<=256,"Duplicate/over-limit row");WorkspaceCaption(r.at("label"));if(r.contains("text"))WorkspaceCaption(r.at("text"),1024);if(r.contains("reason"))WorkspaceCaption(r.at("reason"),1024);if(r.contains("enabled"))Need(r.at("enabled").is_boolean(),"Invalid row flag");}}
 if(v.contains("width")||v.contains("height")||v.contains("pixels")){Need(c->type=="image","Unexpected image data");auto w=Uint(v.at("width")),h=Uint(v.at("height"));Need(w>0&&w<=64&&h>0&&h<=48&&v.at("pixels").is_array()&&v.at("pixels").size()==w*h,"Workspace image bounds");for(auto const&pixel:v.at("pixels"))Need(Uint(pixel)<=0xFFFFFFFFull,"Invalid RGBA8 pixel");}
 }
 if(j.contains("results")){Need(j.at("results").is_array()&&j.at("results").size()<=16,"Workspace result limit");std::set<std::uint64_t>ids;for(auto const&r:j.at("results")){Members(r,{"requestId","state","message"});Need(ids.insert(Uint(r.at("requestId"))).second,"Duplicate result ID");auto state=Str(r.at("state"),16);Need(state=="succeeded"||state=="failed","Invalid result state");if(r.contains("message"))WorkspaceCaption(r.at("message"),1024);}}
 return j;
}
std::uint64_t WorkspaceBroker::Register(WorkspaceDefinition d){std::lock_guard lock(mutex_);if(providers_.size()>=16)return 0;for(auto const&[token,p]:providers_){(void)token;if(p.snapshot.definition->id==d.id)return 0;}auto session=++nextSession_;Provider p;p.snapshot.session=session;p.snapshot.definition=std::make_shared<WorkspaceDefinition const>(std::move(d));providers_.emplace(session,std::move(p));return session;}
bool WorkspaceBroker::Publish(std::uint64_t session,std::string const&raw){std::shared_ptr<WorkspaceDefinition const>d;{std::lock_guard lock(mutex_);auto it=providers_.find(session);if(it==providers_.end())return false;d=it->second.snapshot.definition;}
 try{auto state=ParseWorkspaceState(raw,*d);auto revision=state.at("revision").get<std::uint64_t>();std::lock_guard lock(mutex_);auto it=providers_.find(session);if(it==providers_.end())return false;auto&p=it->second;if(p.snapshot.state&&revision<=p.snapshot.state->at("revision").get<std::uint64_t>())return false;
 if(state.contains("results"))for(auto const&r:state.at("results")){auto id=r.at("requestId").get<std::uint64_t>();p.pending.erase(id);}
 p.snapshot.state=std::make_shared<J const>(std::move(state));return true;}catch(...){return false;}
}
bool WorkspaceBroker::Submit(std::uint64_t session,std::string const&page,std::string const&action,std::uint64_t revision,J values,std::string const&row,std::string&error){
 std::lock_guard lock(mutex_);error="Workspace unavailable or changed; try again.";auto it=providers_.find(session);if(it==providers_.end())return false;auto&p=it->second;if(!p.snapshot.state||p.snapshot.state->at("revision").get<std::uint64_t>()!=revision)return false;auto const&state=*p.snapshot.state;auto const&controls=state.at("controls");WorkspacePage const*pg=nullptr;for(auto const&v:p.snapshot.definition->pages)if(v.id==page)pg=&v;if(!pg||!values.is_object())return false;
 WorkspaceControl const*trigger=nullptr;for(auto const&c:pg->controls)if((c.type=="action"&&c.id==action&&row.empty())||(c.type=="list"&&c.selectAction==action&&!row.empty()))trigger=&c;if(!trigger||!controls.contains(trigger->id))return false;
 auto const&flags=controls.at(trigger->id);if(!flags.value("enabled",true)||flags.value("busy",false)){error="Action is unavailable or busy.";return false;}
 if(!row.empty()){bool found=false;if(flags.contains("rows"))for(auto const&r:flags.at("rows"))if(r.at("id")==row&&r.value("enabled",true))found=true;if(!found)return false;}
 for(auto const&[id,v]:values.items()){auto c=FindWorkspaceControl(*p.snapshot.definition,id);bool onPage=false;for(auto const&f:pg->controls)onPage|=f.id==id;if(!c||!onPage||(c->type!="text"&&c->type!="boolean"))return false;if(c->type=="text"&&(!v.is_string()||!PlainText(v.get<std::string>(),c->minBytes,c->maxBytes,c->identifier))){error="Invalid text field.";return false;}if(c->type=="boolean"&&!v.is_boolean())return false;}
 for(auto const&c:pg->controls)if((c.type=="text"||c.type=="boolean")&&!values.contains(c.id)){error="Missing form value.";return false;}
 for(auto const&[request,pending]:p.pending){(void)request;if(pending==action){error="Waiting for the previous action result.";return false;}}
 if(p.pending.size()>=16||p.commands.size()>=16){error="Command queue is full.";return false;}auto request=++nextRequest_;J command={{"protocolVersion",1},{"session",session},{"requestId",request},{"revision",revision},{"page",page},{"action",action},{"values",std::move(values)}};if(!row.empty())command["rowId"]=row;auto bytes=command.dump();if(bytes.size()>=32768){error="Form exceeds command size limit.";return false;}p.commands.push_back(std::move(bytes));p.pending.emplace(request,action);error.clear();return true;
}
bool WorkspaceBroker::Poll(std::uint64_t session,char*out,std::uint32_t capacity,std::uint32_t&required){std::lock_guard lock(mutex_);required=0;auto it=providers_.find(session);if(it==providers_.end())return false;if(it->second.commands.empty())return true;auto const&data=it->second.commands.front();required=static_cast<std::uint32_t>(data.size()+1);if(!out||capacity<required)return true;std::memcpy(out,data.c_str(),required);it->second.commands.pop_front();return true;}
bool WorkspaceBroker::Unregister(std::uint64_t session){std::lock_guard lock(mutex_);return providers_.erase(session)!=0;}
std::vector<WorkspaceSnapshot> WorkspaceBroker::Snapshots()const{std::lock_guard lock(mutex_);std::vector<WorkspaceSnapshot>out;for(auto const&[session,p]:providers_){(void)session;auto s=p.snapshot;for(auto const&[id,action]:p.pending){(void)id;s.pending.push_back(action);}out.push_back(std::move(s));}return out;}
WorkspaceBroker& Workspaces(){static WorkspaceBroker broker;return broker;}
}
