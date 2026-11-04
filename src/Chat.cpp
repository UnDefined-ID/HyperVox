#include "Chat.hpp"
#include <sstream>

void Chat::histUp() {
    if(m_history.empty()) return;
    if(m_histIdx<0) m_histIdx=0; else if(m_histIdx<(int)m_history.size()-1) m_histIdx++;
    m_input=m_history[m_histIdx];
}
void Chat::histDown() {
    if(m_histIdx<0) return;
    m_histIdx--; m_input=m_histIdx<0?std::string():m_history[m_histIdx];
}

bool Chat::submit(double now) {
    std::string line=m_input; m_input.clear(); m_histIdx=-1;
    if(line.empty()) return false;
    m_history.push_front(line);
    if(m_history.size()>50) m_history.pop_back();
    if(line[0]=='/') {
        // parse into tokens
        std::vector<std::string> args; std::istringstream ss(line.substr(1)); std::string tok;
        while(ss>>tok) args.push_back(tok);
        if(!args.empty()&&onCommand&&onCommand(args)) return false;
        sys("Unknown command: /"+(!args.empty()?args[0]:""), now);
        return false;
    }
    say(line, now); return true;
}
void Chat::say(const std::string& text, double now) {
    m_lines.push_front({text, now, false});
    if(m_lines.size()>kMaxHistory) m_lines.pop_back();
}
void Chat::sys(const std::string& text, double now) {
    m_lines.push_front({text, now, true});
    if(m_lines.size()>kMaxHistory) m_lines.pop_back();
}
