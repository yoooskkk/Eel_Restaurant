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

#include "Manifest.h"
#include "json/prettywriter.h"
#include "json/stringbuffer.h"

#include <fstream>
#include <stdio.h>

#define KEY_VERSION             "version"
#define KEY_PACKAGE_URL         "packageUrl"
#define KEY_MANIFEST_URL        "remoteManifestUrl"
#define KEY_VERSION_URL         "remoteVersionUrl"
#define KEY_GROUP_VERSIONS      "groupVersions"
#define KEY_ENGINE_VERSION      "engineVersion"
#define KEY_UPDATING            "updating"
#define KEY_ASSETS              "assets"
#define KEY_COMPRESSED_FILES    "compressedFiles"
#define KEY_SEARCH_PATHS        "searchPaths"

#define KEY_PATH                "path"
#define KEY_MD5                 "md5"
#define KEY_GROUP               "group"
#define KEY_COMPRESSED          "compressed"
#define KEY_SIZE                "size"
#define KEY_COMPRESSED_FILE     "compressedFile"
#define KEY_DOWNLOAD_STATE      "downloadState"

#define KEY_BUNDLE			"bundle"
#define VERSION_FILENAME    "_version.json"
#define MANIFEST_FILENAME   "_project.json"
#define MANIFEST_ROOT	    "manifest/"

NS_CC_EXT_BEGIN

static int cmpVersion(const std::string& v1, const std::string& v2)
{
    int i;
    int oct_v1[4] = {0}, oct_v2[4] = {0};
    int filled1 = std::sscanf(v1.c_str(), "%d.%d.%d.%d", &oct_v1[0], &oct_v1[1], &oct_v1[2], &oct_v1[3]);
    int filled2 = std::sscanf(v2.c_str(), "%d.%d.%d.%d", &oct_v2[0], &oct_v2[1], &oct_v2[2], &oct_v2[3]);
    
    if (filled1 == 0 || filled2 == 0)
    {
        return strcmp(v1.c_str(), v2.c_str());
    }
    for (i = 0; i < 4; i++)
    {
        if (oct_v1[i] > oct_v2[i])
            return 1;
        else if (oct_v1[i] < oct_v2[i])
            return -1;
    }
    return 0;
}

Manifest::Manifest(const std::string& manifestUrl/* = ""*/)
: _versionLoaded(false)
, _loaded(false)
, _updating(false)
, _manifestRoot("")
, _version("")
, _engineVer("")
, _packageUrl("")
, _bundle("")
, _md5("")
, _zipSize(0)
{
    // Init variables
    _fileUtils = FileUtils::getInstance();
    if (manifestUrl.size() > 0)
        parseFile(manifestUrl);
}

Manifest::Manifest(const std::string& content, const std::string& manifestRoot)
: _versionLoaded(false)
, _loaded(false)
, _updating(false)
, _manifestRoot("")
, _version("")
, _engineVer("")
, _packageUrl("")
, _bundle("")
, _md5("")
, _zipSize(0)
{
    // Init variables
    _fileUtils = FileUtils::getInstance();
    if (content.size() > 0)
        parseJSONString(content, manifestRoot);
}

Manifest::Manifest(const std::string& content, const std::string& manifestRoot, const std::string& packageUrl)
	: _versionLoaded(false)
	, _loaded(false)
	, _updating(false)
	, _manifestRoot("")
	, _version("")
	, _engineVer("")
	, _packageUrl("")
    , _bundle("")
    , _md5("")
	, _zipSize(0)
{
	// Init variables
	_fileUtils = FileUtils::getInstance();
	setPackageUrl(packageUrl);
	if (content.size() > 0)
		parseJSONString(content, manifestRoot);
}

void Manifest::loadJson(const std::string& url)
{
    clear();
    std::string content;
    if (_fileUtils->isFileExist(url))
    {
        // Load file content
        content = _fileUtils->getStringFromFile(url);
        
        if (content.size() == 0)
        {
            CCLOG("Fail to retrieve local file content: %s\n", url.c_str());
        }
        else
        {
            loadJsonFromString(content);
        }
    }
}

void Manifest::loadJsonFromString(const std::string& content)
{
    if (content.size() == 0)
    {
        CCLOG("Fail to parse empty json content.");
    }
    else
    {
        // Parse file with rapid json
        _json.Parse<0>(content.c_str());
        // Print error
        if (_json.HasParseError()) {
            size_t offset = _json.GetErrorOffset();
            if (offset > 0)
                offset--;
            std::string errorSnippet = content.substr(offset, 10);
            CCLOG("File parse error %d at <%s>\n", _json.GetParseError(), errorSnippet.c_str());
        }
    }
}

void Manifest::parseVersion(const std::string& versionUrl)
{
    loadJson(versionUrl);
    
    if (_json.IsObject())
    {
        loadVersion(_json);
    }
}

void Manifest::parseFile(const std::string& manifestUrl)
{
    loadJson(manifestUrl);
	
    if (!_json.HasParseError() && _json.IsObject())
    {
        // Register the local manifest root
        size_t found = manifestUrl.find(MANIFEST_ROOT);
        if (found != std::string::npos)
        {
            _manifestRoot = manifestUrl.substr(0, found);
        }
        loadManifest(_json);
    }
}

void Manifest::parseJSONString(const std::string& content, const std::string& manifestRoot)
{
    loadJsonFromString(content);
    
    if (!_json.HasParseError() && _json.IsObject())
    {
        // Register the local manifest root
        _manifestRoot = manifestRoot;
        loadManifest(_json);
    }
}

bool Manifest::isVersionLoaded() const
{
    return _versionLoaded;
}
bool Manifest::isLoaded() const
{
    return _loaded;
}

void Manifest::setUpdating(bool updating)
{
    if (_loaded && _json.IsObject())
    {
        if (_json.HasMember(KEY_UPDATING) && _json[KEY_UPDATING].IsBool())
        {
            _json[KEY_UPDATING].SetBool(updating);
        }
        else
        {
            _json.AddMember<bool>(KEY_UPDATING, updating, _json.GetAllocator());
        }
        _updating = updating;
    }
}

bool Manifest::versionEquals(const Manifest *b) const {
    // Check manifest version
    if (_version != b->getVersion())
    {
        return false;
    }
    // Check group versions
    else
    {
        std::vector<std::string> bGroups = b->getGroups();
        std::unordered_map<std::string, std::string> bGroupVer = b->getGroupVerions();
        // Check group size
        if (bGroups.size() != _groups.size())
            return false;
        
        // Check groups version
        for (unsigned int i = 0; i < _groups.size(); ++i) {
            std::string gid =_groups[i];
            // Check group name
            if (gid != bGroups[i])
                return false;
            // Check group version
            if (_groupVer.at(gid) != bGroupVer.at(gid))
                return false;
        }
    }
    return true;
}

bool Manifest::versionGreaterOrEquals(const Manifest *b, const std::function<int(const std::string& versionA, const std::string& versionB)>& handle) const
{
    std::string localVersion = getVersion();
    std::string bVersion = b->getVersion();
    bool greater;
    if (handle)
    {
        greater = handle(localVersion, bVersion) >= 0;
    }
    else
    {
        greater = cmpVersion(localVersion, bVersion) >= 0;
    }
    return greater;
}

bool Manifest::versionGreater(const Manifest *b, const std::function<int(const std::string& versionA, const std::string& versionB)>& handle) const
{
    std::string localVersion = getVersion();
    std::string bVersion = b->getVersion();
    bool greater;
    if (handle)
    {
        greater = handle(localVersion, bVersion) > 0;
    }
    else
    {
        greater = cmpVersion(localVersion, bVersion) > 0;
    }
    return greater;
}

bool Manifest::equal(const Manifest*b) const {
	auto localMd5 = getMd5();
	auto bMd5 = b->getMd5();
	if ( localMd5 == bMd5 ){
		return true;
	}
	return false;
}

std::unordered_map<std::string, Manifest::AssetDiff> Manifest::genDiff(const Manifest *b) const
{
    std::unordered_map<std::string, AssetDiff> diff_map;
    const std::unordered_map<std::string, Asset> &bAssets = b->getAssets();
    
    std::string key;
    Asset valueA;
    Asset valueB;
    
    std::unordered_map<std::string, Asset>::const_iterator valueIt, it;
    for (it = _assets.begin(); it != _assets.end(); ++it)
    {
        key = it->first;
        valueA = it->second;
        
        // Deleted
        valueIt = bAssets.find(key);
        if (valueIt == bAssets.cend()) {
            AssetDiff diff;
            diff.asset = valueA;
            diff.type = DiffType::DELETED;
            diff_map.emplace(key, diff);
            continue;
        }
        
        // Modified
        valueB = valueIt->second;
        if (valueA.md5 != valueB.md5) {
            AssetDiff diff;
            diff.asset = valueB;
            diff.type = DiffType::MODIFIED;
            diff_map.emplace(key, diff);
        }
    }
    
    for (it = bAssets.begin(); it != bAssets.end(); ++it)
    {
        key = it->first;
        valueB = it->second;
        
        // Added
        valueIt = _assets.find(key);
        if (valueIt == _assets.cend()) {
            AssetDiff diff;
            diff.asset = valueB;
            diff.type = DiffType::ADDED;
            diff_map.emplace(key, diff);
        }
    }
    
    return diff_map;
}

void Manifest::genResumeAssetsList(DownloadUnits *units , bool& unzip) const
{
    unzip = false;
    for (auto it = _assets.begin(); it != _assets.end(); ++it)
    {
        Asset asset = it->second;
        
        if (asset.downloadState != DownloadState::SUCCESSED && asset.downloadState != DownloadState::UNMARKED)
        {
            DownloadUnit unit;
            unit.customId = it->first;
			if (asset.compressed) {
				
				unit.srcUrl = getPackageUrl() + "zips/" + asset.path;
			}
			else {
				unit.srcUrl = getPackageUrl() + asset.path + "?md5=" + asset.md5;
			}
            
            unit.storagePath = _manifestRoot + asset.path;
            unit.size = asset.size;
            unit.compressed = asset.compressed;
            if (asset.downloadState == DownloadState::UNZIP && _fileUtils->isFileExist(unit.storagePath)){
                unzip = true;
            }
            units->emplace(unit.customId, unit);
        }
    }
}

std::vector<std::string> Manifest::getSearchPaths() const
{
    std::vector<std::string> searchPaths;
    searchPaths.push_back(_manifestRoot);
    
    for (int i = (int)_searchPaths.size()-1; i >= 0; i--)
    {
        std::string path = _searchPaths[i];
        if (path.size() > 0 && path[path.size() - 1] != '/')
            path.append("/");
        path = _manifestRoot + path;
        searchPaths.push_back(path);
    }
    return searchPaths;
}

void Manifest::prependSearchPaths()
{
    std::vector<std::string> searchPaths = FileUtils::getInstance()->getSearchPaths();
    std::vector<std::string>::iterator iter = searchPaths.begin();
    bool needChangeSearchPaths = false;
    if (std::find(searchPaths.begin(), searchPaths.end(), _manifestRoot) == searchPaths.end())
    {
        searchPaths.insert(iter, _manifestRoot);
        needChangeSearchPaths = true;
    }
    
    for (int i = (int)_searchPaths.size()-1; i >= 0; i--)
    {
        std::string path = _searchPaths[i];
        if (path.size() > 0 && path[path.size() - 1] != '/')
            path.append("/");
        path = _manifestRoot + path;
        iter = searchPaths.begin();
        searchPaths.insert(iter, path);
        needChangeSearchPaths = true;
    }
    if (needChangeSearchPaths)
    {
        FileUtils::getInstance()->setSearchPaths(searchPaths);
    }
}


const std::string& Manifest::getPackageUrl() const
{
    return _packageUrl;
}

const void Manifest::setPackageUrl(const std::string& packageUrl) {
	_packageUrl = packageUrl;
	// Append automatically "/"
	if (_packageUrl.size() > 0 && _packageUrl[_packageUrl.size() - 1] != '/')
	{
		_packageUrl.append("/");
	}
}

const std::string Manifest::getManifestFileUrl() const {
	// Add a timestamp to ensure the fetch is up-to-date
    return getPackageUrl() + MANIFEST_ROOT + _bundle + MANIFEST_FILENAME + "?time=" + std::to_string(time(nullptr));
}

const std::string Manifest::getVersionFileUrl() const {
	// Add a timestamp to ensure the fetch is up-to-date
    return getPackageUrl() + MANIFEST_ROOT + _bundle + VERSION_FILENAME + "?time=" + std::to_string(time(nullptr));
}

const std::string& Manifest::getVersion() const
{
    return _version;
}

const std::string &Manifest::getMd5() const {
	return _md5;
}

const std::vector<std::string>& Manifest::getGroups() const
{
    return _groups;
}

const std::unordered_map<std::string, std::string>& Manifest::getGroupVerions() const
{
    return _groupVer;
}

const std::string& Manifest::getGroupVersion(const std::string &group) const
{
    return _groupVer.at(group);
}

const std::unordered_map<std::string, Manifest::Asset>& Manifest::getAssets() const
{
    return _assets;
}

void Manifest::setAssetDownloadState(const std::string &key, const Manifest::DownloadState &state)
{
    auto valueIt = _assets.find(key);
    if (valueIt != _assets.end())
    {
        valueIt->second.downloadState = state;
        
        // Update json object
        if(_json.IsObject())
        {
            if ( _json.HasMember(KEY_ASSETS) )
            {
                rapidjson::Value &assets = _json[KEY_ASSETS];
                if (assets.IsObject())
                {
                    if (assets.HasMember(key.c_str()))
                    {
                        rapidjson::Value &entry = assets[key.c_str()];
                        if (entry.HasMember(KEY_DOWNLOAD_STATE) && entry[KEY_DOWNLOAD_STATE].IsInt())
                        {
                            entry[KEY_DOWNLOAD_STATE].SetInt((int) state);
                        }
                        else
                        {
                            entry.AddMember<int>(KEY_DOWNLOAD_STATE, (int)state, _json.GetAllocator());
                        }
                    }
                }
            }
        }
    }
}

void Manifest::updateToZipAsset(const DownloadUnit& unit) {
	Asset asset;
	asset.compressed = unit.compressed;
	asset.md5 = _md5;
	asset.path = _bundle + "_" + _md5 + ".zip";
	asset.size = unit.size;
	// Update json object
	if (_json.IsObject()) {
		if (_json.HasMember(KEY_ASSETS)) {
			rapidjson::Value &assets = _json[KEY_ASSETS];
			if (assets.IsObject()) {
				auto key = unit.customId.data();
				if (!assets.HasMember(key)) {
					rapidjson::Value value(rapidjson::Type::kObjectType);
					value.SetObject();
					rapidjson::Value name(rapidjson::Type::kStringType);
					name.SetString(key, unit.customId.size(), _json.GetAllocator());
					assets.AddMember(name, value, _json.GetAllocator());
					
					rapidjson::Value &entry = assets[unit.customId.c_str()];
					entry.AddMember<bool>(KEY_COMPRESSED, (int)asset.compressed, _json.GetAllocator());
				
                    entry.AddMember<int>(KEY_SIZE, (int)asset.size, _json.GetAllocator());
                }
			}
		}
	}
	_assets.emplace(unit.customId, asset);
}

void Manifest::clear()
{
    if (_versionLoaded || _loaded)
    {
        _groups.clear();
        _groupVer.clear();

        _version = "";
        _engineVer = "";
        
        _versionLoaded = false;
    }
    
    if (_loaded)
    {
        _assets.clear();
        _searchPaths.clear();
        _loaded = false;
    }
}

Manifest::Asset Manifest::parseAsset(const std::string &path, const rapidjson::Value &json)
{
    Asset asset;
    asset.path = path;
	
    if ( json.HasMember(KEY_MD5) && json[KEY_MD5].IsString() )
    {
        asset.md5 = json[KEY_MD5].GetString();
    }
    else asset.md5 = "";
    
    if ( json.HasMember(KEY_PATH) && json[KEY_PATH].IsString() )
    {
        asset.path = json[KEY_PATH].GetString();
    }
    
    if ( json.HasMember(KEY_COMPRESSED) && json[KEY_COMPRESSED].IsBool() )
    {
        asset.compressed = json[KEY_COMPRESSED].GetBool();
    }
    else asset.compressed = false;
    
    if ( json.HasMember(KEY_SIZE) && json[KEY_SIZE].IsInt() )
    {
        asset.size = json[KEY_SIZE].GetInt();
    }
    else asset.size = 0;
    
    if ( json.HasMember(KEY_DOWNLOAD_STATE) && json[KEY_DOWNLOAD_STATE].IsInt() )
    {
        asset.downloadState = (json[KEY_DOWNLOAD_STATE].GetInt());
    }
    else asset.downloadState = DownloadState::UNMARKED;
    
    return asset;
}

void Manifest::loadVersion(const rapidjson::Document &json)
{
    
    // Retrieve local version
    if ( json.HasMember(KEY_VERSION) && json[KEY_VERSION].IsString() )
    {
        _version = json[KEY_VERSION].GetString();
    }
    
    // Retrieve local group version
    if ( json.HasMember(KEY_GROUP_VERSIONS) )
    {
        const rapidjson::Value& groupVers = json[KEY_GROUP_VERSIONS];
        if (groupVers.IsObject())
        {
            for (rapidjson::Value::ConstMemberIterator itr = groupVers.MemberBegin(); itr != groupVers.MemberEnd(); ++itr)
            {
                std::string group = itr->name.GetString();
                std::string version = "0";
                if (itr->value.IsString())
                {
                    version = itr->value.GetString();
                }
                _groups.push_back(group);
                _groupVer.emplace(group, version);
            }
        }
    }
    
    // Retrieve local engine version
    if ( json.HasMember(KEY_ENGINE_VERSION) && json[KEY_ENGINE_VERSION].IsString() )
    {
        _engineVer = json[KEY_ENGINE_VERSION].GetString();
    }
    
    // Retrieve updating flag
    if ( json.HasMember(KEY_UPDATING) && json[KEY_UPDATING].IsBool() )
    {
        _updating = json[KEY_UPDATING].GetBool();
    }

	// Retrieve package url
	if (json.HasMember(KEY_PACKAGE_URL) && json[KEY_PACKAGE_URL].IsString())
	{
		setPackageUrl(json[KEY_PACKAGE_URL].GetString());
	}

	//md5
	if (json.HasMember(KEY_MD5) && json[KEY_MD5].IsString()) {
		_md5 = json[KEY_MD5].GetString();
	}
	else {
		_md5 = "";
	}
	CCASSERT(_md5 != "", "Md5 is empty!!!!");

	//bundle
	if (json.HasMember(KEY_BUNDLE) && json[KEY_BUNDLE].IsString()) {
		_bundle = json[KEY_BUNDLE].GetString();
	}
	else {
		_bundle = "";
	}
	CCASSERT(_bundle != "", "Bundle is empty!!!!");

    //zip文件大小
	if (json.HasMember(KEY_SIZE) && json[KEY_SIZE].IsInt()) {
		_zipSize = json[KEY_SIZE].GetInt();
	}

    _versionLoaded = true;
}

void Manifest::loadManifest(const rapidjson::Document &json)
{
    loadVersion(json);
    
    // Retrieve all assets
    if ( json.HasMember(KEY_ASSETS) )
    {
        const rapidjson::Value& assets = json[KEY_ASSETS];
        if (assets.IsObject())
        {
            for (rapidjson::Value::ConstMemberIterator itr = assets.MemberBegin(); itr != assets.MemberEnd(); ++itr)
            {
                std::string key = itr->name.GetString();
                Asset asset = parseAsset(key, itr->value);
                _assets.emplace(key, asset);
            }
        }
    }
    
    // Retrieve all search paths
    if ( json.HasMember(KEY_SEARCH_PATHS) )
    {
        const rapidjson::Value& paths = json[KEY_SEARCH_PATHS];
        if (paths.IsArray())
        {
            for (rapidjson::SizeType i = 0; i < paths.Size(); ++i)
            {
                if (paths[i].IsString()) {
                    _searchPaths.push_back(paths[i].GetString());
                }
            }
        }
    }

    _loaded = true;
}

void Manifest::saveToFile(const std::string &filepath)
{
    rapidjson::StringBuffer buffer;
    rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
    _json.Accept(writer);
    
    std::ofstream output(FileUtils::getInstance()->getSuitableFOpen(filepath), std::ofstream::out);

    if(!output.bad())
        output << buffer.GetString() << std::endl;
}

void Manifest::saveVersionToFile(const std::string& filepath) {
	rapidjson::StringBuffer buffer;
	rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
	if (_json.HasMember(KEY_ASSETS)) {
		_json.EraseMember(KEY_ASSETS);
	}
	_json.Accept(writer);

	std::ofstream output(FileUtils::getInstance()->getSuitableFOpen(filepath), std::ofstream::out);

	if (!output.bad())
		output << buffer.GetString() << std::endl;
}

NS_CC_EXT_END
