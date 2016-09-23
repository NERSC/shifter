/* Shifter, Copyright (c) 2016, The Regents of the University of California,
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

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include "PathList.h"

PathList *pathList_init(const char *path) {
    PathList *ret = NULL;
    size_t path_len = 0;
    char *buffer = NULL;
    char *search = NULL;
    char *search_sv = NULL;
    char *tgt = NULL;
    PathComponent *parent = NULL;

    if (path == NULL) {
        return NULL;
    }

    path_len = strlen(path);
    if (path_len == 0) return NULL;

    buffer = strdup(path);
    if (buffer == NULL) {
        return NULL;
    }

    ret = (PathList *) malloc(sizeof(PathList));
    if (ret == NULL) {
        free(buffer);
        return NULL;
    }
    ret->path = NULL;
    ret->relroot = NULL;
    ret->terminal = NULL;

    ret->absolute = path[0] == '/' ? 1 : 0;

    search = buffer;
    parent = NULL;
    while ((tgt = strtok_r(search, "/", &search_sv)) != NULL) {
        PathComponent *comp = NULL;
        search = NULL;

        /* first or repeated slash */
        if (strlen(tgt) == 0) {
            continue;
        }
        if (strcmp(tgt, ".") == 0) {
            continue;
        }

        comp = (PathComponent *) malloc(sizeof(PathComponent));
        comp->item = strdup(tgt);
        comp->child = NULL;
        comp->parent = parent;
        comp->list = ret;

        if (parent == NULL) {
            ret->path = comp;
        } else {
            parent->child = comp;
        }
        parent = comp;
        ret->terminal = comp;
    }

    free(buffer);
    pathList_resolve(ret);
    return ret;
}

int pathList_setRoot(PathList *path, const char *relroot) {
    PathList *root = pathList_init(relroot);
    PathComponent *rootptr = NULL;
    if (root == NULL || !(root->absolute)) {
        if (root != NULL) {
            pathList_free(root);
            root = NULL;
        }
        return -1;
    }
    if (root->path == NULL) {
        /* this means that relroot was effectively '/' */
        path->relroot = NULL;
    } else {
        rootptr = pathList_matchPartial(path, root);
        if (rootptr == NULL) {
            pathList_free(root);
            return -1;
        }
        path->relroot = rootptr;
    }
    pathList_free(root);
    return 0;
}

int pathList_append(PathList *base, const char *path) {
    PathList *newpath = pathList_init(path);
    PathComponent *ptr = NULL;

    if (newpath == NULL) {
        return -1;
    }
    if (base == NULL) {
        pathList_free(newpath);
        return -1;
    }

    if (base->terminal != NULL) {
        base->terminal->child = newpath->path;
        if (newpath->path != NULL) {
            newpath->path->parent = base->terminal;
        }
    } else {
        base->path = newpath->path;
    }
    base->terminal = newpath->terminal;
    newpath->path = NULL;
    newpath->terminal = NULL;
    pathList_free(newpath);

    for (ptr = base->path; ptr != NULL; ptr = ptr->child) {
        ptr->list = base;
    }

    pathList_resolve(base);
    return 0;
}

PathList *pathList_duplicate(PathList *src) {
    PathList *ret = NULL;
    PathComponent *rptr = NULL;
    PathComponent *wptr = NULL;
    PathComponent *wptr_parent = NULL;

    if (src == NULL) return NULL;

    ret = (PathList *) malloc(sizeof(PathList));
    if (ret == NULL) return NULL;

    ret->path = NULL;
    ret->relroot = NULL;
    ret->absolute = src->absolute;
    ret->terminal = NULL;

    for (rptr = src->path; rptr != NULL; rptr = rptr->child) {
        wptr = (PathComponent *) malloc(sizeof(PathComponent));
        wptr->list = ret;
        wptr->parent = wptr_parent;
        wptr->child = NULL;
        wptr->item = strdup(rptr->item);

        if (src->relroot == rptr) {
            ret->relroot = wptr;
        }

        if (wptr_parent == NULL) {
            ret->path = wptr;
        } else {
            wptr_parent->child = wptr;
        }
        wptr_parent = wptr;
        ret->terminal = wptr;
    }

    return ret;
}

PathList *pathList_symlinkResolve(PathList *base, const char *_symlink) {
    PathList *symlink = NULL;
    PathList *newpath = NULL;
    PathComponent *parent = NULL;
    PathComponent *ptr = NULL;

    if (base == NULL || _symlink == NULL) {
        return NULL;
    }
    symlink = pathList_init(_symlink);
    newpath = pathList_duplicate(base);

    if (symlink == NULL || newpath == NULL || newpath->absolute == 0) {
        goto _resolve_symlink_error;
    }

    /* find appropriate parent in newpath for symlink */
    if (symlink->absolute) {
        if (newpath->relroot) {
            parent = newpath->relroot;
        } else {
            parent = NULL;
        }
    } else {
        parent = newpath->terminal;
    }

    /* place symlink immediately under that parent (if possible) */
    if (parent == NULL) {
        pathList_freeComponents(newpath->path);
        newpath->path = symlink->path;
        newpath->relroot = NULL;
        newpath->terminal = symlink->terminal;
    } else {
        if (parent->child != NULL) {
            pathList_freeComponents(parent->child);
        }
        parent->child = symlink->path;
        newpath->terminal = symlink->terminal;

        if (parent == newpath->relroot) {
            parent->child->parent = parent->child;
        } else {
            parent->child->parent = parent;
        }
    }
    symlink->path = NULL;
    symlink->terminal = NULL;
    symlink->relroot = NULL;
    pathList_free(symlink);

    for (ptr = newpath->path; ptr != NULL; ptr = ptr->child) {
        ptr->list = newpath;
    }

    /* simplify the path (remove redundant relative components) */
    pathList_resolve(newpath);

    return newpath;

_resolve_symlink_error:
    if (symlink != NULL) {
        pathList_free(symlink);
        symlink = NULL;
    }
    if (newpath != NULL) {
        pathList_free(newpath);
        newpath = NULL;
    }
    return NULL;
}

void pathList_resolve(PathList *path) {
    PathComponent *curr = NULL;
    PathComponent *target = NULL;
    if (path == NULL || path->absolute == 0) return;

    curr = path->path;
    while (curr != NULL) {
        if (strcmp(curr->item, "..") != 0) {
            curr = curr->child;
            continue;
        }

        /* check boundary conditions and ensure not to traverse past root */
        if (curr->parent == NULL || path->path == curr) {
            /* /../ == / */
            path->path = curr->child;
            if (path->terminal == curr) {
                /* path is going away! */
                path->terminal = curr->child; /*should be NULL */
            }
            target = curr->child;
            pathList_freeComponent(curr);
            curr = target;
            continue; 
        }
        if (curr->parent == path->relroot) {
            /* cannot traverse above relroot */
            /* so delete just this entry */
            curr->parent->child = curr->child;
            if (curr->child != NULL) {
                curr->child->parent = curr->parent;
            }
            if (path->terminal == curr) {
                path->terminal = curr->parent;
            }
            target = curr->child;
            pathList_freeComponent(curr);
            curr = target;
            continue;
        }

        /* not bumping up against root, so need to consume this component and
         * its parent */
        if (curr->parent->parent == NULL || path->path == curr->parent) {
            path->path = curr->child;
            if (curr->child != NULL) {
                curr->child->parent = NULL;
            } else {
                path->terminal = NULL;
            }
            target = curr->child;
            pathList_freeComponent(curr->parent);
            pathList_freeComponent(curr);
            curr = target;
            continue;
        }

        /* removing two components from the middle of the chain */
        curr->parent->parent->child = curr->child;
        if (curr->child != NULL) {
            curr->child->parent = curr->parent->parent;
        } else {
            path->terminal = curr->parent->parent;
        }
        target = curr->child;
        pathList_freeComponent(curr->parent);
        pathList_freeComponent(curr);
        curr = target;
        continue;
    }
}

PathList *pathList_duplicatePartial(PathList *origpath, PathComponent *tohere) {
    PathList *ret = pathList_duplicate(origpath);
    PathComponent *lptr = NULL;
    PathComponent *rptr = NULL;
    PathComponent *trim = NULL;
    if (ret == NULL) return NULL;

    for (lptr = origpath->path, rptr = ret->path;
            lptr && rptr && lptr != tohere;
            lptr = lptr->child, rptr = rptr->child)
    {
        /* do nothing all logic in loop control */
    }

    if (lptr != tohere) {
        pathList_free(ret);
        return NULL;
    }
    ret->terminal = rptr;
    if (rptr != NULL) {
        trim = rptr->child;
        rptr->child = NULL;
        pathList_freeComponents(trim);
    } else {
        /* this is an exceptionally unlikely branch, but if it does happen,
         * this will keep the ret path consistent */
        if (ret->path != NULL) {
            pathList_freeComponents(ret->path);
        }
        ret->path = NULL;
        ret->terminal = NULL;
    }
    return ret;
}

PathList *pathList_commonPath(PathList *a, PathList *b) {
    PathComponent *aptr = NULL;
    PathComponent *bptr = NULL;
    PathList *ret = NULL;

    if (a == NULL || b == NULL) return NULL;
    if (a->absolute != b->absolute) return NULL;
    if (a->relroot != NULL && b->relroot == NULL) return NULL;
    if (a->relroot == NULL && b->relroot != NULL) return NULL;

    ret = (PathList *) malloc(sizeof(PathList));
    if (ret == NULL) return NULL;
    ret->absolute = a->absolute;
    ret->path = NULL;
    ret->relroot = NULL;
    ret->terminal = NULL;

    aptr = a->path;
    bptr = b->path;
    while (aptr && bptr) {
        PathComponent *newcomp = NULL;
        if (strcmp(aptr->item, bptr->item) != 0) {
            break;
        }

        newcomp = (PathComponent *) malloc(sizeof(PathComponent));
        newcomp->item = strdup(aptr->item);
        newcomp->parent = ret->terminal;
        if (ret->terminal != NULL) {
            ret->terminal->child = newcomp;
        }
        ret->terminal = newcomp;
        newcomp->list = ret;
        newcomp->child = NULL;

        if (ret->path == NULL) {
            ret->path = newcomp;
        }

        if ((aptr == a->relroot && bptr != b->relroot) ||
            (aptr != a->relroot && bptr == b->relroot))
        {
            pathList_free(ret);
            return NULL;
        }
        if (aptr == a->relroot && bptr == b->relroot) {
            ret->relroot = newcomp;
        }
        aptr = aptr->child;
        bptr = bptr->child;
    }
    return ret;
}

PathComponent *pathList_appendComponents(
        PathList *dest,
        PathList *src,
        PathComponent *comp)
{

    PathComponent *compPtr = comp;
    PathComponent *newComp = NULL;
    PathComponent *retComp = NULL;
    PathComponent *parent = NULL;
    if (dest == NULL || src == NULL || comp == NULL || comp->list != src) {
        return NULL;
    }

    parent = dest->terminal;
    while (compPtr) {
        newComp = (PathComponent *) malloc(sizeof(PathComponent));
        if (newComp == NULL) {
            if (retComp != NULL) {
                pathList_freeComponents(retComp);
                retComp = NULL;
            }
            return NULL;
        }
        newComp->item = strdup(compPtr->item);
        if (newComp->item == NULL) {
            if (retComp != NULL) {
                pathList_freeComponents(retComp);
            }
            pathList_freeComponent(newComp);
            return NULL;
        }
        newComp->list = dest;
        newComp->parent = parent;
        newComp->child = NULL;
        if (parent) {
            parent->child = newComp;
        }

        if (retComp == NULL) {
            retComp = newComp;
        }
        parent = newComp;
        compPtr = compPtr->child;
    }

    /* the new chain is already linked via parent to the dest Path, but now
     * need to finish the linkage since creation was successful */
    if (dest->path == NULL) {
        /* dest was an empty list? populate it! */
        dest->path = retComp;
    }

    /* link current terminal to head of new chain */
    if (dest->terminal != NULL) {
        dest->terminal->child = retComp;
    }

    /* link terminal to end of new chain */
    dest->terminal = newComp;
    return retComp;
}


void pathList_trimLast(PathList *path) {
    PathComponent *ptr = NULL;
    PathComponent *trim = NULL;

    if (path == NULL) return;
    trim = path->terminal;
    if (trim == NULL) return;

    ptr = trim->parent;
    if (ptr) {
        ptr->child = NULL;
    }
    if (path->path == path->terminal) {
        path->path = ptr;
    }
    path->terminal = ptr;
    pathList_freeComponent(trim);
}


PathComponent *pathList_matchPartial(PathList *fullpath, PathList *partial) {
    PathComponent *retptr = NULL;
    PathComponent *partialptr = NULL;
    if (fullpath == NULL || partial == NULL) return NULL;
    if (fullpath->absolute != partial->absolute) return NULL;

    retptr = fullpath->path;
    partialptr = partial->path;

    while (retptr && partialptr) {
        if (strcmp(retptr->item, partialptr->item) != 0) {
            return NULL;
        }
        retptr = retptr->child;
        partialptr = partialptr->child;
    }

    if (partialptr == NULL && retptr == NULL) {
        /* perfect match */
        return fullpath->terminal;
    }

    if (partialptr != NULL) {
        /* match was too short */
        return NULL;
    }

    /* above match overran */
    if (retptr != NULL) {
        return retptr->parent;
    }
    return NULL;
}

char *pathList_string(PathList *path) {
    return pathList_stringPartial(path, path->terminal);
}

char *pathList_stringPartial(PathList *path, PathComponent *pos) {
    char *ret = NULL;
    char *wptr = NULL;
    PathComponent *ptr = NULL;
    if (path == NULL) return NULL;
    if (pos == NULL && path->path != NULL) return NULL;
    if (pos != NULL && pos->list != path) return NULL;

    ret = (char *) malloc(sizeof(char) * PATH_MAX);
    memset(ret, 0, sizeof(char) * PATH_MAX);
    wptr = ret;

    if (ret == NULL) return NULL;

    for (ptr = path->path; ptr && ptr != pos->child; ptr = ptr->child) {
        size_t used = wptr - ret;
        size_t complen = strlen(ptr->item);
        int bytes = 0;
        if (used + complen + 2 >= PATH_MAX) {
            free(ret);
            return NULL;
        }

        bytes = snprintf(wptr, complen + 2, "%s%s", path->absolute ? "/" : "",
                ptr->item);

        if (bytes < 0 || bytes > complen + 1) {
            free(ret);
            return NULL;
        }
        wptr += bytes;
    }
    if (path->path == NULL && path->absolute) {
        int bytes = snprintf(ret, PATH_MAX, "/");
        if (bytes < 0 || bytes >= PATH_MAX) {
            free(ret);
            return NULL;
        }
    }
    return ret;
}

PathComponent *pathList_symlinkSubstitute(PathList *path,
        PathComponent *link, const char *linkVal)
{
    PathList *linkParent = NULL;
    PathList *origLinkPath = NULL;
    PathList *linkPath = NULL;
    PathList *commonPath = NULL;
    PathComponent *unchecked = NULL;

    if (path == NULL || path->path == NULL || link == NULL
            || link->list == NULL || linkVal == NULL) 
    {
        goto _symlink_sub_error;
    }
    origLinkPath = pathList_duplicatePartial(path, link);
    linkParent = pathList_duplicate(origLinkPath);
    pathList_trimLast(linkParent);
    if (linkParent == NULL) {
        goto _symlink_sub_error;
    }

    linkPath = pathList_symlinkResolve(linkParent, linkVal);
    if (linkPath == NULL) {
        goto _symlink_sub_error;
    }

    commonPath = pathList_commonPath(origLinkPath, linkPath);
    if (commonPath == NULL) {
        goto _symlink_sub_error;
    }

    unchecked = pathList_matchPartial(linkPath, commonPath);

    /* unchecked points to the last matching element between linkPath and
     * commonPath; if NULL, there are no matching portions; if equivalent
     * to the linkPath->terminal, then it must be a perfect match */
    int perfectMatch = 0;
    if (unchecked == NULL) {
        unchecked = linkPath->path;
    } else if (unchecked->child != NULL) {
        unchecked = unchecked->child;
    } else if (unchecked == linkPath->terminal) {
        perfectMatch = 1;
    }

    if (perfectMatch) {
        unchecked = commonPath->terminal;
    } else {
        unchecked = pathList_appendComponents(commonPath, linkPath, unchecked);
        if (unchecked == NULL) {
            goto _symlink_sub_error;
        }
    }
    if (unchecked->list != commonPath) {
        goto _symlink_sub_error;
    }
    if (link->child != NULL) {
        /* if there are further components to resolve _after_ the current
         * one, append them */
        if (pathList_appendComponents(commonPath, path, link->child) == NULL) {
            goto _symlink_sub_error;
        }
    }
    if (perfectMatch && unchecked->child) {
        unchecked = unchecked->child;
    }

    pathList_free(linkParent);
    pathList_free(origLinkPath);
    pathList_free(linkPath);
    linkParent = origLinkPath = linkPath = NULL;

    /* replace all components of path with commonPath */
    pathList_freeComponents(path->path);
    path->path = commonPath->path;
    path->terminal = commonPath->terminal;
    path->relroot = commonPath->relroot;

    PathComponent *ptr = NULL;
    for (ptr = path->path; ptr; ptr = ptr->child) {
        ptr->list = path;
    }

    commonPath->path = NULL;
    commonPath->terminal = NULL;
    commonPath->relroot = NULL;
    pathList_free(commonPath);

    return unchecked;

_symlink_sub_error:
    if (linkParent) pathList_free(linkParent);
    if (origLinkPath) pathList_free(origLinkPath);
    if (linkPath) pathList_free(linkPath);
    if (commonPath) pathList_free(commonPath);
    return NULL;
}

void pathList_free(PathList *path) {
    if (path == NULL) return;

    pathList_freeComponents(path->path);

    path->path = NULL;
    path->relroot = NULL;
    path->terminal = NULL;
    free(path);
}

void pathList_freeComponents(PathComponent *parent) {
    while (parent != NULL) {
        PathComponent *next = parent->child;
        pathList_freeComponent(parent);
        parent = next;
    }
}
    
void pathList_freeComponent(PathComponent *comp) {
    if (comp == NULL) return;

    if (comp->item != NULL) {
        free(comp->item);
        comp->item = NULL;
    }
    free(comp);
}
