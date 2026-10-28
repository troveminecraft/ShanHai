#include "BedrockSeed.h"
#include <cassert>
#include <iostream>
#include <optional>
#include <memory>
struct Entry { std::unique_ptr<std::variant<int, LevelSeed64>> mValue; };
struct Properties {
    Entry base, override;
    Entry const* _getValue(HashedString const& key) const {
        if(key.getString() != "seed") return nullptr;
        return override.mValue ? &override : base.mValue ? &base : nullptr;
    }
};
LevelSeed64 nativeSeed(uint64_t bits) { LevelSeed64 s; s.mValue=bits; return s; }
struct Settings { std::optional<LevelSeed64> mSeed; };
struct Legacy { LevelSeed64 getSeed() const { return nativeSeed(0xfedcba9876543210ull); } };
int main() {
    static_assert(HashedString::computeHash("seed")==0x92eb667ea331f1e6ull);
    Properties p;
    for(auto bits : {0ull,1ull,2934997776556350534ull,0x8000000000000000ull,0xffffffffffffffffull}) {
        p.base.mValue=std::make_unique<std::variant<int,LevelSeed64>>(nativeSeed(bits));
        auto actual=bedrock_compat::seed(p);
        assert(actual.value==bits && actual.to32BitRandomSeed()==uint32_t(bits));
        Settings settings{nativeSeed(bits)};
        assert(bedrock_compat::seed(settings).toUiString()==actual.toUiString());
    }
    assert(bedrock_compat::seed(p).toUiString()=="-1");
    p.override.mValue=std::make_unique<std::variant<int,LevelSeed64>>(nativeSeed(42));
    assert(bedrock_compat::seed(p).value==42);
    auto fails=[&]{try{bedrock_compat::seed(p);return false;}catch(std::runtime_error const&){return true;}};
    p.override.mValue=std::make_unique<std::variant<int,LevelSeed64>>(42);
    assert(fails()); // Wrong override type must not fall back to the base or zero.
    p.override.mValue.reset(); p.base.mValue.reset(); assert(fails());
    assert(bedrock_compat::seed(Legacy{}).value==0xfedcba9876543210ull);
    std::cout<<"Seed: 64-bit values, signed text, settings, overrides, missing/type failures passed\n";
}
