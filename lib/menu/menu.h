#pragma once
#include <cstdint>
#include "hal_interface.h"
#include "session.h"

enum class MenuLevel : uint8_t {
    PARENT,
    FEATURE_SETTINGS,
    PARAM_SETTINGS
};

// Parent menu items
enum class ParentItem : uint8_t {
    FEATURE_SETTINGS = 0,
    PARAM_SETTINGS,
    EXIT,
    PARENT_COUNT
};

// Feature settings items
enum class FeatureItem : uint8_t {
    FORM_CHECK = 0,
    SHOT_FEEDBACK,
    BACK,
    FEATURE_COUNT
};

// Parameter settings items (same as old flat menu)
enum class ParamItem : uint8_t {
    CANT_BASELINE = 0,
    CANT_TOLERANCE,
    ELEV_BASELINE,
    ELEV_TOLERANCE,
    DRAW_BASELINE,
    DRAW_TOLERANCE,
    BACK,
    PARAM_COUNT
};

class Menu {
public:
    Menu(HalInterface* hal, ConfigStore* config);

    void enter();   // Activate menu, draw initial state
    void exit();    // Save and deactivate
    bool isActive() const { return active_; }

    // Button handlers — call these from main when menu is active
    void onButtonA();       // Next item / increment value (+0.5)
    void onButtonB();       // Select item / toggle / decrement value (-0.5)

    // Call every loop to handle edit timeout
    void update(uint32_t now);

    // Render menu to display
    void draw();

private:
    HalInterface* hal_;
    ConfigStore* config_;
    bool active_;
    MenuLevel level_;
    uint8_t selected_;     // Currently highlighted item
    bool editing_;         // True when adjusting a param value
    uint8_t scroll_top_;   // Top visible item index (for scrolling)

    static constexpr float STEP = 0.5f;
    static constexpr uint32_t EDIT_TIMEOUT_MS = 2000;
    static constexpr uint8_t VISIBLE_ITEMS = 4;

    uint32_t last_input_time_;

    uint8_t getItemCount() const;
    const char* getTitle() const;

    // Parent menu helpers
    const char* getParentLabel(ParentItem id) const;

    // Feature menu helpers
    const char* getFeatureLabel(FeatureItem id) const;
    bool getFeatureValue(FeatureItem id) const;
    void toggleFeature(FeatureItem id);

    // Param menu helpers
    const char* getParamLabel(ParamItem id) const;
    float getParamValue(ParamItem id) const;
    void setParamValue(ParamItem id, float val);
    float getParamMin(ParamItem id) const;
    float getParamMax(ParamItem id) const;

    void drawParent();
    void drawFeature();
    void drawParam();
};
