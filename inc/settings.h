#pragma once
#include <string>
#include <tuple>
#include <string_view>
#include <unordered_map>
#include "utils.h"

constexpr int hashMin = 1;
constexpr int hashDefault = 64;
constexpr int hashMax = 1048576; // 2 ^ 20
constexpr int threadsMin = 1;
constexpr int threadsMax = 1024;
constexpr int threadsDefault = 1;

constexpr bool fischerChessDefault = false;
constexpr bool showWDLDefault = true;

namespace settings {
    inline int hash = hashDefault;
    inline int threads = threadsDefault;
    inline bool showWDL = showWDLDefault;

    inline bool useUCI = false;
    inline bool fischerChess = fischerChessDefault;
}

struct tunable {
    const std::string_view name;
    const int defaultval;
    const int min;
    const int max;
    const int step;
    int val = defaultval;
};

inline std::unordered_map<std::string_view, tunable> tunable_list;

#define add_tunable(name, def, MIN, MAX, STEP) \
    inline tunable name = tunable {#name, def, MIN, MAX, STEP}; \
    namespace internal {\
        struct registerTunable_##name {\
            registerTunable_##name() { tunable_list.emplace(name.name, name); }\
        }; \
        inline registerTunable_##name RegisterTunable_##name; \
    } \
    inline int tune_##name() { return name.val; }

inline void debugTunable() {
    for (const auto& [name, param] : tunable_list) {
        cout<<"setting: "<<param.name<<", default val: "<<param.defaultval<<", minval: "<<param.min<<", maxval: "<<param.max<<"\n";
    }
}

inline void generateOpenBenchString() {
    cout << "-------------------------------------------" << endl;
        cout << "Currently " << tunable_list.size() << " tunable values are plugged in:" << endl;
        cout << "-------------------------------------------" << endl;
        for (const auto& [name, param] : tunable_list) {
                cout << param.name << ", ";
                cout << "int" << ", ";
                cout << param.defaultval << ", ";
                cout << param.min << ", ";
                cout << param.max << ", ";
                cout << param.step << ", ";
                cout << "0.002 " << endl;
        }
        cout << "-------------------------------------------" << endl;
}

inline bool isTuning() {
    return tunable_list.size() > 0;
}

inline bool hasParams(const std::string name) {
    return (tunable_list.find(name) != tunable_list.end());
}

inline void setParam(const std::string name, const int val) {
    tunable_list.at(name).val = val;
}