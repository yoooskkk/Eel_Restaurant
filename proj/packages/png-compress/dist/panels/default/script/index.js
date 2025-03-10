"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
const fs_1 = require("fs");
const path_1 = require("path");
const Helper_1 = require("../../../Helper");
let view = null;
let panel = null;
module.exports = Editor.Panel.extend({
    template: (0, fs_1.readFileSync)((0, path_1.join)(__dirname, '../../../../static/template/default/index.html'), 'utf-8'),
    style: (0, fs_1.readFileSync)((0, path_1.join)(__dirname, '../../../../static/style/default/index.css'), 'utf-8'),
    $: {
        startCompressBtn: "#startCompressBtn",
        saveBtn: "#saveBtn"
    },
    messages: {
        //更新进度
        updateProgess(sender, progress) {
            if (view) {
                view.progress = progress;
                if (progress >= 100) {
                    panel.$saveBtn.disabled = false;
                    panel.$startCompressBtn.disabled = false;
                    Helper_1.helper.data.isProcessing = false;
                    Helper_1.helper.save();
                }
            }
        },
        //压缩开始
        onStartCompress() {
            if (view) {
                panel.$saveBtn.disabled = true;
                panel.$startCompressBtn.disabled = true;
                view.progress = 0;
            }
        },
        //构建目录
        onSetBuildDir(sender, dir) {
            if (view) {
                view.buildAssetsDir = dir;
            }
        }
    },
    ready() {
        panel = this;
        let sourcePath = (0, path_1.join)(Editor.Project.path, "assets");
        const vm = new window.Vue({
            data() {
                return {
                    enabled: Helper_1.helper.data.enabled,
                    enabledNoFound: Helper_1.helper.data.enabledNoFound,
                    minQuality: Helper_1.helper.data.minQuality,
                    maxQuality: Helper_1.helper.data.maxQuality,
                    speed: Helper_1.helper.data.speed,
                    excludeFolders: Helper_1.helper.data.excludeFolders,
                    excludeFiles: Helper_1.helper.data.excludeFiles,
                    progress: 0,
                    buildAssetsDir: "",
                    sourceAssetsDir: sourcePath,
                };
            },
            methods: {
                onChangeEnabled(enabled) {
                    // console.log("enabled",enabled);
                    Helper_1.helper.data.enabled = enabled;
                    Helper_1.helper.save();
                },
                onChangeEnabledNoFound(enabled) {
                    Helper_1.helper.data.enabledNoFound = enabled;
                    Helper_1.helper.save();
                },
                onChangeMinQuality(value) {
                    // console.log("minQuality",value);
                    Helper_1.helper.data.minQuality = value;
                },
                onChangeMaxQuality(value) {
                    // console.log("maxQuality",value)
                    Helper_1.helper.data.maxQuality = value;
                },
                onChangeSpeed(value) {
                    // console.log("speed",value);
                    Helper_1.helper.data.speed = value;
                },
                onInputExcludeFoldersOver(value) {
                    // console.log("excludeFolders",value);
                    Helper_1.helper.data.excludeFolders = value;
                },
                onInputExcludeFilesOver(value) {
                    // console.log(`excludeFiles`,value);
                    Helper_1.helper.data.excludeFiles = value;
                },
                /**@description 保存配置 */
                onSaveConfig() {
                    if (Helper_1.helper.data.isProcessing) {
                        Helper_1.helper.logger.warn(`${Helper_1.helper.module}处理过程中，请不要操作`);
                        return;
                    }
                    Helper_1.helper.save();
                },
                onStartCompress() {
                    if (Helper_1.helper.data.isProcessing) {
                        Helper_1.helper.logger.warn(`${Helper_1.helper.module}处理过程中，请不要操作`);
                        return;
                    }
                    let view = this;
                    Helper_1.helper.data.isProcessing = true;
                    Helper_1.helper.startCompress(view.sourceAssetsDir);
                },
                onOpenBulidOutDir() {
                    let view = this;
                    let buildDir = view.buildAssetsDir;
                    if (!!!buildDir) {
                        buildDir = Editor.Project.path;
                    }
                    Editor.Dialog.openFile({
                        title: "打开构建目录",
                        defaultPath: buildDir,
                        properties: ["openDirectory"]
                    });
                },
                onOpenSourceAssetsDir() {
                    let view = this;
                    let sourceDir = view.sourceAssetsDir;
                    if (!!!sourceDir) {
                        sourceDir = Editor.Project.path;
                    }
                    Editor.Dialog.openFile({
                        title: "打开构建目录",
                        defaultPath: sourceDir,
                        properties: ["openDirectory"]
                    });
                }
            },
            created: function () {
                view = this;
                panel.$saveBtn.disabled = Helper_1.helper.data.isProcessing;
                panel.$startCompressBtn.disabled = Helper_1.helper.data.isProcessing;
            },
            mounted: function () {
            },
            el: panel.shadowRoot
        });
    },
    beforeClose() { },
    close() { },
});
