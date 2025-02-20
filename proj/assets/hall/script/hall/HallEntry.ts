
/**
 * @description 登录流程 , 不用导出
 */

import { ViewZOrder } from "../../../scripts/common/config/Config";
import { Entry } from "../../../scripts/framework/core/entry/Entry";
import { Singleton } from "../../../scripts/framework/utils/Singleton";
import HallView from "./view/HallView";



class HallEntry extends Entry {
    // static bundle = Macro.BUNDLE_RESOURCES;
    /**@description 是否是主包入口，只能有一个主包入口 */
    // isMain = false;
    static bundle = "hall";

    protected addNetHandler(): void {

    }
    protected removeNetHandler(): void {

    }
    protected loadResources(completeCb: () => void): void {
        completeCb();
    }
    protected openGameView(): void {
        Manager.uiManager.open({ type: HallView, zIndex: ViewZOrder.zero, bundle: this.bundle });
        Manager.entryManager.onCheckUpdate();
    }
    protected closeGameView(): void {
        Manager.uiManager.close(HallView);
    }
    protected initData(): void {
    }
    protected pauseMessageQueue(): void {

    }
    protected resumeMessageQueue(): void {

    }


    /**@description 管理器通知自己进入GameView */
    onEnter(userData?: any) {
        super.onEnter(userData);
        Log.e(`--------------onEnterHall--------------`);
    }

    /**@description 这个位置说明自己GameView 进入onLoad完成 */
    onEnterGameView(gameView: GameView) {
        super.onEnterGameView(gameView);
        //关闭除登录之外的界面
        Manager.uiManager.closeExcept([HallView]);
        Singleton.instance.destory();
    }

    /**@description 卸载bundle,即在自己bundle删除之前最后的一条消息 */
    onUnloadBundle() {
        //移除本模块网络事件
        this.removeNetHandler();
        //卸载资源
        this.unloadResources();
    }
}
Manager.entryManager.register(HallEntry);
