#include <iostream>
#include <spdlog/spdlog.h>
#include <tesseract/baseapi.h>
#include <leptonica/allheaders.h>
#include <array>
#include <chrono>
#include "chars.h"
#include "request.h"
#include <pix.h>
#include <spdlog/fmt/fmt.h>

std::array<int, 4> lefts = {
    54,
    329,
    602,
    877
};
int width = 173;
int top_a = 63;
int top_b = 315;
int height = 42;

int corr_left = 0;

std::array<int, 4> corr_tops_chars = {
    816,
    1750,
    2692,
    3632
};
int corr_top_series_offset = 80;

std::string_view trim(std::string_view in) {
    for (; in.size() > 0 && isspace(in[0]); in.remove_prefix(1));
    for (; in.size() > 0 && isspace(in.back()); in.remove_suffix(1));
    return in;
}

void replaceAll(std::string& str, const std::string& from, const std::string& to) {
    if (from.empty())
        return;
    size_t start_pos = 0;
    while ((start_pos = str.find(from, start_pos)) != std::string::npos) {
        str.replace(start_pos, from.length(), to);
        start_pos += to.length(); // In case 'to' contains 'from', like replacing 'x' with 'yx'
    }
}

int main(int argc, char** argv) {
 
    int cursor = 1;
    bool keepalive = false;
    if (argc > cursor && strcmp(argv[cursor], "-keepalive") == 0)
        keepalive = true;


    auto start = std::chrono::high_resolution_clock::now();

    load_wl_data();
    std::unique_ptr<tesseract::TessBaseAPI> api = std::make_unique<tesseract::TessBaseAPI>();
    // Initialize tesseract-ocr with English, without specifying tessdata path
    if (api->Init(NULL, "eng")) {
        fprintf(stderr, "Could not initialize tesseract.\n");
        exit(1);
    }

    api->SetPageSegMode(tesseract::PageSegMode::PSM_SINGLE_BLOCK);
    api->SetVariable("tessedit_char_whitelist", "6314Cicad#ompsRurkSn YevM'5JjAtH-W0NhgOTFwbKIyVlqGBzDPL=xöf78*:U./QE29Z&+()ü%–Xéá!,?×ôëâ[]²\"è~º;_³ÉíäðúóÓåç@ï°ÖòßñàÜµ{}êýÄû—½øìþ$");
    std::string input;
    do {
        if (keepalive)
        {
            std::cin >> input;
            spdlog::info("Input: {}", input);
        }
        auto start_cycle = std::chrono::high_resolution_clock::now();

        std::string_view url = keepalive ? input.c_str() : (argc > 1 ? argv[1] : "https://cdn.discordapp.com/attachments/932713994886721576/1034886603568594984/card.webp");
        Pix* image = load_image_from_url(url);

        if (!image) {
            std::cout << "Error: Couldn't load image!\nEOF\n";
            std::cout.flush();
            continue;
        }

#ifdef _DEBUG
            pixWriteAutoFormat("input.png", image);
#endif

        std::cout << "[DBG] Loaded Image after " << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - start_cycle).count() << "ms\n";
        float resolution_ratio = (float)image->h / 419.0f;

        bool corrupted = false;
        if (image->w * 2 < image->h) {
            resolution_ratio = (float)image->h / 2835.0f;
            corrupted = true;
            /*
            std::cout << "Invalid Dimensions!\n";
            std::cout.flush();
            pixDestroy(&image);
            continue;*/
        }

        bool four_drop = corrupted ? image->w < 1200 * resolution_ratio : image->w > 1000 * resolution_ratio;
        const int cards = four_drop ? 4 : 3;
       
        std::array<std::string, 4> raw_chars;
        std::array<std::string, 4> raw_series;
        std::cout << cards << '\n';
        for (int i = 0; i < cards; i++)
        {
            for (int j = 0; j < 2; j++)
            {

              //  printf("New IMAGE:\n\n");
                int left = corrupted ? corr_left : lefts[i];
                int top = corrupted ? corr_tops_chars[i] + j * corr_top_series_offset : (j == 0 ? top_a : top_b);
                Box* box = boxCreate(left * resolution_ratio, top * resolution_ratio, width * resolution_ratio, height * resolution_ratio);

                Pix* cropped = pixClipRectangle(image, box, nullptr);

                float contrast = 2;


                Pix* gray = nullptr;
                if (!corrupted) {
                    gray = pixConvertRGBToLuminance(cropped);
                    gray = pixContrastTRC(gray, gray, contrast);
                }
                
                api->SetImage(corrupted ? cropped : gray);
                api->SetVariable("user_defined_dpi", "70");
                // Get OCR result
                if (api->Recognize(nullptr)) {
                    spdlog::error("api->Recognize failed!");
                }
                /*
                tesseract::ResultIterator* ri = api->GetIterator();
                tesseract::ResultIterator* ri_words = api->GetIterator();
                tesseract::PageIteratorLevel level = tesseract::RIL_SYMBOL;
                if (ri != 0) {
                    do {
                        const char* sym = ri->GetUTF8Text(tesseract::RIL_SYMBOL);
                        float conf = ri->Confidence(tesseract::RIL_SYMBOL);
                        if (ri->Cmp(*ri_words) >= 0) {
                      //      printf("New word:\n");
                            ri_words->Next(tesseract::RIL_WORD);
                        }
                        int x1, y1, x2, y2;
                        ri->BoundingBox(tesseract::RIL_SYMBOL, &x1, &y1, &x2, &y2);
                    //    printf("Symbol: '%s';  \tconf: %.2f; BoundingBox: %d,%d,%d,%d;\n", sym, conf, x1, y1, x2, y2);
                        delete[] sym;
                    } while (ri->Next(tesseract::RIL_SYMBOL));
                delete ri;
                delete ri_words;
                }*/
                auto text = api->GetUTF8Text();
                if (j == 0)
                    raw_chars[i] = std::string(text);
                else
                    raw_series[i] = std::string(text);

                delete[] text;

#ifdef _DEBUG
                    auto file_name = fmt::format("img_{}_{}.png", i, j == 0 ? "chr" : "ser");
                    pixWriteAutoFormat(file_name.c_str(), corrupted ? cropped : gray);
#endif
                



                if (gray)
                    pixDestroy(&gray);

                pixDestroy(&cropped);
                //delete[] outText;
                boxDestroy(&box);
            }
            replaceAll(raw_series[i], "\n", " ");
            replaceAll(raw_chars[i], "\n", " ");
            auto trimmed_series = trim(raw_series[i]);
            auto trimmed_char = trim(raw_chars[i]);
            auto res = predict(trimmed_series, trimmed_char);
         //   spdlog::info("Predict {}: {} {} Confidence: {} Raw: {} {}", i, res.character, res.series, res.confidence, raw_chars[i], raw_series[i]);
            std::cout << res.character << '\t' << res.series << '\t' 
                << res.wl << '\t' << res.date << '\t' << res.confidence << '\t' 
                << trimmed_char << '\t' << trimmed_series << '\n';
        }
        std::cout << "[DBG] Took" << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - start_cycle).count() << "ms\n";
        std::cout << "EOF\n";
        std::cout.flush();
        pixDestroy(&image);
    } while (keepalive);

    api->End();


    spdlog::info("Program took {}ms", std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - start).count());
    return 0;
}