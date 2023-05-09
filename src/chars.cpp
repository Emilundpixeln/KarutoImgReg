#include <boost/json.hpp>
#include <fstream>
#include <spdlog/spdlog.h>
#include "chars.h"
#include "levenshtein.h"
#include <chrono>
#include <iostream>

using namespace std::chrono;
namespace json = boost::json;

json::value parse_file(char const* filename)
{
    std::ifstream f(filename);
    json::stream_parser p;
    json::error_code ec;
    do
    {
        char buf[4096];
        f.read((char*)buf, sizeof(buf));

        std::streamsize nread = f.gcount();

        p.write(buf, nread, ec);
    } while (!f.eof());
    if (ec)
        return nullptr;
    p.finish(ec);
    if (ec)
        return nullptr;
    return p.release();
}

json::value wl_json;
struct Character
{
    std::string_view character;
    int wl;
    int64_t date;
};

struct Series
{
    std::string_view series;
    std::vector<Character> chars;
};
std::vector<Series> wl_data;

void load_wl_data()
{
    wl_json = parse_file("wl_data.json");
    const auto& obj = wl_json.as_object();

    wl_data.reserve(obj.size());
    for (const auto& ser : obj)
    {
        const auto& ser_obj = ser.value().as_object();
        std::vector<Character> chars;
        chars.reserve(ser_obj.size());

        for (const auto& chara : ser_obj)
        {
            const auto& chara_obj = chara.value().as_object();
            const auto& wl = chara_obj.at("wl");
            const auto& date = chara_obj.at("date");
            if (!wl.is_int64()) {
                spdlog::critical("load_wl_data(): wl is not int64! ({} {})", ser.key(), chara.key());
            } 
            if (!date.is_int64()) {
                spdlog::critical("load_wl_data(): date is not int64! ({} {})", ser.key(), chara.key());
            }
            chars.push_back({ chara.key(), (int)wl.as_int64(), date.as_int64() });
        }
      
        wl_data.push_back({ ser.key(), std::move(chars)});
    }
}

PredictResult predict(std::string_view raw_series, std::string_view raw_character)
{
    auto start = high_resolution_clock::now();
    std::vector<std::pair<int/* index*/, int/* distance*/>> series_distances(wl_data.size());

    for (int i = 0; i < wl_data.size(); i++)
    {
        series_distances[i] = std::make_pair(i, levenshtein_distance(wl_data[i].series, raw_series));
    }

    auto middle = std::min((size_t)10, series_distances.size());
    std::partial_sort(series_distances.begin(), series_distances.begin() + middle, series_distances.end(),
        [](const std::pair<int, int>& a, const std::pair<int, int>& b) {
            return a.second < b.second;
        });

    auto got_series_distances = high_resolution_clock::now();
    std::cout << "[DBG] predict finished series_distances in" << duration_cast<milliseconds>(got_series_distances - start).count() << "ms\n";
    std::vector<std::pair<int/* index*/, int/* distance*/>> char_distances;

    int best_series = -1;
    int best_character = -1;
    int best_total_distance = std::numeric_limits<size_t>::max();

    for (int i = 0; i < middle; i++)
    {
        const auto& chars = wl_data[series_distances[i].first].chars;
        size_t series_distance = series_distances[i].second;
        if (series_distance >= best_total_distance)
        {
            // done since series are sorted and character distance is >= 0;
            break;
        }
 
        for (int j = 0; j < chars.size(); j++)
        {
            auto d = levenshtein_distance(chars[j].character, raw_character) + series_distance;
            if (d < best_total_distance) {
                best_total_distance = d;
                best_character = j;
                best_series = i;
            }
        }

    }
    if (best_series == -1 || best_character == -1) {
        spdlog::critical("predict(): No chars found!");
    }
    PredictResult r;
    const auto& ser = wl_data[series_distances[best_series].first];
    const auto& chara = ser.chars[best_character];
    r.character = chara.character;
    r.series = ser.series;
    r.wl = chara.wl;
    r.date = chara.date;
    r.confidence = 1.0 - (float)best_total_distance / (std::max(chara.character.size(), raw_character.size()) + std::max(ser.series.size(), raw_series.size()));

    auto end = high_resolution_clock::now();
    std::cout << "[DBG] predict finished in" << duration_cast<milliseconds>(end - got_series_distances).count() << "ms\n";
    return r;
}
