/*
 * Copyright 2026, Kris Beazley Cricket@epluribusunix.net
 * All rights reserved. Distributed under the terms of the MIT license.
 */

// Haiku Core Application & System Kit Headers
#include <Application.h>
#include <Window.h>
#include <View.h>
#include <Font.h>
#include <OS.h>
#include <SupportKit.h>
#include <Roster.h>
#include <TranslationUtils.h> 
#include <Slider.h> 

// Storage, Path Finder & System File Kits
#include <Directory.h>
#include <Entry.h>
#include <FindDirectory.h>
#include <NodeInfo.h>
#include <Path.h>
#include <FilePanel.h>

// Vector Graphics & Interface Icons Elements
#include <IconUtils.h>   
#include "icons.h"
#include <Bitmap.h>   

// Interface Controls & Layout Managers
#include <Button.h>
#include <TextControl.h>
#include <TextView.h>
#include <ListView.h>
#include <OutlineListView.h>
#include <ScrollView.h>
#include <PopUpMenu.h>
#include <MenuItem.h>
#include <LayoutBuilder.h>
#include <SplitView.h>
#include <Alert.h>
#include <CheckBox.h>
#include <MenuField.h>
#include <Cursor.h>  

// Data Types, Sockets & Networking Kit Headers
#include <String.h>
#include <NetworkAddress.h>
#include <SecureSocket.h>
#include <Socket.h>

// Standard C/C++ STL & POSIX Includes
#include <stdio.h>
#include <unistd.h>
#include <fstream>
#include <map>
#include "nlohmann/json.hpp"



namespace AppInfo {
    static const char* const VERSION_STRING = "Cricket IRC Client v.0.0.11 (Haiku OS)";
}

using json = nlohmann::json;

struct ServerConfig {
    std::string name;
    std::string host;
    uint16 port;
    std::string nick;
    std::string altNick; 
    std::string altNick2; 
    std::string pass;
    std::vector<std::string> autojoin; 
    bool autoConnect; 
    bool autoReconnect;
    bool hideStatusMessages = false;
    bool enableEmoticons;    
    std::string backgroundImagePath; 
    int32 backgroundOpacity; 
};

struct Config {
    bool debugEnable = false;
    std::vector<ServerConfig> servers;
    std::vector<ServerConfig> customServers; 
    int32 serverListFontSize = 12;
    int32 chatLogFontSize = 12;
    int32 userListFontSize = 12;
    std::string quitMessage = AppInfo::VERSION_STRING; 
    std::string awayMessage = "I am away from my computer right now."; 
    bool useCustomDrawFunction = true; 
} cfg;

int selectedConfig = 0;

void ensure_config_dir() {
    BPath path;
    if (find_directory(B_USER_SETTINGS_DIRECTORY, &path) == B_OK) {
        path.Append("cricket");
        create_directory(path.Path(), 0755);
    }
}

void save_config() {
    ensure_config_dir();
    json j;
    j["debugEnable"] = cfg.debugEnable;
    j["serverListFontSize"] = cfg.serverListFontSize;
    j["chatLogFontSize"] = cfg.chatLogFontSize;
    j["userListFontSize"] = cfg.userListFontSize;
    j["quitMessage"] = cfg.quitMessage;
    j["awayMessage"] = cfg.awayMessage;
    j["useCustomDrawFunction"] = cfg.useCustomDrawFunction;
    
    // 1. Save standard/default servers
    json serverArray = json::array();
    for (const auto& srv : cfg.servers) {
        json s;        
        s["name"] = srv.name;
        s["host"] = srv.host;
        s["port"] = srv.port;
        s["nick"] = srv.nick;
        s["altNick"] = srv.altNick;
        s["altNick2"] = srv.altNick2; 
        s["pass"] = srv.pass;
        s["autoConnect"] = srv.autoConnect; 
        s["autoReconnect"] = srv.autoReconnect;
        s["hideStatusMessages"] = srv.hideStatusMessages; 
        s["background_image"] = srv.backgroundImagePath; 
        s["bg_opacity"] = srv.backgroundOpacity; 
        s["enable_emoticons"] = srv.enableEmoticons; 
        json ajArray = json::array();
        for (const auto& chan : srv.autojoin) {
            ajArray.push_back(chan);
        }
        s["autojoin"] = ajArray;
        serverArray.push_back(s);
    }
    j["servers"] = serverArray;

    // 2. Save custom servers into their own isolated JSON array
    json customServerArray = json::array();
    for (const auto& srv : cfg.customServers) {
        json s;        
        s["name"] = srv.name;
        s["host"] = srv.host;
        s["port"] = srv.port;
        s["nick"] = srv.nick;        
        s["altNick"] = srv.altNick;
        s["altNick2"] = srv.altNick2; 
        
        s["pass"] = srv.pass;
        s["autoConnect"] = srv.autoConnect; 
        s["autoReconnect"] = srv.autoReconnect;
        s["hideStatusMessages"] = srv.hideStatusMessages; 
        s["background_image"] = srv.backgroundImagePath;
        s["bg_opacity"] = srv.backgroundOpacity; 
        s["enable_emoticons"] = srv.enableEmoticons;  
        json ajArray = json::array();
        for (const auto& chan : srv.autojoin) {
            ajArray.push_back(chan);
        }
        s["autojoin"] = ajArray;
        customServerArray.push_back(s);
    }
    j["custom_servers"] = customServerArray;

    BPath path;
    if (find_directory(B_USER_SETTINGS_DIRECTORY, &path) == B_OK) {
        path.Append("cricket/cricketConfig.txt");
        std::ofstream outfile(path.Path());
        if (outfile.is_open()) {
            outfile << j.dump(4);
            outfile.close();
        }
    }
}


void load_config() {
    ensure_config_dir();
    cfg.debugEnable = false;
    cfg.useCustomDrawFunction = true;

    cfg.servers.clear(); 
    cfg.customServers.clear();
	
    BPath path;
    bool mustSaveDefaults = false;

    if (find_directory(B_USER_SETTINGS_DIRECTORY, &path) == B_OK) {
        path.Append("cricket/cricketConfig.txt");
        std::ifstream infile(path.Path());        
        if (infile.is_open()) {
            try {
                json j = json::parse(infile);
                cfg.quitMessage = j.value("quitMessage", AppInfo::VERSION_STRING);
                cfg.awayMessage = j.value("awayMessage", "I am away from my computer right now.");
                cfg.debugEnable = j.value("debugEnable", false);             
                cfg.serverListFontSize = j.value("serverListFontSize", (int32)12);
                cfg.chatLogFontSize    = j.value("chatLogFontSize", (int32)12);
                cfg.userListFontSize   = j.value("userListFontSize", (int32)12);  
                cfg.useCustomDrawFunction = j.value("useCustomDrawFunction", true);
                
                // Parse standard servers array
                if (j.contains("servers") && j["servers"].is_array()) {
                    for (const auto& s : j["servers"]) {
                        ServerConfig srv;
                        srv.name = s.value("name", "Unknown Server");
                        srv.host = s.value("host", "127.0.0.1");
                        srv.port = s.value("port", (uint16)6697);
                        srv.nick = s.value("nick", "HaikuUser");
                        srv.altNick = s.value("altNick", srv.nick + "+");  
                        srv.altNick2 = s.value("altNick2", srv.nick + "__"); 
                        srv.pass = s.value("pass", "");
                        srv.autoReconnect = s.value("autoReconnect", false); 
                        srv.autoConnect = s.value("autoConnect", false); 
                        srv.hideStatusMessages = s.value("hideStatusMessages", false);
                        srv.backgroundImagePath = s.value("background_image", ""); 
                        srv.backgroundOpacity = s.value("bg_opacity", 30); 
                        srv.enableEmoticons = s.value("enable_emoticons", true); 
                        if (s.contains("autojoin") && s["autojoin"].is_array()) {
                            for (const auto& chan : s["autojoin"]) {
                                srv.autojoin.push_back(chan.get<std::string>());
                            }
                        }
                        cfg.servers.push_back(srv);
                    }
                }

                // Parse the custom servers array safely
                if (j.contains("custom_servers") && j["custom_servers"].is_array()) {
                    for (const auto& s : j["custom_servers"]) {
                        ServerConfig srv;
                        srv.name = s.value("name", "Custom Server");
                        srv.host = s.value("host", "127.0.0.1");
                        srv.port = s.value("port", (uint16)6697);
                        srv.nick = s.value("nick", "HaikuUser");                        
                        srv.altNick = s.value("altNick", srv.nick + "+");  
                        srv.altNick2 = s.value("altNick2", srv.nick + "__"); 
                        
                        srv.pass = s.value("pass", "");
                        srv.autoReconnect = s.value("autoReconnect", false); 
                        srv.autoConnect = s.value("autoConnect", false); 
                        srv.hideStatusMessages = s.value("hideStatusMessages", false);
                        srv.backgroundImagePath = s.value("background_image", ""); 
                        srv.backgroundOpacity = s.value("bg_opacity", 30); 
                        srv.enableEmoticons = s.value("enable_emoticons", true); 
                        if (s.contains("autojoin") && s["autojoin"].is_array()) {
                            for (const auto& chan : s["autojoin"]) {
                                srv.autojoin.push_back(chan.get<std::string>());
                            }
                        }
                        cfg.customServers.push_back(srv);
                    }
                }
            } catch(...) {
                mustSaveDefaults = true;
            }
            infile.close();
        } else {
            mustSaveDefaults = true;
        }
    }

    if (mustSaveDefaults || cfg.servers.empty()) {
        cfg.servers.clear();        
        cfg.customServers.clear(); 
        cfg.useCustomDrawFunction = true; 

        srand(static_cast<unsigned int>(real_time_clock_usecs()));
        int randomSuffix = 1000 + (rand() % 9000);
        BString dynamicNick;
        dynamicNick << "HaikuIRCUser" << randomSuffix;

        ServerConfig libera;
        libera.name = "Libera Chat";
        libera.host = "irc.libera.chat";
        libera.port = 6697;
        libera.nick = dynamicNick.String();
        libera.altNick = BString(dynamicNick).Append("+").String(); 
        libera.altNick2 = BString(dynamicNick).Append("__").String();
        libera.pass = "";
        libera.autojoin = {"#ubuntu", "#linux"};
        libera.autoConnect = false;
        libera.autoReconnect = false;
        libera.hideStatusMessages = false;
		libera.backgroundImagePath = ""; 
		libera.backgroundOpacity = 30; 
		libera.enableEmoticons = true; 
		 
        ServerConfig oftc;
        oftc.name = "OFTC";
        oftc.host = "irc.oftc.net";
        oftc.port = 6697;
        oftc.nick = dynamicNick.String();
        oftc.altNick = BString(dynamicNick).Append("+").String();  
        oftc.altNick2 = BString(dynamicNick).Append("__").String();
        oftc.pass = "";
        oftc.autojoin = {"#haiku"};
        oftc.autoConnect = false;
        oftc.autoReconnect = false;
        oftc.hideStatusMessages = false;
        oftc.backgroundImagePath = ""; 
        oftc.backgroundOpacity = 30; 
        oftc.enableEmoticons = true; 
        
        cfg.servers.push_back(libera);
        cfg.servers.push_back(oftc);
        
        save_config(); 
    }
}




// Define application messages
enum {
    MSG_START_SIRC       = 'strt',
    MSG_SEND_MESSAGE     = 'send',
    MSG_IRC_RECEIVED     = 'recv',
    MSG_CONNECT_LIBERA   = 'cnlb',
    MSG_TOGGLE_AUTOJOIN  = 'tgaj',
    MSG_TOGGLE_AUTOCONNECT = 'tgac',
    MSG_CONNECT_OFTC     = 'cnof',
    MSG_OPEN_CHAN_LIST   = 'opcl',
    MSG_ADD_LIST_ROW     = 'adlr',
    MSG_CONTEXT_CHAN_LIST = 'cxcl',
    MSG_CLEAR_LIST_ROW   = 'cllr',
    MSG_TOGGLE_HIDE_STATUS = 'tghs',
    MSG_CONTEXT_ABOUT      = 'cxab',
    MSG_TOGGLE_AUTORECONNECT = 'tgar',
    MSG_RECONNECT_SERVER = 'rcsv',
    MSG_CONFIG_SAVE = 'cfsv',
    MSG_CONFIG_CANCEL = 'cfcn',
	MSG_CONTEXT_CONFIGURE_SERVER = 'mccs',
	MSG_SAVE_CONFIG_FILE = 'mscf',
	MSG_CONNECT_CUSTOM_SERVER = 'cncs', 
	MSG_ADD_CUSTOM_SERVER_SUBMIT = 'acss',
	MSG_DISCONNECT_SERVER = 'dscr',
	MSG_TOGGLE_CUSTOM_DRAW = 'tgcd',
	MSG_CLEAR_CUSTOM_BUFFER = 'clcb',
	MSG_TOPIC_CHANGED = 'tpch',
	MSG_CONTEXT_SET_AWAY = 'cxaw', 
	MSG_USER_LIST_CONTEXT_CLICK = 'ulcx',
	MSG_CONTEXT_PRIVMSG = 'cxpm',
	MSG_EMOTE_CLICKED = 'emcl',

};



// Forward Declarations 
class CricketWindow; 
class ServerTreeItem; 






class UserListItem : public BStringItem {
public:
    UserListItem(const char* nickname, bool isAway = false)
        : BStringItem(nickname), fIsAway(isAway) {}

    void SetAway(bool isAway) { fIsAway = isAway; }
    bool IsAway() const { return fIsAway; }

    // Custom drawing hook to render text dim/lighter when away
    virtual void DrawItem(BView* owner, BRect frame, bool complete = false) override {
        // Clear background row depending on selection state
        if (IsSelected()) {
            owner->SetHighColor(ui_color(B_LIST_SELECTED_BACKGROUND_COLOR));
        } else {
            owner->SetHighColor(ui_color(B_LIST_BACKGROUND_COLOR));
        }
        owner->FillRect(frame);

        // Adjust text coloring based on selection and away status
        if (IsSelected()) {
            owner->SetHighColor(ui_color(B_LIST_SELECTED_ITEM_TEXT_COLOR));
        } else if (fIsAway) {
            // Lighten the text. Adjust the RGB values (140, 140, 140) to taste!
            rgb_color awayColor = { 140, 140, 140, 255 }; 
            owner->SetHighColor(awayColor);
        } else {
            owner->SetHighColor(ui_color(B_LIST_ITEM_TEXT_COLOR));
        }

        // Standard Haiku font alignment math
        font_height fh;
        owner->GetFontHeight(&fh);
        float textHeight = fh.ascent + fh.descent;
        
        owner->MovePenTo(frame.left + 4, frame.top + fh.ascent + (frame.Height() - textHeight) / 2);
        owner->DrawString(Text());
    }

private:
    bool fIsAway;
};


enum FragmentType {
    FRAG_TEXT,
    FRAG_ICON
};

// 1. Holds individual text slices after word-wrapping processing
struct StyledRunFragment {
    FragmentType type;
    BString      subText;   
    const uint8* iconData;  
    BBitmap*     cachedBitmap; // <-- ADDED: Holds the pre-rendered bitmap asset
    float        width;     
    BFont        font;
    rgb_color    color;

    StyledRunFragment() : type(FRAG_TEXT), iconData(nullptr), cachedBitmap(nullptr), width(0.0f) {}
    
    // <-- ADDED: Guarantees leak-free automatic memory cleanup
    ~StyledRunFragment() {
        delete cachedBitmap; 
    }
};



// 2. Holds the raw message, its raw style runs, and its calculated display rows
struct StyledLine {
    BStringItem* itemNode; // <-- ADDED: Binds this specific text entry to its channel context node
    BString text;
    text_run_array* runs;
    
    // Explicit template parameters handle nested pointer cleanup automatically
    BObjectList<BObjectList<StyledRunFragment, true>, true> wrappedRows;

    // <-- Constructor now maps the item node tracking layout context
    StyledLine(BStringItem* node, BString t, const text_run_array* r) : wrappedRows(2) {
        itemNode = node;
        text = t;
        if (r != nullptr) {
            size_t size = sizeof(text_run_array) + (sizeof(text_run) * (r->count - 1));
            runs = (text_run_array*)malloc(size);
            if (runs != nullptr) {
                memcpy(runs, r, size);
            }
        } else {
            runs = nullptr;
        }
    }
    
    ~StyledLine() {
        free(runs);
        wrappedRows.MakeEmpty();
    }
};


// 3. Clean Interface Declaration Block (Contains NO method bodies)
class CustomChatView : public BView {
public:
    CustomChatView(BRect frame, const char* name, uint32 resizingMode, uint32 flags);
    virtual ~CustomChatView(); 

    virtual void Draw(BRect updateRect);
    virtual void FrameResized(float newWidth, float newHeight);
    virtual void MouseDown(BPoint point);

    void AddStyledLine(BStringItem* itemNode, const BString& text, const text_run_array* runs);
    void ClearAllLines();    
    void SetLineHeight(float height);
    void RecalculateAllLineWraps();
    void SetActiveChannel(BStringItem* activeNode);
    void SetBackgroundImage(const char* filePath);
    void SetBackgroundDimming(int32 level); 
    virtual void MessageReceived(BMessage* message); 
    virtual void MouseMoved(BPoint point, uint32 transit, const BMessage* dragMessage) override;


private:
    void ParseTextAndIcons(const BString& text, const text_run_array* runs, BObjectList<StyledRunFragment, true>* rawFragments);
    void ComputeWrapForLine(StyledLine* line, float maxWidth, BObjectList<StyledRunFragment, true>* rawFragments);
    
    void ComputeWrapForLine(StyledLine* line, float maxWidth);
	void UpdateScrollRange(bool scrollToBottom = false);
    BObjectList<StyledLine, true> fLines; 
    BStringItem*                  fActiveChannelNode;
    float                         fLineHeight;
    BBitmap* fBackgroundBitmap;
    int32    fBackgroundDimmingLevel; 
};




// Constructor
// Update Constructor to handle initial default allocation state 
// Constructor
// Update Constructor to handle initial default allocation state 
CustomChatView::CustomChatView(BRect frame, const char* name, uint32 resizingMode, uint32 flags)
    : BView(frame, name, resizingMode, flags | B_WILL_DRAW | B_FRAME_EVENTS | B_NAVIGABLE),
      fLines(20),
      fBackgroundBitmap(nullptr),
      fBackgroundDimmingLevel(30) 
{
    fLineHeight = 16.0; 
    fActiveChannelNode = nullptr;
    
    // SetViewColor(ui_color(B_DOCUMENT_BACKGROUND_COLOR));
    SetViewColor(B_TRANSPARENT_COLOR);
    SetLowColor(ui_color(B_DOCUMENT_BACKGROUND_COLOR));
}



void CustomChatView::ParseTextAndIcons(const BString& text, const text_run_array* runs, BObjectList<StyledRunFragment, true>* rawFragments)
{
    // --- NEW: RUNTIME TOGGLE ISOLATION RULE ---
    bool emoticonsEnabled = true;
    
    // Look up whether the active view belongs to a default or user-added server configuration profile
    // Uses the global index layout tracker variable "selectedConfig"
    if (selectedConfig < (int)cfg.servers.size()) {
        emoticonsEnabled = cfg.servers[selectedConfig].enableEmoticons;
    } else {
        int customIdx = selectedConfig - cfg.servers.size();
        if (customIdx >= 0 && customIdx < (int)cfg.customServers.size()) {
            emoticonsEnabled = cfg.customServers[customIdx].enableEmoticons;
        }
    }

    // Fallback: If emoticons are turned off for this server, process the raw string as text and break instantly!
    if (!emoticonsEnabled) {
        int32 runCount = (runs != nullptr && runs->count > 0) ? runs->count : 1;
        BFont baseFont;
        GetFont(&baseFont);

        for (int32 i = 0; i < runCount; i++) {
            int32 startPos = (runs != nullptr) ? runs->runs[i].offset : 0;
            int32 endPos = text.Length();
            BFont runFont = baseFont;
            rgb_color runColor = {255, 255, 255, 255};

            if (runs != nullptr && runs->count > 0) {
                runFont = runs->runs[i].font;
                runColor = runs->runs[i].color;
                if (i + 1 < runs->count) endPos = runs->runs[i+1].offset;
            }

            if (startPos >= text.Length() || endPos <= startPos) continue;

            StyledRunFragment* frag = new StyledRunFragment();
            frag->type = FRAG_TEXT;
            text.CopyInto(frag->subText, startPos, endPos - startPos);
            frag->font = runFont;
            frag->color = runColor;
            frag->width = runFont.StringWidth(frag->subText.String());
            rawFragments->AddItem(frag);
        }
        return; // Exit early safely
    }
    
    
    struct EmoteMap { const char* trigger; const uint8* data; };
    EmoteMap emotes[] = {
        // --- HAPPY / SMILING / LAUGHING ---
        {":)", (const uint8*)kIconSmile},      {":-)", (const uint8*)kIconSmile},
        {"^^", (const uint8*)kIconCheerful},   {"^_^", (const uint8*)kIconCheerful},
        {":D", (const uint8*)kIconLaughing},   {":-D", (const uint8*)kIconLaughing},
        {":d", (const uint8*)kIconLaughing},   {"lol", (const uint8*)kIconLaughing},  
        {"LOL", (const uint8*)kIconLaughing},

        // --- SAD / FROWNING / ANNOYED ---
        {":|", (const uint8*)kIconConfused},   {":-|", (const uint8*)kIconConfused},
        {":/", (const uint8*)kIconConfused},   {":-\\", (const uint8*)kIconConfused},
        {"o_O", (const uint8*)kIconConfused},  {"O_o", (const uint8*)kIconConfused},
        {":(", (const uint8*)kIconFrown},      {":-(", (const uint8*)kIconFrown},
        {">_ <", (const uint8*)kIconAnnoyed},  {">_(", (const uint8*)kIconAnnoyed},
        {">_<", (const uint8*)kIconAnnoyed},

        // --- CRYING / SCONXT ---
        {";'(", (const uint8*)kIconCrying},    {";-'(", (const uint8*)kIconCrying},
        {"T_T", (const uint8*)kIconCrying},    {";_;", (const uint8*)kIconCrying},

        // --- WINKING / PLAYFUL TONGUE ---
        {";)", (const uint8*)kIconWink},       {";-)", (const uint8*)kIconWink},
        {":P", (const uint8*)kIconTongue},     {":p", (const uint8*)kIconTongue},
        {":-P", (const uint8*)kIconTongue},    {":-p", (const uint8*)kIconTongue},

        // --- SHOCK / ASTONISHED / SURPRISED ---
        {":o", (const uint8*)kIconAstonished}, {":O", (const uint8*)kIconAstonished},
        {":-o", (const uint8*)kIconAstonished},{":-O", (const uint8*)kIconAstonished},
        {":0", (const uint8*)kIconAstonished}, {":-0", (const uint8*)kIconAstonished},
        {"O_O", (const uint8*)kIconWideeyed},  {"o_o", (const uint8*)kIconWideeyed},

        // --- COOL / SUNGLASSES ---
        {"8-)", (const uint8*)kIconSunglasses}, {"B)", (const uint8*)kIconSunglasses},
        {"8)", (const uint8*)kIconSunglasses},  {"b)", (const uint8*)kIconSunglasses},

        // --- MISC ROMANTIC / SECRETS ---
        {"<3", (const uint8*)kIconHeart},       {":*", (const uint8*)kIconKiss},       
        {":-*", (const uint8*)kIconKiss},       {":X", (const uint8*)kIconFrown},      
        {":x", (const uint8*)kIconFrown}
    };
    // FIX A: Correctly divide by the size of a single row element slot
    int32 emoteCount = sizeof(emotes) / sizeof(emotes[0]);

    BFont baseFont;
    GetFont(&baseFont);
    
    int32 runCount = (runs != nullptr && runs->count > 0) ? runs->count : 1;
    
    for (int32 i = 0; i < runCount; i++) {
        int32 startPos = 0;
        int32 endPos = text.Length();
        
        BFont runFont = baseFont;
        rgb_color runColor = {255, 255, 255, 255}; 
        
        if (runs != nullptr && runs->count > 0) {
            startPos = runs->runs[i].offset;
            runFont = runs->runs[i].font;
            runColor = runs->runs[i].color;
            
            if (i + 1 < runs->count) {
                endPos = runs->runs[i+1].offset;
            }
        }
        
        if (startPos >= text.Length() || endPos <= startPos) {
            continue;
        }

        BString runText;
        text.CopyInto(runText, startPos, endPos - startPos);
        
        int32 currentPos = 0;
        int32 runLength = runText.Length();
        
        while (currentPos < runLength) {
            int32 nextTrigger = runLength;
            int32 triggerLength = 0;
            const uint8* foundIcon = nullptr;
            
            for (int32 e = 0; e < emoteCount; e++) {
                int32 pos = runText.FindFirst(emotes[e].trigger, currentPos);
                if (pos != B_ERROR && pos < nextTrigger) {
                    
                    bool isValidMatch = true;
                    int32 trigLen = strlen(emotes[e].trigger);
                    
                    // FIX B: Only run the timestamp filter if the trigger begins with a colon!
                    if (emotes[e].trigger[0] == ':' && pos + 1 < runLength) {
                        char nextChar = runText.ByteAt(pos + 1);
                        if (isdigit(nextChar)) {
                            isValidMatch = false;
                        }
                    }

                    // Word boundary isolation filters
                    if (isValidMatch && pos > 0) {
                        char prevChar = runText.ByteAt(pos - 1);
                        if (isalnum(prevChar) || prevChar == '/') {
                            isValidMatch = false;
                        }
                    }
                    if (isValidMatch && (pos + trigLen < runLength)) {
                        char nextChar = runText.ByteAt(pos + trigLen);
                        if (isalnum(nextChar) || nextChar == '/') {
                            isValidMatch = false;
                        }
                    }
                    
                    if (isValidMatch) {
                        nextTrigger = pos;
                        triggerLength = trigLen;
                        foundIcon = emotes[e].data;
                    }
                }
            }
            
            if (foundIcon != nullptr) {
                if (nextTrigger > currentPos) {
                    StyledRunFragment* frag = new StyledRunFragment();
                    frag->type = FRAG_TEXT;
                    runText.CopyInto(frag->subText, currentPos, nextTrigger - currentPos);
                    frag->font = runFont;
                    frag->color = runColor;
                    frag->width = runFont.StringWidth(frag->subText.String());
                    rawFragments->AddItem(frag);
                }
                
                StyledRunFragment* frag = new StyledRunFragment();
                frag->type = FRAG_ICON;
                frag->iconData = foundIcon;
                frag->font = runFont;
                frag->color = runColor;
                frag->width = fLineHeight; 

                BRect renderBounds(0, 0, fLineHeight, fLineHeight);
                frag->cachedBitmap = new BBitmap(renderBounds, B_RGBA32);
                if (BIconUtils::GetVectorIcon(frag->iconData, 8192, frag->cachedBitmap) != B_OK) {
                    delete frag->cachedBitmap;
                    frag->cachedBitmap = nullptr;
                }
                
                rawFragments->AddItem(frag);
                currentPos = nextTrigger + triggerLength;
            } else {
                if (currentPos < runLength) {
                    StyledRunFragment* frag = new StyledRunFragment();
                    frag->type = FRAG_TEXT;
                    runText.CopyInto(frag->subText, currentPos, runLength - currentPos);
                    frag->font = runFont;
                    frag->color = runColor;
                    frag->width = runFont.StringWidth(frag->subText.String());
                    rawFragments->AddItem(frag);
                }
                break;
            }
        }
    }
}


void CustomChatView::SetBackgroundDimming(int32 level)
{
    fBackgroundDimmingLevel = level;
    if (fBackgroundDimmingLevel < 0) fBackgroundDimmingLevel = 0;
    if (fBackgroundDimmingLevel > 100) fBackgroundDimmingLevel = 100;
    Invalidate(); // Refresh canvas
}

void CustomChatView::ClearAllLines() {
    fLines.MakeEmpty();
    Invalidate();
}

void CustomChatView::SetLineHeight(float height) {
    fLineHeight = height;
    Invalidate();
}

void CustomChatView::FrameResized(float newWidth, float newHeight) {
    BView::FrameResized(newWidth, newHeight);
    RecalculateAllLineWraps();
    UpdateScrollRange(false); 
    Invalidate();
}

CustomChatView::~CustomChatView() {
	delete fBackgroundBitmap; 
    fLines.MakeEmpty();
}
// Pipeline Line Addition Point
void CustomChatView::AddStyledLine(BStringItem* itemNode, const BString& text, const text_run_array* runs) {
    if (itemNode == nullptr) return;

    // Create the line tagged to its specific target context
    StyledLine* newLine = new StyledLine(itemNode, text, runs);
    fLines.AddItem(newLine);

    // Only spend CPU cycles compute-wrapping the text layout if it belongs to the visible channel
    if (itemNode == fActiveChannelNode) {
        float maxWidth = Bounds().Width() - 20.0f;
        if (maxWidth <= 50.0f) maxWidth = 50.0f;

        // Build a temporary flat collection of fragments (Text + Emotes mixed)
        BObjectList<StyledRunFragment, true> rawFragments(10);
        ParseTextAndIcons(text, runs, &rawFragments);

        // Compute the wrap based on those parsed tokens
        ComputeWrapForLine(newLine, maxWidth, &rawFragments);
        
        UpdateScrollRange(true); 
        Invalidate();
    }
}


// Update Loop Filter Pass
void CustomChatView::RecalculateAllLineWraps()
{
    float maxWidth = Bounds().Width() - 20.0f;
    if (maxWidth <= 50.0f) maxWidth = 50.0f;

    for (int32 i = 0; i < fLines.CountItems(); i++) {
        StyledLine* line = fLines.ItemAt(i);
        
        // Only spend CPU resources layout-wrapping lines for the active view context
        if (line->itemNode == fActiveChannelNode) {
            // Re-parse the text and icons into fresh fragments for wrapping
            BObjectList<StyledRunFragment, true> rawFragments(10);
            ParseTextAndIcons(line->text, line->runs, &rawFragments);

            // Call the updated 3-parameter function signature
            ComputeWrapForLine(line, maxWidth, &rawFragments);
        }
    }
    UpdateScrollRange(false);
    Invalidate();
}


void CustomChatView::UpdateScrollRange(bool scrollToBottom) {
    // 1. Locate the vertical scrollbar attached to our view parent frame container
    BScrollBar* vScroll = ScrollBar(B_VERTICAL);
    if (vScroll == nullptr) return;

    // 2. Count the total row count for the currently active visible room
    int32 totalActiveRows = 0;
    for (int32 i = 0; i < fLines.CountItems(); i++) {
        StyledLine* line = fLines.ItemAt(i);
        if (line != nullptr && line->itemNode == fActiveChannelNode) {
            totalActiveRows += line->wrappedRows.CountItems();
        }
    }

    // 3. Compute structural height limits
    float totalDocumentHeight = (totalActiveRows * fLineHeight) + 20.0f; // includes visual gutters
    float viewHeight = Bounds().Height();

    // 4. Update the scrollbar range bounds properties
    if (totalDocumentHeight <= viewHeight) {
        // Everything fits on one screen; disable the scrollbar
        vScroll->SetRange(0.0f, 0.0f);
        vScroll->SetProportion(1.0f);
    } else {
        // Document overflows viewport; set maximum scroll bound target
        float maxScrollValue = totalDocumentHeight - viewHeight;
        vScroll->SetRange(0.0f, maxScrollValue);
        
        // Set the size ratio of the scrollbar thumb relative to full canvas height
        vScroll->SetProportion(viewHeight / totalDocumentHeight);

        // 5. Fire auto-scroll step if flagged or if the user is trailing closely at the bottom
        if (scrollToBottom) {
            vScroll->SetValue(maxScrollValue);
        }
    }
}


void CustomChatView::ComputeWrapForLine(StyledLine* line, float maxWidth, BObjectList<StyledRunFragment, true>* rawFragments)
{
    if (line == nullptr) 
        return;
        
    line->wrappedRows.MakeEmpty();

    if (maxWidth <= 0 || rawFragments == nullptr || rawFragments->IsEmpty()) 
        return;

    BObjectList<StyledRunFragment, true>* currentRow = new BObjectList<StyledRunFragment, true>(5);
    float currentX = 0.0f;

    while (!rawFragments->IsEmpty()) {
        StyledRunFragment* frag = rawFragments->RemoveItemAt(0);
        
        if (frag->type == FRAG_ICON || currentX + frag->width <= maxWidth) {
            currentRow->AddItem(frag);
            currentX += frag->width;
        } else {
            BFont font = frag->font;
            float availableWidth = maxWidth - currentX;
            int32 charCount = 0;
            int32 textLen = frag->subText.Length();
            const char* rawStr = frag->subText.String();
            
            // High-performance string measurement boundary check using byte lengths directly
            for (int32 c = 1; c <= textLen; c++) {
                if (font.StringWidth(rawStr, c) <= availableWidth) {
                    charCount = c;
                } else {
                    break;
                }
            }
            
            if (charCount > 0) {
                StyledRunFragment* leftPart = new StyledRunFragment();
                leftPart->type = FRAG_TEXT;
                leftPart->font = frag->font;
                leftPart->color = frag->color;
                frag->subText.CopyInto(leftPart->subText, 0, charCount);
                leftPart->width = font.StringWidth(leftPart->subText.String());
                currentRow->AddItem(leftPart);
                
                frag->subText.Remove(0, charCount);
                frag->width = font.StringWidth(frag->subText.String());
            }
            
            line->wrappedRows.AddItem(currentRow);
            
            currentRow = new BObjectList<StyledRunFragment, true>(5);
            currentRow->AddItem(frag);
            currentX = frag->width;
        }
    }
    
    if (!currentRow->IsEmpty()) {
        line->wrappedRows.AddItem(currentRow);
    } else {
        delete currentRow;
    }
}




// Channel Room Switcher Method
void CustomChatView::SetActiveChannel(BStringItem* activeNode) {
    if (fActiveChannelNode != activeNode) {
        fActiveChannelNode = activeNode;
        // Re-wrap rows because a different chat window layout might require different view space setups
        RecalculateAllLineWraps(); 
        UpdateScrollRange(true); 
        Invalidate();
    }
}

// Intercept System-wide Color/Theme Changes Live
void CustomChatView::MessageReceived(BMessage* message) {
    switch (message->what) {
        case MSG_CLEAR_CUSTOM_BUFFER: {
            // 1. Loop backward and purge only rows matching our current active room context node
            for (int32 i = fLines.CountItems() - 1; i >= 0; i--) {
                StyledLine* line = fLines.ItemAt(i);
                if (line != nullptr && line->itemNode == fActiveChannelNode) {
                    delete fLines.RemoveItemAt(i);
                }
            }

            // 2. Decoupled messaging bypasses compile-order and circular header errors ---
            // We pass an internal message token up to the base BWindow layer to handle map clearing safely.
            if (Window() != nullptr && fActiveChannelNode != nullptr) {
                BMessage clearCacheMsg('clch'); // Clear Channel Cache custom token
                clearCacheMsg.AddPointer("active_node", fActiveChannelNode); 
                Window()->PostMessage(&clearCacheMsg);
            }

            // 3. Recalculate scrollbar heights and force a fresh repaint pass across the clean canvas
            UpdateScrollRange(false);
            Invalidate();
            break;
        }

        
        // Ensure your existing system color theme hooks continue working natively alongside this switch
        case B_COLORS_UPDATED: {
            SetViewColor(ui_color(B_DOCUMENT_BACKGROUND_COLOR));
            SetLowColor(ui_color(B_DOCUMENT_BACKGROUND_COLOR));
            Invalidate();
            break;
        }
        
        default:
            BView::MessageReceived(message);
            break;
    }
}


void CustomChatView::Draw(BRect updateRect) {
    rgb_color systemBgColor = ui_color(B_DOCUMENT_BACKGROUND_COLOR);
    rgb_color systemTextColor = ui_color(B_DOCUMENT_TEXT_COLOR);

    // --- BRANCH A: SCALED CUSTOM BACKGROUND REFRESH ---
    if (fBackgroundBitmap != nullptr) {
        SetDrawingMode(B_OP_COPY);
        DrawBitmap(fBackgroundBitmap, Bounds());

        // --- NEW: HIGH-PERFORMANCE DIMMING OVERLAY PASS ---
        if (fBackgroundDimmingLevel > 0) {
            // Calculate alpha value (0 to 255) based on the slider percentage scale
            uint8 alphaIntensity = (uint8)((fBackgroundDimmingLevel / 100.0f) * 255.0f);
            
            // Generate a transparent black tint color overlay layout block
            rgb_color dimColor = { 0, 0, 0, alphaIntensity };
            
            SetHighColor(dimColor);
            SetDrawingMode(B_OP_ALPHA); // Enforce hardware transparency blending pass
            FillRect(Bounds());
            SetDrawingMode(B_OP_COPY);  // Revert drawing mode safely
        }
    } else {
        SetHighColor(systemBgColor); 
        FillRect(updateRect); 
    }


    // --- BRANCH B: INLINE CHAT RENDERING ---
    float currentY = 10.0f; 
    font_height fh;
    GetFontHeight(&fh);
    float fontAscent = fh.ascent;

    for (int32 i = 0; i < fLines.CountItems(); i++) {
        StyledLine* line = fLines.ItemAt(i);
        if (!line || line->itemNode != fActiveChannelNode) continue;

        for (int32 rowIdx = 0; rowIdx < line->wrappedRows.CountItems(); rowIdx++) {
            BObjectList<StyledRunFragment, true>* row = line->wrappedRows.ItemAt(rowIdx);
            if (!row) continue;

            if (currentY + fLineHeight >= updateRect.top && currentY <= updateRect.bottom) {
                float currentX = 10.0f;

                for (int32 fragIdx = 0; fragIdx < row->CountItems(); fragIdx++) {
                    StyledRunFragment* frag = row->ItemAt(fragIdx);
                    if (!frag) continue;

                    SetFont(&(frag->font));

                    if (frag->type == FRAG_TEXT) {
                        rgb_color renderColor = frag->color;
                        if ((renderColor.red == 255 && renderColor.green == 255 && renderColor.blue == 255) ||
                            (renderColor.red == 0   && renderColor.green == 0   && renderColor.blue == 0)) {
                            renderColor = systemTextColor;
                        }
                        SetHighColor(renderColor);
                        
                        SetDrawingMode(B_OP_ALPHA);
                        DrawString(frag->subText.String(), BPoint(currentX, currentY + fLineHeight - 2.0f));
                        SetDrawingMode(B_OP_COPY);
                        
                        currentX += StringWidth(frag->subText.String());
                    } 
                    else if (frag->type == FRAG_ICON) {
                        BRect iconRect(currentX, 
                                       currentY + fLineHeight - fontAscent - 2.0f, 
                                       currentX + fLineHeight, 
                                       currentY + fLineHeight + fh.descent - 2.0f);
                        
                        if (frag->cachedBitmap != nullptr) {
                            SetDrawingMode(B_OP_ALPHA);
                            DrawBitmap(frag->cachedBitmap, iconRect);
                            SetDrawingMode(B_OP_COPY);
                        }
                        currentX += frag->width + 2.0f;
                    }
                }
            }
            currentY += fLineHeight;
        }
    }
}




// Scoped layout mouse hover tracking engine for dynamic cursor feedback
void CustomChatView::MouseMoved(BPoint point, uint32 transit, const BMessage* dragMessage) {
    // 1. Handle clean exit trajectories right away to prevent stuck cursors
    if (transit == B_EXITED_VIEW) {
        BCursor defaultCursor(B_CURSOR_ID_SYSTEM_DEFAULT);
        SetViewCursor(&defaultCursor);
        BView::MouseMoved(point, transit, dragMessage);
        return;
    }

    bool isHoveringOverLink = false;
    float currentY = 10.0f;

    // 2. Spatial matching mirror layout loop from your MouseDown implementation
    for (int32 i = 0; i < fLines.CountItems(); i++) {
        StyledLine* line = fLines.ItemAt(i);
        
        // --- CHANNEL ISOLATION CHECK ---
        if (!line || line->itemNode != fActiveChannelNode) continue;

        for (int32 rowIdx = 0; rowIdx < line->wrappedRows.CountItems(); rowIdx++) {
            BObjectList<StyledRunFragment, true>* row = line->wrappedRows.ItemAt(rowIdx);
            if (!row) continue;

            // Check if mouse Y coordinate aligns with this row's bounding box
            if (point.y >= currentY && point.y <= (currentY + fLineHeight)) {
                float currentX = 10.0f;

                for (int32 fragIdx = 0; fragIdx < row->CountItems(); fragIdx++) {
                    StyledRunFragment* frag = row->ItemAt(fragIdx);
                    if (!frag) continue;

                    // Measure font segment width dynamically
                    SetFont(&(frag->font));
                    float segmentWidth = StringWidth(frag->subText.String());

                    // Check if segment is a link (Underlined) and X intersects
                    if (frag->font.Face() & B_UNDERSCORE_FACE) {
                        if (point.x >= currentX && point.x <= (currentX + segmentWidth)) {
                            isHoveringOverLink = true;
                            break;
                        }
                    }
                    currentX += segmentWidth;
                }
                break;
            }
            currentY += fLineHeight;
        }
        if (isHoveringOverLink) break;
    }

    // 3. Swap the system cursor icon state seamlessly based on coordinates matching outcome
    if (isHoveringOverLink) {
        BCursor linkCursor(B_CURSOR_ID_FOLLOW_LINK);
        SetViewCursor(&linkCursor);
    } else {
        BCursor defaultCursor(B_CURSOR_ID_SYSTEM_DEFAULT);
        SetViewCursor(&defaultCursor);
    }

    // Call the base class implementation to preserve default system drag/drop pipelines
    BView::MouseMoved(point, transit, dragMessage);
}




// Scoped layout mouse click tracker logic implementation with Channel Filtering Isolation
void CustomChatView::MouseDown(BPoint point) {
    if (Window() == nullptr) return;
    
    int32 buttons = 0;
    Window()->CurrentMessage()->FindInt32("buttons", &buttons);

    // =========================================================================
    // RIGHT-CLICK CONTEXT MENU: Trigger on secondary mouse clicks
    // =========================================================================
    if (buttons == B_SECONDARY_MOUSE_BUTTON) {
        // Instantiate a standard pop-up context panel asynchronously
        BPopUpMenu* contextMenu = new BPopUpMenu("ChatViewContext", false, false);
        
        // Add the Clear option tied to our unique identifier token constant
        BMenuItem* clearItem = new BMenuItem("Clear Buffer", new BMessage(MSG_CLEAR_CUSTOM_BUFFER));
        contextMenu->AddItem(clearItem);
        
        // Command the menu to target this view context instance specifically
        contextMenu->SetTargetForItems(this);
        
        // Open the menu instantly right where the mouse cursor landed on the screen canvas
        contextMenu->Go(ConvertToScreen(point), true, true, true);
        return;
    }

    // =========================================================================
    // LEFT-CLICK LINK TRACKER: Your existing logic remains perfectly intact
    // =========================================================================
    if (buttons != B_PRIMARY_MOUSE_BUTTON) return;

    float currentY = 10.0f;

    for (int32 i = 0; i < fLines.CountItems(); i++) {
        StyledLine* line = fLines.ItemAt(i);
        
        // --- CHANNEL ISOLATION CHECK ---
        // Prevents clicks from registering on hidden lines belonging to background chats
        if (!line || line->itemNode != fActiveChannelNode) continue;

        for (int32 rowIdx = 0; rowIdx < line->wrappedRows.CountItems(); rowIdx++) {
            BObjectList<StyledRunFragment, true>* row = line->wrappedRows.ItemAt(rowIdx);
            if (!row) continue;

            if (point.y >= currentY && point.y <= (currentY + fLineHeight)) {
                float currentX = 10.0f;

                for (int32 fragIdx = 0; fragIdx < row->CountItems(); fragIdx++) {
                    StyledRunFragment* frag = row->ItemAt(fragIdx);
                    if (!frag) continue;

                    SetFont(&(frag->font));
                    float segmentWidth = StringWidth(frag->subText.String());

                    // Check for underline style assignment
                    if (frag->font.Face() & B_UNDERSCORE_FACE) {
                        if (point.x >= currentX && point.x <= (currentX + segmentWidth)) {
                            BString url = frag->subText;
                            url.Trim();
                            char* args = (char*)url.String();
                            be_roster->Launch("text/html", 1, &args);
                            return;
                        }
                    }
                    currentX += segmentWidth;
                }
                return;
            }
            currentY += fLineHeight;
        }
    }
}


void CustomChatView::SetBackgroundImage(const char* filePath)
{
    // Clean up any previously cached background asset to prevent memory leaks
    delete fBackgroundBitmap;
    fBackgroundBitmap = nullptr;

    if (filePath != nullptr) {
        // Automatically decode and load the image file format into RAM
        fBackgroundBitmap = BTranslationUtils::GetBitmap(filePath);
        
        if (fBackgroundBitmap == nullptr) {
            printf("[ERROR] Failed to load background image: %s\n", filePath);
        }
    }

    // Force a full screen redraw pass to present the new background layout
    Invalidate();
}


static void LogDebugStream(const char* serverName, const char* direction, const char* rawData, int32 dataLength) {
    // Escape early if global debug flag isn't active
    if (!cfg.debugEnable) return;

    BPath path;
    if (find_directory(B_USER_SETTINGS_DIRECTORY, &path) == B_OK) {
        path.Append("cricket/cricket_debug_log.txt");
        
        // Open the file in output-append mode
        std::ofstream logFile(path.Path(), std::ios::out | std::ios::app);
        if (logFile.is_open()) {
            // Generate a matching timestamp prefix for the raw data row
            bigtime_t currentTime = real_time_clock_usecs();
            time_t rawTime = (time_t)(currentTime / 1000000);
            struct tm* timeInfo = localtime(&rawTime);
            
            char timeBuffer[32];
            if (timeInfo != nullptr) {
                strftime(timeBuffer, sizeof(timeBuffer), "[%Y-%m-%d %H:%M:%S] ", timeInfo);
                logFile << timeBuffer;
            }

            // Clean data boundaries to prevent printing binary junk characters
            BString cleanBuffer(rawData, dataLength);
            cleanBuffer.ReplaceAll("\r", "");
            cleanBuffer.ReplaceAll("\n", " "); // Flatten multi-line packets into clean singular rows

            // Write the formatted log row transaction
            logFile << "[" << serverName << "] " << direction << ": " << cleanBuffer.String() << "\n";
            logFile.close();
        }
    }
}



class AddServerWindow : public BWindow {
public:
    AddServerWindow(BWindow* targetWindow) 
        // Replaced hardcoded dimensions with a (0,0,1,1) dummy rect to let B_AUTO_UPDATE_SIZE_LIMITS auto-scale height
        : BWindow(BRect(0, 0, 350, 1), "Add Custom Server", 
                  B_TITLED_WINDOW, B_NOT_RESIZABLE | B_NOT_ZOOMABLE | B_AUTO_UPDATE_SIZE_LIMITS) {
        
        fTarget = targetWindow;

        // 1. Initialize modern, auto-aligning input layout fields
        fNameField = new BTextControl("name", "Network Name:", "My IRC Server", nullptr);
        fHostField = new BTextControl("host", "Server Host:", "irc.example.com", nullptr);
        fPortField = new BTextControl("port", "Port:", "6697", nullptr);
        fNickField = new BTextControl("nick", "Nickname:", "HaikuUser", nullptr);
        
        // NEW: Instantiate two new alternative nickname fields with clean template defaults
        fAltNickField  = new BTextControl("altnick", "Alt Nick 1:", "HaikuUser+", nullptr);
        fAltNick2Field = new BTextControl("altnick2", "Alt Nick 2:", "HaikuUser__", nullptr);
        
        fPassField = new BTextControl("pass", "Password (Optional):", "", nullptr);
        
        // Hide password characters automatically
        fPassField->TextView()->HideTyping(true);

        // 2. Control Form Buttons
        fCancelButton = new BButton("cancel", "Cancel", new BMessage(B_QUIT_REQUESTED));
        fSaveButton   = new BButton("save", "Add Server", new BMessage(MSG_ADD_CUSTOM_SERVER_SUBMIT));
        
        // Set the save button as the default highlight action on hitting 'Enter' key
        fSaveButton->MakeDefault(true);

        // 3. Build UI Architecture via Group Layouts
        BLayoutBuilder::Group<>(this, B_VERTICAL, 10)
            .SetInsets(12)
            .AddGrid(5.0f, 5.0f) // Vertically aligns the colons of the text inputs perfectly
                .Add(fNameField->CreateLabelLayoutItem(), 0, 0)
                .Add(fNameField->CreateTextViewLayoutItem(), 1, 0)
                
                .Add(fHostField->CreateLabelLayoutItem(), 0, 1)
                .Add(fHostField->CreateTextViewLayoutItem(), 1, 1)
                
                .Add(fPortField->CreateLabelLayoutItem(), 0, 2)
                .Add(fPortField->CreateTextViewLayoutItem(), 1, 2)
                
                .Add(fNickField->CreateLabelLayoutItem(), 0, 3)
                .Add(fNickField->CreateTextViewLayoutItem(), 1, 3)
                
                // Placed new input fields into their own grid coordinates cleanly
                .Add(fAltNickField->CreateLabelLayoutItem(), 0, 4)
                .Add(fAltNickField->CreateTextViewLayoutItem(), 1, 4)
                
                .Add(fAltNick2Field->CreateLabelLayoutItem(), 0, 5)
                .Add(fAltNick2Field->CreateTextViewLayoutItem(), 1, 5)
                
                // Shifted password down to row 6 to clear the room
                .Add(fPassField->CreateLabelLayoutItem(), 0, 6)
                .Add(fPassField->CreateTextViewLayoutItem(), 1, 6)
            .End()
            .AddGlue() // Pushes inputs up and action control buttons down
            .AddGroup(B_HORIZONTAL, 10)
                .AddGlue() // Right-aligns buttons cleanly
                .Add(fCancelButton)
                .Add(fSaveButton)
            .End();

        // 4. Center this modal dynamically directly over the main application
        CenterIn(targetWindow->Frame());
    }

    void MessageReceived(BMessage* message) override {
        switch (message->what) {
        	
        	
            case MSG_ADD_CUSTOM_SERVER_SUBMIT: {
                // Perform quick sanitization constraints check
                if (strlen(fNameField->Text()) == 0 || strlen(fHostField->Text()) == 0) {
                    return; 
                }

                // Pack everything securely into a payload carrier message
                BMessage reply(MSG_ADD_CUSTOM_SERVER_SUBMIT);
                reply.AddString("name", fNameField->Text());
                reply.AddString("host", fHostField->Text());
                reply.AddInt32("port", atoi(fPortField->Text()));
                reply.AddString("nick", fNickField->Text());
                
                // Pack the alternate fields text strings safely into transmission payload
                reply.AddString("altNick", fAltNickField->Text());
                reply.AddString("altNick2", fAltNick2Field->Text());
                
                reply.AddString("pass", fPassField->Text());

                // Post asynchronous message back to the main UI frame
                fTarget->PostMessage(&reply);
                
                // Close dialog
                PostMessage(B_QUIT_REQUESTED);
                break;
            }
            default:
                BWindow::MessageReceived(message);
                break;
        }
    }

private:
    BWindow*       fTarget;
    BTextControl*  fNameField;
    BTextControl*  fHostField;
    BTextControl*  fPortField;
    BTextControl*  fNickField;
    BTextControl*  fAltNickField; 
    BTextControl*  fAltNick2Field; 
    BTextControl*  fPassField;
    BButton*       fCancelButton;
    BButton*       fSaveButton;
};





// 1. DECLARE HELPER ITEM FIRST: Put ChannelRowItem at the top so the window can see it
class ChannelRowItem : public BStringItem {
public:
    ChannelRowItem(const char* channel, const char* users, const char* topic)
        : BStringItem(channel), fChannel(channel), fUsers(users), fTopic(topic) {
        fRawUserCount = atoi(users); 	
        fUsers << " users";
    }

    BString GetChannelName() const { return fChannel; }
    int32   GetUserCount() const { return fRawUserCount; } 

    void DrawItem(BView* owner, BRect itemRect, bool drawEverything) override {
        owner->PushState();

        if (IsSelected()) {
            owner->SetLowColor(ui_color(B_LIST_SELECTED_BACKGROUND_COLOR));
            owner->SetHighColor(ui_color(B_LIST_SELECTED_ITEM_TEXT_COLOR));
            owner->FillRect(itemRect, B_SOLID_LOW);
        } else {
            owner->SetLowColor(ui_color(B_LIST_BACKGROUND_COLOR));
            owner->SetHighColor(ui_color(B_LIST_ITEM_TEXT_COLOR));
            owner->FillRect(itemRect, B_SOLID_LOW);
        }

        // Read the true runtime font scale from the parent view context container
        BFont dynamicListFont;
        owner->GetFont(&dynamicListFont);
        owner->SetFont(&dynamicListFont);
        
        // FIX #2: Correct native way to compute the baseline Y coordinate using GetFontHeight()
        font_height fh;
        dynamicListFont.GetHeight(&fh); // Compute baseline using the dynamic font metrics
        float baselineY = itemRect.bottom - (fh.descent + fh.leading);

        // Column A: Channel Name (Starts at pixel 10)
        owner->MovePenTo(itemRect.left + 10, baselineY);
        owner->DrawString(fChannel.String());

        // Column B: User Count (Starts at pixel 160)
        owner->MovePenTo(itemRect.left + 160, baselineY);
        owner->SetHighColor(ui_color(B_SUCCESS_COLOR)); 
        owner->DrawString(fUsers.String());

        // Column C: Topic Description (Starts at pixel 250)
        if (IsSelected()) {
            owner->SetHighColor(ui_color(B_LIST_SELECTED_ITEM_TEXT_COLOR));
        } else {
            owner->SetHighColor(ui_color(B_LIST_ITEM_TEXT_COLOR));
        }
        owner->MovePenTo(itemRect.left + 250, baselineY);
        
        BString truncatedTopic = fTopic;
        owner->TruncateString(&truncatedTopic, B_TRUNCATE_END, itemRect.Width() - 260);
        owner->DrawString(truncatedTopic.String());

        owner->PopState();
    }

private:
    BString fChannel;
    BString fUsers;
    BString fTopic;
    int32   fRawUserCount;
};


// Sorts items so that the highest user count bubbles up to the top
// Use const void* to match the native BListView API
static int SortChannelsByUsers(const void* first, const void* second) {
    // 1. Cast the generic raw pointers to constant BListItem pointers safely
    const BListItem* itemPtrA = *static_cast<const BListItem* const*>(first);
    const BListItem* itemPtrB = *static_cast<const BListItem* const*>(second);

    // 2. Perform dynamic_cast checks to verify these are indeed ChannelRowItems
    const ChannelRowItem* itemA = dynamic_cast<const ChannelRowItem*>(itemPtrA);
    const ChannelRowItem* itemB = dynamic_cast<const ChannelRowItem*>(itemPtrB);

    if (itemA == nullptr || itemB == nullptr) return 0;

    // Descending order sort (highest count first)
    if (itemA->GetUserCount() > itemB->GetUserCount()) return -1;
    if (itemA->GetUserCount() < itemB->GetUserCount()) return 1;
    return 0;
}




// 2. DECLARE THE WINDOW
class IRCChannelListWindow : public BWindow {
public:
    IRCChannelListWindow(BWindow* owner, BSecureSocket* targetSocket, ServerTreeItem* serverItem) 
        // Changing to B_DOCUMENT_WINDOW restores the standard Haiku resizable border frame layout
        : BWindow(BRect(150, 150, 800, 650), "Network Channel List", 
                  B_DOCUMENT_WINDOW, B_ASYNCHRONOUS_CONTROLS) {

        
        fOwnerWindow = owner;
        fSocket = targetSocket;
        fServerContext = serverItem; // <-- ADDED: Stores server context item reference safely

        fListView = new BListView("chan_list_view");
        fListView->SetInvocationMessage(new BMessage('join'));
        BScrollView* scrollPane = new BScrollView("scroll_list", fListView, 0, false, true);

        BLayoutBuilder::Group<>(this, B_VERTICAL, 0)
            .SetInsets(5)
            .Add(scrollPane, 1.0)
            .End();

        if (fSocket != nullptr) {
            fSocket->Write("LIST\r\n", 6);
        }
    }

    // Public getter required by the '322' protocol parser block
    BSecureSocket* GetTargetSocket() const { return fSocket; }

    // --- ADDED: Public getter to query the parent network server node if needed ---
    ServerTreeItem* GetServerContext() const { return fServerContext; }

    void DispatchMessage(BMessage* message, BHandler* handler) override {
        if (message->what == B_MOUSE_DOWN && handler == fListView) {
            int32 buttons;
            BPoint point;
            
            if (message->FindInt32("buttons", &buttons) == B_OK && buttons == B_SECONDARY_MOUSE_BUTTON) {
                message->FindPoint("be:view_where", &point);
                int32 index = fListView->IndexOf(point);
                
                if (index >= 0) {
                    fListView->Select(index);
                    ChannelRowItem* selectedItem = dynamic_cast<ChannelRowItem*>(fListView->ItemAt(index));
                    
                    if (selectedItem != nullptr) {
                        BPopUpMenu* menu = new BPopUpMenu("ChannelActions", false, false);
                        menu->AddItem(new BMenuItem("Join Channel", new BMessage('join')));
                        menu->SetTargetForItems(this);
                        menu->Go(fListView->ConvertToScreen(point), true, true, true);
                        return; 
                    }
                }
            }
        }
        BWindow::DispatchMessage(message, handler);
    }

    void MessageReceived(BMessage* message) override {
        switch (message->what) {
 
            case MSG_ADD_LIST_ROW: {
                const char* channelName;
                const char* userCount;
                const char* topic;
        
                if (message->FindString("channel", &channelName) == B_OK &&
                    message->FindString("users", &userCount) == B_OK &&
                    message->FindString("topic", &topic) == B_OK) {
            
                    fListView->AddItem(new ChannelRowItem(channelName, userCount, topic));
            
                    // Automatically re-sorts the rows live on screen!
                    fListView->SortItems(SortChannelsByUsers); 
                }
                break;
            }

            case 'join': {
                int32 selectedIdx = fListView->CurrentSelection();
                if (selectedIdx >= 0) {
                    ChannelRowItem* selectedItem = dynamic_cast<ChannelRowItem*>(fListView->ItemAt(selectedIdx));
                    if (selectedItem != nullptr && fSocket != nullptr) {
                        BString joinCmd;
                        joinCmd << "JOIN " << selectedItem->GetChannelName() << "\r\n";
                        fSocket->Write(joinCmd.String(), joinCmd.Length());
                        
                        if (fOwnerWindow) fOwnerWindow->Activate(true);
                        PostMessage(B_QUIT_REQUESTED); 
                    }
                }
                break;
            }

            default:
                BWindow::MessageReceived(message);
                break;
        }
    }

    void Quit() override {
        // Loop and explicitly free all custom ChannelRowItem allocations out of the heap pool
        while (fListView->CountItems() > 0) {
            BListItem* item = fListView->RemoveItem((int32)0);
            delete item;
        }

        BMessage notification('cldc'); // Notify parent window fActiveListWindow is dead
        if (fOwnerWindow) fOwnerWindow->PostMessage(&notification);
        BWindow::Quit();
    }

private:
    BWindow*         fOwnerWindow;
    BListView*       fListView;
    BSecureSocket*   fSocket;
    ServerTreeItem*  fServerContext; // <-- ADDED: Private context tracker storage field
};



class ChannelTreeItem : public BStringItem {
public:
    // Accept an additional bool isCustom parameter (defaults to false)
    ChannelTreeItem(const char* text, size_t serverIndex, bool isCustom = false) 
        : BStringItem(text), fServerIndex(serverIndex), fIsCustom(isCustom), fHasUnread(false), fAutoJoin(false) {
        
        BString chanName(text);
        
        // Route the initial autojoin check to the correct config vector array
        if (fIsCustom) {
            if (fServerIndex < cfg.customServers.size()) {
                for (const auto& chan : cfg.customServers[fServerIndex].autojoin) {
                    if (chanName.ICompare(chan.c_str()) == 0) {
                        fAutoJoin = true;
                        break;
                    }
                }
            }
        } else {
            if (fServerIndex < cfg.servers.size()) {
                for (const auto& chan : cfg.servers[fServerIndex].autojoin) {
                    if (chanName.ICompare(chan.c_str()) == 0) {
                        fAutoJoin = true;
                        break;
                    }
                }
            }
        }
    }

    void SetUnread(bool unread) { fHasUnread = unread; }
    bool HasUnread() const { return fHasUnread; }
    
    void SetAutoJoin(bool autoJoin) { fAutoJoin = autoJoin; }
    bool IsAutoJoin() const { return fAutoJoin; }
    size_t GetServerIndex() const { return fServerIndex; }
    bool   IsCustom() const { return fIsCustom; } // NEW: Public getter to query custom flag

    // Override the native drawing framework loop
    void DrawItem(BView* owner, BRect itemRect, bool drawEverything) override {
        owner->PushState();

        if (IsSelected()) {
            owner->SetLowColor(ui_color(B_LIST_SELECTED_BACKGROUND_COLOR));
            owner->SetHighColor(ui_color(B_LIST_SELECTED_ITEM_TEXT_COLOR));
            owner->FillRect(itemRect, B_SOLID_LOW);
        } else {
            owner->SetLowColor(ui_color(B_LIST_BACKGROUND_COLOR));
            owner->SetHighColor(ui_color(B_LIST_ITEM_TEXT_COLOR));
            owner->FillRect(itemRect, B_SOLID_LOW);
        }

        BFont dynamicFont;
        owner->GetFont(&dynamicFont);

        if (fHasUnread && !IsSelected()) {
            dynamicFont.SetFace(B_BOLD_FACE);
            owner->SetHighColor(ui_color(B_LINK_TEXT_COLOR)); 
        } else {
            dynamicFont.SetFace(B_REGULAR_FACE);
        }
        owner->SetFont(&dynamicFont);

        font_height fh;
        dynamicFont.GetHeight(&fh);
        float textBaseline = itemRect.top + fh.ascent + (itemRect.Height() - (fh.ascent + fh.descent + fh.leading)) / 2.0f;
        
        owner->MovePenTo(itemRect.left + 20, textBaseline);
        
        BString displayString = Text();
        if (fAutoJoin) displayString << " [A]";
        owner->DrawString(displayString.String());

        owner->PopState();
    }

private:
    size_t fServerIndex;
    bool   fIsCustom; // NEW: Track vector classification path inside instance memory
    bool   fHasUnread;
    bool   fAutoJoin;
};


class ServerTreeItem : public BStringItem {
public:
    ServerTreeItem(const char* text, size_t configIndex, bool isChannel = false, bool isCustom = false) 
        : BStringItem(text), fConfigIndex(configIndex), fIsChannel(isChannel), fIsCustom(isCustom) {
        
        // Safety bounds check to pull structural configs securely
        if (fIsCustom) {
            if (fConfigIndex < cfg.customServers.size()) {
                fAutoConnect = cfg.customServers[fConfigIndex].autoConnect;
                fAutoReconnect = cfg.customServers[fConfigIndex].autoReconnect;
                fHideStatus = cfg.customServers[fConfigIndex].hideStatusMessages;
            } else {
                fAutoConnect = false;
                fAutoReconnect = false;
            }
        } else {
            if (fConfigIndex < cfg.servers.size()) {
                fAutoConnect = cfg.servers[fConfigIndex].autoConnect;
                fAutoReconnect = cfg.servers[fConfigIndex].autoReconnect;
                fHideStatus = cfg.servers[fConfigIndex].hideStatusMessages;
            } else {
                fAutoConnect = false;
                fAutoReconnect = false;
            }
        }
    }
        
    BString GetHost() const { 
        if (fIsCustom && fConfigIndex < cfg.customServers.size())
            return BString(cfg.customServers[fConfigIndex].host.c_str());
        if (!fIsCustom && fConfigIndex < cfg.servers.size())
            return BString(cfg.servers[fConfigIndex].host.c_str());
        return BString("");
    }

    uint16 GetPort() const { 
        if (fIsCustom && fConfigIndex < cfg.customServers.size())
            return cfg.customServers[fConfigIndex].port;
        if (!fIsCustom && fConfigIndex < cfg.servers.size())
            return cfg.servers[fConfigIndex].port;
        return 6697;
    }

    BString GetNick() const { 
        if (fIsCustom && fConfigIndex < cfg.customServers.size())
            return BString(cfg.customServers[fConfigIndex].nick.c_str());
        if (!fIsCustom && fConfigIndex < cfg.servers.size())
            return BString(cfg.servers[fConfigIndex].nick.c_str());
        return BString("HaikuUser");
    }
    
    BString GetAltNick() const { 
        if (fIsCustom && fConfigIndex < cfg.customServers.size())
            return BString(cfg.customServers[fConfigIndex].altNick.c_str());
        if (!fIsCustom && fConfigIndex < cfg.servers.size())
            return BString(cfg.servers[fConfigIndex].altNick.c_str());
        return BString(cfg.servers[0].nick.c_str()) << "+";
    }

    BString GetAltNick2() const { 
        if (fIsCustom && fConfigIndex < cfg.customServers.size())
            return BString(cfg.customServers[fConfigIndex].altNick2.c_str());
        if (!fIsCustom && fConfigIndex < cfg.servers.size())
            return BString(cfg.servers[fConfigIndex].altNick2.c_str());
        return BString(cfg.servers[0].nick.c_str()) << "__";
    }

    BString GetPass() const { 
        if (fIsCustom && fConfigIndex < cfg.customServers.size())
            return BString(cfg.customServers[fConfigIndex].pass.c_str());
        if (!fIsCustom && fConfigIndex < cfg.servers.size())
            return BString(cfg.servers[fConfigIndex].pass.c_str());
        return BString("");
    }

    std::vector<std::string> GetAutojoin() const { 
        if (fIsCustom && fConfigIndex < cfg.customServers.size())
            return cfg.customServers[fConfigIndex].autojoin;
        if (!fIsCustom && fConfigIndex < cfg.servers.size())
            return cfg.servers[fConfigIndex].autojoin;
        return std::vector<std::string>();
    }

    size_t GetIndex() const { return fConfigIndex; }
    bool   IsCustom() const { return fIsCustom; }
    bool   IsChannel() const { return fIsChannel; }
    
    bool IsAutoConnect() const { return fAutoConnect; }
    void SetAutoConnect(bool autoConnect) { fAutoConnect = autoConnect; }
    bool IsAutoReconnect() const { return fAutoReconnect; }
    void SetAutoReconnect(bool autoReconnect) { fAutoReconnect = autoReconnect; }
    bool IsHideStatus() const { return fHideStatus; }
    void SetHideStatus(bool hide) { fHideStatus = hide; }
	void SetIndex(size_t idx) { fConfigIndex = idx; }
	
    // DrawItem that centers text and indents channels perfectly at any font size
    void DrawItem(BView* owner, BRect itemRect, bool drawEverything) override {
        owner->PushState();
        
        if (IsSelected()) {
            owner->SetLowColor(ui_color(B_LIST_SELECTED_BACKGROUND_COLOR));
            owner->SetHighColor(ui_color(B_LIST_SELECTED_ITEM_TEXT_COLOR));
            owner->FillRect(itemRect, B_SOLID_LOW);
        } else {
            owner->SetLowColor(ui_color(B_LIST_BACKGROUND_COLOR));
            owner->SetHighColor(ui_color(B_LIST_ITEM_TEXT_COLOR));
            owner->FillRect(itemRect, B_SOLID_LOW);
        }

        BFont dynamicFont;
        owner->GetFont(&dynamicFont);
        owner->SetFont(&dynamicFont);

        font_height fh;
        dynamicFont.GetHeight(&fh);
        float textBaseline = itemRect.top + fh.ascent + (itemRect.Height() - (fh.ascent + fh.descent + fh.leading)) / 2.0f;
        
        float leftPadding = fIsChannel ? 24.0f : 4.0f;
        owner->MovePenTo(itemRect.left + leftPadding, textBaseline);
        
        BString displayString = Text();
        if (!fIsChannel) {
            if (fAutoConnect) displayString << " [AC]";
            if (fAutoReconnect) displayString << " [AR]"; 
        }
        owner->DrawString(displayString.String());

        owner->PopState();
    }

private:
    size_t fConfigIndex;
    bool fAutoConnect;
    bool fAutoReconnect;
    bool fHideStatus = false;
    bool fIsChannel; 
    bool fIsCustom; // <-- Tracks whether this item references custom array configs
};





class ServerConfigWindow : public BWindow {
public:
    virtual ~ServerConfigWindow() {}

    // Added bool isCustom = false parameter to the constructor signature
    ServerConfigWindow(BWindow* parent, ServerTreeItem* item, size_t serverIdx, bool isCustom = false)
        // Pass an empty dummy size; B_AUTO_UPDATE_SIZE_LIMITS will force it to fit perfectly!
        : BWindow(BRect(0, 0, 1, 1), "Server Properties", B_MODAL_WINDOW_LOOK, 
                  B_MODAL_SUBSET_WINDOW_FEEL, B_NOT_RESIZABLE | B_NOT_ZOOMABLE | B_AUTO_UPDATE_SIZE_LIMITS) {

        
        fParentWindow = parent;
        fItem = item;
        fServerIdx = serverIdx;
        fIsCustom = isCustom; // NEW: Track vector classification type inside instance memory

        AddToSubset(parent);
        CenterIn(parent->Frame());

        ServerConfig& srv = GetActiveConfig();

        fBgPathInput = new BTextControl("bg_path", "Background Image:", srv.backgroundImagePath.c_str(), nullptr);
        fBrowseBgBtn = new BButton("browse_bg", "Browse…", new BMessage('adbg'));
        fFilePanel = new BFilePanel(B_OPEN_PANEL, new BMessenger(this), nullptr, B_FILE_NODE, false);

        fEnableEmoticonsCheck = new BCheckBox("enable_emotes", "Enable custom inline emoticons for this server", nullptr);
        fEnableEmoticonsCheck->SetValue(srv.enableEmoticons ? B_CONTROL_ON : B_CONTROL_OFF);

        // Instantiate the dimming slider control
        fBgOpacitySlider = new BSlider("bg_opacity_sld", "Wallpaper Dimming Level:", nullptr, 0, 100, B_HORIZONTAL);
        fBgOpacitySlider->SetValue(srv.backgroundOpacity);
        fBgOpacitySlider->SetHashMarks(B_HASH_MARKS_BOTTOM);
        fBgOpacitySlider->SetHashMarkCount(11); // Marks every 10%

        // Instantiate the toggle checkbox right below the background settings layout items
        fEnableEmoticonsCheck = new BCheckBox("enable_emotes", "Enable custom inline emoticons for this server", nullptr);
        fEnableEmoticonsCheck->SetValue(srv.enableEmoticons ? B_CONTROL_ON : B_CONTROL_OFF);

        fNickInput = new BTextControl("nick", "Nickname:", srv.nick.c_str(), nullptr);
        
        // 1. Instantiate the input control blank initially
        fPassInput = new BTextControl("pass", "Password:", "", nullptr);
        
        // 2. Extract a pointer to the inner native BTextView workspace 
        BTextView* passTextView = fPassInput->TextView();
        if (passTextView != nullptr) {
            // Enable Haiku's built-in character masking
            passTextView->HideTyping(true);
        }

        // 3. Set the text string buffer from config structure
        // Clearing and setting it after HideTyping ensures Haiku masks it on boot.
        fPassInput->SetText(srv.pass.c_str());


 
        fServerListFontMenu = CreateFontMenu("Server List Font:", cfg.serverListFontSize);
        fChatLogFontMenu    = CreateFontMenu("Chat Log Font:", cfg.chatLogFontSize);
        fUserListFontMenu   = CreateFontMenu("User List Font:", cfg.userListFontSize);

        fAutoConnectCheck = new BCheckBox("autoconnect", "Automatically connect at startup", nullptr);
        fAutoConnectCheck->SetValue(srv.autoConnect ? B_CONTROL_ON : B_CONTROL_OFF);

        fAutoReconnectCheck = new BCheckBox("autoreconnect", "Automatically reconnect on drop", nullptr);
        fAutoReconnectCheck->SetValue(srv.autoReconnect ? B_CONTROL_ON : B_CONTROL_OFF);

        fDebugEnableCheck = new BCheckBox("debug", "Enable low-level socket engine logs", nullptr);
        fDebugEnableCheck->SetValue(cfg.debugEnable ? B_CONTROL_ON : B_CONTROL_OFF);

        fHideStatusCheck = new BCheckBox("hidestatus", "Hide channel status messages (Joins/Parts/Quits)", nullptr);
        fHideStatusCheck->SetValue(srv.hideStatusMessages ? B_CONTROL_ON : B_CONTROL_OFF);

        BButton* cancelBtn = new BButton("cancel", "Cancel", new BMessage('cfcn'));
        BButton* saveBtn = new BButton("save", "Save", new BMessage('cfsv'));
        saveBtn->MakeDefault(true);

		fQuitInput = new BTextControl("quitmsg", "QUIT Message:", cfg.quitMessage.c_str(), nullptr);
		fAwayInput = new BTextControl("awaymsg", "AWAY Message:", cfg.awayMessage.c_str(), nullptr);
		
		fAltNickInput  = new BTextControl("altnick", "Alt Nick 1:", srv.altNick.c_str(), nullptr);
		fAltNick2Input = new BTextControl("altnick2", "Alt Nick 2:", srv.altNick2.c_str(), nullptr);
		
        BLayoutBuilder::Group<>(this, B_VERTICAL, 10)
            .SetInsets(15)
            .AddGrid(5.0f, 5.0f)
    		    // Row 0: Nickname
    			.Add(fNickInput->CreateLabelLayoutItem(), 0, 0)
    			.Add(fNickInput->CreateTextViewLayoutItem(), 1, 0)
    			
    			// Row 1: Alt Nick 1
    			.Add(fAltNickInput->CreateLabelLayoutItem(), 0, 1)
    			.Add(fAltNickInput->CreateTextViewLayoutItem(), 1, 1)
    			
    			// Row 2: Alt Nick 2
    			.Add(fAltNick2Input->CreateLabelLayoutItem(), 0, 2)
    			.Add(fAltNick2Input->CreateTextViewLayoutItem(), 1, 2)
    			
    			// Row 3: Password (Now isolated safely)
    			.Add(fPassInput->CreateLabelLayoutItem(), 0, 3)     
    			.Add(fPassInput->CreateTextViewLayoutItem(), 1, 3)
            
            	// Row 4: FIXED - Shifted Quit Message down to its own row to stop overlaps!
            	.Add(fAwayInput->CreateLabelLayoutItem(), 0, 4)
            	.Add(fAwayInput->CreateTextViewLayoutItem(), 1, 4)  
            	
            	// Row 4: FIXED - Shifted Quit Message down to its own row to stop overlaps!
            	.Add(fQuitInput->CreateLabelLayoutItem(), 0, 5)
            	.Add(fQuitInput->CreateTextViewLayoutItem(), 1, 5)         
    
                // Rows 5-7: FIXED - Corrected font alignments sequentially
            	.Add(fServerListFontMenu->CreateLabelLayoutItem(), 0, 6)
            	.Add(fServerListFontMenu->CreateMenuBarLayoutItem(), 1, 6)
            	
            	.Add(fChatLogFontMenu->CreateLabelLayoutItem(), 0, 7)
            	.Add(fChatLogFontMenu->CreateMenuBarLayoutItem(), 1, 7)
            	
            	.Add(fUserListFontMenu->CreateLabelLayoutItem(), 0, 8)
            	.Add(fUserListFontMenu->CreateMenuBarLayoutItem(), 1, 8)
                .Add(fBgPathInput->CreateLabelLayoutItem(), 0, 9)
                .AddGroup(B_HORIZONTAL, 5, 1, 9)
                    .Add(fBgPathInput->CreateTextViewLayoutItem(), 1.0)
                    .Add(fBrowseBgBtn, 0.0)
                .End()
            .End()
            .Add(fAutoConnectCheck)
            .Add(fAutoReconnectCheck)
            .Add(fHideStatusCheck)
            .Add(fDebugEnableCheck)      
            .Add(fEnableEmoticonsCheck) 
            .Add(fBgOpacitySlider) 
            .AddGlue()
            .AddGroup(B_HORIZONTAL, 10)
                .AddGlue()
                .Add(cancelBtn)
                .Add(saveBtn)
            .End();
            
            
    	if (parent) {
        	CenterIn(parent->Frame()); 
    		} else {
        	CenterOnScreen(); 
    	}

    }
    
    

    void MessageReceived(BMessage* message) override {
        switch (message->what) {
        	
        	
        	case 'adbg':
                if (fFilePanel != nullptr) {
                    fFilePanel->Show();
                }
                break;

            case B_REFS_RECEIVED: {
                // Fired automatically when an item is selected from Haiku's file browser panel
                entry_ref ref;
                if (message->FindRef("refs", &ref) == B_OK) {
                    BEntry entry(&ref, true);
                    BPath path;
                    if (entry.GetPath(&path) == B_OK) {
                        // Push the clean absolute path string right back into text display view
                        fBgPathInput->SetText(path.Path());
                    }
                }
                break;
            }
        	
            case 'cfcn':
            	delete fFilePanel; 
                Quit();
                break;
                
            case 'cfsv': {
                // Dynamically route data saving to the correct active vector configuration structure 
                ServerConfig& srv = GetActiveConfig();
                srv.nick = fNickInput->Text();
                srv.altNick  = fAltNickInput->Text();
				srv.altNick2 = fAltNick2Input->Text();

                // SAFETY GUARD: Only overwrite the password if the user actually typed something new.
                // If the field is visually blank but they didn't touch it, preserve the loaded configuration string!
                BString inputPassword = fPassInput->Text();
                if (inputPassword.Length() > 0) {
                    srv.pass = inputPassword.String();
                }         
                
                srv.backgroundImagePath = fBgPathInput->Text();
                srv.backgroundOpacity = fBgOpacitySlider->Value(); 
                srv.enableEmoticons = (fEnableEmoticonsCheck->Value() == B_CONTROL_ON);
                srv.pass = fPassInput->Text();
                srv.autoConnect = (fAutoConnectCheck->Value() == B_CONTROL_ON);
                srv.autoReconnect = (fAutoReconnectCheck->Value() == B_CONTROL_ON);
                cfg.debugEnable = (fDebugEnableCheck->Value() == B_CONTROL_ON);
                cfg.awayMessage = fAwayInput->Text();
				cfg.quitMessage = fQuitInput->Text();
                srv.hideStatusMessages = (fHideStatusCheck->Value() == B_CONTROL_ON);
				cfg.debugEnable = (fDebugEnableCheck->Value() == B_CONTROL_ON);
				
                BMenuItem* item = fServerListFontMenu->Menu()->FindMarked();
                if (item && item->Message()) cfg.serverListFontSize = item->Message()->FindInt32("size");

                item = fChatLogFontMenu->Menu()->FindMarked();
                if (item && item->Message()) cfg.chatLogFontSize = item->Message()->FindInt32("size");

                item = fUserListFontMenu->Menu()->FindMarked();
                if (item && item->Message()) cfg.userListFontSize = item->Message()->FindInt32("size");

                if (fItem != nullptr) {
                    fItem->SetAutoConnect(srv.autoConnect);
                    fItem->SetAutoReconnect(srv.autoReconnect);
                    fItem->SetHideStatus(srv.hideStatusMessages);
                }

                BMessage updateNotify('mscf'); 
                if (fParentWindow) fParentWindow->PostMessage(&updateNotify);
            	delete fFilePanel; 
                Quit();
                break;
            }

            default:
                BWindow::MessageReceived(message);
                break;
        }
    }

private:
    BWindow*            fParentWindow;
    ServerTreeItem*     fItem;
    size_t              fServerIdx;
    bool                fIsCustom; 
	BTextControl* 		fQuitInput;
	BTextControl* 		fAwayInput;
    BTextControl*       fNickInput;
    BTextControl*       fAltNickInput;
    BTextControl*       fAltNick2Input; 
    BTextControl*       fPassInput;
    BCheckBox*          fAutoConnectCheck;
    BCheckBox*          fAutoReconnectCheck;
    BCheckBox*          fDebugEnableCheck;
    BCheckBox*          fHideStatusCheck;
    BMenuField*         fServerListFontMenu;
    BMenuField*         fChatLogFontMenu;
    BMenuField*         fUserListFontMenu;
    BTextControl*       fBgPathInput;
    BButton*            fBrowseBgBtn;
    BFilePanel*         fFilePanel;
    BCheckBox*          fEnableEmoticonsCheck; 
	BSlider*            fBgOpacitySlider; 
    // NEW: Safe helper function to fetch correct active reference target safely
    ServerConfig& GetActiveConfig() {
        if (fIsCustom && fServerIdx < cfg.customServers.size()) {
            return cfg.customServers[fServerIdx];
        }
        // Fallback or standard hardcoded element reference loop block
        return cfg.servers[fServerIdx];
    }

    BMenuField* CreateFontMenu(const char* label, int32 currentSize) {
        BPopUpMenu* menu = new BPopUpMenu("font_popup");
        int32 sizes[] = {9, 10, 11, 12, 14, 16, 18, 20, 24};
        
        for (int i = 0; i < 9; i++) {
            BString sizeStr;
            sizeStr << sizes[i];
            
            BMessage* msg = new BMessage('fntc');
            msg->AddInt32("size", sizes[i]);
            
            BMenuItem* item = new BMenuItem(sizeStr.String(), msg);
            if (sizes[i] == currentSize) {
                item->SetMarked(true);
            }
            menu->AddItem(item);
        }
        return new BMenuField("font_field", label, menu);
    }
};



// Sorts users by rank (@ on top, then +, then regular users), then alphabetically
static int SortUsersByRank(const void* first, const void* second) {
    const BListItem* itemPtrA = *static_cast<const BListItem* const*>(first);
    const BListItem* itemPtrB = *static_cast<const BListItem* const*>(second);

    const BStringItem* userA = dynamic_cast<const BStringItem*>(itemPtrA);
    const BStringItem* userB = dynamic_cast<const BStringItem*>(itemPtrB);

    if (userA == nullptr || userB == nullptr) return 0;

    BString nameA(userA->Text());
    BString nameB(userB->Text());

    // 1. Assign numeric rank weights (higher numbers = higher priority)
    int32 rankA = 0;
    int32 rankB = 0;

    if (nameA.StartsWith("@")) rankA = 2;
    else if (nameA.StartsWith("+")) rankA = 1;

    if (nameB.StartsWith("@")) rankB = 2;
    else if (nameB.StartsWith("+")) rankB = 1;

    // 2. If ranks are different, sort by rank weight descending
    if (rankA != rankB) {
        return (rankA > rankB) ? -1 : 1;
    }

    // 3. If ranks are identical, strip prefixes and sort alphabetically
    if (rankA > 0) nameA.Remove(0, 1);
    if (rankB > 0) nameB.Remove(0, 1);

    return nameA.ICompare(nameB);
}




class CricketWindow : public BWindow {
public:
    // --- Restore flexible window border decoration masks ---
    CricketWindow() : BWindow(BRect(100, 100, 900, 600), "Cricket IRC Client", 
                        B_DOCUMENT_WINDOW, B_ASYNCHRONOUS_CONTROLS) {
        

        SetFlags(Flags() | B_QUIT_ON_WINDOW_CLOSE);

        //@constructor
        BString windowTitle;
        windowTitle << AppInfo::VERSION_STRING;
        SetTitle(windowTitle.String());
        


        
        // 1. Setup Channel Tree View (Left Side)
        fChannelTree = new BOutlineListView("channel_tree");
        BScrollView* channelScroll = new BScrollView("scroll_channels", fChannelTree, 0, false, true);
        
        // --- NEW COMPACT 4x4 EMOTICON GRID CONTAINER ---
        // UPDATE: Store the instance address directly into your class member pointer
        fEmoticonGrid = new BGridView();
        BGridLayout* gridLayout = fEmoticonGrid->GridLayout();
        
        gridLayout->SetHorizontalSpacing(2.0f);
        gridLayout->SetVerticalSpacing(2.0f);
        gridLayout->SetInsets(2, 2, 2, 2);
        struct EmoteButtonMap { const char* trigger; const uint8* iconData; const char* tooltip; };
        EmoteButtonMap barEmotes[] = {
            // Row 1: First 4 emoticons
            {":)", kIconSmile, "Smile"},       {":(", kIconFrown, "Frown"},
            {";)", kIconWink, "Wink"},         {":P", kIconTongue, "Tongue"},
            // Row 2: Next 4 emoticons
            {"lol", kIconLaughing, "Laughing"},{":O", kIconAstonished, "Shocked"},
            {"<3", kIconHeart, "Heart"},       {":|", kIconConfused, "Confused"}
        };

        // Loop through and position each button precisely inside the grid coordinates
        for (int32 i = 0; i < 8; i++) {
            BMessage* clickMsg = new BMessage('emcl');
            clickMsg->AddString("trigger", barEmotes[i].trigger);

            BButton* btn = new BButton(barEmotes[i].trigger, "", clickMsg);
            
            // Render the vector graphic directly onto the button face
            BBitmap* btnIcon = new BBitmap(BRect(0, 0, 15, 15), B_RGBA32);
            if (BIconUtils::GetVectorIcon(barEmotes[i].iconData, 1024, btnIcon) == B_OK) {
                btn->SetIcon(btnIcon);
            }
            delete btnIcon;

            btn->SetFlat(true);
            btn->SetToolTip(barEmotes[i].tooltip);
            
            // Fixed cell sizing matches standard flat square layout specs
            btn->SetExplicitPreferredSize(BSize(22, 22));
            btn->SetTarget(this);

            // Matrix Mapping: Row calculation = (i / 4), Column calculation = (i % 4)
            int32 column = i % 4;
            int32 row = i / 4;
            
            // AddView(view, column, row, columnSpan, rowSpan)
            gridLayout->AddView(btn, column, row, 1, 1);
        }
        // --- END OF GRID SELECTION SETUP ---

        
        // 2. Load Config and Populate Servers Tree ONCE
        load_config(); 

        // Loop A: Populate hardcoded default servers (Libera / OFTC)
        for (size_t i = 0; i < cfg.servers.size(); i++) {
            ServerTreeItem* serverNode = new ServerTreeItem(cfg.servers[i].name.c_str(), i, false, false);
            serverNode->SetHideStatus(cfg.servers[i].hideStatusMessages);
            fChannelTree->AddItem(serverNode);
            
            if (i == 0) fLiberaNode = serverNode;
            if (i == 1) fOftcNode = serverNode;
        }

        // Loop B: Populate user-defined custom servers dynamically
        for (size_t i = 0; i < cfg.customServers.size(); i++) {
            ServerTreeItem* customNode = new ServerTreeItem(cfg.customServers[i].name.c_str(), i, false, true);
            customNode->SetHideStatus(cfg.customServers[i].hideStatusMessages);
            fChannelTree->AddItem(customNode);
        }
        
        // 3. Create Topic View bar Control
        // REMOVED: MakeEditable(false) to allow typing inside the field
        fTopicView = new BTextControl("topic_view", "Topic:", "No topic set.", new BMessage(MSG_TOPIC_CHANGED));
        
        // Ensure that this text field sends its messaging packet back into your main window context
        fTopicView->SetTarget(this);
        
        // 4. Setup Middle & Right Components
        fChatLog = new BTextView("chat_log");
        fChatLog->MakeEditable(false);
        fChatLog->SetStylable(true);

        BScrollView* chatScroll = new BScrollView("scroll_chat", fChatLog, 0, false, true);

        // --- NEW: Custom Draw Engine View Canvas Instantiation ---
        fCustomChatLog = new CustomChatView(BRect(), "custom_draw_view", B_FOLLOW_ALL, B_WILL_DRAW);
        // Note: Wrapping a direct BView inside a scrollview requires explicit scroll implementation.
        // For simple presentation management, we swap whole container view hierarchies.
        BScrollView* customScroll = new BScrollView("scroll_custom", fCustomChatLog, 0, false, true);

        // --- NEW: Layout-Safe Group View Container Setup ---
        // Using BGroupView seamlessly integrates with Layout Builders for runtime swapping
        fChatContainer = new BGroupView(B_VERTICAL, 0);
        
        fUserList = new BListView("user_list");
        
        fUserList->SetSelectionMessage(new BMessage(MSG_USER_LIST_CONTEXT_CLICK));

        BScrollView* userScroll = new BScrollView("scroll_users", fUserList, 0, false, true);



        fInputControl = new BTextControl("input", "", "", new BMessage(MSG_SEND_MESSAGE));
        fConnectButton = new BButton("connect", "Join #Haiku!", new BMessage(MSG_START_SIRC));

        // Apply saved font choices on startup
        BFont initialFont;

        fChannelTree->GetFont(&initialFont);
        initialFont.SetSize(cfg.serverListFontSize);
        fChannelTree->SetFont(&initialFont);

        fChatLog->GetFont(&initialFont);
        initialFont.SetSize(cfg.chatLogFontSize);
        fChatLog->SetFont(&initialFont);

        // Also push text size preferences right down to custom engine tracker
        fCustomChatLog->SetLineHeight(cfg.chatLogFontSize + 4.0f); 

        fUserList->GetFont(&initialFont);
        initialFont.SetSize(cfg.userListFontSize);
        fUserList->SetFont(&initialFont);
        
        fTopicView->GetFont(&initialFont);
        initialFont.SetSize(cfg.chatLogFontSize);
        fTopicView->SetFont(&initialFont);
        fTopicView->TextView()->SetFontAndColor(&initialFont, B_FONT_SIZE);

       // 5. Layout Architecture using Adjustable Split Panes
        BSplitView* mainSplitter = new BSplitView(B_HORIZONTAL, 5.0f);
        
        BView* centerStackView = BLayoutBuilder::Group<>(B_VERTICAL, 5)
            .Add(fTopicView, 0.0)         
            .Add(fChatContainer, 1.0)     
            .View();

        mainSplitter->AddChild(channelScroll, 0.16f);  
        mainSplitter->AddChild(centerStackView, 0.73f); 
        mainSplitter->AddChild(userScroll, 0.11f);     

        channelScroll->SetExplicitPreferredSize(BSize(130, B_SIZE_UNLIMITED)); 
        userScroll->SetExplicitPreferredSize(BSize(110, B_SIZE_UNLIMITED));    

        // Place the 2-row fEmoticonGrid into your bottom toolbar builder
        BLayoutBuilder::Group<>(this, B_VERTICAL, 5)
            .SetInsets(10)
            .Add(mainSplitter, 1.0) 
            .AddGroup(B_HORIZONTAL, 5, 0.0) 
                .Add(fEmoticonGrid, 0.0)    // UPDATE: Placed your tracked member view cleanly here
                .Add(fInputControl, 1.0)     
                .Add(fConnectButton, 0.0)    
            .End();


        // 6. Initialize State Properties
        fChannelTree->SetSelectionMessage(new BMessage('slch'));
        fActiveBufferItem = nullptr;        
        fLiberaThread = -1;
        fOftcThread = -1;
        fLiberaSocket = nullptr;
        fOftcSocket = nullptr;
        fCurrentServerNode = nullptr;
        
        if (!cfg.servers.empty()) {
            fMyNick = cfg.servers[0].nick.c_str();
        } else {
            // High-precision clock backup fallback to generate matching unique IDs anywhere
            srand(static_cast<unsigned int>(real_time_clock_usecs()));
            int randomSuffix = 1000 + (rand() % 9000);
            fMyNick.SetToFormat("HaikuIRCUser%d", randomSuffix);
        }
        
        // 7. Process Automatic Startup Connections instantly
        for (int32 i = 0; i < fChannelTree->CountItems(); i++) {
            ServerTreeItem* srvNode = dynamic_cast<ServerTreeItem*>(fChannelTree->ItemAt(i));
            if (srvNode != nullptr) {
                bool triggerConnect = false;
                size_t idx = srvNode->GetIndex();

                // Check the configuration storage location based on the server node classification type
                if (srvNode->IsCustom()) {
                    if (idx < cfg.customServers.size()) {
                        triggerConnect = cfg.customServers[idx].autoConnect;
                    }
                } else {
                    if (idx < cfg.servers.size()) {
                        triggerConnect = cfg.servers[idx].autoConnect;
                    }
                }

                if (triggerConnect) {
                    ConnectToServer(srvNode);
                }
            }
        }              
    }




	
void Show()
{
    // 1. Allow the base Haiku window engine to map layout sub-containers first
    BWindow::Show();

    // 2. DYNAMIC BACKGROUND INITIALIZATION ON BOOT
    if (fCustomChatLog != nullptr) {
        std::string targetServerName = "";

        // If a server is actively highlighted on launch, grab its name
        if (fCurrentServerNode != nullptr) {
            targetServerName = fCurrentServerNode->Text();
        } 
        // Fallback: If nothing is selected yet, default to the first server in the tree
        else if (fChannelTree != nullptr && fChannelTree->CountItems() > 0) {
            ServerTreeItem* firstServer = dynamic_cast<ServerTreeItem*>(fChannelTree->ItemAt(0));
            if (firstServer != nullptr) {
                targetServerName = firstServer->Text();
            }
        }

        // Apply background matching configurations
        if (!targetServerName.empty()) {
            bool found = false;
            for (size_t i = 0; i < cfg.servers.size(); i++) {
                if (cfg.servers[i].name == targetServerName) {
                    if (!cfg.servers[i].backgroundImagePath.empty()) {
                        fCustomChatLog->SetBackgroundImage(cfg.servers[i].backgroundImagePath.c_str());
                        fCustomChatLog->SetBackgroundDimming(cfg.servers[i].backgroundOpacity);
                    }
                    found = true;
                    break;
                }
            }
            if (!found) {
                for (size_t i = 0; i < cfg.customServers.size(); i++) {
                    if (cfg.customServers[i].name == targetServerName) {
                        if (!cfg.customServers[i].backgroundImagePath.empty()) {
                            fCustomChatLog->SetBackgroundImage(cfg.customServers[i].backgroundImagePath.c_str());
                            fCustomChatLog->SetBackgroundDimming(cfg.customServers[i].backgroundOpacity);
                        }
                        break;
                    }
                }
            }
        }
    }

    // 3. Trigger the high-performance engine swap pass after the initial draw cycle
    if (cfg.useCustomDrawFunction) {
        BMessage initDrawMsg(MSG_TOGGLE_CUSTOM_DRAW);
        initDrawMsg.AddBool("initial_boot_pass", true);
        this->PostMessage(&initDrawMsg);
    }
}





    ~CricketWindow() {
        // 1. Prepare the custom sign-off message payload
        BString quitPayload;
        quitPayload << "QUIT :" << cfg.quitMessage.c_str() << "\r\n";

        std::vector<std::pair<thread_id, BSecureSocket*>> shutdownSnapshot;
        
        Lock();
        for (auto const& [serverNode, socketPtr] : fServerSockets) {
            if (socketPtr != nullptr) {
                socketPtr->Write(quitPayload.String(), quitPayload.Length());
                thread_id tid = (fServerThreads.count(serverNode) > 0) ? fServerThreads[serverNode] : -1;
                shutdownSnapshot.push_back({tid, socketPtr});
            }
        }
        Unlock();

        // Give the networking hardware a brief moment to flush the QUIT strings out
        snooze(50000); // 50ms

        // 2. Disconnect all sockets to tell the threads to drop out of their loops
        for (auto const& [tid, socketPtr] : shutdownSnapshot) {
            if (socketPtr != nullptr) {
                socketPtr->Disconnect(); 
            }
        }

        // Give the background worker threads a quick window to wake up and close on their own
        snooze(50000); // 50ms

        // 3. FORCE KILL STUBBORN THREADS: Completely breaks any deadlocks
        for (auto const& [tid, socketPtr] : shutdownSnapshot) {
            if (tid >= 0) {
                // If the thread is still alive and didn't close gracefully, kill it instantly!
                // This stops it from trapping wait_for_thread in a deadlock.
                thread_info info;
                if (get_thread_info(tid, &info) == B_OK) {
                    kill_thread(tid); 
                }
            }
        }

        // 4. Clean up memory collections safely now that threads are guaranteed dead
        for (auto const& [key, listPtr] : fChannelUsers) {
            delete listPtr;
        }

        // 5. Force the application process to instantly terminate and leave the Deskbar
        be_app->PostMessage(B_QUIT_REQUESTED); 
    }



void RebuildActiveChannelBuffer() {
	fIsLoadingHistory = true;
    if (fActiveBufferItem == nullptr || !fChatLog || !fCustomChatLog) return;

    // 1. Fetch the raw historical plain-text string from memory cache map
    BString channelHistory = fTextBuffers[fActiveBufferItem];
    if (channelHistory.Length() == 0) return;

    // 2. Clear both layout views completely to prevent duplicated text layering
    fChatLog->SetText("");
    fCustomChatLog->ClearAllLines(); // Canvas is now safely cleared out

    // 3. Ensure the custom log view is pointing at the selected workspace channel room
    if (cfg.useCustomDrawFunction) {
        fCustomChatLog->SetActiveChannel(fActiveBufferItem);
    }

    // 4. Tokenize the massive historical text block into separate individual text lines
    int32 startSearch = 0;
    while (startSearch < channelHistory.Length()) {
        int32 nextNewline = channelHistory.FindFirst("\n", startSearch);
        if (nextNewline == B_ERROR) break;

        BString singleLine;
        channelHistory.CopyInto(singleLine, startSearch, nextNewline - startSearch);
        
        // Pass the individual line through the isolated formatting engine
        this->RenderLineWithoutCaching(fActiveBufferItem, singleLine);

        startSearch = nextNewline + 1;
    }

    // 5. Force exactly ONE layout redraw pass once the history is fully built out
    if (cfg.useCustomDrawFunction) {
        fCustomChatLog->Invalidate();
    } else {
        fChatLog->ScrollToSelection();
    }
    fIsLoadingHistory = false;
}




void RenderLineWithoutCaching(BStringItem* itemNode, BString text) {
    if (!text.EndsWith("\n")) text << "\n";
    if (!fChatLog || !fCustomChatLog) return;

    // Prepare Font Variations
    BFont regularChatFont;
    fChatLog->GetFont(&regularChatFont);
    regularChatFont.SetSize(cfg.chatLogFontSize);

    BFont boldChatFont = regularChatFont;
    boldChatFont.SetFace(B_BOLD_FACE);

    BFont urlLinkFont = regularChatFont;
    urlLinkFont.SetFace(B_UNDERSCORE_FACE);

    // Define explicit, adaptive colors matching Haiku global UI preferences
    rgb_color textColor = ui_color(B_DOCUMENT_TEXT_COLOR);     
    rgb_color hyperlinkColor = {51, 153, 255, 255}; 

    // Find Positions
    int32 openingBracketIdx = text.FindFirst("<");
    int32 closingBracketIdx = text.FindFirst("> ");
    int32 urlStartIdx = text.IFindFirst("http://");
    if (urlStartIdx == B_ERROR) urlStartIdx = text.IFindFirst("https://");

    // 1. Allocate the run array layout cleanly before routing
    int32 maxRuns = 6;
    size_t arraySize = sizeof(text_run_array) + (sizeof(text_run) * (maxRuns - 1));
    text_run_array* runArray = (text_run_array*)malloc(arraySize);
    
    if (runArray != nullptr) {
        int32 runCount = 0;

        // RULE 0: Start of line is always Regular
        runArray->runs[runCount].offset = 0;
        runArray->runs[runCount].font = regularChatFont;
        runArray->runs[runCount].color = textColor;
        runCount++;

        // NICKNAME BOLDING (<Nick>)
        if (openingBracketIdx != B_ERROR && closingBracketIdx != B_ERROR && openingBracketIdx < closingBracketIdx) {
            if (openingBracketIdx > 0) {
                runArray->runs[runCount].offset = openingBracketIdx;
                runArray->runs[runCount].font = boldChatFont;
                runArray->runs[runCount].color = textColor;
                runCount++;
            } else {
                runArray->runs[0].font = boldChatFont;
            }

            // Reset to Regular text right after the closing bracket "> "
            runArray->runs[runCount].offset = closingBracketIdx + 2;
            runArray->runs[runCount].font = regularChatFont;
            runArray->runs[runCount].color = textColor;
            runCount++;
        }

        // URL COLORING (http...)
        if (urlStartIdx != B_ERROR) {
            bool inserted = false;
            for (int32 i = 0; i < runCount; i++) {
                if (runArray->runs[i].offset == urlStartIdx) {
                    runArray->runs[i].font = urlLinkFont;
                    runArray->runs[i].color = hyperlinkColor;
                    inserted = true;
                    break;
                }
            }

            if (!inserted && (runCount == 0 || urlStartIdx > runArray->runs[runCount - 1].offset)) {
                runArray->runs[runCount].offset = urlStartIdx;
                runArray->runs[runCount].font = urlLinkFont;
                runArray->runs[runCount].color = hyperlinkColor;
                runCount++;
            }

            // Reset back to Regular text after the URL
            int32 urlEndIdx = text.FindFirst(" ", urlStartIdx);
            if (urlEndIdx == B_ERROR) urlEndIdx = text.FindFirst("\n", urlStartIdx);
            if (urlEndIdx == B_ERROR) urlEndIdx = text.Length();
            
            // Enforce structural style reset at the link terminal layout boundary 
            if (urlEndIdx <= text.Length() && runCount < maxRuns) {
                runArray->runs[runCount].offset = urlEndIdx;
                runArray->runs[runCount].font = regularChatFont;
                runArray->runs[runCount].color = textColor;
                runCount++;
            }
        }

        runArray->count = runCount;

        // --- Route to the active view layer constraints ---
        if (cfg.useCustomDrawFunction) {
            // Pass incoming itemNode directly to preserve multi-room isolation routes
            fCustomChatLog->AddStyledLine(itemNode, text, runArray);
        } else {
            int32 textLength = fChatLog->TextLength();
            fChatLog->Select(textLength, textLength);
            fChatLog->Insert(text.String(), runArray);
        }
        
        free(runArray);
    } else {
        // Fallback insertion route if system out of memory
        if (cfg.useCustomDrawFunction) {
            // Pass incoming itemNode directly here as well
            fCustomChatLog->AddStyledLine(itemNode, text, nullptr);
        } else {
            int32 textLength = fChatLog->TextLength();
            fChatLog->Select(textLength, textLength);
            fChatLog->Insert(text.String());
        }
    }
}





private:
    //@Menus

void ShowContextMenu(BPoint screenPoint, BListItem* item) {
    if (item == nullptr) return; // Safety check
    
    // 1. Channel menu logic
    ChannelTreeItem* chanItem = dynamic_cast<ChannelTreeItem*>(item);
    if (chanItem != nullptr) {
        BPopUpMenu* menu = new BPopUpMenu("ChannelOptions", false, false);
        
        BString label = chanItem->IsAutoJoin() ? "Disable Auto-Join" : "Enable Auto-Join";
        BMessage* toggleMsg = new BMessage(MSG_TOGGLE_AUTOJOIN);
        toggleMsg->AddPointer("chan_item", chanItem); 
        menu->AddItem(new BMenuItem(label.String(), toggleMsg));
        
        menu->AddSeparatorItem();
        
        BMessage* removeMsg = new BMessage('rmch'); 
        removeMsg->AddPointer("channel_item", chanItem);
        menu->AddItem(new BMenuItem("Remove Channel", removeMsg));
        
        menu->SetTargetForItems(this);
        menu->Go(screenPoint, true, true, true);
        return;
    }

    // 2. Server menu logic
    ServerTreeItem* srvItem = dynamic_cast<ServerTreeItem*>(item);
    if (srvItem != nullptr) {
        BPopUpMenu* menu = new BPopUpMenu("ServerOptions", false, false);
        
        bool isConnected = false;
        if (fServerSockets.find(srvItem) != fServerSockets.end()) {
            if (fServerSockets[srvItem] != nullptr) {
                isConnected = true;
            }
        }

        if (isConnected) {
            BMessage* disconnectMsg = new BMessage(MSG_DISCONNECT_SERVER);
            disconnectMsg->AddPointer("server_item", srvItem);
            menu->AddItem(new BMenuItem("Disconnect", disconnectMsg));
        } else {
            if (srvItem->IsCustom()) {
                BMessage* connectMsg = new BMessage(MSG_CONNECT_CUSTOM_SERVER);
                connectMsg->AddPointer("server_item", srvItem);
                menu->AddItem(new BMenuItem("Connect", connectMsg));
            } else {
                uint32 commandMsg = (srvItem == fLiberaNode) ? MSG_CONNECT_LIBERA : MSG_CONNECT_OFTC;
                menu->AddItem(new BMenuItem("Connect", new BMessage(commandMsg)));
            }
        }
        menu->AddSeparatorItem();

        // Toggle Auto-Connect Option
        BString toggleConnectLabel = "Auto-Connect on Startup";
        BMessage* toggleConnectMsg = new BMessage(MSG_TOGGLE_AUTOCONNECT);
        toggleConnectMsg->AddPointer("server_item", srvItem);
        BMenuItem* connectMenuItem = new BMenuItem(toggleConnectLabel.String(), toggleConnectMsg);
        if (srvItem->IsAutoConnect()) connectMenuItem->SetMarked(true);
        menu->AddItem(connectMenuItem);

        // Toggle Auto-Reconnect Option
        BString toggleReconnectLabel = srvItem->IsAutoReconnect() ? "Auto-Reconnect Enabled" : "Enable Auto-Reconnect";
        BMessage* toggleReconnectMsg = new BMessage(MSG_TOGGLE_AUTORECONNECT);
        toggleReconnectMsg->AddPointer("server_item", srvItem);
        BMenuItem* reconnectMenuItem = new BMenuItem(toggleReconnectLabel.String(), toggleReconnectMsg);
        if (srvItem->IsAutoReconnect()) reconnectMenuItem->SetMarked(true);
        menu->AddItem(reconnectMenuItem);
        
        // Status Messages Suppression Option
        BString toggleStatusLabel = srvItem->IsHideStatus() ? "Status Messages Hidden" : "Status Messages Visible";
        BMessage* toggleStatusMsg = new BMessage(MSG_TOGGLE_HIDE_STATUS);
        toggleStatusMsg->AddPointer("server_item", srvItem);
        BMenuItem* statusMenuItem = new BMenuItem(toggleStatusLabel.String(), toggleStatusMsg);
        if (srvItem->IsHideStatus()) statusMenuItem->SetMarked(true);
        menu->AddItem(statusMenuItem);
   		 
        // Custom Draw Engine Toggle 
        BString toggleDrawLabel = "Enable High Performance Draw Engine";
        BMessage* toggleDrawMsg = new BMessage(MSG_TOGGLE_CUSTOM_DRAW);
        toggleDrawMsg->AddPointer("server_item", srvItem);
        BMenuItem* drawMenuItem = new BMenuItem(toggleDrawLabel.String(), toggleDrawMsg);
        // Uses the runtime configuration flag to set initial checkmark state
        if (cfg.useCustomDrawFunction) drawMenuItem->SetMarked(true);
        menu->AddItem(drawMenuItem);
        
        // Open specific server configuration window
        menu->AddSeparatorItem();
        BMessage* configMsg = new BMessage(MSG_CONTEXT_CONFIGURE_SERVER);
        configMsg->AddPointer("server_item", srvItem);
        menu->AddItem(new BMenuItem("Configure Server...", configMsg));
        
        
        // Channel List Option
        menu->AddSeparatorItem();
        BMessage* listMsg = new BMessage(MSG_CONTEXT_CHAN_LIST);
        listMsg->AddPointer("server_item", srvItem);
        menu->AddItem(new BMenuItem("Channel List", listMsg));
        
        // Put Add Server Here 
        menu->AddSeparatorItem();
        BMessage* addSrvMsg = new BMessage('adcs'); // Reuses your original button message identifier
        menu->AddItem(new BMenuItem("Add Server…", addSrvMsg));
        

        if (srvItem->IsCustom()) {
            menu->AddSeparatorItem();
            BMessage* deleteMsg = new BMessage('dlcs'); 
            deleteMsg->AddPointer("server_item", srvItem);
            menu->AddItem(new BMenuItem("Delete Custom Server", deleteMsg));
        }
        
        
            // About Option
        menu->AddSeparatorItem();
        BMessage* aboutMsg = new BMessage(MSG_CONTEXT_ABOUT);
        menu->AddItem(new BMenuItem("About...", aboutMsg));    
        
        menu->SetTargetForItems(this);
        menu->Go(screenPoint, true, true, true);
    }
}



    void ConnectToServer(ServerTreeItem* targetNode) {
        if (targetNode == nullptr) return;

        // 1. Check if this specific server is already connecting/connected
        if (fServerSockets.count(targetNode) > 0 && fServerSockets[targetNode] != nullptr) {
            return; 
        }
        
       
        // If nothing is selected yet, make this server root item active
        if (fActiveBufferItem == nullptr) {
            fActiveBufferItem = targetNode;
        }

        // 2. Clear out old historical text errors directly on the targetNode tracking key
        fTextBuffers[targetNode] = "";

        // Dynamically read the name from the item text for the log buffer echo
        BString logMessage;
        logMessage.SetToFormat("Connecting to %s...\n", targetNode->Text());
        fTextBuffers[targetNode] << logMessage;
        
        // Update the screen draw engine if the current node matches the active view focus
        if (fActiveBufferItem == targetNode && fChatLog != nullptr) {
            fChatLog->SetText(fTextBuffers[targetNode].String());
        } else if (fActiveBufferItem == targetNode && fCustomChatLog != nullptr && cfg.useCustomDrawFunction) {
            this->RebuildActiveChannelBuffer();
        }

        // 3. Generate a unique worker name thread label string dynamically
        BString threadName;
        threadName.SetToFormat("%sWorker", targetNode->Text());
        threadName.ReplaceAll(" ", ""); 

        // Spawn the thread and associate it natively inside our index tracker map
        thread_id newThread = spawn_thread(NetworkLoop, threadName.String(), B_NORMAL_PRIORITY, targetNode);
        if (newThread >= 0) {
            fServerThreads[targetNode] = newThread;
            
            if (targetNode == fLiberaNode) fLiberaThread = newThread;
            if (targetNode == fOftcNode)   fOftcThread = newThread;

            resume_thread(newThread);
        }
    }


void LogToItemBuffer(BStringItem* itemNode, BString text) {
    if (itemNode == nullptr) return;
    
    // Ensure trailing newline for layout alignment consistency
    if (!text.EndsWith("\n")) text << "\n";
    fTextBuffers[itemNode] << text;
    
    if (fActiveBufferItem == itemNode) {
        if (!fChatLog || !fCustomChatLog || !fChatContainer) return;

        // TERMINAL DIAGNOSTIC: Print the incoming raw text line string
        // if (cfg.debugEnable) printf("\n[DEBUG] LogToItemBuffer Input text: %s", text.String());

        // Prepare Font Variations
        BFont regularChatFont;
        fChatLog->GetFont(&regularChatFont);
        regularChatFont.SetSize(cfg.chatLogFontSize);

        BFont boldChatFont = regularChatFont;
        boldChatFont.SetFace(B_BOLD_FACE);

        BFont urlLinkFont = regularChatFont;
        urlLinkFont.SetFace(B_UNDERSCORE_FACE);

        // Define explicit, adaptive colors matching Haiku global UI preferences
        rgb_color textColor = ui_color(B_DOCUMENT_TEXT_COLOR);     
        rgb_color hyperlinkColor = {51, 153, 255, 255}; 

        // Find Positions
        int32 openingBracketIdx = text.FindFirst("<");
        int32 closingBracketIdx = text.FindFirst("> ");
        int32 urlStartIdx = text.IFindFirst("http://");
        if (urlStartIdx == B_ERROR) urlStartIdx = text.IFindFirst("https://");

        // TERMINAL DIAGNOSTIC: Print calculated indexing values
       // if (cfg.debugEnable) printf("[DEBUG] Offsets calculated -> OpenBracket: %d, CloseBracket: %d, URLStart: %d\n", 
        //       (int)openingBracketIdx, (int)closingBracketIdx, (int)urlStartIdx);

        // 1. Allocate the run array layout cleanly before routing
        int32 maxRuns = 6;
        size_t arraySize = sizeof(text_run_array) + (sizeof(text_run) * (maxRuns - 1));
        text_run_array* runArray = (text_run_array*)malloc(arraySize);
        
        if (runArray != nullptr) {
            int32 runCount = 0;

            // RULE 0: Start of line is always Regular
            runArray->runs[runCount].offset = 0;
            runArray->runs[runCount].font = regularChatFont;
            runArray->runs[runCount].color = textColor;
            runCount++;

            // NICKNAME BOLDING (<Nick>)
            if (openingBracketIdx != B_ERROR && closingBracketIdx != B_ERROR && openingBracketIdx < closingBracketIdx) {
                if (openingBracketIdx > 0) {
                    runArray->runs[runCount].offset = openingBracketIdx;
                    runArray->runs[runCount].font = boldChatFont;
                    runArray->runs[runCount].color = textColor;
                    // if (cfg.debugEnable) printf("[DEBUG] Added Run %d: Bold face applied at index %d\n", runCount, (int)openingBracketIdx);
                    runCount++;
                } else {
                    runArray->runs[0].font = boldChatFont;
                }

                // Reset to Regular text right after the closing bracket "> "
                runArray->runs[runCount].offset = closingBracketIdx + 2;
                runArray->runs[runCount].font = regularChatFont;
                runArray->runs[runCount].color = textColor;
               // if (cfg.debugEnable) printf("[DEBUG] Added Run %d: Regular face reset applied at index %d\n", runCount, (int)(closingBracketIdx + 2));
                runCount++;
            }

            // URL COLORING (http...)
            if (urlStartIdx != B_ERROR) {
                bool inserted = false;
                for (int32 i = 0; i < runCount; i++) {
                    if (runArray->runs[i].offset == urlStartIdx) {
                        runArray->runs[i].font = urlLinkFont;
                        runArray->runs[i].color = hyperlinkColor;
                        inserted = true;
                        break;
                    }
                }

                if (!inserted && (runCount == 0 || urlStartIdx > runArray->runs[runCount - 1].offset)) {
                    runArray->runs[runCount].offset = urlStartIdx;
                    runArray->runs[runCount].font = urlLinkFont;
                    runArray->runs[runCount].color = hyperlinkColor;
                   // if (cfg.debugEnable) printf("[DEBUG] Added Run %d: Blue link applied at index %d\n", runCount, (int)urlStartIdx);
                    runCount++;
                }

                // Reset back to Regular text after the URL
                int32 urlEndIdx = text.FindFirst(" ", urlStartIdx);
                if (urlEndIdx == B_ERROR) urlEndIdx = text.FindFirst("\n", urlStartIdx);
                if (urlEndIdx == B_ERROR) urlEndIdx = text.Length();
                
                // Enforce structural style reset at the link terminal layout boundary 
                if (urlEndIdx <= text.Length() && runCount < maxRuns) {
                    runArray->runs[runCount].offset = urlEndIdx;
                    runArray->runs[runCount].font = regularChatFont;
                    runArray->runs[runCount].color = textColor;
                  //  if (cfg.debugEnable) printf("[DEBUG] Added Run %d: Regular face reset after link applied at index %d\n", runCount, (int)urlEndIdx);
                    runCount++;
                }
            }

            runArray->count = runCount;
           // if (cfg.debugEnable) printf("[DEBUG] Total run array layout counts submitted: %d\n", (int)runCount);

            // 2. Perform the view-swapping via Layout API using the parent scrollviews
            BView* chatScroll = fChatLog->Parent();       
            BView* customScroll = fCustomChatLog->Parent(); 
            BLayout* layout = fChatContainer->GetLayout();

            if (cfg.useCustomDrawFunction) {
                if (chatScroll && chatScroll->Parent() == fChatContainer) {
                    layout->RemoveView(chatScroll);
                }
                if (customScroll && customScroll->Parent() != fChatContainer) {
                    layout->AddView(customScroll);
                }
                
                // Ensure the canvas updates its active channel visibility node during additions
                fCustomChatLog->SetActiveChannel(fActiveBufferItem);
                
                // Pass itemNode through to the custom draw view engine router 
                fCustomChatLog->AddStyledLine(itemNode, text, runArray); 
            } else {
                if (customScroll && customScroll->Parent() == fChatContainer) {
                    layout->RemoveView(customScroll);
                }
                if (chatScroll && chatScroll->Parent() != fChatContainer) {
                    layout->AddView(chatScroll);
                }
                
                int32 textLength = fChatLog->TextLength();
                fChatLog->Select(textLength, textLength);
                fChatLog->Insert(text.String(), runArray);
            }
            
            free(runArray);
        } else {
            // Fallback insertion route if system out of memory
            if (cfg.useCustomDrawFunction) {
                fCustomChatLog->SetActiveChannel(fActiveBufferItem);
                fCustomChatLog->AddStyledLine(itemNode, text, nullptr);
            } else {
                int32 textLength = fChatLog->TextLength();
                fChatLog->Select(textLength, textLength);
                fChatLog->Insert(text.String());
            }
        }

        
        // Handle view boundary updates and scrolling
        if (cfg.useCustomDrawFunction) {
            fChatContainer->InvalidateLayout();
        } else {
            fChatLog->ScrollToSelection();
        }
    }
}



    BStringItem* FindServerLogNode(ServerTreeItem* serverNode) {
        if (!serverNode) return nullptr;                
        return static_cast<BStringItem*>(serverNode);
    }


    ChannelTreeItem* FindChannelNode(ServerTreeItem* serverNode, BString channelName) {
        if (!serverNode) return nullptr;
        for (int32 i = 0; i < fChannelTree->CountItemsUnder(serverNode, true); i++) {
            // CAST TO OUR NEW TYPE:
            ChannelTreeItem* item = dynamic_cast<ChannelTreeItem*>(fChannelTree->ItemUnderAt(serverNode, true, i));
            if (item && BString(item->Text()).ICompare(channelName) == 0) {
                return item;
            }
        }
        return nullptr;
    }
    
    
void UpdateUserAwayState(ChannelTreeItem* chanNode, const char* nickname, bool isAway) {
    if (!chanNode || fChannelUsers.find(chanNode) == fChannelUsers.end() || fChannelUsers[chanNode] == nullptr) {
        return;
    }

    BObjectList<UserListItem, true>* users = fChannelUsers[chanNode];
    BString cleanSearchNick = nickname;
    cleanSearchNick.Trim();

    for (int32 i = 0; i < users->CountItems(); i++) {
        UserListItem* item = users->ItemAt(i);
        if (item != nullptr) {
            BString itemTxt = item->Text();
            itemTxt.Trim();

            int32 prefixCount = 0;
            while (prefixCount < itemTxt.Length()) {
                char c = itemTxt.ByteAt(prefixCount);
                if (c == '~' || c == '&' || c == '%' || c == '@' || c == '+') {
                    prefixCount++;
                } else {
                    break;
                }
            }
            if (prefixCount > 0) {
                itemTxt.Remove(0, prefixCount);
            }

            // If we find our target user inside the persistent cache array, toggle the flag!
            if (itemTxt == cleanSearchNick) {
                item->SetAway(isAway);

                // --- LIVE INDIVIDUAL ROW REPAINT ---
                if (fActiveBufferItem == chanNode) {
                    int32 uiCount = fUserList->CountItems();
                    for (int32 uiIdx = 0; uiIdx < uiCount; uiIdx++) {
                        UserListItem* uiUser = dynamic_cast<UserListItem*>(fUserList->ItemAt(uiIdx));
                        if (uiUser != nullptr) {
                            BString uiItemTxt = uiUser->Text();
                            while (uiItemTxt.StartsWith("~") || uiItemTxt.StartsWith("&") || 
                                   uiItemTxt.StartsWith("%") || uiItemTxt.StartsWith("@") || 
                                   uiItemTxt.StartsWith("+")) {
                                uiItemTxt.Remove(0, 1);
                            }

                            if (uiItemTxt == cleanSearchNick) {
                                uiUser->SetAway(isAway);
                                fUserList->InvalidateItem(uiIdx); // Redraw just this single row frame!
                                break;
                            }
                        }
                    }
                }
                break; 
            }
        }
    }
}




    
    
    

void RefreshUserListUI() {
    if (fActiveBufferItem != nullptr && BString(fActiveBufferItem->Text()).StartsWith("#")) {
        
        while (fUserList->CountItems() > 0) {
            delete fUserList->RemoveItem((int32)0);
        }

        if (fChannelUsers.find(fActiveBufferItem) != fChannelUsers.end()) {
            BObjectList<UserListItem, true>* users = fChannelUsers[fActiveBufferItem];
            if (users != nullptr) {
                
                BFont userListFont;
                fUserList->GetFont(&userListFont);
                userListFont.SetSize(cfg.userListFontSize);

                for (int32 i = 0; i < users->CountItems(); i++) {
                    UserListItem* cachedItem = users->ItemAt(i);
                    if (cachedItem != nullptr) {
                        // Read away status directly from our typed memory list object
                        bool userIsAway = cachedItem->IsAway();

                        // Instantiate our custom visual item class
                        UserListItem* newUserItem = new UserListItem(cachedItem->Text(), userIsAway);
                        fUserList->AddItem(newUserItem);
                        newUserItem->Update(fUserList, &userListFont);
                    }
                }
                
                fUserList->SortItems(SortUsersByRank);
                fUserList->InvalidateLayout();
                fUserList->Invalidate();
            }
        }
    }
}




void ParseAndDisplayIRC(BString line, ServerTreeItem* contextServer) {   	    	
    line.ReplaceAll("\r", "");
    line.ReplaceAll("\n", ""); // Safe to clean network markers; we append gracefully later

    if (line.Length() == 0 || line.StartsWith("PING")) return;

    BString prefix = "", command = "", trailing = "";
    int32 trailingIdx = line.FindFirst(" :");
    if (trailingIdx != B_ERROR) {
        line.CopyInto(trailing, trailingIdx + 2, line.Length() - trailingIdx - 2);
        line.Truncate(trailingIdx);
    }

    int32 firstSpace = line.FindFirst(" ");
    if (line.StartsWith(":")) {
        line.CopyInto(prefix, 1, firstSpace - 1);
        line.Remove(0, firstSpace + 1);
        firstSpace = line.FindFirst(" ");
    }
    if (firstSpace != B_ERROR) {
        line.CopyInto(command, 0, firstSpace);
        line.Remove(0, firstSpace + 1);
    } else {
        command = line;
    }

    // Use the passed context connection. Fallback to Libera only if null.
    if (contextServer == nullptr) {
        contextServer = (fCurrentServerNode != nullptr) ? fCurrentServerNode : fLiberaNode;
    }

    // Catch Successful Join actions (Handles Us AND Other Users)
    if (command == "JOIN") {
        BString channelJoined = trailing.Length() > 0 ? trailing : line;
        
        // --- Aggressively trim all accidental network padding spaces ---
        channelJoined.Trim();
        channelJoined.ReplaceAll(" ", "");
        
        BString userWhoJoined = prefix;
        int32 exclamIdx = userWhoJoined.FindFirst("!");
        if (exclamIdx != B_ERROR) userWhoJoined.Truncate(exclamIdx);

        // Target the specific server connection context pointer explicitly
        ChannelTreeItem* chanNode = FindChannelNode(contextServer, channelJoined);
        if (!chanNode) {
            // Pass contextServer's custom status flag down into the child channel item node
            ChannelTreeItem* newChanNode = new ChannelTreeItem(channelJoined.String(), 
                                                               contextServer->GetIndex(), 
                                                               contextServer->IsCustom());

            fChannelTree->AddUnder(newChanNode, contextServer); 
            fChannelTree->Expand(contextServer);
            
            // --- Explicitly force trailing newline layout alignment consistent with Custom View Rules ---
            LogToItemBuffer(newChanNode, BString("--- Joined channel ") << channelJoined << "\n");
            fChannelUsers[newChanNode] = new BObjectList<UserListItem, true>(20);
            
            // --- Automatically pivot active channel workspace to the newly joined channel room ---
            fActiveBufferItem = newChanNode;
            if (fCustomChatLog != nullptr) {
                fCustomChatLog->SetActiveChannel(fActiveBufferItem);
            }
            
            // Clear out user list panel right away to prepare for the room's NAMES dump payload
            while (fUserList->CountItems() > 0) {
                delete fUserList->RemoveItem((int32)0);
            }
            fTopicView->SetText(BString("Joined ") << channelJoined << ". Awaiting topic payload...");
            
            // === NEW: Request bulk user profiles to populate away states upon entry ===
            BSecureSocket* activeSocket = nullptr;
            if (contextServer != nullptr && fServerSockets.count(contextServer) > 0) {
                activeSocket = fServerSockets[contextServer];
            } else {
                activeSocket = (contextServer == fOftcNode) ? fOftcSocket : fLiberaSocket;
            }

            if (activeSocket != nullptr) {
                BString syncWho;
                syncWho << "WHO " << channelJoined << "\r\n";
                activeSocket->Write(syncWho.String(), syncWho.Length());
            }
            // ======================================================================
            
        } else {
            // Someone else joined a channel we are already in!
            if (fChannelUsers[chanNode] != nullptr) {
                bool userExists = false;
                for (int32 i = 0; i < fChannelUsers[chanNode]->CountItems(); i++) {
                    BString itemTxt = fChannelUsers[chanNode]->ItemAt(i)->Text();
                    // Strip prefix for comparison matching
                    if (itemTxt.StartsWith("@") || itemTxt.StartsWith("+")) itemTxt.Remove(0, 1);
                    if (itemTxt == userWhoJoined) { userExists = true; break; }
                }
                
                if (!userExists) {
                    fChannelUsers[chanNode]->AddItem(new UserListItem(userWhoJoined.String(), false));
                    
                    // Read status visibility suppression from the explicitly scoped node
                    bool hideStatusOnThisServer = contextServer->IsHideStatus();
                    
                    if (!hideStatusOnThisServer) {
                        LogToItemBuffer(chanNode, BString("--> ") << userWhoJoined << " has joined " << channelJoined << "\n");
                    }
                    
                    if (fActiveBufferItem == chanNode) {
                        RefreshUserListUI();
                    }
                }
            }
        }
        return;
    }

   
          // TOPIC: Real-time broadcast notification when a user modifies the room topic
        if (command == "TOPIC") {
            BString senderNick = prefix;
            int32 exclamIdx = senderNick.FindFirst("!");
            if (exclamIdx != B_ERROR) senderNick.Truncate(exclamIdx);

            // Extract the channel target from 'line'
            BString channelTarget = line;
            channelTarget.Trim();
            channelTarget.ReplaceAll(" ", "");

            ChannelTreeItem* chanNode = FindChannelNode(contextServer, channelTarget);
            if (chanNode != nullptr) {
                // Log the topic change alert cleanly inside the room's main chat log window layout frame
                BString topicNotice;
                topicNotice << "--- " << senderNick << " has changed the topic to: " << trailing << "\n";
                LogToItemBuffer(chanNode, topicNotice);

                // If the user is currently looking at this room layout tab, update the top bar instantly
                if (fActiveBufferItem == chanNode) {
                    fTopicView->SetText(trailing.String());
                    fTopicView->Invalidate();
                }
            }
            return;
        }
 
   
   
    

        // RPL_UNAWAY: Server confirms you are no longer marked away
        if (command == "305") {
            if (contextServer == nullptr) {
                contextServer = (fCurrentServerNode != nullptr) ? fCurrentServerNode : fLiberaNode;
            }
            
            BStringItem* serverLog = FindServerLogNode(contextServer);
            BString notice = "--- Server Notification: You are no longer marked as being away.\n";
            LogToItemBuffer(serverLog ? serverLog : fActiveBufferItem, notice);
            
            for (auto it = fChannelUsers.begin(); it != fChannelUsers.end(); ++it) {
                ChannelTreeItem* chanNode = dynamic_cast<ChannelTreeItem*>(it->first);
                if (chanNode != nullptr) {
                    UpdateUserAwayState(chanNode, fMyNick.String(), false); // false = active/bright
                }
            }
            
            return;
        }


		// RPL_NOWAWAY: Server confirms you are now successfully marked away
		if (command == "306") {
    		if (contextServer == nullptr) {
        		contextServer = (fCurrentServerNode != nullptr) ? fCurrentServerNode : fLiberaNode;
    		}
    
    		BStringItem* serverLog = FindServerLogNode(contextServer);
    		BString notice;
    
    		notice << "--- Server Notification: You are now marked as away (" << cfg.awayMessage.c_str() << ").\n";
    
    		LogToItemBuffer(serverLog ? serverLog : fActiveBufferItem, notice);
    
    		for (auto it = fChannelUsers.begin(); it != fChannelUsers.end(); ++it) {
        		ChannelTreeItem* chanNode = dynamic_cast<ChannelTreeItem*>(it->first);
        		if (chanNode != nullptr) {
            		UpdateUserAwayState(chanNode, fMyNick.String(), true);
        		}
    		}
    		return;
		}

        

         // ERR_NICKNAMEINUSE: Handshake registration collision router
        if (command == "433") {
            if (contextServer == nullptr) {
                contextServer = (fCurrentServerNode != nullptr) ? fCurrentServerNode : fLiberaNode;
            }
            
            // --- Safe map lookup to prevent accidental dirty null-key insertion ---
            BSecureSocket* activeSocket = nullptr;
            if (fServerSockets.count(contextServer) > 0) {
                activeSocket = fServerSockets[contextServer];
            } else {
                activeSocket = (contextServer == fOftcNode) ? fOftcSocket : fLiberaSocket;
            }

            if (activeSocket != nullptr) {
                // Keep network-isolated retry counts decoupled per server context instance
                fNickAttempts[contextServer]++;
                int32 attempt = fNickAttempts[contextServer];

                BString fallbackNick;
                if (attempt == 1) {
                    fallbackNick = contextServer->GetAltNick();
                } else if (attempt == 2) {
                    fallbackNick = contextServer->GetAltNick2();
                } else {
                    uint32 randomSeed = static_cast<uint32>(real_time_clock_usecs() & 0xFFFF);
                    fallbackNick.SetToFormat("%s%u", contextServer->GetNick().String(), (randomSeed % 90) + 10);
                }

                fallbackNick.ReplaceAll(" ", "");
                
                // --- Tentatively cache assigned name so your outgoing filters align ---
                // (This can be finalized later when the server replies with an official NICK confirmation acknowledgement)
                fMyNick = fallbackNick;

                BString nickCommand;
                nickCommand << "NICK " << fallbackNick << "\r\n";
                activeSocket->Write(nickCommand.String(), nickCommand.Length());

                BString itemNotice;
                itemNotice << "--- Nickname in use! Trying alternate identifier: " << fallbackNick << "\n";
                
                // Route message safely into the correct targeted network status server log node
                BStringItem* serverLogNode = FindServerLogNode(contextServer);
                if (serverLogNode != nullptr) {
                    LogToItemBuffer(serverLogNode, itemNotice);
                } else {
                    LogToItemBuffer(fActiveBufferItem, itemNotice);
                }
            }
            return;
        }


        // RPL_WHOREPLY: Processes bulk channel user attributes (Bypasses trailing chunk limitations)
        if (command == "352") {
            std::vector<BString> tokens;
            int32 currentPos = 0;
            int32 nextSpace;
            
            // Build the data array using the argument data populated inside 'line'
            while ((nextSpace = line.FindFirst(" ", currentPos)) != B_ERROR) {
                BString t;
                line.CopyInto(t, currentPos, nextSpace - currentPos);
                if (t.Length() > 0) tokens.push_back(t);
                currentPos = nextSpace + 1;
            }
            if (currentPos < line.Length()) {
                BString t;
                line.CopyInto(t, currentPos, line.Length() - currentPos);
                if (t.Length() > 0) tokens.push_back(t);
            }

            // Ensure we have a complete 352 standard argument footprint (Indices 0 through 6)
            if (tokens.size() >= 7) {
                BString channelTarget  = tokens[1]; // Index 1: #ablyss-test
                BString targetUserNick = tokens[5]; // Index 5: ablyss_
                BString flags          = tokens[6]; // Index 6: G or H

                channelTarget.Trim();
                targetUserNick.Trim();
                flags.Trim();

                ChannelTreeItem* chanNode = FindChannelNode(contextServer, channelTarget);
                if (chanNode != nullptr) {
                    // Check if the flags token contains 'G' for Gone / Away
                    bool isAway = (flags.FindFirst("G") != B_ERROR);
                    
                    // Trigger our updated, prefix-safe away status setter function
                    UpdateUserAwayState(chanNode, targetUserNick.String(), isAway);
                }
            }
            return;
        }






        // RPL_AWAY: Triggered when trying to ping or query an away target user
        if (command == "301") {
            // 1. Declare and build the tokens vector explicitly inside this scope
            std::vector<BString> tokens;
            int32 currentPos = 0;
            int32 nextSpace;
            
            while ((nextSpace = trailing.FindFirst(" ", currentPos)) != B_ERROR) {
                BString t;
                trailing.CopyInto(t, currentPos, nextSpace - currentPos);
                if (t.Length() > 0) tokens.push_back(t);
                currentPos = nextSpace + 1;
            }
            if (currentPos < trailing.Length()) {
                BString t;
                trailing.CopyInto(t, currentPos, trailing.Length() - currentPos);
                if (t.Length() > 0) tokens.push_back(t);
            }

            // 2. Safely verify that we found our tokens
            if (tokens.size() >= 2) {
                BString awayUserNick = tokens[1]; // Index 0 is your nick, 1 is their nick

                // 3. Step through your channels to apply the faded visual state safely
                for (auto it = fChannelUsers.begin(); it != fChannelUsers.end(); ++it) {
                    ChannelTreeItem* chanNode = dynamic_cast<ChannelTreeItem*>(it->first);
                    if (chanNode != nullptr) {
                        UpdateUserAwayState(chanNode, awayUserNick.String(), true);
                    }
                }
            }
            return;
        }




        // RPL_WELCOME: Network handshake authentication completed successfully
        if (command == "001") {
            if (contextServer == nullptr) {
                contextServer = (fCurrentServerNode != nullptr) ? fCurrentServerNode : fLiberaNode;
            }
            
            // 1. Reset nickname collision attempt metrics for this server context instance
            fNickAttempts[contextServer] = 0;

            // 2. --- Parse the official nickname confirmed by the remote server ---
            // In IRC protocol specifications, the first parameter after the '001' command 
            // is always the definitive nick assigned to your socket connection.
            BString officialNick = "";
            int32 firstParamSpace = line.FindFirst(" ");
            if (firstParamSpace != B_ERROR) {
                // If a prefix or target address shifts layout offsets, grab token safely
                BString remainingParams = line;
                remainingParams.Remove(0, firstParamSpace + 1);
                int32 nextParamSpace = remainingParams.FindFirst(" ");
                if (nextParamSpace != B_ERROR) {
                    remainingParams.CopyInto(officialNick, 0, nextParamSpace);
                } else {
                    officialNick = remainingParams;
                }
                officialNick.Trim();
                
                if (officialNick.Length() > 0 && officialNick != "*") {
                    fMyNick = officialNick; // Finalize local identity sync tracking
                    if (cfg.debugEnable) {
                        printf("[DEBUG] 001 Welcome confirmation: Finalized local nick as '%s'\n", fMyNick.String());
                    }
                }
            }

            // 3. FETCH THE ACTIVE NETWORK PIPELINE STREAM SOCKET SAFELY
            BSecureSocket* activeSocket = nullptr;
            if (fServerSockets.count(contextServer) > 0) {
                activeSocket = fServerSockets[contextServer];
            } else {
                activeSocket = (contextServer == fOftcNode) ? fOftcSocket : fLiberaSocket;
            }

            // Automatically loop and send JOIN commands ONLY after the server welcomes us!
            if (activeSocket != nullptr) {
                std::vector<std::string> targetAutoJoin;
                bool foundServerInConfig = false;

                // --- Map autojoin lists by Server Name/Address matching rather than volatile indices ---
                BString contextServerAddr = contextServer->Text(); // Assuming .Text() returns address or we use a getter
                
                // Determine whether this is a custom or standard server to pull the correct vector array
                if (contextServer->IsCustom()) {
                    // --- Rely on safe vector index boundary verification ---
                    size_t srvIdx = contextServer->GetIndex();
                    if (srvIdx < cfg.customServers.size()) {
                        targetAutoJoin = cfg.customServers[srvIdx].autojoin;
                        foundServerInConfig = true;
                    }
                } else {
                    size_t srvIdx = contextServer->GetIndex();
                    if (srvIdx < cfg.servers.size()) {
                        targetAutoJoin = cfg.servers[srvIdx].autojoin;
                        foundServerInConfig = true;
                    }
                }


                // Process the autojoin channels safely after welcome validation
                if (foundServerInConfig) {
                    for (const auto& chan : targetAutoJoin) {
                        if (chan.empty()) continue;
                        
                        BString joinCommand;
                        joinCommand << "JOIN " << chan.c_str() << "\r\n";
                        activeSocket->Write(joinCommand.String(), joinCommand.Length());
                        
                        if (cfg.debugEnable) {
                            LogDebugStream(contextServer->Text(), "OUTGOING", joinCommand.String(), joinCommand.Length());
                        }
                    }
                }
            }
            
            // Route structural confirmation status messages down into the correct network view log node
            BStringItem* serverLog = FindServerLogNode(contextServer);
            if (serverLog != nullptr) {
                LogToItemBuffer(serverLog, "--- Connection fully established with network.\n");
            } else {
                LogToItemBuffer(fActiveBufferItem, "--- Connection fully established with network.\n");
            }
        }





	        // RPL_LIST: Individual channel description entry token row
        if (command == "322") { 
            // Server data line parameter syntax: <YourNick> <#Channel> <UserCount> :[Topic Text]
            // NOTE: Because our main loop already extracted 'trailing' (Topic), 
            // the 'line' variable here contains exactly: "YourNick #channel UserCount"
            
            BString channelName = "";
            BString userCount = "";
            
            int32 firstSpace = line.FindFirst(" ");
            if (firstSpace != B_ERROR) {
                // Extract parameters reliably using a temporary copy loop
                BString argsBlock = line;
                argsBlock.Remove(0, firstSpace + 1); // Strip your own nickname out safely
                
                int32 secondSpace = argsBlock.FindFirst(" ");
                if (secondSpace != B_ERROR) {
                    argsBlock.CopyInto(channelName, 0, secondSpace);
                    
                    BString countBlock = argsBlock;
                    countBlock.Remove(0, secondSpace + 1);
                    int32 thirdSpace = countBlock.FindFirst(" ");
                    if (thirdSpace != B_ERROR) {
                        countBlock.CopyInto(userCount, 0, thirdSpace);
                    } else {
                        userCount = countBlock;
                    }
                }
            }
            
            channelName.Trim();
            userCount.Trim();

            // Safety check: skip broken packets or invalid parsing attempts
            if (channelName.Length() == 0) return;

            if (contextServer == nullptr) {
                contextServer = (fCurrentServerNode != nullptr) ? fCurrentServerNode : fLiberaNode;
            }

            // --- Safe multi-threaded pointer validation via BMessenger ---
            if (fActiveListWindow != nullptr) {
                BMessenger messenger(fActiveListWindow);
                if (messenger.IsValid()) {
                    
                    // Fetch accurate active network stream connection out of lookup map safely
                    BSecureSocket* activeNetworkSocket = nullptr;
                    if (fServerSockets.count(contextServer) > 0) {
                        activeNetworkSocket = fServerSockets[contextServer];
                    } else {
                        activeNetworkSocket = (contextServer == fOftcNode) ? fOftcSocket : fLiberaSocket;
                    }

                    bool targetMatches = false;
                    
                    // --- Lock window thread loop safely before invoking class methods ---
                    if (fActiveListWindow->Lock()) {
                        // Securely cross-validate using the verified public target socket getter
                        if (fActiveListWindow->GetTargetSocket() == activeNetworkSocket) {
                            targetMatches = true;
                        }
                        fActiveListWindow->Unlock();
                    }

                    // Only forward data packages if the window targets this specific server stream
                    if (targetMatches) {
                        BMessage* rowPackage = new BMessage(MSG_ADD_LIST_ROW);
                        rowPackage->AddString("channel", channelName.String());
                        rowPackage->AddString("users", userCount.String());
                        rowPackage->AddString("topic", trailing.String()); // 'trailing' holds parsed topic
                        
                        // Fire-and-forget messaging delivers data asynchronously without freezing threads
                        fActiveListWindow->PostMessage(rowPackage);
                    }
                } else {
                    fActiveListWindow = nullptr; // Reset pointer if closed by user
                }
            }
            return;
        }



            // Live PART & QUIT Handlers (Safely removes users dynamically when they depart or disconnect)
        if (command == "PART" || command == "QUIT") {
            BString userWhoLeft = prefix;
            int32 exclamIdx = userWhoLeft.FindFirst("!");
            if (exclamIdx != B_ERROR) userWhoLeft.Truncate(exclamIdx);

            BString targetChannel = line;
            targetChannel.Trim();
            targetChannel.ReplaceAll(" ", "");

            // Ensure we have a valid server context pointer passed to this engine block
            if (contextServer == nullptr) {
                contextServer = (fCurrentServerNode != nullptr) ? fCurrentServerNode : fLiberaNode;
            }

            // Safe Haiku BOutlineListView Sub-Item Traversal Loop
            int32 totalTreeItems = fChannelTree->CountItems();
            for (int32 c = 0; c < totalTreeItems; c++) {
                BListItem* baseItem = fChannelTree->ItemAt(c);
                if (baseItem == nullptr) continue;

                // Ensure this item is nested directly under our targeted server connection node
                if (fChannelTree->Superitem(baseItem) != contextServer) continue;

                ChannelTreeItem* chanNode = dynamic_cast<ChannelTreeItem*>(baseItem);
                if (!chanNode) continue;
                
                // If it's a room-specific PART, skip loops on non-matching channel strings
                if (command == "PART" && BString(chanNode->Text()) != targetChannel) continue;

                // If it's a global QUIT, the user leaves *all* channels belonging to this server!
                if (fChannelUsers[chanNode] != nullptr) {
                    BObjectList<UserListItem, true>* userVector = fChannelUsers[chanNode];
                    
                    // Iterate backward to prevent index shifting bugs when items are deleted
                    for (int32 i = userVector->CountItems() - 1; i >= 0; i--) {
                        BStringItem* userItem = userVector->ItemAt(i);
                        if (userItem == nullptr) continue;

                        BString itemTxt = userItem->Text();
                        
                        // Explicitly clean up the channel rank prefix before checking against the user string
                        if (itemTxt.StartsWith("@") || itemTxt.StartsWith("+")) itemTxt.Remove(0, 1);
                        
                        if (itemTxt == userWhoLeft) {
                            // Safely remove from the vector array and free heap memory pool allocations entirely
                            BStringItem* removedUser = userVector->RemoveItemAt(i);
                            delete removedUser;
                            
                            // Query the specific, thread-safe server context node for configuration properties
                            bool hideStatusOnThisServer = contextServer->IsHideStatus();
                            
                            if (!hideStatusOnThisServer) {
                                BString partNotice = (command == "PART") ? "has left" : "has quit";
                                BString logLine;
                                
                                // --- Replaced unsafe SetToFormat string operators with bulletproof appends ---
                                logLine << "<-- " << userWhoLeft << " " << partNotice;
                                if (trailing.Length() > 0) {
                                    logLine << " (" << trailing << ")\n";
                                } else {
                                    logLine << "\n";
                                }
                                
                                LogToItemBuffer(chanNode, logLine);
                            }
                            
                            // Immediately sync visual modules if the user departed the visible view frame
                            if (fActiveBufferItem == chanNode) {
                                RefreshUserListUI();
                            }
                            
                            // If it was a PART command, the user can only belong to one channel; break safely.
                            if (command == "PART") break;
                        }
                    }
                }
            }
            return;
        }


           // Catch Server Topic Event Numbers (332 = Topic Set, 331 = No Topic Set) or live /TOPIC updates
        if (command == "332" || command == "331" || command == "TOPIC") {
            BString targetChannel = "";
            
            if (command == "332" || command == "331") {
                BString numericArgs = line;
                int32 chSpace = numericArgs.FindFirst(" ");
                if (chSpace != B_ERROR) numericArgs.Remove(0, chSpace + 1);
                int32 nextSpace = numericArgs.FindFirst(" ");
                if (nextSpace != B_ERROR) numericArgs.Truncate(nextSpace);
                targetChannel = numericArgs;
            } else {
                // Live /TOPIC command looks like: <#Channel>
                BString liveArgs = line;
                int32 cmdSpace = liveArgs.FindFirst(" ");
                if (cmdSpace != B_ERROR) liveArgs.Truncate(cmdSpace);
                targetChannel = liveArgs;
            }
            
            targetChannel.Trim();
            targetChannel.ReplaceAll(" ", "");

            if (contextServer == nullptr) {
                contextServer = (fCurrentServerNode != nullptr) ? fCurrentServerNode : fLiberaNode;
            }

            ChannelTreeItem* chanNode = FindChannelNode(contextServer, targetChannel);
            if (chanNode != nullptr) {
                BString dynamicTopic = (command == "331") ? "No topic set." : trailing;
                
                dynamicTopic.Trim();
                dynamicTopic.ReplaceAll("\r", "");
                dynamicTopic.ReplaceAll("\n", "");
                
                // === HISTORY BACKLOG PLAYBACK PROTECTION BARRIER ===
                // If we are currently replaying old lines from our log history text files,
                // do NOT allow it to overwrite the live topic string currently active in memory!
                if (fIsLoadingHistory && fChannelTopics.find(chanNode) != fChannelTopics.end()) {
                    // Skip the memory overwrite pass entirely to protect our live topic!
                } else {
                    // Cache the clean live text payload into our memory map securely
                    fChannelTopics[chanNode] = dynamicTopic;
                    
                    // Force immediate live layout synchronization to the top bar
                    if (fActiveBufferItem == chanNode || (fActiveBufferItem != nullptr && fActiveBufferItem->Text() == targetChannel)) {
                        fTopicView->SetText(dynamicTopic.String());
                        fTopicView->InvalidateLayout();
                        fTopicView->Invalidate();
                    }
                }
                
                if (command == "332") {
                    LogToItemBuffer(chanNode, BString("--- Channel topic: ") << dynamicTopic << "\n");
                } else if (command == "TOPIC") {
                    BString changingUser = prefix;
                    int32 exclamIdx = changingUser.FindFirst("!");
                    if (exclamIdx != B_ERROR) changingUser.Truncate(exclamIdx);
                    
                    LogToItemBuffer(chanNode, BString("--- ") << changingUser << " has changed the topic to: " << dynamicTopic << "\n");
                } else {
                    LogToItemBuffer(chanNode, "--- No channel topic is set.\n");
                }
            }
            return;
        }




        // RPL_ENDOFWHO: Server indicates the complete bulk user data burst is finished
        if (command == "315") {
            // 'line' holds: "<YourNick> <#Channel>"
            std::vector<BString> tokens;
            int32 currentPos = 0;
            int32 nextSpace;
            
            while ((nextSpace = line.FindFirst(" ", currentPos)) != B_ERROR) {
                BString t;
                line.CopyInto(t, currentPos, nextSpace - currentPos);
                if (t.Length() > 0) tokens.push_back(t);
                currentPos = nextSpace + 1;
            }
            if (currentPos < line.Length()) {
                BString t;
                line.CopyInto(t, currentPos, line.Length() - currentPos);
                if (t.Length() > 0) tokens.push_back(t);
            }

            if (tokens.size() >= 2) {
                BString channelTarget = tokens[1]; // Index 1 isolates the room name
                channelTarget.Trim();

                ChannelTreeItem* chanNode = FindChannelNode(contextServer, channelTarget);
                
                
                // BATCH EXECUTION: If this is the active tab frame, refresh the screen just ONCE!
                if (chanNode != nullptr && fActiveBufferItem == chanNode) {
                    RefreshUserListUI();
                }
                
            }
            return;
        }




           // RPL_NAMREPLY: Active channel user list payload burst
        if (command == "353") { 
            // Raw argument string syntax before the colon separator looks like:
            // "<YourNick> <Type: = or * or @> <#Channel>"
            // Since our main loop stripped prefixes, 'line' holds exactly this block.
            
            BString targetChannel = "";
            int32 lastSpace = line.FindLast(" ");
            if (lastSpace != B_ERROR) {
                line.CopyInto(targetChannel, lastSpace + 1, line.Length() - (lastSpace + 1));
            } else {
                targetChannel = line;
            }
            targetChannel.Trim();
            targetChannel.ReplaceAll(" ", "");

            if (targetChannel.Length() == 0) return;

            // Ensure we have a valid server context pointer passed to this engine block
            if (contextServer == nullptr) {
                contextServer = (fCurrentServerNode != nullptr) ? fCurrentServerNode : fLiberaNode;
            }

            // Explicitly cast to ChannelTreeItem to access specialized rank properties if present
            ChannelTreeItem* chanNode = FindChannelNode(contextServer, targetChannel);
            if (chanNode != nullptr && fChannelUsers[chanNode] != nullptr) {
                BObjectList<UserListItem, true>* userVector = fChannelUsers[chanNode];
                
                // --- Isolate tokenization using a safe character loop to stop infinite freezes ---
                BString namesBuffer = trailing;
                namesBuffer.Trim(); // Strip trailing/leading space configurations
                
                int32 searchStart = 0;
                while (searchStart < namesBuffer.Length()) {
                    int32 nextSpace = namesBuffer.FindFirst(" ", searchStart);
                    BString singleUser;
                    
                    if (nextSpace != B_ERROR) {
                        namesBuffer.CopyInto(singleUser, searchStart, nextSpace - searchStart);
                        searchStart = nextSpace + 1;
                    } else {
                        namesBuffer.CopyInto(singleUser, searchStart, namesBuffer.Length() - searchStart);
                        searchStart = namesBuffer.Length(); // Terminates the loop cleanly
                    }
                    
                    singleUser.Trim();
                    if (singleUser.Length() == 0) continue;

                    // --- Duplication Guard Pass ---
                    // Verify the name handle doesn't already occupy a slot in this channel vector array
                    bool duplicateFound = false;
                    for (int32 u = 0; u < userVector->CountItems(); u++) {
                        if (userVector->ItemAt(u) != nullptr && 
                            userVector->ItemAt(u)->Text() == singleUser) {
                            duplicateFound = true;
                            break;
                        }
                    }
						if (!duplicateFound) {
    						userVector->AddItem(new UserListItem(singleUser.String(), false));
						}

                }

                // If the user is actively viewing this tab frame, sync visual modules instantly
                if (fActiveBufferItem == chanNode) {
                    RefreshUserListUI();
                    
                    fUserList->SortItems(SortUsersByRank);
                    fUserList->Invalidate();
                }
            }
            return;
        }



         // Dynamic NICK Modification Handlers (Saves identity renames for Us and other network users)
        if (command == "NICK") {
            BString oldNick = prefix;
            int32 exclamIdx = oldNick.FindFirst("!");
            if (exclamIdx != B_ERROR) oldNick.Truncate(exclamIdx);

            BString newNick = trailing.Length() > 0 ? trailing : line;
            newNick.Trim();
            newNick.ReplaceAll(" ", "");

            if (newNick.Length() == 0) return;

            // Ensure we have a valid server context pointer passed to this engine block
            if (contextServer == nullptr) {
                contextServer = (fCurrentServerNode != nullptr) ? fCurrentServerNode : fLiberaNode;
            }

            // Check nickname against the specific server's runtime tracker
            if (oldNick.ICompare(contextServer->GetNick()) == 0) {
                // Safely update memory vector state without rewriting your settings file ---
                // This updates your client's active tracking loop inline while preserving your raw files!
                size_t serverIdx = contextServer->GetIndex();
                if (contextServer->IsCustom() && serverIdx < cfg.customServers.size()) {
                    cfg.customServers[serverIdx].nick = newNick.String();
                } else if (!contextServer->IsCustom() && serverIdx < cfg.servers.size()) {
                    cfg.servers[serverIdx].nick = newNick.String();
                }
                
                BString itemNotice;
                itemNotice << "--- Your nickname on this server is now " << newNick << "\n";
                LogToItemBuffer(fActiveBufferItem, itemNotice);
                
                // Keep fMyNick sync'd for backwards compatibility if needed elsewhere
                fMyNick = newNick;
            }


            // Safe Haiku BOutlineListView Sub-Item Traversal Loop ---
            // Iterate flatly across all items and cross-validate parental association context
            int32 totalTreeItems = fChannelTree->CountItems();
            for (int32 c = 0; c < totalTreeItems; c++) {
                BListItem* baseItem = fChannelTree->ItemAt(c);
                if (baseItem == nullptr) continue;

                // Ensure this item is nested strictly under our targeted server connection node
                if (fChannelTree->Superitem(baseItem) != contextServer) continue;

                ChannelTreeItem* chanNode = dynamic_cast<ChannelTreeItem*>(baseItem);
                if (!chanNode) continue;
                
                if (fChannelUsers[chanNode] != nullptr) {
                    BObjectList<UserListItem, true>* userVector = fChannelUsers[chanNode];
                    
                    // Slicing from top to bottom is safe here because we preserve vector sizes via in-place updates
                    for (int32 i = 0; i < userVector->CountItems(); i++) {
                        BStringItem* userItem = userVector->ItemAt(i);
                        if (userItem == nullptr) continue;

                        BString currentEntry = userItem->Text();
                        BString modePrefix = "";
                        
                        if (currentEntry.StartsWith("@") || currentEntry.StartsWith("+")) {
                            currentEntry.CopyInto(modePrefix, 0, 1);
                            currentEntry.Remove(0, 1);
                        }

                        if (currentEntry == oldNick) {
                            BString updatedEntry;
                            updatedEntry << modePrefix << newNick;
                            
                            // Free old memory pool allocations and re-insert the updated string element
                            BStringItem* oldItem = userVector->RemoveItemAt(i);
                            delete oldItem;
                            
                            userVector->AddItem(new UserListItem(updatedEntry.String(), false), i);                      
                            
                            BString nickNotice;
                            nickNotice << "--- " << oldNick << " is now known as " << newNick << "\n";
                            LogToItemBuffer(chanNode, nickNotice);
                            
                            // Force the UI to refresh live if the nickname changed in the active channel view
                            if (fActiveBufferItem == chanNode) {
                                RefreshUserListUI();
                                fUserList->SortItems(SortUsersByRank);
                                fUserList->Invalidate();
                            }
                            break; // This user can only appear once per channel vector array
                        }
                    }
                }
            }
            return;
        }




        // PRIVMSG & NOTICE Message Routing Engine
        if (command == "PRIVMSG" || command == "NOTICE") {
            BString senderNick = prefix;
            int32 exclamIdx = senderNick.FindFirst("!");
            if (exclamIdx != B_ERROR) senderNick.Truncate(exclamIdx);            
            
            int32 msgTargetSpace = line.FindFirst(" ");
            BString targetRoom = line;
            if (msgTargetSpace != B_ERROR) targetRoom.Truncate(msgTargetSpace);
            targetRoom.ReplaceAll(" ", "");

            // Handle Automated CTCP Version Queries
            if (command == "PRIVMSG" && (trailing.StartsWith("\x01VERSION\x01") || trailing.StartsWith("\1VERSION\1"))) {
                BSecureSocket* activeSocket = nullptr;
                if (contextServer != nullptr && fServerSockets.count(contextServer) > 0) {
                    activeSocket = fServerSockets[contextServer];
                } else {
                    activeSocket = (contextServer == fOftcNode) ? fOftcSocket : fLiberaSocket;
                }

                if (activeSocket != nullptr) {
                    BString versionReply;
                    versionReply << "NOTICE " << senderNick << " :\x01VERSION " << AppInfo::VERSION_STRING << "\x01\r\n";                    
                    activeSocket->Write(versionReply.String(), versionReply.Length());
                    
                    BString logNotice;
                    logNotice << "--- [CTCP] Version query from " << senderNick << " answered automatically.\n";
                    LogToItemBuffer(FindServerLogNode(contextServer), logNotice);
                }
                return;
            }


            if (contextServer == nullptr) {
                contextServer = (fCurrentServerNode != nullptr) ? fCurrentServerNode : fLiberaNode;
            }

            // 1. Establish destination nodes supporting Channels AND Private Message Queries ---
            BStringItem* targetNode = nullptr;
            if (targetRoom.StartsWith("#")) {
                targetNode = FindChannelNode(contextServer, targetRoom);
            } else {
                // Look for an existing one-on-one query tab
                targetNode = FindChannelNode(contextServer, senderNick);
                
                // === AUTOMATION HOOK: Auto-create tab if someone pings us first ===
                if (targetNode == nullptr) {
                    ChannelTreeItem* newQueryNode = new ChannelTreeItem(senderNick.String(), 
                                                                        contextServer->GetIndex(), 
                                                                        contextServer->IsCustom());
                    
                    fChannelTree->AddUnder(newQueryNode, contextServer);
                    fChannelTree->Expand(contextServer);
                    
                    // Allocate an empty list container for their user panel cache matrix
                    fChannelUsers[newQueryNode] = new BObjectList<UserListItem, true>(20);
                    
                    // Log the opening initialization notice header
                    LogToItemBuffer(newQueryNode, BString("--- Incoming Private Conversation started with ") << senderNick << "\n");
                    
                    targetNode = newQueryNode;
                }
            }


            if (targetNode == nullptr) {
                targetNode = FindServerLogNode(contextServer);
            }

            // 2. Thread-Safe Timing Calculation Engine ---
            BString timestampPrefix = "";
            bigtime_t currentTime = real_time_clock_usecs(); // Native Haiku system clock            
            bigtime_t thirtyMinutesInUsecs = (bigtime_t)30 * 60 * 1000000;
            
            if (targetNode != nullptr) {
                bool needsTimestamp = false;
                if (fLastTimestampTime.count(targetNode) == 0) {
                    needsTimestamp = true;
                } else {
                    bigtime_t lastTime = fLastTimestampTime.find(targetNode)->second;
                    if ((currentTime - lastTime) >= thirtyMinutesInUsecs) {
                        needsTimestamp = true;
                    }
                }

                if (needsTimestamp) {                    
                    fLastTimestampTime[targetNode] = currentTime;
                    time_t rawTime = (time_t)(currentTime / 1000000);
                    struct tm* timeInfo = localtime(&rawTime);                    
                    if (timeInfo != nullptr) {
                        char timeBuffer[32];
                        strftime(timeBuffer, sizeof(timeBuffer), "[%H:%M] ", timeInfo);
                        timestampPrefix = timeBuffer;
                    }
                }
            }

            // 3. Parse and Format Message Strings
            BString formattedMsg;
            formattedMsg << timestampPrefix;
            
            if (command == "PRIVMSG" && (trailing.StartsWith("\x01" "ACTION ") || trailing.StartsWith("\1ACTION "))) {                        
                trailing.Remove(0, 8);                
                if (trailing.EndsWith("\x01") || trailing.EndsWith("\1")) {
                    trailing.Truncate(trailing.Length() - 1);
                }                
                formattedMsg << "* " << senderNick << " " << trailing << "\n";
            } else {
                formattedMsg << "<" << senderNick << "> " << trailing << "\n";
            }

            // 4. AUTOMATION HOOK: Detect NickServ post-auth identification triggers
            if (senderNick.ICompare("NickServ") == 0 && trailing.IFindFirst("identify") != B_ERROR) {                
                BSecureSocket* activeSocket = nullptr;
                if (contextServer != nullptr && fServerSockets.count(contextServer) > 0) {
                    activeSocket = fServerSockets[contextServer];
                } else {
                    activeSocket = (contextServer == fOftcNode) ? fOftcSocket : fLiberaSocket;
                }                
                if (activeSocket != nullptr && contextServer != nullptr) {
                    BString savedPassword = contextServer->GetPass();
                    if (savedPassword.Length() > 0) {
                        BString autoIdentify;
                        autoIdentify << "PRIVMSG NickServ :IDENTIFY " << savedPassword << "\r\n";
                        activeSocket->Write(autoIdentify.String(), autoIdentify.Length());
                        
                        BString logNotice = "--- [Auto-Services] Identification credentials automatically sent to NickServ.\n";
                        LogToItemBuffer(FindServerLogNode(contextServer), logNotice);
                    }
                }
            }

            // 5. Route structured messages to their designated text buffers
            if (targetRoom.StartsWith("#")) {
                ChannelTreeItem* chanNode = dynamic_cast<ChannelTreeItem*>(targetNode);
                if (chanNode) {
                    
                    // === AUTOMATION HOOK: Mark user as back upon active typing ===
                    // If they are speaking in a channel, they are active. Override away state to false.
                    UpdateUserAwayState(chanNode, senderNick.String(), false);
                    // =============================================================

                    if (fActiveBufferItem != chanNode) {
                        chanNode->SetUnread(true);
                        fChannelTree->InvalidateItem(fChannelTree->IndexOf(chanNode)); 
                    }
                    LogToItemBuffer(chanNode, formattedMsg);
                } else {
                    LogToItemBuffer(FindServerLogNode(contextServer), formattedMsg);
                }
            } else {
                // Route Private Query PM messages safely to their own tree nodes if available
                LogToItemBuffer(targetNode, formattedMsg);
            }
            return;
        }
     
        
        // Bracket-Sanitized Global Server Protocol Fallback Route ---
        // This catches generic server data lines (MOTD, Notices) outside of channels
        if (trailing.Length() > 0) {
            if (contextServer == nullptr) {
                contextServer = (fCurrentServerNode != nullptr) ? fCurrentServerNode : fLiberaNode;
            }

            // AUTOMATION HOOK FALLBACK: Intercept raw notices before registration fully establishes
            if (prefix.IFindFirst("NickServ") != B_ERROR && trailing.IFindFirst("identify") != B_ERROR) {                
                BSecureSocket* activeSocket = nullptr;
                if (contextServer != nullptr && fServerSockets.count(contextServer) > 0) {
                    activeSocket = fServerSockets[contextServer];
                } else {
                    activeSocket = (contextServer == fOftcNode) ? fOftcSocket : fLiberaSocket;
                }
                if (activeSocket != nullptr && contextServer != nullptr && contextServer->GetPass().Length() > 0) {
                    BString autoIdentify;
                    autoIdentify << "PRIVMSG NickServ :IDENTIFY " << contextServer->GetPass() << "\r\n";
                    activeSocket->Write(autoIdentify.String(), autoIdentify.Length());
                    
                    BString logNotice = "--- [Auto-Services] Pre-registration identification sent to NickServ.\n";
                    LogToItemBuffer(static_cast<BStringItem*>(contextServer), logNotice);
                }
            }

            if (contextServer != nullptr) {
                BString rawLog;
                rawLog << trailing << "\n";
                LogToItemBuffer(static_cast<BStringItem*>(contextServer), rawLog);
            }
        }
    }



    static status_t NetworkLoop(void* data) {
        ServerTreeItem* targetNode = static_cast<ServerTreeItem*>(data);
        if (targetNode == nullptr) return B_ERROR;

        CricketWindow* window = dynamic_cast<CricketWindow*>(be_app->WindowAt(0));
        if (window == nullptr) return B_ERROR;

        BNetworkAddress address(targetNode->GetHost().String(), targetNode->GetPort());
        BSecureSocket* localSocket = new BSecureSocket();
        
        if (localSocket->Connect(address) != B_OK) {
            BMessage* reply = new BMessage(MSG_IRC_RECEIVED);
            reply->AddString("text", BString("Connection failed to ") << targetNode->GetHost() << "\n");
            reply->AddPointer("server_node", targetNode);
            window->PostMessage(reply);
            delete localSocket;
            return B_ERROR;
        }

        // Dynamically track socket connection instances using our tracker map
        window->Lock(); // Ensure thread-safe map insertion
        window->fServerSockets[targetNode] = localSocket;
        
        // Map legacy pointers to prevent breakages elsewhere 
        if (targetNode == window->fLiberaNode) window->fLiberaSocket = localSocket;
        if (targetNode == window->fOftcNode)   window->fOftcSocket = localSocket;
        window->Unlock();

        // 1. PASS MUST BE SENT FIRST PER IRC SPECIFICATION
        if (targetNode->GetPass().Length() > 0) {
            BString passHandshake;
            passHandshake << "PASS " << targetNode->GetPass() << "\r\n";
            localSocket->Write(passHandshake.String(), passHandshake.Length());
            if (cfg.debugEnable) {
                LogDebugStream(targetNode->Text(), "OUTGOING", passHandshake.String(), passHandshake.Length());
            }
        }

        // 2. Dynamic Nickname Handshake Configuration
        BString nickHandshake;
        nickHandshake << "NICK " << targetNode->GetNick() << "\r\n";
        localSocket->Write(nickHandshake.String(), nickHandshake.Length());
        if (cfg.debugEnable) {
            LogDebugStream(targetNode->Text(), "OUTGOING", nickHandshake.String(), nickHandshake.Length());
        }
		
        // 3. Dynamic User Identity Handshake Configuration
        BString userHandshake;
        userHandshake << "USER haiku 0 * :" << targetNode->GetNick() << " Client\r\n";
        localSocket->Write(userHandshake.String(), userHandshake.Length());
        if (cfg.debugEnable) {
            LogDebugStream(targetNode->Text(), "OUTGOING", userHandshake.String(), userHandshake.Length());
        }

        char buffer[512];
        ssize_t bytesRead;
        BString lineBuffer;
        
        while ((bytesRead = localSocket->Read(buffer, sizeof(buffer) - 1)) > 0) {
            buffer[bytesRead] = '\0';           

            if (cfg.debugEnable) {
                LogDebugStream(targetNode->Text(), "INCOMING", buffer, bytesRead);
            }

            lineBuffer << buffer;

            int32 newlinePos;
            while ((newlinePos = lineBuffer.FindFirst("\n")) != B_ERROR) {
                BString line;
                lineBuffer.CopyInto(line, 0, newlinePos + 1);
                lineBuffer.Remove(0, newlinePos + 1);
                
                if (line.StartsWith("PING")) {
                    BString pong = line;
                    pong.Replace("PING", "PONG", 1);
                    localSocket->Write(pong.String(), pong.Length());
                    if (cfg.debugEnable) {
                        LogDebugStream(targetNode->Text(), "OUTGOING", pong.String(), pong.Length());
                    }
                    continue;
                }

                BMessage* reply = new BMessage(MSG_IRC_RECEIVED);
                reply->AddString("text", line.String());
                reply->AddPointer("server_node", targetNode);
                window->PostMessage(reply);
            }
        }

        // 4. Thread and Socket Cleanup State Reset
        bool triggerReconnect = targetNode->IsAutoReconnect();
        
        // Before accessing the window maps, ensure the window is still fully valid and active
        if (be_app->CountWindows() > 0 && be_app->WindowAt(0) == window) {
            if (window->Lock()) { // Double check thread safety lock boundaries
                window->fServerSockets.erase(targetNode);
                window->fServerThreads.erase(targetNode);
                
                if (targetNode == window->fLiberaNode) {
                    window->fLiberaThread = -1;
                    window->fLiberaSocket = nullptr;
                } else if (targetNode == window->fOftcNode) {
                    window->fOftcThread = -1;
                    window->fOftcSocket = nullptr;
                }
                window->Unlock();
                
                // Only post a reconnection message if the parent workspace isn't shutting down
                if (triggerReconnect) {
                    BMessage* reconnectMessage = new BMessage(MSG_RECONNECT_SERVER);
                    reconnectMessage->AddPointer("server_item", targetNode);
                    window->PostMessage(reconnectMessage);
                }
            }
        }

        delete localSocket;
        return B_OK;
    }




public:
    void DispatchMessage(BMessage* message, BHandler* handler) override {
        // 1. LEFT-CLICK INTERCEPTOR: Catch clicks on WebPositive URL links inside fChatLog
        if (message->what == B_MOUSE_DOWN && handler == fChatLog) {
            int32 buttons;
            BPoint point;
            
            if (message->FindInt32("buttons", &buttons) == B_OK && buttons == B_PRIMARY_MOUSE_BUTTON) {
                message->FindPoint("be:view_where", &point);
                
                int32 textOffset = fChatLog->OffsetAt(point);
                if (textOffset >= 0) {
                    BString fullChatHistory(fChatLog->Text());
                    
                    int32 urlStart = fullChatHistory.IFindLast("http://", textOffset);
                    if (urlStart == B_ERROR) {
                        urlStart = fullChatHistory.IFindLast("https://", textOffset);
                    }
                    
                    if (urlStart != B_ERROR) {
                        int32 urlEnd = fullChatHistory.FindFirst(" ", urlStart);
                        int32 newlineCheck = fullChatHistory.FindFirst("\n", urlStart);
                        
                        if (urlEnd == B_ERROR || (newlineCheck != B_ERROR && newlineCheck < urlEnd)) {
                            urlEnd = newlineCheck;
                        }
                        if (urlEnd == B_ERROR) {
                            urlEnd = fullChatHistory.Length();
                        }

                        if (textOffset >= urlStart && textOffset < urlEnd) {
                            BString targetURL;
                            fullChatHistory.CopyInto(targetURL, urlStart, urlEnd - urlStart);
                            targetURL.ReplaceAll("\r", "");
                            targetURL.ReplaceAll("\n", "");
                            targetURL.Trim();

                            if (targetURL.Length() > 0) {
                                const char* args[] = { targetURL.String(), nullptr };
                                
                                if (be_roster->Launch("application/x-vnd.WebPositive", 1, const_cast<char**>(args)) != B_OK) {
                                    be_roster->Launch("text/html", 1, const_cast<char**>(args));
                                }
                                return; // Stop early so text doesn't highlight
                            }
                        }
                    }
                }
            }
        }
        
        // 2. RIGHT-CLICK INTERCEPTOR: Triggers Context Menus on the Sidebar Tree (fChannelTree)
        if (message->what == B_MOUSE_DOWN && handler == fChannelTree) {
            int32 buttons;
            if (message->FindInt32("buttons", &buttons) == B_OK && buttons == B_SECONDARY_MOUSE_BUTTON) {
                BPoint point;
                message->FindPoint("be:view_where", &point);                
                int32 index = fChannelTree->IndexOf(point);                 
                if (index >= 0) {
                    fChannelTree->Select(index);
                    BListItem* generalItem = fChannelTree->ItemAt(index);
                    
                    if (generalItem != nullptr) {
                        ServerTreeItem* serverItem = dynamic_cast<ServerTreeItem*>(generalItem);
                        if (serverItem != nullptr) {
                            ShowContextMenu(fChannelTree->ConvertToScreen(point), serverItem);
                            return; 
                        }
                        
                        ChannelTreeItem* chanItem = dynamic_cast<ChannelTreeItem*>(generalItem);
                        if (chanItem != nullptr) {
                            ShowContextMenu(fChannelTree->ConvertToScreen(point), chanItem);
                            return; 
                        }
                    }
                }
            }
        }


        
        // Pass everything else to the default handler loop
        BWindow::DispatchMessage(message, handler);
    }




	//@messagereceived
    void MessageReceived(BMessage* message) override {
        switch (message->what) {


        case 'emcl': {
            const char* triggerText;
            if (message->FindString("trigger", &triggerText) == B_OK) {
                BTextView* textView = fInputControl->TextView();
                if (textView == nullptr) break;

                int32 textLen = textView->TextLength();
                
                // Keep typing continuous by forcing focus to the input field first
                fInputControl->MakeFocus(true);
                
                // 1. Move cursor to the absolute end of the message text string
                textView->Select(textLen, textLen);
                
                // 2. Add a leading space if we aren't at the start of an empty line
                if (textLen > 0) {
                    BString currentText = fInputControl->Text();
                    if (!currentText.EndsWith(" ")) {
                        textView->Insert(" ");
                    }
                }
                
                // 3. Insert the exact emoticon trigger text shortcut string
                textView->Insert(triggerText);
                
                // 4. CRITICAL FIX: Insert a real space character via the text view API.
                // This forces the Interface Kit to flag the control as fully updated.
                textView->Insert(" ");
                
                // 5. Ensure the input view follows the text growth bounds
                textView->ScrollToSelection();
            }
            break;
        }




        // PRIVATE QUERY EXECUTOR: Opens a new chat session branch for one-on-one chats
        case MSG_CONTEXT_PRIVMSG: {
            BString targetNick;
            if (message->FindString("target_nick", &targetNick) == B_OK && targetNick.Length() > 0) {
                
                // 1. Resolve which server connection context is currently active
                ServerTreeItem* contextServer = (fCurrentServerNode != nullptr) ? fCurrentServerNode : fLiberaNode;
                if (contextServer == nullptr) break;

                // 2. Check if a private query tree node already exists for this person
                ChannelTreeItem* queryNode = FindChannelNode(contextServer, targetNick);
                
                if (!queryNode) {
                    // Create a brand-new workspace window tab leaf node for this user
                    queryNode = new ChannelTreeItem(targetNick.String(), 
                                                   contextServer->GetIndex(), 
                                                   contextServer->IsCustom());

                    fChannelTree->AddUnder(queryNode, contextServer); 
                    fChannelTree->Expand(contextServer);
                    
                    // Instantiate their local channel room message buffer caches
                    LogToItemBuffer(queryNode, BString("--- Started Private Conversation with ") << targetNick << "\n");
                    fChannelUsers[queryNode] = new BObjectList<UserListItem, true>(20);
                }

                // 3. Automatically swap the active workspace panel view directly over to the new chat tab
                fActiveBufferItem = queryNode;
                if (fCustomChatLog != nullptr) {
                    fCustomChatLog->SetActiveChannel(fActiveBufferItem);
                }

                // Update UI text bars instantly
                fTopicView->SetText(BString("Private query with ") << targetNick);
                
                // Trigger a full layout redraw to update lists and selection boxes cleanly
                RefreshUserListUI();
            }
            break;
        }


         // 1. CONTEXT CLICK HANDLER: Intercepts row interactions to show the menu
        case MSG_USER_LIST_CONTEXT_CLICK: {
            BPoint mousePoint;
            uint32 mouseBtn = 0;
            
            fUserList->GetMouse(&mousePoint, &mouseBtn);
            
            // In Haiku, secondary button clicks are evaluated via bitwise validation flags
            if (mouseBtn & B_SECONDARY_MOUSE_BUTTON) {
                int32 selectedIdx = fUserList->CurrentSelection();
                if (selectedIdx >= 0) {
                    UserListItem* clickedItem = dynamic_cast<UserListItem*>(fUserList->ItemAt(selectedIdx));
                    if (clickedItem != nullptr) {
                        
                        // Strip any common IRC rank prefixes (~, &, %, @, +) safely
                        BString cleanNick = clickedItem->Text();
                        while (cleanNick.StartsWith("~") || cleanNick.StartsWith("&") || 
                               cleanNick.StartsWith("%") || cleanNick.StartsWith("@") || 
                               cleanNick.StartsWith("+")) {
                            cleanNick.Remove(0, 1);
                        }

                        BPopUpMenu* contextMenu = new BPopUpMenu("UserContext", false, false);

                        // CONDITION A: You right-clicked YOUR OWN nickname handle
                        if (cleanNick == fMyNick) {
                            BMenuItem* toggleAwayItem = nullptr;
                            if (clickedItem->IsAway()) {
                                toggleAwayItem = new BMenuItem("Set Status: Back", new BMessage(MSG_CONTEXT_SET_AWAY));
                            } else {
                                toggleAwayItem = new BMenuItem("Set Status: Away", new BMessage(MSG_CONTEXT_SET_AWAY));
                            }
                            contextMenu->AddItem(toggleAwayItem);
                        } 
                        // CONDITION B: You right-clicked SOMEONE ELSE's nickname handle
                        else {
                            BString menuLabel;
                            menuLabel << "Private Message " << cleanNick;
                            
                            BMessage* pmMsg = new BMessage(MSG_CONTEXT_PRIVMSG);
                            pmMsg->AddString("target_nick", cleanNick);
                            
                            BMenuItem* pmItem = new BMenuItem(menuLabel.String(), pmMsg);
                            contextMenu->AddItem(pmItem);
                        }

                        contextMenu->SetTargetForItems(this);

                        // Launch the pop-up menu right under your cursor coordinates safely
                        contextMenu->Go(fUserList->ConvertToScreen(mousePoint), true, true, true);
                    }
                }
            }
            break;
        }



        // 2. EXECUTE SERVER TRANSMISSION: Dispatches the corresponding raw /AWAY parameters
        case MSG_CONTEXT_SET_AWAY: {
            if (fActiveBufferItem != nullptr) {
                ServerTreeItem* targetedServer = (fCurrentServerNode != nullptr) ? fCurrentServerNode : fLiberaNode;
                BSecureSocket* activeSocket = nullptr;
                
                if (targetedServer != nullptr && fServerSockets.count(targetedServer) > 0) {
                    activeSocket = fServerSockets[targetedServer];
                } else {
                    activeSocket = (targetedServer == fOftcNode) ? fOftcSocket : fLiberaSocket;
                }

                if (activeSocket != nullptr) {
                    BString outgoingPayload;
                    bool currentlyAway = false;

                    // Query our active right panel row objects to determine status state
                    for (int32 i = 0; i < fUserList->CountItems(); i++) {
                        UserListItem* uiUser = dynamic_cast<UserListItem*>(fUserList->ItemAt(i));
                        if (uiUser != nullptr) {
                            BString txt = uiUser->Text();
                            while (txt.StartsWith("~") || txt.StartsWith("&") || 
                                   txt.StartsWith("%") || txt.StartsWith("@") || 
                                   txt.StartsWith("+")) {
                                txt.Remove(0, 1);
                            }
                            if (txt == fMyNick) {
                                currentlyAway = uiUser->IsAway();
                                break;
                            }
                        }
                    }

                    if (currentlyAway) {
                        outgoingPayload << "AWAY\r\n"; // Sending empty AWAY clears status on server
                    } else {
                        outgoingPayload << "AWAY :" << cfg.awayMessage.c_str() << "\r\n";
                    }
                    
                    activeSocket->Write(outgoingPayload.String(), outgoingPayload.Length());
                }
            }
            break;
        }



        
        case MSG_TOPIC_CHANGED: {
            if (fActiveBufferItem != nullptr) {
                BString channelName = fActiveBufferItem->Text();
                
                if (channelName.StartsWith("#")) {
                    BString newlyTypedTopic = fTopicView->Text();
                    newlyTypedTopic.Trim();

                    if (newlyTypedTopic.Length() > 0) {
                        // 1. Determine the correct active server node from the active channel node context
                        ChannelTreeItem* activeChan = dynamic_cast<ChannelTreeItem*>(fActiveBufferItem);
                        ServerTreeItem* targetedServer = nullptr;
                        
                        if (activeChan != nullptr) {
                            // Loop through your parent server nodes to locate the match, or query your tree structure directly
                            // Alternatively, fallback to your active server node tracking pointer:
                            targetedServer = (fCurrentServerNode != nullptr) ? fCurrentServerNode : fLiberaNode;
                        } else {
                            targetedServer = (fCurrentServerNode != nullptr) ? fCurrentServerNode : fLiberaNode;
                        }

                        // 2. Safely resolve the associated secure writing socket pipeline
                        BSecureSocket* activeSocket = nullptr;
                        if (targetedServer != nullptr && fServerSockets.count(targetedServer) > 0) {
                            activeSocket = fServerSockets[targetedServer];
                        } else {
                            activeSocket = (targetedServer == fOftcNode) ? fOftcSocket : fLiberaSocket;
                        }

                        if (activeSocket != nullptr) {
                            BString topicCommand;
                            topicCommand << "TOPIC " << channelName << " :" << newlyTypedTopic << "\r\n";
                            activeSocket->Write(topicCommand.String(), topicCommand.Length());
                        }
                    }
                }
            }
            break;
        }



		case MSG_TOGGLE_CUSTOM_DRAW: {
			bool initialBootPass = false;
			message->FindBool("initial_boot_pass", &initialBootPass);

			// Flip the configuration state ONLY if clicked by the user (not on startup)
			if (!initialBootPass) {
				cfg.useCustomDrawFunction = !cfg.useCustomDrawFunction;
				save_config();
			}

			if (fChatLog == nullptr || fCustomChatLog == nullptr || fChatContainer == nullptr)
				break;

			BView* chatScroll = fChatLog->Parent();       
			BView* customScroll = fCustomChatLog->Parent(); 
			BLayout* layout = fChatContainer->GetLayout();

			if (layout == nullptr)
				break;

			if (cfg.useCustomDrawFunction) {
				// Safely swap out standard text layout for the high-performance canvas
				if (chatScroll && chatScroll->Parent() == fChatContainer) {
					layout->RemoveView(chatScroll);
				}
				if (customScroll && customScroll->Parent() != fChatContainer) {
					layout->AddView(customScroll);
				}
			} else {
				if (customScroll && customScroll->Parent() == fChatContainer) {
					layout->RemoveView(customScroll);
				}
				if (chatScroll && chatScroll->Parent() != fChatContainer) {
					layout->AddView(chatScroll);
				}
			}
    
			if (fCustomChatLog) {
				fCustomChatLog->SetExplicitMinSize(BSize(100.0f, 100.0f));
				fCustomChatLog->SetExplicitMaxSize(BSize(B_SIZE_UNSET, B_SIZE_UNSET));
				fCustomChatLog->SetExplicitPreferredSize(BSize(B_SIZE_UNSET, B_SIZE_UNSET));
				fCustomChatLog->ClearAllLines();

				// --- FIX: FORCE BACKGROUND SYNC ON ENGINE INITIALIZATION ---
				// Ensure the unique server background is explicitly re-bound to this specific engine pass
				if (fCurrentServerNode != nullptr) {
					std::string activeServerName = fCurrentServerNode->Text(); 
					for (size_t i = 0; i < cfg.servers.size(); i++) {
						if (cfg.servers[i].name == activeServerName) {
							std::string initialBg = cfg.servers[i].backgroundImagePath;
							int32 initialDim = cfg.servers[i].backgroundOpacity;
							
							if (!initialBg.empty()) {
								fCustomChatLog->SetBackgroundImage(initialBg.c_str());
								fCustomChatLog->SetBackgroundDimming(initialDim);
							}
							break;
						}
					}
				}
			}

			this->RebuildActiveChannelBuffer();
			
			fChatContainer->InvalidateLayout(true);
			this->InvalidateLayout(true);
			break;
		}





        case MSG_DISCONNECT_SERVER: {
            ServerTreeItem* srvItem = nullptr;
            if (message->FindPointer("server_item", (void**)&srvItem) == B_OK && srvItem != nullptr) {
                
                // Thread-safe map inspection
                Lock();
                if (fServerSockets.count(srvItem) > 0 && fServerSockets[srvItem] != nullptr) {
                    
                    // 1. Send an optional quick QUIT message so the remote network drops you gracefully
                    BString quitPayload;
                    quitPayload << "QUIT :Disconnecting from client\r\n";
                    fServerSockets[srvItem]->Write(quitPayload.String(), quitPayload.Length());
                    
                    // 2. Forcefully close the network pipe descriptor. 
                    // This wakes up background worker thread immediately!
                    fServerSockets[srvItem]->Disconnect();
                    
                    // 3. Print a clean disconnection status notice locally
                    BStringItem* serverLog = FindServerLogNode(srvItem);
                    if (serverLog != nullptr) {
                        LogToItemBuffer(serverLog, "--- Disconnected from server by user choice.\n");
                    }
                }
                Unlock();
            }
            break;
        }


        case 'dlcs': { // Delete Custom Server Message
            ServerTreeItem* srvItem = nullptr;
            if (message->FindPointer("server_item", (void**)&srvItem) != B_OK || srvItem == nullptr) {
                break;
            }

            size_t targetIndex = srvItem->GetIndex();
            
            // Check bounds against configuration memory spaces safely
            if (srvItem->IsCustom() && targetIndex < cfg.customServers.size()) {
                
                // 1. Thread Cleanup Stage (No blocking wait loops inside UI lock space)
                Lock();
                thread_id tid = (fServerThreads.count(srvItem) > 0) ? fServerThreads[srvItem] : -1;
                BSecureSocket* socketPtr = (fServerSockets.count(srvItem) > 0) ? fServerSockets[srvItem] : nullptr;
                
                if (socketPtr != nullptr) {
                    BString quitPayload;
                    quitPayload << "QUIT :Server entry deleted by user\r\n";
                    socketPtr->Write(quitPayload.String(), quitPayload.Length());
                    socketPtr->Disconnect(); 
                }

                // Clean the internal key trackers immediately to prevent reuse conflicts
                fServerThreads.erase(srvItem);
                fServerSockets.erase(srvItem);
                Unlock();

                // 2. Kill the socket thread cleanly without freezing the window interaction thread
                if (tid >= 0) {
                    // Signal the Haiku kernel to kill the thread if it fails to exit smoothly on disconnect
                    kill_thread(tid); 
                }

                // 3. Remove item out of global configurations vector array
                cfg.customServers.erase(cfg.customServers.begin() + targetIndex);
                
                // 4. Cleanly strip all nested channel child nodes under this server to prevent dangling sub-views
                int32 srvTreePos = fChannelTree->IndexOf(srvItem);
                if (srvTreePos != B_ERROR) {
                    // Iterate and remove children until we hit another server item or out-of-bounds layout limit
                    while (srvTreePos + 1 < fChannelTree->CountItems()) {
                        BListItem* nextItem = fChannelTree->ItemAt(srvTreePos + 1);
                        // If it's a ServerTreeItem, we reached the next network bracket; stop deleting
                        if (dynamic_cast<ServerTreeItem*>(nextItem) != nullptr) {
                            break;
                        }
                        // It is a subchannel item node (like a BStringItem/ChannelItem) — strip it safely
                        BListItem* removedChild = fChannelTree->RemoveItem(srvTreePos + 1);
                        delete removedChild;
                    }
                }

                // 5. Remove the actual server item node from visual container
                fChannelTree->RemoveItem(srvItem);
                delete srvItem;

                // 6. Fix internal indexes across remaining custom layout slots safely
                for (int32 i = 0; i < fChannelTree->CountItems(); i++) {
                    ServerTreeItem* current = dynamic_cast<ServerTreeItem*>(fChannelTree->ItemAt(i));
                    if (current != nullptr && current->IsCustom()) {
                        size_t curIdx = current->GetIndex();
                        if (curIdx > targetIndex) {
                            current->SetIndex(curIdx - 1); 
                        }
                    }
                }

                // 7. Commit changes to persistent system disk storage
                save_config();
                
                // Force active view adjustments to re-paint tree nodes
                fChannelTree->Invalidate();
            }
            break;
        }



        case 'adcs': {            
            AddServerWindow* dlg = new AddServerWindow(this);
            dlg->Show(); 
            break;
        }

        case MSG_ADD_CUSTOM_SERVER_SUBMIT: {
            BString name, host, nick, pass;
            BString altNick, altNick2; 
            int32 port;

            if (message->FindString("name", &name) == B_OK &&
                message->FindString("host", &host) == B_OK &&
                message->FindInt32("port", &port) == B_OK &&
                message->FindString("nick", &nick) == B_OK) {
                
                message->FindString("altNick", &altNick);
                message->FindString("altNick2", &altNick2);
                message->FindString("pass", &pass); // Can remain blank safely

                // 1. Build the data structure block matching settings scheme
                ServerConfig customSrv;
                customSrv.name = name.String();
                customSrv.host = host.String();
                customSrv.port = static_cast<uint16>(port);
                customSrv.nick = nick.String();
                
                // Assign the extracted alternate strings, applying safe defaults if left blank in the form
                customSrv.altNick = (altNick.Length() > 0) ? altNick.String() : BString(nick).Append("+").String();
                customSrv.altNick2 = (altNick2.Length() > 0) ? altNick2.String() : BString(nick).Append("__").String();
                
                customSrv.pass = pass.String();
                customSrv.autoConnect = false;
                customSrv.autoReconnect = false;
                customSrv.hideStatusMessages = false;

                // 2. Append directly to the config struct vector array and save file changes
                cfg.customServers.push_back(customSrv);
                save_config();

                // 3. Update the UI tree instantly on screen without restarting!
                size_t newIndex = cfg.customServers.size() - 1;
                
                // Keeping precise signature fields exactly the same
                ServerTreeItem* customNode = new ServerTreeItem(customSrv.name.c_str(), newIndex, false, true);
                
                fChannelTree->AddItem(customNode);
                fChannelTree->Invalidate(); 
            }
            break;
        }




        	
    case 'rmch': { // Remove Channel action handler
        ChannelTreeItem* chanItem = nullptr;
        
        if (message->FindPointer("channel_item", (void**)&chanItem) != B_OK || chanItem == nullptr) {
            break;
        }
        
        // 1. Clean dynamic tracking tags from raw string labels before checking prefixes
        BString targetChanName(chanItem->Text());
        int32 tagPos = targetChanName.FindFirst(" [");
        if (tagPos != B_ERROR) {
            targetChanName.Truncate(tagPos);
        }

        // ====================================================================
        // STATE INTERCEPTION VALUE CHECK (Channel vs Private Query)
        // ====================================================================
        // In the IRC protocol, channels MUST begin with a specific prefix symbol 
        // (typically '#', '&', or '!'). If missing, this item is a private query!
        bool isActualIrcChannel = targetChanName.StartsWith("#") || 
                                  targetChanName.StartsWith("&") || 
                                  targetChanName.StartsWith("!");

        ServerTreeItem* parentServer = static_cast<ServerTreeItem*>(fChannelTree->Superitem(chanItem));
        
        // Only modify network sockets or configuration lists if the item is a true channel room
        if (parentServer != nullptr && isActualIrcChannel) {
            
            // Dynamically pull the correct network connection out of our lookup map
            BSecureSocket* activeSocket = nullptr;
            if (fServerSockets.count(parentServer) > 0) {
                activeSocket = fServerSockets[parentServer];
            } else {
                activeSocket = (parentServer == fOftcNode) ? fOftcSocket : fLiberaSocket;
            }

            if (activeSocket != nullptr) {
                BString partCmd;
                partCmd << "PART " << targetChanName << " :Channel removed by user\r\n";
                activeSocket->Write(partCmd.String(), partCmd.Length());
            }
            
            // 2. Remove the channel name from the configuration autojoin vector array
            size_t serverIdx = parentServer->GetIndex();
            
            if (parentServer->IsCustom()) {
                if (serverIdx < cfg.customServers.size()) {
                    auto& autojoinVec = cfg.customServers[serverIdx].autojoin;
                    for (auto it = autojoinVec.begin(); it != autojoinVec.end(); ) {
                        if (targetChanName.ICompare(it->c_str()) == 0) {
                            it = autojoinVec.erase(it);
                        } else {
                            ++it;
                        }
                    }
                }
            } else {
                if (serverIdx < cfg.servers.size()) {
                    auto& autojoinVec = cfg.servers[serverIdx].autojoin;
                    for (auto it = autojoinVec.begin(); it != autojoinVec.end(); ) {
                        if (targetChanName.ICompare(it->c_str()) == 0) {
                            it = autojoinVec.erase(it);
                        } else {
                            ++it;
                        }
                    }
                }
            }
        } else if (parentServer != nullptr && !isActualIrcChannel) {
            if (cfg.debugEnable) {
                printf("[DEBUG] Removing private query tab (%s). Skipping network PART commands.\n", 
                    targetChanName.String());
            }
        }

        // 3. Wipe current chat logs from active memory views if we delete the room we are looking at
        if (fActiveBufferItem == chanItem) {
            fActiveBufferItem = nullptr;
            
            fChatLog->SetText("");
            
            if (fCustomChatLog) {
                fCustomChatLog->SetActiveChannel(nullptr);
                fCustomChatLog->ClearAllLines();
            }
            
            while (fUserList->CountItems() > 0) {
                delete fUserList->RemoveItem((int32)0);
            }
            
            fTopicView->SetText("No active channel buffer targeted.");
        }
        
        // 4. Safely purge dynamic memory trackers associated with this pointer key
        fTextBuffers.erase(chanItem);
        fChannelTopics.erase(chanItem);
        
        if (fChannelUsers.find(chanItem) != fChannelUsers.end()) {
            delete fChannelUsers[chanItem];
            fChannelUsers.erase(chanItem);
        }

        // 5. Safely drop the element out of the UI tree structure entirely
        fChannelTree->RemoveItem(chanItem);
        delete chanItem; 
        
        // 6. Select a fallback tree item if the workspace went completely blank
        if (fActiveBufferItem == nullptr && fChannelTree->CountItems() > 0) {
            fChannelTree->Select(0);
        }

        // 7. Save settings file to disk immediately
        save_config();
        break;
    }



        	
       case 'mscf': {
        save_config(); // Save new dimensions to configuration file

        // 1. Update Server List Font AND recalculate item heights (Left panel)
        BFont treeFont;
        fChannelTree->GetFont(&treeFont);
        treeFont.SetSize(cfg.serverListFontSize);
        fChannelTree->SetFont(&treeFont);

        int32 treeCount = fChannelTree->CountItems();
        for (int32 i = 0; i < treeCount; i++) {
            BListItem* item = fChannelTree->ItemAt(i);
            if (item != nullptr) {
                item->Update(fChannelTree, &treeFont);
            }
        }
        fChannelTree->InvalidateLayout();
        fChannelTree->Invalidate(); 

        // 2. Update Topic View Bar Font Size
        BFont topicFont;
        fTopicView->GetFont(&topicFont);
        topicFont.SetSize(cfg.chatLogFontSize); // Make chat log text size
        fTopicView->SetFont(&topicFont);
        
        if (fTopicView->TextView()) {
            fTopicView->TextView()->SetFontAndColor(&topicFont, B_FONT_SIZE); // Updates the editable region
        }
        fTopicView->InvalidateLayout();
        fTopicView->Invalidate();

        // 3. OPTIMIZED: Update BTextView history without erasing formatted style loops (Center panel)
        BFont chatFont;
        fChatLog->GetFont(&chatFont);
        chatFont.SetSize(cfg.chatLogFontSize);
        fChatLog->SetFont(&chatFont);

        // Tell the text view to smoothly scale all characters to the new size in-place
        fChatLog->SetFontAndColor(&chatFont, B_FONT_SIZE); 
        fChatLog->Invalidate();

        // 4. Update User List Font AND recalculate item heights (Right panel)
        BFont userFont;
        fUserList->GetFont(&userFont);
        userFont.SetSize(cfg.userListFontSize);
        fUserList->SetFont(&userFont);

        int32 userCount = fUserList->CountItems();
        for (int32 i = 0; i < userCount; i++) {
            BListItem* item = fUserList->ItemAt(i);
            if (item != nullptr) {
                item->Update(fUserList, &userFont);
            }
        }
        fUserList->InvalidateLayout();
        fUserList->Invalidate();

        // --- STEP 5: Live Resize and Recalculate Custom Draw Engine Layout Geometry ---
        if (fCustomChatLog != nullptr) {
            fCustomChatLog->SetLineHeight(cfg.chatLogFontSize + 4.0f);
            
            std::string bgPath = "";
            int32 dimmingLevel = 30;
            size_t activeIdx = (size_t)selectedConfig;
            
            if (activeIdx < cfg.servers.size()) {
                bgPath = cfg.servers[activeIdx].backgroundImagePath;
                dimmingLevel = cfg.servers[activeIdx].backgroundOpacity; // <-- EXTRACTED
            } else {
                size_t customIdx = activeIdx - cfg.servers.size();
                if (customIdx < cfg.customServers.size()) {
                    bgPath = cfg.customServers[customIdx].backgroundImagePath;
                    dimmingLevel = cfg.customServers[customIdx].backgroundOpacity; // <-- EXTRACTED
                }
            }

            if (bgPath.empty()) {
                fCustomChatLog->SetBackgroundImage(nullptr);
            } else {
                fCustomChatLog->SetBackgroundImage(bgPath.c_str());
            }

            // Push the dimming slider changes down directly into the graphics card buffer loop
            fCustomChatLog->SetBackgroundDimming(dimmingLevel);
            
            // Force the word-wrapping coordinates to recalculate matching the new scale ---
            fCustomChatLog->RecalculateAllLineWraps();
            
            // Forces the app server to trigger CustomChatView::Draw instantly
            fCustomChatLog->Invalidate();
        }


        break;
    }






    	case MSG_CONTEXT_CONFIGURE_SERVER: {
        	ServerTreeItem* item = nullptr;
        	if (message->FindPointer("server_item", (void**)&item) == B_OK && item != nullptr) {
            
            	size_t matchingIndex = item->GetIndex(); // Extract the true index directly from the item!
            	bool isCustom = item->IsCustom();        // Extract whether it is custom or default
            
            	// Pass the custom flag as a new 4th argument to the dialog window constructor
            	ServerConfigWindow* configWin = new ServerConfigWindow(this, item, matchingIndex, isCustom);
            	configWin->Show();
        	}
        	break;
    	}


            case MSG_RECONNECT_SERVER: {
                void* ptr;
                if (message->FindPointer("server_item", &ptr) == B_OK) {
                    ServerTreeItem* serverItem = static_cast<ServerTreeItem*>(ptr);
                    if (serverItem != nullptr) {
                        BString textNotice;
                        textNotice << "--- [Auto-Reconnect] Connection lost. Attempting to reconnect to " << serverItem->Text() << "...\n";
                        LogToItemBuffer(FindServerLogNode(serverItem), textNotice);
                        
                        // Fire up the native connection runner cleanly
                        ConnectToServer(serverItem);
                    }
                }
                break;
            }


            case MSG_TOGGLE_AUTORECONNECT: {
                void* ptr;
                if (message->FindPointer("server_item", &ptr) == B_OK) {
                    ServerTreeItem* serverItem = static_cast<ServerTreeItem*>(ptr);
                    if (serverItem != nullptr) {
                        size_t idx = serverItem->GetIndex();
                        
                        bool newState = !serverItem->IsAutoReconnect();
                        serverItem->SetAutoReconnect(newState);
                        cfg.servers[idx].autoReconnect = newState;
                        
                        save_config();
                        fChannelTree->InvalidateItem(fChannelTree->IndexOf(serverItem));
                    }
                }
                break;
            }


            case MSG_CONTEXT_ABOUT: {
                // Formulate the informational message body text
                BString aboutText;
                aboutText <<  AppInfo::VERSION_STRING << "\n" 
                		  << "By Kris Beazley \"ablyss\"\n"
                		  << "Copyright 2026 The MIT License\n\n"

                          << "A lightweight, multi-server IRC client built natively "
                          << "for the Haiku Operating System utilizing BSplitView layout architectures.\n\n"
                          << "Features JSON configuration saving, automated services identification, "
                          << "custom spreadsheet channel list navigators, and dynamic dark mode adaptability.";

                // Instantiate a native Haiku informational dialog modal box
                BAlert* aboutAlert = new BAlert("About Cricket", aboutText.String(), "OK", 
                                                nullptr, nullptr, B_WIDTH_AS_USUAL, B_INFO_ALERT);
                
                // Instruct the alert modal box to float asynchronously so it doesn't freeze the main app loop
                aboutAlert->Go();
                break;
            }

        	
		    case MSG_TOGGLE_HIDE_STATUS: {
        		ServerTreeItem* item = nullptr;
        		if (message->FindPointer("server_item", (void**)&item) == B_OK && item != nullptr) {
            		// 1. Flip the state inside the visible UI tree item element
            		bool newStatus = !item->IsHideStatus();
            		item->SetHideStatus(newStatus);
            
            		size_t targetIndex = item->GetIndex();

            		// 2. Directly target the correct array vector index using properties
            		if (item->IsCustom()) {
                        if (targetIndex < cfg.customServers.size()) {
                            cfg.customServers[targetIndex].hideStatusMessages = newStatus;
                        }
            		} else {
                        if (targetIndex < cfg.servers.size()) {
                            cfg.servers[targetIndex].hideStatusMessages = newStatus;
                        }
            		}
            		
            		// 3. Commit changes to CricketConfig.txt JSON file instantly
            		save_config(); 
        		}
        		break;
    		}
        	
        	
            case MSG_CONTEXT_CHAN_LIST: {
                void* ptr = nullptr;
                // FindPointer requires a pointer-to-a-pointer (void**)&ptr cast to work correctly
                if (message->FindPointer("server_item", (void**)&ptr) != B_OK || ptr == nullptr) {
                    break;
                }

                ServerTreeItem* serverItem = static_cast<ServerTreeItem*>(ptr);
                
                // Dynamically fetch the accurate active network connection from our lookup map
                BSecureSocket* activeSocket = nullptr;
                if (fServerSockets.count(serverItem) > 0) {
                    activeSocket = fServerSockets[serverItem];
                } else {
                    // Backwards-compatibility fallback for default servers on startup
                    activeSocket = (serverItem == fOftcNode) ? fOftcSocket : fLiberaSocket;
                }
                
                if (activeSocket != nullptr) {
                    // Safely query the window thread using Haiku's BMessenger API ---
                    bool windowIsValid = false;
                    if (fActiveListWindow != nullptr) {
                        BMessenger messenger(fActiveListWindow);
                        if (messenger.IsValid()) {
                            windowIsValid = true;
                        } else {
                            // The window was closed by the user and cleaned itself up; reset our tracking pointer safely
                            fActiveListWindow = nullptr;
                        }
                    }

                    // Launch or activate the float list view pop-up window securely
                    if (fActiveListWindow == nullptr || !windowIsValid) {
                        // Pass the serverItem pointer along so the pop-up knows its network context parent
                        fActiveListWindow = new IRCChannelListWindow(this, activeSocket, serverItem);
                        fActiveListWindow->Show();
                    } else {
                        // Lock and activate the window since it is still running smoothly in memory
                        if (fActiveListWindow->Lock()) {
                            fActiveListWindow->Activate(true);
                            fActiveListWindow->Unlock();
                        }
                    }
                } else {
                    // Fallback warning if the user attempts to view channels while completely offline
                    BString warning;
                    warning.SetToFormat("System Error: You must connect to '%s' first before requesting a channel list.\n", serverItem->Text());
                    LogToItemBuffer(fActiveBufferItem, warning);
                }
                break;
            }


        	
           case 'cldc':
                fActiveListWindow = nullptr;
                break;

        	
            case MSG_TOGGLE_AUTOCONNECT: {
                void* ptr;
                // Cast to (void**)&ptr to comply with the Haiku FindPointer API signature
                if (message->FindPointer("server_item", (void**)&ptr) == B_OK) {
                    ServerTreeItem* serverItem = static_cast<ServerTreeItem*>(ptr);
                    if (serverItem != nullptr) {
                        size_t idx = serverItem->GetIndex();
                        
                        // Invert state value
                        bool newState = !serverItem->IsAutoConnect();
                        serverItem->SetAutoConnect(newState);
                        
                        // Route the configuration assignment to the correct target profile vector
                        if (serverItem->IsCustom()) {
                            if (idx < cfg.customServers.size()) {
                                cfg.customServers[idx].autoConnect = newState;
                            }
                        } else {
                            if (idx < cfg.servers.size()) {
                                cfg.servers[idx].autoConnect = newState;
                            }
                        }
                        
                        // Flush to settings file and force immediate sidebar graphic redraw loop pass
                        save_config();
                        fChannelTree->InvalidateItem(fChannelTree->IndexOf(serverItem));
                    }
                }
                break;
            }

            
            case B_COLORS_UPDATED: {
                // 1. Fetch the newly selected color vectors
                rgb_color newBgColor = ui_color(B_DOCUMENT_BACKGROUND_COLOR);
                rgb_color newTextColor = ui_color(B_DOCUMENT_TEXT_COLOR);
                rgb_color panelBgColor = ui_color(B_PANEL_BACKGROUND_COLOR);
                rgb_color panelTextColor = ui_color(B_PANEL_TEXT_COLOR);

                // 2. Clear and update the standard BTextView layout tree properties
                if (fChatLog) {
                    fChatLog->SetViewColor(newBgColor);
                    fChatLog->SetLowColor(newBgColor);
                    
                    // Force BTextView's internal styling loop to flag a full redraw
                    fChatLog->SetStylable(true);
                    
                    // Safely update text run foregrounds without altering your custom bold/underlines
                    fChatLog->SetFontAndColor(nullptr, 0, &newTextColor);

                    // Ascend through parent wrappers (like BScrollView) to wipe out sticky backgrounds
                    BView* parentView = fChatLog->Parent();
                    while (parentView != nullptr && parentView != fChatContainer) {
                        parentView->SetViewColor(newBgColor);
                        parentView->SetLowColor(newBgColor);
                        parentView->Invalidate();
                        parentView = parentView->Parent();
                    }
                    
                    fChatLog->Invalidate();
                }

                // 3. Explicitly propagate theme updates down to your Custom Draw Engine View ---
                if (fCustomChatLog) {
                    // Update the custom drawing view base parameters
                    fCustomChatLog->SetViewColor(newBgColor);
                    fCustomChatLog->SetLowColor(newBgColor);
                    
                    // Route the message directly into the view's own MessageReceived handler 
                    // so it updates its system backgrounds and repaints!
                    fCustomChatLog->MessageReceived(message);
                    fCustomChatLog->Invalidate();
                }
                
                // 4. Update the global channel Topic text view layout blocks
                if (fTopicView) {
                    fTopicView->SetViewColor(panelBgColor);
                    fTopicView->SetLowColor(panelBgColor);
                    
                    BTextView* topicText = fTopicView->TextView();
                    if (topicText) {
                        topicText->SetViewColor(panelBgColor);
                        topicText->SetLowColor(panelBgColor);
                        topicText->SetFontAndColor(be_plain_font, B_FONT_ALL, &panelTextColor);
                        topicText->Invalidate();
                    }
                    fTopicView->Invalidate();
                }

                // 5. Instruct the layout engine to completely refresh container structures
                if (fChatContainer) {
                    fChatContainer->InvalidateLayout(true);
                    fChatContainer->Invalidate();
                }
                
                // 6. Fall back to base class processing safely at the end ---
                // This lets the window route the event down to all other standard panels naturally
                BWindow::MessageReceived(message); 
                break;
            }






        	
            case MSG_TOGGLE_AUTOJOIN: {
                void* ptr;
                // Cast to (void**)&ptr to comply with the Haiku FindPointer API signature
                if (message->FindPointer("chan_item", (void**)&ptr) == B_OK) {
                    ChannelTreeItem* chanItem = static_cast<ChannelTreeItem*>(ptr);
                    if (chanItem != nullptr) {
                        size_t srvIdx = chanItem->GetServerIndex();
                        std::string targetChan = chanItem->Text();
                        
                        // Route data manipulation to customServers or servers using the item flag
                        if (chanItem->IsCustom()) {
                            if (srvIdx < cfg.customServers.size()) {
                                auto& vec = cfg.customServers[srvIdx].autojoin;
                                if (chanItem->IsAutoJoin()) {
                                    // Deactivate: Remove the string leaf value from our configuration vector
                                    vec.erase(std::remove(vec.begin(), vec.end(), targetChan), vec.end());
                                    chanItem->SetAutoJoin(false);
                                } else {
                                    // Activate: Append string into configuration profile
                                    vec.push_back(targetChan);
                                    chanItem->SetAutoJoin(true);
                                }
                            }
                        } else {
                            if (srvIdx < cfg.servers.size()) {
                                auto& vec = cfg.servers[srvIdx].autojoin;
                                if (chanItem->IsAutoJoin()) {
                                    // Deactivate: Remove the string leaf value from our configuration vector
                                    vec.erase(std::remove(vec.begin(), vec.end(), targetChan), vec.end());
                                    chanItem->SetAutoJoin(false);
                                } else {
                                    // Activate: Append string into configuration profile
                                    vec.push_back(targetChan);
                                    chanItem->SetAutoJoin(true);
                                }
                            }
                        }
                        
                        // Flush the changes instantly onto the hard disk and force row graphics updates
                        save_config();
                        fChannelTree->InvalidateItem(fChannelTree->IndexOf(chanItem));
                    }
                }
                break;
            }


        	
        case 'slch': {
            int32 selectedIdx = fChannelTree->CurrentSelection();
            if (selectedIdx >= 0) {
                // Change the cast to generic BListItem first to preserve your custom object signatures
                BListItem* rawItem = fChannelTree->ItemAt(selectedIdx);
                BStringItem* selectedItem = dynamic_cast<BStringItem*>(rawItem);
                
                if (selectedItem != nullptr && selectedItem != fActiveBufferItem) {
                    
                    if (fActiveBufferItem != nullptr && !cfg.useCustomDrawFunction) {
                        fTextBuffers[fActiveBufferItem] = fChatLog->Text();
                    }
                    
                    fActiveBufferItem = selectedItem;
                    
                    // === FIXED CONTEXT POINTER SYNCHRONIZATION ===
                    BListItem* superItem = fChannelTree->Superitem(rawItem);
                    if (superItem != nullptr) {
                        ServerTreeItem* parentServer = dynamic_cast<ServerTreeItem*>(superItem);
                        if (parentServer != nullptr) {
                            fCurrentServerNode = parentServer;
                        }
                    } else {
                        ServerTreeItem* clickedServer = dynamic_cast<ServerTreeItem*>(rawItem);
                        if (clickedServer != nullptr) {
                            fCurrentServerNode = clickedServer;
                        }
                    }
                    // =========================================================================
                    
                    // === DYNAMIC BACKGROUND SEPARATION PASS ===
                    if (fCustomChatLog != nullptr && fCurrentServerNode != nullptr) {
                        // Securely read the name via our verified ServerTreeItem cast target
                        BString currentServerName(fCurrentServerNode->Text());
                        bool bgFound = false;

                        // 1. Check primary/standard servers list
                        for (const auto& srv : cfg.servers) {
                            if (BString(srv.name.c_str()) == currentServerName) {
                                fCustomChatLog->SetBackgroundImage(srv.backgroundImagePath.c_str());
                                fCustomChatLog->SetBackgroundDimming(srv.backgroundOpacity);
                                bgFound = true;
                                break;
                            }
                        }

                        // 2. Fallback to user-defined custom server profiles
                        if (!bgFound) {
                            for (const auto& srv : cfg.customServers) {
                                if (BString(srv.name.c_str()) == currentServerName) {
                                    fCustomChatLog->SetBackgroundImage(srv.backgroundImagePath.c_str());
                                    fCustomChatLog->SetBackgroundDimming(srv.backgroundOpacity);
                                    break;
                                }
                            }
                        }
                        
                        fCustomChatLog->Invalidate();
                    }

                    // =========================================================================

                    // Explicitly sync workspace visibility constraints down to Custom Canvas
                    if (fCustomChatLog != nullptr) {
                        fCustomChatLog->SetActiveChannel(fActiveBufferItem);
                    }

                    
                    // ACTION RESET: Check if the clicked item is a custom channel leaf node
                    ChannelTreeItem* chanItem = dynamic_cast<ChannelTreeItem*>(selectedItem);
                    if (chanItem != nullptr && chanItem->HasUnread()) {
                        chanItem->SetUnread(false);
                        fChannelTree->InvalidateItem(selectedIdx); // Force view to drop bold look instantly
                    }
                    
                    // 2. Clear visual modules completely before parsing history entries
                    fChatLog->SetText("");
                    if (fCustomChatLog != nullptr) {
                        fCustomChatLog->ClearAllLines();
                    }

                    while (fUserList->CountItems() > 0) {
                        delete fUserList->RemoveItem((int32)0);
                    }

                    fTopicView->SetText("No topic set.");

                    ServerTreeItem* isServerRootNode = dynamic_cast<ServerTreeItem*>(selectedItem);
                    BString itemText(selectedItem->Text());
                    
                    if (isServerRootNode != nullptr) {
                        // If the clicked object matches a server root node item entity, set the status banner
                        fTopicView->SetText("Network connection logs status feed.");
                    } else if (itemText.StartsWith("#")) {
                        
                        // Scan your cache map by raw text names to bypass pointer/context drift
                        bool topicFoundInCache = false;
                        for (auto it = fChannelTopics.begin(); it != fChannelTopics.end(); ++it) {
                            if (it->first != nullptr && BString(it->first->Text()) == itemText) {
                                fTopicView->SetText(it->second.String());
                                topicFoundInCache = true;
                                break;
                            }
                        }

                        if (!topicFoundInCache) {
                            fTopicView->SetText("No topic set.");
                        }
                        
                        // Restore the active user list including their channel modes!
                        if (fChannelUsers.find(fActiveBufferItem) != fChannelUsers.end()) {
                            BObjectList<UserListItem, true>* userVector = fChannelUsers[fActiveBufferItem];
                            
                            if (userVector != nullptr) {
                                BFont userListFont;
                                fUserList->GetFont(&userListFont);
                                userListFont.SetSize(cfg.userListFontSize);

                                for (int32 i = 0; i < userVector->CountItems(); i++) {
                                    UserListItem* cachedItem = userVector->ItemAt(i);
                                    if (cachedItem != nullptr) {
                                        bool userIsAway = cachedItem->IsAway();
                                        UserListItem* newUserItem = new UserListItem(cachedItem->Text(), userIsAway);
                                        
                                        fUserList->AddItem(newUserItem);
                                        newUserItem->Update(fUserList, &userListFont);
                                    }
                                }
                                // Sort Channel Operators
                                fUserList->SortItems(SortUsersByRank); 
                                
                                fUserList->InvalidateLayout();
                                fUserList->Invalidate();
                            }
                        }
                    }

                    // 3. Re-populate visible chat log panel via Rebuild routing block ---
                    if (fTextBuffers.find(fActiveBufferItem) != fTextBuffers.end()) {
                        this->RebuildActiveChannelBuffer();
                    } else {
                        fChatLog->SetText("");
                        if (fCustomChatLog != nullptr) {
                            fCustomChatLog->ClearAllLines();
                        }
                    }
                    
                    // --- NEW: DYNAMIC PICKER PANEL TOOLBAR SYNCHRONIZATION ---
                    if (fEmoticonGrid != nullptr && fCurrentServerNode != nullptr) {
                        bool emotesActive = true;
                        BString currentServerName(fCurrentServerNode->Text());

                        // Scan through standard configurations for a profile name match
                        for (const auto& srv : cfg.servers) {
                            if (BString(srv.name.c_str()) == currentServerName) {
                                emotesActive = srv.enableEmoticons;
                                break;
                            }
                        }

                        // Scan through user-defined custom server profiles if no standard match was found
                        for (const auto& srv : cfg.customServers) {
                            if (BString(srv.name.c_str()) == currentServerName) {
                                emotesActive = srv.enableEmoticons;
                                break;
                            }
                        }

                        // Toggle visibility of the 4x4 picker group matrix instantly
                        if (emotesActive) {
                            fEmoticonGrid->Show();
                        } else {
                            fEmoticonGrid->Hide();
                        }

                        // Force layout engine to re-flow text controls over collapsed areas
                        this->InvalidateLayout(true);
                    }
                    // --- END OF PICKER PANEL RE-FLOW PATCH ---
                    
                    // Force the standard scrollbar viewport directly back down to the bottom
                    if (!cfg.useCustomDrawFunction) {
                        int32 newLength = fChatLog->TextLength();
                        fChatLog->Select(newLength, newLength);
                        fChatLog->ScrollToSelection();
                    } else if (fCustomChatLog != nullptr) {
                        fCustomChatLog->Invalidate();
                    }
                }
            }
            break;
        }





            case MSG_CONNECT_CUSTOM_SERVER: { 
                ServerTreeItem* customNode = nullptr;
                if (message->FindPointer("server_item", (void**)&customNode) == B_OK && customNode != nullptr) {
                    ConnectToServer(customNode);
                }
                break;
            }        	
        	
            case MSG_CONNECT_LIBERA:
                ConnectToServer(fLiberaNode);
                break;

            case MSG_CONNECT_OFTC:
                ConnectToServer(fOftcNode);
                break;

            case MSG_START_SIRC:
                ConnectToServer(fOftcNode); 
                break;

        case MSG_SEND_MESSAGE: {
            BString text = fInputControl->Text();
            if (text.Length() > 0) {
                BString activeTarget = "";
                ServerTreeItem* contextServer = nullptr;
                bool isServerLogTab = false; // Tracks if we are looking at a server status feed

                int32 selectedIdx = fChannelTree->CurrentSelection();
                if (selectedIdx >= 0) {
                    BStringItem* selectedItem = dynamic_cast<BStringItem*>(fChannelTree->ItemAt(selectedIdx));
                    if (selectedItem != nullptr) {
                        BString itemText(selectedItem->Text());
                        
                        // --- Strip out dynamic activity suffix tags before matching IRC parameters ---
                        int32 tagPos = itemText.FindFirst(" [");
                        if (tagPos != B_ERROR) itemText.Truncate(tagPos);

                        // Extract custom class types directly from the selected sidebar tree item
                        ChannelTreeItem* chanItem = dynamic_cast<ChannelTreeItem*>(selectedItem);
                        ServerTreeItem* servItem = dynamic_cast<ServerTreeItem*>(selectedItem);

                        if (chanItem != nullptr) {
                            // CONDITION 1: It is either a #channel OR a private message query leaf node
                            activeTarget = itemText; 
                            isServerLogTab = false; // Private chats are NOT server status logs!

                            // Climb the tree safely to find the parent server connection context pointer
                            BListItem* parentItem = fChannelTree->Superitem(selectedItem);
                            if (parentItem != nullptr) {
                                contextServer = dynamic_cast<ServerTreeItem*>(parentItem);
                            }
                        } else if (servItem != nullptr || itemText.FindFirst(".") != B_ERROR) {

                            activeTarget = ""; 
                            isServerLogTab = true; // This IS a server status log view node!
                            contextServer = servItem;

                            // Fallback safety if checking old dot-notated server strings
                            if (contextServer == nullptr) {
                                BListItem* parentItem = fChannelTree->Superitem(selectedItem);
                                if (parentItem != nullptr) {
                                    contextServer = dynamic_cast<ServerTreeItem*>(parentItem);
                                }
                            }
                        }
                    }
                }

                
                // 1. Fallback safety constraint mapping
                if (contextServer == nullptr) {
                    contextServer = (fCurrentServerNode != nullptr) ? fCurrentServerNode : fLiberaNode;
                }
                
                // 2. Dynamic lookup matching from our socket array map
                BSecureSocket* activeSocket = nullptr;
                if (fServerSockets.count(contextServer) > 0) {
                    activeSocket = fServerSockets[contextServer];
                } else {
                    activeSocket = (contextServer == fOftcNode) ? fOftcSocket : fLiberaSocket;
                }
                
                if (activeSocket != nullptr) {
                    BString rawPayload;
                    
                    // =========================================================================
                    // SLASH COMMAND INTERPRETER ENGINE
                    // =========================================================================
                    if (text.StartsWith("/")) {
                        BString commandLine = text;
                        commandLine.Remove(0, 1); // strip the '/'                            
                        
                        if (commandLine.ICompare("clear", 5) == 0) {
                            if (fActiveBufferItem != nullptr) {
                                fTextBuffers[fActiveBufferItem] = "";
                                fChatLog->SetText("");
                                
                                // --- Clear your custom text row canvas vectors instantly ---
                                if (fCustomChatLog != nullptr) {
                                    fCustomChatLog->ClearAllLines();
                                }
                            }
                        }
                        
                        else if (commandLine.ICompare("me ", 3) == 0) {
                            commandLine.Remove(0, 3); // strip "me " keyword and space
                            
                            if (activeTarget.Length() > 0 && commandLine.Length() > 0) {
                                rawPayload << "PRIVMSG " << activeTarget << " :" "\x01" "ACTION " << commandLine << "\x01\r\n";
                                
                                BString timestampPrefix = "";
                                bigtime_t currentTime = real_time_clock_usecs();
                                time_t rawTime = (time_t)(currentTime / 1000000);
                                struct tm* timeInfo = localtime(&rawTime);
                                if (timeInfo != nullptr) {
                                    char timeBuffer[32];
                                    strftime(timeBuffer, sizeof(timeBuffer), "[%H:%M] ", timeInfo);
                                    timestampPrefix = timeBuffer;
                                }

                                BString echoStr;
                                echoStr << timestampPrefix << "* " << fMyNick << " " << commandLine << "\n";
                                LogToItemBuffer(fActiveBufferItem, echoStr);
                            } else {
                                BString warning = "System Error: You can only use /me actions inside a channel room leaf node.\n";
                                LogToItemBuffer(fActiveBufferItem, warning);
                            }
                        }
                        
                        else if (commandLine.ICompare("topic ", 6) == 0) {
                            commandLine.Remove(0, 6); // strip "topic " keyword and space
                            commandLine.Trim();

                            if (activeTarget.StartsWith("#")) {
                                if (commandLine.Length() > 0) {
                                    // Construct and send the standard IRC TOPIC payload format
                                    rawPayload << "TOPIC " << activeTarget << " :" << commandLine << "\r\n";
                                } else {
                                    BString warning = "Usage: /topic <New channel topic content string>\n";
                                    LogToItemBuffer(fActiveBufferItem, warning);
                                }
                            } else {
                                BString warning = "System Error: You can only set a topic inside a channel room leaf node.\n";
                                LogToItemBuffer(fActiveBufferItem, warning);
                            }
                        }

                        else if (commandLine.ICompare("msg ", 4) == 0) {
                            commandLine.Remove(0, 4); // strip "msg " keyword
                            int32 firstSpace = commandLine.FindFirst(" ");
                            
                            if (firstSpace != B_ERROR) {
                                BString dmTarget, msgBody;
                                commandLine.CopyInto(dmTarget, 0, firstSpace);
                                commandLine.CopyInto(msgBody, firstSpace + 1, commandLine.Length() - (firstSpace + 1));
                                
                                rawPayload << "PRIVMSG " << dmTarget << " :" << msgBody << "\r\n";
                                
                                BString echoStr;
                                echoStr << "-> To " << dmTarget << ": " << msgBody << "\n";
                                LogToItemBuffer(fActiveBufferItem, echoStr);
                            }
                        } else if (commandLine.ICompare("list", 4) == 0) {
  
                            bool windowIsValid = false;
                            if (fActiveListWindow != nullptr) {
                                BMessenger messenger(fActiveListWindow);
                                if (messenger.IsValid()) {
                                    windowIsValid = true;
                                } else {
                                    fActiveListWindow = nullptr;
                                }
                            }

                            if (fActiveListWindow == nullptr || !windowIsValid) {
                                fActiveListWindow = new IRCChannelListWindow(this, activeSocket, contextServer);
                                fActiveListWindow->Show();
                            } else {
                                if (fActiveListWindow->Lock()) {
                                    fActiveListWindow->Activate(true);
                                    fActiveListWindow->Unlock();
                                }
                            }
                        } else {
                            rawPayload << commandLine << "\r\n";
                        }
                    } else {
                        // =========================================================================
                        // REGULAR CHAT TEXT ROUTING ENGINE (PLAIN CHAT SUBMISSION)
                        // =========================================================================
                        
                        if (isServerLogTab) {
                            BString warning = "System Error: Use slash commands (like /JOIN) when typing inside the server status log.\n";
                            LogToItemBuffer(fActiveBufferItem, warning);
                        } else if (activeTarget.Length() > 0) {
                            // Construct the raw outgoing socket network string package stream
                            rawPayload << "PRIVMSG " << activeTarget << " :" << text << "\r\n";
                            
                            // Generate localized chat timestamp prefix bounds
                            BString timestampPrefix = "";
                            bigtime_t currentTime = real_time_clock_usecs();
                            time_t rawTime = (time_t)(currentTime / 1000000);
                            struct tm* timeInfo = localtime(&rawTime);
                            if (timeInfo != nullptr) {
                                char timeBuffer[32];
                                strftime(timeBuffer, sizeof(timeBuffer), "[%H:%M] ", timeInfo);
                                timestampPrefix = timeBuffer;
                            }


                            BString echoStr;
                            echoStr << timestampPrefix << "<" << fMyNick << "> " << text << "\n";
                            LogToItemBuffer(fActiveBufferItem, echoStr);
                        }
                    }

                    if (rawPayload.Length() > 0) {
                        activeSocket->Write(rawPayload.String(), rawPayload.Length());
                        if (cfg.debugEnable) {
                            LogDebugStream(contextServer->Text(), "OUTGOING", rawPayload.String(), rawPayload.Length());
                        }
                    }
                }
            }
            fInputControl->SetText("");
            fInputControl->MakeFocus(true);
            break;
        }




        case MSG_IRC_RECEIVED: {
            BString rawLine;
            // 1. Intercept incoming raw text payload line
            if (message->FindString("text", &rawLine) == B_OK) {
                
                // 2. Declare a local, scope-isolated node context variable.
                // This eliminates cross-talk when multiple network threads drop lines simultaneously.
                void* nodePtr = nullptr;
                ServerTreeItem* targetServerNode = nullptr;

                if (message->FindPointer("server_node", (void**)&nodePtr) == B_OK && nodePtr != nullptr) {
                    targetServerNode = static_cast<ServerTreeItem*>(nodePtr);
                    
                    // Optional: Synchronize your fallback tracker safely if needed elsewhere
                    fCurrentServerNode = targetServerNode; 
                } else {
                    // Fallback to absolute baseline if network metadata thread signatures fail
                    targetServerNode = fCurrentServerNode != nullptr ? fCurrentServerNode : fLiberaNode;
                }
                
                // 3. Forward the scope-isolated immutable context directly into parsing architecture
                ParseAndDisplayIRC(rawLine, targetServerNode);
            }
            break;
        }




            default:
                BWindow::MessageReceived(message);
                break;
        }
    }


private:

    BOutlineListView* fChannelTree;
    ServerTreeItem*   fLiberaNode;
    ServerTreeItem*   fOftcNode;
    ServerTreeItem*   fCurrentServerNode; 
    BString           fMyNick;
    BStringItem*      fActiveBufferItem;
    
    std::map<BStringItem*, BString> fTextBuffers;
    std::map<BStringItem*, BString> fChannelTopics;
    
    //std::map<BStringItem*, BObjectList<BStringItem, true>*> fChannelUsers;    
    std::map<BStringItem*, BObjectList<UserListItem, true>*> fChannelUsers;
    std::map<BStringItem*, bigtime_t> fLastTimestampTime;
    std::map<ServerTreeItem*, thread_id> fServerThreads;
    std::map<ServerTreeItem*, BSecureSocket*> fServerSockets;
	std::map<ServerTreeItem*, int32> fNickAttempts;

	BTextControl*     fTopicView;
    BTextView*        fChatLog;
    BListView*        fUserList;
    BTextControl*     fInputControl;
    BButton*          fConnectButton;
    
    thread_id         fLiberaThread;
    thread_id         fOftcThread;
    BSecureSocket*    fLiberaSocket;
    BSecureSocket*    fOftcSocket;
    
    IRCChannelListWindow* fActiveListWindow;

    BMenuField*         fServerListFontMenu;
    BMenuField*         fChatLogFontMenu;
    BMenuField*         fUserListFontMenu;

    BMenuField*         CreateFontMenu(const char* label, int32 currentSize);
	BCheckBox*          fHideStatusCheck;	

    BView*           fChatContainer; 
    CustomChatView*  fCustomChatLog; 
    bool fIsLoadingHistory = false; 
	BGridView*    fEmoticonGrid;

}; 

class Cricket : public BApplication {
public:
    Cricket() : BApplication("application/x-vnd.cricket") {}
    void ReadyToRun() override {
        CricketWindow* window = new CricketWindow();
        window->Show();
    }
};

int main() {
    Cricket app;
    app.Run();
    return 0;
}
