// gemini_chat_json_header.cpp
#include <iostream>
#include <string>
#include <vector>
#include <curl/curl.h>
#include "C:/json/json.hpp"   // change if you saved json.hpp elsewhere

using json = nlohmann::json;
using namespace std;

// CONFIG
static const string API_KEY   = "YOUR_API_KEY_HERE";
static const string CA_BUNDLE = "C:/curl/cacert.pem";   // your cacert.pem

// curl write callback
static size_t WriteCallback(void* contents, size_t size, size_t nmemb, string* s) {
    s->append((char*)contents, size * nmemb);
    return size * nmemb;
}

int main() {
    cout << "=== Gemini C++ Chat (json.hpp) ===\n";
    cout << "Type 'exit' to quit.\n\n";

    struct Msg { string role; string text; };
    vector<Msg> history;

    while (true) {
        cout << "You: ";
        string user;
        getline(cin, user);
        if (user == "exit") break;

        // push user message
        history.push_back({"user", user});

        // Build JSON request using nlohmann::json
        json request;
        request["contents"] = json::array();

        for (auto &m : history) {
            json entry;
            entry["role"] = m.role;
            entry["parts"] = json::array();
            entry["parts"].push_back( json::object({ {"text", m.text} }) );
            request["contents"].push_back(entry);
        }

        string body = request.dump(); // serializes with proper escaping

        // perform HTTP request with curl
        CURL* curl = curl_easy_init();
        if (!curl) {
            cerr << "curl init failed\n";
            return 1;
        }

        string response;
        struct curl_slist* headers = nullptr;
        headers = curl_slist_append(headers, "Content-Type: application/json");

        string url = "https://generativelanguage.googleapis.com/v1beta/models/"
                     "gemini-2.0-flash:generateContent?key=" + API_KEY;

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        curl_easy_setopt(curl, CURLOPT_CAINFO, CA_BUNDLE.c_str());
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);

        CURLcode res = curl_easy_perform(curl);
        curl_easy_cleanup(curl);
        curl_slist_free_all(headers);

        if (res != CURLE_OK) {
            cerr << "Request failed: " << curl_easy_strerror(res) << "\n";
            continue;
        }

        // Parse response JSON
        try {
            json root = json::parse(response);

            // If the API returns an error object, show it
            if (root.contains("error")) {
                cerr << "API error: " << root["error"].dump(2) << "\n";
                continue;
            }

            // Safe path to the model text:
            // root["candidates"][0]["content"]["parts"][0]["text"]
            string reply;
            if (root.contains("candidates") &&
                root["candidates"].is_array() &&
                !root["candidates"].empty()) {

                json cand = root["candidates"][0];
                if (cand.contains("content") &&
                    cand["content"].contains("parts") &&
                    cand["content"]["parts"].is_array() &&
                    !cand["content"]["parts"].empty() &&
                    cand["content"]["parts"][0].contains("text")) {

                    reply = cand["content"]["parts"][0]["text"].get<string>();
                }
            }

            if (reply.empty()) {
                // fallback: try to pretty-print entire response
                cout << "Could not find model text; raw response:\n";
                cout << root.dump(2) << "\n";
            } else {
                cout << "\nGemini:\n" << reply << "\n\n";
                history.push_back({"model", reply});
            }
        } catch (const std::exception &e) {
            cerr << "JSON parse error: " << e.what() << "\n";
            cerr << "Raw response:\n" << response << "\n";
        }
    }

    cout << "Chat ended.\n";
    return 0;
}
