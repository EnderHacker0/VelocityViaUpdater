#include <iostream>
#include <fstream>
#include <string>
#include <thread>
#include <map>
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <cstdio>
using json = nlohmann::json;
struct Plugin {
    std::string project;
    std::string jar_path;
    std::string current_version;
    std::string latest_version;
    std::string download_url;
};
int main() {
    const std::string version_file =
        "/home/minecraft/velocity/plugins/via.txt"; //replace this with actual path
    std::map<std::string, std::string> plugins_info = {
        {"ViaVersion",   "/home/minecraft/velocity/plugins/ViaVersion.jar"}, //replace this with actual path
        {"ViaBackwards", "/home/minecraft/velocity/plugins/ViaBackwards.jar"}, //replace this with actual path
        {"ViaRewind",    "/home/minecraft/velocity/plugins/ViaRewind.jar"} //replace this with actual path
    };

    CURL* curl = curl_easy_init();
    if (!curl) {
        std::cerr << "Failed to initialize curl\n";
        return 1;
    }
    while (true) {
        std::map<std::string, std::string> current_versions;
        std::ifstream infile(version_file);
        if (infile.is_open()) {
            std::string line;
            while (std::getline(infile, line)) {
                auto pos = line.find(':');
                if (pos != std::string::npos) {
                    std::string name = line.substr(0, pos);
                    std::string ver  = line.substr(pos + 1);
                    ver.erase(0, ver.find_first_not_of(" \t"));
                    ver.erase(ver.find_last_not_of(" \t") + 1);

                    current_versions[name] = ver;
                }
            }
            infile.close();
        }
        for (auto& [project, jar_path] : plugins_info) {
            Plugin p;
            p.project = project;
            p.jar_path = jar_path;
            p.current_version = current_versions[project];
            std::cout << "\nChecking " << project << "...\n";
            std::string api_url =
                "https://hangar.papermc.io/api/v1/projects/"
                + project +
                "/versions";
            std::string response_string;
            curl_easy_reset(curl);
            curl_easy_setopt(curl, CURLOPT_URL, api_url.c_str());
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,
                +[](void* contents, size_t size, size_t nmemb, void* userp) -> size_t {
                    std::string* s = static_cast<std::string*>(userp);
                    s->append(static_cast<char*>(contents), size * nmemb);
                    return size * nmemb;
                });
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_string);
            curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
            curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15L);
            curl_easy_setopt(curl, CURLOPT_USERAGENT, "via-updater/1.0");
            CURLcode res = curl_easy_perform(curl);
            if (res != CURLE_OK) {
                std::cerr << project << " request failed: "
                          << curl_easy_strerror(res) << "\n";
                continue;
            }
            try {
                json j = json::parse(response_string);
                if (!j.contains("result") ||
                    !j["result"].is_array() ||
                    j["result"].empty()) {

                    std::cerr << "No versions found for " << project << "\n";
                    continue;
                }
                p.latest_version = j["result"][0]["name"].get<std::string>();
                p.download_url =
                    "https://hangar.papermc.io/api/v1/projects/"
                    + project +
                    "/versions/"
                    + p.latest_version +
                    "/VELOCITY/download";

            } catch (const std::exception& e) {
                std::cerr << "JSON parse error for " << project
                          << ": " << e.what() << "\n";
                continue;
            }
            if (p.latest_version == p.current_version) {
                std::cout << project << " is up to date: "
                          << p.current_version << "\n";
                continue;
            }
            std::cout << "Updating " << project
                      << " from " << p.current_version
                      << " to " << p.latest_version << "\n";
            std::remove(p.jar_path.c_str());
            FILE* fp = fopen(p.jar_path.c_str(), "wb");
            if (!fp) {
                perror("Failed to create jar file");
                continue;
            }
            curl_easy_reset(curl);
            curl_easy_setopt(curl, CURLOPT_URL, p.download_url.c_str());
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,
                +[](void* ptr, size_t size, size_t nmemb, void* userdata) -> size_t {
                    return fwrite(ptr, size, nmemb,
                                  static_cast<FILE*>(userdata));
                });
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
            curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
            curl_easy_setopt(curl, CURLOPT_TIMEOUT, 60L);
            curl_easy_setopt(curl, CURLOPT_USERAGENT, "via-updater/1.0");
            CURLcode dl = curl_easy_perform(curl);
            fclose(fp);
            if (dl != CURLE_OK) {
                std::cerr << "Download failed for " << project
                          << ": " << curl_easy_strerror(dl) << "\n";
                continue;
            }
            std::cout << "Downloaded " << project << " successfully\n";
            current_versions[project] = p.latest_version;
        }
        std::ofstream out(version_file, std::ios::trunc);
        if (out.is_open()) {
            for (auto& [name, ver] : current_versions) {
                out << name << ": " << ver << "\n";
            }
            out.close();
        }
        std::cout << "\nSleeping 60 seconds...\n";
        std::this_thread::sleep_for(std::chrono::seconds(60));
    }
    curl_easy_cleanup(curl);
    return 0;
}