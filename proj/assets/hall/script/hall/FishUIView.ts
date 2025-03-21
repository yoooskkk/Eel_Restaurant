const { ccclass, property } = cc._decorator;

@ccclass
export default class FishUIView extends cc.Component {

    @property(cc.SpriteAtlas)
    sps: cc.SpriteAtlas[] = []
    // LIFE-CYCLE CALLBACKS:

    // onLoad () {}

    start() {
    }   
    // update (dt) {}
}
