//
// Created by llj on 19-5-24.
//

#ifndef TOOLLIFE_UNZIP_H
#define TOOLLIFE_UNZIP_H

#include <vector>
#include <string>
#include "inflate.h"

using namespace std;

class Unzip {
public:
    Unzip();
    ~Unzip();

public:
    void unzip(zip_t *pzip, char *path);
    int load_zip_file(char *path);
    int unzipFunc(string srcZipName,string destFolderName);

public:
    vector<string> vcFileLists;
    unsigned char *start;
    int zipfilelength;
};


#endif //TOOLLIFE_UNZIP_H
