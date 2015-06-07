#include <string>
#include <iostream>
#include <fstream>
#include <algorithm> 
#include <functional> 
#include <cctype>
#include <locale>

#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "UdiRootConfig.hh"

using namespace std;


static inline string& ltrim(string& s) {
    s.erase(s.begin(), find_if(s.begin(), s.end(), not1(ptr_fun<int, int>(isspace))));
    return s;
}

static inline string& rtrim(string& s) {
    s.erase(find_if(s.rbegin(), s.rend(), not1(ptr_fun<int, int>(isspace))).base(), s.end());
    return s;
}

static inline string& trim(string& s) {
    return ltrim(rtrim(s));
}

UdiRootConfig::UdiRootConfig() {
    parse("test.conf");
}

void UdiRootConfig::parse(const string& configFile) {
    ifstream input;
    size_t loc;
    string line;

    input.open(configFile.c_str(), ios::in);
    while (input.good()) {
        getline(input, line);
        if (!input.good()) break;

        loc = line.find('=');
        if (loc == string::npos) continue;

        string key = line.substr(0, loc);
        string value = line.substr(loc+1);
        trim(key);
        trim(value);

        if (key == "udiMount") udiMountPoint = value;
        if (key == "loopMount") loopMountPoint = value;
        if (key == "imagePath") imageBasePath = value;
        if (key == "udiRootPath") udiRootPath = value;
        if (key == "udiRootSiteInclude") udiRootSiteInclude = value;
        if (key == "sshPath") sshPath = value;
        if (key == "etcPath") etcPath = value;
        if (key == "kmodBasePath") kmodBasePath = value;
        if (key == "kmodCacheFile") kmodCacheFile = value;
        if (key == "siteFs") {
            size_t last_loc = 0;
            while ((loc = value.find(' ', last_loc)) != string::npos) {
                if (loc == last_loc || loc - last_loc == 1) {
                    value = value.substr(loc+1);
                    last_loc = 0;
                    continue;
                }
                siteFs.push_back(value.substr(last_loc, loc - last_loc));
                last_loc = loc;
            }
            siteFs.push_back(value.substr(last_loc));
            loc = line.find(" ");
        }
        if (key == "imagegw_host") imageGwHost = value;
        if (key == "imagegw_port") imageGwPort = atoi(value.c_str());
        if (key == "batchType") batchType = value;
        if (key == "system") system = value;
        if (key == "nodeContextPrefix") nodeContextPrefix = value;

    }
}

bool UdiRootConfig::validateConfigFile() {
    if (configFile.size() == 0) {
        return false;
    }

    struct 


}

bool UdiRootConfig::validateSiteIncludeFile() {
}

bool UdiRootConfig::validateKernelModulePath() {
}

ostream& operator<<(ostream& os, UdiRootConfig& config) {
    os << "##### UDIROOT CONFIGURATION #####" << endl;
    os << "udiMountPoint: " << config.udiMountPoint << endl;
    os << "loopMountPoint: " << config.loopMountPoint << endl;
    os << "imageBasePath:  " << config.imageBasePath << endl;
    os << "udiRootPath:    " << config.udiRootPath << endl;
    os << "udiRootSiteInclude: " << config.udiRootSiteInclude << endl;
    os << "sshPath: " << config.sshPath << endl;
    os << "etcPath: " << config.etcPath << endl;
    os << "kmodBasePath: " << config.kmodBasePath << endl;
    os << "kmodCacheFile: " << config.kmodCacheFile << endl;
    os << "siteFs: " << config.siteFs.size() << " entries" << endl;
    for (size_t idx = 0; idx < config.siteFs.size(); idx++) {
        os << "    " << config.siteFs[idx] << endl;
    }
    os << "imageGwHost: " << config.imageGwHost << endl;
    os << "imageGwPort: " << config.imageGwPort << endl;
    os << "batchType: " << config.batchType << endl;
    os << "nodeContextPrefix: " << config.nodeContextPrefix << endl;
    os << "######";
    return os;
}
