#include "chatapi.h"
#include <iostream>

using json = nlohmann::json;

ChatAPI::ChatAPI(const std::string& key,
                 const std::string& url,
                 const std::string& model_name,
                 const std::string& user_role)
    : api_key(key), api_url(url), model(model_name), role(user_role)
{}

// Method to send a message to the API
std::string ChatAPI::sendMessage(const std::string& message,
                                 int max_tokens,
                                 float temperature) {
    CURL* curl;
    CURLcode res;
    std::string readBuffer;

    // Initialize CURL
    curl = curl_easy_init();
    if (curl) {
        // Set up the JSON payload
        json payload = {
            {"model", model},
            {"messages", {
                             {{"role", role}, {"content", message}}
                         }},
            {"max_tokens", max_tokens},
            {"temperature", temperature}
        };

        // Set up headers
        struct curl_slist* headers = nullptr;
        headers = curl_slist_append(headers, "Content-Type: application/json");
        headers = curl_slist_append(headers, ("Authorization: Bearer " + api_key).c_str());

        // Set CURL options
        curl_easy_setopt(curl, CURLOPT_URL, api_url.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload.dump().c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);

        // Perform the request
        res = curl_easy_perform(curl);
        if (res != CURLE_OK) {
            std::cerr << "CURL error: " << curl_easy_strerror(res) << std::endl;
        }

        // Clean up
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
    }

    return readBuffer;
}

// Method to parse the API response and extract the assistant's reply
std::string ChatAPI::getAssistantReply(const std::string& jsonResponse) {
    try {
        json responseJson = json::parse(jsonResponse);
        return responseJson["choices"][0]["message"]["content"].get<std::string>();
    } catch (const std::exception& e) {
        std::cerr << "Error parsing JSON response: " << e.what() << std::endl;
        return "";
    }
}
