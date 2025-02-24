

import UIView from "../../../../scripts/framework/core/ui/UIView";

const { ccclass, property } = cc._decorator;

@ccclass
export default class TestUIView extends UIView {

    static getPrefabUrl() {
        return `prefabs/TestUIView`;
    }

    @property(cc.Label)
    label: cc.Label = null;

    @property
    text: string = 'hello';

    // LIFE-CYCLE CALLBACKS:

    onLoad () {
        super.onLoad()
    }

    start() {
        cc.error(this.node)
    }

    // update (dt) {}
}
