#include "chatapi.h"

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

    curl = curl_easy_init();
    if (curl) {
        json payload = {
            {"model", model},
            {"messages", {
                             {{"role", role}, {"content", message}}
                         }},
            {"max_tokens", max_tokens},
            {"temperature", 0.7}
        };

        // Convert payload to a string
        std::string payloadStr = payload.dump();

        struct curl_slist* headers = nullptr;
        headers = curl_slist_append(headers, "Content-Type: application/json");
        headers = curl_slist_append(headers, ("Authorization: Bearer " + api_key).c_str());

        curl_easy_setopt(curl, CURLOPT_URL, api_url.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payloadStr.c_str()); // Pass the string directly
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L); // Set timeout to 30 seconds
        curl_easy_setopt(curl, CURLOPT_BUFFERSIZE, 102400L);  // 100 KB buffer
        //curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L); // Enable verbose output

        res = curl_easy_perform(curl);
        if (res != CURLE_OK) {
            std::cerr << "CURL error: " << curl_easy_strerror(res) << std::endl;
        }

        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
    }

    return readBuffer;
}

// Method to parse the API response and extract the assistant's reply
std::string ChatAPI::getAssistantReply(const std::string& jsonResponse) {
    try {
        json responseJson = json::parse(jsonResponse);

        // Check if the response contains an error
        if (responseJson.contains("error")) {
            std::string errorMessage = responseJson["error"]["message"].get<std::string>();
            std::cerr << "API Error: " << errorMessage << std::endl;
            return "";
        }

        // Extract the assistant's reply
        return responseJson["choices"][0]["message"]["content"].get<std::string>();
    } catch (const std::exception& e) {
        std::cerr << "Error parsing JSON response: " << e.what() << std::endl;
        return "";
    }
}
