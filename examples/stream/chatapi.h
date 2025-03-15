#include <string>
#include <curl/curl.h>
#include <nlohmann/json.hpp>

const std::string OPENAI_API_KEY = "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";
const std::string DEEPSEEK_API_KEY = "sk-e721f7e4fe834b969e1ea5113f0e3d3f";

class ChatAPI {
private:
    std::string api_key;
    std::string api_url;
    std::string model;
    std::string role; // Default role for the user message

    // Callback function to handle HTTP response data
    static size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* s) {
        size_t newLength = size * nmemb;
        try {
            s->append((char*)contents, newLength);
        } catch (std::bad_alloc& e) {
            return 0; // Memory allocation error
        }
        return newLength;
    }

public:
    // Constructor
    ChatAPI(const std::string& key, const std::string& url, const std::string& model_name = "gpt-3.5-turbo", const std::string& user_role = "user");

    std::string sendMessage(const std::string& message, int max_tokens = 150, float temperature = 0.7);

    std::string getAssistantReply(const std::string& jsonResponse);

    // Setters for API configuration
    void setApiKey(const std::string& key) { api_key = key; }
    void setApiUrl(const std::string& url) { api_url = url; }
    void setModel(const std::string& model_name) { model = model_name; }
    void setRole(const std::string& user_role) { role = user_role; }

};
