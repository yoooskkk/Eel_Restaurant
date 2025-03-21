import { existsSync } from "fs";
import { join } from "path";
import FileUtils from "./core/FileUtils";
import { Handler } from "./core/Handler";
import * as FixEngine from "./fix_engine/Helper";
import * as Gulp from "./gulp-compress/Helper";
import * as Assets from "./core/AssetsHelper";
import * as PngCompress from "./png-compress/Helper";
import * as Hotupdate from "./hotupdate/Helper";
import { Environment } from "./core/Environment";

/**
 * @description 辅助类
 */
export class Helper extends Handler {

    private static _instance: Helper = null!;
    static get instance() {
        return this._instance || (this._instance = new Helper);
    }

    /**@description Bunldes 地址 */
    private readonly bundlesUrl = "https://gitee.com/top-discover/QuickFrameworkBundles.git";

    /**@description 引擎修正 */
    private _fixEngine = new FixEngine.default;

    /**@description Gulp 压缩 */
    private _gulp = new Gulp.default()

    /**@description 资源辅助类 */
    private _assets = new Assets.default();

    /**
     * @description 图片压缩
     */
    private _pngCompress = new PngCompress.default();

    /**
     * @description 热更新
     */
    private _hotupdate = new Hotupdate.default();

    /**@description 获取当前分支信息 */
    private async gitCurBranch() {
        this.chdir(this.projPath);
        let result = await this.exec("git branch");
        if (result.isSuccess) {
            let data: string = result.data;
            let arr = data.split("\n");
            for (let index = 0; index < arr.length; index++) {
                const element = arr[index];
                if (element.startsWith("*")) {
                    let branch = element.match(/\d+\.\d+\.\d+/g);
                    if (branch) {
                        return branch[0];
                    }
                }
            }
        }
        return null;
    }

    /**@description 摘取远程bundles */
    async gitBundles() {
        this.log("摘取远程bundles", false);
        let branch = await this.gitCurBranch();

        if (existsSync(this.bundlesPath)) {
            this.logger.log(`已经存在 : ${this.bundlesPath}`);
            this.chdir(this.bundlesPath);
            let result = await this.exec("git pull");
            this.logger.log(`正在更新 : ${this.bundleName}`)
            if (!result.isSuccess) {
                return;
            }
            result = await this.exec(`git checkout ${branch}`);
            if (result.isSuccess) {
                this.logger.log(`切换分支${branch}成功 : ${this.bundleName}`)
            }
        } else {
            this.logger.log(`不存在 : ${this.bundlesPath}`);
            this.chdir(this.projPath);
            this.logger.log(`拉取远程 : ${this.bundleName}`)
            let result = await this.exec(`git clone ${this.bundlesUrl} ${this.bundleName}`);
            if (!result.isSuccess) {
                return;
            }
            this.logger.log(`摘取成功 : ${this.bundleName}`);
            this.chdir(this.bundlesPath);
            result = await this.exec(`git checkout ${branch}`);
            if (result.isSuccess) {
                this.logger.log(`切换分支${branch}成功 : ${this.bundleName}`)
            }
        }
        this.log("摘取远程bundles", true);
    }

    /**@description 链接代码 */
    symlinkSyncCode() {
        this.log("链接代码", false);
        let fromPath = join(this.bundlesPath, this.bundleName);
        FileUtils.instance.symlinkSync(fromPath, this.assetsBundlesPath)
        this.log("链接代码", true);
    }

    /**
     * @description 链接扩展插件代码
     */
    symlinkSyncExtensions() {
        this.log(`链接扩展插件代码`, false);

        for (let i = 0; i < this.extensions.length; i++) {
            const element = this.extensions[i];
            let toPath = join(this.extensionsPath, `${element}/src/core`);
            let formPath = join(__dirname, `core`);
            //core 部分代码
            if ( Environment.isLinkCore(element)){
                this.logger.log(`链接core`);
                FileUtils.instance.symlinkSync(formPath, toPath);
            }
            
            //node_modules 依赖
            if ( Environment.isLinkNodeModules(element) ){
                this.logger.log(`链接node_modules`);
                formPath = this.node_modules;
                toPath = join(this.extensionsPath,`${element}/node_modules`);
                FileUtils.instance.symlinkSync(formPath,toPath);
            }
           

            //链接实现
            if ( Environment.isLinkImpl(element) ){
                this.logger.log(`链接Impl`);
                formPath = join(__dirname, element);
                toPath = join(this.extensionsPath, `${element}/src/impl`);
                FileUtils.instance.symlinkSync(formPath, toPath);
            }

            //链接声明部分
            this.logger.log(`链接声明部分`);
            formPath = join(__dirname,`../@types`);
            toPath = join(this.extensionsPath,`${element}/@types`);
            FileUtils.instance.symlinkSync(formPath,toPath);
        }

        this.log(`链接扩展插件代码`, true);
    }


    /**@description 引擎修改 */
    async fixEngine() {
        this.log(`引擎修正`,false);
        this._fixEngine.run();
        this.log(`引擎修正`,true);
    }

    async gulp(){
        this.log(`Gulp`,false);
        await this._gulp.run();
        this.log(`Gulp`,true);
    }

    async linkGulp(){
        this.log(`链接 gulpfile.js`,false);
        let path = "gulp-compress/gulpfile.js";
        await FileUtils.instance.copyFile(join(__dirname,path),join(__dirname,`../dist/${path}`));
        this.log(`链接 gulpfile.js`,true);
    }

    async getAssets() {
        await this._assets.getAssets();
    }

    /**
     * @description 图集压缩
     */
    async pngCompress(){
        this.log(`图片压缩`,false);
        await this._pngCompress.onAfterBuild(Environment.build);
        this.log(`图片压缩`,true);
    }

    async hotupdate(){
        this.log(`打包热更新`,false);
        await this._hotupdate.run();
        this.log(`打包热更新`,true);
    }

}