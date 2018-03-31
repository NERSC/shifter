#!/usr/bin/python
## Shifter, Copyright (c) 2016, The Regents of the University of California,
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

import os
import sys
import subprocess
import re
import shutil

so_skip_patterns = [
    r'^ld-linux-.*',
    r'^libm\.so.*',
    r'^librt\.so.*',
    r'^libpthread\.so.*',
    r'^libdl\.so.*',
    r'^libc[\.-]*so.*',
    r'^libstdc\+\+\.so.*'
]

def module_list():
    pfp = subprocess.Popen(['modulecmd','python','-t','list'], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    stdout,stderr = pfp.communicate()
    modules = stderr.strip().split('\n')
    return modules[1:]

def module_swap(first, second=None):
    cmd = ['modulecmd','python','swap',first]
    if second:
        cmd.append(second)
    pfp = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    stdout,stderr = pfp.communicate()
    if pfp.returncode == 0:
        exec(stdout)

def module_load(target):
    cmd = ['modulecmd','python','load',target]
    pfp = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    stdout,stderr = pfp.communicate()
    if pfp.returncode == 0:
        exec(stdout)


def load_PrgEnv(family):
    ## determine if a PrgEnv is already loaded
    iterable = (x for x in  module_list() if x.startswith('PrgEnv-'))
    try:
        PrgEnv = iterable.next()
    except StopIteration:
    	PrgEnv = None

    ## if yes, swap to the target, otherwise load it
    if PrgEnv:
        module_swap(PrgEnv, 'PrgEnv-%s' % family)
    else:
        module_load('PrgEnv-%s' % family)

def load_mpich(name):
    ## determine if a cray-mpich or cray-mpich-abi are already loaded
    iterable = (x for x in module_list() if x.startswith('cray-mpich'))
    try:
        loaded = iterable.next()
    except StopIteration:
        loaded = None
  
    ## if loaded, switch to name
    if loaded:
        module_swap(loaded, name)
    else:
        module_load(name)

def fix_module_path():
    try:
        paths = os.environ['MODULEPATH'].split(':')
        paths = [ x for x in paths if x.startswith('/opt/') ]
        os.environ['MODULEPATH'] = ':'.join(paths)
    except:
        pass

def get_module_libpaths(name):
    pfp = subprocess.Popen(['modulecmd','python','show',name], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    stdout,stderr = pfp.communicate()
    lines = stderr.strip().split('\n')
    lines = [ x for x in lines if x.find('CRAY_LD_LIBRARY_PATH') != -1 ]
    paths = []
    for line in lines:
        t_path = line.split()[2]
        paths.extend(t_path.split(':'))
    return paths

## discover libraries within provided paths, identify "real" files and symlink
## structure within the libpath
def getlibs(libpaths, files, isdep=False):
    for path in libpaths:
        if os.path.isdir(path):
            lfiles = [x for x in os.listdir(path) if x.find('.so') != -1]
        elif path.find('.so') != -1:
            path,fname = os.path.split(path)
            print path, fname
            lfiles = [fname]

        for fname in lfiles:
            fullpath = '%s/%s' % (path, fname)
            realpath = os.path.realpath(fullpath)
            basepath,libname = os.path.split(realpath)
            if libname not in files:
                deps = read_libdeps(realpath, so_skip_patterns)
                files[libname] = {
                    'type': 'file',
                    'path': realpath,
                    'deps': deps,
                    'isdep': isdep
                }
            if fname != libname and fname not in files:
                files[fname] = {
                    'type': 'link',
                    'name': fname,
                    'target': libname,
                    'isdep': isdep
                }
    return files

## read library dependency structure free of environmental constraints
def read_libdeps(sopath, skippatterns):
    cmd = ['patchelf', '--print-needed', sopath]
    pfp = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    stdout,stderr = pfp.communicate()
    if pfp.returncode != 0:
        print "Failed to read library deps for %s" % sopath
        return None

    deps = stdout.strip().split('\n')
    keep = []
    for dep in deps:
        for skip in skippatterns:
            if re.match(skip, dep):
                break
        else:
            keep.append(dep)
    return keep

def resolvedeps(files):
    libfiles = [ x for x in files if files[x]['type'] == 'file' ]
    lastlen = len(files.keys())
    for libfile in libfiles:
        pfp = subprocess.Popen(['ldd', files[libfile]['path']], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        stdout,stderr = pfp.communicate()
        lines = [ x.strip() for x in stdout.strip().split('\n') ]
        libs = [ x.split() for x in lines ]
        libs = [ (x[0],x[2]) for x in libs if x[0] in files[libfile]['deps'] ]
        for lib in libs:
            if lib[0] in files:
                continue
            files = getlibs([lib[1]], files, isdep=True)
            if lib[0] not in files:
                files[lib[0]] = {
                    'type': 'link',
                    'name': lib[0],
                    'target': os.path.split(lib[1])[1],
                    'isdep': True,
                }
    if len(files.keys()) != lastlen:
        files = resolvedeps(files)
    return files

def module_avail(name):
    pfp = subprocess.Popen(['modulecmd', 'python', '-t', 'avail', name], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    stdout,stderr = pfp.communicate()
    lines = stderr.strip().split('\n')
    return [ x for x in lines if not x.endswith(':') ]
            
if __name__ == "__main__":
    files = {}

    cmd = ['patchelf', '--help']
    try:
        pfp = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        pfp.communicate()
    except:
        print "Failed to find patchelf command.  Please get patchelf into PATH before proceeding"
        sys.exit(1)

    if len(sys.argv) != 2:
        print "No destination path specified"
        sys.exit(1)


    copyTgtPath = sys.argv[1]
    destTgtPath = '/opt/udiImage/modules/mpich/lib64'

    ## only consider current cray/system modules
    fix_module_path()

    ## set the programming environments and modules we're interested in
    prgenv = [
        ('gnu', {'compiler': 'gcc', 'modules': ('cray-mpich-abi','cray-mpich')}),
        ('intel', {'compiler': 'intel', 'modules': ('cray-mpich-abi','cray-mpich')}),
        ('cray', {'compiler': 'cce', 'modules': ('cray-mpich',)})
    ]
 
    ## find all the shared libraries and resolve dependencies for each of the pe/module combos
    for pe,pedata in prgenv:
        load_PrgEnv(pe)
        comp_ver = [ pedata['compiler'] ]
        comp_ver.extend(module_avail(pedata['compiler']))

        for comp in comp_ver:
            if comp.find('(default)') != -1:
                continue ## redundant default
            module_swap(pedata['compiler'], comp)

            for mpich_module in pedata['modules']:
                load_mpich(mpich_module)
                libpaths = get_module_libpaths(mpich_module)
                files = getlibs(libpaths, files)
                files = resolvedeps(files)

    ## a few are not detected by above strategy
    for mod in ['wlm_detect','alps','rca']:
        libpaths = get_module_libpaths(mod)
        files = getlibs(libpaths, files, isdep=True)
        files = resolvedeps(files)

    os.mkdir(copyTgtPath)
    os.mkdir('%s/%s' % (copyTgtPath, 'dep'))
    destTgtDepPath = '%s/%s' % (destTgtPath, 'dep')


    for fname in files:
        if files[fname]['type'] == 'file':
            ## copy lib to appropriate dest path
            tgtPath = '%s/%s%s' % (copyTgtPath, '%s' % ('dep/' if files[fname]['isdep'] else ''), fname)
            shutil.copy2(files[fname]['path'], tgtPath)

            ## modify copied library to funnel links from tgt libs into dep paths
            cmd = ['patchelf','--set-rpath', destTgtDepPath, tgtPath]
            subprocess.call(cmd)

        if files[fname]['type'] == 'link':
            destPath = '%s/%s%s' % (copyTgtPath, '%s' % ('dep/' if files[fname]['isdep'] else ''), fname)
            print fname, copyTgtPath
            print "src: %s, dest: %s" % (files[fname]['target'], destPath)
            os.symlink(files[fname]['target'], destPath)
        

