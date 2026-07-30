#pragma once
#include "../feature.h"

// Monitors ItemModule::ChangeResource. Tracks balances per resource type.
// Read-only — modifications trigger server error 700.
struct CurrencyFeature : Feature {
    CurrencyFeature();
    void Init() override;
    void OnUpdate() override;
    void OnMenu() override;
};
