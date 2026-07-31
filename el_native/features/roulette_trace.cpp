#include "roulette_trace.h"
#include "../framework.h"
#include "../hook_registry.h"
#include "../il2cpp_resolve.h"
#include "../config_registry.h"
#include "../feature.h"
#include "../../minhook/include/MinHook.h"
#include "../../third_party/imgui/imgui.h"
#include <windows.h>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <vector>
#include <string>

namespace {
using ParseFn=void*(__fastcall*)(void*,void*,void*);
using SpinFn=void(__fastcall*)(void*,void*,void*,void*);
using ProgressFn=void(__fastcall*)(void*,void*,void*,void*);
using LotsFn=void*(__fastcall*)(void*,void*,void*);
using ChooseFn=void(__fastcall*)(void*,void*,void*,void*);
using DeliveryFn=void*(__fastcall*)(void*,void*);
static ParseFn oParse=nullptr; static SpinFn oSpin=nullptr; static ProgressFn oProgress=nullptr; static LotsFn oLots=nullptr;
static ChooseFn oChoose=nullptr; static DeliveryFn oDelivery=nullptr; static bool active=false;
static std::string companionPath;
struct Card { int index=-1; char type[64]={}; char id[128]={}; char quantity[32]={}; bool bad=false; };
struct Snapshot { char source[48]={}; char category[96]={}; char status[64]={}; unsigned count=0; int bad=-1; std::vector<Card> cards; };
static Snapshot snap; static std::mutex mx; static FILE* trace=nullptr;

static bool Readable(const void* p) { if(!p) return false; MEMORY_BASIC_INFORMATION m{}; if(!VirtualQuery(p,&m,sizeof(m))) return false; return m.State==MEM_COMMIT && !(m.Protect&PAGE_NOACCESS) && !(m.Protect&PAGE_GUARD); }
static bool Ptr(const void* p,size_t o,void** v) { if(!p||!v) return false; __try{*v=*(void**)((const uint8_t*)p+o);return true;}__except(EXCEPTION_EXECUTE_HANDLER){*v=nullptr;return false;} }
static bool I32(const void* p,size_t o,int32_t* v) { if(!p||!v) return false; __try{*v=*(int32_t*)((const uint8_t*)p+o);return true;}__except(EXCEPTION_EXECUTE_HANDLER){*v=0;return false;} }
static bool U8(const void* p,size_t o,uint8_t* v) { if(!p||!v) return false; __try{*v=*(uint8_t*)((const uint8_t*)p+o);return true;}__except(EXCEPTION_EXECUTE_HANDLER){*v=0;return false;} }
static void Str(void* p,char* out,size_t cap) { if(!out||!cap){return;} out[0]=0; if(!Readable(p))return; __try{int32_t n=*(int32_t*)((uint8_t*)p+0x10);if(n<=0)return;if((size_t)n>=cap)n=(int32_t)cap-1;const wchar_t* s=(const wchar_t*)((uint8_t*)p+0x14);for(int32_t i=0;i<n;i++){wchar_t c=s[i];out[i]=(c>=0x20&&c<0x7f)?(char)c:'?';}out[n]=0;}__except(EXCEPTION_EXECUTE_HANDLER){out[0]=0;} }
static bool BadToken(const char* s) { return s && (_strnicmp(s,"black",5)==0||_strnicmp(s,"lose",4)==0||_strnicmp(s,"fail",4)==0||_strnicmp(s,"nothing",7)==0||_strnicmp(s,"empty",5)==0||strstr(s,"black_mark")); }
static bool List(void* list,void*** items,int32_t* n) { void* arr=nullptr; if(!Readable(list)||!Ptr(list,0x10,&arr)||!I32(list,0x18,n)||!Readable(arr)||*n<0||*n>64)return false;*items=(void**)((uint8_t*)arr+0x20);return true; }
static void Emit(const char* kind,const char* detail) { if(!trace)trace=fopen("el_native_roulette_trace.jsonl","ab"); if(trace){fprintf(trace,"{\"kind\":\"%s\",\"detail\":\"%s\"}\n",kind,detail?detail:"");fflush(trace);} }
static void Decode(void* list,const char* source) { void** items=nullptr;int32_t n=0;if(!List(list,&items,&n))return; std::lock_guard<std::mutex> l(mx); snap=Snapshot{};strncpy_s(snap.source,source,_TRUNCATE);snap.count=(unsigned)n;for(int32_t i=0;i<n&&i<5;i++){void* r=nullptr;if(!Ptr(items,(size_t)i*8,&r)||!Readable(r))continue;Card c;c.index=i;void* p=nullptr;if(Ptr(r,0x10,&p))Str(p,c.type,sizeof(c.type));if(Ptr(r,0x18,&p))Str(p,c.id,sizeof(c.id));if(Ptr(r,0x20,&p))Str(p,c.quantity,sizeof(c.quantity));c.bad=BadToken(c.type)||BadToken(c.id);if(c.bad&&snap.bad<0)snap.bad=i;snap.cards.push_back(c);}char d[128];_snprintf_s(d,_TRUNCATE,"source=%s,count=%d,bad=%d",source,n,snap.bad);Emit("cards",d); }
static void* __fastcall ParseHook(void* self,void* rewards,void* mi){if(active) Decode(rewards,"CardRoulette.ParseRewards");return oParse?oParse(self,rewards,mi):nullptr;}
static void __fastcall SpinHook(void* self,void* error,void* response,void* mi){if(active){char d[128];_snprintf_s(d,_TRUNCATE,"error=%p,response=%p",error,response);Emit("server_response",d);void* result=nullptr;for(size_t off:{(size_t)0x10,(size_t)0x18,(size_t)0x20,(size_t)0x28}){void* c=nullptr;int32_t n=0;void* r=nullptr;if(Ptr(response,off,&c)&&Readable(c)&&Ptr(c,0x10,&r)&&I32(r,0x18,&n)&&n>=0&&n<=5){result=c;break;}}if(result){void* r=nullptr;uint8_t b=0;Ptr(result,0x10,&r);U8(result,0x18,&b);Decode(r,b?"CardRoulette.server.black_mark":"CardRoulette.server");if(b){std::lock_guard<std::mutex>l(mx);if(snap.bad<0)strncpy_s(snap.status,"black-mark reported; card index unknown",_TRUNCATE);}}}if(oSpin)oSpin(self,error,response,mi);}
static void __fastcall ProgressHook(void* self,void* error,void* response,void* mi){if(active){char d[128];_snprintf_s(d,_TRUNCATE,"error=%p,response=%p",error,response);Emit("progress_response",d);}if(oProgress)oProgress(self,error,response,mi);}
static void* __fastcall LotsHook(void* self,void* category,void* mi){if(active){char c[96];Str(category,c,sizeof(c));{std::lock_guard<std::mutex>l(mx);snap=Snapshot{};strncpy_s(snap.source,"ChooseReward.GetRelevantLots",_TRUNCATE);strncpy_s(snap.category,c,_TRUNCATE);}Emit("lots",c);}return oLots?oLots(self,category,mi):nullptr;}
static void __fastcall ChooseHook(void* self,void* data,void* chosen,void* mi){if(active){int32_t n=0;I32(chosen,0x18,&n);char d[128];_snprintf_s(d,_TRUNCATE,"chosen_list=%p,count=%d",chosen,n);Emit("choice",d);}if(oChoose)oChoose(self,data,chosen,mi);}
static void* __fastcall DeliveryHook(void* self,void* mi){void* r=oDelivery?oDelivery(self,mi):nullptr;if(active){int32_t n=0;I32(r,0x18,&n);char d[128];_snprintf_s(d,_TRUNCATE,"delivery=%p,count=%d",r,n);Emit("delivery",d);}return r;}
template<class T> static bool Install(const char* owner,const char* ns,const char* type,const char* method,int argc,T hook,T* orig){void* t=ResolveMethodOrFallback("Assembly-CSharp",ns,type,method,argc);if(!t||!GlobalHookRegistry().Claim((uintptr_t)t,owner))return false;GlobalHookRegistry().MarkResolved((uintptr_t)t);if(MH_CreateHook(t,(LPVOID)hook,(LPVOID*)orig)!=MH_OK||MH_EnableHook(t)!=MH_OK){GlobalHookRegistry().MarkUnavailable((uintptr_t)t);return false;}GlobalHookRegistry().MarkHooked((uintptr_t)t);return true;}
}

RouletteTraceFeature::RouletteTraceFeature(){name="Bunny Royale Trace";enabled=false;}
void RouletteTraceFeature::Init(){
    GlobalConfigRegistry().RegisterString("bunny.companion_path", &companionPath);
    Install("roulette.parse","CardRoulette","CardRouletteModel","ParseRewards",1,ParseFn(&ParseHook),&oParse);Install("roulette.response","CardRoulette","CardRouletteModel","OnSpinResponseReceived",2,SpinFn(&SpinHook),&oSpin);Install("roulette.progress","CardRoulette","CardRouletteModel","OnProgressResponseReceived",2,ProgressFn(&ProgressHook),&oProgress);Install("roulette.lots","AutoChess.MiniEvents.MiniEventModules.ChooseRewardEvent","ChooseRewardRouletteEventModule","GetRelevantRouletteLotDataForCategory",1,LotsFn(&LotsHook),&oLots);Install("roulette.choose","AutoChess.MiniEvents.MiniEventModules.ChooseRewardEvent","ChooseRewardRouletteEventModule","ChooseReward",2,ChooseFn(&ChooseHook),&oChoose);Install("roulette.delivery","AutoChess.MiniEvents.MiniEventModules.ChooseRewardEvent","ChooseRewardRouletteEventModule","CreateDeliveryDataAndSave",0,DeliveryFn(&DeliveryHook),&oDelivery);LOG("[ROULETTE] observer hooks installed; no reward/payload mutation");}
void RouletteTraceFeature::OnUpdate(){active=enabled;}
void RouletteTraceFeature::OnMenu(){
    static char path[260] = {};
    static bool initialized = false;
    static std::string lastPath;
    if (!initialized || lastPath != companionPath) {
        strncpy_s(path, companionPath.c_str(), _TRUNCATE);
        lastPath = companionPath;
        initialized = true;
    }
    if (ImGui::InputText("Python companion path", path, sizeof(path))) {
        companionPath = path;
        ConfigMarkDirty();
    }
    const DWORD attr = companionPath.empty() ? INVALID_FILE_ATTRIBUTES : GetFileAttributesA(companionPath.c_str());
    ImGui::Text("Companion: %s", (attr != INVALID_FILE_ATTRIBUTES) ? "configured/found" : "not configured");
    ImGui::TextWrapped("Python companion path points to the separate observer/decoder tool. "
                       "EL_Native does not launch it or proxy traffic; it only writes the "
                       "decoded server trace and displays the latest card snapshot.");
    ImGui::SeparatorText("Bunny Royale quick use");
    ImGui::BulletText("Set the path to the companion .py/.exe and start it separately.");
    ImGui::BulletText("Inject EL_Native, enable Bunny Royale Trace, then open the event.");
    ImGui::BulletText("Wait for a cards/server_response entry before choosing a card.");
    ImGui::BulletText("A red card is marked only when the response contains an explicit bad/lose token; otherwise the index stays unknown.");
    ImGui::BulletText("Keep el_native_roulette_trace.jsonl for post-event review.");
    RouletteTraceRender();
}
void RouletteTraceRender(){std::lock_guard<std::mutex>l(mx);ImGui::SeparatorText("Bunny/Rabbit roulette (trace only)");ImGui::Text("source: %s  category: %s  cards: %u",snap.source,snap.category[0]?snap.category:"?",snap.count);if(snap.bad>=0)ImGui::TextColored(ImVec4(1,.2f,.2f,1),"BAD CARD: #%d (explicit token)",snap.bad+1);else ImGui::Text("BAD CARD: unknown (server did not label an index)");for(const auto&c:snap.cards){if(c.bad)ImGui::TextColored(ImVec4(1,.2f,.2f,1),"[X] card #%d %s/%s x%s",c.index+1,c.type,c.id,c.quantity);else ImGui::Text("[ ] card #%d %s/%s x%s",c.index+1,c.type,c.id,c.quantity);}ImGui::TextWrapped("Trace: el_native_roulette_trace.jsonl; hooks are observers only.");}
static RouletteTraceFeature g_feature; static int g_registered=(RegisterFeature(&g_feature),0);
