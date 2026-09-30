/*
 * SPDX-FileCopyrightText: 2023-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "systems/phone/esp_brookesia_phone_app.hpp"

namespace esp_brookesia::apps {

/**
 * @brief SpecAnalyzer application for displaying the app name on the device screen
 * 
 */
class SpecAnalyzer: public systems::phone::App {
public:
    /**
     * @brief Get the singleton instance of SpecAnalyzer
     * 
     * @param use_status_bar Show status bar
     * @param use_navigation_bar Show navigation bar
     * @return SpecAnalyzer* Singleton instance pointer
     */
    static SpecAnalyzer *requestInstance(bool use_status_bar = false, bool use_navigation_bar = false);

    /**
     * @brief Destroy the SpecAnalyzer object
     * 
     */
    ~SpecAnalyzer();

protected:
    /**
     * @brief Construct a new SpecAnalyzer object (private to enforce singleton)
     * 
     * @param use_status_bar Show status bar
     * @param use_navigation_bar Show navigation bar
     */
    SpecAnalyzer(bool use_status_bar, bool use_navigation_bar);

    bool run(void) override;
    bool back(void) override;
    bool close(void) override;
    bool init(void) override;
    bool deinit(void) override;
    bool pause(void) override;
    bool resume(void) override;

private:
    static SpecAnalyzer *_instance;
};

} // namespace esp_brookesia::apps