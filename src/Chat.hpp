#pragma once
#include <deque>
#include <functional>
#include <string>
#include <vector>

struct ChatLine { std::string text; double time=0; bool isSystem=false; };

// All singleplayer commands + a simple chat log.
class Chat {
public:
    static constexpr int kMaxHistory = 100;

    // Called every frame from the game; returns the lines to show on screen
    const std::deque<ChatLine>& lines() const { return m_lines; }
    bool isOpen()  const { return m_open; }
    void open()  { m_open=true;  m_input.clear(); m_histIdx=-1; }
    void close() { m_open=false; m_input.clear(); }
    std::string& input() { return m_input; }
    const std::string& input() const { return m_input; }
    void addChar(char c) { if(m_input.size()<256) m_input+=c; }
    void backspace() { if(!m_input.empty()) m_input.pop_back(); }
    void histUp();
    void histDown();

    // submit the current input; returns false if it was a command (don't echo to chat)
    bool submit(double now);    // triggers m_cmdHandler if set
    void say(const std::string& text, double now);
    void sys(const std::string& text, double now);  // system/command feedback

    // Set this before submit() calls arrive
    std::function<bool(const std::vector<std::string>&)> onCommand;  // args[0]=cmd; returns true=handled

private:
    std::deque<ChatLine> m_lines;
    std::string m_input;
    std::deque<std::string> m_history;
    int m_histIdx = -1;
    bool m_open = false;
};
