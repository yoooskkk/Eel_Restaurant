import { GameEvent } from "../../../../scripts/common/event/CommonEvent";
import GameView from "../../../../scripts/framework/core/ui/GameView";
import { Macro } from "../../../../scripts/framework/defines/Macros";

const { ccclass, property } = cc._decorator;

@ccclass
export default class HallView extends GameView {

    static getPrefabUrl() {
        return `prefabs/HallView`;
    }

    shoproom_S: cc.Node
    kitchen_S: cc.Node
    out1_S: cc.Node

    allScense: {
        shoproom: cc.Node,
        kitchen: cc.Node,
        out1: cc.Node
    }

    _kitchbackbtn: cc.Node = null;

    _kitcheninbtn: cc.Node = null;
    _out1inbtn: cc.Node = null;
    _out2btn: cc.Node = null;

    _out1back: cc.Node = null;

    onLoad() {
        super.onLoad();

        this

        this.shoproom_S = cc.find('shoproomscense', this.node);
        this.kitchen_S = cc.find('kitchenscense', this.node);
        this.out1_S = cc.find('out1scense', this.node);
        this.allScense = {
            shoproom: this.shoproom_S,
            kitchen: this.kitchen_S,
            out1: this.out1_S
        }

        this._kitchbackbtn = cc.find("kitchenscense/roombtn", this.node);

        this._kitcheninbtn = cc.find("shoproomscense/kitchenbtn", this.node);
        this._out1inbtn = cc.find("shoproomscense/out1btn", this.node);

        this._out1back = cc.find("out1scense/out1backbtn", this.node);
        // let version = cc.find("version", this.node)?.getComponent(cc.Label);
        // if (version) {
        //     version.string = Manager.updateManager.getVersion(this.bundle);
        // }
        //进入厨房
        this._kitcheninbtn.on(cc.Node.EventType.TOUCH_END, () => {
            // this.enterBundle(Macro.BUNDLE_SUBGAMEONE);
            this.scenecMananger(2)
        });
        //进入外场景1
        this._out1inbtn.on(cc.Node.EventType.TOUCH_END, () => {
            this.scenecMananger(3)
        });

        //返回商店
        this._kitchbackbtn.on(cc.Node.EventType.TOUCH_END, () => {
            this.scenecMananger(1)
        });
        //返回商店
        this._out1back.on(cc.Node.EventType.TOUCH_END, () => {
            this.scenecMananger(1)
        });

        let nd = new cc.Node()
        cc.tween(nd).delay(3).call(() => {
            dispatch(GameEvent.CHAT_SERVICE_CLOSE, { abc: 11 });
        }).start();



        //初始话场景
        this.scenecMananger(1)

    }

    scenecMananger(curScenseIdx: number = 0) {
        for (const key in this.allScense) {
            this.allScense[key].active = false;
        }
        switch (curScenseIdx) {
            case 1:
                this.allScense.shoproom.active = true
                break;
            case 2:
                this.allScense.kitchen.active = true
                break;
            case 3:
                this.allScense.out1.active = true
                break
            default:
                break;
        }
    }

    doSomeThing(data) {
        Log.d(data)
    }

    start() {
        Manager.dispatcher.add(GameEvent.CHAT_SERVICE_CLOSE, this.doSomeThing, this);

    }

    onDestroy() {
        Manager.dispatcher.remove(GameEvent.CHAT_SERVICE_CLOSE, this)
    }
}
