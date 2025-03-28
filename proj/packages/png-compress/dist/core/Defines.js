"use strict";
/**
 * @description 项目扩展声明
 */
Object.defineProperty(exports, "__esModule", { value: true });
exports.Extensions = exports.CmdType = void 0;
var CmdType;
(function (CmdType) {
    /**@description 拉取 Bunldes 代码 */
    CmdType["GitBundles"] = "-gitBundles";
    /**@description 链接Bundles代码 */
    CmdType["Sync"] = "-sync";
    /**@description 链接扩展插件代码 */
    CmdType["Extensions"] = "-extensions";
    /**@description 引擎修改 */
    CmdType["FixEngine"] = "-fixEngine";
    /**@description Gulp 压缩 */
    CmdType["Gulp"] = "-gulp";
    /**@description 链接 gulpfile.js 到dist */
    CmdType["LinkGulp"] = "-linkGulp";
    /**@description 获取工程目录所有资源信息资源 */
    CmdType["Assets"] = "-assets";
    /**@description 压缩图片资源 */
    CmdType["Pngquant"] = "-pngquant";
    /**@description 热更新 */
    CmdType["Hotupdate"] = "-hotupdate";
})(CmdType = exports.CmdType || (exports.CmdType = {}));
var Extensions;
(function (Extensions) {
    /**@description 资源引用检查，目录只对2.x有效,可能兼容上有问题，后续不再维护 */
    Extensions["CheckResources"] = "check_resources";
    /**@description 引擎修正 */
    Extensions["FixEngine"] = "fix_engine";
    /**@description Gulp 压缩 */
    Extensions["GulpCompress"] = "gulp-compress";
    /**@description 热更新 */
    Extensions["Hotupdate"] = "hotupdate";
    /**@description 图个压缩 */
    Extensions["PngCompress"] = "png-compress";
    /**@description 测试服务器 */
    Extensions["TestServer"] = "test-server";
    /**@description 确定删除bundle */
    Extensions["ConfirmDelSubgames"] = "confirm_del_subgames";
})(Extensions = exports.Extensions || (exports.Extensions = {}));
