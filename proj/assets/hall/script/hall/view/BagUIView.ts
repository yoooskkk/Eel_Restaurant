import UIView from "../../../../scripts/framework/core/ui/UIView";
import { FishData } from "../../data/FishData";
import FishUIView from "../FishUIView";

const { ccclass, property } = cc._decorator;

@ccclass
export default class BagUIView extends UIView {

    static getPrefabUrl() {
        return `prefabs/BagUIView`;
    }

    backbtn: cc.Node;

    fishC: cc.Node;

    fishP: cc.Node;

    @property(cc.Label)
    label: cc.Label = null;

    @property
    text: string = 'hello';

    // LIFE-CYCLE CALLBACKS:

    onLoad() {
        super.onLoad()
        this.backbtn = cc.find('backbtn', this.node)
        this.fishC = cc.find('FishUiView', this.node)
        this.fishP = cc.find('scrollview/view/content', this.node)

        this.backbtn.on(cc.Node.EventType.TOUCH_END, () => {
            this.onClose();
        }, this)
    }

    public onClose() {
        super.close();
    }

    start() {
        let bagfishes = []
        for (let i = 0; i < 10; i++) {
            bagfishes[i] = [] 
            for (let j = 0; j < 5; j++) {
                let tmp = cc.instantiate(this.fishC)               
                bagfishes[i][j] = tmp
            }
        }
        for(let i=0;i<10;i++){
            for(let j=0;j<5;j++){
                bagfishes[i][j].active = true
                bagfishes[i][j].parent = this.fishP
                bagfishes[i][j].getComponent(FishUIView).setFishSp(i, FishData[i][j])
            }
        }
    }

    // update (dt) {}
}
