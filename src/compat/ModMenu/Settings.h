#pragma once
#include <filesystem>
#include <string>
#include <variant>
#include <vector>
#include <string_view>
#include <cstdint>
#include "modmenu/ModMenuBinding.h"
namespace menu {
enum class SettingType {Boolean,Integer,Choice,Key,Text,Slider,Binding};
// Internal legacy adapters only. The public passive v1 contract is unchanged.
enum class TextFormat {None,Delay,SlotMap,KeyList,Public};
enum class KeyFormat {Menu,AutoClick,AutoJumpReset};
struct SliderValue {std::int64_t ticks{};bool operator==(SliderValue const&) const=default;};
struct SliderSpec {bool decimal{};unsigned precision{};std::int64_t minimum{},maximum{},step{};};
using SettingValue=std::variant<bool,int,std::string,SliderValue>;
struct SettingTranslation {std::string en,tw,cn;};
struct SettingField {
    std::string path,label;SettingType type{};SettingValue value;
    int minimum{},maximum{};std::vector<std::string> options;
    SettingValue defaultValue{value};
    std::string category{"General"},description,unit;int step{1};
    bool external{};
    SettingTranslation labelText,categoryText,descriptionText;
    std::vector<SettingTranslation> optionText;
    TextFormat textFormat{};KeyFormat keyFormat{};
    SliderSpec slider;
    std::size_t minBytes{},maxBytes{4096};bool identifier{};
    bool legacyNumericStrings{};unsigned bindingModifiers{3};
};
// Internal source-backed description contract, not an external DLL ABI.
struct SettingsDefinition {
    unsigned version{1};std::string file{"config.json"};std::size_t limit{16384};
    std::vector<SettingField> fields;
    bool external{},hotReload{};
};
SettingsDefinition SettingsSchema(std::string const& id);
SettingsDefinition ParseSettingsDescription(std::string const& bytes,std::string const& id,std::string const& entry,std::string const& version);
bool SafeSettingsId(std::string_view);
bool ResetSetting(SettingField&);
struct SettingsDraft;
bool ResetSettings(SettingsDraft&);
bool SetInteger(SettingField&,std::string_view);
bool ValidSettingValue(SettingField const&);
bool SetText(SettingField&,std::string_view);
std::size_t SettingInputLimit(SettingField const&);
std::string SettingInputError(SettingField const&);
std::string SettingKeyCaption(SettingField const&);
bool AdjustSetting(SettingField&,int steps);
struct SettingsDraft {
    std::string id,original,message;
    std::vector<SettingField> fields;
    bool valid{},saved{};
    bool external{};
    bool hotReload{};
    std::string descriptionSource,manifestSource,manifestFile;
};
struct SettingsAvailability {bool available{};std::string reason;};
SettingsAvailability InspectSettings(std::filesystem::path const& mods,std::string const& id);
bool HasSettings(std::string const& id);
SettingsDraft LoadSettings(std::filesystem::path const& mods,std::string const& id);
SettingsDraft ApplySettings(std::filesystem::path const& mods,SettingsDraft const& draft);
std::string SettingText(SettingField const&);
bool SetKey(SettingField&,unsigned key);
bool SetBinding(SettingField&,modmenu::binding::Binding);
}
