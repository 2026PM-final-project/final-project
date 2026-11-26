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
    if(e.score<0||e.score>1000000000||e.stage!=3||e.players<1||e.players>2||
       e.clearedAt.size()!=(e.legacyConfirmed?10u:19u))return false;
    const auto& time=e.clearedAt;
    for(size_t i=0;i<time.size();++i) {
        if(i==4||i==7) {if(time[i]!='-')return false;}
        else if(i==10) {if(time[i]!=' ')return false;}
        else if(i==13||i==16) {if(time[i]!=':')return false;}
        else if(time[i]<'0'||time[i]>'9')return false;
    }
    auto two=[&](int at) {return (time[at]-'0')*10+time[at+1]-'0';};
    const int year=two(0)*100+two(2),month=two(5),day=two(8);
    if(year<1||month<1||month>12||day<1)return false;
    const int days[]={31,28,31,30,31,30,31,31,30,31,30,31};
    const bool leap=year%4==0&&(year%100!=0||year%400==0);
    return day<=days[month-1]+(month==2&&leap?1:0)&&
        (e.legacyConfirmed||(two(11)<24&&two(14)<60&&two(17)<60));
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
        // Only explicitly confirmed imports may omit their unavailable original time.
        if(in>>e.score>>e.stage>>e.players>>date>>time && !(in>>extra)) {
            e.legacyConfirmed=time=="LEGACY_CONFIRMED";
            e.clearedAt=e.legacyConfirmed?date:date+" "+time;
            if(valid(e))rows.push_back(e);
        }
        if(rows.size()>1000)break;
    }
    sortScores(rows);return rows;
}

// [02] Write-then-replace: a failed write does not truncate the old scoreboard.
bool saveScore(const std::filesystem::path& path,RankEntry entry) {
    if(!valid(entry))return false;
    if(!path.parent_path().empty()) {
        std::error_code error;std::filesystem::create_directories(path.parent_path(),error);
        if(error)return false;
    }
    auto rows=readScores(path);rows.push_back(entry);sortScores(rows);
    auto temporary=path;temporary += ".tmp";
    {
        std::ofstream out(temporary,std::ios::trunc);
        if(!out)return false;
        for(const auto& e:rows)out<<e.score<<'\t'<<e.stage<<'\t'<<e.players<<'\t'<<e.clearedAt
            <<(e.legacyConfirmed?"\tLEGACY_CONFIRMED":"")<<'\n';
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
std::filesystem::path scoreFileForExecutable(const std::filesystem::path& directory) {
    for(auto current=directory;!current.empty();) {
        std::error_code error;
        if(std::filesystem::is_regular_file(current/"Bomberman.sln",error)&&
           std::filesystem::is_regular_file(current/"CMakeLists.txt",error))
            return current/"data"/"scores.tsv";
        auto parent=current.parent_path();if(parent==current)break;current=parent;
    }
    return directory/"scores.tsv";
}
} // namespace bomber
