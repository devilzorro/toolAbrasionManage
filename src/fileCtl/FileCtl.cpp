//
// Created by llj on 19-4-11.
//

#include "FileCtl.h"
#include "miniz/miniz_zip.h"
#include <iostream>

#ifdef WIN32
#include <windows.h>
#include "unzip-win/unzip.h"
#include <io.h>
#include <direct.h>
#include <tchar.h>
#else
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>
#include "unzip/Unzip.h"
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
#ifdef WIN32
    return _unlink(fileName.c_str());
#else
    int ret = unlink(fileName.c_str());
    return ret;
#endif
}

int FileCtl::createPath(string pathName) {
#ifdef WIN32
//    int isExist = _access(pathName.c_str(),00);

    return _mkdir(pathName.c_str());
#else
    int isCreate = mkdir(pathName.c_str(),S_IRUSR | S_IWUSR | S_IXUSR | S_IRWXG | S_IRWXO);
    if (!isCreate) {
        cout<<"create path:"<<pathName<<endl;
    } else {
        cout<<"create path error:"<<pathName<<"|error code:"<<isCreate<<endl;
    }
    return isCreate;
#endif
}

vector<string> FileCtl::readFileList(string dirName) {
    vector<string> retFileNames;
    retFileNames.clear();
#ifdef WIN32
    struct _finddata_t file; //定义结构体变量
    long handle;
    int iRetVal = 0;
    FILE *pf = NULL;
    char cFileAddr[300];
    strcpy(cFileAddr, dirName.c_str());
    _chdir(dirName.c_str());
    strcat(cFileAddr, "*.*");
    handle = _findfirst(cFileAddr, &file);//查找所有文件


    if (handle == -1)//如果handle为－1, 表示当前目录为空, 则结束查找而返回如果handle为－1, 表示当前目录为空, 则结束查找而返回
    {

    }
    else
    {
        while (!(_findnext(handle, &file)))
        {
            if (file.attrib &_A_SUBDIR) //是目录
            {
                if (file.name[0] != '.') //文件名不是'.'或'..'时
                {
                    memset(cFileAddr, 0, sizeof(cFileAddr));
                    _chdir(file.name); //进入该目录
                    printf("%s\n",file.name);//               add---
                    retFileNames.push_back(file.name);
//                    fprintf(fp,"%s\n" ,file.name);

                    _chdir("..");//查找完毕之后, 返回上一级目录找完毕之后, 返回　　　　　　　　　　　　　　　　　上一级目录

                }
            }
        }
        _findclose(handle);
    }
#else
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
#endif
    return retFileNames;
}

int FileCtl::unzipFile(string srcZipFileName, string destFolderName) {
#ifdef WIN32
//    TCHAR *tPath;
//#ifdef UNICODE
//    _stprintf_s(tPath, MAX_PATH, _T("%S"), srcZipFileName.c_str());//%S宽字符
//#else
//    _stprintf_s(tPath, MAX_PATH, _T("%s"), srcZipFileName.c_str());//%s单字符
//#endif

    HZIP hz = OpenZip(_T(srcZipFileName.c_str()),0);
    ZIPENTRY ze; GetZipItem(hz,-1,&ze); int numitems=ze.index;
// -1 gives overall information about the zipfile
    for (int zi=0; zi<numitems; zi++)
    { ZIPENTRY ze; GetZipItem(hz,zi,&ze); // fetch individual details
        printf("unzip fileName:%s\n",ze.name);
        UnzipItem(hz, zi, ze.name);         // e.g. the item's name.
    }
    CloseZip(hz);

//    HZIP hz = OpenZip(srcZipFileName.c_str(),0);
//    ZIPENTRY ze;
//    GetZipItem(hz,-1,&ze);
//    int numitems = ze.index;
//    for (int zi = 0; zi < numitems; zi++)
//    {
//        ZIPENTRY ze;
//        GetZipItem(hz, zi, &ze);
//        printf("unzip fileName:%s\n",ze.name);
//        UnzipItem(hz, zi, ze.name);
//    }
//    CloseZip(hz);
    return 0;
#else
    Unzip unzipObj;
    return unzipObj.unzipFunc(srcZipFileName,destFolderName);
#endif
}


