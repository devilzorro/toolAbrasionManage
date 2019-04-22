//
// Created by llj on 19-4-11.
//

#include "FileCtl.h"
#include "miniz/miniz_zip.h"
#include <iostream>
#include <unistd.h>

#ifdef WIN32
#include <windows>
#else
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#endif

FileCtl::FileCtl() {
#ifdef WIN32

#else
    uploadPath = "/home/i5/data/file/";
    storePath = "/tmp/toolLife/";
    folderName = "";
#endif
}

FileCtl::~FileCtl() {

}

vector<string> FileCtl::split(string strContent,string mark) {
    string::size_type pos;
    vector<string> result;
    strContent += mark;
    int size = strContent.size();
    for(int i=0; i<size; i++)
    {
        pos = strContent.find(mark,i);
        if(pos<size)
        {
            string s = strContent.substr(i,pos-i);
            result.push_back(s);
            i = pos+mark.size()-1;
        }
    }
    return result;
}

void FileCtl::createNewzipFolder(string folderName) {
//    int isCreate = mkdir(folderName.c_str(),S_IRUSR | S_IWUSR | S_IXUSR | S_IRWXG | S_IRWXO);
//    if (!isCreate) {
//        cout<<"create path:"<<folderName<<endl;
//    } else {
//        cout<<"create path error:"<<folderName<<"|error code:"<<isCreate<<endl;
//    }
//    return isCreate;
     this->folderName = folderName;
}

int FileCtl::zipFile(string srcFileName, string destZipPack) {
    int status = 0;
    FILE *srcFile = fopen(srcFileName.c_str(),"rb");
    long fileLength = 0;
    if (srcFile != NULL) {
        fseek(srcFile, 0, SEEK_END);
        fileLength = ftell(srcFile);
        rewind(srcFile);
    } else {
        cout<<"zip openfile err:"<<srcFileName<<endl;
        return -1;
    }

    char *fileData = (char*)malloc(sizeof(char)*fileLength);
    fread(fileData,1,fileLength,srcFile);

    vector<string> vcSrcName = split(srcFileName,"/");
    string tmpName = folderName + vcSrcName[vcSrcName.size()-1];
    status = mz_zip_add_mem_to_archive_file_in_place(destZipPack.c_str(),
                                                     tmpName.c_str(),
                                                     fileData,
                                                     fileLength,
                                                     "no comment",
                                                     (uint16)strlen("no comment"),
                                                     MZ_BEST_COMPRESSION);
    fclose(srcFile);
    free(fileData);
    return status;
}

int FileCtl::mvFile(string fileName, string destName) {
    int ret = rename(fileName.c_str(),destName.c_str());
    return ret;
}

int FileCtl::deleteFile(string fileName) {
    int ret = unlink(fileName.c_str());
    return ret;
}

int FileCtl::createPath(string pathName) {
    int isCreate = mkdir(pathName.c_str(),S_IRUSR | S_IWUSR | S_IXUSR | S_IRWXG | S_IRWXO);
    if (!isCreate) {
        cout<<"create path:"<<pathName<<endl;
    } else {
        cout<<"create path error:"<<pathName<<"|error code:"<<isCreate<<endl;
    }
    return isCreate;
}

vector<string> FileCtl::readFileList(string dirName) {
    vector<string> retFileNames;
    retFileNames.clear();
    DIR *dir;
    struct dirent *dirPtr;
    if ((dir = opendir(dirName.c_str())) == NULL) {
        cout<<"open dir:"<<dirName<<"|error"<<endl;
        return retFileNames;
    }

    while ((dirPtr = readdir(dir)) != NULL) {
        if (strcmp(dirPtr->d_name,".")==0 || strcmp(dirPtr->d_name,"..")==0) {
            //current dir OR parrent dir
        }
        else if (dirPtr->d_type == 8) {
            //file
            cout<<"fileName:"<<dirPtr->d_name<<endl;
            retFileNames.push_back(dirPtr->d_name);
        }
    }
    return retFileNames;
}


