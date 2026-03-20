#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <shellapi.h>
#include <algorithm>
#include <atomic>
#include <cctype>
#include <fstream>
#include <map>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace {
constexpr wchar_t OV_CLASS[] = L"MyCrossOverlay";
constexpr wchar_t CTL_CLASS[] = L"MyCrossCtl";
constexpr UINT WM_APP_SYNC = WM_APP + 1;
constexpr UINT WM_APP_EXIT = WM_APP + 2;
constexpr int PORT = 5188;
constexpr DWORD HEARTBEAT_TIMEOUT_MS = 15000;
constexpr int HOTKEY_ID = 1;
constexpr UINT HOTKEY_MOD = MOD_CONTROL | MOD_ALT | MOD_SHIFT | MOD_NOREPEAT;
constexpr UINT HOTKEY_VK = VK_F12;

struct Config { int x=-1,y=-1,window_size=40,cross_half=10,line_width=2,color_r=0,color_g=255,color_b=0; };
struct State { std::mutex mu; Config cfg; bool running=false; std::wstring active=L"default.ini"; } g_state;

std::atomic<bool> g_exit{false};
std::atomic<DWORD> g_lastPing{0};
HINSTANCE g_inst=nullptr;
std::wstring g_exeDir,g_cfgDir,g_webDir;
std::thread g_overlayThread;
std::atomic<bool> g_overlayReady{false};
HWND g_ctlWnd=nullptr,g_overlayWnd=nullptr;
SOCKET g_listen=INVALID_SOCKET;
bool g_appMode=false;
PROCESS_INFORMATION g_uiProc={};
DWORD g_launchErr=0;

int clampi(int v,int a,int b){return std::max(a,std::min(b,v));}
void normalize(Config& c){c.window_size=clampi(c.window_size,20,800);c.line_width=clampi(c.line_width,1,20);c.cross_half=clampi(c.cross_half,1,std::max(1,c.window_size/2));c.color_r=clampi(c.color_r,0,255);c.color_g=clampi(c.color_g,0,255);c.color_b=clampi(c.color_b,0,255);if(c.x<-1)c.x=-1;if(c.y<-1)c.y=-1;}
int toint(const std::string&s,int d){if(s.empty())return d;char*e=nullptr;long v=strtol(s.c_str(),&e,10);return (e==s.c_str()||*e!='\0')?d:(int)v;}
std::wstring itow(int v){wchar_t b[32] = {}; swprintf(b,32,L"%d",v); return b;}
std::wstring exe_dir(){wchar_t p[MAX_PATH] = {};GetModuleFileNameW(nullptr,p,MAX_PATH);std::wstring s(p);size_t k=s.find_last_of(L"\\/");return k==std::wstring::npos?L".":s.substr(0,k);}
std::wstring utf8w(const std::string&s){if(s.empty())return L"";int n=MultiByteToWideChar(CP_UTF8,0,s.c_str(),-1,nullptr,0);if(n<=0)return L"";std::wstring o((size_t)n-1,L'\0');MultiByteToWideChar(CP_UTF8,0,s.c_str(),-1,o.data(),n);return o;}
std::string wutf8(const std::wstring&s){if(s.empty())return "";int n=WideCharToMultiByte(CP_UTF8,0,s.c_str(),-1,nullptr,0,nullptr,nullptr);if(n<=0)return "";std::string o((size_t)n-1,'\0');WideCharToMultiByte(CP_UTF8,0,s.c_str(),-1,o.data(),n,nullptr,nullptr);return o;}
std::string jesc(const std::string&in){std::string o;for(char c:in){if(c=='\\')o+="\\\\";else if(c=='\"')o+="\\\"";else if(c=='\n')o+="\\n";else if(c=='\r')o+="\\r";else if(c=='\t')o+="\\t";else o+=c;}return o;}
std::wstring trim(const std::wstring&s){size_t l=0;while(l<s.size()&&iswspace(s[l]))++l;size_t r=s.size();while(r>l&&iswspace(s[r-1]))--r;return s.substr(l,r-l);} 
std::wstring profile_name(std::wstring n){n=trim(n);for(auto&c:n)if(c==L'\\'||c==L'/'||c==L':'||c==L'*'||c==L'?'||c==L'"'||c==L'<'||c==L'>'||c==L'|')c=L'_';if(n.empty())return L"";if(n.size()<4||_wcsicmp(n.c_str()+n.size()-4,L".ini")!=0)n+=L".ini";return n;}
std::wstring ppath(const std::wstring&n){return g_cfgDir+L"\\"+n;}

std::vector<std::wstring> profiles(){std::vector<std::wstring> v;WIN32_FIND_DATAW fd={};HANDLE h=FindFirstFileW((g_cfgDir+L"\\*.ini").c_str(),&fd);if(h==INVALID_HANDLE_VALUE)return v;do{if((fd.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)==0)v.emplace_back(fd.cFileName);}while(FindNextFileW(h,&fd));FindClose(h);std::sort(v.begin(),v.end());return v;}
Config load_cfg(const std::wstring&f){Config c;c.x=GetPrivateProfileIntW(L"Crosshair",L"x",c.x,f.c_str());c.y=GetPrivateProfileIntW(L"Crosshair",L"y",c.y,f.c_str());c.window_size=GetPrivateProfileIntW(L"Crosshair",L"window_size",c.window_size,f.c_str());c.cross_half=GetPrivateProfileIntW(L"Crosshair",L"cross_half",c.cross_half,f.c_str());c.line_width=GetPrivateProfileIntW(L"Crosshair",L"line_width",c.line_width,f.c_str());c.color_r=GetPrivateProfileIntW(L"Crosshair",L"color_r",c.color_r,f.c_str());c.color_g=GetPrivateProfileIntW(L"Crosshair",L"color_g",c.color_g,f.c_str());c.color_b=GetPrivateProfileIntW(L"Crosshair",L"color_b",c.color_b,f.c_str());normalize(c);return c;}
bool save_cfg(const std::wstring&f,Config c){normalize(c);return WritePrivateProfileStringW(L"Crosshair",L"x",itow(c.x).c_str(),f.c_str())&&WritePrivateProfileStringW(L"Crosshair",L"y",itow(c.y).c_str(),f.c_str())&&WritePrivateProfileStringW(L"Crosshair",L"window_size",itow(c.window_size).c_str(),f.c_str())&&WritePrivateProfileStringW(L"Crosshair",L"cross_half",itow(c.cross_half).c_str(),f.c_str())&&WritePrivateProfileStringW(L"Crosshair",L"line_width",itow(c.line_width).c_str(),f.c_str())&&WritePrivateProfileStringW(L"Crosshair",L"color_r",itow(c.color_r).c_str(),f.c_str())&&WritePrivateProfileStringW(L"Crosshair",L"color_g",itow(c.color_g).c_str(),f.c_str())&&WritePrivateProfileStringW(L"Crosshair",L"color_b",itow(c.color_b).c_str(),f.c_str());}
void ensure_cfg(){CreateDirectoryW(g_cfgDir.c_str(),nullptr);if(profiles().empty())save_cfg(ppath(L"default.ini"),Config{});} 

RECT overlay_rect(const Config&c){int sw=GetSystemMetrics(SM_CXSCREEN),sh=GetSystemMetrics(SM_CYSCREEN);int cx=c.x<0?sw/2:c.x,cy=c.y<0?sh/2:c.y;cx=clampi(cx,0,sw-1);cy=clampi(cy,0,sh-1);RECT r={};r.left=cx-c.window_size/2;r.top=cy-c.window_size/2;r.right=r.left+c.window_size;r.bottom=r.top+c.window_size;return r;}
void apply_overlay(){Config c;bool run=false;{std::lock_guard<std::mutex>lk(g_state.mu);c=g_state.cfg;run=g_state.running;}if(!run){if(g_overlayWnd){DestroyWindow(g_overlayWnd);g_overlayWnd=nullptr;}return;}RECT r=overlay_rect(c);if(!g_overlayWnd){g_overlayWnd=CreateWindowExW(WS_EX_TOPMOST|WS_EX_LAYERED|WS_EX_TRANSPARENT|WS_EX_NOACTIVATE,OV_CLASS,L"",WS_POPUP,r.left,r.top,r.right-r.left,r.bottom-r.top,nullptr,nullptr,g_inst,nullptr);if(!g_overlayWnd)return;SetLayeredWindowAttributes(g_overlayWnd,RGB(0,0,0),0,LWA_COLORKEY);ShowWindow(g_overlayWnd,SW_SHOW);}else{MoveWindow(g_overlayWnd,r.left,r.top,r.right-r.left,r.bottom-r.top,TRUE);}InvalidateRect(g_overlayWnd,nullptr,TRUE);} 
void post_sync(){if(g_ctlWnd)PostMessageW(g_ctlWnd,WM_APP_SYNC,0,0);} 

LRESULT CALLBACK OverlayProc(HWND h,UINT m,WPARAM w,LPARAM l){switch(m){case WM_PAINT:{Config c;{std::lock_guard<std::mutex>lk(g_state.mu);c=g_state.cfg;}PAINTSTRUCT ps={};HDC dc=BeginPaint(h,&ps);HPEN p=CreatePen(PS_SOLID,c.line_width,RGB(c.color_r,c.color_g,c.color_b));HGDIOBJ op=SelectObject(dc,p);int cx=c.window_size/2,cy=c.window_size/2;MoveToEx(dc,cx-c.cross_half,cy,nullptr);LineTo(dc,cx+c.cross_half,cy);MoveToEx(dc,cx,cy-c.cross_half,nullptr);LineTo(dc,cx,cy+c.cross_half);SelectObject(dc,op);DeleteObject(p);EndPaint(h,&ps);return 0;}case WM_DESTROY:if(h==g_overlayWnd)g_overlayWnd=nullptr;return 0;default:return DefWindowProcW(h,m,w,l);}}
LRESULT CALLBACK CtlProc(HWND h,UINT m,WPARAM w,LPARAM l){switch(m){case WM_APP_SYNC:apply_overlay();return 0;case WM_HOTKEY:if(w==HOTKEY_ID){{std::lock_guard<std::mutex>lk(g_state.mu);g_state.running=false;}apply_overlay();return 0;}break;case WM_APP_EXIT:PostQuitMessage(0);return 0;case WM_DESTROY:UnregisterHotKey(h,HOTKEY_ID);PostQuitMessage(0);return 0;}return DefWindowProcW(h,m,w,l);} 

void overlay_thread(){WNDCLASSW wc1={};wc1.lpfnWndProc=OverlayProc;wc1.hInstance=g_inst;wc1.lpszClassName=OV_CLASS;wc1.hbrBackground=(HBRUSH)GetStockObject(BLACK_BRUSH);RegisterClassW(&wc1);WNDCLASSW wc2={};wc2.lpfnWndProc=CtlProc;wc2.hInstance=g_inst;wc2.lpszClassName=CTL_CLASS;RegisterClassW(&wc2);g_ctlWnd=CreateWindowW(CTL_CLASS,L"",WS_OVERLAPPED,0,0,0,0,nullptr,nullptr,g_inst,nullptr);RegisterHotKey(g_ctlWnd,HOTKEY_ID,HOTKEY_MOD,HOTKEY_VK);g_overlayReady=true;MSG msg={};while(GetMessageW(&msg,nullptr,0,0)>0){TranslateMessage(&msg);DispatchMessageW(&msg);}if(g_overlayWnd){DestroyWindow(g_overlayWnd);g_overlayWnd=nullptr;}if(g_ctlWnd){DestroyWindow(g_ctlWnd);g_ctlWnd=nullptr;}g_overlayReady=false;}
void start_overlay(){g_overlayThread=std::thread(overlay_thread);for(int i=0;i<300&&!g_overlayReady.load();++i)Sleep(10);} 
void stop_overlay(){if(g_ctlWnd)PostMessageW(g_ctlWnd,WM_APP_EXIT,0,0);if(g_overlayThread.joinable())g_overlayThread.join();}

std::string read_file(const std::wstring&p){std::ifstream in(p.c_str(),std::ios::binary);if(!in)return"";std::ostringstream ss;ss<<in.rdbuf();return ss.str();}
std::string urld(const std::string&in){std::string o;for(size_t i=0;i<in.size();++i){if(in[i]=='%'&&i+2<in.size()){o.push_back((char)strtol(in.substr(i+1,2).c_str(),nullptr,16));i+=2;}else if(in[i]=='+')o.push_back(' ');else o.push_back(in[i]);}return o;}
std::map<std::string,std::string> form_parse(const std::string&b){std::map<std::string,std::string>m;size_t s=0;while(s<=b.size()){size_t a=b.find('&',s);std::string p=b.substr(s,a==std::string::npos?std::string::npos:a-s);if(!p.empty()){size_t e=p.find('=');if(e==std::string::npos)m[urld(p)]="";else m[urld(p.substr(0,e))]=urld(p.substr(e+1));}if(a==std::string::npos)break;s=a+1;}return m;}
void send_http(SOCKET c,int code,const char*status,const char*ct,const std::string&body){std::ostringstream o;o<<"HTTP/1.1 "<<code<<' '<<status<<"\r\n"<<"Content-Type: "<<ct<<"\r\n"<<"Content-Length: "<<body.size()<<"\r\nConnection: close\r\nCache-Control: no-cache\r\n\r\n"<<body;std::string r=o.str();send(c,r.c_str(),(int)r.size(),0);} 

bool read_req(SOCKET c,std::string&raw){raw.clear();char b[4096];bool hd=false;size_t he=std::string::npos,cl=0;while(true){int n=recv(c,b,sizeof(b),0);if(n<=0)return false;raw.append(b,b+n);if(!hd){he=raw.find("\r\n\r\n");if(he!=std::string::npos){hd=true;std::string h=raw.substr(0,he+4);size_t p=h.find("Content-Length:");if(p!=std::string::npos){p+=15;while(p<h.size()&&(h[p]==' '||h[p]=='\t'))++p;size_t e=p;while(e<h.size()&&isdigit((unsigned char)h[e]))++e;cl=(size_t)strtoul(h.substr(p,e-p).c_str(),nullptr,10);}}}if(hd){size_t bo=he+4;if(raw.size()-bo>=cl)return true;}}}

Config cfg_from_form(const std::map<std::string,std::string>&f,const Config&o){Config c=o;auto g=[&](const char*k,int d){auto it=f.find(k);return it==f.end()?d:toint(it->second,d);};c.x=g("x",c.x);c.y=g("y",c.y);c.window_size=g("window_size",c.window_size);c.cross_half=g("cross_half",c.cross_half);c.line_width=g("line_width",c.line_width);c.color_r=g("color_r",c.color_r);c.color_g=g("color_g",c.color_g);c.color_b=g("color_b",c.color_b);normalize(c);return c;}

std::string state_json(){Config c;bool run;std::wstring active;{std::lock_guard<std::mutex>lk(g_state.mu);c=g_state.cfg;run=g_state.running;active=g_state.active;}auto ps=profiles();std::ostringstream j;j<<"{"<<"\"running\":"<<(run?"true":"false")<<","<<"\"active_profile\":\""<<jesc(wutf8(active))<<"\","<<"\"hotkey\":\"Ctrl+Alt+Shift+F12\","<<"\"config\":{"<<"\"x\":"<<c.x<<",\"y\":"<<c.y<<",\"window_size\":"<<c.window_size<<",\"cross_half\":"<<c.cross_half<<",\"line_width\":"<<c.line_width<<",\"color_r\":"<<c.color_r<<",\"color_g\":"<<c.color_g<<",\"color_b\":"<<c.color_b<<"},\"profiles\":[";for(size_t i=0;i<ps.size();++i){if(i)j<<',';j<<"\""<<jesc(wutf8(ps[i]))<<"\"";}j<<"]}";return j.str();}

bool api(SOCKET c,const std::string&m,const std::string&p,const std::string&body){g_lastPing=GetTickCount();
if(m=="GET"&&p=="/api/state"){send_http(c,200,"OK","application/json; charset=utf-8",state_json());return true;}
if(m=="POST"&&p=="/api/ping"){send_http(c,200,"OK","text/plain; charset=utf-8","pong");return true;}
if(m=="POST"&&p=="/api/toggle"){auto f=form_parse(body);bool r=(f.find("running")!=f.end()&&f.at("running")=="1");{std::lock_guard<std::mutex>lk(g_state.mu);g_state.running=r;}post_sync();send_http(c,200,"OK","application/json; charset=utf-8",state_json());return true;}
if(m=="POST"&&p=="/api/apply"){auto f=form_parse(body);{std::lock_guard<std::mutex>lk(g_state.mu);g_state.cfg=cfg_from_form(f,g_state.cfg);}post_sync();send_http(c,200,"OK","application/json; charset=utf-8",state_json());return true;}
if(m=="POST"&&p=="/api/profile/load"){auto f=form_parse(body);auto it=f.find("name");if(it==f.end()){send_http(c,400,"Bad Request","text/plain; charset=utf-8","missing name");return true;}auto n=profile_name(utf8w(it->second));if(n.empty()){send_http(c,400,"Bad Request","text/plain; charset=utf-8","invalid name");return true;}auto file=ppath(n);if(GetFileAttributesW(file.c_str())==INVALID_FILE_ATTRIBUTES){send_http(c,404,"Not Found","text/plain; charset=utf-8","profile not found");return true;}Config ld=load_cfg(file);{std::lock_guard<std::mutex>lk(g_state.mu);g_state.cfg=ld;g_state.active=n;}post_sync();send_http(c,200,"OK","application/json; charset=utf-8",state_json());return true;}
if(m=="POST"&&p=="/api/profile/save"){auto f=form_parse(body);std::wstring n;{std::lock_guard<std::mutex>lk(g_state.mu);n=g_state.active;}auto it=f.find("name");if(it!=f.end()){auto nn=profile_name(utf8w(it->second));if(!nn.empty())n=nn;}if(n.empty()){send_http(c,400,"Bad Request","text/plain; charset=utf-8","invalid profile name");return true;}Config out;{std::lock_guard<std::mutex>lk(g_state.mu);g_state.cfg=cfg_from_form(f,g_state.cfg);out=g_state.cfg;g_state.active=n;}if(!save_cfg(ppath(n),out)){send_http(c,500,"Internal Server Error","text/plain; charset=utf-8","save failed");return true;}post_sync();send_http(c,200,"OK","application/json; charset=utf-8",state_json());return true;}
if(m=="POST"&&p=="/api/profile/new"){auto f=form_parse(body);auto it=f.find("name");if(it==f.end()){send_http(c,400,"Bad Request","text/plain; charset=utf-8","missing name");return true;}auto n=profile_name(utf8w(it->second));if(n.empty()){send_http(c,400,"Bad Request","text/plain; charset=utf-8","invalid profile name");return true;}auto file=ppath(n);if(GetFileAttributesW(file.c_str())!=INVALID_FILE_ATTRIBUTES){send_http(c,409,"Conflict","text/plain; charset=utf-8","profile exists");return true;}Config out;{std::lock_guard<std::mutex>lk(g_state.mu);g_state.cfg=cfg_from_form(f,g_state.cfg);out=g_state.cfg;g_state.active=n;}if(!save_cfg(file,out)){send_http(c,500,"Internal Server Error","text/plain; charset=utf-8","create failed");return true;}post_sync();send_http(c,200,"OK","application/json; charset=utf-8",state_json());return true;}
if(m=="POST"&&p=="/api/profile/rename"){auto f=form_parse(body);auto itn=f.find("new_name");if(itn==f.end()){send_http(c,400,"Bad Request","text/plain; charset=utf-8","missing new_name");return true;}std::wstring oldn;auto ito=f.find("old_name");if(ito!=f.end())oldn=profile_name(utf8w(ito->second));if(oldn.empty()){std::lock_guard<std::mutex>lk(g_state.mu);oldn=g_state.active;}auto newn=profile_name(utf8w(itn->second));if(oldn.empty()||newn.empty()){send_http(c,400,"Bad Request","text/plain; charset=utf-8","invalid name");return true;}if(_wcsicmp(oldn.c_str(),newn.c_str())==0){send_http(c,200,"OK","application/json; charset=utf-8",state_json());return true;}auto of=ppath(oldn),nf=ppath(newn);if(GetFileAttributesW(of.c_str())==INVALID_FILE_ATTRIBUTES){send_http(c,404,"Not Found","text/plain; charset=utf-8","source profile not found");return true;}if(GetFileAttributesW(nf.c_str())!=INVALID_FILE_ATTRIBUTES){send_http(c,409,"Conflict","text/plain; charset=utf-8","target exists");return true;}if(!MoveFileW(of.c_str(),nf.c_str())){send_http(c,500,"Internal Server Error","text/plain; charset=utf-8","rename failed");return true;}{std::lock_guard<std::mutex>lk(g_state.mu);if(_wcsicmp(g_state.active.c_str(),oldn.c_str())==0)g_state.active=newn;}send_http(c,200,"OK","application/json; charset=utf-8",state_json());return true;}
if(m=="POST"&&p=="/api/quit"){g_exit=true;send_http(c,200,"OK","text/plain; charset=utf-8","bye");return true;}
return false;}

void handle(SOCKET c){std::string raw;if(!read_req(c,raw)){closesocket(c);return;}size_t he=raw.find("\r\n\r\n");if(he==std::string::npos){send_http(c,400,"Bad Request","text/plain; charset=utf-8","bad request");closesocket(c);return;}std::string line;{std::istringstream hs(raw.substr(0,he));std::getline(hs,line);if(!line.empty()&&line.back()=='\r')line.pop_back();}std::string m,p,v;{std::istringstream rl(line);rl>>m>>p>>v;}size_t q=p.find('?');if(q!=std::string::npos)p=p.substr(0,q);std::string body=raw.substr(he+4);if(api(c,m,p,body)){closesocket(c);return;}if(m=="GET"&&p=="/"){auto html=read_file(g_webDir+L"\\index.html");if(html.empty())send_http(c,500,"Internal Server Error","text/plain; charset=utf-8","index.html missing");else send_http(c,200,"OK","text/html; charset=utf-8",html);closesocket(c);return;}send_http(c,404,"Not Found","text/plain; charset=utf-8","not found");closesocket(c);} 

bool start_server(){WSADATA w={};if(WSAStartup(MAKEWORD(2,2),&w)!=0)return false;g_listen=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);if(g_listen==INVALID_SOCKET){WSACleanup();return false;}int reuse=1;setsockopt(g_listen,SOL_SOCKET,SO_REUSEADDR,(const char*)&reuse,sizeof(reuse));sockaddr_in a={};a.sin_family=AF_INET;a.sin_port=htons(PORT);inet_pton(AF_INET,"127.0.0.1",&a.sin_addr);if(bind(g_listen,(sockaddr*)&a,sizeof(a))!=0){closesocket(g_listen);g_listen=INVALID_SOCKET;WSACleanup();return false;}if(listen(g_listen,16)!=0){closesocket(g_listen);g_listen=INVALID_SOCKET;WSACleanup();return false;}u_long nb=1;ioctlsocket(g_listen,FIONBIO,&nb);return true;}
void stop_server(){if(g_listen!=INVALID_SOCKET){closesocket(g_listen);g_listen=INVALID_SOCKET;}WSACleanup();}
bool wait_server(int ms){DWORD st=GetTickCount();while((int)(GetTickCount()-st)<ms){SOCKET s=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);if(s==INVALID_SOCKET){Sleep(50);continue;}sockaddr_in a={};a.sin_family=AF_INET;a.sin_port=htons(PORT);inet_pton(AF_INET,"127.0.0.1",&a.sin_addr);int rc=connect(s,(sockaddr*)&a,sizeof(a));closesocket(s);if(rc==0)return true;Sleep(60);}return false;}

std::wstring edge_path(){std::wstring p1=L"C:\\Program Files\\Microsoft\\Edge\\Application\\msedge.exe",p2=L"C:\\Program Files (x86)\\Microsoft\\Edge\\Application\\msedge.exe";if(GetFileAttributesW(p1.c_str())!=INVALID_FILE_ATTRIBUTES)return p1;if(GetFileAttributesW(p2.c_str())!=INVALID_FILE_ATTRIBUTES)return p2;return L"msedge.exe";}
bool launch_ui(){g_launchErr=0;std::wstring exe=edge_path();std::wstring cmd=L"\""+exe+L"\" --app=http://127.0.0.1:5188/ --new-window --window-size=1280,860";STARTUPINFOW si={};si.cb=sizeof(si);ZeroMemory(&g_uiProc,sizeof(g_uiProc));if(!CreateProcessW(nullptr,cmd.data(),nullptr,nullptr,FALSE,0,nullptr,nullptr,&si,&g_uiProc)){g_launchErr=GetLastError();return false;}return true;}
void stop_ui(){if(g_uiProc.hProcess){DWORD w=WaitForSingleObject(g_uiProc.hProcess,150);if(w==WAIT_TIMEOUT)TerminateProcess(g_uiProc.hProcess,0);CloseHandle(g_uiProc.hProcess);g_uiProc.hProcess=nullptr;}if(g_uiProc.hThread){CloseHandle(g_uiProc.hThread);g_uiProc.hThread=nullptr;}}

void apply_cli(){int argc=0;LPWSTR*argv=CommandLineToArgvW(GetCommandLineW(),&argc);if(!argv)return;Config c;{std::lock_guard<std::mutex>lk(g_state.mu);c=g_state.cfg;}if(argc>=3){c.x=_wtoi(argv[1]);c.y=_wtoi(argv[2]);}for(int i=1;i<argc;++i){if(wcsncmp(argv[i],L"--x=",4)==0)c.x=_wtoi(argv[i]+4);else if(wcsncmp(argv[i],L"--y=",4)==0)c.y=_wtoi(argv[i]+4);}normalize(c);{std::lock_guard<std::mutex>lk(g_state.mu);g_state.cfg=c;}LocalFree(argv);} 
} // ns

int WINAPI wWinMain(HINSTANCE hInst,HINSTANCE, PWSTR,int){
  g_inst=hInst;g_exeDir=exe_dir();g_cfgDir=g_exeDir+L"\\configs";g_webDir=g_exeDir+L"\\web";
  ensure_cfg();
  {std::lock_guard<std::mutex>lk(g_state.mu);g_state.active=L"default.ini";g_state.cfg=load_cfg(ppath(g_state.active));g_state.running=false;}
  apply_cli();
  start_overlay();post_sync();
  if(!start_server()){MessageBoxW(nullptr,L"启动 HTTP 服务失败（端口 5188）。",L"MyCross",MB_OK|MB_ICONERROR);stop_overlay();return 1;}
  wait_server(3000);g_lastPing=GetTickCount();
  if(!launch_ui()){wchar_t msg[256]={};swprintf(msg,256,L"启动应用窗口失败（错误码: %lu），已回退到默认浏览器。",g_launchErr);MessageBoxW(nullptr,msg,L"MyCross",MB_OK|MB_ICONWARNING);ShellExecuteW(nullptr,L"open",L"http://127.0.0.1:5188/",nullptr,nullptr,SW_SHOWNORMAL);g_appMode=false;}else g_appMode=true;

  while(!g_exit.load()){
    if(g_appMode){DWORD now=GetTickCount(),last=g_lastPing.load();if(now-last>HEARTBEAT_TIMEOUT_MS){g_exit=true;break;}}
    sockaddr_in ca={};int len=sizeof(ca);SOCKET c=accept(g_listen,(sockaddr*)&ca,&len);
    if(c==INVALID_SOCKET){int e=WSAGetLastError();if(e==WSAEWOULDBLOCK){Sleep(25);continue;}Sleep(25);continue;}
    handle(c);
  }

  stop_server();stop_ui();stop_overlay();
  return 0;
}
