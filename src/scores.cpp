#include "scores.hpp"
#include <algorithm>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <sstream>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace bomber {
// [01] Validate untrusted local records; damaged rows do not crash startup.
static bool valid(const RankEntry& e) {
    if(e.score<0||e.score>1000000000||e.stage!=3||e.players<1||e.players>2||e.clearedAt.size()!=19)return false;
    const auto& time=e.clearedAt;
    for(int i=0;i<19;++i) {
        if(i==4||i==7) {if(time[i]!='-')return false;}
        else if(i==10) {if(time[i]!=' ')return false;}
        else if(i==13||i==16) {if(time[i]!=':')return false;}
        else if(time[i]<'0'||time[i]>'9')return false;
    }
    const int hour=(time[11]-'0')*10+time[12]-'0';
    const int minute=(time[14]-'0')*10+time[15]-'0';
    const int second=(time[17]-'0')*10+time[18]-'0';
    return hour<24&&minute<60&&second<60;
}
static void sortScores(std::vector<RankEntry>& rows) {
    std::stable_sort(rows.begin(),rows.end(),[](const RankEntry& a,const RankEntry& b) {
        return a.score>b.score;
    });
    if(rows.size()>10)rows.resize(10);
}
std::vector<RankEntry> readScores(const std::filesystem::path& path) {
    std::ifstream file(path);std::vector<RankEntry> rows;std::string line;
    while(std::getline(file,line)) {
        RankEntry e;std::istringstream in(line);std::string date,time,extra;
        // Date-only records cannot distinguish a stage-three loss from a clear.
        if(in>>e.score>>e.stage>>e.players>>date>>time && !(in>>extra)) {
            e.clearedAt=date+" "+time;
            if(valid(e))rows.push_back(e);
        }
        if(rows.size()>1000)break;
    }
    sortScores(rows);return rows;
}

// [02] Write-then-replace: a failed write does not truncate the old scoreboard.
bool saveScore(const std::filesystem::path& path,RankEntry entry) {
    if(!valid(entry))return false;
    auto rows=readScores(path);rows.push_back(entry);sortScores(rows);
    auto temporary=path;temporary += ".tmp";
    {
        std::ofstream out(temporary,std::ios::trunc);
        if(!out)return false;
        for(const auto& e:rows)out<<e.score<<'\t'<<e.stage<<'\t'<<e.players<<'\t'<<e.clearedAt<<'\n';
        out.flush();if(!out)return false;
    }
#ifdef _WIN32
    return MoveFileExW(temporary.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0;
#else
    std::error_code error;std::filesystem::rename(temporary,path,error);return !error;
#endif
}
std::string localTimestamp() {
    std::time_t now=std::time(nullptr);std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm,&now);
#else
    localtime_r(&now,&tm);
#endif
    std::ostringstream out;out<<std::put_time(&tm,"%Y-%m-%d %H:%M:%S");return out.str();
}
} // namespace bomber
