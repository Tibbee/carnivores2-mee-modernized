#pragma once

#include "Core/GameTypes.h"
#include <vector>
#include <stdexcept>
#include <limits>

class CharacterRoster;

// Compatibility indexing without exposing resize/erase to engine consumers.
template<class T> class RosterView {
    friend class CharacterRoster;
    std::vector<T>* records;
    explicit RosterView(std::vector<T>& value) : records(&value) {}
public:
    T& operator[](int index) const { return records->at(static_cast<size_t>(index)); }
    int size() const { return static_cast<int>(records->size()); }
};

class CharacterRoster {
    std::vector<TDinoInfo> definitions;
    std::vector<int> aggression;
    std::vector<TCharacterInfo> models;
    bool frozen = false;
public:
    CharacterRoster() = default;
    CharacterRoster(const CharacterRoster&) = delete;
    CharacterRoster& operator=(const CharacterRoster&) = delete;
    CharacterRoster(CharacterRoster&&) = delete;
    CharacterRoster& operator=(CharacterRoster&&) = delete;
    // Metadata only, not model payloads. Protect the x86 process from hostile
    // input without reinstating an arbitrary species-count limit.
    static constexpr size_t MetadataBudget = 128u * 1024u * 1024u;
    static constexpr size_t MetadataBytesPerEntry =
        sizeof(TDinoInfo) + sizeof(int) + sizeof(TCharacterInfo);
    static constexpr size_t MaxDefinitions = MetadataBudget / (2 * MetadataBytesPerEntry);
    RosterView<TDinoInfo> Definitions() { return RosterView<TDinoInfo>(definitions); }
    RosterView<int> Aggression() { return RosterView<int>(aggression); }
    RosterView<TCharacterInfo> Models() { return RosterView<TCharacterInfo>(models); }
    int size() const { return static_cast<int>(definitions.size()); }
    bool Frozen() const { return frozen; }

    // Only before publishing consumers, or after all resources/consumers retire.
    void Reset() {
        models.clear();
        definitions.clear();
        aggression.clear();
        frozen = false;
    }
    void BeginDefinition(int index) {
        if (frozen) throw std::logic_error("Character roster is frozen");
        if (index < 0 || index > size() || index + 1 < size())
            throw std::out_of_range("Invalid character definition index");
        if (index < size()) return; // pending record used by line-reader seams
        if (definitions.size() >= MaxDefinitions)
            throw std::length_error("Character roster exceeds 128 MiB metadata budget (including growth)");
        definitions.emplace_back();
        try { aggression.push_back(0); }
        catch (...) { definitions.pop_back(); throw; }
        auto& entry = definitions.back();
        entry.Scale0 = 800;
        entry.ScaleA = 600;
        entry.ShDelta = 0;
    }
    void Finalize() {
        if (frozen) throw std::logic_error("Character roster already finalized");
        if (definitions.empty()) throw std::runtime_error("Character roster has no hunter entry");
        models.resize(definitions.size());
        frozen = true;
    }
};

extern CharacterRoster g_CharacterRoster;
extern RosterView<TDinoInfo> DinoInfo;
extern RosterView<int> DinoAggressMulti;
extern RosterView<TCharacterInfo> ChInfo;
