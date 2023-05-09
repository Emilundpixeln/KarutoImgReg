#pragma once


void load_wl_data();

struct PredictResult
{
    std::string_view character;
    std::string_view series;
    int wl;
    int64_t date;
    float confidence;
};

PredictResult predict(std::string_view raw_series, std::string_view raw_character);