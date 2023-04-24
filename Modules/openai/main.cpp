#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
    #include <windows.h>
#endif

extern "C"
{
    #include "lua.h"
    #include "lauxlib.h"
}
#include <string>
#include <curl/curl.h>
#include "json.hpp"

std::string APIKEY = "";
std::string logtext;

#ifndef _DEBUG
BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        curl_global_init(0);
        break;
    case DLL_PROCESS_DETACH:
        curl_global_cleanup();
        break;
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
        break;
    }
    return TRUE;
}
#endif

void Print(const std::string& s)
{
    OutputDebugStringA(s.c_str());
    printf((s + std::string("\n")).c_str());
    if (not logtext.empty()) logtext += "\n";
    logtext += s;
}

int openai_setapikey(lua_State* L)
{
    APIKEY.clear();
    if (lua_isstring(L, -1)) APIKEY = lua_tostring(L, -1);
    return 0;
}

size_t WriteCallback(char* contents, size_t size, size_t nmemb, void* userp)
{
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

// trim from start (in place)
static inline void ltrim(std::string& s) {
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) {
        return !std::isspace(ch);
        }));
}

// trim from end (in place)
static inline void rtrim(std::string& s) {
    s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) {
        return !std::isspace(ch);
        }).base(), s.end());
}

// trim from both ends (in place)
static inline void trim(std::string& s) {
    rtrim(s);
    ltrim(s);
}

bool openai_newimage(const int width, const int height, const std::string& prompt, const std::string& path)
{
    bool success = false;
    if (width != height or (width != 256 and width != 512 and width != 1024))
    {
        Print("Error: Image dimensions must be 256x256, 512x512, or 1024x1024.");
        return false;
    }

    std::string imageurl;

    if (APIKEY.empty()) return 0;

    std::string url = "https://api.openai.com/v1/images/generations";
    std::string readBuffer;
    std::string bearerTokenHeader = "Authorization: Bearer " + APIKEY;
    std::string contentType = "Content-Type: application/json";

    auto curl = curl_easy_init();

    struct curl_slist* headers = NULL;
    headers = curl_slist_append(headers, bearerTokenHeader.c_str());
    headers = curl_slist_append(headers, contentType.c_str());

    nlohmann::json j3;
    j3["prompt"] = prompt;
    j3["n"] = 1;

    switch (width)
    {
    case 256:
        j3["size"] = "256x256";
        break;
    case 512:
        j3["size"] = "512x512";
        break;
    case 1024:
        j3["size"] = "1024x1024";
        break;
    }
    std::string postfields = j3.dump();

    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postfields.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);

    auto errcode = curl_easy_perform(curl);
    if (errcode == CURLE_OK)
    {
        //OutputDebugStringA(readBuffer.c_str());
        trim(readBuffer);
        if (readBuffer.size() > 1 and readBuffer[0] == '{' and readBuffer[readBuffer.size() - 1] == '}')
        {
            j3 = nlohmann::json::parse(readBuffer);
            if (j3.is_object())
            {
                if (j3["error"].is_object())
                {
                    if (j3["error"]["message"].is_string())
                    {
                        std::string msg = j3["error"]["message"];
                        msg = "Error: " + msg;
                        Print(msg.c_str());
                    }
                    else
                    {
                        Print("Error: Unknown error.");
                    }
                }
                else
                {
                    if (j3["data"].is_array() or j3["data"].size() == 0)
                    {
                        if (j3["data"][0]["url"].is_string())
                        {
                            std::string s = j3["data"][0]["url"];
                            imageurl = s; // I don't know why the extra string is needed here...

                            readBuffer.clear();

                            auto curl2 = curl_easy_init();
                            curl_easy_setopt(curl2, CURLOPT_URL, imageurl.c_str());
                            curl_easy_setopt(curl2, CURLOPT_WRITEFUNCTION, WriteCallback);
                            curl_easy_setopt(curl2, CURLOPT_WRITEDATA, &readBuffer);

                            auto errcode = curl_easy_perform(curl2);
                            if (errcode == CURLE_OK)
                            {
                                FILE* file = fopen(path.c_str(), "wb");
                                if (file == NULL)
                                {
                                    Print("Error: Failed to write file.");
                                }
                                else
                                {
                                    auto w = fwrite(readBuffer.c_str(), 1, readBuffer.size(), file);
                                    if (w == readBuffer.size())
                                    {
                                        success = true;
                                    }
                                    else
                                    {
                                        Print("Error: Failed to write file data.");
                                    }
                                    fclose(file);
                                }
                            }
                            else
                            {
                                Print("Error: Failed to download image.");
                            }

                            curl_easy_cleanup(curl2);
                        }
                        else
                        {
                            Print("Error: Image URL missing.");
                        }
                    }
                    else
                    {
                        Print("Error: Data is not an array, or data is empty.");
                    }
                }
            }
            else
            {
                Print("Error: Response is not a valid JSON object.");
                Print(readBuffer);
            }
        }
        else
        {
            Print("Error: Response is not a valid JSON object.");
            Print(readBuffer);
        }
    }
    else
    {
        Print("Error: Request failed.");
    }

    curl_easy_cleanup(curl);

    return success;
}

int openai_newimage(lua_State* L)
{
    int width, height;
    std::string prompt, path;

    //Get function arguments
    if (lua_isnumber(L, -4))
    {
        width = lua_tointeger(L, -4);
    }
    else
    {
        Print("Error: Expected argument #1 to be a number.");
        return 0;
    }

    if (lua_isnumber(L, -3))
    {
        height = lua_tointeger(L, -3);
    }
    else
    {
        Print("Error: Expected argument #2 to be a number.");
        return 0;
    }

    if (lua_isstring(L, -2))
    {
        prompt = lua_tostring(L, -2);
    }
    else
    {
        Print("Error: Expected argument #2 to be a string.");
        return 0;
    }

    if (lua_isstring(L, -1))
    {
        path = lua_tostring(L, -1);
    }
    else
    {
        Print("Error: Expected argument #4 to be a string.");
        return 0;
    }

    //Execute the command
    bool success = openai_newimage(width, height, prompt, path);

    //Push the return value onto the stack
    lua_pushboolean(L, success);

    return 1;
}

int openai_getlog(lua_State* L)
{
    lua_pushstring(L, logtext.c_str());
    logtext.clear();
    return 1;
}

extern "C"
{
    __declspec(dllexport) int luaopen_openai(lua_State* L)
    {
        lua_newtable(L);
        int sz = lua_gettop(L);
        lua_pushcfunction(L, openai_newimage);   lua_setfield(L, -2, "newimage");
        lua_pushcfunction(L, openai_setapikey);   lua_setfield(L, -2, "setapikey");
        lua_pushcfunction(L, openai_getlog);   lua_setfield(L, -2, "getlog");
        lua_settop(L, sz);
        return 1;
    }
}

//--------------------------------------------
// For testing as executable...
//--------------------------------------------

#ifdef _DEBUG
int main(int argc, char* argv[])
{
    //Set your API key here
    APIKEY = "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";

    std::string path = "C:/ProgramData/Ultra Engine/openai.png";

    int width = 256;
    int height = 256;
    std::string prompt = "tiling seamless texture realistic castle wall";

    bool success = openai_newimage(width, height, prompt, path);

    return 0;
}
#endif