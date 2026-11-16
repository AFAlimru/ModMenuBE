#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
namespace menu {
struct ImageIdentity {std::uint16_t machine{};std::uint32_t timestamp{},imageSize{},checksum{};};
enum class GameBuild {Unsupported,V45,V51,V52,V60};
struct NativeLayout {
 unsigned clientStack{1488},clientGame{424},gameStack{968},stackTop{424},gameTop{424};
 unsigned guiGetter{1744},appOrigin{1552},viewNameOwner{80},viewController{64};
 unsigned transitionA{1120},transitionB{1125},cursorX{828},cursorY{832};
 unsigned screenValid{152},screenGui{168},screenClip{120},uiMode{125};
};
NativeLayout const* LayoutFor(GameBuild) noexcept;
std::uint32_t ResolveNativeRva(GameBuild,std::uint32_t) noexcept;
bool AcceptedIdentity(ImageIdentity const&) noexcept;
bool MatchEntry(std::span<std::byte const>,std::span<std::byte const>) noexcept;
bool VerifyFile(std::span<std::byte const>) noexcept;
bool VerifyLoadedHeaders(std::span<std::byte const>,std::span<std::byte const>,std::uintptr_t) noexcept;
enum class ProfileFailure {None,Module,Path,Open,Map,Disk,LoadedRead,LoadedHeaders,EntryRead,EntryMismatch};
struct ProfileDiagnostic {ProfileFailure failure{};std::uint32_t systemError{},rva{};std::uintptr_t moduleBase{},headerBase{};std::array<std::byte,32> expected{},observed{};};
bool VerifyLoadedEntry(std::uint32_t,std::span<std::byte const>,std::span<std::byte const>,ProfileDiagnostic&) noexcept;
std::string DescribeProfileDiagnostic(ProfileDiagnostic const&);
struct EntryGuard {std::uint32_t rva;std::array<std::uint8_t,32> bytes;};
std::span<EntryGuard const> NativeGuards(GameBuild =GameBuild::V52);
class NativeProfile {
public:
 bool Initialize(void*,ProfileDiagnostic* =nullptr) noexcept;
 bool Entry(std::uintptr_t) const noexcept;
 bool Table(void const*,std::uint32_t) const noexcept;
 std::uintptr_t Virtual(void const*,unsigned) const noexcept;
 std::uintptr_t Base() const{return base_;}
 GameBuild Build() const{return build_;}
 char const* Version() const noexcept;
 NativeLayout const& Layout() const noexcept{return *LayoutFor(build_);}
 std::uintptr_t Address(std::uint32_t key) const noexcept{auto rva=ResolveNativeRva(build_,key);return rva?base_+rva:0;}
 template<class T>T Function(std::uint32_t rva) const{return reinterpret_cast<T>(Address(rva));}
private:
 GameBuild build_{GameBuild::V52};
 std::uintptr_t base_{};std::byte const* disk_{};std::size_t diskSize_{},textOffset_{},textSize_{};
 struct Section{std::uint32_t rva,raw,size,flags;};std::array<Section,96> sections_{};unsigned sectionCount_{};
};
}
