/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// FileManager 
//
// Rob Dobson 2018-2020
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <FileManager.h>
#include <FileSystem.h>
#include <ConfigPinMap.h>
#include <RestAPIEndpointManager.h>
#include <Logger.h>
#include <SysManager.h>

static const char* MODULE_PREFIX = "FileManager";

// #define DEBUG_FILE_MANAGER_FILE_LIST
// #define DEBUG_FILE_MANAGER_FILE_LIST_DETAIL
// #define DEBUG_FILE_MANAGER_UPLOAD

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Constructor
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

FileManager::FileManager(const char *pModuleName, ConfigBase& defaultConfig, ConfigBase* pGlobalConfig, ConfigBase* pMutableConfig) 
        : SysModBase(pModuleName, defaultConfig, pGlobalConfig, pMutableConfig)
{
    _pProtocolExchange = nullptr;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Setup
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void FileManager::setup()
{
    // Apply setup
    applySetup();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Apply Setup
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void FileManager::applySetup()
{
    // Config settings
    bool enableSPIFFS = configGetBool("SPIFFSEnabled", false);
    bool enableLittleFS = configGetBool("LittleFSEnabled", false);
    bool localFsFormatIfCorrupt = configGetBool("LocalFsFormatIfCorrupt", false);
    bool enableSD = configGetBool("SDEnabled", false);
    bool defaultToSDIfAvailable = configGetBool("DefaultSD", false);
    bool cacheFileSystemInfo = configGetBool("CacheFileSysInfo", false);

    // SD pins
    String pinName = configGetString("SDMOSI", "");
    int sdMOSIPin = ConfigPinMap::getPinFromName(pinName.c_str());
    pinName = configGetString("SDMISO", "");
    int sdMISOPin = ConfigPinMap::getPinFromName(pinName.c_str());
    pinName = configGetString("SDCLK", "");
    int sdCLKPin = ConfigPinMap::getPinFromName(pinName.c_str());
    pinName = configGetString("SDCS", "");
    int sdCSPin = ConfigPinMap::getPinFromName(pinName.c_str());

    // Setup file system
    fileSystem.setup(enableSPIFFS, enableLittleFS, localFsFormatIfCorrupt, enableSD, sdMOSIPin, sdMISOPin, sdCLKPin, sdCSPin, 
                defaultToSDIfAvailable, cacheFileSystemInfo);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Service
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void FileManager::service()
{
    // Service the file system
    fileSystem.service();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// REST API Endpoints
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void FileManager::addRestAPIEndpoints(RestAPIEndpointManager& endpointManager)
{
    endpointManager.addEndpoint("reformatfs", RestAPIEndpoint::ENDPOINT_CALLBACK, RestAPIEndpoint::ENDPOINT_GET, 
                    std::bind(&FileManager::apiReformatFS, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3), 
                    "Reformat file system e.g. /local or /local/force");
    endpointManager.addEndpoint("filelist", RestAPIEndpoint::ENDPOINT_CALLBACK, RestAPIEndpoint::ENDPOINT_GET, 
                    std::bind(&FileManager::apiFileList, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3), 
                    "List files in folder e.g. /local/folder ... ~ for / in folder");
    endpointManager.addEndpoint("fileread", RestAPIEndpoint::ENDPOINT_CALLBACK, RestAPIEndpoint::ENDPOINT_GET, 
                    std::bind(&FileManager::apiFileRead, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3), 
                    "Read file ... name", "text/plain");
    endpointManager.addEndpoint("filedelete", RestAPIEndpoint::ENDPOINT_CALLBACK, RestAPIEndpoint::ENDPOINT_GET, 
                    std::bind(&FileManager::apiDeleteFile, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3), 
                    "Delete file e.g. /local/filename ... ~ for / in filename");
    endpointManager.addEndpoint("fileupload", 
                    RestAPIEndpoint::ENDPOINT_CALLBACK, 
                    RestAPIEndpoint::ENDPOINT_POST,
                    std::bind(&FileManager::apiUploadFileComplete, this, 
                            std::placeholders::_1, std::placeholders::_2, std::placeholders::_3),
                    "Upload file", 
                    "application/json", 
                    nullptr,
                    RestAPIEndpoint::ENDPOINT_CACHE_NEVER,
                    nullptr, 
                    nullptr,
                    std::bind(&FileManager::apiUploadFileBlock, this, 
                            std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Format file system
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void FileManager::apiReformatFS(const String &reqStr, String& respStr, const APISourceInfo& sourceInfo)
{
    // File system
    String fileSystemStr = RestAPIEndpointManager::getNthArgStr(reqStr.c_str(), 1);
    String forceFormat = RestAPIEndpointManager::getNthArgStr(reqStr.c_str(), 2);
    bool restartRequired = fileSystem.reformat(fileSystemStr, respStr, forceFormat.equalsIgnoreCase("force"));
    if (restartRequired)
    {
        // Restart required
        SysManager* pSysMan = getSysManager();
        if (pSysMan)
            pSysMan->systemRestart();
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// List files on a file system
// In the reqStr the first part of the path is the file system name (e.g. sd or local, can be blank to default)
// The second part of the path is the folder - note that / must be replaced with ~ in folder
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void FileManager::apiFileList(const String &reqStr, String& respStr, const APISourceInfo& sourceInfo)
{
    // File system
    String fileSystemStr = RestAPIEndpointManager::getNthArgStr(reqStr.c_str(), 1);
    // Folder
    String folderStr = RestAPIEndpointManager::getNthArgStr(reqStr.c_str(), 2);
    String extraPath = RestAPIEndpointManager::getNthArgStr(reqStr.c_str(), 3);
    if (extraPath.length() > 0)
        folderStr += "/" + extraPath;

#ifdef DEBUG_FILE_MANAGER_FILE_LIST
    LOG_I(MODULE_PREFIX, "apiFileList req %s fileSysStr %s folderStr %s", reqStr.c_str(), fileSystemStr.c_str(), folderStr.c_str());
#endif

    folderStr.replace("~", "/");
    if (folderStr.length() == 0)
        folderStr = "/";
    fileSystem.getFilesJSON(reqStr.c_str(), fileSystemStr, folderStr, respStr);

#ifdef DEBUG_FILE_MANAGER_FILE_LIST_DETAIL
    LOG_W(MODULE_PREFIX, "apiFileList respStr %s", respStr.c_str());
#endif
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Read file contents
// In the reqStr the first part of the path is the file system name (e.g. sd or local)
// The second part of the path is the folder and filename - note that / must be replaced with ~ in folder
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void FileManager::apiFileRead(const String &reqStr, String& respStr, const APISourceInfo& sourceInfo)
{
    // File system
    String fileSystemStr = RestAPIEndpointManager::getNthArgStr(reqStr.c_str(), 1);
    // Filename
    String fileNameStr = RestAPIEndpointManager::getNthArgStr(reqStr.c_str(), 2);
    String extraPath = RestAPIEndpointManager::getNthArgStr(reqStr.c_str(), 3);
    if (extraPath.length() > 0)
        fileNameStr += "/" + extraPath;
    fileNameStr.replace("~", "/");
    char* pFileContents = fileSystem.getFileContents(fileSystemStr, fileNameStr);
    if (!pFileContents)
    {
        respStr = "";
        return;
    }
    respStr = pFileContents;
    free(pFileContents);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Delete file on the file system
// In the reqStr the first part of the path is the file system name (e.g. sd or local)
// The second part of the path is the filename - note that / must be replaced with ~ in filename
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void FileManager::apiDeleteFile(const String &reqStr, String& respStr, const APISourceInfo& sourceInfo)
{
    // File system
    String fileSystemStr = RestAPIEndpointManager::getNthArgStr(reqStr.c_str(), 1);
    // Filename
    String filenameStr = RestAPIEndpointManager::getNthArgStr(reqStr.c_str(), 2);
    String extraPath = RestAPIEndpointManager::getNthArgStr(reqStr.c_str(), 3);
    if (extraPath.length() > 0)
        filenameStr += "/" + extraPath;
    bool rslt = false;
    filenameStr.replace("~", "/");
    if (filenameStr.length() != 0)
        rslt = fileSystem.deleteFile(fileSystemStr, filenameStr);
    Raft::setJsonBoolResult(reqStr.c_str(), respStr, rslt);
    LOG_I(MODULE_PREFIX, "deleteFile reqStr %s fs %s, filename %s rslt %s", 
                        reqStr.c_str(), fileSystemStr.c_str(), filenameStr.c_str(),
                        rslt ? "ok" : "fail");
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Upload file to file system - completed
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void FileManager::apiUploadFileComplete(const String &reqStr, String &respStr, const APISourceInfo& sourceInfo)
{
#ifdef DEBUG_FILE_MANAGER_UPLOAD
    LOG_I(MODULE_PREFIX, "uploadFileComplete %s", reqStr.c_str());
#endif
    Raft::setJsonBoolResult(reqStr.c_str(), respStr, true);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Upload file to file system - part of file
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

UtilsRetCode::RetCode FileManager::apiUploadFileBlock(const String& req, FileStreamBlock& fileStreamBlock, const APISourceInfo& sourceInfo)
{
    if (_pProtocolExchange)
        return _pProtocolExchange->handleFileUploadBlock(req, fileStreamBlock, sourceInfo, 
                    FileStreamBase::FILE_STREAM_CONTENT_TYPE_FILE, "");
    return UtilsRetCode::INVALID_OPERATION;
}

// /////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// // Upload file to file system - part of file (from HTTP POST file)
// /////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// bool FileManager::fileStreamDataBlock(FileStreamBlock& fileStreamBlock)
// {
// #ifdef DEBUG_FILE_MANAGER_UPLOAD
//     LOG_I(MODULE_PREFIX, "fileStreamDataBlock fileName %s fileLen %d filePos %d blockLen %d isFinal %d", 
//             fileStreamBlock.filename ? fileStreamBlock.filename : "<null>", 
//             fileStreamBlock.fileLen, fileStreamBlock.filePos,
//             fileStreamBlock.blockLen, fileStreamBlock.finalBlock);
// #endif

//     // Handle block
//     return _fileTransferHelper.handleRxBlock(fileStreamBlock);
// }

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Debug
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

String FileManager::getDebugJSON()
{
    return "{}";
}
