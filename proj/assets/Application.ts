import Alert from "./scripts/common/component/Alert";
import GlobalAudio from "./scripts/common/component/GlobalAudio";
import Loading from "./scripts/common/component/Loading";
import Tips from "./scripts/common/component/Tips";
import UILoading from "./scripts/common/component/UILoading";
import { UIReconnect } from "./scripts/common/component/UIReconnect";
import UpdateLoading from "./scripts/common/component/UpdateLoading";
import { Config } from "./scripts/common/config/Config";
import { StageData } from "./scripts/common/data/StageData";
import { CmmEntry } from "./scripts/common/entry/CmmEntry";
import { CommonLanguage } from "./scripts/common/language/CommonLanguage";
import { Utils } from "./scripts/common/utils/Utils";
import { LogLevel } from "./scripts/framework/defines/Enums";
import { Framewok } from "./scripts/framework/Framework";
import { Singleton } from "./scripts/framework/utils/Singleton";

/**@description 游戏所有运行单例的管理 */
export class _Manager extends Framewok implements GameEventInterface {

    get isLazyRelease(){
        Log.w(`需要使用都自己导出cc.game.EVENT_LOW_MEMORY事件`);
        return true;
    }

    get utils(){
        return Singleton.instance.get(Utils) as Utils;
    }

    /**@description 进入后台的时间 */
    private _enterBackgroundTime = 0;

    /**@description 重连专用提示UI部分 */
    get uiReconnect( ){
        return Singleton.instance.get(UIReconnect) as UIReconnect;
    }

    /**@description 小提示 */
    get tips() {
        return Singleton.instance.get(Tips) as Tips;
    }

    /**@description 界面加载时的全屏Loading,显示加载进度 */
    get uiLoading(): UILoading {
        return Singleton.instance.get(UILoading) as UILoading;
    }

    /**@description 弹出提示框,带一到两个按钮 */
    get alert() {
        return Singleton.instance.get(Alert) as Alert;
    }

    /**@description 公共loading */
    get loading() {
        return Singleton.instance.get(Loading) as Loading;
    }

    get updateLoading(){
        return Singleton.instance.get(UpdateLoading) as UpdateLoading;
    }

    /**@description 获取Stage数据 */
    get stageData(){
        return this.dataCenter.get(StageData) as StageData;
    }

    private _wssCacertUrl = "";
    /**@description websocket wss 证书url地址 */
    set wssCacertUrl(value) {
        this._wssCacertUrl = value;
    }
    get wssCacertUrl() {
        return this._wssCacertUrl;
    }

    /**@description 全局网络播放声音组件，如播放按钮音效，弹出框音效等 */
    private _globalAudio: GlobalAudio = null!;
    get globalAudio() {
        if (this._globalAudio) {
            return this._globalAudio;
        }
        this._globalAudio = this.uiManager.addComponent(GlobalAudio);
        return this._globalAudio;
    }

    init() {
        super.init();
        this.updateManager.hotUpdateUrl = Config.HOT_UPDATE_URL;
        this.updateManager.isAutoVersion = Config.USE_AUTO_VERSION;
        this.updateManager.isSkipCheckUpdate = Config.isSkipCheckUpdate;

        //初始化自定主entry代理
        this.entryManager.delegate = new CmmEntry();

        //语言包初始化
        //cc.log("language init");
        this.language.addSourceDelegate(new CommonLanguage);
    }

    onLoad(node: cc.Node) {
        //预先加载下loading预置体
        Manager.uiManager.onLoad(node);
        //Service onLoad
        Manager.serviceManager.onLoad();
        //入口管理器
        Manager.entryManager.onLoad(node);
    }

    update(node: cc.Node) {
        //Service 网络调试
        Manager.serviceManager.update();

        //远程资源下载任务调度
        Manager.asset.remote.update();
    }

    onDestroy(node: cc.Node) {
        Manager.serviceManager.onDestroy();
        //入口管理器
        Manager.entryManager.onDestroy(node);
    }

    onEnterBackground(): void {
        this._enterBackgroundTime = Date.timeNow();
        Log.d(`[MainController]`, `onEnterBackground ${this._enterBackgroundTime}`);
        Manager.globalAudio.onEnterBackground();
        Manager.serviceManager.onEnterBackground();
    }
    onEnterForgeground(): void {
        let now = Date.timeNow();
        let inBackgroundTime = now - this._enterBackgroundTime;
        Log.d(`[MainController]`, `onEnterForgeground ${now} background total time : ${inBackgroundTime}`);
        Manager.globalAudio.onEnterForgeground(inBackgroundTime);
        Manager.serviceManager.onEnterForgeground(inBackgroundTime);
    }
}

let mgr = new _Manager();
mgr.logger.level = LogLevel.ALL;
(<any>window)["Manager"] = mgr;
mgr.init();