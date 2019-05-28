//
// Created by llj on 19-4-11.
//

#ifndef TOOLLIFE_FILECTL_H
#define TOOLLIFE_FILECTL_H

#include <string>
#include <vector>

typedef unsigned short uint16;

using namespace std;

class FileCtl {
public:
    FileCtl();
    ~FileCtl();

public:
    string storePath;
    string uploadPath;
    string folderName;

    int zipFile(string srcFileName,string destZipPack);
    int unzipFile(string srcFileName,string destFileName);
    void createNewzipFolder(string folderName);
    int mvFile(string fileName,string destName);
    int deleteFile(string fileName);
    int createPath(string pathName);
    vector<string> readFileList(string dirName);
private:
    vector<string> split(string strContent,string mark);
};


#endif //TOOLLIFE_FILECTL_H
