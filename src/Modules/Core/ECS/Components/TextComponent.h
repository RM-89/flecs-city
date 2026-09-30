#pragma once

#include <cstring>

namespace fc
{

/// @brief Text component for debugging use.
struct TextComponent
{
    static constexpr size_t MAX_LENGTH = 256;
    char mText[MAX_LENGTH]{};
    
    TextComponent() = default;

    TextComponent(char* text)
    {
        if (text)
        {
            strncpy(mText, text, MAX_LENGTH - 1);
            mText[MAX_LENGTH - 1] = '\0';
        }
        else
        {
            mText[0] = '\0';
        }
    }

    TextComponent(std::string& text)
    {
        strncpy(mText, text.c_str(), MAX_LENGTH - 1);
        mText[MAX_LENGTH - 1] = '\0';
    }
};

}; // namespace fc
