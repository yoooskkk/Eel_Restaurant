import { GameEvent } from "../../../../scripts/common/event/CommonEvent";
import GameView from "../../../../scripts/framework/core/ui/GameView";
import { Macro } from "../../../../scripts/framework/defines/Macros";

const { ccclass, property } = cc._decorator;

@ccclass
export default class SubGameOneView extends GameView {

    static getPrefabUrl() {
        //预制体路径
        return `prefabs/SubGameOneView`;
    }

    private _backbtn: cc.Node = null;

    private _bulletLayer: cc.Node = null
    onLoad() {
        super.onLoad();
        this._backbtn = cc.find("backbtn", this.node);
        this._bulletLayer = cc.find('bulletLayer', this.node)
        // let version = cc.find("version", this.node)?.getComponent(cc.Label);
        // if (version) {
        //     version.string = Manager.updateManager.getVersion(this.bundle);
        // }
        this._backbtn.on(cc.Node.EventType.TOUCH_END, () => {
            this.enterBundle(Macro.BUNDLE_HALL);
        });

    }

    doSomeThing(data) {
        Log.d(data)
    }

    start() {
        Manager.dispatcher.add(GameEvent.CHAT_SERVICE_CLOSE, this.doSomeThing, this);
        Manager.pool.createPool('bullet')
        Manager.asset.load(Macro.BUNDLE_SUBGAMEONE, 'prefabs/bullet', cc.Prefab, (finish, total, item) => { }, (data) => {
            if (data && data.data && data.data instanceof cc.Prefab) {
                let node = cc.instantiate(data.data)
                Manager.pool.getPool('bullet').cloneNode = node
                Manager.pool.getPool('bullet').put(node)

                this.schedule(() => {
                    Log.d(Manager.pool.getPool('bullet').size)
                    let node = Manager.pool.getPool('bullet').get()
                    node.parent = this._bulletLayer
                    node.position = cc.v3(0, 0, 0,)
                    cc.tween(node).to(2, { position: cc.v3(Math.random() * 400, Math.random() * 200, 0) }).call(() => {
                        node.removeFromParent()
                        Manager.pool.getPool('bullet').put(node)
                    }).start()
                }, 0.01, 1e+9)
            }
        })
    }

    onDestroy() {
        Manager.dispatcher.remove(GameEvent.CHAT_SERVICE_CLOSE, this)
    }
}
