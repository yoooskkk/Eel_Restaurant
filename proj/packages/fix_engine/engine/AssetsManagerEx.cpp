/****************************************************************************
 Copyright (c) 2014 cocos2d-x.org
 Copyright (c) 2015-2016 Chukong Technologies Inc.
 Copyright (c) 2017-2018 Xiamen Yaji Software Co., Ltd.

 http://www.cocos2d-x.org

 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in
 all copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 THE SOFTWARE.
 ****************************************************************************/
#include "AssetsManagerEx.h"
#include "base/ccUTF8.h"
#include "CCAsyncTaskPool.h"

#include <stdio.h>
#include <errno.h>

#ifdef MINIZIP_FROM_SYSTEM
#include <minizip/unzip.h>
#else // from our embedded sources
#include "unzip/unzip.h"
#endif

NS_CC_EXT_BEGIN

#define VERSION_FILENAME       "_version.json"
#define TEMP_MANIFEST_FILENAME "_project.json.temp"
#define TEMP_PACKAGE_SUFFIX    "_temp"
#define MANIFEST_FILENAME      "_project.json"

#define BUFFER_SIZE    8192
#define MAX_FILENAME   512

#define DEFAULT_CONNECTION_TIMEOUT 45

#define SAVE_POINT_INTERVAL 0.1

const std::string AssetsManagerEx::VERSION_ID = "@version";
const std::string AssetsManagerEx::MANIFEST_ID = "@manifest";
#define MANIFEST_PATH "manifest/"
#define MAIN_BUNDLE "main"
#define ASSETS "assets"
#define MD5_UNKNOWN "UNKNOWN"
#define MAIN_JS "main.js"

AssetsManagerEx::AssetsManagerEx(const std::string& manifestUrl, const std::string& storagePath)
: _updateState(State::UNINITED)
, _assets(nullptr)
, _storagePath("")
, _tempVersionPath("")
, _cacheManifestPath("")
, _tempManifestPath("")
, _localManifest(nullptr)
, _tempManifest(nullptr)
, _remoteManifest(nullptr)
, _updateEntry(UpdateEntry::NONE)
, _percent(0)
, _percentByFile(0)
, _totalSize(0)
, _sizeCollected(0)
, _totalDownloaded(0)
, _totalToDownload(0)
, _totalWaitToDownload(0)
, _nextSavePoint(0.0)
, _downloadResumed(false)
, _maxConcurrentTask(32)
, _currConcurrentTask(0)
, _verifyCallback(nullptr)
, _inited(false)
, _canceled(false)
{
    init(manifestUrl, storagePath);
}

AssetsManagerEx::AssetsManagerEx(const std::string& manifestUrl, const std::string& storagePath, const VersionCompareHandle& handle)
: _updateState(State::UNINITED)
, _assets(nullptr)
, _storagePath("")
, _tempVersionPath("")
, _cacheManifestPath("")
, _tempManifestPath("")
, _localManifest(nullptr)
, _tempManifest(nullptr)
, _remoteManifest(nullptr)
, _updateEntry(UpdateEntry::NONE)
, _percent(0)
, _percentByFile(0)
, _totalSize(0)
, _sizeCollected(0)
, _totalDownloaded(0)
, _totalToDownload(0)
, _totalWaitToDownload(0)
, _nextSavePoint(0.0)
, _downloadResumed(false)
, _maxConcurrentTask(32)
, _currConcurrentTask(0)
, _versionCompareHandle(handle)
, _verifyCallback(nullptr)
, _eventCallback(nullptr)
, _inited(false)
, _isUsingBundle(false)
, _bundle("")
, _packageUrl("")
{
    init(manifestUrl, storagePath);
}

void AssetsManagerEx::init(const std::string& manifestUrl, const std::string& storagePath)
{
    // Init variables
    std::string pointer = StringUtils::format("%p", this);
    _eventName = "__cc_assets_manager_" + pointer;
    _fileUtils = FileUtils::getInstance();

    network::DownloaderHints hints =
    {
        static_cast<uint32_t>(_maxConcurrentTask),
        DEFAULT_CONNECTION_TIMEOUT,
        ".tmp"
    };
    _downloader = std::shared_ptr<network::Downloader>(new network::Downloader(hints));
    _downloader->onTaskError = std::bind(&AssetsManagerEx::onError, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4);
    _downloader->onTaskProgress = [this](const network::DownloadTask& task,
                                         int64_t /*bytesReceived*/,
                                         int64_t totalBytesReceived,
                                         int64_t totalBytesExpected)
    {
        this->onProgress(totalBytesExpected, totalBytesReceived, task.requestURL, task.identifier);
    };
    _downloader->onFileTaskSuccess = [this](const network::DownloadTask& task)
    {
        this->onSuccess(task.requestURL, task.storagePath, task.identifier);
    };
    setStoragePath(storagePath);
	_downloadAagin = 1;
	//这里做一下处理，当传入的是一个特定的格式时，修改当前3个缓存的路径
	std::string typeString = "type.";
	auto pos = manifestUrl.find(typeString);
	if ( pos != std::string::npos) {
		auto bundle = manifestUrl.substr(typeString.size());
		this->_isUsingBundle = true;
		this->_bundle = bundle;
		_tempVersionPath = _tempStoragePath + MANIFEST_PATH + bundle + VERSION_FILENAME;
		_cacheManifestPath = _storagePath + MANIFEST_PATH  + bundle + MANIFEST_FILENAME;
		_tempManifestPath = _tempStoragePath + MANIFEST_PATH  + bundle + TEMP_MANIFEST_FILENAME;
	}
	else {
		this->_isUsingBundle = false;
		this->_bundle = "";
		_tempVersionPath = _tempStoragePath + VERSION_FILENAME;
		_cacheManifestPath = _storagePath + MANIFEST_FILENAME;
		_tempManifestPath = _tempStoragePath + TEMP_MANIFEST_FILENAME;
		if (manifestUrl.size() > 0)
		{
			loadLocalManifest(manifestUrl);
		}
	}

    //manifest 目录
    auto dir = basename(_tempStoragePath + MANIFEST_PATH);
	if (!_fileUtils->isDirectoryExist(dir)) {
		_fileUtils->createDirectory(dir);
	}
}

AssetsManagerEx::~AssetsManagerEx()
{
    _downloader->onTaskError = (nullptr);
    _downloader->onFileTaskSuccess = (nullptr);
    _downloader->onTaskProgress = (nullptr);
    CC_SAFE_RELEASE(_localManifest);
    // _tempManifest could share a ptr with _remoteManifest or _localManifest
    if (_tempManifest != _localManifest && _tempManifest != _remoteManifest)
        CC_SAFE_RELEASE(_tempManifest);
    CC_SAFE_RELEASE(_remoteManifest);
}

AssetsManagerEx* AssetsManagerEx::create(const std::string& manifestUrl, const std::string& storagePath)
{
    AssetsManagerEx* ret = new (std::nothrow) AssetsManagerEx(manifestUrl, storagePath);
    if (ret)
    {
        ret->autorelease();
    }
    else
    {
        CC_SAFE_DELETE(ret);
    }
    return ret;
}

void AssetsManagerEx::reset() {
	_updateState = State::UNINITED;
}

void AssetsManagerEx::setPackageUrl(const std::string& url) {
	_packageUrl = url;
	// Append automatically "/"
	if (_packageUrl.size() > 0 && _packageUrl[_packageUrl.size() - 1] != '/')
	{
		_packageUrl.append("/");
	}
}

const std::string& AssetsManagerEx::getPackageUrl() const{
	return _packageUrl;
}

void AssetsManagerEx::setMainBundles(const std::vector<std::string>& bundles) {
	_mainBundles = bundles;
}

void AssetsManagerEx::setDownloadAgainZip(float percent) {
	if (percent < 0.0000f) {
		percent = 1;
	}
	if (percent > 1.0000f) {
		percent = 1;
	}
	_downloadAagin = percent;
}

void AssetsManagerEx::initManifests()
{
    _inited = true;
    _canceled = false;
    // Init and load temporary manifest
    _tempManifest = new (std::nothrow) Manifest();
    if (_tempManifest)
    {
        _tempManifest->setPackageUrl(getPackageUrl());
        _tempManifest->parseFile(_tempManifestPath);
        // Previous update is interrupted
        if (_fileUtils->isFileExist(_tempManifestPath))
        {
            // Manifest parse failed, remove all temp files
            if (!_tempManifest->isLoaded())
            {
                //_fileUtils->removeDirectory(_tempStoragePath);
				removeTempDirectory();
                CC_SAFE_RELEASE(_tempManifest);
                _tempManifest = nullptr;
            }
        }
    }
    else
    {
        _inited = false;
    }

    // Init remote manifest for future usage
    _remoteManifest = new (std::nothrow) Manifest();
    if (!_remoteManifest)
    {
        _inited = false;
    }

    if (!_inited)
    {
        CC_SAFE_RELEASE(_localManifest);
        CC_SAFE_RELEASE(_tempManifest);
        CC_SAFE_RELEASE(_remoteManifest);
        _localManifest = nullptr;
        _tempManifest = nullptr;
        _remoteManifest = nullptr;
    }
}

void AssetsManagerEx::prepareLocalManifest()
{
    // An alias to assets
    _assets = &(_localManifest->getAssets());

    // Add search paths
    _localManifest->prependSearchPaths();
}

bool AssetsManagerEx::loadLocalManifest(Manifest* localManifest, const std::string& storagePath)
{
    if (_updateState > State::UNINITED)
    {
		if (_updateState != State::FAIL_TO_UPDATE) {
			//If the update fails, try to update again
			return false;
		}
    }
    if (!localManifest || !localManifest->isLoaded())
    {
        return false;
    }
    _inited = true;
    _canceled = false;
    // Reset storage path
    if (storagePath.size() > 0)
    {
        setStoragePath(storagePath);
        _tempVersionPath = _tempStoragePath + VERSION_FILENAME;
        _cacheManifestPath = _storagePath + MANIFEST_FILENAME;
        _tempManifestPath = _tempStoragePath + TEMP_MANIFEST_FILENAME;
    }
    // Release existing local manifest
    if (_localManifest)
    {
        CC_SAFE_RELEASE(_localManifest);
    }
    _localManifest = localManifest;
    _localManifest->retain();
    // Find the cached manifest file
    Manifest *cachedManifest = nullptr;
    if (_fileUtils->isFileExist(_cacheManifestPath))
    {
        cachedManifest = new (std::nothrow) Manifest();
        if (cachedManifest)
        {
            cachedManifest->setPackageUrl(getPackageUrl());
            cachedManifest->parseFile(_cacheManifestPath);
            if (!cachedManifest->isLoaded())
            {
                _fileUtils->removeFile(_cacheManifestPath);
                CC_SAFE_RELEASE(cachedManifest);
                cachedManifest = nullptr;
            }
        }
    }
    // Compare with cached manifest to determine which one to use
    if (cachedManifest)
    {
        bool isEqual = _localManifest->equal(cachedManifest);
        if (!isEqual)
        {
            // Recreate storage, to empty the content
			removeCachedDirectory();
            CC_SAFE_RELEASE(cachedManifest);
        }
        else
        {
            CC_SAFE_RELEASE(_localManifest);
            _localManifest = cachedManifest;
        }
    }
    prepareLocalManifest();

    // Init temp manifest and remote manifest
    initManifests();

    if (!_inited)
    {
        return false;
    }
    else
    {
        _updateState = State::UNCHECKED;
        return true;
    }
}

bool AssetsManagerEx::loadLocalManifest(const std::string& manifestUrl)
{
    if (manifestUrl.size() == 0)
    {
        return false;
    }
    if (_updateState > State::UNINITED)
    {
        return false;
    }
    _manifestUrl = manifestUrl;
    // Init and load local manifest
    _localManifest = new (std::nothrow) Manifest();
    if (!_localManifest)
    {
        return false;
    }
    Manifest *cachedManifest = nullptr;
    // Find the cached manifest file
    if (_fileUtils->isFileExist(_cacheManifestPath))
    {
        cachedManifest = new (std::nothrow) Manifest();
        if (cachedManifest)
        {
            cachedManifest->setPackageUrl(getPackageUrl());
            cachedManifest->parseFile(_cacheManifestPath);
            if (!cachedManifest->isLoaded())
            {
                _fileUtils->removeFile(_cacheManifestPath);
                CC_SAFE_RELEASE(cachedManifest);
                cachedManifest = nullptr;
            }
        }
    }

    // Ensure no search path of cached manifest is used to load this manifest
    std::vector<std::string> searchPaths = _fileUtils->getSearchPaths();
    if (cachedManifest)
    {
        std::vector<std::string> cacheSearchPaths = cachedManifest->getSearchPaths();
        std::vector<std::string> trimmedPaths = searchPaths;
        for (auto path : cacheSearchPaths)
        {
            const auto pos = std::find(trimmedPaths.begin(), trimmedPaths.end(), path);
            if (pos != trimmedPaths.end())
            {
                trimmedPaths.erase(pos);
            }
        }
        _fileUtils->setSearchPaths(trimmedPaths);
    }
	_localManifest->setPackageUrl(getPackageUrl());
    // Load local manifest in app package
    _localManifest->parseFile(_manifestUrl);
    if (cachedManifest)
    {
        // Restore search paths
        _fileUtils->setSearchPaths(searchPaths);
    }
    if (_localManifest->isLoaded())
    {
        // Compare with cached manifest to determine which one to use
        if (cachedManifest)
        {
            bool isEqual = _localManifest->equal(cachedManifest);
            if (!isEqual)
            {
                // Recreate storage, to empty the content
				removeCachedDirectory();
                CC_SAFE_RELEASE(cachedManifest);
            }
            else
            {
                CC_SAFE_RELEASE(_localManifest);
                _localManifest = cachedManifest;
            }
        }
        prepareLocalManifest();
    }

    // Fail to load local manifest
    if (!_localManifest->isLoaded())
    {
        CCLOG("AssetsManagerEx : No local manifest file found error.\n");
        dispatchUpdateEvent(EventAssetsManagerEx::EventCode::ERROR_NO_LOCAL_MANIFEST);
        return false;
    }
    initManifests();
    _updateState = State::UNCHECKED;
    return true;
}

void AssetsManagerEx::removeBundleDirectory(const std::string& path) {

	auto saveRemoveDirectory = [=]( const std::string& dir ) {
		if (_fileUtils->isDirectoryExist(dir)) {
			_fileUtils->removeDirectory(dir);
		}
	};
	auto saveRemoveFile = [=](const std::string& fullPath) {
		if (_fileUtils->isFileExist(fullPath)) {
			_fileUtils->removeFile(fullPath);
		}
	};

	if (_bundle == MAIN_BUNDLE) {
		//只删除当前bundle的资源
		for (auto it = _mainBundles.begin(); it != _mainBundles.end(); ++it) {
            if (*it == MAIN_JS) {
				//入口文件单独删除
				continue;
			}
			saveRemoveDirectory(path + *it + "/");
		}
		saveRemoveFile(path + MANIFEST_PATH + _bundle + VERSION_FILENAME);
		saveRemoveFile(path + MANIFEST_PATH + _bundle + MANIFEST_FILENAME);
        saveRemoveFile(path + MAIN_JS);
	}
	else {
		saveRemoveDirectory(path + ASSETS + "/" + _bundle + "/");
		saveRemoveFile(path + MANIFEST_PATH + _bundle + VERSION_FILENAME);
		saveRemoveFile(path + MANIFEST_PATH + _bundle + MANIFEST_FILENAME);
	}
}

void AssetsManagerEx::removeCachedDirectory() {
	removeBundleDirectory(_storagePath);
}

void AssetsManagerEx::removeTempDirectory() {
	removeBundleDirectory(_tempStoragePath);
}

bool AssetsManagerEx::isNeedDownLoadZip(float download, float total) {
	//下载总数占比
	auto percent = download / total;
	if (percent >= 1.0000f) {
		return true;
	}
	if (percent <= 0.0000f) {
		return false;
	}
	if (percent > _downloadAagin) {
		return true;
	}
	return false;
}

void AssetsManagerEx::toDownloadZip() {
	DownloadUnit unit;
	unit.customId = _bundle + "_" + _remoteManifest->getMd5() + ".zip";
	unit.srcUrl = getPackageUrl() + "zips/" + unit.customId;
	unit.storagePath = _tempStoragePath + unit.customId;
	unit.size = _remoteManifest->getZipSize();
	unit.compressed = true;
	_remoteManifest->updateToZipAsset(unit);

	_downloadUnits.emplace(unit.customId, unit);
	_tempManifest->setAssetDownloadState(unit.customId, Manifest::DownloadState::UNSTARTED);
	// Start updating the temp manifest
	_tempManifest->setUpdating(true);
	// Save current download manifest information for resuming
	auto dir = basename(_tempManifestPath);
	if (!_fileUtils->isDirectoryExist(dir)) {
		_fileUtils->createDirectory(dir);
	}
	_tempManifest->saveToFile(_tempManifestPath);

	_totalWaitToDownload = _totalToDownload = (int)_downloadUnits.size();
}

bool AssetsManagerEx::loadRemoteManifest(Manifest* remoteManifest)
{
    if (!_inited || _updateState > State::UNCHECKED)
    {
        return false;
    }
    if (!remoteManifest || !remoteManifest->isLoaded())
    {
        return false;
    }
    // Release existing remote manifest
    if (_remoteManifest)
    {
        CC_SAFE_RELEASE(_remoteManifest);
    }
    _remoteManifest = remoteManifest;
    _remoteManifest->retain();
    // Compare manifest version and set state
    if (_localManifest->equal(_remoteManifest))
    {
        _updateState = State::UP_TO_DATE;
        //_fileUtils->removeDirectory(_tempStoragePath);
		removeTempDirectory();
        dispatchUpdateEvent(EventAssetsManagerEx::EventCode::ALREADY_UP_TO_DATE);
    }
    else
    {
        _updateState = State::NEED_UPDATE;
        dispatchUpdateEvent(EventAssetsManagerEx::EventCode::NEW_VERSION_FOUND);
    }
    return true;
}

std::string AssetsManagerEx::basename(const std::string& path) const
{
    size_t found = path.find_last_of("/\\");

    if (std::string::npos != found)
    {
        return path.substr(0, found);
    }
    else
    {
        return path;
    }
}

std::string AssetsManagerEx::get(const std::string& key) const
{
    auto it = _assets->find(key);
    if (it != _assets->cend()) {
        return _storagePath + it->second.path;
    }
    else return "";
}

const Manifest* AssetsManagerEx::getLocalManifest() const
{
    return _localManifest;
}

const Manifest* AssetsManagerEx::getRemoteManifest() const
{
    return _remoteManifest;
}

const std::string& AssetsManagerEx::getStoragePath() const
{
    return _storagePath;
}

void AssetsManagerEx::setStoragePath(const std::string& storagePath)
{
    _storagePath = storagePath;
    adjustPath(_storagePath);
    _fileUtils->createDirectory(_storagePath);

    _tempStoragePath = _storagePath;
    _tempStoragePath.insert(_storagePath.size() - 1, TEMP_PACKAGE_SUFFIX);
    _fileUtils->createDirectory(_tempStoragePath);
}

void AssetsManagerEx::adjustPath(std::string &path)
{
    if (path.size() > 0 && path[path.size() - 1] != '/')
    {
        path.append("/");
    }
}

bool AssetsManagerEx::decompress(const std::string &zip)
{
    // Find root path for zip file
    size_t pos = zip.find_last_of("/\\");
    if (pos == std::string::npos)
    {
        CCLOG("AssetsManagerEx : no root path specified for zip file %s\n", zip.c_str());
        return false;
    }
    const std::string rootPath = zip.substr(0, pos+1);

    // Open the zip file
    unzFile zipfile = unzOpen(FileUtils::getInstance()->getSuitableFOpen(zip).c_str());
    if (! zipfile)
    {
        CCLOG("AssetsManagerEx : can not open downloaded zip file %s\n", zip.c_str());
        return false;
    }

    // Get info about the zip file
    unz_global_info global_info;
    if (unzGetGlobalInfo(zipfile, &global_info) != UNZ_OK)
    {
        CCLOG("AssetsManagerEx : can not read file global info of %s\n", zip.c_str());
        unzClose(zipfile);
        return false;
    }

    // Buffer to hold data read from the zip file
    char readBuffer[BUFFER_SIZE];
    // Loop to extract all files.
    uLong i;
    for (i = 0; i < global_info.number_entry; ++i)
    {
        // Get info about current file.
        unz_file_info fileInfo;
        char fileName[MAX_FILENAME];
        if (unzGetCurrentFileInfo(zipfile,
                                  &fileInfo,
                                  fileName,
                                  MAX_FILENAME,
                                  NULL,
                                  0,
                                  NULL,
                                  0) != UNZ_OK)
        {
            CCLOG("AssetsManagerEx : can not read compressed file info\n");
            unzClose(zipfile);
            return false;
        }
        const std::string fullPath = rootPath + fileName;

        // Check if this entry is a directory or a file.
        const size_t filenameLength = strlen(fileName);
        if (fileName[filenameLength-1] == '/')
        {
            //There are not directory entry in some case.
            //So we need to create directory when decompressing file entry
            if ( !_fileUtils->createDirectory(basename(fullPath)) )
            {
                // Failed to create directory
                CCLOG("AssetsManagerEx : can not create directory %s\n", fullPath.c_str());
                unzClose(zipfile);
                return false;
            }
        }
        else
        {
            // Create all directories in advance to avoid issue
            std::string dir = basename(fullPath);
            if (!_fileUtils->isDirectoryExist(dir)) {
                if (!_fileUtils->createDirectory(dir)) {
                    // Failed to create directory
                    CCLOG("AssetsManagerEx : can not create directory %s\n", fullPath.c_str());
                    unzClose(zipfile);
                    return false;
                }
            }
            // Entry is a file, so extract it.
            // Open current file.
            if (unzOpenCurrentFile(zipfile) != UNZ_OK)
            {
                CCLOG("AssetsManagerEx : can not extract file %s\n", fileName);
                unzClose(zipfile);
                return false;
            }

            // Create a file to store current file.
            FILE *out = fopen(FileUtils::getInstance()->getSuitableFOpen(fullPath).c_str(), "wb");
            if (!out)
            {
                CCLOG("AssetsManagerEx : can not create decompress destination file %s (errno: %d)\n", fullPath.c_str(), errno);
                unzCloseCurrentFile(zipfile);
                unzClose(zipfile);
                return false;
            }

            // Write current file content to destinate file.
            int error = UNZ_OK;
            do
            {
                error = unzReadCurrentFile(zipfile, readBuffer, BUFFER_SIZE);
                if (error < 0)
                {
                    CCLOG("AssetsManagerEx : can not read zip file %s, error code is %d\n", fileName, error);
                    fclose(out);
                    unzCloseCurrentFile(zipfile);
                    unzClose(zipfile);
                    return false;
                }

                if (error > 0)
                {
                    fwrite(readBuffer, error, 1, out);
                }
            } while(error > 0);

            fclose(out);
        }

        unzCloseCurrentFile(zipfile);

        // Goto next entry listed in the zip file.
        if ((i+1) < global_info.number_entry)
        {
            if (unzGoToNextFile(zipfile) != UNZ_OK)
            {
                CCLOG("AssetsManagerEx : can not read next file for decompressing\n");
                unzClose(zipfile);
                return false;
            }
        }
    }

    unzClose(zipfile);
    return true;
}

void AssetsManagerEx::decompressDownloadedZip(const std::string &customId, const std::string &storagePath)
{
    struct AsyncData
    {
        std::string customId;
        std::string zipFile;
        bool succeed;
    };

    AsyncData* asyncData = new AsyncData;
    asyncData->customId = customId;
    asyncData->zipFile = storagePath;
    asyncData->succeed = false;

    std::function<void(void*)> decompressFinished = [this](void* param) {
        auto dataInner = reinterpret_cast<AsyncData*>(param);
        if (dataInner->succeed)
        {
            fileSuccess(dataInner->customId, dataInner->zipFile);
            _fileUtils->removeFile(dataInner->zipFile);
        }
        else
        {
            std::string errorMsg = "Unable to decompress file " + dataInner->zipFile;
            // Ensure zip file deletion (if decompress failure cause task thread exit anormally)
            _fileUtils->removeFile(dataInner->zipFile);
            dispatchUpdateEvent(EventAssetsManagerEx::EventCode::ERROR_DECOMPRESS, "", errorMsg);
            fileError(dataInner->customId, errorMsg);
        }
        delete dataInner;
    };
    AsyncTaskPool::getInstance()->enqueue(AsyncTaskPool::TaskType::TASK_OTHER, decompressFinished, (void*)asyncData, [this, asyncData]() {
        // Decompress all compressed files
        if (decompress(asyncData->zipFile))
        {
            asyncData->succeed = true;
        }
        // Wait for the decompression to complete before deleting the zip archive
        // _fileUtils->removeFile(asyncData->zipFile);
    });
}

void AssetsManagerEx::dispatchUpdateEvent(EventAssetsManagerEx::EventCode code, const std::string &assetId/* = ""*/, const std::string &message/* = ""*/, int curle_code/* = CURLE_OK*/, int curlm_code/* = CURLM_OK*/)
{
    switch (code)
    {
        case EventAssetsManagerEx::EventCode::ERROR_UPDATING:
        case EventAssetsManagerEx::EventCode::ERROR_PARSE_MANIFEST:
        case EventAssetsManagerEx::EventCode::ERROR_NO_LOCAL_MANIFEST:
        case EventAssetsManagerEx::EventCode::ERROR_DECOMPRESS:
        case EventAssetsManagerEx::EventCode::ERROR_DOWNLOAD_MANIFEST:
        case EventAssetsManagerEx::EventCode::UPDATE_FAILED:
        case EventAssetsManagerEx::EventCode::UPDATE_FINISHED:
        case EventAssetsManagerEx::EventCode::ALREADY_UP_TO_DATE:
            _updateEntry = UpdateEntry::NONE;
            break;
        case EventAssetsManagerEx::EventCode::UPDATE_PROGRESSION:
            break;
        case EventAssetsManagerEx::EventCode::ASSET_UPDATED:
            break;
        case EventAssetsManagerEx::EventCode::NEW_VERSION_FOUND:
            if (_updateEntry == UpdateEntry::CHECK_UPDATE)
            {
                _updateEntry = UpdateEntry::NONE;
            }
            break;
        default:
            break;
    }

    if (_eventCallback != nullptr) {
        EventAssetsManagerEx* event = new (std::nothrow) EventAssetsManagerEx(_eventName, this, code, assetId, message, curle_code, curlm_code);
        _eventCallback(event);
        event->release();
    }
}

AssetsManagerEx::State AssetsManagerEx::getState() const
{
    return _updateState;
}

void AssetsManagerEx::downloadVersion()
{
    if (_updateState > State::PREDOWNLOAD_VERSION)
        return;

    std::string versionUrl = _localManifest->getVersionFileUrl();

    if (versionUrl.size() > 0)
    {
        _updateState = State::DOWNLOADING_VERSION;
        // Download version file asynchronously
        _downloader->createDownloadFileTask(versionUrl, _tempVersionPath, VERSION_ID);
    }
    // No version file found
    else
    {
        CCLOG("AssetsManagerEx : No version file found, step skipped\n");
        _updateState = State::PREDOWNLOAD_MANIFEST;
        downloadManifest();
    }
}

void AssetsManagerEx::parseVersion()
{
    if (_updateState != State::VERSION_LOADED)
        return;
	_remoteManifest->setPackageUrl(getPackageUrl());
    _remoteManifest->parseVersion(_tempVersionPath);

    if (!_remoteManifest->isVersionLoaded())
    {
        CCLOG("AssetsManagerEx : Fail to parse version file, step skipped\n");
        _updateState = State::PREDOWNLOAD_MANIFEST;
        downloadManifest();
    }
    else
    {
        if (_localManifest->equal(_remoteManifest))
        {
            _updateState = State::UP_TO_DATE;
            //_fileUtils->removeDirectory(_tempStoragePath);
			removeTempDirectory();
            dispatchUpdateEvent(EventAssetsManagerEx::EventCode::ALREADY_UP_TO_DATE);
        }
        else
        {
            _updateState = State::PREDOWNLOAD_MANIFEST;
            downloadManifest();
        }
    }
}

void AssetsManagerEx::downloadManifest()
{
    if (_updateState != State::PREDOWNLOAD_MANIFEST)
        return;

    std::string manifestUrl = _localManifest->getManifestFileUrl();

    if (manifestUrl.size() > 0)
    {
        _updateState = State::DOWNLOADING_MANIFEST;
        // Download version file asynchronously
        _downloader->createDownloadFileTask(manifestUrl, _tempManifestPath, MANIFEST_ID);
    }
    // No manifest file found
    else
    {
        CCLOG("AssetsManagerEx : No manifest file found, check update failed\n");
        dispatchUpdateEvent(EventAssetsManagerEx::EventCode::ERROR_DOWNLOAD_MANIFEST);
        _updateState = State::UNCHECKED;
    }
}

void AssetsManagerEx::parseManifest()
{
    if (_updateState != State::MANIFEST_LOADED)
        return;

	_remoteManifest->setPackageUrl(getPackageUrl());
    _remoteManifest->parseFile(_tempManifestPath);

    if (!_remoteManifest->isLoaded())
    {
        CCLOG("AssetsManagerEx : Error parsing manifest file, %s", _tempManifestPath.c_str());
        dispatchUpdateEvent(EventAssetsManagerEx::EventCode::ERROR_PARSE_MANIFEST);
        _updateState = State::UNCHECKED;
    }
    else
    {
        if (_localManifest->equal(_remoteManifest))
        {
            _updateState = State::UP_TO_DATE;
            //_fileUtils->removeDirectory(_tempStoragePath);
			removeTempDirectory();
            dispatchUpdateEvent(EventAssetsManagerEx::EventCode::ALREADY_UP_TO_DATE);
        }
        else
        {
            _updateState = State::NEED_UPDATE;
            
            if (_updateEntry == UpdateEntry::DO_UPDATE)
            {
                startUpdate();
            }
            else if (_updateEntry == UpdateEntry::CHECK_UPDATE)
            {
                prepareUpdate();
            }
            
            dispatchUpdateEvent(EventAssetsManagerEx::EventCode::NEW_VERSION_FOUND);
        }
    }
}

void AssetsManagerEx::prepareUpdate()
{
    if (_updateState != State::NEED_UPDATE)
        return;

    // Clean up before update
    _failedUnits.clear();
    _downloadUnits.clear();
    _totalWaitToDownload = _totalToDownload = 0;
    _nextSavePoint = 0;
    _percent = _percentByFile = _sizeCollected = _totalDownloaded = _totalSize = 0;
    _downloadResumed = false;
    _downloadedSize.clear();
    _totalEnabled = false;
    _unzip = false;
    // Temporary manifest exists, previously updating and equals to the remote version, resuming previous download
    if (_tempManifest && _tempManifest->isLoaded() && _tempManifest->isUpdating() && _tempManifest->equal(_remoteManifest))
	{
		auto dir = basename(_tempManifestPath);
		if (!_fileUtils->isDirectoryExist(dir)) {
			_fileUtils->createDirectory(dir);
		}
        _tempManifest->saveToFile(_tempManifestPath);
        _tempManifest->genResumeAssetsList(&_downloadUnits,_unzip);
        _totalWaitToDownload = _totalToDownload = (int)_downloadUnits.size();
        _downloadResumed = true;

        // Collect total size
        for(auto iter : _downloadUnits)
        {
            const DownloadUnit& unit = iter.second;
            if (unit.size > 0)
            {
                _totalSize += unit.size;
            }
        }
    }
    else
    {
        // Temporary manifest exists, but can't be parsed or version doesn't equals remote manifest (out of date)
        if (_tempManifest)
        {
            // Remove all temp files
            //_fileUtils->removeDirectory(_tempStoragePath);
			removeTempDirectory();
            CC_SAFE_RELEASE(_tempManifest);
            // Recreate temp storage path and save remote manifest
            _fileUtils->createDirectory(_tempStoragePath);
			_fileUtils->createDirectory(basename(_tempManifestPath));
            _remoteManifest->saveToFile(_tempManifestPath);
        }

        // Temporary manifest will be used to register the download states of each asset,
        // in this case, it equals remote manifest.
        _tempManifest = _remoteManifest;

        // Check difference between local manifest and remote manifest
		if (_localManifest->getMd5() == MD5_UNKNOWN) {
			// 如果第一次未下载 ，直接下载zip包进行解压
			toDownloadZip();
		}
		else {
			std::unordered_map<std::string, Manifest::AssetDiff> diff_map = _localManifest->genDiff(_remoteManifest);
			if (diff_map.size() == 0)
			{
				updateSucceed();
				return;
			}
			else
			{
				if (this->isNeedDownLoadZip(diff_map.size(), _remoteManifest->getAssets().size())) {
					toDownloadZip();
				}
				else {
					// Generate download units for all assets that need to be updated or added
					std::string packageUrl = _remoteManifest->getPackageUrl();
					// Preprocessing local files in previous version and creating download folders
					for (auto it = diff_map.begin(); it != diff_map.end(); ++it)
					{
						Manifest::AssetDiff diff = it->second;
						if (diff.type != Manifest::DiffType::DELETED)
						{
							std::string path = diff.asset.path;
							DownloadUnit unit;
							unit.customId = it->first;
							unit.srcUrl = packageUrl + path + "?md5=" + diff.asset.md5;
							unit.storagePath = _tempStoragePath + path;
							unit.size = diff.asset.size;
							unit.compressed = diff.asset.compressed;
							_downloadUnits.emplace(unit.customId, unit);
							_tempManifest->setAssetDownloadState(it->first, Manifest::DownloadState::UNSTARTED);
							_totalSize += unit.size;
						}
					}
					// Start updating the temp manifest
					_tempManifest->setUpdating(true);
					// Save current download manifest information for resuming
					auto dir = basename(_tempManifestPath);
					if (!_fileUtils->isDirectoryExist(dir)) {
						_fileUtils->createDirectory(dir);
					}
					_tempManifest->saveToFile(_tempManifestPath);

					_totalWaitToDownload = _totalToDownload = (int)_downloadUnits.size();
				}
			}
		}
    }
    _updateState = State::READY_TO_UPDATE;
}

void AssetsManagerEx::startUpdate()
{
    if (_updateState == State::NEED_UPDATE)
    {
        prepareUpdate();
    }
    if (_updateState == State::READY_TO_UPDATE)
    {
        _totalSize = 0;
        _updateState = State::UPDATING;
        std::string msg;
        if (_downloadResumed)
        {
            msg = StringUtils::format("Resuming from previous unfinished update, %d files remains to be finished.", _totalToDownload);
        }
        else
        {
            msg = StringUtils::format("Start to update %d files from remote package.", _totalToDownload);
        }
        dispatchUpdateEvent(EventAssetsManagerEx::EventCode::UPDATE_PROGRESSION, "", msg);
		if (_unzip) {
			auto it = _downloadUnits.begin();
			onSuccess(it->second.srcUrl, it->second.storagePath, it->first);
		}
		else {
			batchDownload();
		}
    }
}

void AssetsManagerEx::updateSucceed()
{
    // Set temp manifest's updating
    if (_tempManifest != nullptr) {
        _tempManifest->setUpdating(false);
    }

    // Every thing is correctly downloaded, do the following
    // 1. rename temporary manifest to valid manifest
    if (this->_isUsingBundle) {
		if (_fileUtils->isFileExist(_tempManifestPath)) {
			auto rootPath = basename(_tempManifestPath) + "/";
			_fileUtils->renameFile(rootPath, this->_bundle + TEMP_MANIFEST_FILENAME, this->_bundle + MANIFEST_FILENAME);
			if (_tempManifest) {
				_tempManifest->saveVersionToFile(rootPath + this->_bundle + VERSION_FILENAME);
			}
		}
	}else {
		if (_fileUtils->isFileExist(_tempManifestPath)) {
			_fileUtils->renameFile(_tempStoragePath, TEMP_MANIFEST_FILENAME, MANIFEST_FILENAME);
		}
	}

	// 2. Get the delete files
	std::unordered_map<std::string, Manifest::AssetDiff> diff_map = _localManifest->genDiff(_remoteManifest);

	// 3. merge temporary storage path to storage path so that temporary version turns to cached version
	struct AsyncData
	{
		std::unordered_map<std::string, Manifest::AssetDiff> diff_map;
	};

	auto asyncData = new AsyncData;
	asyncData->diff_map = diff_map;

	auto onComplete = [this](void* param) {
		auto dataInner = reinterpret_cast<AsyncData*>(param);
		// 4. swap the localManifest
		CC_SAFE_RELEASE(_localManifest);
		_localManifest = _remoteManifest;
		_localManifest->setManifestRoot(_storagePath);
		_remoteManifest = nullptr;
		// 5. make local manifest take effect
		prepareLocalManifest();
		// 6. Set update state
		_updateState = State::UP_TO_DATE;
		// 7. Notify finished event
		dispatchUpdateEvent(EventAssetsManagerEx::EventCode::UPDATE_FINISHED);
		// 8. Remove temp storage path
		removeTempDirectory();
		delete dataInner;
	};

	AsyncTaskPool::getInstance()->enqueue(AsyncTaskPool::TaskType::TASK_OTHER, onComplete, (void*)asyncData, [this, asyncData]() {
		if (this->_isUsingBundle) {
			if (_bundle == MAIN_BUNDLE) {
				for (auto i = 0; i < _mainBundles.size(); i++) {
					auto path = _mainBundles[i] + "/";
					if (_mainBundles[i] == MAIN_JS) {
						moveTempToCached(_tempStoragePath, MAIN_JS, asyncData->diff_map, i + 1 == _mainBundles.size());
					}
					else {
						moveTempToCached(_tempStoragePath + path, path, asyncData->diff_map, i + 1 == _mainBundles.size());
					}
				}
			}
			else {
				auto path = ASSETS + std::string("/") + _bundle + "/";
				moveTempToCached(_tempStoragePath + path, path, asyncData->diff_map);
			}
		}
		else {
			moveTempToCached(_tempStoragePath, "", asyncData->diff_map);
		}
	});
}

void AssetsManagerEx::moveTempToCached(const std::string& root,const std::string& path , std::unordered_map<std::string, Manifest::AssetDiff>& diff_map, bool isComplete) {

    auto copyVersions = [=]() {
		//完成后复制版本文件
        if (isComplete) {
            auto tempDstRoot = _storagePath + MANIFEST_PATH;
            if (!_fileUtils->isDirectoryExist(tempDstRoot)) {
                _fileUtils->createDirectory(tempDstRoot);
            }
        
            auto versionDstPath = tempDstRoot + _bundle + VERSION_FILENAME;
            if (_fileUtils->isFileExist(versionDstPath))
            {
                _fileUtils->removeFile(versionDstPath);
            }
        
            auto projectDstPath = tempDstRoot + _bundle + MANIFEST_FILENAME;
            if (_fileUtils->isFileExist(projectDstPath)) {
                _fileUtils->removeFile(projectDstPath);
            }
        
            auto tempSrcRoot = _tempStoragePath + MANIFEST_PATH;
            auto versionSrcPath = tempSrcRoot + _bundle + VERSION_FILENAME;
            auto projectSrcPath = tempSrcRoot + _bundle + MANIFEST_FILENAME;
        
            _fileUtils->renameFile(versionSrcPath, versionDstPath);
            _fileUtils->renameFile(projectSrcPath, projectDstPath);
        }
	};

    // move main.js
	if (path == MAIN_JS){
		auto dstPath = _storagePath + MAIN_JS;
		auto srcPath = root + MAIN_JS;
		if (_fileUtils->isFileExist(dstPath)) {
			_fileUtils->removeFile(dstPath);
		}
		if (_fileUtils->isFileExist(srcPath)) {
			_fileUtils->renameFile(srcPath, dstPath);
		}
		copyVersions();
		return;
	}

	// 3. merge temporary storage path to storage path so that temporary version turns to cached version
	if (_fileUtils->isDirectoryExist(root))
	{
		// Merging all files in temp storage path to storage path
		std::vector<std::string> files;
		_fileUtils->listFilesRecursively(root, &files);
		int baseOffset = (int)_tempStoragePath.length();
		
		//字符串分割函数
		auto split = [](std::string str, std::string pattern = "/")
		{
			std::string::size_type pos;
			std::vector<std::string> result;
			str += pattern;//扩展字符串以方便操作
			int size = str.size();
			for (int i = 0; i < size; i++)
			{
				pos = str.find(pattern, i);
				if (pos < size)
				{
					std::string s = str.substr(i, pos - i);
					if (s.size() > 0) {
						result.push_back(s);
					}
					i = pos + pattern.size() - 1;
				}
			}
			return result;
		};

		auto paths = split(path);
		auto tempDstPath = _storagePath;
		for (auto it = paths.begin(); it != paths.end(); ++it) {
			tempDstPath += *it + "/";
			if (!_fileUtils->isDirectoryExist(tempDstPath)) {
				_fileUtils->createDirectory(tempDstPath);
			}
		}

		std::string relativePath, dstPath;
		for (std::vector<std::string>::iterator it = files.begin(); it != files.end(); ++it)
		{
			relativePath.assign((*it).substr(baseOffset));
			dstPath.assign( _storagePath + relativePath);
			// Create directory
			if (relativePath.back() == '/')
			{
				_fileUtils->createDirectory(dstPath);
			}
			// Copy file
			else
			{
				if (_fileUtils->isFileExist(dstPath))
				{
					_fileUtils->removeFile(dstPath);
				}
				_fileUtils->renameFile(*it, dstPath);
			}

			// Remove from delete list for safe, although this is not the case in general.
			auto diff_itr = diff_map.find(relativePath);
			if (diff_itr != diff_map.end()) {
				diff_map.erase(diff_itr);
			}
		}

		// Preprocessing local files in previous version and creating download folders
		for (auto it = diff_map.begin(); it != diff_map.end(); ++it)
		{
			Manifest::AssetDiff diff = it->second;
			if (diff.type == Manifest::DiffType::DELETED)
			{
				// TODO: Do this when download finish, it don’t matter delete or not.
				std::string exsitedPath = _storagePath + diff.asset.path;
				_fileUtils->removeFile(exsitedPath);
			}
		}
	}
    copyVersions();
}

void AssetsManagerEx::checkUpdate()
{
    if (_updateEntry != UpdateEntry::NONE)
    {
        CCLOGERROR("AssetsManagerEx::checkUpdate, updateEntry isn't NONE");
        return;
    }

    if (!_inited){
        CCLOG("AssetsManagerEx : Manifests uninited.\n");
        dispatchUpdateEvent(EventAssetsManagerEx::EventCode::ERROR_NO_LOCAL_MANIFEST);
        return;
    }
    if (!_localManifest->isLoaded())
    {
        CCLOG("AssetsManagerEx : No local manifest file found error.\n");
        dispatchUpdateEvent(EventAssetsManagerEx::EventCode::ERROR_NO_LOCAL_MANIFEST);
        return;
    }

    _updateEntry = UpdateEntry::CHECK_UPDATE;

    switch (_updateState) {
        case State::FAIL_TO_UPDATE:
            _updateState = State::UNCHECKED;
        case State::UNCHECKED:
        case State::PREDOWNLOAD_VERSION:
        {
            downloadVersion();
        }
            break;
        case State::UP_TO_DATE:
        {
            dispatchUpdateEvent(EventAssetsManagerEx::EventCode::ALREADY_UP_TO_DATE);
        }
            break;
        case State::NEED_UPDATE:
        {
            dispatchUpdateEvent(EventAssetsManagerEx::EventCode::NEW_VERSION_FOUND);
        }
            break;
        default:
            break;
    }
}

void AssetsManagerEx::update()
{
    if (_updateEntry != UpdateEntry::NONE)
    {
        CCLOGERROR("AssetsManagerEx::update, updateEntry isn't NONE");
        return;
    }

    if (!_inited){
        CCLOG("AssetsManagerEx : Manifests uninited.\n");
        dispatchUpdateEvent(EventAssetsManagerEx::EventCode::ERROR_NO_LOCAL_MANIFEST);
        return;
    }
    if (!_localManifest->isLoaded())
    {
        CCLOG("AssetsManagerEx : No local manifest file found error.\n");
        dispatchUpdateEvent(EventAssetsManagerEx::EventCode::ERROR_NO_LOCAL_MANIFEST);
        return;
    }

    _updateEntry = UpdateEntry::DO_UPDATE;

    switch (_updateState) {
        case State::UNCHECKED:
        {
            _updateState = State::PREDOWNLOAD_VERSION;
        }
        case State::PREDOWNLOAD_VERSION:
        {
            downloadVersion();
        }
            break;
        case State::VERSION_LOADED:
        {
            parseVersion();
        }
            break;
        case State::PREDOWNLOAD_MANIFEST:
        {
            downloadManifest();
        }
            break;
        case State::MANIFEST_LOADED:
        {
            parseManifest();
        }
            break;
        case State::FAIL_TO_UPDATE:
        case State::READY_TO_UPDATE:
        case State::NEED_UPDATE:
        {
            // Manifest not loaded yet
            if (!_remoteManifest->isLoaded())
            {
                _updateState = State::PREDOWNLOAD_MANIFEST;
                downloadManifest();
            }
            else if (_updateEntry == UpdateEntry::DO_UPDATE)
            {
                startUpdate();
            }
        }
            break;
        case State::UP_TO_DATE:
        case State::UPDATING:
        case State::UNZIPPING:
            _updateEntry = UpdateEntry::NONE;
            break;
		default:
            break;
    }
}

void AssetsManagerEx::updateAssets(const DownloadUnits& assets)
{
    if (!_inited){
        CCLOG("AssetsManagerEx : Manifests uninited.\n");
        dispatchUpdateEvent(EventAssetsManagerEx::EventCode::ERROR_NO_LOCAL_MANIFEST);
        return;
    }

    if (_updateState != State::UPDATING && _localManifest->isLoaded() && _remoteManifest->isLoaded())
    {
        _updateState = State::UPDATING;
        _downloadUnits.clear();
        _downloadedSize.clear();
        _percent = _percentByFile = _sizeCollected = _totalDownloaded = _totalSize = 0;
        _totalWaitToDownload = _totalToDownload = (int)assets.size();
        _nextSavePoint = 0;
        _totalEnabled = false;
        if (_totalToDownload > 0)
        {
            _downloadUnits = assets;
            this->batchDownload();
        }
        else if (_totalToDownload == 0)
        {
            onDownloadUnitsFinished();
        }
    }
}

const DownloadUnits& AssetsManagerEx::getFailedAssets() const
{
    return _failedUnits;
}

void AssetsManagerEx::downloadFailedAssets()
{
    CCLOG("AssetsManagerEx : Start update %lu failed assets.\n", static_cast<unsigned long>(_failedUnits.size()));
    updateAssets(_failedUnits);
}

void AssetsManagerEx::fileError(const std::string& identifier, const std::string& errorStr, int errorCode, int errorCodeInternal)
{
    auto unitIt = _downloadUnits.find(identifier);
    // Found unit and add it to failed units
    if (unitIt != _downloadUnits.end())
    {
        _totalWaitToDownload--;

        DownloadUnit unit = unitIt->second;
        _failedUnits.emplace(unit.customId, unit);
    }
    dispatchUpdateEvent(EventAssetsManagerEx::EventCode::ERROR_UPDATING, identifier, errorStr, errorCode, errorCodeInternal);
    _tempManifest->setAssetDownloadState(identifier, Manifest::DownloadState::UNSTARTED);

    _currConcurrentTask = std::max(0, _currConcurrentTask-1);
    queueDowload();
}

void AssetsManagerEx::fileSuccess(const std::string &customId, const std::string &storagePath)
{
    // Set download state to SUCCESSED
    _tempManifest->setAssetDownloadState(customId, Manifest::DownloadState::SUCCESSED);

    auto unitIt = _failedUnits.find(customId);
    // Found unit and delete it
    if (unitIt != _failedUnits.end())
    {
        // Remove from failed units list
        _failedUnits.erase(unitIt);
    }

    unitIt = _downloadUnits.find(customId);
    if (unitIt != _downloadUnits.end())
    {
        // Reduce count only when unit found in _downloadUnits
        _totalWaitToDownload--;

        _percentByFile = 100 * (float)(_totalToDownload - _totalWaitToDownload) / _totalToDownload;
        // Notify progression event
        dispatchUpdateEvent(EventAssetsManagerEx::EventCode::UPDATE_PROGRESSION, "");
    }
    // Notify asset updated event
    dispatchUpdateEvent(EventAssetsManagerEx::EventCode::ASSET_UPDATED, customId);

    _currConcurrentTask = std::max(0, _currConcurrentTask-1);
    queueDowload();
}

void AssetsManagerEx::onError(const network::DownloadTask& task,
                              int errorCode,
                              int errorCodeInternal,
                              const std::string& errorStr)
{
    // Skip version error occurred
    if (task.identifier == VERSION_ID)
    {
        CCLOG("AssetsManagerEx : Fail to download version file, step skipped\n");
        _updateState = State::PREDOWNLOAD_MANIFEST;
        downloadManifest();
    }
    else if (task.identifier == MANIFEST_ID)
    {
        dispatchUpdateEvent(EventAssetsManagerEx::EventCode::ERROR_DOWNLOAD_MANIFEST, task.identifier, errorStr, errorCode, errorCodeInternal);
        _updateState = State::FAIL_TO_UPDATE;
    }
    else
    {
        if (_downloadingTask.find(task.identifier) != _downloadingTask.end()) {
            _downloadingTask.erase(task.identifier);
        }
        fileError(task.identifier, errorStr, errorCode, errorCodeInternal);
    }
}

void AssetsManagerEx::onProgress(double total, double downloaded, const std::string& /*url*/, const std::string &customId)
{
    if (customId == VERSION_ID || customId == MANIFEST_ID)
    {
        _percent = 100 * downloaded / total;
        // Notify progression event
        dispatchUpdateEvent(EventAssetsManagerEx::EventCode::UPDATE_PROGRESSION, customId);
        return;
    }
    else
    {
        // Calcul total downloaded
        bool found = false;
        _totalDownloaded = 0;
        for (auto it = _downloadedSize.begin(); it != _downloadedSize.end(); ++it)
        {
            if (it->first == customId)
            {
                it->second = downloaded;
                found = true;
            }
            _totalDownloaded += it->second;
        }
        // Collect information if not registed
        if (!found)
        {
            // Set download state to DOWNLOADING, this will run only once in the download process
            _tempManifest->setAssetDownloadState(customId, Manifest::DownloadState::DOWNLOADING);
            // Register the download size information
            _downloadedSize.emplace(customId, downloaded);
            // Check download unit size existance, if not exist collect size in total size
            if (_downloadUnits[customId].size == 0)
            {
                _totalSize += total;
                _sizeCollected++;
                // All collected, enable total size
                if (_sizeCollected == _totalToDownload)
                {
                    _totalEnabled = true;
                }
            }
        }

        if (_totalEnabled && _updateState == State::UPDATING)
        {
            float currentPercent = 100 * _totalDownloaded / _totalSize;
            // Notify at integer level change
            if ((int)currentPercent != (int)_percent) {
                _percent = currentPercent;
                // Notify progression event
                dispatchUpdateEvent(EventAssetsManagerEx::EventCode::UPDATE_PROGRESSION, customId);
            }
        }
    }
}

void AssetsManagerEx::onSuccess(const std::string &/*srcUrl*/, const std::string &storagePath, const std::string &customId)
{
    if (customId == VERSION_ID)
    {
        _updateState = State::VERSION_LOADED;
        parseVersion();
    }
    else if (customId == MANIFEST_ID)
    {
        _updateState = State::MANIFEST_LOADED;
        parseManifest();
    }
    else
    {
        if (_downloadingTask.find(customId) != _downloadingTask.end()) {
            _downloadingTask.erase(customId);
        }

        bool ok = true;
        auto &assets = _remoteManifest->getAssets();
        auto assetIt = assets.find(customId);
        if (assetIt != assets.end())
        {
            Manifest::Asset asset = assetIt->second;
            if (_verifyCallback != nullptr)
            {
                ok = _verifyCallback(storagePath, asset);
            }
        }

        if (ok)
        {
			auto isCompressed = [&]( )->bool{
				if (assetIt != assets.end()) {
					return assetIt->second.compressed;
				}
				else {
					auto unitIt = _downloadUnits.find(customId);
					if (unitIt != _downloadUnits.end()){
						return unitIt->second.compressed;
					}
				}
				return false;
			};
			bool compressed = isCompressed();//assetIt != assets.end() ? assetIt->second.compressed : false;
            if (compressed)
            {
				_tempManifest->setAssetDownloadState(customId, Manifest::UNZIP);
				_tempManifest->saveToFile(_tempManifestPath);
                decompressDownloadedZip(customId, storagePath);
            }
            else
            {
				
                fileSuccess(customId, storagePath);
            }
        }
        else
        {
            fileError(customId, "Asset file verification failed after downloaded");
        }
    }
}

void AssetsManagerEx::destroyDownloadedVersion()
{
    removeCachedDirectory();
	removeTempDirectory();
}

void AssetsManagerEx::batchDownload()
{
    _queue.clear();
    for(auto iter : _downloadUnits)
    {
        const DownloadUnit& unit = iter.second;
        if (unit.size > 0)
        {
            _totalSize += unit.size;
            _sizeCollected++;
        }

        _queue.push_back(iter.first);
    }
    // All collected, enable total size
    if (_sizeCollected == _totalToDownload)
    {
        _totalEnabled = true;
    }

    queueDowload();
}

void AssetsManagerEx::queueDowload()
{
    if (_totalWaitToDownload == 0 || (_canceled && _currConcurrentTask == 0))
    {
        this->onDownloadUnitsFinished();
        return;
    }

    while (_currConcurrentTask < _maxConcurrentTask && _queue.size() > 0 && !_canceled)
    {
        std::string key = _queue.back();
        _queue.pop_back();

        _currConcurrentTask++;
        DownloadUnit& unit = _downloadUnits[key];
        _fileUtils->createDirectory(basename(unit.storagePath));
        auto downloadTask = _downloader->createDownloadFileTask(unit.srcUrl, unit.storagePath, unit.customId);
        _downloadingTask.emplace(unit.customId, downloadTask);
        _tempManifest->setAssetDownloadState(key, Manifest::DownloadState::DOWNLOADING);
    }
    if (_percentByFile / 100 > _nextSavePoint)
    {
        // Save current download manifest information for resuming
		auto dir = basename(_tempManifestPath);
		if (!_fileUtils->isDirectoryExist(dir)) {
			_fileUtils->createDirectory(dir);
		}
        _tempManifest->saveToFile(_tempManifestPath);
        _nextSavePoint += SAVE_POINT_INTERVAL;
    }
}

void AssetsManagerEx::onDownloadUnitsFinished()
{
    // Always save current download manifest information for resuming
	auto dir = basename(_tempManifestPath);
	if (!_fileUtils->isDirectoryExist(dir)) {
		_fileUtils->createDirectory(dir);
	}
    _tempManifest->saveToFile(_tempManifestPath);
    
    // Finished with error check
    if (_failedUnits.size() > 0)
    {
        _updateState = State::FAIL_TO_UPDATE;
        dispatchUpdateEvent(EventAssetsManagerEx::EventCode::UPDATE_FAILED);
    }
    else if (_updateState == State::UPDATING)
    {
        updateSucceed();
    }
}

void AssetsManagerEx::cancelUpdate()
{
    if (_canceled)
	{
        return;
    }
    _canceled = true;
    std::vector<std::shared_ptr<const network::DownloadTask>> tasks;
    for (const auto& it : _downloadingTask)
    {
        tasks.push_back(it.second);
    }
    for (const auto& it : tasks)
    {
        _downloader->abort(*it);
    }
    _downloadingTask.clear();
}

NS_CC_EXT_END
