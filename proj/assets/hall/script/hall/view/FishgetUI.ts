import { ViewZOrder } from "../../../../scripts/common/config/Config";
import { Utils } from "../../../../scripts/common/utils/Utils";
import UIView from "../../../../scripts/framework/core/ui/UIView";
import BagUIView from "./BagUIView";


const { ccclass, property } = cc._decorator;

@ccclass
export default class FishgetUI extends UIView {

    static getPrefabUrl() {
        return `prefabs/FishgetUIView`;
    }

    backbtn: cc.Node;
    pushbtn: cc.Node;
    pullbtn: cc.Node;
    bagbtn: cc.Node;
    rog: cc.Node;
    line: cc.Node;
    fishshadow: cc.Node
    rodtip: cc.Node
    prnode: cc.Node
    resultnode: cc.Node

    graphics: cc.Graphics

    @property(cc.Color) lineColor: cc.Color = cc.Color.BLACK; // 线的颜色

    drlineflag: boolean = true

    p1: number = -305;
    p2: number = 28;
    p3: number = 200;
    p4: number = -100;

    canFish: boolean = true

    timeStart: boolean = false
    leftTime: number = 8
    lefttimestr: cc.Label = null

    // LIFE-CYCLE CALLBACKS:

    onLoad() {
        super.onLoad()
        this.backbtn = cc.find('backbtn', this.node);
        this.pushbtn = cc.find('pushbtn', this.node);
        this.pullbtn = cc.find('pullbtn', this.node);
        this.bagbtn = cc.find('bagbtn', this.node);
        this.rog = cc.find('rog', this.node);
        this.line = cc.find('line', this.node);
        this.fishshadow = cc.find('fishshadow', this.node);
        this.rodtip = cc.find('rog/rodtip', this.node)
        this.prnode = cc.find('bar/pr', this.node)
        this.resultnode = cc.find('result', this.node)

        this.lefttimestr = cc.find('lefttime/str', this.node).getComponent(cc.Label)
        this.lefttimestr.string = `${this.leftTime}`

        this.rog.angle = 0;
        this.fishshadow.position = cc.v3(720, 270)

        this.backbtn.on(cc.Node.EventType.TOUCH_END, () => {
            this.onClose();
        }, this)
        this.pushbtn.on(cc.Node.EventType.TOUCH_END, () => {
            // this.onClose();
            this.pushRog()
            this.updatePr()
        }, this)
        this.pullbtn.on(cc.Node.EventType.TOUCH_END, () => {
            // this.onClose();
            this.pullRog();
            this.stopPr();
            this.showResult({})

        }, this)
        this.bagbtn.on(cc.Node.EventType.TOUCH_END, () => {
            // this.onClose();
            Manager.uiManager.open({ type: BagUIView, zIndex: ViewZOrder.UI, bundle: this.bundle, args: { value: 10 } })

        }, this)

        this.schedule(() => {
            if (!this.timeStart)
                return
            this.leftTime -= 1;
            if (this.leftTime >= 0) {
                this.lefttimestr.string = `${this.leftTime}`
            } else {
                this.timeStart = false
                this.canFish = true
            }
        }, 1, 1e+9)

        //绘制钓鱼线效果
        this.drlineflag = false;
        this.graphics = cc.find('drline', this.node).getComponent(cc.Graphics)


    }

    start() {

    }

    public updatePr() {
        cc.Tween.stopAllByTarget(this.prnode)
        cc.tween(this.prnode)
            .repeatForever(
                cc.tween().to(1, { y: 135 }).to(1, { y: -135 })
            )
            .start()
    }

    public stopPr() {
        cc.Tween.stopAllByTarget(this.prnode)
    }

    public resetPr() {
        this.prnode.y = 0
    }

    public showResult(info) {
        this.resultnode.active = true
        this.resultnode.opacity = 255
        cc.Tween.stopAllByTarget(this.resultnode)
        cc.tween(this.resultnode)
            .delay(3)
            .to(1, { opacity: 0 })
            .call(() => {
                this.canFish = true
            })
            .start()
    }

    public onClose(): void {
        super.close()
    }

    public pushRog() {
        if (!this.canFish)
            return
        this.timeStart = true
        this.canFish = false
        this.leftTime = 8
        this.rog.angle = -90
        cc.Tween.stopAllByTarget(this.rog)
        cc.tween(this.rog)
            .to(0.2, { angle: 0 }, { easing: "backOut" })
            // .to(0.3, { angle: 0 }, { easing: "elasticOut" })
            .call(() => {
                this.drlineflag = true
            })
            .start()
        this.fishshadow.active = false
        cc.Tween.stopAllByTarget(this.fishshadow)
        let px = Manager.utils.getRandomNumber(this.p1, this.p2);
        let py = Manager.utils.getRandomNumber(this.p3, this.p4);
        cc.tween(this.fishshadow)
            // .to(0.2, { position: cc.v3(Math.random() * 200, Math.random() * 20, 0) })
            .to(0.2, { position: cc.v3(px, py, 0) })
            .call(() => {
                this.fishshadow.active = true
                cc.tween(this.fishshadow).delay(1)//模拟鱼在水中游动
                    .repeatForever(
                        cc.tween().to(0.3, { x: 1 })
                    )
                    .start()
            })
            .start()
    }

    pullRog() {
        this.timeStart = false
        // this.canFish = true
        this.leftTime = 8
        this.rog.angle = 0
        cc.Tween.stopAllByTarget(this.rog)
        cc.tween(this.rog)
            .to(0.2, { angle: -90 }, { easing: "backOut" })
            .call(() => {
                this.drlineflag = true
            })
            .start()

        cc.Tween.stopAllByTarget(this.fishshadow)
        cc.tween(this.fishshadow)
            // .to(0.2, { position: cc.v3(Math.random() * 200, Math.random() * 20, 0) })
            .to(0.2, { position: cc.v3(400, 400, 0) })
            .call(() => {
                this.fishshadow.active = false
            })
            .start()
    }

    drawFishingLine() {
        if (!this.rodtip || !this.fishshadow) return;

        this.graphics.clear(); // 清除上次绘制的线

        // 获取世界坐标
        let startWorld = this.rodtip.convertToWorldSpaceAR(cc.Vec2.ZERO);
        let endWorld = this.fishshadow.convertToWorldSpaceAR(cc.Vec2.ZERO);

        // 转换到当前 Graphics 节点的本地坐标
        let start = this.node.convertToNodeSpaceAR(startWorld);
        let end = this.node.convertToNodeSpaceAR(endWorld);

        // 计算贝塞尔控制点（中点稍微下移，形成自然弧度）
        let mid = cc.v2((start.x + end.x) / 2, (start.y + end.y) / 2 + 100);

        // 设置线条样式
        this.graphics.lineWidth = 3; // 线宽
        this.graphics.strokeColor = this.lineColor; // 颜色

        // 绘制贝塞尔曲线
        this.graphics.moveTo(start.x, start.y);
        this.graphics.quadraticCurveTo(mid.x, mid.y, end.x, end.y);
        this.graphics.stroke();
    }


    update(dt) {
        if (!this.drlineflag) return
        this.drawFishingLine();
    }
}
