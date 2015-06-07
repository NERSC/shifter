/* Shifter, Copyright (c) 2015, The Regents of the University of California,
## through Lawrence Berkeley National Laboratory (subject to receipt of any
## required approvals from the U.S. Dept. of Energy).  All rights reserved.
## 
## Redistribution and use in source and binary forms, with or without
## modification, are permitted provided that the following conditions are met:
##  1. Redistributions of source code must retain the above copyright notice,
##     this list of conditions and the following disclaimer.
##  2. Redistributions in binary form must reproduce the above copyright notice,
##     this list of conditions and the following disclaimer in the documentation
##     and/or other materials provided with the distribution.
##  3. Neither the name of the University of California, Lawrence Berkeley
##     National Laboratory, U.S. Dept. of Energy nor the names of its
##     contributors may be used to endorse or promote products derived from this
##     software without specific prior written permission.
## 
## THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
## AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
## IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
## ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
## LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
## CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
## SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
## INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
## CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
## ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
## POSSIBILITY OF SUCH DAMAGE.
##  
## You are under no obligation whatsoever to provide any bug fixes, patches, or
## upgrades to the features, functionality or performance of the source code
## ("Enhancements") to anyone; however, if you choose to make your Enhancements
## available either publicly, or directly to Lawrence Berkeley National
## Laboratory, without imposing a separate written license agreement for such
## Enhancements, then you hereby grant the following license: a  non-exclusive,
## royalty-free perpetual license to install, use, modify, prepare derivative
## works, incorporate into other computer software, distribute, and sublicense
## such enhancements or derivative works thereof, in binary and source code
## form.
*/
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <vector>
#include <string>
#include <iostream>

using namespace std;

/*
   udiMount=/var/udiMount
   loopMount=/var/loopUdiMount
   chosPath=/scratch2/shifter/chos
   dockerPath=/scratch2/scratchdirs/craydock/docker
   udiRootPath="$context//opt/nersc/udiRoot/20150520_b2084ecb2593f1c5b33c2f36a726b3e5708eecf5"
   udiRootIncludeFile="$context//opt/nersc/udiRoot/20150520_b2084ecb2593f1c5b33c2f36a726b3e5708eecf5/etc/udiRoot.include"
   mapPath=$udiRootPath/fsMap.conf
   sshPath=$udiRootPath/sshd
   etcDir=$udiRootPath/etc_files
   kmodDir=$udiRootPath/kmod/$( uname -r )
   kmodCache=/tmp/udiRootLoadedModules.txt
   siteFs="scratch1 scratch2 scratch3 global/u1 global/u2 global/project global/syscom global/common global/scratch2 var/opt/cray/alps/spool"
   dockergw_host=128.55.50.83
   dockergw_port=7777
   batchType=torque
   system=edison
*/

class UdiRootConfig {
    friend ostream& operator<<(ostream&, UdiRootConfig&);

    private:
    string nodeContextPrefix;
    string udiMountPoint;
    string loopMountPoint;
    string batchType;
    string system;
    string imageBasePath;
    string udiRootPath;
    string udiRootSiteInclude;
    string sshPath;
    string etcPath;
    string kmodBasePath;
    string kmodCacheFile;
    string imageGwHost;
    int imageGwPort;
    vector<string> siteFs;

    void parse(const string& configFile);

    public:
    UdiRootConfig();
    bool validateConfigFile();
    bool validateSiteIncludeFile();
    bool validateKernelModulePath();

};

ostream& operator<<(ostream& os, UdiRootConfig& config);
